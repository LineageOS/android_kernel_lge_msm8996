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
/*
	Last updated : 2015/11/23, by seonyung.kim@lge.com
	change description : fix wrap around problem, 03/18
				       cover non-proxy sensor case, abnormal camera close by kill qcamera-daemon 04/12
				       non-calibration module error, increased max-convergence time 05/06
*/

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__ 

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_proxy.h"
#include "msm_cci.h"
#include "msm_proxy_i2c.h"

#define COMPATIBILITY_CUT_1_0_CUT_1_1   //need to reduce

#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
#include "vl53l0_api.h"
#include "vl53l010_api.h"
#include "vl53l0_platform.h"

u8 moduleMainVersion;
u8 moduleVersion;

#else
#include "vl53l0_api.h"
#include "vl53l0_platform.h"

u8 moduleMainVersion;
u8 moduleVersion;

#endif



DEFINE_MSM_MUTEX(msm_proxy_mutex);

#define IDENTIFICATION__MODEL_ID				0x000
#define IDENTIFICATION__REVISION_ID				0x002
#define REVISION_NOT_CALIBRATED					0x02
#define REVISION_CALIBRATED						0x030
#define FIRMWARE__BOOTUP						0x119
#define RESULT__RANGE_STATUS					0x04D
#define GPIO_HV_PAD01__CONFIG					0x132
#define SYSRANGE__MAX_CONVERGENCE_TIME			0x01C
#define SYSRANGE__RANGE_CHECK_ENABLES			0x02D
#define SYSRANGE__MAX_CONVERGENCE_TIME			0x01C
#define SYSRANGE__EARLY_CONVERGENCE_ESTIMATE	0x022
#define SYSTEM__FRESH_OUT_OF_RESET				0x016
#define SYSRANGE__PART_TO_PART_RANGE_OFFSET		0x024
#define SYSRANGE__CROSSTALK_COMPENSATION_RATE	0x01E
#define SYSRANGE__CROSSTALK_VALID_HEIGHT		0x021
#define SYSRANGE__RANGE_IGNORE_VALID_HEIGHT		0x025
#define SYSRANGE__RANGE_IGNORE_THRESHOLD		0x026
#define SYSRANGE__MAX_AMBIENT_LEVEL_MULT		0x02C
#define SYSALS__INTERMEASUREMENT_PERIOD			0x03E
#define SYSRANGE__INTERMEASUREMENT_PERIOD		0x01B
#define SYSRANGE__START							0x018
#define RESULT__RANGE_VAL						0x062
#define RESULT__RANGE_STRAY						0x063
#define RESULT__RANGE_RAW						0x064
#define RESULT__RANGE_RETURN_SIGNAL_COUNT		0x06C
#define RESULT__RANGE_REFERENCE_SIGNAL_COUNT	0x070
#define RESULT__RANGE_RETURN_AMB_COUNT			0x074
#define RESULT__RANGE_REFERENCE_AMB_COUNT		0x078
#define RESULT__RANGE_RETURN_CONV_TIME			0x07C
#define RESULT__RANGE_REFERENCE_CONV_TIME		0x080
#define SYSTEM__INTERRUPT_CLEAR					0x015
#define RESULT__INTERRUPT_STATUS_GPIO			0x04F
#define SYSTEM__MODE_GPIO1						0x011
#define SYSTEM__INTERRUPT_CONFIG_GPIO			0x014
#define RANGE__RANGE_SCALER						0x096
#define SYSRANGE__PART_TO_PART_RANGE_OFFSET		0x024
#define LOW_LIGHT_RETURN_RATE					1800
#define HIGH_LIGHT_RETURN_RATE					5000
#define LOW_LIGHT_XTALK_RATIO					100
#define HIGH_LIGHT_XTALK_RATIO					35
#define LOW_LIGHT_IGNORETHRES_RATIO				100
#define HIGH_LIGHT_IGNORETHRES_RATIO			28
#define DEFAULT_CROSSTALK					4 // 12 for ST Glass; 2 for LG Glass
#define DEFAULT_IGNORETHRES					0 // 32 fior ST Glass; 0 for LG Glass
#define FILTERNBOFSAMPLES						10
#define FILTERSTDDEVSAMPLES						6
#define MINFILTERSTDDEVSAMPLES					3
#define MINFILTERVALIDSTDDEVSAMPLES				4
#define FILTERINVALIDDISTANCE					65535
#define IT_EEP_REG								0x800
#define COMPLEX_FILTER
#define EEPROM_SLAVE_ID   						(0x54) //0xA8 >> 1

static struct v4l2_file_operations msm_proxy_v4l2_subdev_fops;
static struct i2c_driver msm_proxy_i2c_driver;

uint32_t measurementIndex = 0;
uint32_t defaultZeroVal = 0;
uint32_t defaultAvgVal = 0;
uint32_t noDelayZeroVal = 0;
uint32_t noDelayAvgVal = 0;
uint32_t previousAvgDiff = 0;
uint16_t lastTrueRange[FILTERNBOFSAMPLES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint32_t lastReturnRates[FILTERNBOFSAMPLES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint32_t previousRangeStdDev = 0;
uint32_t previousStdDevLimit = 0;
uint32_t previousReturnRateStdDev = 0;
uint16_t stdFilteredReads = 0;
uint32_t m_chipid = 0;
uint16_t lastMeasurements[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint16_t averageOnXSamples = 4;
uint16_t currentIndex = 0;
int32_t st_offset = 0;

void BabyBear_ParameterOptimization(uint32_t ambientRate);
uint32_t BabyBear_damper(uint32_t inData, uint32_t ambientRate, uint32_t LowLightRatio, uint32_t HighLightRatio);

/* LGE_CHANGE_S, proxy bring_up, 2015-09-25, seonyung.kim@lge.com */
#define EWOK_API_IMPLEMENTATION

#ifdef CONFIG_LG_BB
#define BABYBEAR_API_IMPLEMENTATION
#endif
/* LGE_CHANGE_E, proxy bring_up, 2015-09-25, seonyung.kim@lge.com */


#ifdef COMPLEX_FILTER
void VL6180_InitComplexFilter(void);
uint16_t VL6180_ComplexFilter(uint16_t m_trueRange_mm, uint16_t rawRange, uint32_t m_rtnSignalRate, uint32_t m_rtnAmbientRate, uint16_t errorCode);
uint32_t VL6180_StdDevDamper(uint32_t AmbientRate, uint32_t SignalRate, uint32_t StdDevLimitLowLight, uint32_t StdDevLimitLowLightSNR, uint32_t StdDevLimitHighLight, uint32_t StdDevLimitHighLightSNR);
#else
void VL6180_InitLiteFilter(void);
uint16_t VL6180_LiteFilter(uint16_t m_trueRange_mm, uint16_t rawRange, uint32_t m_rtnSignalRate, uint32_t m_rtnAmbientRate, uint16_t errorCode);
#endif

static struct msm_proxy_ctrl_t msm_proxy_t;

uint16_t SLAVE_ADDRESS = 0;
enum msm_camera_i2c_reg_addr_type DEVICE_INDEX_SIZE;

#ifdef BABYBEAR_API_IMPLEMENTATION
//static VL53L0_DEV Dev;
static VL6180xDev_t Dev;
static VL6180x_RangeData_t RangeData;
#endif
#ifdef EWOK_API_IMPLEMENTATION
static VL53L0_Dev_t tmp;
static VL53L0_DEV Dev = &tmp;
static VL53L0_RangingMeasurementData_t RangeData;
static VL53L0_DeviceInfo_t deviceInfo;
VL53L0_Version_t Version;
VL53L0_Version_t *pVersion	= &Version;

FixPoint1616_t SigmaLimitValue;
FixPoint1616_t SignalLimitValue;
#endif



static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,	//sungmin.woo added
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
	.i2c_write_seq = msm_camera_qup_i2c_write_seq,	//sungmin.woo added
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
	msm_camera_qup_i2c_write_table_w_microdelay,
};

static int32_t msm_proxy_get_subdev_id(struct msm_proxy_ctrl_t *proxy_ctrl,
									   void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	pr_err("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (proxy_ctrl->proxy_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = proxy_ctrl->pdev->id;
	else
		*subdev_id = proxy_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static int msm_proxy_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_proxy_ctrl_t *o_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->proxy_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}

	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_proxy_internal_ops = {
	.close = msm_proxy_close,
};

int32_t proxy_i2c_read(uint32_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type)
{
	int32_t ret = 0;

	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_proxy_t.i2c_client.cci_client;
	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_proxy_t.cci_master;
	msm_proxy_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	ret = msm_proxy_t.i2c_client.i2c_func_tbl->i2c_read(&msm_proxy_t.i2c_client, addr, &data[0], data_type);
	return ret;
}
int32_t proxy_i2c_write(uint32_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type)
{
	int32_t ret = 0;

	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_proxy_t.i2c_client.cci_client;
	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_proxy_t.cci_master;
	msm_proxy_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	ret = msm_proxy_t.i2c_client.i2c_func_tbl->i2c_write(&msm_proxy_t.i2c_client, addr, data, data_type);
	return ret;
}

int32_t proxy_i2c_write_seq(uint32_t addr, uint8_t *data, uint16_t num_byte)
{
	int32_t ret = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_proxy_t.i2c_client.cci_client;
	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_proxy_t.cci_master;
	msm_proxy_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;


	ret = msm_proxy_t.i2c_client.i2c_func_tbl->i2c_write_seq(&msm_proxy_t.i2c_client, addr, &data[0], num_byte);
	return ret;
}

int32_t proxy_i2c_read_seq(uint32_t addr, uint8_t *data, uint16_t num_byte)
{
	int32_t ret = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_proxy_t.i2c_client.cci_client;
	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_proxy_t.cci_master;
	msm_proxy_t.i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	ret = msm_proxy_t.i2c_client.i2c_func_tbl->i2c_read_seq(&msm_proxy_t.i2c_client, addr, &data[0], num_byte);
	return ret;
}


int32_t proxy_i2c_e2p_write(uint16_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type)
{
	int32_t ret = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_proxy_t.i2c_eeprom_client.cci_client;
	cci_client->sid = EEPROM_SLAVE_ID;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_proxy_t.cci_master;
	msm_proxy_t.i2c_eeprom_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	ret = msm_proxy_t.i2c_eeprom_client.i2c_func_tbl->i2c_write(&msm_proxy_t.i2c_eeprom_client, addr, data, data_type);

	return ret;
}

int32_t proxy_i2c_e2p_read(uint16_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type)
{
	int32_t ret = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	cci_client = msm_proxy_t.i2c_eeprom_client.cci_client;
	cci_client->sid = EEPROM_SLAVE_ID;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = msm_proxy_t.cci_master;
	msm_proxy_t.i2c_eeprom_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	ret = msm_proxy_t.i2c_eeprom_client.i2c_func_tbl->i2c_read(&msm_proxy_t.i2c_eeprom_client, addr, data, data_type);
	return ret;

}

int16_t OffsetCalibration(void)
{
#ifdef EWOK_API_IMPLEMENTATION
// for calibration
	int32_t offsetComp;
	int16_t offset;
	
	VL53L0_Error Status = VL53L0_ERROR_NONE; // no-target
	
	//int32_t OffsetCalibrationDataMicroMeter;
	
#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
	pr_err("OffsetCalibration start!\n");

    if (moduleVersion == 1) // cut 1.1
	{
		//VL53L0_GetOffsetCalibrationDataMicroMeter(Dev, &OffsetCalibrationDataMicroMeter);
		Status = VL53L0_PerformOffsetCalibration(Dev, 200<<16, &offsetComp); //(100) is 100mm target dis, &offsetComp is return  
		offsetComp = offsetComp - st_offset;
		
		if ((offsetComp < 31000) && (offsetComp > (-63000)) && (Status == VL53L0_ERROR_NONE) ) {
			pr_err("OffsetCalibration: spec in!\n");
		} else {
			VL53L0_RestoreOffset(Dev, st_offset);
			pr_err("OffsetCalibration: spec out!\n");
		}		
		pr_err("offsetComp:%d OffsetCalibrationDataMicroMeter: %d!\n",offsetComp, st_offset);
    }
	else if(moduleVersion == 0) //cut1.0
	{	
		//VL53L0_GetOffsetCalibrationDataMicroMeter(Dev, &OffsetCalibrationDataMicroMeter);
		VL53L0_PerformOffsetCalibration(Dev, 200<<16, &offsetComp); //(100) is 100mm target dis, &offsetComp is return
		offsetComp = offsetComp - st_offset;

	}	
	offset = offsetComp/1000;
	
	pr_err("OffsetCalibration end!\n");
	return offset;
#else
	//VL53L0_GetOffsetCalibrationDataMicroMeter(Dev, &OffsetCalibrationDataMicroMeter);
	Status = VL53L0_PerformOffsetCalibration(Dev, 200<<16, &offsetComp); //(100) is 100mm target dis, &offsetComp is return  
	offsetComp = offsetComp - st_offset;
	
	if ((offsetComp < 31000) && (offsetComp > (-63000)) && (Status == VL53L0_ERROR_NONE) ) {
		pr_err("OffsetCalibration: spec in!\n");
	} else {
		VL53L0_RestoreOffset(Dev, st_offset);
		pr_err("OffsetCalibration: spec out!\n");
	}		
	pr_err("offsetComp:%d OffsetCalibrationDataMicroMeter: %d!\n",offsetComp, st_offset);
#endif


#else
	int timeOut = 0;
	int i = 0;
	int measuredDistance = 0;
	int realDistance = 200;
	int8_t measuredOffset = 0;
	uint16_t chipidRangeStart = 0;
	uint16_t statusCode = 0;
	uint16_t distance = 0;

	pr_err("OffsetCalibration start!\n");

	proxy_i2c_write(SYSRANGE__PART_TO_PART_RANGE_OFFSET, 0, 1);
	proxy_i2c_write(SYSRANGE__CROSSTALK_COMPENSATION_RATE, 0, 1);
	proxy_i2c_write(SYSRANGE__CROSSTALK_COMPENSATION_RATE + 1, 0, 1);

	for (i = 0; i < 10; i++) {
		proxy_i2c_write(SYSRANGE__START, 1, 1);
		timeOut = 0;
		do {
			proxy_i2c_read(SYSRANGE__START, &chipidRangeStart, 1);
			proxy_i2c_read(RESULT__RANGE_STATUS, &statusCode, 1);

			timeOut += 1;
			if (timeOut > 2000)
				return -1;

		} while (!(((statusCode & 0x01) == 0x01) && (chipidRangeStart == 0x00)));
		proxy_i2c_read(RESULT__RANGE_VAL, &distance, 1);
		distance *= 3;
		measuredDistance = measuredDistance + distance;
	}
	measuredDistance = measuredDistance / 10;
	measuredOffset = (realDistance - measuredDistance) / 3;

	pr_err("OffsetCalibration end!\n");

	return measuredOffset;
#endif
}

#ifdef EWOK_API_IMPLEMENTATION
uint16_t proxy_get_from_sensor_EwokAPI(void)
{

	uint16_t dist = 0;
	VL53L0_Error api_err = 0;
	
	if (msm_proxy_t.check_init_finish != 2) {
		return 0;
	}
	
#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
					
	if (moduleVersion == 1) 
	{// cut 1.1
		api_err = VL53L0_PerformSingleRangingMeasurement(Dev, &RangeData);
	}
	else if(moduleVersion == 0) 
	{//cut1.0	
	
		VL53L010_PerformSingleRangingMeasurement(Dev, &RangeData);
	}
		
#else
	VL53L0_PerformSingleRangingMeasurement(Dev, &RangeData);
#endif
	//pr_err("get_proxy %d error: %d! status: %d\n", RangeData.RangeMilliMeter, RangeData.RangeStatus, api_err);

	if( RangeData.RangeStatus != 0 )
	{
		dist = 8190; // replace the maximum as no-target.
	}
	else
	{
		dist = RangeData.RangeMilliMeter;
	}
    msm_proxy_t.proxy_stat.true_range_millimeter =  dist;
    msm_proxy_t.proxy_stat.ambient_rtn_rate_mcps =  RangeData.AmbientRateRtnMegaCps;
    msm_proxy_t.proxy_stat.measurement_time_usec = RangeData.MeasurementTimeUsec;
    msm_proxy_t.proxy_stat.signal_rtn_rate_mcps =  RangeData.SignalRateRtnMegaCps;
    msm_proxy_t.proxy_stat.effective_spad_rtn_count =  RangeData.EffectiveSpadRtnCount;
	
	return dist;
}
#endif



#ifdef BABYBEAR_API_IMPLEMENTATION
uint16_t proxy_get_from_sensor_BBearAPI(void)
{
	uint16_t dist = 0;

	VL6180x_RangeGetMeasurementIfReady(Dev, &RangeData);
	VL6180x_RangeStartSingleShot(Dev);

    dist = RangeData.range_mm;
    msm_proxy_t.proxy_stat.proxy_val =  dist;
    msm_proxy_t.proxy_stat.proxy_amb =  RangeData.rtnAmbRate;
    msm_proxy_t.proxy_stat.proxy_conv = RangeData.rtnConvTime;
    msm_proxy_t.proxy_stat.proxy_sig =  RangeData.rtnRate;
    msm_proxy_t.proxy_stat.proxy_raw =  RangeData.FilteredData.rawRange_mm;

	return dist;
}
#endif
#ifndef BABYBEAR_API_IMPLEMENTATION
#ifndef EWOK_API_IMPLEMENTATION
uint16_t proxy_get_from_sensor(void)
{
	uint16_t dist = 0;
	uint16_t chipidInter = 0;
	int i = 0;
	int useAveraging = 0; // 1= do rolling averaring; 0 = no rolling averaging
	int nbOfValidData = 0;
	int minValidData = 4;
	uint16_t distAcc = 0;
	uint16_t newDistance = 0;
	uint16_t chipidCount = 0;
	uint32_t rawRange = 0;
	uint32_t m_rtnConvTime = 0;
	uint32_t m_rtnSignalRate = 0;
	uint32_t m_rtnAmbientRate = 0;
	uint32_t m_rtnSignalCount = 0;
	uint32_t m_refSignalCount = 0;
	uint32_t m_rtnAmbientCount = 0;
	uint32_t m_refAmbientCount = 0;
	uint32_t m_refConvTime = 0;
	uint32_t m_refSignalRate = 0;
	uint32_t m_refAmbientRate = 0;
	uint32_t cRtnSignalCountMax = 0x7FFFFFFF;
	uint32_t cDllPeriods = 6;
	uint32_t rtnSignalCountUInt = 0;
	uint32_t calcConvTime = 0;
	uint16_t chipidRangeStart = 0;
	uint16_t statusCode = 0;
	uint16_t errorCode = 0;

	proxy_i2c_read(SYSRANGE__START, &chipidRangeStart, 1);
	proxy_i2c_read(RESULT__RANGE_STATUS, &statusCode, 1);
	errorCode = statusCode >> 4;
	proxy_i2c_read(RESULT__INTERRUPT_STATUS_GPIO, &chipidInter, 1);
	proxy_i2c_read(RESULT__RANGE_VAL, &dist, 1);
	dist *= 3;
	proxy_i2c_read(RESULT__RANGE_RAW, &chipidCount, 1);

	rawRange = (uint32_t)chipidCount;

	ProxyRead32bit(RESULT__RANGE_RETURN_SIGNAL_COUNT, &rtnSignalCountUInt);

	if (rtnSignalCountUInt > cRtnSignalCountMax)
		rtnSignalCountUInt = 0;

	m_rtnSignalCount = rtnSignalCountUInt;

	ProxyRead32bit(RESULT__RANGE_REFERENCE_SIGNAL_COUNT, &m_refSignalCount);
	ProxyRead32bit(RESULT__RANGE_RETURN_AMB_COUNT, &m_rtnAmbientCount);
	ProxyRead32bit(RESULT__RANGE_REFERENCE_AMB_COUNT, &m_refAmbientCount);
	ProxyRead32bit(RESULT__RANGE_RETURN_CONV_TIME, &m_rtnConvTime);
	ProxyRead32bit(RESULT__RANGE_REFERENCE_CONV_TIME, &m_refConvTime);

	calcConvTime = m_refConvTime;
	if (m_rtnConvTime > m_refConvTime)
		calcConvTime = m_rtnConvTime;
	if (calcConvTime == 0)
		calcConvTime = 63000;

	m_rtnSignalRate  = (m_rtnSignalCount * 1000) / calcConvTime;
	m_refSignalRate  = (m_refSignalCount * 1000) / calcConvTime;
	m_rtnAmbientRate = (m_rtnAmbientCount * cDllPeriods * 1000) / calcConvTime;
	m_refAmbientRate = (m_rtnAmbientCount * cDllPeriods * 1000) / calcConvTime;

	BabyBear_ParameterOptimization((uint32_t) m_rtnAmbientRate);

	if (((statusCode & 0x01) == 0x01) && (chipidRangeStart == 0x00)) {
		if (useAveraging == 1) {
			for (i = 0; i < 7; i++) {
				lastMeasurements[i] = lastMeasurements[i + 1];
			}
			if (rawRange != 255)
				lastMeasurements[7] = dist;
			else
				lastMeasurements[7] = 65535;

			if (currentIndex < 8) {
				minValidData = (currentIndex + 1) / 2;
				currentIndex++;
			} else
				minValidData = 4;

			nbOfValidData = 0;
			distAcc = 0;
			newDistance = 255 * 3; // Max distance, equivalent as when no target
			for (i = 7; i >= 0; i--) {
				if (lastMeasurements[i] != 65535) {
					// This measurement is valid
					nbOfValidData = nbOfValidData + 1;
					distAcc = distAcc + lastMeasurements[i];
					if (nbOfValidData >= minValidData) {
						newDistance = distAcc / nbOfValidData;
						break;
					}
				}
			}
			// Copy the new distance
			dist = newDistance;
		}

#ifdef COMPLEX_FILTER
		dist = VL6180_ComplexFilter(dist, rawRange * 3, m_rtnSignalRate, m_rtnAmbientRate, errorCode);
#else
		dist = VL6180_LiteFilter(dist, rawRange * 3, m_rtnSignalRate, m_rtnAmbientRate, errorCode);
#endif

		// Start new measurement
		proxy_i2c_write(SYSRANGE__START, 0x01, 1);
		m_chipid = dist;

	}

	else
		dist = m_chipid;

	//need to check rc value here //
	msm_proxy_t.proxy_stat.proxy_val = dist;
	msm_proxy_t.proxy_stat.proxy_conv = calcConvTime;
	msm_proxy_t.proxy_stat.proxy_sig = m_rtnSignalRate;
	msm_proxy_t.proxy_stat.proxy_amb = m_rtnAmbientRate;
	msm_proxy_t.proxy_stat.proxy_raw = rawRange * 3;

	return dist;

}
#endif
#endif

static void get_proxy(struct work_struct *work)
{
	struct msm_proxy_ctrl_t *proxy_struct = container_of(work, struct msm_proxy_ctrl_t, proxy_work);
	uint16_t *proxy = &proxy_struct->last_proxy;
	int16_t offset = 0;  //0x801
	int16_t calCount = 0; //0x800
	int16_t finVal = 0; //0x800~801
	uint16_t moduleId = 0;
	uint32_t count = 0; //0x802~805 	  
	uint16_t count_sep1 = 0; //0x802~803  
	uint16_t count_sep2 = 0; //0x804~805  
	uint8_t VhvSettings = 0;  //0x806	  
	uint8_t PhaseCal = 0;  //0x807		  
	uint8_t isApertureSpads = 0;  //0x808 

	while (1) {
		if (!proxy_struct->pause_workqueue) {
			if (proxy_struct->proxy_cal) {
				proxy_struct->proxy_stat.cal_done = 0;  //cal done
/* LGE_CHANGE_S, for initialization time reduce(spad calibration), 2016-02-11, seonyung.kim@lge.com */																																					 
				VL53L0_PerformRefCalibration(Dev, &VhvSettings,&PhaseCal);													 
				VL53L0_PerformRefSpadManagement(Dev,  &count, &isApertureSpads);											 
				pr_err("perform ref calibration= %d, %d, %d, %d to eeprom\n", VhvSettings, PhaseCal, count, isApertureSpads);

 			 	offset = OffsetCalibration();
				count_sep1 = count >> 16;																					 
				count_sep2 = 0xFFFF & count;																				 
				pr_err("VL53L0 count:%d, count_sep1:%d, count_sep2:%d\n",count, count_sep1, count_sep2);					 

/* LGE_CHANGE_E, for initialization time reduce(spad calibration), 2016-02-13, seonyung.kim@lge.com */	
				//VL53L0_PerformRefSpadManagement(Dev,&refSpadCount,&isApertureSpads);
				pr_err("write offset = %x to eeprom\n", offset);

				proxy_i2c_e2p_read(0x700, &moduleId, 1);

#ifdef EWOK_API_IMPLEMENTATION
				if ((offset < 31) && (offset > (-63))) {
#else
				if ((offset < 11) && (offset > (-21))) {
#endif
					//if ((moduleId == 0x00) || (moduleId == 0x01) || (moduleId == 0x02)) {
						
						pr_err("VL53L0 VhvSettings:%d, PhaseCal:%d, isApertureSpads:%d\n",VhvSettings, PhaseCal, isApertureSpads);	 
						proxy_i2c_e2p_read(IT_EEP_REG, &finVal, 2);
						calCount = finVal >> 8;

						calCount++;
						finVal = (calCount << 8) | (0x00FF & offset);
						proxy_i2c_e2p_write(IT_EEP_REG, finVal, 2);
						msleep(1);
						proxy_i2c_e2p_write(IT_EEP_REG+2, count_sep1, 2); //0x802~803
						msleep(1);
						proxy_i2c_e2p_write(IT_EEP_REG+4, count_sep2, 2); //0x804~805	
						msleep(1);											 
						proxy_i2c_e2p_write(IT_EEP_REG+6, VhvSettings, 1);	//0x806 
						msleep(1);												 
						proxy_i2c_e2p_write(IT_EEP_REG+7, PhaseCal, 1); //0x807 	
						msleep(1);												 
						proxy_i2c_e2p_write(IT_EEP_REG+8, isApertureSpads, 1);	//0x808 	
						msleep(1);

						pr_err("KSY read inot cal count = %x to eeprom\n", finVal);
						pr_err("KSY read inot offset = %x to eeprom\n", offset);
					//}

					proxy_struct->proxy_stat.cal_count = calCount;
					proxy_struct->proxy_cal = 0;
					proxy_struct->proxy_stat.cal_done = 1;  //cal done
					msm_proxy_t.proxy_cal = 0;
				} else { // Calibration failed by spec out
					proxy_struct->proxy_stat.cal_done = 2;  //cal fail
					msm_proxy_t.proxy_cal = 0;
				}
			}
#ifdef BABYBEAR_API_IMPLEMENTATION
			*proxy = proxy_get_from_sensor_BBearAPI();
#else
	#ifdef EWOK_API_IMPLEMENTATION
			*proxy = proxy_get_from_sensor_EwokAPI();
  	#else
			*proxy = proxy_get_from_sensor();
	#endif
#endif
		}
		if (proxy_struct->i2c_fail_cnt >= proxy_struct->max_i2c_fail_thres) {
			pr_err("proxy workqueue force end due to i2c fail!\n");
			break;
		}
		msleep(53);
		if (proxy_struct->exit_workqueue)
			break;
	}
	pr_err("end workqueue!\n");
}
int16_t stop_proxy(void)
{
	pr_err("stop_proxy!\n");
	if (msm_proxy_t.exit_workqueue == 0) {
		if (msm_proxy_t.wq_init_success) {
			msm_proxy_t.exit_workqueue = 1;
			destroy_workqueue(msm_proxy_t.work_thread);
			msm_proxy_t.work_thread = NULL;
			msm_proxy_t.check_init_finish = 0;
			pr_err("destroy_workqueue!\n");
		}
	}
	return 0;
}
int16_t pause_proxy(void)
{
	pr_err("pause_proxy!\n");
	msm_proxy_t.pause_workqueue = 1;
	pr_err("pause_workqueue = %d\n", msm_proxy_t.pause_workqueue);
	return 0;
}
int16_t restart_proxy(void)
{
	pr_err("restart_proxy!\n");
	msm_proxy_t.pause_workqueue = 0;
	pr_err("pause_workqueue = %d\n", msm_proxy_t.pause_workqueue);
	return 0;
}
uint16_t msm_proxy_thread_start(void)
{
	pr_err("msm_proxy_thread_start\n");

	if (msm_proxy_t.exit_workqueue) {
		msm_proxy_t.exit_workqueue = 0;
		msm_proxy_t.work_thread = create_singlethread_workqueue("my_work_thread");
		if (!msm_proxy_t.work_thread) {
			pr_err("creating work_thread fail!\n");
			return 1;
		}

		msm_proxy_t.wq_init_success = 1;

		INIT_WORK(&msm_proxy_t.proxy_work, get_proxy);
		pr_err("INIT_WORK done!\n");

		queue_work(msm_proxy_t.work_thread, &msm_proxy_t.proxy_work);
		pr_err("queue_work done!\n");
	}
	return 0;
}
uint16_t msm_proxy_thread_end(void)
{
	uint16_t ret = 0;
	pr_err("msm_proxy_thread_end\n");
	ret = stop_proxy();
	return ret;
}
uint16_t msm_proxy_thread_pause(void)
{
	uint16_t ret = 0;
	pr_err("msm_proxy_thread_pause\n");
	ret = pause_proxy();
	return ret;
}
uint16_t msm_proxy_thread_restart(void)
{
	uint16_t ret = 0;
	pr_err("msm_proxy_thread_restart\n");
	msm_proxy_t.i2c_fail_cnt = 0;
	ret = restart_proxy();
	return ret;
}
uint16_t msm_proxy_cal(void)
{
	uint16_t ret = 0;
	pr_err("msm_proxy_cal\n");
	msm_proxy_t.proxy_cal = 1;
	return ret;
}

int32_t msm_init_proxy(void)
{
#ifdef BABYBEAR_API_IMPLEMENTATION
	return msm_init_proxy_BBearAPI();
#else
#ifdef EWOK_API_IMPLEMENTATION
	return msm_init_proxy_EwokAPI();
#else
	int rc = 0;
	int i = 0;
	uint8_t byteArray[4] = {0, 0, 0, 0};
	int8_t offsetByte = 0;
	int16_t finVal = 0;
	uint8_t calCount = 0;
	uint16_t modelID = 0;
	uint16_t revID = 0;
	uint16_t chipidRange = 0;
	uint16_t chipidRangeMax = 0;
	uint16_t chipidgpio = 0;
	uint32_t shift = 0;
	uint32_t dataMask = 0;
	uint16_t readI2C = 0x0;
	uint32_t ninepointseven = 0;
	uint16_t crosstalkHeight = 0;
	uint16_t ignoreThreshold = 0;
	uint16_t ignoreThresholdHeight = 0;
	uint16_t proxyStatus = 0;
	uint16_t proxyFatal = 0;
	uint16_t dataByte = 0;
	uint16_t ambpart2partCalib1 = 0;
	uint16_t ambpart2partCalib2 = 0;
	uint16_t moduleId = 0;
	uint8_t shiftModuleId = 0;

	pr_err("msm_init_proxy ENTER!\n");

	proxy_i2c_read(RESULT__RANGE_STATUS, &proxyStatus, 1);
	proxy_i2c_read(0x290, &proxyFatal, 1);

	if ((proxyStatus & 0x01) && ((proxyStatus >> 4) == 0) && (proxyFatal == 0))
		pr_err("init proxy alive!\n");

	else {
		pr_err("init proxy fail!, no proxy sensor found!\n");
		return -1;
	}

	proxy_i2c_read(IDENTIFICATION__MODEL_ID, &modelID, 1);
	proxy_i2c_read(IDENTIFICATION__REVISION_ID, &revID, 1);
	pr_err("Model ID : 0x%X, REVISION ID : 0x%X\n", modelID, revID);   //if revID == 2;(not calibrated), revID == 3 (calibrated)
	if (revID != REVISION_CALIBRATED) {
		pr_err("not calibrated!\n");
		//return -1;
	}

	//waitForStandby
	for (i = 0; i < 100; i++) {
		proxy_i2c_read(FIRMWARE__BOOTUP, &modelID, 1);
		if ((modelID & 0x01) == 1) {
			i = 100;
		}
	}
	//range device ready
	for (i = 0; i < 100; i++) {
		proxy_i2c_read(RESULT__RANGE_STATUS, &modelID, 1);
		if ((modelID & 0x01) == 1) {
			i = 100;
		}
	}
	//performRegisterTuningCut1_1
	proxy_i2c_write(GPIO_HV_PAD01__CONFIG, 0x30, 1);
	proxy_i2c_write(0x0207, 0x01, 1);
	proxy_i2c_write(0x0208, 0x01, 1);
	proxy_i2c_write(0x0133, 0x01, 1);
	proxy_i2c_write(0x0096, 0x00, 1);
	proxy_i2c_write(0x0097, 0xFD, 1);
	proxy_i2c_write(0x00e3, 0x00, 1);
	proxy_i2c_write(0x00e4, 0x04, 1);
	proxy_i2c_write(0x00e5, 0x02, 1);
	proxy_i2c_write(0x00e6, 0x01, 1);
	proxy_i2c_write(0x00e7, 0x03, 1);
	proxy_i2c_write(0x00f5, 0x02, 1);
	proxy_i2c_write(0x00D9, 0x05, 1);

	// AMB P2P calibration
	proxy_i2c_read(SYSTEM__FRESH_OUT_OF_RESET, &dataByte, 1);
	if (dataByte == 0x01) {
		proxy_i2c_read(0x26, &dataByte, 1);
		ambpart2partCalib1 = dataByte << 8;
		proxy_i2c_read(0x27, &dataByte, 1);
		ambpart2partCalib1 = ambpart2partCalib1 + dataByte;
		proxy_i2c_read(0x28, &dataByte, 1);
		ambpart2partCalib2 = dataByte << 8;
		proxy_i2c_read(0x29, &dataByte, 1);
		ambpart2partCalib2 = ambpart2partCalib2 + dataByte;
		if (ambpart2partCalib1 != 0) {
			// p2p calibrated
			proxy_i2c_write(0xDA, (ambpart2partCalib1 >> 8) & 0xFF, 1);
			proxy_i2c_write(0xDB, ambpart2partCalib1 & 0xFF, 1);
			proxy_i2c_write(0xDC, (ambpart2partCalib2 >> 8) & 0xFF, 1);
			proxy_i2c_write(0xDD, ambpart2partCalib2 & 0xFF, 1);
		} else {
			// No p2p Calibration, use default settings
			proxy_i2c_write(0xDB, 0xCE, 1);
			proxy_i2c_write(0xDC, 0x03, 1);
			proxy_i2c_write(0xDD, 0xF8, 1);
		}
	}
	proxy_i2c_write(0x009f, 0x00, 1);
	proxy_i2c_write(0x00a3, 0x3c, 1);
	proxy_i2c_write(0x00b7, 0x00, 1);
	proxy_i2c_write(0x00bb, 0x3c, 1);
	proxy_i2c_write(0x00b2, 0x09, 1);
	proxy_i2c_write(0x00ca, 0x09, 1);
	proxy_i2c_write(0x0198, 0x01, 1);
	proxy_i2c_write(0x01b0, 0x17, 1);
	proxy_i2c_write(0x01ad, 0x00, 1);
	proxy_i2c_write(0x00FF, 0x05, 1);
	proxy_i2c_write(0x0100, 0x05, 1);
	proxy_i2c_write(0x0199, 0x05, 1);
	proxy_i2c_write(0x0109, 0x07, 1);
	proxy_i2c_write(0x010a, 0x30, 1);
	proxy_i2c_write(0x003f, 0x46, 1);
	proxy_i2c_write(0x01a6, 0x1b, 1);
	proxy_i2c_write(0x01ac, 0x3e, 1);
	proxy_i2c_write(0x01a7, 0x1f, 1);
	proxy_i2c_write(0x0103, 0x01, 1);
	proxy_i2c_write(0x0030, 0x00, 1);
	proxy_i2c_write(0x001b, 0x0A, 1);
	proxy_i2c_write(0x003e, 0x0A, 1);
	proxy_i2c_write(0x0131, 0x04, 1);
	proxy_i2c_write(0x0011, 0x10, 1);
	proxy_i2c_write(0x0014, 0x24, 1);
	proxy_i2c_write(0x0031, 0xFF, 1);
	proxy_i2c_write(0x00d2, 0x01, 1);
	proxy_i2c_write(0x00f2, 0x01, 1);

	// RangeSetMaxConvergenceTime
	proxy_i2c_write(SYSRANGE__MAX_CONVERGENCE_TIME, 0x3F, 1);
	proxy_i2c_read(SYSRANGE__RANGE_CHECK_ENABLES, &chipidRangeMax, 1);
	chipidRangeMax = chipidRangeMax & 0xFE; // off ECE
	chipidRangeMax = chipidRangeMax | 0x02; // on ignore thr
	proxy_i2c_write(SYSRANGE__RANGE_CHECK_ENABLES, chipidRangeMax, 1);
	proxy_i2c_write(SYSRANGE__MAX_AMBIENT_LEVEL_MULT, 0xFF, 1);//SNR

	// ClearSystemFreshOutofReset
	proxy_i2c_write(SYSTEM__FRESH_OUT_OF_RESET, 0x0, 1);
	ProxyRead16bit(RANGE__RANGE_SCALER, &readI2C);

	//Range_Set_scalar
	for (i = 0; i < sizeof(uint16_t); i++) {
		shift = (sizeof(uint16_t) - i - 1) * 0x08;
		dataMask = (0xFF << shift);
		byteArray[i] = (u8)(((uint16_t)((uint16_t)85 & 0x01ff) & dataMask) >> shift);
		proxy_i2c_write(RANGE__RANGE_SCALER + i, byteArray[i], 1);
	}

	//readRangeOffset
	proxy_i2c_e2p_read(0x700, &moduleId, 2);
	shiftModuleId = moduleId >> 8;
	pr_err("KSY module ID : %d\n", shiftModuleId);

	if ((shiftModuleId == 0x00) || (shiftModuleId == 0x01) || (shiftModuleId == 0x02)) {
		proxy_i2c_e2p_read(IT_EEP_REG, &finVal, 2);
		offsetByte = 0x00FF & finVal;
		calCount = (0xFF00 & finVal) >> 8;
#ifdef EWOK_API_IMPLEMENTATION
		if ((offsetByte <= -63) || (offsetByte >= 31)|| (offsetByte >= 100)) {
#else
		if ((offsetByte <= -21) || (offsetByte >= 11) || (offsetByte >= 100)) {
#endif
			proxy_i2c_e2p_write(IT_EEP_REG, 0, 2);
			calCount = 0;
			offsetByte = 0;
		}
		msm_proxy_t.proxy_stat.cal_count = calCount;
		pr_err("inot read offset = %d from eeprom\n", offsetByte);
		proxy_i2c_write(SYSRANGE__PART_TO_PART_RANGE_OFFSET, offsetByte, 1);

	} 
	
	// Babybear_SetStraylight
	ninepointseven = 25;
	proxy_i2c_write(SYSRANGE__CROSSTALK_COMPENSATION_RATE, (ninepointseven >> 8) & 0xFF, 1);
	proxy_i2c_write(SYSRANGE__CROSSTALK_COMPENSATION_RATE + 1, ninepointseven & 0xFF, 1);

	crosstalkHeight = 40;
	proxy_i2c_write(SYSRANGE__CROSSTALK_VALID_HEIGHT, crosstalkHeight & 0xFF, 1);


	// Will ignore all low distances (<100mm) with a low return rate
	ignoreThreshold = 64; // 64 = 0.5Mcps
	ignoreThresholdHeight = 33; // 33 * scaler3 = 99mm
	proxy_i2c_write(SYSRANGE__RANGE_IGNORE_THRESHOLD, (ignoreThreshold >> 8) & 0xFF, 1);
	proxy_i2c_write(SYSRANGE__RANGE_IGNORE_THRESHOLD + 1, ignoreThreshold & 0xFF, 1);
	proxy_i2c_write(SYSRANGE__RANGE_IGNORE_VALID_HEIGHT, ignoreThresholdHeight & 0xFF, 1);

	// Init of Averaging samples : in case of adding glass
	for (i = 0; i < 8; i++) {
		lastMeasurements[i] = 65535; // 65535 means no valid data
	}
	currentIndex = 0;

	// SetSystemInterruptConfigGPIORanging
	proxy_i2c_read(SYSTEM__INTERRUPT_CONFIG_GPIO, &chipidgpio, 1);
	proxy_i2c_write(SYSTEM__INTERRUPT_CONFIG_GPIO, (chipidgpio | 0x04), 1);


	//RangeSetSystemMode
	chipidRange = 0x01;
	proxy_i2c_write(SYSRANGE__START, chipidRange, 1);

#ifdef COMPLEX_FILTER
	VL6180_InitComplexFilter();
#else
	VL6180_InitLiteFilter();
#endif

	return rc;
	#endif // EWOK_API_IMPLEMENTATION
#endif //BabyBear implementation
}
uint16_t msm_get_proxy(struct msm_proxy_info_t *proxy_info)
{
	uint16_t proxy = 0;
	proxy = msm_proxy_t.last_proxy;

	memcpy(proxy_info, &msm_proxy_t.proxy_stat, sizeof(msm_proxy_t.proxy_stat));

	return proxy;
}
void BabyBear_ParameterOptimization(uint32_t ambientRate)
{
	uint32_t newCrossTalk = 0;
	uint32_t newIgnoreThreshold = 0;

	// Compute new values
	newCrossTalk = BabyBear_damper(DEFAULT_CROSSTALK, ambientRate, LOW_LIGHT_XTALK_RATIO, HIGH_LIGHT_XTALK_RATIO);
	newIgnoreThreshold = BabyBear_damper(DEFAULT_IGNORETHRES, ambientRate, LOW_LIGHT_IGNORETHRES_RATIO, HIGH_LIGHT_IGNORETHRES_RATIO);
	// Program new values
	proxy_i2c_write(SYSRANGE__CROSSTALK_COMPENSATION_RATE, (newCrossTalk >> 8) & 0xFF, 1);
	proxy_i2c_write(SYSRANGE__CROSSTALK_COMPENSATION_RATE + 1, newCrossTalk & 0xFF, 1);

}



uint32_t BabyBear_damper(uint32_t inData, uint32_t ambientRate, uint32_t LowLightRatio, uint32_t HighLightRatio)
{
	int weight = 0;
	uint32_t newVal = 0;

	if (ambientRate <= LOW_LIGHT_RETURN_RATE) {
		weight = LowLightRatio;
	} else {
		if (ambientRate >= HIGH_LIGHT_RETURN_RATE)
			weight = HighLightRatio;
		else
			weight = (int)LowLightRatio + (((int)ambientRate - LOW_LIGHT_RETURN_RATE) * ((int)HighLightRatio - (int)LowLightRatio) / (HIGH_LIGHT_RETURN_RATE - LOW_LIGHT_RETURN_RATE));

	}

	newVal = (inData * weight) / 100;

	return newVal;
}


void VL6180_InitLiteFilter(void)
{
	measurementIndex = 0;
	defaultZeroVal = 0;
	defaultAvgVal = 0;
	noDelayZeroVal = 0;
	noDelayAvgVal = 0;
	previousAvgDiff = 0;
}

uint16_t VL6180_LiteFilter(uint16_t m_trueRange_mm, uint16_t rawRange, uint32_t m_rtnSignalRate, uint32_t m_rtnAmbientRate, uint16_t errorCode)
{
	uint16_t m_newTrueRange_mm = 0;
	uint16_t maxOrInvalidDistance = 0;
	uint16_t registerValue = 0;
	uint32_t register32BitsValue1 = 0;
	uint32_t register32BitsValue2 = 0;
	uint16_t bypassFilter = 0;
	uint32_t VAvgDiff = 0;
	uint32_t idealVAvgDiff = 0;
	uint32_t minVAvgDiff = 0;
	uint32_t maxVAvgDiff = 0;
	uint16_t wrapAroundLowRawRangeLimit = 20;
	uint32_t wrapAroundLowReturnRateLimit = 800;
	uint16_t wrapAroundLowRawRangeLimit2 = 55;
	uint32_t wrapAroundLowReturnRateLimit2 = 300;
	uint32_t wrapAroundLowReturnRateFilterLimit = 600;
	uint16_t wrapAroundHighRawRangeFilterLimit = 350;
	uint32_t wrapAroundHighReturnRateFilterLimit = 900;
	uint32_t wrapAroundMaximumAmbientRateFilterLimit = 7500;
	uint32_t maxVAvgDiff2 = 1800;
	uint8_t wrapAroundDetected = 0;

	// Determines max distance
	maxOrInvalidDistance = (uint16_t)(255 * 3);

	// Check if distance is Valid or not
	switch (errorCode) {
	case 0x0C:
		m_trueRange_mm = maxOrInvalidDistance;
		break;
	case 0x0D:
		m_trueRange_mm = maxOrInvalidDistance;
		break;
	default:
		if (rawRange >= maxOrInvalidDistance)
			m_trueRange_mm = maxOrInvalidDistance;
		break;
	}

	if ((rawRange < wrapAroundLowRawRangeLimit) && (m_rtnSignalRate < wrapAroundLowReturnRateLimit))
		m_trueRange_mm = maxOrInvalidDistance;

	if ((rawRange < wrapAroundLowRawRangeLimit2) && (m_rtnSignalRate < wrapAroundLowReturnRateLimit2))
		m_newTrueRange_mm = maxOrInvalidDistance;

	bypassFilter = 0;

	if (m_rtnAmbientRate > wrapAroundMaximumAmbientRateFilterLimit)
		bypassFilter = 1;

	if (!(((rawRange < wrapAroundHighRawRangeFilterLimit) && (m_rtnSignalRate < wrapAroundLowReturnRateFilterLimit)) ||
		  ((rawRange >= wrapAroundHighRawRangeFilterLimit) && (m_rtnSignalRate < wrapAroundHighReturnRateFilterLimit))
		 ))
		bypassFilter = 1;

	proxy_i2c_read(0x01AC, &registerValue, 1);
	if (bypassFilter == 1) {
		// Do not go through the filter
		if (registerValue != 0x3E)
			proxy_i2c_write(0x01AC, 0x3E, 1);
	} else {
		// Go through the filter
		ProxyRead32bit(0x010C, &register32BitsValue1);
		ProxyRead32bit(0x0110, &register32BitsValue2);

		if (registerValue == 0x3E) {
			defaultZeroVal = register32BitsValue1;
			defaultAvgVal = register32BitsValue2;
			proxy_i2c_write(0x01AC, 0x3F, 1);
		} else {
			noDelayZeroVal = register32BitsValue1;
			noDelayAvgVal = register32BitsValue2;
			proxy_i2c_write(0x01AC, 0x3E, 1);
		}

		// Computes current VAvgDiff
		if (defaultAvgVal > noDelayAvgVal)
			VAvgDiff = defaultAvgVal - noDelayAvgVal;
		else
			VAvgDiff = 0;
		previousAvgDiff = VAvgDiff;

		// Check the VAvgDiff
		idealVAvgDiff = defaultZeroVal - noDelayZeroVal;
		if (idealVAvgDiff > maxVAvgDiff2)
			minVAvgDiff = idealVAvgDiff - maxVAvgDiff2;
		else
			minVAvgDiff = 0;
		maxVAvgDiff = idealVAvgDiff + maxVAvgDiff2;
		if (VAvgDiff < minVAvgDiff || VAvgDiff > maxVAvgDiff)
			wrapAroundDetected = 1;
	}
	if (wrapAroundDetected == 1)
		m_newTrueRange_mm = maxOrInvalidDistance;
	else
		m_newTrueRange_mm = m_trueRange_mm;

	measurementIndex = measurementIndex + 1;

	return m_newTrueRange_mm;
}

void VL6180_InitComplexFilter(void)
{
	int i = 0;

	measurementIndex = 0;
	defaultZeroVal = 0;
	defaultAvgVal = 0;
	noDelayZeroVal = 0;
	noDelayAvgVal = 0;
	previousAvgDiff = 0;
	stdFilteredReads = 0;
	previousRangeStdDev = 0;
	previousReturnRateStdDev = 0;

	for (i = 0; i < FILTERNBOFSAMPLES; i++) {
		lastTrueRange[i] = FILTERINVALIDDISTANCE;
		lastReturnRates[i] = 0;
	}
}

uint32_t VL6180_StdDevDamper(uint32_t AmbientRate, uint32_t SignalRate, uint32_t StdDevLimitLowLight, uint32_t StdDevLimitLowLightSNR, uint32_t StdDevLimitHighLight, uint32_t StdDevLimitHighLightSNR)
{
	uint32_t newStdDev = 0;
	uint16_t snr = 0;

	if (AmbientRate > 0)
		snr = (uint16_t)((100 * SignalRate) / AmbientRate);
	else
		snr = 9999;

	if (snr >= StdDevLimitLowLightSNR)
		newStdDev = StdDevLimitLowLight;
	else {
		if (snr <= StdDevLimitHighLightSNR)
			newStdDev = StdDevLimitHighLight;
		else
			newStdDev = (uint32_t)(StdDevLimitHighLight + (snr - StdDevLimitHighLightSNR) * (int)(StdDevLimitLowLight - StdDevLimitHighLight) / (StdDevLimitLowLightSNR - StdDevLimitHighLightSNR));
	}

	return newStdDev;
}
uint16_t VL6180_ComplexFilter(uint16_t m_trueRange_mm, uint16_t rawRange, uint32_t m_rtnSignalRate, uint32_t m_rtnAmbientRate, uint16_t errorCode)
{
	uint16_t m_newTrueRange_mm = 0;
	uint16_t i = 0;
	uint16_t bypassFilter = 0;
	uint16_t registerValue = 0;
	uint32_t register32BitsValue1 = 0;
	uint32_t register32BitsValue2 = 0;
	uint16_t validDistance = 0;
	uint16_t maxOrInvalidDistance = 0;
	uint16_t wrapAroundFlag = 0;
	uint16_t noWrapAroundFlag = 0;
	uint16_t noWrapAroundHighConfidenceFlag = 0;
	uint16_t flushFilter = 0;
	uint32_t rateChange = 0;
	uint16_t stdDevSamples = 0;
	uint32_t stdDevDistanceSum = 0;
	uint32_t stdDevDistanceMean = 0;
	uint32_t stdDevDistance = 0;
	uint32_t stdDevRateSum = 0;
	uint32_t stdDevRateMean = 0;
	uint32_t stdDevRate = 0;
	uint32_t stdDevLimitWithTargetMove = 0;
	uint32_t VAvgDiff = 0;
	uint32_t idealVAvgDiff = 0;
	uint32_t minVAvgDiff = 0;
	uint32_t maxVAvgDiff = 0;
	uint16_t wrapAroundLowRawRangeLimit = 20;
	uint32_t wrapAroundLowReturnRateLimit = 800;
	uint16_t wrapAroundLowRawRangeLimit2 = 55;
	uint32_t wrapAroundLowReturnRateLimit2 = 300;
	uint32_t wrapAroundLowReturnRateFilterLimit = 600;
	uint16_t wrapAroundHighRawRangeFilterLimit = 350;
	uint32_t wrapAroundHighReturnRateFilterLimit = 900;
	uint32_t wrapAroundMaximumAmbientRateFilterLimit = 7500;
	uint32_t MinReturnRateFilterFlush = 75;
	uint32_t MaxReturnRateChangeFilterFlush = 50;
	uint32_t stdDevLimit = 300;
	uint32_t stdDevLimitLowLight = 300;
	uint32_t stdDevLimitLowLightSNR = 30; // 0.3
	uint32_t stdDevLimitHighLight = 2500;
	uint32_t stdDevLimitHighLightSNR = 5; //0.05
	uint32_t stdDevHighConfidenceSNRLimit = 8;
	uint32_t stdDevMovingTargetStdDevLimit = 90000;
	uint32_t stdDevMovingTargetReturnRateLimit = 3500;
	uint32_t stdDevMovingTargetStdDevForReturnRateLimit = 5000;
	uint32_t maxVAvgDiff2 = 1800;
	uint16_t wrapAroundNoDelayCheckPeriod = 2;
	uint16_t stdFilteredReadsIncrement = 2;
	uint16_t stdMaxFilteredReads = 4;

	// End Filter Parameters
	maxOrInvalidDistance = (uint16_t)(255 * 3);
	// Check if distance is Valid or not
	switch (errorCode) {
	case 0x0C:
		m_trueRange_mm = maxOrInvalidDistance;
		validDistance = 0;
		break;
	case 0x0D:
		m_trueRange_mm = maxOrInvalidDistance;
		validDistance = 1;
		break;
	default:
		if (rawRange >= maxOrInvalidDistance)
			validDistance = 0;
		else
			validDistance = 1;
		break;
	}
	m_newTrueRange_mm = m_trueRange_mm;

	// Checks on low range data
	if ((rawRange < wrapAroundLowRawRangeLimit) && (m_rtnSignalRate < wrapAroundLowReturnRateLimit)) {
		//Not Valid distance
		m_newTrueRange_mm = maxOrInvalidDistance;
		bypassFilter = 1;
	}
	if ((rawRange < wrapAroundLowRawRangeLimit2) && (m_rtnSignalRate < wrapAroundLowReturnRateLimit2)) {
		//Not Valid distance
		m_newTrueRange_mm = maxOrInvalidDistance;
		bypassFilter = 1;
	}

	// Checks on Ambient rate level
	if (m_rtnAmbientRate > wrapAroundMaximumAmbientRateFilterLimit) {
		// Too high ambient rate
		flushFilter = 1;
		bypassFilter = 1;
	}
	// Checks on Filter flush
	if (m_rtnSignalRate < MinReturnRateFilterFlush) {
		// Completely lost target, so flush the filter
		flushFilter = 1;
		bypassFilter = 1;
	}
	if (lastReturnRates[0] != 0) {
		if (m_rtnSignalRate > lastReturnRates[0])
			rateChange = (100 * (m_rtnSignalRate - lastReturnRates[0])) / lastReturnRates[0];
		else
			rateChange = (100 * (lastReturnRates[0] - m_rtnSignalRate)) / lastReturnRates[0];
	} else
		rateChange = 0;
	if (rateChange > MaxReturnRateChangeFilterFlush)
		flushFilter = 1;

	if (flushFilter == 1) {
		measurementIndex = 0;
		for (i = 0; i < FILTERNBOFSAMPLES; i++) {
			lastTrueRange[i] = FILTERINVALIDDISTANCE;
			lastReturnRates[i] = 0;
		}
	} else {
		for (i = (uint16_t)(FILTERNBOFSAMPLES - 1); i > 0; i--) {
			lastTrueRange[i] = lastTrueRange[i - 1];
			lastReturnRates[i] = lastReturnRates[i - 1];
		}
	}
	if (validDistance == 1)
		lastTrueRange[0] = m_trueRange_mm;
	else
		lastTrueRange[0] = FILTERINVALIDDISTANCE;
	lastReturnRates[0] = m_rtnSignalRate;

	// Check if we need to go through the filter or not
	if (!(((rawRange < wrapAroundHighRawRangeFilterLimit) && (m_rtnSignalRate < wrapAroundLowReturnRateFilterLimit)) ||
		  ((rawRange >= wrapAroundHighRawRangeFilterLimit) && (m_rtnSignalRate < wrapAroundHighReturnRateFilterLimit))
		 ))
		bypassFilter = 1;

	// Check which kind of measurement has been made
	proxy_i2c_read(0x01AC, &registerValue, 1);

	// Read data for filtering
	ProxyRead32bit(0x010C, &register32BitsValue1);
	ProxyRead32bit(0x0110, &register32BitsValue2);

	if (registerValue == 0x3E) {
		defaultZeroVal = register32BitsValue1;
		defaultAvgVal = register32BitsValue2;
	} else {
		noDelayZeroVal = register32BitsValue1;
		noDelayAvgVal = register32BitsValue2;
	}

	if (bypassFilter == 1) {
		// Do not go through the filter
		if (registerValue != 0x3E)
			proxy_i2c_write(0x01AC, 0x3E, 1);
		// Set both Defaut and NoDelay To same value
		defaultZeroVal = register32BitsValue1;
		defaultAvgVal = register32BitsValue2;
		noDelayZeroVal = register32BitsValue1;
		noDelayAvgVal = register32BitsValue2;
		measurementIndex = 0;

		// Return immediately
		return m_newTrueRange_mm;
	}

	if (measurementIndex % wrapAroundNoDelayCheckPeriod == 0)
		proxy_i2c_write(0x01AC, 0x3F, 1);
	else
		proxy_i2c_write(0x01AC, 0x3E, 1);
	measurementIndex = (uint16_t)(measurementIndex + 1);

	// Computes current VAvgDiff
	if (defaultAvgVal > noDelayAvgVal)
		VAvgDiff = defaultAvgVal - noDelayAvgVal;
	else
		VAvgDiff = 0;
	previousAvgDiff = VAvgDiff;

	// Check the VAvgDiff
	if (defaultZeroVal > noDelayZeroVal)
		idealVAvgDiff = defaultZeroVal - noDelayZeroVal;
	else
		idealVAvgDiff = noDelayZeroVal - defaultZeroVal;
	if (idealVAvgDiff > maxVAvgDiff2)
		minVAvgDiff = idealVAvgDiff - maxVAvgDiff2;
	else
		minVAvgDiff = 0;
	maxVAvgDiff = idealVAvgDiff  + maxVAvgDiff2;
	if (VAvgDiff < minVAvgDiff || VAvgDiff > maxVAvgDiff)
		wrapAroundFlag = 1;
	else {
		// Go through filtering check

		// stdDevLimit Damper on SNR
		stdDevLimit = VL6180_StdDevDamper(m_rtnAmbientRate, m_rtnSignalRate, stdDevLimitLowLight, stdDevLimitLowLightSNR, stdDevLimitHighLight, stdDevLimitHighLightSNR);

		// Standard deviations computations
		stdDevSamples = 0;
		stdDevDistanceSum = 0;
		stdDevDistanceMean = 0;
		stdDevDistance = 0;
		stdDevRateSum = 0;
		stdDevRateMean = 0;
		stdDevRate = 0;
		for (i = 0; (i < FILTERNBOFSAMPLES) && (stdDevSamples < FILTERSTDDEVSAMPLES); i++) {
			if (lastTrueRange[i] != FILTERINVALIDDISTANCE) {
				stdDevSamples = (uint16_t)(stdDevSamples + 1);
				stdDevDistanceSum = (uint32_t)(stdDevDistanceSum + lastTrueRange[i]);
				stdDevRateSum = (uint32_t)(stdDevRateSum + lastReturnRates[i]);
			}
		}
		if (stdDevSamples > 0) {
			stdDevDistanceMean = (uint32_t)(stdDevDistanceSum / stdDevSamples);
			stdDevRateMean = (uint32_t)(stdDevRateSum / stdDevSamples);
		}
		stdDevSamples = 0;
		stdDevDistanceSum = 0;
		stdDevRateSum = 0;
		for (i = 0; (i < FILTERNBOFSAMPLES) && (stdDevSamples < FILTERSTDDEVSAMPLES); i++) {
			if (lastTrueRange[i] != FILTERINVALIDDISTANCE) {
				stdDevSamples = (uint16_t)(stdDevSamples + 1);
				stdDevDistanceSum = (uint32_t)(stdDevDistanceSum + (int)(lastTrueRange[i] - stdDevDistanceMean) * (int)(lastTrueRange[i] - stdDevDistanceMean));
				stdDevRateSum = (uint32_t)(stdDevRateSum + (int)(lastReturnRates[i] - stdDevRateMean) * (int)(lastReturnRates[i] - stdDevRateMean));
			}
		}
		if (stdDevSamples >= MINFILTERSTDDEVSAMPLES) {
			stdDevDistance = (uint16_t)(stdDevDistanceSum / stdDevSamples);
			stdDevRate = (uint16_t)(stdDevRateSum / stdDevSamples);
		} else {
			stdDevDistance = 0;
			stdDevRate = 0;
		}

		// Check Return rate standard deviation
		if (stdDevRate < stdDevMovingTargetStdDevLimit) {
			if (stdDevSamples < MINFILTERVALIDSTDDEVSAMPLES) {
				if (measurementIndex < FILTERSTDDEVSAMPLES)
					m_newTrueRange_mm = 800;
				else
					m_newTrueRange_mm = maxOrInvalidDistance;
			} else {
				// Check distance standard deviation
				if (stdDevRate < stdDevMovingTargetReturnRateLimit)
					stdDevLimitWithTargetMove = stdDevLimit + (((stdDevMovingTargetStdDevForReturnRateLimit - stdDevLimit) * stdDevRate) / stdDevMovingTargetReturnRateLimit);
				else
					stdDevLimitWithTargetMove = stdDevMovingTargetStdDevForReturnRateLimit;

				if ((stdDevDistance * stdDevHighConfidenceSNRLimit) < stdDevLimitWithTargetMove)
					noWrapAroundHighConfidenceFlag = 1;
				else {
					if (stdDevDistance < stdDevLimitWithTargetMove) {
						if (stdDevSamples >= MINFILTERVALIDSTDDEVSAMPLES)
							noWrapAroundFlag = 1;
						else
							m_newTrueRange_mm = maxOrInvalidDistance;
					} else
						wrapAroundFlag = 1;
				}
			}
		} else
			wrapAroundFlag = 1;
	}

	if (m_newTrueRange_mm == maxOrInvalidDistance) {
		if (stdFilteredReads > 0)
			stdFilteredReads = (uint16_t)(stdFilteredReads - 1);
	} else {
		if (wrapAroundFlag == 1) {
			m_newTrueRange_mm = maxOrInvalidDistance;
			stdFilteredReads = (uint16_t)(stdFilteredReads + stdFilteredReadsIncrement);
			if (stdFilteredReads > stdMaxFilteredReads)
				stdFilteredReads = stdMaxFilteredReads;
		} else {
			if (noWrapAroundFlag == 1) {
				if (stdFilteredReads > 0) {
					m_newTrueRange_mm = maxOrInvalidDistance;
					if (stdFilteredReads > stdFilteredReadsIncrement)
						stdFilteredReads = (uint16_t)(stdFilteredReads - stdFilteredReadsIncrement);
					else
						stdFilteredReads = 0;
				}
			} else {
				if (noWrapAroundHighConfidenceFlag == 1)
					stdFilteredReads = 0;
			}
		}

	}
	previousRangeStdDev = stdDevDistance;
	previousReturnRateStdDev = stdDevRate;
	previousStdDevLimit = stdDevLimitWithTargetMove;

	return m_newTrueRange_mm;
}

static int32_t msm_proxy_vreg_control(struct msm_proxy_ctrl_t *o_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_proxy_vreg *vreg_cfg;

	vreg_cfg = &o_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_PROXY_MAX_VREGS) {
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

static int32_t msm_proxy_power_up(struct msm_proxy_ctrl_t *o_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);

	rc = msm_proxy_vreg_control(o_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}
	o_ctrl->proxy_state = PROXY_POWER_UP;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_proxy_power_down(struct msm_proxy_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	if (o_ctrl->proxy_state != PROXY_POWER_DOWN) {

    rc = msm_proxy_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		rc = stop_proxy();
    msm_proxy_t.exit_workqueue = 1;
		o_ctrl->proxy_state = PROXY_POWER_DOWN;
	}
	CDBG("Exit\n");
	return rc;
}

static int msm_proxy_init(struct msm_proxy_ctrl_t *o_ctrl)
{
	int rc = 0;	
	uint32_t retry = 0;
    uint16_t revision_id = 0;
	CDBG("Enter\n");

	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}

	if (o_ctrl->proxy_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");

		msm_proxy_t.check_init_finish = 0;

		for (retry = 0; retry < 3; retry++) {
			rc = proxy_i2c_read(0xc2, &revision_id, 1);
			
			if (rc < 0) { 
				pr_err("i2c failed\n");
				msleep(1);
				continue;
			} else {
				break;
			}
		}
	}
	return rc;
}

static int32_t msm_proxy_config(struct msm_proxy_ctrl_t *proxy_ctrl,
	void __user *argp)
{
	struct msm_proxy_cfg_data *cdata =
		(struct msm_proxy_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(proxy_ctrl->proxy_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
      case CFG_PROXY_INIT:
        rc = msm_proxy_init(proxy_ctrl); 
		if (rc < 0)
			    pr_err("msm_proxy_init failed %d\n", rc);
        break;
      case CFG_PROXY_ON:{
        rc = msm_init_proxy();
        pr_err("%s: Proxy is on! error_code = %d  \n", __func__, rc);
        break;
      } 
      case CFG_GET_PROXY:{
        struct msm_proxy_info_t proxy_stat;
        uint16_t read_proxy_data = 0;
        read_proxy_data = msm_get_proxy(&proxy_stat);

        memcpy(&cdata->cfg.set_info,&proxy_stat,sizeof(cdata->cfg.set_info));
#if 0
        CDBG("%s: Get Proxy data! proxy_val is = %d \n", __func__, proxy_stat.proxy_val);
        CDBG("%s: Get Proxy data! proxy_conv is = %d \n", __func__, proxy_stat.proxy_conv);
        CDBG("%s: Get Proxy data! proxy_sig is = %d \n", __func__, proxy_stat.proxy_sig);
        CDBG("%s: Get Proxy data! proxy_amb is = %d \n", __func__, proxy_stat.proxy_amb);
        CDBG("%s: Get Proxy data! proxy_raw is = %d \n", __func__, proxy_stat.proxy_raw);
        CDBG("%s: Get Proxy data! cal_count is = %d \n", __func__, proxy_stat.cal_count);
        CDBG("%s: Get Proxy data! cal_done is = %d \n", __func__, proxy_stat.cal_done);
        CDBG("%s: Get Proxy data! Range is = %d \n", __func__, read_proxy_data);
#endif
        }
        break;
      case CFG_PROXY_THREAD_ON:{
        uint16_t ret = 0;
        CDBG("%s: CFG_PROXY_THREAD_ON \n", __func__);
        ret = msm_proxy_thread_start();
        }
        break;
      case CFG_PROXY_THREAD_OFF:{
        uint16_t ret = 0;
        CDBG("%s: CFG_PROXY_THREAD_OFF \n", __func__);
        ret = msm_proxy_thread_end();
        }
        break;
      case CFG_PROXY_THREAD_PAUSE:{
        uint16_t ret = 0;
        CDBG("%s: CFG_PROXY_THREAD_PAUSE \n", __func__);
        ret = msm_proxy_thread_pause();
        }
        break;
      case CFG_PROXY_THREAD_RESTART:{
        uint16_t ret = 0;
        CDBG("%s: CFG_PROXY_THREAD_RESTART \n", __func__);
        ret = msm_proxy_thread_restart();
        }
        break;
      case CFG_PROXY_CAL:{
        uint16_t ret = 0;
        CDBG("%s: CFG_PROXY_CAL \n", __func__);
        ret = msm_proxy_cal();
        }
        break;
      case CFG_PROXY_POWERUP:
        rc = msm_proxy_power_up(proxy_ctrl);
        if (rc < 0)
          pr_err("Failed proxy power up%d\n", rc);
        break;
      case CFG_PROXY_POWERDOWN:
		rc = msm_proxy_power_down(proxy_ctrl);
	if (rc < 0)
		pr_err("msm_proxy_power_down failed %d\n", rc);
        break;
	default:
		break;
	}
	mutex_unlock(proxy_ctrl->proxy_mutex);
	CDBG("Exit\n");
	return rc;
}

static long msm_proxy_subdev_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct msm_proxy_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d a_ctrl %p argp %p\n", __func__, __LINE__, a_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_proxy_get_subdev_id(a_ctrl, argp);
  case VIDIOC_MSM_PROXY_CFG:
		return msm_proxy_config(a_ctrl, argp);
    break;
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_proxy_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct msm_proxy_cfg_data32 *u32 =
		(struct msm_proxy_cfg_data32 *)arg;
	struct msm_proxy_cfg_data proxy_data;
	void __user *parg = (void __user *)arg;

	CDBG("Enter\n");
	switch (cmd) {
	case VIDIOC_MSM_PROXY_CFG32:
		cmd = VIDIOC_MSM_PROXY_CFG;
		switch (u32->cfgtype) {
      case CFG_PROXY_INIT:
      case CFG_PROXY_ON:
      case CFG_PROXY_OFF:
      case CFG_GET_PROXY:
      case CFG_PROXY_THREAD_ON:
      case CFG_PROXY_THREAD_PAUSE:
      case CFG_PROXY_THREAD_RESTART:
      case CFG_PROXY_THREAD_OFF:
      case CFG_PROXY_CAL:
        break;
      default:
        proxy_data.cfgtype = u32->cfgtype;
        parg = &proxy_data;
        break;
		}

	}
	return msm_proxy_subdev_ioctl(sd, cmd, parg);
}

static long msm_proxy_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_proxy_subdev_do_ioctl);
}
#endif

static int32_t msm_proxy_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_proxy_ctrl_t *proxy_ctrl = v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	mutex_lock(proxy_ctrl->proxy_mutex);
  if (on)
		rc = msm_proxy_power_up(proxy_ctrl);
	else {
	  rc = msm_proxy_power_down(proxy_ctrl);
	}
	mutex_unlock(proxy_ctrl->proxy_mutex);
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_proxy_subdev_core_ops = {
	.ioctl = msm_proxy_subdev_ioctl,
	.s_power = msm_proxy_power,
};

static struct v4l2_subdev_ops msm_proxy_subdev_ops = {
	.core = &msm_proxy_subdev_core_ops,
};

static int32_t msm_proxy_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_proxy_ctrl_t *proxy_ctrl_t = NULL;
	pr_err("Enter\n");

	if (client == NULL) {
		pr_err("msm_ois_i2c_probe: client is null\n");
		return -EINVAL;
	}

	proxy_ctrl_t = kzalloc(sizeof(struct msm_proxy_ctrl_t),
		GFP_KERNEL);
	if (!proxy_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	CDBG("client = 0x%p\n", client);
	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&proxy_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", proxy_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		kfree(proxy_ctrl_t);
		return rc;
	}

	proxy_ctrl_t->i2c_driver = &msm_proxy_i2c_driver;
	proxy_ctrl_t->i2c_client.client = client;

	proxy_ctrl_t->proxy_device_type = MSM_CAMERA_I2C_DEVICE;
	proxy_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	proxy_ctrl_t->proxy_v4l2_subdev_ops = &msm_proxy_subdev_ops;
	proxy_ctrl_t->proxy_mutex = &msm_proxy_mutex;

	snprintf(proxy_ctrl_t->msm_sd.sd.name, sizeof(proxy_ctrl_t->msm_sd.sd.name),
			 "%s", proxy_ctrl_t->i2c_driver->driver.name);

	v4l2_i2c_subdev_init(&proxy_ctrl_t->msm_sd.sd,
						 proxy_ctrl_t->i2c_client.client,
						 proxy_ctrl_t->proxy_v4l2_subdev_ops);
	v4l2_set_subdevdata(&proxy_ctrl_t->msm_sd.sd, proxy_ctrl_t);
	proxy_ctrl_t->msm_sd.sd.internal_ops = &msm_proxy_internal_ops;
	proxy_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&proxy_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	proxy_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	proxy_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_PROXY;
	proxy_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&proxy_ctrl_t->msm_sd);
	proxy_ctrl_t->proxy_state = PROXY_POWER_DOWN;

	CDBG("succeeded\n");
	pr_err("Exit\n");

probe_failure:
	kfree(proxy_ctrl_t);
	return rc;
}

static int32_t msm_proxy_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;

	struct msm_camera_cci_client *cci_client = NULL;
#ifdef CONFIG_LG_PROXY
	struct msm_camera_cci_client *cci_eeprom_client = NULL;
#endif
#ifdef BABYBEAR_API_IMPLEMENTATION
	SLAVE_ADDRESS = 0x29;
	DEVICE_INDEX_SIZE = MSM_CAMERA_I2C_WORD_ADDR; //16 bit index
#else
	#ifdef EWOK_API_IMPLEMENTATION
		SLAVE_ADDRESS = 0x29;
		DEVICE_INDEX_SIZE = MSM_CAMERA_I2C_BYTE_ADDR; // 8 bit index
		//Dev->I2cDevAddr = SLAVE_ADDRESS;
		//Dev->comms_type = 0x01;
		//Dev->comms_speed_khz = 400;
	#else
		SLAVE_ADDRESS = 0x29;
	#endif
#endif

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
							  &msm_proxy_t.cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_proxy_t.cci_master, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}

 	msm_proxy_t.proxy_mutex = &msm_proxy_mutex;
	msm_proxy_t.pdev = pdev;

	msm_proxy_t.proxy_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_proxy_t.i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_proxy_t.i2c_client.cci_client = kzalloc(sizeof(
											struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_proxy_t.i2c_client.cci_client) {
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	msm_proxy_t.i2c_client.cci_client->sid = SLAVE_ADDRESS;    //Slave address
	msm_proxy_t.i2c_client.cci_client->retries = 3;
	msm_proxy_t.i2c_client.cci_client->id_map = 0;

	msm_proxy_t.i2c_client.cci_client->cci_i2c_master = MASTER_0;

	msm_proxy_t.i2c_client.addr_type = DEVICE_INDEX_SIZE;

	cci_client = msm_proxy_t.i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	
#ifdef CONFIG_LG_PROXY   //separate eeprom from i2c
	msm_proxy_t.i2c_eeprom_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_proxy_t.i2c_eeprom_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_proxy_t.i2c_eeprom_client.cci_client) {
		kfree(msm_proxy_t.vreg_cfg.cam_vreg);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_eeprom_client = msm_proxy_t.i2c_eeprom_client.cci_client;
	cci_eeprom_client->cci_subdev = msm_cci_get_subdev();
	cci_eeprom_client->cci_i2c_master = msm_proxy_t.cci_master;
#endif

	
	v4l2_subdev_init(&msm_proxy_t.msm_sd.sd,
					 msm_proxy_t.proxy_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_proxy_t.msm_sd.sd, &msm_proxy_t);
	msm_proxy_t.msm_sd.sd.internal_ops = &msm_proxy_internal_ops;
	msm_proxy_t.msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_proxy_t.msm_sd.sd.name,
			 ARRAY_SIZE(msm_proxy_t.msm_sd.sd.name), "msm_proxy");
	media_entity_init(&msm_proxy_t.msm_sd.sd.entity, 0, NULL, 0);
	msm_proxy_t.msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_proxy_t.msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_PROXY;
	msm_proxy_t.msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_proxy_t.msm_sd);
	msm_proxy_t.proxy_state = PROXY_POWER_DOWN;

  msm_proxy_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
    msm_proxy_v4l2_subdev_fops.compat_ioctl32 =
      msm_proxy_subdev_fops_ioctl;
#endif
	msm_proxy_t.msm_sd.sd.devnode->fops = &msm_proxy_v4l2_subdev_fops;

	msm_proxy_t.sid_proxy = msm_proxy_t.i2c_client.cci_client->sid;
	msm_proxy_t.proxy_func_tbl = NULL;
	msm_proxy_t.exit_workqueue = 1;
	msm_proxy_t.pause_workqueue = 0;
	msm_proxy_t.max_i2c_fail_thres = 5;
	msm_proxy_t.i2c_fail_cnt = 0;
	msm_proxy_t.proxy_cal = 0;
	msm_proxy_t.proxy_stat.cal_done = 0;

	CDBG("Exit\n");

	return rc;
}

static const struct i2c_device_id msm_proxy_i2c_id[] = {
	{"qcom,proxy", (kernel_ulong_t)NULL},
	{ }
};

static const struct of_device_id msm_proxy_i2c_dt_match[] = {
	{.compatible = "qcom,proxy"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_proxy_i2c_dt_match);

static struct i2c_driver msm_proxy_i2c_driver = {
	.id_table = msm_proxy_i2c_id,
	.probe  = msm_proxy_i2c_probe,
	.remove = __exit_p(msm_proxy_i2c_remove),
	.driver = {
		.name = "qcom,proxy",
		.owner = THIS_MODULE,
		.of_match_table = msm_proxy_i2c_dt_match,
	},
};

static const struct of_device_id msm_proxy_dt_match[] = {
	{.compatible = "qcom,proxy"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_proxy_dt_match);

static struct platform_driver msm_proxy_platform_driver = {
	.driver = {
		.name = "qcom,proxy",
		.owner = THIS_MODULE,
		.of_match_table = msm_proxy_dt_match,
	},
};

static struct msm_proxy_ctrl_t msm_proxy_t = {
	.i2c_driver = &msm_proxy_i2c_driver,
	.pdriver = &msm_proxy_platform_driver,
	.proxy_v4l2_subdev_ops = &msm_proxy_subdev_ops,
};

static int __init msm_proxy_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_probe(msm_proxy_t.pdriver,
							   msm_proxy_platform_probe);

	CDBG("Enter %d\n", rc);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(msm_proxy_t.i2c_driver);
}

#ifdef EWOK_API_IMPLEMENTATION
int msm_init_proxy_EwokAPI(void)
{
	//	FixPoint1616_t SnrLimitValue;	// patch for wrong distance
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	int32_t status_int;
	uint8_t pMeasurementDataReady = 0;
	int8_t e2p_offset = 0;
	int8_t sensor_offset = 0;
	int16_t finVal = 0;
	uint8_t calCount = 0;
	uint16_t moduleId = 0;
	FixPoint1616_t XTalkCompMegaCps;
	//int32_t offsetComp; 
//	uint32_t refSpadCount = 0;	
//	uint8_t isApertureSpads = 0;	
	uint32_t count = 0; //0x802~805 		
	uint16_t count_sep1 = 0; //0x802~803	
	uint16_t count_sep2 = 0; //0x804~805	
	uint16_t VhvSettings = 0;  //0x806		
	uint16_t PhaseCal = 0;	//0x807 		
	uint16_t isApertureSpads = 0;  //0x808	
	uint8_t VhvSettings_8 = 0;	//0x806 	
	uint8_t PhaseCal_8 = 0;  //0x807		
	uint8_t isApertureSpads_8 = 0;	//0x808 
	
	pr_err("msm_init_proxy_EwokAPI(): Starting...\n");
	msm_proxy_t.check_init_finish = 1;  //Init start
	msm_proxy_t.proxy_stat.cal_done = 0;
#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
// get device info.
	VL53L0_GetProductRevision(Dev, &moduleMainVersion, &moduleVersion);


// VL53L0_GetVersion	
    if(Status == VL53L0_ERROR_NONE)
    {
        if(moduleVersion == 1) 
		{// cut 1.1
        	
			if(Status == VL53L0_ERROR_NONE)
			{
				status_int = VL53L0_GetVersion(pVersion);
				if (status_int != 0)
					Status = VL53L0_ERROR_CONTROL_INTERFACE;
			} else {
				return Status;
				}
        }
		else if(moduleVersion == 0)
		{//cut1.0	
			if(Status == VL53L0_ERROR_NONE)
			{
				status_int = VL53L010_GetVersion(pVersion);
				if (status_int != 0)
					Status = VL53L0_ERROR_CONTROL_INTERFACE;
			} else {
				return Status;
				}
		}
    } else {
		return Status;
		}
#else
	// get device info.
	VL53L0_GetProductRevision(Dev, &moduleMainVersion, &moduleVersion);

	if(Status == VL53L0_ERROR_NONE)
	{
		
        if(moduleVersion == 1) 
		{// cut 1.1
			status_int = VL53L0_GetVersion(pVersion);
			if (status_int != 0)
				Status = VL53L0_ERROR_CONTROL_INTERFACE;
        }
		
		else if(moduleVersion == 0)
		{//cut1.0
			Status = VL53L0_ERROR_NOT_SUPPORTED;
		}		
	} else {
		return Status;
		}
#endif

#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
		
		if(Status == VL53L0_ERROR_NONE)
		{
			if (moduleVersion == 1) 
			{// cut 1.1
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L0_DataInit(Dev); // Data initialization
				} else {
					return Status;
					}
				
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L0_StaticInit(Dev); // Device Initialization
					/*
					//VL53L0_PerformRefSpadManagement(Dev,&refSpadCount,&isApertureSpads);
					//pr_err("spad calibration : %d, %d\n", refSpadCount, isApertureSpads);
					refSpadCount =13;
					isApertureSpads = 1;
					VL53L0_SetReferenceSpads(Dev,refSpadCount,isApertureSpads);
					*/

/* LGE_CHANGE_S, for initialization time reduce(spad calibration), 2016-02-15, seonyung.kim@lge.com */																			  
					proxy_i2c_e2p_read(IT_EEP_REG+2, &count_sep1, 2);																						   
					proxy_i2c_e2p_read(IT_EEP_REG+4, &count_sep2, 2);																						   
					count = (count_sep1 << 16) | (0xFFFF & count_sep2); 																					   
					pr_err("VL53L0 count:%d, count_sep1:%d, count_sep2:%d\n",count, count_sep1, count_sep2);												   
																																							   
					proxy_i2c_e2p_read(IT_EEP_REG+6, &VhvSettings, 1);																						   
					proxy_i2c_e2p_read(IT_EEP_REG+7, &PhaseCal, 1); 																						   
					proxy_i2c_e2p_read(IT_EEP_REG+8, &isApertureSpads, 1);																					   
					pr_err("VL53L0 VhvSettings:%d, PhaseCal:%d, isApertureSpads:%d\n",VhvSettings, PhaseCal, isApertureSpads);
					if(VhvSettings == 255 /* EEPROM default value */
						|| (count == 0 || VhvSettings == 0 || PhaseCal == 0)) { /* If one of three values has 0 value it means wrong state. It needs re-calibration */
						VL53L0_PerformRefCalibration(Dev, &VhvSettings_8,&PhaseCal_8);																	   
						VL53L0_PerformRefSpadManagement(Dev,  &count, &isApertureSpads_8);																   
						pr_err("VL53L0 count:%d, count_sep1:%d, count_sep2:%d(not calibrated!!)\n",count, count_sep1, count_sep2);						   
						pr_err("VL53L0 VhvSettings:%d, PhaseCal:%d, isApertureSpads:%d(not calibrated!!)\n",VhvSettings_8, PhaseCal_8, isApertureSpads_8); 
					}																																		   
					else {																													   
																																						   
						VL53L0_SetRefCalibration(Dev, (uint8_t)VhvSettings,(uint8_t)PhaseCal);															   
						VL53L0_SetReferenceSpads(Dev, count, (uint8_t)isApertureSpads); 																   
					}																																		   
/* LGE_CHANGE_E, for initialization time reduce(spad calibration), 2016-02-15, seonyung.kim@lge.com */																			  


				} else {
					return Status;
					}
			}
			else if(moduleVersion == 0) 
			{//cut1.0	
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L010_DataInit(Dev); // Data initialization
				} else {
					return Status;
					}
				
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L010_StaticInit(Dev); // Device Initialization
				} else {
				return Status;
				}
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L010_GetDeviceInfo(Dev, &deviceInfo);
				} else {
					return Status;
					}
			}
		} else {
			return Status;
			}
			
#else
		if(Status == VL53L0_ERROR_NONE)
		{
			Status = VL53L0_DataInit(Dev); // Data initialization
		} else {
			return Status;
			}
	
		if(Status == VL53L0_ERROR_NONE)
		{
			Status = VL53L0_StaticInit(Dev); // Device Initialization
		} else {
			return Status;
			}
		
#endif

#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1

	if (moduleVersion == 1)
	{ //readRangeOffset
		proxy_i2c_e2p_read(0x700, &moduleId, 1);
		
		//if ((moduleId == 0x00) || (moduleId == 0x01) || (moduleId == 0x02)) {
			proxy_i2c_e2p_read(IT_EEP_REG, &finVal, 2);
			e2p_offset = 0x00FF & finVal;
			calCount = (0xFF00 & finVal) >> 8;

#ifdef EWOK_API_IMPLEMENTATION
		if ((e2p_offset <= -63) || (e2p_offset >= 31)|| (calCount >= 100)) {
#else
		if ((e2p_offset <= -21) || (e2p_offset >= 11) || (calCount >= 100)) {
#endif
			proxy_i2c_e2p_write(IT_EEP_REG, 0, 2);
			calCount = 0;
			e2p_offset = 0;
		}
		msm_proxy_t.proxy_stat.cal_count = calCount;
		pr_err("read offset = %d from eeprom\n", e2p_offset);
		VL53L0_GetOffsetCalibrationDataMicroMeter(Dev,&st_offset);
		sensor_offset = (int8_t)((st_offset/1000) + e2p_offset);
		VL53L0_SetOffsetCalibrationDataMicroMeter(Dev,(int32_t)(sensor_offset*1000)); 
		pr_err("e2p_offset = %d, st_offset = %d, sensor_offset = %d\n", e2p_offset, st_offset, sensor_offset);
		//}
	}
	else if(moduleVersion == 0)
	{
	//readRangeOffset
		proxy_i2c_e2p_read(0x700, &moduleId, 1);

		if ((moduleId == 0x00) || (moduleId == 0x01) || (moduleId == 0x02)) {
			proxy_i2c_e2p_read(IT_EEP_REG, &finVal, 2);
			e2p_offset = 0x00FF & finVal;
			calCount = (0xFF00 & finVal) >> 8;

#ifdef EWOK_API_IMPLEMENTATION
			if ((e2p_offset <= -63) || (e2p_offset >= 31)|| (calCount >= 100)) {
#else
			if ((e2p_offset <= -21) || (e2p_offset >= 11) || (calCount >= 100)) {
#endif
				proxy_i2c_e2p_write(IT_EEP_REG, 0, 2);
				calCount = 0;
				e2p_offset = 0;
			}
			msm_proxy_t.proxy_stat.cal_count = calCount;
			pr_err("read offset = %d from eeprom\n", e2p_offset);
			VL53L010_GetOffsetCalibrationDataMicroMeter(Dev,&st_offset);
			sensor_offset = (int8_t)((st_offset/1000) + e2p_offset);
			VL53L010_SetOffsetCalibrationDataMicroMeter(Dev,(int32_t)(sensor_offset*1000));

		} 
	}
#else
//readRangeOffset
		proxy_i2c_e2p_read(0x700, &moduleId, 1);

		if ((moduleId == 0x00) || (moduleId == 0x01) || (moduleId == 0x02)) {
			proxy_i2c_e2p_read(IT_EEP_REG, &finVal, 2);
			e2p_offset = 0x00FF & finVal;
			calCount = (0xFF00 & finVal) >> 8;
#ifdef EWOK_API_IMPLEMENTATION
			if ((e2p_offset <= -63) || (e2p_offset >= 31)|| (calCount >= 100)) {
#else
			if ((e2p_offset <= -21) || (e2p_offset >= 11) || (calCount >= 100)) {
#endif
				proxy_i2c_e2p_write(IT_EEP_REG, 0, 2);
				calCount = 0;
				e2p_offset = 0;
			}
			msm_proxy_t.proxy_stat.cal_count = calCount;
			pr_err("read offset = %d from eeprom\n", e2p_offset);
			VL53L0_GetOffsetCalibrationDataMicroMeter(Dev,&st_offset);
			sensor_offset = (int8_t)((st_offset/1000) + e2p_offset);
			VL53L0_SetOffsetCalibrationDataMicroMeter(Dev,(int32_t)(sensor_offset*1000));

		} 

#endif


// calibration **********************************

	
	{ // forced x-talk cal
#if defined(CONFIG_MACH_MSM8996_ELSA) || defined(CONFIG_MACH_MSM8996_ANNA)
	XTalkCompMegaCps = 8; // glass window
#else
	XTalkCompMegaCps = 24;
#endif
	//Enable the XTalk compensation
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetXTalkCompensationEnable(Dev, 1);
		} else {
			return Status;
			}
	
	//Enable the XTalk compensation
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetXTalkCompensationRateMegaCps(Dev,XTalkCompMegaCps);
		} else {
			return Status;
			}
			
	}

// calibration **********************************



#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
			
		if (moduleVersion == 1) 
		{// cut 1.1
			if(Status == VL53L0_ERROR_NONE)
			{
				/* LGE_CHANGE_S, changed LD clk to reduce the S5K2P7 sensor power noise. 2016-11-21 sungmin.cho@lge.com */
				#ifdef CONFIG_MACH_MSM8996_ELSA
				uint16_t module_name_1, module_name_2; // 0x814, 0x815~0x816
				#endif
				/* LGE_CHANGE_E, changed LD clk to reduce the S5K2P7 sensor power noise. 2016-11-21 sungmin.cho@lge.com */
				/*
				 *	Setup the Limit SIGMA and Signal Rate
				 */
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L0_SetLimitCheckEnable(Dev, 
						VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
				} else {
					return Status;
					}
				if(Status == VL53L0_ERROR_NONE)
				{
					SigmaLimitValue = 15 << 16;
					Status = VL53L0_SetLimitCheckValue(Dev, 
						VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, SigmaLimitValue);
				} else {
					return Status;
					}
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L0_SetLimitCheckEnable(Dev, 
						VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 1);
				} else {
					return Status;
					}
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L0_SetLimitCheckValue(Dev, 
						VL53L0_CHECKENABLE_RANGE_IGNORE_THRESHOLD, (XTalkCompMegaCps * 15)/10);
				} else {
					return Status;
					}
/* LGE_CHANGE_S, changed LD clk to reduce the S5K2P7 sensor power noise. 2016-11-21 sungmin.cho@lge.com */
#ifdef CONFIG_MACH_MSM8996_ELSA
				// read module name
				proxy_i2c_e2p_read(0x814, &module_name_1, 1);
				proxy_i2c_e2p_read(0x815, &module_name_2, 2);

				pr_err("%s, module_name_1 = 0x%x, module_name_2 = 0x%x\n", __func__, module_name_1, module_name_2);

				if (module_name_1 == 0x45 && module_name_2 == 0x7a02)
				{
					pr_err("%s, S5K2P7", __func__);
					if (Status == VL53L0_ERROR_NONE) {
						Status = VL53L0_SetVcselPulsePeriod(Dev,
							 VL53L0_VCSEL_PERIOD_PRE_RANGE, 18);
					}
					if (Status == VL53L0_ERROR_NONE) {
						Status = VL53L0_SetVcselPulsePeriod(Dev,
							VL53L0_VCSEL_PERIOD_FINAL_RANGE, 14);
					}
				}
				else
				{
					if (Status == VL53L0_ERROR_NONE) {
						Status = VL53L0_SetVcselPulsePeriod(Dev,
							 VL53L0_VCSEL_PERIOD_PRE_RANGE, 14);
					}
					if (Status == VL53L0_ERROR_NONE) {
						Status = VL53L0_SetVcselPulsePeriod(Dev,
							VL53L0_VCSEL_PERIOD_FINAL_RANGE, 10);
					}
				}
#else
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L0_SetVcselPulsePeriod(Dev, 
						 VL53L0_VCSEL_PERIOD_PRE_RANGE, 14);
				}
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L0_SetVcselPulsePeriod(Dev, 
						VL53L0_VCSEL_PERIOD_FINAL_RANGE, 10);
				}

#endif
/* LGE_CHANGE_E, changed LD clk to reduce the S5K2P7 sensor power noise. 2016-11-21 sungmin.cho@lge.com */

				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L0_SetMeasurementTimingBudgetMicroSeconds(Dev, 33000);//11130);
				} else {
					return Status;
					}
			}
		}
		else if(moduleVersion == 0)
		{//cut1.0	
			if(Status == VL53L0_ERROR_NONE)
			{
				/*
				 *	Setup the Limit SIGMA and Signal Rate
				 */
				if(Status == VL53L0_ERROR_NONE)
				{
					SigmaLimitValue = 15 << 16;
					Status = VL53L010_SetLimitCheckValue(Dev, 
						VL53L010_CHECKENABLE_SIGMA_FINAL_RANGE, SigmaLimitValue);
				} else {
					return Status;
					}
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L010_GetLimitCheckValue(Dev, 
						VL53L010_CHECKENABLE_SIGMA_FINAL_RANGE, &SigmaLimitValue);
				} else {
					return Status;
					}
			
				if(Status == VL53L0_ERROR_NONE)
				{
					SignalLimitValue = (FixPoint1616_t)(0.02 * 65536);
					Status = VL53L010_SetLimitCheckValue(Dev, 
						VL53L010_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, SignalLimitValue);
				} else {
					return Status;
					}
				if(Status == VL53L0_ERROR_NONE)
				{
					Status = VL53L010_GetLimitCheckValue(Dev,
						VL53L010_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, &SignalLimitValue);
				} else {
					return Status;
					}
				// Enable/Disable Sigma and Signal check
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L010_SetLimitCheckEnable(Dev, 
						VL53L010_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
				} else {
					return Status;
					}
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L010_SetLimitCheckEnable(Dev, 
						VL53L010_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
				} else {
					return Status;
					}
				if (Status == VL53L0_ERROR_NONE) {
					Status = VL53L010_SetMeasurementTimingBudgetMicroSeconds(Dev, 33000);//11130);
				} else {
					return Status;
					}
			} else {
				return Status;
				}		
		}
				
#else
		if(Status == VL53L0_ERROR_NONE)
		{
			/*
			 *	Setup the Limit SIGMA and Signal Rate
			 */
			if(Status == VL53L0_ERROR_NONE)
			{
				SigmaLimitValue = 15 << 16;
				Status = VL53L0_SetLimitCheckValue(Dev, 
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, SigmaLimitValue);
			} else {
				return Status;
				}
			if(Status == VL53L0_ERROR_NONE)
			{
				Status = VL53L0_GetLimitCheckValue(Dev, 
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, &SigmaLimitValue);
			} else {
				return Status;
				}
		
			// Enable/Disable Sigma and Signal check
			if (Status == VL53L0_ERROR_NONE) {
				Status = VL53L0_SetLimitCheckEnable(Dev, 
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
			} else {
				return Status;
				}
			if (Status == VL53L0_ERROR_NONE) {
				Status = VL53L0_SetLimitCheckEnable(Dev, 
					VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
			} else {
				return Status;
				}
			if (Status == VL53L0_ERROR_NONE) {
				Status = VL53L0_SetMeasurementTimingBudgetMicroSeconds(Dev, 33000);//11130);
			} else {
				return Status;
				}
		} else {
			return Status;
			}
#endif

#ifdef COMPATIBILITY_CUT_1_0_CUT_1_1
			
		if (moduleVersion == 1)  
		{// cut 1.1
			VL53L0_SetDeviceMode(Dev, VL53L0_DEVICEMODE_SINGLE_RANGING); // Setup in single ranging mode					
		}
		else if(moduleVersion == 0) 
		{//cut1.0	
			VL53L010_GetMeasurementDataReady(Dev, &pMeasurementDataReady);
			VL53L010_SetDeviceMode(Dev, VL53L0_DEVICEMODE_SINGLE_RANGING); // Setup in single ranging mode
		}
			
#else
		VL53L0_SetDeviceMode(Dev, VL53L0_DEVICEMODE_SINGLE_RANGING); // Setup in single ranging mode
#endif

	if (Status != VL53L0_ERROR_NONE)
	{
		pr_err("msm_init_proxy_EwokAPI(): VL53L0_GetMeasurementDataReady() failed! %d %d\n", Status, msm_proxy_t.check_init_finish);
		return -1;
	} else
		msm_proxy_t.check_init_finish = 2; // Init Complete
	
	pr_err("msm_init_proxy_EwokAPI(): End!\n");
	return 0;
}
#endif

#ifdef BABYBEAR_API_IMPLEMENTATION
int msm_init_proxy_BBearAPI(void)
{
	int rc = 0;
	int i;
	uint8_t byteArray[4];
	int8_t offsetByte;
	int16_t fin_val;
	uint8_t cal_count;
	uint16_t modelID = 0;
	uint16_t revID = 0;
	uint32_t shift, dataMask;
	uint32_t ninepointseven=0;
	//seonyung
	uint16_t CrosstalkHeight;
	uint16_t IgnoreThreshold;
	uint16_t IgnoreThresholdHeight;
	int proxy_status = 0;
	uint16_t module_id = 0;
	uint8_t shift_module_id = 0;

	pr_err("msm_init_proxy_BBearAPI() ENTER!\n");


	//Dev = SLAVE_ADDRESS;
	//Dev.comms_type = VL53L0_COMMS_I2C;
	//Dev.comms_speed_khz = 400;
	//Dev.nvm_format = VL53L0_NVM_FORMAT_00_FAKE;
	//Dev.read_nvm_flag = READ_NVM_CONTENTS;

	VL6180x_RangeGetDeviceReady(Dev,&proxy_status);
	VL6180x_InitData(Dev);
	//VL53L0_DataInit(Dev);
	//VL53LO_Error ret = VL53LO_GetRangeStatus(Dev, &proxy_status);
/*	if(ret != 0)
	{
		pr_err("msm_init_proxy(), VL53LO_GetRangeStatus() fail!\n");
		return -1;
	}*/
	if((proxy_status & 0x01) && ((proxy_status>>3) == 0))
	{
		pr_err("init proxy alive!\n");
	}
	else
	{
		pr_err("init proxy fail!, no proxy sensor found!\n");
		return -1;
	}



	VL6180x_RdWord(Dev, IDENTIFICATION__MODEL_ID, &modelID);
	VL6180x_RdWord(Dev, IDENTIFICATION__REVISION_ID, &revID);

	//revID = revID >> 4;
	pr_err("Model ID : 0x%X, REVISION ID : 0x%X\n", modelID, revID);   //if revID == 2;(not calibrated), revID == 3 (calibrated)
	if(revID != REVISION_CALIBRATED)
	{
		pr_err("not calibrated!\n");
		//return -1;
	}

	VL6180x_WaitDeviceBooted(Dev);

	//VL53LO_GetRangeStatus(Dev, &modelID);
	VL6180x_RangeGetDeviceReady(Dev,&proxy_status);
	//performRegisterTuningCut1_1
	VL6180x_WrByte(Dev, GPIO_HV_PAD01__CONFIG, 0x30);



	//VL6180x_RangeStaticInit(Dev);
	VL6180x_StaticInit(Dev);

	// RangeSetMaxConvergenceTime
	VL6180x_WrByte(Dev, SYSRANGE__MAX_CONVERGENCE_TIME, 0x3F);


	VL6180x_RangeSetEceState(Dev, 1);
	VL6180x_WrByte(Dev, SYSRANGE_MAX_AMBIENT_LEVEL_MULT, 0xFF);
	// ClearSystemFreshOutofReset
	VL6180x_WrByte(Dev, SYSTEM_FRESH_OUT_OF_RESET, 0x0);


	//Range_Set_scalar
	for(i = 0; i < sizeof(u16); i++)
	{
		shift = (sizeof(u16) - i - 1)* 0x08;
		dataMask = (0xFF << shift);
		byteArray[i] = (u8)(((u16)((u16)85 & 0x01ff) & dataMask) >> shift);

		VL6180x_WrByte(Dev, RANGE__RANGE_SCALER + i, byteArray[i]);

	}
	//readRangeOffset
	proxy_i2c_e2p_read(0x700, &module_id, 2);
	shift_module_id = module_id >> 8;
	pr_err("KSY module ID : %d\n", shift_module_id);

	if((shift_module_id == 0x00) || (shift_module_id == 0x01) || (shift_module_id == 0x02))  // It module
	{
		proxy_i2c_e2p_read(IT_EEP_REG, &fin_val, 2);
		offsetByte = 0x00FF & fin_val;
		cal_count = (0xFF00 & fin_val) >> 8;
#ifdef EWOK_API_IMPLEMENTATION
		if ((offsetByte <= -63) || (offsetByte >= 31)|| (cal_count >= 100)) {
#else
		if ((offsetByte <= -21) || (offsetByte >= 11) || (cal_count >= 100)) {
#endif
			proxy_i2c_e2p_write(IT_EEP_REG, 0, 2);
			cal_count = 0;
			offsetByte = 0;
		}
		//	offsetByte -= 255;
		msm_proxy_t.proxy_stat.cal_count = cal_count;
		pr_err("inot read offset = %d from eeprom\n", offsetByte);

		VL6180x_WrByte(Dev, SYSRANGE__PART_TO_PART_RANGE_OFFSET, offsetByte);
	}
		// Babybear_SetStraylight
	ninepointseven=25;

	VL6180x_SetXTalkCompensationRate(Dev,ninepointseven);

	CrosstalkHeight = 40;
	VL6180x_WrByte(Dev, SYSRANGE__CROSSTALK_VALID_HEIGHT, CrosstalkHeight);


	// Will ignore all low distances (<100mm) with a low return rate
	IgnoreThreshold = 64; // 64 = 0.5Mcps
	IgnoreThresholdHeight = 33; // 33 * scaler3 = 99mm
	VL6180x_WrWord(Dev, SYSRANGE__RANGE_IGNORE_THRESHOLD, IgnoreThreshold);
	VL6180x_WrByte(Dev, SYSRANGE__RANGE_IGNORE_VALID_HEIGHT, IgnoreThresholdHeight);

	// Init of Averaging samples : in case of adding glass
	for(i=0; i<8;i++){
		lastMeasurements[i]=65535; // 65535 means no valid data
	}
	currentIndex = 0;

	// SetSystemInterruptConfigGPIORanging
	VL6180x_RangeConfigInterrupt(Dev,CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);


	//RangeSetSystemMode
	VL6180x_RangeStartSingleShot(Dev);

	_filter_Init(Dev);

	return rc;
}
#endif

module_init(msm_proxy_init_module);
MODULE_DESCRIPTION("MSM PROXY");
MODULE_LICENSE("GPL v2");
