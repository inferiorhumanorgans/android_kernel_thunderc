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
/* LGE_CHANGE_S 2010-09-05, taehung.kim@lge.com
 * support to read nv flag(manual mode on)
 */
unsigned lge_get_nv_manual_mode_state(void)
{
	int err;
	unsigned manual_mode=-1;
	unsigned cmd_manual_mode=2;

	err = msm_proc_comm(PCOM_CUSTOMER_CMD2, &manual_mode, &cmd_manual_mode);
	if (err < 0) {
		pr_err("%s: msm_proc_comm(PCOM_CUSTOMER_CMD2) failed\n",
		       __func__);
		return err;
	}

	return manual_mode;
}
EXPORT_SYMBOL(lge_get_nv_manual_mode_state);

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
