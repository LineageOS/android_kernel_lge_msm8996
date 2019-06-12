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
#ifndef MSM_TCS_H
#define MSM_TCS_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_sd.h"


#define DEFINE_MSM_MUTEX(mutexname) \
static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)
#define	MSM_TCS_MAX_VREGS (10)

#define TCS_SUCCESS 		0

#define TCS_NOT_READY         	1	
#define TCS_READY_OK         	2	
#define TCS_IS_WORKING         	3
#define TCS_EXIT         	4

#define TCS_ERR_OLD_MODULE     	11
#define TCS_ERR_I2C      	12
#define TCS_ERR_TIMEOUT       	13
#define TCS_ERR_WORK       	14


///////////FOR AMS Sensor /////////////////////
enum tcs3490_ctrl_reg {
	AGAIN_1        = (0 << 0),
	AGAIN_4        = (1 << 0),
	AGAIN_16       = (2 << 0),
	AGAIN_64       = (3 << 0),
};

#define ALS_PERSIST(p) (((p) & 0xf) << 3)


typedef struct  {
	uint32_t clear_raw;
	uint32_t red_raw;
	uint32_t green_raw;
	uint32_t blue_raw;
	uint32_t ir;
} tcs3490_als_info;

typedef struct  {
	uint8_t als_time;
	uint16_t als_deltaP;
	uint8_t als_gain;
	uint8_t persist;
	uint16_t BIN_Info;
 } tcs3490_parameters;

#define TCS3490_CMD_ALS_INT_CLR  0xE6
#define TCS3490_CMD_ALL_INT_CLR	0xE7

#define INTEGRATION_CYCLE  270
#define I2C_ADDR_OFFSET	0X80

enum tcs3490_regs {
	TCS3490_CONTROL,
	TCS3490_ALS_TIME,                  // 0x81
	TCS3490_RESV_1,
	TCS3490_WAIT_TIME,               // 0x83
	TCS3490_ALS_MINTHRESHLO,   // 0x84
	TCS3490_ALS_MINTHRESHHI,   // 0x85
	TCS3490_ALS_MAXTHRESHLO,  // 0x86
	TCS3490_ALS_MAXTHRESHHI,  // 0x87
	TCS3490_RESV_2,                     // 0x88
	TCS3490_PRX_MINTHRESHLO,  // 0x89 -> Not used for TCS3490

	TCS3490_RESV_3,                    // 0x8A
	TCS3490_PRX_MAXTHRESHHI, // 0x8B  -> Not used for TCS3490
	TCS3490_PERSISTENCE,          // 0x8C
	TCS3490_CONFIG,                    // 0x8D
	TCS3490_PRX_PULSE_COUNT,  // 0x8E  -> Not used for TCS3490
	TCS3490_GAIN,                        // 0x8F  : Gain Control Register
	TCS3490_AUX,                          // 0x90
	TCS3490_REVID,
	TCS3490_CHIPID,
	TCS3490_STATUS,                    // 0x93

	TCS3490_CLR_CHANLO,            // 0x94
	TCS3490_CLR_CHANHI,            // 0x95
	TCS3490_RED_CHANLO,           // 0x96
	TCS3490_RED_CHANHI,           // 0x97
	TCS3490_GRN_CHANLO,           // 0x98
	TCS3490_GRN_CHANHI,           // 0x99
	TCS3490_BLU_CHANLO,           // 0x9A
	TCS3490_BLU_CHANHI,           // 0x9B
	TCS3490_PRX_HI,                    // 0x9C
	TCS3490_PRX_LO,                    // 0x9D

	TCS3490_PRX_OFFSET,            // 0x9E
	TCS3490_RESV_4,                    // 0x9F
	TCS3490_IRBEAM_CFG,            // 0xA0
	TCS3490_IRBEAM_CARR,          // 0xA1
	TCS3490_IRBEAM_NS,              // 0xA2
	TCS3490_IRBEAM_ISD,            // 0xA3
	TCS3490_IRBEAM_NP,              // 0xA4
	TCS3490_IRBEAM_IPD,            // 0xA5
	TCS3490_IRBEAM_DIV,            // 0xA6
	TCS3490_IRBEAM_LEN,            // 0xA7

	TCS3490_IRBEAM_STAT,         // 0xA8
	TCS3490_REG_MAX,

};

enum tcs3490_en_reg {
	TCS3490_EN_PWR_ON   = (1 << 0),
	TCS3490_EN_ALS      = (1 << 1),
	TCS3490_EN_PRX      = (1 << 2),
	TCS3490_EN_WAIT     = (1 << 3),
	TCS3490_EN_ALS_IRQ  = (1 << 4),
	TCS3490_EN_PRX_IRQ  = (1 << 5),
	TCS3490_EN_IRQ_PWRDN = (1 << 6),
	TCS3490_EN_BEAM     = (1 << 7),
};

enum tcs3490_status {
	TCS3490_ST_ALS_VALID  = (1 << 0),
	TCS3490_ST_PRX_VALID  = (1 << 1),
	TCS3490_ST_BEAM_IRQ   = (1 << 3),
	TCS3490_ST_ALS_IRQ    = (1 << 4),
	TCS3490_ST_PRX_IRQ    = (1 << 5),
	TCS3490_ST_PRX_SAT    = (1 << 6),
};

enum {
	TCS3490_ALS_GAIN_MASK = (3 << 0),
	TCS3490_PRX_GAIN_MASK = (3 << 2),
	TCS3490_ALS_AGL_MASK  = (1 << 2),
	TCS3490_ALS_AGL_SHIFT = 2,
	TCS3490_ATIME_PER_100 = 273,
	TCS3490_ATIME_DEFAULT_MS = 50,
	SCALE_SHIFT = 11,
	RATIO_SHIFT = 10,
	MAX_ALS_VALUE = 0xffff,
	MIN_ALS_VALUE = 10,
	//GAIN_SWITCH_LEVEL = 100,
	GAIN_SWITCH_LEVEL = 5000,
	GAIN_AUTO_INIT_VALUE = AGAIN_16,
};

static uint8_t const als_gains[] = {
	1,
	4,
	16,
	64
};



typedef struct  {
	 tcs3490_als_info als_inf;
	 tcs3490_parameters params;

	uint8_t  shadow[42];

	int in_suspend;
	int wake_irq;
	int irq_pending;
	bool unpowered;
	bool als_enabled;

	int segment_num;
	int seg_num_max;
	bool als_gain_auto;
	uint8_t device_index;
	uint8_t bc_symbol_table[128];
	uint16_t bc_nibbles;
	uint16_t hop_count;
	uint8_t hop_next_slot;
	uint8_t hop_index;
}tcs3490_chip ;
////////////////////////////////////////////////////////////////////////


enum msm_tcs_state_t {
	TCS_POWER_UP,
	TCS_POWER_DOWN,
};

struct msm_tcs_fn_t {
	int (*tcs_off) (void);
};

struct msm_tcs_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_TCS_MAX_VREGS];
	int num_vreg;
};

struct msm_tcs_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t tcs_device_type;

	struct msm_sd_subdev 	msm_sd;
	struct mutex *tcs_mutex;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *tcs_v4l2_subdev_ops;
	struct msm_tcs_info_t tcs_stat;
	enum cci_i2c_master_t cci_master;

	uint16_t sid_tcs;
	struct msm_tcs_fn_t *tcs_func_tbl;
	struct work_struct tcs_work;
	struct workqueue_struct *work_thread;
	uint16_t last_tcs;
	uint8_t exit_workqueue;
	uint8_t pause_workqueue;
	uint8_t wq_init_success;
	uint32_t max_i2c_fail_thres;
	uint32_t i2c_fail_cnt;
	uint8_t  tcs_status;
	uint32_t tcs_error;
	uint32_t rgbsum_ir;
	uint32_t subdev_id;
	uint8_t wq_gain_ctrl_init_success;
	uint8_t read_count;
	bool    stop_reading;

	enum msm_tcs_state_t tcs_state;
	struct msm_tcs_vreg vreg_cfg;


	///////////FOR AMS Sensor /////////////////////
	tcs3490_als_info 		als_inf;
	tcs3490_parameters 	params;
	uint8_t  				shadow[42];
	bool 					als_gain_auto;
	bool als_gain_reduced;
	bool 					reg_clr;
	bool als_gain_param_changed;
	 //////////////////////// /////////////////////
};

//#define MSM_TCS_DEBUG
#undef CDBG
#ifdef MSM_TCS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

int32_t msm_init_tcs(void);
uint16_t msm_get_tcs(struct msm_tcs_info_t* tcs_info);
uint16_t msm_tcs_thread_start(void);
uint16_t msm_tcs_thread_end(void);
uint16_t msm_tcs_thread_pause(void);
uint16_t msm_tcs_thread_restart(void);

#endif
