/*
 * Copyright 2018 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <fs.h>
#include <fsl_ddr.h>
#include <asm/io.h>
#include <hwconfig.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <environment.h>
#include <asm/arch-fsl-layerscape/soc.h>
#include <i2c.h>
#include <wdt.h>
#include <asm/arch/soc.h>
#ifdef CONFIG_FSL_LS_PPA
#include <asm/arch/ppa.h>
#endif
#include <fsl_immap.h>
#include <netdev.h>
#include <video_fb.h>

#include <fdtdec.h>
#include <miiphy.h>
#include <fsl_memac.h>
#include <fsl_sec.h>

#include "sl28.h"
#include "../common/emb_eep.h"

extern int EMB_EEP_I2C_EEPROM_BUS_NUM_1;

DECLARE_GLOBAL_DATA_PTR;

#define SL28_MAX_ETHNUM 0

#define CPLD_I2C_ADDR 0x4a
#define REG_CPLD_VER  0x03
#define REG_BRD_CTRL  0x08
#define BRD_CTRL_BOOT_SEL_MASK 0x70
#define BRD_CTRL_BOOT_SEL_SHIFT 4

#define GPINFO_HW_VARIANT_MASK 0xff
#define GPINFO_HAS_EC_SGMII    BIT(8)
#define GPINFO_HAS_EC_RGMII    BIT(9)
#define GPINFO_HAS_SW_SGMII0   BIT(10)
#define GPINFO_HAS_SW_SGMII1   BIT(11)

#define DCFG_RCWSR25 0x160
#define DCFG_RCWSR27 0x168
#define DCFG_RCWSR29 0x170
#define DCFG_SCRATCHWR2 0x204
static int sl28_variant(void)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);

	debug("%s: rcwsr25=%08x\n", __func__, rcwsr25);
	return rcwsr25 & GPINFO_HW_VARIANT_MASK;
}

static bool sl28_has_ec_rgmii(void)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);
	u32 rcwsr27 = in_le32(DCFG_BASE + DCFG_RCWSR27);

	debug("%s: rcwsr25=%08x rcwsr27=%08x\n", __func__, rcwsr25, rcwsr27);

	if (!(rcwsr25 & GPINFO_HAS_EC_RGMII))
		return false;

	if ((rcwsr27 & 0x3000007e) != 0)
		return false;

	return true;
}

static bool sl28_has_ec_sgmii(void)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);
	u32 rcwsr29 = in_le32(DCFG_BASE + DCFG_RCWSR29);

	debug("%s: rcwsr25=%08x rcwsr29=%08x\n", __func__, rcwsr25, rcwsr29);

	if (!(rcwsr25 & GPINFO_HAS_EC_SGMII))
		return false;

	if ((rcwsr29 & 0x000f0000) != 0x00080000)
		return false;

	return true;
}

static bool sl28_has_sw_sgmii(int n)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);
	u32 rcwsr29 = in_le32(DCFG_BASE + DCFG_RCWSR29);

	debug("%s: rcwsr25=%08x rcwsr29=%08x\n", __func__, rcwsr25, rcwsr29);

	if (n >= 2)
		return false;

	if (n == 0 && !(rcwsr25 & GPINFO_HAS_SW_SGMII0))
		return false;

	if (n == 1 && !(rcwsr25 & GPINFO_HAS_SW_SGMII1))
		return false;

	if (n == 0 && (rcwsr29 & 0x000f0000) != 0x00090000)
		return false;

	if (n == 1 && (rcwsr29 & 0x00f00000) != 0x00900000)
		return false;

	return true;
}

static bool sl28_has_qsgmii(void)
{
	u32 rcwsr29 = in_le32(DCFG_BASE + DCFG_RCWSR29);

	debug("%s: rcwsr29=%08x\n", __func__, rcwsr29);

	if ((rcwsr29 & 0x00f00000) != 0x00500000)
		return false;

	return true;
}

static bool sl28_has_internal_switch(void)
{
	int i;

	for (i = 0; i < 4; i++)
		if (sl28_has_sw_sgmii(i))
			return true;

	return false;
}


static int sl28_cpld_version(void)
{
	struct udevice *dev;
	static int version = -1;

	if (version >= 0)
		return version;

	if (i2c_get_chip_for_busnum(0, CPLD_I2C_ADDR, 1, &dev))
		return -1;

	version = dm_i2c_reg_read(dev, REG_CPLD_VER);

	return version;
}

static int sl28_get_boot_selection(void)
{
	struct udevice *dev;
	int sel;

	if (i2c_get_chip_for_busnum(0, CPLD_I2C_ADDR, 1, &dev))
		return -1;

	sel = dm_i2c_reg_read(dev, REG_BRD_CTRL);
	sel &= BRD_CTRL_BOOT_SEL_MASK;
	sel >>= BRD_CTRL_BOOT_SEL_SHIFT;

	debug("%s: boot_sel=%d\n", __func__, sel);

	return sel;
}

/*
 * Kontron supplies a list of pregenerated RCWs for different use cases.
 * Map the rcw configuration back to the naming scheme.
 */
static char *sl28_rcw_filename(char *buf, size_t len)
{
	u64 scratchwr2 = in_le32(DCFG_BASE + DCFG_SCRATCHWR2);

	strncpy(buf, (char*)scratchwr2, len);
	buf[len-1] = '\0';

	return buf;
}

static void sl28_stop_or_kick_failsafe_watchdog(void)
{
	int ret;
	struct udevice *dev;
	char *kick = env_get("kick_failsafe_watchdog");

	ret = uclass_get_device_by_seq(UCLASS_WDT, 2, &dev);
	if (ret)
		return;

	if (kick)
		wdt_reset(dev);
	else
		wdt_stop(dev);
}

int board_init(void)
{
	int cpld_version = sl28_cpld_version();
#ifdef CONFIG_ENV_IS_NOWHERE
	gd->env_addr = (ulong)&default_environment[0];
#endif

#ifdef CONFIG_FSL_CAAM
	sec_init();
#endif

#ifdef CONFIG_FSL_LS_PPA
	ppa_init();
#endif

	/* run PCI init to kick off ENETC */
	pci_init();

	if (cpld_version >= 0)
		printf("CPLD:  v%d\n", cpld_version);
	else
		printf("CPLD:  n/a\n");

	return 0;
}

int checkboard(void)
{
	static const char *variants[] = {
		"unknown",
		"4 Lane",
		"TSN-on-module",
		"Single PHY",
		"Dual PHY"
	};
	char buf[32];
	int variant = sl28_variant();
	const char *variant_name;

	if (variant >= 0 && variant < ARRAY_SIZE(variants))
		variant_name = variants[variant];
	else
		variant_name = variants[0];

	printf("       Hardware Variant: %s (%d)\n"
	       "       RCW: %s\n",
		   variant_name, variant, sl28_rcw_filename(buf, sizeof(buf)));

	return 0;
}

static void sl28_set_prompt(void)
{
	enum boot_source src = sl28_boot_source();

	switch (src) {
	case BOOT_SOURCE_FSPI:
		env_set("PS1", "[FAILSAFE] => ");
		break;
	case BOOT_SOURCE_SDHC:
		env_set("PS1", "[SDHC] => ");
		break;
	default:
		env_set("PS1", NULL);
	}
}

#define BOOT_SEL_SATA     0
#define BOOT_SEL_SD_CARD  1
#define BOOT_SEL_FSPI_CS1 2
#define BOOT_SEL_DSPI_CS0 3
#define BOOT_SEL_USB      4
#define BOOT_SEL_PXE      5
#define BOOT_SEL_EMMC     6
#define BOOT_SEL_ANY      7

static void sl28_evaluate_bootsel_pins(void)
{
	int boot_sel = sl28_get_boot_selection();

	switch (boot_sel) {
	case BOOT_SEL_SD_CARD:
		env_set("boot_targets", "mmc0");
		break;
	case BOOT_SEL_USB:
		env_set("boot_targets", "usb0");
		break;
	case BOOT_SEL_PXE:
		env_set("boot_targets", "pxe");
		break;
	case BOOT_SEL_EMMC:
		env_set("boot_targets", "mmc1");
		break;
	case BOOT_SEL_ANY:
		/* do nothing, leave it up to the user */
		break;
	case BOOT_SEL_SATA:
	case BOOT_SEL_FSPI_CS1:
	case BOOT_SEL_DSPI_CS0:
	default:
		printf("Unsupported boot source (boot_sel was %d). Dropping to shell.",
				boot_sel);
		env_set("bootcmd", NULL);
		break;
	}
}

static void sl28_stop_in_failsafe_or_test_mode(void)
{
	enum boot_source src = sl28_boot_source();

	switch (src) {
	case BOOT_SOURCE_FSPI:
	case BOOT_SOURCE_SDHC:
		env_set("bootcmd", NULL);
		break;
	default:
		/* do nothing */
		break;
	}
}

static void sl28_fixup_failsafe_environment(void)
{
	if (sl28_boot_source() != BOOT_SOURCE_FSPI)
		return;

	/* hdp load takes a decimal representation or hexadecimal depending the
	 * the prefix. Unfortunately, env_set_hex() doesn't prepend the prefix.
	 * So we are using the decimal representation here. */
	env_set_ulong("hdp_fw_addr", HDP_FW_ADDR - 0x200000);
}

static void sl28_load_env_script(void)
{
	loff_t size;
	const char *filename = "u-boot-env.txt";
	int bufsize = 16 * 1024;
	char *buf = malloc(bufsize);

	/* this is only for production purposes */
	if (sl28_boot_source() != BOOT_SOURCE_SDHC)
		goto out;

	if (fs_set_blk_dev("mmc", "0:1", FS_TYPE_ANY))
		goto out;

	if (!fs_exists(filename))
		goto out;

	if (fs_set_blk_dev("mmc", "0:1", FS_TYPE_ANY))
		goto out;

	if (fs_read(filename, (ulong)buf, 0, bufsize, &size))
		goto out;

	if (!himport_r(&env_htab, buf, size, '\n', H_NOCLEAR, 1, 0, NULL))
		goto out;

	printf("Imported additional environment from %s\n", filename);

out:
	free(buf);
}

int armv8_ls_board_late_init(void);
int board_late_init(void)
{
	char buf[32];
	int cpld_version;

	armv8_ls_board_late_init();

	cpld_version = sl28_cpld_version();
	if (cpld_version >= 0) {
		env_set_ulong("cpld_version", cpld_version);
	} else {
		env_set("cpld_version", NULL);
	}
	env_set_ulong("variant", sl28_variant());
	env_set("rcw_filename", sl28_rcw_filename(buf, sizeof(buf)));
	env_set_ulong("bootsource", sl28_boot_source());
	env_set_ulong("bootsel", sl28_get_boot_selection());

	sl28_set_prompt();
	sl28_fixup_failsafe_environment();

	/* the following order is important! */
	sl28_evaluate_bootsel_pins();
	sl28_stop_in_failsafe_or_test_mode();
	sl28_load_env_script();

#if defined(CONFIG_KEX_EEP_BOOTCOUNTER)
	emb_eep_update_bootcounter(1);
#endif

	/* keep this at the lastest point in time */
	sl28_stop_or_kick_failsafe_watchdog();

	return 0;
}

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}

void detail_board_ddr_info(void)
{
	puts("\nDDR    ");
	print_size(gd->bd->bi_dram[0].size + gd->bd->bi_dram[1].size, "");
	print_ddr_info(0);
}

#ifdef CONFIG_OF_BOARD_SETUP
/*
 * Hardware default stream IDs are 0x4000 + PCI function #, but that's outside
 * the acceptable range for SMMU.  Use Linux DT values instead or at least
 * smaller defaults.
 */
#define ECAM_NUM_PFS			7
#define ECAM_IERB_BASE			0x1F0800000
#define ECAM_PFAMQ(pf, vf)		((ECAM_IERB_BASE + 0x800 + (pf) * \
					  0x1000 + (vf) * 4))
/* cache related transaction attributes for PCIe functions */
#define ECAM_IERB_MSICAR		(ECAM_IERB_BASE + 0xa400)
#define ECAM_IERB_MSICAR_VALUE		0x30

/* number of VFs per PF, VFs have their own AMQ settings */
static const u8 enetc_vfs[ECAM_NUM_PFS] = { 2, 2 };

void setup_ecam_amq(void *blob)
{
	int streamid, sid_base, off;
	int pf, vf, vfnn = 1;
	u32 iommu_map[4];
	int err;

	/*
	 * Look up the stream ID settings in the DT, if found apply the values
	 * to HW, otherwise use HW values shifted down by 4.
	 */
	off = fdt_node_offset_by_compatible(blob, 0, "pci-host-ecam-generic");
	if (off < 0) {
		debug("ECAM node not found\n");
		return;
	}

	err = fdtdec_get_int_array(blob, off, "iommu-map", iommu_map, 4);
	if (err) {
		sid_base = in_le32(ECAM_PFAMQ(0, 0)) >> 4;
		debug("\"iommu-map\" not found, using default SID base %04x\n",
		      sid_base);
	} else {
		sid_base = iommu_map[2];
	}
	/* set up AMQs for all integrated PCI functions */
	for (pf = 0; pf < ECAM_NUM_PFS; pf++) {
		streamid = sid_base + pf;
		out_le32(ECAM_PFAMQ(pf, 0), streamid);

		/* set up AMQs for VFs, if any */
		for (vf = 0; vf < enetc_vfs[pf]; vf++, vfnn++) {
			streamid = sid_base + ECAM_NUM_PFS + vfnn;
			out_le32(ECAM_PFAMQ(pf, vf + 1), streamid);
		}
	}
}

void setup_ecam_cacheattr(void)
{
	/* set MSI cache attributes */
	out_le32(ECAM_IERB_MSICAR, ECAM_IERB_MSICAR_VALUE);
}

#define IERB_PFMAC(pf, vf, n)		(ECAM_IERB_BASE + 0x8000 + (pf) * \
					 0x100 + (vf) * 8 + (n) * 4)

static int ierb_fno_to_pf[] = {0, 1, 2, -1, -1, -1, 3};

/* ENETC Port MAC address registers, accepts big-endian format */
static void ierb_set_mac_addr(int fno, const u8 *addr)
{
	u16 lower = *(const u16 *)(addr + 4);
	u32 upper = *(const u32 *)addr;

	if (ierb_fno_to_pf[fno] < 0)
		return;

	out_le32(IERB_PFMAC(ierb_fno_to_pf[fno], 0, 0), upper);
	out_le32(IERB_PFMAC(ierb_fno_to_pf[fno], 0, 1), (u32)lower);
}

/* copies MAC addresses in use to IERB so Linux can also use them */
void setup_mac_addr(void *blob)
{
	struct eth_pdata *plat;
	struct udevice *it;
	struct uclass *uc;
	int fno, offset;
	u32 portno;
	char path[256];

	uclass_get(UCLASS_ETH, &uc);
	uclass_foreach_dev(it, uc) {
		if (!it->driver || !it->driver->name)
			continue;
		if (!strcmp(it->driver->name, "enetc_eth")) {
			/* PFs use the same addresses in Linux and U-Boot */
			plat = dev_get_platdata(it);
			if (!plat)
				continue;

			fno = PCI_FUNC(pci_get_devfn(it));
			ierb_set_mac_addr(fno, plat->enetaddr);
		} else if (!strcmp(it->driver->name, "felix-port")) {
			/* Switch ports should also use the same addresses */
			plat = dev_get_platdata(it);
			if (!plat)
				continue;
			if (!ofnode_valid(it->node))
				continue;
			if (ofnode_read_u32(it->node, "reg", &portno))
				continue;
			sprintf(path,
				"/soc/pcie@1f0000000/switch@0,5/ports/port@%d",
				(int)portno);
			offset = fdt_path_offset(blob, path);
			if (offset < 0)
				continue;
			fdt_setprop(blob, offset, "mac-address",
				    plat->enetaddr, 6);
		}
	}
}

int ft_board_setup(void *blob, bd_t *bd)
{
	u64 base[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];

	ft_cpu_setup(blob, bd);

	/* fixup DT for the two GPP DDR banks */
	base[0] = gd->bd->bi_dram[0].start;
	size[0] = gd->bd->bi_dram[0].size;
	base[1] = gd->bd->bi_dram[1].start;
	size[1] = gd->bd->bi_dram[1].size;

#ifdef CONFIG_RESV_RAM
	/* reduce size if reserved memory is within this bank */
	if (gd->arch.resv_ram >= base[0] &&
	    gd->arch.resv_ram < base[0] + size[0])
		size[0] = gd->arch.resv_ram - base[0];
	else if (gd->arch.resv_ram >= base[1] &&
		 gd->arch.resv_ram < base[1] + size[1])
		size[1] = gd->arch.resv_ram - base[1];
#endif

	fdt_fixup_memory_banks(blob, base, size, 2);

	fdt_fixup_icid(blob);

	setup_ecam_amq(blob);
	setup_ecam_cacheattr();
	setup_mac_addr(blob);

	return 0;
}
#endif

int misc_init_r(void)
{
	debug("%s()\n", __func__);

#ifdef CONFIG_EMB_EEP_I2C_EEPROM
	EMB_EEP_I2C_EEPROM_BUS_NUM_1 = CONFIG_EMB_EEP_I2C_EEPROM_BUS_NUM_EE1;
	emb_eep_init_r(1, SL28_MAX_ETHNUM);
#endif

	return 0;
}

static int sl28_rgmii_phy_config(struct phy_device *phydev)
{
	unsigned short val;

	/* To enable AR8031 ouput a 125MHz clk from CLK_25M */
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x7);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, 0x8016);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x4007);

	val = phy_read(phydev, MDIO_DEVAD_NONE, 0xe);
	val &= 0xffe3;
	val |= 0x18;
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, val);

	/* introduce tx clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x5);
	val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1e);
	val |= 0x0100;
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, val);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	if (!phydev)
		return 0;

	if (sl28_has_ec_rgmii() && phydev->addr == 4)
		sl28_rgmii_phy_config(phydev);

	if (phydev->drv && phydev->drv->config)
		return phydev->drv->config(phydev);

	return 0;
}

#if defined(CONFIG_VIDEO)
void *video_hw_init(void)
{
	return NULL;
}
#endif


int board_fix_fdt(void *rw_fdt_blob)
{
	int config_node;
	enum boot_source src = sl28_boot_source();

	debug("%s\n", __func__);

	fdt_increase_size(rw_fdt_blob, 32);

	if (src == BOOT_SOURCE_FSPI || src == BOOT_SOURCE_SDHC) {
		config_node = fdt_path_offset(rw_fdt_blob, "/config");
		fdt_setprop_u32(rw_fdt_blob, config_node,
		                "load-environment", 0);
	}

	if (!sl28_has_ec_sgmii()) {
		debug("%s: disabling fixup0\n", __func__);
		fdt_status_disabled_by_alias(rw_fdt_blob, "fixup0");
	}

	if (!sl28_has_ec_rgmii()) {
		debug("%s: disabling fixup1\n", __func__);
		fdt_status_disabled_by_alias(rw_fdt_blob, "fixup1");
	}

	if (sl28_has_internal_switch()) {
		debug("%s: enabling fixup2\n", __func__);
		fdt_status_okay_by_alias(rw_fdt_blob, "fixup2");
		debug("%s: enabling fixup3\n", __func__);
		fdt_status_okay_by_alias(rw_fdt_blob, "fixup3");
	}

	return 0;
}

#if defined(CONFIG_CMD_KBOARDINFO)
char *getSerNo(void)
{
	ENTRY e;
	static ENTRY *ep;

	e.key = "serial#";
	e.data = NULL;
	hsearch_r (e, FIND, &ep, &env_htab, 0);
	if (ep == NULL)
		return "na";
	else
		return ep->data;
}

char *getSapId(int eeprom_num)
{
	return (emb_eep_find_string_in_dmi(eeprom_num, 2, 5));
}

char *getManufacturer(int eeprom_num)
{
	return (emb_eep_find_string_in_dmi(eeprom_num, 2, 1));
}

char *getProductName(int eeprom_num)
{
	return (emb_eep_find_string_in_dmi(eeprom_num, 2, 2));
}

char *getManufacturerDate (int eeprom_num)
{
        return (emb_eep_find_string_in_dmi(eeprom_num, 160, 2));
}

char *getRevision (int eeprom_num)
{
        return (emb_eep_find_string_in_dmi(eeprom_num, 2, 3));
}

char *getMacAddress (int eeprom_num, int eth_num)
{
	char *macaddress;

	macaddress = emb_eep_find_mac_in_dmi(eeprom_num, eth_num);
	if (SL28_MAX_ETHNUM == 0)
		return macaddress;

	if (eth_num > SL28_MAX_ETHNUM)
		return NULL;

	return (macaddress ? macaddress : "na");
}

uint64_t getBootCounter (int eeprom_num)
{
	uint64_t bc;
	char *tmp;

	tmp = emb_eep_find_string_in_dmi(eeprom_num, 161, 1);
	if (tmp != NULL) {
		memcpy (&bc, tmp, sizeof(uint64_t));
		return (be64_to_cpu(bc));
	} else
		return (-1ULL);
}

#endif
