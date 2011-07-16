/* arch/arm/mach-msm/lge/lg_rapi_client.c
 *
 * Copyright (C) 2009 LGE, Inc.
 * Created by khlee
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
#include <linux/kernel.h>
#include <linux/err.h>
#include <mach/oem_rapi_client.h>
#include <mach/lg_diag_testmode.h>
#if defined(CONFIG_MACH_MSM7X27_ALOHAV)
#include <mach/msm_battery_alohav.h>
#elif defined(CONFIG_MACH_MSM7X27_THUNDERC)
#include <mach/msm_battery_thunderc.h>
#else
#include <mach/msm_battery.h>
#endif
#include <mach/board_lge.h>
#include <mach/lg_pcb_version.h>

#define GET_INT32(buf)           (int32_t)be32_to_cpu(*((uint32_t*)(buf)))
#define PUT_INT32(buf, v)        (*((uint32_t*)buf) = (int32_t)be32_to_cpu((uint32_t)(v)))
#define GET_U_INT32(buf)         ((uint32_t)GET_INT32(buf))
#define PUT_U_INT32(buf, v)      PUT_INT32(buf, (int32_t)(v))

#define GET_LONG(buf)            ((long)GET_INT32(buf))
#define PUT_LONG(buf, v) \
	(*((u_long*)buf) = (long)be32_to_cpu((u_long)(v)))

#define GET_U_LONG(buf)	         ((u_long)GET_LONG(buf))
#define PUT_U_LONG(buf, v)	      PUT_LONG(buf, (long)(v))

#define GET_BOOL(buf)            ((bool_t)GET_LONG(buf))
#define GET_ENUM(buf, t)         ((t)GET_LONG(buf))
#define GET_SHORT(buf)           ((short)GET_LONG(buf))
#define GET_U_SHORT(buf)         ((u_short)GET_LONG(buf))

#define PUT_ENUM(buf, v)         PUT_LONG(buf, (long)(v))
#define PUT_SHORT(buf, v)        PUT_LONG(buf, (long)(v))
#define PUT_U_SHORT(buf, v)      PUT_LONG(buf, (long)(v))

#define LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE 128
#define LG_RAPI_CLIENT_MAX_IN_BUFF_SIZE 128

static uint32_t open_count;
struct msm_rpc_client *client;

static int old_cable_type = -1;

int LG_rapi_init(void)
{
	client = oem_rapi_client_init();
	if (IS_ERR(client)) {
		pr_err("%s: couldn't open oem rapi client\n", __func__);
		return PTR_ERR(client);
	}
	open_count++;

	return 0;
}

void Open_check(void)
{
	/* to double check re-open; */
	if (open_count > 0)
		return;
	LG_rapi_init();
}

int msm_chg_LG_cable_type(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE];
	int retValue = 0;
	int rc = -1;
	int errCount = 0;

	Open_check();

	do {
		arg.event = LG_FW_RAPI_CLIENT_EVENT_GET_LINE_TYPE;
		arg.cb_func = NULL;
		arg.handle = (void *)0;
		arg.in_len = 0;
		arg.input = NULL;
		arg.out_len_valid = 1;
		arg.output_valid = 1;
		arg.output_size = 4;

		ret.output = NULL;
		ret.out_len = NULL;

		rc = oem_rapi_client_streaming_function(client, &arg, &ret);
		if (rc < 0) {
			retValue = old_cable_type;
		} 
		else {
			memcpy(output, ret.output, *ret.out_len);
			retValue = GET_INT32(output);

			if (retValue == 0) // no init cable 
				retValue = old_cable_type;
			else //read ok.
				old_cable_type = retValue;
		}

		if (ret.output)
			kfree(ret.output);
		if (ret.out_len)
			kfree(ret.out_len);

	} while (rc < 0 && errCount++ < 3);

#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
	if (lge_bd_rev < HW_PCB_REV_B && retValue == 10) // LT_130K
		retValue = 0;
#endif

	printk("USB Cable type: %s(): %d\n", __func__, retValue);
	return retValue;
}

void send_to_arm9(void *pReq, void *pRsp)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_TESTMODE_EVENT_FROM_ARM11;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(DIAG_TEST_MODE_F_req_type);
	arg.input = (char *)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;

	if (((DIAG_TEST_MODE_F_req_type *) pReq)->sub_cmd_code ==
	    TEST_MODE_FACTORY_RESET_CHECK_TEST)
		arg.output_size =
		    sizeof(DIAG_TEST_MODE_F_rsp_type) -
		    sizeof(test_mode_rsp_type);
	else
		arg.output_size = sizeof(DIAG_TEST_MODE_F_rsp_type);

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	memcpy(pRsp, ret.output, *ret.out_len);
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
}

void set_operation_mode(boolean info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_SET_OPERATION_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(boolean);
	arg.input = (char*) &info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*) NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
}
EXPORT_SYMBOL(set_operation_mode);

#ifdef CONFIG_MACH_MSM7X27_THUNDERC
void battery_info_get(struct batt_info *resp_buf)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;
	struct batt_info rsp_buf;

	Open_check();

	arg.event = LG_FW_A2M_BATT_INFO_GET;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(rsp_buf);

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (ret_val == 0) {
		memcpy(&rsp_buf, ret.output, *ret.out_len);

		resp_buf->valid_batt_id = GET_U_INT32(&rsp_buf.valid_batt_id);
		resp_buf->batt_therm = GET_U_INT32(&rsp_buf.batt_therm);
		resp_buf->batt_temp = GET_INT32(&rsp_buf.batt_temp);
#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
		resp_buf->chg_current = GET_U_INT32(&rsp_buf.chg_current);
		resp_buf->batt_thrm_state =
		    GET_U_INT32(&rsp_buf.batt_thrm_state);
#endif
	} else {		/* In case error */
		resp_buf->valid_batt_id = 1;	/* authenticated battery id */
		resp_buf->batt_therm = 100;	/* 100 battery therm adc */
		resp_buf->batt_temp = 30;	/* 30 degree celcius */
#if defined(CONFIG_MACH_MSM7X27_THUNDERC_SPRINT)
		resp_buf->chg_current = 0;
		resp_buf->batt_thrm_state = 0;
#endif
	}

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

EXPORT_SYMBOL(battery_info_get);

void pseudo_batt_info_set(struct pseudo_batt_info_type *info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_A2M_PSEUDO_BATT_INFO_SET;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(struct pseudo_batt_info_type);
	arg.input = (char *)info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	/* alloc memory for response */

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}
EXPORT_SYMBOL(pseudo_batt_info_set);

void block_charging_set(int bypass)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_A2M_BLOCK_CHARGING_SET;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *) &bypass;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	/* alloc memory for response */

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}
EXPORT_SYMBOL(block_charging_set);
#endif /* CONFIG_MACH_MSM7X27_THUNDERC */

void msm_get_MEID_type(char *sMeid)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_MEID_GET;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 15;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	memcpy(sMeid, ret.output, 14);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

#ifdef CONFIG_MACH_MSM7X27_THUNDERC
void set_charging_timer(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_SET_CHARGING_TIMER;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

EXPORT_SYMBOL(set_charging_timer);

void get_charging_timer(int *info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;
	int resp_buf;

	Open_check();

	arg.event = LG_FW_GET_CHARGING_TIMER;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(int);

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (ret_val == 0) {
		memcpy(&resp_buf, ret.output, *ret.out_len);

		*info = GET_INT32(&resp_buf);
	} else {
		*info = 1;	//default value
	}

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

EXPORT_SYMBOL(get_charging_timer);
#endif

#ifdef  CONFIG_LGE_PCB_VERSION
int lg_get_hw_version(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int pcb_ver;

	Open_check();

	arg.event = LG_FW_GET_PCB_VERSION;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 4;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	memcpy(&pcb_ver, ret.output, *ret.out_len);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return GET_INT32(&pcb_ver);
}

EXPORT_SYMBOL(lg_get_hw_version);
#endif /* CONFIG_LGE_PCB_VERSION */

#ifdef CONFIG_LGE_THERM_NO_STOP_CHARGING
void set_charging_therm_no_stop_charging(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_RAPI_CLIENT_EVENT_SET_THM_NO_STOP_CHARGING;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

EXPORT_SYMBOL(set_charging_therm_no_stop_charging);
#endif

void remote_set_charging_stat_realtime_update(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_SET_CHARGING_STAT_REALTIME_UPDATE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}

EXPORT_SYMBOL(remote_set_charging_stat_realtime_update);

void remote_get_charging_stat_realtime_update(int *info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;
	int resp_buf;

	Open_check();

	arg.event = LG_FW_GET_CHARGING_STAT_REALTIME_UPDATE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(int);

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (ret_val == 0) {
		memcpy(&resp_buf, ret.output, *ret.out_len);
		*info = GET_INT32(&resp_buf);
	} else {
		*info = 0;	//default value
	}

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
}
EXPORT_SYMBOL(remote_get_charging_stat_realtime_update);

void remote_get_prl_version(int *info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;
	int resp_buf;

	Open_check();

	arg.event = LG_FW_GET_PRL_VERSION;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(int);

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (ret_val == 0) {
		memcpy(&resp_buf, ret.output, *ret.out_len);
		*info = GET_INT32(&resp_buf);
	} else {
		*info = 0;	//default value
	}

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
}
EXPORT_SYMBOL(remote_get_prl_version);

void remote_set_ftm_boot(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_SET_FTM_BOOT;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);
}
EXPORT_SYMBOL(remote_set_ftm_boot);

void remote_get_ftm_boot(int *info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;
	int resp_buf;
	int errCount = 0;

	Open_check();

	do {
		arg.event = LG_FW_GET_FTM_BOOT;
		arg.cb_func = NULL;
		arg.handle = (void *)0;
		arg.in_len = 0;
		arg.input = NULL;
		arg.out_len_valid = 1;
		arg.output_valid = 1;
		arg.output_size = sizeof(int);

		ret.output = NULL;
		ret.out_len = NULL;

		ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
		if (ret_val == 0) {
			memcpy(&resp_buf, ret.output, *ret.out_len);
			*info = GET_INT32(&resp_buf);
		} else {
			*info = 0;	//default value
		}

		if (ret.output)
			kfree(ret.output);
		if (ret.out_len)
			kfree(ret.out_len);

	} while (ret_val < 0 && errCount++ < 3);
}
EXPORT_SYMBOL(remote_get_ftm_boot);

