/*
 * Copyright (C) 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>

int board_early_init_f(void)
{
	fsl_lsch3_early_init_f();
	return 0;
}

