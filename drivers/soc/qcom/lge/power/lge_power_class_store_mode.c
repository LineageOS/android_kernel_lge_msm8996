/* driver/soc/qcom/lge/power/lge_power_class_store_mode.c
 *
 * LGE Store demo mode Driver.
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

#define pr_fmt(fmt) "[LGE-SM] %s : " fmt, __func__

#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
#include <soc/qcom/lge/power/lge_power_class.h>
#include <soc/qcom/lge/power/lge_cable_detect.h>
#endif
#include <linux/of.h>
#include <linux/of_device.h>

struct lge_store_mode {
	struct lge_power lge_sm_lpc;
	struct lge_power *lge_cd_lpc;
	struct power_supply *batt_psy;
	int store_demo_enabled;
	int charging_enable;
	int chg_present;
	int capacity;
	int llk_max_thr_soc;
	int llk_min_thr_soc;
	int iusb_enable;
#ifdef CONFIG_LGE_PM_ONLY_STORE_MODE
	int default_set;
#endif
	struct work_struct llk_work;
};

static void lge_sm_llk_work(struct work_struct *work)
{
	struct lge_store_mode *chip = container_of(work, struct lge_store_mode,
						llk_work);
	int prev_chg_enable = chip->charging_enable;
	int prev_iusb_enable = chip->iusb_enable;

	if (chip->chg_present) {
		if (chip->capacity > chip->llk_max_thr_soc) {
			chip->iusb_enable = 0;
		} else {
			chip->iusb_enable = 1;
		}
		if (chip->capacity >= chip->llk_max_thr_soc) {
			chip->charging_enable = 0;
			pr_info("Stop charging by LLK_mode.\n");
		}
		if (chip->capacity < chip->llk_min_thr_soc) {
			chip->charging_enable = 1;
			pr_info("Start Charging by LLK_mode.\n");
		}
		if ((chip->charging_enable != prev_chg_enable)
			|| (chip->iusb_enable != prev_iusb_enable)) {
			pr_info("lge_power_changed in LLK_mode.\n");
			lge_power_changed(&chip->lge_sm_lpc);
		}
	}
}

static char *sm_supplied_from[] = {
	"battery",
};

static char *sm_lge_supplied_from[] = {
	"lge_cable_detect",
};

static char *sm_supplied_to[] = {
	"battery",
};

static void lge_sm_external_power_changed(struct lge_power *lpc)
{
	int rc = 0;

	union power_supply_propval ret = {0,};
	struct lge_store_mode *chip
			= container_of(lpc,
					struct lge_store_mode, lge_sm_lpc);

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy) {
		pr_err("battery is not yet ready\n");
	} else {
#ifdef CONFIG_LGE_PM_ONLY_STORE_MODE
		if (chip->default_set)
			chip->store_demo_enabled = chip->default_set;
#endif
		if (chip->store_demo_enabled == 1) {
			rc = chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CAPACITY, &ret);
			if (rc) {
				pr_err ("cannot get capacity!\n");
			} else {
				chip->capacity = ret.intval;
				pr_info("capacity : %d\n", chip->capacity);
				schedule_work(&chip->llk_work);
			}
		}
	}
}

static void lge_sm_external_lge_power_changed(struct lge_power *lpc)
{
	int rc = 0;
	union lge_power_propval lge_val = {0,};
	struct lge_store_mode *chip
			= container_of(lpc,
					struct lge_store_mode, lge_sm_lpc);

	if (!chip->lge_cd_lpc)
		chip->lge_cd_lpc = lge_power_get_by_name("lge_cable_detect");
	if (!chip->lge_cd_lpc) {
		pr_err("cable_detect is not yet ready\n");
	} else {
		rc = chip->lge_cd_lpc->get_property(chip->lge_cd_lpc,
				LGE_POWER_PROP_CHG_PRESENT, &lge_val);
		chip->chg_present = lge_val.intval;
	}
}

static enum lge_power_property lge_power_lge_sm_properties[] = {
	LGE_POWER_PROP_STORE_DEMO_ENABLED,
	LGE_POWER_PROP_CHARGING_ENABLED,
	LGE_POWER_PROP_USB_CHARGING_ENABLED,
};

static int
lge_power_lge_sm_property_is_writeable(struct lge_power *lpc,
				enum lge_power_property lpp)
{
	int ret = 0;
	switch (lpp) {
	case LGE_POWER_PROP_STORE_DEMO_ENABLED:
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

static int lge_power_lge_sm_set_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			const union lge_power_propval *val)
{
	int ret_val = 0;
	struct lge_store_mode *chip
			= container_of(lpc,	struct lge_store_mode, lge_sm_lpc);

	switch (lpp) {
	case LGE_POWER_PROP_STORE_DEMO_ENABLED:
		chip->store_demo_enabled = val->intval;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_SIMPLE
		if (chip->store_demo_enabled == 0)
			chip->charging_enable = 1;
#endif
		break;

	default:
		pr_info("Invalid Store demo mode property value(%d)\n", (int)lpp);
		ret_val = -EINVAL;
		break;
	}
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_SIMPLE
	lge_power_changed(&chip->lge_sm_lpc);
#endif
	return ret_val;
}

static int lge_power_lge_sm_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;

	struct lge_store_mode *chip
			= container_of(lpc, struct lge_store_mode, lge_sm_lpc);
	switch (lpp) {
	case LGE_POWER_PROP_STORE_DEMO_ENABLED:
		val->intval = chip->store_demo_enabled;
		break;

	case LGE_POWER_PROP_CHARGING_ENABLED:
		val->intval = chip->charging_enable;
		break;

	case LGE_POWER_PROP_USB_CHARGING_ENABLED:
		val->intval = chip->iusb_enable;
		break;

	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}
static int lge_store_mode_probe(struct platform_device *pdev)
{
	struct lge_store_mode *chip;
	struct lge_power *lge_power_sm;
	int ret;
	pr_info("LG Store mode probe Start~!!\n");
	chip = kzalloc(sizeof(struct lge_store_mode), GFP_KERNEL);

	if(!chip){
		pr_err("lge_store_mode memory allocation failed.\n");
		return -ENOMEM;
	}

	chip->charging_enable = 1;
	chip->store_demo_enabled = 0;
	lge_power_sm = &chip->lge_sm_lpc;
	lge_power_sm->name = "lge_sm";
	chip->iusb_enable = 1;

#ifdef CONFIG_LGE_PM_ONLY_STORE_MODE
	ret = of_property_read_u32(pdev->dev.of_node, "lge,default-set",
			&chip->default_set);
	if (ret < 0) {
		pr_err("Failed to get default-set value\n");
		chip->default_set = 0;
	}
#endif
	ret = of_property_read_u32(pdev->dev.of_node, "lge,llk_max",
			&chip->llk_max_thr_soc);
	if (ret < 0) {
		pr_err("llk_max_thr_soc cannot be get\n");
		chip->llk_max_thr_soc = 50;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "lge,llk_min",
			&chip->llk_min_thr_soc);
	if (ret < 0) {
		pr_err("llk_min_thr_soc cannot be get\n");
		chip->llk_min_thr_soc = 45;
	}

	lge_power_sm->properties = lge_power_lge_sm_properties;
	lge_power_sm->num_properties
		= ARRAY_SIZE(lge_power_lge_sm_properties);
	lge_power_sm->get_property
		= lge_power_lge_sm_get_property;
	lge_power_sm->set_property
		= lge_power_lge_sm_set_property;
	lge_power_sm->property_is_writeable
		= lge_power_lge_sm_property_is_writeable;
	lge_power_sm->supplied_to = sm_supplied_to;
	lge_power_sm->num_supplicants	= ARRAY_SIZE(sm_supplied_to);
	lge_power_sm->lge_supplied_from = sm_lge_supplied_from;
	lge_power_sm->supplied_from = sm_supplied_from;
	lge_power_sm->num_supplies	= ARRAY_SIZE(sm_supplied_from);
	lge_power_sm->lge_supplied_from = sm_lge_supplied_from;
	lge_power_sm->num_lge_supplies	= ARRAY_SIZE(sm_lge_supplied_from);
	lge_power_sm->external_lge_power_changed
		= lge_sm_external_lge_power_changed;
	lge_power_sm->external_power_changed
		= lge_sm_external_power_changed;
	ret = lge_power_register(&pdev->dev, lge_power_sm);
	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}
	INIT_WORK(&chip->llk_work, lge_sm_llk_work);
	pr_info("LG store mode probe done~!!\n");

	return 0;
err_free:
	kfree(chip);
	return ret;
}

static int lge_store_mode_remove(struct platform_device *pdev)
{
	struct lge_store_mode *chip = platform_get_drvdata(pdev);

	lge_power_unregister(&chip->lge_sm_lpc);
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id lge_store_mode_match_table[] = {
	{.compatible = "lge,store_mode"},
	{},
};
#endif
static struct platform_driver store_mode_driver = {
	.probe  = lge_store_mode_probe,
	.remove = lge_store_mode_remove,
	.driver = {
		.name = "lge_store_mode",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lge_store_mode_match_table,
#endif
	},
};

static int __init lge_store_mode_init(void)
{
	int rc;
	rc = platform_driver_register(&store_mode_driver);

	return rc;
}

static void __exit lge_store_mode_exit(void)
{
	platform_driver_unregister(&store_mode_driver);
}

module_init(lge_store_mode_init);
module_exit(lge_store_mode_exit);

MODULE_DESCRIPTION("LGE Store demo mode driver");
MODULE_LICENSE("GPL");
