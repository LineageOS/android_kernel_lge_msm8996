/* touch_lg4945_watch.c
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

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_lg4945.h"
#include "touch_lg4945_watch.h"

#define SPI_READ_HEADER_SIZE (6)
#define SPI_WRITE_HEADER_SIZE (4)
#define SPI_MAX_CLOCK_SPEED (20 * 1000000)
/* 4'h 0~9: font '0'~'9', 4'hA : font ':' */
#define EXT_WATCH_FONT_SEL		0xD07A
/* 0~9:D800-DCFF, A(:):D800-D9DF */
#define EXT_WATCH_FONT_OFFSET_ADDR	0xD800

static int ext_watch_get_mode(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 *ptr = NULL;
	u16 offset = EXT_WATCH_CTRL;
	u16 idx = 0;
	u32 size = sizeof(u8);
	int ret = 0;

	TOUCH_I("start\n");

	if (ret)
		goto error;

	ptr = (u8 *)(&d->watch.ext_wdata.mode.watch_ctrl);
	offset = EXT_WATCH_CTRL;
	ret = lg4945_reg_read(dev,  offset++, &ptr[0], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[1], size);
	TOUCH_I("Get Offet[%X] watch_ctrl %02X %02X\n",
		EXT_WATCH_CTRL, ptr[0], ptr[1]);

	ptr = (u8 *)(&d->watch.ext_wdata.mode.watch_area);
	offset = EXT_WATCH_AREA;
	ret = lg4945_reg_read(dev,  offset++, &ptr[0], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[1], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[2], size);
	TOUCH_I("Get Offet[%X] watch_area %02X %02X %02X\n",
		EXT_WATCH_AREA, ptr[0], ptr[1], ptr[2]);

	ptr = (u8 *)(&d->watch.ext_wdata.mode.blink_area);
	offset = EXT_WATCH_BLINK_AREA;
	ret = lg4945_reg_read(dev,  offset++, &ptr[0], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[1], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[2], size);
	TOUCH_I("Get Offet[%X] blink_area %02X %02X %02X\n",
		EXT_WATCH_BLINK_AREA, ptr[0], ptr[1], ptr[2]);

	ptr = (u8 *)(&d->watch.ext_wdata.mode.grad);
	offset = EXT_WATCH_GRAD;
	ret = lg4945_reg_read(dev,  offset++, &ptr[0], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[1], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[2], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[3], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[4], size);
	ret = lg4945_reg_read(dev,  offset++, &ptr[5], size);
	TOUCH_I("Get Offet[%X] grad %02X %02X %02X %02X %02X %02X\n",
		EXT_WATCH_GRAD, ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);

	offset = EXT_WATCH_LUT;
	for (idx = 0; idx < EXT_WATCH_LUT_NUM; idx++) {
		ptr = (u8 *)(&d->watch.ext_wdata.mode.lut[idx]);
		ret = lg4945_reg_read(dev,  offset++, &ptr[0], size);
		ret = lg4945_reg_read(dev,  offset++, &ptr[1], size);
		ret = lg4945_reg_read(dev,  offset++, &ptr[2], size);
		TOUCH_I("Get Offet[%X] LUT[%d] : B[%02X] G[%02X] R[%02X]\n",
			offset - 3, idx, ptr[0], ptr[1], ptr[2]);
	}

	if (ret)
		goto error;
	TOUCH_I("end\n");
	return ret;

error:
	TOUCH_E("failed %d\n", ret);
	return -EIO;
}

static int ext_watch_get_position(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 *ptr = (u8 *)(&d->watch.ext_wdata.position);
	struct ext_watch_status_cfg status_cfg;
	int ret = 0;

	TOUCH_I("start\n");

	ret = lg4945_reg_read(dev, EXT_WATCH_POSITION_R, ptr,
				sizeof(u32) * 3);

	if (ret)
		goto error;
	TOUCH_I("Get Hour Position [%d][%d]\n",
		d->watch.ext_wdata.position.h10x_pos,
		d->watch.ext_wdata.position.h1x_pos);
	TOUCH_I("Get Min Position [%d][%d]\n",
		d->watch.ext_wdata.position.m10x_pos,
		d->watch.ext_wdata.position.m1x_pos);
	TOUCH_I("Get Colon Position [%d]\n",
			d->watch.ext_wdata.position.clx_pos);

	ret = lg4945_reg_read(dev, EXT_WATCH_SATATE, (u8 *)&status_cfg,
				sizeof(u32));
	if (ret)
		goto error;
	d->watch.ext_wdata.position.zero_disp = status_cfg.zero_en;
	d->watch.ext_wdata.position.h24_en = status_cfg.en_24;
	d->watch.ext_wdata.position.clock_disp_mode = status_cfg.disp_mode;
	d->watch.ext_wdata.position.midnight_hour_zero_en
		= status_cfg.midnight_hour_zero_en;
	d->watch.ext_wdata.position.bhprd = status_cfg.bhprd;

	TOUCH_I("Get Zero Display [%d]\n",
		d->watch.ext_wdata.position.zero_disp);
	TOUCH_I("Get 24H Mode [%d]\n",
		d->watch.ext_wdata.position.h24_en);
	TOUCH_I("Get Clock Mode [%d]\n",
		d->watch.ext_wdata.position.clock_disp_mode);
	TOUCH_I("Get Midnight Mode [%d]\n",
		d->watch.ext_wdata.position.midnight_hour_zero_en);
	TOUCH_I("Get Blink period [%d]\n", d->watch.ext_wdata.position.bhprd);
	TOUCH_I("Get Current Watch[%d]\n", status_cfg.step);
	TOUCH_I("Get Watch Enable[%d]\n", status_cfg.en);

	if (d->watch.ext_wdata.position.clock_disp_mode)
		TOUCH_I("Get Current Time[%02d][%02d]\n",
			status_cfg.cur_min, status_cfg.cur_sec);
	else
		TOUCH_I("Get Current Time[%02d][%02d]\n",
		status_cfg.cur_hour, status_cfg.cur_min);

	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

static int ext_watch_get_current_time(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	TOUCH_TRACE();
	ret = lg4945_reg_read(dev, EXT_WATCH_RTC_CTST,
		(u8 *)&d->watch.ext_wdata.time.rtc_ctst, sizeof(u32));

	if (ret < 0) {
		TOUCH_E("%s Fail\n", __func__);
		return ret;
	}

	TOUCH_I("%s : %02d:%02d:%02d\n", __func__,
		d->watch.ext_wdata.time.rtc_ctst.hour,
		d->watch.ext_wdata.time.rtc_ctst.min,
		d->watch.ext_wdata.time.rtc_ctst.sec);

	return ret;
}

static int ext_watch_set_mode(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 *ptr = NULL;
	u16 offset = EXT_WATCH_CTRL;
	u32 size = sizeof(u32);
	u32 pack[3] = {0, };
	int ret = 0;

	if (ret < 0)
		return ret;

	ptr = (u8 *)(&d->watch.ext_wdata.mode.watch_ctrl);
	offset = EXT_WATCH_CTRL;
	pack[0] = ptr[0];
	pack[1] = ptr[1];
	ret = lg4945_reg_write(dev,  offset, &pack[0], size * 2);


	ptr = (u8 *)(&d->watch.ext_wdata.mode.watch_area);
	offset = EXT_WATCH_AREA;
	pack[0] = ptr[0];
	pack[1] = ptr[1];
	pack[2] = ptr[2];
	ret = lg4945_reg_write(dev,  offset, &pack[0], size * 3);

	ptr = (u8 *)(&d->watch.ext_wdata.mode.blink_area);
	offset = EXT_WATCH_BLINK_AREA;
	pack[0] = ptr[0];
	pack[1] = ptr[1];
	pack[2] = ptr[2];
	ret = lg4945_reg_write(dev,  offset, &pack[0], size * 3);

	ptr = (u8 *)(&d->watch.ext_wdata.mode.grad);
	offset = EXT_WATCH_GRAD;
	ret = lg4945_reg_write(dev,  offset, &ptr[0], size * 6);

	offset = EXT_WATCH_LUT;
	ptr = (u8 *)(&d->watch.ext_wdata.mode.lut[0]);
	ret = lg4945_reg_write(dev,  offset, &ptr[0],
		size * 3 * EXT_WATCH_LUT_NUM);

	if (ret < 0)
		TOUCH_E("failed %d\n", ret);

	return ret;
}

static int ext_watch_set_current_time(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u32 rtc_ctrl = EXT_WATCH_RTC_STOP;
	u16 rtc_count = 305;		/* for time, 1 /rtc_ecnt */
	int ret = 0;


	d->watch.ext_wdata.time.rtc_ecnt = 32764;
	d->watch.ext_wdata.time.rtc_sctcnt =
		(int)((d->watch.ext_wdata.time.rtc_sctcnt * rtc_count) / 10);

	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_RUN,
		(u8 *)&rtc_ctrl, sizeof(u32));
	if (ret)
		goto error;
	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_SCT,
		(u8 *)&d->watch.ext_wdata.time.rtc_sct, sizeof(u32));
	if (ret)
		goto error;

	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_SCTCNT,
		(u8 *)&d->watch.ext_wdata.time.rtc_sctcnt, sizeof(u32));
	if (ret)
		goto error;

	rtc_ctrl = d->watch.ext_wdata.time.rtc_ecnt & 0xFFFF;
	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_ECNT,
		(u8 *)&rtc_ctrl, sizeof(u32));
	if (ret)
		goto error;

	rtc_ctrl = EXT_WATCH_RTC_START;
	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_RUN,
		(u8 *)&rtc_ctrl, sizeof(u32));
	if (ret)
		goto error;

	TOUCH_I("%s : %02d:%02d:%02d CLK[%d Hz]\n", __func__,
		d->watch.ext_wdata.time.rtc_sct.hour,
		d->watch.ext_wdata.time.rtc_sct.min,
		d->watch.ext_wdata.time.rtc_sct.sec,
		d->watch.ext_wdata.time.rtc_ecnt);

	return ret;

error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}



static int ext_watch_set_position(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 *ptr = (u8 *)(&d->watch.ext_wdata.position);
	int ret = 0;

	if (d->lcd_mode != LCD_MODE_U3) {
		TOUCH_I("skip %s, lcd mode[U%d]\n", __func__, d->lcd_mode);
	} else {
		ret = lg4945_reg_write(dev, EXT_WATCH_POSITION,
					ptr, sizeof(u32)*5);
		if (ret)
			goto error;
	}
	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}


/* ext_watch_set_onoff
 *
 * 'power state' can has only 'ON' or 'OFF'. (not 'SLEEP' or 'WAKE')
 */
static int ext_watch_onoff(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_CTRL,
			&d->watch.ext_wdata.time.disp_waton, sizeof(u8));
	if (ret)
		goto error;
	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}


/* ext_watch_shutdown
 *
 * 'power state' can has only  'SLEEP' or 'WAKE' (not 'ON' or 'OFF')
 */
static int ext_watch_shutdown(struct device *dev, u8 onoff)
{
	u32 rtc_ctrl = EXT_WATCH_RTC_STOP;
	int ret = 0;

	if (onoff == EXT_WATCH_RTC_START)
		rtc_ctrl = EXT_WATCH_RTC_START;

	ret = lg4945_reg_write(dev, EXT_WATCH_RTC_RUN,
		(u8 *)&rtc_ctrl, sizeof(u32));

	if (ret)
		goto error;
	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}
void lg4945_ext_watch_font_download_func(
	struct work_struct *font_download_work)
{
	struct lg4945_data *d =
		container_of(to_delayed_work(font_download_work),
				struct lg4945_data, font_download_work);
	struct touch_core_data *ts = to_touch_core(d->dev);

	u32 wdata = 0;
	int ret = 0;

	if (d->lcd_mode != LCD_MODE_U3) {
		TOUCH_E("skip font download, lcd mode[U%d]\n", d->lcd_mode);
		ret = -EBUSY;
		goto error;
	}

	TOUCH_I("%s start\n", __func__);
	/* Font memory access enable */
	wdata = 1;
	ret = lg4945_reg_write(ts->dev, EXT_WATCH_FONT_ACC_EN,
		(u8 *)&wdata, sizeof(u32));
	if (ret)
		goto error;

	ret = lg4945_reg_write(ts->dev, EXT_WATCH_FONT_COMP_ADDR,
		(u8 *)d->watch.ext_wdata.comp_buf,
		COMP_FONTM_MAX_SIZE);

	if (ret)
		goto error;
	wdata = 0;
	ret = lg4945_reg_write(ts->dev, EXT_WATCH_FONT_ACC_EN,
		(u8 *)&wdata, sizeof(u32));
	if (ret)
		goto error;
	atomic_set(&d->watch.state.font_mem, DOWN_LOADED);
	TOUCH_I("%s done\n", __func__);
	return;
error:
	TOUCH_E("Fail [%d]\n", ret);
}
int lg4945_ext_watch_check_font_download(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	TOUCH_I("%s:start\n", __func__);
	if (atomic_read(&d->watch.state.is_font) != DOWN_LOADED) {
		TOUCH_I("%s : font data is not\n", __func__);
		return -ENOMEM;
	}

	if (touch_boot_mode_check(dev) >= MINIOS_AAT)
		return -EBUSY;

	if (atomic_read(&d->watch.state.font_mem) == DOWN_LOADING) {
		TOUCH_I("%s : doing download..\n", __func__);
		return -EBUSY;
	}
	if (atomic_read(&d->watch.state.font_mem) == DOWN_LOADED) {
		TOUCH_I("%s : not need download..\n", __func__);
		return -EBUSY;
	}

	if (d->watch.ext_wdata.font_data == NULL) {
		TOUCH_E("%s : font data is not ready\n",
			__func__);
		return -ENOMEM;
	}

	queue_delayed_work(ts->wq, &d->font_download_work, 0);

	return ret;
}

#if 1
static int ext_watch_font_dump(struct device *dev, char *font_dump)
{
	u32 font_sel = 0;
	u32 font_data_offset = 0;
	u32 wdata = 0;
	u32 size = 0;
	int ret = 0;

	TOUCH_TRACE();

	/* Font memory access enable */
	wdata = 1;
	ret = lg4945_reg_write(dev, EXT_WATCH_FONT_ACC_EN,
		(u8 *)&wdata, sizeof(u32));
	if (ret)
		goto error;

	size = sizeof(u32) * EXT_WATCH_FONT_NUM_SIZE;

	for (font_sel = 0; font_sel < 10; font_sel++) {
		/* Font select : '0' ~ '9' */
		font_data_offset = font_sel * size;
		ret = lg4945_reg_write(dev, EXT_WATCH_FONT_SEL,
					(u8 *)&font_sel, sizeof(u32));
		if (ret)
			goto error;

		ret = lg4945_reg_read(dev, EXT_WATCH_FONT_OFFSET_ADDR,
			(u8 *)&font_dump[font_data_offset], size);
		if (ret)
			goto error;
	}

	/* Font select : ':' */
	font_data_offset = font_sel * size;
	size = sizeof(u32) * EXT_WATCH_FONT_CHAR_SIZE;
	ret = lg4945_reg_write(dev, EXT_WATCH_FONT_SEL,
		(u8 *)&font_sel, sizeof(u32));
	if (ret)
		goto error;
	ret = lg4945_reg_read(dev, EXT_WATCH_FONT_OFFSET_ADDR,
			(u8 *)&font_dump[font_data_offset], size);
	if (ret)
		goto error;

	/* Font memory access disable */
	wdata = 0;
	ret = lg4945_reg_write(dev, EXT_WATCH_FONT_ACC_EN,
		(u8 *)&wdata, sizeof(u32));
	if (ret)
		goto error;

	TOUCH_I("%s done\n", __func__);
	return ret;

error:
	TOUCH_E("fail %d\n", ret);
	return -EIO;
}
#else
static int ext_watch_font_dump(struct device *dev, char *font_dump)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u32 font_sel = 0;
	u32 font_data_offset = 0;
	u32 wdata = 0;
	u32 size = 0;
	int ret = 0;

	if (!ts->xfer) {
		TOUCH_I("%s alloc failed\n", __func__);
		return -EIO;
	}

	TOUCH_I("%s start\n", __func__);

	ts->xfer->bits_per_word = 8;
	ts->xfer->msg_count = 2;

	/* Font memory access enable */
	wdata = 1;
	ret = lg4945_reg_write(dev, EXT_WATCH_FONT_ACC_EN,
		(u8 *)&wdata, sizeof(u32));
	if (ret)
		goto error;

	size = sizeof(u32) * EXT_WATCH_FONT_NUM_SIZE;

	for (font_sel = 0; font_sel < 10; font_sel++) {
		/* Font select : '0' ~ '9' */
		font_data_offset = font_sel * size;

		ts->xfer->data[0].tx.addr = EXT_WATCH_FONT_SEL;
		ts->xfer->data[0].tx.buf = (u8 *)&font_sel;
		ts->xfer->data[0].tx.size = sizeof(u32);
		ts->xfer->data[1].rx.addr = EXT_WATCH_FONT_OFFSET_ADDR;
		ts->xfer->data[1].rx.buf = font_dump + font_data_offset;
		ts->xfer->data[1].rx.size = size;
		lg4945_xfer_msg(dev, ts->xfer);
	}

	/* Font select : ':' */
	font_data_offset = font_sel * size;
	size = sizeof(u32) * EXT_WATCH_FONT_CHAR_SIZE;

	ts->xfer->data[0].tx.addr = EXT_WATCH_FONT_SEL;
	ts->xfer->data[0].tx.buf = (u8 *)&font_sel;
	ts->xfer->data[0].tx.size = sizeof(u32);
	ts->xfer->data[1].rx.addr = EXT_WATCH_FONT_OFFSET_ADDR;
	ts->xfer->data[1].rx.buf = font_dump + font_data_offset;
	ts->xfer->data[1].rx.size = size;
	lg4945_xfer_msg(dev, ts->xfer);

	/* Font memory access disable */
	wdata = 0;
	ret = lg4945_reg_write(dev, EXT_WATCH_FONT_ACC_EN,
		(u8 *)&wdata, sizeof(u32));
	if (ret)
		goto error;

	TOUCH_I("%s done\n", __func__);
	return ret;

error:
	TOUCH_E("fail %d\n", ret);
	return -EIO;
}
#endif

static int ext_watch_set_cfg(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	d->watch.ext_wdata.mode.watch_ctrl.alpha = 1; /* bypass foreground */
	ret = ext_watch_set_mode(dev);
	if (ret)
		goto error;

	ret = ext_watch_set_position(dev);
	if (ret)
		goto error;

	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -ECANCELED;

}
static int ext_watch_get_dic_st(struct device *dev)
{
	u32 dic_status = 0;
	u32 watch_status = 0;
	int ret = 0;

	TOUCH_TRACE();

	ret = lg4945_reg_read(dev,  SYS_DISPMODE_STATUS, (u8 *)&dic_status,
		sizeof(u32));
	if (ret)
		goto error;
	ret = lg4945_reg_read(dev,  EXT_WATCH_RTC_CTRL, (u8 *)&watch_status,
		sizeof(u32));

	TOUCH_I("DIC_STATUS U%d , Watch %s\n",
		(dic_status>>5)&0x3, watch_status ? "ON" : "OFF");

	return ret;
error:
	TOUCH_E("Fail %d\n", ret);
	return -EIO;
}

static ssize_t store_extwatch_fontonoff
	(struct device *dev, const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 value;
	u8 zero = '0';
	int ret = 0;

	if (atomic_read(&d->block_watch_cfg) == SUPPORT)
		return count;

	memcpy((char *)&value, buf, sizeof(u8));

	if (value == POWER_OFF || value == zero)
		value = 0x00;
	else
		value = 0x01;

	TOUCH_I("%s : %s\n", __func__,
			value ? "On" : "Off");

	ret = ext_watch_set_cfg(dev);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	d->watch.ext_wdata.time.disp_waton = value;
	ret = ext_watch_onoff(dev);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	return count;
}

static ssize_t store_block_cfg(struct device *dev,
		const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 value;
	u8 zero = '0';

	memcpy(&value, buf, sizeof(u8));

	if (value == NOT_SUPPORT || value == zero)
		value = 0x00;
	else
		value = 0x01;

	atomic_set(&d->block_watch_cfg, value);

	TOUCH_I("%s : %s\n", __func__,
			value ? "BLOCK" : "OPEN");

	return count;
}
static ssize_t show_block_cfg(struct device *dev,
	char *buf)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "block_watch_cfg : %d\n",
		atomic_read(&d->block_watch_cfg));

	return ret;
}

static ssize_t store_extwatch_onoff(struct device *dev,
		const char *buf, size_t count)
{
	u8 value;
	u8 zero = '0';
	int ret = 0;

	memcpy(&value, buf, sizeof(u8));

	if (value == POWER_OFF || value == zero)
		value = 0x00;
	else
		value = 0x01;

	ret = ext_watch_shutdown(dev, value);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	TOUCH_I("%s : %s\n", __func__,
			value ? "START" : "STOP");

	return count;
}

static ssize_t show_extwatch_fontdata_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontDataQuery query;

	query.Font_supported = SUPPORT;
	query.max_font_x_size = 160;
	query.max_font_y_size = 64;
	query.max_cln_x_size = 160;
	query.max_cln_y_size = 24;

	memcpy(buf, (char *)&query, sizeof(struct ExtWatchFontDataQuery));
	return sizeof(struct ExtWatchFontDataQuery);
}

static ssize_t show_extwatch_fontposition_query
	(struct device *dev, char *buf)
{
	struct ExtWatchFontPositionQuery query;

	query.vertical_position_supported = NOT_SUPPORT;
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
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	TOUCH_TRACE();
	if (d == NULL)
		return ret;

	ret = lg4945_reg_read(dev, EXT_WATCH_RTC_CTST,
		(u8 *)&d->watch.ext_wdata.time.rtc_ctst, sizeof(u32));

	if (ret) {
		TOUCH_E("lg4945_read error : %d\n", ret);
		return ret;
	}
	ret = snprintf(buf, PAGE_SIZE, "%02d:%02d:%02d:%03d\n",
		d->watch.ext_wdata.time.rtc_ctst.hour,
		d->watch.ext_wdata.time.rtc_ctst.min,
		d->watch.ext_wdata.time.rtc_ctst.sec,
		d->watch.ext_wdata.time.rtc_sctcnt);

	return ret;
}

static ssize_t store_extwatch_fonteffect_config(struct device *dev,
		const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct ExtWatchFontEffectConfig cfg;
	char period[8] = {0};

	if (atomic_read(&d->block_watch_cfg) == SUPPORT)
		return count;

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontEffectConfig));

	d->watch.ext_wdata.position.h24_en = cfg.h24_en;
	d->watch.ext_wdata.position.zero_disp = cfg.zero_disp;
	d->watch.ext_wdata.position.clock_disp_mode = cfg.clock_disp_type;
	d->watch.ext_wdata.position.midnight_hour_zero_en
		= cfg.midnight_hour_zero_en;
	d->watch.ext_wdata.position.bhprd = cfg.blink.blink_type;
	d->watch.ext_wdata.mode.blink_area.bstartx = cfg.blink.bstartx;
	d->watch.ext_wdata.mode.blink_area.bendx = cfg.blink.bendx;
	d->watch.ext_wdata.time.disp_waton = cfg.watchon;

	switch (cfg.blink.blink_type) {
	default:
	case 0:
		snprintf(period, 8, "Off");
		break;
	case 1:
		snprintf(period, 8, "0.5 sec");
		break;
	case 2:
		snprintf(period, 8, "1 sec");
		break;
	case 3:
		snprintf(period, 8, "2 sec");
		break;
	}

	TOUCH_I("%s : 24h mode %s, Zero Dispaly %s,%s Type %s mode "
		"Blink area [%d , %d] Period %s "
		"Watch On/Off : %s\n", __func__,
		cfg.h24_en ? "Enable" : "Disable",
		cfg.zero_disp ? "Enable" : "Disable",
		cfg.clock_disp_type ? "MM:SS" : "HH:MM",
		cfg.midnight_hour_zero_en ? "00:00" : "12:00",
		cfg.blink.bstartx, cfg.blink.bendx, period,
		d->watch.ext_wdata.time.disp_waton ? "On" : "Off");

	return count;
}
static ssize_t store_extwatch_fontproperty_config
	(struct device *dev, const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct ExtWatchFontPropertyConfig cfg;
	int idx = 0;
	char log[256] = {0};
	int len = 0;

	if (atomic_read(&d->block_watch_cfg) == SUPPORT)
		return count;

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontPropertyConfig));

	len += snprintf(log + len, 256 - len, "%s : LUT[%d] ",
		__func__, (int)cfg.max_num);

	for (idx = 0; idx < (int)cfg.max_num; idx++) {
		d->watch.ext_wdata.mode.lut[idx].b = cfg.LUT[idx].RGB_blue;
		d->watch.ext_wdata.mode.lut[idx].g = cfg.LUT[idx].RGB_green;
		d->watch.ext_wdata.mode.lut[idx].r = cfg.LUT[idx].RGB_red;

		len += snprintf(log + len, 256 - len, "%d:%02X%02X%02X ", idx,
			cfg.LUT[idx].RGB_blue, cfg.LUT[idx].RGB_green,
			cfg.LUT[idx].RGB_red);
	}

	TOUCH_I("%s\n", log);

	return count;
}

static ssize_t store_extwatch_fontposition_config(struct device *dev,
		const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct ExtWatchFontPostionConfig cfg;

	if (atomic_read(&d->block_watch_cfg) == SUPPORT)
		return count;

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontPostionConfig));

	d->watch.ext_wdata.mode.watch_area.watstartx = cfg.watstartx;
	d->watch.ext_wdata.mode.watch_area.watendx = cfg.watendx;
	d->watch.ext_wdata.position.h10x_pos = cfg.h10x_pos;
	d->watch.ext_wdata.position.h1x_pos = cfg.h1x_pos;
	d->watch.ext_wdata.position.m10x_pos = cfg.m10x_pos;
	d->watch.ext_wdata.position.m1x_pos = cfg.m1x_pos;
	d->watch.ext_wdata.position.clx_pos = cfg.clx_pos;

	if ( cfg.watstartx < 0x190 || cfg.watendx > 0x5A0 )
		TOUCH_E("check the position. (invalid range)\n");

	TOUCH_I("%s : Watch area [%d , %d] position [%d %d %d %d %d]\n",
		__func__, cfg.watstartx, cfg.watendx, cfg.h10x_pos,
		cfg.h1x_pos, cfg.clx_pos, cfg.m10x_pos, cfg.m1x_pos);

	return count;
}

static ssize_t store_extwatch_timesync_config(struct device *dev,
		const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct ExtWatchTimeSyncConfig cfg;
	int ret = 0;

	if (atomic_read(&d->block_watch_cfg) == SUPPORT)
		return count;

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchTimeSyncConfig));

	d->watch.ext_wdata.time.rtc_sct.hour = cfg.rtc_cwhour;
	d->watch.ext_wdata.time.rtc_sct.min = cfg.rtc_cwmin;
	d->watch.ext_wdata.time.rtc_sct.sec = cfg.rtc_cwsec;
	d->watch.ext_wdata.time.rtc_sctcnt = cfg.rtc_cwmilli;

	TOUCH_I("%s : %02d:%02d:%02d.%03d\n", __func__,
		cfg.rtc_cwhour, cfg.rtc_cwmin, cfg.rtc_cwsec, cfg.rtc_cwmilli);

	ret = ext_watch_set_current_time(dev);
	if (ret)
		TOUCH_E("%s fail\n", __func__);

	return count;
}


static ssize_t show_extwatch_setting(struct device *dev, char *buf)
{
	TOUCH_TRACE();

	ext_watch_get_mode(dev);
	ext_watch_get_position(dev);
	ext_watch_get_current_time(dev);
	ext_watch_get_dic_st(dev);

	return 0;
}

static WATCH_ATTR(watch_onoff, NULL, store_extwatch_onoff);
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
	&watch_attr_watch_onoff.attr,
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
	struct lg4945_data *d = container_of(kobj, struct lg4945_data, kobj);
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
	struct lg4945_data *d = container_of(kobj, struct lg4945_data, kobj);
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
	struct lg4945_data *d = container_of(kobj, struct lg4945_data, kobj);
	ssize_t retval = -EFAULT;

	if (off == 0 && count == PAGE_SIZE)
		ext_watch_font_dump(d->dev, d->watch.ext_wdata.font_data);

	if (count == 0 && off + count >= d->watch.fontdata_size) {
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
	struct lg4945_data *d = to_lg4945_data_from_kobj(kobj);
	struct touch_core_data *ts = to_touch_core(d->dev);
	ssize_t retval = -EFAULT;
	int idx = 0;
	int cur_data;
	int data_count;
	int comp_size = 4;
	int comp_real_size = 0;
	int comp_size_result = 0;
	u8 background = 0;

	if (atomic_read(&d->block_watch_cfg) == SUPPORT)
		return retval;

	if (buf == NULL) {
		TOUCH_E("%s buf is NULL\n", __func__);
		goto finish;
	}
	if (d->watch.ext_wdata.font_data == NULL) {
		TOUCH_E("%s font_data is NULL\n", __func__);
		goto finish;
	}
	if (off + count > FONT_DATA_SIZE)	{
		TOUCH_E("%s off + count[%d] is bigger than FONT_DATA_SIZE\n",
			__func__, (int)off+(int)count);
		goto finish;
	}

	d->watch.font_written_size = off + count;

	if (count == 0 && off + count >= d->watch.fontdata_size) {
		TOUCH_I("%s size error offset[%d] size[%d]\n", __func__,
				(int)off, (int)count);
	} else {
		memcpy(&d->watch.ext_wdata.font_data[off], buf, count);
		retval = count;

		if (off + count == FONT_DATA_SIZE) {
			if (off + count != FONT_DATA_SIZE) {
				TOUCH_E("%s data check.. not match\n",
					__func__);
				goto finish;
			}
			/* Font data compress */
			memset(&d->watch.ext_wdata.comp_buf[0],
			0, d->watch.font_written_comp_size);

			if (d->watch.ext_wdata.font_data[0] !=
				d->watch.ext_wdata.font_data[31])
				background = d->watch.ext_wdata.font_data[0];

			/*check font width size*/
			while (idx < 32) {
				if (!d->watch.ext_wdata.font_data[idx++])
					break;
			}
			if (idx == 1) {
				d->watch.font_width = 64;
			} else {
				d->watch.font_width = (idx - 1) << 1;
			}

			if (d->watch.font_width < 4 || d->watch.font_width > 48 ||
				(d->watch.font_width % 4) > 0) {
				TOUCH_E("font width(%d) is not multiple of 4\n",
					d->watch.font_width);
				retval = -EIO;
				goto finish;
			} else {
				TOUCH_I("font width : %dpixel\n",
					d->watch.font_width);
			}

			idx = 0;

			while (idx < d->watch.font_written_size) {
				cur_data = d->watch.ext_wdata.font_data[idx] ?
					d->watch.ext_wdata.font_data[idx]
					: background;
				idx++;
				data_count = 1;
				while (idx < d->watch.font_written_size
					&& (d->watch.ext_wdata.font_data[idx] ?
					d->watch.ext_wdata.font_data[idx]
					: background) == cur_data
					&& data_count < 0xFF) {
					idx++;
					data_count++;
				}
				if (data_count == 1) {
					cur_data |= E_COMP_1_NUM;
					d->watch.ext_wdata.comp_buf[comp_size++]
						= cur_data;
				} else if (data_count == 2) {
					cur_data |= E_COMP_2_NUM;
					d->watch.ext_wdata.comp_buf[comp_size++]
						= cur_data;
				} else if (data_count == 0xFF) {
					cur_data |= E_COMP_255_NUM;
					d->watch.ext_wdata.comp_buf[comp_size++]
						= cur_data;
				} else {
					d->watch.ext_wdata.comp_buf[comp_size++]
						= cur_data;
					d->watch.ext_wdata.comp_buf[comp_size++]
						= data_count;
				}
			}

			if (comp_size > COMP_FONTM_MAX_SIZE) {
				TOUCH_E("comp_size %d > %d\n",
					comp_size, COMP_FONTM_MAX_SIZE);
				retval = -EIO;
				goto finish;
			}

			comp_real_size = comp_size - 4;
			memcpy(&d->watch.ext_wdata.comp_buf[0], &comp_real_size, 4);

			/* write font width size to buffer */
			if ( d->fw.version[0] == 1 && d->fw.version[1] >= 30 )
				d->watch.ext_wdata.comp_buf[2] = d->watch.font_width;
			else
				TOUCH_E("not support the dynamic pixel font data[v%d.%d]\n",
				d->fw.version[0], d->fw.version[1]);

			if (comp_size % 4 == 0) {
				comp_size_result = comp_size + 4;
			} else {
				int pad_size = 4 - comp_size % 4;
				comp_size_result = comp_size + pad_size + 4;
			}
			*((u32 *)&d->watch.ext_wdata.comp_buf[(comp_size_result)
				- 4]) = COMP_FONT_TRANFER_CODE;

			TOUCH_I("font comp complete [%d][%d]\n", comp_real_size, comp_size);
			queue_delayed_work(ts->wq, &d->font_download_work, 0);
			atomic_set(&d->watch.state.is_font, DOWN_LOADED);
		} else if (off && count != PAGE_SIZE)
			TOUCH_I("%s size error offset[%d] size[%d]\n", __func__,
				(int)off, (int)count);
	}
finish:
	return retval;
}

static int watch_fontdata_attr_init(struct lg4945_data *d)
{
	d->watch.font_written_size = 0;
	d->watch.font_width = 0;
	d->watch.font_written_comp_size = COMP_FONTM_MAX_SIZE;
	d->watch.fontdata_size = MAX_WATCH_DATA_SIZE;
	d->watch.fontdata_comp_size = COMP_FONTM_MAX_SIZE;
	d->watch.ext_wdata.font_data =
			kzalloc(d->watch.fontdata_size, GFP_KERNEL);
	d->watch.ext_wdata.comp_buf =
			kzalloc(d->watch.fontdata_comp_size, GFP_KERNEL);
	if (d->watch.ext_wdata.font_data && d->watch.ext_wdata.comp_buf) {
		TOUCH_I("%s font_buffer(%d KB) malloc\n", __func__,
			d->watch.fontdata_size/1024);
	} else {
		TOUCH_E("%s font_buffer(%d KB) malloc failed\n", __func__,
			d->watch.fontdata_size/1024);
		return 1;
	}

	sysfs_bin_attr_init(&d->watch.fontdata_attr);
	d->watch.fontdata_attr.attr.name = "config_fontdata";
	d->watch.fontdata_attr.attr.mode = S_IWUSR | S_IRUSR;
	d->watch.fontdata_attr.read = watch_access_read;
	d->watch.fontdata_attr.write = watch_access_write;
	d->watch.fontdata_attr.size = d->watch.fontdata_size;

	if (sysfs_create_bin_file(&d->kobj, &d->watch.fontdata_attr) < 0)
		TOUCH_E("Failed to create %s\n",
			d->watch.fontdata_attr.attr.name);
	return 0;
}

int lg4945_watch_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret;

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
int lg4945_watch_init(struct device *dev)
{
	int ret = 0;

	TOUCH_I("%s\n", __func__);

	if (touch_boot_mode_check(dev) >= MINIOS_AAT)
		return -EBUSY;

	ret = ext_watch_set_cfg(dev);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	ret = ext_watch_onoff(dev);
	if (ret)
		TOUCH_E("Fail %d\n", ret);

	return ret;
}
void lg4945_watch_remove(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	kfree(d->watch.ext_wdata.font_data);
	kfree(d->watch.ext_wdata.comp_buf);
}
