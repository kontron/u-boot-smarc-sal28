/*
 * Copyright (C) 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include "sl28.h"

DECLARE_GLOBAL_DATA_PTR;

#define PORSR1_RCW_SRC      0x07800000
#define PORSR1_RCW_SRC_SDHC 0x04000000
#define PORSR1_RCW_SRC_MMC  0x04800000
#define PORSR1_RCW_SRC_I2C  0x05000000
#define PORSR1_RCW_SRC_FSPI 0x07800000

int board_early_init_f(void)
{
	fsl_lsch3_early_init_f();
	return 0;
}

u32 get_lpuart_clk(void)
{
	return gd->bus_clk / CONFIG_SYS_FSL_LPUART_CLK_DIV;
}

enum boot_source sl28_boot_source(void)
{
	u32 rcw_src = in_le32(DCFG_BASE + DCFG_PORSR1) & PORSR1_RCW_SRC;

	debug("%s: rcw_src=%x\n", __func__, rcw_src);

	switch (rcw_src) {
	case PORSR1_RCW_SRC_SDHC:
		return BOOT_SOURCE_SDHC;
	case PORSR1_RCW_SRC_MMC:
		return BOOT_SOURCE_MMC;
	case PORSR1_RCW_SRC_I2C:
		return BOOT_SOURCE_I2C;
	case PORSR1_RCW_SRC_FSPI:
		return BOOT_SOURCE_FSPI;
	}

	printf("unknown bootsource (%08x)\n", rcw_src);
	return BOOT_SOURCE_UNKNOWN;
}
