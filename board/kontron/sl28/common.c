/*
 * Copyright (C) 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include "sl28.h"

int board_early_init_f(void)
{
	fsl_lsch3_early_init_f();
	return 0;
}

int board_boot_source(void)
{
	return(in_le32(DCFG_BASE + DCFG_PORSR1) & PORSR1_RCW_SRC);
}

