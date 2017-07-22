/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 Copyright ?2012 Synaptics Incorporated. All rights reserved.

 The information in this file is confidential under the terms
 of a non-disclosure agreement with Synaptics and is provided
 AS IS.

 The information in this file shall remain the exclusive property
 of Synaptics and may be the subject of Synaptics?patents, in
 whole or part. Synaptics?intellectual property rights in the
 information in this file are not expressly or implicitly licensed
 or otherwise transferred to you as a result of such information
 being made available to you.
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include <linux/kernel.h>	/* printk */
#include <linux/delay.h>	/* msleep */
#include <linux/time.h>		/* struct timeval t_interval[TIME_PROFILE_MAX];*/
#include <linux/math64.h>	/*for abs func*/
#include <linux/string.h>	/* memset */
#include <linux/i2c.h>

#include <touch_hwif.h>
#include <touch_core.h>
#include "../touch_s3320.h"
#include "../touch_s3320_prd.h"


#define TRX_max 32
#define CAP_FILE_PATH "/sns/touch/cap_diff_test.txt"
#define DS5_BUFFER_SIZE 6000

enum {
	RSP_F54_FULL_TEST = 0,
	RSP_RAW_DATA_PRINT,
	RSP_DELTA_PRINT,
	RSP_NOISE_PP_PRINT,
	RSP_CAL_TEST,
	RSP_LPWG_RAW_DATA,
};

extern int UpperImage[TRX_max][TRX_max];
extern int LowerImage[TRX_max][TRX_max];
extern int SensorSpeedUpperImage[TRX_max][TRX_max];
extern int SensorSpeedLowerImage[TRX_max][TRX_max];
extern int ADCUpperImage[TRX_max][TRX_max];
extern int ADCLowerImage[TRX_max][TRX_max];
extern int RspNoise[TRX_max][TRX_max];
extern unsigned char RxChannelCount;
extern unsigned char TxChannelCount;
extern int f54_window_crack;
extern int f54_window_crack_check_mode;

extern void SCAN_PDT(void);
extern int F54Test(int input, int mode, char *buf);
extern void write_file(char *data, int write_time);
extern int Read8BitRegisters(unsigned short regAddr,
		unsigned char *data, int length);
extern int Write8BitRegisters(unsigned short regAddr,
		unsigned char *data, int length);
extern int synaptics_get_limit(struct device *dev, char *breakpoint,
					unsigned char Tx, unsigned char Rx,
					int limit_data[32][32]);

