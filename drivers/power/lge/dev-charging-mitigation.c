#define pr_fmt(fmt) "CHARGING-MITIGATION: %s: " fmt, __func__
#define pr_chgmiti(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

#define CHARGING_MITIGATION_DEVICE	"lge-chg-mitigation"

#define VOTER_THERMALD_IUSB		"thermald-iusb"
#define VOTER_THERMALD_IBAT		"thermald-ibat"
#define VOTER_THERMALD_IDC		"thermald-idc"
#define VOTER_QCHGSTAT_LCD		"qchgstat-lcd"
#define VOTER_QCHGSTAT_CALL		"qchgstat-call"

// Configure here
#define QCHGSTAT_IBAT_CONTROLLABLE	true
#define QCHGSTAT_LCD_VTYPE		(QCHGSTAT_IBAT_CONTROLLABLE ? LIMIT_VOTER_IBAT : LIMIT_VOTER_IUSB)
#define QCHGSTAT_LCD_LIMIT		1000
#define QCHGSTAT_CALL_VTYPE		(QCHGSTAT_IBAT_CONTROLLABLE ? LIMIT_VOTER_IBAT : LIMIT_VOTER_IUSB)
#define QCHGSTAT_CALL_LIMIT		500

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "inc-limit-voter.h"

enum quickchg_state {
	STATE_NONE = 0,
	STATE_LCD_ON,
	STATE_LCD_OFF,
	STATE_CALL_ON,
	STATE_CALL_OFF,
};

struct charging_mitigation {
	// Voters
	struct limit_voter thermal_mitigation_iusb;
	struct limit_voter thermal_mitigation_ibat;
	struct limit_voter thermal_mitigation_idc;
	struct limit_voter quickchg_state_lcd;
	struct limit_voter quickchg_state_call;

	// sysfs data
	int sysfs_mitigation_iusb;
	int sysfs_mitigation_ibat;
	int sysfs_mitigation_idc;
	int sysfs_mitigation_qstate;
};

static ssize_t mitigation_iusb_show(struct device *dev,
		struct device_attribute *attr, char *buffer);
static ssize_t mitigation_iusb_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t mitigation_ibat_show(struct device *dev,
		struct device_attribute *attr, char *buffer);
static ssize_t mitigation_ibat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t mitigation_idc_show(struct device *dev,
		struct device_attribute *attr, char *buffer);
static ssize_t mitigation_idc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t mitigation_qstate_show(struct device *dev,
		struct device_attribute *attr, char *buffer);
static ssize_t mitigation_qstate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

static DEVICE_ATTR(mitigation_iusb, S_IWUSR|S_IRUGO, mitigation_iusb_show, mitigation_iusb_store);
static DEVICE_ATTR(mitigation_ibat, S_IWUSR|S_IRUGO, mitigation_ibat_show, mitigation_ibat_store);
static DEVICE_ATTR(mitigation_idc, S_IWUSR|S_IRUGO, mitigation_idc_show, mitigation_idc_store);
static DEVICE_ATTR(mitigation_qstate, S_IWUSR|S_IRUGO, mitigation_qstate_show, mitigation_qstate_store);

static struct attribute* charging_mitigation_attrs [] = {
	&dev_attr_mitigation_iusb.attr,
	&dev_attr_mitigation_ibat.attr,
	&dev_attr_mitigation_idc.attr,
	&dev_attr_mitigation_qstate.attr,
	NULL
};

static const struct attribute_group charging_mitigation_files = {
	.attrs  = charging_mitigation_attrs,
};

static ssize_t mitigation_iusb_show(struct device *dev,
		struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	return snprintf(buffer, PAGE_SIZE, "%d", mitigation_me->sysfs_mitigation_iusb);
}
static ssize_t mitigation_iusb_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	sscanf(buf, "%d", &mitigation_me->sysfs_mitigation_iusb);

	if (0 < mitigation_me->sysfs_mitigation_iusb)
		limit_voter_set(&mitigation_me->thermal_mitigation_iusb,
			mitigation_me->sysfs_mitigation_iusb);
	else
		limit_voter_release(&mitigation_me->thermal_mitigation_iusb);

	pr_chgmiti("iusb mitigatin = %d\n", mitigation_me->sysfs_mitigation_iusb);

	return size;
}

static ssize_t mitigation_ibat_show(struct device *dev,
		struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	return snprintf(buffer, PAGE_SIZE, "%d", mitigation_me->sysfs_mitigation_ibat);
}
static ssize_t mitigation_ibat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	sscanf(buf, "%d", &mitigation_me->sysfs_mitigation_ibat);

	if (0 < mitigation_me->sysfs_mitigation_ibat)
		limit_voter_set(&mitigation_me->thermal_mitigation_ibat,
			mitigation_me->sysfs_mitigation_ibat);
	else
		limit_voter_release(&mitigation_me->thermal_mitigation_ibat);

	pr_chgmiti("ibat mitigatin = %d\n", mitigation_me->sysfs_mitigation_ibat);

	return size;
}

static ssize_t mitigation_idc_show(struct device *dev,
		struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	return snprintf(buffer, PAGE_SIZE, "%d", mitigation_me->sysfs_mitigation_idc);
}
static ssize_t mitigation_idc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	sscanf(buf, "%d", &mitigation_me->sysfs_mitigation_idc);

	if (0 < mitigation_me->sysfs_mitigation_idc)
		limit_voter_set(&mitigation_me->thermal_mitigation_idc,
			mitigation_me->sysfs_mitigation_idc);
	else
		limit_voter_release(&mitigation_me->thermal_mitigation_idc);

	pr_chgmiti("idc mitigatin = %d\n", mitigation_me->sysfs_mitigation_idc);

	return size;
}

static ssize_t mitigation_qstate_show(struct device *dev,
		struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	return snprintf(buffer, PAGE_SIZE, "%d", mitigation_me->sysfs_mitigation_qstate);
}
static ssize_t mitigation_qstate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	sscanf(buf, "%d", &mitigation_me->sysfs_mitigation_qstate);

	switch (mitigation_me->sysfs_mitigation_qstate) {
	case STATE_LCD_ON:
		limit_voter_set(&mitigation_me->quickchg_state_lcd, QCHGSTAT_LCD_LIMIT);
		pr_chgmiti("LCD on decreasing chg_current\n");
		break;
	case STATE_LCD_OFF:
		limit_voter_release(&mitigation_me->quickchg_state_lcd);
		pr_chgmiti("LCD off return max chg_current\n");
		break;
	case STATE_CALL_ON:
		limit_voter_set(&mitigation_me->quickchg_state_call, QCHGSTAT_CALL_LIMIT);
		pr_chgmiti("Call on decreasing chg_current\n");
		break;
	case STATE_CALL_OFF:
		limit_voter_release(&mitigation_me->quickchg_state_call);
		pr_chgmiti("Call off return max chg_current\n");
		break;
	default:
		limit_voter_release(&mitigation_me->quickchg_state_lcd);
		limit_voter_release(&mitigation_me->quickchg_state_call);
		break;
	}

	pr_chgmiti("set quick_charging_state[%d]\n", mitigation_me->sysfs_mitigation_qstate);
	return size;
}

static int charging_mitigation_voters(struct charging_mitigation* mitigation_me) {
// For Thermald iusb
	if (limit_voter_register(&mitigation_me->thermal_mitigation_iusb, VOTER_THERMALD_IUSB,
			LIMIT_VOTER_IUSB, NULL, NULL, NULL))
		return -EINVAL;

// For Thermald ibat
	if (limit_voter_register(&mitigation_me->thermal_mitigation_ibat, VOTER_THERMALD_IBAT,
			LIMIT_VOTER_IBAT, NULL, NULL, NULL))
		return -EINVAL;

// For Thermald idc
	if (limit_voter_register(&mitigation_me->thermal_mitigation_idc, VOTER_THERMALD_IDC,
			LIMIT_VOTER_IDC, NULL, NULL, NULL))
		return -EINVAL;

// For qchgstat lcd
	if (limit_voter_register(&mitigation_me->quickchg_state_lcd, VOTER_QCHGSTAT_LCD,
			QCHGSTAT_LCD_VTYPE, NULL, NULL, NULL))
		return -EINVAL;

// For qchgstat call
	if (limit_voter_register(&mitigation_me->quickchg_state_call, VOTER_QCHGSTAT_CALL,
			QCHGSTAT_CALL_VTYPE, NULL, NULL, NULL))
		return -EINVAL;

	return 0;
}

static struct platform_device charging_mitigation_device = {
	.name = CHARGING_MITIGATION_DEVICE,
	.id = -1,
	.dev = {
		.platform_data = NULL,
	}
};

static int __init charging_mitigation_init(void) {
	struct charging_mitigation* mitigation_me = kzalloc(sizeof(struct charging_mitigation), GFP_KERNEL);
	if (mitigation_me)
		charging_mitigation_device.dev.platform_data = mitigation_me;
	else
		goto out;

	if (charging_mitigation_voters(mitigation_me) < 0) {
		pr_err("Fail to preset voters\n");
		goto out;
	}
	if (platform_device_register(&charging_mitigation_device) < 0) {
		pr_chgmiti("unable to register charging mitigation device\n");
		goto out;
	}
	if (sysfs_create_group(&charging_mitigation_device.dev.kobj, &charging_mitigation_files) < 0) {
		pr_chgmiti("unable to create charging mitigation sysfs\n");
		goto out;
	}
	goto success;

out :
	kfree(mitigation_me);
success:
	return 0;
}

static void __exit charging_mitigation_exit(void) {
	kfree(charging_mitigation_device.dev.platform_data);
	platform_device_unregister(&charging_mitigation_device);
}

module_init(charging_mitigation_init);
module_exit(charging_mitigation_exit);

MODULE_DESCRIPTION(CHARGING_MITIGATION_DEVICE);
MODULE_LICENSE("GPL v2");

