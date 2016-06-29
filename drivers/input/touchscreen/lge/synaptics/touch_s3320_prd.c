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

#include <touch_hwif.h>
#include <touch_core.h>

#include "touch_s3320.h"
#include "touch_s3320_prd.h"
#include "./DS5/RefCode_F54.h"

static char line[98304] = {0};
int UpperImage[TRX_max][TRX_max];
int LowerImage[TRX_max][TRX_max];
int SensorSpeedUpperImage[TRX_max][TRX_max];
int SensorSpeedLowerImage[TRX_max][TRX_max];
int ADCUpperImage[TRX_max][TRX_max];
int ADCLowerImage[TRX_max][TRX_max];
int RspLowerSlope[TRX_max][TRX_max];
int RspUpperSlope[TRX_max][TRX_max];
int RspNoise[TRX_max][TRX_max];

int f54_window_crack_check_mode = 0;
int f54_window_crack = 0;

static struct prd_handle *prd;

int synaptics_prd_handle(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int ret = 0;

	prd = kzalloc(sizeof(*prd), GFP_KERNEL);

	if (!prd) {
		TOUCH_E("Failed to alloc mem for prd\n");
		ret = -ENOMEM;
		return ret;
	}
	prd->c_data = ts;

	return ret;
}

int Read8BitRegisters(unsigned short regAddr,
				unsigned char *data, int length)
{
	struct touch_core_data *ts = prd->c_data;
	return synaptics_read(ts->dev, regAddr, data, length);
}

int Write8BitRegisters(unsigned short regAddr,
				unsigned char *data, int length)
{
	struct touch_core_data *ts = prd->c_data;
	return synaptics_write(ts->dev, regAddr, data, length);
}

static void log_file_size_check(struct device *dev)
{
	char *fname = NULL;
	struct file *file;
	loff_t file_size = 0;
	int i = 0;
	char buf1[512] = {0};
	char buf2[512] = {0};
	mm_segment_t old_fs = get_fs();
	int ret = 0;
	int mfts_mode = 0;

	set_fs(KERNEL_DS);

	mfts_mode = touch_boot_mode_check(dev);

	switch (mfts_mode) {
	case MFTS_NONE:
		fname = "/sdcard/touch_self_test.txt";
		break;
	case MFTS_FOLDER:
		fname = "/data/logger/touch_self_mfts_folder.txt";
		break;
	case MFTS_FLAT:
		fname = "/data/logger/touch_self_mfts_flat.txt";
		break;
	case MFTS_CURVED:
		fname = "/data/logger/touch_self_mfts_curved.txt";
		break;
	default:
		TOUCH_I("%s : not support mfts_mode\n", __func__);
		break;
	}

	if (fname) {
		file = filp_open(fname, O_RDONLY, 0666);
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

void write_file(char *data, int write_time)
{
	struct touch_core_data *ts = prd->c_data;

	int fd = 0;
	char *fname = NULL;
	char time_string[64] = {0};
	struct timespec my_time;
	struct tm my_date;
	mm_segment_t old_fs = get_fs();
	int mfts_mode = 0;

	set_fs(KERNEL_DS);

	mfts_mode = touch_boot_mode_check(ts->dev);

	switch (mfts_mode) {
	case MFTS_NONE:
		fname = "/sdcard/touch_self_test.txt";
		break;
	case MFTS_FOLDER:
		fname = "/data/logger/touch_self_mfts_folder.txt";
		break;
	case MFTS_FLAT:
		fname = "/data/logger/touch_self_mfts_flat.txt";
		break;
	case MFTS_CURVED:
		fname = "/data/logger/touch_self_mfts_curved.txt";
		break;
	default:
		TOUCH_I("%s : not support mfts_mode\n", __func__);
		break;
	}

	if (fname) {
		fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);

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

static int sdcard_spec_file_read(struct device *dev)
{
	int ret = 0;
	int fd = 0;
	char *path[4] = { "/data/logger/p1_limit.txt",
		"/data/logger/p1_limit_mfts.txt",
		"/data/logger/p1_limit_mfts_flat.txt",
		"/data/logger/p1_limit_mfts_curved.txt" };
	int mfts_mode = 0;

	mm_segment_t old_fs = get_fs();

	if(touch_boot_mode_check(dev) >= MINIOS_MFTS_FOLDER)
		mfts_mode = 1;
	else
		mfts_mode = 0;

	set_fs(KERNEL_DS);

	fd = sys_open(path[mfts_mode], O_RDONLY, 0);
	if (fd >= 0) {
		sys_read(fd, line, sizeof(line));
		sys_close(fd);
		TOUCH_I("%s file existing\n", path[mfts_mode]);
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
	const char *path[4] = { ts->panel_spec, ts->panel_spec_mfts,
				ts->panel_spec_mfts_flat,
				ts->panel_spec_mfts_curved };
	int mfts_mode = 0;

	if(touch_boot_mode_check(dev) >= MINIOS_MFTS_FOLDER)
		mfts_mode = 1;
	else
		mfts_mode = 0;

	if (ts->panel_spec == NULL || ts->panel_spec_mfts == NULL
		|| ts->panel_spec_mfts_flat == NULL
		|| ts->panel_spec_mfts_curved == NULL) {
		TOUCH_I("panel_spec_file name is null\n");
		ret = -1;
		goto error;
	}

	if (request_firmware(&fwlimit, path[mfts_mode], dev) < 0) {
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

int synaptics_get_limit(struct device *dev, char *breakpoint,
			unsigned char Tx, unsigned char Rx,
			int limit_data[32][32])
{
	int p = 0;
	int q = 0;
	int r = 0;
	int cipher = 1;
	int tx_num = 0, rx_num = 0;
	int ret = 0;
	char *found;
	int mfts_mode = 0;
	int file_exist = 0;

	if (breakpoint == NULL) {
		ret = -1;
		goto error;
	}

	mfts_mode = touch_boot_mode_check(dev);
	/*if (mfts_mode > 0)
		mfts_mode = 1;

	if (mfts_mode > 1 || mfts_mode < 0) {
		ret = -1;
		goto error;
	}*/

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
		TOUCH_I("failed to find breakpoint. The panel_spec_file is wrong\n");
		ret = -1;
		goto error;
	}

	memset(limit_data, 0, (TRX_max * TRX_max) * 4);

	while (1) {
		if (line[q] == ',') {
			cipher = 1;
			for (p = 1; (line[q - p] >= '0') &&
					(line[q - p] <= '9'); p++) {
				limit_data[tx_num][rx_num] +=
					((line[q - p] - '0') * cipher);
				cipher *= 10;

			}
			if (line[q - p] == '-') {
				limit_data[tx_num][rx_num] = (-1) *
					(limit_data[tx_num][rx_num]);
			}
			r++;

			if (r % (int)Rx == 0) {
				rx_num = 0;
				tx_num++;
			} else {
				rx_num++;
			}
		}
		q++;

		if (r == (int)Tx * (int)Rx) {
			TOUCH_I("panel_spec_file scanning is success\n");
			break;
		}
	}

error:
	return ret;
}

static void firmware_version_log(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;
	unsigned char buffer[LOG_BUF_SIZE] = {0,};
	int mfts_mode = 0;

	TOUCH_I("%s\n", __func__);

	mfts_mode = touch_boot_mode_check(dev);
	if (mfts_mode)
		ret = synaptics_ic_info(dev);

	ret = snprintf(buffer, LOG_BUF_SIZE,
			"======== Firmware Info ========\n");

	if (d->fw.version[0] > 0x50) {
		ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"version : %s\n", d->fw.version);
	} else {
		ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"version : v%d.%02d\n",
			((d->fw.version[3] & 0x80) >> 7),
			(d->fw.version[3] & 0x7F));
	}

	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"product id : %s\n", d->fw.product_id);
	ret += snprintf(buffer + ret, LOG_BUF_SIZE - ret,
			"===============================\n\n");

	write_file(buffer, TIME_INFO_SKIP);
}

static int synaptics_im_test(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	unsigned int i = 0, im_fail_max = 150;
	u8 buf_1 = 0, buf_2 = 0, curr = 0;
	u16 im = 0, im_test_max = 0, result = 0;
	char f54_buf[1000] = {0};
	int f54_len = 0;
	int im_result = 0;

	TOUCH_I("%s\n", __func__);

	f54_len = snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
				"RSP IM Test Result\n");

	synaptics_set_page(dev, LPWG_PAGE);
	synaptics_read(dev, LPWG_PARTIAL_REG + 71, &curr, sizeof(curr));
	curr = (curr & 0xff) | 0x02;
	synaptics_write(dev, LPWG_PARTIAL_REG + 71, &curr, sizeof(curr));
	touch_msleep(20);

	for (i = 0 ; i < 10 ; i++) {
		synaptics_set_page(dev, ANALOG_PAGE);
		synaptics_read(dev, d->f54_reg.interference__metric_LSB,
					&buf_1, sizeof(buf_1));
		synaptics_read(dev, d->f54_reg.interference__metric_MSB,
					&buf_2, sizeof(buf_2));
		im = (buf_2 << 8) | buf_1;

		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"%d : Current IM value = %d\n", i, im);

		if (im > im_test_max)
			im_test_max = im;

		TOUCH_I("im_test_max : %u, retry_cnt : %u\n", im_test_max, i);
		touch_msleep(5);
	}
	result = im_test_max;
	TOUCH_I("result : %u\n", result);

	curr = (curr & 0xff) & 0xfd;
	synaptics_set_page(dev, LPWG_PAGE);
	synaptics_write(dev, LPWG_PARTIAL_REG + 71, &curr, sizeof(curr));

	f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
				"\nMAX IM value = %d\n", result);

	if (result < im_fail_max) {
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"RSP IM TEST passed\n\n");
		im_result = 1;
	} else {
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"RSP IM TEST failed\n\n");
		im_result = 0;
	}

	write_file(f54_buf, TIME_INFO_SKIP);
	touch_msleep(30);

	return im_result;
}


static int synaptics_adc_test(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	unsigned int i = 0, temp_cnt = 0;
	u8 buf[42] = {0};
	u16 result = 0, adc_result = 0, adc_fail_max = 3800, adc_fail_min = 400;
	u16 adc[20] = {0};
	char f54_buf[1000] = {0};
	int f54_len = 0;

	TOUCH_I("%s\n", __func__);

	f54_len = snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
				"ADC Test Result\n");
	write_file(f54_buf, TIME_INFO_SKIP);
	f54_len = 0;

	synaptics_set_page(dev, ANALOG_PAGE);
	synaptics_read(dev, d->f54_reg.incell_statistic, buf, 50);
	synaptics_set_page(dev, DEFAULT_PAGE);

	for (i = 0 ; i < 21 ; i++) {
		if (i < 4)
			continue;

		temp_cnt = i * 2;
		adc[i] = (buf[temp_cnt + 1] << 8) | buf[temp_cnt];
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"%d : Adc mesured value = %d\n", i, adc[i]);
		TOUCH_I("Adc value adc[%d] = %d\n", i + 1, adc[i]);

		if (adc[i] > adc_fail_max || adc[i] < adc_fail_min)
			adc_result++;

		write_file(f54_buf, TIME_INFO_SKIP);
		f54_len = 0;
	}

	if (adc_result) {
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"ADC TEST Failed\n");
		TOUCH_E("ADC Test has failed!!\n");
		result = 0;
	} else {
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"ADC TEST Passed\n\n");
		TOUCH_I("ADC Test has passed\n");
		result = 1;
	}

	write_file(f54_buf, TIME_INFO_SKIP);
	touch_msleep(30);

	return result;
}

static int synaptics_sd_rsp(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int ret = 0;
	int lower_limit = 0;
	int upper_limit = 0;
	int noise_limit = 0;
	int im_ret = 0;
	int rawdata_ret = 0;
	int short_ret = 0;
	int coarse_cal_ret = 0;

	TOUCH_I("%s\n", __func__);

	lower_limit = synaptics_get_limit(dev, "RspLowerImageLimit",
				TxChannelCount, RxChannelCount, LowerImage);
	upper_limit = synaptics_get_limit(dev, "RspUpperImageLimit",
				TxChannelCount, RxChannelCount, UpperImage);
	noise_limit = synaptics_get_limit(dev, "RspNoiseP2PLimit",
				TxChannelCount, RxChannelCount, RspNoise);

	if (lower_limit < 0 || upper_limit < 0 || noise_limit < 0) {
		TOUCH_E("[Fail] lower_limit = %d, upper_limit = %d, noise_limit = %d\n",
				lower_limit, upper_limit, noise_limit);
		TOUCH_E("[Fail] Can not check the limit of raw cap or noise\n");
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
				"Can not check the limit of raw cap or noise\n");
		return ret;
	}
	TOUCH_I("[Success] Can check the limit of raw cap, noise\n");

	mutex_lock(&ts->lock);
	touch_disable_irq(ts->irq);
	SCAN_PDT();

	im_ret = synaptics_im_test(dev);
	touch_msleep(20);
	rawdata_ret = F54Test('q', 0, buf);
	touch_msleep(100);
	short_ret = F54Test('s', 0, buf);
	touch_msleep(100);
	coarse_cal_ret = F54Test('q', 4, buf);
	touch_msleep(100);

	synaptics_init(dev);
	touch_enable_irq(ts->irq);
	mutex_unlock(&ts->lock);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"\n========RESULT=======\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Channel Status : %s",
				(short_ret == 1) ? "Pass\n" : "Fail\n");

	if (im_ret == 1 && rawdata_ret == 1 && coarse_cal_ret == 1) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Raw Data : Pass\n\n");
	}

	if (!(rawdata_ret && im_ret && coarse_cal_ret)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Raw Data : Fail (%s/%s/%s)\n\n",
					(im_ret == 0) ? "0" : "1",
					(rawdata_ret == 0) ? "0" : "1",
					(coarse_cal_ret == 0) ? "0" : "1");
	}

	return ret;
}

static int synaptics_sd_adc(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int ret = 0;
	int lower_limit = 0;
	int upper_limit = 0;
	int lower_sensor = 0;
	int upper_sensor = 0;
	int lower_adc = 0;
	int upper_adc = 0;
	int adc_ret = 0;
	int rawdata_ret = 0;
	int short_ret = 0;
	int high_resistance_ret = 0;
	int sensor_speed_ret = 0;
	int adc_range_ret = 0;

	TOUCH_I("%s\n", __func__);

	lower_limit = synaptics_get_limit(dev, "LowerImageLimit",
				TxChannelCount, RxChannelCount, LowerImage);
	upper_limit = synaptics_get_limit(dev, "UpperImageLimit",
				TxChannelCount, RxChannelCount, UpperImage);
	lower_sensor = synaptics_get_limit(dev, "SensorSpeedLowerImageLimit",
				TxChannelCount, RxChannelCount, SensorSpeedLowerImage);
	upper_sensor = synaptics_get_limit(dev, "SensorSpeedUpperImageLimit",
				TxChannelCount, RxChannelCount, SensorSpeedUpperImage);
	lower_adc = synaptics_get_limit(dev, "ADCLowerImageLimit",
				TxChannelCount, RxChannelCount, ADCLowerImage);
	upper_adc = synaptics_get_limit(dev, "ADCUpperImageLimit",
				TxChannelCount, RxChannelCount, ADCUpperImage);

	if (lower_limit < 0 || upper_limit < 0) {
		TOUCH_E("[Fail] lower_limit = %d, upper_limit = %d\n",
				lower_limit, upper_limit);
		TOUCH_E("[Fail] Can not check the limit of raw cap\n");
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
				"Can not check the limit of raw cap\n");
		return ret;
	}

	if (lower_sensor < 0 || upper_sensor < 0) {
		TOUCH_E("[Fail] lower_sensor = %d, upper_sensor = %d\n",
				lower_sensor, upper_sensor);
		TOUCH_E("[Fail] Can not check the limit of sensor\n");
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
				"Can not check the limit of sensor\n");
		return ret;
	}

	if (lower_adc < 0 || upper_adc < 0) {
		TOUCH_E("[Fail] lower_adc = %d, upper_adc = %d\n",
				lower_adc, upper_adc);
		TOUCH_E("[Fail] Can not check the limit of adc range\n");
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
				"Can not check the limit of adc range\n");
		return ret;
	}
	TOUCH_I("[Success] Can check the limit of raw cap, sensor, adc range\n");

	mutex_lock(&ts->lock);
	touch_disable_irq(ts->irq);
	SCAN_PDT();

	adc_ret = synaptics_adc_test(dev);
	touch_msleep(20);
	rawdata_ret = F54Test('a', 0, buf);
	touch_msleep(30);
	short_ret = F54Test('f', 0, buf);
	touch_msleep(50);
	high_resistance_ret = F54Test('g', 0, buf);
	touch_msleep(50);
	sensor_speed_ret = F54Test('c', 0, buf);
	touch_msleep(50);
	adc_range_ret = F54Test('b', 0, buf);

	synaptics_init(dev);
	touch_enable_irq(ts->irq);
	mutex_unlock(&ts->lock);
	touch_msleep(30);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"\n========RESULT=======\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "Channel Status : %s",
				(short_ret == 1 && high_resistance_ret == 1)
				? "Pass\n" : "Fail ");

	if (!(short_ret && high_resistance_ret)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "(%s/%s)",
					(short_ret == 0 ? "0" : "1"),
					(high_resistance_ret == 0 ? "0" : "1"));
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "Raw Data : %s",
				((rawdata_ret > 0) && adc_ret == 1)
				? "Pass\n" : "Fail");

	if (!(rawdata_ret && adc_ret)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "(%s/%s)",
					(rawdata_ret == 0 ? "0" : "1"),
					(adc_ret == 0 ? "0" : "1"));
	}

	return ret;
}

static ssize_t show_sd(struct device *dev, char *buf)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->power_state == POWER_SLEEP) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"power_state[suspend]. Can not sd.\n");
		return ret;
	}

	/* file create , time log */
	write_file("\nShow_sd Test Start", TIME_INFO_SKIP);
	write_file("\n", TIME_INFO_WRITE);
	TOUCH_I("Show_sd Test Start\n");

	firmware_version_log(dev);

	if (synaptics_is_product(d, "PLG468", 6))
		ret = synaptics_sd_rsp(dev, buf);
	else
		ret = synaptics_sd_adc(dev, buf);

	write_file("Show_sd Test End\n", TIME_INFO_WRITE);
	TOUCH_I("Show_sd Test End\n");
	log_file_size_check(dev);

	return ret;
}

static ssize_t show_delta(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->power_state == POWER_SLEEP) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"power_state[suspend]. Can not delta_test.\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	if (d->need_scan_pdt) {
		SCAN_PDT();
		d->need_scan_pdt = false;
	}

	touch_disable_irq(ts->irq);

	if (synaptics_is_product(d, "PLG468", 6))
		ret = F54Test('q', 2, buf);
	else
		ret = F54Test('m', 0, buf);

	if (ret == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Test Result : full_raw_cap fail.\n");
	}

	touch_enable_irq(ts->irq);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_rawdata(struct device *dev, char *buf)
{
	struct touch_core_data *ts  = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->power_state == POWER_SLEEP) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"power_state[suspend]. Can not rawdata_test.\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	if (d->need_scan_pdt) {
		SCAN_PDT();
		d->need_scan_pdt = false;
	}
	touch_disable_irq(ts->irq);

	if (synaptics_is_product(d, "PLG468", 6))
		ret = F54Test('q', 1, buf);
	else
		ret = F54Test('a', 1, buf);

	if (ret == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Test Result : full_raw_cap fail.\n");
	}

	touch_enable_irq(ts->irq);
	synaptics_init(dev);
	mutex_unlock(&ts->lock);

	return ret;
}


static ssize_t show_chstatus(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;
	int short_ret = 0;
	int high_resistance_ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->power_state == POWER_SLEEP) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"power_state[suspend]. Can not short_test.\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	if (d->need_scan_pdt) {
		SCAN_PDT();
		d->need_scan_pdt = false;
	}

	touch_disable_irq(ts->irq);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "========RESULT=======\n");

	if (synaptics_is_product(d, "PLG468", 6)) {
		short_ret = F54Test('s', 0, buf);

		ret = snprintf(buf + ret, PAGE_SIZE - ret, "Channel Status : %s",
					(short_ret == 1) ? "Pass\n" : "Fail\n");
	} else {
		short_ret = F54Test('f', 0, buf);
		high_resistance_ret = F54Test('g', 0, buf);

		ret = snprintf(buf + ret, PAGE_SIZE - ret, "TRex Short result : %s",
					(short_ret > 0) ? "Pass\n" : "Fail\n");

		ret = snprintf(buf + ret, PAGE_SIZE - ret, "High Resistance result : %s",
					(high_resistance_ret > 0) ? "Pass\n" : "Fail\n");
	}

	touch_enable_irq(ts->irq);
	synaptics_init(dev);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_noise_delta(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;
	int noise_ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->power_state == POWER_SLEEP) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"power_state[suspend]. Can not noise_test.\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	if (d->need_scan_pdt) {
		SCAN_PDT();
		d->need_scan_pdt = false;
	}

	touch_disable_irq(ts->irq);

	if (synaptics_is_product(d, "PLG468", 6)) {
		ret = F54Test('q', 3, buf);
	} else {
		noise_ret = F54Test('q', 3, buf);

		ret = snprintf(buf + ret, PAGE_SIZE - ret, "Noise Delta result : %s",
					(noise_ret > 0) ? "Pass\n" : "Fail\n");
	}

	touch_enable_irq(ts->irq);
	synaptics_init(dev);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_abs(struct device *dev, char *buf)
{
	TOUCH_TRACE();

	return 0;
}

static ssize_t show_sensor_speed(struct device *dev, char *buf)
{
	TOUCH_TRACE();

	return 0;
}

static ssize_t show_adc_range(struct device *dev, char *buf)
{
	TOUCH_TRACE();

	return 0;
}

static ssize_t show_gnd(struct device *dev, char *buf)
{
	TOUCH_TRACE();

	return 0;
}

static int synaptics_lpwg_sd_rsp(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int ret = 0;
	int lower_limit = 0;
	int upper_limit = 0;
	int lpwg_ret = 0;

	TOUCH_I("%s\n", __func__);

	lower_limit = synaptics_get_limit(dev, "RspLPWGLowerLimit",
				TxChannelCount, RxChannelCount, LowerImage);
	upper_limit = synaptics_get_limit(dev, "RspLPWGUpperLimit",
				TxChannelCount, RxChannelCount, UpperImage);

	if (lower_limit < 0 || upper_limit < 0) {
		TOUCH_E("[Fail] lower_limit = %d, upper_limit = %d\n",
				lower_limit, upper_limit);
		TOUCH_E("[Fail] Can not check the limit of LPWG raw cap\n");
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
				"Can not check the limit of LPWG raw cap\n");
		return ret;
	}
	TOUCH_I("[Success] Can check the limit of LPWG raw cap\n");

	mutex_lock(&ts->lock);
	touch_msleep(1000);
	SCAN_PDT();
	touch_disable_irq(ts->irq);
	touch_msleep(30);
	lpwg_ret = F54Test('q', 5, buf);
	touch_msleep(20);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n========RESULT=======\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "LPWG RawData : Not Support\n");

	touch_enable_irq(ts->irq);
	mutex_unlock(&ts->lock);

	return ret;
}

static int synaptics_lpwg_adc_test(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	unsigned int i = 0;
	u8 buf[5] = {0}, curr;
	u16 result = 0, adc_result = 0;
	unsigned int adc_fail_max = 54299, adc_fail_min = 20998;
	u16 adc[20] = {0};
	char f54_buf[1000] = {0};
	int f54_len = 0;

	TOUCH_I("%s\n", __func__);

	f54_len = snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
				"LPWG ADC Test Result\n");
	write_file(f54_buf, TIME_INFO_SKIP);
	f54_len = 0;

	synaptics_set_page(dev, LPWG_PAGE);
	curr = 1;
	synaptics_write(dev, d->f51_reg.lpwg_adc_offset_reg, &curr, sizeof(curr));
	touch_msleep(20);

	for (i = 0 ; i < 17 ; i++) {
		synaptics_read(dev, d->f51_reg.lpwg_adc_offset_reg,
					&buf[0], sizeof(buf[0]));

		if (buf[0] != i) {
			TOUCH_E("LPWG ADC Test offset update erro\n");
			return -EIO;
		}

		synaptics_read(dev, d->f51_reg.lpwg_adc_fF_reg1,
					&buf[1], sizeof(buf[1]));
		synaptics_read(dev, d->f51_reg.lpwg_adc_fF_reg2,
					&buf[2], sizeof(buf[2]));
		synaptics_read(dev, d->f51_reg.lpwg_adc_fF_reg3,
					&buf[3], sizeof(buf[3]));
		synaptics_read(dev, d->f51_reg.lpwg_adc_fF_reg4,
					&buf[4], sizeof(buf[4]));

		adc[i] = (buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];
		TOUCH_I("LPWG ADC Test value : %d\t"
				"(buf[4] 0x%02x, buf[3] 0x%02x\t"
				"buf[2] 0x%02x, buf[1] 0x%02x)\n",
				adc[i], buf[4], buf[3], buf[2], buf[1]);
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"%d : LPWG Adc mesured value = %d\n", i, adc[i]);

		TOUCH_I("LPWG Adc value adc[%d] = %d\n", i + 1, adc[i]);

		if (adc[i] > adc_fail_max || adc[i] < adc_fail_min)
			adc_result++;

		write_file(f54_buf, TIME_INFO_SKIP);
		f54_len = 0;
	}

	if (adc_result) {
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"LPWG ADC TEST Failed\n");
		TOUCH_E("LPWG ADC Test has failed!!\n");
		result = 0;
	} else {
		f54_len += snprintf(f54_buf + f54_len, sizeof(f54_buf) - f54_len,
					"LPWG ADC TEST Passed\n\n");
		TOUCH_I("LPWG ADC Test has passed\n");
		result = 1;
	}

	write_file(f54_buf, TIME_INFO_SKIP);
	touch_msleep(30);

	return result;
}

static int synaptics_lpwg_sd_adc(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int ret = 0;
	int lpwg_ret = 0;

	mutex_lock(&ts->lock);
	touch_disable_irq(ts->irq);
	lpwg_ret = synaptics_lpwg_adc_test(dev);
	touch_msleep(20);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n========RESULT=======\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "LPWG RawData : %s",
				(lpwg_ret == 1) ? "Pass\n" : "Fail\n");

	touch_enable_irq(ts->irq);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_lpwg_sd(struct device *dev, char *buf)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->power_state != POWER_SLEEP) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"power_state[not suspend]. Can not lpwg_sd.\n");
		return ret;
	}

	/* file create , time log */
	write_file("\nShow_lpwg_sd Test Start", TIME_INFO_SKIP);
	write_file("\n", TIME_INFO_WRITE);
	TOUCH_I("Show_lpwg_sd Test Start\n");

	firmware_version_log(dev);

	if (synaptics_is_product(d, "PLG468", 6))
		ret = synaptics_lpwg_sd_rsp(dev, buf);
	else
		ret = synaptics_lpwg_sd_adc(dev, buf);

	write_file("Show_lpwg_sd Test End\n", TIME_INFO_WRITE);
	TOUCH_I("Show_lpwg_sd Test End\n");
	log_file_size_check(dev);

	return ret;
}

static TOUCH_ATTR(sd, show_sd, NULL);
static TOUCH_ATTR(delta, show_delta, NULL);
static TOUCH_ATTR(rawdata, show_rawdata, NULL);
static TOUCH_ATTR(chstatus, show_chstatus, NULL);
static TOUCH_ATTR(noise_test, show_noise_delta, NULL);
static TOUCH_ATTR(abs_test, show_abs, NULL);
static TOUCH_ATTR(sensor_speed_test, show_sensor_speed, NULL);
static TOUCH_ATTR(adc_range_test, show_adc_range, NULL);
static TOUCH_ATTR(gnd_test, show_gnd, NULL);
static TOUCH_ATTR(lpwg_sd, show_lpwg_sd, NULL);

static struct attribute *prd_attribute_list[] = {
	&touch_attr_sd.attr,
	&touch_attr_delta.attr,
	&touch_attr_rawdata.attr,
	&touch_attr_chstatus.attr,
	&touch_attr_noise_test.attr,
	&touch_attr_abs_test.attr,
	&touch_attr_sensor_speed_test.attr,
	&touch_attr_adc_range_test.attr,
	&touch_attr_gnd_test.attr,
	&touch_attr_lpwg_sd.attr,
	NULL,
};

static const struct attribute_group prd_attribute_group = {
	.attrs = prd_attribute_list,
};

int s3320_prd_register_sysfs(struct device *dev)
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
