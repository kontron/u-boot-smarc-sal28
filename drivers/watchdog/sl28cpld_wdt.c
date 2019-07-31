// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Kontron Europe GmbH
 */

#include <common.h>
#include <dm.h>
#include <wdt.h>
#include <i2c.h>

/*
 * The device tree binding is other than in linux, because we bind it as an I2C
 * directly, there is no multi function driver in between yet.
 */

#define WDOG_CTRL 0x04
#define WDOG_TOUT 0x05
#define WDOG_KICK 0x06
#define WDOG_CNT  0x07

#define WDOG_CTRL_EN_MASK 0x03
#define WDOG_CTRL_EN0                 BIT(0)
#define WDOG_CTRL_EN1                 BIT(1)
#define WDOG_CTRL_LOCK                BIT(2)
#define WDOG_CTRL_ISSUE_RESET         BIT(6)
#define WDOG_CTRL_ASSERT_WDT_TIME_OUT BIT(7)
#define WDOG_KICK_VAL                 0x6b

static int sl28cpld_wdt_reset(struct udevice *dev)
{
	u8 val = WDOG_KICK_VAL;
	return dm_i2c_write(dev, WDOG_KICK, &val, 1);
}

static int sl28cpld_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	int ret;
	u8 val;
	u8 tout = timeout / 1000;

	ret = dm_i2c_read(dev, WDOG_CTRL, &val, 1);
	if (ret)
		return ret;

	/* disable watchdog */
	val &= ~WDOG_CTRL_EN_MASK;
	ret = dm_i2c_write(dev, WDOG_CTRL, &val, 1);
	if (ret)
		return ret;

	/* set timeout */
	ret = dm_i2c_write(dev, WDOG_TOUT, &tout, 1);
	if (ret)
		return ret;

	/* kick it */
	ret = sl28cpld_wdt_reset(dev);
	if (ret)
		return ret;

	/* enable it, depending on the flags either the recovery one or
	 * the normal one */
	if (flags & BIT(0))
		val |= WDOG_CTRL_EN1;
	else
		val |= WDOG_CTRL_EN0;

	if (flags & BIT(1))
		val |= WDOG_CTRL_LOCK;

	if (flags & BIT(2))
		val &= ~WDOG_CTRL_ISSUE_RESET;
	else
		val |= WDOG_CTRL_ISSUE_RESET;

	if (flags & BIT(3))
		val |= WDOG_CTRL_ASSERT_WDT_TIME_OUT;
	else
		val &= ~WDOG_CTRL_ASSERT_WDT_TIME_OUT;

	return dm_i2c_write(dev, WDOG_CTRL, &val, 1);
}

static int sl28cpld_wdt_stop(struct udevice *dev)
{
	int ret;
	u8 val;

	ret = dm_i2c_read(dev, WDOG_CTRL, &val, 1);
	if (ret)
		return ret;

	val &= ~WDOG_CTRL_EN_MASK;
	return dm_i2c_write(dev, WDOG_CTRL, &val, 1);
}

static int sl28cpld_wdt_expire_now(struct udevice *dev, ulong flags)
{
	sl28cpld_wdt_start(dev, 0, flags);

	return 0;
}

static int sl28cpld_wdt_probe(struct udevice *dev)
{
	debug("%s: Probing wdt%u (sl28cpld-wdt)\n", __func__, dev->seq);
	i2c_set_chip_flags(dev, DM_I2C_CHIP_RD_ADDRESS |
			   DM_I2C_CHIP_WR_ADDRESS);

	return 0;
}

static const struct wdt_ops sl28cpld_wdt_ops = {
	.start = sl28cpld_wdt_start,
	.reset = sl28cpld_wdt_reset,
	.stop = sl28cpld_wdt_stop,
	.expire_now = sl28cpld_wdt_expire_now,
};

static const struct udevice_id sl28cpld_wdt_ids[] = {
	{ .compatible = "kontron,sl28cpld-wdt" },
	{}
};

U_BOOT_DRIVER(wdt_sandbox) = {
	.name = "wdt_sl28cpld",
	.id = UCLASS_WDT,
	.probe	= sl28cpld_wdt_probe,
	.of_match = sl28cpld_wdt_ids,
	.ops = &sl28cpld_wdt_ops,
};
