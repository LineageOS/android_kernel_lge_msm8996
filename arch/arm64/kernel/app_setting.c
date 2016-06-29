/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/notifier.h>

#include <asm/app_api.h>

#define APP_SETTING_BIT 	30
#define MAX_LEN 		100
#define MAX_ENTRIES		10

char *lib_names[MAX_ENTRIES];
unsigned int lib_name_entries;
static struct mutex mutex;

static char lib_str[MAX_LEN] = "";
static struct kparam_string kps = {
	.string			= lib_str,
	.maxlen			= MAX_LEN,
};
static int set_name(const char *str, struct kernel_param *kp);
module_param_call(lib_name, set_name, param_get_string, &kps, S_IWUSR);

static uint32_t enable;
static int app_setting_set(const char *val, struct kernel_param *kp);
module_param_call(enable, app_setting_set, param_get_uint,
		  &enable, 0644);

static DEFINE_PER_CPU(int, app_setting_applied);

static void app_setting_enable(void *unused)
{
	set_app_setting_bit(APP_SETTING_BIT);
	this_cpu_write(app_setting_applied, 1);
}

static void app_setting_disable(void *unused)
{
	clear_app_setting_bit(APP_SETTING_BIT);
	this_cpu_write(app_setting_applied, 0);
}

static int set_name(const char *str, struct kernel_param *kp)
{
	int len = strlen(str);
	char *name;

	if (len >= MAX_LEN) {
		pr_err("app_setting: name string too long\n");
		return -ENOSPC;
	}

	/* echo adds '\n' which we need to chop off later */
	name = (char *)kzalloc(len, GFP_KERNEL);
	if (!name) {
		pr_err("app_setting: malloc failed\n");
		return -ENOMEM;
	}
	strcpy(name, str);
	if (name[len - 1] == '\n')
		name[len - 1] = '\0';

	mutex_lock(&mutex);
	if (lib_name_entries < MAX_ENTRIES) {
		lib_names[lib_name_entries++] = name;
	} else {
		pr_err("app_setting: set name failed. Max entries reached\n");
		mutex_unlock(&mutex);
		return -EPERM;
	}
	mutex_unlock(&mutex);

	return 0;
}

static int app_setting_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_uint(val, kp);
	if (ret) {
		pr_err("app_setting: error setting param value %d\n", ret);
		return ret;
	}

	get_online_cpus();
	if (enable)
		on_each_cpu(app_setting_enable, NULL, 1);
	else
		on_each_cpu(app_setting_disable, NULL, 1);
	put_online_cpus();

	return 0;
}

static int app_setting_notifier_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		if (enable && !__this_cpu_read(app_setting_applied))
			app_setting_enable(NULL);
		else if (!enable && __this_cpu_read(app_setting_applied))
			app_setting_disable(NULL);
		break;
	}
	return 0;
}

static struct notifier_block app_setting_notifier = {
	.notifier_call = app_setting_notifier_callback,
};

static int __init app_setting_init(void)
{
	int ret;

	ret = register_hotcpu_notifier(&app_setting_notifier);
	if (ret)
		return ret;
	mutex_init(&mutex);

	return 0;
}
module_init(app_setting_init);
