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
#include <linux/string.h>
#include <linux/kernel.h>

#include <mach/lg_pcb_version.h>

#ifdef CONFIG_LGE_PCB_VERSION  /* LG_FW_PCB_VERSION */

extern int lg_get_hw_version(void);

int lg_get_board_pcb_version(void)
{
    static int pcb_version = HW_PCB_UNKNOWN;
    
    if(pcb_version == HW_PCB_UNKNOWN)
    {
        pcb_version = lg_get_hw_version();
    }
	printk(KERN_INFO "lg_get_board_pcb_version - pcd version=%d\n",pcb_version);

    return pcb_version;
}

void lg_set_hw_version_string(char *pcb_version)
{
	enum hw_pcb_version_type hw_version;

	hw_version=lg_get_board_pcb_version();
	
	switch(hw_version)			
	{
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

//20100929 yongman.kwon@lge.com [MS690] for check prl version for wifi on/off [START]
//LG_FW_CHECK_PRL_VERSION
extern unsigned short lg_get_prl_version(void);
void lg_set_prl_version_string(char * prl_version)
{
	sprintf(prl_version, "%d", lg_get_prl_version());
}
//20100929 yongman.kwon@lge.com [MS690] for check prl version for wifi on/off [END]

//20101130 yongman.kwon@lge.com [MS690] support HITACHI & SHARP [START]
#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
extern int g_mddi_lcd_probe;
void lg_get_lcd_version_string(char * lcd_version)
{
	sprintf(lcd_version, "%d", g_mddi_lcd_probe);
}
#endif
//20101130 yongman.kwon@lge.com [MS690] support HITACHI & SHARP [END]
