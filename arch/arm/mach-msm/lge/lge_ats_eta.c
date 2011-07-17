/* arch/arm/mach-msm/lge/lge_ats_eta.c
 *
 * Copyright (C) 2010 LGE, Inc.
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
#include <linux/kernel.h>
#include <linux/input.h>
#include <mach/msm_rpcrouter.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <mach/board_lge.h>
#include <linux/lge_alohag_at.h>
#include <mach/lge_base64.h>
#include "lge_ats.h"

#define JIFFIES_TO_MS(t) ((t) * 1000 / HZ)

unsigned int ats_mtc_log_mask = 0x00000000;

extern int event_log_start(void);
extern int event_log_end(void);

int eta_execute(char *string)
{
	int ret;
	char cmdstr[100];
	int fd;
	char *envp[] = {
		"HOME=/",
		"TERM=linux",
		NULL,
	};

	char *argv[] = {
		"/system/bin/eta",
		cmdstr,
		NULL,
	};

	fd = sys_open((const char __user *) "/system/bin/eta", O_RDONLY ,0);
	if ( fd < 0 ) {
		printk("\n [ETA]can not open /system/bin/eta - execute /system/bin/eta\n");
		sprintf(cmdstr, "/system/bin/eta");
	}
	else {
		printk("\n [ETA]execute /system/bin/eta\n");
		snprintf(cmdstr, sizeof(cmdstr), "%s", string);
		sys_close(fd);
	}

	printk(KERN_INFO "[ETA]execute eta : data - %s\n", cmdstr);
	ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (ret != 0) {
		printk(KERN_ERR "[ETA]execute failed, ret = %d\n", ret);
	}
	else
		printk(KERN_INFO "[ETA]execute ok, ret = %d\n", ret);

	return ret;
}

void ats_mtc_send_key_log_to_eta(struct ats_mtc_key_log_type* p_ats_mtc_key_log)
{
	int index = 0;
	int lenb64 = 0;
	int exec_result = 0;
	unsigned long long eta_time_val = 0;
	unsigned char eta_cmd_buf[50];
	unsigned char eta_cmd_buf_encoded[50];
	
	memset(eta_cmd_buf, 0, sizeof(eta_cmd_buf));
	memset(eta_cmd_buf_encoded, 0, sizeof(eta_cmd_buf_encoded));
				
	eta_cmd_buf[index++] = 0xF0; //MTC_CMD_CODE
	eta_cmd_buf[index++] = 0x08; //MTC_LOG_REQ_CMD

	//LOG_ID, 1 key, 2 touch
	eta_cmd_buf[index++] = p_ats_mtc_key_log->log_id; 
	eta_cmd_buf[index++] = p_ats_mtc_key_log->log_len; //LOG_LEN
	eta_cmd_buf[index++] = 0; //LOG_LEN

	eta_time_val = JIFFIES_TO_MS(jiffies);
	eta_cmd_buf[index++] = eta_time_val & 0xff; //LSB
	eta_cmd_buf[index++] = (eta_time_val >> 8) & 0xff;
	eta_cmd_buf[index++] = (eta_time_val >> 16) & 0xff;
	eta_cmd_buf[index++] = (eta_time_val >> 24) & 0xff;
	eta_cmd_buf[index++] = (eta_time_val >> 32) & 0xff;
	eta_cmd_buf[index++] = (eta_time_val >> 40) & 0xff;
	eta_cmd_buf[index++] = (eta_time_val >> 48) & 0xff;
	eta_cmd_buf[index++] = (eta_time_val >> 56) & 0xff; // MSB

	index = 13;

	if (p_ats_mtc_key_log->log_id == ATS_MTC_KEY_LOG_ID_KEY) {
		eta_cmd_buf[index++] = (p_ats_mtc_key_log->x_hold)&0xFF; // hold
		eta_cmd_buf[index++] = (p_ats_mtc_key_log->y_code)&0xFF; //key code

		for(index = 15; index<23; index++) { // ACTIVE_UIID 8
			eta_cmd_buf[index] = 0;
		}
	}
	else if(p_ats_mtc_key_log->log_id == ATS_MTC_KEY_LOG_ID_TOUCH) {
		eta_cmd_buf[index++] = 1; // MAIN LCD
		eta_cmd_buf[index++] = p_ats_mtc_key_log->action;
		// index = 15
		eta_cmd_buf[index++] = (p_ats_mtc_key_log->x_hold)&0xFF;
		// index = 16
		eta_cmd_buf[index++] = ((p_ats_mtc_key_log->x_hold)>>8)&0xFF;
		// index = 17
		eta_cmd_buf[index++] = (p_ats_mtc_key_log->y_code)&0xFF;
		// index = 18
		eta_cmd_buf[index++] = ((p_ats_mtc_key_log->y_code)>>8)&0xFF;

		for(index = 19; index<27; index++) { // ACTIVE_UIID 8
			eta_cmd_buf[index] = 0;
		}
	}

	lenb64 = base64_encode((char *)eta_cmd_buf, index, (char *)eta_cmd_buf_encoded);
			
	exec_result = eta_execute(eta_cmd_buf_encoded);
	printk(KERN_INFO "[ETA]AT+MTC exec_result %d\n",exec_result);
}
EXPORT_SYMBOL(ats_mtc_send_key_log_to_eta);

int lge_ats_handle_atcmd_eta(struct msm_rpc_server *server,
			 struct rpc_request_hdr *req, unsigned len)
{
	int result = HANDLE_OK;
	int loop = 0;
	char ret_string[MAX_STRING_RET];
	uint32_t ret_value1 =0;
	uint32_t ret_value2 = 0;
	static AT_SEND_BUFFER_t totalBuffer[LIMIT_MAX_SEND_SIZE_BUFFER];
	static uint32_t totalBufferSize = 0;
	uint32_t at_cmd,at_act;
	int len_b64;
	char *decoded_params;
	unsigned char b0;
	unsigned char b1;
	unsigned char b2;
	unsigned char b3;
	unsigned long logmask = 0x00;
	struct rpc_ats_atcmd_eta_args *args = (struct rpc_ats_atcmd_eta_args *)(req + 1);

	memset(server->retvalue.ret_string, 0, sizeof(server->retvalue.ret_string));

	memset(ret_string, 0, sizeof(ret_string));

	/* init for LARGE Buffer */
	if(args->sendNum == 0) {
		// init when first send
		memset(totalBuffer, 0, sizeof(totalBuffer));
		totalBufferSize = 0;
	}
	
	args->at_cmd = be32_to_cpu(args->at_cmd);
	args->at_act = be32_to_cpu(args->at_act);
	args->sendNum = be32_to_cpu(args->sendNum);
	args->endofBuffer = be32_to_cpu(args->endofBuffer);
	args->buffersize = be32_to_cpu(args->buffersize);
		
	printk(KERN_INFO "[ETA]handle_misc_rpc_call at_cmd = 0x%X, "
			"at_act=%d, sendNum=%d:\n",
	      		args->at_cmd, args->at_act,args->sendNum);
	printk(KERN_INFO "[ETA]handle_misc_rpc_call endofBuffer = %d, buffersize=%d:\n",
	      		args->endofBuffer, args->buffersize);
	printk(KERN_INFO "[ETA]input buff[0] = 0x%X,buff[1]=0x%X,buff[2]=0x%X:\n",
			args->buffer[0],args->buffer[1],args->buffer[2]);
	if (args->sendNum < MAX_SEND_LOOP_NUM) {
		for (loop = 0; loop < args->buffersize; loop++) {
			totalBuffer[MAX_SEND_SIZE_BUFFER*args->sendNum + loop] =  
				(args->buffer[loop]);
		}
		
		totalBufferSize += args->buffersize;
			
	}
	printk(KERN_INFO "[ETA]handle_misc_rpc_call buff[0] = 0x%X, "
			"buff[1]=0x%X, buff[2]=0x%X\n",
	      		totalBuffer[0 + args->sendNum*MAX_SEND_SIZE_BUFFER], 
			totalBuffer[1 + args->sendNum*MAX_SEND_SIZE_BUFFER], 
			totalBuffer[2 + args->sendNum*MAX_SEND_SIZE_BUFFER]);

	if (!args->endofBuffer )
		return HANDLE_OK_MIDDLE;

	at_cmd = args->at_cmd;
	at_act = args->at_act;

	/* please use
	 * static uint8_t totalBuffer[LIMIT_MAX_SEND_SIZE_BUFFER];
	 * static uint32_t totalBufferSize = 0;
	 * uint32_t at_cmd,at_act;
	 */
	switch (at_cmd) {
		case ATCMD_MTC: {
			int exec_result =0;

			printk(KERN_INFO "\n[ETA]ATCMD_MTC\n ");

			if (at_act != ATCMD_ACTION)
				result = HANDLE_FAIL;

			printk(KERN_INFO "[ETA]totalBuffer : [%s] size: %d\n", 
					totalBuffer, totalBufferSize);
			exec_result = eta_execute(totalBuffer);
			printk(KERN_INFO "[ETA]AT+MTC exec_result %d\n",exec_result);
			
			decoded_params = kmalloc(totalBufferSize, GFP_KERNEL);
			if (!decoded_params) {
				printk(KERN_ERR "%s: Insufficent memory!!!\n", __func__);
				result = HANDLE_ERROR;
				break;
			}
			printk(KERN_INFO "[ETA] encoded_addr: 0x%X, decoded_addr: 0x%X, "
					"size: %d\n",  
					(unsigned int)totalBuffer, 
					(unsigned int)decoded_params, 
					sizeof(char)*totalBufferSize);
			
			len_b64 = base64_decode((char *)totalBuffer, 
					(unsigned char *)decoded_params, totalBufferSize);
			printk(KERN_INFO "[ETA] sub cmd: 0x%X, param1: 0x%X, "
					"param2: 0x%X (length = %d)\n",  
					decoded_params[1], 
					decoded_params[2], 
					decoded_params[3], 
					strlen(decoded_params));

			switch(decoded_params[1]) {
				case 0x07://MTC_LOGGING_MASK_REQ_CMD:
					printk(KERN_INFO "[ETA] logging mask request cmd :"
							"%d\n", decoded_params[1]);

					b0 = decoded_params[2];
					b1 = decoded_params[3];
					b2 = decoded_params[4];
					b3 = decoded_params[5];

					logmask = b3<<24 | b2<<16 | b1<<8 | b0;

					switch(logmask) {
						case 0x00000000://ETA_LOGMASK_DISABLE_ALL:
						case 0xFFFFFFFF://ETA_LOGMASK_ENABLE_ALL:
						case 0x00000001://ETA_LOGITEM_KEY:
						case 0x00000002://ETA_LOGITEM_TOUCHPAD:
						case 0x00000003://ETA_LOGITME_KEYTOUCH:
							ats_mtc_log_mask = logmask;
							break;
						default: //ETA_LOGMASK_DISABLE_ALL;
							ats_mtc_log_mask = 0x00000000;
							break;
					}

					/* add key log by younchan.kim*/
					if (logmask & 0xFFFFFFFF)
						event_log_start();
					else
						event_log_end();
					break;
					
				default:
					break;
			}
			
			if (decoded_params)
				kfree(decoded_params);

			sprintf(ret_string, "edcb");
			ret_value1 = 10;
			ret_value2 = 20;

		}
		break;

		default :
			result = HANDLE_ERROR;
			break;
	}

	/* give to RPC server result */
	strncpy(server->retvalue.ret_string, ret_string, MAX_STRING_RET);
	server->retvalue.ret_string[MAX_STRING_RET-1] = 0;
	server->retvalue.ret_value1 = ret_value1;
	server->retvalue.ret_value2 = ret_value2;
	if (args->endofBuffer ) {
		/* init when first send */
		memset(totalBuffer, 0, sizeof(totalBuffer));
		totalBufferSize = 0;
	}

	if (result == HANDLE_OK)
		result = RPC_RETURN_RESULT_OK;
	else if (result == HANDLE_OK_MIDDLE)
		result = RPC_RETURN_RESULT_MIDDLE_OK;
	else
		result= RPC_RETURN_RESULT_ERROR;

	printk(KERN_INFO"%s: resulte = %d\n", __func__, result);

	return result;
}
