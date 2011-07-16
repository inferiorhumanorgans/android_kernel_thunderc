/* arch/arm/mach-msm/board-thunderc-panel.c
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

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <mach/msm_rpcrouter.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#include "devices.h"
#include "board-thunderc.h"

#include <mach/board_lge.h>

#define MSM_FB_LCDC_VREG_OP(name, op, level)			\
do { \
	vreg = vreg_get(0, name); \
	vreg_set_level(vreg, level); \
	if (vreg_##op(vreg)) \
		printk(KERN_ERR "%s: %s vreg operation failed \n", \
			(vreg_##op == vreg_enable) ? "vreg_enable" \
				: "vreg_disable", name); \
} while (0)

static char *msm_fb_vreg[] = {
	"gp4",
	"gp2",
};

static int mddi_power_save_on;
static int msm_fb_mddi_power_save(int on)
{
	struct vreg *vreg;
	int flag_on = !!on;

	if (mddi_power_save_on == flag_on)
		return 0;

	mddi_power_save_on = flag_on;

	if (on) {
		MSM_FB_LCDC_VREG_OP(msm_fb_vreg[0], enable, 1800);
		MSM_FB_LCDC_VREG_OP(msm_fb_vreg[1], enable, 2800);
	} 
	else {
		MSM_FB_LCDC_VREG_OP(msm_fb_vreg[0], disable, 0);
		MSM_FB_LCDC_VREG_OP(msm_fb_vreg[1], disable, 0);
	}

	return 0;
}

static struct mddi_platform_data mddi_pdata = {
	.mddi_power_save = msm_fb_mddi_power_save,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
};

static void __init msm_fb_add_devices(void)
{
	if(lge_bd_rev >= 8) /* >= Rev 1.0 */
		strcpy(msm_fb_vreg[1], "gp1");
	
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("pmdh", &mddi_pdata);
	msm_fb_register_device("lcdc", 0);
}

#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HVGA) || \
	defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
static int mddi_novatek_pmic_backlight(int level)
{
	/* TODO: Backlight control here */
	return 0;
}

static struct msm_panel_novatek_pdata mddi_novatek_panel_data = {
	.gpio = 102,				/* lcd reset_n */
	.pmic_backlight = mddi_novatek_pmic_backlight,
	.initialized = 1,
};

static struct platform_device mddi_novatek_panel_device = {
	.name   = "mddi_novatek_hvga",
	.id     = 0,
	.dev    = {
		.platform_data = &mddi_novatek_panel_data,
	}
};
#endif

#if defined(CONFIG_FB_MSM_MDDI_HITACHI_HVGA) || \
	defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
static int mddi_hitachi_pmic_backlight(int level)
{
	/* TODO: Backlight control here */
	return 0;
}

#if 1
	static struct msm_panel_hitachi_pdata mddi_hitachi_panel_data = {
		.gpio = 102,				/* lcd reset_n */
		.pmic_backlight = mddi_hitachi_pmic_backlight,
		.initialized = 1,
#else
	static struct msm_panel_common_pdata mddi_hitachi_panel_data = {
		.gpio = 102,				/* lcd reset_n */
		.pmic_backlight = mddi_hitachi_pmic_backlight,
#endif
};

static struct platform_device mddi_hitachi_panel_device = {
	.name   = "mddi_hitachi_hvga",
	.id     = 0,
	.dev    = {
		.platform_data = &mddi_hitachi_panel_data,
	}
};
#endif

/* backlight device */
static struct gpio_i2c_pin bl_i2c_pin[] = {
	[0] = {
		.sda_pin	= 89,
		.scl_pin	= 88,
		.reset_pin	= 82,
		.irq_pin	= 0,
	},
};

static struct i2c_gpio_platform_data bl_i2c_pdata = {
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay				= 2,
};

static struct platform_device bl_i2c_device = {
	.name	= "i2c-gpio",
	.dev.platform_data = &bl_i2c_pdata,
};

static struct aat28xx_platform_data aat2870bl_data = {
	.gpio = 82,
	.version = 2862,
};

static struct i2c_board_info bl_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("aat2870bl", 0x60),
		.type = "aat2870bl",
		.platform_data = &aat2870bl_data,
	},
};

struct device* thunderc_backlight_dev(void)
{
	return &bl_i2c_device.dev;
}

void __init thunderc_init_i2c_backlight(int bus_num)
{
	bl_i2c_device.id = bus_num;
	
	init_gpio_i2c_pin(&bl_i2c_pdata, bl_i2c_pin[0],	&bl_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &bl_i2c_bdinfo[0], 1);
	platform_device_register(&bl_i2c_device);
}

/* common functions */
void __init lge_add_lcd_devices(void)
{
#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
  platform_device_register(&mddi_novatek_panel_device);	
  platform_device_register(&mddi_hitachi_panel_device);	
#else	
#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HVGA)
	platform_device_register(&mddi_novatek_panel_device);
#else
	platform_device_register(&mddi_hitachi_panel_device);
#endif
#endif /* CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA */

	msm_fb_add_devices();

	lge_add_gpio_i2c_device(thunderc_init_i2c_backlight);
}
