/*
 * Static library for Battery authentication
 *
 * Copyright (C) 2016 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "BATTERY-AUTHENTICATION: %s: " fmt, __func__

#define pr_batauth(reason, fmt, ...)				\
do {								\
	if (batauth_debug & (reason))				\
		pr_info(fmt, ##__VA_ARGS__);			\
	else							\
		pr_debug(fmt, ##__VA_ARGS__);			\
} while (0)

#define pr_assert(exp)						\
do {								\
	if ((batauth_debug & ASSERT) && !(exp)) {		\
		pr_batauth(ASSERT, "Assertion failed\n");	\
		dump_stack();					\
	}							\
} while (0)

#define DEVICE_NAME "lge-battery-auth"

#include <linux/module.h>
#include <linux/platform_device.h>

#include "inc-battery-authentication.h"

enum debug_reason {
	ASSERT = BIT(0),
	ERROR = BIT(1),
	INFO = BIT(2),
	UPDATE = BIT(3),

	VERBOSE = BIT(7),
};

static int batauth_debug = ERROR | INFO | UPDATE;
static enum battery_authentication batauth_type = BATTERY_AUTHENTICATION_UNKNOWN;

static ssize_t show_batauth_name(struct device *dev,
		struct device_attribute *attr, char *buffer);
static ssize_t show_batauth_type(struct device *dev,
		struct device_attribute *attr, char *buffer);
static ssize_t show_batauth_valid(struct device *dev,
		struct device_attribute *attr, char *buffer);

static DEVICE_ATTR(batauth_valid, S_IRUGO, show_batauth_valid, NULL);
static DEVICE_ATTR(batauth_name, S_IRUGO, show_batauth_name, NULL);
static DEVICE_ATTR(batauth_type, S_IRUGO, show_batauth_type, NULL);

static struct attribute* batauth_attrs [] = {
	&dev_attr_batauth_valid.attr,
	&dev_attr_batauth_name.attr,
	&dev_attr_batauth_type.attr,
	NULL
};

static const struct attribute_group batauth_files = {
	.attrs  = batauth_attrs,
};

static struct platform_device batauth_device = {
	.name = DEVICE_NAME,
	.id = -1,
	.dev = {
		.platform_data = NULL,
	}
};

static ssize_t show_batauth_valid(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	return snprintf(buffer, PAGE_SIZE, "%d", battery_authentication_valid());;
}

static ssize_t show_batauth_name(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	return snprintf(buffer, PAGE_SIZE, "%s", battery_authentication_name());
}

static ssize_t show_batauth_type(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	return snprintf(buffer, PAGE_SIZE, "%d", battery_authentication_type());;
}

bool battery_authentication_valid(void) {
	return (batauth_type!=BATTERY_AUTHENTICATION_ABSENT) &&
		(batauth_type!=BATTERY_AUTHENTICATION_UNKNOWN);
}

const char* battery_authentication_name(void) {
	return name_from_type(batauth_type);
}

enum battery_authentication battery_authentication_type(void) {
	return batauth_type;
}

void battery_authentication_force(void) {
	if (batauth_type != BATTERY_AUTHENTICATION_UNKNOWN) {
		pr_batauth(ERROR, "Auth information '%d' was passed"
			" from cmdline already\n", batauth_type);
	}

	batauth_type = BATTERY_AUTHENTICATION_FORCED;
	pr_batauth(UPDATE, "Auth information forced to be passed\n");
}

bool battery_authentication_present(void) {
	return batauth_type != BATTERY_AUTHENTICATION_ABSENT;
}

static int __init battery_authentication_setup(char* batauth_passed) {
	batauth_type = type_from_name(batauth_passed);

	pr_batauth(UPDATE, "Battery : %s %d\n", batauth_passed, batauth_type);
	return 1;
}

static int __init battery_authentication_init(void) {
	if (platform_device_register(
		&batauth_device) < 0) {
		pr_batauth(ERROR, "unable to register batauth device\n");
		goto out;
	}

	if (sysfs_create_group(&batauth_device.dev.kobj,
		&batauth_files) < 0) {
		pr_batauth(ERROR, "unable to create batauth sysfs\n");
		goto out;
	}

out :
	return 0;
}

static void __exit battery_authentication_exit(void) {
	platform_device_unregister(&batauth_device);
}

__setup("lge.battid=", battery_authentication_setup);

module_init(battery_authentication_init);
module_exit(battery_authentication_exit);

MODULE_DESCRIPTION("LGE battery authentication");
MODULE_LICENSE("GPL");
