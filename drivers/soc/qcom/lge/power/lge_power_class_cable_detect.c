/*
 *  Copyright (C) 2014, YongK Kim <yongk.kim@lge.com>
 *  Driver for cable detect
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *
 */
#define pr_fmt(fmt) "[LGE-CD] %s : " fmt, __func__

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <soc/qcom/lge/power/lge_power_class.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <soc/qcom/lge/power/lge_cable_detect.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
#include <linux/power/lge_battery_id.h>
#endif

#define MODULE_NAME		"lge_cable_detector"
#define DRIVER_DESC		"Cable detect driver"
#define DRIVER_AUTHOR	"yongk.kim@lge.com"
#define DRIVER_VERSION	"1.0"

#define MAX_CABLE_NUM	15
#define DEFAULT_USB_VAL 1200001
#define CABLE_DETECT_DELAY	msecs_to_jiffies(250)

#define CURRENT_900MA	900

struct cable_info_table {
	int 			threshhold_low;
	int 			threshhold_high;
	cable_adc_type 	type;
	unsigned int 	ta_ma;
	unsigned int 	usb_ma;
	unsigned int 	ibat_ma;
	unsigned int 	qc_ibat_ma;
	struct list_head list;
};


struct cable_detect {
	struct device 		*dev;
	struct lge_power 	lge_cd_lpc;
	struct lge_power 	*lge_adc_lpc;
	struct lge_power 	*lge_cc_lpc;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
	struct lge_power 	*lge_batt_id_lpc;
#endif
	int 		usb_adc_val;
	int 		cable_type;
	int 		cable_type_boot;
	int 		usb_current;
	int 		ta_current;
	int 		is_updated;
	int 		modified_usb_ma;
	int 		chg_present;
	int 		chg_type;
	int 		floated_charger;
	int 		chg_enable;
	int 		chg_done;
	int 		chg_usb_enable;
	int 		ibat_current;
	int 		modified_ibat_ma;
	int 		is_factory_cable;
	int 		is_factory_cable_boot;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
	int 		is_hvdcp_present;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
	int 		ctype_present;
#endif
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
	int 		batt_present;
#endif
	int 		default_adc;
	int 		qc_ibat_current;
	int 		pif_boot;
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	int 		usb_current_max_mode;
	int 		usb_max_mode_current;
#endif
	struct delayed_work cable_detect_work;
	struct list_head 	cable_data_list;
};

static cable_boot_type boot_cable_type = NONE_INIT_CABLE;

static int check_factory_cable(int cable_type, bool runTime)
{
	if (runTime) {
		if (cable_type == CABLE_ADC_56K ||
			cable_type == CABLE_ADC_130K ||
			cable_type == CABLE_ADC_910K)
			return FACTORY_CABLE;
		else
			return NORMAL_CABLE;
	} else {
		if (cable_type == LT_CABLE_56K ||
			cable_type == LT_CABLE_130K ||
			cable_type == LT_CABLE_910K)
			return FACTORY_CABLE;
		else
			return NORMAL_CABLE;
	}
}
static int __cable_detect_get_usb_id_adc(struct cable_detect *cd)
{
	int rc = 0;
	union lge_power_propval lge_val = {0,};

	rc = cd->lge_adc_lpc->get_property(cd->lge_adc_lpc,
			LGE_POWER_PROP_USB_ID_PHY, &lge_val);
	if (rc == 0)
		cd->usb_adc_val = (int)lge_val.int64val;
	else
		cd->usb_adc_val = cd->default_adc;

	return rc;
}

static int cable_detect_get_usb_id_adc(struct cable_detect *cd)
{
	__cable_detect_get_usb_id_adc(cd);

	return cd->usb_adc_val;
}

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
static int lge_check_battery_present(struct cable_detect *cd) {
	union lge_power_propval lge_val = {0,};
	int batt_present = 0;
	int rc;

	if (!cd->lge_batt_id_lpc)
		cd->lge_batt_id_lpc = lge_power_get_by_name("lge_batt_id");
	if (!cd->lge_batt_id_lpc) {
		pr_err("battery id is not probed! Set no battery\n");
		batt_present = 0;
	} else {
		rc = cd->lge_batt_id_lpc->get_property(
				cd->lge_batt_id_lpc,
				LGE_POWER_PROP_PRESENT, &lge_val);
		if (rc == 0)
			batt_present = lge_val.intval;
		else
			batt_present = 0;
	}

	return batt_present;
}

static int lge_check_battery_id_info(struct cable_detect *cd) {
	union lge_power_propval lge_val = {0,};
	int batt_id = 0;
	int rc;

	if (!cd->lge_batt_id_lpc)
		cd->lge_batt_id_lpc = lge_power_get_by_name("lge_batt_id");
	if (!cd->lge_batt_id_lpc) {
		pr_err("battery id is not probed! Set no battery\n");
		batt_id = 0;
	} else {
		rc = cd->lge_batt_id_lpc->get_property(
				cd->lge_batt_id_lpc,
				LGE_POWER_PROP_BATTERY_ID_CHECKER, &lge_val);
		if (rc == 0)
			batt_id = lge_val.intval;
		else
			batt_id = 0;
	}

	return batt_id;
}
#endif

#define FACTORY_BOOT_CURRENT 1500
#define FACTORY_BOOT_IBAT 500
static int cable_detect_read_cable_info(struct cable_detect *cd)
{
	int adc = 0;
	static int before_cable_info;
	struct cable_info_table *cable_info_table;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
	int batt_present = lge_check_battery_present(cd);

	if (cd->is_factory_cable_boot && !cd->lge_adc_lpc && !batt_present) {
#else
	if (cd->is_factory_cable_boot && !cd->lge_adc_lpc) {
#endif
		cd->ta_current = FACTORY_BOOT_CURRENT;
		cd->usb_current = FACTORY_BOOT_CURRENT;
		cd->ibat_current = FACTORY_BOOT_IBAT;

		cd->qc_ibat_current = FACTORY_BOOT_IBAT;
		if (cd->cable_type_boot == LT_CABLE_56K)
			cd->cable_type = CABLE_ADC_56K;
		else if (cd->cable_type_boot == LT_CABLE_130K)
			cd->cable_type = CABLE_ADC_130K;
		else if (cd->cable_type_boot == LT_CABLE_910K)
			cd->cable_type = CABLE_ADC_910K;

		pr_err("iusb %d, ibat %d\n", cd->usb_current,
						cd->ibat_current);
		return cd->cable_type;
	}

	adc = cable_detect_get_usb_id_adc(cd);
	list_for_each_entry(cable_info_table, &cd->cable_data_list, list) {
	if (adc >= cable_info_table->threshhold_low &&
			adc <= cable_info_table->threshhold_high) {
		cd->usb_current = cable_info_table->usb_ma;
		cd->ta_current = cable_info_table->ta_ma;
		cd->ibat_current = cable_info_table->ibat_ma;
		cd->qc_ibat_current = cable_info_table->qc_ibat_ma;
		cd->is_factory_cable = check_factory_cable(cable_info_table->type, true);
		if (before_cable_info != cable_info_table->type) {
			pr_info("adc = %d\n", adc);
			pr_info("cable info --> %d\n", cable_info_table->type);
		}
		before_cable_info = cable_info_table->type;

		return cable_info_table->type;
		}
	}

	return -EINVAL;
}

static void lge_cable_detect_work(struct work_struct *work){
	struct cable_detect *cd =
			container_of(work, struct cable_detect,
					cable_detect_work.work);
	if (cd->cable_type != cable_detect_read_cable_info(cd))
		lge_power_changed(&cd->lge_cd_lpc);

	cd->cable_type = cable_detect_read_cable_info(cd);
}

static char *lge_power_cable_detect_supplied_from[] = {
	"ac",
	"usb",
	"usb_pd",
};

static char *lge_power_cable_detect_supplied_to[] = {
	"lge_cc",
	"lge_batt_id",
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_DOCK
	"lge_dock",
#endif
};

static char *cd_supplied_to[] = {
	"battery",
};

static char *lge_cd_supplied_to[] = {
	"battery",
	"bms",
};

static enum
lge_power_property lge_power_cable_detect_properties[] = {
	LGE_POWER_PROP_IS_FACTORY_CABLE,
	LGE_POWER_PROP_IS_FACTORY_MODE_BOOT,
	LGE_POWER_PROP_CABLE_TYPE,
	LGE_POWER_PROP_CABLE_TYPE_BOOT,
	LGE_POWER_PROP_USB_CURRENT,
	LGE_POWER_PROP_TA_CURRENT,
	LGE_POWER_PROP_UPDATE_CABLE_INFO,
	LGE_POWER_PROP_CHG_PRESENT,
	LGE_POWER_PROP_CURRENT_MAX,
	LGE_POWER_PROP_TYPE,
	LGE_POWER_PROP_FLOATED_CHARGER,
	LGE_POWER_PROP_CHARGING_ENABLED,
	LGE_POWER_PROP_CHARGING_USB_ENABLED,
	LGE_POWER_PROP_CHARGING_CURRENT_MAX,
	LGE_POWER_PROP_IBAT_CURRENT,
	LGE_POWER_PROP_QC_IBAT_CURRENT,
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
	LGE_POWER_PROP_HVDCP_PRESENT,
#endif
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	LGE_POWER_PROP_USB_CURRENT_MAX_MODE,
#endif
	LGE_POWER_PROP_CHARGE_DONE,
};

static int
lge_power_cable_detect_property_is_writeable(struct lge_power *lpc,
		enum lge_power_property lpp)
{
	int ret = 0;

	switch (lpp) {
	case LGE_POWER_PROP_UPDATE_CABLE_INFO:
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	case LGE_POWER_PROP_USB_CURRENT_MAX_MODE:
#endif
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

static int
lge_power_lge_cable_detect_set_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			const union lge_power_propval *val)
{
	union power_supply_propval ret = {0,};
	int ret_val = 0;
	struct cable_detect *cd
				= container_of(lpc, struct cable_detect,
					lge_cd_lpc);

	switch (lpp) {
	case LGE_POWER_PROP_UPDATE_CABLE_INFO:
		if (val->intval == 1) {
			cd->cable_type = cable_detect_read_cable_info(cd);
			if (ret_val < 0)
				ret_val = -EINVAL;
			else
				cd->is_updated = 1;
		}
		break;

#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	case LGE_POWER_PROP_USB_CURRENT_MAX_MODE:
		cd->usb_current_max_mode = val->intval;
		if (cd->usb_current_max_mode) {
			cd->modified_usb_ma = cd->usb_max_mode_current*1000;
		} else {
			if (cd->chg_type == POWER_SUPPLY_TYPE_USB) {
				cd->modified_usb_ma = cd->usb_current*1000;
			} else if (cd->chg_type == POWER_SUPPLY_TYPE_USB_DCP) {
				cd->modified_usb_ma = cd->ta_current*1000;
			} else if (cd->chg_type
					== POWER_SUPPLY_TYPE_USB_HVDCP) {
				cd->modified_usb_ma = cd->ta_current*1000;
			} else {
				cd->usb_psy->get_property(
					cd->usb_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
				cd->modified_usb_ma = ret.intval;
			}

		}
		lge_power_changed(&cd->lge_cd_lpc);
		break;
#endif

	case LGE_POWER_PROP_CHARGE_DONE:
		cd->chg_done = val->intval;
		break;

	default:
		pr_info("Invalid cable detect property value(%d)\n", (int)lpp);
		ret_val = -EINVAL;
		break;
	}
	return ret_val;
}

static int
lge_power_lge_cable_detect_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;
	struct cable_detect *cd
			= container_of(lpc, struct cable_detect, lge_cd_lpc);
	int batt_id = lge_check_battery_id_info(cd);

	switch (lpp) {
	case LGE_POWER_PROP_IS_FACTORY_CABLE:
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
		if (!(cd->is_factory_cable_boot &&
				!lge_check_battery_present(cd)) ||
				!(cd->is_factory_cable_boot &&
				batt_id == BATT_ID_UNKNOWN)) {
#endif
			cd->is_factory_cable = cd->cable_type
				== CABLE_ADC_56K ? FACTORY_CABLE :
				cd->cable_type == CABLE_ADC_130K ? FACTORY_CABLE :
				cd->cable_type == CABLE_ADC_910K ? FACTORY_CABLE :
				NORMAL_CABLE;
			if ((cd->chg_present == 0) && (cd->is_factory_cable ==1))
				cd->is_factory_cable = 0;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
		}
#endif
		val->intval = cd->is_factory_cable;
		break;

	case LGE_POWER_PROP_IS_FACTORY_MODE_BOOT:
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
		if (!(cd->is_factory_cable_boot &&
				!lge_check_battery_present(cd)) ||
				!(cd->is_factory_cable_boot &&
				batt_id == BATT_ID_UNKNOWN)) {
#endif
		cd->is_factory_cable_boot = cd->cable_type_boot
			== LT_CABLE_56K ? FACTORY_CABLE :
			cd->cable_type_boot == LT_CABLE_130K ? FACTORY_CABLE :
			cd->cable_type_boot == LT_CABLE_910K ? FACTORY_CABLE :
			NORMAL_CABLE;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
		}
#endif
		val->intval = cd->is_factory_cable_boot;
		break;

	case LGE_POWER_PROP_CABLE_TYPE:
		if ((cd->chg_present == 0) && (cd->is_factory_cable ==1))
			cd->cable_type = CABLE_ADC_NONE;
		val->intval = cd->cable_type;
		break;

	case LGE_POWER_PROP_CABLE_TYPE_BOOT:
		val->intval = cd->cable_type_boot;
		break;

	case LGE_POWER_PROP_USB_CURRENT:
		val->intval = cd->usb_current;
		break;

	case LGE_POWER_PROP_TA_CURRENT:
		val->intval = cd->ta_current;
		break;

	case LGE_POWER_PROP_IBAT_CURRENT:
		val->intval = cd->ibat_current;
		break;

	case LGE_POWER_PROP_UPDATE_CABLE_INFO:
		val->strval = "W_ONLY";
		break;

	case LGE_POWER_PROP_CURRENT_MAX:
		val->intval = cd->modified_usb_ma;
		break;

	case LGE_POWER_PROP_CHG_PRESENT:
		val->intval = cd->chg_present;
		break;

	case LGE_POWER_PROP_TYPE:
		val->intval = cd->chg_type;
		break;

	case LGE_POWER_PROP_FLOATED_CHARGER:
		val->intval = cd->floated_charger;
		break;

	case LGE_POWER_PROP_CHARGING_USB_ENABLED:
		if (cd->chg_usb_enable < 0)
			ret_val = -EINVAL;
		else
			val->intval = cd->chg_usb_enable;
		break;
	case LGE_POWER_PROP_CHARGING_ENABLED:
		val->intval = cd->chg_enable;
		break;

	case LGE_POWER_PROP_QC_IBAT_CURRENT:
		val->intval = cd->qc_ibat_current;
		break;

	case LGE_POWER_PROP_CHARGING_CURRENT_MAX:
		val->intval = cd->modified_ibat_ma;
		break;

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
	case LGE_POWER_PROP_HVDCP_PRESENT:
		val->intval = cd->is_hvdcp_present;
		break;
#endif

#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	case LGE_POWER_PROP_USB_CURRENT_MAX_MODE:
		val->intval = cd->usb_current_max_mode;
		break;
#endif

	default:
		pr_err("Invalid cable detect property value(%d)\n",
			(int)lpp);
		ret_val = -EINVAL;
		break;
	}
	return ret_val;
}

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
static int lge_cable_detect_is_56k_910k(struct cable_detect *cd)
{
	if (cd->cable_type == CABLE_ADC_56K
		|| cd->cable_type == CABLE_ADC_910K
		|| cd->cable_type_boot == LT_CABLE_56K
		|| cd->cable_type_boot == LT_CABLE_910K)
		return 1;
	else
		return 0;
}
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
#define CTYPE_PD_MAX 	3000
#endif
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
#define HVDCP_IUSB_MAX 3000
#define HVDCP_IUSB_MIN 2000
#define HVDCP_IBAT_MIN 2000
#endif
static void lge_cable_detect_external_power_changed(struct lge_power *lpc)
{
	union power_supply_propval ret = {0,};
	union lge_power_propval lge_val = {0,};
	struct cable_detect *cd =
		container_of(lpc, struct cable_detect, lge_cd_lpc);
	int rc = 0;
	static int prev_chg_present = 0;
	static int before_iusb;
	static int before_ibat;
	static int before_chg_type;
	int is_changed = 0;

	/* whenever power_supply_changed is called, adc should be read.*/
	cd->cable_type = cable_detect_read_cable_info(cd);
	cd->usb_psy = power_supply_get_by_name("usb");
	if(!cd->usb_psy){
		pr_err("[LGE-CD] usb power_supply is not probed yet!!!\n");
	} else {
		rc = cd->usb_psy->get_property(
				cd->usb_psy, POWER_SUPPLY_PROP_TYPE, &ret);
		cd->chg_type = ret.intval;

		if (cd->is_factory_cable) {
			cd->modified_usb_ma = cd->usb_current * 1000;
			cd->modified_ibat_ma = cd->ibat_current * 1000;
			cd->floated_charger = 0;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
			cd->is_hvdcp_present = 0;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
			cd->ctype_present = 0;
#endif
		} else if (cd->chg_type == POWER_SUPPLY_TYPE_USB) {
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
			if (cd->usb_current_max_mode)
				cd->modified_usb_ma =
					cd->usb_max_mode_current * 1000;
			else
#endif
				cd->modified_usb_ma = cd->usb_current * 1000;
			cd->modified_ibat_ma = cd->usb_current * 1000;
			if (cd->floated_charger != cd->usb_psy->is_floated_charger)
				is_changed = 1;
			cd->floated_charger = cd->usb_psy->is_floated_charger;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
			cd->is_hvdcp_present = 0;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
			cd->ctype_present = 0;
#endif
		} else if (cd->chg_type == POWER_SUPPLY_TYPE_USB_DCP) {
			cd->modified_usb_ma = cd->ta_current * 1000;
			cd->modified_ibat_ma = cd->ibat_current * 1000;
			cd->floated_charger = 0;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
			cd->is_hvdcp_present = 0;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
			cd->ctype_present = 0;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
		} else if (cd->chg_type == POWER_SUPPLY_TYPE_CTYPE ||
				cd->chg_type == POWER_SUPPLY_TYPE_CTYPE_PD) {
			if (cd->chg_type == POWER_SUPPLY_TYPE_CTYPE) {
				cd->modified_usb_ma = cd->ta_current * 1000;
				cd->modified_ibat_ma = cd->ibat_current * 1000;
			} else if (cd->chg_type == POWER_SUPPLY_TYPE_CTYPE_PD) {
				cd->modified_usb_ma = cd->ta_current * 1000;
				cd->modified_ibat_ma = CTYPE_PD_MAX * 1000;
			} else {
				cd->modified_usb_ma = cd->usb_current * 1000;
				cd->modified_ibat_ma = cd->ibat_current * 1000;
			}
			cd->floated_charger = 0;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
			cd->is_hvdcp_present = 0;
#endif
			cd->ctype_present = 1;
#endif
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
		} else if (cd->chg_type == POWER_SUPPLY_TYPE_USB_HVDCP ||
				cd->chg_type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
			if (cd->chg_type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
				cd->modified_usb_ma = HVDCP_IUSB_MAX * 1000;
				cd->modified_ibat_ma = cd->qc_ibat_current * 1000;
			} else if (cd->chg_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
				cd->modified_usb_ma = HVDCP_IUSB_MIN * 1000;
				cd->modified_ibat_ma = cd->qc_ibat_current * 1000;
			}
			cd->is_hvdcp_present = 1;
			cd->floated_charger = 0;
#ifdef CONFIG_LGE_USB_TYPE_C
			cd->ctype_present = 0;
#endif
#endif
		} else {
			cd->usb_psy->get_property(
				cd->usb_psy, POWER_SUPPLY_PROP_CURRENT_MAX,
				&ret);
			cd->modified_usb_ma = ret.intval;
			cd->modified_ibat_ma = ret.intval;
			cd->floated_charger = 0;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
			cd->is_hvdcp_present = 0;
			cd->chg_type = 0;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
			cd->ctype_present = 0;
#endif
		}
		rc = cd->usb_psy->get_property(
				cd->usb_psy, POWER_SUPPLY_PROP_PRESENT, &ret);
		cd->chg_present = ret.intval;
		if (cd->chg_present != prev_chg_present) {
			schedule_delayed_work(&cd->cable_detect_work,
					CABLE_DETECT_DELAY);
			is_changed = 1;
			pr_err("chg_present %d, prev %d\n",
					cd->chg_present, prev_chg_present);
		}
		prev_chg_present = cd->chg_present;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
		if (!cd->lge_batt_id_lpc) {
			cd->lge_batt_id_lpc = lge_power_get_by_name("lge_batt_id");
			pr_err("battery id is not probed!!!\n");
		} else {
			cd->batt_present = lge_check_battery_present(cd);
			if ((lge_cable_detect_is_56k_910k(cd) == 1)
					&& (cd->batt_present == 0))
				cd->chg_enable = 0;
			else
				cd->chg_enable = 1;
		}
#endif
		rc = cd->usb_psy->get_property(cd->usb_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &ret);
		if (rc < 0) {
			cd->chg_usb_enable = -1;
		} else {
			cd->chg_usb_enable = ret.intval;
		}
	}
	if (!cd->lge_cc_lpc)
		cd->lge_cc_lpc = lge_power_get_by_name("lge_cc");
	if (cd->lge_cc_lpc) {
		if ((cd->is_hvdcp_present || cd->ctype_present)
				&& cd->lge_cc_lpc) {
			lge_val.intval = cd->chg_type;
			cd->lge_cc_lpc->set_property(cd->lge_cc_lpc,
					LGE_POWER_PROP_TYPE, &lge_val);
		}
	}

	if ((before_iusb != cd->modified_usb_ma) ||
				(before_ibat != cd->modified_ibat_ma) ||
				(before_chg_type != cd->chg_type) ||
				is_changed) {
		pr_err("ibat %d, iusb %d\n",
					cd->modified_ibat_ma, cd->modified_usb_ma);
		is_changed = 0;
		lge_power_changed(&cd->lge_cd_lpc);
	}

	before_iusb = cd->modified_usb_ma;
	before_ibat = cd->modified_ibat_ma;
	before_chg_type = cd->chg_type;
}

static void get_cable_data_from_dt(struct cable_detect *cd)
{
	int i;
	u32 cable_value[6];
	int rc = 0;
	struct device_node *node_temp = cd->dev->of_node;
	const char *propname[MAX_CABLE_NUM] = {
		"lge,no-init-cable",
		"lge,cable-mhl-1k",
		"lge,cable-u-28p7k",
		"lge,cable-28p7k",
		"lge,cable-56k",
		"lge,cable-100k",
		"lge,cable-130k",
		"lge,cable-180k",
		"lge,cable-200k",
		"lge,cable-220k",
		"lge,cable-270k",
		"lge,cable-330k",
		"lge,cable-620k",
		"lge,cable-910k",
		"lge,cable-none"
	};
	for (i = 0 ; i < MAX_CABLE_NUM ; i++) {
		struct cable_info_table *cable_info_table;
		cable_info_table = kzalloc(sizeof(struct cable_info_table),
							GFP_KERNEL);
		of_property_read_u32_array(node_temp, propname[i],
							cable_value, 6);
		cable_info_table->threshhold_low = cable_value[0];
		cable_info_table->threshhold_high = cable_value[1];
		cable_info_table->type = i;
		cable_info_table->ta_ma = cable_value[2];
		cable_info_table->usb_ma = cable_value[3];
		cable_info_table->ibat_ma = cable_value[4];
		cable_info_table->qc_ibat_ma = cable_value[5];
		if (i == (MAX_CABLE_NUM - 1))
			cd->default_adc = cable_info_table->threshhold_high - 2;
		list_add_tail(&cable_info_table->list, &cd->cable_data_list);
	}

#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	rc = of_property_read_u32(node_temp, "lge,usb_max_mode_current",
						&cd->usb_max_mode_current);
	if (rc){
		pr_err("lge,usb_max_mode_current is not defined\n");
		cd->usb_max_mode_current = CURRENT_900MA;
	}
#endif
}

static void
cable_detect_kfree_cable_info_table(struct cable_detect *cd)
{
	struct cable_info_table *cable_info_table, *n;

	list_for_each_entry_safe(cable_info_table, n, \
			&cd->cable_data_list, list) {
		list_del(&cable_info_table->list);
		kfree(cable_info_table);
	}
}


static int cable_detect_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct  lge_power *lge_power_cd;
	struct cable_detect *cd =
		kzalloc(sizeof(struct cable_detect), GFP_KERNEL);
	int batt_id = 0;

	pr_info("lge_cable_detect probe start!\n");

	cd->dev = &pdev->dev;
	cd->cable_type = 0;
	cd->cable_type_boot = boot_cable_type;
	cd->modified_usb_ma = 0;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
	cd->is_hvdcp_present = 0;
#endif
	platform_set_drvdata(pdev, cd);

	INIT_LIST_HEAD(&cd->cable_data_list);

	get_cable_data_from_dt(cd);

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
	cd->lge_batt_id_lpc = lge_power_get_by_name("lge_batt_id");
	if (!cd->lge_batt_id_lpc) {
		pr_err("battery id is not probed!!!\n");
	} else
		cd->batt_present = lge_check_battery_present(cd);
	batt_id = lge_check_battery_id_info(cd);
#endif
	cd->lge_adc_lpc = lge_power_get_by_name("lge_adc");
	if (!cd->lge_adc_lpc) {
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
		if ((cd->cable_type_boot == LT_CABLE_56K ||
					cd->cable_type_boot == LT_CABLE_130K ||
					cd->cable_type_boot == LT_CABLE_910K) &&
				(!cd->batt_present || batt_id == BATT_ID_UNKNOWN)) {
#else
		if (cd->cable_type_boot == LT_CABLE_56K ||
					cd->cable_type_boot == LT_CABLE_130K ||
					cd->cable_type_boot == LT_CABLE_910K) {
#endif

			pr_err("Skip to progress getting lge_adc by PIF\n");
			cd->is_factory_cable_boot = 1;
			cd->is_factory_cable = cd->is_factory_cable_boot;
		} else {
			pr_err("lge_adc property failed to get\n");
			ret = -EPROBE_DEFER;
			goto error;
		}
	}

	lge_power_cd = &cd->lge_cd_lpc;
	lge_power_cd->name = "lge_cable_detect";
	lge_power_cd->properties = lge_power_cable_detect_properties;
	lge_power_cd->num_properties
			= ARRAY_SIZE(lge_power_cable_detect_properties);
	lge_power_cd->set_property
			= lge_power_lge_cable_detect_set_property;
	lge_power_cd->property_is_writeable
			= lge_power_cable_detect_property_is_writeable;
	lge_power_cd->get_property
			= lge_power_lge_cable_detect_get_property;
	lge_power_cd->supplied_from
			= lge_power_cable_detect_supplied_from;
	lge_power_cd->num_supplies
			= ARRAY_SIZE(lge_power_cable_detect_supplied_from);
	lge_power_cd->external_power_changed
			= lge_cable_detect_external_power_changed;
	lge_power_cd->lge_supplied_to
			= lge_power_cable_detect_supplied_to;
	lge_power_cd->num_lge_supplicants
			= ARRAY_SIZE(lge_power_cable_detect_supplied_to);
	lge_power_cd->lge_psy_supplied_to = cd_supplied_to;
	lge_power_cd->num_lge_psy_supplicants
			= ARRAY_SIZE(cd_supplied_to);
	lge_power_cd->supplied_to = lge_cd_supplied_to;
	lge_power_cd->num_supplicants = ARRAY_SIZE(lge_cd_supplied_to);

	ret = lge_power_register(cd->dev, lge_power_cd);
	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
				ret);
		goto error;
	}

	cd->cable_type = cable_detect_read_cable_info(cd);
	cd->chg_present = 0;
	cd->floated_charger = 0;
	cd->chg_usb_enable = -1;
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	cd->usb_current_max_mode = 0;
#endif
	INIT_DELAYED_WORK(&cd->cable_detect_work, lge_cable_detect_work);

	pr_info("cable_detect probe end!\n");

	return ret;

error:
	cable_detect_kfree_cable_info_table(cd);
	kfree(cd);
	return ret;
}

static int cable_detect_remove(struct platform_device *pdev)
{
	struct cable_detect *cd = platform_get_drvdata(pdev);
	cable_detect_kfree_cable_info_table(cd);
	cancel_delayed_work_sync(&cd->cable_detect_work);
	kfree(cd);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id cable_detect_match_table[] = {
	{ .compatible = "lge,cable-detect" },
	{ },
};
#endif

static struct platform_driver cable_detect_device_driver = {
	.probe = cable_detect_probe,
	.remove = cable_detect_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cable_detect_match_table,
#endif
	},
};

static int __init cable_detect_init(void)
{
	return platform_driver_register(&cable_detect_device_driver);
}

static void __exit cable_detect_exit(void)
{
	platform_driver_unregister(&cable_detect_device_driver);
}

module_init(cable_detect_init);
module_exit(cable_detect_exit);

static int __init boot_cable_setup(char *boot_cable)
{
	if (!strcmp(boot_cable, "LT_56K"))
		boot_cable_type = LT_CABLE_56K;
	else if (!strcmp(boot_cable, "LT_130K"))
		boot_cable_type = LT_CABLE_130K;
	else if (!strcmp(boot_cable, "400MA"))
		boot_cable_type = USB_CABLE_400MA;
	else if (!strcmp(boot_cable, "DTC_500MA"))
		boot_cable_type = USB_CABLE_DTC_500MA;
	else if (!strcmp(boot_cable, "Abnormal_400MA"))
		boot_cable_type = ABNORMAL_USB_CABLE_400MA;
	else if (!strcmp(boot_cable, "LT_910K"))
		boot_cable_type = LT_CABLE_910K;
	else if (!strcmp(boot_cable, "NO_INIT"))
		boot_cable_type = NONE_INIT_CABLE;
	else
		boot_cable_type = NONE_INIT_CABLE;

	pr_info("Boot cable : %s %d\n", boot_cable, boot_cable_type);

	return 1;
}
__setup("bootcable.type=", boot_cable_setup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);
