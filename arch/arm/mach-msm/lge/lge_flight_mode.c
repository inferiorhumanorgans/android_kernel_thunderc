/*
 *
 *
 *
 */

#include <linux/platform_device.h>

extern int lg_get_power_check_mode(void);
extern int lg_get_flight_mode(void);

static int power_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int power_check_mode = 0;
	power_check_mode = (int)lg_get_power_check_mode();

	printk("[INFORPC] : %s, power = %d\n", __func__, power_check_mode);

	return sprintf(buf, "%d\n", power_check_mode);
}

static int power_check_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
/*	int flight_mode = 0;
	flight_mode = (int)lg_get_flight_mode();

	printk("[INFORPC] : %s, flight mode = %d\n", __func__, flight_mode);

	return flight_mode;
*/
	return 1;
}

DEVICE_ATTR(check_power_mode, 0777, power_check_show, power_check_store);

static unsigned int g_flight = 0;
static int flight_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#if 1
	int flight_mode = 0;
	flight_mode = (int)lg_get_flight_mode();

	printk("[INFORPC] : %s, flight mode = %d\n", __func__, flight_mode);

	return sprintf(buf, "%d\n", flight_mode);
#else
	printk("[INFORPC]%s\n", __func__);

	return sprintf(buf, "%d\n", g_flight);
#endif	
}

static int flight_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int test_result=0;

	printk("[INFORPC]%s\n", __func__);
	
	sscanf(buf, "%d\n", &test_result);
	g_flight = test_result;	
	
	return count;
}
DEVICE_ATTR(flight, 0777, flight_show, flight_store);

static int __init lge_tempdevice_probe(struct platform_device *pdev)
{
	int err;

	printk("[INFORPC]%s\n", __func__);

	err = device_create_file(&pdev->dev, &dev_attr_check_power_mode);
	if (err < 0) {
		printk("%s : Cannot create the sysfs\n", __func__);
	}
	
	err = device_create_file(&pdev->dev, &dev_attr_flight);
	if (err < 0) {
		printk("%s : Cannot create the sysfs\n", __func__);
	}
}

static struct platform_device lgetemp_device = {
	.name = "autoall",
	.id		= -1,
};

static struct platform_driver this_driver = {
	.probe = lge_tempdevice_probe,
	.driver = {
		.name = "autoall",
	},
};

int __init lge_tempdevice_init(void)
{
	printk("%s\n", __func__);
	platform_device_register(&lgetemp_device);

	return platform_driver_register(&this_driver);
}

module_init(lge_tempdevice_init);
MODULE_DESCRIPTION("Just temporal code for PV");
MODULE_AUTHOR("MoonCheol Kang <neo.kang@lge.com>");
MODULE_LICENSE("GPL");
