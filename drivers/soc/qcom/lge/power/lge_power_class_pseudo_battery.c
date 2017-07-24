/* driver/soc/qcom/lge/power/lge_power_class_pseudo_battery.c
 *
 * LGE Pseudo Battery Driver.
 *
 * Copyright (C) 2015 LGE
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "[LG_PB] %s : " fmt, __func__

#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <soc/qcom/lge/power/lge_power_class.h>

struct lge_pseudo_battery {
	struct lge_power lge_pb_lpc;
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};

static char *pb_supplied_to[] = {
	"battery",
	"rt5058-charger",
};

static char *pb_lge_supplied_to[] = {
	"lge_cable_detect",
	"lge_batt_id",
	"lge_cc",
};

static enum lge_power_property lge_power_lge_pb_properties[] = {
	LGE_POWER_PROP_PSEUDO_BATT,
	LGE_POWER_PROPS_PSEUDO_BATT_MODE,
	LGE_POWER_PROPS_PSEUDO_BATT_ID,
	LGE_POWER_PROPS_PSEUDO_BATT_THERM,
	LGE_POWER_PROPS_PSEUDO_BATT_TEMP,
	LGE_POWER_PROPS_PSEUDO_BATT_VOLT,
	LGE_POWER_PROPS_PSEUDO_BATT_CAPACITY,
	LGE_POWER_PROPS_PSEUDO_BATT_CHARGING,
};

static int
lge_power_lge_pb_property_is_writeable(struct lge_power *lpc,
				enum lge_power_property lpp)
{
	int ret = 0;
	switch (lpp) {
	case LGE_POWER_PROP_PSEUDO_BATT:
	case LGE_POWER_PROPS_PSEUDO_BATT_MODE:
	case LGE_POWER_PROPS_PSEUDO_BATT_ID:
	case LGE_POWER_PROPS_PSEUDO_BATT_THERM:
	case LGE_POWER_PROPS_PSEUDO_BATT_TEMP:
	case LGE_POWER_PROPS_PSEUDO_BATT_VOLT:
	case LGE_POWER_PROPS_PSEUDO_BATT_CAPACITY:
	case LGE_POWER_PROPS_PSEUDO_BATT_CHARGING:
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

static int lge_power_lge_pb_set_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			const union lge_power_propval *val)
{
	int ret_val = 0;
	struct lge_pseudo_battery *chip
			= container_of(lpc,	struct lge_pseudo_battery, lge_pb_lpc);

	switch (lpp) {
	case LGE_POWER_PROP_PSEUDO_BATT:
		chip->mode = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_MODE:
		chip->mode = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_ID:
		chip->id = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_THERM:
		chip->therm = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_TEMP:
		chip->temp = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_VOLT:
		chip->volt = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_CAPACITY:
		chip->capacity = val->intval;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_CHARGING:
		chip->charging = val->intval;
		break;
	default:
		pr_info("Invalid pseudo battery property value(%d)\n", (int)lpp);
		ret_val = -EINVAL;
		break;
	}
	lge_power_changed(&chip->lge_pb_lpc);
	return ret_val;
}

static int lge_power_lge_pb_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;

	struct lge_pseudo_battery *chip
			= container_of(lpc, struct lge_pseudo_battery, lge_pb_lpc);
	switch (lpp) {
	case LGE_POWER_PROP_PSEUDO_BATT:
		val->intval = chip->mode;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_MODE:
		val->intval = chip->mode;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_ID:
		val->intval = chip->id;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_THERM:
		val->intval = chip->therm;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_TEMP:
		val->intval = chip->temp;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_VOLT:
		val->intval = chip->volt;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_CAPACITY:
		val->intval = chip->capacity;
		break;
	case LGE_POWER_PROPS_PSEUDO_BATT_CHARGING:
		val->intval = chip->charging;
		break;
	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}

#ifdef CONFIG_OF
static void lge_pb_of_property_read(struct platform_device *pdev, struct lge_power *lpc)
{
	int ret = 0;
	struct lge_pseudo_battery *chip
			= container_of(lpc,	struct lge_pseudo_battery, lge_pb_lpc);

	ret = of_property_read_u32(pdev->dev.of_node, "lge,id",
			&chip->id);
	if (ret < 0) {
		pr_err("id cannot be get\n");
		chip->id = 1;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "lge,therm",
			&chip->therm);
	if (ret < 0) {
		pr_err("therm cannot be get\n");
		chip->therm = 100;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "lge,temp",
			&chip->temp);
	if (ret < 0) {
		pr_err("temp cannot be get\n");
		chip->temp = 40;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "lge,volt",
			&chip->volt);
	if (ret < 0) {
		pr_err("volt cannot be get\n");
		chip->volt = 4100000;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "lge,capacity",
			&chip->capacity);
	if (ret < 0) {
		pr_err("capacity cannot be get\n");
		chip->capacity = 80;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "lge,charging",
			&chip->charging);
	if (ret < 0) {
		pr_err("charging cannot be get\n");
		chip->charging = 1;
	}
}
#endif

static int lge_pseudo_battery_probe(struct platform_device *pdev)
{
	struct lge_pseudo_battery *chip;
	struct lge_power *lge_power_pb;
	int ret;
	pr_info("LG Pseudo Battery probe Start~!!\n");
	chip = kzalloc(sizeof(struct lge_pseudo_battery), GFP_KERNEL);

	if(!chip){
		pr_err("lge_pseudo_battery memory allocation failed.\n");
		return -ENOMEM;
	}

	chip->mode = 0;

	lge_power_pb = &chip->lge_pb_lpc;
	lge_power_pb->name = "pseudo_battery";
#ifdef CONFIG_OF
	lge_pb_of_property_read(pdev, &chip->lge_pb_lpc);
#endif
	lge_power_pb->properties = lge_power_lge_pb_properties;
	lge_power_pb->num_properties
		= ARRAY_SIZE(lge_power_lge_pb_properties);
	lge_power_pb->get_property
		= lge_power_lge_pb_get_property;
	lge_power_pb->set_property
		= lge_power_lge_pb_set_property;
	lge_power_pb->property_is_writeable
		= lge_power_lge_pb_property_is_writeable;
	lge_power_pb->supplied_to = pb_supplied_to;
	lge_power_pb->num_supplicants	= ARRAY_SIZE(pb_supplied_to);
	lge_power_pb->lge_supplied_to = pb_lge_supplied_to;
	lge_power_pb->num_lge_supplicants
		= ARRAY_SIZE(pb_lge_supplied_to);
	ret = lge_power_register(&pdev->dev, lge_power_pb);
	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}
	pr_info("LG pseudo battery mode probe done~!!\n");
	return 0;
err_free:
	kfree(chip);
	return ret;
}

static int lge_pseudo_battery_remove(struct platform_device *pdev)
{
	struct lge_pseudo_battery *chip = platform_get_drvdata(pdev);

	lge_power_unregister(&chip->lge_pb_lpc);
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id lge_pseudo_battery_match_table[] = {
	{.compatible = "lge,pseudo_battery"},
	{},
};
#endif
static struct platform_driver pseudo_battery_driver = {
	.probe  = lge_pseudo_battery_probe,
	.remove = lge_pseudo_battery_remove,
	.driver = {
		.name = "pseudo_battery",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lge_pseudo_battery_match_table,
#endif
	},
};

static int __init lge_pseudo_battery_init(void)
{
	int rc;
	rc = platform_driver_register(&pseudo_battery_driver);

	return rc;
}

static void __exit lge_pseudo_battery_exit(void)
{
	platform_driver_unregister(&pseudo_battery_driver);
}

module_init(lge_pseudo_battery_init);
module_exit(lge_pseudo_battery_exit);

MODULE_DESCRIPTION("LGE Pseudo Battery driver");
MODULE_LICENSE("GPL");
