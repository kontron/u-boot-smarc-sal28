/*
 * Copyright 2018 Kontron Europe GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <fsl_ddr.h>
#include <asm/io.h>
#include <hwconfig.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <env_internal.h>
#include <asm/arch-fsl-layerscape/soc.h>
#include <i2c.h>
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

#include "sl28.h"
#include "../common/emb_eep.h"

extern int EMB_EEP_I2C_EEPROM_BUS_NUM_1;

DECLARE_GLOBAL_DATA_PTR;

#define CPLD_I2C_ADDR 0x4a
#define REG_CPLD_VER  0x03

#define GPINFO_HW_VARIANT_MASK 0xff
#define GPINFO_HAS_EC_SGMII    BIT(8)
#define GPINFO_HAS_EC_RGMII    BIT(9)
#define GPINFO_HAS_SW_SGMII0   BIT(10)
#define GPINFO_HAS_SW_SGMII1   BIT(11)

#define DCFG_RCWSR25 0x160
#define DCFG_RCWSR27 0x168
#define DCFG_RCWSR29 0x170
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

/*
 * Kontron supplies a list of pregenerated RCWs for different use cases.
 * Map the rcw configuration back to the naming scheme.
 */
static char *sl28_rcw_filename(char *buf, size_t len)
{
	static const char map[] = "_x_X_q__gG_124s_";
	char postfix[8];
	u32 rcwsr29 = in_le32(DCFG_BASE + DCFG_RCWSR29);
	int variant = sl28_variant();

	postfix[0] = map[(rcwsr29 >> 24) & 0xf];
	postfix[1] = map[(rcwsr29 >> 28) & 0xf];
	postfix[2] = map[(rcwsr29 >> 16) & 0xf];
	postfix[3] = map[(rcwsr29 >> 20) & 0xf];
	postfix[4] = '\0';

	/* mask the PCI lanes which are not available on each variant */
	switch(variant)
	{
	case 2:
		postfix[3] = '_';
		/* fall through */
	case 3:
	case 4:
		postfix[2] = '_';
		break;
	default:
		break;
	}

	snprintf(buf, len,
		 "sl28-%d-%s.bin", variant, postfix);
	buf[len-1] = '\0';

	return buf;
}

int board_init(void)
{
	int cpld_version = sl28_cpld_version();
#ifdef CONFIG_ENV_IS_NOWHERE
	gd->env_addr = (ulong)&default_environment[0];
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

int fsl_board_late_init(void)
{
	char buf[32];
	int cpld_version;

	cpld_version = sl28_cpld_version();
	if (cpld_version >= 0) {
		env_set_ulong("cpld_version", cpld_version);
	} else {
		env_set("cpld_version", NULL);
	}
	env_set_ulong("variant", sl28_variant());
	env_set("rcw_filename", sl28_rcw_filename(buf, sizeof(buf)));

#if defined(CONFIG_KEX_EEP_BOOTCOUNTER)
	emb_eep_update_bootcounter(1);
#endif

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
#ifdef CONFIG_FSL_ENETC
extern void enetc_setup(void *blob);
#endif
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

#ifdef CONFIG_FSL_ENETC
	enetc_setup(blob);
#endif
	return 0;
}
#endif

#define NETC_PF0_BAR0_BASE	0x1f8010000UL
#define NETC_PF0_ECAM_BASE	0x1f0000000UL
#define NETC_PF1_BAR0_BASE	0x1f8050000UL
#define NETC_PF1_ECAM_BASE	0x1f0001000UL
#define NETC_PF5_BAR0_BASE	0x1f8140000UL
#define NETC_PF2_ECAM_BASE	0x1f0002000UL
#define NETC_PF5_ECAM_BASE	0x1f0005000UL
#define NETC_PCS_QSGMIICR1	0x001ea1884UL
#define NETC_PCS_SGMIICR1(n)	(0x001ea1804UL + (n) * 0x10)
extern void enetc_imdio_write(struct mii_dev *bus, int port, int dev, int reg, u16 val);
extern int enetc_imdio_read(struct mii_dev *bus, int port, int dev, int reg);

static void setup_ec_rgmii(void)
{
	struct mii_dev *ext_bus;
	char *mdio_name = "netc_mdio";
	int phy_addr = 4;
	int value;

	/* turn on PCI function */
	out_le16(NETC_PF1_ECAM_BASE + 4, 0xffff);
	out_le32(NETC_PF1_BAR0_BASE + 0x8300, 0x8006);

	/* configure AQR PHY */
	ext_bus = miiphy_get_dev_by_name(mdio_name);
	if (!ext_bus) {
		printf("couldn't find MDIO bus, ignoring the PHY\n");
		return;
	}
	/* Atheros magic */
	/* MMD7 0x8016 */
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x0d, 0x0007);
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x0e, 0x8016);
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x0d, 0x4007);
	value = ext_bus->read(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x0e);
	if (value == 0xffff)
		goto phy_err;
	value |= 0x0018;
	value &= ~0x0180;
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x0e, value);

	/* rgmii tx clock delay */
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x1d, 0x0005);
	value = ext_bus->read(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x1e);
	if (value == 0xffff)
		goto phy_err;
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x1e, value | 0x0100);

	/* PHC control debug register 0*/
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x1d, 0x001f);
	value = ext_bus->read(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x1e);
	if (value == 0xffff)
		goto phy_err;
	value |= 0x0008; /* set RGMII IO to 1V8 */
	value |= 0x0004; /* enable PLL */
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x1e, value);

	/* restart AN */
	ext_bus->write(ext_bus, phy_addr, MDIO_DEVAD_NONE, 0x0d, 0x1200);

	return;
phy_err:
	printf("RGMII PHY access error, giving up.\n");
}

static void setup_sgmii_common(struct mii_dev *bus, int port)
{
	u32 tmp;
	u16 value;
	int to;

	tmp = in_le32(NETC_PCS_SGMIICR1(port));
	tmp &= ~0xf8000000;
	tmp |= port << 27;
	out_le32(NETC_PCS_SGMIICR1(port), tmp);

	value = PHY_SGMII_IF_MODE_SGMII | PHY_SGMII_IF_MODE_AN;
	enetc_imdio_write(bus, port, MDIO_DEVAD_NONE, 0x14, value);
	/* Dev ability according to SGMII specification */
	value = PHY_SGMII_DEV_ABILITY_SGMII;
	enetc_imdio_write(bus, port, MDIO_DEVAD_NONE, 0x04, value);
	/* Adjust link timer for SGMII */
	enetc_imdio_write(bus, port, MDIO_DEVAD_NONE, 0x13, 0x0003);
	enetc_imdio_write(bus, port, MDIO_DEVAD_NONE, 0x12, 0x06a0);

	/* restart AN */
	value = PHY_SGMII_CR_DEF_VAL | PHY_SGMII_CR_RESET_AN;
	enetc_imdio_write(bus, port, MDIO_DEVAD_NONE, 0x00, value);

	/* wait for link */
	to = 1000;
	do {
		value = enetc_imdio_read(bus, port, MDIO_DEVAD_NONE, 0x01);
		if ((value & 0x0024) == 0x0024)
			break;
	} while (--to);
	if ((value & 0x0024) != 0x0024)
		printf("PCS[%d] didn't link up, giving up. (%04x)\n", port,value);
}

static void setup_ec_sgmii(void)
{
	struct mii_dev bus = {0};
	bus.priv = (void *)NETC_PF0_BAR0_BASE + 0x8030;

	/* turn on PCI function */
	out_le16(NETC_PF0_ECAM_BASE + 4, 0xffff);

	setup_sgmii_common(&bus, 0);
}

static void setup_sw_sgmii(int n)
{
	struct mii_dev bus = {0};
	bus.priv = (void *)NETC_PF5_BAR0_BASE + 0x8030;

	/* turn on PCI function */
	out_le16(NETC_PF2_ECAM_BASE + 4, 0xffff);
	out_le16(NETC_PF5_ECAM_BASE + 4, 0xffff);

	setup_sgmii_common(&bus, n);
}

void setup_qsgmii(void)
{
	struct mii_dev bus = {0};
	u16 value;
	int i, to;

	//out_le32(NETC_PCS_QSGMIICR1, 0x20000000);

	/* turn on PCI functions */
	out_le16(NETC_PF2_ECAM_BASE + 4, 0xffff);
	out_le16(NETC_PF5_ECAM_BASE + 4, 0xffff);

	bus.priv = (void *)NETC_PF5_BAR0_BASE + 0x8030;

	for (i = 0; i < 4; i++) {
		value = PHY_SGMII_IF_MODE_SGMII | PHY_SGMII_IF_MODE_AN;
		enetc_imdio_write(&bus, i, MDIO_DEVAD_NONE, 0x14, value);
		/* Dev ability according to SGMII specification */
		value = PHY_SGMII_DEV_ABILITY_SGMII;
		enetc_imdio_write(&bus, i, MDIO_DEVAD_NONE, 0x04, value);
		/* Adjust link timer for SGMII */
		enetc_imdio_write(&bus, i, MDIO_DEVAD_NONE, 0x13, 0x0003);
		enetc_imdio_write(&bus, i, MDIO_DEVAD_NONE, 0x12, 0x06a0);
	}

#if 0
	int phy_addr;
	char *mdio_name;
	mdio_name = "netc_mdio";
	phy_addr = 0x10;
	struct mii_dev *ext_bus;

	/* set up VSC PHY - this works on RDB only for now*/
	ext_bus = miiphy_get_dev_by_name(mdio_name);
	if (!ext_bus) {
		PCS_ERR("couldn't find MDIO bus, skipping external PHY config\n");
		return;
	}

	for (i = phy_addr; i < phy_addr + 4; i++) {
		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x1f, 0x0010);
		value = ext_bus->read(ext_bus, i, MDIO_DEVAD_NONE, 0x13);
		value = (value & 0x3fff) | (1 << 14);
		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x12, 0x80e0);

		to = 1000;

		do {
			value = ext_bus->read(ext_bus, i, MDIO_DEVAD_NONE, 0x12);
			if (!(value & 0x8000))
				break;
		} while (--to);
		if (value & 0x8000)
			PCS_ERR("PHY[%d] reset timeout\n", i);


		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x1f, 0x0000);
		value = ext_bus->read(ext_bus, i, MDIO_DEVAD_NONE, 0x17);
		value = (value & 0xf8ff);
		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x17, value);

		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x1f, 0x0003);
		value = ext_bus->read(ext_bus, i, MDIO_DEVAD_NONE, 0x10);
		value = value | 0x80;
		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x10, value);
		ext_bus->write(ext_bus, i, MDIO_DEVAD_NONE, 0x1f, 0x0000);
		ext_bus->write(ext_bus, 1, MDIO_DEVAD_NONE, 0x00, 0x3300);
	}
#endif

	for (i = 0; i < 4; i++) {
		to = 1000;
		do {
			value = enetc_imdio_read(&bus, i, MDIO_DEVAD_NONE, 1);
			if ((value & 0x0024) == 0x0024)
				break;
		} while (--to);
		debug("BMSR: %04x\n", value);
		if ((value & 0x24) != 0x24) {
			debug("PCS[%d] didn't link up, giving up.\n", i);
			break;
		}
	}
}

int misc_init_r(void)
{
	debug("%s()\n", __func__);

#ifdef CONFIG_EMB_EEP_I2C_EEPROM
	EMB_EEP_I2C_EEPROM_BUS_NUM_1 = CONFIG_EMB_EEP_I2C_EEPROM_BUS_NUM_EE1;
	emb_eep_init_r(1, 1, 4);
#endif

	return 0;
}

int last_stage_init(void)
{
	int i;

	if (sl28_has_ec_rgmii())
		setup_ec_rgmii();

	if (sl28_has_ec_sgmii())
		setup_ec_sgmii();

	for (i = 0; i < 4; i++) {
		if (sl28_has_sw_sgmii(i))
			setup_sw_sgmii(i);
	}

	if (sl28_has_qsgmii())
		setup_qsgmii();

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
	bool has_internal_switch = false;
	int i;
	int config_node;
	u32 rcw_src = board_boot_source();

	debug("%s\n", __func__);

	fdt_increase_size(rw_fdt_blob, 32);

	if (rcw_src != PORSR1_RCW_SRC_I2C) {
		config_node = fdt_path_offset(rw_fdt_blob, "/config");
		fdt_setprop_u32(rw_fdt_blob, config_node,
		                "load-environment", 0);
	}

	if (!sl28_has_ec_sgmii()) {
		debug("%s: disabling eth_p0\n", __func__);
		fdt_status_disabled_by_alias(rw_fdt_blob, "eth_p0");
	}

	if (!sl28_has_ec_rgmii()) {
		debug("%s: disabling eth_p1\n", __func__);
		fdt_status_disabled_by_alias(rw_fdt_blob, "eth_p1");
	}

	for (i = 0; i < 4; i++)
		if (sl28_has_sw_sgmii(i)) {
			has_internal_switch = true;
			break;
		}

	if (sl28_has_qsgmii())
		has_internal_switch = true;

	if (has_internal_switch) {
		debug("%s: enabling eth_p2\n", __func__);
		fdt_status_okay_by_alias(rw_fdt_blob, "eth_p2");
	}

	return 0;
}

#if defined(CONFIG_CMD_KBOARDINFO)
char *getSerNo(void)
{
	struct env_entry e;
	static struct env_entry *ep;

	e.key = "serial#";
	e.data = NULL;
	hsearch_r (e, ENV_FIND, &ep, &env_htab, 0);
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
        if (macaddress != NULL)
                return (macaddress);
        else
                return "na";
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
