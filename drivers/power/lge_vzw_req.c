/* LGE VZW requirement Driver.
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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <soc/qcom/lge/power/lge_power_class.h>
#include <linux/of.h>
#include <linux/of_device.h>

struct lge_vzw_req {
	struct device 		*dev;
	struct power_supply vzw_require;
	struct lge_power *lge_vzw_lpc;
};

static char *vzw_req_lge_supplied_from[] = {
	"lge_vzw",
};

static void lge_vzw_external_lge_power_changed(struct power_supply *psy)
{
	power_supply_changed(psy);
}

static enum power_supply_property lge_power_vzw_properties[] = {
	POWER_SUPPLY_PROP_VZW_CHG,
};

static int lge_power_vzw_get_property(struct power_supply *lpc,
			enum power_supply_property lpp,
			union power_supply_propval *val)
{
	int ret_val = 0;

	struct lge_vzw_req *vzw_req
			= container_of(lpc, struct lge_vzw_req, vzw_require);

	union lge_power_propval lge_val = {0,};

	if (!vzw_req->lge_vzw_lpc)
		vzw_req->lge_vzw_lpc = lge_power_get_by_name("lge_vzw");

	if (!vzw_req->lge_vzw_lpc)
		return -EINVAL;

	switch (lpp) {
	case POWER_SUPPLY_PROP_VZW_CHG:
		ret_val = vzw_req->lge_vzw_lpc->get_property(vzw_req->lge_vzw_lpc,
					LGE_POWER_PROP_VZW_CHG, &lge_val);
		val->intval = lge_val.intval;
		break;
	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}

static int lge_vzw_probe(struct platform_device *pdev)
{
	struct lge_vzw_req *vzw_req;
	struct power_supply *lge_power_vzw;
	int ret;

	pr_info("[LGE-VZW] LG VZW requirement probe Start~!!\n");

	vzw_req = kzalloc(sizeof(struct lge_vzw_req), GFP_KERNEL);
	if(!vzw_req){
		pr_err("lge_vzw_req memory allocation failed.\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, vzw_req);
	vzw_req->dev = &pdev->dev;

	lge_power_vzw = &vzw_req->vzw_require;
	lge_power_vzw->name = "lgcc";

	lge_power_vzw->properties = lge_power_vzw_properties;
	lge_power_vzw->num_properties
		= ARRAY_SIZE(lge_power_vzw_properties);
	lge_power_vzw->lge_power_supplied_from = vzw_req_lge_supplied_from;
	lge_power_vzw->num_lge_power_supplies =
		ARRAY_SIZE(vzw_req_lge_supplied_from);
	lge_power_vzw->get_property
		= lge_power_vzw_get_property;
	lge_power_vzw->external_lge_power_changed
		= lge_vzw_external_lge_power_changed;

	ret = power_supply_register(vzw_req->dev, lge_power_vzw);
	if (ret < 0) {
		pr_err("[LGCC-VZW] Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}

	pr_info("[LGCC-VZW] LG VZW probe done~!!\n");

	return 0;

err_free:
	power_supply_unregister(&vzw_req->vzw_require);
	kfree(vzw_req);
	return ret;
}

static struct of_device_id lge_vzw_req_match_table[] = {
	{	.compatible 		= "lge,vzw_req_property",},
	{ },
};

static int lge_vzw_remove(struct platform_device *pdev)
{
	struct lge_vzw_req *vzw_req = platform_get_drvdata(pdev);

	power_supply_unregister(&vzw_req->vzw_require);

	platform_set_drvdata(pdev, NULL);
	kfree(vzw_req);
	return 0;
}

static struct platform_driver vzw_driver = {
	.probe  = lge_vzw_probe,
	.remove = lge_vzw_remove,
	.driver = {
		.name = "lge_vzw_require_driver",
		.owner = THIS_MODULE,
		.of_match_table = lge_vzw_req_match_table,
	},
};

static int __init lge_vzw_init(void)
{
	return platform_driver_register(&vzw_driver);
}

static void __exit lge_vzw_exit(void)
{
	platform_driver_unregister(&vzw_driver);
}

module_init(lge_vzw_init);
module_exit(lge_vzw_exit);

MODULE_DESCRIPTION("LGE VZW requirement driver");
MODULE_LICENSE("GPL");
