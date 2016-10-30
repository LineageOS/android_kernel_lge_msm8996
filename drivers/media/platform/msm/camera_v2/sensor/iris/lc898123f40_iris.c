/* Copyright (c) 2014 - 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_iris.h"
#include "msm_cci.h"

#undef CONFIG_COMPAT

DEFINE_MSM_MUTEX(msm_iris_mutex);
//#define MSM_IRIS_DEBUG
#undef CDBG
#ifdef MSM_IRIS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static struct v4l2_file_operations msm_iris_v4l2_subdev_fops;
static int32_t msm_iris_power_up(struct msm_iris_ctrl_t *o_ctrl);
static int32_t msm_iris_power_down(struct msm_iris_ctrl_t *o_ctrl);

static struct i2c_driver msm_iris_i2c_driver;

static int32_t msm_iris_write_settings(struct msm_iris_ctrl_t *o_ctrl,
	uint16_t size, struct reg_settings_iris_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	struct msm_camera_i2c_seq_reg_array *reg_setting;
	CDBG("Enter\n");

	for (i = 0; i < size; i++) {
		switch (settings[i].i2c_operation) {
		case MSM_IRIS_WRITE: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
			case MSM_CAMERA_I2C_WORD_DATA:
				rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_write(
					&o_ctrl->i2c_client,
					settings[i].reg_addr,
					settings[i].reg_data,
					settings[i].data_type);
				break;
			case MSM_CAMERA_I2C_DWORD_DATA:
			reg_setting =
			kzalloc(sizeof(struct msm_camera_i2c_seq_reg_array),
				GFP_KERNEL);
				if (!reg_setting)
					return -ENOMEM;

				reg_setting->reg_addr = settings[i].reg_addr;
				reg_setting->reg_data[0] = (uint8_t)
					((settings[i].reg_data &
					0xFF000000) >> 24);
				reg_setting->reg_data[1] = (uint8_t)
					((settings[i].reg_data &
					0x00FF0000) >> 16);
				reg_setting->reg_data[2] = (uint8_t)
					((settings[i].reg_data &
					0x0000FF00) >> 8);
				reg_setting->reg_data[3] = (uint8_t)
					(settings[i].reg_data & 0x000000FF);
				reg_setting->reg_data_size = 4;
				rc = o_ctrl->i2c_client.i2c_func_tbl->
					i2c_write_seq(&o_ctrl->i2c_client,
					reg_setting->reg_addr,
					reg_setting->reg_data,
					reg_setting->reg_data_size);
				kfree(reg_setting);
				reg_setting = NULL;
				if (rc < 0)
					return rc;
				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
			if (settings[i].delay > 20)
				msleep(settings[i].delay);
			else if (0 != settings[i].delay)
				usleep_range(settings[i].delay * 1000,
					(settings[i].delay * 1000) + 1000);
		}
			break;

		case MSM_IRIS_POLL: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
			case MSM_CAMERA_I2C_WORD_DATA:

				rc = o_ctrl->i2c_client.i2c_func_tbl
					->i2c_poll(&o_ctrl->i2c_client,
					settings[i].reg_addr,
					settings[i].reg_data,
					settings[i].data_type,
					settings[i].delay);
				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
		}

		case MSM_IRIS_DELAY:
			if (settings[i].delay > 20)
				msleep(settings[i].delay);
			else if (0 != settings[i].delay)
				usleep_range(settings[i].delay * 1000,
					(settings[i].delay * 1000) + 1000);
			rc = 0;
			break;

		}

		if (rc < 0)
			break;
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_iris_vreg_control(struct msm_iris_ctrl_t *o_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_iris_vreg *vreg_cfg;

	vreg_cfg = &o_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_IRIS_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(o_ctrl->pdev->dev),
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);
	}
	return rc;
}

static int32_t msm_iris_power_down(struct msm_iris_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	if (o_ctrl->iris_state != IRIS_DISABLE_STATE) {

		rc = msm_iris_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		o_ctrl->i2c_tbl_index = 0;
		o_ctrl->iris_state = IRIS_OPS_INACTIVE;
	}
	CDBG("Exit\n");
	return rc;
}

static int msm_iris_init(struct msm_iris_ctrl_t *o_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");

	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}

	if (o_ctrl->iris_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	o_ctrl->iris_state = IRIS_OPS_ACTIVE;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_iris_control(struct msm_iris_ctrl_t *o_ctrl,
	struct msm_iris_set_info_t *set_info)
{
	struct reg_settings_iris_t *settings = NULL;
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	if (o_ctrl->iris_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = o_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->iris_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = o_ctrl->cci_master;
		cci_client->i2c_freq_mode = set_info->iris_params.i2c_freq_mode;
	} else {
		o_ctrl->i2c_client.client->addr =
			set_info->iris_params.i2c_addr;
	}
	o_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;


	if (set_info->iris_params.setting_size > 0 &&
		set_info->iris_params.setting_size
		< MAX_IRIS_REG_SETTINGS) {
		settings = kmalloc(
			sizeof(struct reg_settings_iris_t) *
			(set_info->iris_params.setting_size),
			GFP_KERNEL);
		if (settings == NULL) {
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(settings,
			(void *)set_info->iris_params.settings,
			set_info->iris_params.setting_size *
			sizeof(struct reg_settings_iris_t))) {
			kfree(settings);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_iris_write_settings(o_ctrl,
			set_info->iris_params.setting_size,
			settings);
		kfree(settings);
		if (rc < 0) {
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}


static int32_t msm_iris_config(struct msm_iris_ctrl_t *o_ctrl,
	void __user *argp)
{
	struct msm_iris_cfg_data *cdata =
		(struct msm_iris_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(o_ctrl->iris_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_IRIS_INIT:
		rc = msm_iris_init(o_ctrl);
		if (rc < 0)
			pr_err("msm_iris_init failed %d\n", rc);
		break;
	case CFG_IRIS_POWERDOWN:
		rc = msm_iris_power_down(o_ctrl);
		if (rc < 0)
			pr_err("msm_iris_power_down failed %d\n", rc);
		break;
	case CFG_IRIS_POWERUP:
		rc = msm_iris_power_up(o_ctrl);
		if (rc < 0)
			pr_err("Failed iris power up%d\n", rc);
		break;
	case CFG_IRIS_CONTROL:
		rc = msm_iris_control(o_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("Failed iris control%d\n", rc);
		break;
	case CFG_IRIS_I2C_WRITE_SEQ_TABLE: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

#ifdef CONFIG_COMPAT
		if (is_compat_task()) {
			memcpy(&conf_array,
				(void *)cdata->cfg.settings,
				sizeof(struct msm_camera_i2c_seq_reg_setting));
		} else
#endif
		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.settings,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = o_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_seq_table(&o_ctrl->i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}
	default:
		break;
	}
	mutex_unlock(o_ctrl->iris_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_iris_get_subdev_id(struct msm_iris_ctrl_t *o_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->iris_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = o_ctrl->pdev->id;
	else
		*subdev_id = o_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq = msm_camera_qup_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_iris_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_iris_ctrl_t *o_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	mutex_lock(o_ctrl->iris_mutex);
	if (o_ctrl->iris_device_type == MSM_CAMERA_PLATFORM_DEVICE &&
		o_ctrl->iris_state != IRIS_DISABLE_STATE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	o_ctrl->iris_state = IRIS_DISABLE_STATE;
	mutex_unlock(o_ctrl->iris_mutex);
	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_iris_internal_ops = {
	.close = msm_iris_close,
};

static long msm_iris_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc;
	struct msm_iris_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d o_ctrl %p argp %p\n", __func__, __LINE__, o_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_iris_get_subdev_id(o_ctrl, argp);
	case VIDIOC_MSM_IRIS_CFG:
		return msm_iris_config(o_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		if (!o_ctrl->i2c_client.i2c_func_tbl) {
			pr_err("o_ctrl->i2c_client.i2c_func_tbl NULL\n");
			return -EINVAL;
		}
		rc = msm_iris_power_down(o_ctrl);
		if (rc < 0) {
			pr_err("%s:%d IRIS Power down failed\n",
				__func__, __LINE__);
		}
		return msm_iris_close(sd, NULL);
	default:
		return -ENOIOCTLCMD;
	}
}

static int32_t msm_iris_power_up(struct msm_iris_ctrl_t *o_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);

	rc = msm_iris_vreg_control(o_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	o_ctrl->iris_state = IRIS_ENABLE_STATE;
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_iris_subdev_core_ops = {
	.ioctl = msm_iris_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_iris_subdev_ops = {
	.core = &msm_iris_subdev_core_ops,
};

static const struct i2c_device_id msm_iris_i2c_id[] = {
	{"qcom,iris", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_iris_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_iris_ctrl_t *iris_ctrl_t = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_iris_i2c_probe: client is null\n");
		return -EINVAL;
	}

	iris_ctrl_t = kzalloc(sizeof(struct msm_iris_ctrl_t),
		GFP_KERNEL);
	if (!iris_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		rc = -EINVAL;
		goto probe_failure;
	}

	CDBG("client = 0x%p\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&iris_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", iris_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	iris_ctrl_t->i2c_driver = &msm_iris_i2c_driver;
	iris_ctrl_t->i2c_client.client = client;
	/* Set device type as I2C */
	iris_ctrl_t->iris_device_type = MSM_CAMERA_I2C_DEVICE;
	iris_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	iris_ctrl_t->iris_v4l2_subdev_ops = &msm_iris_subdev_ops;
	iris_ctrl_t->iris_mutex = &msm_iris_mutex;

	/* Assign name for sub device */
	snprintf(iris_ctrl_t->msm_sd.sd.name, sizeof(iris_ctrl_t->msm_sd.sd.name),
		"%s", iris_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&iris_ctrl_t->msm_sd.sd,
		iris_ctrl_t->i2c_client.client,
		iris_ctrl_t->iris_v4l2_subdev_ops);
	v4l2_set_subdevdata(&iris_ctrl_t->msm_sd.sd, iris_ctrl_t);
	iris_ctrl_t->msm_sd.sd.internal_ops = &msm_iris_internal_ops;
	iris_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&iris_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	iris_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	iris_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_IRIS;
	iris_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&iris_ctrl_t->msm_sd);
	iris_ctrl_t->iris_state = IRIS_DISABLE_STATE;
	pr_info("msm_iris_i2c_probe: succeeded\n");
	CDBG("Exit\n");

probe_failure:
	kfree(iris_ctrl_t);
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_iris_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	long rc = 0;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	struct msm_iris_cfg_data32 *u32;
	struct msm_iris_cfg_data iris_data;
	void *parg;
	struct msm_camera_i2c_seq_reg_setting settings;
	struct msm_camera_i2c_seq_reg_setting32 settings32;

	if (!file || !arg) {
		pr_err("%s:failed NULL parameter\n", __func__);
		return -EINVAL;
	}
	vdev = video_devdata(file);
	sd = vdev_to_v4l2_subdev(vdev);
	u32 = (struct msm_iris_cfg_data32 *)arg;
	parg = arg;

	iris_data.cfgtype = u32->cfgtype;

	switch (cmd) {
	case VIDIOC_MSM_IRIS_CFG32:
		cmd = VIDIOC_MSM_IRIS_CFG;

		switch (u32->cfgtype) {
		case CFG_IRIS_CONTROL:
			iris_data.cfg.set_info.iris_params.setting_size =
				u32->cfg.set_info.iris_params.setting_size;
			iris_data.cfg.set_info.iris_params.i2c_addr =
				u32->cfg.set_info.iris_params.i2c_addr;
			iris_data.cfg.set_info.iris_params.i2c_freq_mode =
				u32->cfg.set_info.iris_params.i2c_freq_mode;
			iris_data.cfg.set_info.iris_params.i2c_addr_type =
				u32->cfg.set_info.iris_params.i2c_addr_type;
			iris_data.cfg.set_info.iris_params.i2c_data_type =
				u32->cfg.set_info.iris_params.i2c_data_type;
			iris_data.cfg.set_info.iris_params.settings =
				compat_ptr(u32->cfg.set_info.iris_params.
				settings);
			parg = &iris_data;
			break;
		case CFG_IRIS_I2C_WRITE_SEQ_TABLE:
			if (copy_from_user(&settings32,
				(void *)compat_ptr(u32->cfg.settings),
				sizeof(
				struct msm_camera_i2c_seq_reg_setting32))) {
				pr_err("copy_from_user failed\n");
				return -EFAULT;
			}

			settings.addr_type = settings32.addr_type;
			settings.delay = settings32.delay;
			settings.size = settings32.size;
			settings.reg_setting =
				compat_ptr(settings32.reg_setting);

			iris_data.cfgtype = u32->cfgtype;
			iris_data.cfg.settings = &settings;
			parg = &iris_data;
			break;
		default:
			parg = &iris_data;
			break;
		}
	}
	rc = msm_iris_subdev_ioctl(sd, cmd, parg);

	return rc;
}

static long msm_iris_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_iris_subdev_do_ioctl);
}
#endif

static int32_t msm_iris_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_iris_ctrl_t *msm_iris_t = NULL;
	struct msm_iris_vreg *vreg_cfg;
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_iris_t = kzalloc(sizeof(struct msm_iris_ctrl_t),
		GFP_KERNEL);
	if (!msm_iris_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_iris_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_iris_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_iris_t->cci_master, rc);
	if (rc < 0 || msm_iris_t->cci_master >= MASTER_MAX) {
		kfree(msm_iris_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_iris_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_iris_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	msm_iris_t->iris_v4l2_subdev_ops = &msm_iris_subdev_ops;
	msm_iris_t->iris_mutex = &msm_iris_mutex;

	/* Set platform device handle */
	msm_iris_t->pdev = pdev;
	/* Set device type as platform device */
	msm_iris_t->iris_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_iris_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_iris_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_iris_t->i2c_client.cci_client) {
		kfree(msm_iris_t->vreg_cfg.cam_vreg);
		kfree(msm_iris_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_iris_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = msm_iris_t->cci_master;
	v4l2_subdev_init(&msm_iris_t->msm_sd.sd,
		msm_iris_t->iris_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_iris_t->msm_sd.sd, msm_iris_t);
	msm_iris_t->msm_sd.sd.internal_ops = &msm_iris_internal_ops;
	msm_iris_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_iris_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_iris_t->msm_sd.sd.name), "msm_iris");
	media_entity_init(&msm_iris_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_iris_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_iris_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_IRIS;
	msm_iris_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_iris_t->msm_sd);
	msm_iris_t->iris_state = IRIS_DISABLE_STATE;
	msm_cam_copy_v4l2_subdev_fops(&msm_iris_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_iris_v4l2_subdev_fops.compat_ioctl32 =
		msm_iris_subdev_fops_ioctl;
#endif
	msm_iris_t->msm_sd.sd.devnode->fops =
		&msm_iris_v4l2_subdev_fops;

	CDBG("Exit\n");
	return rc;
}

static const struct of_device_id msm_iris_i2c_dt_match[] = {
	{.compatible = "qcom,iris"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_iris_i2c_dt_match);

static struct i2c_driver msm_iris_i2c_driver = {
	.id_table = msm_iris_i2c_id,
	.probe  = msm_iris_i2c_probe,
	.remove = __exit_p(msm_iris_i2c_remove),
	.driver = {
		.name = "qcom,iris",
		.owner = THIS_MODULE,
		.of_match_table = msm_iris_i2c_dt_match,
	},
};

static const struct of_device_id msm_iris_dt_match[] = {
	{.compatible = "qcom,iris", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_iris_dt_match);

static struct platform_driver msm_iris_platform_driver = {
	.probe = msm_iris_platform_probe,
	.driver = {
		.name = "qcom,iris",
		.owner = THIS_MODULE,
		.of_match_table = msm_iris_dt_match,
	},
};

static int __init msm_iris_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_register(&msm_iris_platform_driver);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_iris_i2c_driver);
}

static void __exit msm_iris_exit_module(void)
{
	platform_driver_unregister(&msm_iris_platform_driver);
	i2c_del_driver(&msm_iris_i2c_driver);
	return;
}

module_init(msm_iris_init_module);
module_exit(msm_iris_exit_module);
MODULE_DESCRIPTION("MSM IRIS");
MODULE_LICENSE("GPL v2");
