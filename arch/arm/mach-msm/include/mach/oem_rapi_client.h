/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __ASM__ARCH_OEM_RAPI_CLIENT_H
#define __ASM__ARCH_OEM_RAPI_CLIENT_H

/*
 * OEM RAPI CLIENT Driver header file
 */

#include <linux/types.h>
#include <mach/msm_rpcrouter.h>

enum {
	OEM_RAPI_CLIENT_EVENT_NONE = 0,

	/*
	 * list of oem rapi client events
	 */

#if defined (CONFIG_LGE_SUPPORT_RAPI)
	/* LGE_CHANGES_S [khlee@lge.com] 2009-12-04, [VS740] use OEMRAPI */
	LG_FW_RAPI_START = 100,
	LG_FW_RAPI_CLIENT_EVENT_GET_LINE_TYPE = LG_FW_RAPI_START,
	LG_FW_TESTMODE_EVENT_FROM_ARM11 = LG_FW_RAPI_START + 1,
	LG_FW_A2M_BATT_INFO_GET = LG_FW_RAPI_START + 2,
	LG_FW_A2M_PSEUDO_BATT_INFO_SET = LG_FW_RAPI_START + 3,
	LG_FW_MEID_GET = LG_FW_RAPI_START + 4,
	LG_FW_SET_OPERATION_MODE = LG_FW_RAPI_START + 5,
	/* LGE_CHANGES_S [woonghee.park@lge.com] 2010-05-18, [VS740], 
	 * LG_FW_CHARGING_TIMER
	 */
	LG_FW_SET_CHARGING_TIMER = LG_FW_RAPI_START + 6,
	LG_FW_GET_CHARGING_TIMER = LG_FW_RAPI_START + 7,
	/* LGE_CHANGES_E [woonghee.park@lge.com] */
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-05-29, [LS670] PCB Version */
	LG_FW_GET_PCB_VERSION = LG_FW_RAPI_START + 8,
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-05-29, [LS670] LG_FW_RTN_RESET */
	LG_FW_RAPI_CLIENT_EVENT_SET_RTN_RESET= LG_FW_RAPI_START + 9,
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-08-09, [LS670] 
	 * no stop charging even if hot or cold battery 
	 */
	LG_FW_RAPI_CLIENT_EVENT_SET_THM_NO_STOP_CHARGING = LG_FW_RAPI_START + 10,
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-08-9 */
	LG_FW_A2M_BLOCK_CHARGING_SET = LG_FW_RAPI_START + 11,
	/* LGE_CHANGE [james.jang@lge.com] 2010-08-25 */
	LG_FW_CIQ_EXCEPTION_ERROR_TEST = LG_FW_RAPI_START + 12,
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-09-01 */
	LG_FW_SET_CHARGING_STAT_REALTIME_UPDATE = LG_FW_RAPI_START + 13,
	LG_FW_GET_CHARGING_STAT_REALTIME_UPDATE = LG_FW_RAPI_START + 14,
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-09-12, prl version */
	LG_FW_GET_PRL_VERSION = LG_FW_RAPI_START + 15,
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-09-28, ftm boot */
	LG_FW_SET_FTM_BOOT = LG_FW_RAPI_START + 16,
	LG_FW_GET_FTM_BOOT = LG_FW_RAPI_START + 17,
#endif
	OEM_RAPI_CLIENT_EVENT_MAX

};

struct oem_rapi_client_streaming_func_cb_arg {
	uint32_t  event;
	void      *handle;
	uint32_t  in_len;
	char      *input;
	uint32_t out_len_valid;
	uint32_t output_valid;
	uint32_t output_size;
};

struct oem_rapi_client_streaming_func_cb_ret {
	uint32_t *out_len;
	char *output;
};

struct oem_rapi_client_streaming_func_arg {
	uint32_t event;
	int (*cb_func)(struct oem_rapi_client_streaming_func_cb_arg *,
		       struct oem_rapi_client_streaming_func_cb_ret *);
	void *handle;
	uint32_t in_len;
	char *input;
	uint32_t out_len_valid;
	uint32_t output_valid;
	uint32_t output_size;
};

struct oem_rapi_client_streaming_func_ret {
	uint32_t *out_len;
	char *output;
};

int oem_rapi_client_streaming_function(
	struct msm_rpc_client *client,
	struct oem_rapi_client_streaming_func_arg *arg,
	struct oem_rapi_client_streaming_func_ret *ret);

int oem_rapi_client_close(void);

struct msm_rpc_client *oem_rapi_client_init(void);

#endif
