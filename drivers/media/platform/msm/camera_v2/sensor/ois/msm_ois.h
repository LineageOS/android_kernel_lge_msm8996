/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#ifndef MSM_OIS_H
#define MSM_OIS_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"

//#define OIS_HALL_READ_WORK_Q
#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define	MSM_OIS_MAX_VREGS (10)

#ifdef CONFIG_LG_OIS
	//#define MSM_OIS_DEBUG
	#undef CDBG
	#ifdef MSM_OIS_DEBUG
	#define CDBG(fmt, args...) pr_err(fmt, ##args)
	#else
	#define CDBG(fmt, args...) pr_debug(fmt, ##args)
	#endif

	//OIS ERROR CODE
	#define OIS_SUCCESS 0
	#define OIS_FAIL    -1
	#define OIS_INIT_OLD_MODULE		1
	#define OIS_INIT_NOT_SUPPORTED  -2
	#define OIS_INIT_CHECKSUM_ERROR -3
	#define OIS_INIT_EEPROM_ERROR   -4
	#define OIS_INIT_I2C_ERROR      -5
	#define OIS_INIT_TIMEOUT		-6
	#define OIS_INIT_LOAD_BIN_ERROR -7
	#define OIS_INIT_NOMEM			-8
	#define OIS_INIT_GYRO_ADJ_FAIL	 2
	#define OIS_INIT_SRV_GAIN_FAIL	 4
#endif

struct msm_ois_ctrl_t;

enum msm_ois_state_t {
	OIS_ENABLE_STATE,
	OIS_OPS_ACTIVE,
	OIS_OPS_INACTIVE,
	OIS_DISABLE_STATE,
};

#ifdef CONFIG_LG_OIS
enum ois_mode_t {
	OIS_MODE_PREVIEW_CAPTURE,
	OIS_MODE_VIDEO,
	OIS_MODE_CAPTURE,
	OIS_MODE_CENTERING_ONLY,
	OIS_MODE_CENTERING_OFF
};

enum ois_pwm_mode_t {
	OIS_IMG_SENSOR_REG_A,
	OIS_IMG_SENSOR_REG_B,
	OIS_IMG_SENSOR_REG_C,
	OIS_IMG_SENSOR_REG_D,
	OIS_IMG_SENSOR_REG_E,
	OIS_IMG_SENSOR_REG_F,
	OIS_IMG_SENSOR_REG_G,
	OIS_IMG_SENSOR_REG_H,
};

struct msm_ois_func_tbl {
	int32_t (*ini_set_ois)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*enable_ois)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*disable_ois)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*ois_move_lens)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*ois_mode)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*ois_stat)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*ois_pwm_mode)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int ois_cur_mode;
};
struct msm_ois {
	struct msm_ois_func_tbl func_tbl;
};
#endif

struct msm_ois_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_OIS_MAX_VREGS];
	int num_vreg;
};

struct msm_ois_board_info {
	char ois_name[MAX_OIS_NAME_SIZE];
	uint32_t i2c_slaveaddr;
	struct msm_ois_opcode opcode;
};

struct msm_ois_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t ois_device_type;
	struct msm_sd_subdev msm_sd;
	struct mutex *ois_mutex;
	enum msm_camera_i2c_data_type i2c_data_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *ois_v4l2_subdev_ops;
	void *user_data;
	uint16_t i2c_tbl_index;
	enum cci_i2c_master_t cci_master;
	uint32_t subdev_id;
	enum msm_ois_state_t ois_state;
	struct msm_ois_vreg vreg_cfg;
	struct msm_camera_gpio_conf *gconf;
	struct msm_pinctrl_info pinctrl_info;
	uint8_t cam_pinctrl_status;

#ifdef CONFIG_LG_OIS
	struct msm_camera_i2c_client i2c_eeprom_client;
	struct msm_ois_func_tbl *func_tbl;
	uint16_t sid_ois;

#ifdef OIS_HALL_READ_WORK_Q
	struct work_struct ois_work;
	struct workqueue_struct *work_thread;
	uint8_t exit_workqueue;
	uint8_t pause_workqueue;
	uint8_t wq_init_success;
#endif
#endif
	struct msm_ois_board_info *oboard_info;
};

#endif
