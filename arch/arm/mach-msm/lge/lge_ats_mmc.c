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

#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

int external_memory_test(void)
{
	int return_value = 0;
	char *src = NULL, *dest = NULL;
	off_t fd_offset;
	int fd = -1;
	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-19, kernel data segment
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();

	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-19, kernel data segment
	set_fs(KERNEL_DS);
	fd = sys_open((const char __user *) "/sdcard/SDTest.txt", O_CREAT | O_RDWR, 0);
	set_fs(old_fs);
	if ( fd < 0 ) {
		printk(KERN_ERR "[ATCMD_EMT] Can not access SD card\n");
		return return_value;
	}

	src = kmalloc(10, GFP_KERNEL);
	if (NULL == src) {
		printk(KERN_ERR "[ATCMD_EMT] memory allocation is failed\n");
		goto file_fail;
	}
	sprintf(src,"TEST");

	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-19, kernel data segment
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_write(fd, (const char __user *) src, 5);
	set_fs(old_fs);
	if(ret < 0) {
		printk(KERN_ERR "[ATCMD_EMT] Can not write SD card \n");
		goto file_fail;
	}
	fd_offset = sys_lseek(fd, 0, 0);

	dest = kmalloc(10, GFP_KERNEL);
	if (NULL == dest) {
		printk(KERN_ERR "[ATCMD_EMT] memory allocation is failed\n");
		goto file_fail;
	}

	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-19, kernel data segment
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_read(fd, (char __user *) dest, 5);
	set_fs(old_fs);

	if(ret < 0) {
		printk(KERN_ERR "[ATCMD_EMT]Can not read SD card \n");
		goto file_fail;
	}
	if ((memcmp(src, dest, 4)) == 0)
		return_value = 1;
	else
		return_value = 0;

file_fail:
	if (fd > 0) {
		sys_close(fd);
		sys_unlink((const char __user *)"/sdcard/SDTest.txt");
	}
	if (src) 
		kfree(src);
	if (dest)
		kfree(dest);

	return return_value;
}
