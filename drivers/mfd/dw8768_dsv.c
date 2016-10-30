/*
 * DW8768 DSV MFD Driver
 *
 * Copyright 2015 LG Electronics Inc,
 *
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#include <linux/mfd/dw8768_dsv.h>

static struct dw8768_dsv *dw8768_dsv_base;
static struct mfd_cell dw8768_dsv_devs[] = {
	{ .name = "dw8768_dsv_dev" },
};

int dw8768_dsv_read_byte(struct dw8768_dsv *dw8768_dsv, u8 reg, u8 *read)
{
	int ret;
	unsigned int val;

	ret = regmap_read(dw8768_dsv->regmap, reg, &val);
	if (ret < 0)
		return ret;

	*read = (u8)val;
	return 0;
}
EXPORT_SYMBOL_GPL(dw8768_dsv_read_byte);

int dw8768_dsv_write_byte(struct dw8768_dsv *dw8768_dsv, u8 reg, u8 data)
{
	return regmap_write(dw8768_dsv->regmap, reg, data);
}
EXPORT_SYMBOL_GPL(dw8768_dsv_write_byte);

int dw8768_dsv_update_bits(struct dw8768_dsv *dw8768_dsv, u8 reg, u8 mask, u8 data)
{
	int ret;
	ret = regmap_update_bits(dw8768_dsv->regmap, reg, mask, data);
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_dsv_update_bits);

int dw8768_fast_discharge(void)
{
	int ret = 0;

	if (dw8768_dsv_base == NULL)
		return -EINVAL;

	pr_err("[LGD_SIC] DW8783 Fast Discharge.\n");
	ret = dw8768_dsv_write_byte(dw8768_dsv_base, DW8768_DISCHARGE_REG, 0x83);
	msleep(20);
	ret = dw8768_dsv_write_byte(dw8768_dsv_base, DW8768_DISCHARGE_REG, 0x80);
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_fast_discharge);

int dw8768_mode_change(int mode)
{
	int ret = 0;

	pr_info("%s mode = %d\n", __func__, mode);
	if (dw8768_dsv_base == NULL)
		return -EINVAL;

	if (mode == 0)
		ret = dw8768_dsv_write_byte(dw8768_dsv_base, DW8768_ENABLE_REG, 0x07);
	else if (mode == 1)
		ret = dw8768_dsv_write_byte(dw8768_dsv_base, DW8768_ENABLE_REG, 0x0F);

	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_mode_change);

int dw8768_lgd_dsv_setting(int enable)
{
	int ret = 0;

	if (dw8768_dsv_base == NULL)
		return -EINVAL;
	if (enable == 1) {
		ret  = dw8768_dsv_write_byte(dw8768_dsv_base, 0x00, 0x0F);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x01, 0x0F);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x03, 0x87);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x05, 0x07);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x07, 0x08);
		pr_info("DW DSV Normal mode set, ret = %d\n", ret);
	} else if (enable == 2) {
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x03, 0x87);
		pr_info("DW DSV FD ON set, ret = %d\n", ret);
	} else if (enable == 3) {
		ret  = dw8768_dsv_write_byte(dw8768_dsv_base, 0x00, 0x0F);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x01, 0x0F);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x03, 0x87);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x05, 0x0F);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x07, 0x08);
		pr_info("DW DSV Normal mode with ENM_R to high,  ret = %d\n", ret);
	} else {
		ret  = dw8768_dsv_write_byte(dw8768_dsv_base, 0x00, 0x09);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x01, 0x09);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x03, 0x84);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x05, 0x07);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x07, 0x08);
		pr_info("DW DSV LPWG mode set, ret = %d\n", ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_lgd_dsv_setting);

int dw8768_lgd_fd_mode_change(int enable)
{
	int ret = 0;

	if (dw8768_dsv_base == NULL)
		return -EINVAL;

	if (enable) {
		ret = dw8768_dsv_write_byte(dw8768_dsv_base, 0x00, 0x09);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x01, 0x09);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x03, 0x87);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x05, 0x07);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x07, 0x08);
		pr_info("dw8768_lgd_fd_mode_change enable : %d, ret = %d\n",
				enable, ret);

	} else {
		ret = dw8768_dsv_write_byte(dw8768_dsv_base, 0x00, 0x09);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x01, 0x09);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x03, 0x84);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x05, 0x07);
		ret += dw8768_dsv_write_byte(dw8768_dsv_base, 0x07, 0x08);
		pr_info("dw8768_lgd_fd_mode_change enable : %d, ret = %d\n",
				enable, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_lgd_fd_mode_change);

static struct regmap_config dw8768_dsv_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = DW8768_DSV_MAX_REGISTERS,
};

static int dw8768_dsv_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct dw8768_dsv *dw8768_dsv;
	struct device *dev = &cl->dev;
	struct dw8768_dsv_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	pr_info("%s start\n", __func__);

	dw8768_dsv = devm_kzalloc(dev, sizeof(*dw8768_dsv), GFP_KERNEL);
	if (!dw8768_dsv)
		return -ENOMEM;

	dw8768_dsv->pdata = pdata;

	dw8768_dsv->regmap = devm_regmap_init_i2c(cl, &dw8768_dsv_regmap_config);
	if (IS_ERR(dw8768_dsv->regmap)) {
		pr_err("Failed to allocate register map\n");
		devm_kfree(dev, dw8768_dsv);
		return PTR_ERR(dw8768_dsv->regmap);
	}

	dw8768_dsv->dev = &cl->dev;
	i2c_set_clientdata(cl, dw8768_dsv);
	dw8768_dsv_base = dw8768_dsv;

	rc = mfd_add_devices(dev, -1, dw8768_dsv_devs, ARRAY_SIZE(dw8768_dsv_devs),
			       NULL, 0, NULL);
	if (rc) {
		pr_err("Failed to add dw8768_dsv subdevice ret=%d\n", rc);
		return -ENODEV;
	}

	return rc;
}

static int dw8768_dsv_remove(struct i2c_client *cl)
{
	struct dw8768_dsv *dw8768_dsv = i2c_get_clientdata(cl);

	mfd_remove_devices(dw8768_dsv->dev);

	return 0;
}

static const struct i2c_device_id dw8768_dsv_ids[] = {
	{ "dw8768_dsv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dw8768_dsv_ids);

#ifdef CONFIG_OF
static const struct of_device_id dw8768_dsv_of_match[] = {
	{ .compatible = "dw8768_dsv", },
	{ }
};
MODULE_DEVICE_TABLE(of, dw8768_dsv_of_match);
#endif

static struct i2c_driver dw8768_dsv_driver = {
	.driver = {
		.name = "dw8768_dsv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dw8768_dsv_of_match),
#endif
	},
	.id_table = dw8768_dsv_ids,
	.probe = dw8768_dsv_probe,
	.remove = dw8768_dsv_remove,
};
module_i2c_driver(dw8768_dsv_driver);

MODULE_DESCRIPTION("dw8768_dsv MFD Core");
