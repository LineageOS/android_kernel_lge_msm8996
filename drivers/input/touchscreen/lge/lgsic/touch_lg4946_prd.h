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
 *  Include to touch core Header File
 */
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_lg4946.h"

#ifndef PRODUCTION_TEST_H
#define PRODUCTION_TEST_H

/* production test */
#define tc_tsp_test_ctl			(0xC04)
#define tc_tsp_test_sts			(0x265)
#define tc_tsp_test_pf_result		(0x266)
#define tc_tsp_test_rawdata_off_info	(0x2FB)
#define tc_tsp_test_diff_off_info	(0x2FD)
#define tc_tsp_test_os_rst_tune_off_info (0x2FE)

#define tc_tsp_test_data_offset		(0x07B)
#define tc_tsp_data_access_addr		(0x301)

#define RAWDATA_OFFSET			(0xE00)
#define rawdata_ctl_read		(0x2A4)
#define rawdata_ctl_write		(0xC49)

struct lg4946_test_off {
	u16 offset0;
	u16 offset1;
} __packed;

struct lg4946_test_off_info {
	struct lg4946_test_off m1_m2_raw;
	struct lg4946_test_off frame0_1;
	struct lg4946_test_off frame2_short;
	struct lg4946_test_off os_result;
} __packed;

/* tune code */
#define tc_tune_code_size		260
#define tc_total_ch_size		32
#define TSP_TUNE_CODE_L_GOFT_OFFSET		0
#define TSP_TUNE_CODE_L_M1_OFT_OFFSET		2
#define TSP_TUNE_CODE_L_G1_OFT_OFFSET		(TSP_TUNE_CODE_L_M1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_L_G2_OFT_OFFSET	(TSP_TUNE_CODE_L_G1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_L_G3_OFT_OFFSET		(TSP_TUNE_CODE_L_G2_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_GOFT_OFFSET		(TSP_TUNE_CODE_L_G3_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_M1_OFT_OFFSET		(TSP_TUNE_CODE_R_GOFT_OFFSET + 2)
#define TSP_TUNE_CODE_R_G1_OFT_OFFSET		(TSP_TUNE_CODE_R_M1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_G2_OFT_OFFSET		(TSP_TUNE_CODE_R_G1_OFT_OFFSET + tc_total_ch_size)
#define TSP_TUNE_CODE_R_G3_OFT_OFFSET		(TSP_TUNE_CODE_R_G2_OFT_OFFSET + tc_total_ch_size)
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
	M1_NOISE_TEST,
	OPEN_NODE_TEST,
	SHORT_NODE_TEST,
	M2_NOISE_TEST,
	DOZE1_M2_RAWDATA_TEST = 5,
	DOZE1_M1_RAWDATA_TEST = 6,
	DOZE2_M2_RAWDATA_TEST,
	DOZE2_M1_RAWDATA_TEST,
	M2_DIFF_TEST = 10,
	M1_DIFF_TEST = 11,
};

enum {
	NORMAL_MODE = 0,
	PRODUCTION_MODE,
};

extern void touch_msleep(unsigned int msecs);
extern int lge_is_valid_U2_FTRIM_reg(void);
int lg4946_prd_register_sysfs(struct device *dev);
void lg4946_te_test_logging(struct device *dev, char *buf);

#endif


