/* FPC1021 Area sensor driver
 *
 * Copyright (c) 2013 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef __FPC_BTP_REGS_H
#define __FPC_BTP_REGS_H

typedef enum {
	FPC_BTP_REG_FPC_STATUS 			= 20, 	// RO, 1 bytes
	FPC_BTP_REG_READ_INTERRUPT		= 24,	// RO, 1 byte
	FPC_BTP_REG_READ_INTERRUPT_WITH_CLEAR 	= 28,   // RO, 1 byte
	FPC_BTP_REG_READ_ERROR_WITH_CLEAR 	= 56, 	// RO, 1 byte
	FPC_BTP_REG_MISO_EDGE_RIS_EN 		= 64, 	// WO, 1 byte
	FPC_BTP_REG_FPC_CONFIG 			= 68, 	// RW, 1 byte
	FPC_BTP_REG_IMG_SMPL_SETUP 		= 76, 	// RW, 3 bytes
	FPC_BTP_REG_CLOCK_CONFIG 		= 80, 	// RW, 1 byte
	FPC_BTP_REG_IMG_CAPT_SIZE 		= 84, 	// RW, 4 bytes
	FPC_BTP_REG_IMAGE_SETUP 		= 92,	// RW, 1 byte
	FPC_BTP_REG_ADC_TEST_CTRL 		= 96, 	// RW, 1 byte
	FPC_BTP_REG_IMG_RD 			= 100, 	// RW, 1 byte
	FPC_BTP_REG_SAMPLE_PX_DLY 		= 104, 	// RW, 8 bytes
	FPC_BTP_REG_PXL_RST_DLY 		= 108, 	// RW, 1 byte
	FPC_BTP_REG_TST_COL_PATTERN_EN 		= 120, 	// RW, 2 bytes
	FPC_BTP_REG_CLK_BIST_RESULT 		= 124, 	// RW, 4 bytes
	FPC_BTP_REG_ADC_WEIGHT_SETUP 		= 132, 	// RW, 1 byte
	FPC_BTP_REG_ANA_TEST_MUX		= 136, 	// RW, 4 bytes
	FPC_BTP_REG_FINGER_DRIVE_CONF 		= 140, 	// RW, 1 byte
	FPC_BTP_REG_FINGER_DRIVE_DLY 		= 144, 	// RW, 1 byte
	FPC_BTP_REG_OSC_TRIM 			= 148, 	// RW, 2 bytes
	FPC_BTP_REG_ADC_WEIGHT_TABLE 		= 152,	// RW, 10 bytes
	FPC_BTP_REG_ADC_SETUP 			= 156, 	// RW, 5 bytes
	FPC_BTP_REG_ADC_SHIFT_GAIN 		= 160, 	// RW, 2 bytes
	FPC_BTP_REG_BIAS_TRIM 			= 164,	// RW, 1 byte
	FPC_BTP_REG_PXL_CTRL 			= 168, 	// RW, 2 bytes
	FPC_BTP_REG_FPC_DEBUG 			= 208, 	// RO, 2 bytes
	FPC_BTP_REG_FINGER_PRESENT_STATUS	= 212, 	// RO, 2 bytes
	FPC_BTP_REG_FNGR_DET_THRES 		= 216, 	// RW, 1 byte
	FPC_BTP_REG_FNGR_DET_CNTR 		= 220, 	// RW, 2 bytes
	FPC_BTP_REG_HWID 			= 252, 	// RO, 2 bytes
} fpc_btp_reg_t;

#define FPC_BTP_REG_SIZE(reg) (				    	\
	((reg) == FPC_BTP_REG_FPC_STATUS)? 			1 : \
	((reg) == FPC_BTP_REG_READ_INTERRUPT)?			1 : \
	((reg) == FPC_BTP_REG_READ_INTERRUPT_WITH_CLEAR)?	1 : \
	((reg) == FPC_BTP_REG_READ_ERROR_WITH_CLEAR)? 		1 : \
	((reg) == FPC_BTP_REG_MISO_EDGE_RIS_EN)? 		1 : \
	((reg) == FPC_BTP_REG_FPC_CONFIG)? 			1 : \
	((reg) == FPC_BTP_REG_IMG_SMPL_SETUP)? 			3 : \
	((reg) == FPC_BTP_REG_CLOCK_CONFIG)?			1 : \
	((reg) == FPC_BTP_REG_IMG_CAPT_SIZE)? 			4 : \
	((reg) == FPC_BTP_REG_IMAGE_SETUP)?			1 : \
	((reg) == FPC_BTP_REG_ADC_TEST_CTRL)? 			1 : \
	((reg) == FPC_BTP_REG_IMG_RD)?				1 : \
	((reg) == FPC_BTP_REG_SAMPLE_PX_DLY)?			8 : \
	((reg) == FPC_BTP_REG_PXL_RST_DLY)?			1 : \
	((reg) == FPC_BTP_REG_TST_COL_PATTERN_EN)?		2 : \
	((reg) == FPC_BTP_REG_CLK_BIST_RESULT)?			4 : \
	((reg) == FPC_BTP_REG_ADC_WEIGHT_SETUP)? 		1 : \
	((reg) == FPC_BTP_REG_ANA_TEST_MUX)?			4 : \
	((reg) == FPC_BTP_REG_FINGER_DRIVE_CONF)?		1 : \
	((reg) == FPC_BTP_REG_FINGER_DRIVE_DLY)? 		1 : \
	((reg) == FPC_BTP_REG_OSC_TRIM)?			2 : \
	((reg) == FPC_BTP_REG_ADC_WEIGHT_TABLE)? 		10: \
	((reg) == FPC_BTP_REG_ADC_SETUP)?			5 : \
	((reg) == FPC_BTP_REG_ADC_SHIFT_GAIN)? 			2 : \
	((reg) == FPC_BTP_REG_BIAS_TRIM)?			1 : \
	((reg) == FPC_BTP_REG_PXL_CTRL)? 			2 : \
	((reg) == FPC_BTP_REG_FPC_DEBUG)?			2 : \
	((reg) == FPC_BTP_REG_FINGER_PRESENT_STATUS)?		2 : \
	((reg) == FPC_BTP_REG_FNGR_DET_THRES)? 			1 : \
	((reg) == FPC_BTP_REG_FNGR_DET_CNTR)? 			2 : \
	((reg) == FPC_BTP_REG_HWID)? 				2 : 0 )

#endif // __FPC_BTP_REGS_H


