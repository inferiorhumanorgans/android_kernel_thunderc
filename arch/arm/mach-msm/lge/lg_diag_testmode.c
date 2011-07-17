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
#include <linux/input.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <mach/lg_diagcmd.h>
#include <mach/lg_diag_testmode.h>

#include "lg_fw_diag_communication.h"

static struct diagcmd_dev *diagpdev;

extern PACK(void *) diagpkt_alloc(diagpkt_cmd_code_type code,
				  unsigned int length);
extern PACK(void *) diagpkt_free(PACK(void *)pkt);
extern void send_to_arm9(void *pReq, void *pRsp);
extern testmode_user_table_entry_type testmode_mstr_tbl[TESTMODE_MSTR_TBL_SIZE];
/* LGE_CHANGE_S [sm.shim@lge.com] 2010-07-26, Testmode cmd 22 Key test merge from VS660 */
extern int diag_event_log_start(void);
extern int diag_event_log_end(void);
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-07-26, Testmode cmd 22 Key test merge from VS660 */
/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
extern void set_operation_mode(boolean isOnline);
extern struct input_dev* get_ats_input_dev(void);
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
// LGE_CHANGE [dojip.kim@lge.com] 2010-09-28, ftm boot 
extern void remote_set_ftm_boot(int info);

/* LGE_CHANGE [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
extern int boot_info;

#define KEY_TRANS_MAP_SIZE 70

typedef struct {
	word LG_common_key_code;
	unsigned int Android_key_code;
} keycode_trans_type;

#if 1	//LG_FW_MTC_GISELE
keycode_trans_type keytrans_table[KEY_TRANS_MAP_SIZE] = {
	{KERNELHOMEKEY, KERNELHOMEKEY},	//HOME
	{KERNELBACKKEY, KERNELBACKKEY},	//BACK
	{KERNELPPOWERKEY, KERNELPPOWERKEY},	//POWER
	{KERNELPVOLUPKEY, KERNELPVOLUPKEY},	//Volume Up
	{KERNELVOLDNKEY, KERNELVOLDNKEY},	//Volume Down
	{KERNELCAMERAKEY, KERNELCAMERAKEY},	//CAMERA
	{KERNELMENUKEY, KERNELMENUKEY},	//MENU
	{0x30, KEY_0}, {0x31, KEY_1}, {0x32, KEY_2}, {0x33, KEY_3},
	{0x34, KEY_4}, {0x35, KEY_5}, {0x36, KEY_6}, {0x37, KEY_7},
	{0x38, KEY_8}, {0x39, KEY_9},
	{0x2A, KEY_STAR}, {0x23, KEY_SHARP},
	{0x3E, 0x3E}, {0x3D, 0x3D},	//CALL, ENDCALL
	//HEADSET
};
#endif

struct statfs_local {
	__u32 f_type;
	__u32 f_bsize;
	__u32 f_blocks;
	__u32 f_bfree;
	__u32 f_bavail;
	__u32 f_files;
	__u32 f_ffree;
	__kernel_fsid_t f_fsid;
	__u32 f_namelen;
	__u32 f_frsize;
	__u32 f_spare[5];
};

/* ===============================================================================*/

PACK(void *) LGF_TestMode(PACK(void *)req_pkt_ptr, /* pointer to request packet  */
			  uint16 pkt_len) /* length of request packet   */
{ 
	DIAG_TEST_MODE_F_req_type *req_ptr =
			(DIAG_TEST_MODE_F_req_type *) req_pkt_ptr;
	DIAG_TEST_MODE_F_rsp_type *rsp_ptr;
	unsigned int rsp_len;
	testmode_func_type func_ptr = NULL;
	int nIndex = 0;

	diagpdev = diagcmd_get_dev();

	if (req_ptr->sub_cmd_code == TEST_MODE_FACTORY_RESET_CHECK_TEST) {
		rsp_len = sizeof(DIAG_TEST_MODE_F_rsp_type) -
			    sizeof(test_mode_rsp_type);
	}
	else
		rsp_len = sizeof(DIAG_TEST_MODE_F_rsp_type);

	rsp_ptr = (DIAG_TEST_MODE_F_rsp_type *) diagpkt_alloc(DIAG_TEST_MODE_F, 
			rsp_len);
	// LGE_CHANGE [dojip.kim@lge.com] 2010-10-10, null check
	if (NULL == rsp_ptr) {
		printk(KERN_ERR "LGF_TestMode: failed the memory allocation\n");
		return NULL;
	}

	rsp_ptr->sub_cmd_code = req_ptr->sub_cmd_code;
	rsp_ptr->ret_stat_code = TEST_OK_S;

	for (nIndex = 0; nIndex < TESTMODE_MSTR_TBL_SIZE; nIndex++) {
		if (testmode_mstr_tbl[nIndex].cmd_code == req_ptr->sub_cmd_code) {
			if (testmode_mstr_tbl[nIndex].which_procesor ==
			    ARM11_PROCESSOR)
				func_ptr = testmode_mstr_tbl[nIndex].func_ptr;
			break;
		}
	}

	if (func_ptr != NULL)
		return func_ptr(&(req_ptr->test_mode_req), rsp_ptr);
	else
		send_to_arm9((void *)req_ptr, (void *)rsp_ptr);

	return (rsp_ptr);
}

EXPORT_SYMBOL(LGF_TestMode);

void *linux_app_handler(test_mode_req_type * pReq,
			DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	diagpkt_free(pRsp);
	return 0;
}

void *not_supported_command_handler(test_mode_req_type * pReq,
				    DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	return pRsp;
}

/* LCD QTEST */
PACK(void *)LGF_LcdQTest(PACK(void *)req_pkt_ptr,	/* pointer to request packet  */
			 uint16 pkt_len)
{	/* length of request packet   */
	/* Returns 0 for executing lg_diag_app */
	return 0;
}

EXPORT_SYMBOL(LGF_LcdQTest);

/* TEST_MODE_BLUETOOTH_TEST */
void *LGF_TestModeBlueTooth(test_mode_req_type * pReq,
			    DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	printk(KERN_ERR "[_BTUI_] [%s:%d] BTSubCmd=<%d>\n", __func__, __LINE__,
	       pReq->bt);

	if (diagpdev != NULL) {
		update_diagcmd_state(diagpdev, "BT_TEST_MODE", pReq->bt);
		if (pReq->bt == 1)
			msleep(5900);	//6sec timeout
		else if (pReq->bt == 2)
			ssleep(1);
		else
			ssleep(3);
		pRsp->ret_stat_code = TEST_OK_S;
	} 
	else {
		printk(KERN_ERR "[_BTUI_] [%s:%d] BTSubCmd=<%d> ERROR\n",
		       __func__, __LINE__, pReq->bt);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

/* LGE_CHANGE_S [sm.shim@lge.com] 2010-07-27, 
 * Testmode cmd 83 Bluetooth RW merge from VS660 
 */
byte *pReq_valid_address(byte *pstr)
{
	int pcnt=0;
	byte value_pstr=0, *pstr_tmp;

	pstr_tmp = pstr;
	do {
		++pcnt;
		value_pstr = *(pstr_tmp++);
	} while(!('0'<=value_pstr && value_pstr<='9') && 
			!('a'<=value_pstr && value_pstr<='f') && 
			!('A'<=value_pstr && value_pstr<='F')&&(pcnt<BT_RW_CNT));

	return (--pstr_tmp);
}

byte g_bd_addr[BT_RW_CNT];

void* LGF_TestModeBlueTooth_RW(
        test_mode_req_type*	pReq,
        DIAG_TEST_MODE_F_rsp_type	*pRsp)
{

	byte *p_Req_addr;

	p_Req_addr = pReq_valid_address(pReq->bt_rw);
	
	if(!p_Req_addr) {
		pRsp->ret_stat_code = TEST_FAIL_S;
		return pRsp;
	}
	
	printk(KERN_ERR "[_BTUI_] [%s:%d] BTSubCmd=<%s>\n", __func__, __LINE__, p_Req_addr);

	if (diagpdev != NULL) {
		//250-83-0 bluetooth write
		if(strlen(p_Req_addr) > 0) {
			/* LGE_CHANGE [sm.shim@lge.com] 2010-08-03, merge from VS660 */
			//update_diagcmd_state(diagpdev, "BT_TEST_MODE_RW", 0);
			update_diagcmd_state(diagpdev, "BT_TEST_MODE_RW", (int)p_Req_addr);
			memset((void*)g_bd_addr, 0x00, BT_RW_CNT);
			memcpy((void*)g_bd_addr, p_Req_addr, BT_RW_CNT);
			msleep(5900); //6sec timeout
		}
		//250-83-1 bluetooth read
		else {
			update_diagcmd_state(diagpdev, "BT_TEST_MODE_RW", 1);
			if(strlen(g_bd_addr)==0) {
				pRsp->ret_stat_code = TEST_FAIL_S;
				return pRsp;
			}
			memset((void*)pRsp->test_mode_rsp.read_bd_addr, 0x00, BT_RW_CNT);
			memcpy((void*)pRsp->test_mode_rsp.read_bd_addr, g_bd_addr, BT_RW_CNT);
		}
		pRsp->ret_stat_code = TEST_OK_S;
	}
	else {
		printk(KERN_ERR "[_BTUI_] [%s:%d] BTSubCmd=<%d> ERROR\n", 
				__func__, __LINE__, (int)pReq->bt_rw);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-07-27, 
 * Testmode cmd 83 Bluetooth RW merge from VS660 
 */

void *LGF_TestPhotoSensor(test_mode_req_type * pReq,
			  DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (diagpdev != NULL) {
		update_diagcmd_state(diagpdev, "ALC", pReq->motor);
	} 
	else {
		printk("\n[%s] error MOTOR", __func__);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

void *LGF_TestMotor(test_mode_req_type * pReq, DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (diagpdev != NULL) {
		update_diagcmd_state(diagpdev, "MOTOR", pReq->motor);
	} 
	else {
		printk("\n[%s] error MOTOR", __func__);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

void *LGF_TestAcoustic(test_mode_req_type * pReq,
		       DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (diagpdev != NULL) {
		update_diagcmd_state(diagpdev, "ACOUSTIC", pReq->acoustic);
	} 
	else {
		printk("\n[%s] error ACOUSTIC", __func__);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

void *LGF_TestModeMP3(test_mode_req_type * pReq,
		      DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (diagpdev != NULL) {
		if (pReq->mp3_play == MP3_SAMPLE_FILE) {
			pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		}
		else {
			update_diagcmd_state(diagpdev, "MP3", pReq->mp3_play);
		}
	} else {
		printk("\n[%s] error MP3", __func__);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

void *LGF_TestModeSpeakerPhone(test_mode_req_type * pReq,
			       DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (diagpdev != NULL) {
		if ((pReq->speaker_phone == NOMAL_Mic1)
		    || (pReq->speaker_phone == NC_MODE_ON)
		    || (pReq->speaker_phone == ONLY_MIC2_ON_NC_ON)
		    || (pReq->speaker_phone == ONLY_MIC1_ON_NC_ON)
		    ) {
			pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		} 
		else {
			update_diagcmd_state(diagpdev, "SPEAKERPHONE",
					     pReq->speaker_phone);
		}
	} else {
		printk("\n[%s] error SPEAKERPHONE", __func__);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

void *LGT_TestModeVolumeLevel(test_mode_req_type * pReq,
			      DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (diagpdev != NULL) {
		update_diagcmd_state(diagpdev, "VOLUMELEVEL",
				     pReq->volume_level);
	} else {
		printk("\n[%s] error VOLUMELEVEL", __func__);
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}
	return pRsp;
}

char key_buf[MAX_KEY_BUFF_SIZE];
boolean if_condition_is_on_key_buffering = FALSE;
int count_key_buf = 0;

boolean lgf_factor_key_test_rsp(char key_code)
{
	/* sanity check */
	if (count_key_buf >= MAX_KEY_BUFF_SIZE)
		return FALSE;

	key_buf[count_key_buf++] = key_code;
	return TRUE;
}

EXPORT_SYMBOL(lgf_factor_key_test_rsp);

void *LGT_TestModeKeyTest(test_mode_req_type * pReq,
			  DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	if (pReq->key_test_start) {
		if_condition_is_on_key_buffering = TRUE;
		memset((void *)key_buf, 0x00, MAX_KEY_BUFF_SIZE);
		count_key_buf = 0;
		/* LGE_CHANGE [sm.shim@lge.com] 2010-07-26, 
		 * Testmode cmd 22 Key test merge from VS660 
		 */
		diag_event_log_start();
	} 
	else {
		if_condition_is_on_key_buffering = FALSE;
		memcpy((void *)((DIAG_TEST_MODE_KEY_F_rsp_type *) pRsp)->
		       key_pressed_buf, (void *)key_buf, MAX_KEY_BUFF_SIZE);
		/* LGE_CHANGE [sm.shim@lge.com] 2010-07-26, 
		 * Testmode cmd 22 Key test merge from VS660 
		 */
		diag_event_log_end();
	}
	return pRsp;
}

void *LGF_TestCam(test_mode_req_type * pReq, DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	switch (pReq->camera) {
	case CAM_TEST_SAVE_IMAGE:
	case CAM_TEST_CAMERA_SELECT:
	case CAM_TEST_FLASH_ON:
	case CAM_TEST_FLASH_OFF:
	/* LGE_CHANGE [sm.shim@lge.com] 2010-08-17, modify cam_save_ moving_file command */
	//case CAM_TEST_CAMCORDER_SAVE_MOVING_FILE:
	case CAM_TEST_CAMCORDER_FLASH_ON:
	case CAM_TEST_CAMCORDER_FLASH_OFF:
	case CAM_TEST_STROBE_LIGHT_ON:
	case CAM_TEST_STROBE_LIGHT_OFF:
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;

	default:
		if (diagpdev != NULL) {

			update_diagcmd_state(diagpdev, "CAMERA", pReq->camera);
		} else {
			printk("\n[%s] error CAMERA", __func__);
			pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		}
		break;
	}
	return pRsp;
}

unsigned int LGF_KeycodeTrans(word input)
{
	int index = 0;
	// if we can not find, return the org value. 
	unsigned int ret = (unsigned int)input;	

	for (index = 0; index < KEY_TRANS_MAP_SIZE; index++) {
		if (keytrans_table[index].LG_common_key_code == input) {
			ret = keytrans_table[index].Android_key_code;
			break;
		}
	}

	return ret;
}

/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
void LGF_SendKey(word keycode)
{
	struct input_dev* idev = NULL;

	idev = get_ats_input_dev();

	if(idev == NULL)
		printk("%s: input device addr is NULL\n",__func__);

	input_report_key(idev,(unsigned int)keycode, 1);
	msleep(20);
	input_report_key(idev,(unsigned int)keycode, 0);
}

uint8_t if_condition_is_on_air_plain_mode = 0;
void* LGF_PowerSaveMode(test_mode_req_type* pReq, DIAG_TEST_MODE_F_rsp_type* pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;

	switch(pReq->sleep_mode){
	case SLEEP_MODE_ON:
		LGF_SendKey(KEY_END);
		break;
	case AIR_PLAIN_MODE_ON:
		// LGE_CHANGE [dojip.kim@lge.com] 2010-09-28, ftm boot 
		remote_set_ftm_boot(0); // clear flag
		if_condition_is_on_air_plain_mode = 1;
		set_operation_mode(FALSE);
		break;
	// LGE_CHANGE [dojip.kim@lge.com] 2010-09-28, ftm boot 
	case FTM_BOOT_ON: /* kernel mode */
		remote_set_ftm_boot(1); // set flag
		break;
	default:
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		break;
	}
	return pRsp;
}
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */

char external_memory_copy_test(void)
{
	char return_value = 1;
	char *src = NULL;
	char *dest = NULL;
	off_t fd_offset;
	int fd;
	int ret;
	mm_segment_t old_fs;

	// LGE_CHANGE [dojip.kim@lge.com] 2010-07-26, kernel data segment
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open((const char __user *)"/sdcard/SDTest.txt", O_CREAT | O_RDWR, 0);
	set_fs(old_fs);
	if (fd < 0) {
		printk(KERN_ERR "[ATCMD_EMT] Can not access SD card\n");
		return return_value;
	}
	src = kmalloc(10, GFP_KERNEL);
	if (NULL == src) {
		printk(KERN_ERR "[ATCMD_EMT] memory allocation is failed\n");
		goto file_fail;
	}
	sprintf(src, "TEST");
	// LGE_CHANGE [dojip.kim@lge.com] 2010-07-26, kernel data segment
	old_fs = get_fs();
	set_fs(KERNEL_DS); 
	ret =  sys_write(fd, (const char __user *)src, 5);
	set_fs(old_fs);
	if (ret < 0) {
		printk(KERN_ERR "[ATCMD_EMT] Can not write SD card \n");
		goto file_fail;
	}
	fd_offset = sys_lseek(fd, 0, 0);

	dest = kmalloc(10, GFP_KERNEL);
	if (NULL == dest) {
		printk(KERN_ERR "[ATCMD_EMT] memory allocation is failed\n");
		goto file_fail;
	}

	// LGE_CHANGE [dojip.kim@lge.com] 2010-07-26, kernel data segment
	old_fs = get_fs();
	set_fs(KERNEL_DS); 
	ret = sys_read(fd, (char __user *)dest, 5);
	set_fs(old_fs);
	if (ret < 0) {
		printk(KERN_ERR "[ATCMD_EMT]Can not read SD card \n");
		goto file_fail;
	}
	if ((memcmp(src, dest, 4)) == 0)
		return_value = 0;
	else
		return_value = 1;

file_fail:
	sys_close(fd);
	sys_unlink((const char __user *)"/sdcard/SDTest.txt");
	if (src)
		kfree(src);
	if (dest)
		kfree(dest);
	return return_value;
}

void *LGF_ExternalSocketMemory(test_mode_req_type * pReq,
			       DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	struct statfs_local sf;
	pRsp->ret_stat_code = TEST_OK_S;

	printk(KERN_ERR "khlee debug %d \n", pReq->esm);

	switch (pReq->esm) {
	case EXTERNAL_SOCKET_MEMORY_CHECK:
		pRsp->test_mode_rsp.memory_check = external_memory_copy_test();
		break;

	case EXTERNAL_FLASH_MEMORY_SIZE:
		if (sys_statfs("/sdcard", (struct statfs *)&sf) != 0) {
			printk(KERN_ERR
			       "[Testmode]can not get sdcard infomation \n");
			pRsp->ret_stat_code = TEST_FAIL_S;
			break;
		}
		printk(KERN_ERR "blocks %d  \n", sf.f_blocks);
		printk(KERN_ERR "block size %d \n", sf.f_bsize);

		/* LGE_CHANGE [sm.shim@lge.com] 2010-08-31, 
		 * SD card total size bug fix (MB) 
		 */
		pRsp->test_mode_rsp.socket_memory_size = 
			(((sf.f_blocks / 1024) * sf.f_bsize) / 1024);
		break;
	/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
	case EXTERNAL_SOCKET_ERASE:
		/* LGE_CHANGE [sm.shim@lge.com] 2010-09-04, 
		 * bug fix: No SD card return OK 
		 */
		if (external_memory_copy_test())
		{
			pRsp->ret_stat_code = TEST_FAIL_S;
			break;
		}
		if (diagpdev != NULL) {
			update_diagcmd_state(diagpdev, "MMCFORMAT", 1);
			// LGE_CHANGE [dojip.kim@lge.com] 2010-09-26,
			// delay time : 5 sec -> 10 sec (from VS660)
			ssleep(10);
			pRsp->ret_stat_code = TEST_OK_S;
		} 
		else {
			printk("\n[%s] error MMCFORMAT", __func__ );
			pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		}
		break;

	case EXTERNAL_FLASH_MEMORY_USED_SIZE:
		if (sys_statfs("/sdcard", (struct statfs *)&sf) != 0) {
			printk(KERN_ERR "[Testmode]can not get sdcard information \n");
			pRsp->ret_stat_code = TEST_FAIL_S;
			break;
		}
		/* LGE_CHANGE [sm.shim@lge.com] 2010-08-31, 
		 * SD card free size bug fix (Byte) 
		 */
		pRsp->test_mode_rsp.socket_memory_usedsize = 
			((long long)(sf.f_blocks - 
			 (long long)sf.f_bfree) * sf.f_bsize);
		break;
	/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-13, 
	 * Testmode merge from VS660 
	 */
	default:
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		break;
	}

	return pRsp;
}

/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
void * LGF_TestModeFboot ( test_mode_req_type* pReq ,DIAG_TEST_MODE_F_rsp_type	*pRsp)
{
	printk(KERN_ERR "khlee debug %d \n", pReq->fboot);
	/* LGE_CHANGE [sm.shim@lge.com] 2010-09-03, Boot Complete Test fix */
	pRsp->ret_stat_code = TEST_FAIL_S;
	switch( pReq->fboot){
	case FIRST_BOOTING_COMPLETE_CHECK:
		pRsp->test_mode_rsp.boot_complete = boot_info;
		printk("LOG Very Very emergency!!!!%d \n",boot_info);
		/* LGE_CHANGE [sm.shim@lge.com] 2010-09-03, 
		 * Boot Complete Test fix 
		 */
		if (boot_info == 1) {
			pRsp->ret_stat_code = TEST_OK_S;
		} 
		break;
	default:
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		break;
	}
	return pRsp;
}
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */

void *LGF_MemoryVolumeCheck(test_mode_req_type * pReq,
			    DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	struct statfs_local sf;
	unsigned int total = 0;
	unsigned int used = 0;
	unsigned int remained = 0;
	pRsp->ret_stat_code = TEST_OK_S;

	if (sys_statfs("/data", (struct statfs *)&sf) != 0) {
		printk(KERN_ERR "[Testmode]can not get sdcard infomation \n");
		pRsp->ret_stat_code = TEST_FAIL_S;
	} 
	else {
		total = (sf.f_blocks * sf.f_bsize) >> 20;
		remained = (sf.f_bavail * sf.f_bsize) >> 20;
		used = total - remained;

		switch (pReq->mem_capa) {
		case MEMORY_TOTAL_CAPA_TEST:
			pRsp->test_mode_rsp.mem_capa = total;
			break;

		case MEMORY_USED_CAPA_TEST:
			pRsp->test_mode_rsp.mem_capa = used;
			break;

		case MEMORY_REMAIN_CAPA_TEST:
			pRsp->test_mode_rsp.mem_capa = remained;
			break;

		default:
			pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
			break;
		}
	}
	return pRsp;
}

void *LGF_TestModeManual(test_mode_req_type * pReq,
			 DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	pRsp->ret_stat_code = TEST_OK_S;
	pRsp->test_mode_rsp.manual_test = TRUE;
	return pRsp;
}

#ifndef SKW_TEST
static unsigned char test_mode_factory_reset_status = FACTORY_RESET_START;
#define BUF_PAGE_SIZE 2048

#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
#define FACTORY_RESET_STR       "FACT_RESET_"
#define FACTORY_RESET_STR_SIZE	11
#define FACTORY_RESET_BLK 1	// read / write on the first block

#define MSLEEP_CNT 100

static unsigned int mtd_part_num = 0;
static unsigned int mtd_part_size = 0;
static int mtd_factory_blk = 0;
extern int init_mtd_access(int partition, int block);
extern int lge_erase_block(int ebnum);
extern int lge_write_block(int ebnum, unsigned char *buf, size_t size);
extern int lge_read_block(int ebnum, unsigned char *buf);
extern unsigned int lge_get_mtd_part_info(void);
extern int lge_get_mtd_factory_mode_blk(int target_blk);
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */
#endif

void *LGF_TestModeFactoryReset(test_mode_req_type * pReq,
			       DIAG_TEST_MODE_F_rsp_type * pRsp)
{
	unsigned char pbuf[BUF_PAGE_SIZE];
	int mtd_op_result = 0;
	unsigned char startStatus;
	/* LGE_CHANGE [james.jang@lge.com] 2010-08-30 */
	static unsigned char firstStartStatus = 0;;

	pRsp->ret_stat_code = TEST_OK_S;

#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
	if (test_mode_factory_reset_status == FACTORY_RESET_START) {
		mtd_part_num = lge_get_mtd_part_info();
		mtd_part_size = mtd_part_num & 0x0000FFFF;
		mtd_part_num = (mtd_part_num >> 16) & 0x0000FFFF;
		init_mtd_access(mtd_part_num, 0);

		mtd_factory_blk =
			lge_get_mtd_factory_mode_blk(FACTORY_RESET_BLK);
		printk("mtd info num : %d, size : %d, factory_blk : %d\n",
				mtd_part_num, mtd_part_size, mtd_factory_blk);
	}
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */

	switch (pReq->factory_reset) {
	case FACTORY_RESET_CHECK:
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
		memset((void *)pbuf, 0, sizeof(pbuf));
		mtd_op_result = lge_read_block(mtd_factory_blk, pbuf);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_read_block, error num = %d \n",
			       mtd_op_result);
		} 
		else {
			printk(KERN_INFO "\n[Testmode]factory reset memcmp\n");
			// tag read sucess
			if (memcmp(pbuf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) {
				startStatus =
				    pbuf[FACTORY_RESET_STR_SIZE] - '0';
				printk(KERN_INFO
				       "[Testmode]factory reset backup status = %d \n",
				       startStatus);

				/* LGE_CHANGE [james.jang@lge.com] 2010-08-30 */
				if (firstStartStatus == 0)
					firstStartStatus = startStatus;
			}
		}

		test_mode_factory_reset_status = FACTORY_RESET_INITIAL;
		memset((void *)pbuf, 0, sizeof(pbuf));
		sprintf(pbuf, "%s%d", FACTORY_RESET_STR,
			test_mode_factory_reset_status);
		printk(KERN_INFO "[Testmode]factory reset status = %d\n",
		       test_mode_factory_reset_status);

		mtd_op_result = lge_erase_block(mtd_factory_blk);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_erase_block, error num = %d \n",
			       mtd_op_result);
		} else {
			mtd_op_result =
			    lge_write_block(mtd_factory_blk, pbuf,
					    BUF_PAGE_SIZE);
			if (mtd_op_result != 0) {
				printk(KERN_ERR
				       "[Testmode]lge_write_block, error num = %d \n",
				       mtd_op_result);
			}
		}

		printk(KERN_INFO "[Testmode]send_to_arm9 start\n");
		send_to_arm9((void *)(((byte *) pReq) -
				      sizeof(diagpkt_header_type) -
				      sizeof(word)), pRsp);
		printk(KERN_INFO "[Testmode]send_to_arm9 end\n");

		/* LG_FW khlee 2010.03.04 -If we start at 5, 
		 * we have to go to APP reset state(3) directly 
		 */
		/* LGE_CHANGE [james.jang@lge.com] 2010-08-30 */
		//if (startStatus == FACTORY_RESET_COLD_BOOT_END)
		/* LGE_CHANGE [sm.shim@lge.com] 2010-09-03, add Factory reset flag 6 */
		if (firstStartStatus == FACTORY_RESET_COLD_BOOT_END || 
			firstStartStatus == FACTORY_RESET_USER_START) 
			test_mode_factory_reset_status = FACTORY_RESET_COLD_BOOT_START;
		else
			test_mode_factory_reset_status = FACTORY_RESET_ARM9_END;

		memset((void *)pbuf, 0, sizeof(pbuf));
		sprintf(pbuf, "%s%d", FACTORY_RESET_STR,
			test_mode_factory_reset_status);
		printk(KERN_INFO "[Testmode]factory reset status = %d\n",
		       test_mode_factory_reset_status);

		mtd_op_result = lge_erase_block(mtd_factory_blk);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_erase_block, error num = %d \n",
			       mtd_op_result);
		} else {
			mtd_op_result =
			    lge_write_block(mtd_factory_blk, pbuf,
					    BUF_PAGE_SIZE);
			if (mtd_op_result != 0) {
				printk(KERN_ERR
				       "[Testmode]lge_write_block, error num = %d \n",
				       mtd_op_result);
			}
		}

#else		 /**/
		send_to_arm9((void *)(((byte *) pReq) -
				  sizeof(diagpkt_header_type) -
				  sizeof(word)), pRsp);
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */
		pRsp->ret_stat_code = TEST_OK_S;
		break;

	case FACTORY_RESET_COMPLETE_CHECK:
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		printk(KERN_ERR "[Testmode]not supported\n");
#else
		printk(KERN_INFO "[Testmode]send_to_arm9 start\n");
		send_to_arm9((void *)(((byte *) pReq) -
				      sizeof(diagpkt_header_type) -
				      sizeof(word)), pRsp);
		printk(KERN_INFO "[Testmode]send_to_arm9 end\n");
		pRsp->ret_stat_code = TEST_OK_S;
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */
		break;

	case FACTORY_RESET_STATUS_CHECK:
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
		memset((void *)pbuf, 0, sizeof(pbuf));
		mtd_op_result = lge_read_block(mtd_factory_blk, pbuf);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_read_block, error num = %d \n",
			       mtd_op_result);
			//pRsp->factory_reset = FACTORY_RESET_NA;
		} 
		else {
			// tag read sucess
			if (memcmp(pbuf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) {
				test_mode_factory_reset_status =
				    pbuf[FACTORY_RESET_STR_SIZE] - '0';
				printk(KERN_INFO
				       "[Testmode]factory reset status = %d \n",
				       test_mode_factory_reset_status);
				pRsp->ret_stat_code =
				    test_mode_factory_reset_status;
			} 
			else {
				printk(KERN_ERR
				       "[Testmode]factory reset tag fail\n");
				test_mode_factory_reset_status =
				    FACTORY_RESET_START;
				pRsp->ret_stat_code =
				    test_mode_factory_reset_status;
			}

		}
#else
		pRsp->ret_stat_code = 7;	//FACTORY_RESET_NA;
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */

		break;

	case FACTORY_RESET_COLD_BOOT:
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
		test_mode_factory_reset_status = FACTORY_RESET_COLD_BOOT_START;
		memset((void *)pbuf, 0, sizeof(pbuf));
		sprintf(pbuf, "%s%d", FACTORY_RESET_STR,
			test_mode_factory_reset_status);
		printk(KERN_INFO "[Testmode]factory reset status = %d\n",
		       test_mode_factory_reset_status);
		mtd_op_result = lge_erase_block(mtd_factory_blk);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_erase_block, error num = %d \n",
			       mtd_op_result);
		} else {
			mtd_op_result =
			    lge_write_block(mtd_factory_blk, pbuf,
					    BUF_PAGE_SIZE);
			if (mtd_op_result != 0) {
				printk(KERN_ERR
				       "[Testmode]lge_write_block, error num = %d \n",
				       mtd_op_result);
			}
		}

#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */
		break;

	case FACTORY_RESET_ERASE_USERDATA:
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
		test_mode_factory_reset_status = FACTORY_RESET_COLD_BOOT_START;
		memset((void *)pbuf, 0, sizeof(pbuf));
		sprintf(pbuf, "%s%d", FACTORY_RESET_STR,
			test_mode_factory_reset_status);
		printk(KERN_INFO
		       "[Testmode-erase userdata]factory reset status = %d\n",
		       test_mode_factory_reset_status);
		mtd_op_result = lge_erase_block(mtd_factory_blk);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_erase_block, error num = %d \n",
			       mtd_op_result);
			pRsp->ret_stat_code = TEST_FAIL_S;
		} 
		else {
			mtd_op_result =
			    lge_write_block(mtd_factory_blk, pbuf,
					    BUF_PAGE_SIZE);
			if (mtd_op_result != 0) {
				printk(KERN_ERR
				       "[Testmode]lge_write_block, error num = %d \n",
				       mtd_op_result);
				pRsp->ret_stat_code = TEST_FAIL_S;
			} else {
				pRsp->ret_stat_code = TEST_OK_S;
			}
		}
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */
		break;

	default:
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
		break;
	}
	return pRsp;
}

int factory_reset_check(void)
{
	unsigned char pbuf[BUF_PAGE_SIZE];
	int mtd_op_result = 0;

	mtd_part_num = lge_get_mtd_part_info();
	mtd_part_size = mtd_part_num & 0x0000FFFF;
	mtd_part_num = (mtd_part_num >> 16) & 0x0000FFFF;

	init_mtd_access(mtd_part_num, 0);
	mtd_factory_blk = lge_get_mtd_factory_mode_blk(FACTORY_RESET_BLK);

	// check_staus
	memset((void *)pbuf, 0, sizeof(pbuf));
	mtd_op_result = lge_read_block(mtd_factory_blk, pbuf);
	if (mtd_op_result != 0) {
		printk(KERN_ERR
		       "factory_reset_check : lge_read_block, error num = %d \n",
		       mtd_op_result);
	} else {
		// tag read sucess
		if (memcmp(pbuf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) {
			test_mode_factory_reset_status =
			    pbuf[FACTORY_RESET_STR_SIZE] - '0';
			printk(KERN_INFO "factory_reset_check : status = %d \n",
			       test_mode_factory_reset_status);
		} else {
			printk(KERN_ERR "factory_reset_check : tag fail\n");
			test_mode_factory_reset_status = FACTORY_RESET_START;
		}

	}

	// if status is cold boot start then mark it end
	/* 4 : temp value between factory reset operation and reboot */
	/* 6 : value to indicate to disable usb debug setting for ui factory reset */
	if (test_mode_factory_reset_status == FACTORY_RESET_COLD_BOOT_START || 
			test_mode_factory_reset_status == 4 ||	
			test_mode_factory_reset_status == 6) {
		memset((void *)pbuf, 0, sizeof(pbuf));
		test_mode_factory_reset_status = FACTORY_RESET_COLD_BOOT_END;

		diagpdev = diagcmd_get_dev();
		if (diagpdev != NULL) {
			update_diagcmd_state(diagpdev, "ADBSET", 0);
		}

		sprintf(pbuf, "%s%d", FACTORY_RESET_STR,
			test_mode_factory_reset_status);
		printk(KERN_INFO "factory_reset_check : status = %d\n",
		       test_mode_factory_reset_status);
		mtd_op_result = lge_erase_block(mtd_factory_blk);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "factory_reset_check : lge_erase_block, error num = %d \n",
			       mtd_op_result);
		} else {
			mtd_op_result =
			    lge_write_block(mtd_factory_blk, pbuf,
					    BUF_PAGE_SIZE);
			if (mtd_op_result != 0) {
				printk(KERN_ERR
				       "factory_reset_check : "
				       "lge_write_block, error num = %d \n",
				       mtd_op_result);
			}
		}
	}
	return 0;
}

EXPORT_SYMBOL(factory_reset_check);

void *LGF_TestScriptItemSet(test_mode_req_type * pReq,
			    DIAG_TEST_MODE_F_rsp_type * pRsp)
{
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
	int mtd_op_result = 0;
#endif

	if (pReq->test_mode_test_scr_mode == TEST_SCRIPT_ITEM_SET) {
#ifdef CONFIG_LGE_MTD_DIRECT_ACCESS
		if (test_mode_factory_reset_status == FACTORY_RESET_START) {
			mtd_part_num = lge_get_mtd_part_info();
			mtd_part_size = mtd_part_num & 0x0000FFFF;
			mtd_part_num = (mtd_part_num >> 16) & 0x0000FFFF;
			init_mtd_access(mtd_part_num, 0);

			mtd_factory_blk =
			    lge_get_mtd_factory_mode_blk(FACTORY_RESET_BLK);
			printk
			    ("mtd info num : %d, size : %d, factory_blk : %d\n",
			     mtd_part_num, mtd_part_size, mtd_factory_blk);
		}

		mtd_op_result = lge_erase_block(mtd_factory_blk);
		if (mtd_op_result != 0) {
			printk(KERN_ERR
			       "[Testmode]lge_erase_block, error num = %d \n",
			       mtd_op_result);
			pRsp->ret_stat_code = TEST_FAIL_S;
		} 
		else
#endif /*CONFIG_LGE_MTD_DIRECT_ACCESS */
		// LG_FW khlee 2010.03.16 - 
		// They want to ACL on state in test script state.
		update_diagcmd_state(diagpdev, "ALC", 1);
	}

	send_to_arm9((void *)(((byte *) pReq) -
			      sizeof(diagpkt_header_type) -
			      sizeof(word)), pRsp);
	
	return pRsp;
}

/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
void* LGF_TestModeDBIntegrityCheck(test_mode_req_type* pReq,
		DIAG_TEST_MODE_F_rsp_type *pRsp)
{

	printk(KERN_ERR "[_DBCHECK_] [%s:%d] DBCHECKSubCmd=<%d>\n", 
			__func__, __LINE__, pReq->bt);

	if (diagpdev != NULL){
		update_diagcmd_state(diagpdev, "DBCHECK", pReq->db_check);
		pRsp->ret_stat_code = TEST_OK_S;
	}
	else {
		printk("\n[%s] error DBCHECK", __func__ );
		pRsp->ret_stat_code = TEST_NOT_SUPPORTED_S;
	}

	return pRsp;
}
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */

/*  USAGE
 *    1. If you want to handle at ARM9 side, 
 *    you have to insert fun_ptr as NULL and mark ARM9_PROCESSOR
 *    2. If you want to handle at ARM11 side , 
 *    you have to insert fun_ptr as you want and mark AMR11_PROCESSOR.
 */

testmode_user_table_entry_type testmode_mstr_tbl[TESTMODE_MSTR_TBL_SIZE] = {
	/*    sub_command    fun_ptr   which procesor         */
	/* 0 ~ 5 */
	{TEST_MODE_VERSION, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_LCD, linux_app_handler, ARM11_PROCESSOR}
	,
	{TEST_MODE_MOTOR, LGF_TestMotor, ARM11_PROCESSOR}
	,
	{TEST_MODE_ACOUSTIC, LGF_TestAcoustic, ARM11_PROCESSOR}
	,
	/* 5 ~ 10 */
	{TEST_MODE_CAM, LGF_TestCam, ARM11_PROCESSOR}
	,
	/* 11 ~ 15 */

	/* 16 ~ 20 */

	/* 21 ~ 25 */
	{TEST_MODE_KEY_TEST, LGT_TestModeKeyTest, ARM11_PROCESSOR}
	,
	{TEST_MODE_EXT_SOCKET_TEST, linux_app_handler, ARM11_PROCESSOR}
	,
	{TEST_MODE_BLUETOOTH_TEST, LGF_TestModeBlueTooth, ARM11_PROCESSOR}
	,
	/* 26 ~ 30 */
	{TEST_MODE_MP3_TEST, LGF_TestModeMP3, ARM11_PROCESSOR}
	,
	/* 31 ~ 35 */
	{TEST_MODE_ACCEL_SENSOR_TEST, linux_app_handler, ARM11_PROCESSOR}
	,
	{TEST_MODE_WIFI_TEST, linux_app_handler, ARM11_PROCESSOR}
	,
	/* 36 ~ 40 */
	{TEST_MODE_KEY_DATA_TEST, linux_app_handler, ARM11_PROCESSOR}
	,
	/* 41 ~ 45 */
	{TEST_MODE_MEMORY_CAPA_TEST, LGF_MemoryVolumeCheck, ARM11_PROCESSOR}
	,
	/* LGE_CHANGE [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
	//{TEST_MODE_SLEEP_MODE_TEST, linux_app_handler, ARM11_PROCESSOR}
	{TEST_MODE_SLEEP_MODE_TEST, LGF_PowerSaveMode, ARM11_PROCESSOR}
	,
	{TEST_MODE_SPEAKER_PHONE_TEST, LGF_TestModeSpeakerPhone, ARM11_PROCESSOR}
	,
	/* LGE_CHANGE [sm.shim@lge.com] 2010-08-09, Photo Sensor disable */
	/*
	{TEST_MODE_PHOTO_SENSER_TEST, linux_app_handler, ARM11_PROCESSOR}
	,
	*/

	/* 46 ~ 50 */
	{TEST_MODE_MRD_USB_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_PROXIMITY_SENSOR_TEST, linux_app_handler, ARM11_PROCESSOR}
	,
	{TEST_MODE_TEST_SCRIPT_MODE, LGF_TestScriptItemSet, ARM11_PROCESSOR}
	,

	{TEST_MODE_FACTORY_RESET_CHECK_TEST, LGF_TestModeFactoryReset, ARM11_PROCESSOR}
	,			//
	/* 51 ~ */
	{TEST_MODE_VOLUME_TEST, LGT_TestModeVolumeLevel, ARM11_PROCESSOR}
	,
	/* LGE_CHANGE [sm.shim@lge.com] 2010-08-22, 
	 * merge First Boot Complete Test from VS660 
	 */
	{ TEST_MODE_FIRST_BOOT_COMPLETE_TEST, LGF_TestModeFboot, ARM11_PROCESSOR}
	,
	/*70~    */
	{TEST_MODE_PID_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_SW_VERSION, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_IME_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_IMPL_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_SIM_LOCK_TYPE_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_UNLOCK_CODE_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_IDDE_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_FULL_SIGNATURE_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_NT_CODE_TEST, NULL, ARM9_PROCESSOR}
	,
	{TEST_MODE_SIM_ID_TEST, NULL, ARM9_PROCESSOR}
	,
	/*80~   */
	{TEST_MODE_CAL_CHECK, NULL, ARM9_PROCESSOR}
	,
	/* LGE_CHANGE [sm.shim@lge.com] 2010-07-27, 
	 * Testmode cmd 83 Bluetooth RW merge from VS660 
	 */
	{TEST_MODE_BLUETOOTH_TEST_RW, LGF_TestModeBlueTooth_RW, ARM11_PROCESSOR}
	,
	{TEST_MODE_SKIP_WELCOM_TEST, NULL, ARM9_PROCESSOR}
	,
	/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
	{ TEST_MODE_MAC_READ_WRITE, linux_app_handler, ARM11_PROCESSOR }
	,
	/*90~	*/
	{ TEST_MODE_DB_INTEGRITY_CHECK,	LGF_TestModeDBIntegrityCheck, ARM11_PROCESSOR}
	,
	/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-13, Testmode merge from VS660 */
};
