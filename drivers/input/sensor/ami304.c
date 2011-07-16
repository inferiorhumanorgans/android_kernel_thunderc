/* drivers/i2c/chips/ami304.c - AMI304 compass driver
 *
 * Copyright (C) 2009 AMIT Technology Inc.
 * Author: Kyle Chen <sw-support@amit-inc.com>
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include "ami304.h"

#include <mach/board_lge.h>

#define AMI304_DEBUG_PRINT	1
#define AMI304_ERROR_PRINT	1

//#define AMI304_TEST		1

/* AMI304 Debug mask value
 * usage: echo [mask_value] > /sys/module/ami304/parameters/debug_mask
 * All		: 127
 * No msg	: 0
 * default	: 2
 */
enum {
	AMI304_DEBUG_ERR_CHECK		= 1U << 0,
	AMI304_DEBUG_USER_ERROR		= 1U << 1,
	AMI304_DEBUG_FUNC_TRACE		= 1U << 2,
	AMI304_DEBUG_DEV_STATUS		= 1U << 3,
	AMI304_DEBUG_DEV_DEBOUNCE	= 1U << 4,
	AMI304_DEBUG_GEN_INFO		= 1U << 5,
	AMI304_DEBUG_INTR_INFO		= 1U << 6,
};

static unsigned int ami304_debug_mask = AMI304_DEBUG_USER_ERROR;

module_param_named(debug_mask, ami304_debug_mask, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(AMI304_DEBUG_PRINT)
#define AMID(fmt, args...)  printk(KERN_ERR "AMI304-DBG[%-18s:%5d]" fmt, __FUNCTION__, __LINE__, ## args)
#else
#define AMID(fmt, args...)
#endif


#if defined(AMI304_ERROR_PRINT)
#define AMIE(fmt, args...)  printk(KERN_ERR "AMI304-ERR[%-18s:%5d]" fmt, __FUNCTION__, __LINE__, ## args)
#else
#define AMIE(fmt, args...)
#endif

static struct i2c_client *ami304_i2c_client = NULL;
static int AMI304_Init(int mode);

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>

struct early_suspend ami304_sensor_early_suspend;

static void ami304_early_suspend(struct early_suspend *h);
static void ami304_late_resume(struct early_suspend *h);
static atomic_t ami304_report_enabled = ATOMIC_INIT(0);
#endif

#if defined(CONFIG_PM)
static int ami304_suspend(struct device *device);
static int ami304_resume(struct device *device);
#endif

#define AMI_ORIENTATION_SENSOR		0
#define AMI_MAGNETIC_FIELD_SENSOR	1
#define AMI_ACCELEROMETER_SENSOR		2

/* Addresses to scan */
static unsigned short normal_i2c[] = { AMI304_I2C_ADDRESS, I2C_CLIENT_END };

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(ami304);

struct _ami304_data {
	rwlock_t lock;
	int mode;
	int rate;
	volatile int updated;
} ami304_data;

struct _ami304mid_data {
	rwlock_t datalock;
	rwlock_t ctrllock;
	int controldata[10];
	int yaw;
	int roll;
	int pitch;
	int nmx;
	int nmy;
	int nmz;
	int nax;
	int nay;
	int naz;
	int mag_status;
#if defined(AMI304_MAG_OFFSET_ADJ)
	u8 coar[3];
	u8 fine[3];
#endif
} ami304mid_data;

struct ami304_i2c_data {
	struct input_dev *input_dev;
	struct i2c_client *client;
};

static atomic_t dev_open_count;
static atomic_t hal_open_count;
static atomic_t daemon_open_count;

static atomic_t o_status;
static atomic_t m_status;
static atomic_t a_status;

#if defined(AMI304_MAG_OFFSET_ADJ)
#define		OFFX_REG		0x6c
#define		OFFY_REG		0x72
#define		OFFZ_REG		0x78

#define SEH_SEQ_1_0 0
#define SEH_SEQ_1_1 1
#define SEH_SEQ_1_2 2
#define SEH_SEQ_2_0 3
#define SEH_SEQ_2_1 4
#define SEH_SEQ_END 5
#define SEH_SEQ_ERR 6

#define SEH_OVER_MIN 100
#define SEH_OVER_MAX 3950

static void AMI304_ChageWindowSeq1_0(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out);
static void AMI304_ChageWindowSeq1_1(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out);
static void AMI304_ChageWindowSeq1_2(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out);
static void AMI304_ChageWindowSeq2_0(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out);
static void AMI304_ChageWindowSeq2_1(u8 axis , u8 seq[][2], s8 *fine,s16 *mea);

static int AMI304_I2c_Read(u8 regaddr, u8 *buf, u8 buf_len)
{
	int res = 0;

	res = i2c_master_send(ami304_i2c_client, &regaddr, 1);
	if (res<=0) goto exit_AMI304_I2c_Read;
	res = i2c_master_recv(ami304_i2c_client, buf, buf_len);
	if (res<=0) goto exit_AMI304_I2c_Read;
	
	return res;

exit_AMI304_I2c_Read:
	return res;
}

static int AMI304_I2c_Read_W(u8 regaddr, u16 *buf, u8 buf_len)
{
	int	res;

	res = AMI304_I2c_Read(regaddr,(u8*)&buf[0],buf_len*2);
	if (res<=0) goto exit_AMI304_I2c_Read_W;
	
	return res;

exit_AMI304_I2c_Read_W:
	return res;
}

static int AMI304_I2c_Write(u8 reg_adr, u8 *buf, u8 buf_len)
{
	int res = 0;
	u8 databuf[buf_len+2];

	databuf[0] = reg_adr;
	memcpy(&databuf[1], buf, buf_len);
	databuf[buf_len+1] = 0x00;
	res = i2c_master_send(ami304_i2c_client, databuf, buf_len+1);	
	if (res<=0) goto exit_AMI304_I2c_Write;	
	return res;

exit_AMI304_I2c_Write:
	return res;
}

static int AMI304_I2c_Write_W(u8 reg_adr, u16 *buf, u8 buf_len)
{
	int	res;

	res = AMI304_I2c_Write(reg_adr,(u8 *)&buf[0],2*buf_len);
	if (res<=0) goto exit_AMI304_I2c_Write_W;

	return res;
	
exit_AMI304_I2c_Write_W:
	return res;	
}

static int AMI304_GetOffset(u8 *coar, u8 *fine)
{

	int res = 0;	
	u16	 wk[3];	

	res = AMI304_I2c_Read_W(OFFX_REG,&wk[0],1);
	if (res<=0) goto exit_AMI304_GetOffset;

	res = AMI304_I2c_Read_W(OFFY_REG,&wk[1],1);
	if (res<=0) goto exit_AMI304_GetOffset;

	res = AMI304_I2c_Read_W(OFFZ_REG,&wk[2],1);	
	if (res<=0) goto exit_AMI304_GetOffset;

	coar[0] = (wk[0] & 0x01c0) >> 6;
	coar[1] = (wk[1] & 0x01c0) >> 6;
	coar[2] = (wk[2] & 0x01c0) >> 6;

	fine[0] = (wk[0] & 0x003f);
	fine[1] = (wk[1] & 0x003f);
	fine[2] = (wk[2] & 0x003f);

#if defined(AMI304_TEST)
	printk(KERN_INFO "AMI304:%s Read Magnetic Offset : x=%d y=%d z=%d\n",__FUNCTION__, wk[0], wk[1], wk[2]);
#endif

exit_AMI304_GetOffset:

	return res;
}

static int AMI304_SetOffset(u8 *coar,u8 *fine)
{
	int	 res =0;
	u16	 wk[3];

	wk[0] = ( (u16)coar[0]<< 6) | (u16)fine[0];
	wk[1] = ( (u16)coar[1]<< 6) | (u16)fine[1];
	wk[2] = ( (u16)coar[2]<< 6) | (u16)fine[2];

	/* x */
	res = AMI304_I2c_Write_W(OFFX_REG,&wk[0],1);
	if (res<=0) goto exit_AMI304_SetOffset;

	/* y */
	res = AMI304_I2c_Write_W(OFFY_REG,&wk[1],1);
	if (res<=0) goto exit_AMI304_SetOffset;

	/* z */
	res = AMI304_I2c_Write_W(OFFZ_REG,&wk[2],1);	
	if (res<=0) goto exit_AMI304_SetOffset;

#if defined(AMI304_TEST)
	printk(KERN_INFO "AMI304:%s Set Magnetic Offset : x=%d y=%d z=%d\n",__FUNCTION__, wk[0], wk[1], wk[2]);
#endif

exit_AMI304_SetOffset:

	return res;
}

static void AMI304_ChageWindowSeq1_0(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out)
{
	if (mea[axis] < SEH_OVER_MIN ) {
		seq[axis][0] = SEH_SEQ_1_1;
		seq[axis][1] = 0;
	} else if ((SEH_OVER_MIN < mea[axis]) &&  (mea[axis] < SEH_OVER_MAX)) {
		AMI304_ChageWindowSeq2_0(axis,seq,coar,fine,mea, fine_out);
	} else {
		seq[axis][0] = SEH_SEQ_1_2;
		seq[axis][1] = 0;
	}
}
 
static void AMI304_ChageWindowSeq1_1(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out)
{
	static u16 mea_old[6];

	if (seq[axis][1] == 0) 
	{
		coar[axis] +=1;	
		seq[axis][1] = 1;

		mea_old[axis] = mea[axis];
	
	} else {

		if ((mea_old[axis] < 2048 ) && (mea[axis] > SEH_OVER_MAX))
		{
			AMI304_ChageWindowSeq2_1(axis,seq,fine,mea);
		} else if (mea[axis] < SEH_OVER_MIN )
		{
			coar[axis] +=1;
		} else {
			AMI304_ChageWindowSeq2_0(axis,seq,coar,fine,mea, fine_out);
		}
	}
	// check coarse 
	if (( coar[axis] < 0 ) ||  ( 7 < coar[axis]))
	{
		seq[axis][0] = SEH_SEQ_ERR;	
		seq[axis][1] = 1;
	}
}

static void AMI304_ChageWindowSeq1_2(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out)
{
	if (seq[axis][1] == 0) 
	{
		coar[axis] -=1;	
		seq[axis][1] = 1;
	
	} else {

		if (mea[axis] < SEH_OVER_MAX)	
		{
			AMI304_ChageWindowSeq2_0(axis,seq,coar,fine,mea, fine_out);
		} else { // SEH_OVER_MAX < mea[axis] 
			coar[axis] -=1;
		}

	}
	// check coarse 
	if (( coar[axis] < 0 ) ||  ( 7 < coar[axis]))
	{
		seq[axis][0] = SEH_SEQ_ERR;	
		seq[axis][1] = 1;
	}
}

static void AMI304_ChageWindowSeq2_0(u8 axis , u8 seq[][2], s8 *coar, s8 *fine, s16 *mea, u16 *fine_out)
{

	//float wk;
	int wk, wk2;
	s8 wk_fine;

	//wk = (float)(mea[axis] - 2048 ) / fine_out[axis];
	wk = (mea[axis] - 2048 ) / fine_out[axis];
	wk2 = (mea[axis] - 2048 ) % fine_out[axis];

	if ( wk > 0 ) {
		//wk_fine = fine[axis] + (s16)(wk + 0.5);
		if(wk2*2 >= fine_out[axis])
			wk_fine = fine[axis] + wk;
		else
			wk_fine = fine[axis] + wk+1;
	} else {
		//wk_fine = fine[axis] + (s16)(wk - 0.5);
		if(wk2*(-2) >= fine_out[axis])
			wk_fine = fine[axis] + wk;
		else
			wk_fine = fine[axis] + wk-1;
	}

	if ( wk_fine < 0 ) {
		seq[axis][0]  = SEH_SEQ_1_1;
		seq[axis][1] = 0;
	} else if ((0<= wk_fine) && ( wk_fine <=63))
	{
		fine[axis] = wk_fine;
		seq[axis][0] = SEH_SEQ_END;
		seq[axis][1] = 0;
	} else {
		seq[axis][0] = SEH_SEQ_1_2;
		seq[axis][1] = 0;
	}
}

static void AMI304_ChageWindowSeq2_1(u8 axis , u8 seq[][2], s8 *fine,s16 *mea)
{

	if (  SEH_OVER_MAX < mea[axis]  ) {
		fine[axis] += 5;
	} else {
		seq[axis][0] = SEH_SEQ_2_0;
		seq[axis][1] = 0;
	}

}

static int AMI304_Mea(s16 *val)
{

	s16 mi_mes[3];
	u8 wk_buf[9];
	int	 res;

	memset(wk_buf,0x00,9);

	wk_buf[0] = 0x40;
	res = AMI304_I2c_Write(AMI304_REG_CTRL3,wk_buf,1);
	if (res<=0) goto exit_AMI304_Mea;

	res = AMI304_I2c_Read(AMI304_REG_DATAXH,&wk_buf[0],6);
	if (res<=0) goto exit_AMI304_Mea;

	mi_mes[0] = (s16)wk_buf[1] << 8 | wk_buf[0];	// x-axis
	mi_mes[1] = (s16)wk_buf[3] << 8 | wk_buf[2];	// y-axis
	mi_mes[2] = (s16)wk_buf[5] << 8 | wk_buf[4];	// z-axis

	val[0] = mi_mes[0]+2048;
	val[1] = mi_mes[1]+2048;
	val[2] = mi_mes[2]+2048;

	return res;
	
exit_AMI304_Mea:
	return res;

}

static int AMI304_SearchOffset(void)
{
	int res;
	s16 mea[7];
	int i;
	u8 run_flg[3] = {1,1,1};
	u8 seq[3][2] ={{SEH_SEQ_1_0,0},
		       {SEH_SEQ_1_0,0},
		       {SEH_SEQ_1_0,0}};

	u8 wk_coar[3];
	u8 wk_fine[3];

	u16 g_au16fine_output[3];
	u16 buf[8];

	res = AMI304_I2c_Read_W(0x92,buf,1);
	if (res<=0) goto exit_AMI304_SearchOffset;

	g_au16fine_output[0] = (buf[0] & 0xff00) >> 8;
	g_au16fine_output[1] = (buf[0] & 0x00ff);
	g_au16fine_output[2] = (buf[1] & 0xff00) >> 8;

	res = AMI304_GetOffset(wk_coar,wk_fine);
	if (res<=0) goto exit_AMI304_SearchOffset;

	buf[0] = 0x0001;
	res = AMI304_I2c_Write_W(AMI304_REG_CTRL4,buf,1);
	if (res<=0) goto exit_AMI304_SearchOffset;

	res = AMI304_Mea(mea);
	if (res<=0) goto exit_AMI304_SearchOffset;

	while (1) {
		for (i=0;i<3;i++) {
			
			// cal coarse, fine
			if (run_flg[i] == 1 ) {

				switch (seq[i][0]) {
				case SEH_SEQ_1_0:
					AMI304_ChageWindowSeq1_0(i,seq,(s8 *)wk_coar,(s8 *)wk_fine,mea, g_au16fine_output);
					break;
				case SEH_SEQ_1_1:
					AMI304_ChageWindowSeq1_1(i,seq,(s8 *)wk_coar,(s8 *)wk_fine,mea, g_au16fine_output);
					break;
				case SEH_SEQ_1_2:
					AMI304_ChageWindowSeq1_2(i,seq,(s8 *)wk_coar,(s8 *)wk_fine,mea, g_au16fine_output);
					break;
				case SEH_SEQ_2_0:
					AMI304_ChageWindowSeq2_0(i,seq,(s8 *)wk_coar,(s8 *)wk_fine,mea, g_au16fine_output);
					break;
				case SEH_SEQ_2_1:
					break;
				case SEH_SEQ_END:
					run_flg[i] = 0;
					break;
				case SEH_SEQ_ERR:
					res = SEH_SEQ_ERR;
					goto exit_AMI304_SearchOffset;
				}
			}
		}

		res = AMI304_SetOffset((u8 *)&wk_coar[0],(u8 *)&wk_fine[0]);
		if (res<=0) goto exit_AMI304_SearchOffset;

		write_lock(&ami304mid_data.ctrllock);
		ami304mid_data.coar[0] = wk_coar[0];
		ami304mid_data.coar[1] = wk_coar[1];
		ami304mid_data.coar[2] = wk_coar[2];
		ami304mid_data.fine[0] = wk_fine[0];
		ami304mid_data.fine[1] = wk_fine[1];
		ami304mid_data.fine[2] = wk_fine[2];
		write_unlock(&ami304mid_data.ctrllock);

		res = AMI304_Mea(mea);
		if (res<=0) goto exit_AMI304_SearchOffset;

		if ((run_flg[0] == 0) && (run_flg[1] == 0) && (run_flg[2] == 0))
		{
			break;
		}
	}

	buf[0] = 0x0000;
	res = AMI304_I2c_Write_W(AMI304_REG_CTRL4,buf,1);
	if (res<=0) goto exit_AMI304_SearchOffset;

#if defined(AMI304_TEST)
	printk(KERN_INFO "AMI304:%s success\n",__FUNCTION__);
#endif
	AMI304_Init(AMI304_FORCE_MODE); // default is Force State

	return 0;
	
exit_AMI304_SearchOffset:

	AMI304_SetOffset((u8 *)&wk_coar[0],(u8 *)&wk_fine[0]);
	write_lock(&ami304mid_data.ctrllock);
	ami304mid_data.coar[0] = wk_coar[0];
	ami304mid_data.coar[1] = wk_coar[1];
	ami304mid_data.coar[2] = wk_coar[2];
	ami304mid_data.fine[0] = wk_fine[0];
	ami304mid_data.fine[1] = wk_fine[1];
	ami304mid_data.fine[2] = wk_fine[2];
	write_unlock(&ami304mid_data.ctrllock);
	buf[0] = 0x0000;
	res = AMI304_I2c_Write_W(AMI304_REG_CTRL4,buf,1);

	AMI304_Init(AMI304_FORCE_MODE); // default is Force State
	
	if (res<=0) {
		printk(KERN_ERR "AMI304_SearchOffset error: res value=%d\n", res);
	}

	return -3;
}
#endif

static int AMI304_Init(int mode)
{
	u8 databuf[10];
	u8 regaddr;
	u8 ctrl1, ctrl2, ctrl3;
	int res = 0;

	regaddr = AMI304_REG_CTRL1;
	res = i2c_master_send(ami304_i2c_client, &regaddr, 1);
	if (res<=0) goto exit_AMI304_Init;
	res = i2c_master_recv(ami304_i2c_client, &ctrl1, 1);
	if (res<=0) goto exit_AMI304_Init;

	regaddr = AMI304_REG_CTRL2;
	res = i2c_master_send(ami304_i2c_client, &regaddr, 1);
	if (res<=0) goto exit_AMI304_Init;
	res = i2c_master_recv(ami304_i2c_client, &ctrl2, 1);
	if (res<=0) goto exit_AMI304_Init;

	regaddr = AMI304_REG_CTRL3;
	res = i2c_master_send(ami304_i2c_client, &regaddr, 1);
	if (res<=0) goto exit_AMI304_Init;
	res = i2c_master_recv(ami304_i2c_client, &ctrl3, 1);
	if (res<=0) goto exit_AMI304_Init;

	databuf[0] = AMI304_REG_CTRL1;
	if( mode==AMI304_FORCE_MODE )
	{
		databuf[1] = ctrl1 | AMI304_CTRL1_PC1 | AMI304_CTRL1_FS1_FORCE;
		write_lock(&ami304_data.lock);
		ami304_data.mode = AMI304_FORCE_MODE;
		write_unlock(&ami304_data.lock);
	}
	else
	{
		databuf[1] = ctrl1 | AMI304_CTRL1_PC1 | AMI304_CTRL1_FS1_NORMAL | AMI304_CTRL1_ODR1;
		write_lock(&ami304_data.lock);
		ami304_data.mode = AMI304_NORMAL_MODE;
		write_unlock(&ami304_data.lock);
	}
	res = i2c_master_send(ami304_i2c_client, databuf, 2);
	if (res<=0) goto exit_AMI304_Init;

	databuf[0] = AMI304_REG_CTRL2;
	databuf[1] = ctrl2 | AMI304_CTRL2_DREN;
	res = i2c_master_send(ami304_i2c_client, databuf, 2);
	if (res<=0) goto exit_AMI304_Init;

	databuf[0] = AMI304_REG_CTRL3;
	databuf[1] = ctrl3 | AMI304_CTRL3_B0_LO_CLR;
	res = i2c_master_send(ami304_i2c_client, databuf, 2);
	if (res<=0) goto exit_AMI304_Init;

#if defined(AMI304_MAG_OFFSET_ADJ)
	//AMI304_SearchOffset(); //just for test.
#endif

exit_AMI304_Init:
	if (res<=0) {
		if (printk_ratelimit()) {
			AMIE("I2C error: ret value=%d\n", res);
		}
		return -3;
	}
	return 0;
}

static int AMI304_SetMode(int newmode)
{
	int mode = 0;

	read_lock(&ami304_data.lock);
	mode = ami304_data.mode;
	read_unlock(&ami304_data.lock);

	if (mode == newmode)
		return 0;

	return AMI304_Init(newmode);
}

static int AMI304_ReadChipInfo(char *buf, int bufsize)
{
	if ((!buf)||(bufsize<=30))
		return -1;
	if (!ami304_i2c_client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "AMI304 Chip");
	return 0;
}

static int AMI304_ReadSensorData(char *buf, int bufsize)
{
	char cmd;
	int mode = 0;
	unsigned char databuf[10] = {0,};
	int res = 0;

	if ((!buf)||(bufsize<=80))
		return -1;
	if (!ami304_i2c_client)
	{
		*buf = 0;
		return -2;
	}

	read_lock(&ami304_data.lock);
	mode = ami304_data.mode;
	read_unlock(&ami304_data.lock);

	databuf[0] = AMI304_REG_CTRL3;
	databuf[1] = AMI304_CTRL3_FORCE_BIT;
	res = i2c_master_send(ami304_i2c_client, databuf, 2);
	if (res<=0) goto exit_AMI304_ReadSensorData;

	// We can read all measured data in once
	cmd = AMI304_REG_DATAXH;
	res = i2c_master_send(ami304_i2c_client, &cmd, 1);
	if (res<=0) goto exit_AMI304_ReadSensorData;
	res = i2c_master_recv(ami304_i2c_client, &(databuf[0]), 6);
	if (res<=0) goto exit_AMI304_ReadSensorData;

	sprintf(buf, "%02x %02x %02x %02x %02x %02x", databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);

	if (AMI304_DEBUG_DEV_STATUS & ami304_debug_mask)
	{
		int mx, my, mz;
		mx = my = mz = 0;

		mx = (int)(databuf[0] | (databuf[1] << 8));
		my = (int)(databuf[2] | (databuf[3] << 8));
		mz = (int)(databuf[4] | (databuf[5] << 8));

		if (mx>32768)  mx = mx-65536;
		if (my>32768)  my = my-65536;
		if (mz>32768)  mz = mz-65536;

		//AMID("X=%d, Y=%d, Z=%d\n", (int)(databuf[0] | (databuf[1]  << 8)), (int)(databuf[2] | (databuf[3] << 8)), (int)(databuf[4] | (databuf[5] << 8)));
		AMID("X=%d, Y=%d, Z=%d\n", mx, my, mz);
	}

exit_AMI304_ReadSensorData:
	if (res<=0) {
		AMIE("I2C error: ret value=%d\n", res);
		return -3;
	}
	return 0;
}

static int AMI304_ReadPostureData(char *buf, int bufsize)
{
	if ((!buf)||(bufsize<=80))
		return -1;

	read_lock(&ami304mid_data.datalock);
	sprintf(buf, "%d %d %d %d", ami304mid_data.yaw, ami304mid_data.pitch, ami304mid_data.roll, ami304mid_data.mag_status);
	read_unlock(&ami304mid_data.datalock);
	return 0;
}

static int AMI304_ReadCaliData(char *buf, int bufsize)
{
	if ((!buf)||(bufsize<=80))
		return -1;

	read_lock(&ami304mid_data.datalock);
	sprintf(buf, "%d %d %d %d %d %d %d", ami304mid_data.nmx, ami304mid_data.nmy, ami304mid_data.nmz,ami304mid_data.nax,ami304mid_data.nay,ami304mid_data.naz,ami304mid_data.mag_status);
	read_unlock(&ami304mid_data.datalock);
	return 0;
}

static int AMI304_ReadMiddleControl(char *buf, int bufsize)
{
	if ((!buf)||(bufsize<=80))
		return -1;

	read_lock(&ami304mid_data.ctrllock);
	sprintf(buf, "%d %d %d %d %d %d %d %d %d %d",
		ami304mid_data.controldata[0], ami304mid_data.controldata[1], ami304mid_data.controldata[2],ami304mid_data.controldata[3],ami304mid_data.controldata[4],
		ami304mid_data.controldata[5], ami304mid_data.controldata[6], ami304mid_data.controldata[7], ami304mid_data.controldata[8], ami304mid_data.controldata[9]);
	read_unlock(&ami304mid_data.ctrllock);
	return 0;
}

static int AMI304_Report_Value(int en_dis)
{
	struct ami304_i2c_data *data = i2c_get_clientdata(ami304_i2c_client);
	char report_enable = 0;

	if( !en_dis )
		return 0;

	if(atomic_read(&o_status))
	{
		input_report_abs(data->input_dev, ABS_RX, ami304mid_data.yaw);	/* yaw */
		input_report_abs(data->input_dev, ABS_RY, ami304mid_data.pitch);/* pitch */
		input_report_abs(data->input_dev, ABS_RZ, ami304mid_data.roll);/* roll */
		input_report_abs(data->input_dev, ABS_RUDDER, ami304mid_data.mag_status);/* status of orientation sensor */
		report_enable = 1;
	}

	if(atomic_read(&a_status))
	{
		input_report_abs(data->input_dev, ABS_X, ami304mid_data.nax);/* x-axis raw acceleration */
		input_report_abs(data->input_dev, ABS_Y, ami304mid_data.nay);/* y-axis raw acceleration */
		input_report_abs(data->input_dev, ABS_Z, ami304mid_data.naz);/* z-axis raw acceleration */
		report_enable = 1;
	}

	if(atomic_read(&m_status))
	{
		input_report_abs(data->input_dev, ABS_HAT0X, ami304mid_data.nmx); /* x-axis of raw magnetic vector */
		input_report_abs(data->input_dev, ABS_HAT0Y, ami304mid_data.nmy); /* y-axis of raw magnetic vector */
		input_report_abs(data->input_dev, ABS_BRAKE, ami304mid_data.nmz); /* z-axis of raw magnetic vector */
		input_report_abs(data->input_dev, ABS_WHEEL, ami304mid_data.mag_status);/* status of magnetic sensor */
		report_enable = 1;
	}

	if (AMI304_DEBUG_DEV_DEBOUNCE & ami304_debug_mask)
	{
		AMID("yaw: %d, pitch: %d, roll: %d\n", ami304mid_data.yaw, ami304mid_data.pitch, ami304mid_data.roll);
		AMID("nax: %d, nay: %d, naz: %d\n", ami304mid_data.nax, ami304mid_data.nay, ami304mid_data.naz);
		AMID("nmx: %d, nmy: %d, nmz: %d\n", ami304mid_data.nmx, ami304mid_data.nmy, ami304mid_data.nmz);
		AMID("mag_status: %d\n", ami304mid_data.mag_status);
	}

	if(report_enable)
		input_sync(data->input_dev);

	return 0;
}

static ssize_t show_chipinfo_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	char strbuf[AMI304_BUFSIZE];
	AMI304_ReadChipInfo(strbuf, AMI304_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

static ssize_t show_sensordata_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	char strbuf[AMI304_BUFSIZE];
	AMI304_ReadSensorData(strbuf, AMI304_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

static ssize_t show_posturedata_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	char strbuf[AMI304_BUFSIZE];
	AMI304_ReadPostureData(strbuf, AMI304_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

static ssize_t show_calidata_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	char strbuf[AMI304_BUFSIZE];
	AMI304_ReadCaliData(strbuf, AMI304_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

static ssize_t show_midcontrol_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	char strbuf[AMI304_BUFSIZE];
	AMI304_ReadMiddleControl(strbuf, AMI304_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

static ssize_t store_midcontrol_value(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	write_lock(&ami304mid_data.ctrllock);
	memcpy(&ami304mid_data.controldata[0], buf, sizeof(int)*10);
 	write_unlock(&ami304mid_data.ctrllock);
	return count;
}

static ssize_t show_mode_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	int mode=0;
	read_lock(&ami304_data.lock);
	mode = ami304_data.mode;
	read_unlock(&ami304_data.lock);
	return sprintf(buf, "%d\n", mode);
}

static ssize_t store_mode_value(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int mode = 0;
	sscanf(buf, "%d", &mode);
 	AMI304_SetMode(mode);
	return count;
}

/* Test mode attribute */
static ssize_t show_pitch_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ami304mid_data.pitch);
}

static ssize_t show_roll_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ami304mid_data.roll);
}

static DEVICE_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DEVICE_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DEVICE_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DEVICE_ATTR(calidata, S_IRUGO, show_calidata_value, NULL);
static DEVICE_ATTR(midcontrol, S_IRUGO | S_IWUSR, show_midcontrol_value, store_midcontrol_value );
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, show_mode_value, store_mode_value );
static DEVICE_ATTR(pitch, S_IRUGO | S_IWUSR, show_pitch_value, NULL);
static DEVICE_ATTR(roll, S_IRUGO | S_IWUSR, show_roll_value, NULL);

static int ami304_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	if( atomic_cmpxchg(&dev_open_count, 0, 1)==0 ) {
		if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
			AMID("Open device node:ami304\n");
		ret = nonseekable_open(inode, file);
	}
	return ret;
}

static int ami304_release(struct inode *inode, struct file *file)
{
	atomic_set(&dev_open_count, 0);
	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("Release device node:ami304\n");
	return 0;
}

static int ami304_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)
{
	char strbuf[AMI304_BUFSIZE];
	int controlbuf[10];
	void __user *data;
	int retval=0;
	int mode=0;

	switch (cmd) {
		case AMI304_IOCTL_INIT:
			read_lock(&ami304_data.lock);
			mode = ami304_data.mode;
			read_unlock(&ami304_data.lock);
			AMI304_Init(mode);
			break;

		case AMI304_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadChipInfo(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case AMI304_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadSensorData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case AMI304_IOCTL_READ_POSTUREDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadPostureData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

	        case AMI304_IOCTL_READ_CALIDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadCaliData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
	        	break;

	        case AMI304_IOCTL_READ_CONTROL:
			read_lock(&ami304mid_data.ctrllock);
			memcpy(controlbuf, &ami304mid_data.controldata[0], sizeof(controlbuf));
			read_unlock(&ami304mid_data.ctrllock);
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_to_user(data, controlbuf, sizeof(controlbuf))) {
				retval = -EFAULT;
				goto err_out;
			}
	        	break;

		case AMI304_IOCTL_SET_CONTROL:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(controlbuf, data, sizeof(controlbuf))) {
				retval = -EFAULT;
				goto err_out;
			}
			write_lock(&ami304mid_data.ctrllock);
			memcpy(&ami304mid_data.controldata[0], controlbuf, sizeof(controlbuf));
			write_unlock(&ami304mid_data.ctrllock);
			break;

		case AMI304_IOCTL_SET_MODE:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(&mode, data, sizeof(mode))) {
				retval = -EFAULT;
				goto err_out;
			}
			AMI304_SetMode(mode);
			break;

		default:
			if (AMI304_DEBUG_USER_ERROR & ami304_debug_mask)
				AMIE("not supported command= 0x%04x\n", cmd);
			retval = -ENOIOCTLCMD;
			break;
	}

err_out:
	return retval;
}

static int ami304daemon_open(struct inode *inode, struct file *file)
{
	//return nonseekable_open(inode, file);
	int ret = -1;
	if( atomic_cmpxchg(&daemon_open_count, 0, 1)==0 ) {
		if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
			AMID("Open device node:ami304daemon\n");
		ret = 0;
	}
	return ret;
}

static int ami304daemon_release(struct inode *inode, struct file *file)
{
	atomic_set(&daemon_open_count, 0);
	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("Release device node:ami304daemon\n");
	return 0;
}

static int ami304daemon_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	int valuebuf[4];
	int calidata[7];
	int controlbuf[10];
	char strbuf[AMI304_BUFSIZE];
	void __user *data;
	int retval=0;
	int mode;
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	int en_dis_Report=1;
#endif
#if defined(AMI304_MAG_OFFSET_ADJ)
	int mag_offset[3];
	u8 wk_coar[3];
	u8 wk_fine[3];
#endif

	switch (cmd) {

		case AMI304MID_IOCTL_GET_SENSORDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadSensorData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case AMI304MID_IOCTL_SET_POSTURE:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(&valuebuf, data, sizeof(valuebuf))) {
				retval = -EFAULT;
				goto err_out;
			}
			write_lock(&ami304mid_data.datalock);
			ami304mid_data.yaw   = valuebuf[0];
			ami304mid_data.pitch = valuebuf[1];
			ami304mid_data.roll  = valuebuf[2];
			ami304mid_data.mag_status = valuebuf[3];
			write_unlock(&ami304mid_data.datalock);
			break;

		case AMI304MID_IOCTL_SET_CALIDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(&calidata, data, sizeof(calidata))) {
				retval = -EFAULT;
				goto err_out;
			}
			write_lock(&ami304mid_data.datalock);
			ami304mid_data.nmx = calidata[0];
			ami304mid_data.nmy = calidata[1];
			ami304mid_data.nmz = calidata[2];
			ami304mid_data.nax = calidata[3];
			ami304mid_data.nay = calidata[4];
			ami304mid_data.naz = calidata[5];
			ami304mid_data.mag_status = calidata[6];
			write_unlock(&ami304mid_data.datalock);
#if defined(CONFIG_HAS_EARLYSUSPEND)
			/*
			 * Disable input report at early suspend state
			 * On-Demand Governor set max cpu frequency when input event is appeared
			 */
			AMI304_Report_Value(atomic_read(&ami304_report_enabled));
#else
			AMI304_Report_Value(en_dis_Report);
#endif
			break;

		case AMI304MID_IOCTL_GET_CONTROL:
			read_lock(&ami304mid_data.ctrllock);
			memcpy(controlbuf, &ami304mid_data.controldata[0], sizeof(controlbuf));
			read_unlock(&ami304mid_data.ctrllock);
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_to_user(data, controlbuf, sizeof(controlbuf))) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case AMI304MID_IOCTL_SET_CONTROL:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(controlbuf, data, sizeof(controlbuf))) {
				retval = -EFAULT;
				goto err_out;
			}
			write_lock(&ami304mid_data.ctrllock);
			memcpy(&ami304mid_data.controldata[0], controlbuf, sizeof(controlbuf));
			write_unlock(&ami304mid_data.ctrllock);
			break;

		case AMI304MID_IOCTL_SET_MODE:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(&mode, data, sizeof(mode))) {
				retval = -EFAULT;
				goto err_out;
			}
			AMI304_SetMode(mode);
			break;
								
		//Add for input_device sync			
		case AMI304MID_IOCTL_SET_REPORT:
#if defined(CONFIG_HAS_EARLYSUSPEND)
			break;
#else
			data = (void __user *) arg;
			if (data == NULL)
				break;	
			if (copy_from_user(&en_dis_Report, data, sizeof(mode))) {
				retval = -EFAULT;
				goto err_out;
			}				
//			read_lock(&ami304mid_data.datalock);
			AMI304_Report_Value(en_dis_Report);
//			read_lock(&ami304mid_data.datalock);
#endif
			break;

#if defined(AMI304_MAG_OFFSET_ADJ)
		case AMI304MID_IOCTL_MAG_OFFSET_ADJ:
			AMI304_SearchOffset();
			break;

		case AMI304MID_IOCTL_GET_MAG_OFFSET:
			read_lock(&ami304mid_data.ctrllock);
			mag_offset[0] = ( (u16)ami304mid_data.coar[0]<< 6) | (u16)ami304mid_data.fine[0];
			mag_offset[1] = ( (u16)ami304mid_data.coar[1]<< 6) | (u16)ami304mid_data.fine[1];
			mag_offset[2] = ( (u16)ami304mid_data.coar[2]<< 6) | (u16)ami304mid_data.fine[2];
			read_unlock(&ami304mid_data.ctrllock);			
			data = (void __user *) arg;
			if (data == NULL)
				break;	
			if (copy_to_user(data, mag_offset, sizeof(mag_offset))) {
				retval = -EFAULT;
				goto err_out;
			}					
			break;

		case AMI304MID_IOCTL_SET_MAG_OFFSET:
			data = (void __user *) arg;
			if (data == NULL)
				break;	
			if (copy_from_user(mag_offset, data, sizeof(mag_offset))) {
				retval = -EFAULT;
				goto err_out;
			}
			wk_coar[0] = (mag_offset[0] & 0x000001c0) >> 6;
			wk_coar[1] = (mag_offset[1] & 0x000001c0) >> 6;
			wk_coar[2] = (mag_offset[2] & 0x000001c0) >> 6;
			wk_fine[0] = (mag_offset[0] & 0x0000003f);
			wk_fine[1] = (mag_offset[1] & 0x0000003f);
			wk_fine[2] = (mag_offset[2] & 0x0000003f);
			AMI304_SetOffset((u8 *)&wk_coar,(u8 *)&wk_fine);
			write_lock(&ami304mid_data.ctrllock);
			ami304mid_data.coar[0] = wk_coar[0];
			ami304mid_data.coar[1] = wk_coar[1];
			ami304mid_data.coar[2] = wk_coar[2];
			ami304mid_data.fine[0] = wk_fine[0];
			ami304mid_data.fine[1] = wk_fine[1];
			ami304mid_data.fine[2] = wk_fine[2];
			write_unlock(&ami304mid_data.ctrllock);
			break;
#endif
		default:
			if (AMI304_DEBUG_USER_ERROR & ami304_debug_mask)
				AMIE("not supported command= 0x%04x\n", cmd);
			retval = -ENOIOCTLCMD;
			break;
	}

err_out:
	return retval;
}

static int ami304hal_open(struct inode *inode, struct file *file)
{
	//return nonseekable_open(inode, file);
	atomic_inc(&hal_open_count);
	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("Open device node:ami304hal %d times.\n", atomic_read(&hal_open_count));
	return 0;
}

static int ami304hal_release(struct inode *inode, struct file *file)
{
	atomic_dec(&hal_open_count);
	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("Release ami304hal, remainder is %d times.\n", atomic_read(&hal_open_count));
	return 0;
}

static int ami304hal_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)
{
	char strbuf[AMI304_BUFSIZE];
	void __user *data;
	int retval=0;
	unsigned int mode =0;
	int controlbuf[10];

	switch (cmd) {

		case AMI304HAL_IOCTL_GET_SENSORDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadSensorData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case AMI304HAL_IOCTL_GET_POSTURE:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadPostureData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
			break;

		case AMI304HAL_IOCTL_GET_CALIDATA:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			AMI304_ReadCaliData(strbuf, AMI304_BUFSIZE);
			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				retval = -EFAULT;
				goto err_out;
			}
	        	break;

		case AMI304HAL_IOCTL_SET_ACTIVE:
			data = (void __user *) arg;
			if (data == NULL)
				break;

			if (copy_from_user(&mode, data, sizeof(mode))) {
				retval = -EFAULT;
				goto err_out;
			}

			if (AMI304_DEBUG_GEN_INFO & ami304_debug_mask)
				AMID("ami304hal active sensor %d\n", mode);

			if(mode & (0x00000001<<AMI_ORIENTATION_SENSOR))
				atomic_set(&o_status, 1);
			else
				atomic_set(&o_status, 0);

			if(mode & (0x00000001<<AMI_MAGNETIC_FIELD_SENSOR))
				atomic_set(&m_status, 1);
			else
				atomic_set(&m_status, 0);

			if(mode & (0x00000001<<AMI_ACCELEROMETER_SENSOR))
				atomic_set(&a_status, 1);
			else
				atomic_set(&a_status, 0);

	        	break;

		case AMI304HAL_IOCTL_GET_CONTROL:
			read_lock(&ami304mid_data.ctrllock);
			memcpy(controlbuf, &ami304mid_data.controldata[0], sizeof(controlbuf));
			read_unlock(&ami304mid_data.ctrllock);
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_to_user(data, controlbuf, sizeof(controlbuf))) {
				retval = -EFAULT;
				goto err_out;
			}
			break;


		case AMI304HAL_IOCTL_SET_CONTROL:
			data = (void __user *) arg;
			if (data == NULL)
				break;
			if (copy_from_user(controlbuf, data, sizeof(controlbuf))) {
				retval = -EFAULT;
				goto err_out;
			}
			write_lock(&ami304mid_data.ctrllock);
			memcpy(&ami304mid_data.controldata[0], controlbuf, sizeof(controlbuf));
			write_unlock(&ami304mid_data.ctrllock);
			//AMID("Dleay setting = %dms\n", ami304mid_data.controldata[0] / 1000);
			break;

		default:
			if (AMI304_DEBUG_USER_ERROR & ami304_debug_mask)
				AMIE("not supported command= 0x%04x\n", cmd);
			retval = -ENOIOCTLCMD;
			break;
	}

err_out:
	return retval;
}

static struct file_operations ami304_fops = {
	.owner = THIS_MODULE,
	.open = ami304_open,
	.release = ami304_release,
	.ioctl = ami304_ioctl,
};

static struct miscdevice ami304_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ami304",
	.fops = &ami304_fops,
};


static struct file_operations ami304daemon_fops = {
	.owner = THIS_MODULE,
	.open = ami304daemon_open,
	.release = ami304daemon_release,
	.ioctl = ami304daemon_ioctl,
};

static struct miscdevice ami304daemon_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ami304daemon",
	.fops = &ami304daemon_fops,
};

static struct file_operations ami304hal_fops = {
	.owner = THIS_MODULE,
	.open = ami304hal_open,
	.release = ami304hal_release,
	.ioctl = ami304hal_ioctl,
};

static struct miscdevice ami304hal_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ami304hal",
	.fops = &ami304hal_fops,
};

static int __init ami304_probe(struct i2c_client *client, const struct i2c_device_id * devid)
{
	int err = 0;
	struct ami304_i2c_data *data;
	struct ecom_platform_data* ecom_pdata;

	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("motion start....!\n");

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		AMIE("adapter can NOT support I2C_FUNC_I2C.\n");
		return -ENODEV;
	}

	if (!(data = kmalloc(sizeof(struct ami304_i2c_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct ami304_i2c_data));

	i2c_set_clientdata(client, data);
	ami304_i2c_client = client;

	ecom_pdata = ami304_i2c_client->dev.platform_data;
	ecom_pdata->power(1);
	AMI304_Init(AMI304_FORCE_MODE); // default is Force State

	atomic_set(&o_status, 0);
	atomic_set(&m_status, 0);
	atomic_set(&a_status, 0);

#if defined(CONFIG_HAS_EARLYSUSPEND)
	ami304_sensor_early_suspend.suspend = ami304_early_suspend;
	ami304_sensor_early_suspend.resume = ami304_late_resume;
	register_early_suspend(&ami304_sensor_early_suspend);

	atomic_set(&ami304_report_enabled, 1);
#endif

	data->input_dev = input_allocate_device();
	if (!data->input_dev) {
		err = -ENOMEM;
		AMIE("ami304_i2c_detect: Failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	set_bit(EV_ABS, data->input_dev->evbit);
	/* yaw */
	input_set_abs_params(data->input_dev, ABS_RX, 0, 360, 0, 0);
	/* pitch */
	input_set_abs_params(data->input_dev, ABS_RY, -180, 180, 0, 0);
	/* roll */
	input_set_abs_params(data->input_dev, ABS_RZ, -90, 90, 0, 0);
	/* status of magnetic sensor */
	input_set_abs_params(data->input_dev, ABS_RUDDER, 0, 5, 0, 0);

	/* x-axis acceleration */
	input_set_abs_params(data->input_dev, ABS_X, -2000, 2000, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(data->input_dev, ABS_Y, -2000, 2000, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(data->input_dev, ABS_Z, -2000, 2000, 0, 0);

	/* x-axis of raw magnetic vector */
	input_set_abs_params(data->input_dev, ABS_HAT0X, -3000, 3000, 0, 0);
	/* y-axis of raw magnetic vector */
	input_set_abs_params(data->input_dev, ABS_HAT0Y, -3000, 3000, 0, 0);
	/* z-axis of raw magnetic vector */
	input_set_abs_params(data->input_dev, ABS_BRAKE, -3000, 3000, 0, 0);
	/* status of acceleration sensor */
	input_set_abs_params(data->input_dev, ABS_WHEEL, 0, 5, 0, 0);

	data->input_dev->name = "Acompass";

	err = input_register_device(data->input_dev);
	if (err) {
		AMIE("ami304_i2c_detect: Unable to register input device: %s\n",
		       data->input_dev->name);
		goto exit_input_register_device_failed;
	}
	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
	        AMID("register input device successfully!!!\n");

	err = misc_register(&ami304_device);
	if (err) {
		AMIE("ami304_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	err = device_create_file(&client->dev, &dev_attr_chipinfo);
	err = device_create_file(&client->dev, &dev_attr_sensordata);
	err = device_create_file(&client->dev, &dev_attr_posturedata);
	err = device_create_file(&client->dev, &dev_attr_calidata);
	err = device_create_file(&client->dev, &dev_attr_midcontrol);
	err = device_create_file(&client->dev, &dev_attr_mode);
	/* Test mode attribute */
	err = device_create_file(&client->dev, &dev_attr_pitch);
	err = device_create_file(&client->dev, &dev_attr_roll);

	err = misc_register(&ami304daemon_device);
	if (err) {
		AMIE("ami304daemon_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	err = misc_register(&ami304hal_device);
	if (err) {
		AMIE("ami304hal_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	return 0;
exit_misc_device_register_failed:
exit_input_register_device_failed:
	input_free_device(data->input_dev);
exit_input_dev_alloc_failed:
	kfree(data);
exit:
	return err;
}

static int ami304_remove(struct	i2c_client *client)
{
	struct ami304_i2c_data *data = i2c_get_clientdata(client);

	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);

	ami304_i2c_client = NULL;
	kfree(data);

	device_remove_file(&client->dev, &dev_attr_chipinfo);
	device_remove_file(&client->dev, &dev_attr_sensordata);
	device_remove_file(&client->dev, &dev_attr_posturedata);
	device_remove_file(&client->dev, &dev_attr_calidata);
	device_remove_file(&client->dev, &dev_attr_midcontrol);
	device_remove_file(&client->dev, &dev_attr_mode);
	/* Test mode attribute */
	device_remove_file(&client->dev, &dev_attr_pitch);
	device_remove_file(&client->dev, &dev_attr_roll);

	misc_deregister(&ami304_device);
	misc_deregister(&ami304daemon_device);
	misc_deregister(&ami304hal_device);

#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ami304_sensor_early_suspend);
#endif

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void ami304_early_suspend(struct early_suspend *h)
{
	atomic_set(&ami304_report_enabled, 0);
}

static void ami304_late_resume(struct early_suspend *h)
{
	atomic_set(&ami304_report_enabled, 1);
}
#endif

#if defined(CONFIG_PM)
static int ami304_suspend(struct device *device)
{
	struct ecom_platform_data* ecom_pdata;

	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("AMI304 suspend....!\n");

	ecom_pdata = ami304_i2c_client->dev.platform_data;
	ecom_pdata->power(0);

	return 0;
}

static int ami304_resume(struct device *device)
{
	struct ecom_platform_data* ecom_pdata;
	ecom_pdata = ami304_i2c_client->dev.platform_data;

	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("AMI304 resume....!\n");

	ecom_pdata->power(1);
	AMI304_Init(ami304_data.mode);

	return 0;
}
#endif

static const struct i2c_device_id motion_ids[] = {
	{ "ami304_sensor", 0 },
	{ },
};

#if defined(CONFIG_PM)
static struct dev_pm_ops ami304_pm_ops = {
	.suspend = ami304_suspend,
	.resume = ami304_resume,
};
#endif

static struct i2c_driver ami304_i2c_driver = {
	.probe		= ami304_probe,
	.remove		= ami304_remove,
	.id_table	= motion_ids,
	.driver = {
		.owner = THIS_MODULE,
		.name	= "ami304_sensor",
#if defined(CONFIG_PM)
		.pm	= &ami304_pm_ops,
#endif
	},
};

static int __init ami304_init(void)
{
	int ret;

	if (AMI304_DEBUG_FUNC_TRACE & ami304_debug_mask)
		AMID("AMI304 MI sensor driver: init\n");
	rwlock_init(&ami304mid_data.ctrllock);
	rwlock_init(&ami304mid_data.datalock);
	rwlock_init(&ami304_data.lock);
	memset(&ami304mid_data.controldata[0], 0, sizeof(int)*10);
	ami304mid_data.controldata[0] = 20*1000; //Loop Delay
	ami304mid_data.controldata[1] = 0; // Run
	ami304mid_data.controldata[2] = 0; // Disable Start-AccCali
	ami304mid_data.controldata[3] = 1; // Enable Start-Cali
	ami304mid_data.controldata[4] = 350; // MW-Timout
	ami304mid_data.controldata[5] = 10; // MW-IIRStrength_M
	ami304mid_data.controldata[6] = 10; // MW-IIRStrength_G
	atomic_set(&dev_open_count, 0);
	atomic_set(&hal_open_count, 0);
	atomic_set(&daemon_open_count, 0);

	ret = i2c_add_driver(&ami304_i2c_driver);
	if (ret) {
		AMIE("failed to probe i2c \n");
		i2c_del_driver(&ami304_i2c_driver);
	}

	return ret;
}

static void __exit ami304_exit(void)
{
	atomic_set(&dev_open_count, 0);
	atomic_set(&hal_open_count, 0);
	atomic_set(&daemon_open_count, 0);
	i2c_del_driver(&ami304_i2c_driver);
}

module_init(ami304_init);
module_exit(ami304_exit);

MODULE_AUTHOR("Kyle K.Y. Chen");
MODULE_DESCRIPTION("AMI304 MI sensor input_dev driver v1.0.5.10");
MODULE_LICENSE("GPL");
