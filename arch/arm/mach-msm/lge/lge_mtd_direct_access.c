/*
 * arch/arm/mach-msm/lge/lge_mtd_direct_access.c
 *
 * Copyright (C) 2010 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/div64.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/sched.h>
// LGE_FOTA
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <mach/lg_comdef.h>
#include <linux/syscalls.h>

#if defined(CONFIG_MACH_MSM7X27_THUNDERC)
#define MISC_PART_NUM 8
#define MISC_PART_SIZE 4
#define PERSIST_PART_NUM 9
#define PERSIST_PART_SIZE 12
#define PAGE_NUM_PER_BLK 64
#define PAGE_SIZE_BYTE 2048
//LGE_FOTA
#define FOTA_DATA_PART_NUM	7
#define FOTA_DATA_PART_SIZE	1479 //this size is variable..
#define FOTA_CACHE_PART_NUM	1
#define FOTA_CACHE_PART_SIZE	640
#define FOTA_USD_PART_NUM	5
#define FOTA_USD_PART_SIZE	4
#else
#define MISC_PART_NUM	4
#endif

static struct mtd_info *mtd;

// kthread_lg_diag: page allocation failure. order:5, mode:0xd0, lge_init_mtd_access: error: cannot allocate memory
//static unsigned char *global_buf;
static unsigned char global_buf[PAGE_NUM_PER_BLK*PAGE_SIZE_BYTE];
static unsigned char global_buf_fota[PAGE_NUM_PER_BLK*PAGE_SIZE_BYTE];
static unsigned char *bbt;

static int pgsize;
static int bufsize;
static int ebcnt;
static int pgcnt;

int lge_erase_block(int ebnum);
int lge_write_block(int ebnum, unsigned char *buf, size_t size);
int lge_read_block(int ebnum, unsigned char *buf);

static int scan_for_bad_eraseblocks(void);
static int is_block_bad(int ebnum);

int init_mtd_access(int partition, int block);

static int dev;
static int target_block;
static int dummy_arg;
int boot_info = 0;

module_param(dev, int, S_IRUGO);
module_param(target_block, int, S_IRUGO);
module_param(boot_info, int, S_IWUSR | S_IRUGO);

static int test_init(void)
{
	int partition = PERSIST_PART_NUM;
	int block = 0;

	return init_mtd_access(partition, block);
}

//LGE_FOTA
static int test_init_fota_usd(void)
{
	int partition = FOTA_USD_PART_NUM;
	int block = 0;

	return init_mtd_access(partition, block);
}

static int test_init_fota_cache(void)
{
	int partition = FOTA_CACHE_PART_NUM;
	int block = 0;

	return init_mtd_access(partition, block);
}

static int test_init_fota_data(void)
{
	int partition = FOTA_DATA_PART_NUM;
	int block = 0;

	return init_mtd_access(partition, block);
}

static int test_erase_block(void)
{
	int i;
	int err;

	/* Erase eraseblock */
	printk(KERN_INFO"%s: erasing block\n", __func__);

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_erase_block(i);
	if (err) {
		printk(KERN_INFO"%s: erased %u block fail\n", __func__, i);
		return err;
	}

	printk(KERN_INFO"%s: erased %u block\n", __func__, i);

	return 0;
}
static int test_write_block(const char *val, struct kernel_param *kp)
{
	int i;
	int err;
	unsigned char *test_string;
	unsigned long flag=0;

	flag = simple_strtoul(val,NULL,10);
	if(flag==5)
		test_string="FACT_RESET_5";
	else if(flag==6)
		test_string="FACT_RESET_6";
	else
		return -1;

	test_init();
	test_erase_block();
	printk(KERN_INFO"%s: writing block: flag = %d\n", __func__,flag);

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_write_block(i, test_string, strlen(test_string));
	if (err) {
		printk(KERN_INFO"%s: write %u block fail\n", __func__, i);
		return err;
	}

	printk(KERN_INFO"%s: write %u block\n", __func__, i);

	return 0;
}
module_param_call(write_block, test_write_block, param_get_bool, &dummy_arg,S_IWUSR | S_IRUGO);

//LGE_FOTA
char fota_status_str[100]="";
char fota_status_result[10]="";
static char *write_fota_status(int fota_status)
{
	int fd_fota, fd_fota_tmp;
	int normal_block_seq;
	int err=0;
	mm_segment_t old_fs=get_fs();
	struct file *phMscd_Filp = NULL;
	int fd_fota_silent_reset = 0;

	printk(KERN_INFO"%s \n", __func__);
	set_fs(get_ds());

	switch(fota_status)
	{
		case 0x00:
		case 0xFFFF:
			// No action
			set_fs(old_fs);
			printk(KERN_INFO"%s: update success.. \n", __func__,err);
			return 0;
			
		//invalid package
		case 0x4804:
		case 0x480E:
		case 0x4101:
			strcpy(fota_status_result,"403");
			break;

		//fail package
		case 0x6000:
		case 0x6001:
		case 0x6002:
		case 0x6003:
		case 0x6004:
		case 0x4105:
			strcpy(fota_status_result,"402");
			break;

		//Insufficient RAM
		case 0x5000:
		case 0x5001:
		case 0x5002:
		case 0x5003:
		case 0x5004:
		case 0x4102:
		case 0x480B:
		case 0x4004:
		case 0x4007:
			strcpy(fota_status_result,"501");
			break;

		default :
			strcpy(fota_status_result,"410");
			break;
	}

	fota_status_result[0] = fota_status/0x1000 + 48;
	fota_status_result[1] = fota_status/0x0100 - fota_status_result[0]*0x10 + 48;
	fota_status_result[2] = fota_status/0x0010 - fota_status_result[0]*0x100 - fota_status_result[1]*0x10 +48;
	fota_status_result[3] = fota_status - fota_status_result[0]*0x1000 - fota_status_result[1]*0x100 - fota_status_result[2]*0x10 + 48;

	printk(KERN_INFO"%s: make fota_status_value string = %s\n", __func__,fota_status_result);

	sprintf(fota_status_str, "IP_PREVIOUS_UPGRADE_FAILED %s", fota_status_result);

	printk(KERN_INFO"%s: make fota_status_result string = %s\n", __func__,fota_status_str);

	fd_fota = sys_open("/cache/fota/ipth_config_dfs.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd_fota < 0) {
		printk(KERN_INFO"%s: fail to open 'ipth_config_dfs.txt''\n", __func__);
		return 1;
	}
	sys_write(fd_fota, fota_status_str, 30);
	sys_close(fd_fota);
#if 0
	fd_fota_tmp = sys_open("/cache/fota/ipth_config_dfs.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd_fota_tmp < 0) {
		return 1;
	}
	sys_write(fd_fota_tmp, fota_status_result, 4);
	sys_close(fd_fota_tmp);
#endif
	set_fs(old_fs);

	return 0;

}
static int test_write_block_fota(const char *val, struct kernel_param *kp)
{
	int err;
	dword ipth_config_data;

	ipth_config_data = (global_buf[8]) + (global_buf[9])*0x0100;
	printk(KERN_INFO"%s: read fota error code %u for fota\n", __func__, ipth_config_data);

	err = write_fota_status(ipth_config_data);

	memset(global_buf, 0, 10);

	if (err) {
		printk(KERN_INFO"%s: write block fail for fota\n", __func__);
		return err;
	}
	
	printk(KERN_INFO"%s: success read & write config file for fota..\n", __func__);

	return 0;
}
module_param_call(write_block_fota, test_write_block_fota, param_get_bool, &dummy_arg,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);


#define FACTORY_RESET_STR_SIZE 11
#define FACTORY_RESET_STR "FACT_RESET_"
static int test_read_block(char *buf, struct kernel_param *kp)
{
	int i;
	int err;
	unsigned char status=0;
	char* temp = "1";

	printk(KERN_INFO"%s: read block\n", __func__);
	test_init();

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_read_block(i, global_buf);
	if (err) {
		printk(KERN_INFO"%s: read %u block fail\n", __func__, i);
		goto error;
	}

	printk(KERN_INFO"%s: read %u block\n", __func__, i);
	printk(KERN_INFO"%s: %s\n", __func__, global_buf);
	
	if(memcmp(global_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE)==0){
		status = global_buf[FACTORY_RESET_STR_SIZE];
		err = sprintf(buf,"%s",global_buf+FACTORY_RESET_STR_SIZE);
		return err;
	}
	else{
error:
		err = sprintf(buf,"%s",temp);
		return err;
	}
	
}
module_param_call(read_block, param_get_bool, test_read_block, &dummy_arg, S_IWUSR | S_IRUGO);

//LGE_FOTA
static int test_read_block_fota(char *buf, struct kernel_param *kp)
{
	int i;
	int err;
	int normal_block_seq=0;
	dword ipth_config_data;
#if 0
	printk(KERN_INFO"%s: read block for fota\n", __func__);
	test_init_fota_usd();
	
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		else
			break;
	}
	
	err = lge_read_block(normal_block_seq, global_buf_fota);
	if (err) {
		printk(KERN_INFO"%s: read block fail for fota\n", __func__);
		goto error;
	}


	ipth_config_data = (global_buf_fota[8]) + (global_buf_fota[9])*0x0100;
	printk(KERN_INFO"%s: read fota ipth_config_data %u for fota\n", __func__, ipth_config_data);
	printk(KERN_INFO"%s: ------------------------------------------------ \n", __func__);
	printk(KERN_INFO"%s: display error code \n", __func__);
	printk(KERN_INFO"%s: global_buf_fota[0] : %u \n", __func__,global_buf_fota[0]+48 );
	printk(KERN_INFO"%s: global_buf_fota[1] : %u  \n", __func__,global_buf_fota[1]+48 );
	printk(KERN_INFO"%s: global_buf_fota[2] : %u  \n", __func__,global_buf_fota[2]+48 );
	printk(KERN_INFO"%s: global_buf_fota[3] : %u  \n", __func__,global_buf_fota[3]+48 );
	printk(KERN_INFO"%s: global_buf_fota[4] : %u  \n", __func__,global_buf_fota[4]+48 );
	printk(KERN_INFO"%s: global_buf_fota[5] : %u  \n", __func__,global_buf_fota[5]+48 );
	printk(KERN_INFO"%s: global_buf_fota[6] : %u  \n", __func__,global_buf_fota[6]+48 );
	printk(KERN_INFO"%s: global_buf_fota[7] : %u  \n", __func__,global_buf_fota[7]+48 );
	printk(KERN_INFO"%s: global_buf_fota[8] : %u  \n", __func__,global_buf_fota[8]+48 );
	printk(KERN_INFO"%s: global_buf_fota[9] : %u  \n", __func__,global_buf_fota[9]+48 );
	printk(KERN_INFO"%s: global_buf_fota[10] : %u  \n", __func__,global_buf_fota[10]+48 );
	printk(KERN_INFO"%s: global_buf_fota[11] : %u  \n", __func__,global_buf_fota[11]+48 );
	printk(KERN_INFO"%s: global_buf_fota[12] : %u  \n", __func__,global_buf_fota[12]+48 );
	printk(KERN_INFO"%s: global_buf_fota[13] : %u  \n", __func__,global_buf_fota[13]+48 );
	printk(KERN_INFO"%s: ------------------------------------------------ \n", __func__);

		err = write_fota_status(ipth_config_data);


	// magic number for default operation beaf
		global_buf_fota[8] = 0xAF;
		global_buf_fota[9] = 0xBE;

		test_erase_block();

		for (i = 0; i < ebcnt; ++i) {
			if (bbt[i])
				continue;
			else
				break;
		}

		printk(KERN_INFO"%s: write %d block..\n", __func__, i);
		err = lge_write_block(i, global_buf_fota, PAGE_NUM_PER_BLK*PAGE_SIZE_BYTE);
		if (err) 
		{
			printk(KERN_INFO"%s: write %u block fail\n", __func__, i);
			return err;
		}
error:
		err = sprintf(buf,"%s",__func__);
		return err;
#endif

	write_fota_status(0x4104);


}
module_param_call(read_block_fota, param_get_bool, test_read_block_fota, &dummy_arg, S_IWUSR | S_IRUGO);

int lge_erase_block(int ebnum)
{
	int err;
	struct erase_info ei;
	loff_t addr = ebnum * mtd->erasesize;

	memset(&ei, 0, sizeof(struct erase_info));
	ei.mtd  = mtd;
	ei.addr = addr;
	ei.len  = mtd->erasesize;

	err = mtd->erase(mtd, &ei);
	if (err) {
		printk(KERN_INFO"%s: error %d while erasing EB %d\n", __func__,  err, ebnum);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		printk(KERN_INFO"%s: some erase error occurred at EB %d\n", __func__,
		       ebnum);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(lge_erase_block);

int lge_write_block(int ebnum, unsigned char *buf, size_t size)
{
	int err = 0;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;

	memset(global_buf, 0, mtd->erasesize);
	memcpy(global_buf, buf, size);
	err = mtd->write(mtd, addr, pgsize, &written, global_buf);
	if (err || written != pgsize)
		printk(KERN_INFO"%s: error: write failed at %#llx\n",
		      __func__, (long long)addr);

	return err;
}
EXPORT_SYMBOL(lge_write_block);

int lge_read_block(int ebnum, unsigned char *buf)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;

	//memset(buf, 0, mtd->erasesize); 
	memset(buf, 0, pgsize);
	err = mtd->read(mtd, addr, pgsize, &read, buf);
	if (err || read != pgsize)
		printk(KERN_INFO"%s: error: read failed at %#llx\n",
		      __func__, (long long)addr);

	return err;
}
EXPORT_SYMBOL(lge_read_block);

static int is_block_bad(int ebnum)
{
	loff_t addr = ebnum * mtd->erasesize;
	int ret;

	ret = mtd->block_isbad(mtd, addr);
	if (ret)
		printk(KERN_INFO"%s: block %d is bad\n", __func__, ebnum);
	return ret;
}

static int scan_for_bad_eraseblocks(void)
{
	int i, bad = 0;

	bbt = kmalloc(ebcnt, GFP_KERNEL);
	if (!bbt) {
		printk(KERN_INFO"%s: error: cannot allocate memory\n", __func__);
		return -ENOMEM;
	}
	memset(bbt, 0 , ebcnt);

	printk(KERN_INFO"%s: scanning for bad eraseblocks\n", __func__);
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
	printk(KERN_INFO"%s: scanned %d eraseblocks, %d are bad\n", __func__, i, bad);
	return 0;
}

int init_mtd_access(int partition, int block)
{
	int err = 0;
	uint64_t tmp;

	dev = partition;
	target_block = block;
	
	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		printk(KERN_INFO"%s: cannot get MTD device\n", __func__);
		return err;
	}

	if (mtd->type != MTD_NANDFLASH) {
		printk(KERN_INFO"%s: this test requires NAND flash\n", __func__);
		goto out;
	}

	if (mtd->writesize == 1) {
		printk(KERN_INFO"%s: not NAND flash, assume page size is 512 "
		       "bytes.\n", __func__);
		pgsize = 512;
	} else
		pgsize = mtd->writesize;

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;

	printk(KERN_INFO"%s: MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n", __func__,
	       (unsigned long long)mtd->size, mtd->erasesize,
	       pgsize, ebcnt, pgcnt, mtd->oobsize);

	err = -ENOMEM;
	bufsize = pgsize * 2;
	
	#if 0 
	global_buf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!global_buf) {
		printk(KERN_INFO"%s: error: cannot allocate memory\n", __func__);
		goto out;
	}
	#endif
	err = scan_for_bad_eraseblocks();
	if (err)
		goto out;

	return 0;

out:
	return err;
}

//FACTORY_RESET
unsigned int lge_get_mtd_part_info(void)
{
	unsigned int result = 0;

	result = (PERSIST_PART_NUM<<16) | (PERSIST_PART_SIZE&0x0000FFFF);
	printk(KERN_INFO"%s: part_num : %d, count : %d\n", __func__, PERSIST_PART_NUM, PERSIST_PART_SIZE);
	return result;
}

//LGE_FOTA
unsigned int lge_get_mtd_part_info_fota(void)
{
	unsigned int result = 0;

	result = (FOTA_USD_PART_NUM<<16) | (FOTA_USD_PART_SIZE&0x0000FFFF);
	printk(KERN_INFO"%s: part_num : %d, count : %d\n", __func__, FOTA_USD_PART_NUM, FOTA_USD_PART_SIZE);
	return result;
}

int lge_get_mtd_factory_mode_blk(int target_blk)
{
	int i = 0;
	int factory_mode_blk = 0;

	for (i = 0; i < ebcnt; i++) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if(bbt[i])
			continue;
		else
		{
			factory_mode_blk += 1;
			if(factory_mode_blk == target_blk)
				return i;
		}
	}
	return -1;
}

static int __init lge_mtd_direct_access_init(void)
{
	printk(KERN_INFO"%s: finished\n", __func__);

	return 0;
}

static void __exit lge_mtd_direct_access_exit(void)
{
	return;
}

module_init(lge_mtd_direct_access_init);
module_exit(lge_mtd_direct_access_exit);

MODULE_DESCRIPTION("LGE mtd direct access apis");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
