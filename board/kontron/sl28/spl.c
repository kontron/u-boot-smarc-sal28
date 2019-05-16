/*
 * Copyright (C) 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/spl.h>
#include <linux/libfdt.h>
#include "sl28.h"

DECLARE_GLOBAL_DATA_PTR;

void board_boot_order(u32 *spl_boot_list)
{
	u32 rcw_src = board_boot_source();
	unsigned int payload_offs = 0;

	debug("%s: rcw_src=%x\n", __func__, rcw_src);

	switch (rcw_src) {
	case PORSR1_RCW_SRC_SDHC:
		puts("SDHC boot\n");
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		break;
	case PORSR1_RCW_SRC_I2C:
		puts("SPI boot\n");
		payload_offs = 0x230000;
		spl_boot_list[0] = BOOT_DEVICE_SPI;
		break;
	case PORSR1_RCW_SRC_FSPI:
		puts("Failsafe SPI boot\n");
		spl_boot_list[0] = BOOT_DEVICE_SPI;
		break;
	default:
		panic("unknown bootsource (%x)\n", rcw_src);
		break;
	}

	if (payload_offs) {
		void *fdt_blob = (void*)gd->fdt_blob;
		int node;
		debug("setting SPI payload_offs to %x\n", payload_offs);
		payload_offs = cpu_to_be32(payload_offs);
		node = fdt_path_offset(fdt_blob, "/config");
		node = fdt_setprop(fdt_blob, node, "u-boot,spl-payload-offset",
		            &payload_offs, sizeof(payload_offs));
	}
}
