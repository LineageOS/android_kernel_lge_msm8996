/*
 * Boost bypass driver
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
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

struct boost_bypass_reg_data {
	struct regulator        *regs;
	const char              *reg_name;
	u32                     max_volt_uv;
	u32                     min_volt_uv;
	int                     flag;
};

struct boost_bypass_device {
	struct boost_bypass_reg_data *reg_data;
};

static int lge_boost_bypass_regulator_setup(struct device *dev,
		struct boost_bypass_device *bdev, bool on)
{
	int rc = 0;

	bdev->reg_data->regs = regulator_get(dev, bdev->reg_data->reg_name);

	if (IS_ERR_OR_NULL(bdev->reg_data->regs)) {
		rc = PTR_ERR(bdev->reg_data->regs);
		pr_err("%s : fail to get boost bypass regulator\n", __func__);
		return -ENODEV;
	} else {
		pr_err("%s : regulator exist\n", __func__);
	}

	if (bdev->reg_data->flag) {
		pr_err("%s : already set\n", __func__);
		return 0;
	}

	if (on == false) {
		pr_err("%s : vote boost bypass set to off\n", __func__);
		goto regulator_off;
	} else {
		pr_err("%s : vote boost bypass set to on\n", __func__);
	}

	if (regulator_count_voltages(bdev->reg_data->regs) > 0) {
		rc = regulator_set_voltage(bdev->reg_data->regs,
				bdev->reg_data->min_volt_uv,
				bdev->reg_data->max_volt_uv);
		if (rc) {
			pr_err("%s : regulator set voltage failed\n", __func__);
			regulator_put(bdev->reg_data->regs);
			goto regulator_off;
		}
	}

	rc = regulator_enable(bdev->reg_data->regs);
	return rc;

regulator_off:
	if (regulator_count_voltages(bdev->reg_data->regs) > 0) {
		regulator_set_voltage(bdev->reg_data->regs,
				0, bdev->reg_data->max_volt_uv);
	}

	regulator_put(bdev->reg_data->regs);
	rc = regulator_disable(bdev->reg_data->regs);
	return rc;
}

static int lge_boost_bypass_parse_dt(struct device *dev,
		struct boost_bypass_device *bdev)
{
	const char *byp_str;
	int val, rc = 0;

	bdev->reg_data = devm_kzalloc(dev,
			sizeof(struct boost_bypass_reg_data), GFP_KERNEL);

	if (IS_ERR_OR_NULL(bdev->reg_data)) {
		pr_err("%s : fail to alloc bypass_reg_data \n", __func__);
		return -ENOMEM;
	}

	rc = of_property_read_string(dev->of_node, "regulator-name", &byp_str);
	if (!rc) {
		bdev->reg_data->reg_name = byp_str;
	} else {
		pr_err("%s : cannot find byp regulators string\n", __func__);
		goto fail;
	}

	rc = of_property_read_u32(dev->of_node, "max-voltage", &val);
	if (!rc) {
		bdev->reg_data->max_volt_uv = val;
	} else {
		pr_err("%s : cannot find byp regulators voltage\n", __func__);
		goto fail;
	}

	rc = of_property_read_u32(dev->of_node, "min-voltage", &val);
	if (!rc) {
		bdev->reg_data->min_volt_uv = val;
	} else {
		pr_err("%s : cannot find byp regulators voltage\n", __func__);
		goto fail;
	}

	pr_err("%s : name = %s / volt = %d/%d\n",
			__func__,
			bdev->reg_data->reg_name,
			bdev->reg_data->max_volt_uv,
			bdev->reg_data->min_volt_uv);
	return rc;

fail:
	devm_kfree(dev, bdev->reg_data);
	return -ENODEV;
}

static int lge_boost_bypass_probe(struct platform_device *pdev)
{
	struct boost_bypass_device *bdev;
	int rc = 0;

	pr_err("%s : probe\n", __func__);
	bdev = devm_kzalloc(&pdev->dev,
			sizeof(struct boost_bypass_device),
			GFP_KERNEL);

	if (IS_ERR_OR_NULL(bdev)) {
		pr_err("%s : fail to alloc bypass_device \n", __func__);
		goto probe_fail;
	}

	rc = lge_boost_bypass_parse_dt(&pdev->dev, bdev);
	if (rc) {
		pr_err("%s : fail to get dt data\n", __func__);
		goto probe_fail;
	}

	rc = lge_boost_bypass_regulator_setup(&pdev->dev, bdev, true);
	if (rc) {
		pr_err("%s : fail to get dt data\n", __func__);
		goto probe_fail;
	}

	pr_err("%s : success\n", __func__);
	return 0;

probe_fail:
	pr_err("%s : fail %d\n", __func__, rc);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id lge_boost_bypass_match_table[] = {
	{ .compatible = "lge,boost-bypass" },
	{ }
};
#endif

static struct platform_driver lge_boost_bypass_driver = {
	.probe = lge_boost_bypass_probe,
	.driver = {
		.name = "lge-boost-bypass",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lge_boost_bypass_match_table,
#endif
	},
};

static int __init lge_boost_bypass_init(void)
{
	pr_info("%s : start\n", __func__);
	return platform_driver_register(&lge_boost_bypass_driver);
}

static void lge_boost_bypass_exit(void)
{
	platform_driver_unregister(&lge_boost_bypass_driver);
}

late_initcall(lge_boost_bypass_init);
module_exit(lge_boost_bypass_exit);
MODULE_DESCRIPTION("LGE bypass booster driver");
MODULE_LICENSE("GPL v2");
