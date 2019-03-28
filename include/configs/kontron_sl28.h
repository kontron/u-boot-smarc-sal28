/*
 * Copyright 2019 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __SL28_H
#define __SL28_H

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
#define CONFIG_MISC_INIT_R

#define CONFIG_SYS_LOAD_ADDR    (CONFIG_SYS_DDR_SDRAM_BASE + 0x10000000)

/* Environment */
/* Allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_LOADADDR 0x81000000
#define ENV_MEM_LAYOUT_SETTINGS \
	"scriptaddr=0x90000000\0" \
	"pxefile_addr_r=0x90100000\0" \
	"kernel_addr_r=" __stringify(CONFIG_LOADADDR) "\0" \
	"fdt_addr_r=0x82000000\0" \
	"ramdisk_addr_r=0x82100000\0"

#define BOOTENV_DEV_DAILY(devtypeu, devtypel, instance) \
	"boot_script_daily=10.0.1.36:s/sl28\0" \
	"bootcmd_daily=" \
		"setenv autoload no; " \
		"dhcp; " \
		"if tftp ${scriptaddr} ${boot_script_daily}; then " \
			"source ${scriptaddr}:" #instance "; " \
		"fi;" \
		"\0"
#define BOOTENV_DEV_NAME_DAILY(devtypeu, devtypel, instance) \
	"daily "

#ifndef CONFIG_SPL_BUILD
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 1) \
	func(MMC, mmc, 0) \
	func(USB, usb, 0) \
	func(DAILY, daily, daily)
#include <config_distro_bootcmd.h>
#else
#define BOOTENV
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
        "bootargs=default_hugepagesz=2m hugepagesz=2m hugepages=256 video=1920x1080-32@60 cma=256M\0" \
        "fdt_high=0xffffffffffffffff\0" \
        "initrd_high=0xffffffffffffffff\0" \
		"set_tftp_rcw_uri=setenv uri 10.0.1.36:b/sl28/rcw/$rcw_filename\0" \
		"set_tftp_uboot_uri=setenv uri 10.0.1.36:b/sl28/u-boot\0" \
		"set_tftp_dp_firmware_uri=setenv uri 10.0.1.36:b/sl28/dp-firmware\0" \
		"set_tftp_spi_flash_img_uri=setenv uri 10.0.1.36:b/sl28/spi-flash.img\0" \
        "update_rcw=setenv autoload no; dhcp && run set_tftp_rcw_uri && tftp $uri " \
                "&& i2c write $fileaddr 50 0.2 $filesize\0" \
        "update_uboot=setenv autoload no; dhcp && run set_tftp_uboot_uri && tftp $uri " \
                "&& sf probe 0 && sf update $fileaddr 0x210000 $filesize\0" \
        "update_dp_firmware=setenv autoload no; dhcp && run set_tftp_dp_firmware_uri && tftp $uri " \
                "&& sf probe 0 && sf update $fileaddr 0x300000 $filesize\0" \
        "update_all=setenv autoload no; dhcp && run set_tftp_spi_flash_img_uri && tftp $uri " \
                "&& sf probe 0 && sf update $fileaddr 0 $filesize\0" \
		ENV_MEM_LAYOUT_SETTINGS \
		BOOTENV

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE               512     /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE               (CONFIG_SYS_CBSIZE + \
                                        sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_BARGSIZE             CONFIG_SYS_CBSIZE /* Boot args buffer */

#define CONFIG_SYS_MAXARGS              64      /* max command args */

#define CONFIG_SYS_BOOTM_LEN   (64 << 20)      /* Increase max gunzip size */

/*  MMC  */
#ifdef CONFIG_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_MMC_HAS_CAPBLT_VS33
#endif

#ifdef CONFIG_SPL
#define CONFIG_SPL_BSS_START_ADDR      0x80100000
#define CONFIG_SPL_BSS_MAX_SIZE                0x00100000
#define CONFIG_SPL_LDSCRIPT "arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_MAX_SIZE            0x16000
#define CONFIG_SPL_STACK               (CONFIG_SYS_FSL_OCRAM_BASE + 0x9ff0)
#define CONFIG_SPL_TARGET              "u-boot-with-spl.bin"
#define CONFIG_SPL_TEXT_BASE           0x18010000

#define CONFIG_SYS_SPL_MALLOC_SIZE     0x00100000
#define CONFIG_SYS_SPL_MALLOC_START    0x80200000
#define CONFIG_SYS_MONITOR_LEN         (1024 * 1024)

#define CONFIG_SPL_SPI_LOAD
#define CONFIG_SYS_SPI_U_BOOT_OFFS	0x26000
#define CONFIG_SYS_SPI_U_BOOT_SIZE	(0x100000 - (CONFIG_SYS_SPI_U_BOOT_OFFS))

#endif

#define CONFIG_SYS_CLK_FREQ             100000000 /* 100 MHz base clock */
#define CONFIG_DDR_CLK_FREQ             100000000 /* 100 MHz base clock */
#define COUNTER_FREQUENCY_REAL          (CONFIG_SYS_CLK_FREQ/4)

/* DDR */
/* #define CONFIG_SYS_DDR_RAW_TIMING */
#define CONFIG_DDR_ECC
#define CONFIG_ECC_INIT_VIA_DDRCONTROLLER
#define CONFIG_MEM_INIT_VALUE           0xdeadbeef

#define CONFIG_VERY_BIG_RAM
#define CONFIG_CHIP_SELECTS_PER_CTRL    4
#define CONFIG_NR_DRAM_BANKS            2
#define CONFIG_DIMM_SLOTS_PER_CTLR      2
#define CONFIG_SYS_SDRAM_SIZE           0x80000000
#define CONFIG_SYS_DDR_SDRAM_BASE       0x80000000UL
#define CONFIG_SYS_FSL_DDR_SDRAM_BASE_PHY       0
#define CONFIG_SYS_SDRAM_BASE           CONFIG_SYS_DDR_SDRAM_BASE
#define CONFIG_SYS_DDR_BLOCK2_BASE      0x2080000000ULL
#define CONFIG_SYS_FSL_DDR_MAIN_NUM_CTRLS       1

/* FlexSPI */
#ifdef CONFIG_NXP_FSPI
#define NXP_FSPI_FLASH_SIZE             SZ_4M
#define NXP_FSPI_FLASH_NUM              1
#endif

/* Store environment at top of flash */
#define CONFIG_SYS_FLASH_BASE           0x20000000
#define CONFIG_ENV_OFFSET               0x3f0000
#define CONFIG_ENV_SIZE                 0x2000
#define CONFIG_ENV_SECT_SIZE            0x10000
#define CONFIG_ENV_ADDR                 (CONFIG_SYS_FLASH_BASE + \
                                         CONFIG_ENV_OFFSET)

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_MONITOR_BASE CONFIG_SPL_TEXT_BASE
#else
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE
#endif

/* SATA */
#define CONFIG_SYS_SCSI_MAX_SCSI_ID             1
#define CONFIG_SYS_SCSI_MAX_LUN                 1
#define CONFIG_SYS_SCSI_MAX_DEVICE              (CONFIG_SYS_SCSI_MAX_SCSI_ID * \
                                                CONFIG_SYS_SCSI_MAX_LUN)
#define SCSI_VEND_ID 0x1b4b
#define SCSI_DEV_ID  0x9170
#define CONFIG_SCSI_DEV_LIST {SCSI_VEND_ID, SCSI_DEV_ID}
#define CONFIG_SCSI_AHCI_PLAT
#define CONFIG_SYS_SATA1                        AHCI_BASE_ADDR1

/* needed for SDHC DMA transfers */
#define CONFIG_TZPC_OCRAM_BSS_HEAP_NS
#define OCRAM_NONSECURE_SIZE            0x00010000

#endif /* __SL28_H */
