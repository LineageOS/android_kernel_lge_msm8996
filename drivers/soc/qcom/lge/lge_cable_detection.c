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

#include <soc/qcom/lge/lge_cable_detection.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/power_supply.h>
#include <soc/qcom/smem.h>
#ifdef CONFIG_LGE_ALICE_FRIENDS
#include <soc/qcom/lge/board_lge.h>
#endif

struct chg_cable_info_table {
	int threshhold;
	enum acc_cable_type type;
	unsigned ta_ma;
	unsigned usb_ma;
};

#define ADC_NO_INIT_CABLE   0
#define C_NO_INIT_TA_MA     0
#define C_NO_INIT_USB_MA    0
#define ADC_CABLE_NONE      1900000
#define C_NONE_TA_MA        1800
#define C_NONE_USB_MA       500

#define MAX_CABLE_NUM       15

#define DEFAULT_CABLE_MAXSET   3
#define DEFAULT_CABLE_USESET   2
#define DEFAULT_CABLE_MUXSET   0x39

static struct chg_cable_info lge_cable_info;
static bool cable_type_defined;
static struct chg_cable_info_table lge_acc_cable_type_data[MAX_CABLE_NUM];
static enum qpnp_vadc_channels amux_ch;

static char *lge_cable_type_str[] = {
	"NOT INIT", "MHL 1K", "U_28P7K",
	"28P7K", "56K", "100K",  "130K",
	"180K", "200K", "220K",  "270K",
	"330K", "620K", "910K",  "OPEN"
};

bool lge_is_factory_cable(void)
{
	unsigned int cable_type = lge_cable_info.cable_type;
	unsigned int *cable_smem_type;
	unsigned int cable_smem_size;

	if (cable_type != NO_INIT_CABLE) {
		pr_debug("cable init, use lge cable type api = %s\n",
				lge_cable_type_str[cable_type]);
		if (cable_type == CABLE_56K ||
			cable_type == CABLE_130K ||
			cable_type == CABLE_910K)
			return true;
		else
			return false;
	} else {
		cable_smem_type = (unsigned int *)
			(smem_get_entry(SMEM_ID_VENDOR1,
					&cable_smem_size, 0, 0));

		if (!cable_smem_type) {
			pr_err("cable_smem_type is not ready\n");
			return false;
		}

		pr_debug("cable not init, use smem data = %u\n",
				*cable_smem_type);

		if (*cable_smem_type == LT_CABLE_56K ||
				*cable_smem_type == LT_CABLE_130K ||
				*cable_smem_type == LT_CABLE_910K)
			return true;
		else
			return false;
	}
}

int lge_smem_cable_type(void)
{
	unsigned int *cable_smem_type;
	unsigned int cable_smem_size;
	cable_smem_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1,
				&cable_smem_size, 0, 0));
	if (!cable_smem_type)
		return -EINVAL;
	else
		return *cable_smem_type;
}

void get_cable_data_from_dt(void *of_node)
{
	int i;
	u32 cable_value[3];

	int ret = 0;
	u32 c_maxset = 0;
	u32 c_useset = 0;

	struct device_node *node_temp = (struct device_node *)of_node;
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

	if (cable_type_defined) {
		pr_info("Cable type is already defined\n");
		return;
	}

	ret = of_property_read_u32(node_temp, "lge,cable-adc-maxset",
			&c_maxset);
	if (ret) {
		pr_info("Cable maxset is not exist, use default maxset\n");
		c_maxset = DEFAULT_CABLE_MAXSET;
	}

	ret = of_property_read_u32(node_temp, "lge,cable-adc-useset",
			&c_useset);
	if (ret) {
		pr_info("Cable useset is not exist, use default useset\n");
		c_useset = DEFAULT_CABLE_USESET;
	}

	ret = of_property_read_u32(node_temp, "lge,cable-adc-muxset",
			&amux_ch);
	if (ret) {
		pr_info("Cable muxset is not exist, use default muxset\n");
		amux_ch = DEFAULT_CABLE_MUXSET;
	}

	pr_info("Cable maxset %u, useset %u, muxset %u\n",
			c_maxset, c_useset, amux_ch);

	for (i = 0; i < MAX_CABLE_NUM; i++) {
		of_property_read_u32_array(node_temp, propname[i],
				cable_value, c_maxset);
		lge_acc_cable_type_data[i].threshhold = cable_value[c_useset];
		lge_acc_cable_type_data[i].type = i;
		lge_acc_cable_type_data[i].ta_ma = cable_value[0];
		lge_acc_cable_type_data[i].usb_ma = cable_value[1];
	}
	cable_type_defined = 1;
}

int lge_pm_get_cable_info(struct qpnp_vadc_chip *vadc,
		struct chg_cable_info *cable_info)
{
	struct qpnp_vadc_result result;
	struct chg_cable_info *info = cable_info;
	struct chg_cable_info_table *table;
	int table_size = ARRAY_SIZE(lge_acc_cable_type_data);
	int acc_read_value = 0;
	int i, rc;
	int count = 1;

#ifdef CONFIG_LGE_ALICE_FRIENDS
	if (lge_get_alice_friends() != LGE_ALICE_FRIENDS_NONE) {
		info->cable_type = CABLE_NONE;
		info->ta_ma = C_NONE_TA_MA;
		info->usb_ma = C_NONE_USB_MA;
		return 0;
	}
#endif

	if (!info) {
		pr_err("%s : invalid info parameters\n", __func__);
		return -EINVAL;
	}

	if (!vadc) {
		pr_err("%s : invalid vadc parameters\n", __func__);
		return -EINVAL;
	}

	if (!cable_type_defined) {
		pr_err("%s : cable type is not defined yet.\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		rc = qpnp_vadc_read(vadc, amux_ch, &result);
		if (rc < 0) {
			if (rc == -ETIMEDOUT) {
				/* reason: adc read timeout,
				 * assume it is open cable
				 */
				info->cable_type = CABLE_NONE;
				info->ta_ma = C_NONE_TA_MA;
				info->usb_ma = C_NONE_USB_MA;
			}
			pr_err("%s : adc read error - %d\n", __func__, rc);
			return rc;
		}

		acc_read_value = (int)result.physical;
		pr_info("%s : adc_read-%d\n", __func__, (int)result.physical);
		/* mdelay(10); */
	}

	info->cable_type = NO_INIT_CABLE;
	info->ta_ma = C_NO_INIT_TA_MA;
	info->usb_ma = C_NO_INIT_USB_MA;

	/* assume: adc value must be existed in ascending order */
	for (i = 0; i < table_size; i++) {
		table = &lge_acc_cable_type_data[i];

		if (acc_read_value <= table->threshhold) {
			info->cable_type = table->type;
			info->ta_ma = table->ta_ma;
			info->usb_ma = table->usb_ma;
			break;
		}
	}

	pr_err("\n\n[PM]Cable detected: %d(%s)(%d, %d)\n\n",
			acc_read_value,
			lge_cable_type_str[info->cable_type],
			info->ta_ma, info->usb_ma);

	return 0;
}

/* Belows are for using in interrupt context */
enum acc_cable_type lge_pm_get_cable_type(void)
{
	return lge_cable_info.cable_type;
}

unsigned lge_pm_get_ta_current(void)
{
	return lge_cable_info.ta_ma;
}

unsigned lge_pm_get_usb_current(void)
{
	return lge_cable_info.usb_ma;
}

/* This must be invoked in process context */
void lge_pm_read_cable_info(struct qpnp_vadc_chip *vadc)
{
	lge_cable_info.cable_type = NO_INIT_CABLE;
	lge_cable_info.ta_ma = C_NO_INIT_TA_MA;
	lge_cable_info.usb_ma = C_NO_INIT_USB_MA;

	lge_pm_get_cable_info(vadc, &lge_cable_info);
}
