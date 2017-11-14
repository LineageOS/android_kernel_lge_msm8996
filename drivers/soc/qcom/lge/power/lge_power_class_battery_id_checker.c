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

#define pr_fmt(fmt) "[LGE-BATT-ID] %s : " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/string.h>

#include <linux/power/lge_battery_id.h>
#include <soc/qcom/lge/power/lge_power_class.h>

#define MODULE_NAME "lge_battery_id_check"

static int lge_battery_info = BATT_ID_UNKNOWN;

static char *batt_cell[] = {
		"LGC",
		"Tocad",
};

struct lge_battery_id{
	struct device 		*dev;
	struct lge_power lge_batt_id_lpc;
	struct lge_power *lge_cd_lpc;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_PSEUDO_BATTERY
	struct lge_power *lge_pb_lpc;
	int pseudo_batt;
	int pseudo_batt_id;
#endif
	int battery_id_info;
	int is_factory_cable;
	enum cell_type batt_cell_no;
};
#ifdef CONFIG_OF
const char *pack_name, *batt_capacity;
#endif

char *batt_info;

static char *make_battery_info(struct lge_battery_id *battery_id) {
	if (batt_info == NULL)
		batt_info = kmalloc(30, GFP_KERNEL);

	sprintf(batt_info, "LGE_%s_%s_%smAh",pack_name,
		batt_cell[battery_id->batt_cell_no], batt_capacity);
	return batt_info;
}

static void set_lge_batt_cell_no(struct lge_battery_id *battery_id) {

	switch(battery_id->battery_id_info) {
		case BATT_ID_RA4301_VC1:
		case BATT_ID_SW3800_VC0:
			battery_id->batt_cell_no = LGC_LLL;
			break;

		case BATT_ID_RA4301_VC0:
		case BATT_ID_SW3800_VC1:
			battery_id->batt_cell_no = TCD_AAC;
			break;
		default:
			battery_id->batt_cell_no = LGC_LLL;
			break;
	}
}

static bool is_lge_batt_valid(struct lge_battery_id *battery_id)
{
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
	if (battery_id->is_factory_cable == 1)
		return true;
#endif
#ifdef CONFIG_LGE_PM_EMBEDDED_BATTERY
	return true;
#endif

	if (battery_id->battery_id_info == BATT_ID_DS2704_N ||
			battery_id->battery_id_info == BATT_ID_DS2704_L ||
			battery_id->battery_id_info == BATT_ID_DS2704_C ||
			battery_id->battery_id_info == BATT_ID_ISL6296_N ||
			battery_id->battery_id_info == BATT_ID_ISL6296_L ||
			battery_id->battery_id_info == BATT_ID_ISL6296_C ||
			battery_id->battery_id_info == BATT_ID_RA4301_VC0 ||
			battery_id->battery_id_info == BATT_ID_RA4301_VC1 ||
			battery_id->battery_id_info == BATT_ID_RA4301_VC2 ||
			battery_id->battery_id_info == BATT_ID_SW3800_VC0 ||
			battery_id->battery_id_info == BATT_ID_SW3800_VC1 ||
			battery_id->battery_id_info == BATT_ID_SW3800_VC2)
		return true;

	return false;
}

static int read_lge_battery_id(struct lge_battery_id *battery_id)
{
	return battery_id->battery_id_info;
}

static bool get_prop_batt_id_for_aat(struct lge_battery_id *battery_id)
{
	static int check_batt_id;
	if (read_lge_battery_id(battery_id))
		check_batt_id = 1;
	else
		check_batt_id = 0;
	return check_batt_id;
}

static enum lge_power_property lge_power_lge_batt_id_properties[] = {
	LGE_POWER_PROP_BATTERY_ID_CHECKER,
	LGE_POWER_PROP_VALID_BATT,
	LGE_POWER_PROP_CHECK_BATT_ID_FOR_AAT,
	LGE_POWER_PROP_TYPE,
	LGE_POWER_PROP_BATT_PACK_NAME,
	LGE_POWER_PROP_BATT_CAPACITY,
	LGE_POWER_PROP_BATT_CELL,
	LGE_POWER_PROP_PRESENT,
	LGE_POWER_PROP_BATT_INFO,
};
static enum lge_power_property
lge_power_lge_batt_id_uevent_properties[] = {
	LGE_POWER_PROP_VALID_BATT,
};
static int lge_power_lge_batt_id_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;

	struct lge_battery_id *battery_id
			= container_of(lpc,
					struct lge_battery_id, lge_batt_id_lpc);
	switch (lpp) {
	case LGE_POWER_PROP_BATTERY_ID_CHECKER:
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_PSEUDO_BATTERY
		if (battery_id->pseudo_batt == 1)
			val->intval = battery_id->pseudo_batt_id;
		else
#endif
		val->intval = read_lge_battery_id(battery_id);
		break;
	case LGE_POWER_PROP_VALID_BATT:
		val->intval = is_lge_batt_valid(battery_id);
		break;
	case LGE_POWER_PROP_CHECK_BATT_ID_FOR_AAT:
		val->intval = get_prop_batt_id_for_aat(battery_id);
		break;
	case LGE_POWER_PROP_TYPE:
		val->intval = 0;
		break;
	case LGE_POWER_PROP_BATT_PACK_NAME:
		val->strval = pack_name;
		break;
	case LGE_POWER_PROP_BATT_CAPACITY:
		val->strval = batt_capacity;
		break;
	case LGE_POWER_PROP_BATT_CELL:
		val->strval = batt_cell[battery_id->batt_cell_no];
		break;
	case LGE_POWER_PROP_PRESENT:
		if (lge_battery_info == BATT_NOT_PRESENT)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case LGE_POWER_PROP_BATT_INFO:
		val->strval = make_battery_info(battery_id);
		break;

	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}

static void lge_batt_id_external_lge_power_changed(struct lge_power *lpc)
{
	int rc = 0;
	union lge_power_propval lge_val = {0,};
	struct lge_battery_id *battery_id
			= container_of(lpc,
					struct lge_battery_id, lge_batt_id_lpc);
	battery_id->lge_cd_lpc = lge_power_get_by_name("lge_cable_detect");
	if (!battery_id->lge_cd_lpc) {
		pr_err("%s : cable_detect is not yet ready\n", __func__);
	} else {
		rc = battery_id->lge_cd_lpc->get_property(battery_id->lge_cd_lpc,
				LGE_POWER_PROP_IS_FACTORY_CABLE, &lge_val);
		if (lge_val.intval)
			pr_info("factory cable : %d\n", lge_val.intval);

		battery_id->is_factory_cable = lge_val.intval;
	}

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_PSEUDO_BATTERY
	if (!battery_id->lge_pb_lpc) {
		battery_id->lge_pb_lpc = lge_power_get_by_name("pseudo_battery");
		if (!battery_id->lge_pb_lpc) {
			pr_err("%s : cable_detect is not yet ready\n", __func__);
		} else {
			rc = battery_id->lge_pb_lpc->get_property(battery_id->lge_pb_lpc,
					LGE_POWER_PROP_PSEUDO_BATT, &lge_val);
			pr_info("pseudo battery : %d\n", lge_val.intval);
			battery_id->pseudo_batt = lge_val.intval;
			if (battery_id->pseudo_batt == 1) {
				rc = battery_id->lge_pb_lpc->get_property(battery_id->lge_pb_lpc,
						LGE_POWER_PROPS_PSEUDO_BATT_ID, &lge_val);
				battery_id->pseudo_batt_id = lge_val.intval;
			}
		}

	}
#endif
}

static int lge_battery_id_probe(struct platform_device *pdev)
{
	struct lge_battery_id *battery_id;
	struct lge_power *lge_power_batt_id;
	int ret;
	pr_info("LG Battery ID probe Start~!!\n");
	battery_id = kzalloc(sizeof(struct lge_battery_id), GFP_KERNEL);

	if(!battery_id){
		pr_err("lge_battery_id memory allocation failed.\n");
		return -ENOMEM;
	}

	battery_id->dev = &pdev->dev;
	battery_id->battery_id_info = lge_battery_info;
	pr_info("Battery info : %d\n", battery_id->battery_id_info);

#ifdef CONFIG_OF
	ret = of_property_read_string(pdev->dev.of_node, "lge,pack_name",
						&pack_name);
	if (ret < 0) {
		pr_err("Pack name cannot be get\n");
		pack_name = "BL44E1F";
	}

	ret = of_property_read_string(pdev->dev.of_node, "lge,batt_capacity",
						&batt_capacity);
	if (ret < 0) {
		pr_err("battery capacity cannot be get\n");
		batt_capacity = "3200";
	}

	ret = of_property_read_u32(pdev->dev.of_node, "lge,batt_cell_no",
                                       &battery_id->batt_cell_no);
	if (ret < 0) {
#ifdef CONFIG_LGE_PM_EMBEDDED_BATTERY
		pr_err("[LGE-BID] lge,batt_cell_no cannot be get\n");
#endif
		set_lge_batt_cell_no(battery_id);
	}
#endif

	battery_id->is_factory_cable = 0;
	lge_power_batt_id = &battery_id->lge_batt_id_lpc;

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_PSEUDO_BATTERY
	battery_id->pseudo_batt = 0;
#endif
	lge_power_batt_id->name = "lge_batt_id";

	lge_power_batt_id->properties = lge_power_lge_batt_id_properties;
	lge_power_batt_id->num_properties
		= ARRAY_SIZE(lge_power_lge_batt_id_properties);
	lge_power_batt_id->get_property
		= lge_power_lge_batt_id_get_property;
	lge_power_batt_id->external_lge_power_changed
		= lge_batt_id_external_lge_power_changed;
	lge_power_batt_id->uevent_properties
		= lge_power_lge_batt_id_uevent_properties;
	lge_power_batt_id->num_uevent_properties
		= ARRAY_SIZE(lge_power_lge_batt_id_uevent_properties);

	ret = lge_power_register(battery_id->dev, lge_power_batt_id);
	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}

	pr_info("LG Battery ID probe done~!!\n");

	return 0;
err_free:
	kfree(battery_id);
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id lge_battery_id_match_table[] = {
	{.compatible = "lge,batt_id"},
	{},
};
#endif
static int lge_battery_id_remove(struct platform_device *pdev)
{
	struct lge_battery_id *battery_id = platform_get_drvdata(pdev);

	lge_power_unregister(&battery_id->lge_batt_id_lpc);

	platform_set_drvdata(pdev, NULL);
	kfree(battery_id);
	return 0;
}

static struct platform_driver lge_battery_id_driver = {
	.probe = lge_battery_id_probe,
	.remove = lge_battery_id_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = lge_battery_id_match_table,
#endif
	},
};

static int __init lge_battery_id_init(void)
{
	return platform_driver_register(&lge_battery_id_driver);
}

static void lge_battery_id_exit(void)
{
	platform_driver_unregister(&lge_battery_id_driver);
}

fs_initcall(lge_battery_id_init);
module_exit(lge_battery_id_exit);

int lge_is_battery_present(void)
{
	if (lge_battery_info == BATT_NOT_PRESENT)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(lge_is_battery_present);
static int __init battery_information_setup(char *batt_info)
{
	if (!strcmp(batt_info, "DS2704_N"))
		lge_battery_info = BATT_ID_DS2704_N;
	else if (!strcmp(batt_info, "DS2704_L"))
		lge_battery_info = BATT_ID_DS2704_L;
	else if (!strcmp(batt_info, "DS2704_C"))
		lge_battery_info = BATT_ID_DS2704_C;
	else if (!strcmp(batt_info, "ISL6296_N"))
		lge_battery_info = BATT_ID_ISL6296_N;
	else if (!strcmp(batt_info, "ISL6296_L"))
		lge_battery_info = BATT_ID_ISL6296_L;
	else if (!strcmp(batt_info, "ISL6296_C"))
		lge_battery_info = BATT_ID_ISL6296_C;
	else if (!strcmp(batt_info, "RA4301_VC0"))
		lge_battery_info = BATT_ID_RA4301_VC0;
	else if (!strcmp(batt_info, "RA4301_VC1"))
		lge_battery_info = BATT_ID_RA4301_VC1;
	else if (!strcmp(batt_info, "RA4301_VC2"))
		lge_battery_info = BATT_ID_RA4301_VC2;
	else if (!strcmp(batt_info, "SW3800_VC0"))
		lge_battery_info = BATT_ID_SW3800_VC0;
	else if (!strcmp(batt_info, "SW3800_VC1"))
		lge_battery_info = BATT_ID_SW3800_VC1;
	else if (!strcmp(batt_info, "SW3800_VC2"))
		lge_battery_info = BATT_ID_SW3800_VC2;
	else if (!strcmp(batt_info, "MISSED"))
		lge_battery_info = BATT_NOT_PRESENT;
	else
		lge_battery_info = BATT_ID_UNKNOWN;

	pr_info("Battery : %s %d\n", batt_info, lge_battery_info);

	return 1;
}

__setup("lge.battid=", battery_information_setup);

MODULE_DESCRIPTION("LGE power monitor class");
MODULE_AUTHOR("Daeho Choi <daeho.choi@lge.com>");
MODULE_LICENSE("GPL");
