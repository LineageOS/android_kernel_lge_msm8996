/*******************************************************************************
################################################################################
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 and only version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
#------------------------------------------------------------------------------
#                             Imaging Division
################################################################################
********************************************************************************/


/* vl6180x_platform.h STM32 Nucleo F401 mulitple device  sample code project */

#ifndef VL6180x_PLATFORM
#define VL6180x_PLATFORM




#define I2C_BUFFER_CONFIG 1



#define VL6180x_SINGLE_DEVICE_DRIVER 	1
#define VL6180x_RANGE_STATUS_ERRSTRING  1
#define VL6180X_SAFE_POLLING_ENTER 		0
#define VL6180X_LOG_ENABLE			    0


#define VL6180x_DEV_DATA_ATTR
#define ROMABLE_DATA

#include "vl6180x_def.h"
#include "linux/jiffies.h"



#if VL6180X_LOG_ENABLE
/*  dot not include non ansi here trace was a case :( */
#ifdef TRACE
#include "diag/trace.h"
#define LOG_GET_TIME()  0
#else
/* these is nto stm32 vl6180x GNuArm eclpse build*/
#define trace_printf(msg, ...) pr_err(msg, ##__VA_ARGS__)
#define LOG_GET_TIME() 0 /* add your code here expect to be an integer native (%d) type  value  */
#endif



#define LOG_FUNCTION_START(fmt,...) \
    trace_printf("beg %s start @%d\t"fmt"\n", __func__, LOG_GET_TIME(), ##__VA_ARGS__)

#define LOG_FUNCTION_END(status)\
        trace_printf("end %s @%d %d\n", __func__, LOG_GET_TIME(), (int)status)

#define LOG_FUNCTION_END_FMT(status, fmt, ... )\
        trace_printf("End %s @%d %d\t"fmt"\n" , __func__, LOG_GET_TIME(), (int)status, ##__VA_ARGS__)

#define VL6180x_ErrLog(msg, ... )\
    do{\
        trace_printf("ERR in %s line %d\n" msg, __func__, __LINE__, ##__VA_ARGS__);\
    }while(0)

#else /* VL6180X_LOG_ENABLE no logging */

//	#define LOG_FUNCTION_START(...) (void)0
//	#define LOG_FUNCTION_END(...) (void)0
//	#define LOG_FUNCTION_END_FMT(...) (void)0
//    #define VL6180x_ErrLog(message, ...) pr_err(message, ##__VA_ARGS__)
#endif


#if  VL6180x_SINGLE_DEVICE_DRIVER
    typedef uint8_t VL6180xDev_t;


#else /* VL6180x_SINGLE_DEVICE_DRIVER */

    struct MyVL6180Dev_t {
        struct VL6180xDevData_t Data;
    #if I2C_BUFFER_CONFIG == 2
        uint8_t i2c_buffer[VL6180x_MAX_I2C_XFER_SIZE];
        #define VL6180x_GetI2cBuffer(dev, n) ((dev)->i2c_buffer)
    #endif
        uint8_t I2cAddr;
        uint8_t DevID;
				uint8_t DevPresent; 
    };
    typedef struct MyVL6180Dev_t *VL6180xDev_t;

#define VL6180xDevDataGet(dev, field) (dev->Data.field)
#define VL6180xDevDataSet(dev, field, data) (dev->Data.field)=(data)

#endif /* #else VL6180x_SINGLE_DEVICE_DRIVER */

void VL6180x_PollDelay(VL6180xDev_t dev);


//#define VL6180x_PollDelay(dev)   (void)0


/*int VL6180x_RdDWord(VL6180xDev_t dev, uint16_t index, uint32_t *data);
int VL6180x_RdWord(VL6180xDev_t dev, uint16_t index, uint16_t *data);
int VL6180x_RdByte(VL6180xDev_t dev, uint16_t index, uint8_t *data);
int VL6180x_UpdateByte(VL6180xDev_t dev, uint16_t index, uint8_t AndData, uint8_t OrData);
int VL6180x_WrDWord(VL6180xDev_t dev, uint16_t index, uint32_t data);
int VL6180x_WrWord(VL6180xDev_t dev, uint16_t index, uint16_t data);
int VL6180x_WrByte(VL6180xDev_t dev, uint16_t index, uint8_t data);*/


#endif  /* VL6180x_PLATFORM */



