/* arch/arm/mach-msm/lge/board-thunderc-misc.c
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
#include <linux/power_supply.h>
#include <asm/setup.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/msm_battery.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/io.h>
#include <linux/delay.h>

#include <mach/board_lge.h>
#include "board-thunderc.h"

#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
/* LGE_CHANGE_S [dojip.kim@lge.com] 2010-05-11, button leds */
static void button_backlight_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
// LGE_CHANGE [james.jang@lge.com] 2010-08-07, again reduce the current
/* from 0 to 150 mA in 10 mA increments */
// LGE_CHANGE [dojip.kim@lge.com] 2010-07-14, reduce the current
//#define MAX_KEYPAD_BL_LEVEL	16  /* 15: 150 mA */
//#define MAX_KEYPAD_BL_LEVEL	127 /* 2: 20 mA */
#define MAX_KEYPAD_BL_LEVEL	255 /* 1: 10 mA */
	pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
}

struct led_classdev thunderc_custom_leds[] = {
	{
		.name = "button-backlight",
		.brightness_set = button_backlight_set,
		.brightness = LED_OFF,
	},
};

static int register_leds(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(thunderc_custom_leds); i++) {
		rc = led_classdev_register(&pdev->dev, &thunderc_custom_leds[i]);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led class driver: "
					"thunderc_custom_leds\n");
			return rc;
		}
	}

	return rc;
}

static void unregister_leds(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(thunderc_custom_leds); ++i)
		led_classdev_unregister(&thunderc_custom_leds[i]);
}

static void suspend_leds(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(thunderc_custom_leds); ++i)
		led_classdev_suspend(&thunderc_custom_leds[i]);
}

static void resume_leds(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(thunderc_custom_leds); ++i)
		led_classdev_resume(&thunderc_custom_leds[i]);
}

int keypad_led_set(unsigned char value)
{
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-10-18, conflict with button_led. */
	return 0;
	/*
	return pmic_set_led_intensity(LED_KEYPAD, value);
	*/
}

static struct msm_pmic_leds_pdata leds_pdata = {
	.custom_leds		= thunderc_custom_leds,
	.register_custom_leds	= register_leds,
	.unregister_custom_leds	= unregister_leds,
	.suspend_custom_leds	= suspend_leds,
	.resume_custom_leds	= resume_leds,
	.msm_keypad_led_set	= keypad_led_set,
};

static struct platform_device msm_device_pmic_leds = {
        .name                   = "pmic-leds",
	.id                     = -1,
	.dev.platform_data	= &leds_pdata,
};
/* LGE_CHANGE_E [dojip.kim@lge.com] 2010-05-11 */
#else /* THUNDER_VZW */
/* add led device for VS660 Rev.D by  younchan.kim 2010-05-27  */

static void pmic_mpp_isink_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	int mpp_number;
	int on_off;

	if (!strcmp(led_cdev->name ,"red"))
		mpp_number = (int)PM_MPP_20;
	else if (!strcmp(led_cdev->name, "green"))
		mpp_number = (int)PM_MPP_21;
	else if (!strcmp(led_cdev->name, "blue"))
		mpp_number = (int)PM_MPP_22;
	else
		return;

	if(value == 0)
		on_off = (int)PM_MPP__I_SINK__SWITCH_DIS;
	else
		on_off = (int)PM_MPP__I_SINK__SWITCH_ENA;

	pmic_secure_mpp_config_i_sink((enum mpp_which)mpp_number,
			PM_MPP__I_SINK__LEVEL_20mA, (enum mpp_i_sink_switch)on_off);
}

static void button_backlight_set(struct led_classdev* led_cdev, enum led_brightness value)
{
	int i;
	int mpp_number;
	int on_off;

	if(value == 0)
		on_off = (int)PM_MPP__I_SINK__SWITCH_DIS;
	else
		on_off = (int)PM_MPP__I_SINK__SWITCH_ENA;

	mpp_number = (int)PM_MPP_19;
	for(i=0; i<4; i++){
		pmic_secure_mpp_config_i_sink((enum mpp_which)mpp_number,PM_MPP__I_SINK__LEVEL_20mA, (enum mpp_i_sink_switch)on_off);
		mpp_number++;
	}
}
struct led_classdev thunderc_custom_leds[] = {
	#if 0
	{
		.name = "red",
		.brightness_set = pmic_mpp_isink_set,
		.brightness = LED_OFF,
	},
	{
		.name = "green",
		.brightness_set = pmic_mpp_isink_set,
		.brightness = LED_OFF,
	},
	{
		.name = "blue",
		.brightness_set = pmic_mpp_isink_set,
		.brightness = LED_OFF,
	},
	#else
	{
		.name = "button-backlight",
		.brightness_set = button_backlight_set,
		.brightness = LED_OFF,
	},
	#endif
};

static int register_leds(struct platform_device *pdev)
{
	int rc;
	int i;

	for(i = 0 ; i < ARRAY_SIZE(thunderc_custom_leds) ; i++) {
		rc = led_classdev_register(&pdev->dev, &thunderc_custom_leds[i]);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led class driver : thunderc_custom_leds \n");
			return rc;
		}
		pmic_mpp_isink_set(&thunderc_custom_leds[i], LED_OFF);
	}

	return rc;
}

static void unregister_leds (void)
{
	int i;
	for (i = 0; i< ARRAY_SIZE(thunderc_custom_leds); ++i)
		led_classdev_unregister(&thunderc_custom_leds[i]);
}

static void suspend_leds (void)
{
	int i;
	for (i = 0; i< ARRAY_SIZE(thunderc_custom_leds); ++i)
		led_classdev_suspend(&thunderc_custom_leds[i]);
}

static void resume_leds (void)
{
	int i;
	for (i = 0; i< ARRAY_SIZE(thunderc_custom_leds); ++i)
		led_classdev_resume(&thunderc_custom_leds[i]);
}

int keypad_led_set(unsigned char value)
{
	int ret;

	ret = pmic_set_led_intensity(LED_KEYPAD, value);

	return ret;
}

static struct msm_pmic_leds_pdata leds_pdata = {
	.custom_leds		= thunderc_custom_leds,
	.register_custom_leds	= register_leds,
	.unregister_custom_leds	= unregister_leds,
	.suspend_custom_leds	= suspend_leds,
	.resume_custom_leds	= resume_leds,
	.msm_keypad_led_set	= keypad_led_set,
};

static struct platform_device msm_device_pmic_leds = {
	.name                           = "pmic-leds",
	.id                                     = -1,
	.dev.platform_data      = &leds_pdata,
};
#endif /* CONFIG_MACH_MSM7X27_THUNDERC_SPRINT */

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design     = 3200,
	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-07, [SPRINT] 4200 -> 4250
#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
	.voltage_max_design     = 4250,
#else
	.voltage_max_design     = 4200,
#endif
	.avail_chg_sources      = AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
};

static struct platform_device msm_batt_device = {
	.name           = "msm-battery",
	.id         = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};

/* Vibrator Functions for Android Vibrator Driver */
#define VIBE_IC_VOLTAGE			3300
#define GPIO_LIN_MOTOR_PWM		28

#define GP_MN_CLK_MDIV_REG		0x004C
#define GP_MN_CLK_NDIV_REG		0x0050
#define GP_MN_CLK_DUTY_REG		0x0054

/* about 22.93 kHz, should be checked */
#define GPMN_M_DEFAULT			21
#define GPMN_N_DEFAULT			4500
/* default duty cycle = disable motor ic */
#define GPMN_D_DEFAULT			(GPMN_N_DEFAULT >> 1) 
#define PWM_MAX_HALF_DUTY		((GPMN_N_DEFAULT >> 1) - 80) /* minimum operating spec. should be checked */

#define GPMN_M_MASK				0x01FF
#define GPMN_N_MASK				0x1FFF
#define GPMN_D_MASK				0x1FFF

#define REG_WRITEL(value, reg)	writel(value, (MSM_WEB_BASE+reg))

extern int aat2870bl_ldo_set_level(struct device * dev, unsigned num, unsigned vol);
extern int aat2870bl_ldo_enable(struct device * dev, unsigned num, unsigned enable);

// LGE_CHANGE [dojip.kim@lge.com] 2010-07-02, 
// retry to set the power because sometimes that failed
int thunderc_vibrator_power_set(int enable)
{
	static int is_enabled = 0;
	struct device *dev = thunderc_backlight_dev();
	int retry = 5;
	int ret = 0;
	int en = !!enable;

	if (dev==NULL) {
		printk(KERN_ERR "%s: backlight devive get failed\n", __FUNCTION__);
		return -1;
	}

	if (en == is_enabled)
		return 0;

	while(retry--) {
		/* 3300 mV for Motor IC */				
		if (en)
			ret = aat28xx_ldo_set_level(dev, 1, (VIBE_IC_VOLTAGE) * en);

		ret |= aat28xx_ldo_enable(dev, 1, en);
		if (ret < 0) {
			printk(KERN_ERR "%s: vibrator LDO failed\n", 
					__FUNCTION__);
			if (retry < 3)
				mdelay(100);
			continue;
		}
		is_enabled = en;
		return 0;
	}

	return -EIO;
}

int thunderc_vibrator_pwm_set(int enable, int amp)
{
	int gain = ((PWM_MAX_HALF_DUTY*amp) >> 7)+ GPMN_D_DEFAULT;

	if (enable) {
		// LGE_CHANGE [dojip.kim@lge.com] 2010-07-21, pwm sleep (from MS690)
		REG_WRITEL((GPMN_M_DEFAULT & GPMN_M_MASK), GP_MN_CLK_MDIV_REG);
		REG_WRITEL((~( GPMN_N_DEFAULT - GPMN_M_DEFAULT )&GPMN_N_MASK), GP_MN_CLK_NDIV_REG);
		REG_WRITEL((gain & GPMN_D_MASK), GP_MN_CLK_DUTY_REG);

		/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-12, GP_MN */
		gpio_tlmm_config(GPIO_CFG(GPIO_LIN_MOTOR_PWM,1,GPIO_OUTPUT,
					  GPIO_PULL_DOWN,GPIO_2MA),GPIO_ENABLE);
		REG_WRITEL((gain & GPMN_D_MASK), GP_MN_CLK_DUTY_REG);
	} else {
		// LGE_CHANGE [dojip.kim@lge.com] 2010-07-21, pwm sleep (from MS690)
		REG_WRITEL(0x00, GP_MN_CLK_MDIV_REG);
		REG_WRITEL(0x1000, GP_MN_CLK_NDIV_REG);
		REG_WRITEL(0x1FFF, GP_MN_CLK_DUTY_REG);

		REG_WRITEL(GPMN_D_DEFAULT, GP_MN_CLK_DUTY_REG);
		/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-12, GPIO */
		gpio_tlmm_config(GPIO_CFG(GPIO_LIN_MOTOR_PWM,0,GPIO_OUTPUT,
					  GPIO_PULL_DOWN,GPIO_2MA),GPIO_ENABLE);
		gpio_direction_output(GPIO_LIN_MOTOR_PWM, 0);
	}

	return 0;
}

int thunderc_vibrator_ic_enable_set(int enable)
{
	/* nothing to do, thunder does not using Motor Enable pin */
	return 0;
}

static struct android_vibrator_platform_data thunderc_vibrator_data = {
	.enable_status = 0,
	.power_set = thunderc_vibrator_power_set,
	.pwm_set = thunderc_vibrator_pwm_set,
	.ic_enable_set = thunderc_vibrator_ic_enable_set,
	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-30, 100->115 (by HW request)
	// LGE_CHANGE [dojip.kim@lge.com] 2010-09-03, 110
	.amp_value = 110,
};

static struct platform_device android_vibrator_device = {
	.name   = "android-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &thunderc_vibrator_data,
	},
};

/* ear sense driver */
static char *ear_state_string[] = {
	"0",
	"1",
};

enum {
	EAR_STATE_EJECT = 0,
	EAR_STATE_INJECT = 1, 
};

enum {
	EAR_EJECT = 0,
	EAR_INJECT = 1,
};

#if 0
static int thunderc_hs_mic_bias_power(int enable)
{
	struct vreg *hs_bias_vreg;
	static int is_enabled = 0;

	hs_bias_vreg = vreg_get(NULL, "ruim");

	if (IS_ERR(hs_bias_vreg)) {
		printk(KERN_ERR "%s: vreg_get failed\n", __FUNCTION__);
		return PTR_ERR(hs_bias_vreg);
	}

	if (enable) {
		if (is_enabled) {
			//printk(KERN_INFO "HS Mic. Bias power was enabled, already\n");
			return 0;
		}

		if (vreg_set_level(hs_bias_vreg, 2600) <0) {
			printk(KERN_ERR "%s: vreg_set_level failed\n", __FUNCTION__);
			return -EIO;
		}

		if (vreg_enable(hs_bias_vreg) < 0 ) {
			printk(KERN_ERR "%s: vreg_enable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 1;
	} else {
		if (!is_enabled) {
			//printk(KERN_INFO "HS Mic. Bias power was disabled, already\n");
			return 0;
		}

		if (vreg_set_level(hs_bias_vreg, 0) <0) {
			printk(KERN_ERR "%s: vreg_set_level failed\n", __FUNCTION__);
			return -EIO;
		}

		if (vreg_disable(hs_bias_vreg) < 0) {
			printk(KERN_ERR "%s: vreg_disable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 0;
	}
	return 0;
}
#endif

static int thunderc_gpio_earsense_work_func(void)
{
	int state;
	int gpio_value;

	gpio_value = gpio_get_value(GPIO_EAR_SENSE);
	printk(KERN_INFO"%s: ear sense detected : %s\n", __func__, 
			gpio_value?"injected":"ejected");
	if (gpio_value == EAR_EJECT) {
		state = EAR_STATE_EJECT;
		/* LGE_CHANGE_S, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
		//thunderc_hs_mic_bias_power(0);
		/* LGE_CHANGE_E, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
	} else {
		state = EAR_STATE_INJECT;
		/* LGE_CHANGE_S, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
		//thunderc_hs_mic_bias_power(1);
		/* LGE_CHANGE_E, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
	}

	return state;
}

static char *thunderc_gpio_earsense_print_state(int state)
{
	return ear_state_string[state];
}

static int thunderc_gpio_earsense_sysfs_store(const char *buf, size_t size)
{
	int state;

	if (!strncmp(buf, "eject", size - 1))
		state = EAR_STATE_EJECT;
	else if (!strncmp(buf, "inject", size - 1))
		state = EAR_STATE_INJECT;
	else
		return -EINVAL;

	return state;
}

static unsigned thunderc_earsense_gpios[] = {
	GPIO_EAR_SENSE,
};

static struct lge_gpio_switch_platform_data thunderc_earsense_data = {
	.name = "h2w",
	.gpios = thunderc_earsense_gpios,
	.num_gpios = ARRAY_SIZE(thunderc_earsense_gpios),
	.irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.wakeup_flag = 1,
	.work_func = thunderc_gpio_earsense_work_func,
	.print_state = thunderc_gpio_earsense_print_state,
	.sysfs_store = thunderc_gpio_earsense_sysfs_store,
};

static struct platform_device thunderc_earsense_device = {
	.name   = "lge-switch-gpio",
	.id = 1,
	.dev = {
		.platform_data = &thunderc_earsense_data,
	},
};

static struct platform_device *thunderc_misc_devices[] __initdata = {
	/* LGE_CHANGE
	 * ADD VS740 BATT DRIVER IN THUNDERC
	 * 2010-05-13, taehung.kim@lge.com
	 */
	&msm_batt_device, 
	&msm_device_pmic_leds,
	&android_vibrator_device,
	&thunderc_earsense_device,
};

void __init lge_add_misc_devices(void)
{
	platform_add_devices(thunderc_misc_devices, ARRAY_SIZE(thunderc_misc_devices));
}
