/*
 * lge_pseudo_batt.c
 *
 * Copyright (C) 2014 LG Electronics. Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <soc/qcom/lge/lge_pseudo_batt.h>

struct pseudo_batt_info_type pseudo_batt_info = {
	.mode = 0,
};

void set_pseudo_batt_info(struct pseudo_batt_info_type *info)
{
	struct power_supply *batt_psy;

	pr_info("pseudo_batt_set\n");

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		pr_err("called before init\n");
		return;
	}

	pseudo_batt_info.mode = info->mode;
	pseudo_batt_info.id = info->id;
	pseudo_batt_info.therm = info->therm;
	pseudo_batt_info.temp = info->temp;
	pseudo_batt_info.volt = info->volt;
	pseudo_batt_info.capacity = info->capacity;
	pseudo_batt_info.charging = info->charging;
	power_supply_changed(batt_psy);
}

int get_pseudo_batt_info(int type)
{
	switch (type) {
	case PSEUDO_BATT_MODE:
		return pseudo_batt_info.mode;
	case PSEUDO_BATT_ID:
		return pseudo_batt_info.id;
	case PSEUDO_BATT_THERM:
		return pseudo_batt_info.therm;
	case PSEUDO_BATT_TEMP:
		return pseudo_batt_info.temp;
	case PSEUDO_BATT_VOLT:
		return pseudo_batt_info.volt;
	case PSEUDO_BATT_CAPACITY:
		return pseudo_batt_info.capacity;
	case PSEUDO_BATT_CHARGING:
		return pseudo_batt_info.charging;
	default:
		pr_err("pseudo_batt : Wrong value\n");
		break;
	}
	return false;
}

