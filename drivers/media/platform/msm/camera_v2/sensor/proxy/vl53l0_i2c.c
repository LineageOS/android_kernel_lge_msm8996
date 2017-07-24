
/*******************************************************************************
Copyright © 2014, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/
/*
 * $Date: 2015-01-08 13:30:24 +0000 (Thu, 08 Jan 2015) $
 * $Revision: 2039 $
 */

/**
 * @file vl6180x_i2c.c
 *
 * Copyright (C) 2014 ST MicroElectronics
 *
 * provide variable word size byte/Word/dword VL6180x register access via i2c
 *
 */
#include "vl53l0_platform.h"
#include "vl53l0_i2c.h"

#define I2C_BUFFER_CONFIG 1

#ifndef I2C_BUFFER_CONFIG
#error "I2C_BUFFER_CONFIG not defined"
/* TODO you must define value for  I2C_BUFFER_CONFIG in configuration or platform h */
#endif


#if I2C_BUFFER_CONFIG == 0
    /* GLOBAL config buffer */
    uint8_t i2c_global_buffer[VL53L0_MAX_I2C_XFER_SIZE];

    #define DECL_I2C_BUFFER
    #define VL53L0_GetI2cBuffer(Dev, n_byte)  i2c_global_buffer

#elif I2C_BUFFER_CONFIG == 1
    /* ON STACK */
    #define DECL_I2C_BUFFER  uint8_t LocBuffer[VL53L0_MAX_I2C_XFER_SIZE];
    #define VL53L0_GetI2cBuffer(Dev, n_byte)  LocBuffer
#elif I2C_BUFFER_CONFIG == 2
    /* user define buffer type declare DECL_I2C_BUFFER  as access  via VL53L0_GetI2cBuffer */
    #define DECL_I2C_BUFFER
#else
#error "invalid I2C_BUFFER_CONFIG "
#endif



#define VL53L0_I2C_USER_VAR         /* none but could be for a flag var to get/pass to mutex interruptible  return flags and try again */
#define VL53L0_GetI2CAccess(Dev)    /* todo mutex acquire */
#define VL53L0_DoneI2CAcces(Dev)    /* todo mutex release */


VL53L0_Error VL53L0_WriteMulti(VL53L0_DEV dev, uint16_t index, uint8_t *pdata, uint32_t count){

    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t *buffer;
    int i;
    DECL_I2C_BUFFER
    VL53L0_I2C_USER_VAR

    if (count>=VL53L0_MAX_I2C_XFER_SIZE){
        Status = VL53L0_ERROR_INVALID_PARAMS;
    }

    if (Status == VL53L0_ERROR_NONE) {

        VL53L0_GetI2CAccess(dev);

        // Do the access
        buffer=VL53L0_GetI2cBuffer(dev,(uint8_t)count+1);
        buffer[0]=index&0xFF;
        for(i=0;i<count;i++){
            buffer[i+1]=*(pdata+i);
        }
        Status=VL53L0_I2CWrite(dev, buffer,(uint8_t)count+1);
        VL53L0_DoneI2CAcces(Dev);
    }
    return Status;
}



VL53L0_Error VL53L0_ReadMulti(VL53L0_DEV dev, uint16_t index, uint8_t *pdata, uint32_t count){
    VL53L0_I2C_USER_VAR
    int i;
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t *buffer;
    DECL_I2C_BUFFER

    if (count>=VL53L0_MAX_I2C_XFER_SIZE){
        Status = VL53L0_ERROR_INVALID_PARAMS;
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        VL53L0_GetI2CAccess(dev);

        // Do the access
        buffer=VL53L0_GetI2cBuffer(dev,count);
        buffer[0]=index&0xFF;

        if( !Status ){
            Status=VL53L0_I2CRead(dev, buffer,(uint8_t)count);
            if( !Status ){
                for(i=0;i<count;i++){
                    *(pdata+i) = buffer[i];
                }
            }
        }
        VL53L0_DoneI2CAcces(Dev);
    }
    return Status;
}




VL53L0_Error VL53L0_WrByte(VL53L0_DEV dev, uint16_t index, uint8_t data)
{
	VL53L0_Error status = VL53L0_ERROR_NONE;
    uint8_t *buffer;
    DECL_I2C_BUFFER
    VL53L0_I2C_USER_VAR

    VL53L0_GetI2CAccess(dev);

    buffer=VL53L0_GetI2cBuffer(dev,2);
    buffer[0]=index&0x00FF;
    buffer[1]=data;

    status=VL53L0_I2CWrite(dev, buffer,(uint8_t)2);
    VL53L0_DoneI2CAcces(dev);
    return status;
}

VL53L0_Error VL53L0_WrWord(VL53L0_DEV dev, uint16_t index, uint16_t data)
{
	VL53L0_Error  status= VL53L0_ERROR_NONE;
    DECL_I2C_BUFFER
    uint8_t *buffer;
    VL53L0_I2C_USER_VAR

    VL53L0_GetI2CAccess(dev);

    buffer=VL53L0_GetI2cBuffer(dev,3);
    buffer[0]=index&0x00FF;
    buffer[1]=data>>8;
    buffer[2]=data&0x00FF;

    status=VL53L0_I2CWrite(dev, buffer,(uint8_t)3);
    VL53L0_DoneI2CAcces(dev);
    return status;
}

VL53L0_Error VL53L0_WrDWord(VL53L0_DEV dev, uint16_t index, uint32_t data)
{
	VL53L0_I2C_USER_VAR
    DECL_I2C_BUFFER
    VL53L0_Error status = VL53L0_ERROR_NONE;
    uint8_t *buffer;

    VL53L0_GetI2CAccess(dev);

    buffer=VL53L0_GetI2cBuffer(dev,5);
    buffer[0]=index&0x00FF;
    buffer[1]=data>>24;
    buffer[2]=(data>>16)&0x00FF;
    buffer[3]=(data>>8)&0x00FF;
    buffer[4]=data&0xFF;
    status=VL53L0_I2CWrite(dev, buffer,(uint8_t)5);
    VL53L0_DoneI2CAcces(dev);

    return status;
}


VL53L0_Error VL53L0_UpdateByte(VL53L0_DEV dev, uint16_t index, uint8_t AndData, uint8_t OrData)
{
	VL53L0_I2C_USER_VAR
	VL53L0_Error status = VL53L0_ERROR_NONE;
    uint8_t *buffer;
    DECL_I2C_BUFFER

    VL53L0_GetI2CAccess(dev);

    // Do the access
    buffer=VL53L0_GetI2cBuffer(dev,2);
    buffer[0]=index&0x00FF;

    /* read data direct into buffer[0] */
    status=VL53L0_I2CRead(dev, buffer,1);
    if( !status )
    {
    	// write data is put in buffer[2]
    	buffer[1]=(buffer[0]&AndData)|OrData;
    	// set up index again
    	buffer[0]=index&0x00FF;
    	status=VL53L0_I2CWrite(dev, buffer, (uint8_t)2);
    }

    VL53L0_DoneI2CAcces(dev);
    return status;
}


VL53L0_Error VL53L0_RdByte(VL53L0_DEV dev, uint16_t index, uint8_t *data)
{
	VL53L0_I2C_USER_VAR
	VL53L0_Error status = VL53L0_ERROR_NONE;
    uint8_t *buffer;
    DECL_I2C_BUFFER

    VL53L0_GetI2CAccess(dev);

    buffer=VL53L0_GetI2cBuffer(dev,1);
    buffer[0]=index&0xFF;

    status=VL53L0_I2CRead(dev, buffer,1);
    if( !status ){
    	*data=buffer[0];
    }

    //pr_err("VL53L0_RdByte = %d, index = %d, len = ", buffer[0],index);

    VL53L0_DoneI2CAcces(dev);

    return status;
}

VL53L0_Error VL53L0_RdWord(VL53L0_DEV dev, uint16_t index, uint16_t *data){
	VL53L0_I2C_USER_VAR
	VL53L0_Error status = VL53L0_ERROR_NONE;
    uint8_t *buffer;
    DECL_I2C_BUFFER

    VL53L0_GetI2CAccess(dev);

    buffer=VL53L0_GetI2cBuffer(dev,2);
    buffer[0]=index&0xFF;

    status=VL53L0_I2CRead(dev, buffer,2);
    if( !status )
    {
    	/* VL6180x register are Big endian if cpu is be direct read direct into *data is possible */
    	*data=((uint16_t)buffer[0]<<8)|(uint16_t)buffer[1];
    }

    VL53L0_DoneI2CAcces(dev);
    return status;
}

VL53L0_Error  VL53L0_RdDWord(VL53L0_DEV dev, uint16_t index, uint32_t *data){
	VL53L0_I2C_USER_VAR
	VL53L0_Error status= VL53L0_ERROR_NONE;
    uint8_t *buffer;
    DECL_I2C_BUFFER

    VL53L0_GetI2CAccess(dev);

    buffer=VL53L0_GetI2cBuffer(dev,4);
    buffer[0]=index&0x00FF;

    status=VL53L0_I2CRead(dev, buffer,4);
    if( !status ){
    	/* VL6180x register are Big endian if cpu is be direct read direct into data is possible */
    	*data=((uint32_t)buffer[0]<<24)|((uint32_t)buffer[1]<<16)|((uint32_t)buffer[2]<<8)|((uint32_t)buffer[3]);
    }

    VL53L0_DoneI2CAcces(dev);
    return status;
}
