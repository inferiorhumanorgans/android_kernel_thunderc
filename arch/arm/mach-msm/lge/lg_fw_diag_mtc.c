/* arch/arm/mach-msm/lge/lg_fw_diag_mtc.c
 *
 * Copyright (C) 2009,2010 LGE, Inc.
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
#include <mach/lg_diagcmd.h>
#include <mach/lg_fw_diag_mtc.h>
#include <mach/lge_base64.h>

#include <linux/unistd.h>	/*for open/close */
#include <linux/fcntl.h>	/*for O_RDWR */

#include <linux/fb.h>		/* to handle framebuffer ioctls */
#include <linux/ioctl.h>
#include <linux/uaccess.h>

#include <linux/syscalls.h>	//for sys operations

#include <linux/input.h>	// for input_event
#include <linux/fs.h>		// for file struct
#include <linux/types.h>	// for ssize_t
#include <linux/input.h>	// for event parameters
#include <linux/jiffies.h>
#if 1 //LG_FW_MTC_GISELE
#include <linux/crc-ccitt.h>
#include <linux/delay.h>

#define ESC_CHAR     0x7D
#define CONTROL_CHAR 0x7E
#define ESC_MASK     0x20

#define CRC_16_L_SEED           0xFFFF

#define CRC_16_L_STEP(xx_crc, xx_c) \
	crc_ccitt_byte(xx_crc, xx_c)

void *lg_diag_mtc_req_pkt_ptr;
unsigned short lg_diag_mtc_req_pkt_length;

#endif //LG_FW_MTC_GISELE

#ifndef LG_FW_DUAL_TOUCH
#define LG_FW_DUAL_TOUCH
#endif

/*
 * EXTERNAL FUNCTION AND VARIABLE DEFINITIONS
 */
extern PACK(void *) diagpkt_alloc(diagpkt_cmd_code_type code,
				  unsigned int length);
extern PACK(void *) diagpkt_free(PACK(void *)pkt);
extern void send_to_arm9(void *pReq, void *pRsp);

#if defined (CONFIG_MACH_MSM7X27_THUNDERC) || defined (LG_FW_ATS_ETA_MTC_KEY_LOGGING)
extern unsigned int ats_mtc_log_mask;
extern void diagpkt_commit(PACK(void *)pkt);
#endif /*LG_FW_ATS_ETA_MTC_KEY_LOGGING */


/*
 * LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE
 *
 * This section contains local definitions for constants, macros, types,
 * variables and other items needed by this module.
 */
#if defined (CONFIG_MACH_MSM7X27_THUNDERC) || defined (LG_FW_ATS_ETA_MTC_KEY_LOGGING)
#define JIFFIES_TO_MS(t) ((t) * 1000 / HZ)
#endif /*LG_FW_ATS_ETA_MTC_KEY_LOGGING */

extern mtc_user_table_entry_type mtc_mstr_tbl[MTC_MSTR_TBL_SIZE];

unsigned char g_diag_mtc_check = 0;
static char mtc_running = 0;

static mtc_lcd_info_type lcd_info;

extern int diagchar_ioctl(unsigned int iocmd, unsigned long ioarg);
static int ats_mtc_set_lcd_info(mtc_scrn_id_type ScreenType);
static ssize_t read_framebuffer(byte * pBuf);

typedef enum {
	/*
	 *    0       Move pointer to the specified location
	 *    1       Move the pointer by the specified values
	 *    2       Tap at the current location
	 *    3       Double tap at the current location
	 *    4       Touch down at the current location
	 *    5       Touch up at the current location
	 *                                             */
	MOVE_ABSOLUTE,
	MOVE_RELATIVE,
	TAP,
	DOUBLE_TAP,
	TOUCH_DOWN,
	TOUCH_UP
} TOUCH_ACTIONS;

#if 1 //LG_FW_MTC_GISELE
/*
 * FUNCTION	add_hdlc_packet.
 */
static void add_hdlc_packet(struct mtc_data_buffer *mb, char data)
{
	mb->data[mb->data_length++] = data;

	//if (mb->data_length == BUFFER_MAX_SIZE) {
	if (mb->data_length >= BUFFER_MAX_SIZE) {
		mb->data_length = BUFFER_MAX_SIZE;

		msleep(10);

		if (diagchar_ioctl (DIAG_IOCTL_BULK_DATA, (unsigned long)mb)) {
			printk(KERN_ERR "[MTC] %s: diagchar_ioctl error\n", __func__);
		} 

		mb->data_length = 0;
	}
}

/*
 * FUNCTION	add_hdlc_esc_packet.
 */
static void add_hdlc_esc_packet(struct mtc_data_buffer *mb, char data)
{
	if (data == ESC_CHAR || data == CONTROL_CHAR) {
		add_hdlc_packet(mb, ESC_CHAR);
		add_hdlc_packet(mb, (data ^ ESC_MASK));
	} 
	else {
		add_hdlc_packet(mb, data);
	}
}

/*
 * FUNCTION	mtc_send_hdlc_packet.
 */
static void mtc_send_hdlc_packet(byte * pBuf, int len)
{
	int i;
	struct mtc_data_buffer *mb;
	word crc = CRC_16_L_SEED;

	mb = kzalloc(sizeof(struct mtc_data_buffer), GFP_ATOMIC);
	if (mb == NULL) {
		printk(KERN_ERR "[MTC] %s: failed to alloc memory\n", __func__);
		return;
	}

	//Generate crc data.
	for (i = 0; i < len; i++) {
		add_hdlc_esc_packet(mb, pBuf[i]);
		crc = CRC_16_L_STEP(crc, (word) pBuf[i]);
	}

	crc ^= CRC_16_L_SEED;
	add_hdlc_esc_packet(mb, ((unsigned char)crc));
	add_hdlc_esc_packet(mb, ((unsigned char)((crc >> 8) & 0xFF)));
	add_hdlc_packet(mb, CONTROL_CHAR);

	if (diagchar_ioctl(DIAG_IOCTL_BULK_DATA, (unsigned long)mb)) {
		printk(KERN_ERR "[MTC] %s: ioctl ignored\n", __func__);
	}

	kfree(mb);
	mb = NULL;
}


/*
 * FUNCTION	translate_key_code.
 */
dword translate_key_code(dword keycode)
{
	if (keycode == KERNELFOCUSKEY)
		return KERNELCAMERAKEY;
	else
		return keycode;

}

/*
 * FUNCTION	mtc_send_key_log_packet.
 */
void mtc_send_key_log_packet(unsigned long keycode, unsigned long state)
{
	ext_msg_type msg;
	dword sendKeyValue = 0;

	/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04 [LS670]
	 * don't send a raw diag packet in running MTC
	 */
	if (mtc_running)
		return;

	memset(&msg, 0, sizeof(ext_msg_type));

	sendKeyValue = translate_key_code(keycode);

	msg.cmd_code = 121;
	msg.ts_type = 0;	//2;
	msg.num_args = 2;
	msg.drop_cnt = 0;
	//ts_get(&msg.time[0]); 
	msg.time[0] = 0;
	msg.time[1] = 0;
	msg.line_number = 261;
	msg.ss_id = 0;
	msg.ss_mask = 2;
	msg.args[0] = sendKeyValue;
	msg.args[1] = state;

	memcpy(&msg.code[0], "Debounced %d", sizeof("Debounced %d"));
	//msg.code[12] = '\0';

	memcpy(&msg.file_name[0], "DiagDaemon.c", sizeof("DiagDaemon.c"));
	//msg.fle_name[13] = '\0';

	mtc_send_hdlc_packet((byte *) & msg, sizeof(ext_msg_type));
}

EXPORT_SYMBOL(mtc_send_key_log_packet);
#endif //LG_FW_MTC_GISELE

/*
 * INTERNAL FUNCTION DEFINITIONS
 */
PACK(void *) LGF_MTCProcess(PACK(void *)req_pkt_ptr,/* pointer to request packet  */
			    unsigned short pkt_len)
{			/* length of request packet   */
	DIAG_MTC_F_req_type *req_ptr = (DIAG_MTC_F_req_type *) req_pkt_ptr;
	DIAG_MTC_F_rsp_type *rsp_ptr = NULL;
	mtc_func_type func_ptr = NULL;
	int nIndex = 0;
	g_diag_mtc_check = 1;
	mtc_running = 1;

	for (nIndex = 0; nIndex < MTC_MSTR_TBL_SIZE; nIndex++) {
		if (mtc_mstr_tbl[nIndex].cmd_code == req_ptr->hdr.sub_cmd) {
			if (mtc_mstr_tbl[nIndex].which_procesor ==
			    MTC_ARM11_PROCESSOR)
				func_ptr = mtc_mstr_tbl[nIndex].func_ptr;

			break;
		} 
		else if (mtc_mstr_tbl[nIndex].cmd_code == MTC_MAX_CMD)
			break;
		else
			continue;
	}

	if (func_ptr != NULL) {
		printk(KERN_INFO "[MTC]cmd_code : [0x%X], sub_cmd : [0x%X]\n",
		       req_ptr->hdr.cmd_code, req_ptr->hdr.sub_cmd);
		rsp_ptr = func_ptr((DIAG_MTC_F_req_type *) req_ptr);
	} else
		send_to_arm9((void *)req_ptr, (void *)rsp_ptr);

	/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04 [LS670] */
	mtc_running = 0;

	return (rsp_ptr);
}

EXPORT_SYMBOL(LGF_MTCProcess);

DIAG_MTC_F_rsp_type *mtc_info_req_proc(DIAG_MTC_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_MTC_F_rsp_type *pRsp;

	printk(KERN_INFO "[MTC]mtc_info_req_proc\n");

	rsp_len = sizeof(mtc_info_rsp_type);
	printk(KERN_INFO "[MTC] mtc_info_req_proc rsp_len :(%d)\n", rsp_len);

	pRsp = (DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
		/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04, null check */
		return pRsp;
	}

	pRsp->hdr.cmd_code = DIAG_MTC_F;
	pRsp->hdr.sub_cmd = MTC_INFO_REQ_CMD;

	if (pReq->mtc_req.info.screen_id == MTC_SUB_LCD)	// N/A
	{
		printk(KERN_ERR "[MTC]mtc_info_req_proc, "
				"sub lcd is not supported\n");
		return pRsp;
	}

	if (pReq->mtc_req.info.screen_id == MTC_MAIN_LCD) {
		ats_mtc_set_lcd_info(MTC_MAIN_LCD);

		pRsp->mtc_rsp.info.scrn_id = MTC_MAIN_LCD;

	}
#ifdef LGE_USES_SUBLCD
	else if (pReq->mtc_req.info.screen_id == MTC_SUB_LCD) {
		ats_mtc_set_lcd_info(MTC_SUB_LCD);
		pRsp->mtc_rsp.info.scrn_id = MTC_SUB_LCD;
	}
#endif
	else {
		printk(KERN_ERR "[MTC]mtc_info_req_proc, "
				"unknown screen_id type : %d\n",
		       pRsp->mtc_rsp.info.scrn_id);
		return pRsp;
	}

	pRsp->mtc_rsp.info.scrn_width = lcd_info.width_max;
	pRsp->mtc_rsp.info.scrn_height = lcd_info.height_max;
	pRsp->mtc_rsp.info.bits_pixel = lcd_info.bits_pixel;

	return pRsp;
}

static int ats_mtc_set_lcd_info(mtc_scrn_id_type ScreenType)
{
	struct fb_var_screeninfo fb_varinfo;
	int fbfd;

	if ((fbfd = sys_open("/dev/graphics/fb0", O_RDWR, 0)) == -1) {
		printk(KERN_ERR "[MTC]ats_mtc_set_lcd_info, Can't open %s\n",
		       "/dev/graphics/fb0");
		return 0;
	}

	memset((void *)&fb_varinfo, 0, sizeof(struct fb_var_screeninfo));
	if (sys_ioctl(fbfd, FBIOGET_VSCREENINFO, (long unsigned int)&fb_varinfo)
	    < 0) {
		printk(KERN_ERR "[MTC]ats_mtc_set_lcd_info, ioctl failed\n");
		return 0;
	}
	printk(KERN_INFO
	       "[MTC]ats_mtc_set_lcd_info, fbvar.xres= %d, fbvar.yres= %d, fbvar.pixel= %d\n",
	       fb_varinfo.xres, fb_varinfo.yres, fb_varinfo.bits_per_pixel);

	sys_close(fbfd);

	if (ScreenType == MTC_MAIN_LCD) {
		lcd_info.id = MTC_MAIN_LCD;
		lcd_info.width_max = fb_varinfo.xres;
		lcd_info.height_max = fb_varinfo.yres;
	}
#if defined (LG_SUBLCD_INCLUDE)
	else if (ScreenType == MTC_SUB_LCD) {
		lcd_info.id = MTC_SUB_LCD;
		lcd_info.width_max = fb_varinfo.xres;
		lcd_info.height_max = fb_varinfo.yres;
	}
#endif

	//To Get the Bits Depth
	lcd_info.bits_pixel = fb_varinfo.bits_per_pixel;

	if (lcd_info.bits_pixel == MTC_BIT_65K) {
		lcd_info.mask.blue = MTC_65K_CMASK_BLUE;
		lcd_info.mask.green = MTC_65K_CMASK_GREEN;
		lcd_info.mask.red = MTC_65K_CMASK_RED;
	} else if (lcd_info.bits_pixel == MTC_BIT_262K) {
		lcd_info.mask.blue = MTC_262K_CMASK_BLUE;
		lcd_info.mask.green = MTC_262K_CMASK_GREEN;
		lcd_info.mask.red = MTC_262K_CMASK_RED;
	} else	// default 16 bit
	{
		lcd_info.bits_pixel = MTC_BIT_65K;
		lcd_info.mask.blue = MTC_65K_CMASK_BLUE;
		lcd_info.mask.green = MTC_65K_CMASK_GREEN;
		lcd_info.mask.red = MTC_65K_CMASK_RED;
	}

	lcd_info.degrees = 0;	//No rotation .. manual Data    
	return 1;
}

// from GISELE
DIAG_MTC_F_rsp_type *mtc_capture_screen(DIAG_MTC_F_req_type * pReq)
{
	unsigned int rsp_len;
	static DIAG_MTC_F_rsp_type *pCaputureRsp;
	ssize_t bmp_size;

	printk(KERN_INFO "[MTC]mtc_capture_screen\n");

	/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04, 
	 * allocation the memory for bmp_data
	 */
	rsp_len = sizeof(mtc_capture_rsp_type) + MTC_SCRN_BUF_SIZE_MAX;
	printk(KERN_INFO "[MTC] mtc_capture_screen rsp_len :(%d)\n", rsp_len);

	if (pCaputureRsp == NULL) {
		printk(KERN_ERR "[MTC] MEMORY ALLOC\n");
		pCaputureRsp = 
			(DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
		if (pCaputureRsp == NULL) {
			printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
			/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04, null check */
			return pCaputureRsp;
		}
	}

	pCaputureRsp->hdr.cmd_code = DIAG_MTC_F;
	pCaputureRsp->hdr.sub_cmd = MTC_CAPTURE_REQ_CMD;

	pCaputureRsp->mtc_rsp.capture.scrn_id = lcd_info.id;
	pCaputureRsp->mtc_rsp.capture.bmp_width = lcd_info.width_max;
	pCaputureRsp->mtc_rsp.capture.bmp_height = lcd_info.height_max;
	pCaputureRsp->mtc_rsp.capture.bits_pixel = lcd_info.bits_pixel;
	pCaputureRsp->mtc_rsp.capture.mask.blue = lcd_info.mask.blue;
	pCaputureRsp->mtc_rsp.capture.mask.green = lcd_info.mask.green;
	pCaputureRsp->mtc_rsp.capture.mask.red = lcd_info.mask.red;

	memset(pCaputureRsp->mtc_rsp.capture.bmp_data, 0,
	       MTC_SCRN_BUF_SIZE_MAX);
	bmp_size = read_framebuffer(pCaputureRsp->mtc_rsp.capture.bmp_data);
	printk(KERN_INFO "[MTC]mtc_capture_screen, "
			"Read framebuffer & Bmp convert complete.. %d\n",
	       (int)bmp_size);

	mtc_send_hdlc_packet((byte *) & pCaputureRsp->mtc_rsp.capture,
			     (int)rsp_len);
	return NULL; // no response packet
}

static ssize_t read_framebuffer(byte * pBuf)
{
	struct file *phMscd_Filp = NULL;
	ssize_t read_size = 0;

	mm_segment_t old_fs = get_fs();

	set_fs(get_ds());

	phMscd_Filp = filp_open("/dev/graphics/fb0", O_RDONLY | O_LARGEFILE, 0);

	if (!phMscd_Filp)
		printk("open fail screen capture \n");

	read_size = phMscd_Filp->f_op->read(phMscd_Filp, 
			pBuf, MTC_SCRN_BUF_SIZE_MAX, &phMscd_Filp->f_pos);
	filp_close(phMscd_Filp, NULL);
	set_fs(old_fs);

	return read_size;
}

#if defined (CONFIG_MACH_MSM7X27_THUNDERC) || defined (LG_FW_ATS_ETA_MTC_KEY_LOGGING)
static char eta_prev_action = 0xff;
struct ats_mtc_key_log_type ats_mtc_key_log;
extern unsigned int ats_mtc_log_mask;

void ats_eta_mtc_key_logging(int scancode, unsigned char keystate)
{

	/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04 [LS670]
	 * don't send a raw diag packet in running MTC
	 */
	if (mtc_running)
		return;

	/*
	23 bytes for key log -> will be encoded to 32 byte (base64)
	CMD_CODE		1 (0xF0)
	SUB_CMD 	1 (0x08)
	LOG_ID			1 (1 key, 2 touch)
	LOG_LEN 		2 (data length in bytes)
	LOG_DATA		LOG_LEN = 18
		- TIME		8 (timestamp in milliseconds)
		- HOLD		1 (Press or release)
		- KEYCODE	1
		- ACTIVE_UIID 8 (Activated UI ID)
	*/

	if((ats_mtc_log_mask&0x00000001) != 0) { /* ETA_LOGITEM_KEY */
		ats_mtc_key_log.log_id = 1; //LOG_ID, 1 key, 2 touch
		ats_mtc_key_log.log_len = 18; //LOG_LEN

		switch(scancode) {
			/* camera focus */
			case 247:
				scancode = 32;
				break;

			/* camera capture */
			case 212:
				scancode = 65;
				break;
		}

		ats_mtc_key_log.x_hold = (unsigned int)keystate; // hold
		ats_mtc_key_log.y_code = (unsigned int)scancode; // key code

		printk(KERN_INFO "[ETA] key code 0x%X, hold : %d \n",scancode, keystate);
		/*
		if(g_diag_mtc_check == 0)
			ats_mtc_send_key_log_to_eta(&ats_mtc_key_log);
		else
		*/
		mtc_send_key_log_data(&ats_mtc_key_log);
	}

}
EXPORT_SYMBOL(ats_eta_mtc_key_logging);

/*	
 27 bytes for key log -> will be encoded to ?? byte (base64)
 CMD_CODE		1 (0xF0)
 SUB_CMD 		1 (0x08)
 LOG_ID			1 (1 key, 2 touch)
 LOG_LEN 		2 (data length in bytes)
 LOG_DATA		LOG_LEN = 22
 - TIME		8 (timestamp in milliseconds)
 - SCREEN_ID 1
 - ACTION	1 (Touch-action Type)
 - X 			2 (Absolute X Coordinate)
 - Y 			2 (Absolute Y Coordinate)
 - ACTIVE_UIID 8 (Activated UI ID)
*/
void ats_eta_mtc_touch_logging (int pendown, int x, int y)
{

	/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04 [LS670]
	 * don't send a raw diag packet in running MTC
	 */
	if (mtc_running)
		return;
	
	if((ats_mtc_log_mask&0x00000002) != 0) {
		ats_mtc_key_log.log_id = 2; //LOG_ID, 1 key, 2 touch
		ats_mtc_key_log.log_len = 22; //LOG_LEN

		if (!pendown) { // release
			ats_mtc_key_log.action = (unsigned char)TOUCH_UP;
			eta_prev_action = TOUCH_UP;
		}
		else { // down
			if(eta_prev_action == TOUCH_DOWN) {
				ats_mtc_key_log.action = (unsigned char)MOVE_ABSOLUTE;
				// do not need to response all move to events
				if(g_diag_mtc_check == 1)
					mdelay(50); 
			}
			else
				ats_mtc_key_log.action = (unsigned char)TOUCH_DOWN;
			eta_prev_action = TOUCH_DOWN;
		}

		ats_mtc_key_log.x_hold = x;
		ats_mtc_key_log.y_code = y;

		printk(KERN_INFO "[ETA] TOUCH X : %d, Y : %d \n", 
				ats_mtc_key_log.x_hold, ats_mtc_key_log.y_code);
    		/*
		if(g_diag_mtc_check == 0)
			ats_mtc_send_key_log_to_eta(&ats_mtc_key_log);
		else
		*/
		mtc_send_key_log_data(&ats_mtc_key_log);
	}
}
EXPORT_SYMBOL(ats_eta_mtc_touch_logging);

DIAG_MTC_F_rsp_type *mtc_logging_mask_req_proc(DIAG_MTC_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_MTC_F_rsp_type *pRsp;

	rsp_len = sizeof(mtc_log_req_type);
	printk(KERN_INFO "[MTC] mtc_logging_mask_req_proc rsp_len :(%d)\n",
	       rsp_len);
	pRsp = (DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
		return pRsp;
	}

	switch (pReq->mtc_req.log.mask) {
	case 0x00000000:	//ETA_LOGMASK_DISABLE_ALL:
	case 0xFFFFFFFF:	//ETA_LOGMASK_ENABLE_ALL:
	case 0x00000001:	//ETA_LOGITEM_KEY:
	case 0x00000002:	//ETA_LOGITEM_TOUCHPAD:
	case 0x00000003:	//ETA_LOGITME_KEYTOUCH:
		ats_mtc_log_mask = pReq->mtc_req.log.mask;
		pRsp->mtc_rsp.log.mask = ats_mtc_log_mask;
		break;

	default:
		ats_mtc_log_mask = 0x00000000;	// //ETA_LOGMASK_DISABLE_ALL
		pRsp->mtc_rsp.log.mask = ats_mtc_log_mask;
		break;
	}

	return pRsp;
}

void mtc_send_key_log_data(struct ats_mtc_key_log_type *p_ats_mtc_key_log)
{
	unsigned int rsp_len;
	DIAG_MTC_F_rsp_type *pRsp;

	rsp_len = sizeof(mtc_log_data_rsp_type);
	printk(KERN_INFO "[MTC] mtc_send_key_log_data rsp_len :(%d)\n",
	       rsp_len);
	pRsp = (DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
		/* LGE_CHANGE [dojip.kim@lge.com] 2010-06-04, null check */
		//diagpkt_commit(pRsp);
		return;
	}

	pRsp->hdr.cmd_code = DIAG_MTC_F;
	pRsp->hdr.sub_cmd = MTC_LOG_REQ_CMD;

	//LOG_ID, 1 key, 2 touch
	pRsp->mtc_rsp.log_data.log_id = p_ats_mtc_key_log->log_id;	
	//LOG_LEN
	pRsp->mtc_rsp.log_data.log_len = p_ats_mtc_key_log->log_len;	

	// key
	if (p_ats_mtc_key_log->log_id == 1)	{
		pRsp->mtc_rsp.log_data.log_type.log_data_key.time =
		    (unsigned long long)JIFFIES_TO_MS(jiffies);
		// hold
		pRsp->mtc_rsp.log_data.log_type.log_data_key.hold = 
		    (unsigned char)((p_ats_mtc_key_log->x_hold) & 0xFF);	
		//key code
		pRsp->mtc_rsp.log_data.log_type.log_data_key.keycode = 
		    ((p_ats_mtc_key_log->y_code) & 0xFF);	
		pRsp->mtc_rsp.log_data.log_type.log_data_key.active_uiid = 0;
	} 
	// touch
	else {
		pRsp->mtc_rsp.log_data.log_type.log_data_touch.time =
		    (unsigned long long)JIFFIES_TO_MS(jiffies);
		// MAIN LCD
		pRsp->mtc_rsp.log_data.log_type.log_data_touch.screen_id = 
		    (unsigned char)1;	
		pRsp->mtc_rsp.log_data.log_type.log_data_touch.action =
		    (unsigned char)p_ats_mtc_key_log->action;
		pRsp->mtc_rsp.log_data.log_type.log_data_touch.x =
		    (unsigned short)p_ats_mtc_key_log->x_hold;
		pRsp->mtc_rsp.log_data.log_type.log_data_touch.y =
		    (unsigned short)p_ats_mtc_key_log->y_code;
		pRsp->mtc_rsp.log_data.log_type.log_data_touch.active_uiid = 0;
	}

	diagpkt_commit(pRsp);
}

EXPORT_SYMBOL(mtc_send_key_log_data);

#endif /*LG_FW_ATS_ETA_MTC_KEY_LOGGING */

DIAG_MTC_F_rsp_type *mtc_null_rsp(DIAG_MTC_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_MTC_F_rsp_type *pRsp;

	rsp_len = sizeof(mtc_req_hdr_type);
	printk(KERN_INFO "[MTC] mtc_null_rsp rsp_len :(%d)\n", rsp_len);

	pRsp = (DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
	}

	pRsp->hdr.cmd_code = pReq->hdr.cmd_code;
	pRsp->hdr.sub_cmd = pReq->hdr.sub_cmd;

	return pRsp;
}

DIAG_MTC_F_rsp_type *mtc_execute(DIAG_MTC_F_req_type * pReq)
{
	int ret;
	char cmdstr[100];
	int fd;

	unsigned int req_len = 0;
	unsigned int rsp_len;
	DIAG_MTC_F_rsp_type *pRsp = NULL;
	unsigned char *mtc_cmd_buf_encoded = NULL;
	int lenb64 = 0;

	char *envp[] = {
		"HOME=/",
		"TERM=linux",
		NULL,
	};

	char *argv[] = {
		"/system/bin/mtc",
		cmdstr,
		NULL,
	};

	printk(KERN_INFO "[MTC]mtc_execute\n");

	switch (pReq->hdr.sub_cmd) {
	case MTC_KEY_EVENT_REQ_CMD:
		req_len = sizeof(mtc_key_req_type);
		rsp_len = sizeof(mtc_key_req_type);
		printk(KERN_INFO "[MTC] KEY_EVENT_REQ rsp_len :(%d)\n",
		       rsp_len);
		pRsp =
		    (DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
		if (pRsp == NULL) {
			printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
			return pRsp;
		}
		pRsp->mtc_rsp.key.hold = pReq->mtc_req.key.hold;
		pRsp->mtc_rsp.key.key_code = pReq->mtc_req.key.key_code;
		break;

	case MTC_TOUCH_REQ_CMD:
		req_len = sizeof(mtc_touch_req_type);
		rsp_len = sizeof(mtc_touch_req_type);
		printk(KERN_INFO "[MTC] TOUCH_EVENT_REQ rsp_len :(%d)\n",
		       rsp_len);
		pRsp =
		    (DIAG_MTC_F_rsp_type *) diagpkt_alloc(DIAG_MTC_F, rsp_len);
		if (pRsp == NULL) {
			printk(KERN_ERR "[MTC] diagpkt_alloc failed\n");
			return pRsp;
		}
		pRsp->mtc_rsp.touch.screen_id = pReq->mtc_req.touch.screen_id;
		pRsp->mtc_rsp.touch.action = pReq->mtc_req.touch.action;
		pRsp->mtc_rsp.touch.x = pReq->mtc_req.touch.x;
		pRsp->mtc_rsp.touch.y = pReq->mtc_req.touch.y;
		break;

	default:
		printk(KERN_ERR "[MTC] unknown sub_cmd : (%d)\n",
		       pReq->hdr.sub_cmd);
		break;
	}

	if (NULL == pRsp) {
		printk(KERN_ERR "[MTC] pRsp is Null\n");
		return pRsp;
	}

	pRsp->hdr.cmd_code = pReq->hdr.cmd_code;
	pRsp->hdr.sub_cmd = pReq->hdr.sub_cmd;

	mtc_cmd_buf_encoded = kmalloc(sizeof(unsigned char) * 50, GFP_KERNEL);
	memset(cmdstr, 0x00, 50);
	memset(mtc_cmd_buf_encoded, 0x00, 50);

	lenb64 =
	    base64_encode((char *)pReq, req_len, (char *)mtc_cmd_buf_encoded);

	fd = sys_open((const char __user *)"/system/bin/mtc", O_RDONLY, 0);
	if (fd < 0) {
		printk ("\n [MTC]can not open /system/bin/mtc "
				"- execute /system/bin/mtc\n");
		sprintf(cmdstr, "/system/bin/mtc ");
	} 
	else {
		memcpy((void *)cmdstr, (void *)mtc_cmd_buf_encoded, lenb64);
		/*
		printk ("[MTC] cmdstr[16] : %d, cmdstr[17] : %d, cmdstr[18] : %d",
		     cmdstr[16], cmdstr[17], cmdstr[18]);
		printk ("[MTC] cmdstr[19] : %d, cmdstr[20] : %d, cmdstr[21] : %d",
		     cmdstr[19], cmdstr[20], cmdstr[21]);
		*/
		printk("\n [MTC]execute /system/bin/mtc, %s\n", cmdstr);
		sys_close(fd);
	}
	// END: eternalblue@lge.com.2009-10-23

	printk(KERN_INFO "[MTC]execute mtc : data - %s\n\n", cmdstr);
	ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (ret != 0) {
		printk(KERN_ERR "[MTC]execute failed, ret = %d\n", ret);
	} 
	else {
		printk(KERN_INFO "[MTC]execute ok, ret = %d\n", ret);
	}

	kfree(mtc_cmd_buf_encoded);

	return pRsp;
}

EXPORT_SYMBOL(mtc_execute);

/*  USAGE (same as testmode
 *    1. If you want to handle at ARM9 side, 
 *       you have to insert fun_ptr as NULL and mark ARM9_PROCESSOR
 *    2. If you want to handle at ARM11 side , 
 *       you have to insert fun_ptr as you want and mark AMR11_PROCESSOR.
 */
mtc_user_table_entry_type mtc_mstr_tbl[MTC_MSTR_TBL_SIZE] = {
        /*sub_command		fun_ptr		which procesor*/
	{MTC_INFO_REQ_CMD, mtc_info_req_proc, MTC_ARM11_PROCESSOR},
	{MTC_CAPTURE_REQ_CMD, mtc_capture_screen, MTC_ARM11_PROCESSOR},
	{MTC_KEY_EVENT_REQ_CMD, mtc_execute, MTC_ARM11_PROCESSOR},
	{MTC_TOUCH_REQ_CMD, mtc_execute, MTC_ARM11_PROCESSOR},
#if defined (CONFIG_MACH_MSM7X27_THUNDERC) || defined (LG_FW_ATS_ETA_MTC_KEY_LOGGING)
	{MTC_LOGGING_MASK_REQ_CMD, mtc_logging_mask_req_proc,MTC_ARM11_PROCESSOR},
	{MTC_LOG_REQ_CMD, NULL, MTC_ARM11_PROCESSOR}, /*mtc_send_key_log_data */
#endif /*LG_FW_ATS_ETA_MTC_KEY_LOGGING */
	{MTC_PHONE_RESTART_REQ_CMD, NULL, MTC_ARM9_PROCESSOR},
	{MTC_FACTORY_RESET, mtc_null_rsp, MTC_ARM9_ARM11_BOTH},
	{MTC_PHONE_REPORT, NULL, MTC_ARM9_PROCESSOR},
	{MTC_PHONE_STATE, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_CAPTURE_PROP, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_NOTIFICATION_REQUEST, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_CUR_PROC_NAME_REQ_CMD, mtc_null_rsp, MTC_NOT_SUPPORTED},
	/*ETA command */
	{MTC_KEY_EVENT_UNIV_REQ_CMD, mtc_null_rsp, MTC_NOT_SUPPORTED},	
	{MTC_MEMORY_DUMP, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_BATTERY_POWER, NULL, MTC_ARM9_PROCESSOR},
	{MTC_BACKLIGHT_INFO, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_FLASH_MODE, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_MODEM_MODE, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_CELL_INFORMATION, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_HANDOVER, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_ERROR_CMD, mtc_null_rsp, MTC_NOT_SUPPORTED},
	{MTC_MAX_CMD, mtc_null_rsp, MTC_NOT_SUPPORTED},
};
