/* production_test.c
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
#define TS_MODULE "[prd]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <soc/qcom/lge/board_lge.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_lg4945.h"
#include "touch_lg4945_prd.h"

static char line[BUF_SIZE];
static char W_Buf[BUF_SIZE];
static u16 M2_Rawdata_buf[ROW_SIZE*COL_SIZE];
static u16 M1_Rawdata_buf[ROW_SIZE*M1_COL_SIZE];

static void log_file_size_check(struct device *dev)
{
	char *fname = NULL;
	struct file *file;
	loff_t file_size = 0;
	int i = 0;
	char buf1[128] = {0};
	char buf2[128] = {0};
	mm_segment_t old_fs = get_fs();
	int ret = 0;
	int boot_mode = 0;

	set_fs(KERNEL_DS);

	boot_mode = touch_boot_mode_check(dev);

	switch (boot_mode) {
	case NORMAL_BOOT:
		fname = "/sdcard/touch_self_test.txt";
		break;
	case MINIOS_AAT:
		fname = "/data/logger/touch_self_test.txt";
		break;
	case MINIOS_MFTS_FOLDER:
	case MINIOS_MFTS_FLAT:
	case MINIOS_MFTS_CURVED:
		fname = "/data/logger/touch_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}

	if (fname) {
		file = filp_open(fname, O_RDONLY, 0666);
		sys_chmod(fname, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n",
				__func__);
		goto error;
	}

	if (IS_ERR(file)) {
		TOUCH_I("%s : ERR(%ld) Open file error [%s]\n",
				__func__, PTR_ERR(file), fname);
		goto error;
	}

	file_size = vfs_llseek(file, 0, SEEK_END);
	TOUCH_I("%s : [%s] file_size = %lld\n",
			__func__, fname, file_size);

	filp_close(file, 0);

	if (file_size > MAX_LOG_FILE_SIZE) {
		TOUCH_I("%s : [%s] file_size(%lld) > MAX_LOG_FILE_SIZE(%d)\n",
				__func__, fname, file_size, MAX_LOG_FILE_SIZE);

		for (i = MAX_LOG_FILE_COUNT - 1; i >= 0; i--) {
			if (i == 0)
				sprintf(buf1, "%s", fname);
			else
				sprintf(buf1, "%s.%d", fname, i);

			ret = sys_access(buf1, 0);

			if (ret == 0) {
				TOUCH_I("%s : file [%s] exist\n",
						__func__, buf1);

				if (i == (MAX_LOG_FILE_COUNT - 1)) {
					if (sys_unlink(buf1) < 0) {
						TOUCH_E("%s : failed to remove file [%s]\n",
								__func__, buf1);
						goto error;
					}

					TOUCH_I("%s : remove file [%s]\n",
							__func__, buf1);
				} else {
					sprintf(buf2, "%s.%d",
							fname,
							(i + 1));

					if (sys_rename(buf1, buf2) < 0) {
						TOUCH_E("%s : failed to rename file [%s] -> [%s]\n",
								__func__, buf1, buf2);
						goto error;
					}

					TOUCH_I("%s : rename file [%s] -> [%s]\n",
							__func__, buf1, buf2);
				}
			} else {
				TOUCH_I("%s : file [%s] does not exist (ret = %d)\n",
						__func__, buf1, ret);
			}
		}
	}

error:
	set_fs(old_fs);
	return;
}
static void write_file(struct device *dev, char *data, int write_time)
{
	int fd = 0;
	char *fname = NULL;
	char time_string[64] = {0};
	struct timespec my_time;
	struct tm my_date;
	mm_segment_t old_fs = get_fs();
	int boot_mode = 0;

	set_fs(KERNEL_DS);

	boot_mode = touch_boot_mode_check(dev);

	switch (boot_mode) {
	case NORMAL_BOOT:
		fname = "/sdcard/touch_self_test.txt";
		break;
	case MINIOS_AAT:
		fname = "/data/logger/touch_self_test.txt";
		break;
	case MINIOS_MFTS_FOLDER:
	case MINIOS_MFTS_FLAT:
	case MINIOS_MFTS_CURVED:
		fname = "/data/logger/touch_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}

	if (fname) {
		fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
		sys_chmod(fname, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n", __func__);
		return;
	}

	if (fd >= 0) {
		if (write_time == TIME_INFO_WRITE) {
			my_time = __current_kernel_time();
			time_to_tm(my_time.tv_sec,
					sys_tz.tz_minuteswest * 60 * (-1),
					&my_date);
			snprintf(time_string, 64,
				"\n[%02d-%02d %02d:%02d:%02d.%03lu]\n",
				my_date.tm_mon + 1,
				my_date.tm_mday, my_date.tm_hour,
				my_date.tm_min, my_date.tm_sec,
				(unsigned long) my_time.tv_nsec / 1000000);
			sys_write(fd, time_string, strlen(time_string));
		}
		sys_write(fd, data, strlen(data));
		sys_close(fd);
	} else {
		TOUCH_I("File open failed\n");
	}
	set_fs(old_fs);
}

static int write_test_mode(struct device *dev, u8 type)
{
	u32 testmode = 0;
	u8 doze_mode = 0x1;
	int retry = 20;
	u32 rdata = 0x01;
	int waiting_time = 400;

	switch (type) {
	case OPEN_NODE_TEST:
		testmode = (doze_mode << 8) + type;
		waiting_time = 10;
		break;
	case SHORT_NODE_TEST:
		testmode = (doze_mode << 8) + type;
		waiting_time = 1000;
		break;
	case DOZE1_M2_RAWDATA_TEST:
		testmode = (doze_mode << 8) + type;
		break;
	case DOZE2_M1_RAWDATA_TEST:
		type = 0x6;
		testmode = (doze_mode << 9) + type;
		break;
	case DOZE2_M2_RAWDATA_TEST:
		type = 0x5;
		testmode = (doze_mode << 9) + type;
		break;
	}

	/* TestType Set */
	lg4945_reg_write(dev, tc_tsp_test_ctl,
			(u8 *)&testmode,
			sizeof(testmode));
	TOUCH_I("write testmode = %x\n", testmode);
	touch_msleep(waiting_time);

	/* Check Test Result - wait until 0 is written */
	do {
		touch_msleep(100);
		lg4945_reg_read(dev, tc_tsp_test_sts,
				(u8 *)&rdata,
				sizeof(rdata));
		TOUCH_I("rdata = 0x%x\n", rdata);
	} while ((rdata != 0xAA) && retry--);

	if (rdata != 0xAA) {
		TOUCH_I("ProductionTest Type [%d] Time out\n", type);
		goto error;
	}
	return 1;
error:
	TOUCH_E("[%s] fail\n", __func__);
	return 0;
}

static int prd_os_result_get(struct device *dev, u32 *buf)
{
	lg4945_reg_read(dev, tc_tsp_test_os_result,
		(u8 *)buf, sizeof(u32)*ROW_SIZE);

	return 0;
}

static int prd_os_xline_result_read(struct device *dev,
	u8 (*buf)[COL_SIZE], int type)
{
	int i = 0;
	int j = 0;
	u32 buffer[ROW_SIZE] = {0,};
	int ret = 0;
	u8 w_val = 0x0;

	switch (type) {
	case OPEN_NODE_TEST:
		w_val = 0x1;
		break;
	case SHORT_NODE_TEST:
		w_val = 0x2;
		break;
	}

	ret = prd_os_result_get(dev, buffer);

	if (ret == 0) {
		for (i = 0; i < ROW_SIZE; i++) {
			for (j = 0; j < COL_SIZE; j++) {
				if ((buffer[i] & (0x1 << j)) != 0)
					buf[i][j] =
						(buf[i][j] | w_val);
			}
		}
	}

	return ret;
}

static int prd_open_short_test(struct device *dev)
{
	int type = 0;
	int ret = 0;
	int write_test_mode_result = 0;
	u32 open_result = 0;
	u32 short_result = 0;
	u32 openshort_all_result = 0;
	u8 buf[ROW_SIZE][COL_SIZE];
	int i = 0;
	int j = 0;

	/* Test Type Write */
	write_file(dev, "[OPEN_SHORT_ALL_TEST]\n", TIME_INFO_SKIP);

	memset(&buf, 0x0, sizeof(buf));

	/* 1. open_test */
	type = OPEN_NODE_TEST;
	write_test_mode_result = write_test_mode(dev, type);
	if (write_test_mode_result == 0) {
		TOUCH_E("write_test_mode fail\n");
		return 0x3;
	}

	lg4945_reg_read(dev, tc_tsp_test_pf_result,
			(u8 *)&open_result, sizeof(open_result));
	TOUCH_I("open_result = %d\n", open_result);

	if (open_result) {
		ret = prd_os_xline_result_read(dev, buf, type);
		openshort_all_result |= 0x1;
	}

	/* 2. short_test */
	type = SHORT_NODE_TEST;
	write_test_mode_result = write_test_mode(dev, type);
	if (write_test_mode_result == 0) {
		TOUCH_E("write_test_mode fail\n");
		return 0x3;
	}

	lg4945_reg_read(dev, tc_tsp_test_pf_result,
		(u8 *)&short_result, sizeof(short_result));
	TOUCH_I("short_result = %d\n", short_result);

	if (short_result) {
		ret = prd_os_xline_result_read(dev, buf, type);
		openshort_all_result |= 0x2;
	}

	/* fail case */
	if (openshort_all_result != 0) {
		ret = snprintf(W_Buf, BUF_SIZE, "\n   : ");
		for (i = 0; i < COL_SIZE; i++)
			ret += snprintf(W_Buf + ret,
					BUF_SIZE - ret,
					" [%2d] ", i);

		for (i = 0; i < ROW_SIZE; i++) {
			ret += snprintf(W_Buf + ret,
					BUF_SIZE - ret,
					"\n[%2d] ", i);

			for (j = 0; j < COL_SIZE; j++) {
				ret += snprintf(W_Buf + ret,
					BUF_SIZE - ret, "%5s ",
				((buf[i][j] & 0x3) == 0x3) ?  "O,S" :
				((buf[i][j] & 0x1) == 0x1) ?  "O" :
				((buf[i][j] & 0x2) == 0x2) ?  "S" : "-");
			}
		}
		ret += snprintf(W_Buf + ret, BUF_SIZE - ret, "\n");
	} else
		ret = snprintf(W_Buf + ret, BUF_SIZE - ret,
				"OPEN_SHORT_ALL_TEST : Pass\n");

	write_file(dev, W_Buf, TIME_INFO_SKIP);

	return openshort_all_result;
}

static void prd_read_rawdata(struct device *dev, u8 type)
{
	int __m1_frame_size = ROW_SIZE*M1_COL_SIZE*RAWDATA_SIZE;
	int __m2_frame_size = ROW_SIZE*COL_SIZE*RAWDATA_SIZE;
	int i = 0;
	static u16 M1_Rawdata_temp[ROW_SIZE*M1_COL_SIZE];

	if (__m1_frame_size % 4)
		__m1_frame_size = (((__m1_frame_size >> 2) + 1) << 2);
	if (__m2_frame_size % 4)
		__m2_frame_size = (((__m2_frame_size >> 2) + 1) << 2);

	switch (type) {
	case DOZE2_M1_RAWDATA_TEST:
		memset(M1_Rawdata_buf, 0, sizeof(M1_Rawdata_buf));
		memset(M1_Rawdata_temp, 0, sizeof(M1_Rawdata_buf));
		lg4945_reg_read(dev, tc_tsp_test_m1_raw_data,
				(u8 *)&M1_Rawdata_temp,
				__m1_frame_size);

		for (i = 0; i < ROW_SIZE; i++) {
			M1_Rawdata_buf[i*2] = M1_Rawdata_temp[ROW_SIZE+i];
			M1_Rawdata_buf[i*2+1] = M1_Rawdata_temp[i];
		}
		break;
	case DOZE1_M2_RAWDATA_TEST:
	case DOZE2_M2_RAWDATA_TEST:
		memset(M2_Rawdata_buf, 0, sizeof(M2_Rawdata_buf));
		lg4945_reg_read(dev, tc_tsp_test_m2_raw_data,
				(u8 *)&M2_Rawdata_buf,
				__m2_frame_size);
		break;
	}
}

static int sdcard_spec_file_read(struct device *dev)
{
	int ret = 0;
	int fd = 0;
	char *path[2] = { "/mnt/sdcard/h1_limit.txt",
		"/mnt/sdcard/h1_limit_mfts.txt"
	};
	int path_idx = 0;

	mm_segment_t old_fs = get_fs();

	if(touch_boot_mode_check(dev) >= MINIOS_MFTS_FOLDER)
		path_idx = 1;
	else
		path_idx = 0;

	set_fs(KERNEL_DS);
	fd = sys_open(path[path_idx], O_RDONLY, 0);
	if (fd >= 0) {
		sys_read(fd, line, sizeof(line));
		sys_close(fd);
		TOUCH_I("%s file existing\n", path[path_idx]);
		ret = 1;
	}
	set_fs(old_fs);

	return ret;
}

static int spec_file_read(struct device *dev)
{
	int ret = 0;
	struct touch_core_data *ts = to_touch_core(dev);
	const struct firmware *fwlimit = NULL;
	const char *path[2] = { ts->panel_spec,
		ts->panel_spec_mfts
	};
	int path_idx = 0;

	if(touch_boot_mode_check(dev) >= MINIOS_MFTS_FOLDER)
		path_idx = 1;
	else
		path_idx = 0;

	if (ts->panel_spec == NULL || ts->panel_spec_mfts == NULL) {
		TOUCH_I("panel_spec_file name is null\n");
		ret = -1;
		goto error;
	}

	if (request_firmware(&fwlimit, path[path_idx], dev) < 0) {
		TOUCH_I("request ihex is failed in normal mode\n");
		ret = -1;
		goto error;
	}

	if (fwlimit->data == NULL) {
		ret = -1;
		TOUCH_I("fwlimit->data is NULL\n");
		goto error;
	}

	strlcpy(line, fwlimit->data, sizeof(line));

error:
	if (fwlimit)
		release_firmware(fwlimit);

	return ret;
}

static int sic_get_limit(struct device *dev, char *breakpoint)
{
	int p = 0;
	int q = 0;
	int cipher = 1;
	int ret = 0;
	char *found;
	int boot_mode = 0;
	int file_exist = 0;
	int limit_data = 0;


	if (breakpoint == NULL) {
		ret = -1;
		goto error;
	}

	boot_mode = touch_boot_mode_check(dev);

	if (boot_mode > MINIOS_MFTS_CURVED
			|| boot_mode < NORMAL_BOOT) {
		ret = -1;
		goto error;
	}

	file_exist = sdcard_spec_file_read(dev);
	if (!file_exist) {
		ret = spec_file_read(dev);
		if (ret == -1)
			goto error;
	}

	if (line == NULL) {
		ret =  -1;
		goto error;
	}

	found = strnstr(line, breakpoint, sizeof(line));
	if (found != NULL) {
		q = found - line;
	} else {
		TOUCH_I(
			"failed to find breakpoint. The panel_spec_file is wrong\n");
		ret = -1;
		goto error;
	}

	while (1) {
		if (line[q] == ',') {
			cipher = 1;
			for (p = 1; (line[q - p] >= '0') &&
					(line[q - p] <= '9'); p++) {
				limit_data += ((line[q - p] - '0') * cipher);
				cipher *= 10;

			}
			TOUCH_I("limit_data : %d\n", limit_data);
			ret = limit_data;
			break;
		}
		q++;
	}

	if (ret == 0) {
		ret = -1;
		goto error;

	} else {
		TOUCH_I("panel_spec_file scanning is success\n");
		return ret;
	}

error:
	return ret;
}

static int prd_print_rawdata(struct device *dev, char *buf, u8 type)
{
	int i = 0, j = 0;
	int ret = 0;
	int min = 9999;
	int max = 0;
	u16 *rawdata_buf = NULL;
	int col_size = 0;

	/* print a frame data */
	ret = snprintf(buf, PAGE_SIZE, "\n   : ");

	switch (type) {
	case DOZE2_M1_RAWDATA_TEST:
		col_size = M1_COL_SIZE;
		rawdata_buf = M1_Rawdata_buf;
		break;
	case DOZE2_M2_RAWDATA_TEST:
	case DOZE1_M2_RAWDATA_TEST:
		col_size = COL_SIZE;
		rawdata_buf = M2_Rawdata_buf;
		break;
	}

	for (i = 0; i < col_size; i++)
		ret += snprintf(buf + ret, PAGE_SIZE - ret, " [%2d] ", i);

	for (i = 0; i < ROW_SIZE; i++) {
		char log_buf[LOG_BUF_SIZE] = {0, };
		int log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,  "\n[%2d] ", i);
		log_ret += snprintf(log_buf + log_ret,
				LOG_BUF_SIZE - log_ret,  "[%2d]  ", i);
		for (j = 0; j < col_size; j++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%5d ", rawdata_buf[i*col_size+j]);
			log_ret += snprintf(log_buf + log_ret,
					LOG_BUF_SIZE - log_ret,
					"%5d ", rawdata_buf[i*col_size+j]);
			if (rawdata_buf[i*col_size+j] != 0 &&
					rawdata_buf[i*col_size+j] < min)
				min = rawdata_buf[i*col_size+j];
			if (rawdata_buf[i*col_size+j] > max)
				max = rawdata_buf[i*col_size+j];
		}
		TOUCH_I("%s\n", log_buf);
	}


	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"\nRawdata min : %d , max : %d\n", min, max);

	return ret;
}


/* Rawdata compare result
	Pass : reurn 0
	Fail : return 1
*/
static int prd_compare_rawdata(struct device *dev, u8 type)
{
	/* spec reading */
	int lower_limit = 0;
	int upper_limit = 0;
	char lower_str[64] = {0, };
	char upper_str[64] = {0, };
	u16 *rawdata_buf = NULL;
	int col_size = 0;
	int i, j;
	int ret = 0;
	int result = 0;

	switch (type) {
	case DOZE2_M1_RAWDATA_TEST:
		snprintf(lower_str, sizeof(lower_str),
				"DOZE2_M1_Lower");
		snprintf(upper_str, sizeof(upper_str),
				"DOZE2_M1_Upper");
		col_size = M1_COL_SIZE;
		rawdata_buf = M1_Rawdata_buf;
		break;
	case DOZE2_M2_RAWDATA_TEST:
		snprintf(lower_str, sizeof(lower_str),
				"DOZE2_M2_Lower");
		snprintf(upper_str, sizeof(upper_str),
				"DOZE2_M2_Upper");
		col_size = COL_SIZE;
		rawdata_buf = M2_Rawdata_buf;
		break;
	case DOZE1_M2_RAWDATA_TEST:
		snprintf(lower_str, sizeof(lower_str),
				"DOZE1_M2_Lower");
		snprintf(upper_str, sizeof(upper_str),
				"DOZE1_M2_Upper");
		col_size = COL_SIZE;
		rawdata_buf = M2_Rawdata_buf;
		break;
	}

	lower_limit = sic_get_limit(dev, lower_str);
	upper_limit = sic_get_limit(dev, upper_str);

	for (i = 0; i < ROW_SIZE; i++) {
		for (j = 0; j < col_size; j++) {
			if ((rawdata_buf[i*col_size+j] < lower_limit) ||
				(rawdata_buf[i*col_size+j] > upper_limit)) {
				if ((type != DOZE2_M1_RAWDATA_TEST) &&
						(i <= 1 && j <= 4)) {
					if (rawdata_buf[i*col_size+j] != 0) {
						result = 1;
						ret += snprintf(W_Buf + ret, BUF_SIZE - ret,
								"F [%d][%d] = %d\n", i, j, rawdata_buf[i*col_size+j]);
					}
				} else {
					result = 1;
					ret += snprintf(W_Buf + ret, BUF_SIZE - ret,
							"F [%d][%d] = %d\n", i, j, rawdata_buf[i*col_size+j]);
				}
			}
		}
	}

	return result;
}

static void tune_display(struct device *dev, char *tc_tune_code,
	int offset, int type)
{
	char log_buf[tc_tune_code_size] = {0,};
	int ret = 0;
	int i = 0;
	char temp[tc_tune_code_size] = {0,};

	switch (type) {
	case 1:
		ret = snprintf(log_buf, tc_tune_code_size,
				"GOFT tune_code_read : ");
		if ((tc_tune_code[offset] >> 4) == 1) {
			temp[offset] = tc_tune_code[offset] - (0x1 << 4);
			ret += snprintf(log_buf + ret,
					tc_tune_code_size - ret,
					"-%d  ", temp[offset]);
		} else {
			ret += snprintf(log_buf + ret,
					tc_tune_code_size - ret,
					" %d  ", tc_tune_code[offset]);
		}
		TOUCH_I("%s\n", log_buf);
		ret += snprintf(log_buf + ret, tc_tune_code_size - ret, "\n");
		write_file(dev, log_buf, TIME_INFO_SKIP);
		break;
	case 2:
		ret = snprintf(log_buf, tc_tune_code_size,
				"LOFT tune_code_read : ");
		for (i = 0; i < tc_total_ch_size; i++) {
			if ((tc_tune_code[offset+i]) >> 5 == 1) {
				temp[offset+i] =
					tc_tune_code[offset+i] - (0x1 << 5);
				ret += snprintf(log_buf + ret,
						tc_tune_code_size - ret,
						"-%d  ", temp[offset+i]);
			} else {
				ret += snprintf(log_buf + ret,
						tc_tune_code_size - ret,
						" %d  ",
						tc_tune_code[offset+i]);
			}
		}
		TOUCH_I("%s\n", log_buf);
		ret += snprintf(log_buf + ret, tc_tune_code_size - ret, "\n");
		write_file(dev, log_buf, TIME_INFO_SKIP);
		break;
	}
}

static void read_tune_code(struct device *dev, u8 type)
{
	u8 tune_code_read_buf[276] = {0,};

	lg4945_reg_read(dev, tc_tune_code_base,
			(u8 *)&tune_code_read_buf[0], tc_tune_code_size);

	write_file(dev, "\n[Read Tune Code]\n", TIME_INFO_SKIP);
	switch (type) {
	case DOZE2_M1_RAWDATA_TEST:
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_L_GOFT_OFFSET, 1);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_R_GOFT_OFFSET, 1);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_L_M1_OFT_OFFSET, 2);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_R_M1_OFT_OFFSET, 2);
		break;
	case DOZE1_M2_RAWDATA_TEST:
	case DOZE2_M2_RAWDATA_TEST:
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_L_GOFT_OFFSET + 1, 1);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_R_GOFT_OFFSET + 1, 1);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_L_G1_OFT_OFFSET, 2);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_L_G2_OFT_OFFSET, 2);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_L_G3_OFT_OFFSET, 2);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_R_G1_OFT_OFFSET, 2);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_R_G2_OFT_OFFSET, 2);
		tune_display(dev, tune_code_read_buf,
			TSP_TUNE_CODE_R_G3_OFT_OFFSET, 2);
		break;
	}
	write_file(dev, "\n", TIME_INFO_SKIP);

}

static int prd_rawdata_test(struct device *dev, u8 type)
{
	char test_type[32] = {0, };
	int result = 0;
	int write_test_mode_result = 0;

	switch (type) {
	case DOZE1_M2_RAWDATA_TEST:
		snprintf(test_type, sizeof(test_type),
				"[DOZE1_M2_RAWDATA_TEST]");
		break;
	case DOZE2_M1_RAWDATA_TEST:
		snprintf(test_type, sizeof(test_type),
				"[DOZE2_M1_RAWDATA_TEST]");
		break;
	case DOZE2_M2_RAWDATA_TEST:
		snprintf(test_type, sizeof(test_type),
				"[DOZE2_M2_RAWDATA_TEST]");
		break;
	default:
		TOUCH_I("Test Type not defined\n");
		return 1;
	}

	/* Test Type Write */
	write_file(dev, test_type, TIME_INFO_SKIP);

	write_test_mode_result = write_test_mode(dev, type);
	if (write_test_mode_result == 0) {
		TOUCH_E("production test couldn't be done\n");
		return result;
	}

	prd_read_rawdata(dev, type);

	result = prd_print_rawdata(dev, W_Buf, type);
	write_file(dev, W_Buf, TIME_INFO_SKIP);

	memset(W_Buf, 0, BUF_SIZE);
	/* rawdata compare result(pass : 0 fail : 1) */
	result = prd_compare_rawdata(dev, type);
	write_file(dev, W_Buf, TIME_INFO_SKIP);

	/* To Do - tune code result check */
	/*result = */read_tune_code(dev, type);

	return result;
}

static void ic_run_info_print(struct device *dev)
{
	unsigned char buffer[LOG_BUF_SIZE] = {0,};
	int ret = 0;
	u32 rdata[4] = {0};

	lg4945_reg_read(dev, info_lot_num, (u8 *)&rdata, sizeof(rdata));

	ret = snprintf(buffer, LOG_BUF_SIZE,
			"\n===== Production Info =====\n");
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"lot : %d\n", rdata[0]);
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"serial : 0x%X\n", rdata[1]);
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"date : 0x%X 0x%X\n",
			rdata[2], rdata[3]);
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"date : %04d.%02d.%02d %02d:%02d:%02d Site%d\n\n",
		rdata[2] & 0xFFFF, (rdata[2] >> 16 & 0xFF),
		(rdata[2] >> 24 & 0xFF), rdata[3] & 0xFF,
		(rdata[3] >> 8 & 0xFF),
		(rdata[3] >> 16 & 0xFF),
		(rdata[3] >> 24 & 0xFF));

	write_file(dev, buffer, TIME_INFO_SKIP);
}

static void firmware_version_log(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;
	unsigned char buffer[LOG_BUF_SIZE] = {0,};
	int boot_mode = 0;

	boot_mode = touch_boot_mode_check(dev);
	if (boot_mode >= MINIOS_MFTS_FOLDER)
		ret = lg4945_ic_info(dev);

	ret = snprintf(buffer, LOG_BUF_SIZE,
			"======== Firmware Info ========\n");
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"version : v%d.%02d\n",
			d->fw.version[0], d->fw.version[1]);
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"revision : %d\n", d->fw.revision);
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"product id : %s\n", d->fw.product_id);

	write_file(dev, buffer, TIME_INFO_SKIP);
}

static int ic_exception_check(struct device *dev, char *buf)
{
#if 0
	struct lg4945_data *d = to_lg4945_data(dev);
	int boot_mode = 0;
	int ret = 0;

	boot_mode = touch_boot_mode_check(dev);
	/* MINIOS mode, MFTS mode check */
	if ((lge_get_boot_mode() > LGE_BOOT_MODE_NORMAL) || boot_mode > 0) {
		if (d->fw.revision < 2 ||
				d->fw.revision > 3 ||
				d->fw.version[0] != 1) {
			TOUCH_I("ic_revision : %d, fw_version : v%d.%02d\n",
					d->fw.revision,
					d->fw.version[0], d->fw.version[1]);

			ret = snprintf(buf, PAGE_SIZE,
					"========RESULT=======\n");

			if (d->fw.version[0] != 1) {
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"version[v%d.%02d] : Fail\n",
					d->fw.version[0], d->fw.version[1]);
			} else {
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"version[v%d.%02d] : Pass\n",
					d->fw.version[0], d->fw.version[1]);
			}

			if (d->fw.revision < 2 || d->fw.revision > 3) {
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"revision[%d] : Fail\n",
					d->fw.revision);
			} else {
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"revision[%d] : Pass\n",
					d->fw.revision);
			}
			write_file(dev, buf, TIME_INFO_SKIP);

			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Raw data : Fail\n");
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Channel Status : Fail\n");
			write_file(dev, "", TIME_INFO_WRITE);
		}
	}
	return ret;
#endif
	return 0;
}

static ssize_t show_sd(struct device *dev, char *buf)
{

	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int openshort_ret = 0;
	int rawdata_ret = 0;
	int ret = 0;

	/* file create , time log */
	write_file(dev, "\nShow_sd Test Start", TIME_INFO_SKIP);
	write_file(dev, "\n", TIME_INFO_WRITE);
	TOUCH_I("Show_sd Test Start\n");


	/* LCD mode check */
	if (d->lcd_mode != LCD_MODE_U3) {
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"LCD mode is not U3. Test Result : Fail\n");
		return ret;
	}

	/* ic rev check - MINIOS mode, MFTS mode check */
	ret = ic_exception_check(dev, buf);
	if (ret > 0)
		return ret;

	firmware_version_log(dev);
	ic_run_info_print(dev);

	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	lg4945_tc_driving(dev, LCD_MODE_STOP);
	touch_msleep(20);

	/*
		DOZE1_M2_RAWDATA_TEST
		rawdata - pass : 0, fail : 1
		rawdata tunecode - pass : 0, fail : 2
	*/
	rawdata_ret = prd_rawdata_test(dev, DOZE1_M2_RAWDATA_TEST);
	/*
		OPEN_SHORT_ALL_TEST
		open - pass : 0, fail : 1
		short - pass : 0, fail : 2
	*/
	openshort_ret = prd_open_short_test(dev);

	ret = snprintf(buf, PAGE_SIZE,
			"\n========RESULT=======\n");
	if (rawdata_ret == 0)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Raw Data : Pass\n");
	else
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Raw Data : Fail\n");

	if (openshort_ret == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Channel Status : Pass\n");
	} else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Channel Status : Fail (%d/%d)\n",
			((openshort_ret & 0x1) == 0x1) ? 0 : 1,
			((openshort_ret & 0x2) == 0x2) ? 0 : 1);
	}

	write_file(dev, buf, TIME_INFO_SKIP);

	ts->driver->power(dev, POWER_OFF);
	ts->driver->power(dev, POWER_ON);
	touch_msleep(90);
	ts->driver->init(dev);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	write_file(dev, "Show_sd Test End\n", TIME_INFO_WRITE);
	log_file_size_check(dev);
	TOUCH_I("Show_sd Test End\n");
	return ret;
}

static ssize_t get_data(struct device *dev, int16_t *buf, u32 wdata)
{
	u32 rdata = 1;
	int retry = 40;
	int __frame_size = ROW_SIZE*COL_SIZE*RAWDATA_SIZE;

	/* write 1 : GETRAWDATA
	   write 2 : GETDELTA */
	TOUCH_I("======== get data ========\n");

	lg4945_reg_write(dev, rawdata_ctl_write, (u8 *)&wdata, sizeof(wdata));
	TOUCH_I("wdata = %d\n", wdata);

	/* wait until 0 is written */
	do {
		touch_msleep(200);
		lg4945_reg_read(dev, rawdata_ctl_read,
				(u8 *)&rdata, sizeof(rdata));

	} while ((rdata != 0) && retry--);
	/* check whether 0 is written or not */

	if (rdata != 0) {
		TOUCH_E("== get data time out! ==\n");
		goto error;
	}

	/*read data*/
	if (__frame_size % 4)
		__frame_size = (((__frame_size >> 2) + 1) << 2);

	lg4945_reg_read(dev, M2_RAWDATA_ADDR, (u8 *)buf, __frame_size);

	return 0;

error:
	return 1;
}

static ssize_t show_delta(struct device *dev, char *buf)
{
	int ret = 0;
	int ret2 = 0;
	int16_t *delta = NULL;
	int i = 0;
	int j = 0;

	delta = kzalloc(sizeof(int16_t) * (COL_SIZE*ROW_SIZE), GFP_KERNEL);

	if (delta == NULL) {
		TOUCH_E("delta mem_error\n");
		return ret;
	}

	ret = snprintf(buf, PAGE_SIZE, "======== Deltadata ========\n");

	ret2 = get_data(dev, delta, 2);  /* 2 == deltadata */
	if (ret2 == 1) {
		TOUCH_E("Test fail (Check if LCD is OFF)\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Test fail (Check if LCD is OFF)\n");
		goto error;
	}

	for (i = 0 ; i < ROW_SIZE; i++) {
		char log_buf[LOG_BUF_SIZE] = {0,};
		int log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", i);
		log_ret += snprintf(log_buf + log_ret,
				LOG_BUF_SIZE - log_ret, "[%2d]  ", i);

		for (j = 0 ; j < COL_SIZE ; j++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%5d ", delta[i * COL_SIZE + j]);
			log_ret += snprintf(log_buf + log_ret,
				LOG_BUF_SIZE - log_ret,
				"%5d ", delta[i * COL_SIZE + j]);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		TOUCH_I("%s\n", log_buf);
	}

error:
	if (delta != NULL)
		kfree(delta);

	return ret;
}

static ssize_t show_fdata(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;
	int ret2 = 0;
	u8 type = DOZE1_M2_RAWDATA_TEST;

	/* LCD off */
	if (d->lcd_mode != LCD_MODE_U3) {
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
				"LCD Off. Test Result : Fail\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	ret2 = write_test_mode(dev, type);
	if (ret2 == 0) {
		TOUCH_E("write_test_mode fail\n");
		ts->driver->power(dev, POWER_OFF);
		ts->driver->power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
		ts->driver->init(dev);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
		return ret;
	}

	prd_read_rawdata(dev, type);
	ret = prd_print_rawdata(dev, buf, type);

	ts->driver->power(dev, POWER_OFF);
	ts->driver->power(dev, POWER_ON);
	touch_msleep(90);
	ts->driver->init(dev);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_rawdata(struct device *dev, char *buf)
{
	int ret = 0;
	int ret2 = 0;
	int16_t *rawdata = NULL;
	int i = 0;
	int j = 0;

	rawdata = kzalloc(sizeof(int16_t) * (COL_SIZE*ROW_SIZE), GFP_KERNEL);

	if (rawdata == NULL) {
		TOUCH_E("mem_error\n");
		return ret;
	}

	ret = snprintf(buf, PAGE_SIZE, "======== rawdata ========\n");

	ret2 = get_data(dev, rawdata, 1);  /* 2 == deltadata */
	if (ret2 == 1) {
		TOUCH_E("Test fail (Check if LCD is OFF)\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Test fail (Check if LCD is OFF)\n");
		goto error;
	}

	for (i = 0 ; i < ROW_SIZE ; i++) {
		char log_buf[LOG_BUF_SIZE] = {0,};
		int log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", i);
		log_ret += snprintf(log_buf + log_ret,
				LOG_BUF_SIZE - log_ret,
				"[%2d]  ", i);

		for (j = 0 ; j < COL_SIZE ; j++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%5d ",
				rawdata[i * COL_SIZE + j]);
			log_ret += snprintf(log_buf + log_ret,
				LOG_BUF_SIZE - log_ret,
				"%5d ",
				rawdata[i * COL_SIZE + j]);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		TOUCH_I("%s\n", log_buf);
	}

error:
	if (rawdata != NULL)
		kfree(rawdata);

	return ret;
}

static ssize_t show_lpwg_sd(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int m1_rawdata_ret = 0;
	int m2_rawdata_ret = 0;
	int ret = 0;

	/* file create , time log */
	write_file(dev, "\nShow_lpwg_sd Test Start", TIME_INFO_SKIP);
	write_file(dev, "\n", TIME_INFO_WRITE);
	TOUCH_I("Show_lpwg_sd Test Start\n");

	/* LCD mode check */
	if (d->lcd_mode != LCD_MODE_U0) {
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"LCD mode is not U0. Test Result : Fail\n");
		return ret;
	}

	firmware_version_log(dev);
	ic_run_info_print(dev);

	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	lg4945_tc_driving(dev, LCD_MODE_STOP);

	m2_rawdata_ret = prd_rawdata_test(dev, DOZE2_M2_RAWDATA_TEST);
	m1_rawdata_ret = prd_rawdata_test(dev, DOZE2_M1_RAWDATA_TEST);

	ret = snprintf(buf, PAGE_SIZE, "========RESULT=======\n");

	if (!m1_rawdata_ret && !m2_rawdata_ret) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"LPWG RawData : %s\n", "Pass");
	} else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"LPWG RawData : %s (%d/%d)\n", "Fail",
			m1_rawdata_ret ? 0 : 1, m2_rawdata_ret ? 0 : 1);
	}

	write_file(dev, buf, TIME_INFO_SKIP);

	ts->driver->power(dev, POWER_OFF);
	ts->driver->power(dev, POWER_ON);
	touch_msleep(90);
	ts->driver->init(dev);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	write_file(dev, "Show_lpwg_sd Test End\n", TIME_INFO_WRITE);
	log_file_size_check(dev);
	TOUCH_I("Show_lpwg_sd Test End\n");

	return ret;
}

static TOUCH_ATTR(sd, show_sd, NULL);
static TOUCH_ATTR(delta, show_delta, NULL);
static TOUCH_ATTR(fdata, show_fdata, NULL);
static TOUCH_ATTR(rawdata, show_rawdata, NULL);
static TOUCH_ATTR(lpwg_sd, show_lpwg_sd, NULL);

static struct attribute *prd_attribute_list[] = {
	&touch_attr_sd.attr,
	&touch_attr_delta.attr,
	&touch_attr_fdata.attr,
	&touch_attr_rawdata.attr,
	&touch_attr_lpwg_sd.attr,
	NULL,
};

static const struct attribute_group prd_attribute_group = {
	.attrs = prd_attribute_list,
};

int lg4945_prd_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &prd_attribute_group);

	if (ret < 0) {
		TOUCH_E("failed to create sysfs\n");
		return ret;
	}

	return ret;
}
