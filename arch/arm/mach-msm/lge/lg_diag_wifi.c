/*
 * Copyright (c) 2010 LGE. All rights reserved.
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
 * Program : UDM 
 * Author : khlee
 * Date : 2010.01.26
 */
#include <linux/module.h>
#include <mach/lg_diag_wifi.h>
#include <mach/lg_diagcmd.h>
#include <mach/lg_diag_testmode.h>

extern PACK(void *) diagpkt_alloc (diagpkt_cmd_code_type code, unsigned int length);

PACK (void *)LGF_WIFI(
        PACK (void *)req_pkt_ptr,	/* pointer to request packet  */
        uint16	pkt_len )      /* length of request packet   */
{
	DIAG_LGE_WIFI_MAC_ADDRESS_req_tag *req_ptr = 
			(DIAG_LGE_WIFI_MAC_ADDRESS_req_tag *) req_pkt_ptr;
	DIAG_LGE_WIFI_MAC_ADDRESS_rsp_tag *rsp_ptr = NULL;

	printk(KERN_ERR "[WIFI] SubCmd=<%d>\n",req_ptr->sub_cmd);

	switch( req_ptr->sub_cmd )
	{
	default:
	      break;
	}

	return (rsp_ptr);	
}
EXPORT_SYMBOL(LGF_WIFI); 
