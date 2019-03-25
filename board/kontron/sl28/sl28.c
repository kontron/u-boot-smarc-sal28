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

#include "../common/emb_eep.h"

DECLARE_GLOBAL_DATA_PTR;

#define GPINFO_HW_VARIANT_MASK 0xff
#define GPINFO_HAS_SGMII       BIT(8)
#define GPINFO_HAS_RGMII       BIT(9)

#define DCFG_RCWSR25 0x160
#define DCFG_RCWSR27 0x168
#define DCFG_RCWSR29 0x170
static int sl28_variant(void)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);

	debug("%s: rcwsr25=%08x\n", __func__, rcwsr25);
	return rcwsr25 & GPINFO_HW_VARIANT_MASK;
}

static bool sl28_has_rgmii(void)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);
	u32 rcwsr27 = in_le32(DCFG_BASE + DCFG_RCWSR27);

	debug("%s: rcwsr25=%08x rcwsr27=%08x\n", __func__, rcwsr25, rcwsr27);

	if (!(rcwsr25 & GPINFO_HAS_RGMII))
		return 0;

	if ((rcwsr27 & 0x3000007e) != 0)
		return 0;

	return 1;
}

static bool sl28_has_sgmii(void)
{
	u32 rcwsr25 = in_le32(DCFG_BASE + DCFG_RCWSR25);
	u32 rcwsr29 = in_le32(DCFG_BASE + DCFG_RCWSR29);

	debug("%s: rcwsr25=%08x rcwsr29=%08x\n", __func__, rcwsr25, rcwsr29);

	if (!(rcwsr25 & GPINFO_HAS_SGMII))
		return 0;

	if ((rcwsr29 & 0x000f0000) != 0x00080000)
		return 0;

	return 1;
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
#ifdef CONFIG_ENV_IS_NOWHERE
	gd->env_addr = (ulong)&default_environment[0];
#endif

#ifdef CONFIG_FSL_LS_PPA
	ppa_init();
#endif

	/* run PCI init to kick off ENETC */
	pci_init();

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

	env_set_ulong("variant", sl28_variant());
	env_set("rcw_filename", sl28_rcw_filename(buf, sizeof(buf)));

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

void setup_rgmii(void)
{
	#define NETC_PF1_BAR0_BASE	0x1f8050000
	#define NETC_PF1_ECAM_BASE	0x1F0001000
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

extern void enetc_imdio_write(struct mii_dev *bus, int port, int dev, int reg, u16 val);
extern int enetc_imdio_read(struct mii_dev *bus, int port, int dev, int reg);
static void setup_sgmii(void)
{
	#define NETC_PF0_BAR0_BASE	0x1f8010000
	#define NETC_PF0_ECAM_BASE	0x1f0000000
	//#define NETC_PCS_SGMIICR1(n)	(0x001ea1804 + (n) * 0x10)
	struct mii_dev bus = {0};
	u16 value;
	int to;

	//out_le32(NETC_PCS_SGMIICR1(0), 0x00000000);
	// writing this kills the link for some reason

	/* turn on PCI function */
	out_le16(NETC_PF0_ECAM_BASE + 4, 0xffff);

	bus.priv = (void *)NETC_PF0_BAR0_BASE + 0x8030;
	value = PHY_SGMII_IF_MODE_SGMII | PHY_SGMII_IF_MODE_AN;
	enetc_imdio_write(&bus, 0, MDIO_DEVAD_NONE, 0x14, value);
	/* Dev ability according to SGMII specification */
	value = PHY_SGMII_DEV_ABILITY_SGMII;
	enetc_imdio_write(&bus, 0, MDIO_DEVAD_NONE, 0x04, value);
	/* Adjust link timer for SGMII */
	enetc_imdio_write(&bus, 0, MDIO_DEVAD_NONE, 0x13, 0x0003);
	enetc_imdio_write(&bus, 0, MDIO_DEVAD_NONE, 0x12, 0x06a0);

	/* restart AN */
	value = PHY_SGMII_CR_DEF_VAL | PHY_SGMII_CR_RESET_AN;

	enetc_imdio_write(&bus, 0, MDIO_DEVAD_NONE, 0x00, value);
	/* wait for link */
	to = 1000;
	do {
		value = enetc_imdio_read(&bus, 0, MDIO_DEVAD_NONE, 0x01);
		if ((value & 0x0024) == 0x0024)
			break;
	} while (--to);
	if ((value & 0x0024) != 0x0024)
		printf("PCS[0] didn't link up, giving up. (%04x)\n", value);
}

int misc_init_r(void)
{
	debug("%s()\n", __func__);
#ifdef CONFIG_EMB_EEP_I2C
	/* EMB_EEP_I2C_BUS_NUM_1 = CONFIG_EMB_EEP_I2C_BUS_NUM_EE1; */
	emb_eep_init_r(1, 1, 4);
#endif

	return 0;
}

int last_stage_init(void)
{
	if (sl28_has_rgmii())
		setup_rgmii();

	if (sl28_has_sgmii())
		setup_sgmii();

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
	debug("%s\n", __func__);

	fdt_increase_size(rw_fdt_blob, 32);

	if (!sl28_has_sgmii()) {
		debug("%s: disabling eth_p0\n", __func__);
		fdt_status_disabled_by_alias(rw_fdt_blob, "eth_p0");
	}

	if (!sl28_has_rgmii()) {
		debug("%s: disabling eth_p1\n", __func__);
		fdt_status_disabled_by_alias(rw_fdt_blob, "eth_p1");
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
#endif
