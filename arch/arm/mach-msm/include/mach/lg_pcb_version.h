/* arch/arm/mach-msm/include/mach/lg_pcb_version.h
 *
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
 *
 */

#ifndef __ASM__ARCH_MSM_LG_PCB_VERSION_H
#define __ASM__ARCH_MSM_LG_PCB_VERSION_H

enum hw_pcb_version_type{
	HW_PCB_UNKNOWN,
	HW_PCB_REV_A,
	HW_PCB_REV_B,
	HW_PCB_REV_C,
	HW_PCB_REV_D,
	HW_PCB_REV_E,
	HW_PCB_REV_F,
	HW_PCB_REV_G,
	HW_PCB_REV_10,
	HW_PCB_REV_11,
	HW_PCB_REV_12,
	HW_PCB_REV_13,
	HW_PCB_REV_14,
	HW_PCB_REV_15,
	HW_PCB_REV_16,
	HW_PCB_REV_MAX
};

int lg_get_board_pcb_version(void);
void lg_set_hw_version_string(char *pcb_version);

#endif
