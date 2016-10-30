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

#include "vl6180x_types.h"
#include "vl6180x_def.h"

// For Delay
#include <linux/time.h>


#include "vl6180x_i2c.h"

#include "msm_proxy.h"
#include "msm_proxy_i2c.h"




int VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
	// the index is always at least 16bit
	uint32_t index = 0;
	int32_t ret = -1;

	// check for no data as the first 2 bytes are the index
	if (len<=2)
		return 0;

	index = (buff[0]<<8)|(buff[1]&0x00FF);

	// increment buffer past the index bytes
	buff++;
	buff++;

	ret = proxy_i2c_write_seq( index,buff,len-2 );

	return ret;
}


/*
 * Will read a maximum of 4 bytes
 */
int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
	// the index is always at least 16bit
	uint32_t index = 0;
	int32_t ret = -1;

	index = (buff[0]<<8)|(buff[1]&0x00FF);

	ret = proxy_i2c_read_seq( index, (uint8_t *)buff, len );

	return ret;
}



void VL6180x_PollDelay(VL6180xDev_t dev)
{
	usleep(1000); // Wait for 1ms
}
