/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "[LGE-UEVENT] %s : " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/string.h>
#include <linux/power_supply.h>
#include <soc/qcom/lge/power/lge_power_class.h>

#define MODULE_NAME "lge_uevent"


struct lge_uevent{
	struct lge_power lge_uevent_lpc;
};


struct uevent_offset {
	enum power_supply_property psp;
	int offset;
};

static struct uevent_offset uevent_offset_info[] = {
	{POWER_SUPPLY_PROP_VOLTAGE_NOW   ,   50000}, // batt, usb / batt, input volt(uv)
	{POWER_SUPPLY_PROP_CAPACITY      ,       1}, // batt      / SOC(%)
	{POWER_SUPPLY_PROP_CURRENT_NOW   ,  200000}, // batt, bms / current(ua)
	{POWER_SUPPLY_PROP_TEMP          ,      10}, // batt      / temp(.x)
	{POWER_SUPPLY_PROP_VOLTAGE_OCV   ,   50000}, // bms       / OCV(uv)
	{POWER_SUPPLY_PROP_RESISTANCE_NOW,      11}, // bms       / RESISTANCE(mohm)
};



static int
power_supply_is_check_uevent(enum power_supply_property psp)
{
	int i = 0;
	int num_offset = ARRAY_SIZE(uevent_offset_info);
	for (i = 0; i < num_offset; i++) {
		if (psp == uevent_offset_info[i].psp)
			return i;
	}
	return -1;
}

static int update_power_supply_property(struct power_supply *psy)
{
	int ret = 0;
	int i =0;
	int update_event = 0;
	int check_psp = 0;
	int diff = 0;
	union power_supply_propval val = {0,};

	if (psy->property_data == NULL)
		return 1;
	for (i = 0; i < psy->num_properties; i++) {
		check_psp = power_supply_is_check_uevent(psy->properties[i]);
		if (psy->get_property(psy, psy->properties[i], &val))
				return 0;
		diff = psy->property_data[i] - val.intval;
		diff = abs(diff);
		if (check_psp >= 0) {
			if (diff >= uevent_offset_info[check_psp].offset) {
				pr_debug("property: %d is changed\n", i);
				psy->property_data[i] = val.intval;
				update_event = 1;
			} else {
				pr_debug("prop:%d is changed within offset\n", i);
				update_event = 0;
			}
		} else {
			if (diff >0) {
				psy->property_data[i] = val.intval;
				update_event = 1;
			} else {
				update_event = 0;
			}
		}
		ret = ret | update_event;
	}
	return ret;
}

static char *lge_power_uevent_supplied_from[] = {
	"battery",
	"usb",
	"bms",
	"fuelgauge",
};

static void
lge_uevent_external_power_changed_with_psy(struct lge_power *lpc,
										struct power_supply *psy)
{
	psy->update_uevent = 0;
	psy->update_uevent = update_power_supply_property(psy);
}

static enum lge_power_property lge_power_lge_uevent_properties[] = {
	LGE_POWER_PROP_STATUS,
};

static int lge_power_lge_uevent_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;

	switch (lpp) {
	case LGE_POWER_PROP_STATUS:
		val->intval = 1;
		break;

	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}


static int lge_uevent_probe(struct platform_device *pdev)
{
	struct lge_uevent *luevent;
	struct lge_power *lge_power_uevent;
	int ret;
	pr_info("LG UEVENT probe Start~!!\n");
	luevent = kzalloc(sizeof(struct lge_uevent), GFP_KERNEL);

	if(!luevent){
		pr_err("luevent memory allocation failed.\n");
		return -ENOMEM;
	}


	lge_power_uevent = &luevent->lge_uevent_lpc;

	lge_power_uevent->name = "uevent_checker";

	lge_power_uevent->properties = lge_power_lge_uevent_properties;
	lge_power_uevent->num_properties
		= ARRAY_SIZE(lge_power_lge_uevent_properties);
	lge_power_uevent->get_property
		= lge_power_lge_uevent_get_property;
	lge_power_uevent->supplied_from
			= lge_power_uevent_supplied_from;
	lge_power_uevent->num_supplies
			= ARRAY_SIZE(lge_power_uevent_supplied_from);
	lge_power_uevent->external_power_changed_with_psy
			= lge_uevent_external_power_changed_with_psy;
	ret = lge_power_register(&pdev->dev, lge_power_uevent);

	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}

	pr_info("LG UEVENT probe done~!!\n");

	return 0;
err_free:
	kfree(luevent);
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id lge_uevent_match_table[] = {
	{.compatible = "lge,uevent"},
	{},
};
#endif
static int lge_uevent_remove(struct platform_device *pdev)
{
	struct lge_uevent *luevent = platform_get_drvdata(pdev);

	lge_power_unregister(&luevent->lge_uevent_lpc);

	platform_set_drvdata(pdev, NULL);
	kfree(luevent);
	return 0;
}

static struct platform_driver lge_uevent_driver = {
	.probe = lge_uevent_probe,
	.remove = lge_uevent_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = lge_uevent_match_table,
#endif
	},
};

static int __init lge_uevent_init(void)
{
	return platform_driver_register(&lge_uevent_driver);
}

static void lge_uevent_exit(void)
{
	platform_driver_unregister(&lge_uevent_driver);
}

fs_initcall(lge_uevent_init);
module_exit(lge_uevent_exit);


MODULE_DESCRIPTION("LGE uevent driver");
MODULE_AUTHOR("Daeho Choi <daeho.choi@lge.com>");
MODULE_LICENSE("GPL");
