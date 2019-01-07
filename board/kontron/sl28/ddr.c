/*
 * Copyright 2018 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fsl_ddr_sdram.h>
#include <fsl_ddr_dimm_params.h>
#include <asm/arch/soc.h>
#include <asm/arch/clock.h>
#include <asm/io.h>
#include "ddr.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SYS_DDR_RAW_TIMING
void fsl_ddr_board_options(memctl_options_t *popts,
			   dimm_params_t *pdimm,
			   unsigned int ctrl_num)
{
	const struct board_specific_parameters *pbsp, *pbsp_highest = NULL;
	ulong ddr_freq;

	if (ctrl_num > 1) {
		printf("Not supported controller number %d\n", ctrl_num);
		return;
	}
	if (!pdimm->n_ranks)
		return;

	/*
	 * we use identical timing for all slots. If needed, change the code
	 * to  pbsp = rdimms[ctrl_num] or pbsp = udimms[ctrl_num];
	 */
	pbsp = udimms[0];

	/* Get clk_adjust, wrlvl_start, wrlvl_ctl, according to the board ddr
	 * freqency and n_banks specified in board_specific_parameters table.
	 */
	ddr_freq = get_ddr_freq(0) / 1000000;
	while (pbsp->datarate_mhz_high) {
		if (pbsp->n_ranks == pdimm->n_ranks) {
			if (ddr_freq <= pbsp->datarate_mhz_high) {
				popts->clk_adjust = pbsp->clk_adjust;
				popts->wrlvl_start = pbsp->wrlvl_start;
				popts->wrlvl_ctl_2 = pbsp->wrlvl_ctl_2;
				popts->wrlvl_ctl_3 = pbsp->wrlvl_ctl_3;
				popts->cpo_override = pbsp->cpo_override;
				popts->write_data_delay =
					pbsp->write_data_delay;
				goto found;
			}
			pbsp_highest = pbsp;
		}
		pbsp++;
	}

	if (pbsp_highest) {
		printf("Error: board specific timing not found for %lu MT/s\n",
		       ddr_freq);
		printf("Trying to use the highest speed (%u) parameters\n",
		       pbsp_highest->datarate_mhz_high);
		popts->clk_adjust = pbsp_highest->clk_adjust;
		popts->wrlvl_start = pbsp_highest->wrlvl_start;
		popts->wrlvl_ctl_2 = pbsp->wrlvl_ctl_2;
		popts->wrlvl_ctl_3 = pbsp->wrlvl_ctl_3;
	} else {
		panic("DIMM is not supported by this board");
	}
found:
	debug("Found timing match: n_ranks %d, data rate %d, rank_gb %d\n"
		"\tclk_adjust %d, wrlvl_start %d, wrlvl_ctrl_2 0x%x, wrlvl_ctrl_3 0x%x\n",
		pbsp->n_ranks, pbsp->datarate_mhz_high, pbsp->rank_gb,
		pbsp->clk_adjust, pbsp->wrlvl_start, pbsp->wrlvl_ctl_2,
		pbsp->wrlvl_ctl_3);


	/* force DDR bus width to 32 bits */
	popts->data_bus_width = 1;
	popts->otf_burst_chop_en = 0;
	popts->burst_length = DDR_BL8;
	/* popts->bstopre = 0;		* enable auto precharge */

	/*
	 * Factors to consider for half-strength driver enable:
	 *	- number of DIMMs installed
	 */
	popts->half_strength_driver_enable = 0;

	/*
	 * Write leveling override
	 */
	popts->wrlvl_override = 1;
	popts->wrlvl_sample = 0xf;

	/*
	 * RWT override
	 */

	popts->trwt_override = 1;
	popts->trwt = 2;

	/*
	 * Rtt and Rtt_WR override
	 */
	popts->rtt_override = 0;

	/* Enable ZQ calibration */
	popts->zq_en = 1;

	popts->cswl_override = DDR_CSWL_CS0;

	/* optimize cpo for erratum A-009942 */
	popts->cpo_sample = 0x39;

	/* DHC_EN = 1, ODT = 75 Ohm */
	popts->ddr_cdr1 = DDR_CDR1_DHC_EN | DDR_CDR1_ODT(DDR_CDR_ODT_75ohm);
	popts->ddr_cdr2 = DDR_CDR2_ODT(DDR_CDR_ODT_75ohm);
}

dimm_params_t ddr_raw_timing = {
	.n_ranks = 2,
	.rank_density = 0x80000000u,
	.capacity = 0x080000000u,
	.primary_sdram_width = 32,
	.ec_sdram_width = 4,
	.registered_dimm = 0,
	.mirrored_dimm = 1,
	.n_row_addr = 16,
	.n_col_addr = 10,
	.n_banks_per_sdram_device = 8,
	.edc_config = 2,
	.burst_lengths_bitmask = 0x0c,

	/* timing settings based on DDR3L-1600 speed bin */
	.tckmin_x_ps = 1071,
	.tckmax_ps = 1900,
	/* .caslat_x = 0x2fe << 4,	* 5,6,7,8,9,10,11,13 */
	.caslat_x = 0x0001ffe0,
	.taa_ps = 13500,	/* min. 13.75ns */
	.twr_ps = 15000,	/* min. 15ns */
	.trcd_ps = 13500,	/* 11*tCK */
	.trrd_ps = 7500,	/* 6*tCK */
	.trp_ps = 13750,	/* 11*tCK */
	.tras_ps = 35000,	/* 28*tCK */
	.trc_ps = 49500,	/* 39*tCK */
	.trfc_ps = 260000,	/* 208*tCK */
	.tfaw_ps = 30000,	/* 24*tCK */
	.twtr_ps = 7500,	/* 4*tCK or 7.5ns */
	.trtp_ps = 7500,	/* 4*tCK or 7.5ns */
	.refresh_rate_ps = 3900000,	/* 3.9us for ext. temp. range */
};

int fsl_ddr_get_dimm_params(dimm_params_t *pdimm,
			    unsigned int controller_number,
			    unsigned int dimm_number)
{
	static const char dimm_model[] = "Fixed DDR on board";

	if (((controller_number == 0) && (dimm_number == 0)) ||
	    ((controller_number == 1) && (dimm_number == 0))) {
		memcpy(pdimm, &ddr_raw_timing, sizeof(dimm_params_t));
		memset(pdimm->mpart, 0, sizeof(pdimm->mpart));
		memcpy(pdimm->mpart, dimm_model, sizeof(dimm_model) - 1);
	}

	return 0;
}
#else

static phys_size_t fixed_sdram(void)
{
	size_t ddr_size;

	fsl_ddr_cfg_regs_t ddr_cfg_regs = {
		.cs[0].bnds		= 0x0000007f,
		.cs[0].config		= 0x80044402,
		.cs[0].config_2		= 0,
		.cs[1].bnds		= 0x008000ff,
		.cs[1].config		= 0x80004402,
		.cs[1].config_2		= 0,

		.timing_cfg_3		= 0x010e1000,
		.timing_cfg_0		= 0x9055000c,
		.timing_cfg_1		= 0xbcb48c66,
		.timing_cfg_2		= 0x0040d118,
		.ddr_sdram_cfg		= 0xe70c0004,
		.ddr_sdram_cfg_2	= 0x00401011,
		.ddr_sdram_mode		= 0x00061c70,
		.ddr_sdram_mode_2	= 0x00180000,

		.ddr_sdram_md_cntl	= 0x0600001f,
		.ddr_sdram_interval	= 0x18600618,
		.ddr_data_init		= 0xdeadbeef,

		.ddr_sdram_clk_cntl	= 0x02000000,
		.ddr_init_addr		= 0,
		.ddr_init_ext_addr	= 0,

		.timing_cfg_4		= 0x00000001,
		.timing_cfg_5		= 0x04401400,
		.timing_cfg_6		= 0x0,
		.timing_cfg_7		= 0x0,

		.ddr_zq_cntl		= 0x89080600,
		.ddr_wrlvl_cntl		= 0x8655f608,
		.ddr_sr_cntr		= 0,
		.ddr_sdram_rcw_1	= 0,
		.ddr_sdram_rcw_2	= 0,
		.ddr_wrlvl_cntl_2	= 0x0708080a,
		.ddr_wrlvl_cntl_3	= 0x0a0b0c09,

		.ddr_sdram_mode_9	= 0x00000400,
		.ddr_sdram_mode_9	= 0x04000000,

		.timing_cfg_8		= 0x06115600,

		.dq_map_0		= 0x5b65b658,
		.dq_map_1		= 0xd96d8000,
		.dq_map_2		= 0,
		.dq_map_3		= 0x01600000,

		.ddr_cdr1		= 0x80040000,
		.ddr_cdr2		= 0x00000001,
	};


	fsl_ddr_set_memctl_regs(&ddr_cfg_regs, 0, 0);
	ddr_size = 4ULL << 30;

	return ddr_size;
}
#endif

int fsl_initdram(void)
{
#ifdef CONFIG_SYS_DDR_RAW_TIMING
	puts("Initializing DDR....using raw memory timing\n");
	gd->ram_size =  fsl_ddr_sdram();
#else
	puts("Initializeing DDR....using fixed timing\n");
	gd->ram_size = fixed_sdram();
#endif

	return 0;
}
