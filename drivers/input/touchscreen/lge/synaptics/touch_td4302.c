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
#define TS_MODULE "[td4302]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>

/*
 *  Include to touch core Header File
 */
#include <touch_hwif.h>
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_td4302.h"

/*
 * PLG349 - Z2
 * PLG446 - P1(JDI)
 * PLG469 - P1(LGD)
 */

int synaptics_spi_transfer(struct device *dev, struct touch_bus_msg *msg, int rw, int x_count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct spi_transfer *x = NULL;
	struct spi_message m;
	int ret = 0;
	int index = 0;
	TOUCH_TRACE();

	spi_message_init(&m);
	x = kcalloc(x_count, sizeof(struct spi_transfer), GFP_KERNEL);
	for (index = 0; index <  x_count; index++) {
		x[index].len = 1;
		x[index].delay_usecs = SPI_DELAY;
		if (rw < 128) {
			x[index].tx_buf = &msg->tx_buf[index];
		} else {
			if (index < ADDRESS_WORD_LEN)
				x[index].tx_buf = &msg->tx_buf[index];
			else
				x[index].rx_buf =
					&msg->rx_buf[index - ADDRESS_WORD_LEN];
		}
		spi_message_add_tail(&x[index], &m);
	}
	x[index - 1].delay_usecs = SPI_DELAY;

	ret = spi_sync(spi, &m);

	if (ret < 0)
		TOUCH_I("%s ret = %d\n", __func__, ret);

	kfree(x);

	return ret;
}

static struct synaptics_rmidev_exp_fhandler rmidev_fhandler;

static int synaptics_rmi4_spi_set_page(struct device *dev, unsigned short addr)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct touch_bus_msg msg;
	unsigned int xfer_count = PAGE_SELECT_LEN + 1;
	unsigned char page;
	int ret = 0;

	page = ((addr >> 8) & ~MASK_7BIT);

	if (page != d->curr_page) {
		ts->tx_buf[0] = SPI_WRITE;
		ts->tx_buf[1] = MASK_8BIT;
		ts->tx_buf[2] = page;

		msg.tx_buf = ts->tx_buf;
		msg.tx_size = PAGE_SELECT_LEN + 1;
		msg.rx_buf = NULL;
		msg.rx_size = 0;

		ret = synaptics_spi_transfer(dev, &msg, ts->tx_buf[0],
				xfer_count);

		if (ret == 0) {
			d->curr_page = page;
			ret = PAGE_SELECT_LEN;
		} else {
			TOUCH_I("%s: Failed to complete SPI transfer, error = %d\n", __func__, ret);
		}
	} else {
		ret = PAGE_SELECT_LEN;
	}

	return ret;
}

int synaptics_read(struct device *dev, u8 page, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct touch_bus_msg msg;
	unsigned int xfer_count = size + ADDRESS_WORD_LEN;
	int ret = 0;

//	TOUCH_I("%s : addr[%X] size[%d]\n", __func__, addr, size);

	addr |= (page << 8);
	ts->tx_buf[0] = (addr >> 8) | SPI_READ;
	ts->tx_buf[1] = addr & MASK_8BIT;

	mutex_lock(&d->io_lock);

	synaptics_rmi4_spi_set_page(dev, addr);
	msg.tx_buf = ts->tx_buf;
	msg.tx_size = ADDRESS_WORD_LEN;
	msg.rx_buf = ts->rx_buf;
	msg.rx_size = ADDRESS_WORD_LEN + size;

	ret = synaptics_spi_transfer(dev, &msg, ts->tx_buf[0], xfer_count);

	if (ret < 0) {
		TOUCH_E("touch bus read error : %d\n", ret);
		return ret;
	}

	memcpy(data, ts->rx_buf, size);
	mutex_unlock(&d->io_lock);
	return 0;
}

int synaptics_write(struct device *dev, u8 page, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct touch_bus_msg msg;
	unsigned int xfer_count = size + ADDRESS_WORD_LEN;
	int ret = 0;

//	TOUCH_I("%s : addr[%X] size[%d]\n", __func__, addr, size);

	addr |= (page << 8);
	ts->tx_buf[0] = (addr >> 8) & ~SPI_READ;
	ts->tx_buf[1] = addr & MASK_8BIT;
	memcpy(&ts->tx_buf[ADDRESS_WORD_LEN], data, size);

	mutex_lock(&d->io_lock);

	synaptics_rmi4_spi_set_page(dev, addr);
	msg.tx_buf = ts->tx_buf;
	msg.tx_size = ADDRESS_WORD_LEN + size;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	ret = synaptics_spi_transfer(dev, &msg, ts->tx_buf[0], xfer_count);

	mutex_unlock(&d->io_lock);

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

	ret = synaptics_read(dev, FINGER_PAGE, INTERRUPT_ENABLE_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to read interrupt enable - ret:%d\n", ret);
		return ret;
	}

	if (enable)
		val |= (INTERRUPT_MASK_ABS0 | INTERRUPT_MASK_CUSTOM);
	else
		val &= ~INTERRUPT_MASK_ABS0;


	ret = synaptics_write(dev, FINGER_PAGE, INTERRUPT_ENABLE_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to write interrupt enable - ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("write interrupt : enable:%d, val:%02X\n", enable, val);

	return 0;
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

	ret = synaptics_read(dev, FINGER_PAGE, d->f12.dsc.query_base + 5,
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

	ret = synaptics_read(dev, FINGER_PAGE, d->f12.dsc.query_base + 8,
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

	ret = synaptics_read(dev, FINGER_PAGE, d->f12_reg.ctrl[23],
			     ctrl_23_data, sizeof(ctrl_23_data));

	if (ret < 0) {
		TOUCH_E("faied to get f12_ctrl32_data (ret: %d)\n", ret);
		return ret;
	}

	d->object_report = ctrl_23_data[0];
	d->num_of_fingers = min_t(u8, ctrl_23_data[1], (u8) MAX_NUM_OF_FINGERS);

	TOUCH_I("object_report[0x%02X], num_of_fingers[%d]\n",
			d->object_report, d->num_of_fingers);

	ret = synaptics_read(dev, FINGER_PAGE, d->f12_reg.ctrl[8],
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
		for (pdt = PDT_START; pdt > PDT_END; pdt -= sizeof(dsc)) {
			pdt |= (page << 8);
			ret = synaptics_read(dev, page, pdt, (unsigned char *)&dsc, sizeof(dsc));

			if (ret < 0) {
				TOUCH_E("read descrptore %d (ret: %d)\n",
					pdt, ret);
				return ret;
			}
			pdt &= ~(MASK_8BIT << 8);

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

	return 0;
}

static int synaptics_ic_info(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int ret;

	ret = synaptics_read(dev, FLASH_PAGE, FLASH_CONFIG_ID_REG, &(d->fw.version[0]),
			sizeof(d->fw.version));
	ret = synaptics_read(dev, FLASH_PAGE, CUSTOMER_FAMILY_REG, &(d->fw.family),
			sizeof(d->fw.family));
	ret = synaptics_read(dev, FLASH_PAGE, FW_REVISION_REG, &(d->fw.revision),
			sizeof(d->fw.revision));
	ret = synaptics_read(dev, FLASH_PAGE, PRODUCT_ID_REG, &(d->fw.product_id[0]),
			sizeof(d->fw.product_id));

	TOUCH_I("Img Version = V%d.%02d\n", d->fw.version[3] & 0x80 ? 1 : 0,
			d->fw.version[3] & 0x7F);
	TOUCH_I("CUSTOMER_FAMILY_REG = %d\n", d->fw.family);
	TOUCH_I("FW_REVISION_REG = %d\n", d->fw.revision);
	TOUCH_I("PRODUCT ID = %s\n", d->fw.product_id);

	return ret;
}

static int synaptics_sleep_control(struct device *dev, u8 mode)
{
/*
	struct synaptics_data *d = to_synaptics_data(dev);
	u8 val;
	int ret;

	ret = synaptics_read(dev, DEVICE_CONTROL_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to read finger report enable - ret:%d\n", ret);
		return ret;
	}

	val &= 0xf8;

	if (mode)
		val |= 1;

	ret = synaptics_write(dev, DEVICE_CONTROL_REG, &val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to write finger report enable - ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("%s - mode:%d\n", __func__, mode);
*/
	return 0;
}

static int synaptics_tci_report_enable(struct device *dev, bool enable)
{
	struct synaptics_data *d = to_synaptics_data(dev);

	u8 val[3];
	int ret;

	synaptics_irq_enable(dev, enable ? false : true);

	ret = synaptics_read(dev, FINGER_PAGE, FINGER_REPORT_REG, val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to read finger report enable - ret:%d\n", ret);
		return ret;
	}

	val[2] &= 0xfc;

	if (enable)
		val[2] |= 0x2;

	ret = synaptics_write(dev, FINGER_PAGE, FINGER_REPORT_REG, val, sizeof(val));
	if (ret < 0) {
		TOUCH_E("failed to write finger report enable - ret:%d\n", ret);
		return ret;
	}

	TOUCH_I("%s - enable:%d\n", __func__, enable);

	return 0;
}

static int synaptics_tci_knock(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct tci_info *info;
	u8 lpwg_data[6];
	int ret;

	TOUCH_TRACE();

	ret = synaptics_read(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	TOUCH_I("0 : %d,%d,%d,%d,%d,%d\n",
		lpwg_data[0],
		lpwg_data[1],
		lpwg_data[2],
		lpwg_data[3],
		lpwg_data[4],
		lpwg_data[5]);

	info = &ts->tci.info[0];

	info->tap_count &= 0x7;
	info->intr_delay = 68;
	info->tap_distance = 10;

	lpwg_data[0] |= ((info->tap_count << 3) | 1);
	lpwg_data[1] = info->min_intertap;
	lpwg_data[2] = info->max_intertap;
	lpwg_data[3] = info->touch_slop;
	lpwg_data[4] = info->tap_distance;
	lpwg_data[5] = 0;

	ret = synaptics_write(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	ret = synaptics_read(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG2,
			      &lpwg_data[0], sizeof(u8));

	TOUCH_I("1 : %d\n", lpwg_data[0]);

	lpwg_data[0] &= 0xfe;

	ret = synaptics_write(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG2,
			      lpwg_data, sizeof(u8));
	if (ret < 0) {
		TOUCH_E("failed to write LPWG_TAPCOUNT_REG2\n");
		return ret;
	}

	return ret;
}

static int synaptics_tci_password(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	struct tci_info *info;
	u8 lpwg_data[6];
	int ret;

	TOUCH_TRACE();

	ret = synaptics_read(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	TOUCH_I("0: %d,%d,%d,%d,%d,%d\n",
		lpwg_data[0],
		lpwg_data[1],
		lpwg_data[2],
		lpwg_data[3],
		lpwg_data[4],
		lpwg_data[5]);

	info = &ts->tci.info[0];

	info->tap_count &= 0x7;
	info->intr_delay = ts->tci.double_tap_check ? 68 : 0;
	info->tap_distance = 7;

	lpwg_data[0] |= ((info->tap_count << 3) | 1);
	lpwg_data[1] = info->min_intertap;
	lpwg_data[2] = info->max_intertap;
	lpwg_data[3] = info->touch_slop;
	lpwg_data[4] = info->tap_distance;
	lpwg_data[5] = (info->intr_delay << 1 | 1);

	ret = synaptics_write(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG,
			      lpwg_data, sizeof(lpwg_data));

	ret = synaptics_read(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG2,
			      lpwg_data, sizeof(lpwg_data));

	TOUCH_I("1: %d,%d,%d,%d,%d,%d\n",
		lpwg_data[0],
		lpwg_data[1],
		lpwg_data[2],
		lpwg_data[3],
		lpwg_data[4],
		lpwg_data[5]);

	info = &ts->tci.info[1];
	info->tap_count &= 0x7;
	lpwg_data[0] |= ((info->tap_count << 3) | 1);
	lpwg_data[1] = info->min_intertap;
	lpwg_data[2] = info->max_intertap;
	lpwg_data[3] = info->touch_slop;
	lpwg_data[4] = info->tap_distance;
	lpwg_data[5] = (info->intr_delay << 1 | 1);

	ret = synaptics_write(dev, LPWG_PAGE, LPWG_TAPCOUNT_REG2,
			      lpwg_data, sizeof(lpwg_data));

	if (ret < 0) {
		TOUCH_E("failed to write LPWG_TAPCOUNT_REG2\n");
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
	buffer[2] = (ts->lpwg.area[0].x >> 0) & 0xff;
	buffer[3] = (ts->lpwg.area[0].x >> 8) & 0xff;
	buffer[4] = (ts->lpwg.area[1].x >> 0) & 0xff;
	buffer[5] = (ts->lpwg.area[1].x >> 8) & 0xff;
	buffer[6] = (ts->lpwg.area[1].x >> 0) & 0xff;
	buffer[7] = (ts->lpwg.area[1].x >> 8) & 0xff;

	synaptics_write(dev, COMMON_PAGE, d->f12_reg.ctrl[18], buffer, sizeof(buffer));

	return 0;
}

static int synaptics_lpwg_control(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_I("synaptics_lpwg_control mode=%d\n", mode);

	switch (mode) {
	case LPWG_DOUBLE_TAP:
		ts->tci.mode = 0x01;
		synaptics_tci_knock(dev);
		synaptics_tci_report_enable(dev, true);
		break;

	case LPWG_PASSWORD:
		ts->tci.mode = 0x02;
		synaptics_tci_password(dev);
		synaptics_tci_report_enable(dev, true);
		break;

	default:
		ts->tci.mode = 0;
		ret = synaptics_tci_report_enable(dev, false);
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
			synaptics_sleep_control(dev, 1);
			synaptics_lpwg_control(dev, LPWG_NONE);
		} else if (ts->lpwg.screen) {
			TOUCH_I("%s(%d) - FB_SUSPEND & screen on -> skip\n",
				__func__, __LINE__);
			return 0;
		} else if (ts->lpwg.sensor == PROX_NEAR) {
			/* deep sleep */
			TOUCH_I("%s(%d) - deep sleep by prox\n",
				__func__, __LINE__);
			synaptics_sleep_control(dev, 1);
			synaptics_lpwg_control(dev, LPWG_NONE);
		} else if (ts->lpwg.qcover == HALL_NEAR) {
			/* knock on */
			TOUCH_I("%s(%d) - knock on by hall\n",
				__func__, __LINE__);
			synaptics_sleep_control(dev, 1);
			synaptics_lpwg_control(dev, LPWG_DOUBLE_TAP);
		} else {
			/* knock on/code */
			TOUCH_I("%s(%d) - knock %d\n",
				__func__, __LINE__, ts->lpwg.mode);
			synaptics_sleep_control(dev, 1);
			synaptics_lpwg_control(dev, ts->lpwg.mode);
		}
		return 0;
	}

	/* resume */
	if (ts->lpwg.screen) {
		/* normal */
		TOUCH_I("%s(%d) - normal\n",
				__func__, __LINE__);
		synaptics_sleep_control(dev, 1);
		synaptics_lpwg_control(dev, LPWG_NONE);
	} else if (ts->lpwg.mode == LPWG_NONE) {
		/* normal */
		TOUCH_I("%s(%d) - normal on screen off\n",
				__func__, __LINE__);
		synaptics_sleep_control(dev, 1);
		synaptics_lpwg_control(dev, LPWG_NONE);
	} else if (ts->lpwg.sensor == PROX_NEAR) {
		/* wake up */
		TOUCH_I("%s(%d) - wake up on screen off and prox\n",
				__func__, __LINE__);
		TOUCH_I("%s - wake up is not ready\n", __func__);
		synaptics_sleep_control(dev, 0);
		synaptics_lpwg_control(dev, LPWG_NONE);
	} else {
		/* partial */
		TOUCH_I("%s(%d) - parial mode\n",
				__func__, __LINE__);
		TOUCH_I("%s - partial is not ready\n", __func__);
		synaptics_sleep_control(dev, 0);
		synaptics_lpwg_control(dev, LPWG_NONE);
	}

	return 0;
}

static void synaptic_init_tci_info(struct device *dev)
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

	mutex_init(&d->io_lock);

	touch_set_device(ts, d);

	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 0);

	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	touch_power_init(dev);
	touch_bus_init(dev, 4096);

	synaptics_page_description(dev);

	synaptic_init_tci_info(dev);

	return 0;
}

static int synaptics_remove(struct device *dev)
{
	TOUCH_TRACE();

	return 0;
}

static int synaptics_fw_compare(struct device *dev, const struct firmware *fw)
{
	return 1;
#if 0
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	u8 dev_major = d->fw.version[0];
	u8 dev_minor = d->fw.version[1];
//	u32 bin_ver_offset = *((u32 *)&fw->data[0xe8]); // Unknown
	u32 bin_pid_offset = *((u32 *)&fw->data[0x10]);
	char pid[12] = {0};
	u8 bin_major;
	u8 bin_minor;
	int update = 0;

/*	if ((bin_ver_offset > FLASH_FW_SIZE) || (bin_pid_offset > FLASH_FW_SIZE)) {
		TOUCH_I("INVALID OFFSET\n");
		return -1;
	}
*/
//	bin_major = fw->data[bin_ver_offset];
	bin_minor = fw->data[bin_ver_offset + 1];
	memcpy(pid, &fw->data[bin_pid_offset], 8);
	TOUCH_I("bin...\n");

	if (ts->force_fwup) {
		update = 1;
	} else if (bin_major && dev_major) {
		if (bin_minor != dev_minor)
			update = 1;
	} else if (bin_major ^ dev_major) {
		update = 0;
	} else if (!bin_major && !dev_major) {
		if (bin_minor > dev_minor)
			update = 1;
	}

	TOUCH_I(
		"bin-ver: %d.%02d (%s), dev-ver: %d.%02d -> update: %d, force_fwup: %d\n",
		bin_major, bin_minor, pid, dev_major, dev_minor,
		update, ts->force_fwup);
	return update;

#endif
}

static int synaptics_suspend(struct device *dev)
{
	TOUCH_TRACE();

	synaptics_lpwg_mode(dev);

	return 0;
}

static int synaptics_get_status(struct device *dev, u8 *device, u8 *interrupt)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	u8 dev_status;
	u8 irq_status;
	int ret;

	ret = synaptics_read(dev, COMMON_PAGE, DEVICE_STATUS_REG,
			&dev_status, sizeof(dev_status));

	if (ret < 0) {
		TOUCH_E("failed to read device status - ret:%d\n", ret);
		return ret;
	}

	ret = synaptics_read(dev, COMMON_PAGE, INTERRUPT_STATUS_REG,
			&irq_status, sizeof(irq_status));

	if (ret < 0) {
		TOUCH_E("failed to read interrupt status - ret:%d\n", ret);
		return ret;
	}

//	TOUCH_I("status[device:%02x, interrupt:%02x]\n",
//		dev_status, irq_status);

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
	u16 object_attention = 0;
	int ret;

	ret = synaptics_read(dev, FINGER_PAGE, d->f12_reg.data[15],
			(unsigned char *)&object_attention, sizeof(object_attention));

	if (ret < 0) {
		TOUCH_E("%s, %d : get object_attention data failed\n",
			__func__, __LINE__);
		return ret;
	}
	for (; object_to_read > 0 ;) {
		if (object_attention & (0x1 << (object_to_read - 1)))
			break;

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
		ret = synaptics_read(dev, FINGER_PAGE, FINGER_DATA_REG,
				     (unsigned char *)objects, sizeof(*obj) * object_to_read);
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

	ret = synaptics_read(dev, LPWG_PAGE, LPWG_DATA_REG,
					 (unsigned char *)buffer, sizeof(u32) * count);

	if (ret < 0)
		return ret;

	for (i = 0; i < count; i++) {
		ts->lpwg.code[i].x = buffer[0] & 0xffff;
		ts->lpwg.code[i].y = (buffer[0] >> 16) & 0xffff;

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

	ret = synaptics_read(dev, LPWG_PAGE, LPWG_STATUS_REG, &status, 1);
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

	return ret;
}

static int synaptics_irq_handler(struct device *dev)
{
	u8 dev_status;
	u8 irq_status;
	int ret;

	ret = synaptics_get_status(dev, &dev_status, &irq_status);

	if (irq_status & INTERRUPT_MASK_ABS0)
		ret = synaptics_irq_abs(dev);

	if (irq_status & INTERRUPT_MASK_LPWG)
		ret = synaptics_irq_lpwg(dev);

	return ret;
}

static ssize_t store_reg_ctrl(struct device *dev,
				const char *buf, size_t count)
{
	char command[6] = {0};
	u16 reg = 0;
	int value = 0;
	int offset = 0;
	u8 buffer[50] = {0};

	int page = 0;

	if (sscanf(buf, "%5s %d %hx %x %x", command, &page, &reg, &offset,
				&value) <= 0)
		return count;

	if (!strcmp(command, "write")) {
		if (synaptics_read(dev, page, reg, (unsigned char *)&buffer[0],
					offset+1) < 0)
			TOUCH_E("reg addr 0x%x read fail\n", reg);

		buffer[offset] = (u8)value;

		if (synaptics_write(dev, page, reg, (unsigned char *)&buffer[0],
					offset+1) < 0)
			TOUCH_E("reg addr 0x%x write fail\n", reg);
		else
			TOUCH_I("page[%x] reg[%x] offset[%x] = 0x%x\n",
					page, reg, offset, buffer[offset]);
	} else if (!strcmp(command, "read")) {
		if (synaptics_read(dev, page, reg, (unsigned char *)&buffer[0],
					offset+1) < 0)
			TOUCH_E("reg addr 0x%x read fail\n", reg);
		else
			TOUCH_I("page[%x] reg[%x] offset[%x] = 0x%x\n",
					page, reg, offset, buffer[offset]);
	} else {
		TOUCH_D(BASE_INFO, "Usage\n");
		TOUCH_D(BASE_INFO, "Write page reg offset value\n");
		TOUCH_D(BASE_INFO, "Read page reg offset\n");
	}
	return count;
}

static ssize_t store_irq_enable_ctrl(struct device *dev,
				const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (atomic_read(&ts->state.irq_enable) != value) {
		if (value == INTERRUPT_DISABLE) {
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			TOUCH_I("interrupt irq disable \n");
		} else {
			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			TOUCH_I("interrupt irq enable \n");
		}
	} else {
		TOUCH_I("Already %s irq \n", (value ? "enable":"disable"));
	}
	return count;
}

static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(irq_enable, NULL, store_irq_enable_ctrl);

static struct attribute *synaptics_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_irq_enable.attr,
	NULL,
};

static const struct attribute_group synaptics_attribute_group = {
	.attrs = synaptics_attribute_list,
};

static int synaptics_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &synaptics_attribute_group);
	if (ret < 0)
		TOUCH_E("synaptics sysfs register failed\n");

	return 0;
}

static int synaptics_f51_remap(struct device *dev)
{
	return 0;
}

static int synaptics_rmidev_init(struct device *dev)
{
	int ret = 0;

	TOUCH_TRACE();

	if (rmidev_fhandler.insert) { //TODO DEBUG_OPTION_2
		ret = rmidev_fhandler.exp_fn->init(dev);

		if (ret < 0) {
			TOUCH_I("%s : Failed to init rmi_dev settings\n", __func__);
		} else {
			rmidev_fhandler.initialized = true;
		}
	}

	return ret;
}

static int synaptics_init(struct device *dev)
{
	TOUCH_TRACE();

	synaptics_ic_info(dev);

	synaptics_f51_remap(dev);

	synaptics_irq_enable(dev, true);
	synaptics_irq_clear(dev);

	synaptics_rmidev_init(dev);

	return 0;
}

static int synaptics_power(struct device *dev, int ctrl)
{
	#if 0
	struct touch_core_data *ts = to_touch_core(dev);
	#endif

	TOUCH_TRACE();

	switch (ctrl) {
	case POWER_OFF:
		TOUCH_I("%s, off\n", __func__);
		#if 0
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_power_vio(dev, 0);
		touch_power_vdd(dev, 0);
		#endif
		break;

	case POWER_ON:
		TOUCH_I("%s, on\n", __func__);
		#if 0
		touch_power_vdd(dev, 1);
		touch_power_vio(dev, 1);
		touch_gpio_direction_output(ts->reset_pin, 1);
		#endif
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
	int *value = (int *)param;

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
		TOUCH_I(
			"LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
			ts->lpwg.mode,
			ts->lpwg.screen ? "ON" : "OFF",
			ts->lpwg.sensor ? "FAR" : "NEAR",
			ts->lpwg.qcover ? "CLOSE" : "OPEN");
		synaptics_lpwg_mode(dev);
		break;
	}
	return 0;
}

static int synaptics_notify(struct device *dev, ulong event, void *data)
{
	TOUCH_TRACE();

	return 0;
}

static int synaptics_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

static int synaptics_get(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

irqreturn_t synaptcis_fw_irq_thread(int irq, void *dev_id)
{
	struct touch_core_data *ts = (struct touch_core_data *) dev_id;
	struct synaptics_data *d = to_synaptics_data(ts->dev);
	int ret = 0;
	u8 irq_status;

	TOUCH_TRACE();

	ret = synaptics_read(ts->dev, COMMON_PAGE, INTERRUPT_STATUS_REG,
			&irq_status, sizeof(irq_status));

	if (ret < 0) {
		TOUCH_E("failed to read irq status - ret:%d\n", ret);
		return ret;
	}

	if (irq_status & INTERRUPT_MASK_FLASH) {
		synaptics_rmi4_fwu_attn(ts->dev);
	}
	return IRQ_HANDLED;
}

static int synaptics_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);
	const struct firmware *fw = NULL;
	char fwpath[256] = {0};
	int ret, result = 0;
	unsigned char command = 0x01;
	unsigned char zero = 0x00;
	TOUCH_TRACE();

	if (ts->force_fwup == 0) {
		TOUCH_I("do not upgrade while probe\n");
		return -EPERM;
	}

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

	if (synaptics_fw_compare(dev, fw)) {
		free_irq(ts->irq, ts);

		ret = request_threaded_irq(ts->irq, NULL,
				synaptcis_fw_irq_thread, ts->irqflags | IRQF_ONESHOT,
				LGE_TOUCH_NAME, ts);
		if (ret) {
			TOUCH_E("failed to request_thread_irq(irq:%d, ret:%d)\n",
				ts->irq, ret);
			goto reset;
		}

		ret = synaptics_write(dev, DEFAULT_PAGE,
				INTERRUPT_ENABLE_REG,
				&zero,
				sizeof(zero));
		if (ret < 0)
			goto change_irq_handle;

		ret = synaptics_fw_updater(dev, fw);
		if (ret < 0) {
			TOUCH_E("FW upgrade Fail \n");
			goto change_irq_handle;
		}
	} else {
		release_firmware(fw);
		return -EPERM;
	}

change_irq_handle:
	disable_irq(ts->irq);
	free_irq(ts->irq, ts);

	result = request_threaded_irq(ts->irq, touch_irq_handler,
			touch_irq_thread, ts->irqflags | IRQF_ONESHOT,
			LGE_TOUCH_NAME, ts);
	if (result) {
		TOUCH_E("failed to request_thread_irq(irq:%d, ret:%d)\n",
			ts->irq, result);
	}
	disable_irq(ts->irq);
	atomic_set(&ts->state.irq_enable, 0);
reset:
	/*sw reset*/
	ret = synaptics_write(dev, COMMON_PAGE,
		DEVICE_COMMAND_REG,
		&command,
		sizeof(command));
	if(ret < 0)
		TOUCH_I("reset fail\n");

	touch_msleep(90);
	release_firmware(fw);
	return 0;
}

void synaptics_rmidev_function(struct synaptics_rmidev_exp_fn *exp_fn,
		bool insert)
{
	TOUCH_TRACE();

	rmidev_fhandler.insert = insert;

	if (insert) {
		rmidev_fhandler.exp_fn = exp_fn;
		rmidev_fhandler.insert = true;
		rmidev_fhandler.remove = false;
	} else {
		rmidev_fhandler.exp_fn = NULL;
		rmidev_fhandler.insert = false;
		rmidev_fhandler.remove = true;
	}

	return;
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

#define MATCH_NAME			"lge,td4302"

static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
};

static struct touch_hwif hwif = {
	.bus_type = HWIF_SPI,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
	.bits_per_word = 8,
	.spi_mode = SPI_MODE_3,
	.max_freq = (10 * 1000000),
};

static int __init touch_device_init(void)
{
	TOUCH_TRACE();

	if (touch_get_device_type() != TYPE_TD4302 ) {
		TOUCH_I("%s, td4302 returned\n", __func__);
		return 0;
	}
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
