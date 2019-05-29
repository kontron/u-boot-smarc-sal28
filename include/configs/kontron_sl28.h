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

/******************************************************************************
 * Link Definitions
 */
#define CONFIG_SYS_INIT_SP_ADDR         (CONFIG_SYS_FSL_OCRAM_BASE + 0xeff0)

#define CONFIG_SKIP_LOWLEVEL_INIT

#ifdef CONFIG_CMD_MEMTEST
#define CONFIG_SYS_MEMTEST_START        0x80000000
#define CONFIG_SYS_MEMTEST_END          0x9fffffff
#endif

/******************************************************************************
 * SMP Definitinos
 */
#define CPU_RELEASE_ADDR                secondary_boot_func

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY               25000000        /* 25MHz */

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN           (CONFIG_ENV_SIZE + 2048 * 1024)

/* Early heap for SPL DM */
#define CONFIG_MALLOC_F_ADDR            CONFIG_SYS_FSL_OCRAM_BASE

/* Serial Port */
#define CONFIG_SYS_NS16550_CLK          (get_bus_freq(0) / 2)
#define CONFIG_SYS_BAUDRATE_TABLE       { 9600, 19200, 38400, 57600, 115200 }

/******************************************************************************
 * ENETC
 */
#ifdef CONFIG_FSL_ENETC
#define CFG_ENETC_PHYS_ADDR 0x1f0000000ULL
#define CFG_ENETC_PHYS_SIZE 0x10000000UL
#define CONFIG_FSL_MEMAC

#define CONFIG_HAS_ETH1
#define CONFIG_HAS_ETH2
#endif

/* needed for RGMII phy init */
#define CONFIG_LAST_STAGE_INIT

/******************************************************************************
 * Miscellaneous configurable options
 */
#define CONFIG_MISC_INIT_R

#define CONFIG_SYS_LOAD_ADDR    (CONFIG_SYS_DDR_SDRAM_BASE + 0x10000000)

#ifdef CONFIG_SPL_BUILD
#undef CONFIG_CMD_KBOARDINFO
#undef CONFIG_KBOARDINFO_MODULE
#undef CONFIG_EMB_EEP_I2C_EEPROM
#endif

/******************************************************************************
 * Default Environment Variables
 */

/* Allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_LOADADDR 0x81000000
#define ENV_MEM_LAYOUT_SETTINGS \
	"scriptaddr=0x90000000\0" \
	"pxefile_addr_r=0x90100000\0" \
	"kernel_addr_r=" __stringify(CONFIG_LOADADDR) "\0" \
	"fdt_addr_r=0x83000000\0" \
	"ramdisk_addr_r=0x83100000\0"

#ifdef CONFIG_KONTRON_TEST_EXTENSIONS
#define ENV_KONTRON_UPDATE_EXTENSIONS
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
#define ENV_KONTRON_TEST_EXTENSIONS \
	"update_rcw=setenv autoload no; dhcp " \
		"&& tftp 10.0.1.36:b/sl28/rcw/$rcw_filename " \
		"&& i2c write $fileaddr 50 0.2 $filesize " \
		"&& sf probe 0 && sf update $fileaddr 0 $filesize\0" \
	"update_uboot=setenv autoload no; dhcp && " \
		"tftp 10.0.1.36:b/sl28/latest/u-boot " \
		"&& sf probe 0 && sf update $fileaddr 0x210000 $filesize\0" \
	"update_dp_firmware=setenv autoload no; dhcp " \
		"&& tftp 10.0.1.36:b/sl28/latest/dp-firmware " \
		"&& sf probe 0 && sf update $fileaddr 0x300000 $filesize\0" \
	"update_all=setenv autoload no; dhcp && " \
		"tftp 10.0.1.36:b/sl28/latest/spi-flash.img " \
		"&& sf probe 0 && sf update $fileaddr 0 $filesize\0"
#else
#define BOOTENV_DEV_DAILY(devtypeu, devtypel, instance)
#define BOOTENV_DEV_NAME_DAILY(devtypeu, devtypel, instance)
#define ENV_KONTRON_TEST_EXTENSIONS
#define ENV_KONTRON_UPDATE_EXTENSIONS \
	"update_variant=setenv sl28_variant v${variant}; run update\0" \
	"update_rcw=setenv upd_parts rcw-${variant}; run update\0" \
	"update_uboot=setenv upd_parts u-boot; run update\0" \
	"update_dp_firmware=setenv upd_parts dp-firmware; run update\0" \
	"update_all=setenv upd_parts spi-flash; run update\0" \
	"update=run updUsb || run updSd || run updNet || run updFal\0" \
	"updFal=echo update failed\0" \
	"updUsb=usb start && usb dev 0 && load usb 0:1 ${loadaddr} ${updfile} " \
		"&& source ${loadaddr}:install && true\0" \
	"updSd=mmc dev 0 && load mmc 0:1 ${loadaddr} ${updfile} && "\
		"source ${loadaddr}:install && true\0" \
	"updNet=setenv autoload no && dhcp; if tftp ${loadaddr} ${updserver}${updfile}; " \
		"then source ${loadaddr}:install; else run updFal; fi\0" \
	"updfile=update_sl28.itb\0"
#endif


#ifndef CONFIG_SPL_BUILD
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 1) \
	func(MMC, mmc, 0) \
	func(NVME, nvme, 0) \
	func(USB, usb, 0) \
	func(PXE, pxe, 0) \
	func(DAILY, daily, daily)
#include <config_distro_bootcmd.h>
#else
#define BOOTENV
#endif

#define ENV_KONTRON_LVDS_EXTENSIONS \
	"lvds_single_lane_vesa_24bpp=i2c dev 0; i2c mw 20 81 00\0" \
	"lvds_single_lane_jeida_24bpp=i2c dev 0; i2c mw 20 81 10\0" \
	"lvds_single_lane_18bpp=i2c dev 0; i2c mw 20 81 20\0" \
	"lvds_dual_lane_vesa_24bpp=i2c dev 0; i2c mw 20 81 0b\0" \
	"lvds_dual_lane_jeida_24bpp=i2c dev 0; i2c mw 20 81 1b\0" \
	"lvds_dual_lane_18bpp=i2c dev 0; i2c mw 20 81 2b\0"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0xffffffffffffffff\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"hdp_fw_addr=0x20100000\0" \
	"hdpload=hdp load ${hdp_fw_addr} 0x2000\0" \
	"env_addr=0x203e0004\0" \
	"envload=env import -d -b ${env_addr}\0" \
	ENV_KONTRON_LVDS_EXTENSIONS \
	ENV_KONTRON_UPDATE_EXTENSIONS \
	ENV_KONTRON_TEST_EXTENSIONS \
	ENV_MEM_LAYOUT_SETTINGS \
	BOOTENV

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE               512     /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE               (CONFIG_SYS_CBSIZE + \
                                        sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_BARGSIZE             CONFIG_SYS_CBSIZE /* Boot args buffer */

#define CONFIG_SYS_MAXARGS              64      /* max command args */

#define CONFIG_SYS_BOOTM_LEN   (64 << 20)      /* Increase max gunzip size */
#define CONFIG_CMDLINE_PS_SUPPORT

/******************************************************************************
 * MMC
 */
#ifdef CONFIG_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_MMC_HAS_CAPBLT_VS33
#endif

#ifdef CONFIG_SPL
#define CONFIG_SPL_BSS_START_ADDR      0x80100000
#define CONFIG_SPL_BSS_MAX_SIZE                0x00100000
#define CONFIG_SPL_LDSCRIPT "arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_MAX_SIZE            0x20000
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

/******************************************************************************
 * DDR
 */
#define CONFIG_SYS_DDR_RAW_TIMING
#define CONFIG_DDR_ECC
#define CONFIG_ECC_INIT_VIA_DDRCONTROLLER
#define CONFIG_MEM_INIT_VALUE           0xdeadbeef

#define CONFIG_VERY_BIG_RAM
#define CONFIG_CHIP_SELECTS_PER_CTRL    4
#define CONFIG_DIMM_SLOTS_PER_CTLR      1
#define CONFIG_SYS_SDRAM_SIZE           0x80000000
#define CONFIG_SYS_DDR_SDRAM_BASE       0x80000000UL
#define CONFIG_SYS_FSL_DDR_SDRAM_BASE_PHY       0
#define CONFIG_SYS_SDRAM_BASE           CONFIG_SYS_DDR_SDRAM_BASE
#define CONFIG_SYS_DDR_BLOCK2_BASE      0x2080000000ULL
#define CONFIG_SYS_FSL_DDR_MAIN_NUM_CTRLS       1

/******************************************************************************
 * FlexSPI
 */
#ifdef CONFIG_NXP_FSPI
#define NXP_FSPI_FLASH_SIZE             SZ_4M
#define NXP_FSPI_FLASH_NUM              1
#endif

/******************************************************************************
 * Environment organization
 */

#define CONFIG_SYS_FLASH_BASE           0x20000000
#define CONFIG_ENV_OFFSET               0x3e0000
#define CONFIG_ENV_SIZE                 0x2000
#define CONFIG_ENV_SECT_SIZE            0x10000
#define CONFIG_ENV_ADDR                 (CONFIG_SYS_FLASH_BASE + \
                                         CONFIG_ENV_OFFSET)

#define CONFIG_SYS_REDUNDAND_ENVIRONMENT
#define CONFIG_ENV_OFFSET_REDUND        0x3f0000
#define CONFIG_ENV_SIZE_REDUND          (CONFIG_ENV_SIZE)

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_MONITOR_BASE CONFIG_SPL_TEXT_BASE
#else
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE
#endif

/******************************************************************************
 * SATA
 */
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
