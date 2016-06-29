/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#define T4KA3_SENSOR_NAME "t4ka3"
#include <soc/qcom/lge/board_lge.h>
DEFINE_MSM_MUTEX(t4ka3_mut);

#undef CDBG
#ifdef CONFIG_T4KA3_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define IOVDD_IS_NOT_I2C_PULL_UP 1

static struct msm_sensor_ctrl_t t4ka3_s_ctrl;

static struct msm_sensor_power_setting t4ka3_power_setting[] = {
	{ //[0]
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{ //[1]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{ //[2]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},
	{ //[3]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
#if (IOVDD_IS_NOT_I2C_PULL_UP == 1) //HDK Board
	{ //[4]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_I2C_PULL_UP,
		.config_val = 0,
		.delay = 1,
	},
#endif
	{ //[5]
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 2,
	},
	{ //[6]
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 2,
	},
	{ //[7]
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 2,
	},
};
/*LGE add for sequence of power down */
static struct msm_sensor_power_setting t4ka3_power_down_setting[] = {
	{ //[0]
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 1,
	},
	{ //[1]
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 2,
	},
	{ //[2]
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{ //[3]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
#if (IOVDD_IS_NOT_I2C_PULL_UP == 1) //HDK Board
	{ //[4]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_I2C_PULL_UP,
		.config_val = 0,
		.delay = 0,
	},
#endif
	{ //[5]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 0,
	},
	{ //[6]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
};

static struct msm_sensor_power_setting t4ka3_power_setting_hdk[] = {
	{ //[0]
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{ //[1]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{ //[2]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},
	{ //[3]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
#if (IOVDD_IS_NOT_I2C_PULL_UP == 1) //HDK Board
	{ //[4]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_I2C_PULL_UP,
		.config_val = 0,
		.delay = 1,
	},
#endif
	{ //[5]
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 2,
	},
	{ //[6]
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 2,
	},
	{ //[7]
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 2,
	},
};
/*LGE add for sequence of power down */
static struct msm_sensor_power_setting t4ka3_power_down_setting_hdk[] = {
	{ //[0]
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 1,
	},
	{ //[1]
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 2,
	},
	{ //[2]
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{ //[3]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
#if (IOVDD_IS_NOT_I2C_PULL_UP == 1) //HDK Board
	{ //[4]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_I2C_PULL_UP,
		.config_val = 0,
		.delay = 0,
	},
#endif
	{ //[5]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 0,
	},
	{ //[6]
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info t4ka3_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id t4ka3_i2c_id[] = {
	{T4KA3_SENSOR_NAME, (kernel_ulong_t)&t4ka3_s_ctrl},
	{ }
};

static int32_t msm_t4ka3_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &t4ka3_s_ctrl);
}

static struct i2c_driver t4ka3_i2c_driver = {
	.id_table = t4ka3_i2c_id,
	.probe  = msm_t4ka3_i2c_probe,
	.driver = {
		.name = T4KA3_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client t4ka3_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id t4ka3_dt_match[] = {
	{.compatible = "qcom,t4ka3", .data = &t4ka3_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, t4ka3_dt_match);

static struct platform_driver t4ka3_platform_driver = {
	.driver = {
		.name = "qcom,t4ka3",
		.owner = THIS_MODULE,
		.of_match_table = t4ka3_dt_match,
	},
};

static int32_t t4ka3_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	pr_err("%s: E\n", __func__);
	match = of_match_device(t4ka3_dt_match, &pdev->dev);

/* LGE : WBT */
	if(!match)
	{
		pr_err(" %s failed ",__func__);
	      return -ENODEV;
	}

	rc = msm_sensor_platform_probe(pdev, match->data);
	pr_err("%s: X\n", __func__);
	return rc;
}

static int __init t4ka3_init_module(void)
{
	int32_t rc = 0;
	int32_t rev = 0;

	pr_err("%s: E\n", __func__);

	rev = lge_get_board_revno();
	pr_err("%s:HW rev = %d\n", __func__, rev);

	switch(rev) {
		case 0: //HW_REV_EVB1
		case 1: //HW_REV_EVB2
		case 2: //HW_REV_EVB3
		case 3: //HW_REV_0
			pr_err("%s:t4ka3_power_setting_hdk\n", __func__);
			t4ka3_s_ctrl.power_setting_array.power_setting = t4ka3_power_setting_hdk;
			t4ka3_s_ctrl.power_setting_array.size = ARRAY_SIZE(t4ka3_power_setting_hdk);
			t4ka3_s_ctrl.power_setting_array.power_down_setting= t4ka3_power_down_setting_hdk;
			t4ka3_s_ctrl.power_setting_array.size_down = ARRAY_SIZE(t4ka3_power_down_setting_hdk);
			break;
		case 4: //HW_REV_0_1
			pr_err("%s:t4ka3_power_setting\n", __func__);
			t4ka3_s_ctrl.power_setting_array.power_setting = t4ka3_power_setting;
			t4ka3_s_ctrl.power_setting_array.size = ARRAY_SIZE(t4ka3_power_setting);
			t4ka3_s_ctrl.power_setting_array.power_down_setting= t4ka3_power_down_setting;
			t4ka3_s_ctrl.power_setting_array.size_down = ARRAY_SIZE(t4ka3_power_down_setting);
		case 5: //HW_REV_A
		case 6: //HW_REV_B
		case 7: //HW_REV_C
		case 8: //HW_REV_D
		case 9: //HW_REV_E
		case 10: //HW_REV_F
		case 11: //HW_REV_G
		case 12: //HW_REV_1_0
		case 13: //HW_REV_1_1
		case 14: //HW_REV_1_2
		default:
			pr_err("%s:Default! => t4ka3_power_setting\n", __func__);
			t4ka3_s_ctrl.power_setting_array.power_setting = t4ka3_power_setting;
			t4ka3_s_ctrl.power_setting_array.size = ARRAY_SIZE(t4ka3_power_setting);
			t4ka3_s_ctrl.power_setting_array.power_down_setting= t4ka3_power_down_setting;
			t4ka3_s_ctrl.power_setting_array.size_down = ARRAY_SIZE(t4ka3_power_down_setting);
			break;
	}

	rc = platform_driver_probe(&t4ka3_platform_driver,
		t4ka3_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s: X rc = %d\n", __func__, rc);
	return i2c_add_driver(&t4ka3_i2c_driver);
}

static void __exit t4ka3_exit_module(void)
{
	if (t4ka3_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&t4ka3_s_ctrl);
		platform_driver_unregister(&t4ka3_platform_driver);
	} else
		i2c_del_driver(&t4ka3_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t t4ka3_s_ctrl = {
	.sensor_i2c_client = &t4ka3_sensor_i2c_client,
	.msm_sensor_mutex = &t4ka3_mut,
	.sensor_v4l2_subdev_info = t4ka3_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(t4ka3_subdev_info),
};

module_init(t4ka3_init_module);
module_exit(t4ka3_exit_module);
MODULE_DESCRIPTION("t4ka3");
MODULE_LICENSE("GPL v2");
