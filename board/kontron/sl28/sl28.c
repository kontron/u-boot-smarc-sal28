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
#include <environment.h>
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

DECLARE_GLOBAL_DATA_PTR;

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

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}

int board_early_init_f(void)
{
	fsl_lsch3_early_init_f();
	return 0;
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

#define PCS_INF(fmt, args...)  do {} while (0)
#define PCS_ERR(fmt, args...)  printf("PCS: " fmt, ##args)

void setup_rgmii(void)
{
	#define NETC_PF1_BAR0_BASE	0x1f8050000
	#define NETC_PF1_ECAM_BASE	0x1F0001000
	struct mii_dev *ext_bus;
	char *mdio_name = "netc_mdio";
	int phy_addr = 4;
	int value;

	PCS_INF("trying to set up RGMII\n");

	/* turn on PCI function */
	out_le16(NETC_PF1_ECAM_BASE + 4, 0xffff);
	out_le32(NETC_PF1_BAR0_BASE + 0x8300, 0x8006);

	/* configure AQR PHY */
	ext_bus = miiphy_get_dev_by_name(mdio_name);
	if (!ext_bus) {
		PCS_ERR("couldn't find MDIO bus, ignoring the PHY\n");
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
	PCS_ERR("RGMII PHY access error, giving up.\n");
}

extern int serdes_protocol;
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

	if ((serdes_protocol & 0xf0) != 0x0080)
		return;

	PCS_INF("trying to set up SGMII, this is hardcoded for SERDES x8xx!!!!\n");

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
	PCS_INF("BMSR %04x\n", value);
	if ((value & 0x0024) != 0x0024)
		PCS_ERR("PCS[0] didn't link up, giving up.\n");
}

int last_stage_init(void)
{
	puts("Configuring RGMII and SGMII\n");
	setup_rgmii();
	setup_sgmii();

	return 0;
}

#if defined(CONFIG_VIDEO)
void *video_hw_init(void)
{
	return NULL;
}
#endif
