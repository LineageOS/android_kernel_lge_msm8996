/* touch_synaptics.c
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
#define TS_MODULE "[s3320]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>

/*
 *  Include to Local Header File
 */
#include "touch_s3320.h"
#include "touch_s3320_prd.h"

/*
 * PLG349 - Z2
 * PLG446 - P1(JDI)
 * PLG469 - P1(LGD)
 */

static struct synaptics_exp_fhandler rmidev_fhandler;

bool synaptics_is_product(struct synaptics_data *d,
				const char *product_id, size_t len)
{
	return strncmp(d->fw.product_id, product_id, len)
			? false : true;
}

bool synaptics_is_img_product(struct synaptics_data *d,
				const char *product_id, size_t len)
{
	return strncmp(d->fw.img_product_id, product_id, len)
			? false : true;
}

int synaptics_read(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct touch_bus_msg msg;
	int ret = 0;

	ts->tx_buf[0] = addr;

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = 1;

	msg.rx_buf = ts->rx_buf;
	msg.rx_size = size;

	ret = touch_bus_read(dev, &msg);

	if (ret < 0) {
		TOUCH_E("touch bus read error : %d\n", ret);
		return ret;
	}

	memcpy(data, &ts->rx_buf[0], size);
	return 0;
}

int synaptics_write(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct touch_bus_msg msg;
	int ret = 0;

	ts->tx_buf[0] = addr;
	memcpy(&ts->tx_buf[1], data, size);

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = size+1;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	ret = touch_bus_write(dev, &msg);

	if (ret < 0) {
		TOUCH_E("touch bus write error : %d\n", ret);
		return ret;
	}

	return 0;
}

static int synaptics_irq_enable(struct device *dev, bool enable)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 val;
	int ret;

	ret = synaptics_read(dev, INTERRUPT_ENABLE_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to read interrupt enable - ret:%d\n", ret);
		return ret;
	}

	if (enable)
		val |= (INTERRUPT_MASK_ABS0 | INTERRUPT_MASK_CUSTOM);
	else
		val &= ~INTERRUPT_MASK_ABS0;


	ret = synaptics_write(dev, INTERRUPT_ENABLE_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to write interrupt enable - ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("write interrupt : enable:%d, val:%02X\n", enable, val);

	return 0;
}

int synaptics_set_page(struct device *dev, u8 page)
{
	int ret = synaptics_write(dev, PAGE_SELECT_REG, &page, 1);

	if (ret >= 0)
		to_synaptics_data(dev)->curr_page = page;

	return ret;
}

static int synaptics_get_f12(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 query_5_data[5];
	u8 query_8_data[3];
	u8 ctrl_23_data[2];
	u8 ctrl_8_data[14];

	u32 query_5_present = 0;
	u16 query_8_present = 0;

	u8 offset;
	int i;
	int ret;

	ret = synaptics_read(dev, d->f12.dsc.query_base + 5,
			     query_5_data, sizeof(query_5_data));

	if (ret < 0) {
		TOUCH_E("faied to get query5 (ret: %d)\n", ret);
		return ret;
	}

	query_5_present = (query_5_present << 8) | query_5_data[4];
	query_5_present = (query_5_present << 8) | query_5_data[3];
	query_5_present = (query_5_present << 8) | query_5_data[2];
	query_5_present = (query_5_present << 8) | query_5_data[1];
	TOUCH_I("qeury_5_present=0x%08X [%02X %02X %02X %02X %02X]\n",
			query_5_present, query_5_data[0], query_5_data[1],
			query_5_data[2], query_5_data[3], query_5_data[4]);

	for (i = 0, offset = 0; i < 32; i++) {
		d->f12_reg.ctrl[i] = d->f12.dsc.control_base + offset;

		if (query_5_present & (1 << i)) {
			TOUCH_I("f12_reg.ctrl[%d]=0x%02X (0x%02x+%d)\n",
					i, d->f12_reg.ctrl[i],
					d->f12.dsc.control_base, offset);
			offset++;
		}
	}

	ret = synaptics_read(dev, d->f12.dsc.query_base + 8,
			query_8_data, sizeof(query_8_data));

	if (ret < 0) {
		TOUCH_E("faied to get query8 (ret: %d)\n", ret);
		return ret;
	}

	query_8_present = (query_8_present << 8) | query_8_data[2];
	query_8_present = (query_8_present << 8) | query_8_data[1];
	TOUCH_I("qeury_8_present=0x%08X [%02X %02X %02X]\n",
			query_8_present, query_8_data[0],
			query_8_data[1], query_8_data[2]);

	for (i = 0, offset = 0; i < 16; i++) {
		d->f12_reg.data[i] = d->f12.dsc.data_base + offset;

		if (query_8_present & (1 << i)) {
			TOUCH_I("d->f12_reg.data[%d]=0x%02X (0x%02x+%d)\n",
					i, d->f12_reg.data[i],
					d->f12.dsc.data_base, offset);
			offset++;
		}
	}

	ret = synaptics_read(dev, d->f12_reg.ctrl[23],
			     ctrl_23_data, sizeof(ctrl_23_data));

	if (ret < 0) {
		TOUCH_E("faied to get f12_ctrl32_data (ret: %d)\n", ret);
		return ret;
	}

	d->object_report = ctrl_23_data[0];
	d->num_of_fingers = min_t(u8, ctrl_23_data[1], (u8) MAX_NUM_OF_FINGERS);

	TOUCH_I("object_report[0x%02X], num_of_fingers[%d]\n",
			d->object_report, d->num_of_fingers);

	ret = synaptics_read(dev, d->f12_reg.ctrl[8],
			     ctrl_8_data, sizeof(ctrl_8_data));

	if (ret < 0) {
		TOUCH_E("faied to get f12_ctrl8_data (ret: %d)\n", ret);
		return ret;
	}

	TOUCH_I("ctrl_8-sensor_max_x[%d], sensor_max_y[%d]\n",
			((u16)ctrl_8_data[0] << 0) |
			((u16)ctrl_8_data[1] << 8),
			((u16)ctrl_8_data[2] << 0) |
			((u16)ctrl_8_data[3] << 8));

	return 0;
}

static int synaptics_page_description(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	struct function_descriptor dsc;
	u8 page;

	unsigned short pdt;
	int ret;

	TOUCH_TRACE();

	memset(&d->f01, 0, sizeof(struct synaptics_function));
	memset(&d->f11, 0, sizeof(struct synaptics_function));
	memset(&d->f12, 0, sizeof(struct synaptics_function));
	memset(&d->f1a, 0, sizeof(struct synaptics_function));
	memset(&d->f34, 0, sizeof(struct synaptics_function));
	memset(&d->f51, 0, sizeof(struct synaptics_function));
	memset(&d->f54, 0, sizeof(struct synaptics_function));
	memset(&d->f55, 0, sizeof(struct synaptics_function));

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		ret = synaptics_set_page(dev, page);

		if (ret < 0) {
			TOUCH_E("faied to set page %d (ret: %d)\n", page, ret);
			return ret;
		}

		for (pdt = PDT_START; pdt > PDT_END; pdt -= sizeof(dsc)) {
			ret = synaptics_read(dev, pdt, &dsc, sizeof(dsc));

			if (ret < 0) {
				TOUCH_E("read descrptore %d (ret: %d)\n",
					pdt, ret);
				return ret;
			}

			if (!dsc.fn_number)
				break;

			TOUCH_I("dsc - %02x, %02x, %02x, %02x, %02x, %02x\n",
				dsc.query_base, dsc.command_base,
				dsc.control_base, dsc.data_base,
				dsc.int_source_count, dsc.fn_number);

			switch (dsc.fn_number) {
			case 0x01:
				d->f01.dsc = dsc;
				d->f01.page = page;
				break;

			case 0x11:
				d->f11.dsc = dsc;
				d->f11.page = page;
				break;

			case 0x12:
				d->f12.dsc = dsc;
				d->f12.page = page;
				synaptics_get_f12(dev);
				break;

			case 0x1a:
				d->f1a.dsc = dsc;
				d->f1a.page = page;
				break;

			case 0x34:
				d->f34.dsc = dsc;
				d->f34.page = page;
				break;

			case 0x51:
				d->f51.dsc = dsc;
				d->f51.page = page;
				break;

			case 0x54:
				d->f54.dsc = dsc;
				d->f54.page = page;
				break;

			default:
				break;
			}
		}
	}

	TOUCH_D(BASE_INFO,
		"common[%dP:0x%02x] finger_f12[%dP:0x%02x] flash[%dP:0x%02x] analog[%dP:0x%02x] lpwg[%dP:0x%02x]\n",
		d->f01.page, d->f01.dsc.fn_number,
		d->f12.page, d->f12.dsc.fn_number,
		d->f34.page, d->f34.dsc.fn_number,
		d->f54.page, d->f54.dsc.fn_number,
		d->f51.page, d->f51.dsc.fn_number);

	if (!(d->f01.dsc.fn_number &&
	      d->f12.dsc.fn_number &&
	      d->f34.dsc.fn_number &&
	      d->f54.dsc.fn_number &&
	      d->f51.dsc.fn_number))
		return -EINVAL;

	ret = synaptics_set_page(dev, 0);

	if (ret) {
		TOUCH_E("faied to set page %d (ret: %d)\n", 0, ret);
		return ret;
	}

	return 0;
}

static int synaptics_get_type_bootloader(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;
	u8 temp_pid[11] = {0,};

	ret = synaptics_read(dev, d->f01.dsc.query_base + 11,
			d->fw.product_id, sizeof(d->fw.product_id) - 1);

	if (ret < 0) {
		TOUCH_I("[%s]read error...\n", __func__);
		return ret;
	}

	TOUCH_I("[%s] IC_product_id: %s\n",
			__func__, d->fw.product_id);

	if (synaptics_is_product(d, "S332U", 5) || synaptics_is_product(d, "S3320T", 6)) {
		ret = synaptics_read(dev, d->f34.dsc.control_base,
				temp_pid, sizeof(temp_pid) - 1);

		if (ret < 0) {
			TOUCH_I("[%s]read error...\n", __func__);
			return ret;
		}

		memset(d->fw.product_id, 0,
				sizeof(d->fw.product_id));
		memcpy(d->fw.product_id, &temp_pid[4], 6);

		TOUCH_I("[%s] Product_ID_Reset ! , addr = 0x%x, P_ID = %s\n",
			__func__, d->f34.dsc.control_base, d->fw.product_id);
	} else {
		return -EINVAL;
	}

	return 0;
}

int synaptics_ic_info(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret;

	ret = synaptics_page_description(dev);

	if (ret < 0) {
		TOUCH_I("[%s]read error...\n", __func__);
		return ret;
	}

	ret = synaptics_get_type_bootloader(dev);

	if (ret < 0) {
		TOUCH_I("[%s]get type bootloader error...\n", __func__);
		return ret;
	}

	ret = synaptics_read(dev, d->f34.dsc.control_base,
			d->fw.version, sizeof(d->fw.version) - 1);

	if (ret < 0) {
		TOUCH_I("[%s]read error...\n", __func__);
		return ret;
	}

	return 0;
}

static int synaptics_sleep_control(struct device *dev, u8 mode)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 val;
	int ret;

	ret = synaptics_read(dev, DEVICE_CONTROL_REG, &val, sizeof(val));

	if (ret < 0) {
		TOUCH_E("failed to read finger report enable - ret:%d\n", ret);
		return ret;
	}

	val &= 0xf8;

	if ((val == DEVICE_CONTROL_SLEEP) && !mode) {
		return 0;
	}

	val |= (mode ? DEVICE_CONTROL_NOSLEEP : DEVICE_CONTROL_SLEEP);

	ret = synaptics_write(dev, DEVICE_CONTROL_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to write finger report enable - ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("%s - mode:%d\n", __func__, mode);

	return 0;
}

static int synaptics_tci_report_enable(struct device *dev, bool enable)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 val[3];
	int ret;

	synaptics_irq_enable(dev, enable ? false : true);

	ret = synaptics_read(dev, FINGER_REPORT_REG, val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to read finger report enable - ret:%d\n", ret);
		return ret;
	}

	val[2] &= 0xfc;

	if (enable)
		val[2] |= 0x2;

	ret = synaptics_write(dev, FINGER_REPORT_REG, val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to write finger report enable - ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("%s - enable:%d\n", __func__, enable);

	return 0;
}

static int synaptics_tci_knock(struct device *dev, u8 value)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct tci_info *info;

	u8 lpwg_data[7];
	int ret;

	TOUCH_TRACE();

	ret = synaptics_set_page(dev, LPWG_PAGE);

	if (ret < 0) {
		TOUCH_E("failed to set page to LPWG_PAGE\n");
		return ret;
	}

	ret = synaptics_read(dev, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	TOUCH_I("0 : %d,%d,%d,%d,%d,%d,%d\n",
		lpwg_data[0],
		lpwg_data[1],
		lpwg_data[2],
		lpwg_data[3],
		lpwg_data[4],
		lpwg_data[5],
		lpwg_data[6]);

	info = &ts->tci.info[0];

	info->tap_count &= 0x7;
	info->intr_delay = 0;
	info->tap_distance = 10;

	lpwg_data[0] |= ((info->tap_count << 3) | 1);
	lpwg_data[1] = info->min_intertap;
	lpwg_data[2] = info->max_intertap;
	lpwg_data[3] = info->touch_slop;
	lpwg_data[4] = info->tap_distance;
	lpwg_data[6] = info->intr_delay;

	if (!value)
		lpwg_data[0] &= 0xfe;

	ret = synaptics_write(dev, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	ret = synaptics_read(dev, LPWG_TAPCOUNT_REG2,
			      &lpwg_data[0], sizeof(u8));

	TOUCH_I("1 : %d\n", lpwg_data[0]);

	lpwg_data[0] &= 0xfe;

	ret = synaptics_write(dev, LPWG_TAPCOUNT_REG2,
			      lpwg_data, sizeof(u8));
	if (ret < 0) {
		TOUCH_E("failed to write LPWG_TAPCOUNT_REG2\n");
		return ret;
	}

	ret = synaptics_set_page(dev, DEFAULT_PAGE);

	if (ret < 0) {
		TOUCH_E("failed to set page to DEFAULT_PAGE\n");
		return ret;
	}

	return ret;
}

static int synaptics_tci_password(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct tci_info *info;

	u8 lpwg_data[7];
	int ret;

	TOUCH_TRACE();

	ret = synaptics_set_page(dev, LPWG_PAGE);

	if (ret < 0) {
		TOUCH_E("failed to set page to LPWG_PAGE\n");
		return ret;
	}

	ret = synaptics_read(dev, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	TOUCH_I("0: %d,%d,%d,%d,%d,%d,%d\n",
		lpwg_data[0],
		lpwg_data[1],
		lpwg_data[2],
		lpwg_data[3],
		lpwg_data[4],
		lpwg_data[5],
		lpwg_data[6]);

	info = &ts->tci.info[0];

	info->tap_count &= 0x7;
	info->intr_delay = ts->tci.double_tap_check ? 68 : 0;
	info->tap_distance = 7;

	lpwg_data[0] |= ((info->tap_count << 3) | 1);
	lpwg_data[1] = info->min_intertap;
	lpwg_data[2] = info->max_intertap;
	lpwg_data[3] = info->touch_slop;
	lpwg_data[4] = info->tap_distance;
	lpwg_data[6] = (info->intr_delay << 1 | 1);

	ret = synaptics_write(dev, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	ret = synaptics_read(dev, LPWG_TAPCOUNT_REG2,
			      lpwg_data, sizeof(lpwg_data));

	TOUCH_I("1: %d,%d,%d,%d,%d,%d,%d\n",
		lpwg_data[0],
		lpwg_data[1],
		lpwg_data[2],
		lpwg_data[3],
		lpwg_data[4],
		lpwg_data[5],
		lpwg_data[6]);

	info = &ts->tci.info[1];
	info->tap_count &= 0x7;
	lpwg_data[0] |= ((info->tap_count << 3) | 1);
	lpwg_data[1] = info->min_intertap;
	lpwg_data[2] = info->max_intertap;
	lpwg_data[3] = info->touch_slop;
	lpwg_data[4] = info->tap_distance;
	lpwg_data[6] = (info->intr_delay << 1 | 1);

	ret = synaptics_write(dev, LPWG_TAPCOUNT_REG2,
			      lpwg_data, sizeof(lpwg_data));

	if (ret < 0) {
		TOUCH_E("failed to write LPWG_TAPCOUNT_REG2\n");
		return ret;
	}

	ret = synaptics_set_page(dev, DEFAULT_PAGE);

	if (ret < 0) {
		TOUCH_E("failed to set page to DEFAULT_PAGE\n");
		return ret;
	}

	return ret;
}

static int synaptics_tci_partial_knock(struct device *dev, u8 value)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 buf;
	int ret = 0;

	TOUCH_I("%s\n", __func__);

	if (d->lpwg_by_lcd_notifier)
		return ret;

	ret = synaptics_set_page(dev, LPWG_PAGE);

	if (ret < 0) {
		TOUCH_E("failed to set page to LPWG_PAGE\n");
		return ret;
	}

	if (synaptics_is_product(d, "PLG468", 6)) {
		ret = synaptics_read(dev, d->f51_reg.lpwg_partial_reg, &buf, sizeof(buf));

		if (ret < 0) {
			TOUCH_E("failed to read LPWG_PARTIAL_REG\n");
			return ret;
		}

		if (value)
			buf &= 0xfc;
		else
			buf &= 0xfe;

		buf |= value;

		ret = synaptics_write(dev, d->f51_reg.lpwg_partial_reg, &buf, sizeof(buf));

		if (ret < 0) {
			TOUCH_E("failed to write LPWG_PARTIAL_REG\n");
			return ret;
		}
	} else {
		buf = value;
		ret = synaptics_write(dev, d->f51_reg.lpwg_partial_reg, &buf, sizeof(buf));
	}

	ret = synaptics_set_page(dev, DEFAULT_PAGE);

	if (ret < 0) {
		TOUCH_E("failed to set page to DEFAULT_PAGE\n");
		return ret;
	}

	return ret;
}

static int synaptics_tci_active_area(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	u8 buffer[8];

	buffer[0] = (ts->lpwg.area[0].x >> 0) & 0xff;
	buffer[1] = (ts->lpwg.area[0].x >> 8) & 0xff;
	buffer[2] = (ts->lpwg.area[0].y >> 0) & 0xff;
	buffer[3] = (ts->lpwg.area[0].y >> 8) & 0xff;
	buffer[4] = (ts->lpwg.area[1].x >> 0) & 0xff;
	buffer[5] = (ts->lpwg.area[1].x >> 8) & 0xff;
	buffer[6] = (ts->lpwg.area[1].y >> 0) & 0xff;
	buffer[7] = (ts->lpwg.area[1].y >> 8) & 0xff;

	synaptics_write(dev, d->f12_reg.ctrl[18], buffer, sizeof(buffer));

	return 0;
}

static int synaptics_lpwg_control(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_I("synaptics_lpwg_control mode=%d\n", mode);

	switch (mode) {
	case LPWG_DOUBLE_TAP:
		ts->tci.mode = 0x01;
		synaptics_tci_knock(dev, ts->tci.mode);
		synaptics_tci_partial_knock(dev, 1);
		break;

	case LPWG_PASSWORD:
		ts->tci.mode = 0x02;
		synaptics_tci_password(dev);
		synaptics_tci_partial_knock(dev, 1);
		break;

	default:
		ts->tci.mode = 0;
		synaptics_tci_partial_knock(dev, ts->tci.mode);
		synaptics_tci_knock(dev, ts->tci.mode);
		break;
	}

	return 0;
}

static int synaptics_lpwg_mode(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		if (ts->lpwg.mode == LPWG_NONE) {
			/* deep sleep */
			TOUCH_I("%s(%d) - deep sleep\n",
				__func__, __LINE__);
			synaptics_lpwg_control(dev, LPWG_NONE);
		} else if (ts->lpwg.screen) {
			TOUCH_I("%s(%d) - FB_SUSPEND & screen on -> skip\n",
				__func__, __LINE__);
			return 0;
		} else if (ts->lpwg.sensor == PROX_NEAR) {
			/* deep sleep */
			TOUCH_I("%s(%d) - deep sleep by prox\n",
				__func__, __LINE__);
			synaptics_lpwg_control(dev, LPWG_DOUBLE_TAP);
		} else if (ts->lpwg.qcover == HALL_NEAR) {
			/* knock on */
			TOUCH_I("%s(%d) - knock on by hall\n",
				__func__, __LINE__);
			synaptics_lpwg_control(dev, LPWG_DOUBLE_TAP);
		} else {
			/* knock on/code */
			TOUCH_I("%s(%d) - knock %d, screen %d, proxy %d, qcover %d\n",
				__func__, __LINE__,
				ts->lpwg.mode, ts->lpwg.screen, ts->lpwg.sensor, ts->lpwg.qcover);
			synaptics_lpwg_control(dev, ts->lpwg.mode);
		}
		return 0;
	}

	/* resume */
	if (ts->lpwg.screen) {
		/* normal */
		TOUCH_I("%s(%d) - normal\n",
				__func__, __LINE__);
		synaptics_lpwg_control(dev, LPWG_NONE);
	} else if (ts->lpwg.mode == LPWG_NONE) {
		/* normal */
		TOUCH_I("%s(%d) - normal on screen off\n",
				__func__, __LINE__);
		synaptics_lpwg_control(dev, LPWG_NONE);
	} else if (ts->lpwg.sensor == PROX_NEAR) {
		/* wake up */
		TOUCH_I("%s(%d) - wake up on screen off and prox\n",
				__func__, __LINE__);
		TOUCH_I("%s - wake up is not ready\n", __func__);
		synaptics_lpwg_control(dev, LPWG_NONE);
	} else {
		/* partial */
		TOUCH_I("%s(%d) - partial mode(knock %d, screen %d, proxy %d, qcover %d)\n",
				__func__, __LINE__,
				ts->lpwg.mode, ts->lpwg.screen, ts->lpwg.sensor, ts->lpwg.qcover);
		TOUCH_I("%s - partial is not ready\n", __func__);
		synaptics_lpwg_control(dev, LPWG_NONE);
	}

	return 0;
}

static void synaptics_init_tci_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	ts->tci.info[0].tap_count = 2;
	ts->tci.info[0].min_intertap = 6;
	ts->tci.info[0].max_intertap = 70;
	ts->tci.info[0].touch_slop = 100;
	ts->tci.info[0].tap_distance = 10;
	ts->tci.info[0].intr_delay = 0;

	ts->tci.info[1].tap_count = 2;
	ts->tci.info[1].min_intertap = 6;
	ts->tci.info[1].max_intertap = 70;
	ts->tci.info[1].touch_slop = 100;
	ts->tci.info[1].tap_distance = 255;
	ts->tci.info[1].intr_delay = 68;
}

static int synaptics_remove(struct device *dev)
{
	if (rmidev_fhandler.initialized
		&& rmidev_fhandler.inserted) {
		rmidev_fhandler.exp_fn->remove(dev);
		rmidev_fhandler.initialized = false;
	}

	return 0;
}

static int synaptics_get_status(struct device *dev, u8 *device, u8 *interrupt)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	u8 dev_status;
	u8 irq_status;
	int ret;

	ret = synaptics_read(dev, DEVICE_STATUS_REG,
			&dev_status, sizeof(dev_status));

	if (ret < 0) {
		TOUCH_E("failed to read device status - ret:%d\n", ret);
		return ret;
	}

	ret = synaptics_read(dev, INTERRUPT_STATUS_REG,
			&irq_status, sizeof(irq_status));

	if (ret < 0) {
		TOUCH_E("failed to read interrupt status - ret:%d\n", ret);
		return ret;
	}

	if (device)
		*device = dev_status;

	if (interrupt)
		*interrupt = irq_status;

	return ret;
}

static int synaptics_irq_clear(struct device *dev)
{
	return synaptics_get_status(dev, NULL, NULL);
}

static int synaptics_get_object_count(struct device *dev, u8 *object)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 object_to_read = d->num_of_fingers;
	u8 buf[2] = {0,};
	u16 object_attention = 0;
	int ret;

	ret = synaptics_read(dev, d->f12_reg.data[15],
			(u8 *) buf, sizeof(buf));

	if (ret < 0) {
		TOUCH_E("%s, %d : get object_attention data failed\n",
			__func__, __LINE__);
		return ret;
	}

	object_attention = (((u16)((buf[1] << 8) & 0xFF00) | (u16)((buf[0])&0xFF)));

	for (; object_to_read > 0 ;) {
		if (object_attention & (0x1 << (object_to_read - 1)))
			break;
        else
		    object_to_read--;
	}

	*object = object_to_read;
	return 0;
}

static int synaptics_irq_abs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct synaptics_object objects[MAX_NUM_OF_FINGERS];
	struct synaptics_object *obj;
	struct touch_data *tdata;
	u8 i;
	u8 finger_index;
	u8 object_to_read = 0;
	int ret;

	ts->new_mask = 0;
	ret = synaptics_get_object_count(dev, &object_to_read);

	if (ret < 0) {
		TOUCH_E("faied to read object count\n");
		return ret;
	}

	TOUCH_D(ABS, "object_to_read: %d\n", object_to_read);

	if (object_to_read > 0) {
		ret = synaptics_read(dev, FINGER_DATA_REG,
				     objects, sizeof(*obj) * object_to_read);
		if (ret < 0) {
			TOUCH_E("faied to read finger data\n");
			return ret;
		}

		finger_index = 0;

		for (i = 0; i < object_to_read; i++) {
			obj = objects + i;

			if (obj->type > F12_MAX_OBJECT)
				TOUCH_D(ABS, "id : %d, type : %d\n",
					i, obj->type);

			if (obj->type == F12_FINGER_STATUS) {
				ts->new_mask |= (1 << i);
				tdata = ts->tdata + i;

				tdata->id = i;
				tdata->type = obj->type;
				tdata->x = obj->x_lsb | obj->x_msb << 8;
				tdata->y = obj->y_lsb | obj->y_msb << 8;
				tdata->pressure = obj->z;

				if (obj->wx > obj->wy) {
					tdata->width_major = obj->wx;
					tdata->width_minor = obj->wy;
					tdata->orientation = 0;
				} else {
					tdata->width_major = obj->wy;
					tdata->width_minor = obj->wx;
					tdata->orientation = 1;
				}

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
	}

	ts->intr_status |= TOUCH_IRQ_FINGER;

	return 0;
}

static int synaptics_tci_getdata(struct device *dev, int count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	u32 buffer[12];
	int i = 0;
	int ret;

	ret = synaptics_read(dev, LPWG_DATA_REG,
					 buffer, sizeof(u32) * count);

	if (ret < 0)
		return ret;

	for (i = 0; i < count; i++) {
		ts->lpwg.code[i].x = buffer[i] & 0xffff;
		ts->lpwg.code[i].y = (buffer[i] >> 16) & 0xffff;

		TOUCH_I("LPWG data %d, %d\n",
			ts->lpwg.code[i].x, ts->lpwg.code[i].y);
	}

	ts->lpwg.code[i].x = -1;
	ts->lpwg.code[i].y = -1;

	return 0;
}

static int synaptics_irq_lpwg(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	u8 status;
	int ret;

	ret = synaptics_set_page(dev, LPWG_PAGE);

	if (ret < 0)
		return ret;

	ret = synaptics_read(dev, LPWG_STATUS_REG, &status, 1);

	if (ret < 0)
		return ret;

	if (status & LPWG_STATUS_DOUBLETAP) {
		synaptics_tci_getdata(dev, 2);
		ts->intr_status |= TOUCH_IRQ_KNOCK;
	} else if (status & LPWG_STATUS_PASSWORD) {
		synaptics_tci_getdata(dev, ts->lpwg.code_num);
		ts->intr_status |= TOUCH_IRQ_PASSWD;
	} else {
		/* Overtab */
		ts->lpwg.code_num = 1;
		synaptics_tci_getdata(dev, ts->lpwg.code_num);
		ts->intr_status |= TOUCH_IRQ_PASSWD;
	}

	return synaptics_set_page(dev, DEFAULT_PAGE);
}

static int synaptics_irq_handler(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 dev_status;
	u8 irq_status;
	int ret;

	TOUCH_TRACE();

	ret = synaptics_get_status(dev, &dev_status, &irq_status);

	if (irq_status & INTERRUPT_MASK_ABS0)
		ret = synaptics_irq_abs(dev);

	if (irq_status & d->lpwg_mask)
		ret = synaptics_irq_lpwg(dev);

	return ret;
}

void synaptics_rmidev_function(struct synaptics_exp_fn *rmidev_fn,
		bool insert)
{
	rmidev_fhandler.inserted = insert;

	if (insert)
		rmidev_fhandler.exp_fn = rmidev_fn;
	else
		rmidev_fhandler.exp_fn = NULL;

	return;
}

static int synaptics_rmi_dev(struct device *dev)
{
	int ret = 0;

	if (rmidev_fhandler.inserted) {
		if (!rmidev_fhandler.initialized) {
			ret = rmidev_fhandler.exp_fn->init(dev);

			if (ret < 0)
				TOUCH_E("Failed to rmi_dev init\n");
			else
				rmidev_fhandler.initialized = true;
		}
	}

	return 0;
}

static int synaptics_f51_regmap(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	d->f51_reg.lpwg_status_reg = LPWG_STATUS_REG;
	d->f51_reg.lpwg_data_reg = LPWG_DATA_REG;
	d->f51_reg.lpwg_tapcount_reg = LPWG_TAPCOUNT_REG;
	d->f51_reg.lpwg_min_intertap_reg = LPWG_MIN_INTERTAP_REG;
	d->f51_reg.lpwg_max_intertap_reg = LPWG_MAX_INTERTAP_REG;
	d->f51_reg.lpwg_touch_slop_reg = LPWG_TOUCH_SLOP_REG;
	d->f51_reg.lpwg_tap_distance_reg = LPWG_TAP_DISTANCE_REG;
	d->f51_reg.lpwg_interrupt_delay_reg = LPWG_INTERRUPT_DELAY_REG;

	d->f51_reg.lpwg_tapcount_reg2 = LPWG_TAPCOUNT_REG + LPWG_BLKSIZ;
	d->f51_reg.lpwg_min_intertap_reg2 = LPWG_MIN_INTERTAP_REG + LPWG_BLKSIZ;
	d->f51_reg.lpwg_max_intertap_reg2 = LPWG_MAX_INTERTAP_REG + LPWG_BLKSIZ;
	d->f51_reg.lpwg_touch_slop_reg2 = LPWG_TOUCH_SLOP_REG + LPWG_BLKSIZ;
	d->f51_reg.lpwg_tap_distance_reg2 = LPWG_TAP_DISTANCE_REG + LPWG_BLKSIZ;
	d->f51_reg.lpwg_interrupt_delay_reg2 = LPWG_INTERRUPT_DELAY_REG + LPWG_BLKSIZ;

	if (synaptics_is_product(d, "PLG468", 6)) {
		d->f51_reg.overtap_cnt_reg = d->f51.dsc.data_base + 57;
		d->f51_reg.request_reset_reg = d->f51.dsc.data_base + 69;
		d->f51_reg.lpwg_partial_reg = LPWG_PARTIAL_REG + 71;
		d->f51_reg.lpwg_fail_count_reg = d->f51.dsc.data_base + 0x21;
		d->f51_reg.lpwg_fail_index_reg = d->f51.dsc.data_base + 0x22;
		d->f51_reg.lpwg_fail_reason_reg = d->f51.dsc.data_base + 0x23;
	} else {
		d->f51_reg.overtap_cnt_reg = d->f51.dsc.data_base + 73;
		d->f51_reg.lpwg_partial_reg = LPWG_PARTIAL_REG;
		d->f51_reg.lpwg_fail_count_reg = d->f51.dsc.data_base + 0x31;
		d->f51_reg.lpwg_fail_index_reg = d->f51.dsc.data_base + 0x32;
		d->f51_reg.lpwg_fail_reason_reg = d->f51.dsc.data_base + 0x33;

		d->f51_reg.lpwg_adc_offset_reg =
			d->f51_reg.lpwg_interrupt_delay_reg2 + 44;
		d->f51_reg.lpwg_adc_fF_reg1 =
			d->f51_reg.lpwg_interrupt_delay_reg2 + 45;
		d->f51_reg.lpwg_adc_fF_reg2 =
			d->f51_reg.lpwg_interrupt_delay_reg2 + 46;
		d->f51_reg.lpwg_adc_fF_reg3 =
			d->f51_reg.lpwg_interrupt_delay_reg2 + 47;
		d->f51_reg.lpwg_adc_fF_reg4 =
			d->f51_reg.lpwg_interrupt_delay_reg2 + 48;
	}

	TOUCH_I("[%s] Complete to match-up regmap.\n", __func__);

	return 0;
}

static int synaptics_f54_regmap(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	if (synaptics_is_product(d, "PLG468", 6)) {
		d->f54_reg.interference__metric_LSB = 0x05;
		d->f54_reg.interference__metric_MSB = 0x06;
		d->f54_reg.current_noise_status = 0x09;
		d->f54_reg.freq_scan_im = 0x0A;
	} else {
		d->f54_reg.interference__metric_LSB = 0x04;
		d->f54_reg.interference__metric_MSB = 0x05;
		d->f54_reg.current_noise_status = 0x08;
		d->f54_reg.freq_scan_im = 0x0B;
		d->f54_reg.incell_statistic = 0x10;
		d->f54_reg.general_control = 0x11;
	}

	TOUCH_I("[%s] Complete to match-up regmap.\n", __func__);

	return 0;
}


int synaptics_init(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	synaptics_ic_info(dev);

	synaptics_rmi_dev(dev);
	synaptics_prd_handle(dev);

	synaptics_f51_regmap(dev);
	synaptics_f54_regmap(dev);

	synaptics_lpwg_mode(dev);

	synaptics_irq_enable(dev, true);
	synaptics_irq_clear(dev);

	d->lpwg_by_lcd_notifier = false;

	return 0;
}

static int synaptics_power(struct device *dev, int ctrl)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	TOUCH_TRACE();

	switch (ctrl) {
	case POWER_OFF:
		TOUCH_I("%s, off\n", __func__);
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_power_vio(dev, 0);
		touch_power_vdd(dev, 0);
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
	case POWER_SLEEP_STATUS:
		synaptics_sleep_control(dev, ts->lpwg.sensor);
		ctrl = POWER_SLEEP;
		break;
	default:
		break;
	}

	d->power_state = ctrl;

	return 0;
}

static int synaptics_suspend(struct device *dev)
{
	TOUCH_TRACE();
	synaptics_lpwg_mode(dev);
	synaptics_power(dev, POWER_SLEEP_STATUS);

	return 0;
}

static int synaptics_resume(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	TOUCH_TRACE();

	synaptics_power(dev, POWER_OFF);
	synaptics_power(dev, POWER_ON);
	touch_msleep(ts->caps.hw_reset_delay);
	synaptics_lpwg_mode(dev);
	return 0;
}

static int synaptics_lpwg(struct device *dev, u32 code, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int *value = (int *)param;
	u8 val = 0;

	TOUCH_TRACE();

	switch (code) {
	case LPWG_ACTIVE_AREA:
		ts->lpwg.area[0].x = value[0];
		ts->lpwg.area[0].y = value[2];
		ts->lpwg.area[1].x = value[1];
		ts->lpwg.area[1].y = value[3];
		TOUCH_I("LPWG AREA (%d,%d)-(%d,%d)\n",
			ts->lpwg.area[0].x, ts->lpwg.area[0].y,
			ts->lpwg.area[1].x, ts->lpwg.area[1].y);
		synaptics_tci_active_area(dev);
		break;

	case LPWG_UPDATE_ALL:
		ts->lpwg.mode = value[0];
		ts->lpwg.screen = value[1];
		ts->lpwg.sensor = value[2];
		ts->lpwg.qcover = value[3];
		TOUCH_I("LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
			ts->lpwg.mode,
			ts->lpwg.screen ? "ON" : "OFF",
			ts->lpwg.sensor ? "FAR" : "NEAR",
			ts->lpwg.qcover ? "CLOSE" : "OPEN");
		synaptics_lpwg_mode(dev);
		break;

	case LPWG_TAP_COUNT:
		ts->tci.info[1].tap_count = value[0];
		ts->lpwg.code_num = ts->tci.info[1].tap_count;
		break;

	case LPWG_ON:
		if (synaptics_is_product(d, "PLG468", 6)) {
			TOUCH_I("[%s] CONTROL_REG : DEVICE_CONTROL_NOSLEEP\n", __func__);
			val = DEVICE_CONTROL_NOSLEEP | DEVICE_CONTROL_CONFIGURED;
			synaptics_write(dev, DEVICE_CONTROL_REG, &val, sizeof(val));
		}

		synaptics_tci_report_enable(dev, true);
		d->lpwg_by_lcd_notifier = true;
		break;

	case LPWG_OFF:
		if (synaptics_is_product(d, "PLG446", 6)) {
			TOUCH_I("[%s] CONTROL_REG : DEVICE_CONTROL_NORMAL_OP\n", __func__);
			val = DEVICE_CONTROL_NORMAL_OP | DEVICE_CONTROL_CONFIGURED;
			synaptics_write(dev, DEVICE_CONTROL_REG, &val, sizeof(val));
		}

		synaptics_tci_report_enable(dev, false);
		d->lpwg_by_lcd_notifier = false;
		break;

	case LPWG_NO_SLEEP:
		touch_msleep(20);
		TOUCH_I("[%s] CONTROL_REG : DEVICE_CONTROL_NOSLEEP\n", __func__);
		val = DEVICE_CONTROL_NOSLEEP | DEVICE_CONTROL_CONFIGURED;
		synaptics_write(dev, DEVICE_CONTROL_REG, &val, sizeof(val));

		if (synaptics_is_product(d, "PLG446", 6))
			touch_msleep(30);
		break;
	}
	return 0;
}

static int synaptics_bin_fw_version(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	const struct firmware *fw = NULL;
	const u8 *firmware = NULL;
	int rc = 0;

	rc = request_firmware(&fw, d->fw.def_fw, dev);
	if (rc != 0) {
		TOUCH_E("[%s] request_firmware() failed %d\n", __func__, rc);
		return -EIO;
	}

	firmware = fw->data;

	memcpy(d->fw.img_product_id,
			&firmware[d->fw.fw_pid_addr], 6);
	memcpy(d->fw.img_version,
			&firmware[d->fw.fw_ver_addr], 4);

	release_firmware(fw);

	return rc;
}

static char *synaptics_productcode_parse(unsigned char *product)
{
	static char str[128] = {0};
	int len = 0;
	char inch[2] = {0};
	char paneltype = 0;
	char version[2] = {0};
	const char *str_panel[]
		= { "ELK", "Suntel", "Tovis", "Innotek", "JDI", "LGD", };
	const char *str_ic[] = { "Synaptics", };
	int i;

	i = (product[0] & 0xF0) >> 4;
	if (i < 6)
		len += snprintf(str + len, sizeof(str) - len,
				"%s\n", str_panel[i]);
	else
		len += snprintf(str + len, sizeof(str) - len,
				"Unknown\n");

	i = (product[0] & 0x0F);
	if (i < 5 && i != 1)
		len += snprintf(str + len, sizeof(str) - len,
				"%dkey\n", i);
	else
		len += snprintf(str + len, sizeof(str) - len,
				"Unknown\n");

	i = (product[1] & 0xF0) >> 4;
	if (i < 1)
		len += snprintf(str + len, sizeof(str) - len,
				"%s\n", str_ic[i]);
	else
		len += snprintf(str + len, sizeof(str) - len,
				"Unknown\n");

	inch[0] = (product[1] & 0x0F);
	inch[1] = ((product[2] & 0xF0) >> 4);
	len += snprintf(str + len, sizeof(str) - len,
			"%d.%d\n", inch[0], inch[1]);

	paneltype = (product[2] & 0x0F);
	len += snprintf(str + len, sizeof(str) - len,
			"PanelType %d\n", paneltype);

	version[0] = ((product[3] & 0x80) >> 7);
	version[1] = (product[3] & 0x7F);
	len += snprintf(str + len, sizeof(str) - len,
			"version : v%d.%02d\n", version[0], version[1]);

	return str;
}

static int synaptics_get_cmd_version(struct device *dev, char *buf)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int offset = 0;
	int ret = 0;

	TOUCH_I("%s\n", __func__);

	ret = synaptics_ic_info(dev);
	ret += synaptics_bin_fw_version(dev);

	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Read Fail Touch IC Info\n");
		return offset;
	}

	offset = snprintf(buf + offset, PAGE_SIZE - offset,
				"\n======== Firmware Info ========\n");

	if (d->fw.version[0] > 0x50) {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"ic_version[%s]\n", d->fw.version);
	} else {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"ic_version RAW = %02X %02X %02X %02X\n",
				d->fw.version[0], d->fw.version[1],
				d->fw.version[2], d->fw.version[3]);
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"=== ic_fw_version info ===\n%s",
				synaptics_productcode_parse(d->fw.version));
	}

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"IC_product_id[%s]\n", d->fw.product_id);

	if (synaptics_is_product(d, "PLG468", 6)
		|| synaptics_is_product(d, "PLG446", 6)) {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Touch IC : s3320(BL 7.2)\n\n");
	} else {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Touch product ID read fail\n\n");
	}

	if (d->fw.img_version[0] > 0x50) {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"img_version[%s]\n", d->fw.img_version);
	} else {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"img_version RAW = %02X %02X %02X %02X\n",
				d->fw.img_version[0], d->fw.img_version[1],
				d->fw.img_version[2], d->fw.img_version[3]);
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"=== img_version info ===\n%s",
				synaptics_productcode_parse(d->fw.img_version));
	}

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Img_product_id[%s]\n", d->fw.img_product_id);

	if (synaptics_is_img_product(d, "PLG468", 6)
		|| synaptics_is_img_product(d, "PLG446", 6)) {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Touch IC : s3320\n");
	} else {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"Touch product ID read fail\n");
	}
	TOUCH_I("end\n");
	return offset;
}

static int synaptics_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int offset = 0;
	int ret = 0;

	ret = synaptics_ic_info(dev);

	if (ret < 0) {
		offset += snprintf(buf + offset, PAGE_SIZE, "-1\n");
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Read Fail Touch IC Info\n");
		return offset;
	}

	if (d->fw.version[0] > 0x50) {
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
				"%s\n", d->fw.version);
	} else {
		offset = snprintf(buf + offset, PAGE_SIZE - offset,
					"v%d.%02d(0x%X/0x%X/0x%X/0x%X)\n",
					(d->fw.version[3] & 0x80 ? 1 : 0),
					d->fw.version[3] & 0x7F,
					d->fw.version[0],
					d->fw.version[1],
					d->fw.version[2],
					d->fw.version[3]);
	}

	return offset;
}

static int synaptics_notify(struct device *dev, ulong event, void *data)
{
	TOUCH_TRACE();

	return 0;
}

static ssize_t store_reg_ctrl(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u8 buffer[50] = {0};
	char command[6] = {0};
	int page = 0;
	int reg = 0;
	int offset = 0;
	int value = 0;

	if (sscanf(buf, "%s %d %d %d %d ",
				command, &page, &reg, &offset, &value) <= 0)
		return count;

	mutex_lock(&ts->lock);
	synaptics_set_page(dev, page);
	if (!strcmp(command, "write")) {
		synaptics_read(dev, reg, buffer, offset + 1);
		buffer[offset] = (u8)value;
		synaptics_write(dev, reg, buffer, offset + 1);
	} else if (!strcmp(command, "read")) {
		synaptics_read(dev, reg, buffer, offset + 1);
		TOUCH_I("page[%d] reg[%d] offset[%d] = 0x%x\n",
				page, reg, offset, buffer[offset]);
	} else {
		TOUCH_E("Usage\n");
		TOUCH_E("Write page reg offset value\n");
		TOUCH_E("Read page reg offset\n");
	}
	synaptics_set_page(dev, DEFAULT_PAGE);
	mutex_unlock(&ts->lock);
	return count;
}

static ssize_t show_noise(struct device *dev, char *buf)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int offset = 0;

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Test Count : %d\n", d->noise.cnt);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Current Noise State : %d\n", d->noise.cns_avg);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Interference Metric : %d\n", d->noise.im_avg);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Freq Scan IM : %d\n", d->noise.freq_scan_im_avg);

	return offset;
}

static ssize_t store_noise(struct device *dev,
		const char *buf, size_t count)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value == NOISE_DISABLE || value == NOISE_ENABLE) {
		d->noise.check_noise = value;
	} else {
		TOUCH_E("invalid_value(%d)\n", value);
		return count;
	}

	TOUCH_I("noise_log = %s\n", (d->noise.check_noise == NOISE_ENABLE)
			? "NOISE_ENABLE" : "NOISE_DISABLE");

	return count;
}

static ssize_t show_noise_log(struct device *dev, char *buf)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int offset = 0;

	offset += snprintf(buf + offset, PAGE_SIZE - offset, "%d\n",
				d->noise.noise_log);

	return offset;
}

static ssize_t store_noise_log(struct device *dev,
		const char *buf, size_t count)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value == NOISE_DISABLE || value == NOISE_ENABLE) {
		d->noise.noise_log = value;
	} else {
		TOUCH_E("invalid_value(%d)\n", value);
		return count;
	}

	TOUCH_I("noise_log = %s\n", (d->noise.noise_log == NOISE_ENABLE)
			? "NOISE_LOG_ENABLE" : "NOISE_LOG_DISABLE");

	return count;
}

static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(noise_log, show_noise_log, store_noise_log);
static TOUCH_ATTR(noise, show_noise, store_noise);

static struct attribute *s3320_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_noise_log.attr,
	&touch_attr_noise.attr,
	NULL,
};

static const struct attribute_group s3320_attribute_group = {
	.attrs = s3320_attribute_list,
};

static int synaptics_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int ret = 0;
	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &s3320_attribute_group);
	if (ret < 0)
		TOUCH_E("s3320 sysfs register failed\n");

	s3320_prd_register_sysfs(dev);

	return 0;
}

static int synaptics_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_I("%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case SET_SLEEP_STATUS:
		TOUCH_I("LCD_EVENT_TOUCH_SET_SLEEP_STATUS\n");
		synaptics_power(dev, POWER_SLEEP_STATUS);
		break;

	case SET_NO_SLEEP:
		TOUCH_I("LCD_EVENT_TOUCH_NO_SLEEP\n");
		synaptics_lpwg(dev, LPWG_NO_SLEEP, NULL);
		break;

	case SET_EARLY_RESET:
		TOUCH_I("LCD_EVENT_TOUCH_SET_EARLY_RESET\n");
		break;

	case SET_POWER_OFF:
		TOUCH_I("LCD_EVENT_TOUCH_SET_POWER_OFF\n");
		synaptics_power(dev, POWER_OFF);
		break;

	case SET_DEEP_ACTIVE:
		TOUCH_I("Deep to Active!\n");
		break;

	default:
		break;
	}

	return 0;
}

static int synaptics_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_I("%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = synaptics_get_cmd_version(dev, (char *)output);
		break;

	case CMD_ATCMD_VERSION:
		ret = synaptics_get_cmd_atcmd_version(dev, (char *)output);
		break;

	default:
		break;
	}

	return ret;
}

static int synaptics_get_panel_id(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int panel_id = 0xFF;
	int value = 0;

	static bool panel_id_init;

	if (panel_id_init && d->fw.panel_id) {
		TOUCH_I("panel id : %d\n", d->fw.panel_id);
		return d->fw.panel_id;
	}

	if (!panel_id_init) {
		if (ts->maker_id_pin) {
			touch_gpio_init(ts->maker_id_pin, "touch_id");
		} else {
			TOUCH_I("maker_id_gpio is invalid\n");
		}
		panel_id_init = true;
	}

	if (ts->maker_id_pin) {
		value = gpio_get_value(ts->maker_id_pin);
		TOUCH_I("MAKER_ID : %s\n", value ? "High" : "Low");
		panel_id = (value & 0x1);
	}

	panel_id_init = true;
	d->fw.panel_id = panel_id;
	TOUCH_I("Touch panel id : %d\n", d->fw.panel_id);

	return panel_id;
}

static void synaptics_select_firmware(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	if (d->fw.def_fw)
		return;

	d->fw.panel_id = synaptics_get_panel_id(dev);

	d->fw.fw_pid_addr = 0x4f4;
	d->fw.fw_ver_addr = 0x4f0;

	if (d->fw.panel_id == 0xFF) {
		TOUCH_I("Fail to get panel id\n");
		d->fw.def_fw = ts->def_fwpath[0];
	} else {
		TOUCH_I("Success to get panel id\n");
		d->fw.def_fw = ts->def_fwpath[d->fw.panel_id];

		if (d->fw.panel_id)
			d->lpwg_mask = (1 << 5);
		else
			d->lpwg_mask = INTERRUPT_MASK_LPWG;
	}

	TOUCH_I("fw_image : %s\n", d->fw.def_fw);
	TOUCH_I("fw_pid_addr(0x%x), fw_ver_addr(0x%x)\n",
			d->fw.fw_pid_addr, d->fw.fw_ver_addr);
}

static int synaptics_fw_compare(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int i = 0;
	int update = 0;

	if (ts->force_fwup) {
		update = 1;
	} else if (d->fw.version[0] > 0x50) {
		if (d->fw.img_version[0] > 0x50) {
			TOUCH_I("product_id[%s(ic):%s(img)] version[%s(ic):%s(img)]\n",
				d->fw.product_id, d->fw.img_product_id,
				d->fw.version, d->fw.img_version);
			if (strncmp(d->fw.version, d->fw.img_version, 4)) {
				TOUCH_I("fw version mismatch\n");
				update = 1;
			}
		} else {
			TOUCH_I("product_id[%s(ic):%s(img)] version[%s(ic):V%d.%02d(img)]\n",
				d->fw.product_id, d->fw.img_product_id,
				d->fw.version, (d->fw.img_version[3] & 0x80 ? 1 : 0),
				d->fw.img_version[3] & 0x7F);
			if (strncmp(d->fw.version, d->fw.img_version, 4)) {
				TOUCH_I("fw version mismatch.\n");
				update = 1;
			}
		}
	} else {
		if (!(d->fw.version[3] & 0x80)) {
			TOUCH_I("Test fw version[V%d.%02d]\n",
				(d->fw.version[3] & 0x80 ? 1 : 0), d->fw.version[3] & 0x7F);
		} else if (d->fw.img_version[0] > 0x50) {
			TOUCH_I("product_id[%s(ic):%s(img)] fw_version[V%d.%02d(ic):%s(img)]\n",
				d->fw.product_id, d->fw.img_product_id,
				(d->fw.version[3] & 0x80 ? 1 : 0), d->fw.version[3] & 0x7F,
				d->fw.img_version);
			if (strncmp(d->fw.version, d->fw.img_version, 4)) {
				TOUCH_I("fw version mismatch.\n");
				update = 1;
			}
		} else {
			TOUCH_I("product_id[%s(ic):%s(img)]\n",
				d->fw.product_id, d->fw.img_product_id);
			TOUCH_I("ic_version[V%d.%02d(0x%02X 0x%02X 0x%02X 0x%02X)]\n ",
				(d->fw.version[3] & 0x80 ? 1 : 0), d->fw.version[3] & 0x7F,
				d->fw.version[0], d->fw.version[1],
				d->fw.version[2], d->fw.version[3]);
			TOUCH_I("img_version[V%d.%02d(0x%02X 0x%02X 0x%02X 0x%02X)]\n",
				(d->fw.img_version[3] & 0x80 ? 1 : 0), d->fw.img_version[3] & 0x7F,
				d->fw.img_version[0], d->fw.img_version[1],
				d->fw.img_version[2], d->fw.img_version[3]);
			for (i = 0; i < FW_VER_INFO_NUM; i++) {
				if (d->fw.version[i] != d->fw.img_version[i]) {
					TOUCH_I("version mismatch(ic_version[%d]:0x%02X, img_version[%d]:0x%02X)\n",
						i, d->fw.version[i], i, d->fw.img_version[i]);
					update = 1;
				}
			}
		}
	}

	return update;
}

static int synaptics_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret = 0;
	int update = 0;
	char fwpath[256] = {0};
	const struct firmware *fw = NULL;
	const u8 *firmware = NULL;

	if (!ts->force_fwup) {
		ts->test_fwpath[0] = '\0';
		synaptics_select_firmware(dev);
	}

	if (ts->test_fwpath[0]) {
		memcpy(fwpath, &ts->test_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from test_fwpath:%s\n",
				&ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
		memcpy(fwpath, d->fw.def_fw, sizeof(fwpath));
		TOUCH_I("ic_product_id = %s\n", d->fw.product_id);
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

	firmware = fw->data;

	memcpy(d->fw.img_product_id,
			&firmware[d->fw.fw_pid_addr], 6);
	memcpy(d->fw.img_version,
			&firmware[d->fw.fw_ver_addr], 4);

	TOUCH_I("img_product_id : %s\n", d->fw.img_product_id);

	update = synaptics_fw_compare(dev);

	if (update) {
		ret = FirmwareUpgrade(dev, fw);
		d->need_scan_pdt = true;
	} else {
		TOUCH_I("need not fw version upgrade\n");
	}

	release_firmware(fw);

	return ret;
}

static int lcd_notifier_callback(struct notifier_block *this,
		unsigned long event, void *data)
{
	struct touch_core_data *ts =
		container_of(this, struct touch_core_data, notif);

	int mode = 0;

	switch (event) {
	case LCD_EVENT_TOUCH_PWR_OFF:
		TOUCH_I("LCD_EVENT_TOUCH_PWR_OFF\n");
		synaptics_power(ts->dev, POWER_OFF);
		break;
	case LCD_EVENT_TOUCH_LPWG_ON:
		TOUCH_I("LCD_EVENT_TOUCH_LPWG_ON\n");
		mutex_lock(&ts->lock);
		synaptics_lpwg(ts->dev, LPWG_ON, NULL);
		mutex_unlock(&ts->lock);
		break;
	case LCD_EVENT_TOUCH_LPWG_OFF:
		/*if (!boot_mode) {
			TOUCH_I(
					"%s  : Ignore resume in Chargerlogo mode\n",
					__func__);
			return 0;
		}*/
		TOUCH_I("LCD_EVENT_TOUCH_LPWG_OFF\n");
		mutex_lock(&ts->lock);
		synaptics_lpwg(ts->dev, LPWG_OFF, NULL);
		mutex_unlock(&ts->lock);
		break;
	case LCD_EVENT_TOUCH_SLEEP_STATUS:
		mode = *(int *)(unsigned long)data;
		synaptics_set(ts->dev, mode, NULL, NULL);
		break;
	default:
		break;
	}
	return 0;
}

static int synaptics_init_notify(struct touch_core_data *ts)
{
	int ret;

	ts->notif.notifier_call = lcd_notifier_callback;
	ret = touch_register_client(&ts->notif);

	if (ret < 0) {
		TOUCH_E("failed to regiseter lcd_notifier_callback\n");
	}

	return ret;
}

static int synaptics_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d;

	TOUCH_TRACE();

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);

	if (!d) {
		TOUCH_E("failed to allocate synaptics data\n");
		return -ENOMEM;
	}

	touch_set_device(ts, d);

	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 0);

	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	touch_power_init(dev);
	touch_bus_init(dev, 4096);

	synaptics_init_notify(ts);
	synaptics_init_tci_info(dev);

	return 0;
}

static struct touch_driver touch_driver = {
	.probe = synaptics_probe,
	.remove = synaptics_remove,
	.suspend = synaptics_suspend,
	.resume = synaptics_resume,
	.init = synaptics_init,
	.upgrade = synaptics_upgrade,
	.irq_handler = synaptics_irq_handler,
	.power = synaptics_power,
	.lpwg = synaptics_lpwg,
	.notify = synaptics_notify,
	.register_sysfs = synaptics_register_sysfs,
	.set = synaptics_set,
	.get = synaptics_get,
};


#define MATCH_NAME			"synaptics,s3320"

static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
};

static struct touch_hwif hwif = {
	.bus_type = HWIF_I2C,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = touch_match_ids,
};

static int __init touch_device_init(void)
{
	TOUCH_TRACE();
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
