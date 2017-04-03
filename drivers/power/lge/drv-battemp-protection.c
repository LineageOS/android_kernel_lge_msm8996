#define pr_fmt(fmt) "BATTEMP-PROTECTION: %s: " fmt, __func__
#define pr_batprot(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

#define BATTEMP_PROTECTION_COMPATIBLE	"lge,battemp-prot"
#define BATTEMP_PROTECTION_DRIVER	"lge-battemp-prot"
#define BATTEMP_PROTECTION_NAME		"battemp-prot"

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

#include "inc-limit-voter.h"

#define OF_PROP_READ_S32(dnode, buf, prop, rc)					\
do {										\
	if (rc)									\
		break;								\
										\
	rc = of_property_read_s32(dnode, "lge," prop, &buf);			\
										\
	if (rc)									\
		pr_batprot("Error reading " #prop " property rc = %d\n", rc);	\
	else									\
		pr_batprot("%s : %d\n", prop, buf);				\
} while (0)

#define OF_PROP_READ_STR(dnode, buf, prop, rc)					\
do {										\
	if (rc)									\
		break;								\
										\
	rc = of_property_read_string(dnode, "lge," prop, &buf);			\
										\
	if (rc)									\
		pr_batprot("Error reading " #prop " property rc = %d\n", rc);	\
	else									\
		pr_batprot("%s : %s\n", prop, buf);				\
} while (0)

#define LIMIT_PARTIALLY_CHARGING 450

enum protection_status {
	/* We don't have to consider not-charging status */
	CHARGING_RELEASED_UNDEFINED,
	CHARGING_RELEASED_TOTALLY,
	CHARGING_RELEASED_PARTIALLY,
	CHARGING_BLOCKED_DUE_TO_VOLTAGE,	// warm over 4V
	CHARGING_BLOCKED_DUE_TO_COLD,
	CHARGING_BLOCKED_DUE_TO_HOT,
};

struct battemp_protection {
//	struct device battemp_device; 	Do you REALLY need the 'device'?
	struct wake_lock battemp_wakelock;
	struct delayed_work battemp_work;

	struct limit_voter protect_voter;
	enum protection_status protect_status;

// below fields are set in device tree, except period_user
	int threashold_degc_upto_normal;	//  -50 by default
	int threashold_degc_upto_warm;		//  450 by default
	int threashold_degc_upto_hot;		//  550 by default
	int threashold_degc_downto_warm;	//  520 by default
	int threashold_degc_downto_normal;	//  430 by default
	int threashold_degc_downto_cold;	// -100 by default
	int threashold_mv_upto_critical;	// 4000 by default
	int threashold_mv_downto_safe;		// Not used yet

// polling period defined in device tree
	int period_ms_normal;			// 6000 by default
	int period_ms_decreased;		// 3000 by default
	int period_ms_blocked;			// 1000 by default
#define	    period_ms_emergency		1000	// for initialize
};

static const char* battemp_protection_status_to_string(
	enum protection_status status) {

	switch (status) {
	case CHARGING_RELEASED_UNDEFINED :
		return "CHARGING_RELEASED_UNDEFINED";
	case CHARGING_RELEASED_TOTALLY :;
		return "CHARGING_RELEASED_TOTALLY";
	case CHARGING_RELEASED_PARTIALLY :
		return "CHARGING_RELEASED_PARTIALLY";
	case CHARGING_BLOCKED_DUE_TO_VOLTAGE :
		return "CHARGING_BLOCKED_DUE_TO_VOLTAGE";
	case CHARGING_BLOCKED_DUE_TO_COLD :
		return "CHARGING_BLOCKED_DUE_TO_COLD";
	case CHARGING_BLOCKED_DUE_TO_HOT :
		return "CHARGING_BLOCKED_DUE_TO_HOT";
	default :
		return "Error parsing status";
	}
}
static long battemp_protection_status_to_period(
	struct battemp_protection* protection_me) {

	int msecs = 0;

	switch (protection_me->protect_status) {
	case CHARGING_BLOCKED_DUE_TO_COLD :
	case CHARGING_BLOCKED_DUE_TO_HOT :
	case CHARGING_BLOCKED_DUE_TO_VOLTAGE :
		msecs = protection_me->period_ms_blocked;
		break;
	case CHARGING_RELEASED_PARTIALLY :
		msecs = protection_me->period_ms_decreased;
		break;
	case CHARGING_RELEASED_TOTALLY :
		msecs = protection_me->period_ms_normal;
		break;
	case CHARGING_RELEASED_UNDEFINED :
		msecs = period_ms_emergency; // To update earlier
		break;
	default :
		pr_batprot("Check the protect_status\n");
		break;
	}

	return msecs_to_jiffies(msecs);
}

static int battemp_protection_status_to_limit(
	enum protection_status status) {

	switch (status) {
	case CHARGING_BLOCKED_DUE_TO_COLD :
	case CHARGING_BLOCKED_DUE_TO_HOT :
	case CHARGING_BLOCKED_DUE_TO_VOLTAGE :
		return LIMIT_TOTALLY_BLOCKED;

	case CHARGING_RELEASED_UNDEFINED :
	case CHARGING_RELEASED_TOTALLY :
		return LIMIT_TOTALLY_RELEASED;

	case CHARGING_RELEASED_PARTIALLY :
		return LIMIT_PARTIALLY_CHARGING;

	default :
		return -EINVAL;
	}
}
static int battemp_protection_get_rawdata(int* now_degc_battemp,
	int* now_mv_voltage) {
	int rc = 0;

	struct power_supply* battery_veneer = power_supply_get_by_name("battery-veneer");
	if (battery_veneer && battery_veneer->get_property) {
		union power_supply_propval temp = {0, };
		union power_supply_propval uvol = {0, };

		if (!battery_veneer->get_property(battery_veneer, POWER_SUPPLY_PROP_TEMP, &temp) &&
			!battery_veneer->get_property(battery_veneer, POWER_SUPPLY_PROP_VOLTAGE_NOW, &uvol)) {
				*now_degc_battemp = temp.intval;
				*now_mv_voltage = uvol.intval / 1000;
		}
		else
			rc = -EINVAL;
	}
	else {
		// Default temperature and voltage.
		// Do not use them except testing purpose.
		*now_degc_battemp = 250;
		*now_mv_voltage = 3500;
		rc = -EINVAL;
	}

	return rc;
}

static int battemp_protection_set_health(struct battemp_protection* protection_me,
	int temperature) {
	int rc = 0;

	struct power_supply* battery_veneer = power_supply_get_by_name("battery-veneer");
	if (battery_veneer && battery_veneer->set_property) {
		int health = POWER_SUPPLY_HEALTH_UNKNOWN;
		union power_supply_propval val = {0, };

		if (protection_me->threashold_degc_upto_hot < temperature)
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (temperature < protection_me->threashold_degc_downto_cold)
			health = POWER_SUPPLY_HEALTH_COLD;
		else
			health = POWER_SUPPLY_HEALTH_GOOD;

		val.intval = health;
		rc = battery_veneer->set_property(battery_veneer,
			POWER_SUPPLY_PROP_HEALTH, &val);
	}
	else
		rc = -EINVAL;

	return rc;
}

static int battemp_protection_get_status(struct battemp_protection* protection_me,
	int now_battemp, int now_voltage) {
	enum protection_status status_new;

	#define STAT_NOW (protection_me->protect_status)
	#define TEMP_NOW (now_battemp)
	#define VOLT_NOW (now_voltage)

	#define TEMP_UPTO_NORMAL (protection_me->threashold_degc_upto_normal)		//  -50 by default
	#define TEMP_UPTO_WARM (protection_me->threashold_degc_upto_warm)		//  450 by default
	#define TEMP_UPTO_HOT (protection_me->threashold_degc_upto_hot)			//  550 by default
	#define TEMP_DOWNTO_WARM (protection_me->threashold_degc_downto_warm)		//  520 by default
	#define TEMP_DOWNTO_NORMAL (protection_me->threashold_degc_downto_normal)	//  430 by default
	#define TEMP_DOWNTO_COLD (protection_me->threashold_degc_downto_cold)		// -100 by default

	#define VOLT_UPTO_CRITICAL (protection_me->threashold_mv_upto_critical)		// 4000 by default
	#define VOLT_DOWNTO_SAFE (protection_me->threashold_mv_downto_safe)		// Not used yet
	#define VOLT_IS_CRITICAL (VOLT_UPTO_CRITICAL < VOLT_NOW)			// 4000 by default
	#define VOLT_IS_SAFE (VOLT_NOW < VOLT_DOWNTO_SAFE)				// Not used yet

	switch (STAT_NOW) {
	case CHARGING_RELEASED_UNDEFINED :
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			status_new = CHARGING_BLOCKED_DUE_TO_COLD;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			status_new = CHARGING_RELEASED_TOTALLY;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			status_new = (VOLT_IS_CRITICAL) ? CHARGING_BLOCKED_DUE_TO_VOLTAGE : CHARGING_RELEASED_PARTIALLY;
		else
			status_new = CHARGING_BLOCKED_DUE_TO_HOT;
		break;

	case CHARGING_BLOCKED_DUE_TO_COLD : // on the cold
		if (TEMP_NOW < TEMP_UPTO_NORMAL)
			status_new = CHARGING_BLOCKED_DUE_TO_COLD;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			status_new = CHARGING_RELEASED_TOTALLY;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			status_new = (VOLT_IS_CRITICAL) ? CHARGING_BLOCKED_DUE_TO_VOLTAGE : CHARGING_RELEASED_PARTIALLY;
		else
			status_new = CHARGING_BLOCKED_DUE_TO_HOT;
		break;

	case CHARGING_RELEASED_TOTALLY : // on the normal
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			status_new = CHARGING_BLOCKED_DUE_TO_COLD;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			status_new = CHARGING_RELEASED_TOTALLY;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			status_new = (VOLT_IS_CRITICAL) ? CHARGING_BLOCKED_DUE_TO_VOLTAGE : CHARGING_RELEASED_PARTIALLY;
		else
			status_new = CHARGING_BLOCKED_DUE_TO_HOT;
		break;

	case CHARGING_RELEASED_PARTIALLY : // on the warm
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			status_new = CHARGING_BLOCKED_DUE_TO_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_NORMAL )
			status_new = CHARGING_RELEASED_TOTALLY;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			status_new = (VOLT_IS_CRITICAL) ? CHARGING_BLOCKED_DUE_TO_VOLTAGE : CHARGING_RELEASED_PARTIALLY;
		else
			status_new = CHARGING_BLOCKED_DUE_TO_HOT;
		break;

	case CHARGING_BLOCKED_DUE_TO_VOLTAGE : // on the warm
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			status_new = CHARGING_BLOCKED_DUE_TO_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_NORMAL )
			status_new = CHARGING_RELEASED_TOTALLY;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			status_new = (VOLT_IS_CRITICAL) ? CHARGING_BLOCKED_DUE_TO_VOLTAGE : CHARGING_RELEASED_PARTIALLY;
		else
			status_new = CHARGING_BLOCKED_DUE_TO_HOT;
		break;

	case CHARGING_BLOCKED_DUE_TO_HOT : // on the hot
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			status_new = CHARGING_BLOCKED_DUE_TO_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_NORMAL )
			status_new = CHARGING_RELEASED_TOTALLY;
		else if (TEMP_NOW < TEMP_DOWNTO_WARM)
			status_new = (VOLT_IS_CRITICAL) ? CHARGING_BLOCKED_DUE_TO_VOLTAGE : CHARGING_RELEASED_PARTIALLY;
		else
			status_new = CHARGING_BLOCKED_DUE_TO_HOT;
		break;
	}

	return status_new;
}

static void battemp_protection_polling(struct work_struct *work) {

	enum protection_status status_new;
	int now_battemp_degc;
	int now_voltage_mv;

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct battemp_protection* protection_me = container_of(delayed_work,
		struct battemp_protection, battemp_work);

	if (!battemp_protection_get_rawdata(&now_battemp_degc, &now_voltage_mv)) {
		// Update health on every update for battery temp
		battemp_protection_set_health(protection_me, now_battemp_degc);

		status_new = battemp_protection_get_status(protection_me,
			now_battemp_degc, now_voltage_mv);
	}
	else {
		pr_batprot("temperature and u_voltage are not valid.\n");
		status_new = protection_me->protect_status; // To bypass this work.
	}

	if (protection_me->protect_status == status_new)
		goto reschedule;

	// configure wakelock
	if (status_new == CHARGING_RELEASED_TOTALLY ||
		status_new == CHARGING_RELEASED_PARTIALLY) {
		if (wake_lock_active(&protection_me->battemp_wakelock)) {
			pr_batprot("Releasing wake lock\n");
			wake_unlock(&protection_me->battemp_wakelock);
		}
	}
	else {
		if (!wake_lock_active(&protection_me->battemp_wakelock)) {
			pr_batprot("Acquiring wake lock\n");
			wake_lock(&protection_me->battemp_wakelock);
		}
	}

	pr_batprot("%s -> %s, Battemp=%d, Voltage=%d\n",
		battemp_protection_status_to_string(protection_me->protect_status),
		battemp_protection_status_to_string(status_new),
		now_battemp_degc, now_voltage_mv);

	// limit current here
	limit_voter_set(&protection_me->protect_voter,
		battemp_protection_status_to_limit(status_new));

	// Update member status in 'protection_me'
	protection_me->protect_status = status_new;

reschedule:
	schedule_delayed_work(delayed_work,
		battemp_protection_status_to_period(protection_me));
	return;
}

static int battemp_protection_probe_parameters(struct device_node* dev_node,
	struct battemp_protection* protection_me) {
	int rc = 0;

	OF_PROP_READ_S32(dev_node, protection_me->threashold_degc_upto_normal,
			"threashold-degc-upto-normal", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_degc_upto_warm,
			"threashold-degc-upto-warm", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_degc_upto_hot,
			"threashold-degc-upto-hot", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_degc_downto_warm,
			"threashold-degc-downto-warm", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_degc_downto_normal,
			"threashold-degc-downto-normal", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_degc_downto_cold,
			"threashold-degc-downto-cold", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_mv_upto_critical,
			"threashold-mv-upto-critical", rc);
	OF_PROP_READ_S32(dev_node, protection_me->threashold_mv_downto_safe,
			"threashold-mv-downto-safe", rc);

	OF_PROP_READ_S32(dev_node, protection_me->period_ms_normal,
			"period-ms-normal", rc);
	OF_PROP_READ_S32(dev_node, protection_me->period_ms_decreased,
			"period-ms-decreased", rc);
	OF_PROP_READ_S32(dev_node, protection_me->period_ms_blocked,
			"period-ms-blocked", rc);

	return rc;
}

static int battemp_protection_probe_preset(struct device_node* dev_node,
	struct battemp_protection* protection_me) {
	int rc = 0;

	const char* voter_name;
	int voter_type;

	OF_PROP_READ_STR(dev_node, voter_name, "voter-name", rc);
	OF_PROP_READ_S32(dev_node, voter_type, "voter-type", rc);
	if (rc)
		return rc;

	rc = limit_voter_register(&protection_me->protect_voter, voter_name,
			voter_type, NULL, NULL, NULL);
	if (rc)
		return rc;

	wake_lock_init(&protection_me->battemp_wakelock,
			WAKE_LOCK_SUSPEND, "lge_charging_scenario");

	INIT_DELAYED_WORK(&protection_me->battemp_work,
			battemp_protection_polling);

	return rc;
}

static int battemp_protection_probe(struct platform_device *pdev) {

	struct battemp_protection* protection_me;
	struct device_node *dev_node = pdev->dev.of_node;
	int ret;

	pr_batprot("Start\n");

	protection_me = kzalloc(sizeof(struct battemp_protection), GFP_KERNEL);
	if (!protection_me) {
		pr_err("Failed to alloc memory\n");
		goto error;
	}

	ret = battemp_protection_probe_parameters(dev_node, protection_me);
	if (ret < 0) {
		pr_err("Fail to parse parameters\n");
		goto err_hw_init;
	}

	ret = battemp_protection_probe_preset(dev_node, protection_me);
	if (ret < 0) {
		pr_err("Fail to preset structure\n");
		goto err_hw_init;
	}

	platform_set_drvdata(pdev, protection_me);
	protection_me->protect_status = CHARGING_RELEASED_UNDEFINED;
	battemp_protection_polling(&protection_me->battemp_work.work);

	pr_batprot("Complete probing\n");
	return 0;

err_hw_init:
	kfree(protection_me);

error:
	pr_err("Failed to probe\n");

	return ret;
}

static int battemp_protection_remove(struct platform_device *pdev) {
	struct battemp_protection *voter_battemp = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	kfree(voter_battemp);
	return 0;
}

static const struct of_device_id battemp_protection_match [] = {
	{ .compatible = BATTEMP_PROTECTION_COMPATIBLE },
	{ },
};

static const struct platform_device_id battemp_protection_id [] = {
	{ BATTEMP_PROTECTION_DRIVER, 0 },
	{ },
};

static struct platform_driver battemp_protection_driver = {
	.driver = {
		.name = BATTEMP_PROTECTION_DRIVER,
		.owner = THIS_MODULE,
		.of_match_table = battemp_protection_match,
	},
	.probe = battemp_protection_probe,
	.remove = battemp_protection_remove,
	.id_table = battemp_protection_id,
};

static int __init battemp_protection_init(void) {
	return platform_driver_register(&battemp_protection_driver);
}

static void __exit battemp_protection_exit(void) {
	platform_driver_unregister(&battemp_protection_driver);
}

module_init(battemp_protection_init);
module_exit(battemp_protection_exit);

MODULE_DESCRIPTION(BATTEMP_PROTECTION_DRIVER);
MODULE_LICENSE("GPL v2");

