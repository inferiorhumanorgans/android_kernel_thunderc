/* arch/arm/mach-msm/board-thunderc-input.c
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
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio_event.h>
#include <linux/keyreset.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#include <mach/rpc_server_handset.h>
#include <mach/board_lge.h>

#include "board-thunderc.h"

static int prox_power_set(unsigned char onoff);

/* head set device */
static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "7k_handset",
	.pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
static unsigned int keypad_row_gpios[] = {
	32, 33, 34
};
#else
static unsigned int keypad_row_gpios[] = {
	32, 33
};
#endif

static unsigned int keypad_col_gpios[] = {38, 37,36};

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios) + (col))

#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
static const unsigned short keypad_keymap_thunder[9] = {
    [KEYMAP_INDEX(0, 0)] = KEY_MENU,
    [KEYMAP_INDEX(0, 1)] = KEY_HOME,
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(1, 0)] = KEY_SEARCH,
	[KEYMAP_INDEX(1, 1)] = KEY_BACK,
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(2, 0)] = KEY_CAMERA,
	[KEYMAP_INDEX(2, 1)] = KEY_FOCUS,
	[KEYMAP_INDEX(2, 2)] = KEY_CHAT,
};
#else
/* change key map for H/W Rev.B -> Rev.C  2010-06-13 younchan,kim
	[Rev.B key map]
static const unsigned short keypad_keymap_thunder[6] = {
	[KEYMAP_INDEX(0, 0)] = KEY_BACK,
	[KEYMAP_INDEX(0, 1)] = KEY_MENU,
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(1, 0)] = KEY_SEARCH,
	[KEYMAP_INDEX(1, 1)] = KEY_HOME,
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEDOWN,
};
*/
/* add Rev.C key map 2010-05-13 younchan.kim */
static const unsigned short keypad_keymap_thunder[6] = {
	[KEYMAP_INDEX(0, 0)] = KEY_MENU,
	[KEYMAP_INDEX(0, 1)] = KEY_HOME,
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(1, 0)] = KEY_SEARCH,
	[KEYMAP_INDEX(1, 1)] = KEY_BACK,
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEDOWN,
};
#endif
static struct gpio_event_matrix_info thunder_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_thunder,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_DRIVE_INACTIVE
};

static struct gpio_event_info *thunder_keypad_info[] = {
	&thunder_keypad_matrix_info.info
};

static struct gpio_event_platform_data thunder_keypad_data = {
	.name		= "thunder_keypad",
	.info		= thunder_keypad_info,
	.info_count	= ARRAY_SIZE(thunder_keypad_info)
};
struct platform_device keypad_device_thunder= {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &thunder_keypad_data,
	},
};

/* keyreset platform device */
static int thunderc_reset_keys_up[] = {
	KEY_HOME,
	0
};

static struct keyreset_platform_data thunderc_reset_keys_pdata = {
	.keys_up = thunderc_reset_keys_up,
	.keys_down = {
		KEY_BACK,
		KEY_VOLUMEDOWN,
		KEY_MENU,
		0
	},
};

struct platform_device thunderc_reset_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &thunderc_reset_keys_pdata,
};

/* input platform device */
static struct platform_device *thunder_input_devices[] __initdata = {
	&hs_device,
	&keypad_device_thunder,
	&thunderc_reset_keys_device,
};

/* MCS6000 Touch */
static struct gpio_i2c_pin ts_i2c_pin[] = {
	[0] = {
		.sda_pin	= TS_GPIO_I2C_SDA,
		.scl_pin	= TS_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= TS_GPIO_IRQ,
	},
};

static struct i2c_gpio_platform_data ts_i2c_pdata = {
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay				= 2,
};

static struct platform_device ts_i2c_device = {
	.name	= "i2c-gpio",
	.dev.platform_data = &ts_i2c_pdata,
};

#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
int ts_set_vreg(unsigned char onoff)
#else
static int ts_set_vreg(unsigned char onoff)
#endif
{
	struct vreg *vreg_touch;
	int rc;
	static int old_onoff = 0;

	printk("[Touch] %s() onoff:%d\n",__FUNCTION__, onoff);

	if (old_onoff == onoff)
		return 0;

	vreg_touch = vreg_get(0, "synt");

	if(IS_ERR(vreg_touch)) {
		printk("[Touch] vreg_get fail : touch\n");
		return -1;
	}

	if (onoff) {
		rc = vreg_set_level(vreg_touch, 3050);
		if (rc != 0) {
			printk("[Touch] vreg_set_level failed\n");
			return -1;
		}
		vreg_enable(vreg_touch);
		old_onoff = onoff;
	} else {
		vreg_disable(vreg_touch);
		old_onoff = onoff;
	}

	return 0;
}
#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
EXPORT_SYMBOL(ts_set_vreg);
#endif

static struct touch_platform_data ts_pdata = {
	.ts_x_min = TS_X_MIN,
	.ts_x_max = TS_X_MAX,
	.ts_y_min = TS_Y_MIN,
	.ts_y_max = TS_Y_MAX,
	.power = ts_set_vreg,
	.irq 	  = TS_GPIO_IRQ,
	.scl      = TS_GPIO_I2C_SCL,
	.sda      = TS_GPIO_I2C_SDA,
};

static struct i2c_board_info ts_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("touch_mcs6000", TS_I2C_SLAVE_ADDR),
		.type = "touch_mcs6000",
		.platform_data = &ts_pdata,
	},
};

static void __init thunderc_init_i2c_touch(int bus_num)
{
	ts_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&ts_i2c_pdata, ts_i2c_pin[0],	&ts_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &ts_i2c_bdinfo[0], 1);
	platform_device_register(&ts_i2c_device);
}

/* accelerometer */
static int kr3dh_config_gpio(int config)
{
	if (config) { /* for wake state */
	}
	else { /* for sleep state */
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_INT, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	}

	return 0;
}

static int kr_init(void)
{
	return 0;
}

static void kr_exit(void)
{
}

static int power_on(void)
{
	return 0;
}

static int power_off(void)
{
	return 0;
}

struct kr3dh_platform_data kr3dh_data = {
	.poll_interval = 100,
	.min_interval = 0,
	.g_range = 0x00,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.power_on = power_on,
	.power_off = power_off,
	.kr_init = kr_init,
	.kr_exit = kr_exit,
	.gpio_config = kr3dh_config_gpio,
};

static struct gpio_i2c_pin accel_i2c_pin[] = {
	[0] = {
		.sda_pin	= ACCEL_GPIO_I2C_SDA,
		.scl_pin	= ACCEL_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= ACCEL_GPIO_INT,
	},
};

static struct i2c_gpio_platform_data accel_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};

static struct platform_device accel_i2c_device = {
	.name = "i2c-gpio",
	.dev.platform_data = &accel_i2c_pdata,
};

static struct i2c_board_info accel_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("KR3DH", ACCEL_I2C_ADDRESS_H),
		.type = "KR3DH",
		.platform_data = &kr3dh_data,
	},
	[1] = {
		I2C_BOARD_INFO("KR3DM", ACCEL_I2C_ADDRESS),
		.type = "KR3DM",
		.platform_data = &kr3dh_data,
	},
};

static void __init thunderc_init_i2c_acceleration(int bus_num)
{
	accel_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&accel_i2c_pdata, accel_i2c_pin[0], &accel_i2c_bdinfo[0]);

	if (lge_bd_rev >= 9) /* KR_3DH >= Rev. 1.1 */
		i2c_register_board_info(bus_num, &accel_i2c_bdinfo[0], 1);
	else
		i2c_register_board_info(bus_num, &accel_i2c_bdinfo[1], 1);
	platform_device_register(&accel_i2c_device);
}

/* proximity & ecompass */

#define ECOM_POWER_OFF		0
#define ECOM_POWER_ON		1

static int ecom_is_power_on = ECOM_POWER_OFF;

static int ecom_power_set(unsigned char onoff)
{
	int ret = 0;
	struct vreg *gp3_vreg = vreg_get(0, "gp3");
	struct vreg *gp6_vreg = vreg_get(0, "gp6"); /* prox */

	printk("[Ecompass] %s() onoff %d, prev_status %d\n",__FUNCTION__, 
			onoff, ecom_is_power_on);

	if (onoff) {
		if (ecom_is_power_on == ECOM_POWER_OFF) {
			vreg_set_level(gp3_vreg, 3000);
			vreg_enable(gp3_vreg);
			/* proximity power on , 
			 * when we turn off I2C line be set to low caues sensor H/W characteristic 
			 */
#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
			vreg_set_level(gp6_vreg, 2900);
#else
			vreg_set_level(gp6_vreg, 2800);
#endif
			vreg_enable(gp6_vreg);

			ecom_is_power_on = ECOM_POWER_ON;
		}
	} else {
		if (ecom_is_power_on == ECOM_POWER_ON) {
			vreg_disable(gp3_vreg);

			/* proximity power on , 
			 * when we turn off I2C line be set to low caues sensor H/W characteristic 
			 */
			vreg_disable(gp6_vreg);

			ecom_is_power_on = ECOM_POWER_OFF;
		}
	}

	return ret;
}

static struct ecom_platform_data ecom_pdata = {
	.pin_int        	= ECOM_GPIO_INT,
	.pin_rst		= 0,
	.power          	= ecom_power_set,
};

#define PROX_POWER_OFF		0
#define PROX_POWER_ON		1

static int prox_is_power_on = PROX_POWER_OFF;

static int prox_power_set(unsigned char onoff)
{
	int ret = 0;
	struct vreg *gp6_vreg = vreg_get(0, "gp6");

	printk("[Proxi] %s() onoff %d, prev_status %d\n",__FUNCTION__, 
			onoff, prox_is_power_on);

	if (onoff) {
		if (prox_is_power_on == PROX_POWER_OFF) {
#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
			vreg_set_level(gp6_vreg, 2900);
#else
			vreg_set_level(gp6_vreg, 2800);
#endif
			vreg_enable(gp6_vreg);

			prox_is_power_on = PROX_POWER_ON;
		}
	} 
	else {
		if (prox_is_power_on == PROX_POWER_ON) {
			vreg_disable(gp6_vreg);

			prox_is_power_on = PROX_POWER_OFF;
		}
	}

	return ret;
}

static struct proximity_platform_data proxi_pdata = {
	.irq_num	= PROXI_GPIO_DOUT,
	.power		= prox_power_set,
	.methods		= 0, // normal mode (1 - interrupt mode)
	.operation_mode		= 0, // A mode (1 - B1, 2 - B2)
	.debounce	 = 0,
	.cycle = 2,
};

static struct i2c_board_info prox_ecom_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("proximity_gp2ap", PROXI_I2C_ADDRESS),
		.type = "proximity_gp2ap",
		.platform_data = &proxi_pdata,
	},
	[1] = {
		I2C_BOARD_INFO("ami304_sensor", ECOM_I2C_ADDRESS),
		.type = "ami304_sensor",
		.platform_data = &ecom_pdata,
	},
};

static struct gpio_i2c_pin proxi_ecom_i2c_pin[] = {
	[0] = {
		.sda_pin	= PROXI_GPIO_I2C_SDA,
		.scl_pin	= PROXI_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= PROXI_GPIO_DOUT,
	},
	[1] = {
		.sda_pin	= ECOM_GPIO_I2C_SDA,
		.scl_pin	= ECOM_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= ECOM_GPIO_INT,
	},
};

static struct i2c_gpio_platform_data proxi_ecom_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};

static struct platform_device proxi_ecom_i2c_device = {
        .name = "i2c-gpio",
        .dev.platform_data = &proxi_ecom_i2c_pdata,
};


static void __init thunderc_init_i2c_prox_ecom(int bus_num)
{
	proxi_ecom_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&proxi_ecom_i2c_pdata, proxi_ecom_i2c_pin[0], &prox_ecom_i2c_bdinfo[0]);
	init_gpio_i2c_pin(&proxi_ecom_i2c_pdata, proxi_ecom_i2c_pin[1], &prox_ecom_i2c_bdinfo[1]);

	i2c_register_board_info(bus_num, &prox_ecom_i2c_bdinfo[0], 2);
	platform_device_register(&proxi_ecom_i2c_device);
}

/* common function */
void __init lge_add_input_devices(void)
{

	platform_add_devices(thunder_input_devices, ARRAY_SIZE(thunder_input_devices));

	lge_add_gpio_i2c_device(thunderc_init_i2c_touch);
	lge_add_gpio_i2c_device(thunderc_init_i2c_prox_ecom);
	lge_add_gpio_i2c_device(thunderc_init_i2c_acceleration);
}
