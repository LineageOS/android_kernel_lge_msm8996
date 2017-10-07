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
#include <soc/qcom/smem.h>


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


static char last[32] __initdata="";

static char dev_name[LG_MODEL_NAME_SIZE] __initdata=CONFIG_DIRTYSANTA_FIXUP_DEVICENAME;
static char sim_num __initdata=CONFIG_DIRTYSANTA_FIXUP_SIMCOUNT;


static int __init dirtysanta_fixup_loadcfg(void)
{
	const char MODELNAMEEQ[]="model.name=";
	const char SIMNUMEQ[]="lge.sim_num=";
	char *_dev_name;
	int dev_name_len;
	char *_sim_num;

	if(!dev_name[0]) {
		if(!(_dev_name=strstr(saved_command_line, MODELNAMEEQ))) {
			pr_err("DirtySanta: \"%s\" not passed on kernel command-line\n", MODELNAMEEQ);
			return -EINVAL;
		}
		_dev_name+=strlen(MODELNAMEEQ);
		dev_name_len=strchrnul(_dev_name, ' ')-_dev_name;

		if(dev_name_len>=LG_MODEL_NAME_SIZE) {
			pr_warning("DirtySanta: model.name is longer than VENDOR0 buffer, truncating!\n");
			dev_name_len=LG_MODEL_NAME_SIZE-1;
		}

		memcpy(dev_name, _dev_name, dev_name_len);
		dev_name[dev_name_len]='\0';
	}

	if(!sim_num) {
		if(!(_sim_num=strstr(saved_command_line, SIMNUMEQ))) {
			pr_err("DirtySanta: \"%s\" not passed on kernel command-line\n", SIMNUMEQ);
			return -EINVAL;
		}
		sim_num=_sim_num[strlen(SIMNUMEQ)]-'0';

		if(sim_num<1||sim_num>2)
			pr_warning("DirtySanta: SIM count of %d is odd\n", sim_num);
	}

	pr_info("DirtySanta: values: \"%s%s\" \"%s%d\"\n", MODELNAMEEQ,
dev_name, SIMNUMEQ, sim_num);

	return 0;
}


static int __init dirtysanta_fixup_msm_modem(void)
{
	struct lge_smem_vendor0 *ptr;

	char *msg;

	unsigned size;

	const char caller[]="dirtysanta_early_fixup";

	if(IS_ERR_OR_NULL(ptr=smem_get_entry(SMEM_ID_VENDOR0, &size, 0, SMEM_ANY_HOST_FLAG))) {
		pr_info("DirtySanta: Qualcomm smem not initialized as of \"%s()\"\n", caller);
		return -EFAULT;
	}

	if(size<sizeof(struct lge_smem_vendor0)) {
		pr_err("DirtySanta: Memory area returned by smem_get_entry() too small, when called by \"%s()\"\n", caller);
		return -ENOMEM;
	}

	if(!sim_num||!dev_name[0]) {
		int ret;
		if((ret=dirtysanta_fixup_loadcfg())) return ret;
	}

	if(!strcmp(ptr->lg_model_name, dev_name)&&ptr->sim_num==sim_num) {
		pr_info("DirtySanta: Previous overwrite of VENDOR0 (\"%s()\") still effective.\n", last);
		return 0;
	}

	if(last[0]) msg=KERN_INFO "DirtySanta: Needing to overwrite VENDOR0 a second time from \"%s()\" (last was \"%s()\")\n";
	else msg=KERN_INFO "DirtySanta: Overwriting VENDOR0 model name first time from \"%s()\"\n";

	printk(msg, caller, last);

	strlcpy(last, caller, sizeof(last));

	strcpy(ptr->lg_model_name, dev_name);
	ptr->sim_num=sim_num;

	return 0;
}

/* command-line is loaded at core_initcall() **
** smem handler is initialized at arch_initcall() */
subsys_initcall(dirtysanta_fixup_msm_modem);

