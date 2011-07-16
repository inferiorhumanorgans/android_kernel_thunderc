/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

/*
per.c

DESCRIPTION: Performance count interface for linux via proc in the T32
command file style
*/

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sysrq.h>
#include <linux/time.h>
#include "linux/proc_fs.h"
#include "linux/kernel_stat.h"
#include "asm/uaccess.h"
#include "cp15_registers.h"
#include "perf.h"

/*
FUNCTION perf_if_proc_init

DESCRIPTION  Initialize the proc interface for thje performance data.
*/
static __init int per_init(void)
{

  if (atomic_read(&pm_op_lock) == 1) {
	printk(KERN_INFO "Can not load KSAPI, monitors are in use\n");
	return -1;
  }
  atomic_set(&pm_op_lock, 1);
  per_process_perf_init();
  printk(KERN_INFO "ksapi init\n");
  return 0;
}

static void __exit per_exit(void)
{
  per_process_perf_exit();
  printk(KERN_INFO "ksapi exit\n");
  atomic_set(&pm_op_lock, 0);
}

MODULE_LICENSE("GPL v2");
module_init(per_init);
module_exit(per_exit);
