/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef __MSM_BATTERY_THUNDERC_H__
#define __MSM_BATTERY_THUNDERC_H__

#include <linux/power_supply.h>

struct batt_info {
	u32 valid_batt_id;
	u32 batt_therm;
	u32 batt_temp;
#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
	u32 chg_current;
	u32 batt_thrm_state;
#endif
};

struct pseudo_batt_info_type {
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};

enum {
	POWER_SUPPLY_PROP_BATTERY_ID_CHECK = POWER_SUPPLY_PROP_SERIAL_NUMBER + 1,
	POWER_SUPPLY_PROP_BATTERY_TEMP_ADC,
	POWER_SUPPLY_PROP_PSEUDO_BATT,
	POWER_SUPPLY_PROP_CHARGING_TIMER,
	POWER_SUPPLY_PROP_BLOCK_CHARGING,
#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
	POWER_SUPPLY_PROP_BATTERY_THRM_STATE,
#if defined(CONFIG_LGE_THERM_NO_STOP_CHARGING)
	POWER_SUPPLY_PROP_THERM_NO_STOP_CHARGING,
#endif
#endif
};

#endif
