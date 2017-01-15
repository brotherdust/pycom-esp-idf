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
#ifndef _SOC_SDMMC_REG_H_
#define _SOC_SDMMC_REG_H_
#include "soc.h"

#define SD_HOST_REG_BASE	0x60028000  // or DR_REG_SDMMC_BASE
#define SD_HOST_REG_CTRL	(SD_HOST_REG_BASE + 0x00)
#define SD_HOST_REG_PWREN	(SD_HOST_REG_BASE + 0x04)
#define SD_HOST_REG_CLKDIV	(SD_HOST_REG_BASE + 0x08)
#define SD_HOST_REG_CLKSRC	(SD_HOST_REG_BASE + 0x0c)
#define SD_HOST_REG_CLKENA	(SD_HOST_REG_BASE + 0x10)
#define SD_HOST_REG_TMOUT	(SD_HOST_REG_BASE + 0x14)
#define SD_HOST_REG_CTYPE	(SD_HOST_REG_BASE + 0x18)
#define SD_HOST_REG_BLKSIZ	(SD_HOST_REG_BASE + 0x1c)
#define SD_HOST_REG_BYTCNT	(SD_HOST_REG_BASE + 0x20)
#define SD_HOST_REG_INTMASK	(SD_HOST_REG_BASE + 0x24)
#define SD_HOST_REG_CMDARG	(SD_HOST_REG_BASE + 0x28)
#define SD_HOST_REG_CMD		(SD_HOST_REG_BASE + 0x2c)
#define SD_HOST_REG_RESP0		(SD_HOST_REG_BASE + 0x30)
#define SD_HOST_REG_RESP1		(SD_HOST_REG_BASE + 0x34)
#define SD_HOST_REG_RESP2		(SD_HOST_REG_BASE + 0x38)
#define SD_HOST_REG_RESP3		(SD_HOST_REG_BASE + 0x3c)

#define SD_HOST_REG_MINTSTS		(SD_HOST_REG_BASE + 0x40)
#define SD_HOST_REG_RINTSTS		(SD_HOST_REG_BASE + 0x44)
#define SD_HOST_REG_STATUS		(SD_HOST_REG_BASE + 0x48)
#define SD_HOST_REG_FIFOTH		(SD_HOST_REG_BASE + 0x4c)
#define SD_HOST_REG_CDETECT		(SD_HOST_REG_BASE + 0x50)
#define SD_HOST_REG_WRTPRT		(SD_HOST_REG_BASE + 0x54)
#define SD_HOST_REG_GPIO		(SD_HOST_REG_BASE + 0x58)
#define SD_HOST_REG_TCBCNT		(SD_HOST_REG_BASE + 0x5c)
#define SD_HOST_REG_TBBCNT		(SD_HOST_REG_BASE + 0x60)
#define SD_HOST_REG_DEBNCE		(SD_HOST_REG_BASE + 0x64)
#define SD_HOST_REG_USRID		(SD_HOST_REG_BASE + 0x68)
#define SD_HOST_REG_VERID		(SD_HOST_REG_BASE + 0x6c)
#define SD_HOST_REG_HCON		(SD_HOST_REG_BASE + 0x70)
#define SD_HOST_REG_UHS_REG		(SD_HOST_REG_BASE + 0x74)
#define SD_HOST_REG_RST_N		(SD_HOST_REG_BASE + 0x78)
#define SD_HOST_REG_BMOD		(SD_HOST_REG_BASE + 0x80)
#define SD_HOST_REG_PLDMND		(SD_HOST_REG_BASE + 0x84)
#define SD_HOST_REG_DBADDR		(SD_HOST_REG_BASE + 0x88)
#define SD_HOST_REG_DBADDRU		(SD_HOST_REG_BASE + 0x8c)
#define SD_HOST_REG_IDSTS		(SD_HOST_REG_BASE + 0x8c)
#define SD_HOST_REG_IDINTEN		(SD_HOST_REG_BASE + 0x90)
#define SD_HOST_REG_DSCADDR		(SD_HOST_REG_BASE + 0x94)
#define SD_HOST_REG_DSCADDRL		(SD_HOST_REG_BASE + 0x98)
#define SD_HOST_REG_DSCADDRU		(SD_HOST_REG_BASE + 0x9c)
#define SD_HOST_REG_BUFADDRL		(SD_HOST_REG_BASE + 0xa0)
#define SD_HOST_REG_BUFADDRU		(SD_HOST_REG_BASE + 0xa4)
#define SD_HOST_REG_CARDTHRCTL		(SD_HOST_REG_BASE + 0x100)
#define SD_HOST_REG_BACK_END_POWER		(SD_HOST_REG_BASE + 0x104)
#define SD_HOST_REG_UHS_REG_EXT		(SD_HOST_REG_BASE + 0x108)
#define SD_HOST_REG_EMMC_DDR_REG		(SD_HOST_REG_BASE + 0x10c)
#define SD_HOST_REG_ENABLE_SHIFT		(SD_HOST_REG_BASE + 0x110)


#define SD_HOST_REG_CLOCK		(SD_HOST_REG_BASE + 0x120)



#define SDMMC_INTMASK_EBE           BIT(15)
#define SDMMC_INTMASK_ACD           BIT(14)
#define SDMMC_INTMASK_SBE           BIT(13)
#define SDMMC_INTMASK_HLE           BIT(12)
#define SDMMC_INTMASK_FRUN          BIT(11)
#define SDMMC_INTMASK_HTO           BIT(10)
#define SDMMC_INTMASK_DTO           BIT(9)
#define SDMMC_INTMASK_RTO           BIT(8)
#define SDMMC_INTMASK_DCRC          BIT(7)
#define SDMMC_INTMASK_RCRC          BIT(6)
#define SDMMC_INTMASK_RXDR          BIT(5)
#define SDMMC_INTMASK_TXDR          BIT(4)
#define SDMMC_INTMASK_DATA_OVER     BIT(3)
#define SDMMC_INTMASK_CMD_DONE      BIT(2)
#define SDMMC_INTMASK_RESP_ERR      BIT(1)
#define SDMMC_INTMASK_CD            BIT(0)


#define SDMMC_IDMAC_INTMASK_AI      BIT(9)
#define SDMMC_IDMAC_INTMASK_NI      BIT(8)
#define SDMMC_IDMAC_INTMASK_CES     BIT(5)
#define SDMMC_IDMAC_INTMASK_DU      BIT(4)
#define SDMMC_IDMAC_INTMASK_FBE     BIT(2)
#define SDMMC_IDMAC_INTMASK_RI      BIT(1)
#define SDMMC_IDMAC_INTMASK_TI      BIT(0)


#endif /* _SOC_SDMMC_REG_H_  */




