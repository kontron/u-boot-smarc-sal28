/*
 * Copyright 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __SL28_H
#define __SL28_H

/* SPL build */
#ifdef CONFIG_SPL_BUILD
#define SPL_NO_FMAN
#define SPL_NO_DSPI
#define SPL_NO_PCIE
#define SPL_NO_ENV
#define SPL_NO_MISC
#define SPL_NO_USB
#define SPL_NO_SATA
#define SPL_NO_QE
#define SPL_NO_EEPROM
#endif
#if (defined(CONFIG_SPL_BUILD) && defined(CONFIG_SD_BOOT_QSPI))
#define SPL_NO_IFC
#endif

#define CONFIG_REMAKE_ELF
#define CONFIG_FSL_LAYERSCAPE
#define CONFIG_MP

#include <asm/arch/stream_id_lsch3.h>
#include <asm/arch/config.h>
#include <asm/arch/soc.h>

/* Link Definitions */
#define CONFIG_SYS_INIT_SP_ADDR         (CONFIG_SYS_FSL_OCRAM_BASE + 0xfff0)

#define CONFIG_SKIP_LOWLEVEL_INIT

#ifdef CONFIG_CMD_MEMTEST
#define CONFIG_SYS_MEMTEST_START        0x80000000
#define CONFIG_SYS_MEMTEST_END          0x9fffffff
#endif

/*
 * SMP Definitinos
 */
#define CPU_RELEASE_ADDR                secondary_boot_func

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY               25000000        /* 25MHz */

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN           (CONFIG_ENV_SIZE + 2048 * 1024)

/* I2C */
#define CONFIG_SYS_I2C

/* Serial Port */
#define CONFIG_CONS_INDEX       1
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE     1
#define CONFIG_SYS_NS16550_CLK          (get_bus_freq(0) / 2)

#define CONFIG_SYS_MAXARGS		64	/* max command args */

#define CONFIG_BAUDRATE                 115200
#define CONFIG_SYS_BAUDRATE_TABLE       { 9600, 19200, 38400, 57600, 115200 }

/* ENETC */
#ifdef CONFIG_FSL_ENETC
#define CFG_ENETC_PHYS_ADDR 0x1f0000000ULL
#define CFG_ENETC_PHYS_SIZE 0x10000000UL
#define CONFIG_FSL_MEMAC
#endif

/* needed for RGMII phy init */
#define CONFIG_LAST_STAGE_INIT

/* Miscellaneous configurable options */
#define CONFIG_SYS_LOAD_ADDR    (CONFIG_SYS_DDR_SDRAM_BASE + 0x10000000)

/* Allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_EXTRA_ENV_SETTINGS               \
	"hwconfig=fsl_ddr:bank_intlv=0\0"       \
	"ethact=enetc#1\0"

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE               512     /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE               (CONFIG_SYS_CBSIZE + \
                                        sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_BARGSIZE             CONFIG_SYS_CBSIZE /* Boot args buffer */

#ifndef SPL_NO_MISC
#ifndef CONFIG_CMDLINE_EDITING
#define CONFIG_CMDLINE_EDITING          1
#endif
#endif

/*  MMC  */
#ifndef SPL_NO_MMC
#ifdef CONFIG_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_MMC_HAS_CAPBLT_VS33
#endif
#endif

#ifdef CONFIG_SPL
#define CONFIG_SPL_BSS_START_ADDR      0x80100000
#define CONFIG_SPL_BSS_MAX_SIZE                0x00100000
#define CONFIG_SPL_LDSCRIPT "arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_MAX_SIZE            0x16000
#define CONFIG_SPL_STACK               (CONFIG_SYS_FSL_OCRAM_BASE + 0x9ff0)
#define CONFIG_SPL_TARGET              "u-boot-with-spl.bin"
#define CONFIG_SPL_TEXT_BASE           0x18010000

#define CONFIG_SPL_I2C_SUPPORT

#define CONFIG_SYS_SPL_MALLOC_SIZE     0x00100000
#define CONFIG_SYS_SPL_MALLOC_START    0x80200000
#define CONFIG_SYS_MONITOR_LEN         (512 * 1024)
#endif

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_MONITOR_BASE CONFIG_SPL_TEXT_BASE
#else
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE
#endif

#define CONFIG_SYS_CLK_FREQ             100000000 /* 100 MHz base clock */
#define CONFIG_DDR_CLK_FREQ             100000000 /* 100 MHz base clock */
#define COUNTER_FREQUENCY_REAL          (CONFIG_SYS_CLK_FREQ/4)

/* DDR */
#define CONFIG_SYS_DDR_RAW_TIMING
#define CONFIG_DDR_ECC
#define CONFIG_ECC_INIT_VIA_DDRCONTROLLER
#define CONFIG_MEM_INIT_VALUE		0xdeadbeef

#define CONFIG_VERY_BIG_RAM
#define CONFIG_CHIP_SELECTS_PER_CTRL    2
#define CONFIG_NR_DRAM_BANKS            2
#define CONFIG_DIMM_SLOTS_PER_CTLR      1
#define CONFIG_SYS_SDRAM_SIZE		0x80000000
#define CONFIG_SYS_DDR_SDRAM_BASE       0x80000000UL
#define CONFIG_SYS_FSL_DDR_SDRAM_BASE_PHY       0
#define CONFIG_SYS_SDRAM_BASE           CONFIG_SYS_DDR_SDRAM_BASE
#define CONFIG_SYS_DDR_BLOCK2_BASE      0x2080000000ULL
#define CONFIG_SYS_FSL_DDR_MAIN_NUM_CTRLS       1

/* FlexSPI */
#ifdef CONFIG_NXP_FSPI
#define NXP_FSPI_FLASH_SIZE		SZ_4M
#define NXP_FSPI_FLASH_NUM		1
#endif

/* Store environment at top of flash */
#define CONFIG_SYS_FLASH_BASE           0x20000000
#define CONFIG_ENV_OFFSET               0x1f0000
#define CONFIG_ENV_SIZE                 0x2000
#define CONFIG_ENV_SECT_SIZE            0x10000
#define CONFIG_ENV_ADDR                 (CONFIG_SYS_FLASH_BASE + \
                                         CONFIG_ENV_OFFSET)

/* XXX remove me, but once removed u-boot runs into an exception */
#define CONFIG_HWCONFIG
#define HWCONFIG_BUFFER_SIZE		128

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_MONITOR_BASE CONFIG_SPL_TEXT_BASE
#else
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE
#endif

/* SATA */
#ifndef SPL_NO_SATA
#ifndef CONFIG_CMD_EXT2
#define CONFIG_CMD_EXT2
#endif
#define CONFIG_SYS_SCSI_MAX_SCSI_ID             1
#define CONFIG_SYS_SCSI_MAX_LUN                 1
#define CONFIG_SYS_SCSI_MAX_DEVICE              (CONFIG_SYS_SCSI_MAX_SCSI_ID * \
                                                CONFIG_SYS_SCSI_MAX_LUN)
#define SCSI_VEND_ID 0x1b4b
#define SCSI_DEV_ID  0x9170
#define CONFIG_SCSI_DEV_LIST {SCSI_VEND_ID, SCSI_DEV_ID}
#define CONFIG_SCSI_AHCI_PLAT
#define CONFIG_SYS_SATA1                        AHCI_BASE_ADDR1
#endif

#endif /* __SL28_H */