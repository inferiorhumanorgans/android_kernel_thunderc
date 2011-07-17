/*  drivers/input/keyboard/synaptics_i2c_rmi.c *  * Copyright  (C) 2007  Google,
Inc. * * This software is licensed  under the terms of the GNU General  Public *
License version 2, as  published by the Free  Software Foundation, and *  may be
copied,  distributed,  and modified  under  those terms.  *  * This  program  is
distributed in  the hope  that it  will be  useful, *  but WITHOUT ANY WARRANTY;
without  even  the  implied  warranty of  *  MERCHANTABILITY  or  FITNESS FOR  A
PARTICULAR PURPOSE.  See the * GNU General Public License for more details. * */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/synaptics_i2c_rmi.h>
#include <mach/gpio.h>
#include <linux/i2c/twl4030.h>

#ifdef SYNAPTICS_FW_REFLASH
#include "synaptics_reflash.h"
#endif

#ifdef SYNAPTICS_FW_REFLASH
#define FW_REFLASH_SUCCEED 0
#endif

 #ifdef SYNAPTICS_TOUCH_DEBUG
 #define DEBUG_MSG(args...)	printk(KERN_INFO args)
 #else
 #define DEBUG_MSG(args...)
 #endif

 #ifdef SYNAPTICS_TOUCH_ERR
#define ERR_MSG(args...)	printk(KERN_ERR args)
 #else
#define ERR_MSG(args...)
 #endif
 
#define SYNAPTICS_TS_POLLING_TIME 	1 /* polling time(msec) when touch was pressed */ 

#define INT_STATUS_REG				0x14

#define SYNAPTICS_INT_REG			0x21
#define SYNAPTICS_CONTROL_REG		0x20
#define REPORT_MODE_2D				0x22
#define MAX_X_POS_LOW_REG			0x28
#define MAX_X_POS_HIGH_REG			0x29
#define MAX_Y_POS_LOW_REG			0x2A
#define MAX_Y_POS_HIGH_REG			0x2B

#define QUERY_BASE_REG				0xE3

#define SYNAPTICS_INT_FLASH		1<<0
#define SYNAPTICS_INT_STATUS 	1<<1
#define SYNAPTICS_INT_ABS0 		1<<2

#define SYNAPTICS_CONTROL_SLEEP 	1<<0
#define SYNAPTICS_CONTROL_NOSLEEP	1<<2

#define FINGER_MAX 2
#define START_ADDR      0x13
#define PRODUCT_ID_STRING_NUM	11
#define CMD_REG_BLOCK_NUM		38
/************************************/
/******* enum ***********************/
typedef enum {
	SYNAPTICS_2000 = 0,
	SYNAPTICS_2100,
	SYNAPTICS_3000,
};
/************************************/

/***********************************************************/
/**************** structure ***********************************/
struct synaptics_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	bool has_relative_report;
	struct hrtimer timer;
	struct work_struct  work;
#ifdef SYNAPTICS_FW_REFLASH
	struct work_struct  work_for_reflash;
#endif
	uint16_t max[2];
	int snap_state[2][2];
	int snap_down_on[2];
	int snap_down_off[2];
	int snap_up_on[2];
	int snap_up_off[2];
	int snap_down[2];
	int snap_up[2];
	uint32_t flags;
	int reported_finger_count;
	int8_t sensitivity_adjust;
	int (*power)(int on);
	struct early_suspend early_suspend;
	int fw_revision;
};

typedef struct									// synaptics 2000	// synaptics 21000
{
	unsigned char device_status_reg;            //0x13
	unsigned char interrupt_status_reg;			//0x14
	unsigned char finger_state_reg;				//0x15

	// Finger 0
	unsigned char X_high_position_finger0_reg;  //0x16
	unsigned char Y_high_position_finger0_reg;	//0x17
	unsigned char XY_low_position_finger0_reg;	//0x18
	unsigned char XY_width_finger0_reg;			//0x19
	unsigned char Z_finger0_reg;				//0x1A
	// Finger 1
	unsigned char X_high_position_finger1_reg;  //0x1B
	unsigned char Y_high_position_finger1_reg;	//0x1C
	unsigned char XY_low_position_finger1_reg;	//0x1D
	unsigned char XY_width_finger1_reg;			//0x1E
	unsigned char Z_finger1_reg;				//0x1F
 } ts_sensor_data;

typedef struct 									// synaptics 2000	// synaptics 21000
{
	unsigned char device_command;				//0x58				//0x5C
	unsigned char command_2d;					//0x59				//0x5D
	unsigned char bootloader_id0;				//0x5A				//0x5E
	unsigned char bootloader_id1;				//0x5B				//0x5F
	unsigned char flash_properties;				//0x5C				//0x60
	unsigned char block_size0;					//0x5D				//0x61
	unsigned char block_size1;					//0x5E				//0x62
	unsigned char firmware_block_cnt0;			//0x5F				//0x63
	unsigned char firmware_block_cnt1;			//0x60				//0x64
	unsigned char config_block_cnt0;			//0x61				//0x65
	unsigned char config_block_cnt1;			//0x62				//0x66
	unsigned char manufact_id_query;			//0x63				//0x67
	unsigned char product_properties_query;		//0x64				//0x68
	unsigned char customer_family_query;		//0x65				//0x69
	unsigned char firmware_revision_query;		//0x66				//0x6A
	unsigned char device_serialization_query0;	//0x67				//0x6B
	unsigned char device_serialization_query1;	//0x68				//0x6C
	unsigned char device_serialization_query2;	//0x69				//0x6D
	unsigned char device_serialization_query3;	//0x6A				//0x6E
	unsigned char device_serialization_query4;	//0x6B				//0x6F
	unsigned char device_serialization_query5;	//0x6C				//0x70
	unsigned char device_serialization_query6;	//0x6D				//0x71
	unsigned char product_id_query0;			//0x6E				//0x72
	unsigned char product_id_query1;			//0x6F				//0x73
	unsigned char product_id_query2;			//0x70				//0x74
	unsigned char product_id_query3;			//0x71				//0x75
	unsigned char product_id_query4;			//0x72				//0x76
	unsigned char product_id_query5;			//0x73				//0x77
	unsigned char product_id_query6;			//0x74				//0x78
	unsigned char product_id_query7;			//0x75				//0x79
	unsigned char product_id_query8;			//0x76				//0x7A
	unsigned char product_id_query9;			//0x77				//0x7B
	unsigned char per_device_query;				//0x78				//0x7C
	unsigned char reporting_mode_2d;			//0x79				//0x7D
	unsigned char number_x_electrode_2d;		//0x7A				//0x7E
	unsigned char number_y_electrode_2d;		//0x7B				//0x7F
	unsigned char maximum_electorde_2d;			//0x7C				//0x80
	unsigned char absolute_query_2d;			//0x7D				//0x81
}ts_sensor_command;

typedef struct {
	unsigned char finger_count;
	int X_position[FINGER_MAX];
	int Y_position[FINGER_MAX];
} ts_finger_data;
/***********************************************************/

/***********************************************************************************/
/*********** MACROS ****************************************************************/
// 0x00 - not present, 0x01 - present & accurate, 0x10 - present but not accurate, 0x11 - Reserved
#define TS_SNTS_GET_FINGER_STATE_0(finger_status_reg) \
		(finger_status_reg&0x03)
#define TS_SNTS_GET_FINGER_STATE_1(finger_status_reg) \
		((finger_status_reg&0x0C)>>2)
#define TS_SNTS_GET_FINGER_STATE_2(finger_status_reg) \
		((finger_status_reg&0x30)>>4)
#define TS_SNTS_GET_FINGER_STATE_3(finger_status_reg) \
      ((finger_status_reg&0xC0)>>6)
#define TS_SNTS_GET_FINGER_STATE_4(finger_status_reg) \
      (finger_status_reg&0x03)

#define TS_SNTS_GET_X_POSITION(high_reg, low_reg) \
		((int)(high_reg*0x10) + (int)(low_reg&0x0F))
#define TS_SNTS_GET_Y_POSITION(high_reg, low_reg) \
		((int)(high_reg*0x10) + (int)((low_reg&0xF0)/0x10))

#define TS_SNTS_HAS_PINCH(gesture_reg) \
		((gesture_reg&0x40)>>6)
#define TS_SNTS_HAS_FLICK(gesture_reg) \
		((gesture_reg&0x10)>>4)
#define TS_SNTS_HAS_DOUBLE_TAP(gesture_reg) \
		((gesture_reg&0x04)>>2)

#define TS_SNTS_GET_REPORT_RATE(device_control_reg) \
		((device_control_reg&0x40)>>6)
// 1st bit : '0' - Allow sleep mode, '1' - Full power without sleeping
// 2nd and 3rd bit : 0x00 - Normal Operation, 0x01 - Sensor Sleep
#define TS_SNTS_GET_SLEEP_MODE(device_control_reg) \
		(device_control_reg&0x07)
/***********************************************************************************/

/****************************************************************/
/**************** STATIC VARIABLE *********************************/
static struct workqueue_struct *synaptics_wq;
#ifdef SYNAPTICS_FW_REFLASH
static struct workqueue_struct *synaptics_fwdl_wq;
#endif
static ts_sensor_data ts_reg_data={0};
static ts_finger_data curr_ts_data;
static ts_sensor_command ts_cmd_reg_data={0};
static int ts_pre_state = 0; /* for checking the touch state */
//static int longpress_pre = 0;
//static int flicking = 0;
static uint16_t max_x, max_y;
static int kind_of_product = SYNAPTICS_2000;


#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h);
static void synaptics_ts_late_resume(struct early_suspend *h);
#endif
/****************************************************************/

#ifdef SYNAPTICS_FW_REFLASH
static void synaptics_ts_fw_reflash_work_func(struct work_struct *work)
{
	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work_for_reflash);
	int ret;
	int inactive_area_left;
	int inactive_area_right;
	int inactive_area_top;
	int inactive_area_bottom;
	int snap_left_on;
	int snap_left_off;
	int snap_right_on;
	int snap_right_off;
	int snap_top_on;
	int snap_top_off;
	int snap_bottom_on;
	int snap_bottom_off;
	int fuzz_x, fuzz_y, fuzz_p, fuzz_w;
	
	DEBUG_MSG("start F/W reflash for synaptics 2000 series!!\n");

	/* disable irq */
	if (ts->use_irq)
		disable_irq(ts->client->irq);

	if(SynaDoReflash(ts->client, ts->fw_revision) == FW_REFLASH_SUCCEED)
	{
		DEBUG_MSG("synaptics_ts_fw_reflash_work_func : SynaDoReflash succeed!\n");

#if 1
		ret = i2c_smbus_read_word_data(ts->client, MAX_X_POS_LOW_REG);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_x = (ret & 0xFF);

		ret = i2c_smbus_read_word_data(ts->client, MAX_X_POS_HIGH_REG);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_x |= (((ret & 0xFF) << 8) & 0xff00);
		ts->max[0] = max_x;

		ret = i2c_smbus_read_word_data(ts->client, MAX_Y_POS_LOW_REG);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_y = (ret & 0xFF);
				
		ret = i2c_smbus_read_word_data(ts->client, MAX_Y_POS_HIGH_REG);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_y |= (((ret & 0xFF) << 8) & 0xff00);
		ts->max[1] = max_y;

		DEBUG_MSG("synaptics_ts_probe : max_x = 0x%x\n",max_x);
		DEBUG_MSG("synaptics_ts_probe : max_y = 0x%x\n",max_y);

#else
		ret = i2c_smbus_read_word_data(ts->client, 0x28);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_x = (ret & 0xFF);

		ret = i2c_smbus_read_word_data(ts->client, 0x29);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_x |= (((ret & 0xFF) << 8) & 0xff00);
		ts->max[0] = max_x;

		ret = i2c_smbus_read_word_data(ts->client, 0x2A);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_y = (ret & 0xFF);
				
		ret = i2c_smbus_read_word_data(ts->client, 0x2B);
		if (ret < 0) {
			ERR_MSG("i2c_smbus_read_word_data failed\n");
		}
		max_y |= (((ret & 0xFF) << 8) & 0xff00);
		ts->max[1] = max_y;
				
		DEBUG_MSG("synaptics_ts_probe : max_x = 0x%x, max_y = 0x%x\n",max_x, max_y);
#endif

		inactive_area_left = inactive_area_left * max_x / 0x10000;
		inactive_area_right = inactive_area_right * max_x / 0x10000;
		inactive_area_top = inactive_area_top * max_y / 0x10000;
		inactive_area_bottom = inactive_area_bottom * max_y / 0x10000;
		snap_left_on = snap_left_on * max_x / 0x10000;
		snap_left_off = snap_left_off * max_x / 0x10000;
		snap_right_on = snap_right_on * max_x / 0x10000;
		snap_right_off = snap_right_off * max_x / 0x10000;
		snap_top_on = snap_top_on * max_y / 0x10000;
		snap_top_off = snap_top_off * max_y / 0x10000;
		snap_bottom_on = snap_bottom_on * max_y / 0x10000;
		snap_bottom_off = snap_bottom_off * max_y / 0x10000;
		fuzz_x = fuzz_x * max_x / 0x10000;
		fuzz_y = fuzz_y * max_y / 0x10000;
		ts->snap_down[!!(ts->flags & SYNAPTICS_SWAP_XY)] = -inactive_area_left;
		ts->snap_up[!!(ts->flags & SYNAPTICS_SWAP_XY)] = max_x + inactive_area_right;
		ts->snap_down[!(ts->flags & SYNAPTICS_SWAP_XY)] = -inactive_area_top;
		ts->snap_up[!(ts->flags & SYNAPTICS_SWAP_XY)] = max_y + inactive_area_bottom;
		ts->snap_down_on[!!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_left_on;
		ts->snap_down_off[!!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_left_off;
		ts->snap_up_on[!!(ts->flags & SYNAPTICS_SWAP_XY)] = max_x - snap_right_on;
		ts->snap_up_off[!!(ts->flags & SYNAPTICS_SWAP_XY)] = max_x - snap_right_off;
		ts->snap_down_on[!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_top_on;
		ts->snap_down_off[!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_top_off;
		ts->snap_up_on[!(ts->flags & SYNAPTICS_SWAP_XY)] = max_y - snap_bottom_on;
		ts->snap_up_off[!(ts->flags & SYNAPTICS_SWAP_XY)] = max_y - snap_bottom_off;

		DEBUG_MSG("synaptics_ts_probe: inactive_x %d %d, inactive_y %d %d\n",
		       inactive_area_left, inactive_area_right,
		       inactive_area_top, inactive_area_bottom);
		DEBUG_MSG("synaptics_ts_probe: snap_x %d-%d %d-%d, snap_y %d-%d %d-%d\n",
		       snap_left_on, snap_left_off, snap_right_on, snap_right_off,
		       snap_top_on, snap_top_off, snap_bottom_on, snap_bottom_off);
		
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
	}
	
	/* enable irq */
	if (ts->use_irq)
		enable_irq(ts->client->irq);
	
	return;
}
#endif

static void synaptics_ts_work_func(struct work_struct *work)
{
	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work);

	int int_mode;
	int width0, width1;
	int touch2_prestate = 0;
	int touch1_prestate = 0;

	int tmp_x=0, tmp_y=0;
	int finger0_status=0, finger1_status=0;

	DEBUG_MSG("synaptics_ts_work_func\n");
	
	int_mode = i2c_smbus_read_byte_data(ts->client, INT_STATUS_REG);

	if(int_mode & SYNAPTICS_INT_ABS0)
	{
		while (1) 
		{
			i2c_smbus_read_i2c_block_data(ts->client, START_ADDR, sizeof(ts_reg_data), &ts_reg_data);

			finger0_status = TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg);
			finger1_status = TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg);
			
			DEBUG_MSG("synaptics_ts_work_func : finger0_status = 0x%x, finger1_status = 0x%x\n",finger0_status,finger1_status);
			
			if((finger0_status == 0) && (ts_pre_state == 0)) 
			{
				DEBUG_MSG("synaptics_ts_work_func: Synaptics Touch is is the idle state\n");
				//longpress_pre = 0;
				//flicking = 0;
				//msleep(100); /* FIXME:  temporal delay due to interrupt not cleared by touch IC */
				goto SYNAPTICS_TS_IDLE;
			}

			if((finger0_status == 1) || (finger0_status == 2)) 
			{
				ts_pre_state = 1;
			} 
			else 
			{
				ts_pre_state = 0;
			}

			if((finger0_status == 1) || (finger0_status == 2))
			{
				touch1_prestate = 1;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				
				curr_ts_data.X_position[0] = tmp_x;
		  		curr_ts_data.Y_position[0] = tmp_y;

				if ((((ts_reg_data.XY_width_finger0_reg & 240) >> 4) - (ts_reg_data.XY_width_finger0_reg & 15)) > 0)
					width0 = (ts_reg_data.XY_width_finger0_reg & 240) >> 4;
				else
					width0 = ts_reg_data.XY_width_finger0_reg & 15;
				
	        	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width0);
	       		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[0]);
        		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[0]);

				DEBUG_MSG("push : first_x= %d, first_y = %d, width = %d\n", curr_ts_data.X_position[0], curr_ts_data.Y_position[0], width0);

				input_mt_sync(ts->input_dev);
			}
			else if((finger0_status == 0) && (touch1_prestate == 1))
			{
				touch1_prestate = 0;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				
				curr_ts_data.X_position[0] = tmp_x;
		  		curr_ts_data.Y_position[0] = tmp_y;

				if ((((ts_reg_data.XY_width_finger0_reg & 240) >> 4) - (ts_reg_data.XY_width_finger0_reg & 15)) > 0)
					width0 = (ts_reg_data.XY_width_finger0_reg & 240) >> 4;
				else
					width0 = ts_reg_data.XY_width_finger0_reg & 15;

	        	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width0);
	       		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[0]);
        		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[0]);
				input_mt_sync(ts->input_dev);

				DEBUG_MSG("release : first_x= %d, first_y = %d, width = %d\n", curr_ts_data.X_position[0], curr_ts_data.Y_position[0], width0);
			}
			else if(finger0_status == 0)
			{
				touch1_prestate = 0;
			}


			if((finger1_status == 1) || (finger1_status == 2)/* && (touch1_prestate == 1)*/)
			{
				ts_pre_state = 1;
				touch2_prestate = 1;
				
				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);
				
				if ((((ts_reg_data.XY_width_finger1_reg & 240) >> 4) - (ts_reg_data.XY_width_finger1_reg & 15)) > 0)
					width1 = (ts_reg_data.XY_width_finger1_reg & 240) >> 4;
				else
					width1 = ts_reg_data.XY_width_finger1_reg & 15;
				
				curr_ts_data.X_position[1] = tmp_x;
				curr_ts_data.Y_position[1] = tmp_y;
				
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width1);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[1]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[1]);
				input_mt_sync(ts->input_dev);
				
				DEBUG_MSG("push : second_x= %d, second_y = %d, width = %d\n", curr_ts_data.X_position[1], curr_ts_data.Y_position[1], width1);
			}
			else if((finger1_status == 0) /*&& (touch1_prestate == 1)*/ && (touch2_prestate == 1))
			{
				touch2_prestate = 0;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);

				if ((((ts_reg_data.XY_width_finger1_reg & 240) >> 4) - (ts_reg_data.XY_width_finger1_reg & 15)) > 0)
					width1 = (ts_reg_data.XY_width_finger1_reg & 240) >> 4;
				else
					width1 = ts_reg_data.XY_width_finger1_reg & 15;

				curr_ts_data.X_position[1] = tmp_x;
			  	curr_ts_data.Y_position[1] = tmp_y;

				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width1);
			    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[1]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[1]);
				input_mt_sync(ts->input_dev);

				DEBUG_MSG("release : second_x= %d, second_y = %d, width = %d\n", curr_ts_data.X_position[1], curr_ts_data.Y_position[1], width1);
			}
			else if(finger1_status == 0)
			{
				touch2_prestate = 0;
			}

			input_sync(ts->input_dev);

			if (ts_pre_state == 0) 
			{
				break;
			}
		
			msleep(SYNAPTICS_TS_POLLING_TIME);	
		}/* End of While(1) */
	}
	
SYNAPTICS_TS_IDLE:
	if (ts->use_irq) 
	{		
		enable_irq(ts->client->irq);
	}
}

static void synaptics_ts_new_work_func(struct work_struct *work)
{
	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work);

	int int_mode;
	int width0, width1;
	int touch2_prestate = 0;
	int touch1_prestate = 0;

	int tmp_x=0, tmp_y=0;
	int finger0_status=0, finger1_status=0;

	DEBUG_MSG("synaptics_ts_new_work_func\n");
	
	int_mode = i2c_smbus_read_byte_data(ts->client, INT_STATUS_REG);

	if(int_mode & SYNAPTICS_INT_ABS0)
	{
		while (1) 
		{
			i2c_smbus_read_i2c_block_data(ts->client, START_ADDR, sizeof(ts_reg_data), &ts_reg_data);

			finger0_status = TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg);
			finger1_status = TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg);
			
			DEBUG_MSG("synaptics_ts_new_work_func : finger0_status = 0x%x, finger1_status = 0x%x\n",finger0_status,finger1_status);
			
			if((finger0_status == 0) && (ts_pre_state == 0)) 
			{
				DEBUG_MSG("synaptics_ts_new_work_func: Synaptics Touch is is the idle state\n");
				//longpress_pre = 0;
				//flicking = 0;
				//msleep(100); /* FIXME:  temporal delay due to interrupt not cleared by touch IC */
				goto SYNAPTICS_TS_IDLE;
			}

			if((finger0_status == 1)) 
			{
				ts_pre_state = 1;
			} 
			else 
			{
				ts_pre_state = 0;
			}

			if(finger0_status == 1)
			{
				touch1_prestate = 1;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				
				curr_ts_data.X_position[0] = tmp_x;
		  		curr_ts_data.Y_position[0] = tmp_y;

				if ((((ts_reg_data.XY_width_finger0_reg & 240) >> 4) - (ts_reg_data.XY_width_finger0_reg & 15)) > 0)
					width0 = (ts_reg_data.XY_width_finger0_reg & 240) >> 4;
				else
					width0 = ts_reg_data.XY_width_finger0_reg & 15;

	        	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width0);
	       		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[0]);
        		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[0]);
				input_mt_sync(ts->input_dev);

				DEBUG_MSG("push : first_x= %d, first_y = %d, width = %d\n", curr_ts_data.X_position[0], curr_ts_data.Y_position[0], width0);
			}
			else if((finger0_status == 0) && (touch1_prestate == 1))
			{
				touch1_prestate = 0;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger0_reg, ts_reg_data.XY_low_position_finger0_reg);
				
				curr_ts_data.X_position[0] = tmp_x;
		  		curr_ts_data.Y_position[0] = tmp_y;

				if ((((ts_reg_data.XY_width_finger0_reg & 240) >> 4) - (ts_reg_data.XY_width_finger0_reg & 15)) > 0)
					width0 = (ts_reg_data.XY_width_finger0_reg & 240) >> 4;
				else
					width0 = ts_reg_data.XY_width_finger0_reg & 15;

	        	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width0);
	       		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[0]);
        		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[0]);
				input_mt_sync(ts->input_dev);

				DEBUG_MSG("release : first_x= %d, first_y = %d, width = %d\n", curr_ts_data.X_position[0], curr_ts_data.Y_position[0], width0);
			}
			else if(finger0_status == 0)
			{
				touch1_prestate = 0;
			}

			if((finger1_status == 1)/* && (touch1_prestate == 1)*/)
			{
				ts_pre_state = 1;
				touch2_prestate = 1;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);

				if ((((ts_reg_data.XY_width_finger1_reg & 240) >> 4) - (ts_reg_data.XY_width_finger1_reg & 15)) > 0)
					width1 = (ts_reg_data.XY_width_finger1_reg & 240) >> 4;
				else
					width1 = ts_reg_data.XY_width_finger1_reg & 15;

				curr_ts_data.X_position[1] = tmp_x;
			  	curr_ts_data.Y_position[1] = tmp_y;

				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width1);
			    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[1]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[1]);
				input_mt_sync(ts->input_dev);

				DEBUG_MSG("push : second_x= %d, second_y = %d, width = %d\n", curr_ts_data.X_position[1], curr_ts_data.Y_position[1], width1);
			}
			else if((finger1_status == 0) /*&& (touch1_prestate == 1)*/ && (touch2_prestate == 1))
			{
				touch2_prestate = 0;

				tmp_x = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.X_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);
				tmp_y = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.Y_high_position_finger1_reg, ts_reg_data.XY_low_position_finger1_reg);

				if ((((ts_reg_data.XY_width_finger1_reg & 240) >> 4) - (ts_reg_data.XY_width_finger1_reg & 15)) > 0)
					width1 = (ts_reg_data.XY_width_finger1_reg & 240) >> 4;
				else
					width1 = ts_reg_data.XY_width_finger1_reg & 15;

				curr_ts_data.X_position[1] = tmp_x;
			  	curr_ts_data.Y_position[1] = tmp_y;

				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width1);
			    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[1]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[1]);
				input_mt_sync(ts->input_dev);

				DEBUG_MSG("release : second_x= %d, second_y = %d, width = %d\n", curr_ts_data.X_position[1], curr_ts_data.Y_position[1], width1);
			}
			else if(finger1_status == 0)
			{
				touch2_prestate = 0;
			}

			input_sync(ts->input_dev);

			if (ts_pre_state == 0) 
			{
				break;
			}
		
			msleep(SYNAPTICS_TS_POLLING_TIME);	
		}/* End of While(1) */
	}
	
SYNAPTICS_TS_IDLE:
	if (ts->use_irq) 
	{		
		enable_irq(ts->client->irq);
	}
}

static enum hrtimer_restart synaptics_ts_timer_func(struct hrtimer *timer)
{
	struct synaptics_ts_data *ts = container_of(timer, struct synaptics_ts_data, timer);

	queue_work(synaptics_wq, &ts->work);
//	if (ts_pre_state == 1) {
		hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL); /* 12.5 msec */
//	} else {
//		hrtimer_start(&ts->timer, ktime_set(0, 200000000), HRTIMER_MODE_REL); /* 200 msec */
//	}

    return HRTIMER_NORESTART;
}

static irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id)
{
	struct synaptics_ts_data *ts = dev_id;

	//disable_irq(ts->client->irq);
	disable_irq_nosync(ts->client->irq);
	queue_work(synaptics_wq, &ts->work);
	
	return IRQ_HANDLED;
}

static void synaptics_ts_get_device_inform(int product_num)
{
	int i;
	unsigned char reg_block_num[CMD_REG_BLOCK_NUM]={0x00};
	
	switch(product_num)
	{
		case SYNAPTICS_2000:
			for(i=0;i<CMD_REG_BLOCK_NUM;i++)
				reg_block_num[i] = i+0x58;
			break;
		case SYNAPTICS_2100:
			for(i=0;i<CMD_REG_BLOCK_NUM;i++)
				reg_block_num[i]=i+0x5C;
			break;
		case SYNAPTICS_3000:
		default:
			for(i=0;i<CMD_REG_BLOCK_NUM;i++)
				reg_block_num[i]=0x00;
			ERR_MSG("synaptics_ts_get_device_inform : Not supported deivce!!\n");
			break;
	}
	memcpy(&ts_cmd_reg_data, &reg_block_num[0], CMD_REG_BLOCK_NUM);
	
	return;
}

/*************************************************************************************************
 * 1. Set interrupt configuration
 * 2. Disable interrupt
 * 3. Power up
 * 4. Read RMI Version
 * 5. Read Firmware version & Upgrade firmware automatically
 * 6. Read Data To Initialization Touch IC
 * 7. Set some register
 * 8. Enable interrupt
*************************************************************************************************/
static int synaptics_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_data *ts;
	int i;
	int ret = 0;
	int fuzz_x, fuzz_y, fuzz_p, fuzz_w;
	struct synaptics_i2c_rmi_platform_data *pdata;
	unsigned long irqflags;
	int inactive_area_left;
	int inactive_area_right;
	int inactive_area_top;
	int inactive_area_bottom;
	int snap_left_on;
	int snap_left_off;
	int snap_right_on;
	int snap_right_off;
	int snap_top_on;
	int snap_top_off;
	int snap_bottom_on;
	int snap_bottom_off;
	uint32_t panel_version;
	int product_id_quwery_reg;
	char product_name[PRODUCT_ID_STRING_NUM];

	DEBUG_MSG("%s() -- start\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ERR_MSG("synaptics_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	INIT_WORK(&ts->work, synaptics_ts_work_func);
#ifdef SYNAPTICS_FW_REFLASH
	INIT_WORK(&ts->work_for_reflash, synaptics_ts_fw_reflash_work_func);
#endif
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;
	if (pdata)
		ts->power = pdata->power;
	if (ts->power) {
		ret = ts->power(1);
		if (ret < 0) {
			ERR_MSG("synaptics_ts_probe power on failed\n");
			goto err_power_failed;
		}
	}

	for(i=0;i <PRODUCT_ID_STRING_NUM;i++)
		product_name[i]=NULL;
	
	ret = i2c_smbus_read_byte_data(ts->client, QUERY_BASE_REG);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_byte_data failed\n");
	}
	product_id_quwery_reg= ret + 11;

	ret = i2c_smbus_read_i2c_block_data(ts->client, product_id_quwery_reg, sizeof(product_name)-1, product_name);
	if (ret < 0)
	{
		ERR_MSG("synaptics_ts_probe : i2c_smbus_read_i2c_block_data failed: product_id_quwery_reg\n");
	}
	DEBUG_MSG("synaptics_ts_probe : product_name = %s\n",product_name);

	if(strcmp(product_name, "TM1590-001")==0)
	{
		kind_of_product = SYNAPTICS_2000;
	}
	else if(strcmp(product_name, "TM1610-001")==0)
	{
		kind_of_product = SYNAPTICS_2100;
	}
	else
	{
		kind_of_product = SYNAPTICS_3000;
	}
	DEBUG_MSG("synaptics_ts_probe : kind_of_product = 0x%x\n",kind_of_product);

	synaptics_ts_get_device_inform(kind_of_product);
	if(kind_of_product != SYNAPTICS_2000)
	{
		DEBUG_MSG("synaptics_ts_probe : work function changed : synaptics_ts_new_work_func\n");
		INIT_WORK(&ts->work, synaptics_ts_new_work_func);
	}
	DEBUG_MSG("synaptics_ts_probe : ts_cmd_reg_data.device_command = 0x%x\n",ts_cmd_reg_data.device_command);
	DEBUG_MSG("synaptics_ts_probe : ts_cmd_reg_data.absolute_query_2d = 0x%x\n",ts_cmd_reg_data.absolute_query_2d);

#if 1
	ret = i2c_smbus_read_byte_data(ts->client, ts_cmd_reg_data.customer_family_query);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_byte_data failed\n");
	}
	DEBUG_MSG("synaptics_ts_probe: Customer family 0x%x\n", ret);

	ret = i2c_smbus_read_byte_data(ts->client, ts_cmd_reg_data.firmware_revision_query);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_byte_data failed\n");
	}
	DEBUG_MSG("synaptics_ts_probe: Firmware Revision 0x%x\n", ret);

	ts->fw_revision = ret;
#else
	ret = i2c_smbus_read_byte_data(ts->client, 0x65);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_byte_data failed\n");
	}
	DEBUG_MSG("synaptics_ts_probe: Customer family 0x%x\n", ret);

	ret = i2c_smbus_read_byte_data(ts->client, 0x66);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_byte_data failed\n");
	}
	DEBUG_MSG("synaptics_ts_probe: Firmware Revision 0x%x\n", ret);

	ts->fw_revision = ret;
#endif

	if (pdata) {
		while (pdata->version > panel_version)
			pdata++;
		ts->flags = pdata->flags;
		ts->sensitivity_adjust = pdata->sensitivity_adjust;
		irqflags = pdata->irqflags;
		inactive_area_left = pdata->inactive_left;
		inactive_area_right = pdata->inactive_right;
		inactive_area_top = pdata->inactive_top;
		inactive_area_bottom = pdata->inactive_bottom;
		snap_left_on = pdata->snap_left_on;
		snap_left_off = pdata->snap_left_off;
		snap_right_on = pdata->snap_right_on;
		snap_right_off = pdata->snap_right_off;
		snap_top_on = pdata->snap_top_on;
		snap_top_off = pdata->snap_top_off;
		snap_bottom_on = pdata->snap_bottom_on;
		snap_bottom_off = pdata->snap_bottom_off;
		fuzz_x = pdata->fuzz_x;
		fuzz_y = pdata->fuzz_y;
		fuzz_p = pdata->fuzz_p;
		fuzz_w = pdata->fuzz_w;
	} else {
		irqflags = 0;
		inactive_area_left = 0;
		inactive_area_right = 0;
		inactive_area_top = 0;
		inactive_area_bottom = 0;
		snap_left_on = 0;
		snap_left_off = 0;
		snap_right_on = 0;
		snap_right_off = 0;
		snap_top_on = 0;
		snap_top_off = 0;
		snap_bottom_on = 0;
		snap_bottom_off = 0;
		fuzz_x = 0;
		fuzz_y = 0;
		fuzz_p = 0;
		fuzz_w = 0;
	}

  	memset(&ts_reg_data, 0x0, sizeof(ts_sensor_data));
  	memset(&curr_ts_data, 0x0, sizeof(ts_finger_data));

#if 1
	ret = i2c_smbus_read_word_data(ts->client, MAX_X_POS_LOW_REG);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_x = (ret & 0xFF);

	ret = i2c_smbus_read_word_data(ts->client, MAX_X_POS_HIGH_REG);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_x |= (((ret & 0xFF) << 8) & 0xff00);
	ts->max[0] = max_x;

	ret = i2c_smbus_read_word_data(ts->client, MAX_Y_POS_LOW_REG);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_y = (ret & 0xFF);
			
	ret = i2c_smbus_read_word_data(ts->client, MAX_Y_POS_HIGH_REG);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_y |= (((ret & 0xFF) << 8) & 0xff00);
	ts->max[1] = max_y;

	DEBUG_MSG("synaptics_ts_probe : max_x = 0x%x\n",max_x);
	DEBUG_MSG("synaptics_ts_probe : max_y = 0x%x\n",max_y);
#else
	ret = i2c_smbus_read_word_data(ts->client, 0x28);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_x = (ret & 0xFF);

	ret = i2c_smbus_read_word_data(ts->client, 0x29);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_x |= (((ret & 0xFF) << 8) & 0xff00);
	ts->max[0] = max_x;

	ret = i2c_smbus_read_word_data(ts->client, 0x2A);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_y = (ret & 0xFF);
			
	ret = i2c_smbus_read_word_data(ts->client, 0x2B);
	if (ret < 0) {
		ERR_MSG("i2c_smbus_read_word_data failed\n");
	}
	max_y |= (((ret & 0xFF) << 8) & 0xff00);
	ts->max[1] = max_y;

	DEBUG_MSG("synaptics_ts_probe : max_x = 0x%x\n",max_x);
	DEBUG_MSG("synaptics_ts_probe : max_y = 0x%x\n",max_y);
#endif

	ret = i2c_smbus_read_i2c_block_data(ts->client, START_ADDR, sizeof(ts_reg_data), &ts_reg_data);
	if (ret < 0) 
	{
		ERR_MSG("synaptics_ts_probe : i2c_smbus_read_i2c_block_data failed: START_ADDR\n");
	}
	DEBUG_MSG("synaptics_ts_probe : status_reg(%d), interrupt_status_reg(%d,)\n", ts_reg_data.device_status_reg, ts_reg_data.interrupt_status_reg);

	ret = i2c_smbus_read_byte_data(ts->client, SYNAPTICS_CONTROL_REG);
	if (ret < 0) {
		ERR_MSG("synaptics_ts_probe : i2c_smbus_read_byte_data failed: SYNAPTICS_CONTROL_REG\n");
	}
	DEBUG_MSG("synaptics_ts_probe: device control 0x%x\n", ret);

	ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_CONTROL_REG, SYNAPTICS_CONTROL_NOSLEEP);
	if (ret < 0) {
		ERR_MSG("synaptics_ts_probe: i2c_smbus_write_byte_data failed / control SYNAPTICS_CONTROL_REG\n");
	}
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		ERR_MSG("synaptics_ts_probe: Failed to allocate input device\n");
		
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "synaptics-rmi-ts";
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
    set_bit(KEY_BACK, ts->input_dev->keybit);
    //set_bit(EV_TG, ts->input_dev->evbit);
    	
	inactive_area_left = inactive_area_left * max_x / 0x10000;
	inactive_area_right = inactive_area_right * max_x / 0x10000;
	inactive_area_top = inactive_area_top * max_y / 0x10000;
	inactive_area_bottom = inactive_area_bottom * max_y / 0x10000;
	snap_left_on = snap_left_on * max_x / 0x10000;
	snap_left_off = snap_left_off * max_x / 0x10000;
	snap_right_on = snap_right_on * max_x / 0x10000;
	snap_right_off = snap_right_off * max_x / 0x10000;
	snap_top_on = snap_top_on * max_y / 0x10000;
	snap_top_off = snap_top_off * max_y / 0x10000;
	snap_bottom_on = snap_bottom_on * max_y / 0x10000;
	snap_bottom_off = snap_bottom_off * max_y / 0x10000;
	fuzz_x = fuzz_x * max_x / 0x10000;
	fuzz_y = fuzz_y * max_y / 0x10000;
	ts->snap_down[!!(ts->flags & SYNAPTICS_SWAP_XY)] = -inactive_area_left;
	ts->snap_up[!!(ts->flags & SYNAPTICS_SWAP_XY)] = max_x + inactive_area_right;
	ts->snap_down[!(ts->flags & SYNAPTICS_SWAP_XY)] = -inactive_area_top;
	ts->snap_up[!(ts->flags & SYNAPTICS_SWAP_XY)] = max_y + inactive_area_bottom;
	ts->snap_down_on[!!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_left_on;
	ts->snap_down_off[!!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_left_off;
	ts->snap_up_on[!!(ts->flags & SYNAPTICS_SWAP_XY)] = max_x - snap_right_on;
	ts->snap_up_off[!!(ts->flags & SYNAPTICS_SWAP_XY)] = max_x - snap_right_off;
	ts->snap_down_on[!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_top_on;
	ts->snap_down_off[!(ts->flags & SYNAPTICS_SWAP_XY)] = snap_top_off;
	ts->snap_up_on[!(ts->flags & SYNAPTICS_SWAP_XY)] = max_y - snap_bottom_on;
	ts->snap_up_off[!(ts->flags & SYNAPTICS_SWAP_XY)] = max_y - snap_bottom_off;

	DEBUG_MSG("synaptics_ts_probe: inactive_x %d %d, inactive_y %d %d\n",
	       inactive_area_left, inactive_area_right,
	       inactive_area_top, inactive_area_bottom);
	DEBUG_MSG("synaptics_ts_probe: snap_x %d-%d %d-%d, snap_y %d-%d %d-%d\n",
	       snap_left_on, snap_left_off, snap_right_on, snap_right_off,
	       snap_top_on, snap_top_off, snap_bottom_on, snap_bottom_off);
	
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, fuzz_x, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, fuzz_y, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 15, fuzz_p, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, fuzz_w, 0);

	/* ts->input_dev->name = ts->keypad_info->name; */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		ERR_MSG("synaptics_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	DEBUG_MSG("########## irq [%d], irqflags[0x%x]\n", client->irq, irqflags);
	
	if (client->irq) 
	{
		ret = request_irq(client->irq, synaptics_ts_irq_handler, irqflags, client->name, ts);

		if (ret == 0) 
		{
			ts->use_irq = 1;
			DEBUG_MSG("request_irq\n");
		}
		else
		{
			dev_err(&client->dev, "request_irq failed\n");
		}
	}
	if (!ts->use_irq) 
	{
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = synaptics_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = synaptics_ts_early_suspend;
	ts->early_suspend.resume = synaptics_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef SYNAPTICS_FW_REFLASH
	if(synaptics_fwdl_wq)
		queue_work(synaptics_fwdl_wq, &ts->work_for_reflash);
#endif

	DEBUG_MSG("synaptics_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");
	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_power_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int synaptics_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	DEBUG_MSG("synaptics_ts_suspend : enter!!\n");
		
	if (ts->use_irq)
		disable_irq(client->irq);
	else
		hrtimer_cancel(&ts->timer);
	ret = cancel_work_sync(&ts->work);

	if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);
	
	ret = i2c_smbus_write_byte_data(ts->client, SYNAPTICS_CONTROL_REG, SYNAPTICS_CONTROL_SLEEP); /* sleep */
	if (ret < 0)
		ERR_MSG("synaptics_ts_suspend: i2c_smbus_write_byte_data failed\n");

	if (ts->power) {
		ret = ts->power(0);
		if (ret < 0)
			ERR_MSG("synaptics_ts_resume power off failed\n");
	}
	return 0;
}

static int synaptics_ts_resume(struct i2c_client *client)
{
	int ret;
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	DEBUG_MSG("synaptics_ts_resume : enter!!\n");

	if (ts->power) {
		ret = ts->power(1);
		if (ret < 0)
			ERR_MSG("synaptics_ts_resume power on failed\n");
	}
	
    i2c_smbus_write_byte_data(ts->client, SYNAPTICS_CONTROL_REG, SYNAPTICS_CONTROL_NOSLEEP); /* wake up */

	if (ts->use_irq)
		enable_irq(client->irq);

	if (!ts->use_irq)
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);	

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;
	ts = container_of(h, struct synaptics_ts_data, early_suspend);
	synaptics_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void synaptics_ts_late_resume(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;
	ts = container_of(h, struct synaptics_ts_data, early_suspend);
	synaptics_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id synaptics_ts_id[] = {
	{ "synaptics-rmi-ts", 0 },
	{ }
};

static struct i2c_driver synaptics_ts_driver = {
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= synaptics_ts_suspend,
	.resume		= synaptics_ts_resume,
#endif
	.id_table	= synaptics_ts_id,
	.driver = {
		.name	= "synaptics-rmi-ts",
        .owner = THIS_MODULE,
	},
};

static int __devinit synaptics_ts_init(void)
{
	synaptics_wq = create_singlethread_workqueue("synaptics_wq");
#ifdef SYNAPTICS_FW_REFLASH
	synaptics_fwdl_wq = create_singlethread_workqueue("synaptics_fwdl_wq");
#endif

	DEBUG_MSG ("Synaptics ts_init\n");

#ifdef SYNAPTICS_FW_REFLASH
	if ((!synaptics_wq)||(!synaptics_fwdl_wq))
		return -ENOMEM;
#else
	if (!synaptics_wq)
		return -ENOMEM;
#endif

	return i2c_add_driver(&synaptics_ts_driver);
}

static void __exit synaptics_ts_exit(void)
{
	i2c_del_driver(&synaptics_ts_driver);
    
	if (synaptics_wq)
		destroy_workqueue(synaptics_wq);

#ifdef SYNAPTICS_FW_REFLASH
	if (synaptics_fwdl_wq)
		destroy_workqueue(synaptics_fwdl_wq);
#endif
}

module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

MODULE_DESCRIPTION("Synaptics Touchscreen Driver");
MODULE_LICENSE("GPL");


