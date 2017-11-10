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
#include <linux/ctype.h>
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


#ifdef CONFIG_DIRTYSANTA_FIXUP_SYSFS
typedef struct device_attribute attr_type;
static ssize_t dirtysanta_show(struct device *, attr_type *, char *);
static ssize_t dirtysanta_store(struct device *, attr_type *, const char *,
size_t);

static attr_type attrs[] = {
	__ATTR(dirtysanta_lg_model_name,
		S_IRUGO, dirtysanta_show, dirtysanta_store),
	__ATTR(dirtysanta_sim_num,
		S_IRUGO, dirtysanta_show, dirtysanta_store),
};
#endif


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

#ifdef CONFIG_DIRTYSANTA_FIXUP_SYSFS
			attrs[0].attr.mode|=S_IWUSR;
#endif
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

#ifdef CONFIG_DIRTYSANTA_FIXUP_SYSFS
			attrs[1].attr.mode|=S_IWUSR;
#endif
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


#ifdef CONFIG_DIRTYSANTA_FIXUP_SYSFS
int dirtysanta_attach(struct device *dev)
{
	int i;
	for(i=0; i<ARRAY_SIZE(attrs); ++i)
		device_create_file(dev, attrs+i);

	return 1;
}
EXPORT_SYMBOL(dirtysanta_attach);


void dirtysanta_detach(struct device *dev)
{
	int i;
	for(i=0; i<ARRAY_SIZE(attrs); ++i)
		device_remove_file(dev, attrs+i);
}
EXPORT_SYMBOL(dirtysanta_detach);


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

	switch(attr-attrs) {
	case 0:
		return snprintf(buf, PAGE_SIZE, "%s\n", ptr->lg_model_name);
	case 1:
		return snprintf(buf, PAGE_SIZE, "%u\n", ptr->sim_num);
	default:
		return -EINVAL;
	}
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

	switch(attr-attrs) {
		int rc, cnt;
		size_t use;
	case 0:
		/* filter out this extreme case early */
		if(len>=(LG_MODEL_NAME_SIZE+LG_MODEL_NAME_SIZE/2)) {
			pr_notice("DirtySanta: Model name is too long\n");
			return -EINVAL;
		}

		/* most likely \n, but an extra \0 can be caught too */
		for(use=0; use<len; ++use) if(!isprint(buf[use]))
			break;

		if(use>=LG_MODEL_NAME_SIZE) {
			pr_notice("DirtySanta: Model name is too long\n");
			return -EINVAL;
		}
		if(strncmp("LG-H99", buf, 6))
			pr_notice("DirtySanta: Model name is unusual\n");

		/* ARM compilers are expected to inline memcpy(), but GCC 4.9
		** fails to ensure proper alignment, ->lg_model_name only has
		** 2-byte alignment, not 4/8-byte alignment
		** ("Unhandled fault: alignment fault" panic).  GCC version 5.3
		** is believed to fix this, later versions are likely better.
		** https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66917 */
#if defined(__GNUC__) && !defined(__llvm__) && (__GNUC__<5 || (__GNUC__==5 && __GNUC_MINOR__<3))
#if 0
/* this is why you use blacklisting, NOT whitelisting!!! */
#warning "Using GCC 4.9 memcpy() workaround."
#endif
		ptr->lg_model_name[use]='\0';
		while(use--!=0) ptr->lg_model_name[use]=buf[use];
#else
		strlcpy(ptr->lg_model_name, buf, use+1);
#endif
		pr_info("DirtySanta: Modem name \"%s\"\n", ptr->lg_model_name);

		return len;

	case 1:
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

	default:
		return -EINVAL;
	}
}
#endif // CONFIG_DIRTYSANTA_FIXUP_SYSFS
