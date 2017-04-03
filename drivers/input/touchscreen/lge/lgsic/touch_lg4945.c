/* touch_lg4945.c
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
#define TS_MODULE "[lg4945]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_lg4945.h"
#include "touch_lg4945_bl_code.h"
#include "touch_lg4945_abt.h"
#include "touch_lg4945_prd.h"

static const char *debug_type[] = {
	"Disable Type",
	"Buffer Type",
	"Always Report Type"
};
#define TCI_FAIL_NUM 11
static const char const *tci_debug_str[TCI_FAIL_NUM] = {
	"NONE",
	"DISTANCE_INTER_TAP",
	"DISTANCE_TOUCHSLOP",
	"TIMEOUT_INTER_TAP_LONG",
	"MULTI_FINGER",
	"DELAY_TIME",/* It means Over Tap */
	"TIMEOUT_INTER_TAP_SHORT",
	"PALM_STATE",
	"TAP_TIMEOVER",
	"DEBUG9",
	"DEBUG10"
};
#define SWIPE_FAIL_NUM 7
static const char const *swipe_debug_str[SWIPE_FAIL_NUM] = {
	"ERROR",
	"1FINGER_FAST_RELEASE",
	"MULTI_FINGER",
	"FAST_SWIPE",
	"SLOW_SWIPE",
	"OUT_OF_AREA",
	"RATIO_FAIL",
};

int lg4945_xfer_msg(struct device *dev, struct touch_xfer_msg *xfer)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	int ret = 0;
	int i = 0;

	mutex_lock(&d->spi_lock);

	for (i = 0; i < xfer->msg_count; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		if (rx->size) {
			tx->data[0] = 0;
			tx->data[1] = ((rx->size > 4) ? 3 : 1);
			tx->data[2] = ((rx->addr >> 8) & 0xff);
			tx->data[3] = (rx->addr & 0xff);
			tx->data[4] = 0;
			tx->data[5] = 0;
			rx->size += R_HEADER_SIZE;
		} else {
			if (tx->size > (MAX_XFER_BUF_SIZE - W_HEADER_SIZE)) {
				TOUCH_E("buffer overflow\n");
				mutex_unlock(&d->spi_lock);
				return -EOVERFLOW;
			}

			tx->data[0] = 0;
			tx->data[1] = ((tx->size == 1) ? 2 : 0);
			tx->data[2] = ((tx->addr >> 8) & 0xff);
			tx->data[3] = (tx->addr  & 0xff);
			memcpy(&tx->data[W_HEADER_SIZE], tx->buf, tx->size);
			tx->size += W_HEADER_SIZE;
		}
	}

	ret = touch_bus_xfer(dev, xfer);
	if (ret) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->spi_lock);
		return ret;
	}

	for (i = 0; i < xfer->msg_count; i++) {
		rx = &xfer->data[i].rx;

		if (rx->size) {
			memcpy(rx->buf, rx->data + R_HEADER_SIZE,
				(rx->size - R_HEADER_SIZE));

			rx->size = 0;
		}
	}

	mutex_unlock(&d->spi_lock);

	return 0;
}

int lg4945_reg_read(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	struct touch_bus_msg msg;
	int ret = 0;

	mutex_lock(&d->spi_lock);
	ts->tx_buf[0] = 0;
	ts->tx_buf[1] = ((size > 4) ? 3 : 1);
	ts->tx_buf[2] = ((addr >> 8) & 0xff);
	ts->tx_buf[3] = (addr & 0xff);
	ts->tx_buf[4] = 0;
	ts->tx_buf[5] = 0;

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = R_HEADER_SIZE;
	msg.rx_buf = ts->rx_buf;
	msg.rx_size = R_HEADER_SIZE + size;

	ret = touch_bus_read(dev, &msg);

	if (ret) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->spi_lock);
		return ret;
	}

	memcpy(data, &ts->rx_buf[R_HEADER_SIZE], size);
	mutex_unlock(&d->spi_lock);
	return 0;
}

int lg4945_reg_write(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	struct touch_bus_msg msg;
	int ret = 0;

	mutex_lock(&d->spi_lock);
	ts->tx_buf[0] = 0;
	ts->tx_buf[1] = ((size == 1) ? 2 : 0);
	ts->tx_buf[2] = ((addr >> 8) & 0xff);
	ts->tx_buf[3] = (addr  & 0xff);

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = W_HEADER_SIZE + size;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	memcpy(&ts->tx_buf[W_HEADER_SIZE], data, size);

	ret = touch_bus_write(dev, &msg);
	mutex_unlock(&d->spi_lock);

	if (ret) {
		TOUCH_E("touch bus error : %d\n", ret);
		return ret;
	}

	return 0;
}

static int lg4945_cmd_write(struct device *dev, u8 cmd)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct touch_bus_msg msg;
	u8 input[2] = {0, };
	int ret = 0;

	input[0] = 0;
	input[1] = cmd;

	msg.tx_buf = input;
	msg.tx_size = 2;

	msg.rx_buf = NULL;
	msg.rx_size = 0;

	mutex_lock(&d->spi_lock);

	ret = touch_bus_write(dev, &msg);

	mutex_unlock(&d->spi_lock);

	if (ret) {
		TOUCH_E("touch bus error : %d\n", ret);
		return ret;
	}

	return 0;
}

static int lg4945_power(struct device *dev, int ctrl)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	TOUCH_TRACE();

	switch (ctrl) {
	case POWER_OFF:
		TOUCH_I("%s, off\n", __func__);
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_power_vio(dev, 0);
		touch_power_vdd(dev, 0);
		touch_msleep(1);
		atomic_set(&d->watch.state.font_mem, EMPTY);
		break;

	case POWER_ON:
		TOUCH_I("%s, on\n", __func__);
		touch_power_vdd(dev, 1);
		touch_power_vio(dev, 1);
		touch_gpio_direction_output(ts->reset_pin, 1);
		break;


	case POWER_SLEEP:
		TOUCH_I("%s, sleep\n", __func__);
		break;

	case POWER_WAKE:
		TOUCH_I("%s, wake\n", __func__);
		break;
	}

	return 0;
}

static void lg4945_get_tci_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	ts->tci.info[TCI_1].tap_count = 2;
	ts->tci.info[TCI_1].min_intertap = 6;
	ts->tci.info[TCI_1].max_intertap = 70;
	ts->tci.info[TCI_1].touch_slop = 100;
	ts->tci.info[TCI_1].tap_distance = 10;
	ts->tci.info[TCI_1].intr_delay = 0;

	ts->tci.info[TCI_2].min_intertap = 6;
	ts->tci.info[TCI_2].max_intertap = 70;
	ts->tci.info[TCI_2].touch_slop = 100;
	ts->tci.info[TCI_2].tap_distance = 255;
	ts->tci.info[TCI_2].intr_delay = 20;
}

static void lg4945_get_swipe_info(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);

	d->swipe.info[SWIPE_L].distance = 5;
	d->swipe.info[SWIPE_L].ratio_thres = 100;
	d->swipe.info[SWIPE_L].ratio_distance = 2;
	d->swipe.info[SWIPE_L].ratio_period = 5;
	d->swipe.info[SWIPE_L].min_time = 0;
	d->swipe.info[SWIPE_L].max_time = 150;
	d->swipe.info[SWIPE_L].area.x1 = 401;	/* 0 */
	d->swipe.info[SWIPE_L].area.y1 = 0;	/* 2060 */
	d->swipe.info[SWIPE_L].area.x2 = 1439;
	d->swipe.info[SWIPE_L].area.y2 = 159;	/* 2559 */

	d->swipe.info[SWIPE_R].distance = 5;
	d->swipe.info[SWIPE_R].ratio_thres = 100;
	d->swipe.info[SWIPE_R].ratio_distance = 2;
	d->swipe.info[SWIPE_R].ratio_period = 5;
	d->swipe.info[SWIPE_R].min_time = 0;
	d->swipe.info[SWIPE_R].max_time = 150;
	d->swipe.info[SWIPE_R].area.x1 = 401;
	d->swipe.info[SWIPE_R].area.y1 = 0;
	d->swipe.info[SWIPE_R].area.x2 = 1439;
	d->swipe.info[SWIPE_R].area.y2 = 159;

	d->swipe.mode = SWIPE_LEFT_BIT | SWIPE_RIGHT_BIT;
}

int lg4945_ic_info(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u32 version = 0;
	u32 revision = 0;
	u32 bootmode = 0;
	u32 product[2] = {0};
	char rev_str[32] = {0};

#if 0
	ts->xfer->msg_count = 4;

	ts->xfer->data[0].rx.addr = tc_version;
	ts->xfer->data[0].rx.buf = (u8 *)&version;
	ts->xfer->data[0].rx.size = sizeof(version);

	ts->xfer->data[1].rx.addr = info_chip_revision;
	ts->xfer->data[1].rx.buf = (u8 *)&revision;
	ts->xfer->data[1].rx.size = sizeof(revision);

	ts->xfer->data[2].rx.addr = tc_product_id1;
	ts->xfer->data[2].rx.buf = (u8 *)&product[0];
	ts->xfer->data[2].rx.size = sizeof(product);

	ts->xfer->data[3].rx.addr = SYS_DISPMODE_ST;
	ts->xfer->data[3].rx.buf = (u8 *)&bootmode;
	ts->xfer->data[3].rx.size = sizeof(bootmode);

	lg4945_xfer_msg(dev, ts->xfer);
#else
	ret = lg4945_reg_read(dev, tc_version, &version, sizeof(version));
	if (ret < 0) {
		TOUCH_D(BASE_INFO, "version : %x\n", version);
		return ret;
	}

	ret = lg4945_reg_read(dev, info_chip_revision, &revision, sizeof(revision));
	ret = lg4945_reg_read(dev, tc_product_id1, &product[0], sizeof(product));
	ret = lg4945_reg_read(dev, SYS_DISPMODE_ST, &bootmode, sizeof(bootmode));
#endif

	d->fw.version[0] = ((version >> 8) & 0xFF);
	d->fw.version[1] = version & 0xFF;
	d->fw.revision = revision & 0xFF;
	memcpy(&d->fw.product_id[0], &product[0], sizeof(product));

	if (d->fw.revision == 0xFF)
		snprintf(rev_str, 32, "revision: Flash Erased(0xFF)");
	else
		snprintf(rev_str, 32, "revision: %d", d->fw.revision);

	TOUCH_D(BASE_INFO, "version : v%d.%02d, chip : %d, protocol : %d\n" \
		"[Touch] %s\n" \
		"[Touch] product id : %s\n" \
		"[Touch] flash boot : %s, %s, crc : %s\n",
		d->fw.version[0], d->fw.version[1],
		(version >> 16) & 0xFF, (version >> 24) & 0xFF, rev_str,  d->fw.product_id,
		(bootmode >> 1 & 0x1) ? "BUSY" : "idle",
		(bootmode >> 2 & 0x1) ? "done" : "booting",
		(bootmode >> 3 & 0x1) ? "ERROR" : "ok");

	if ((((version >> 16) & 0xFF) != 6) || (((version >> 24) & 0xFF) != 3)) {
		TOUCH_I("FW is in abnormal state because of ESD or something.\n");
		lg4945_power(dev, POWER_OFF);
		lg4945_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
	}

	return ret;
}

static int lg4945_get_tci_data(struct device *dev, int count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 i = 0;
	u32 rdata[MAX_LPWG_CODE];

	if (!count)
		return 0;

	ts->lpwg.code_num = count;

	memcpy(&rdata, d->info.data, sizeof(u32) * count);

	for (i = 0; i < count; i++) {
		ts->lpwg.code[i].x = rdata[i] & 0xffff;
		ts->lpwg.code[i].y = (rdata[i] >> 16) & 0xffff;

		if (ts->lpwg.mode == LPWG_PASSWORD)
			TOUCH_I("LPWG data xxxx, xxxx\n");
		else
			TOUCH_I("LPWG data %d, %d\n",
				ts->lpwg.code[i].x, ts->lpwg.code[i].y);
	}
	ts->lpwg.code[count].x = -1;
	ts->lpwg.code[count].y = -1;

	return 0;
}

static int lg4945_get_swipe_data(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	u32 rdata[3];
	int count = 1;

	/* swipe_info */
	/* start (X, Y), end (X, Y), time = 2bytes * 5 = 10 bytes */
	memcpy(&rdata, d->info.data, sizeof(u32) * 3);

	TOUCH_I("Swipe Gesture: start(%4d,%4d) end(%4d,%4d) swipe_time(%dms)\n",
			rdata[0] & 0xffff, rdata[0] >> 16,
			rdata[1] & 0xffff, rdata[1] >> 16,
			rdata[2] & 0xffff);

	ts->lpwg.code_num = count;
	ts->lpwg.code[0].x = rdata[1] & 0xffff;
	ts->lpwg.code[0].y = rdata[1]  >> 16;

	ts->lpwg.code[count].x = -1;
	ts->lpwg.code[count].y = -1;

	return 0;
}

static int lg4945_get_u3fake(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);

	return d->u3fake;
}

static void lg4945_set_u3fake(struct device *dev, u8 value)
{
	struct lg4945_data *d = to_lg4945_data(dev);

	d->u3fake = value;
}

static void set_debug_reason(struct device *dev, int type)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u32 wdata[2] = {0, };
	wdata[0] = (u32)type;

	wdata[0] |= (d->tci_debug_type == 1) ? 0x01 << 2 : 0x01 << 3;
	wdata[1] = TCI_DEBUG_ALL;
	TOUCH_I("TCI%d-type:%d\n", type + 1, wdata[0]);

	lg4945_reg_write(dev, TCI_FAIL_DEBUG_W, wdata, sizeof(wdata));

	return;
}

static int lg4945_tci_knock(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	struct tci_info *info2 = &ts->tci.info[TCI_2];
	u32 lpwg_data[7];

	if (d->tci_debug_type != 0)
		set_debug_reason(dev, TCI_1);

	lpwg_data[0] = ts->tci.mode;
	lpwg_data[1] = info1->tap_count | (info2->tap_count << 16);
	lpwg_data[2] = info1->min_intertap | (info2->min_intertap << 16);
	lpwg_data[3] = info1->max_intertap | (info2->max_intertap << 16);
	lpwg_data[4] = info1->touch_slop | (info2->touch_slop << 16);
	lpwg_data[5] = info1->tap_distance | (info2->tap_distance << 16);
	lpwg_data[6] = info1->intr_delay | (info2->intr_delay << 16);

	return lg4945_reg_write(dev, TCI_ENABLE_W,
			&lpwg_data[0], sizeof(lpwg_data));
}

static int lg4945_tci_password(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	if (d->tci_debug_type != 0)
		set_debug_reason(dev, TCI_2);

	return lg4945_tci_knock(dev);
}

static int lg4945_tci_active_area(struct device *dev,
		u32 x1, u32 y1, u32 x2, u32 y2)
{
	int ret = 0;

	ret = lg4945_reg_write(dev, ACT_AREA_X1_W,
			&x1, sizeof(x1));
	ret = lg4945_reg_write(dev, ACT_AREA_Y1_W,
			&y1, sizeof(y1));
	ret = lg4945_reg_write(dev, ACT_AREA_X2_W,
			&x2, sizeof(x2));
	ret = lg4945_reg_write(dev, ACT_AREA_Y2_W,
			&y2, sizeof(y2));

	return ret;
}

static int lg4945_tci_control(struct device *dev, int type)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	struct tci_info *info2 = &ts->tci.info[TCI_2];
	u32 lpwg_data;
	int ret = 0;

	switch (type) {
	case ENABLE_CTRL:
		lpwg_data = ts->tci.mode;
		ret = lg4945_reg_write(dev, TCI_ENABLE_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case TAP_COUNT_CTRL:
		lpwg_data = info1->tap_count | (info2->tap_count << 16);
		ret = lg4945_reg_write(dev, TAP_COUNT_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case MIN_INTERTAP_CTRL:
		lpwg_data = info1->min_intertap | (info2->min_intertap << 16);
		ret = lg4945_reg_write(dev, MIN_INTERTAP_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case MAX_INTERTAP_CTRL:
		lpwg_data = info1->max_intertap | (info2->max_intertap << 16);
		ret = lg4945_reg_write(dev, MAX_INTERTAP_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case TOUCH_SLOP_CTRL:
		lpwg_data = info1->touch_slop | (info2->touch_slop << 16);
		ret = lg4945_reg_write(dev, TOUCH_SLOP_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case TAP_DISTANCE_CTRL:
		lpwg_data = info1->tap_distance | (info2->tap_distance << 16);
		ret = lg4945_reg_write(dev, TAP_DISTANCE_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case INTERRUPT_DELAY_CTRL:
		lpwg_data = info1->intr_delay | (info2->intr_delay << 16);
		ret = lg4945_reg_write(dev, INT_DELAY_W,
				&lpwg_data, sizeof(lpwg_data));
		break;

	case ACTIVE_AREA_CTRL:
		ret = lg4945_tci_active_area(dev,
				ts->tci.area.x1,
				ts->tci.area.y1,
				ts->tci.area.x2,
				ts->tci.area.y2);
		break;

	case ACTIVE_AREA_RESET_CTRL:
		ret = lg4945_tci_active_area(dev,
				(65 | 65 << 16),
				(1374 | 1374 << 16),
				(65 | 65 << 16),
				(2494 | 2494 << 16));
		break;

	default:
		break;
	}

	return ret;
}

static int lg4945_lpwg_control(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	int ret = 0;

	switch (mode) {
	case LPWG_DOUBLE_TAP:
		ts->tci.mode = 0x01;
		info1->intr_delay = 0;
		info1->tap_distance = 10;

		ret = lg4945_tci_knock(dev);
		break;

	case LPWG_PASSWORD:
		ts->tci.mode = 0x01 | (0x01 << 16);
		info1->intr_delay = ts->tci.double_tap_check ? 68 : 0;
		info1->tap_distance = 7;

		ret = lg4945_tci_password(dev);
		break;

	default:
		ts->tci.mode = 0;
		ret = lg4945_tci_control(dev, ENABLE_CTRL);
		break;
	}

	TOUCH_I("lg4945_lpwg_control mode = %d\n", mode);

	return ret;
}

static int lg4945_swipe_active_area(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct swipe_info *left = &d->swipe.info[SWIPE_L];
	struct swipe_info *right = &d->swipe.info[SWIPE_R];
	u32 active_area[4] = {0x0, };
	int ret = 0;

	active_area[0] = (right->area.x1) | (left->area.x1 << 16);
	active_area[1] = (right->area.y1) | (left->area.y1 << 16);
	active_area[2] = (right->area.x2) | (left->area.x2 << 16);
	active_area[3] = (right->area.y2) | (left->area.y2 << 16);

	ret = lg4945_reg_write(dev, SWIPE_ACT_AREA_X1_W,
			active_area, sizeof(active_area));

	return ret;
}

static int lg4945_swipe_control(struct device *dev, int type)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct swipe_info *left = &d->swipe.info[SWIPE_L];
	struct swipe_info *right = &d->swipe.info[SWIPE_R];
	u32 swipe_data = 0;
	int ret = 0;

	switch (type) {
	case SWIPE_ENABLE_CTRL:
		swipe_data = d->swipe.mode;
		ret = lg4945_reg_write(dev, SWIPE_ENABLE_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_DISABLE_CTRL:
		swipe_data = 0;
		ret = lg4945_reg_write(dev, SWIPE_ENABLE_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_DIST_CTRL:
		swipe_data = (right->distance) | (left->distance << 16);
		ret = lg4945_reg_write(dev, SWIPE_DIST_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_RATIO_THR_CTRL:
		swipe_data = (right->ratio_thres) | (left->ratio_thres << 16);
		ret = lg4945_reg_write(dev, SWIPE_RATIO_THR_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_RATIO_PERIOD_CTRL:
		swipe_data = (right->ratio_period) | (left->ratio_period << 16);
		ret = lg4945_reg_write(dev, SWIPE_RATIO_PERIOD_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_RATIO_DIST_CTRL:
		swipe_data = (right->ratio_distance) |
				(left->ratio_distance << 16);
		ret = lg4945_reg_write(dev, SWIPE_RATIO_DIST_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_TIME_MIN_CTRL:
		swipe_data = (right->min_time) | (left->min_time << 16);
		ret = lg4945_reg_write(dev, SWIPE_TIME_MIN_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_TIME_MAX_CTRL:
		swipe_data = (right->max_time) | (left->max_time << 16);
		ret = lg4945_reg_write(dev, SWIPE_TIME_MAX_W,
				&swipe_data, sizeof(swipe_data));
		break;
	case SWIPE_AREA_CTRL:
		ret = lg4945_swipe_active_area(dev);
		break;
	default:
		break;
	}

	return ret;
}

static int lg4945_swipe_mode(struct device *dev, u8 lcd_mode)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	struct swipe_info *left = &d->swipe.info[SWIPE_L];
	struct swipe_info *right = &d->swipe.info[SWIPE_R];
	u32 swipe_data[11] = {0x0, };
	int ret = 0;

	if (!d->swipe.mode)
		return ret;

	if (lcd_mode != LCD_MODE_U2) {
		ret = lg4945_swipe_control(dev, SWIPE_DISABLE_CTRL);
		TOUCH_I("swipe disable\n");
	} else {
		swipe_data[0] = d->swipe.mode;
		swipe_data[1] = (right->distance) | (left->distance << 16);
		swipe_data[2] = (right->ratio_thres) | (left->ratio_thres << 16);
		swipe_data[3] = (right->ratio_distance) |
					(left->ratio_distance << 16);
		swipe_data[4] = (right->ratio_period) | (left->ratio_period << 16);
		swipe_data[5] = (right->min_time) | (left->min_time << 16);
		swipe_data[6] = (right->max_time) | (left->max_time << 16);
		swipe_data[7] = (right->area.x1) | (left->area.x1 << 16);
		swipe_data[8] = (right->area.y1) | (left->area.y1 << 16);
		swipe_data[9] = (right->area.x2) | (left->area.x2 << 16);
		swipe_data[10] = (right->area.y2) | (left->area.y2 << 16);

		ret = lg4945_reg_write(dev, SWIPE_ENABLE_W,
			&swipe_data[0], sizeof(swipe_data));

		TOUCH_I("swipe enable\n");
	}

	return ret;
}

#define CLK_ON			0x98
#define CLK_OFF			0x90
#define OSC_ON			0x88
#define OSC_OFF			0x80

static int lg4945_clock(struct device *dev, bool onoff)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);

	if (onoff) {
		lg4945_cmd_write(dev, OSC_ON);
		lg4945_cmd_write(dev, CLK_ON);
		atomic_set(&ts->state.sleep, IC_NORMAL);
	} else {
		if (d->lcd_mode == LCD_MODE_U0) {
			lg4945_cmd_write(dev, CLK_OFF);
			lg4945_cmd_write(dev, OSC_OFF);
			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
		}
	}

	TOUCH_I("lg4945_clock -> %s\n",
		onoff ? "ON" : d->lcd_mode == 0 ? "OFF" : "SKIP");

	return 0;
}

int lg4945_tc_driving(struct device *dev, int mode)
{
	u32 ctrl = 0;
	u8 rdata;

	switch (mode) {
	case LCD_MODE_U0:
		ctrl = 0x01;
		break;

	case LCD_MODE_U1:
		ctrl = 0x81;
		break;

	case LCD_MODE_U2:
		ctrl = 0x101;
		break;

	case LCD_MODE_U3:
		ctrl = 0x181;
		break;

	case LCD_MODE_U3_PARTIAL:
		ctrl = 0x381;
		break;

	case LCD_MODE_U3_QUICKCOVER:
		ctrl = 0x581;
		break;

	case LCD_MODE_STOP:
		ctrl = 0x02;
		break;
	}

	/* swipe set */
	lg4945_swipe_mode(dev, mode);

	TOUCH_I("lg4945_tc_driving = %d\n", mode);
	lg4945_reg_read(dev, 0xD015, (u8 *)&rdata, sizeof(u32));
	TOUCH_I("IC Mode = %d\n", rdata >> 5 & 0x03);
	lg4945_reg_write(dev, tc_drive_ctl, &ctrl, sizeof(ctrl));
	touch_msleep(20);

	return 0;
}

static void lg4945_deep_sleep(struct device *dev)
{
	lg4945_tc_driving(dev, LCD_MODE_STOP);
	lg4945_clock(dev, 0);
}

static void lg4945_debug_tci(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 debug_reason_buf[TCI_MAX_NUM][TCI_DEBUG_MAX_NUM];
	u32 rdata[9] = {0, };
	u8 count[2] = {0, };
	u8 count_max = 0;
	u32 i, j = 0;
	u8 buf = 0;

	if (!d->tci_debug_type)
		return;

	lg4945_reg_read(dev, TCI_DEBUG_R, &rdata, sizeof(rdata));

	count[TCI_1] = (rdata[0] & 0xFFFF);
	count[TCI_2] = ((rdata[0] >> 16) & 0xFFFF);
	count_max = (count[TCI_1] > count[TCI_2]) ? count[TCI_1] : count[TCI_2];

	if (count_max == 0)
		return;

	if (count_max > TCI_DEBUG_MAX_NUM) {
		count_max = TCI_DEBUG_MAX_NUM;
		if (count[TCI_1] > TCI_DEBUG_MAX_NUM)
			count[TCI_1] = TCI_DEBUG_MAX_NUM;
		if (count[TCI_2] > TCI_DEBUG_MAX_NUM)
			count[TCI_2] = TCI_DEBUG_MAX_NUM;
	}

	for (i = 0; i < ((count_max-1)/4)+1; i++) {
		memcpy(&debug_reason_buf[TCI_1][i*4], &rdata[i+1], sizeof(u32));
		memcpy(&debug_reason_buf[TCI_2][i*4], &rdata[i+5], sizeof(u32));
	}

	TOUCH_I("TCI count_max = %d\n", count_max);
	for (i = 0; i < TCI_MAX_NUM; i++) {
		TOUCH_I("TCI count[%d] = %d\n", i, count[i]);
		for (j = 0; j < count[i]; j++) {
			buf = debug_reason_buf[i][j];
			TOUCH_I("TCI_%d - DBG[%d/%d]: %s\n",
				i + 1, j + 1, count[i],
				(buf > 0 && buf < TCI_FAIL_NUM) ?
					tci_debug_str[buf] :
					tci_debug_str[0]);
		}
	}
}

static void lg4945_debug_swipe(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 debug_reason_buf[SWIPE_MAX_NUM][SWIPE_DEBUG_MAX_NUM];
	u32 rdata[5] = {0 , };
	u8 count[2] = {0, };
	u8 count_max = 0;
	u32 i, j = 0;
	u8 buf = 0;

	if (!d->swipe_debug_type)
		return;

	lg4945_reg_read(dev, SWIPE_DEBUG_R, &rdata, sizeof(rdata));

	count[SWIPE_R] = (rdata[0] & 0xFFFF);
	count[SWIPE_L] = ((rdata[0] >> 16) & 0xFFFF);
	count_max = (count[SWIPE_R] > count[SWIPE_L]) ?
			count[SWIPE_R] : count[SWIPE_L];

	if (count_max == 0)
		return;

	if (count_max > SWIPE_DEBUG_MAX_NUM) {
		count_max = SWIPE_DEBUG_MAX_NUM;
		if (count[SWIPE_R] > SWIPE_DEBUG_MAX_NUM)
			count[SWIPE_R] = SWIPE_DEBUG_MAX_NUM;
		if (count[SWIPE_L] > SWIPE_DEBUG_MAX_NUM)
			count[SWIPE_L] = SWIPE_DEBUG_MAX_NUM;
	}

	for (i = 0; i < ((count_max-1)/4)+1; i++) {
		memcpy(&debug_reason_buf[SWIPE_R][i*4], &rdata[i+1], sizeof(u32));
		memcpy(&debug_reason_buf[SWIPE_L][i*4], &rdata[i+3], sizeof(u32));
	}

	for (i = 0; i < SWIPE_MAX_NUM; i++) {
		for (j = 0; j < count[i]; j++) {
			buf = debug_reason_buf[i][j];
			TOUCH_I("SWIPE_%s - DBG[%d/%d]: %s\n",
				i == SWIPE_R ? "Right" : "Left",
				j + 1, count[i],
				(buf > 0 && buf < SWIPE_FAIL_NUM) ?
					swipe_debug_str[buf] :
					swipe_debug_str[0]);
		}
	}
}


static int lg4945_lpwg_mode(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		if (ts->role.mfts_lpwg) {
			lg4945_lpwg_control(dev, LPWG_DOUBLE_TAP);
			lg4945_tc_driving(dev, d->lcd_mode);
			return 0;
		}
		if (ts->lpwg.mode == LPWG_NONE) {
			/* deep sleep */
			TOUCH_I("suspend ts->lpwg.mode == LPWG_NONE\n");
			lg4945_deep_sleep(dev);
		} else if (ts->lpwg.screen) {
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
				lg4945_clock(dev, 1);

			TOUCH_I("Skip lpwg_mode\n");
#if defined(CONFIG_MACH_MSM8996_ELSA)
			//lg4945_debug_tci(dev);
			//lg4945_debug_swipe(dev);
#else
			lg4945_debug_tci(dev);
			lg4945_debug_swipe(dev);
#endif
		} else if (ts->lpwg.qcover == HALL_NEAR) {
			/* knock on/code disable */
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
				lg4945_clock(dev, 1);

			lg4945_lpwg_control(dev, LPWG_NONE);
			lg4945_tc_driving(dev, d->lcd_mode);
		} else {
			/* knock on/code */
#if defined(CONFIG_MACH_MSM8996_ELSA)
	//		lg4945_deep_sleep(dev);
#else
			lg4945_deep_sleep(dev);
#endif

#if defined(CONFIG_MACH_MSM8996_ELSA)
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
				lg4945_clock(dev, 1);

			lg4945_lpwg_control(dev, ts->lpwg.mode);
			lg4945_tc_driving(dev, d->lcd_mode);
#endif
		}
		return 0;
	}

	/* resume */
	if (ts->lpwg.screen) {
		/* normal */
		TOUCH_I("resume ts->lpwg.screen\n");
		lg4945_lpwg_control(dev, LPWG_NONE);
		lg4945_tc_driving(dev, d->lcd_mode);
	} else if (ts->lpwg.mode == LPWG_NONE) {
		/* wake up */
		TOUCH_I("resume ts->lpwg.mode == LPWG_NONE\n");
		lg4945_tc_driving(dev, LCD_MODE_STOP);
	} else {
		/* partial */
		TOUCH_I("resume Partial\n");
		if (ts->lpwg.qcover == HALL_NEAR)
			lg4945_lpwg_control(dev, LPWG_NONE);
		else
			lg4945_lpwg_control(dev, ts->lpwg.mode);
		lg4945_tc_driving(dev, LCD_MODE_U3_PARTIAL);
	}

	return 0;
}

static int lg4945_lpwg(struct device *dev, u32 code, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int *value = (int *)param;

	switch (code) {
	case LPWG_ACTIVE_AREA:
		ts->tci.area.x1 = value[0];
		ts->tci.area.x2 = value[1];
		ts->tci.area.y1 = value[2];
		ts->tci.area.y2 = value[3];
		TOUCH_I("LPWG_ACTIVE_AREA: x0[%d], x1[%d], x2[%d], x3[%d]\n",
			value[0], value[1], value[2], value[3]);
		break;

	case LPWG_TAP_COUNT:
		ts->tci.info[TCI_2].tap_count = value[0];
		break;

	case LPWG_DOUBLE_TAP_CHECK:
		ts->tci.double_tap_check = value[0];
		break;

	case LPWG_UPDATE_ALL:
		ts->lpwg.mode = value[0];
		ts->lpwg.screen = value[1];
		ts->lpwg.sensor = value[2];
		ts->lpwg.qcover = value[3];

		TOUCH_I(
			"LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
			ts->lpwg.mode,
			ts->lpwg.screen ? "ON" : "OFF",
			ts->lpwg.sensor ? "FAR" : "NEAR",
			ts->lpwg.qcover ? "CLOSE" : "OPEN");

		lg4945_lpwg_mode(dev);

		break;

	case LPWG_REPLY:
		break;

	}

	return 0;
}

static void lg4945_connect(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int charger_state = atomic_read(&ts->state.connect);
	int wireless_state = atomic_read(&ts->state.wireless);

	TOUCH_TRACE();

	d->charger = 0;
	/* wire */
	if (charger_state == CONNECT_INVALID)
		d->charger = CONNECT_NONE;
	else if ((charger_state == CONNECT_DCP)
			|| (charger_state == CONNECT_PROPRIETARY))
		d->charger = CONNECT_TA;
	else if (charger_state == CONNECT_HUB)
		d->charger = CONNECT_OTG;
	else
		d->charger = CONNECT_USB;

	/* wireless */
	if (wireless_state)
		d->charger = d->charger | CONNECT_WIRELESS;

	TOUCH_I("%s: write charger_state = 0x%02X\n", __func__, d->charger);
	if (atomic_read(&ts->state.pm) > DEV_PM_RESUME) {
		TOUCH_I("DEV_PM_SUSPEND - Don't try SPI\n");
		return;
	}

	lg4945_reg_write(dev, SPR_CHARGER_STS, &d->charger, sizeof(u32));
}

static void lg4945_lcd_mode(struct device *dev, u32 mode)
{
	struct lg4945_data *d = to_lg4945_data(dev);

	TOUCH_I("lcd_mode: %d (prev: %d)\n", mode, d->lcd_mode);

	d->lcd_mode = mode;

	if (lg4945_get_u3fake(dev) &&
		(mode == LCD_MODE_U2 || mode == LCD_MODE_U0)) {
		lg4945_set_u3fake(dev,0);
	}
}

static int lg4945_usb_status(struct device *dev, u32 mode)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	TOUCH_I("TA Type: %d\n", atomic_read(&ts->state.connect));
	lg4945_connect(dev);
	return 0;
}

static int lg4945_wireless_status(struct device *dev, u32 onoff)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	TOUCH_I("Wireless charger: 0x%02X\n", atomic_read(&ts->state.wireless));
	lg4945_connect(dev);
	return 0;
}

static int lg4945_notify(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	switch (event) {
	case NOTIFY_TOUCH_RESET:
		TOUCH_I("NOTIFY_TOUCH_RESET!\n");
		if (lg4945_get_u3fake(dev)) {
			TOUCH_I("U3_FAKE_MODE! - Ignore Reset\n");
			ret = NOTIFY_STOP;
			break;
		} else if (atomic_read(&ts->state.fb) == FB_RESUME) {
			TOUCH_I("FB_RESUME state - Ignore Reset\n");
			ret = NOTIFY_STOP;
			break;
		}
		TOUCH_I("NOTIFY_TOUCH_RESET!\n");
		atomic_set(&d->watch.state.font_mem, EMPTY);
		atomic_set(&d->block_watch_cfg, 1);
		break;
	case LCD_EVENT_LCD_MODE:
		TOUCH_I("LCD_EVENT_LCD_MODE!\n");
		lg4945_lcd_mode(dev, *(u32 *)data);
		break;
	case NOTIFY_CONNECTION:
		TOUCH_I("NOTIFY_CONNECTION!\n");
		ret = lg4945_usb_status(dev, *(u32 *)data);
		break;
	case NOTIFY_WIRELEES:
		TOUCH_I("NOTIFY_WIRELEES!\n");
		ret = lg4945_wireless_status(dev, *(u32 *)data);
		break;
	case NOTIFY_IME_STATE:
		TOUCH_I("NOTIFY_IME_STATE!\n");
		ret = lg4945_reg_write(dev, REG_IME_STATE,
			(u32*)data, sizeof(u32));
		break;
	default:
		TOUCH_E("%lu is not supported\n", event);
		break;
	}

	return ret;
}

static void lg4945_init_works(struct lg4945_data *d)
{
	INIT_DELAYED_WORK(&d->font_download_work, lg4945_ext_watch_font_download_func);
}

static void lg4945_init_locks(struct lg4945_data *d)
{
	mutex_init(&d->spi_lock);
}

static int lg4945_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = NULL;

	TOUCH_TRACE();

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);

	if (!d) {
		TOUCH_E("failed to allocate synaptics data\n");
		return -ENOMEM;
	}

	d->dev = dev;
	touch_set_device(ts, d);

	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 0);

	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	touch_gpio_init(ts->maker_id_pin, "touch_make_id");
	touch_gpio_direction_input(ts->maker_id_pin);

	touch_power_init(dev);
	touch_bus_init(dev, MAX_BUF_SIZE);

	lg4945_init_works(d);
	lg4945_init_locks(d);

	if (touch_boot_mode() == TOUCH_CHARGER_MODE) {
		touch_gpio_init(ts->reset_pin, "touch_reset");
		touch_gpio_direction_output(ts->reset_pin, 1);
		/* Deep Sleep */
		lg4945_deep_sleep(dev);
		return 0;
	}

	lg4945_get_tci_info(dev);
	lg4945_get_swipe_info(dev);

	d->lcd_mode = LCD_MODE_U3;
	d->tci_debug_type = 1;
	lg4945_sic_abt_probe();

	return 0;
}

static int lg4945_remove(struct device *dev)
{
	TOUCH_TRACE();
	lg4945_sic_abt_remove();
	lg4945_watch_remove(dev);

	return 0;
}

static int lg4945_fw_compare(struct device *dev, const struct firmware *fw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	u8 dev_major = d->fw.version[0];
	u8 dev_minor = d->fw.version[1];
	u32 bin_ver_offset = *((u32 *)&fw->data[0xb0]);
	u32 bin_pid_offset = *((u32 *)&fw->data[0xb8]);
	char pid[12];
	u8 bin_major;
	u8 bin_minor;
	int update = 0;

	bin_major = fw->data[bin_ver_offset];
	bin_minor = fw->data[bin_ver_offset + 1];
	memcpy(pid, &fw->data[bin_pid_offset], 8);
	pid[8] = '\0';

	if (ts->force_fwup) {
		update = 1;
	} else if (bin_major && dev_major) {
		if (bin_minor != dev_minor)
			update = 1;
	} else if (bin_major ^ dev_major) {
		update = 1;
	} else if (!bin_major && !dev_major) {
		if (bin_minor > dev_minor)
			update = 1;
	}
	TOUCH_I(
		"bin-ver: %d.%02d (%s), dev-ver: %d.%02d -> update: %d, force_fwup: %d\n",
		bin_major, bin_minor, pid, dev_major, dev_minor,
		update, ts->force_fwup);

	return update;
}

static int lg4945_condition_wait(struct device *dev,
				    u16 addr, u32 *value, u32 expect,
				    u32 mask, u32 delay, u32 retry)
{
	u32 data = 0;

	do {
		touch_msleep(delay);
		lg4945_read_value(dev, addr, &data);

		if ((data & mask) == expect) {
			if (value)
				*value = data;
			TOUCH_I(
				"%d, addr[%04x] data[%08x], mask[%08x], expect[%08x]\n",
				retry, addr, data, mask, expect);
			return 0;
		}
	} while (--retry);

	if (value)
		*value = data;

	TOUCH_I("%s addr[%04x], expect[%x], mask[%x], data[%x]\n",
		__func__, addr, expect, mask, data);

	return -EPERM;
}

#define BLCODE_START			0
#define FLASH_CFG_SECTOR		68
#define FLASH_CFG_SIZE			(1 * 1024)
#define FLASH_FW_SIZE			(67 * 1024)
#define FLASH_DL_SIZE			(16 * 1024)

static int lg4945_fw_upgrade(struct device *dev,
			     const struct firmware *fw)
{
	u8 *fwdata = (u8 *) fw->data;
	u32 data;
	u32 size;
	int offset;
	int ret;

	TOUCH_I("%s - START\n", __func__);
	/* CM3 hold */
	lg4945_write_value(dev, SYS_RST_CTL, 2);

	/* sram write enable */
	lg4945_write_value(dev, SYS_SDRAM_CTL, 1);

	/* down load bl for fast write */
	lg4945_reg_write(dev, 0, lg4945_bl_code_v1, sizeof(lg4945_bl_code_v1));

	/* boot done */
	lg4945_write_value(dev, SYS_BOOT_CTL, 1);

	/* CM3 release */
	lg4945_write_value(dev, SYS_RST_CTL, 0);

	/* bl_code done check */
	ret = lg4945_condition_wait(dev, FLASH_BOOTCHK, NULL,
				    FLASH_BOOTCHK_VALUE, 0xFFFFFFFF, 10, 200);
	if (ret < 0) {
		TOUCH_E("failed : \'boot check\'\n");
		return -EPERM;
	}
	/* erase flash */
	lg4945_write_value(dev, FLASH_CTRL, 2);

	ret = lg4945_condition_wait(dev, FLASH_STS, NULL, 0, 1, 10, 100);
	if (ret < 0) {
		TOUCH_E("failed : \'erase\'\n");
		return -EPERM;
	}

	for (offset = 0; offset <= FLASH_FW_SIZE; offset += FLASH_DL_SIZE) {
		if ((offset + FLASH_DL_SIZE) < FLASH_FW_SIZE)
			size = FLASH_DL_SIZE;
		else
			size = FLASH_FW_SIZE - offset;

		/* destination */
		lg4945_write_value(dev, FLASH_DEST, offset);

		/* data write to sram */
		lg4945_reg_write(dev, FLASH_START, &fwdata[offset], size);

		/* transfer to flash */
		lg4945_write_value(dev, FLASH_CTRL, (size << 8) | 1);

		/* busy check */
		ret = lg4945_condition_wait(dev, FLASH_STS,
					    NULL, 0, 1, 10, 100);
		if (ret < 0) {
			TOUCH_E("failed : \'write\'\n");
			return -EPERM;
		}
	}

	/* config download */
	lg4945_write_value(dev, FLASH_DEST, FLASH_CFG_SECTOR * 1024);

	/* data write to sram */
	lg4945_reg_write(dev, FLASH_START, &fwdata[FLASH_FW_SIZE],
			 FLASH_CFG_SIZE);

	/* transfer to flash */
	lg4945_write_value(dev, FLASH_CTRL, (size << 8) | 1);

	/* busy check */
	lg4945_condition_wait(dev, FLASH_STS, NULL, 0, 1, 10, 100);
	if (ret < 0) {
		TOUCH_E("failed : \'write\'\n");
		return -EPERM;
	}

	/* crc check */
	lg4945_write_value(dev, SYS_CRC_CTL, 7);

	ret = lg4945_condition_wait(dev, SYS_CRC_STS, &data, 0, 1, 10, 100);
	if (ret < 0) {
		TOUCH_E("failed : \'crc\'\n");
		return -EPERM;
	}

	/* release CM3 */
	lg4945_write_value(dev, SYS_RST_CTL, 0);

	TOUCH_I("===== Firmware download Okay =====\n");
	TOUCH_I(" Code CRC: %d, Config CRC: %d\n",
		(data >> 1) & 1, (data >> 2) & 1);
	TOUCH_I("==================================\n");

	return 0;
}

static int lg4945_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	const struct firmware *fw = NULL;
	char fwpath[256] = {0};
	int ret = 0;
	int i = 0;

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		return -EPERM;
	}

	if (ts->test_fwpath[0]) {
		memcpy(fwpath, &ts->test_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from test_fwpath:%s\n",
			&ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
		memcpy(fwpath, ts->def_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from def_fwpath : %s\n", fwpath);
	} else {
		TOUCH_E("no firmware file\n");
		return -EPERM;
	}

	if (fwpath == NULL) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	TOUCH_I("fwpath[%s]\n", fwpath);

	ret = request_firmware(&fw, fwpath, dev);

	if (ret < 0) {
		TOUCH_E("fail to request_firmware fwpath: %s (ret:%d)\n",
			fwpath, ret);

		return ret;
	}

	TOUCH_I("fw size:%zu, data: %p\n", fw->size, fw->data);

	if (lg4945_fw_compare(dev, fw)) {
		ret = -EINVAL;
		touch_msleep(200);
		for (i = 0; i < 2 && ret; i++)
			ret = lg4945_fw_upgrade(dev, fw);
	} else {
		release_firmware(fw);
		return -EPERM;
	}

	release_firmware(fw);

	return ret;
}

static int lg4945_suspend(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	/*
		struct lg4945_data *d = to_lg4945_data(dev);
	*/
	int mfts_mode = 0;
	TOUCH_TRACE();

	if (touch_boot_mode() == TOUCH_CHARGER_MODE)
		return -EPERM;

	mfts_mode = touch_boot_mode_check(dev);
	if ((mfts_mode >= MINIOS_MFTS_FOLDER) && !ts->role.mfts_lpwg) {
		TOUCH_I("%s : touch_suspend - MFTS\n", __func__);
		lg4945_power(dev, POWER_OFF);
		return -EPERM;
	} else {
		TOUCH_I("%s : touch_suspend start\n", __func__);
	}

	/*
		d->lcd_mode = 0; 
	*/
	lg4945_lpwg_mode(dev);
	return 0;
}

static int lg4945_resume(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int mfts_mode = 0;

	TOUCH_TRACE();

	mfts_mode = touch_boot_mode_check(dev);
	if ((mfts_mode >= MINIOS_MFTS_FOLDER) && !ts->role.mfts_lpwg) {
		lg4945_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
		lg4945_ic_info(dev);
		if (lg4945_upgrade(dev) == 0) {
			lg4945_power(dev, POWER_OFF);
			lg4945_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
		}
	}
	if (touch_boot_mode() == TOUCH_CHARGER_MODE) {
		lg4945_deep_sleep(dev);
		return -EPERM;
	} else if (lg4945_get_u3fake(dev)) {
		TOUCH_I("RESUME SKIP (U3FAKE)\n");
		lg4945_clock(dev,1);
		return 0;
	}

	d->lcd_mode = 3;

	return 0;
}

static int lg4945_init(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	u32 data = 1;
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_I("%s: charger_state = 0x%02X\n", __func__, d->charger);

	lg4945_sic_abt_init(dev);
	lg4945_ic_info(dev);

	ret = lg4945_reg_write(dev, tc_device_ctl, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'tc_device_ctrl\', ret:%d\n", ret);

	ret = lg4945_reg_write(dev, tc_interrupt_ctl, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'tc_interrupt_ctrl\', ret:%d\n", ret);

	data = 2;
	ret = lg4945_reg_write(dev, SPR_SPI_ACCESS, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'SPR_SPI_ACCESS\', ret:%d\n", ret);

	ret = lg4945_reg_write(dev, SPR_CHARGER_STS, &d->charger, sizeof(u32));
	if (ret)
		TOUCH_E("failed to write \'spr_charger_sts\', ret:%d\n", ret);

	lg4945_lpwg_mode(dev);

	if (ret)
		TOUCH_E("failed to lpwg_control, ret:%d\n", ret);

	atomic_set(&d->block_watch_cfg, 0);

	ret = lg4945_watch_init(dev);

	if (ret)
		TOUCH_E("failed to init watch cfg, ret:%d\n", ret);

	ret = lg4945_ext_watch_check_font_download(d->dev);
	if (ret)
		TOUCH_I("skip font download, ret:%d\n", ret);

	return 0;
}

static int lg4945_check_status(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;
	u32 status = d->info.device_status;

	if (!(status & (1 << 5))) {
		ret = -ERANGE;
	} else if (status & (1 << 10)) {
		TOUCH_I("ESD Error Detected\n");
		ret = -ERANGE;
	}

	return ret;
}

static int lg4945_irq_abs_data(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	struct lg4945_touch_data *data = d->info.data;
	struct touch_data *tdata;
	u32 touch_count = 0;
	u8 finger_index = 0;
	int ret = 0;
	int i = 0;

	touch_count = d->info.touch_cnt;
	ts->new_mask = 0;

	/* check if palm detected */
	if (data[0].track_id == PALM_ID) {
		if (data[0].event == TOUCHSTS_DOWN) {
			ts->is_cancel = 1;
			TOUCH_I("Palm Detected\n");
		} else if (data[0].event == TOUCHSTS_UP) {
			ts->is_cancel = 0;
			TOUCH_I("Palm Released\n");
		}
		ts->tcount = 0;
		ts->intr_status = TOUCH_IRQ_FINGER;
		return ret;
	}

	for (i = 0; i < touch_count; i++) {
		if (data[i].track_id >= MAX_FINGER)
			continue;

		if (data[i].event == TOUCHSTS_DOWN
			|| data[i].event == TOUCHSTS_MOVE) {
			ts->new_mask |= (1 << data[i].track_id);
			tdata = ts->tdata + data[i].track_id;

			tdata->id = data[i].track_id;
			tdata->type = data[i].tool_type;
			tdata->x = data[i].x;
			tdata->y = data[i].y;
			tdata->pressure = data[i].pressure;
			tdata->width_major = data[i].width_major;
			tdata->width_minor = data[i].width_minor;

			if (data[i].width_major == data[i].width_minor)
				tdata->orientation = 1;
			else
				tdata->orientation = data[i].angle;

			finger_index++;

			TOUCH_D(ABS,
				"tdata [id:%d t:%d x:%d y:%d z:%d-%d,%d,%d]\n",
					tdata->id,
					tdata->type,
					tdata->x,
					tdata->y,
					tdata->pressure,
					tdata->width_major,
					tdata->width_minor,
					tdata->orientation);
		}
	}

	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

	return ret;
}

static int lg4945_irq_abs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);

	/* check if touch cnt is valid */
	if (d->info.touch_cnt == 0 || d->info.touch_cnt > ts->caps.max_id)
		return -ERANGE;

	return lg4945_irq_abs_data(dev);
}

static int lg4945_irq_lpwg(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	if (d->info.wakeup_type == KNOCK_1) {
		if (ts->lpwg.mode != LPWG_NONE) {
			lg4945_get_tci_data(dev,
				ts->tci.info[TCI_1].tap_count);
			ts->intr_status = TOUCH_IRQ_KNOCK;
		}
	} else if (d->info.wakeup_type == KNOCK_2) {
		if (ts->lpwg.mode == LPWG_PASSWORD) {
			lg4945_get_tci_data(dev,
				ts->tci.info[TCI_2].tap_count);
			ts->intr_status = TOUCH_IRQ_PASSWD;
		}
	} else if (d->info.wakeup_type == SWIPE_LEFT) {
		TOUCH_I("SWIPE_LEFT\n");
		lg4945_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_SWIPE_LEFT;
	} else if (d->info.wakeup_type == SWIPE_RIGHT) {
		TOUCH_I("SWIPE_RIGHT\n");
		lg4945_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_SWIPE_RIGHT;
	} else if (d->info.wakeup_type == KNOCK_OVERTAP) {
		TOUCH_I("LPWG wakeup_type is Overtap\n");
		lg4945_get_tci_data(dev, 1);
		ts->intr_status = TOUCH_IRQ_PASSWD;
	} else if (d->info.wakeup_type == CUSTOM_DEBUG) {
		TOUCH_I("LPWG wakeup_type is CUSTOM_DEBUG\n");
		lg4945_debug_tci(dev);
		lg4945_debug_swipe(dev);
	} else {
		TOUCH_I("LPWG wakeup_type is not support type![%d]\n",
			d->info.wakeup_type);
	}

	return ret;
}

#if USE_ABT_MONITOR_APP
static int lg4945_irq_handler(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;
	u8 all_data[260];

	int report_mode = sic_abt_is_set_func();
	if (lg4945_reg_read(dev, tc_status,
			    &all_data[0], sizeof(all_data)) < 0) {
		TOUCH_I("report data reg addr read fail\n");
		goto error;
	}

	memcpy(&d->info, all_data, sizeof(d->info));

	ret = lg4945_check_status(dev);
	if (ret < 0)
		goto error;

	if (report_mode) {
		if (d->info.wakeup_type == ABS_MODE)
			ret = lg4945_irq_abs(dev);
		else
			ret = lg4945_irq_lpwg(dev);

		lg4945_sic_abt_report_mode(dev, &all_data[4]);
	} else {
		if (d->info.wakeup_type == ABS_MODE) {
			if (lg4945_sic_abt_is_debug_mode()) {
				lg4945_sic_abt_onchip_debug(dev, &all_data[4]);
				ret = lg4945_irq_abs(dev);
			} else {
				lg4945_sic_abt_ocd_off(dev);
				ret = lg4945_irq_abs(dev);
			}
		} else {
			ret = lg4945_irq_lpwg(dev);
		}
	}
error:
	return ret;
}
#else
static int lg4945_irq_handler(struct device *dev)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;

	lg4945_reg_read(dev, tc_status, &d->info, sizeof(d->info));

	ret = lg4945_check_status(dev);
	if (ret < 0)
		goto error;

	if (d->info.wakeup_type == ABS_MODE)
		ret = lg4945_irq_abs(dev);
	else
		ret = lg4945_irq_lpwg(dev);
error:
	return ret;
}
#endif

static ssize_t store_reg_ctrl(struct device *dev,
				const char *buf, size_t count)
{
	char command[6] = {0};
	u32 reg = 0;
	int value = 0;
	u32 data = 1;
	u16 reg_addr;

	if (sscanf(buf, "%5s %x %x", command, &reg, &value) <= 0)
		return count;

	reg_addr = reg;
	if (!strcmp(command, "write")) {
		data = value;
		if (lg4945_reg_write(dev, reg_addr, &data, sizeof(u32)) < 0)
			TOUCH_E("reg addr 0x%x write fail\n", reg_addr);
		else
			TOUCH_I("reg[%x] = 0x%x\n", reg_addr, data);
	} else if (!strcmp(command, "read")) {
		if (lg4945_reg_read(dev, reg_addr, &data, sizeof(u32)) < 0)
			TOUCH_E("reg addr 0x%x read fail\n", reg_addr);
		else
			TOUCH_I("reg[%x] = 0x%x\n", reg_addr, data);
	} else {
		TOUCH_D(BASE_INFO, "Usage\n");
		TOUCH_D(BASE_INFO, "Write reg value\n");
		TOUCH_D(BASE_INFO, "Read reg\n");
	}
	return count;
}

static ssize_t show_u3fake(struct device *dev, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		lg4945_get_u3fake(dev));

	return ret;
}

static ssize_t store_u3fake(struct device *dev,
				const char *buf, size_t count)
{
	u8 is_u3fake = 0;

	if (sscanf(buf, "%d", (int *)&is_u3fake) <= 0)
		return count;

	TOUCH_I("u3fake = %d\n", (int)is_u3fake);

	lg4945_set_u3fake(dev, is_u3fake);

	return count;
}

static ssize_t show_tci_debug(struct device *dev, char *buf)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;
	u32 rdata = -1;

	if (lg4945_reg_read(dev, TCI_FAIL_DEBUG_R,
				(u8 *)&rdata, sizeof(rdata)) < 0) {
		TOUCH_I("Fail to Read TCI Debug Reason type\n");
		return ret;
	}

	ret = snprintf(buf + ret, PAGE_SIZE,
			"Read TCI Debug Reason type[IC] = %s\n",
			debug_type[(rdata & 0x8) ? 2 :
					(rdata & 0x4 ? 1 : 0)]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Read TCI Debug Reason type[Driver] = %s\n",
			debug_type[d->tci_debug_type]);
	TOUCH_I("Read TCI Debug Reason type = %s\n",
			debug_type[d->tci_debug_type]);

	return ret;
}

static ssize_t store_tci_debug(struct device *dev,
						const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value > 2 || value < 0) {
		TOUCH_I("SET TCI debug reason wrong, 0, 1, 2 only\n");
		return count;
	}

	d->tci_debug_type = (u8)value;
	TOUCH_I("SET TCI Debug reason type = %s\n", debug_type[value]);

	return count;
}

static ssize_t show_swipe_debug(struct device *dev, char *buf)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int ret = 0;
	u32 rdata = -1;

	if (lg4945_reg_read(dev, SWIPE_FAIL_DEBUG_R,
				(u8 *)&rdata, sizeof(rdata)) < 0) {
		TOUCH_I("Fail to Read SWIPE Debug reason type\n");
		return ret;
	}

	ret = snprintf(buf + ret, PAGE_SIZE,
			"Read SWIPE Debug reason type[IC] = %s\n",
			debug_type[rdata]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Read SWIPE Debug reason type[Driver] = %s\n",
			debug_type[d->swipe_debug_type]);
	TOUCH_I("Read SWIPE Debug reason type = %s\n",
			debug_type[d->swipe_debug_type]);

	return ret;
}

static ssize_t store_swipe_debug(struct device *dev,
						const char *buf, size_t count)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value > 2 || value < 0) {
		TOUCH_I("SET SWIPE debug reason wrong, 0, 1, 2 only\n");
		return count;
	}

	d->swipe_debug_type = (u8)value;
	TOUCH_I("Write SWIPE Debug reason type = %s\n", debug_type[value]);

	return count;
}

static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(u3fake, show_u3fake, store_u3fake);
static TOUCH_ATTR(tci_debug, show_tci_debug, store_tci_debug);
static TOUCH_ATTR(swipe_debug, show_swipe_debug, store_swipe_debug);

static struct attribute *lg4945_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_u3fake.attr,
	&touch_attr_tci_debug.attr,
	&touch_attr_swipe_debug.attr,
	NULL,
};

static const struct attribute_group lg4945_attribute_group = {
	.attrs = lg4945_attribute_list,
};

static int lg4945_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &lg4945_attribute_group);
	if (ret < 0)
		TOUCH_E("lg4945 sysfs register failed\n");

	lg4945_watch_register_sysfs(dev);
	lg4945_prd_register_sysfs(dev);
	lg4945_sic_abt_register_sysfs(&ts->kobj);

	return 0;
}

static int lg4945_get_cmd_version(struct device *dev, char *buf)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int offset = 0;
	int ret = 0;
	u32 rdata[4] = {0};

	ret = lg4945_ic_info(dev);
	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Read Fail Touch IC Info\n");
		return offset;
	}

	offset = snprintf(buf + offset, PAGE_SIZE - offset, "version : v%d.%02d\n",
		d->fw.version[0], d->fw.version[1]);

	if (d->fw.revision == 0xFF) {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"revision : Flash Erased(0xFF)\n");
	} else {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"revision : %d\n", d->fw.revision);
	}

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"product id : [%s]\n\n", d->fw.product_id);

	lg4945_reg_read(dev, info_lot_num, (u8 *)&rdata, sizeof(rdata));
	offset += snprintf(buf + offset, PAGE_SIZE - offset, "lot : %d\n", rdata[0]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset, "serial : 0x%X\n", rdata[1]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset, "date : 0x%X 0x%X\n",
		rdata[2], rdata[3]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset, "date : %04d.%02d.%02d " \
		"%02d:%02d:%02d Site%d\n",
		rdata[2] & 0xFFFF, (rdata[2] >> 16 & 0xFF), (rdata[2] >> 24 & 0xFF),
		rdata[3] & 0xFF, (rdata[3] >> 8 & 0xFF), (rdata[3] >> 16 & 0xFF),
		(rdata[3] >> 24 & 0xFF));

	return offset;
}

static int lg4945_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct lg4945_data *d = to_lg4945_data(dev);
	int offset = 0;
	int ret = 0;

	ret = lg4945_ic_info(dev);
	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Read Fail Touch IC Info\n");
		return offset;
	}

	offset = snprintf(buf, PAGE_SIZE, "v%d.%02d\n",
		d->fw.version[0], d->fw.version[1]);

	return offset;
}

static int lg4945_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

static int lg4945_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_D(BASE_INFO, "%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = lg4945_get_cmd_version(dev, (char *)output);
		break;

	case CMD_ATCMD_VERSION:
		ret = lg4945_get_cmd_atcmd_version(dev, (char *)output);
		break;

	default:
		break;
	}

	return ret;
}

static struct touch_driver touch_driver = {
	.probe = lg4945_probe,
	.remove = lg4945_remove,
	.suspend = lg4945_suspend,
	.resume = lg4945_resume,
	.init = lg4945_init,
	.irq_handler = lg4945_irq_handler,
	.power = lg4945_power,
	.upgrade = lg4945_upgrade,
	.lpwg = lg4945_lpwg,
	.notify = lg4945_notify,
	.register_sysfs = lg4945_register_sysfs,
	.set = lg4945_set,
	.get = lg4945_get,
};

#define MATCH_NAME			"lge,lg4945"

static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
};

static struct touch_hwif hwif = {
	.bus_type = HWIF_SPI,
	.name = "lg4945",
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
	.bits_per_word = 8,
	.spi_mode = SPI_MODE_0,
	.max_freq = (5 * 1000000),
};

static int __init touch_device_init(void)
{
	TOUCH_TRACE();
#if !defined(CONFIG_MACH_MSM8996_ELSA) // LCD Maker ID is not used in ELSA.
	if (touch_get_device_type() != TYPE_LG4945 ) {
		TOUCH_I("%s, lg4945 returned\n", __func__);
		return 0;
	}
#endif
	TOUCH_I("%s, sic4945 start\n", __func__);
	return touch_bus_device_init(&hwif, &touch_driver);
}

static void __exit touch_device_exit(void)
{
	TOUCH_TRACE();
	touch_bus_device_exit(&hwif);
}

module_init(touch_device_init);
module_exit(touch_device_exit);

MODULE_AUTHOR("hoyeon.jang@lge.com");
MODULE_DESCRIPTION("LGE touch driver v3");
MODULE_LICENSE("GPL");
