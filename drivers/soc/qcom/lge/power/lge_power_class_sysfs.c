/*
 *  Sysfs interface for the universal lg power cloass monitor class
 *
 *  Copyright (C) 2015 Daeho Choi <daeho.choi@lge.com>
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <soc/qcom/lge/power/lge_power_class.h>
#include "lge_power.h"
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#include <soc/qcom/lge/lge_pseudo_batt.h>
#endif


#define LGE_POWER_ATTR(_name)					\
{		\
	.attr = { .name = #_name },					\
	.show = lge_power_show_property,				\
	.store = lge_power_store_property,				\
}

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#define LGE_PSEUDO_BATT_ATTR(_name)					\
{		\
	.attr = { .name = #_name, .mode = 0644},		\
	.show = lge_pseudo_batt_show_property,			\
	.store = lge_pseudo_batt_store_property,		\
}
#endif

static struct device_attribute lge_power_attrs[];

static ssize_t lge_power_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf) {

	static char *cable_text[] = {
		"no-init-cable",
		"cable-mhl-1k",
		"cable-u-28p7k",
		"cable-28p7k",
		"cable-56k",
		"cable-100k",
		"cable-130k",
		"cable-180k",
		"cable-200k",
		"cable-220k",
		"cable-270k",
		"cable-330k",
		"cable-620k",
		"cable-910k",
		"cable-none"
	};

	static char *cable_boot_text[] = {
		"lt-56k",
		"lt-130k",
		"usb-400mA",
		"usb-dtc-500mA",
		"abnormal-usb-400mA",
		"lt-910k",
		"none-init"
	};
	ssize_t ret = 0;
	struct lge_power *lpc = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - lge_power_attrs;
	union lge_power_propval value;


	ret = lpc->get_property(lpc, off, &value);

	if (ret < 0) {
		if (ret == -ENODATA)
			dev_dbg(dev, "driver has no data for `%s' property\n",
					attr->attr.name);
		else if (ret != -ENODEV)
			dev_err(dev, "driver failed to report `%s' property: %zd\n",
					attr->attr.name, ret);
		return ret;
	}

	if (off == LGE_POWER_PROP_CABLE_TYPE) {
		return sprintf(buf, "%s\n", cable_text[value.intval]);
	} else if (off == LGE_POWER_PROP_CABLE_TYPE_BOOT) {
		return sprintf(buf, "%s\n", cable_boot_text[value.intval-6]);
#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	} else if (off == LGE_POWER_PROP_HW_REV) {

		return sprintf(buf, "%s\n", value.strval);
#endif
	} else if (off == LGE_POWER_PROP_BATT_PACK_NAME) {
		return sprintf(buf, "%s\n", value.strval);
	} else if (off == LGE_POWER_PROP_BATT_CAPACITY) {
		return sprintf(buf, "%s\n", value.strval);
	} else if (off == LGE_POWER_PROP_BATT_CELL) {
		return sprintf(buf, "%s\n", value.strval);
	} else if (off == LGE_POWER_PROP_BATT_INFO) {
		return sprintf(buf, "%s\n", value.strval);
	} else if (off == LGE_POWER_PROP_UPDATE_CABLE_INFO) {
		return sprintf(buf, "%s\n", value.strval);
	}
	return sprintf(buf, "%d\n", value.intval);
}

static ssize_t lge_power_store_property(struct device *dev,
		struct device_attribute *attr,
					const char *buf, size_t count) {
	ssize_t ret;
	struct lge_power *lpc = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - lge_power_attrs;
	union lge_power_propval value;
	long long_val;

	/* TODO: support other types than int */
	ret = kstrtol(buf, 10, &long_val);
	if (ret < 0)
		return ret;

	value.intval = long_val;

	ret = lpc->set_property(lpc, off, &value);
	if (ret < 0)
		return ret;

	return count;
}

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
static ssize_t lge_pseudo_batt_show_property(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t ret;
	struct lge_power *lpc = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - lge_power_attrs;
	union lge_power_propval value;

	static char *pseudo_batt[] = {
		"NORMAL", "PSEUDO",
	};

	ret = lpc->get_property(lpc, off, &value);

	if (ret < 0) {
		if (ret != -ENODEV)
			dev_err(dev, "driver failed to report `%s' property\n",
					attr->attr.name);
		return ret;
	}
	if (off == LGE_POWER_PROP_PSEUDO_BATT)
		return sprintf(buf, "[%s]\nusage: echo [mode] [ID] \
			[therm] [temp] [volt] [cap] [charging] \
			> pseudo_batt\n",
				pseudo_batt[value.intval]);

	return 0;
}

static ssize_t lge_pseudo_batt_store_property(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = -EINVAL;
	struct pseudo_batt_info_type info;

	if (sscanf(buf, "%d %d %d %d %d %d %d",
			&info.mode, &info.id, &info.therm,
				&info.temp, &info.volt,
				&info.capacity, &info.charging) != 7) {
		if (info.mode == 1) {
			pr_err("usage : echo [mode] [ID] [therm] \
				[temp] [volt] [cap] [charging] \
				> pseudo_batt");
			goto out;
		}
	}
	set_pseudo_batt_info(&info);
	ret = count;

out:
	return ret;
}
#endif

/* Must be in the same order as POWER_SUPPLY_PROP_* */
static struct device_attribute lge_power_attrs[] = {
	/* Properties of type `int' */
	LGE_POWER_ATTR(status),
	LGE_POWER_ATTR(health),
	LGE_POWER_ATTR(present),
	LGE_POWER_ATTR(charging_enabled),
	LGE_POWER_ATTR(current_max),
	LGE_POWER_ATTR(input_current_max),
	LGE_POWER_ATTR(temp),
	LGE_POWER_ATTR(type),
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
	LGE_PSEUDO_BATT_ATTR(pseudo_batt),
	LGE_POWER_ATTR(pseudo_batt_mode),
	LGE_POWER_ATTR(pseudo_batt_id),
	LGE_POWER_ATTR(pseudo_batt_therm),
	LGE_POWER_ATTR(pseudo_batt_temp),
	LGE_POWER_ATTR(pseudo_batt_volt),
	LGE_POWER_ATTR(pseudo_batt_capacity),
	LGE_POWER_ATTR(pseudo_batt_charging),
#endif
#if defined (CONFIG_LGE_PM_BATTERY_ID_CHECKER) \
		|| defined (CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER)
	LGE_POWER_ATTR(batt_id_check),
	LGE_POWER_ATTR(valid_batt_id),
	LGE_POWER_ATTR(batt_id_for_aat),
#endif
#ifdef CONFIG_LGE_PM
	LGE_POWER_ATTR(safety_timer),
#endif
#ifdef CONFIG_LGE_PM_CHARGING_BQ24296_CHARGER
	LGE_POWER_ATTR(ext_pwr),
	LGE_POWER_ATTR(removed),
#elif defined(CONFIG_LGE_PM_CHARGING_BQ24262_CHARGER)
	LGE_POWER_ATTR(ext_pwr),
#endif
#ifdef CONFIG_LGE_PM_CHARGING_VZW_POWER_REQ
	LGE_POWER_ATTR(vzw_chg),
#endif

#if defined(CONFIG_LGE_PM_CHARGING_BQ24296_CHARGER) \
		|| defined(CONFIG_LGE_PM_CHARGING_BQ24262_CHARGER)
	LGE_POWER_ATTR(charger_timer),
	LGE_POWER_ATTR(charging_complete),
#endif
#if defined(CONFIG_LGE_PM_BATTERY_EXTERNAL_FUELGAUGE)
	LGE_POWER_ATTR(use_fuelgauge),
#endif
#ifdef CONFIG_CHG_DETECTOR_MAX14656
	LGE_POWER_ATTR(usb_chg_detect_done),
	LGE_POWER_ATTR(usb_chg_type),
	LGE_POWER_ATTR(usb_dcd_timeout),
#endif
#if defined(CONFIG_LGE_PM_LLK_MODE)
	LGE_POWER_ATTR(store_demo_enabled),
#endif
	LGE_POWER_ATTR(hw_rev),
	LGE_POWER_ATTR(hw_rev_no),
#ifdef CONFIG_LGE_PM
	LGE_POWER_ATTR(calculated_soc),
#endif
	LGE_POWER_ATTR(chg_temp_status),
	LGE_POWER_ATTR(chg_temp_current),
	LGE_POWER_ATTR(batt_temp_status),
	LGE_POWER_ATTR(test_chg_scn),
	LGE_POWER_ATTR(test_batt_therm),
	LGE_POWER_ATTR(is_factory_cable),
	LGE_POWER_ATTR(is_factory_cable_boot),
	LGE_POWER_ATTR(cable_type),
	LGE_POWER_ATTR(cable_type_boot),
	LGE_POWER_ATTR(usb_current),
	LGE_POWER_ATTR(ta_current),
	LGE_POWER_ATTR(ibat_current),
	LGE_POWER_ATTR(update_cbl_info),
	LGE_POWER_ATTR(xo_therm_phy),
	LGE_POWER_ATTR(xo_therm_raw),
	LGE_POWER_ATTR(batt_therm_phy),
	LGE_POWER_ATTR(batt_therm_raw),
	LGE_POWER_ATTR(usb_id_phy),
	LGE_POWER_ATTR(usb_id_raw),
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
	LGE_POWER_ATTR(hvdcp_present),
	LGE_POWER_ATTR(type_usb_hvdcp),
	LGE_POWER_ATTR(type_usb_hvdcp_3),
#endif
	LGE_POWER_ATTR(pa0_therm_phy),
	LGE_POWER_ATTR(pa0_therm_raw),
	LGE_POWER_ATTR(pa1_therm_phy),
	LGE_POWER_ATTR(pa1_therm_raw),
	LGE_POWER_ATTR(bd1_therm_phy),
	LGE_POWER_ATTR(bd1_therm_raw),
	LGE_POWER_ATTR(bd2_therm_phy),
	LGE_POWER_ATTR(bd2_therm_raw),
#if defined(CONFIG_LGE_PM_CHARGING_SUPPORT_PHIHONG)
	LGE_POWER_ATTR(check_phihong),
#endif
	LGE_POWER_ATTR(pseudo_batt_ui),
	LGE_POWER_ATTR(btm_state),
	LGE_POWER_ATTR(otp_current),
	LGE_POWER_ATTR(is_chg_limit),
	LGE_POWER_ATTR(chg_present),
	LGE_POWER_ATTR(floated_charger),
	LGE_POWER_ATTR(vzw_chg),
	LGE_POWER_ATTR(batt_pack_name),
	LGE_POWER_ATTR(batt_capacity),
	LGE_POWER_ATTR(batt_cell),
	LGE_POWER_ATTR(chg_usb_enable),
	LGE_POWER_ATTR(chg_current_max),
	LGE_POWER_ATTR(batt_info),
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	LGE_POWER_ATTR(usb_current_max_mode),
#endif
	LGE_POWER_ATTR(qc_ibat_current),
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TDMB_MODE
	LGE_POWER_ATTR(tdmb_mode_on),
#endif
	LGE_POWER_ATTR(charge_done),
	LGE_POWER_ATTR(voltage_now),
};

static struct attribute *
__lge_power_attrs[ARRAY_SIZE(lge_power_attrs) + 1];

static umode_t lge_power_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int attrno)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct lge_power *lpc = dev_get_drvdata(dev);
	umode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
	int i;


	for (i = 0; i < lpc->num_properties; i++) {
		int property = lpc->properties[i];

		if (property == attrno) {
			if (lpc->property_is_writeable &&
			    lpc->property_is_writeable(lpc, property) > 0)
				mode |= S_IWUSR;

			return mode;
		}
	}

	return 0;
}

static struct attribute_group lge_power_attr_group = {
	.attrs = __lge_power_attrs,
	.is_visible = lge_power_attr_is_visible,
};

static const struct attribute_group *lge_power_attr_groups[] = {
	&lge_power_attr_group,
	NULL,
};

void lge_power_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = lge_power_attr_groups;

	for (i = 0; i < ARRAY_SIZE(lge_power_attrs); i++)
		__lge_power_attrs[i] = &lge_power_attrs[i].attr;
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}

int lge_power_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct lge_power *lpc = dev_get_drvdata(dev);
	int ret = 0, j;
	char *prop_buf;
	char *attrname;

	dev_dbg(dev, "uevent\n");

	if (!lpc || !lpc->dev) {
		dev_dbg(dev, "No lge power yet\n");
		return ret;
	}

	dev_dbg(dev, "POWER_SUPPLY_NAME=%s\n", lpc->name);

	ret = add_uevent_var(env, "POWER_SUPPLY_NAME=%s", lpc->name);
	if (ret)
		return ret;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < lpc->num_uevent_properties; j++) {
		struct device_attribute *attr;
		char *line;

		attr = &lge_power_attrs[lpc->uevent_properties[j]];

		ret = lge_power_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			/* When a battery is absent, we expect -ENODEV. Don't abort;
			   send the uevent with at least the the PRESENT=0 property */
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;

		line = strchr(prop_buf, '\n');
		if (line)
			*line = 0;

		attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
		if (!attrname) {
			ret = -ENOMEM;
			goto out;
		}

		dev_dbg(dev, "prop %s=%s\n", attrname, prop_buf);

		ret = add_uevent_var(env, "POWER_SUPPLY_%s=%s", attrname,
					prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}

