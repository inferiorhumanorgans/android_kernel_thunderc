/* include/linux/ami304.h - AMI304 compass driver
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

/*
 * Definitions for ami304 compass chip.
 */
#ifndef AMI304_H
#define AMI304_H

#include <linux/ioctl.h>

//new Addr=0x0E(Low), old Addr=0x0F(High)
#define AMI304_I2C_ADDRESS 			0x0E  

//#define AMI304_MAG_OFFSET_ADJ

/* AMI304 Internal Register Address  (Please refer to AMI304 Specifications) */
#define AMI304_REG_CTRL1			0x1B
#define AMI304_REG_CTRL2			0x1C
#define AMI304_REG_CTRL3			0x1D
#define AMI304_REG_CTRL4			0x5C
#define AMI304_REG_DATAXH			0x10
#define AMI304_REG_DATAXL			0x11
#define AMI304_REG_DATAYH			0x12
#define AMI304_REG_DATAYL			0x13
#define AMI304_REG_DATAZH			0x14
#define AMI304_REG_DATAZL			0x15
#define AMI304_REG_WIA				0x0F

/* AMI304 Control Bit  (Please refer to AMI304 Specifications) */
#define AMI304_CTRL1_PC1			0x80
#define AMI304_CTRL1_FS1_NORMAL			0x00 //Normal
#define AMI304_CTRL1_FS1_FORCE			0x02 //Force
#define AMI304_CTRL1_ODR1			0x10 //20SPS(20HZ)
#define AMI304_CTRL2_DREN			0x08
#define AMI304_CTRL2_DRP			0x04
#define AMI304_CTRL3_NOFORCE_BIT		0x00
#define AMI304_CTRL3_FORCE_BIT			0x40
#define AMI304_CTRL3_B0_LO_CLR			0x00

/* IOCTLs for ami304 misc. device library */
#define AMI304IO						   0x83
#define AMI304_IOCTL_INIT                  _IO(AMI304IO, 0x01)
#define AMI304_IOCTL_READ_CHIPINFO         _IOR(AMI304IO, 0x02, int)
#define AMI304_IOCTL_READ_SENSORDATA       _IOR(AMI304IO, 0x03, int)
#define AMI304_IOCTL_READ_POSTUREDATA      _IOR(AMI304IO, 0x04, int)
#define AMI304_IOCTL_READ_CALIDATA         _IOR(AMI304IO, 0x05, int)
#define AMI304_IOCTL_READ_CONTROL          _IOR(AMI304IO, 0x06, int)
#define AMI304_IOCTL_SET_CONTROL           _IOW(AMI304IO, 0x07, int)
#define AMI304_IOCTL_SET_MODE              _IOW(AMI304IO, 0x08, int)

/* IOCTLs for AMI304 middleware misc. device library */
#define AMI304MIDIO						   0x84
#define AMI304MID_IOCTL_GET_SENSORDATA     _IOR(AMI304MIDIO, 0x01, int)
#define AMI304MID_IOCTL_SET_POSTURE        _IOW(AMI304MIDIO, 0x02, int)
#define AMI304MID_IOCTL_SET_CALIDATA       _IOW(AMI304MIDIO, 0x03, int)
#define AMI304MID_IOCTL_SET_CONTROL        _IOW(AMI304MIDIO, 0x04, int)
#define AMI304MID_IOCTL_GET_CONTROL        _IOR(AMI304MIDIO, 0x05, int)
#define AMI304MID_IOCTL_SET_MODE           _IOW(AMI304MIDIO, 0x06, int)
#define AMI304MID_IOCTL_SET_REPORT         _IOW(AMI304MIDIO, 0x07, int)
#define AMI304MID_IOCTL_MAG_OFFSET_ADJ     _IO(AMI304MIDIO, 0x08)
#define AMI304MID_IOCTL_GET_MAG_OFFSET     _IOW(AMI304MIDIO, 0x09, int)
#define AMI304MID_IOCTL_SET_MAG_OFFSET     _IOW(AMI304MIDIO, 0x0a, int)

/* IOCTLs for AMI304 HAL misc. device library */
#define AMI304HALIO						   0x85
#define AMI304HAL_IOCTL_GET_SENSORDATA     _IOR(AMI304HALIO, 0x01, int)
#define AMI304HAL_IOCTL_GET_POSTURE        _IOR(AMI304HALIO, 0x02, int)
#define AMI304HAL_IOCTL_GET_CALIDATA       _IOR(AMI304HALIO, 0x03, int)
#define AMI304HAL_IOCTL_SET_ACTIVE         _IOW(AMI304HALIO, 0x04, int)
#define AMI304HAL_IOCTL_SET_CONTROL        _IOW(AMI304HALIO, 0x05, int)
#define AMI304HAL_IOCTL_GET_CONTROL        _IOR(AMI304HALIO, 0x06, int)

#define AMI304_BUFSIZE				256
#define AMI304_NORMAL_MODE			0
#define AMI304_FORCE_MODE			1
#define AMI304_IRQ				IRQ_EINT9

#endif
