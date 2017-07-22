/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
#include "RefCode_F54.h"
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>	/* msleep */
#include <linux/file.h>		/* for file access */
#include <linux/syscalls.h>	/* for file access */
#include <linux/uaccess.h>	/* for file access */
#include <linux/firmware.h>

#define MAX_LOG_FILE_SIZE	(10 * 1024 * 1024)	/* 10 MBytes */
#define MAX_LOG_FILE_COUNT	(4)

static char line[98304] = {0};
int UpperImage[32][32];
int LowerImage[32][32];
int SensorSpeedUpperImage[32][32];
int SensorSpeedLowerImage[32][32];
int ADCUpperImage[32][32];
int ADCLowerImage[32][32];
int RspLowerSlope[32][32];
int RspUpperSlope[32][32];
int RspNoise[32][32];

int Read8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	return touch_i2c_read(ds4_i2c_client, regAddr, length, data);
}

int Write8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	return touch_i2c_write(ds4_i2c_client, regAddr, length, data);
}

void delayMS(int val)
{
	/* Wait for val MS */
	msleep(val);
}

void log_file_size_check(char *filename)
{
	struct file *file;
	loff_t file_size = 0;
	int i = 0;
	char buf1[1024] = {0};
	char buf2[1024] = {0};
	mm_segment_t old_fs = get_fs();
	int ret = 0;

	set_fs(KERNEL_DS);

	if (filename) {
		file = filp_open(filename, O_RDONLY, 0666);
	} else {
		TOUCH_E("%s : filename is NULL, can not open FILE\n",
				__func__);
		goto error;
	}

	if (IS_ERR(file)) {
		TOUCH_I("%s : ERR(%ld) Open file error [%s]\n",
				__func__, PTR_ERR(file), filename);
		goto error;
	}

	file_size = vfs_llseek(file, 0, SEEK_END);
	TOUCH_I("%s : [%s] file_size = %lld\n",
			__func__, filename, file_size);

	filp_close(file, 0);

	if (file_size > MAX_LOG_FILE_SIZE) {
		TOUCH_I("%s : [%s] file_size(%lld) > MAX_LOG_FILE_SIZE(%d)\n",
				__func__, filename, file_size, MAX_LOG_FILE_SIZE);

		for (i = MAX_LOG_FILE_COUNT - 1; i >= 0; i--) {
			if (i == 0)
				sprintf(buf1, "%s", filename);
			else
				sprintf(buf1, "%s.%d", filename, i);

			ret = sys_access(buf1, 0);

			if (ret == 0) {
				TOUCH_I("%s : file [%s] exist\n", __func__, buf1);

				if (i == (MAX_LOG_FILE_COUNT - 1)) {
					if (sys_unlink(buf1) < 0) {
						TOUCH_E(
								"%s : failed to remove file [%s]\n",
								__func__, buf1);
						goto error;
					}

					TOUCH_I(
							"%s : remove file [%s]\n",
							__func__, buf1);
				} else {
					sprintf(buf2, "%s.%d", filename,
							(i + 1));

					if (sys_rename(buf1, buf2) < 0) {
						TOUCH_E(
								"%s : failed to rename file [%s] -> [%s]\n",
								__func__, buf1, buf2);
						goto error;
					}

					TOUCH_I(
							"%s : rename file [%s] -> [%s]\n",
							__func__, buf1, buf2);
				}
			} else {
				TOUCH_I("%s : file [%s] does not exist (ret = %d)\n", __func__, buf1, ret);
			}
		}
	}

error:
	set_fs(old_fs);
	return;
}

int write_file(char *filename, char *data)
{
	struct file *file;
	loff_t pos = 0;

	file = filp_open(filename,
			O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (IS_ERR(file)) {
		TOUCH_I("%s :  Open file error [ %s ]\n", __func__,
				filename);
		return PTR_ERR(file);
	}

	vfs_write(file, data, strlen(data), &pos);
	filp_close(file, 0);

	return 0;
}

void _write_time_log(char *filename, char *data, int data_include)
{
	struct file *file;
	loff_t pos = 0;
	char *fname = NULL;
	char time_string[64] = {0};
	struct timespec my_time;
	struct tm my_date;

	mm_segment_t old_fs = get_fs();

	my_time = __current_kernel_time();
	time_to_tm(my_time.tv_sec, sys_tz.tz_minuteswest * 60 * (-1), &my_date);
	snprintf(time_string,
			sizeof(time_string),
			"%02d-%02d %02d:%02d:%02d.%03lu\n\n",
			my_date.tm_mon + 1,
			my_date.tm_mday,
			my_date.tm_hour,
			my_date.tm_min,
			my_date.tm_sec,
			(unsigned long) my_time.tv_nsec / 1000000);

	set_fs(KERNEL_DS);

	if (filename == NULL) {
		switch (mfts_mode) {
		case 0:
			fname = "/mnt/sdcard/touch_self_test.txt";
			break;
		case 1:
			fname = "/mnt/sdcard/touch_self_test_mfts_folder.txt";
			break;
		case 2:
			fname = "/mnt/sdcard/touch_self_test_mfts_flat.txt";
			break;
		case 3:
			fname = "/mnt/sdcard/touch_self_test_mfts_curved.txt";
			break;
		default:
			TOUCH_I("%s : not support mfts_mode\n", __func__);
			break;
		}
	} else {
		fname = filename;
	}

	if (fname) {
		file = filp_open(fname,
			O_WRONLY|O_CREAT|O_APPEND, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n", __func__);
		return;
	}

	if (IS_ERR(file)) {
		TOUCH_I("%s :  Open file error [%s], err = %ld\n",
				__func__, fname, PTR_ERR(file));

		set_fs(old_fs);
		return;
	}

	vfs_write(file, time_string, strlen(time_string), &pos);
	filp_close(file, 0);

	set_fs(old_fs);
}

void write_time_log(char *filename, char *data, int data_include)
{
	_write_time_log(filename, data, data_include);
}

int _write_log(char *filename, char *data)
{
	struct file *file;
	loff_t pos = 0;
	int flags;
	char *fname = "/mnt/sdcard/touch_self_test.txt";
	char *fname_mfts_folder = "/mnt/sdcard/touch_self_test_mfts_folder.txt";
	char *fname_mfts_flat = "/mnt/sdcard/touch_self_test_mfts_flat.txt";
	char *fname_mfts_curved = "/mnt/sdcard/touch_self_test_mfts_curved.txt";
	int cap_file_exist = 0;

	if (f54_window_crack || f54_window_crack_check_mode == 0) {
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);
		flags = O_WRONLY | O_CREAT;

		if (filename == NULL) {
			flags |= O_APPEND;
			switch (mfts_mode) {
			case 0:
				filename = fname;
				break;
			case 1:
				filename = fname_mfts_folder;
				break;
			case 2:
				filename = fname_mfts_flat;
				break;
			case 3:
				filename = fname_mfts_curved;
				break;
			default:
				TOUCH_I("%s : not support mfts_mode\n",
					__func__);
				break;
			}
		} else {
			cap_file_exist = 1;
		}

		if (filename) {
			file = filp_open(filename, flags, 0666);
		} else {
			TOUCH_E("%s : filename is NULL, can not open FILE\n",
				__func__);
			return -1;
		}

		if (IS_ERR(file)) {
			TOUCH_I("%s : ERR(%ld)  Open file error [%s]\n",
					__func__, PTR_ERR(file), filename);
			set_fs(old_fs);
			return PTR_ERR(file);
		}

		vfs_write(file, data, strlen(data), &pos);
		filp_close(file, 0);
		set_fs(old_fs);

		log_file_size_check(filename);
	}
	return cap_file_exist;
}

int write_log(char *filename, char *data)
{
		return _write_log(filename, data);
}

void read_log(char *filename, const struct touch_platform_data *pdata)
{
	struct file *file;
	loff_t pos = 0;
	int ret;
	char *buf = NULL;
	int rx_num = 0;
	int tx_num = 0;
	int data_pos = 0;
	int offset = 0;

	struct touch_platform_data *ppdata =
		(struct touch_platform_data *)pdata;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	file = filp_open(filename, O_RDONLY, 0);

	if (IS_ERR(file)) {
		TOUCH_I("%s : ERR(%ld)  Open file error [%s]\n",
				__func__, PTR_ERR(file), filename);
		set_fs(old_fs);
		return;
	}

	buf = kzalloc(1024, GFP_KERNEL);
	if (buf == NULL) {
		TOUCH_I("%s : allocation fail\n", __func__);
		return;
	}

	TOUCH_I(
			"[%s]open read_log funcion in /sns/touch/cap_diff_test.txt\n",
				__func__);

	while (vfs_read(file, buf, 1024, &pos)) {
		TOUCH_I("[%s]sys_read success\n", __func__);

		for (rx_num = 0; rx_num < (ppdata->rx_ch_count) - 1; rx_num++) {
			ret = sscanf(buf + data_pos, "%d%n",
					&ppdata->rx_cap[rx_num],
					&offset);
			if (ret > 0)
				data_pos += offset;
		}

		for (tx_num = 0; tx_num < (ppdata->tx_ch_count) - 1; tx_num++) {
			ret = sscanf(buf + data_pos,
						"%d%n",
						&ppdata->tx_cap[tx_num],
						&offset);
		       if (ret > 0)
				data_pos += offset;
		}

		TOUCH_I("[%s]rx_num = %d, tx_num = %d\n",
				__func__, rx_num, tx_num);
		TOUCH_I("[%s]rx_ch_count = %d, tx_ch_count = %d\n",
				__func__, ppdata->rx_ch_count,
				ppdata->tx_ch_count);

		if ((rx_num == (ppdata->rx_ch_count) - 1) &&
				(tx_num == (ppdata->tx_ch_count) - 1))
			break;
	}

	kfree(buf);
	filp_close(file, 0);

	set_fs(old_fs);
}

int check_cal_magic_key(void)
{
	int ret = 0;
	char *fname = "/sdcard/touch_lge_cal.txt";
	char *cal_magic = "lge3845";
	char buf[10];
	int flags;
	int retry = 0;
	int file = 0;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	flags = O_RDONLY;

	TOUCH_I("[%s] Start Checking Magic Key..\n", __func__);
	do {
	    mdelay(50);
	    file = sys_open(fname, flags, 0);
	    retry++;
	    TOUCH_I("[%s] Chcking Magic Key... retry = %d\n", __func__, retry);
	} while (file < 0 && retry < 50);

	if (file < 0) {
	    TOUCH_I("[%s] In-Cal Checking File does not exist\n", __func__);
	    set_fs(old_fs);
	    return -EIO;
	}

	sys_read(file, buf, 7);
	TOUCH_I("[%s] buf = %s\n", __func__, buf);

	if (!strncmp(cal_magic, buf, 7)) {
	    TOUCH_I("[%s] In Calibration Magic File Match.\n", __func__);
	} else {
	    TOUCH_I("[%s] In Calibration Magic File Not Match.\n", __func__);
	    sys_close(file);
	    set_fs(old_fs);
	    return -ENOMEM;
	}

	sys_close(file);
	set_fs(old_fs);
	return ret;
}

int get_limit(unsigned char Tx, unsigned char Rx, struct i2c_client client,
		const struct touch_platform_data *pdata, char *breakpoint,
		int limit_data[32][32])
{
	int p = 0;
	int q = 0;
	int r = 0;
	int cipher = 1;
	int ret = 0;
	int rx_num = 0;
	int tx_num = 0;
	const struct firmware *fwlimit = NULL;
	char *found;
	char *normal_path = "/mnt/sdcard/p1_limit.txt";
	char *folder_path = "/mnt/sdcard/p1_limit_mfts_folder.txt";
	char *flat_path = "/mnt/sdcard/p1_limit_mfts_flat.txt";
	char *curved_path = "/mnt/sdcard/p1_limit_mfts_curved.txt";
	struct file *file;
	int file_exist = 0;
	loff_t pos = 0;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	TOUCH_I("breakpoint = [%s]\n", breakpoint);

	if (pdata->panel_spec == NULL
		|| pdata->panel_spec_mfts_folder == NULL
		|| pdata->panel_spec_mfts_flat == NULL
		|| pdata->panel_spec_mfts_curved == NULL) {
		TOUCH_I("panel_spec_file name is null\n");
		ret =  -1;
		goto exit;
	}

	if (breakpoint == NULL) {
		ret =  -1;
		goto exit;
	}

	switch (mfts_mode) {
	case 0:
		file = filp_open(normal_path, O_RDONLY, 0);
		if (!IS_ERR(file)) {
			TOUCH_I(
			"normal_limit file is exist under /mnt/sdcard/\n");
			file_exist = 1;

		} else {
			if (request_firmware(&fwlimit, pdata->panel_spec,
			&client.dev) < 0) {
				TOUCH_I(
				"request ihex is failed in normal mode\n");
				ret =  -1;
				goto exit;
			}
		}
		break;
	case 1:
		file = filp_open(folder_path, O_RDONLY, 0);
		if (!IS_ERR(file)) {
			TOUCH_I(
			"folder_limit file is exist under /mnt/sdcard/\n");
			file_exist = 1;
		} else {
			if (request_firmware(&fwlimit,
			pdata->panel_spec_mfts_folder, &client.dev)
			< 0) {
				TOUCH_I(
				"request ihex is failed in mfts_folder mode\n");
				ret =  -1;
				goto exit;
			}
		}
		break;
	case 2:
		file = filp_open(flat_path, O_RDONLY, 0);
		if (!IS_ERR(file)) {
			TOUCH_I(
			"flat_limit file is exist under /mnt/sdcard/\n");
			file_exist = 1;
		} else {
			if (request_firmware(&fwlimit,
			pdata->panel_spec_mfts_flat, &client.dev)
			< 0) {
				TOUCH_I(
				"request ihex is failed in mfts_flat mode\n");
				ret =  -1;
				goto exit;
			}
		}
		break;
	case 3:
		file = filp_open(curved_path, O_RDONLY, 0);
		if (!IS_ERR(file)) {
			TOUCH_I(
			"curved_limit file is exist under /mnt/sdcard/\n");
			file_exist = 1;
		} else {
			if (request_firmware(&fwlimit,
			pdata->panel_spec_mfts_curved, &client.dev)
			< 0){
				TOUCH_I(
				"request ihex is failed in mfts_curved mode\n");
				ret =  -1;
				goto exit;
			}
		}
		break;
	default:
		TOUCH_I("%s : not support mfts_mode\n", __func__);
		ret =  -1;
		goto exit;
		break;
	}

	if (file_exist) {
		vfs_read(file, line, sizeof(line), &pos);
	} else {
		if (fwlimit->data == NULL) {
			ret =  -1;
			goto exit;
		}

		strlcpy(line, fwlimit->data, sizeof(line));
	}

	if (line == NULL) {
		ret =  -1;
		goto exit;
	}

	found = strnstr(line, breakpoint, sizeof(line));
	if (found != NULL) {
		q = found - line;
	} else {
		TOUCH_I(
				"failed to find breakpoint. The panel_spec_file is wrong\n");
		ret = -1;
		goto exit;
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

	if (file_exist) {
		filp_close(file, 0);
		set_fs(old_fs);
	}

	if (fwlimit)
		release_firmware(fwlimit);

	return ret;

exit:
	if (file_exist) {
		filp_close(file, 0);
		set_fs(old_fs);
	}

	if (fwlimit)
		release_firmware(fwlimit);

	return ret;
}
