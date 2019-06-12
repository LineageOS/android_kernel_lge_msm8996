/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#ifndef MSM_PROXY_H
#define MSM_PROXY_H

// only other definition is in msm_cam_sensor.h, which also must be changed
#define EWOK_API_IMPLEMENTATION
//#define BABYBEAR_API_IMPLEMENTATION



#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_sd.h"

#ifdef BABYBEAR_API_IMPLEMENTATION
#include "vl6180x_api.h"
#endif
#ifdef EWOK_API_IMPLEMENTATION
#include "vl53l0_api.h"
#endif
#ifndef BABYBEAR_API_IMPLEMENTATION
	#ifndef EWOK_API_IMPLEMENTATION
	#include "vl6180x_api.h"
	#endif
#endif


#define DEFINE_MSM_MUTEX(mutexname) \
static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)
#define	MSM_PROXY_MAX_VREGS (10)

#define PROXY_SUCCESS 0
#define PROXY_FAIL    -1
#define PROXY_INIT_OLD_MODULE		1
#define PROXY_INIT_NOT_SUPPORTED  -2
#define PROXY_INIT_CHECKSUM_ERROR -3
#define PROXY_INIT_EEPROM_ERROR   -4
#define PROXY_INIT_I2C_ERROR      -5
#define PROXY_INIT_TIMEOUT		-6
#define PROXY_INIT_LOAD_BIN_ERROR -7
#define PROXY_INIT_NOMEM			-8
#define PROXY_INIT_GYRO_ADJ_FAIL	 2

enum msm_proxy_state_t {
	PROXY_POWER_UP,
	PROXY_POWER_DOWN,
};

struct msm_proxy_fn_t {
	int (*proxy_off) (void);
};

struct msm_proxy_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_PROXY_MAX_VREGS];
	int num_vreg;
};

struct msm_proxy_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	struct msm_camera_i2c_client i2c_eeprom_client;
	enum msm_camera_device_type_t proxy_device_type;
	struct msm_sd_subdev msm_sd;
	struct mutex *proxy_mutex;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *proxy_v4l2_subdev_ops;
	struct msm_proxy_info_t proxy_stat;
	enum cci_i2c_master_t cci_master;

	uint16_t sid_proxy;
	struct msm_proxy_fn_t *proxy_func_tbl;
	struct work_struct proxy_work;
	struct workqueue_struct *work_thread;
	uint16_t last_proxy;
	uint8_t exit_workqueue;
	uint8_t pause_workqueue;
	uint8_t wq_init_success;
	uint8_t check_init_finish;
	uint32_t max_i2c_fail_thres;
	uint32_t i2c_fail_cnt;
	uint8_t proxy_cal;
	uint32_t subdev_id;
	enum msm_proxy_state_t proxy_state;
	struct msm_proxy_vreg vreg_cfg;
};

//#define MSM_PROXY_DEBUG
#undef CDBG
#ifdef MSM_PROXY_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

uint16_t msm_proxy_cal(void);
int32_t msm_init_proxy(void);
int msm_init_proxy_BBearAPI(void);
int msm_init_proxy_EwokAPI(void);
uint16_t msm_get_proxy(struct msm_proxy_info_t* proxy_info);
uint16_t msm_proxy_thread_start(void);
uint16_t msm_proxy_thread_end(void);
uint16_t msm_proxy_thread_pause(void);
uint16_t msm_proxy_thread_restart(void);

#endif
