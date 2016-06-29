/*
 * LGE Accessory adc driver
 *
 * Copyright (C) 2015 LGE, Inc.
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
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <soc/qcom/lge/lge_acc_nt_type.h>

struct nt_string_map {
        nt_type_t nt_type;
        char *type_name;
};

static struct nt_string_map nt_string_table[] = {
        {NT_TYPE_ERROR_MIN,         "NT_TYPE_ERROR_MIN"      },
        {NT_TYPE_NA_VZW,            "NT_TYPE_VZW"            },
        {NT_TYPE_KR_LGU,            "NT_TYPE_LGU"            },
        {NT_TYPE_CM,                "NT_TYPE_CM"             },
        {NT_TYPE_EU_GLOBAL,         "NT_TYPE_EU_GLOBAL"      },
        {NT_TYPE_NA_SPR,            "NT_TYPE_SPR"            },
        {NT_TYPE_KR_SKT,            "NT_TYPE_SKT"            },
        {NT_TYPE_HM,                "NT_TYPE_HM"             },
        {NT_TYPE_JP_KDDI,           "NT_TYPE_KDDI"           },
        {NT_TYPE_NA_ATT,            "NT_TYPE_ATT"            },
        {NT_TYPE_KR_KT,             "NT_TYPE_KT"             },
        {NT_TYPE_EU_AUSTRALIA,      "NT_TYPE_AUSTRALIA"      },
        {NT_TYPE_NA_TMUS,           "NT_TYPE_TMUS"           },
        {NT_TYPE_RESERVED1,         "NT_TYPE_RESERVED1"      },
        {NT_TYPE_RESERVED2,         "NT_TYPE_RESERVED2"      },
        {NT_TYPE_ERROR_MAX,         "NT_TYPE_ERROR_MAX"      },
        {NT_TYPE_ERROR_UNKNOWN,     "NT_TYPE_ERROR_UNKNOWN"  },
};

static nt_type_t acc_nt_type;

static nt_type_t get_nt_type_from_name(char *nt_type_string)
{
	uint32_t i, table_size;

	table_size = sizeof(nt_string_table) / sizeof(nt_string_table[0]);

	for(i=0;i<table_size;i++)
		if(!strcmp(nt_type_string, nt_string_table[i].type_name))
			return nt_string_table[i].nt_type;

	return NT_TYPE_ERROR_UNKNOWN;
}

static int __init nt_type_setup(char *nt_type)
{
	pr_err("%s: acc_nt_type - %s\n", __func__, nt_type);

	acc_nt_type = get_nt_type_from_name(nt_type);

	return 1;
}
__setup("lge.acc_nt_type=",nt_type_setup);

nt_type_t get_acc_nt_type(void)
{
	return acc_nt_type;
}

static int dummy_arg;

struct acc_nt_data_from_lk {
	int lk_nt_raw_data;
	const char *lk_nt_type_check;
};

static struct acc_nt_data_from_lk *nt_data_from_lk;

static int read_lk_nt_raw_data(char *buffer, const struct kernel_param *kp)
{
	if(nt_data_from_lk && nt_data_from_lk->lk_nt_raw_data > 0)
		return sprintf(buffer, "%d", nt_data_from_lk->lk_nt_raw_data);
	else
		return sprintf(buffer, "-1");
}
module_param_call(lk_nt_raw_data, NULL, read_lk_nt_raw_data, &dummy_arg, S_IWUSR | S_IRUGO);

static int read_lk_nt_type_check(char *buffer, const struct kernel_param *kp)
{
	if(nt_data_from_lk && nt_data_from_lk->lk_nt_type_check)
		return sprintf(buffer, "%s", nt_data_from_lk->lk_nt_type_check);
	else
		return sprintf(buffer, "NULL");
}
module_param_call(lk_nt_type_check, NULL, read_lk_nt_type_check, &dummy_arg, S_IWUSR | S_IRUGO);

static int read_acc_nt_value_from_lk(void)
{
	int ret = 0;
	struct device_node *node;

	node = of_find_node_by_path("/chosen");
	if(node == NULL){
		pr_err("%s: of_find_node_by_path failed\n", __func__);
		return -EINVAL;
	}

	nt_data_from_lk = kmalloc(sizeof(struct acc_nt_data_from_lk), GFP_KERNEL);

	if(!nt_data_from_lk){
		pr_err("%s: failed to allocate driver data\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32(node, "lge,lk_nt_raw_data", (u32 *)&(nt_data_from_lk->lk_nt_raw_data));
	if(ret){
		pr_err("wrong lk_nt_raw_data. Please check 'lge,lk_nt_raw_data' property\n");
		goto exit;
	}

	ret = of_property_read_string(node, "lge,lk_nt_type_check", &nt_data_from_lk->lk_nt_type_check);
	if(ret){
		pr_err("%s: failed to read lge,lk_nt_raw_data from chosen node\n", __func__);
		goto exit;
	}

	pr_err("%s: lge,lk_nt_raw_data (%d), lge,lk_nt_type_check (%s)\n", __func__, nt_data_from_lk->lk_nt_raw_data, nt_data_from_lk->lk_nt_type_check);

	return ret;

exit:
	nt_data_from_lk->lk_nt_raw_data = 0;
	nt_data_from_lk->lk_nt_type_check = NULL;
	return ret;
}


struct lge_acc_nt {
	char *name;
	int resistor;
	int threshold_min;
	int threshold_max;
};

#define ADC_READ_CNT   3
#define ADC_NOT_INIT  -1

#define NUM_OF_NT  15
struct lge_acc_nt lge_acc_nt_data[NUM_OF_NT] = {
	{ "Error"    ,        0,  INT_MIN,   135000 }, /* Error */
	{ "vzw"      ,    15000,   135000,   254000 }, /*  30%  */
	{ "uplus"    ,    20000,   254000,   329000 }, /*  35%  */
	{ "M1"       ,    27000,   329000,   416000 }, /*  35%  */
	{ "Global"   ,    36000,   416000,   511000 }, /*  35%  */
	{ "SPR"      ,    47000,   511000,   600000 }, /*  35%  */
	{ "SKT"      ,    56000,   600000,   675000 }, /*  35%  */
	{ "M2"       ,    68000,   675000,   774000 }, /*  35%  */
	{ "KDDI"     ,    91000,   774000,   901000 }, /*  35%  */
	{ "ATT"      ,   120000,   901000,  1043000 }, /*  35%  */
	{ "KT"       ,   180000,  1043000,  1185000 }, /*  35%  */
	{ "Austraila",   220000,  1185000,  1277000 }, /*  35%  */
	{ "reserve1" ,   300000,  1277000,  1508000 }, /*  35%  */
	{ "reserve2" ,        0,  1508000,  1900000 },
	{ "Error"    ,        0,  1900000,  INT_MAX }, /* Error */
};

static int acc_nt = -ENODEV;
module_param_named(
	acc_nt_val, acc_nt, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static void dump_acc_nt(int adc, int rc, int *vadc_avg)
{
	int i = 0;

	pr_err("%s : rc     = %d\n", __func__, rc);
	pr_err("%s : adc    = %d\n", __func__, adc);
	pr_err("%s : acc_nt = %d\n", __func__, acc_nt);

	for (i = 0; i < ADC_READ_CNT; i++) {
		pr_err("%s : vadc_avg[%d] = %d\n",
				__func__, i, vadc_avg[i]);
	}
}

static int calc_average(int a, int b)
{
	return ((a + b) / 2);
}

static int lge_acc_nt_probe(struct platform_device *pdev)
{
	struct qpnp_vadc_chip *vadc_dev;
	struct qpnp_vadc_result result;

	int range_num = sizeof(lge_acc_nt_data) / sizeof(struct lge_acc_nt);
	int vadc_avg[ADC_READ_CNT] = { 0, };
	int i, j, rc = 0;
	int adc = ADC_NOT_INIT;

	if (of_find_property(pdev->dev.of_node, "qcom,acc-nt-vadc", NULL)) {
		vadc_dev = qpnp_get_vadc(&pdev->dev, "acc-nt");
		if (IS_ERR(vadc_dev)) {
			rc = PTR_ERR(vadc_dev);
			if (rc != -EPROBE_DEFER) {
				pr_err("%s : fail to get acc_nt adc channel\n",
						__func__);
			} else
				pr_err("%s : adc is not ready\n", __func__);

			goto fail_dump;
		}
	}

	/*
	 * - vadc result averaging three times
	 * - each time, if vadc error, try more two times
	 * - if vadc read fail is detected although try more two times,
	 *   store rc value to vadc_avg array
	 * - count out that error values during vadc_avg averaging
	 *   if all values are error, return error
	 */
	for (i = 0; i < ADC_READ_CNT; i++) {
		for (j = 0; j < ADC_READ_CNT; j++) {
			rc = qpnp_vadc_read(vadc_dev,
					LR_MUX9_PU1_AMUX_THM5, &result);
			if (rc < 0) {
				pr_err("%s : fail to read vadc, %d\n",
						__func__, rc);
				continue;
			} else {
				break;
			}
		}

		if (rc < 0)
			/* three times fail */
			vadc_avg[i] = rc;
		else
			/* at least, one time success */
			vadc_avg[i] = result.physical;

		pr_err("%s : vadc_avg[%d]  = %d\n",
				__func__, i, vadc_avg[i]);
	}

	for (i = 0; i < ADC_READ_CNT; i++) {
		if (vadc_avg[i] >= 0) {
			/*
			 * find first no error vadc value in three time
			 * others, average
			 */
			if (adc == ADC_NOT_INIT)
				adc = vadc_avg[i];
			else
				adc = calc_average(adc, vadc_avg[i]);
		}
	}

	if (adc == ADC_NOT_INIT) {
		pr_err("%s : not exist correct vadc read value at array\n",
				__func__);
		acc_nt = -EINVAL;
		goto fail_dump;
	}

	pr_err("%s : NT adc = %d\n", __func__, adc);

	/* INT value should be return btw INT_MIN and INT_MAX */
	for (i = 0; i < range_num; i++) {
		if (adc > lge_acc_nt_data[i].threshold_max)
			continue;
		else
			break;
	}

	/* Error detect NT's min/max adc */
	if (!strcmp(lge_acc_nt_data[i].name, "Error")) {
		pr_err("%s : error detect, adc = %d, i = %d, range_num = %d\n",
				__func__, adc, i, range_num);
		acc_nt = -ERANGE;
		goto fail_dump;
	}

	pr_err("%s : lge_acc_nt_data = %s / %d / %d / %d\n",
			__func__,
			lge_acc_nt_data[i].name,
			lge_acc_nt_data[i].resistor,
			lge_acc_nt_data[i].threshold_min,
			lge_acc_nt_data[i].threshold_max);

	acc_nt = i;

	pr_err("%s : done\n", __func__);

	read_acc_nt_value_from_lk();

	return 0;

fail_dump :
	dump_acc_nt(adc, rc, vadc_avg);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id lge_acc_nt_match_table[] = {
	{ .compatible = "lge,acc-nt" },
	{ }
};
#endif

static struct platform_driver lge_acc_nt_driver = {
	.probe = lge_acc_nt_probe,
	.driver = {
		.name = "lge-acc-nt",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lge_acc_nt_match_table,
#endif
	},
};

static int __init lge_acc_nt_init(void)
{
	pr_info("%s : start\n", __func__);
	return platform_driver_register(&lge_acc_nt_driver);
}

static void lge_acc_nt_exit(void)
{
	platform_driver_unregister(&lge_acc_nt_driver);
}

late_initcall(lge_acc_nt_init);
module_exit(lge_acc_nt_exit);
MODULE_DESCRIPTION("LGE accessory NT driver");
MODULE_LICENSE("GPL v2");
