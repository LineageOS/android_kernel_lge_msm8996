/* Copyright (c) 2017, Elliott Mitchell. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * version 3, or any later versions as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/init.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <soc/qcom/smem.h>

#include "dirtysanta_fixup.h"


#define LG_MODEL_NAME_SIZE 22


struct lge_smem_vendor0 {
	// following 2 fields are common to all devices
	int hw_rev;
	char model_name[10]; // "MSM8996_H1"
	// following fields depend on the device
	char nt_code[2048];
	char lg_model_name[LG_MODEL_NAME_SIZE];
	int sim_num;
	int flag_gpio;

	// the structure in memory is 2096 bytes, likely for alignment
};


static char ds_dev_name[LG_MODEL_NAME_SIZE] __initdata=CONFIG_DIRTYSANTA_FIXUP_DEVICENAME;
static char sim_num __initdata=CONFIG_DIRTYSANTA_FIXUP_SIMCOUNT;


typedef struct device_attribute attr_type;
static ssize_t dirtysanta_show(struct device *, attr_type *, char *);
static ssize_t dirtysanta_store(struct device *, attr_type *, const char *,
size_t);

static attr_type lg_model_name_attr = __ATTR(dirtysanta_lg_model_name,
	S_IRUGO, dirtysanta_show, dirtysanta_store);
static attr_type sim_num_attr = __ATTR(dirtysanta_sim_num,
	S_IRUGO, dirtysanta_show, dirtysanta_store);


static int __init dirtysanta_fixup_loadcfg(void)
{
	const char MODELNAMEEQ[]="model.name=";
	const char SIMNUMEQ[]="lge.sim_num=";
	int dev_name_len;
	char *match;
	int ret=0;

	if(!ds_dev_name[0]) {
		if(!(match=strstr(saved_command_line, MODELNAMEEQ))) {
			pr_err("DirtySanta: \"%s\" not passed on kernel command-line\n", MODELNAMEEQ);

			lg_model_name_attr.attr.mode|=S_IWUSR;
		} else {
			match+=strlen(MODELNAMEEQ);
			dev_name_len=strchrnul(match, ' ')-match;

			if(dev_name_len>=LG_MODEL_NAME_SIZE) {
				pr_warning("DirtySanta: model.name is longer than VENDOR0 buffer, truncating!\n");
				dev_name_len=LG_MODEL_NAME_SIZE-1;
			}

			memcpy(ds_dev_name, match, dev_name_len);
			ds_dev_name[dev_name_len]='\0';
		}
	}

	if(!sim_num) {
		if(!(match=strstr(saved_command_line, SIMNUMEQ))) {
			pr_err("DirtySanta: \"%s\" not passed on kernel command-line\n", SIMNUMEQ);

			sim_num_attr.attr.mode|=S_IWUSR;
		} else {
			sim_num=match[strlen(SIMNUMEQ)]-'0';

			if(sim_num<1||sim_num>2)
				pr_warning("DirtySanta: SIM count of %d is odd\n", sim_num);
		}
	}

	pr_info("DirtySanta: values: \"%s%s\" \"%s%d\"\n", MODELNAMEEQ,
ds_dev_name, SIMNUMEQ, sim_num);

	return ret;
}


static int __init dirtysanta_fixup_msm_modem(void)
{
	struct lge_smem_vendor0 *ptr;

	unsigned size;

	if(IS_ERR_OR_NULL(ptr=smem_get_entry(SMEM_ID_VENDOR0, &size, 0, SMEM_ANY_HOST_FLAG))) {
		pr_info("DirtySanta: Qualcomm smem not initialized as of subsys_init\n");
		return -EFAULT;
	}

	if(size<sizeof(struct lge_smem_vendor0)) {
		pr_err("DirtySanta: Memory area returned by smem_get_entry() too small\n");
		return -ENOMEM;
	}

	if(!sim_num||!ds_dev_name[0]) {

		int ret;
		if((ret=dirtysanta_fixup_loadcfg())) return ret;

	}

	pr_info("DirtySanta: Overwriting VENDOR0 area in subsys_init\n");

	strcpy(ptr->lg_model_name, ds_dev_name);
	ptr->sim_num=sim_num;

	return 0;
}

/* command-line is loaded at core_initcall() **
** smem handler is initialized at arch_initcall() */
subsys_initcall(dirtysanta_fixup_msm_modem);


int dirtysanta_attach(struct device *dev)
{
	device_create_file(dev, &lg_model_name_attr);
	device_create_file(dev, &sim_num_attr);

	return 1;
}
EXPORT_SYMBOL(dirtysanta_attach);


static ssize_t dirtysanta_show(struct device *dev, attr_type *attr, char *buf)
{
	struct lge_smem_vendor0 *ptr;
	unsigned size;

	if(IS_ERR_OR_NULL(ptr=smem_get_entry(SMEM_ID_VENDOR0, &size, 0, SMEM_ANY_HOST_FLAG))) {
		pr_info("DirtySanta: Qualcomm smem not initialized.\n");
		return -EFAULT;
	}

	if(size<sizeof(struct lge_smem_vendor0)) {
		pr_err("DirtySanta: Memory area returned by smem_get_entry() too small\n");
		return -ENOMEM;
	}

	if(attr==&lg_model_name_attr)
		return snprintf(buf, PAGE_SIZE, "%s\n", ptr->lg_model_name);
	else if(attr==&sim_num_attr)
		return snprintf(buf, PAGE_SIZE, "%u\n", ptr->sim_num);
	return -EINVAL;
}

static ssize_t dirtysanta_store(struct device *dev, attr_type *attr,
const char *buf, size_t len)
{
	struct lge_smem_vendor0 *ptr;
	unsigned size;

	if(IS_ERR_OR_NULL(ptr=smem_get_entry(SMEM_ID_VENDOR0, &size, 0, SMEM_ANY_HOST_FLAG))) {
		pr_info("DirtySanta: Qualcomm smem not initialized.\n");
		return -EFAULT;
	}

	if(size<sizeof(struct lge_smem_vendor0)) {
		pr_err("DirtySanta: Memory area returned by smem_get_entry() too small\n");
		return -ENOMEM;
	}

	if(attr==&lg_model_name_attr) {
		if(len>=LG_MODEL_NAME_SIZE) {
			pr_notice("DirtySanta: Model name is too long\n");
			return -EINVAL;
		}
		if(strncmp("LG-H99", buf, 6))
			pr_notice("DirtySanta: Model name is unusual\n");

		memcpy(ptr->lg_model_name, buf, len);
		ptr->lg_model_name[len]='\0';
		pr_info("DirtySanta: Modem name \"%s\"\n", ptr->lg_model_name);

		return len;
	} else if(attr==&sim_num_attr) {
		int rc, cnt;
		if((rc=kstrtoint(buf, 0, &cnt))) return rc;
		if(cnt<=0) {
			pr_notice("DirtySanta: Got zero/negative SIM count: %d\n", cnt);
			return -EINVAL;
		} else if(cnt>2) {
			if(cnt>4) {
				pr_notice("DirtySanta: Got excessive SIM count: %d\n", cnt);
				return -EINVAL;
			}
			pr_info("DirtySanta: Got unusually high SIM count\n");
		}

		pr_info("DirtySanta: SIM count %d\n", cnt);
		ptr->sim_num=cnt;
		return len;
	}

	return -EINVAL;
}

