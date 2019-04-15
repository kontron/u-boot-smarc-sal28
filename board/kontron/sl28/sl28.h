/*
 * Copyright (C) 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __SL28_H__
#define __SL28_H__

int board_boot_source(void);

#define PORSR1_RCW_SRC      0x07800000
#define PORSR1_RCW_SRC_SDHC 0x04000000
#define PORSR1_RCW_SRC_MMC  0x04800000
#define PORSR1_RCW_SRC_I2C  0x05000000
#define PORSR1_RCW_SRC_FSPI 0x07800000

#endif
