/* touch_lg4946_watch.c
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
#include "touch_lg4946.h"
#include "touch_lg4946_watch.h"

// LOOK UP TABLE for CRC16 generation
// Polynomial X^16+X^15+X^2+1
u16 lg4946_crc16lut[] = {
	0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
	0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
	0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
	0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
	0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
	0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
	0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
	0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
	0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
	0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
	0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
	0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
	0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
	0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
	0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
	0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
	0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
	0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
	0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
	0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
	0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
	0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
	0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
	0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
	0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
	0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
	0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
	0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
	0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
	0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
	0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
	0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

static int ext_watch_rtc_start(struct device *dev, u8 start)
{
	u32 rtc_ctrl = EXT_WATCH_RTC_STOP;
	int ret = 0;

	if (start == EXT_WATCH_RTC_START)
		rtc_ctrl = EXT_WATCH_RTC_START;

	ret = lg4946_reg_write(dev, EXT_WATCH_RTC_RUN,
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_mode_cfg mode;
	struct ext_watch_lut_bits *lut = NULL;
	u8 *ptr = NULL;
	u16 idx = 0;
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	TOUCH_I("%s start\n", __func__);

	memset(&mode, 0x0, sizeof(struct ext_watch_mode_cfg));

	lg4946_xfer_msg_ready(dev, 5);

	ts->xfer->data[0].rx.addr = EXT_WATCH_CTRL;
	ts->xfer->data[0].rx.buf = (u8 *)(&mode.watch_ctrl);
	ts->xfer->data[0].rx.size = sizeof(u32);

	ts->xfer->data[1].rx.addr = EXT_WATCH_AREA_X;
	ts->xfer->data[1].rx.buf = (u8 *)(&mode.watch_area_x);
	ts->xfer->data[1].rx.size = sizeof(u32);

	ts->xfer->data[2].rx.addr = EXT_WATCH_AREA_Y;
	ts->xfer->data[2].rx.buf = (u8 *)(&mode.watch_area_y);
	ts->xfer->data[2].rx.size = sizeof(u32);

	ts->xfer->data[3].rx.addr = EXT_WATCH_BLINK_AREA;
	ts->xfer->data[3].rx.buf = (u8 *)(&mode.blink_area);
	ts->xfer->data[3].rx.size = sizeof(u32);

	ts->xfer->data[4].rx.addr = EXT_WATCH_LUT;
	ts->xfer->data[4].rx.buf = (u8 *)(mode.lut);
	ts->xfer->data[4].rx.size = sizeof(u32) * EXT_WATCH_LUT_NUM;

	ret = lg4946_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	ptr = (u8 *)(&mode.watch_ctrl);
	loglen = snprintf(log, 256,
		"Get Offset[%X] watch_ctrl %02X %02X (Mode:%d, Alpha:%X)\n",
		EXT_WATCH_CTRL, ptr[0], ptr[1],
		mode.watch_ctrl.dispmode, mode.watch_ctrl.alpha);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256,
		"Get Offset[%X] Watch area x[%d , %d] y[%d , %d]\n",
		EXT_WATCH_AREA_X,
		mode.watch_area_x.watstart, mode.watch_area_x.watend,
		mode.watch_area_y.watstart, mode.watch_area_y.watend);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256, "Get Offset[%X] Blink area x[%d , %d]\n",
		EXT_WATCH_BLINK_AREA,
		mode.blink_area.bstartx, mode.blink_area.bendx);
	memcpy(&buf[*len], log, loglen);
	*len += loglen;
	TOUCH_I("%s", log);

	loglen = snprintf(log, 256 - loglen, "Get Offset[%X] LUT[%d] ",
		EXT_WATCH_LUT, EXT_WATCH_LUT_NUM);

	for (idx = 0; idx < EXT_WATCH_LUT_NUM; idx++) {
		lut = &mode.lut[idx];
		loglen += snprintf(log + loglen, 256 - loglen, "%d:%02X%02X%02X ",
			idx + 1, lut->b, lut->g, lut->r);
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_position_cfg position = {0};
	struct ext_watch_status_cfg status_cfg = {0};
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	lg4946_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].rx.addr = EXT_WATCH_POSITION_R;
	ts->xfer->data[0].rx.buf = (u8 *)(&position);
	ts->xfer->data[0].rx.size = sizeof(u32) * 3;

	ts->xfer->data[1].rx.addr = EXT_WATCH_STATE;
	ts->xfer->data[1].rx.buf = (u8 *)&status_cfg;
	ts->xfer->data[1].rx.size = sizeof(u32);

	ret = lg4946_xfer_msg(dev, ts->xfer);
	if (ret)
		goto error;

	loglen = snprintf(log, 256,"Get Offset[%X] Position [%d %d %d %d %d]\n",
		EXT_WATCH_POSITION_R, position.h10x_pos, position.h1x_pos,
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
		EXT_WATCH_STATE, position.zero_disp, position.h24_en,
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_status_cfg status_cfg = {0};
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	TOUCH_TRACE();

	lg4946_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].rx.addr = EXT_WATCH_STATE;
	ts->xfer->data[0].rx.buf = (u8 *)&status_cfg;
	ts->xfer->data[0].rx.size = sizeof(u32);

	ts->xfer->data[1].rx.addr = EXT_WATCH_RTC_CTST;
	ts->xfer->data[1].rx.buf = (u8 *)&d->watch.ext_wdata.time.rtc_ctst;
	ts->xfer->data[1].rx.size = sizeof(u32);

	ret = lg4946_xfer_msg(dev, ts->xfer);
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_mode_cfg *mode = &d->watch.ext_wdata.mode;
	int ret = 0;

	lg4946_xfer_msg_ready(dev, 5);

	ts->xfer->data[0].tx.addr = EXT_WATCH_CTRL;
	ts->xfer->data[0].tx.buf = (u8 *)(&d->watch.ext_wdata.mode.watch_ctrl);
	ts->xfer->data[0].tx.size = sizeof(u32);

	ts->xfer->data[1].tx.addr = EXT_WATCH_AREA_X;
	ts->xfer->data[1].tx.buf = (u8 *)(&d->watch.ext_wdata.mode.watch_area_x);
	ts->xfer->data[1].tx.size = sizeof(u32);

	ts->xfer->data[2].tx.addr = EXT_WATCH_AREA_Y;
	ts->xfer->data[2].tx.buf = (u8 *)(&d->watch.ext_wdata.mode.watch_area_y);
	ts->xfer->data[2].tx.size = sizeof(u32);

	ts->xfer->data[3].tx.addr = EXT_WATCH_BLINK_AREA;
	ts->xfer->data[3].tx.buf = (u8 *)(&d->watch.ext_wdata.mode.blink_area);
	ts->xfer->data[3].tx.size = sizeof(u32);

	ts->xfer->data[4].tx.addr = EXT_WATCH_LUT;
	ts->xfer->data[4].tx.buf = (u8 *)(&d->watch.ext_wdata.mode.lut[0]);
	ts->xfer->data[4].tx.size = sizeof(u32) * EXT_WATCH_LUT_NUM;

	ret = lg4946_xfer_msg(dev, ts->xfer);
	if (ret < 0) {
		TOUCH_E("failed %d\n", ret);
	} else if (log) {
		TOUCH_I("\t%s : X[%d , %d] Y[%d , %d] LUT[%02X%02X%02X]\n",
			__func__,
			mode->watch_area_x.watstart, mode->watch_area_x.watend,
			mode->watch_area_y.watstart, mode->watch_area_y.watend,
			mode->lut[6].b, mode->lut[6].g, mode->lut[6].r);
	}

	return ret;
}

static int ext_watch_set_current_time(struct device *dev)
{
	struct lg4946_data *d = to_lg4946_data(dev);
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

	lg4946_xfer_msg_ready(dev, 3);

	ts->xfer->data[0].tx.addr = EXT_WATCH_RTC_SCT;
	ts->xfer->data[0].tx.buf = (u8 *)&d->watch.ext_wdata.time.rtc_sct;
	ts->xfer->data[0].tx.size = sizeof(u32);

	ts->xfer->data[1].tx.addr = EXT_WATCH_RTC_SCTCNT;
	ts->xfer->data[1].tx.buf = (u8 *)&d->watch.ext_wdata.time.rtc_sctcnt;
	ts->xfer->data[1].tx.size = sizeof(u32);

	ts->xfer->data[2].tx.addr = EXT_WATCH_RTC_ECNT;
	ts->xfer->data[2].tx.buf = (u8 *)&rtc_ctrl;
	ts->xfer->data[2].tx.size = sizeof(u32);

	ret = lg4946_xfer_msg(dev, ts->xfer);
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	u8 *ptr = (u8 *)(&d->watch.ext_wdata.position);
	struct ext_watch_position_cfg *cfg = &d->watch.ext_wdata.position;
	int ret = 0;

	lg4946_xfer_msg_ready(dev, 5);

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

	ret = lg4946_xfer_msg(dev, ts->xfer);
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
	struct lg4946_data *d = to_lg4946_data(dev);
	int ret = 0;

	if ((atomic_read(&d->watch.state.rtc_status) != RTC_RUN) &&
		d->watch.ext_wdata.time.disp_waton) {
		d->watch.ext_wdata.time.rtc_sct.hour = d->watch.ext_wdata.time.rtc_ctst.hour;
		d->watch.ext_wdata.time.rtc_sct.min = d->watch.ext_wdata.time.rtc_ctst.min;
		d->watch.ext_wdata.time.rtc_sct.sec = d->watch.ext_wdata.time.rtc_ctst.sec;
		d->watch.ext_wdata.time.rtc_sctcnt = 0;
		ret = ext_watch_set_current_time(dev);
	}

	if ((d->lcd_mode == LCD_MODE_U2
		|| d->lcd_mode == LCD_MODE_U2_UNBLANK)
		&& d->watch.ext_wdata.time.disp_waton) {
		ret = lg4946_reg_write(dev, EXT_WATCH_DCS_CTRL,
				&d->watch.ext_wdata.time.disp_waton, sizeof(u32));
		if (log)
			TOUCH_I("%s(%X) : %s for U2\n", __func__, EXT_WATCH_DCS_CTRL,
				d->watch.ext_wdata.time.disp_waton ? "On" : "Off");
	}

	ret = lg4946_reg_write(dev, EXT_WATCH_DISPLAY_ON,
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

void lg4946_watch_display_off(struct device *dev)
{
	u32 disp = 0;

	lg4946_reg_write(dev, EXT_WATCH_DISPLAY_ON, &disp, sizeof(u32));

	TOUCH_I("%s\n", __func__);
}

u16 lg4946_crc16wordcalc(const u16 *data, u32 datalen, u16 initval)
{
	u32 i = 0;
	u16 CRCSum = 0;
	u8 tempData = 0;

	CRCSum = initval;
	for (i = 0; i < datalen; i += 2) {
		tempData = (u8)((data[i] >> 8) & 0xFF);
		CRCSum = (CRCSum << 8) ^
			lg4946_crc16lut[((CRCSum >> 8) & 0xFF) ^ tempData];

		tempData = (u8)(data[i] & 0xFF);
		CRCSum = (CRCSum << 8) ^
			lg4946_crc16lut[((CRCSum >> 8) & 0xFF) ^ tempData];
	}

	return CRCSum;
}

u32 lg4946_font_crc_cal(char *data, u32 data_size)
{
	u32 crc_value = 0;
	u32 crc_size = 0;

	crc_size = sizeof(struct ext_watch_font_header) + data_size;

	/*CRC Calculation*/
	crc_value = lg4946_crc16wordcalc((const u16*)&data[0], crc_size / 2, 0) \
		| (lg4946_crc16wordcalc((const u16*)&data[2], crc_size / 2, 0) << 16);

	return crc_value & 0x3FFFFFFF;
}

void lg4946_font_download(struct work_struct *font_download_work)
{
	struct lg4946_data *d = container_of(to_delayed_work(font_download_work),
				struct lg4946_data, font_download_work);
	struct touch_core_data *ts = to_touch_core(d->dev);
	struct ext_watch_font_header *fonthdr = NULL;
	int ret = 0;
	int remained = 0;
	u32 offset = 0;
	int rwsize = 0;
	int idx = 0;
	int value = 1;

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
		ret = lg4946_reg_write(ts->dev, EXT_WATCH_MEM_CTRL,
			(u8*)&value, sizeof(u32));
		TOUCH_I("U2 Mode : Watch Display off, Memory clk on\n");
	}

	fonthdr = (struct ext_watch_font_header *)d->watch.ext_wdata.font_data;
	TOUCH_I("\tMagic[%08X], id[%d], size[%d]\n",
		fonthdr->magic_code, fonthdr->font_id, fonthdr->size);
	TOUCH_I("\twidth_num[%d], width_colon[%d], height[%d]\n",
		fonthdr->width_num, fonthdr->width_colon, fonthdr->height);

	memcpy((u8 *)&d->watch.ext_wdata.font_crc,
		(u8 *)&d->watch.ext_wdata.font_data[sizeof(struct ext_watch_font_header) +
		fonthdr->size], sizeof(u32));
	TOUCH_I("\tInput CRC = %08X\n", d->watch.ext_wdata.font_crc);

	d->watch.ext_wdata.font_crc =
		lg4946_font_crc_cal(d->watch.ext_wdata.font_data, fonthdr->size);

	TOUCH_I("\tCalculated CRC = %08X\n", d->watch.ext_wdata.font_crc);

	if (d->watch.font_written_size ==
		sizeof(struct ext_watch_font_header) + fonthdr->size) {
		d->watch.font_written_size += sizeof(d->watch.ext_wdata.font_crc);
	}

	memcpy((u8 *)&d->watch.ext_wdata.font_data[
		sizeof(struct ext_watch_font_header) + fonthdr->size],
		(u8 *)&d->watch.ext_wdata.font_crc, sizeof(u32));

	remained = d->watch.font_written_size;

	while(1) {
		if (remained > MAX_RW_SIZE) {
			rwsize = MAX_RW_SIZE;
			remained -= MAX_RW_SIZE;
		} else {
			rwsize = remained;
			remained = 0;
		}

		ret = lg4946_reg_write(ts->dev, EXT_WATCH_FONT_OFFSET,
			(u8*)&offset, sizeof(u32));
		if (ret)
			goto error;

		ret = lg4946_reg_write(ts->dev, EXT_WATCH_FONT_ADDR,
			(u8 *)&d->watch.ext_wdata.font_data[idx], rwsize);
		if (ret)
			goto error;

		idx += rwsize;
		offset = (idx / 4);

		if (remained == 0)
			break;
	}

	ret = lg4946_reg_write(ts->dev, EXT_WATCH_FONT_CRC,
		(u8 *)&value, sizeof(u32));

	atomic_set(&d->watch.state.font_status, FONT_READY);

	if (d->lcd_mode == LCD_MODE_U2) {
		value = 0;
		ret = lg4946_reg_write(ts->dev, EXT_WATCH_MEM_CTRL,
			(u8*)&value, sizeof(u32));
		TOUCH_I("U2 Mode : Watch Memory clk off\n");
	}

	TOUCH_I("%s %d bytes done\n", __func__, d->watch.font_written_size);
	mutex_unlock(&ts->lock);
	return;

error:
	atomic_set(&d->watch.state.font_status, FONT_EMPTY);
	TOUCH_I("%s fail %d\n", __func__, ret);
	return;
}

int lg4946_check_font_status(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4946_data *d = to_lg4946_data(dev);
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

	lg4946_reg_read(dev, tc_status, &status, sizeof(int));
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	int ret = 0;
	int remained = 0;
	u32 offset = 0;
	int rwsize = 0;
	int idx = 0;
	int value = 1;

	mutex_lock(&ts->lock);
	TOUCH_I("%s start\n", __func__);

	if (d->lcd_mode == LCD_MODE_U2) {
		ret = lg4946_reg_write(ts->dev, EXT_WATCH_MEM_CTRL,
			(u8*)&value, sizeof(u32));
		TOUCH_I("U2 Mode : Watch Memory clk on\n");
	}

	remained = d->watch.font_written_size;

	while(1) {
		if (remained > MAX_RW_SIZE) {
			rwsize = MAX_RW_SIZE;
			remained -= MAX_RW_SIZE;
		} else {
			rwsize = remained;
			remained = 0;
		}

		ret = lg4946_reg_write(dev, EXT_WATCH_FONT_OFFSET,
			(u8*)&offset, sizeof(u32));
		if (ret)
			goto error;

		ret = lg4946_reg_read(dev, EXT_WATCH_FONT_ADDR,
			(u8 *)&font_dump[idx], rwsize);
		if (ret)
			goto error;

		idx += rwsize;
		offset = (idx / 4);

		if (remained == 0)
			break;
	}

	TOUCH_I("%s done\n", __func__);

	if (d->lcd_mode == LCD_MODE_U2) {
		value = 0;
		ret = lg4946_reg_write(ts->dev, EXT_WATCH_MEM_CTRL,
			(u8*)&value, sizeof(u32));
		TOUCH_I("U2 Mode : Watch Memory clk off\n");
	}
	mutex_unlock(&ts->lock);
	return ret;

error:
	TOUCH_I("%s fail %d\n", __func__, ret);
	mutex_unlock(&ts->lock);
	return -EIO;
}

static int ext_watch_set_cfg(struct device *dev, char log)
{
	struct lg4946_data *d = to_lg4946_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	d->watch.ext_wdata.mode.watch_ctrl.alpha = 1; /* bypass foreground */
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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct touch_core_data *ts = to_touch_core(d->dev);
	u32 dic_status = 0;
	u32 watch_status = 0;
	int ret = 0;
	char log[256] = {0};
	int loglen = 0;

	TOUCH_TRACE();

	lg4946_xfer_msg_ready(dev, 2);

	ts->xfer->data[0].rx.addr = SYS_DISPMODE_STATUS;
	ts->xfer->data[0].rx.buf = (u8 *)&dic_status;
	ts->xfer->data[0].rx.size = sizeof(u32);

	ts->xfer->data[1].rx.addr = EXT_WATCH_DISPLAY_STATUS;
	ts->xfer->data[1].rx.buf = (u8 *)&watch_status;
	ts->xfer->data[1].rx.size = sizeof(u32);

	ret = lg4946_xfer_msg(dev, ts->xfer);
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
	struct lg4946_data *d = to_lg4946_data(dev);
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

	if (d->lcd_mode == LCD_MODE_U2_UNBLANK && value) {
		TOUCH_I("%s : Ignore HW Clock On in U2 Unblank\n", __func__);
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
	struct lg4946_data *d = to_lg4946_data(dev);
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
	struct lg4946_data *d = to_lg4946_data(dev);
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
	query.max_font_x_size = 255;
	query.max_font_y_size = 184;
	query.max_cln_x_size = 255;
	query.max_cln_y_size = 48;

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
	struct lg4946_data *d = to_lg4946_data(dev);
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
	struct lg4946_data *d = to_lg4946_data(dev);
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
	d->watch.ext_wdata.mode.blink_area.bstartx = cfg.blink.bstartx;
	d->watch.ext_wdata.mode.blink_area.bendx = cfg.blink.bendx;

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
	struct lg4946_data *d = to_lg4946_data(dev);
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
		d->watch.ext_wdata.mode.lut[idx].b = cfg.LUT[idx].RGB_blue;
		d->watch.ext_wdata.mode.lut[idx].g = cfg.LUT[idx].RGB_green;
		d->watch.ext_wdata.mode.lut[idx].r = cfg.LUT[idx].RGB_red;

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
	struct lg4946_data *d = to_lg4946_data(dev);
	struct ExtWatchFontPostionConfig cfg;

	if (atomic_read(&d->block_watch_cfg) == BLOCKED) {
		TOUCH_I("%s : blocked\n", __func__);
		return count;
	}

	memcpy((char *)&cfg, buf, sizeof(struct ExtWatchFontPostionConfig));

	d->watch.ext_wdata.mode.watch_area_x.watstart = cfg.watstartx;
	d->watch.ext_wdata.mode.watch_area_x.watend = cfg.watendx;
	d->watch.ext_wdata.mode.watch_area_y.watstart = cfg.watstarty;
	d->watch.ext_wdata.mode.watch_area_y.watend = cfg.watendy;
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
	struct lg4946_data *d = to_lg4946_data(dev);
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
	struct lg4946_data *d = container_of(kobj, struct lg4946_data, kobj);
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
	struct lg4946_data *d = container_of(kobj, struct lg4946_data, kobj);
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
	struct lg4946_data *d = container_of(kobj, struct lg4946_data, kobj);
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
	struct lg4946_data *d = to_lg4946_data_from_kobj(kobj);
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
		d->watch.font_written_size = off + retval;
		mod_delayed_work(ts->wq, &d->font_download_work, 20);
	}
finish:
	return retval;
}

static int watch_fontdata_attr_init(struct lg4946_data *d)
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

int lg4946_watch_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4946_data *d = to_lg4946_data(dev);
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

int lg4946_watch_init(struct device *dev)
{
	struct lg4946_data *d = to_lg4946_data(dev);
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

void lg4946_watch_remove(struct device *dev)
{
	struct lg4946_data *d = to_lg4946_data(dev);
	kfree(d->watch.ext_wdata.font_data);
}

