/* 
 * arch/arm/mach-msm/lge/lg_diag_testmode_sysfs.c
 *
 * Copyright (C) 2010 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License vpseudomeidion 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/limits.h>
#include <mach/board_lge.h>
#include <mach/lg_diagcmd.h>
#include <mach/lg_diag_testmode.h>

#define TESTMODE_DRIVER_NAME "testmode"

extern uint8_t if_condition_is_on_air_plain_mode;
static ssize_t sleep_flight_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int value = if_condition_is_on_air_plain_mode;
	if (value)
		printk("testmode sysfs: flight mode\n");

	return sprintf(buf, "%d\n", value);
}

static DEVICE_ATTR(sleep_flight, 0444, sleep_flight_show, NULL);

static int __devinit testmode_probe(struct platform_device *pdev)
{
	int ret;

	ret = device_create_file(&pdev->dev, &dev_attr_sleep_flight);
	if (ret < 0) {
		printk("device_create_file error!\n");
		return ret;
	}
	
	return ret;
}

static int __devexit testmode_remove(struct platform_device *pdev)
{	
	device_remove_file(&pdev->dev, &dev_attr_sleep_flight);

	return 0;
}

static struct platform_driver testmode_driver = {
	.probe = testmode_probe,
	.remove = __devexit_p(testmode_remove),
	.driver = {
		.name = TESTMODE_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init testmode_init(void)
{
	return platform_driver_register(&testmode_driver);
}
module_init(testmode_init);

static void __exit testmode_exit(void)
{
	platform_driver_unregister(&testmode_driver);
}
module_exit(testmode_exit);

MODULE_DESCRIPTION("TESTMODE Driver");
MODULE_LICENSE("GPL");
