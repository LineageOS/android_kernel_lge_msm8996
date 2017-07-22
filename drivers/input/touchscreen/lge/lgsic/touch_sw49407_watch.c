/* touch_sw49407_watch.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: hoyeon.jang@lge.com
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
#define TS_MODULE "[watch]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/syscalls.h>
#include <linux/file.h>
/*
 *  Include to touch core Header File
 */
#include <touch_hwif.h>
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_sw49407.h"
#include "touch_sw49407_watch.h"

static int ext_watch_rtc_start(struct device *dev, u8 start)
{
	u32 rtc_ctrl = EXT_WATCH_RTC_STOP;
	int ret = 0;

	if (start == EXT_WATCH_RTC_START)
		rtc_ctrl = EXT_WATCH_RTC_START;

	ret = sw49407_reg_write(dev, EXT_WATCH_RTC_RUN,
		(u8 *)&rtc_ctrl, sizeof(u32));
	if (ret)
		goto error;

	return ret;

error:
	TOUCH_E("Fail %d\n", ret);

	return -EIO;
}

static int ext_watch_get_mode(struct device *dev, char *buf, int *len)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_ctrl_cfg mode_cfg;
	struct ext_watch_lut_cfg lut_cfg;
	struct ext_watch_lut_bits *lut = NULL;
	u32 wdata;
	u8 *ptr = NULL;
	u16 idx = 0;
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	TOUCH_I("%s start\n", __func__);

	memset(&mode_cfg, 0x0, sizeof(struct ext_watch_ctrl_cfg));
	memset(&lut_cfg, 0x0, sizeof(struct ext_watch_lut_cfg));

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].tx.addr = EXT_WATCH_DCST_OFFSET;
	wdata = EXT_WATCH_CTRL_DCST_OFT;
	ts->xfer->data[0].tx.buf = (u8 *)&wdata;
	ts->xfer->data[0].tx.size = sizeof(u32);
	ts->xfer->data[0].rx.size = 0;

	ts->xfer->data[1].rx.addr = EXT_WATCH_DCST_ADDR;
	ts->xfer->data[1].rx.buf = (u8 *)&mode_cfg;
	ts->xfer->data[1].rx.size = sizeof(struct ext_watch_ctrl_cfg);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].tx.addr = EXT_WATCH_DCST_OFFSET;
	wdata = EXT_WATCH_LUT_DCST_OFT;
	ts->xfer->data[0].tx.buf = (u8 *)&wdata;
	ts->xfer->data[0].tx.size = sizeof(u32);
	ts->xfer->data[0].rx.size = 0;

	ts->xfer->data[1].rx.addr = EXT_WATCH_DCST_ADDR;
	ts->xfer->data[1].rx.buf = (u8 *)&lut_cfg;
	ts->xfer->data[1].rx.size = sizeof(struct ext_watch_lut_cfg);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	ptr = (u8 *)(&mode_cfg);
	loglen = snprintf(log, 256,
		"watch_ctrl %02X %02X (Mode:%d, Alpha:%X)\n",
		ptr[0], ptr[1],
		mode_cfg.u2_u3_disp_on, mode_cfg.alpha |
		(mode_cfg.alpha_fg << 8));
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256,
			"Watch area x[%d , %d] y[%d , %d]\n",
			mode_cfg.watch_x_start_7to0 |
				(mode_cfg.watch_x_start_11to8 << 8),
			mode_cfg.watch_x_end_3to0   |
				(mode_cfg.watch_x_end_11to4 << 4),
			mode_cfg.watch_y_start_7to0 |
				(mode_cfg.watch_y_start_11to8 << 8),
			mode_cfg.watch_y_end_3to0   |
				(mode_cfg.watch_y_end_11to4 << 4));
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256, "Blink area x[%d , %d]\n",
				mode_cfg.blink_x_start_7to0 |
					(mode_cfg.blink_x_start_11to8 << 8),
				mode_cfg.blink_x_end_3to0   |
					(mode_cfg.blink_x_end_11to4 << 4));
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256 - loglen, "LUT[%d] ", EXT_WATCH_LUT_MAX);

	for (idx = 0; idx < EXT_WATCH_LUT_MAX; idx++) {
		lut = &lut_cfg.lut[idx];
		loglen += snprintf(log + loglen, 256 - loglen,
					"%d:%02X%02X%02X ", idx + 1,
					lut->b, lut->g, lut->r);
	}

	loglen += snprintf(log + loglen, 256 - loglen, "\n");
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	TOUCH_I("%s end\n", __func__);

	return ret;

error:
	TOUCH_I("%s failed\n", __func__);

	return -EIO;
}

static int ext_watch_get_position(struct device *dev, char *buf, int *len)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_position_cfg position = {0};
	struct ext_watch_status_cfg status_cfg = {0};
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].rx.addr = d->reg_info.r_tc_sts_spi_addr +
							EXT_WATCH_POSITION_R;
	ts->xfer->data[0].rx.buf = (u8 *)(&position);
	ts->xfer->data[0].rx.size = sizeof(u32) * 3;

	ts->xfer->data[1].rx.addr = d->reg_info.r_tc_sts_spi_addr +
							EXT_WATCH_STATUS;
	ts->xfer->data[1].rx.buf = (u8 *)&status_cfg;
	ts->xfer->data[1].rx.size = sizeof(u32);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	loglen = snprintf(log, 256,"Get Offset[%X] Position [%d %d %d %d %d]\n",
		d->reg_info.r_aod_spi_addr + EXT_WATCH_POSITION_R,
					position.h10x_pos, position.h1x_pos,
		position.clx_pos, position.m10x_pos, position.m1x_pos);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	position.zero_disp = status_cfg.zero_en;
	position.h24_en = status_cfg.en_24;
	position.clock_disp_mode = status_cfg.disp_mode;
	position.midnight_hour_zero_en = status_cfg.midnight_hour_zero_en;
	position.bhprd = status_cfg.bhprd;

	loglen = snprintf(log, 256,
		"Get Offset[%X] Zero Display[%d], 24H Mode[%d], Clock Mode[%d]\n",
		d->reg_info.r_aod_spi_addr + EXT_WATCH_STATUS,
        position.zero_disp, position.h24_en,
		position.clock_disp_mode);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256, "Get Midnight Mode[%d], Blink period[%d]\n",
		position.midnight_hour_zero_en, position.bhprd);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256, "Get Watch Step[%d], Watch Enable[%d]\n",
		status_cfg.step, status_cfg.en);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	return ret;

error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

int ext_watch_get_current_time(struct device *dev, char *buf, int *len)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_status_cfg status_cfg = {0};
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	TOUCH_TRACE();

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].rx.addr = d->reg_info.r_tc_sts_spi_addr +
							EXT_WATCH_STATUS;
	ts->xfer->data[0].rx.buf = (u8 *)&status_cfg;
	ts->xfer->data[0].rx.size = sizeof(u32);

	ts->xfer->data[1].rx.addr = EXT_WATCH_RTC_CTST;
	ts->xfer->data[1].rx.buf = (u8 *)&d->watch.ext_wdata.time.rtc_ctst;
	ts->xfer->data[1].rx.size = sizeof(u32);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret < 0) {
		TOUCH_E("%s Fail\n", __func__);
		return ret;
	}

	loglen = snprintf(log, 256,
		"%s : Display[%02d:%02d:%02d], RTC[%02d:%02d:%02d]\n", __func__,
		status_cfg.cur_hour, status_cfg.cur_min, status_cfg.cur_sec,
		d->watch.ext_wdata.time.rtc_ctst.hour,
		d->watch.ext_wdata.time.rtc_ctst.min,
		d->watch.ext_wdata.time.rtc_ctst.sec);

	if (buf) {
		memcpy(&buf[*len], log, loglen);
		*len += loglen;
	}
	TOUCH_I("%s", log);

	return ret;
}

static int ext_watch_set_mode(struct device *dev, char log)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_ctrl_cfg *mode_cfg = &d->watch.ext_wdata.mode;
	struct ext_watch_lut_cfg *lut_cfg = &d->watch.ext_wdata.lut;
	u32 wdata;
	int ret = 0;

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].tx.addr = EXT_WATCH_DCST_OFFSET;
	wdata = EXT_WATCH_CTRL_DCST_OFT;
	ts->xfer->data[0].tx.buf = (u8 *)&wdata;
	ts->xfer->data[0].tx.size = sizeof(u32);
	ts->xfer->data[0].rx.size = 0;

	ts->xfer->data[1].tx.addr = EXT_WATCH_DCST_ADDR;
	ts->xfer->data[1].tx.buf = (u8 *)mode_cfg;
	ts->xfer->data[1].tx.size = sizeof(struct ext_watch_ctrl_cfg);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret < 0)
		TOUCH_E("failed %d\n", ret);

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].tx.addr = EXT_WATCH_DCST_OFFSET;
	wdata = EXT_WATCH_LUT_DCST_OFT;
	ts->xfer->data[0].tx.buf = (u8 *)&wdata;
	ts->xfer->data[0].tx.size = sizeof(u32);
	ts->xfer->data[0].rx.size = 0;

	ts->xfer->data[1].tx.addr = EXT_WATCH_DCST_ADDR;
	ts->xfer->data[1].tx.buf = (u8 *)lut_cfg;
	ts->xfer->data[1].tx.size = sizeof(struct ext_watch_lut_cfg);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret < 0) {
		TOUCH_E("failed %d\n", ret);
	} else if (log) {
		TOUCH_I("\t%s : X[%d , %d] Y[%d , %d] LUT[%02X%02X%02X]\n",
			__func__,
			mode_cfg->watch_x_start_7to0 |
				(mode_cfg->watch_x_start_11to8 << 8),
			mode_cfg->watch_x_end_3to0   |
				(mode_cfg->watch_x_end_11to4 << 4),
			mode_cfg->watch_y_start_7to0 |
				(mode_cfg->watch_y_start_11to8 << 8),
			mode_cfg->watch_y_end_3to0   |
				(mode_cfg->watch_y_end_11to4 << 4),
			lut_cfg->lut[6].b, lut_cfg->lut[6].g, lut_cfg->lut[6].r);
	}
	return ret;
}

static int ext_watch_set_current_time(struct device *dev)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	u32 rtc_ctrl = 0;
	u16 rtc_count = 305;		/* 30.5 us */
	int ret = 0;

	d->watch.ext_wdata.time.rtc_ecnt = 32764;
	d->watch.ext_wdata.time.rtc_sctcnt =
		(int)((d->watch.ext_wdata.time.rtc_sctcnt * rtc_count) / 10);
	rtc_ctrl = d->watch.ext_wdata.time.rtc_ecnt & 0xFFFF;

	ret = ext_watch_rtc_start(dev, EXT_WATCH_RTC_STOP);
	if (ret)
		goto error;

	sw49407_xfer_msg_ready(dev, 3);

	ts->xfer->data[0].tx.addr = EXT_WATCH_RTC_SCT;
	ts->xfer->data[0].tx.buf = (u8 *)&d->watch.ext_wdata.time.rtc_sct;
	ts->xfer->data[0].tx.size = sizeof(u32);

	ts->xfer->data[1].tx.addr = EXT_WATCH_RTC_SCTCNT;
	ts->xfer->data[1].tx.buf = (u8 *)&d->watch.ext_wdata.time.rtc_sctcnt;
	ts->xfer->data[1].tx.size = sizeof(u32);

	ts->xfer->data[2].tx.addr = EXT_WATCH_RTC_ECNT;
	ts->xfer->data[2].tx.buf = (u8 *)&rtc_ctrl;
	ts->xfer->data[2].tx.size = sizeof(u32);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	ret = ext_watch_rtc_start(dev, EXT_WATCH_RTC_START);
	if (ret)
		goto error;

	TOUCH_I("%s : %02d:%02d:%02d CLK[%d Hz]\n", __func__,
		d->watch.ext_wdata.time.rtc_sct.hour,
		d->watch.ext_wdata.time.rtc_sct.min,
		d->watch.ext_wdata.time.rtc_sct.sec,
		d->watch.ext_wdata.time.rtc_ecnt);

	atomic_set(&d->watch.state.rtc_status, RTC_RUN);

	return ret;

error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

static int ext_watch_set_position(struct device *dev, char log)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	u8 *ptr = (u8 *)(&d->watch.ext_wdata.position);
	struct ext_watch_position_cfg *cfg = &d->watch.ext_wdata.position;
	int ret = 0;

	sw49407_xfer_msg_ready(dev, 5);

	ts->xfer->data[0].tx.addr = EXT_WATCH_POSITION;
	ts->xfer->data[0].tx.buf = ptr;
	ts->xfer->data[0].tx.size = sizeof(u32);

	ts->xfer->data[1].tx.addr = EXT_WATCH_POSITION + 1;
	ts->xfer->data[1].tx.buf = &ptr[4];
	ts->xfer->data[1].tx.size = sizeof(u32);

	ts->xfer->data[2].tx.addr = EXT_WATCH_POSITION + 2;
	ts->xfer->data[2].tx.buf = &ptr[8];
	ts->xfer->data[2].tx.size = sizeof(u32);

	ts->xfer->data[3].tx.addr = EXT_WATCH_POSITION + 3;
	ts->xfer->data[3].tx.buf = &ptr[12];
	ts->xfer->data[3].tx.size = sizeof(u32);

	ts->xfer->data[4].tx.addr = EXT_WATCH_POSITION + 4;
	ts->xfer->data[4].tx.buf = &ptr[16];
	ts->xfer->data[4].tx.size = sizeof(u32);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret) {
		goto error;
	} else if (log) {
		TOUCH_I("\t%s : position [%d %d %d %d %d]\n", __func__,
			cfg->h10x_pos, cfg->h1x_pos, cfg->clx_pos,
			cfg->m10x_pos, cfg->m1x_pos);
	}
	return ret;

error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

static int ext_watch_display_onoff(struct device *dev, char log)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	int ret = 0;

	if ((atomic_read(&d->watch.state.rtc_status) != RTC_RUN) &&
			d->watch.ext_wdata.time.disp_waton) {
		d->watch.ext_wdata.time.rtc_sct.hour =
					d->watch.ext_wdata.time.rtc_ctst.hour;
		d->watch.ext_wdata.time.rtc_sct.min =
					d->watch.ext_wdata.time.rtc_ctst.min;
		d->watch.ext_wdata.time.rtc_sct.sec =
					d->watch.ext_wdata.time.rtc_ctst.sec;
		d->watch.ext_wdata.time.rtc_sctcnt = 0;
		ret = ext_watch_set_current_time(dev);
	}

	ret = sw49407_reg_write(dev, EXT_WATCH_DISPLAY_ON,
			&d->watch.ext_wdata.time.disp_waton, sizeof(u32));
	if (ret)
		goto error;

	if (log)
		TOUCH_I("%s(%X) : %s\n", __func__, EXT_WATCH_DISPLAY_ON,
			d->watch.ext_wdata.time.disp_waton ? "On" : "Off");

	return ret;

error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

void sw49407_watch_display_off(struct device *dev)
{
	u32 disp = 0;

	sw49407_reg_write(dev, EXT_WATCH_DISPLAY_ON, &disp, sizeof(u32));

	TOUCH_I("%s\n", __func__);
}

void sw49407_font_download(struct work_struct *font_download_work)
{
	struct sw49407_data *d = container_of(to_delayed_work(font_download_work),
				struct sw49407_data, font_download_work);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_font_header *fonthdr = NULL;
	int ret = 0;
	int remained = 0;
	u32 offset = 0;
	int rwsize = 0;
	int idx = 0;
	int value = 1;
	u32 status = 0;
	int retry = 0;

	if (atomic_read(&d->watch.state.font_status) == FONT_EMPTY) {
		TOUCH_I("%s : data not downloaded\n", __func__);
		return;
	}

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		atomic_set(&d->watch.state.font_status, FONT_DOWNLOADING);
		TOUCH_I("%s : IC deep sleep\n", __func__);
		return;
	}

	mutex_lock(&ts->lock);
	TOUCH_I("%s start\n", __func__);

	if (d->lcd_mode == LCD_MODE_U2) {
		d->watch.ext_wdata.time.disp_waton = 0;
		ret = ext_watch_display_onoff(d->dev, 1);
		TOUCH_I("U2 Mode : Watch Display off\n");
	}

	ret = sw49407_reg_write(ts->dev, EXT_WATCH_FONT_DN_FLAG,
		(u8*)&value, sizeof(u32));
	TOUCH_I("Watch Font download flag on\n");
	if (ret)
		goto error;

	fonthdr = (struct ext_watch_font_header *)d->watch.ext_wdata.font_data;
	TOUCH_I("\tMagic[%08X], id[%d], size[%d]\n",
		fonthdr->magic_code, fonthdr->font_id, fonthdr->size);
	TOUCH_I("\twidth_num[%d], width_colon[%d], height[%d]\n",
		fonthdr->width_num, fonthdr->width_colon, fonthdr->height);

	while(1){
		idx = 0;
		offset = 0;
		remained = d->watch.font_written_size;

		while(1) {
			if (remained > MAX_RW_SIZE) {
				rwsize = MAX_RW_SIZE;
				remained -= MAX_RW_SIZE;
			} else {
				rwsize = remained;
				remained = 0;
			}

			ret = sw49407_reg_write(ts->dev, EXT_WATCH_FONT_OFFSET,
				(u8*)&offset, sizeof(u32));
			if (ret)
				goto error;

			ret = sw49407_reg_write(ts->dev, EXT_WATCH_FONT_ADDR,
				(u8 *)&d->watch.ext_wdata.font_data[idx], rwsize);
			if (ret)
				goto error;

			idx += rwsize;
			offset = (idx / 4);

			if (remained == 0)
				break;
		}

		ret = sw49407_reg_write(ts->dev, EXT_WATCH_FONT_CRC_TEST,
			(u8 *)&value, sizeof(u32));
		if (ret)
			goto error;

		touch_msleep(10);

		ret = sw49407_reg_read(ts->dev, tc_status, &status, sizeof(u32));
		if (ret)
			goto error;

		if ((status >> 8) & 0x1) {
			TOUCH_I("%s : retry = %d crc ok\n", __func__, retry);
			break;
		} else {
			TOUCH_I("%s : retry = %d crc fail [tc_status %08X]\n", __func__, retry, status);
			if(++retry == 3)
				goto error;
		}
	}
	atomic_set(&d->watch.state.font_status, FONT_READY);

	value = 0;
	ret = sw49407_reg_write(ts->dev, EXT_WATCH_FONT_DN_FLAG,
		(u8*)&value, sizeof(u32));
	TOUCH_I("Watch Font download flag off\n");
	if (ret)
		goto error;

	TOUCH_I("%s %d bytes done\n", __func__, d->watch.font_written_size);
	mutex_unlock(&ts->lock);
	return;

error:
	value = 0;
	ret = sw49407_reg_write(ts->dev, EXT_WATCH_FONT_DN_FLAG,
		(u8*)&value, sizeof(u32));
	TOUCH_I(" Watch Font download flag off\n");

	atomic_set(&d->watch.state.font_status, FONT_ERROR);
	TOUCH_I("%s fail %d\n", __func__, ret);
	mutex_unlock(&ts->lock);
	return;
}

int sw49407_check_font_status(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw49407_data *d = to_sw49407_data(dev);
	int ret = 0;
	int status = 1;

	if (touch_boot_mode_check(dev) >= MINIOS_MFTS_FOLDER)
		return -EBUSY;

	switch (atomic_read(&d->watch.state.font_status)) {
		case FONT_EMPTY :
			return -EBUSY;

		case FONT_DOWNLOADING :
			mod_delayed_work(ts->wq, &d->font_download_work, 0);
			return 0;
	}

	sw49407_reg_read(dev, tc_status, &status, sizeof(int));
	if ((status >> 8) & 0x1) {
		TOUCH_I("%s : crc ok\n", __func__);
		return 0;
	} else {
		TOUCH_I("%s : crc fail [tc_status %08X]\n", __func__, status);
		mod_delayed_work(ts->wq, &d->font_download_work, 0);
	}
	return ret;
}

static int ext_watch_font_dump(struct device *dev, char *font_dump)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	int ret = 0;
	u32 value = 0;
	int remained = 0;
	u32 offset = 0;
	int rwsize = 0;
	int idx = 0;

	mutex_lock(&ts->lock);
	TOUCH_I("%s start\n", __func__);

    value = 1;
    ret = sw49407_reg_write(dev, EXT_WATCH_FONT_DN_FLAG,
        (u8*)&value, sizeof(u32));
    if (ret)
        goto error;

	remained = d->watch.font_written_size;

	while(1) {
		if (remained > MAX_RW_SIZE) {
			rwsize = MAX_RW_SIZE;
			remained -= MAX_RW_SIZE;
		} else {
			rwsize = remained;
			remained = 0;
		}

		ret = sw49407_reg_write(dev, EXT_WATCH_FONT_OFFSET,
			(u8*)&offset, sizeof(u32));
		if (ret)
			goto error;

		ret = sw49407_reg_read(dev, EXT_WATCH_FONT_ADDR,
			(u8 *)&font_dump[idx], rwsize);
		if (ret)
			goto error;

		idx += rwsize;
		offset = (idx / 4);

		if (remained == 0)
			break;
	}

	value = 0;
	ret = sw49407_reg_write(dev, EXT_WATCH_FONT_DN_FLAG,
	(u8*)&value, sizeof(u32));

	if (ret)
		goto error;
	TOUCH_I("%s %d bytes done\n", __func__, d->watch.font_written_size);
	mutex_unlock(&ts->lock);
	return ret;

error:
	value = 0;
	sw49407_reg_write(dev, EXT_WATCH_FONT_DN_FLAG,
	(u8*)&value, sizeof(u32));
	TOUCH_I("%s fail %d\n", __func__, ret);
	mutex_unlock(&ts->lock);
	return -EIO;
}

static int ext_watch_set_cfg(struct device *dev, char log)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	d->watch.ext_wdata.mode.alpha_fg = 1; /* bypass foreground */

	ret = ext_watch_set_mode(dev, log);
	if (ret)
		goto error;

	ret = ext_watch_set_position(dev, log);
	if (ret)
		goto error;

	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -ECANCELED;

}

static int ext_watch_get_dic_st(struct device *dev, char *buf, int *len)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	u32 dic_status = 0;
	u32 watch_status = 0;
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	TOUCH_TRACE();

	sw49407_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].rx.addr = spr_subdisp_st;
	ts->xfer->data[0].rx.buf = (u8 *)&dic_status;
	ts->xfer->data[0].rx.size = sizeof(u32);

	ts->xfer->data[1].rx.addr = EXT_WATCH_DISPLAY_STATUS;
	ts->xfer->data[1].rx.buf = (u8 *)&watch_status;
	ts->xfer->data[1].rx.size = sizeof(u32);

	ret = sw49407_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	if (dic_status == 2) {
		loglen = snprintf(log, 256, "Display Mode U2 , Watch Display %s\n",
			(watch_status & 1) ? "ON" : "OFF");
	} else {
		loglen = snprintf(log, 256, "Display Mode U%d\n", dic_status);
	}
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

static ssize_t store_extwatch_fontonoff
	(struct device *dev, const char *buf, size_t count)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	u8 value;
	u8 zero = '0';
	int ret = 0;

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	mutex_lock(&ts->lock);

	memcpy((char *)&value, buf, sizeof(u8));

	if (value == POWER_OFF || value == zero)
		value = 0x00;
	else
		value = 0x01;

	d->watch.ext_wdata.time.disp_waton = value;

	if ((d->lcd_mode == LCD_MODE_U2_UNBLANK && value)
		|| (d->lcd_mode == LCD_MODE_U0 && value)
		|| (atomic_read(&d->global_reset) == GLOBAL_RESET_START)) {
		TOUCH_I("%s : Ignore HW Clock On in %s\n", __func__,
			atomic_read(&d->global_reset) == GLOBAL_RESET_START
			? "GLOBAL Reset" : d->lcd_mode == LCD_MODE_U0
			? "U0" : "U2 Unblank");
		goto Exit;
	}

	TOUCH_I("%s %d\n", __func__, value);

	ret = ext_watch_set_cfg(dev, 1);
	if (ret)
		TOUCH_I("%s fail %d\n", __func__, ret);

	ret = ext_watch_display_onoff(dev, 1);
	if (ret)
		TOUCH_I("%s fail %d\n", __func__, ret);

Exit :
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t store_block_cfg(struct device *dev,
		const char *buf, size_t count)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	u8 value;
	u8 zero = '0';

	memcpy(&value, buf, sizeof(u8));

	if (value == UNBLOCKED || value == zero)
		value = 0x00;
	else
		value = 0x01;

	atomic_set(&d->block_watch_cfg, value);

	TOUCH_I("%s : %s\n", __func__,
			value ? "BLOCKED" : "UNBLOCKED");

	return count;
}
static ssize_t show_block_cfg(struct device *dev,
	char *buf)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "block_watch_cfg : %d\n",
		atomic_read(&d->block_watch_cfg));

	return ret;
}

static ssize_t store_extwatch_rtc_onoff(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u8 value = 0;
	u8 zero = '0';
	int ret = 0;

	memcpy(&value, buf, sizeof(u8));

	if (value == 0 || value == zero)
		value = 0x00;
	else
		value = 0x01;

	mutex_lock(&ts->lock);

	ret = ext_watch_rtc_start(dev, value);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	TOUCH_I("%s : %s\n", __func__, value ? "START" : "STOP");

	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_extwatch_fontdata_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontDataQuery query;

	query.Font_supported = SUPPORT;
	query.max_font_x_size = 66;
	query.max_font_y_size = 160;
	query.max_cln_x_size = 24;
	query.max_cln_y_size = 160;

	memcpy(buf, (char *)&query, sizeof(struct ExtWatchFontDataQuery));
	return sizeof(struct ExtWatchFontDataQuery);
}

static ssize_t show_extwatch_fontposition_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontPositionQuery query;

	query.vertical_position_supported = SUPPORT;
	query.horizontal_position_supported = SUPPORT;

	memcpy(buf, (char *)&query, sizeof(struct ExtWatchFontPositionQuery));
	return sizeof(struct ExtWatchFontPositionQuery);
}

static ssize_t show_extwatch_fonttime_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontTimeQuery query;

	query.h24_supported = SUPPORT;
	query.AmPm_supported = NOT_SUPPORT;

	TOUCH_I("%s %2d %2d\n", __func__,
		query.h24_supported, query.AmPm_supported);

	memcpy(buf, (char *)&query, sizeof(struct ExtWatchFontTimeQuery));
	return sizeof(struct ExtWatchFontTimeQuery);
}

static ssize_t show_extwatch_fontcolor_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontColorQuery query;

	query.max_num = EXT_WATCH_LUT_MAX;
	query.LUT_supported = SUPPORT;
	query.alpha_supported = SUPPORT;
	query.gradation_supported = SUPPORT;

	memcpy(buf, (char *)&query, sizeof(struct ExtWatchFontColorQuery));
	return sizeof(struct ExtWatchFontColorQuery);
}

static ssize_t show_extwatch_fonteffect_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontEffectQuery query;

	query.zero_supported = SUPPORT;
	query.blink_type = 2;

	memcpy(buf, (char *)&query, sizeof(struct ExtWatchFontEffectQuery));
	return sizeof(struct ExtWatchFontEffectQuery);
}

static ssize_t show_extwatch_current_time
	(struct device *dev, char *buf)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	int len = 0;

	TOUCH_TRACE();

	if (d == NULL)
		return 0;

	ext_watch_get_current_time(dev, buf, &len);

	return len;
}

static ssize_t store_extwatch_fonteffect_config(struct device *dev,
		const char *buf, size_t count)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct ExtWatchFontEffectConfig cfg;
	char period[8] = {0};

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontEffectConfig));

	d->watch.ext_wdata.position.h24_en = cfg.h24_en;
	d->watch.ext_wdata.position.zero_disp = cfg.zero_disp;
	d->watch.ext_wdata.position.clock_disp_mode = cfg.clock_disp_type;
	d->watch.ext_wdata.position.midnight_hour_zero_en
		= cfg.midnight_hour_zero_en;
	d->watch.ext_wdata.position.bhprd = cfg.blink.blink_type;
	d->watch.ext_wdata.mode.blink_x_start_7to0 = cfg.blink.bstartx & 0xFF;
	d->watch.ext_wdata.mode.blink_x_start_11to8 = (cfg.blink.bstartx >> 8) & 0xF;
	d->watch.ext_wdata.mode.blink_x_end_3to0 = cfg.blink.bendx & 0xF;
	d->watch.ext_wdata.mode.blink_x_end_11to4 = (cfg.blink.bendx >> 4) & 0xFF;

	switch (cfg.blink.blink_type) {
	default:
	case 0:
		snprintf(period, 8, "Off");
		break;
	case 1:
		snprintf(period, 8, "0.125s");
		break;
	case 2:
		snprintf(period, 8, "0.25s");
		break;
	case 3:
		snprintf(period, 8, "0.5s");
		break;
	case 4:
		snprintf(period, 8, "1s");
		break;
	case 5:
		snprintf(period, 8, "2s");
		break;
	case 6:
		snprintf(period, 8, "4s");
		break;
	case 7:
		snprintf(period, 8, "8s");
		break;
	}

	TOUCH_I("%s : 24h mode %s, Zero Dispaly %s,%s Type %s mode "
		"Blink area [%d , %d] Period %s \n", __func__,
		cfg.h24_en ? "Enable" : "Disable",
		cfg.zero_disp ? "Enable" : "Disable",
		cfg.clock_disp_type ? "MM:SS" : "HH:MM",
		cfg.midnight_hour_zero_en ? "00:00" : "12:00",
		cfg.blink.bstartx, cfg.blink.bendx, period);

	return count;
}
static ssize_t store_extwatch_fontproperty_config
	(struct device *dev, const char *buf, size_t count)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct ExtWatchFontPropertyConfig cfg;
	int idx = 0;
	char log[256] = {0};
	int len = 0;

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontPropertyConfig));

	len += snprintf(log + len, 256 - len, "%s : LUT[%d] ",
		__func__, (int)cfg.max_num);

	for (idx = 0; idx < (int)cfg.max_num; idx++) {
		d->watch.ext_wdata.lut.lut[idx].b = cfg.LUT[idx].RGB_blue;
		d->watch.ext_wdata.lut.lut[idx].g = cfg.LUT[idx].RGB_green;
		d->watch.ext_wdata.lut.lut[idx].r = cfg.LUT[idx].RGB_red;

		len += snprintf(log + len, 256 - len, "%d:%02X%02X%02X ", idx + 1,
			cfg.LUT[idx].RGB_blue, cfg.LUT[idx].RGB_green,
			cfg.LUT[idx].RGB_red);
	}

	TOUCH_I("%s\n", log);

	return count;
}

static ssize_t store_extwatch_fontposition_config(struct device *dev,
		const char *buf, size_t count)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct ExtWatchFontPostionConfig cfg;
	u32 pos_shift;

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontPostionConfig));

	if(cfg.watstartx%3!=0) {
		cfg.watstartx+=3-(cfg.watstartx%3);
		TOUCH_I("\t%s : watch start x changed[%d]\n",__func__, cfg.watstartx);
	}

	if(cfg.watendx%3!=0) {
		cfg.watendx+=3-(cfg.watendx%3);
		TOUCH_I("\t%s : watch end x changed[%d]\n",__func__, cfg.watendx);
	}

	if(cfg.watstartx > EXT_WATCH_MAX_STARTX) {
		pos_shift = cfg.watstartx - EXT_WATCH_MAX_STARTX;
		cfg.watstartx = EXT_WATCH_MAX_STARTX;
		cfg.h10x_pos += pos_shift;
		cfg.h1x_pos += pos_shift;
		cfg.clx_pos += pos_shift;
		cfg.m10x_pos += pos_shift;
		cfg.m1x_pos += pos_shift;
		TOUCH_I("\t%s : watch position shift[%d]\n",__func__, pos_shift);
	}

	d->watch.ext_wdata.mode.watch_x_start_7to0 = cfg.watstartx & 0xFF;
	d->watch.ext_wdata.mode.watch_x_start_11to8 = (cfg.watstartx >> 8) & 0xF;
	d->watch.ext_wdata.mode.watch_x_end_3to0 = cfg.watendx & 0xF;
	d->watch.ext_wdata.mode.watch_x_end_11to4 = (cfg.watendx >> 4) & 0xFF;
	d->watch.ext_wdata.mode.watch_y_start_7to0 = cfg.watstarty & 0xFF;
	d->watch.ext_wdata.mode.watch_y_start_11to8 = (cfg.watstarty >> 8) & 0xF;
	d->watch.ext_wdata.mode.watch_y_end_3to0 = cfg.watendy & 0xF;
	d->watch.ext_wdata.mode.watch_y_end_11to4 = (cfg.watendy >> 4) & 0xFF;
	d->watch.ext_wdata.position.h10x_pos = cfg.h10x_pos;
	d->watch.ext_wdata.position.h1x_pos = cfg.h1x_pos;
	d->watch.ext_wdata.position.m10x_pos = cfg.m10x_pos;
	d->watch.ext_wdata.position.m1x_pos = cfg.m1x_pos;
	d->watch.ext_wdata.position.clx_pos = cfg.clx_pos;

	TOUCH_I("%s : Watch area X[%d , %d] Y[%d , %d] position [%d %d %d %d %d]\n",
		__func__, cfg.watstartx, cfg.watendx, cfg.watstarty, cfg.watendy, cfg.h10x_pos,
		cfg.h1x_pos, cfg.clx_pos, cfg.m10x_pos, cfg.m1x_pos);

	return count;
}

static ssize_t store_extwatch_timesync_config(struct device *dev,
		const char *buf, size_t count)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	struct ExtWatchTimeSyncConfig cfg;
	int ret = 0;

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	mutex_lock(&ts->lock);

	TOUCH_I("%s\n", __func__);

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchTimeSyncConfig));

	d->watch.ext_wdata.time.rtc_sct.hour = cfg.rtc_cwhour;
	d->watch.ext_wdata.time.rtc_sct.min = cfg.rtc_cwmin;
	d->watch.ext_wdata.time.rtc_sct.sec = cfg.rtc_cwsec;
	d->watch.ext_wdata.time.rtc_sctcnt = cfg.rtc_cwmilli;

	ret = ext_watch_set_current_time(dev);
	if (ret)
		TOUCH_E("%s fail\n", __func__);

	ret = ext_watch_get_current_time(dev, NULL, NULL);
	if (ret)
		TOUCH_E("%s fail\n", __func__);

	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_extwatch_setting(struct device *dev, char *buf)
{
	int len = 0;

	TOUCH_TRACE();

	ext_watch_get_mode(dev, buf, &len);
	ext_watch_get_position(dev, buf, &len);
	ext_watch_get_current_time(dev, buf, &len);
	ext_watch_get_dic_st(dev, buf, &len);

	return len;
}

static WATCH_ATTR(rtc_onoff, NULL, store_extwatch_rtc_onoff);
static WATCH_ATTR(block_cfg, show_block_cfg, store_block_cfg);
static WATCH_ATTR(config_fontonoff, NULL, store_extwatch_fontonoff);
static WATCH_ATTR(config_fonteffect, NULL,
				store_extwatch_fonteffect_config);
static WATCH_ATTR(config_fontproperty, NULL,
				store_extwatch_fontproperty_config);
static WATCH_ATTR(config_fontposition, NULL,
				store_extwatch_fontposition_config);
static WATCH_ATTR(config_timesync,
				NULL, store_extwatch_timesync_config);
static WATCH_ATTR(current_time, show_extwatch_current_time, NULL);
static WATCH_ATTR(query_fontdata,
				show_extwatch_fontdata_query, NULL);
static WATCH_ATTR(query_fontposition,
				show_extwatch_fontposition_query, NULL);
static WATCH_ATTR(query_timesync,
				show_extwatch_fonttime_query, NULL);
static WATCH_ATTR(query_fontcolor,
				show_extwatch_fontcolor_query, NULL);
static WATCH_ATTR(query_fonteffect,
				show_extwatch_fonteffect_query, NULL);
static WATCH_ATTR(get_cfg, show_extwatch_setting, NULL);

static struct attribute *watch_attribute_list[] = {
	&watch_attr_rtc_onoff.attr,
	&watch_attr_block_cfg.attr,
	&watch_attr_config_fontonoff.attr,
	&watch_attr_config_fonteffect.attr,
	&watch_attr_config_fontproperty.attr,
	&watch_attr_config_fontposition.attr,
	&watch_attr_config_timesync.attr,
	&watch_attr_current_time.attr,
	&watch_attr_query_fontdata.attr,
	&watch_attr_query_fontposition.attr,
	&watch_attr_query_timesync.attr,
	&watch_attr_query_fontcolor.attr,
	&watch_attr_query_fonteffect.attr,
	&watch_attr_get_cfg.attr,
	NULL,
};

static const struct attribute_group watch_attribute_group = {
	.attrs = watch_attribute_list,
};

static ssize_t watch_attr_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct sw49407_data *d = container_of(kobj, struct sw49407_data, kobj);
	struct touch_attribute *priv =
			container_of(attr, struct touch_attribute, attr);
	ssize_t ret = 0;

	if (priv->show)
		ret = priv->show(d->dev, buf);

	return ret;
}

static ssize_t watch_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct sw49407_data *d = container_of(kobj, struct sw49407_data, kobj);
	struct touch_attribute *priv =
			container_of(attr, struct touch_attribute, attr);
	ssize_t ret = 0;

	if (priv->store)
		ret = priv->store(d->dev, buf, count);

	return ret;
}

static const struct sysfs_ops watch_sysfs_ops = {
	.show	= watch_attr_show,
	.store	= watch_attr_store,
};

static struct kobj_type watch_kobj_type = {
	.sysfs_ops	= &watch_sysfs_ops,
};

static ssize_t watch_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct sw49407_data *d = container_of(kobj, struct sw49407_data, kobj);
	struct touch_core_data *ts = to_touch_core(d->dev);
	ssize_t retval = -EFAULT;

	if (off == 0) {
		if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
			TOUCH_I("%s : IC deep sleep. cannot read\n", __func__);
		} else {
			while (1) {
				if (atomic_read(&d->watch.state.font_status) == FONT_DOWNLOADING) {
					TOUCH_I("%s waiting\n", __func__);
					touch_msleep(10);
				} else {
					break;
				}
			}
			ext_watch_font_dump(d->dev, d->watch.ext_wdata.font_data);
		}
	}

	if (off + count > MAX_FONT_SIZE) {
		TOUCH_I("%s size error offset[%d] size[%d]\n", __func__,
			(int)off, (int)count);
	} else {
		memcpy(buf, &d->watch.ext_wdata.font_data[off], count);
		retval = count;
	}

	return retval;
}

static ssize_t watch_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct sw49407_data *d = to_sw49407_data_from_kobj(kobj);
	struct touch_core_data *ts = to_touch_core(d->dev);
	ssize_t retval = -EFAULT;

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	if (buf == NULL) {
		TOUCH_E("%s buf is NULL\n", __func__);
		goto finish;
	}

	if (d->watch.ext_wdata.font_data == NULL) {
		TOUCH_E("%s font_data is NULL\n", __func__);
		goto finish;
	}

	if (off + count > MAX_FONT_SIZE)	{
		TOUCH_E("%s off + count[%d] is bigger than MAX_FONT_SIZE\n",
			__func__, (int)off+(int)count);
		goto finish;
	}

	d->watch.font_written_size = off + count;

	if (off + count > MAX_FONT_SIZE) {
		TOUCH_I("%s size error offset[%d] size[%d]\n", __func__,
				(int)off, (int)count);
		atomic_set(&d->watch.state.font_status, FONT_EMPTY);
	} else {
		atomic_set(&d->watch.state.font_status, FONT_DOWNLOADING);
		memcpy(&d->watch.ext_wdata.font_data[off], buf, count);
		retval = count;
		TOUCH_I("%s size offset[%d] size[%d]\n", __func__,
				(int)off, (int)count);
		mod_delayed_work(ts->wq, &d->font_download_work, 20);
	}
finish:
	return retval;
}

static int watch_fontdata_attr_init(struct sw49407_data *d)
{
	d->watch.font_written_size = 0;
	d->watch.ext_wdata.font_data = kzalloc(MAX_FONT_SIZE, GFP_KERNEL);
	if (d->watch.ext_wdata.font_data) {
		TOUCH_I("%s font_buffer(%d KB) malloc\n", __func__,
			MAX_FONT_SIZE/1024);
	} else {
		TOUCH_E("%s font_buffer(%d KB) malloc failed\n", __func__,
			MAX_FONT_SIZE/1024);
		return 1;
	}

	sysfs_bin_attr_init(&d->watch.fontdata_attr);
	d->watch.fontdata_attr.attr.name = "config_fontdata";
	d->watch.fontdata_attr.attr.mode = S_IWUSR | S_IRUSR;
	d->watch.fontdata_attr.read = watch_access_read;
	d->watch.fontdata_attr.write = watch_access_write;
	d->watch.fontdata_attr.size = MAX_FONT_SIZE;

	if (sysfs_create_bin_file(&d->kobj, &d->watch.fontdata_attr) < 0)
		TOUCH_E("Failed to create %s\n", d->watch.fontdata_attr.attr.name);
	return 0;
}

int sw49407_watch_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw49407_data *d = to_sw49407_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = kobject_init_and_add(&d->kobj, &watch_kobj_type,
			ts->input->dev.kobj.parent->parent,
			"%s", LGE_EXT_WATCH_NAME);

	ret = sysfs_create_group(&d->kobj, &watch_attribute_group);
	if (ret < 0) {
		TOUCH_E("failed to create sysfs\n");
		return ret;
	}

	watch_fontdata_attr_init(d);

	return ret;
}

int sw49407_watch_init(struct device *dev)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	int ret = 0;

	if (touch_boot_mode_check(dev) >= MINIOS_MFTS_FOLDER)
		return -EBUSY;

	if (atomic_read(&d->watch.state.font_status) == FONT_EMPTY)
		return -EBUSY;

	TOUCH_I("%s\n", __func__);

	ret = ext_watch_set_cfg(dev, 0);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	ret = ext_watch_display_onoff(dev, 1);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	return ret;
}

void sw49407_watch_remove(struct device *dev)
{
	struct sw49407_data *d = to_sw49407_data(dev);
	kfree(d->watch.ext_wdata.font_data);
}

