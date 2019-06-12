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

#include "vl53l0_types.h"
#include "vl53l0_def.h"

// For Delay
#include <linux/time.h>

#include <media/msmb_camera.h>
#include "vl53l0_i2c.h"
#include "msm_proxy.h"
#include "msm_proxy_i2c.h"



VL53L0_Error  VL53L0_I2CWrite(VL53L0_DEV dev, uint8_t  *buff, uint8_t len)
{
	// the index is always at least 16bit
	uint32_t index = 0;
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	// check for no data as the first 2 bytes are the index
	if (len<2)
		return 0;

	index = buff[0]&0xFF;

	// increment buffer past the index byte
	buff++;
	//pr_err("proxy_i2c_write_seq(), index = 0x%X, data = %d", index,buff[0]);
	Status = proxy_i2c_write_seq( index,buff,len-1 );

	return Status;
}


/*
 * Will read a maximum of 4 bytes
 */
VL53L0_Error VL53L0_I2CRead(VL53L0_DEV dev, uint8_t *buff, uint8_t len)
{
	// the index is always at least 16bit
	uint32_t index = 0;
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	index = buff[0]&0x000000FF;

	Status = proxy_i2c_read_seq( index, (uint8_t *)buff, len );

    //pr_err("VL53L0_I2CRead = %d, index = %d, len = %d", (uint8_t) *buff,index,len );

	return Status;
}



VL53L0_Error VL53L0_PollingDelay(VL53L0_DEV dev)
{
	usleep_range(1000, 1000+10); // Wait for 1ms
	return VL53L0_ERROR_NONE;
}
