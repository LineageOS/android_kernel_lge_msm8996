/* production_test.h
 *
 * Copyright (C) 2015 LGE.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Include to touch core Header File
 */
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_lg4945.h"

#ifndef PRODUCTION_TEST_H
#define PRODUCTION_TEST_H

/* production test */
#define tc_tsp_test_ctl			(0xC003)
#define tc_tsp_test_sts			(0x8A63)
#define tc_tsp_test_m2_raw_data		(0x8A80)
#define tc_tsp_test_m1_raw_data		(0x8A80)
#define tc_tsp_test_pf_result		(0x8A64)
#define tc_tsp_test_os_result		(0x8EA4)
#define tc_mem_sel			(0xD6A4)

/* tune code */
#define tc_tune_code_base		(0x8C40)
#define tc_tune_code_size		276
#define tc_total_ch_size		34
#define TSP_TUNE_CODE_L_GOFT_OFFSET		0
#define TSP_TUNE_CODE_L_M1_OFT_OFFSET		2
#define TSP_TUNE_CODE_L_G1_OFT_OFFSET		(TSP_TUNE_CODE_L_M1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_L_G2_OFT_OFFSET		(TSP_TUNE_CODE_L_G1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_L_G3_OFT_OFFSET		(TSP_TUNE_CODE_L_G2_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_GOFT_OFFSET		(TSP_TUNE_CODE_L_G3_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_M1_OFT_OFFSET		(TSP_TUNE_CODE_R_GOFT_OFFSET + 2)
#define TSP_TUNE_CODE_R_G1_OFT_OFFSET		(TSP_TUNE_CODE_R_M1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_G2_OFT_OFFSET		(TSP_TUNE_CODE_R_G1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_G3_OFT_OFFSET		(TSP_TUNE_CODE_R_G2_OFT_OFFSET + tc_total_ch_size)
#define M2_RAWDATA_ADDR				(0x8A80u)
#define M1_RAWDATA_ADDR				(0x8A80u)
#define rawdata_ctl_read				(0x8BE2)
#define rawdata_ctl_write				(0xC229u)

#define PATH_SIZE		64
#define BURST_SIZE		512
#define RAWDATA_SIZE		2
#define ROW_SIZE		32
#define COL_SIZE		18
#define M1_COL_SIZE		2
#define LOG_BUF_SIZE		256
#define BUF_SIZE (PAGE_SIZE * 2)
#define MAX_LOG_FILE_SIZE	(10 * 1024 * 1024) /* 10 M byte */
#define MAX_LOG_FILE_COUNT	(4)

enum {
	TIME_INFO_SKIP,
	TIME_INFO_WRITE,
};

enum {
	NO_TEST = 0,
	OPEN_SHORT_ALL_TEST,
	OPEN_NODE_TEST,
	SHORT_NODE_TEST,
	DOZE1_M2_RAWDATA_TEST = 5,
	DOZE1_M1_RAWDATA_TEST = 6,
	DOZE2_M2_RAWDATA_TEST,
	DOZE2_M1_RAWDATA_TEST,
};

enum {
	NORMAL_MODE = 0,
	PRODUCTION_MODE,
};

extern void touch_msleep(unsigned int msecs);
int lg4945_prd_register_sysfs(struct device *dev);

#endif


