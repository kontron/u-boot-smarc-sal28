/*
 * Copyright 2018 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DDR_H__
#define __DDR_H__

#define DDR_CDR2_VREF_OVRD_EN   0x8000
#define DDR_CDR2_VREF_OVRD_100  0x3C00

struct board_specific_parameters {
	u32 n_ranks;
	u32 datarate_mhz_high;
	u32 rank_gb;
	u32 clk_adjust;
	u32 wrlvl_start;
	u32 wrlvl_ctl_2;
	u32 wrlvl_ctl_3;
	u32 cpo_override;
	u32 write_data_delay;
	u32 force_2t;
};

/*
 * These tables contain all valid speeds we want to override with board
 * specific parameters. datarate_mhz_high values need to be in ascending order
 * for each n_ranks group.
 */
static const struct board_specific_parameters udimm0[] = {
	/*
	 * memory controller 0
	 *   num|  hi | rank|  clk| wrlvl |   wrlvl   |  wrlvl    | cpo |wrdata|2T
	 * ranks| mhz | GB  |adjst| start |   ctl2    |  ctl3     |     |delay |
	 */
#ifdef CONFIG_SYS_FSL_DDR3
	{      1, 1350,    0,    8,      8, 0x0708080a, 0x0a0b0c09, 0x1f,     8, 0},
	{      1, 1666,    2,   10,      7, 0x060a0900, 0x0000000b, 0x1f,     8, 0},
	{      2, 1350,    0,    8,      8, 0x0708080a, 0x0a0b0c09, 0x1f,     8, 0},
	{      2, 1666,    2,   10,      7, 0x060a0900, 0x0000000b, 0x1f,     8, 0},
	{      2, 1666,    0,   10,      7, 0x060a0900, 0x0000000b, 0x1f,     8, 0},
	{      4, 1350,    0,    8,      8, 0x0708080a, 0x0a0b0c09, 0x1f,     8, 0},
	{      4, 1666,    0,   10,      7, 0x060a0900, 0x0000000b, 0x1f,     8, 0},
#else
#error DDR type not defined
#endif
	{}
};

static const struct board_specific_parameters *udimms[] = {
	udimm0,
};

#endif
