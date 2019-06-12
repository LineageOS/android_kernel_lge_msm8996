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

//#define MSM_TCS_DEBUG
#undef CDBG
#ifdef MSM_TCS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_tcs.h"
#include "msm_cci.h"
#include "msm_tcs_i2c.h"

DEFINE_MSM_MUTEX(msm_tcs_mutex);

extern int32_t tcs3490_device_Scan_All_Data(struct msm_tcs_ctrl_t* state);
extern int32_t tcs3490_Read_all(struct msm_tcs_ctrl_t* state);
extern int32_t tcs3490_Read_BINs(struct msm_tcs_ctrl_t* state);
static bool tcs_init_flag = FALSE;
static bool tcs_dup_init = FALSE;

static struct msm_tcs_ctrl_t msm_tcs_t;

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
	msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_seq = msm_camera_qup_i2c_write_seq,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
	msm_camera_qup_i2c_write_table_w_microdelay,
};



static struct v4l2_file_operations msm_tcs_v4l2_subdev_fops;
static struct i2c_driver msm_tcs_i2c_driver;

static int32_t msm_tcs_get_subdev_id(struct msm_tcs_ctrl_t *tcs_ctrl, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (tcs_ctrl->tcs_device_type == MSM_CAMERA_PLATFORM_DEVICE)
	{
		*subdev_id = tcs_ctrl->pdev->id;
	}
	else
	{
		*subdev_id = tcs_ctrl->subdev_id;
	}

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static int msm_tcs_close(struct v4l2_subdev *sd,  struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_tcs_ctrl_t *o_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->tcs_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		if (tcs_dup_init == TRUE) {
			if (tcs_init_flag == FALSE) {
				pr_err("[ERROR] TCS dup_init = TRUE, init_flag = FALSE\n");
			}
			pr_err("IMX234 or IMX268 is closed, but TCS is alive.\n");
			tcs_dup_init = FALSE;
		}
		else {
			if (tcs_init_flag == FALSE) {
				pr_err("[ERROR] TCS is already closed.\n");
			}
	 		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
	 			&o_ctrl->i2c_client, MSM_CCI_RELEASE);
	 		if (rc < 0) {
	 			pr_err("cci_init failed\n");
	 		}
			tcs_init_flag = FALSE;
		}
	}

	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_tcs_internal_ops = {
	.close = msm_tcs_close,
};

int32_t tcs_i2c_read(uint32_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type)
{
	int32_t ret = 0;

	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_tcs_t.i2c_client.cci_client;
	cci_client->sid = 0x39;    //AMS (no shift)
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_tcs_t.cci_master;
	msm_tcs_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	ret = msm_tcs_t.i2c_client.i2c_func_tbl->i2c_read(&msm_tcs_t.i2c_client, addr, &data[0], data_type);

	if (ret < 0) {
		msm_tcs_t.i2c_fail_cnt++;
		pr_err("TCS i2c_read_fail_cnt = %d, addr %x\n", msm_tcs_t.i2c_fail_cnt, addr);
	}
	return ret;
}
int32_t tcs_i2c_write(uint32_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type)
{
	int32_t ret = 0;

	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_tcs_t.i2c_client.cci_client;
	cci_client->sid = 0x39;    //AMS (no shift)
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_tcs_t.cci_master;
	msm_tcs_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

    ret = msm_tcs_t.i2c_client.i2c_func_tbl->i2c_write(&msm_tcs_t.i2c_client, addr, data, data_type);
	if (ret < 0) {
		msm_tcs_t.i2c_fail_cnt++;
		pr_err("TCS i2c_write_fail_cnt = %d addr = %x\n", msm_tcs_t.i2c_fail_cnt, addr);
	}
	return ret;
}

int32_t tcs_i2c_write_seq(uint32_t addr, uint8_t *data, uint16_t num_byte)
{
	int32_t ret = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_tcs_t.i2c_client.cci_client;
	cci_client->sid = 0x39;    //AMS (no shift)
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_tcs_t.cci_master;
	msm_tcs_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	ret = msm_tcs_t.i2c_client.i2c_func_tbl->i2c_write_seq(&msm_tcs_t.i2c_client, addr, &data[0], num_byte);

	if (ret < 0) {
		msm_tcs_t.i2c_fail_cnt++;
		pr_err("TCS i2c_write_seq fail_cnt = %d\n", msm_tcs_t.i2c_fail_cnt);
	}
	return ret;
}

int32_t tcs_i2c_read_seq(uint32_t addr, uint8_t *data, uint16_t num_byte)
{
	int32_t ret = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_tcs_t.i2c_client.cci_client;
	cci_client->sid =  0x39;    //AMS (no shift)
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_tcs_t.cci_master;
	msm_tcs_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	ret = msm_tcs_t.i2c_client.i2c_func_tbl->i2c_read_seq(&msm_tcs_t.i2c_client, addr, &data[0], num_byte);
	if (ret < 0) {
                msm_tcs_t.i2c_fail_cnt++;
                pr_err("TCS i2c_read_seq fail_cnt = %d addr = %x\n", msm_tcs_t.i2c_fail_cnt, addr);
        }

	return ret;
}


uint16_t msm_get_tcs(struct msm_tcs_info_t *rgb_info)
{
	if(msm_tcs_t.read_count > 0 &&  msm_tcs_t.stop_reading==false &&  msm_tcs_t.pause_workqueue==0)
    {
	   rgb_info->status = msm_tcs_t.shadow[TCS3490_STATUS];
	   rgb_info->status |= (msm_tcs_t.als_gain_reduced == true ? 99 : msm_tcs_t.params.als_gain) << 8;
	   rgb_info->status |= (msm_tcs_t.params.als_time) << 16;
	   rgb_info->clear = msm_tcs_t.als_inf.clear_raw;
	   rgb_info->red = msm_tcs_t.als_inf.red_raw;
	   rgb_info->green = msm_tcs_t.als_inf.green_raw;
	   rgb_info->blue = msm_tcs_t.als_inf.blue_raw;
	   rgb_info->ir = msm_tcs_t.als_inf.ir;
	   rgb_info->extra1 = msm_tcs_t.params.BIN_Info;

	   rgb_info->extra2 =   msm_tcs_t.rgbsum_ir ;
    }
	else
   {
	   memset(rgb_info, 0, 8*sizeof(uint32_t));
	   rgb_info->extra1 = msm_tcs_t.params.BIN_Info;
   }

//pr_err("Get Data(tcs) : status= %x, CLR = %d, RED = %d,GRN = %d, BLU = %d  IR = %d, extra1: %x, extra2 = %x  wait_cnt = %d\n",
//  rgb_info->status, rgb_info->clear ,rgb_info->red, rgb_info->green, rgb_info->blue ,rgb_info->ir, rgb_info->extra1, rgb_info->extra2, msm_tcs_t.read_count);

	return 0;

}

static void get_tcs(struct work_struct *work)
{
	int32_t ret = 0;
	struct msm_tcs_ctrl_t *tcs_struct = container_of(work, struct msm_tcs_ctrl_t, tcs_work);
	//Execute Read All to Initialize Data
	tcs3490_Read_all(&msm_tcs_t);
	ret = tcs3490_Read_BINs(&msm_tcs_t);
	msm_tcs_t.als_gain_param_changed=true;
  msm_tcs_t.read_count = 1;

	//RGBC Sensor Main loop
	while(1) {
		//RGBC Sensor Main Function
		if(!tcs_struct->pause_workqueue) {
			ret = tcs3490_device_Scan_All_Data(&msm_tcs_t);
			if( ret < 0 )
			{
				pr_err("tcs_struct workqueue force end due to scan fail!\n");
				msm_tcs_t.tcs_status = TCS_EXIT;
	            break;

			}
			msm_tcs_t.tcs_status = TCS_IS_WORKING;
		}

		//I2C Error Check
		if(tcs_struct->i2c_fail_cnt >= tcs_struct->max_i2c_fail_thres) {
			pr_err("tcs_struct workqueue force end due to i2c fail!\n");
			msm_tcs_t.tcs_error = TCS_ERR_I2C;
			msm_tcs_t.tcs_status = TCS_EXIT;
			break;
		}
		msleep(5);	//Need to be adjusted with proper value.

		//Check Exit Thread
		if(tcs_struct->exit_workqueue) {
			msm_tcs_t.tcs_status = TCS_EXIT;
			break;
		}
	}
	pr_err("end workqueue!\n");


}
int16_t stop_tcs(void)
{
	CDBG("stop_tcs!\n");
	if (msm_tcs_t.exit_workqueue == 0) {
		if (msm_tcs_t.wq_init_success) {
			msm_tcs_t.exit_workqueue = 1;
                        flush_workqueue(msm_tcs_t.work_thread);
			destroy_workqueue(msm_tcs_t.work_thread);
			msm_tcs_t.work_thread = NULL;
			/* LGE_CHANGE, CST, deinitialize wq_init_success */
			msm_tcs_t.wq_init_success = 0;
			pr_err("destroy_workqueue!\n");
		}
	}
	return 0;
}
int16_t pause_tcs(void)
{
	CDBG("pause_tcs!\n");
	msm_tcs_t.pause_workqueue = 1;
//Reset
	memset(&msm_tcs_t.als_inf, 0,  5*sizeof(uint32_t));
	msm_tcs_t.shadow[TCS3490_STATUS] = 0;
	pr_err("pause_workqueue = %d\n", msm_tcs_t.pause_workqueue);
	return 0;
}
int16_t restart_tcs(void)
{
	CDBG("restart_tcs!\n");
	msm_tcs_t.pause_workqueue = 0;
	CDBG("pause_workqueue = %d\n", msm_tcs_t.pause_workqueue);
	return 0;
}
uint16_t msm_tcs_thread_start(void)
{
	int ret = 0;

	CDBG("msm_tcs_thread_start\n");

	if (msm_tcs_t.exit_workqueue) {
		msm_tcs_t.exit_workqueue = 0;
		msm_tcs_t.work_thread = create_singlethread_workqueue("tcs_work_thread");
		if (!msm_tcs_t.work_thread) {
			/* LGE_CHANGE, CST, deinitialize wq_init_success */
			msm_tcs_t.wq_init_success = 0;
			pr_err("creating work_thread fail!\n");
			return 1;
		}

		msm_tcs_t.wq_init_success = 1;

		INIT_WORK(&msm_tcs_t.tcs_work, get_tcs);
		CDBG("INIT_WORK done!\n");

		ret = queue_work(msm_tcs_t.work_thread, &msm_tcs_t.tcs_work);
		CDBG("tcs queue_work done!%d  %s\n", ret, ret? "already waiting" : "added ok");
	}
	return 0;
}
uint16_t msm_tcs_thread_end(void)
{
	uint16_t ret = 0;
	CDBG("msm_tcs_thread_end\n");
	if (tcs_dup_init == FALSE) {
		ret = stop_tcs();
	}
	return ret;
}
uint16_t msm_tcs_thread_pause(void)
{
	uint16_t ret = 0;
	CDBG("msm_tcs_thread_pause\n");
	if (tcs_dup_init == FALSE) {
		ret = pause_tcs();
	}
	return ret;
}
uint16_t msm_tcs_thread_restart(void)
{
	uint16_t ret = 0;
	CDBG("msm_tcs_thread_restart\n");
		msm_tcs_t.i2c_fail_cnt = 0;
	if (tcs_dup_init == FALSE) {
		ret = restart_tcs();
	}
	return ret;
}

int32_t msm_init_tcs(void)
{
	int32_t rc=0;

	uint16_t reg_val = 0;
	uint16_t read_val = 0;

	CDBG(" tcs3490 init\n");

    if (tcs_dup_init == TRUE) return rc;

          memset(&msm_tcs_t.shadow, 0, sizeof(msm_tcs_t.shadow));
	memset(&msm_tcs_t.params, 0, sizeof(msm_tcs_t.params));
	rc = tcs_i2c_read(0x92,	&read_val, 1);
	if(rc < 0 )
	{
		msm_tcs_t.tcs_error = TCS_ERR_I2C;
		return 	rc;
	}
	CDBG(" TCS3490_CHIPID= 0x%x\n", read_val);

	//set Defaults
	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_ALS_TIME, 0xEE, 1); /* 50ms */
	if(rc < 0 )
        {
                msm_tcs_t.tcs_error = TCS_ERR_I2C;
                return  rc;
        }
	msm_tcs_t.shadow[TCS3490_ALS_TIME] = 0xEE;   //50ms
	msm_tcs_t.params.als_time = 0xEE;

	reg_val = ALS_PERSIST(0);
	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_PERSISTENCE, reg_val, 1);
	if(rc < 0 )
        {
                msm_tcs_t.tcs_error = TCS_ERR_I2C;
                return  rc;
        }

	msm_tcs_t.shadow[TCS3490_PERSISTENCE] = reg_val;
	msm_tcs_t.params.als_deltaP = reg_val;

	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_GAIN, AGAIN_16, 1);
	if(rc < 0 )
        {
                msm_tcs_t.tcs_error = TCS_ERR_I2C;
                return  rc;
        }

	msm_tcs_t.shadow[TCS3490_GAIN] = AGAIN_16;
	msm_tcs_t.params.als_gain = AGAIN_16;

	msm_tcs_t.als_gain_auto = true;
	msm_tcs_t.reg_clr = true; //initial : CLEAR
	msm_tcs_t.als_gain_reduced = false;

    msm_tcs_t.read_count = 0;
    msm_tcs_t.stop_reading = false;
	reg_val = TCS3490_EN_PWR_ON | TCS3490_EN_ALS |TCS3490_EN_ALS_IRQ;
	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_CONTROL, reg_val, 1);
	if(rc < 0 )
        {
                msm_tcs_t.tcs_error = TCS_ERR_I2C;
                return  rc;
        }
	msm_tcs_t.tcs_status = TCS_READY_OK;

	return rc;
}

static int32_t msm_tcs_vreg_control(struct msm_tcs_ctrl_t *o_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_tcs_vreg *vreg_cfg;

	vreg_cfg = &o_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_TCS_MAX_VREGS) {
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

static int32_t msm_tcs_power_up(struct msm_tcs_ctrl_t *o_ctrl)
{
	int rc = 0;
	pr_err("%s called\n", __func__);

	rc = msm_tcs_vreg_control(o_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}
	o_ctrl->tcs_state = TCS_POWER_UP;
	pr_err("Exit\n");
	return rc;
}

static int32_t msm_tcs_power_down(struct msm_tcs_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	pr_err("Enter\n");
	if (o_ctrl->tcs_state != TCS_POWER_DOWN && tcs_dup_init != TRUE) {

		rc = msm_tcs_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		rc = stop_tcs();
		msm_tcs_t.exit_workqueue = 1;
		o_ctrl->tcs_state = TCS_POWER_DOWN;
	}
	pr_err("Exit\n");
	return rc;
}

static int msm_tcs_init(struct msm_tcs_ctrl_t *o_ctrl)
{
	int rc = 0;
	uint16_t read_val = 0;

	pr_err("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if(tcs_init_flag == TRUE) {
		rc = 0;
		if(tcs_dup_init == TRUE) {
			pr_err("[ERROR] TCS is already on.\n");
		}
		else {
			pr_err("TCS is already on! skip tcs_init.\n");
			tcs_dup_init = TRUE;
		}
	}
	else {
		if(tcs_dup_init == TRUE) {
			pr_err("[ERROR] TCS dup_init is on, but init is not yet.\n");
		}
		if (o_ctrl->tcs_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
			rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
				&o_ctrl->i2c_client, MSM_CCI_INIT);
			if (rc < 0)
				pr_err("cci_init failed\n");
		}

		tcs_init_flag = TRUE;
	}
	rc = tcs_i2c_read(0x92, &read_val, 1);
	if(rc < 0 )
	{
		pr_err("TCS read chip ID fail\n");
		msm_tcs_t.tcs_error = TCS_ERR_I2C;
		return 	rc;
	}

	pr_err("exit\n");

	return rc;
}

static int32_t msm_tcs_config(struct msm_tcs_ctrl_t *tcs_ctrl,
	void __user *argp)
{
	struct msm_tcs_cfg_data *cdata =
		(struct msm_tcs_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(tcs_ctrl->tcs_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
		case CFG_TCS_INIT:{
			rc = msm_tcs_init(tcs_ctrl);
			if (rc < 0)
				pr_err("msm_tcs_init failed %d\n", rc);
			}
			break;
		case CFG_TCS_ON:{
			rc = msm_init_tcs();
			CDBG("%s: tcs is on! error_code = %d \n", __func__, rc);
			}
			break;
		case CFG_GET_TCS:{
			struct msm_tcs_info_t tcs_stat;
			uint16_t read_tcs_data = 0;
			read_tcs_data = msm_get_tcs(&tcs_stat);
			memcpy(&cdata->cfg.set_info,&tcs_stat,sizeof(cdata->cfg.set_info));
			}
			break;
		case CFG_TCS_THREAD_ON:{
			uint16_t ret = 0;
			CDBG("%s: CFG_tcs_THREAD_ON \n", __func__);
			ret = msm_tcs_thread_start();
			}
			break;
		case CFG_TCS_THREAD_OFF:{
			uint16_t ret = 0;
			CDBG("%s: CFG_tcs_THREAD_OFF \n", __func__);
			ret = msm_tcs_thread_end();
			}
			break;
		case CFG_TCS_THREAD_PAUSE:{
			uint16_t ret = 0;
			CDBG("%s: CFG_tcs_THREAD_PAUSE \n", __func__);
			ret = msm_tcs_thread_pause();
			}
			break;
		case CFG_TCS_THREAD_RESTART:{
			uint16_t ret = 0;
			CDBG("%s: CFG_tcs_THREAD_RESTART \n", __func__);
			ret = msm_tcs_thread_restart();
			}
			break;
		case CFG_TCS_POWERUP:
			rc = msm_tcs_power_up(tcs_ctrl);
			if (rc < 0) {
				pr_err("Failed tcs power up%d\n", rc);
			}
			break;
		case CFG_TCS_POWERDOWN:
			rc = msm_tcs_power_down(tcs_ctrl);
			if (rc < 0) {
				pr_err("msm_tcs_power_down failed %d\n", rc);
			}
			break;
		case CFG_TCS_AAT_MODE: {
      struct msm_tcs_info_t tcs_stat;
			uint16_t read_tcs_data = 0;
			read_tcs_data = msm_get_tcs(&tcs_stat);
			tcs_stat.status |= (msm_tcs_t.tcs_error) << 24;
			memcpy(&cdata->cfg.set_info,&tcs_stat,sizeof(cdata->cfg.set_info));
      }
      break;
		default:
			break;
	}
	mutex_unlock(tcs_ctrl->tcs_mutex);
	CDBG("Exit\n");
	return rc;
}

static long msm_tcs_subdev_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct msm_tcs_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d a_ctrl %p argp %p\n", __func__, __LINE__, a_ctrl, argp);
	switch (cmd) {
		case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
			return msm_tcs_get_subdev_id(a_ctrl, argp);
		case VIDIOC_MSM_TCS_CFG:
			return msm_tcs_config(a_ctrl, argp);
			break;
		default:
			return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_tcs_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct msm_tcs_cfg_data32 *u32 =
		(struct msm_tcs_cfg_data32 *)arg;
	struct msm_tcs_cfg_data tcs_data;
	void __user *parg = (void __user *)arg;

	CDBG("Enter\n");
	switch (cmd) {
	case VIDIOC_MSM_TCS_CFG32:
		cmd = VIDIOC_MSM_TCS_CFG;
		switch (u32->cfgtype) {
			case CFG_TCS_INIT:
			case CFG_TCS_ON:
			case CFG_TCS_OFF:
			case CFG_GET_TCS:
			case CFG_TCS_THREAD_ON:
			case CFG_TCS_THREAD_PAUSE:
			case CFG_TCS_THREAD_RESTART:
			case CFG_TCS_THREAD_OFF:
			case CFG_TCS_AAT_MODE:
				break;
			 default:
				tcs_data.cfgtype = u32->cfgtype;
				parg = &tcs_data;
				break;
		}

	}
	return msm_tcs_subdev_ioctl(sd, cmd, parg);
}

static long msm_tcs_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_tcs_subdev_do_ioctl);
}
#endif

static int32_t msm_tcs_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_tcs_ctrl_t *tcs_ctrl = v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	mutex_lock(tcs_ctrl->tcs_mutex);
	if (on) {
		rc = msm_tcs_power_up(tcs_ctrl);
	}
	else {
	  rc = msm_tcs_power_down(tcs_ctrl);
	}
	mutex_unlock(tcs_ctrl->tcs_mutex);
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_tcs_subdev_core_ops = {
	.ioctl = msm_tcs_subdev_ioctl,
	.s_power = msm_tcs_power,
};

static struct v4l2_subdev_ops msm_tcs_subdev_ops = {
	.core = &msm_tcs_subdev_core_ops,
};

static int32_t msm_tcs_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_tcs_ctrl_t *tcs_ctrl_t = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_ois_i2c_probe: client is null\n");
		return -EINVAL;
	}

	tcs_ctrl_t = kzalloc(sizeof(struct msm_tcs_ctrl_t),
		GFP_KERNEL);
	if (!tcs_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	CDBG("client = 0x%p\n", client);
	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&tcs_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", tcs_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	tcs_ctrl_t->i2c_driver = &msm_tcs_i2c_driver;
	tcs_ctrl_t->i2c_client.client = client;

	tcs_ctrl_t->tcs_device_type = MSM_CAMERA_I2C_DEVICE;
	tcs_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	tcs_ctrl_t->tcs_v4l2_subdev_ops = &msm_tcs_subdev_ops;
	tcs_ctrl_t->tcs_mutex = &msm_tcs_mutex;

	snprintf(tcs_ctrl_t->msm_sd.sd.name, sizeof(tcs_ctrl_t->msm_sd.sd.name),
			"%s", tcs_ctrl_t->i2c_driver->driver.name);

	v4l2_i2c_subdev_init(&tcs_ctrl_t->msm_sd.sd,
						tcs_ctrl_t->i2c_client.client,
						tcs_ctrl_t->tcs_v4l2_subdev_ops);
	v4l2_set_subdevdata(&tcs_ctrl_t->msm_sd.sd, tcs_ctrl_t);
	tcs_ctrl_t->msm_sd.sd.internal_ops = &msm_tcs_internal_ops;
	tcs_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&tcs_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	tcs_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	tcs_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_TCS;
	tcs_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&tcs_ctrl_t->msm_sd);
	tcs_ctrl_t->tcs_state = TCS_POWER_DOWN;

	CDBG("succeeded\n");
	CDBG("Exit\n");

probe_failure:
	kfree(tcs_ctrl_t);
	return rc;
}

static int32_t msm_tcs_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;

	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		CDBG("of_node NULL : %d\n", EINVAL);
		return -EINVAL;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
							  &pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
							&msm_tcs_t.cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_tcs_t.cci_master, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	msm_tcs_t.tcs_mutex = &msm_tcs_mutex;
	msm_tcs_t.pdev = pdev;

	msm_tcs_t.tcs_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_tcs_t.i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_tcs_t.i2c_client.cci_client = kzalloc(sizeof(struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_tcs_t.i2c_client.cci_client) {
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	msm_tcs_t.i2c_client.cci_client->sid = 0x39;    //Slave address
	msm_tcs_t.i2c_client.cci_client->retries = 3;
	msm_tcs_t.i2c_client.cci_client->id_map = 0;

	msm_tcs_t.i2c_client.cci_client->cci_i2c_master = MASTER_0;

	msm_tcs_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	cci_client = msm_tcs_t.i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	v4l2_subdev_init(&msm_tcs_t.msm_sd.sd,
					 msm_tcs_t.tcs_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_tcs_t.msm_sd.sd, &msm_tcs_t);
	msm_tcs_t.msm_sd.sd.internal_ops = &msm_tcs_internal_ops;
	msm_tcs_t.msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_tcs_t.msm_sd.sd.name,
			 ARRAY_SIZE(msm_tcs_t.msm_sd.sd.name), "msm_tcs");
	media_entity_init(&msm_tcs_t.msm_sd.sd.entity, 0, NULL, 0);
	msm_tcs_t.msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_tcs_t.msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_TCS;
	msm_tcs_t.msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_tcs_t.msm_sd);
	msm_tcs_t.tcs_state = TCS_POWER_DOWN;

	msm_tcs_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_tcs_v4l2_subdev_fops.compat_ioctl32 = msm_tcs_subdev_fops_ioctl;
#endif
	msm_tcs_t.msm_sd.sd.devnode->fops = &msm_tcs_v4l2_subdev_fops;

	msm_tcs_t.sid_tcs = msm_tcs_t.i2c_client.cci_client->sid;
	msm_tcs_t.tcs_func_tbl = NULL;
	msm_tcs_t.exit_workqueue = 1;
	msm_tcs_t.pause_workqueue = 0;
	msm_tcs_t.max_i2c_fail_thres = 5;
	msm_tcs_t.i2c_fail_cnt = 0;

	CDBG("Exit\n");

	return rc;
}

static const struct i2c_device_id msm_tcs_i2c_id[] = {
	{"qcom,tcs", (kernel_ulong_t)NULL},
	{ }
};

static const struct of_device_id msm_tcs_i2c_dt_match[] = {
	{.compatible = "qcom,tcs"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_tcs_i2c_dt_match);

static struct i2c_driver msm_tcs_i2c_driver = {
	.id_table = msm_tcs_i2c_id,
	.probe  = msm_tcs_i2c_probe,
	.remove = __exit_p(msm_tcs_i2c_remove),
	.driver = {
	.name = "qcom,tcs",
	.owner = THIS_MODULE,
	.of_match_table = msm_tcs_i2c_dt_match,
	},
};

static const struct of_device_id msm_tcs_dt_match[] = {
	{.compatible = "qcom,tcs"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_tcs_dt_match);

static struct platform_driver msm_tcs_platform_driver = {
	.driver = {
		.name = "qcom,tcs",
		.owner = THIS_MODULE,
		.of_match_table = msm_tcs_dt_match,
	},
};

static struct msm_tcs_ctrl_t msm_tcs_t = {
	.i2c_driver = &msm_tcs_i2c_driver,
	.pdriver = &msm_tcs_platform_driver,
	.tcs_v4l2_subdev_ops = &msm_tcs_subdev_ops,
};

static int __init msm_tcs_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_probe(msm_tcs_t.pdriver,
							msm_tcs_platform_probe);

	CDBG("Enter %d\n", rc);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(msm_tcs_t.i2c_driver);
}

module_init(msm_tcs_init_module);
MODULE_DESCRIPTION("MSM TCS");
MODULE_LICENSE("GPL v2");
