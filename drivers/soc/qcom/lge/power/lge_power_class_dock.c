/* arch/arm/mach-msm/lge/lge_dock.c
 *
 * LGE Dock Driver.
 *
 * Copyright (C) 2013 LGE
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

#define pr_fmt(fmt) "[LGE-DOCK] %s : " fmt, __func__

#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <soc/qcom/lge/power/lge_power_class.h>
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
#include <soc/qcom/lge/power/lge_cable_detect.h>
#endif

enum {
	EXTRA_DOCK_STATE_UNDOCKED = 0,
	EXTRA_DOCK_STATE_DESK = 1,
	EXTRA_DOCK_STATE_CAR = 2,
	EXTRA_DOCK_STATE_LE_DESK = 3,
	EXTRA_DOCK_STATE_HE_DESK = 4
};

static DEFINE_MUTEX(dock_lock);
//static bool dock_state;

static struct switch_dev dockdev = {
	.name = "dock",
};
struct lge_dock{
	struct lge_power lge_dock_lpc;
	struct lge_power *lge_cd_lpc;
	bool dock_state;
	cable_adc_type cable_type;
	int chg_type;
};

static int check_dock_cable_type(struct lge_dock *chip)
{
	pr_debug("entered check_dock_cable_type");
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
	if (chip->cable_type == CABLE_ADC_330K) {
		pr_debug("dock_state true");
		chip->dock_state = true;
	} else
#endif
	{
		pr_debug("dock_state false");
		chip->dock_state = false;
	}
	return 0;
}

static void check_dock_connected(struct lge_dock *chip)
{
	if (check_dock_cable_type(chip) < 0)
		pr_err("can't read adc!\n");

	if ((chip->dock_state) && chip->chg_type) {
		switch_set_state(&dockdev, EXTRA_DOCK_STATE_DESK);
		pr_info("desk dock\n");
	} else {
		switch_set_state(&dockdev, EXTRA_DOCK_STATE_UNDOCKED);
		pr_debug("undocked\n");
	}
	mutex_unlock(&dock_lock);
}

static void lge_dock_external_lge_power_changed(struct lge_power *lpc)
{
	int rc = 0;
	union lge_power_propval lge_val = {0,};
	struct lge_dock *chip
			= container_of(lpc,
					struct lge_dock, lge_dock_lpc);
	chip->lge_cd_lpc = lge_power_get_by_name("lge_cable_detect");
	if (!chip->lge_cd_lpc) {
		pr_err("cable_detect is not yet ready\n");
	} else {
		rc = chip->lge_cd_lpc->get_property(chip->lge_cd_lpc,
				LGE_POWER_PROP_CABLE_TYPE, &lge_val);
		pr_info("cable_type : %d\n", lge_val.intval);

		chip->cable_type = lge_val.intval;
		rc = chip->lge_cd_lpc->get_property(chip->lge_cd_lpc,
				LGE_POWER_PROP_TYPE, &lge_val);
		pr_info("type : %d\n", lge_val.intval);
		chip->chg_type = lge_val.intval;
		check_dock_connected(chip);
	}

}

static enum lge_power_property lge_power_lge_dock_properties[] = {
	LGE_POWER_PROP_TYPE,
	LGE_POWER_PROP_STATUS,
};
static int lge_power_lge_dock_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;

	struct lge_dock *chip
			= container_of(lpc,
					struct lge_dock, lge_dock_lpc);
	switch (lpp) {
	case LGE_POWER_PROP_TYPE:
		val->intval = chip->chg_type;
		break;

	case LGE_POWER_PROP_STATUS:
		val->intval = chip->dock_state;
		break;

	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}
static int lge_dock_probe(struct platform_device *pdev)
{
	struct lge_dock *chip;
	struct lge_power *lge_power_dock;
	int ret;
	pr_info("LG DOCK probe Start~!!\n");
	chip = kzalloc(sizeof(struct lge_dock), GFP_KERNEL);

	if(!chip){
		pr_err("lge_dock memory allocation failed.\n");
		return -ENOMEM;
	}
	lge_power_dock = &chip->lge_dock_lpc;
	lge_power_dock->name = "lge_dock";

	lge_power_dock->properties = lge_power_lge_dock_properties;
	lge_power_dock->num_properties
		= ARRAY_SIZE(lge_power_lge_dock_properties);
	lge_power_dock->get_property
		= lge_power_lge_dock_get_property;
	lge_power_dock->external_lge_power_changed
		= lge_dock_external_lge_power_changed;
	ret = lge_power_register(&pdev->dev, lge_power_dock);
	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}

	chip->dock_state = false;
	chip->chg_type = 0;
	chip->cable_type = 0;
	pr_info("LG DOCK probe done~!!\n");
	return 0;
err_free:
	kfree(chip);
	return ret;
}

static int lge_dock_remove(struct platform_device *pdev)
{
	struct lge_dock *chip = platform_get_drvdata(pdev);

	lge_power_unregister(&chip->lge_dock_lpc);
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id lge_dock_match_table[] = {
	{.compatible = "lge,dock"},
	{},
};
#endif
static struct platform_driver dock_driver = {
	.probe  = lge_dock_probe,
	.remove = lge_dock_remove,
	.driver = {
		.name = "lge_dock",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lge_dock_match_table,
#endif
	},
};

static int __init lge_dock_init(void)
{
	int rc;
	rc = platform_driver_register(&dock_driver);
	if (switch_dev_register(&dockdev) < 0) {
		pr_err("failed to register dock driver.\n");
		rc = -ENODEV;
	}
	return rc;
}

static void __exit lge_dock_exit(void)
{
	switch_dev_unregister(&dockdev);
	platform_driver_unregister(&dock_driver);
}

module_init(lge_dock_init);
module_exit(lge_dock_exit);

MODULE_DESCRIPTION("LGE dock driver");
MODULE_LICENSE("GPL");
