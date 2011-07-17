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
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <mach/lg_diagcmd.h>
#include <mach/lg_diag_keypress.h>
#include <linux/input.h>

#define HS_RELEASE_K 0xFFFF

extern PACK(void *) diagpkt_alloc(diagpkt_cmd_code_type code,
				  unsigned int length);
extern unsigned int LGF_KeycodeTrans(word input);
//extern void Send_Touch( unsigned int x, unsigned int y);

static unsigned saveKeycode = 0;

/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-11, virtual key */
extern struct input_dev *get_ats_input_dev(void);

void SendKeyToInputDevie(unsigned int code, int value)
{
	struct input_dev *ats_input_dev;

	/* LGE_CHANGE [james.jang@lge.com] 2010-09-05, block it */
	// printk("keycode = %d, value = %d\n", code, value);

	/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-11, virtual key */
	ats_input_dev = get_ats_input_dev();
	if (ats_input_dev)
		input_report_key(ats_input_dev, code, value);
}

void SendKey(unsigned int keycode, unsigned char bHold)
{
	switch(keycode) {
		/* camera focus */
	case 32:
		keycode = 247;
		break;

		/* camera capture */
	case 65:
		keycode = 212;
		break;
	}
	 
	if(keycode != HS_RELEASE_K)
		SendKeyToInputDevie(keycode, 1);	// press event

	if(bHold) {
		saveKeycode = keycode;
	} 
	else {
		if (keycode != HS_RELEASE_K)
			SendKeyToInputDevie(keycode, 0);	// release  event
		else
			SendKeyToInputDevie(saveKeycode, 0);	// release  event
	}
}

PACK(void *) LGF_KeyPress(PACK(void *)req_pkt_ptr,	/* pointer to request packet  */
			  uint16 pkt_len)
{				/* length of request packet   */
	DIAG_HS_KEY_F_req_type *req_ptr =
	    (DIAG_HS_KEY_F_req_type *) req_pkt_ptr;
	DIAG_HS_KEY_F_rsp_type *rsp_ptr;
	unsigned int keycode = 0;
	const int rsp_len = sizeof(DIAG_HS_KEY_F_rsp_type);

	rsp_ptr =
	    (DIAG_HS_KEY_F_rsp_type *)diagpkt_alloc(DIAG_HS_KEY_F, rsp_len);

	rsp_ptr->key = req_ptr->key;
	keycode = req_ptr->key;

	if(keycode == 0xff)
		keycode = HS_RELEASE_K;

	if(keycode != 0x40)
		SendKey(keycode, req_ptr->hold);

	return (rsp_ptr);
}
EXPORT_SYMBOL(LGF_KeyPress);
