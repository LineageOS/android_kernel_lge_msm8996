#ifndef _VL6180X_I2C_H_
#define _VL6180X_I2C_H_

//#include "vl53l0_def.h"
//#include "vl53l0_platform.h"
#include "vl6180x_api.h"

#define VL6180x_I2C_USER_VAR
#define VL6180x_GetI2CAccess(dev) (void)0
#define VL6180x_DoneI2CAcces(dev) (void)0


#ifdef __cplusplus
extern "C" {
#endif
int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len);
int VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t *buff, uint8_t len);


#ifdef __cplusplus
}
#endif

#endif
