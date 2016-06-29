/*
 * Device driver for monitoring ambient light intensity in (lux)
 * proximity detection (prox), and Beam functionality within the
 * AMS-TAOS TCS family of devices.
 *
 * Copyright (c) 2013, AMS-TAOS USA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>

#include "msm_tcs.h"
#include "msm_tcs_i2c.h"

/*#define TCS3490_DEBUG*/
#undef CDBG
#ifdef TCS3490_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do{}while(0)
#endif

#define TCS3490_USE_GAINREDUCE

#define TCS3490_ATIME_UNIT_STEP 	18	//50ms
#define TCS3490_ATIME_50MS		1
#define TCS3490_ATIME_100MS	2

#define TCS3490_SET_CLEAR		0
#define TCS3490_SET_IR			1
#define TCS3490_AEN_DISABLE	0
#define TCS3490_AEN_ENABLE		1



static int32_t tcs3490_Set_als_gain(struct msm_tcs_ctrl_t* state, int gain);
static int32_t tcs3490_Set_als_time(struct msm_tcs_ctrl_t* state, int ATIME_Grade);
static int32_t tcs3490_Set_als_GAINREDUCE(struct msm_tcs_ctrl_t* state, bool ALSGAINREDUCE);
static int32_t tcs3490_Set_AEN(struct msm_tcs_ctrl_t* state, uint8_t AEN_Enable);
static int32_t tcs3490_Set_ClearIR_Chennel(struct msm_tcs_ctrl_t* state, uint8_t SetIR);
static int32_t tcs3490_Check_RGBC_Valid(struct msm_tcs_ctrl_t* state, int ValidData_TimeOut);
static int32_t tcs3490_Auto_Gain_Control(struct msm_tcs_ctrl_t* state);
int32_t tcs3490_Read_BINs(struct msm_tcs_ctrl_t* state);
int32_t tcs3490_Read_all(struct msm_tcs_ctrl_t* state);
int32_t tcs3490_device_Scan_All_Data(struct msm_tcs_ctrl_t* state);

#define TCS3490_IMPLEMENTATION


static int32_t tcs3490_Set_als_gain(struct msm_tcs_ctrl_t* state, int gain)
{
	int32_t rc;
	uint16_t ctrl_reg  = state->shadow[TCS3490_GAIN] & ~TCS3490_ALS_GAIN_MASK;

	CDBG("AMS set als gain = %d\n", gain);

	switch (gain) {
	case AGAIN_1:
		ctrl_reg |= AGAIN_1;
		break;
	case AGAIN_4:
		ctrl_reg |= AGAIN_4;
		break;
	case AGAIN_16:
		ctrl_reg |= AGAIN_16;
		break;
	case AGAIN_64:
		ctrl_reg |= AGAIN_64;
		break;
	default:
		pr_err(" wrong als gain %d\n", gain);
		return -100;
	}

	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_GAIN, ctrl_reg, 1);
	if (rc < 0) {
		state->tcs_error = TCS_ERR_I2C;
		return rc;
	}
	else
	{
		state->shadow[TCS3490_GAIN] = ctrl_reg;
		state->params.als_gain = gain;
		CDBG("AMS TCS3490: new als gain %d\n", gain);
	}
    state->als_gain_param_changed = true;		//Set Autogain changed flag.
	return rc;
}

static int32_t tcs3490_Set_als_time(struct msm_tcs_ctrl_t* state, int ATIME_Grade)
{
	int32_t rc;
	uint16_t reg_als_time;

	reg_als_time = 256 - (ATIME_Grade*TCS3490_ATIME_UNIT_STEP);

	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_ALS_TIME, reg_als_time,1);
	if (rc < 0) {
		state->tcs_error = TCS_ERR_I2C;
		return rc;
	}
	else
	{
		state->shadow[TCS3490_ALS_TIME] = reg_als_time;
		state->params.als_time= reg_als_time;
		CDBG("AMS TCS3490: new als Time %d\n", reg_als_time);
	}
	state->als_gain_param_changed = true;		//Set Autogain changed flag.
	return rc;
}


static int32_t tcs3490_Set_als_GAINREDUCE(struct msm_tcs_ctrl_t* state, bool ALSGAINREDUCE)
{
	int32_t rc;
	uint16_t set_ALSGAINREDUCE;

	if (ALSGAINREDUCE == true)
	{
		set_ALSGAINREDUCE = 0x01<<4;
	}
	else
	{
		set_ALSGAINREDUCE = 0x00;
	}

	rc = tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_AUX, set_ALSGAINREDUCE,1);
	if (rc < 0) {
		state->tcs_error = TCS_ERR_I2C;
		return rc;
	}
	else
	{
		state->shadow[TCS3490_AUX] = set_ALSGAINREDUCE;
		state->als_gain_reduced= ALSGAINREDUCE;
		CDBG("AMS TCS3490: new als Gain Controlled GAINREDUCED %d\n", set_ALSGAINREDUCE);
	}

	state->als_gain_param_changed = true;		//Set Autogain changed flag.

	return rc;
}


static int32_t tcs3490_Auto_Gain_Control(struct msm_tcs_ctrl_t* state)
{
	//Auto Gain Control
	uint32_t ReCalc_ClearValue; //TLEE for AutoGain
	uint32_t ReCalc_IRValue; //TLEE for AutoGain
	uint32_t Clear_MaxResolution;
	uint32_t GainRangeTable[5];
	uint8_t gain = state->params.als_gain;
	uint8_t atime =  state->params.als_time;


	int32_t rc = -11;
	uint8_t Prev_Gain;
	uint8_t ATIME_Grade;
	uint8_t Prev_ATIME_Grade;

	ATIME_Grade = (256-atime)/TCS3490_ATIME_UNIT_STEP;
	Prev_ATIME_Grade = ATIME_Grade;
	Clear_MaxResolution = TCS3490_ATIME_UNIT_STEP * 1024;
	//Clear_MaxResolution = 18,432

	GainRangeTable[0] = Clear_MaxResolution;
	GainRangeTable[1] = Clear_MaxResolution / 4;
	GainRangeTable[2] = Clear_MaxResolution / 16;
	GainRangeTable[3] = Clear_MaxResolution / 64;
	GainRangeTable[4] = Clear_MaxResolution / 128;

	Prev_Gain = gain;


	if(ATIME_Grade == TCS3490_ATIME_50MS)		//ATIME 50ms
	{

#ifdef TCS3490_USE_GAINREDUCE
		if(state->als_gain_reduced)
		{
			ReCalc_ClearValue = state->als_inf.clear_raw;
			ReCalc_IRValue = state->als_inf.ir;
			ReCalc_ClearValue += (ReCalc_ClearValue >> 1);	//around x1.5
			ReCalc_IRValue += (ReCalc_IRValue >> 1);			//around x1.5
			if(( ReCalc_ClearValue < (GainRangeTable[0]*8)/10) && ( ReCalc_IRValue < (GainRangeTable[0]*8)/10))	//Get Dark
			{
				//Back to Normal a Factor
				rc = tcs3490_Set_als_GAINREDUCE(state, false);
			}
		}
		else
		{
#endif
		if(gain == AGAIN_1)
		{
			ReCalc_ClearValue = state->als_inf.clear_raw;
			ReCalc_IRValue = state->als_inf.ir;
			if(( ReCalc_ClearValue < (GainRangeTable[1]*8)/10) && ( ReCalc_IRValue < (GainRangeTable[1]*8)/10))	//Get Dark
			{
				rc = tcs3490_Set_als_gain(state, AGAIN_4);
			}
			else if(( ReCalc_ClearValue >(GainRangeTable[0] * 8)/10) || ( ReCalc_IRValue >(GainRangeTable[0] * 8)/10)) //Get Bright
			{
				//change a Factor to 1.58
				rc = tcs3490_Set_als_GAINREDUCE(state, true);
			}
		}
		else if(gain == AGAIN_4)
		{
			ReCalc_ClearValue = state->als_inf.clear_raw / 4;
			ReCalc_IRValue = state->als_inf.ir/ 4;
			if(( ReCalc_ClearValue < (GainRangeTable[2]*8)/10) && ( ReCalc_IRValue < (GainRangeTable[2]*8)/10))	//Get Dark
			{
				rc = tcs3490_Set_als_gain(state, AGAIN_16);
			}
			else if(( ReCalc_ClearValue >(GainRangeTable[1] * 8)/10) || ( ReCalc_IRValue >(GainRangeTable[1] * 8)/10)) //Get Bright
			{
				rc = tcs3490_Set_als_gain(state, AGAIN_1);
			}
		}
		else if(gain == AGAIN_16)
		{
			ReCalc_ClearValue = state->als_inf.clear_raw / 16;
			ReCalc_IRValue = state->als_inf.ir/ 16;
			if(( ReCalc_ClearValue < (GainRangeTable[3]*8)/10) && ( ReCalc_IRValue < (GainRangeTable[3]*8)/10))	//Get Dark
			{
				rc = tcs3490_Set_als_gain(state, AGAIN_64);
			}
			else if(( ReCalc_ClearValue >(GainRangeTable[2] * 8)/10) || ( ReCalc_IRValue >(GainRangeTable[2] * 8)/10)) //Get Bright
			{
				rc = tcs3490_Set_als_gain(state, AGAIN_4);
			}
		}
		else if(gain == AGAIN_64)
		{
			ReCalc_ClearValue = state->als_inf.clear_raw / 64;
			ReCalc_IRValue = state->als_inf.ir/ 64;
			if(( ReCalc_ClearValue < (GainRangeTable[4]*8)/10) && ( ReCalc_IRValue < (GainRangeTable[4]*8)/10))	//Get Dark
			{
				//Change ATIME to 100ms
				rc = tcs3490_Set_als_time(state, TCS3490_ATIME_100MS);
			}
			else if(( ReCalc_ClearValue >(GainRangeTable[3] * 8)/10) || ( ReCalc_IRValue >(GainRangeTable[3] * 8)/10))//Get Bright
			{
				rc = tcs3490_Set_als_gain(state, AGAIN_16);
			}
		}
#ifdef TCS3490_USE_GAINREDUCE
		}
#endif
	}
	else if(ATIME_Grade == TCS3490_ATIME_100MS )	//ATIME_Grade == 4 : ATIME 100ms (25*4)
	{
			ReCalc_ClearValue = state->als_inf.clear_raw / 128;
			ReCalc_IRValue = state->als_inf.ir/ 128;
			if( (ReCalc_ClearValue >(GainRangeTable[4] * 8)/10) || (ReCalc_IRValue>(GainRangeTable[4] * 8)/10))//Get Bright
			{
				//Change ATIME to 50ms
				rc = tcs3490_Set_als_time(state, TCS3490_ATIME_50MS);
			}
	}


	ATIME_Grade = atime/TCS3490_ATIME_UNIT_STEP;
	if( (state->params.als_gain != Prev_Gain) || (ATIME_Grade != Prev_ATIME_Grade) )
	{
		//rc = tcs3490_read_all2(state);
		CDBG("AMS GainRangeTable  %d,%d,%d,%d,%d, ClearValue: %d\n", GainRangeTable[0],GainRangeTable[1],GainRangeTable[2],GainRangeTable[3],GainRangeTable[4],ReCalc_ClearValue );
		CDBG("AMS Gain Changed, PreGain:%d, CurGain:%d, Pre_ATIME: %d, Cur_ATIME: %d\n",Prev_Gain, state->params.als_gain, Prev_ATIME_Grade, ATIME_Grade);
		return 1;
	}

	return 0;

}

static int32_t tcs3490_Set_AEN(struct msm_tcs_ctrl_t* state, uint8_t AEN_Enable)
{
	uint16_t reg_val;
	int32_t rc;

	if(AEN_Enable == TCS3490_AEN_ENABLE)
	{
		//Enanble AEN
		reg_val = TCS3490_EN_PWR_ON | TCS3490_EN_ALS |TCS3490_EN_ALS_IRQ;
		rc=tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_CONTROL, reg_val,1);
		if(rc < 0)
		{
			state->tcs_error = TCS_ERR_I2C;
			pr_err("AMS TCS3490: I2C writing Fail - Set AEN %d\n", reg_val);
		}

	}
	else
	{
		//Disanble AEN
		reg_val = TCS3490_EN_PWR_ON  |TCS3490_EN_ALS_IRQ;
		rc=tcs_i2c_write( I2C_ADDR_OFFSET+TCS3490_CONTROL, reg_val,1);
		if(rc <0 )
                {
                        state->tcs_error = TCS_ERR_I2C;
                        pr_err("AMS TCS3490: I2C writing Fail - Set AEN %d\n", reg_val);
                }

	}
	return rc;
}

static int32_t tcs3490_Set_ClearIR_Chennel(struct msm_tcs_ctrl_t* state, uint8_t SetIR)
{
	int32_t rc;

	rc = tcs3490_Set_AEN(state,TCS3490_AEN_DISABLE);
	if(rc < 0)
		return rc; //i2c fail

	if(SetIR == TCS3490_SET_IR)
	{
		//Connect IR PhotoDiode
		rc=tcs_i2c_write( 0xC0, 0x01<<7 ,1);   //clr -> IR
		if(rc < 0)
                {
                        state->tcs_error = TCS_ERR_I2C;
						return rc;
                }

		state->reg_clr = false;
	}
	else
	{
		//Connect Clear PhotoDiode
		rc=tcs_i2c_write( 0xC0, 0x00 ,1);   //IR -> clr
		if(rc < 0)
                {
                        state->tcs_error = TCS_ERR_I2C;
						return rc;
                }

		state->reg_clr = true;
	}

	rc = tcs3490_Set_AEN(state,TCS3490_AEN_ENABLE);
	return rc;
}


static int32_t tcs3490_Check_RGBC_Valid(struct msm_tcs_ctrl_t* state, int ValidData_TimeOut)
{
	int32_t ClearCnt = 0;
	int32_t rc = 0;
	uint16_t read_val = 0;
	uint16_t TimeOut_cnt = 0;

	while(1)
	{
		ClearCnt++;
		rc = tcs_i2c_read( I2C_ADDR_OFFSET+TCS3490_STATUS, &read_val, 1);
		state->shadow[TCS3490_STATUS] = (uint8_t)read_val;
		if( (read_val & 0x01) == 0x01 ) {
			break;
		}

		TimeOut_cnt++;
		if(TimeOut_cnt >= ValidData_TimeOut)
		{
			//Time out
			state->tcs_error = TCS_ERR_TIMEOUT;   // i2c fail or others
			return TCS_ERR_TIMEOUT  ;
		}
		msleep(2);
	}

	return ClearCnt;
}

int32_t tcs3490_Read_BINs(struct msm_tcs_ctrl_t* state)
{
	int32_t ret = 0;
	uint8_t  temp_data[2];

	ret = tcs_i2c_read_seq(0xD5, (uint8_t*)&temp_data[0], 2);
	if(ret<0)
	{
		state->tcs_error = TCS_ERR_I2C;
		return TCS_ERR_I2C;
	}
	state->params.BIN_Info = ((0xFF & temp_data[1]) << 8) | (0xFF & temp_data[0]);

	if( (temp_data[1]&0x80) != 0x80)
	{
		//it's not CT710 (no BINs information).
		//need to do Error notification.
		state->params.BIN_Info = 0x0;

		pr_err("Not CT710 IC!!");
		state->tcs_error = TCS_ERR_OLD_MODULE;

		return TCS_ERR_OLD_MODULE;
	}

	return ret ;
}

int32_t tcs3490_Read_all(struct msm_tcs_ctrl_t* state)
{
	int32_t ret = 0;

	uint8_t  temp_data_clr[8];
	uint8_t  temp_data_ir[8];
	//uint16_t read_val = 0;
	uint16_t ClearCnt=0, IRCnt=0;
	uint8_t tcs3490_getdata_retry_cnt;

	uint8_t als_time = 0;
	uint32_t R_temp = 0 ;
	uint32_t G_temp = 0 ;
	uint32_t B_temp = 0 ;

	tcs3490_getdata_retry_cnt = 0;
	als_time = state->shadow[TCS3490_ALS_TIME] ;


	while(1)
	{
		//Set ready to read RGBC data set
		ret = tcs3490_Set_ClearIR_Chennel(state,TCS3490_SET_CLEAR);

		if(state->pause_workqueue)
                {
                pr_err("tcs stop Reading(CLR) \n");
		state->stop_reading = true;
		return ret;
                }

		//Wait for RGBC vaild
		//pr_err("tcs Enter Sleep -Clr \n");
		if ( ((256-als_time)/TCS3490_ATIME_UNIT_STEP) == TCS3490_ATIME_50MS)
			msleep(50);
		else
			msleep(100);
		//pr_err("tcs Exit Sleep - Clr\n");
		ClearCnt = tcs3490_Check_RGBC_Valid(state, 50); //to do : timeout count
		if(ClearCnt == TCS_ERR_TIMEOUT)
		{
			tcs3490_getdata_retry_cnt++;
			if(tcs3490_getdata_retry_cnt == 3)
			{
				//tcs3490 RGBC valid time out error: Clear cycle
				pr_err("Kernel Data(rgb_info) : AMS tcs3490 RGBC valid time out error: Clear cycle\n");
				state->tcs_error = TCS_ERR_TIMEOUT;
				return TCS_ERR_TIMEOUT;
			}
		}
		else
		{
			break;
		}
	}


	ret = tcs_i2c_read_seq(I2C_ADDR_OFFSET+ TCS3490_CLR_CHANLO,  (uint8_t  *)&temp_data_clr[0], 8);

	if(ret < 0)
	{
		state->tcs_error = TCS_ERR_I2C;
                return  ret;
	}
	tcs3490_getdata_retry_cnt = 0;
	while(1)
	{
		//Set ready to read RGB + IR data set
		tcs3490_Set_ClearIR_Chennel(state,TCS3490_SET_IR);

		//Wait for RGBC vaild

		if(state->pause_workqueue)
                {
		state->stop_reading = true;
                pr_err("tcs stop Reading(IR) \n");
                        //break;
			return ret;
                }
		//pr_err("tcs Enter Sleep - Ir\n");
		if (( (256-als_time)/TCS3490_ATIME_UNIT_STEP) == TCS3490_ATIME_50MS)
			msleep(50);
		else
			msleep(100);
		//pr_err("tcs Exit Sleep - Ir\n");
		IRCnt = tcs3490_Check_RGBC_Valid(state, 50); // to do : check the timeout count
		if(IRCnt == TCS_ERR_TIMEOUT)
		{
			tcs3490_getdata_retry_cnt++;
			if(tcs3490_getdata_retry_cnt == 3)
			{
				//tcs3490 RGBC valid time out error: IR cycle
				pr_err("Kernel Data(rgb_info) : AMS tcs3490 RGBC valid time out error: IR cycle\n");
				state->tcs_error = TCS_ERR_TIMEOUT;
				return TCS_ERR_TIMEOUT;
			}
		}
		else
		{
			break;
		}
	}



	ret = tcs_i2c_read_seq(I2C_ADDR_OFFSET+ TCS3490_CLR_CHANLO, (uint8_t  *)&temp_data_ir[0], 8);

	if(ret < 0)
	{
		state->tcs_error = TCS_ERR_I2C;
                return  ret;
	}

	//Place RGBC & IR data at once
	state->als_inf.clear_raw  = ((0xFF & temp_data_clr[1]) << 8) | (0xFF & temp_data_clr[0]);
	state->als_inf.red_raw	= ((0xFF & temp_data_clr[3]) << 8) | (0xFF & temp_data_clr[2]);
	state->als_inf.green_raw  =((0xFF & temp_data_clr[5]) << 8) | (0xFF & temp_data_clr[4]);
	state->als_inf.blue_raw  =((0xFF & temp_data_clr[7]) << 8) | (0xFF & temp_data_clr[6]);
	
	state->als_inf.ir=((0xFF & temp_data_ir[1]) << 8) | (0xFF & temp_data_ir[0]);
	R_temp = ((0xFF & temp_data_ir[3]) << 8) | (0xFF & temp_data_ir[2]);
	G_temp  =((0xFF & temp_data_ir[5]) << 8) | (0xFF & temp_data_ir[4]);
	B_temp  =((0xFF & temp_data_ir[7]) << 8) | (0xFF & temp_data_ir[6]);
	state->rgbsum_ir = R_temp + G_temp + B_temp;


    //Changed Gain and ATime should be updated at here!! - Tortion.
	if(state->als_gain_param_changed)
	{
		if((state->shadow[TCS3490_AUX]&0x80) == 0x80)
				state->als_gain_reduced= true;
		else
				state->als_gain_reduced= false;

		state->params.als_time= state->shadow[TCS3490_ALS_TIME];
		state->params.als_gain = state->shadow[TCS3490_GAIN];
		state->als_gain_param_changed = false;

	}

	state->stop_reading = false;
	//Debugging Message
	//tcs_i2c_read(0xC0, &read_val, 1);
	//CDBG("Kernel Data(rgb_info) : tcs , CLR-IR= %d, CLR ,%d, RED,%d,GRN,%d,BLU,%d  IR %d, CrCnt: %d, IrCnt:%d    Gain : %d, ATIME: %d \n", read_val,
	//		state->als_inf.clear_raw, state->als_inf.red_raw ,state->als_inf.green_raw, state->als_inf.blue_raw, state->als_inf.ir,ClearCnt, IRCnt, state->params.als_gain, state->params.als_time);

	return ret;
}

int32_t  tcs3490_device_Scan_All_Data(struct msm_tcs_ctrl_t* state)
{
	int32_t ret =0;

	ret=tcs3490_Auto_Gain_Control(state);

	ret=tcs3490_Read_all(state);

	return ret;
}

