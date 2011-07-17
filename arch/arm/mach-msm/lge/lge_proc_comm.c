/* 
 * arch/arm/mach-msm/lge/lge_proc_comm.c
 *
 * Copyright (C) 2010 LGE, Inc
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <mach/board_lge.h>
#include "../proc_comm.h"

#if defined(CONFIG_LGE_DETECT_PIF_PATCH)
unsigned lge_get_pif_info(void)
{
	int err;
	unsigned pif_value = -1;
	unsigned cmd_pif = 8;

	err = msm_proc_comm(PCOM_CUSTOMER_CMD2, &pif_value, &cmd_pif);
	if (err < 0) {
		pr_err("%s: msm_proc_comm(PCOM_CUSTOMER_CMD2) failed\n",
		       __func__);
		return err;
	}
	
	return pif_value;
}
EXPORT_SYMBOL(lge_get_pif_info);

unsigned lge_get_lpm_info(void)
{
	int err;
	unsigned low_power_mode = 0;
	unsigned cmd_lpm = 7;
	
	err = msm_proc_comm(PCOM_CUSTOMER_CMD2, &low_power_mode, &cmd_lpm);
	if (err < 0) {
		pr_err("%s: msm_proc_comm(PCOM_CUSTOMER_CMD2) failed\n",
		       __func__);
		return err;
	}

	return low_power_mode;
}
EXPORT_SYMBOL(lge_get_lpm_info);
#endif

// LGE_CHANGE [dojip.kim@lge.com] 2010-08-04, power on status 
#if defined(CONFIG_LGE_GET_POWER_ON_STATUS)
/*
 * return value:
 *         PM_PWR_ON_EVENT_KEYPAD     0x1
 *         PM_PWR_ON_EVENT_RTC        0x2
 *         PM_PWR_ON_EVENT_CABLE      0x4
 *         PM_PWR_ON_EVENT_SMPL       0x8
 *         PM_PWR_ON_EVENT_WDOG       0x10
 *         PM_PWR_ON_EVENT_USB_CHG    0x20
 *         PM_PWR_ON_EVENT_WALL_CHG   0x40
 */
unsigned lge_get_power_on_status(void)
{
	int err;
	unsigned status;
	unsigned ftm;

	err = msm_proc_comm(PCOM_GET_POWER_ON_STATUS, &status, &ftm);
	if (err < 0) {
		pr_err("%s: msm_proc_comm(PCOM_GET_POWER_ON_STATUS) failed\n",
		       __func__);
		return err;
	}

	return status;
}
EXPORT_SYMBOL(lge_get_power_on_status);
#endif

// LGE_CHANGE [dojip.kim@lge.com] 2010-08-24, notify the power status
int lge_set_sleep_status(int status)
{
	int cmd_state = 1;
	int err;

	err = msm_proc_comm(PCOM_CUSTOMER_CMD2, &status, &cmd_state);
	if (err < 0) {
		pr_err("%s: msm_proc_comm(PCOM_CUSTOMER_CMD2) failed\n",
		       __func__);
		return err;
	}

	return status;
}
EXPORT_SYMBOL(lge_set_sleep_status);
