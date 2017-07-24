#define pr_fmt(fmt) "CHARGING-MITIGATION: %s: " fmt, __func__
#define pr_chgmiti(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

#define CHARGING_MITIGATION_DEVICE	"lge-chg-mitigation"

// Interacting with thermal deamon
#define VOTER_THERMALD_IUSB		"thermald-iusb"
#define VOTER_THERMALD_IBAT		"thermald-ibat"
#define VOTER_THERMALD_IDC		"thermald-idc"
// Interacting with user scenario
#define VOTER_SCENARIO_CALL		"scenario-call"
#define VOTER_SCENARIO_TDMB		"scenario-tdmb"

// Configure here
#define SCENARIO_IBAT_CONTROLLABLE	true
#define VOTER_TYPE_CALL			(SCENARIO_IBAT_CONTROLLABLE ? LIMIT_VOTER_IBAT : LIMIT_VOTER_IUSB)
#define VOTER_TYPE_TDMB			(SCENARIO_IBAT_CONTROLLABLE ? LIMIT_VOTER_IBAT : LIMIT_VOTER_IUSB)
// Limit values in milli-Ampere
#define VOTER_LIMIT_LCD			1000
#define VOTER_LIMIT_CALL		500
#define VOTER_LIMIT_TDMB_500		500
#define VOTER_LIMIT_TDMB_300		300

// For legacy sysfs named "quick_charging_state"
// NOTICE : Mitigation for LCD On/Off should be deprecated!
// Restricting chg current on LCD On/Off is transferred to thermal-daemon
#define SCENARIO_LCD_ON			1
#define SCENARIO_LCD_OFF		2
#define SCENARIO_CALL_ON		3
#define SCENARIO_CALL_OFF		4
// For TDMB sysfs named "tdmb_mode_on"
// NOTICE : Mitigation for TDMB is only for KR, but DON'T DISABLE its declaration here
// DO disable it in 'charging_mitigation_attrs' with feature
#define SCENARIO_TDMB_OFF		0
#define SCENARIO_TDMB_500		1
#define SCENARIO_TDMB_300		2

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "inc-limit-voter.h"

struct charging_mitigation {
	// Voters
	// Controlled by Thermal Engine
	struct limit_voter voter_thermald_iusb;
	struct limit_voter voter_thermald_ibat;
	struct limit_voter voter_thermald_idc;
	// Controlled by User scenario
	struct limit_voter voter_scenario_call;
	struct limit_voter voter_scenario_tdmb;
};

static ssize_t thermald_iusb_show(struct device *dev,
	struct device_attribute *attr, char *buffer);
static ssize_t thermald_iusb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t thermald_ibat_show(struct device *dev,
	struct device_attribute *attr, char *buffer);
static ssize_t thermald_ibat_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t thermald_idc_show(struct device *dev,
	struct device_attribute *attr, char *buffer);
static ssize_t thermald_idc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t scenario_call_show(struct device *dev,
	struct device_attribute *attr, char *buffer);
static ssize_t scenario_call_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t scenario_tdmb_show(struct device *dev,
	struct device_attribute *attr, char *buffer);
static ssize_t scenario_tdmb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

static DEVICE_ATTR(thermald_iusb, S_IWUSR|S_IRUGO, thermald_iusb_show, thermald_iusb_store);
static DEVICE_ATTR(thermald_ibat, S_IWUSR|S_IRUGO, thermald_ibat_show, thermald_ibat_store);
static DEVICE_ATTR(thermald_idc, S_IWUSR|S_IRUGO, thermald_idc_show, thermald_idc_store);
static DEVICE_ATTR(scenario_call, S_IWUSR|S_IRUGO, scenario_call_show, scenario_call_store);
static DEVICE_ATTR(scenario_tdmb, S_IWUSR|S_IRUGO, scenario_tdmb_show, scenario_tdmb_store);

static struct attribute* charging_mitigation_attrs [] = {
	&dev_attr_thermald_iusb.attr,
	&dev_attr_thermald_ibat.attr,
	&dev_attr_thermald_idc.attr,
	&dev_attr_scenario_call.attr,
	&dev_attr_scenario_tdmb.attr,
	NULL
};

static const struct attribute_group charging_mitigation_files = {
	.attrs  = charging_mitigation_attrs,
};

static ssize_t thermald_iusb_show(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	struct limit_voter* voter_iusb = &mitigation_me->voter_thermald_iusb;

	return snprintf(buffer, PAGE_SIZE, "%d", voter_iusb->limit);
}
static ssize_t thermald_iusb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	int limit_ma;
	sscanf(buf, "%d", &limit_ma);

	if (0 < limit_ma) {
		// mitigation_me->voter_thermald_iusb->limit will be updated to limit_ma
		// after limit_voter_set
		limit_voter_set(&mitigation_me->voter_thermald_iusb,
			limit_ma);
	}
	else
		limit_voter_release(&mitigation_me->voter_thermald_iusb);

	pr_chgmiti("iusb mitigation = %d\n", limit_ma);

	return size;
}

static ssize_t thermald_ibat_show(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	struct limit_voter* voter_ibat = &mitigation_me->voter_thermald_ibat;

	return snprintf(buffer, PAGE_SIZE, "%d", voter_ibat->limit);
}
static ssize_t thermald_ibat_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	int limit_ma;
	sscanf(buf, "%d", &limit_ma);

	if (0 < limit_ma) {
		// mitigation_me->voter_thermald_ibat->limit will be updated to limit_ma
		// after limit_voter_set
		limit_voter_set(&mitigation_me->voter_thermald_ibat,
			limit_ma);
	}
	else
		limit_voter_release(&mitigation_me->voter_thermald_ibat);

	pr_chgmiti("ibat mitigation = %d\n", limit_ma);

	return size;
}

static ssize_t thermald_idc_show(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	struct limit_voter* voter_idc = &mitigation_me->voter_thermald_idc;

	return snprintf(buffer, PAGE_SIZE, "%d", voter_idc->limit);
}
static ssize_t thermald_idc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	int limit_ma;
	sscanf(buf, "%d", &limit_ma);

	if (0 < limit_ma) {
		// mitigation_me->voter_thermald_idc->limit will be updated to limit_ma
		// after limit_voter_set
		limit_voter_set(&mitigation_me->voter_thermald_idc,
			limit_ma);
	}
	else
		limit_voter_release(&mitigation_me->voter_thermald_idc);

	pr_chgmiti("idc mitigation = %d\n", limit_ma);

	return size;
}

static ssize_t scenario_call_show(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	struct limit_voter* voter_call = &mitigation_me->voter_scenario_call;

	return snprintf(buffer, PAGE_SIZE, "%d", voter_call->limit);
}
static ssize_t scenario_call_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	int qchgstat;
	sscanf(buf, "%d", &qchgstat);

	switch (qchgstat) {
	case SCENARIO_CALL_ON:
		// mitigation_me->voter_scenario_call->limit will be set to 500
		limit_voter_set(&mitigation_me->voter_scenario_call, VOTER_LIMIT_CALL);
		pr_chgmiti("Call on : decreasing chg current\n");
		break;
	case SCENARIO_CALL_OFF:
		// mitigation_me->voter_scenario_call->limit will be released
		limit_voter_release(&mitigation_me->voter_scenario_call);
		pr_chgmiti("Call off : returning to max chg current\n");
		break;

	case SCENARIO_LCD_ON:
	case SCENARIO_LCD_OFF:
	default:
		pr_chgmiti("command '%d' is ignored.\n", qchgstat);
		break;
	}

	pr_chgmiti("set quick_charging_state [%d]\n", qchgstat);
	return size;
}

static ssize_t scenario_tdmb_show(struct device *dev,
	struct device_attribute *attr, char *buffer) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	struct limit_voter* voter_tdmb = &mitigation_me->voter_scenario_tdmb;

	return snprintf(buffer, PAGE_SIZE, "%d", voter_tdmb->limit);
}
static ssize_t scenario_tdmb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {

	struct charging_mitigation* mitigation_me = dev->platform_data;
	int tdmb;
	sscanf(buf, "%d", &tdmb);

	switch (tdmb) {
	case SCENARIO_TDMB_500:
		// mitigation_me->voter_scenario_tdmb->limit will be set to 500
		limit_voter_set(&mitigation_me->voter_scenario_tdmb, VOTER_LIMIT_TDMB_500);
		pr_chgmiti("Call off : returning to max chg current\n");
		break;
	case SCENARIO_TDMB_300:
		// mitigation_me->voter_scenario_tdmb->limit will be set to 300
		limit_voter_set(&mitigation_me->voter_scenario_tdmb, VOTER_LIMIT_TDMB_300);
		pr_chgmiti("Call off : returning to max chg current\n");
		break;
	case SCENARIO_TDMB_OFF:
		// mitigation_me->voter_scenario_tdmb->limit will be released
		limit_voter_release(&mitigation_me->voter_scenario_tdmb);
		pr_chgmiti("Call on : decreasing chg current\n");
		break;
	default:
		pr_chgmiti("command '%d' is ignored.\n", tdmb);
		break;
	}

	pr_chgmiti("set tdmb_mode_on [%d]\n", tdmb);
	return size;
}

static int charging_mitigation_voters(struct charging_mitigation* mitigation_me) {
	// For Thermald iusb
	if (limit_voter_register(&mitigation_me->voter_thermald_iusb, VOTER_THERMALD_IUSB,
		LIMIT_VOTER_IUSB, NULL, NULL, NULL))
		return -EINVAL;

	// For Thermald ibat
	if (limit_voter_register(&mitigation_me->voter_thermald_ibat, VOTER_THERMALD_IBAT,
		LIMIT_VOTER_IBAT, NULL, NULL, NULL))
		return -EINVAL;

	// For Thermald idc
	if (limit_voter_register(&mitigation_me->voter_thermald_idc, VOTER_THERMALD_IDC,
		LIMIT_VOTER_IDC, NULL, NULL, NULL))
		return -EINVAL;

	// For scenario call
	if (limit_voter_register(&mitigation_me->voter_scenario_call, VOTER_SCENARIO_CALL,
		VOTER_TYPE_CALL, NULL, NULL, NULL))
		return -EINVAL;

	// For scenario tdmb
	if (limit_voter_register(&mitigation_me->voter_scenario_tdmb, VOTER_SCENARIO_TDMB,
		VOTER_TYPE_TDMB, NULL, NULL, NULL))
		return -EINVAL;

	return 0;
}

static struct platform_device charging_mitigation_device = {
	.name = CHARGING_MITIGATION_DEVICE,
	.id = -1,	// Set -1 explicitly to make device name simple
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

