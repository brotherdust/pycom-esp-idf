// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "soc/sdmmc_reg.h"
#include "soc/sdmmc_struct.h"
#include "sdmmc_defs.h"
#include "sdmmc_types.h"
#include "sdmmc_periph.h"
#include "sdmmc_req.h"

#define SDMMC_DMA_DESC_CNT  4

static const char* TAG = "sdmmc_req";

typedef enum {
    SDMMC_IDLE,
    SDMMC_SENDING_CMD,
    SDMMC_SENDING_DATA,
    SDMMC_BUSY,
} sdmmc_req_state_t;

typedef struct {
    uint8_t* ptr;
    size_t size_remaining;
    size_t next_desc;
    size_t desc_remaining;
} sdmmc_transfer_state_t;

const uint32_t SDMMC_DATA_ERR_MASK =
        SDMMC_INTMASK_DTO | SDMMC_INTMASK_DCRC |
        SDMMC_INTMASK_HTO | SDMMC_INTMASK_SBE  |
        SDMMC_INTMASK_EBE;

const uint32_t SDMMC_DMA_DONE_MASK =
        SDMMC_IDMAC_INTMASK_RI | SDMMC_IDMAC_INTMASK_TI |
        SDMMC_IDMAC_INTMASK_NI;

const uint32_t SDMMC_CMD_ERR_MASK =
        SDMMC_INTMASK_RTO |
        SDMMC_INTMASK_RCRC |
        SDMMC_INTMASK_RESP_ERR;

static QueueHandle_t s_event_queue;
static sdmmc_desc_t s_dma_desc[SDMMC_DMA_DESC_CNT];
static sdmmc_transfer_state_t s_cur_transfer = { 0 };

static esp_err_t sdmmc_handle_idle_state_events();
static sdmmc_hw_cmd_t make_hw_cmd(sdmmc_command_t* cmd);
static esp_err_t sdmmc_handle_event(sdmmc_command_t* cmd, sdmmc_req_state_t* pstate);
static esp_err_t sdmmc_process_events(sdmmc_event_t evt, sdmmc_command_t* cmd, sdmmc_req_state_t* pstate);
static void sdmmc_process_command_response(uint32_t status, sdmmc_command_t* cmd);
static void sdmmc_fill_dma_descriptors(size_t num_desc);

esp_err_t sdmmc_req_init()
{
    s_event_queue = xQueueCreate(32, sizeof(sdmmc_event_t));
    if (!s_event_queue) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = sdmmc_hw_init(40000, s_event_queue);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

void sdmmc_req_deinit()
{
    vQueueDelete(s_event_queue);
}

esp_err_t sdmmc_req_run(sdmmc_command_t* cmdinfo)
{
    // dispose of any events which happened asynchronously
    sdmmc_handle_idle_state_events();
    // convert cmdinfo to hardware register value
    sdmmc_hw_cmd_t hw_cmd = make_hw_cmd(cmdinfo);
    if (cmdinfo->data) {
        // these constraints should be handled by upper layer
        assert(cmdinfo->datalen >= 4);
        assert(cmdinfo->blklen % 4 == 0);
        // this clears "owned by IDMAC" bits
        memset(s_dma_desc, 0, sizeof(s_dma_desc));
        // initialize first descriptor
        s_dma_desc[0].first_descriptor = 1;
        // save transfer info
        s_cur_transfer.ptr = (uint8_t*) cmdinfo->data;
        s_cur_transfer.size_remaining = cmdinfo->datalen;
        s_cur_transfer.next_desc = 0;
        s_cur_transfer.desc_remaining = (cmdinfo->datalen + SDMMC_DMA_MAX_BUF_LEN - 1) / SDMMC_DMA_MAX_BUF_LEN;
        // prepare descriptors
        sdmmc_fill_dma_descriptors(SDMMC_DMA_DESC_CNT);
        // write transfer info into hardware
        sdmmc_idma_prepare_transfer(&s_dma_desc[0], cmdinfo->blklen, cmdinfo->datalen);
    }
    // write command into hardware, this also sends the command to the card
    sdmmc_start_command(hw_cmd, cmdinfo->arg);
    // process events until transfer is complete
    esp_err_t ret = ESP_OK;
    cmdinfo->error = ESP_OK;
    sdmmc_req_state_t state = SDMMC_SENDING_CMD;
    while (state != SDMMC_IDLE) {
        ret = sdmmc_handle_event(cmdinfo, &state);
        if (ret != ESP_OK) {
            break;
        }
    }
    return ESP_OK;
}

static void sdmmc_fill_dma_descriptors(size_t num_desc)
{
    for (size_t i = 0; i < num_desc; ++i) {
        if (s_cur_transfer.size_remaining == 0) {
            return;
        }
        const size_t next = s_cur_transfer.next_desc;
        sdmmc_desc_t* desc = &s_dma_desc[next];
        assert(!desc->owned_by_idmac);
        size_t size_to_fill =
            (s_cur_transfer.size_remaining < SDMMC_DMA_MAX_BUF_LEN) ?
                s_cur_transfer.size_remaining : SDMMC_DMA_MAX_BUF_LEN;
        bool last = size_to_fill == s_cur_transfer.size_remaining;
        desc->last_descriptor = last;
        desc->second_address_chained = 1;
        desc->owned_by_idmac = 1;
        desc->buffer1_ptr = s_cur_transfer.ptr;
        desc->next_desc_ptr = (last) ? NULL : &s_dma_desc[(next + 1) % SDMMC_DMA_DESC_CNT];
        desc->buffer1_size = size_to_fill;

        s_cur_transfer.size_remaining -= size_to_fill;
        s_cur_transfer.ptr += size_to_fill;
        s_cur_transfer.next_desc = (s_cur_transfer.next_desc + 1) % SDMMC_DMA_DESC_CNT;
        ESP_LOGD(TAG, "fill %d desc=%d rem=%d next=%d last=%d sz=%d",
                num_desc, next, s_cur_transfer.size_remaining,
                s_cur_transfer.next_desc, desc->last_descriptor, desc->buffer1_size);
    }
}

static esp_err_t sdmmc_handle_idle_state_events()
{
    /* Handle any events which have happened in between transfers.
     * Under current assumptions (no SDIO support) only card detect events
     * can happen in the idle state.
     */
    sdmmc_event_t evt;
    while (xQueueReceive(s_event_queue, &evt, 0)) {
        if (evt.sdmmc_status & SDMMC_INTMASK_CD) {
            ESP_LOGV(TAG, "card detect event");
            evt.sdmmc_status &= ~SDMMC_INTMASK_CD;
        }
        if (evt.sdmmc_status != 0 || evt.dma_status != 0) {
            ESP_LOGE(TAG, "handle_idle_state_events unhandled: %08x %08x",
                    evt.sdmmc_status, evt.dma_status);
        }

    }
    return ESP_OK;
}


static esp_err_t sdmmc_handle_event(sdmmc_command_t* cmd, sdmmc_req_state_t* state)
{
    sdmmc_event_t evt;
    xQueueReceive(s_event_queue, &evt, portMAX_DELAY);
    ESP_LOGV(TAG, "sdmmc_handle_event: evt %08x %08x", evt.sdmmc_status, evt.dma_status);
    sdmmc_process_events(evt, cmd, state);
    return ESP_OK;
}

static sdmmc_hw_cmd_t make_hw_cmd(sdmmc_command_t* cmd)
{
    sdmmc_hw_cmd_t res = { 0 };

    res.cmd_index = cmd->opcode;
    if (cmd->opcode == MMC_STOP_TRANSMISSION) {
        res.stop_abort_cmd = 1;
    } else {
        res.wait_complete = 1;
    }
    if (cmd->opcode == SD_APP_SET_BUS_WIDTH) {
        res.send_auto_stop = 1;
        res.data_expected = 1;
    }
    if (cmd->flags & SCF_RSP_PRESENT) {
        res.response_expect = 1;
        if (cmd->flags & SCF_RSP_136) {
            res.response_long = 1;
        }
    }
    if (cmd->flags & SCF_RSP_CRC) {
        res.check_response_crc = 1;
    }
    res.use_hold_reg = 1;
    if (cmd->data) {
        res.data_expected = 1;
        if ((cmd->flags & SCF_CMD_READ) == 0) {
            res.rw = 1;
        }
        assert(cmd->datalen % cmd->blklen == 0);
        if ((cmd->datalen / cmd->blklen) > 1) {
            res.send_auto_stop = 1;
        }
    }
    res.card_num = 1;
    ESP_LOGV(TAG, "%s: opcode=%d, rexp=%d, crc=%d", __func__,
            res.cmd_index, res.response_expect, res.check_response_crc);
    return res;
}

static void sdmmc_process_command_response(uint32_t status, sdmmc_command_t* cmd)
{
    if (cmd->flags & SCF_RSP_PRESENT) {
        if (cmd->flags & SCF_RSP_136) {
            cmd->response[3] = SDMMC.resp[0];
            cmd->response[2] = SDMMC.resp[1];
            cmd->response[1] = SDMMC.resp[2];
            cmd->response[0] = SDMMC.resp[3];

        } else {
            cmd->response[0] = SDMMC.resp[0];
            cmd->response[1] = 0;
            cmd->response[2] = 0;
            cmd->response[3] = 0;
        }
    }

    if ((status & SDMMC_INTMASK_RTO) &&
        cmd->opcode != MMC_ALL_SEND_CID &&
        cmd->opcode != MMC_SELECT_CARD &&
        cmd->opcode != MMC_STOP_TRANSMISSION) {
        cmd->error = ESP_ERR_TIMEOUT;
    } else if ((cmd->flags & SCF_RSP_CRC) && (status & SDMMC_INTMASK_RCRC)) {
        cmd->error = ESP_ERR_INVALID_CRC;
    } else if (status & SDMMC_INTMASK_RESP_ERR) {
        cmd->error = ESP_ERR_INVALID_RESPONSE;
    }
    if (cmd->error != 0) {
        if (cmd->data) {
            sdmmc_idma_stop();
        }
        ESP_LOGE(TAG, "%s: error %d", __func__, cmd->error);
    }
}

static void process_data_status(uint32_t status, sdmmc_command_t* cmd)
{
    if (status & SDMMC_DATA_ERR_MASK) {
        if (status & SDMMC_INTMASK_DTO) {
            cmd->error = ESP_ERR_TIMEOUT;
        } else if (status & SDMMC_INTMASK_DCRC) {
            cmd->error = ESP_ERR_INVALID_CRC;
        } else if ((status & SDMMC_INTMASK_EBE) &&
                (cmd->flags & SCF_CMD_READ) == 0) {
            cmd->error = ESP_ERR_TIMEOUT;
        } else {
            cmd->error = ESP_FAIL;
        }
        SDMMC.ctrl.fifo_reset = 1;
    }
}

static bool mask_check_and_clear(uint32_t* state, uint32_t mask) {
    bool ret = ((*state) & mask) != 0;
    *state &= ~mask;
    return ret;
}

static esp_err_t sdmmc_process_events(sdmmc_event_t evt, sdmmc_command_t* cmd, sdmmc_req_state_t* pstate)
{
    const char* const s_state_names[] __attribute__((unused)) = {
        "IDLE",
        "SENDING_CMD",
        "SENDIND_DATA",
        "BUSY"
    };

    sdmmc_req_state_t state = *pstate;
    sdmmc_req_state_t prev_state;
    sdmmc_event_t orig_evt = evt;
    ESP_LOGV(TAG, "%s: state=%s", __func__, s_state_names[state]);

    do {
        prev_state = state;
        switch (state) {
            case SDMMC_IDLE:
                break;

            case SDMMC_SENDING_CMD:
                if (mask_check_and_clear(&evt.sdmmc_status, SDMMC_CMD_ERR_MASK)) {
                    sdmmc_process_command_response(orig_evt.sdmmc_status, cmd);
                    break;
                }

                if (!mask_check_and_clear(&evt.sdmmc_status, SDMMC_INTMASK_CMD_DONE)) {
                    break;
                }
                sdmmc_process_command_response(orig_evt.sdmmc_status, cmd);
                if (cmd->error != ESP_OK || cmd->data == NULL) {
                    state = SDMMC_IDLE;
                    break;
                }
                state = SDMMC_SENDING_DATA;
                break;


            case SDMMC_SENDING_DATA:
                if (mask_check_and_clear(&evt.sdmmc_status, SDMMC_DATA_ERR_MASK)) {
                    process_data_status(orig_evt.sdmmc_status, cmd);
                    sdmmc_idma_stop();
                }

                if (mask_check_and_clear(&evt.dma_status, SDMMC_DMA_DONE_MASK)) {
                    s_cur_transfer.desc_remaining--;
                    if (s_cur_transfer.size_remaining) {
                        sdmmc_fill_dma_descriptors(1);
                    }
                    if (s_cur_transfer.desc_remaining == 0) {
                        state = SDMMC_BUSY;
                    }
                }
                break;

            case SDMMC_BUSY:
                if (!mask_check_and_clear(&evt.sdmmc_status, SDMMC_INTMASK_DATA_OVER)) {
                    break;
                }
                process_data_status(orig_evt.sdmmc_status, cmd);
                state = SDMMC_IDLE;
                break;
        }
        ESP_LOGV(TAG, "%s prev_state=%s state=%s", __func__, s_state_names[prev_state], s_state_names[state]);
    } while (state != prev_state);
    *pstate = state;
    return ESP_OK;
}



