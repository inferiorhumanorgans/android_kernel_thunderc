/* arch/arm/mach-msm/include/mach/lg_pcb_version.h
 *
 * Copyright (C) 2009, 2010 LGE, Inc.
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

#include <linux/string.h>
#include <mach/lg_pcb_version.h>

#ifdef CONFIG_LGE_PCB_VERSION  /* LG_FW_PCB_VERSION */

extern int lg_get_hw_version(void);

int lg_get_board_pcb_version(void)
{
	static int pcb_version = HW_PCB_UNKNOWN;

	if(pcb_version == HW_PCB_UNKNOWN) {
		pcb_version = lg_get_hw_version();
	}

	return pcb_version;
}

void lg_set_hw_version_string(char *pcb_version)
{
	enum hw_pcb_version_type hw_version;

	hw_version=lg_get_board_pcb_version();
	
	switch(hw_version) {
		case HW_PCB_REV_A: 				
			strcpy(pcb_version, "A");
			break;
		case HW_PCB_REV_B: 
			strcpy(pcb_version, "B");
			break;
		case HW_PCB_REV_C: 	
			strcpy(pcb_version, "C");
			 break;
		case HW_PCB_REV_D: 
			strcpy(pcb_version, "D");
			 break;
		case HW_PCB_REV_E: 	
			strcpy(pcb_version, "E");
			 break;
		case HW_PCB_REV_F:
			strcpy(pcb_version, "F");
			 break;
		case HW_PCB_REV_G:
			strcpy(pcb_version, "G");
			 break;
		case HW_PCB_REV_10:
			strcpy(pcb_version, "1.0");
			 break;
		case HW_PCB_REV_11:	
			strcpy(pcb_version, "1.1");
			 break;
		case HW_PCB_REV_12:
			strcpy(pcb_version, "1.2");
			 break;
		case HW_PCB_REV_13:
			strcpy(pcb_version, "1.3");
			 break;
		case HW_PCB_REV_14:	
			strcpy(pcb_version, "1.4");
			 break;
		case HW_PCB_REV_15:	
			strcpy(pcb_version, "1.5");
			 break;
		case HW_PCB_REV_16:	
			strcpy(pcb_version, "1.6");
			 break;
		default:	
			strcpy(pcb_version, "Unknown");
			 break;
	}
}

#endif /* CONFIG_LGE_PCB_VERSION */

