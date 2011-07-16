/*
 * RTC subsystem, initialize system time on startup
 *
 * Copyright (C) 2005 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/rtc.h>

/* IMPORTANT: the RTC only stores whole seconds. It is arbitrary
 * whether it stores the most close value or the value with partial
 * seconds truncated. However, it is important that we use it to store
 * the truncated value. This is because otherwise it is necessary,
 * in an rtc sync function, to read both xtime.tv_sec and
 * xtime.tv_nsec. On some processors (i.e. ARM), an atomic read
 * of >32bits is not possible. So storing the most close value would
 * slow down the sync API. So here we have the truncated value and
 * the best guess is to add 0.5s.
 */

#ifdef CONFIG_LGE_RTC_INTF_ALARM_SYNC
extern int alarm_set_rtc(struct timespec new_time);
extern bool alarm_need_to_set(void);
#endif

int rtc_hctosys(void)
{
	int err;
	struct rtc_time tm;
	struct rtc_device *rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

	if (rtc == NULL) {
		printk("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -ENODEV;
	}

	err = rtc_read_time(rtc, &tm);
	if (err == 0) {
		err = rtc_valid_tm(&tm);
		if (err == 0) {
			struct timespec tv;

			tv.tv_nsec = NSEC_PER_SEC >> 1;

			rtc_tm_to_time(&tm, &tv.tv_sec);

#ifdef CONFIG_LGE_RTC_INTF_ALARM_SYNC
			if (alarm_need_to_set()) {
				//printk("%s: called alarm_set_rtc(tv)\n", __func__);
				alarm_set_rtc(tv);
			} else {
				//printk("%s: called do_settimeofday(&tv)\n", __func__);
				do_settimeofday(&tv);
			}
#else
			do_settimeofday(&tv);
#endif

		}
		else
			dev_err(rtc->dev.parent,
				"hctosys: invalid date/time\n");
	}
	else
		dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");

	rtc_class_close(rtc);

	return 0;
}

late_initcall(rtc_hctosys);
