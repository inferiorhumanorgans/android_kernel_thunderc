/* arch/arm/mach-msm/lge/board-thunderc-bt.c
 * Copyright (C) 2009 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <asm/setup.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board_lge.h>
#include "board-thunderc.h"
#include <linux/delay.h>
#include <linux/rfkill.h>

#ifdef CONFIG_BT
static unsigned bt_config_power_on[] = {
	GPIO_CFG(BT_WAKE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* WAKE */
	GPIO_CFG(BT_RFR, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* RFR */
	GPIO_CFG(BT_CTS, 2, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),	/* CTS */
	GPIO_CFG(BT_RX, 2, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),	/* Rx */
	GPIO_CFG(BT_TX, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* Tx */
	GPIO_CFG(BT_PCM_DOUT, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(BT_PCM_DIN, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(BT_PCM_SYNC, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(BT_PCM_CLK, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_CLK */
	GPIO_CFG(BT_HOST_WAKE, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),	/* HOST_WAKE */
	GPIO_CFG(BT_RESET_N, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* RESET_N */
};
static unsigned bt_config_power_off[] = {
	GPIO_CFG(BT_WAKE, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* WAKE */
	GPIO_CFG(BT_RFR, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* RFR */
	GPIO_CFG(BT_CTS, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* CTS */
	GPIO_CFG(BT_RX, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Rx */
	GPIO_CFG(BT_TX, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Tx */
	GPIO_CFG(BT_PCM_DOUT, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(BT_PCM_DIN, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(BT_PCM_SYNC, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(BT_PCM_CLK, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_CLK */
	GPIO_CFG(BT_HOST_WAKE, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* HOST_WAKE */
	GPIO_CFG(BT_RESET_N, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* RESET_N */	
};

static int thunderc_bluetooth_toggle_radio(void *data, bool state)
{
	int ret;
	int (*power_control)(int enable);

	power_control = ((struct bluetooth_platform_data *)data)->bluetooth_power;
	ret = (*power_control)((state == RFKILL_USER_STATE_SOFT_BLOCKED) ? 1 : 0);
	return ret;
}

static int thunderc_bluetooth_power(int on)
{
	int pin, rc;

	printk(KERN_DEBUG "%s\n", __func__);

	if (on) {
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
					"%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, bt_config_power_on[pin], rc);
				return -EIO;
			}
		}
		//Turn Bluetooth Power On if and only if not turned on by WLAN yet.
		if (!gpio_get_value(CONFIG_BCM4325_GPIO_WL_REGON)) //#23
			gpio_set_value(CONFIG_BCM4325_GPIO_WL_REGON, 1); //#23
		mdelay(100);
		gpio_set_value(BT_RESET_N, 0);
		mdelay(100);
		gpio_set_value(BT_RESET_N, 1);
		mdelay(100);

	} else {
		//Turn Bluetooth Power Off if and only if not used by WLAN anymore.
		if (!gpio_get_value(CONFIG_BCM4325_GPIO_WL_RESET)) //#93
			gpio_set_value(CONFIG_BCM4325_GPIO_WL_REGON, 0); //#23

		gpio_set_value(BT_RESET_N, 0);
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
			rc = gpio_tlmm_config(bt_config_power_off[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
					"%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, bt_config_power_off[pin], rc);
				return -EIO;
			}
		}
	}
	return 0;
}

static struct bluetooth_platform_data thunderc_bluetooth_data = {
	.bluetooth_power = thunderc_bluetooth_power,
	.bluetooth_toggle_radio = thunderc_bluetooth_toggle_radio,
};

static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
	.dev = {
		.platform_data = &thunderc_bluetooth_data,
	},		
};

static void __init bt_power_init(void)
{
	int pin, rc;
	
	/* Power On */
	for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
		rc = gpio_tlmm_config(bt_config_power_on[pin],
				      GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR
				"%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, bt_config_power_on[pin], rc);
		}
	}

	gpio_direction_output(CONFIG_BCM4325_GPIO_WL_REGON, 1); //#23

	mdelay(100);
	gpio_set_value(BT_RESET_N, 0);
	mdelay(100);
	gpio_set_value(BT_RESET_N, 1);
	mdelay(100);

	/* 1 sec */
	ssleep(1);

	/* Power Off */
	gpio_set_value(CONFIG_BCM4325_GPIO_WL_REGON, 0); //#23

	gpio_set_value(BT_RESET_N, 0);
	for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
		rc = gpio_tlmm_config(bt_config_power_off[pin],
				      GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR
				"%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, bt_config_power_off[pin], rc);
		}
	}
}
#else
#define bt_power_init(x) do {} while (0)
#endif

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= BT_HOST_WAKE,
		.end	= BT_HOST_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= BT_WAKE,
		.end	= BT_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(BT_HOST_WAKE),
		.end	= MSM_GPIO_TO_INT(BT_HOST_WAKE),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct bluesleep_platform_data thunderc_bluesleep_data = {
	.bluetooth_port_num = 0,
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
	.dev = {
		.platform_data = &thunderc_bluesleep_data,
	},	
};

void __init lge_add_btpower_devices(void)
{
	bt_power_init();
#ifdef CONFIG_BT
	platform_device_register(&msm_bt_power_device);
#endif
	platform_device_register(&msm_bluesleep_device);
}
