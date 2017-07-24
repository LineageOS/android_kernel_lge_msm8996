/*******************************************************************************
Copyright © 2015, STMicroelectronics International N.V.
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


#ifndef _VL53L0_I2C_H_
#define _VL53L0_I2C_H_

//#include "vl53l0_def.h"
//#include "vl53l0_platform.h"
#include "vl53l0_api.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file vl53l0_platform.h
 *
 * @brief All end user OS/platform/application porting
 */


/** @defgroup VL53L0_i2c_group Platform Definitions
 *  @brief    Functions and Defines definitions linked to a specific platform
 *  @{
 */


/** Maximum buffer size to be used in i2c */
#define VL53L0_MAX_I2C_XFER_SIZE   64 /* Maximum buffer size to be used in i2c */


/**
 * @brief       Write data buffer to device via i2c
 * @param Dev    :  Device Handle
 * @param buff   :  The data buffer
 * @param len    :  The length of the transaction in byte
 * @return  VL53L0_ERROR_NONE      : Success
 * @return  "Other error code"  : See ::VL53L0_Error
 * @ingroup porting_i2c
 */
VL53L0_Error  VL53L0_I2CWrite(VL53L0_DEV dev, uint8_t  *buff, uint8_t len);

/**
 *
 * @brief       Read data buffer from device via i2c
 * @param Dev    :  Device Handle
 * @param buff   :  The data buffer to fill
 * @param len    :  The length of the transaction in byte
 * @return  VL53L0_ERROR_NONE      : Success
 * @return  "Other error code"  : See ::VL53L0_Error
 * @ingroup  porting_i2c
 */
VL53L0_Error VL53L0_I2CRead(VL53L0_DEV dev, uint8_t *buff, uint8_t len);

/** @} end of VL53L0_i2c_group */

#ifdef __cplusplus
}
#endif

#endif  /* _VL53L0_I2C_H_ */



