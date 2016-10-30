/* touch_synaptics.h
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

#ifndef LGE_TOUCH_SYNAPTICS_H
#define LGE_TOUCH_SYNAPTICS_H

#include <linux/device.h>

#define DEFAULT_PAGE			0x00
#define COMMON_PAGE			(d->f01.page)
#define FINGER_PAGE			(d->f12.page)
#define ANALOG_PAGE			(d->f54.page)
#define FLASH_PAGE			(d->f34.page)
#define LPWG_PAGE			(d->f51.page)

#define DEVICE_CONTROL_REG		(d->f01.dsc.control_base + 0)
#define DEVICE_CONTROL_SLEEP		0x01
#define	DEVICE_CONTROL_NOSLEEP		0x04

#define INTERRUPT_ENABLE_REG		(d->f01.dsc.control_base + 1)
#define DOZE_INTERVAL_REG		(d->f01.dsc.control_base + 2)

#define DEVICE_STATUS_REG		(d->f01.dsc.data_base + 0)
#define INTERRUPT_STATUS_REG		(d->f01.dsc.data_base + 1)
#define INTERRUPT_MASK_FLASH		(1 << 0)
#define INTERRUPT_MASK_STATUS		(1 << 1)
#define INTERRUPT_MASK_ABS0		(1 << 2)
#define INTERRUPT_MASK_BUTTON		(1 << 4)
#define INTERRUPT_MASK_CUSTOM		(1 << 6)
#define INTERRUPT_MASK_LPWG		INTERRUPT_MASK_CUSTOM

#define FINGER_DATA_REG			(d->f12.dsc.data_base + 0)
#define F12_NO_OBJECT_STATUS		(0x00)
#define F12_FINGER_STATUS		(0x01)
#define F12_STYLUS_STATUS		(0x02)
#define F12_PALM_STATUS			(0x03)
#define F12_HOVERING_FINGER_STATUS	(0x05)
#define F12_GLOVED_FINGER_STATUS	(0x06)
#define F12_MAX_OBJECT			(0x06)

#define LPWG_STATUS_REG			(d->f51.dsc.data_base)
#define LPWG_STATUS_DOUBLETAP		(1 << 0)
#define LPWG_STATUS_PASSWORD		(1 << 1)
#define LPWG_DATA_REG			(d->f51.dsc.data_base + 1)
#define LPWG_OVER_TAPCOUNT		(d->f51.dsc.data_base + 73)

#define LPWG_TAPCOUNT_REG		(d->f51.dsc.control_base)
#define LPWG_MIN_INTERTAP_REG		(d->f51.dsc.control_base + 1)
#define LPWG_MAX_INTERTAP_REG		(d->f51.dsc.control_base + 2)
#define LPWG_TOUCH_SLOP_REG		(d->f51.dsc.control_base + 3)
#define LPWG_TAP_DISTANCE_REG		(d->f51.dsc.control_base + 4)
#define LPWG_INTERRUPT_DELAY_REG	(d->f51.dsc.control_base + 6)

#define LPWG_TAPCOUNT_REG2		(d->f51.dsc.control_base + 7)
#define LPWG_MIN_INTERTAP_REG2		(d->f51.dsc.control_base + 8)
#define LPWG_MAX_INTERTAP_REG2		(d->f51.dsc.control_base + 9)
#define LPWG_TOUCH_SLOP_REG2		(d->f51.dsc.control_base + 10)
#define LPWG_TAP_DISTANCE_REG2		(d->f51.dsc.control_base + 11)
#define LPWG_INTERRUPT_DELAY_REG2	(d->f51.dsc.control_base + 13)

#if defined(ENABLE_TCI_DEBUG)
#define LPWG_TCI1_FAIL_COUNT_REG	(d->f51.dsc.data_base + 49)
#define LPWG_TCI1_FAIL_INDEX_REG	(d->f51.dsc.data_base + 50)
#define LPWG_TCI1_FAIL_BUFFER_REG	(d->f51.dsc.data_base + 51)
#define LPWG_TCI2_FAIL_COUNT_REG	(d->f51.dsc.data_base + 61)
#define LPWG_TCI2_FAIL_INDEX_REG	(d->f51.dsc.data_base + 62)
#define LPWG_TCI2_FAIL_BUFFER_REG	(d->f51.dsc.data_base + 63)
#endif

#define FINGER_REPORT_REG		(d->f12_reg.ctrl[20])

/* Flash Memory Management */
#define FLASH_CONFIG_ID_REG             (d->f34.dsc.control_base)
#define FLASH_CONTROL_REG               (d->f34.dsc.data_base + 2)
#define FLASH_STATUS_REG                (d->f34.dsc.data_base + 3)
#define FLASH_STATUS_MASK               0xFF

/* RMI_DEVICE_CONTROL */
/* Manufacturer ID */
#define MANUFACTURER_ID_REG             (d->f01.dsc.query_base)
/* CUSTOMER_FAMILY QUERY */
#define CUSTOMER_FAMILY_REG             (d->f01.dsc.query_base + 2)
/* FW revision */
#define FW_REVISION_REG                 (d->f01.dsc.query_base + 3)
/* Product ID */
#define PRODUCT_ID_REG                  (d->f01.dsc.query_base + 11)
#define DEVICE_COMMAND_REG              (d->f01.dsc.command_base)


#include <linux/spi/spi.h>

#define SPI_READ 0x80
#define SPI_WRITE 0x00

#define MASK_7BIT 0x7F
#define MASK_8BIT 0xFF

#define PAGE_SELECT_LEN (2)
#define ADDRESS_WORD_LEN (2)

#define SPI_DELAY 200

#define PLATFORM_DRIVER_NAME "synaptics_dsx"

#define MAX_NUM_OF_FINGERS			10
#define PAGE_SELECT_REG				0xff
#define PAGES_TO_SERVICE			10
#define PDT_START				0x00e9
#define PDT_END					0x00D0

struct synaptics_f12_reg {
	u8 ctrl[32];
	u8 data[16];
};

struct synaptics_f51_reg {
	u8 lpwg_status_reg;
	u8 lpwg_data_reg;
	u8 lpwg_tapcount_reg;
	u8 lpwg_min_intertap_reg;
	u8 lpwg_max_intertap_reg;
	u8 lpwg_touch_slop_reg;
	u8 lpwg_tap_distance_reg;
	u8 lpwg_interrupt_delay_reg;
	u8 lpwg_tapcount_reg2;
	u8 lpwg_min_intertap_reg2;
	u8 lpwg_max_intertap_reg2;
	u8 lpwg_touch_slop_reg2;
	u8 lpwg_tap_distance_reg2;
	u8 lpwg_interrupt_delay_reg2;
	u8 overtap_cnt_reg;
	u8 request_reset_reg;
	u8 lpwg_partial_reg;
	u8 lpwg_fail_count_reg;
	u8 lpwg_fail_index_reg;
	u8 lpwg_fail_reason_reg;
	u8 lpwg_adc_offset_reg;
	u8 lpwg_adc_fF_reg1;
	u8 lpwg_adc_fF_reg2;
	u8 lpwg_adc_fF_reg3;
	u8 lpwg_adc_fF_reg4;
};

struct synaptics_f54_reg {
	u8 interference__metric_LSB;
	u8 interference__metric_MSB;
	u8 current_noise_status;
	u8 cid_im;
	u8 freq_scan_im;
	u8 incell_statistic;
};

struct synaptics_fw_info {
	u8 version[5];
	u8 product_id[11];
	u8 img_version[5];
	u8 img_product_id[11];
	u8 *fw_start;
	u8 family;
	u8 revision;
	unsigned long fw_size;
	u8 need_rewrite_firmware;
};

struct function_descriptor {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count:3;
	u8 reserved_1:2;
	u8 fn_version:2;
	u8 reserved_2:1;
	u8 fn_number;
};

struct synaptics_function {
	struct function_descriptor dsc;
	u8 page;
};

struct synaptics_object {
	u8 type;
	u8 x_lsb;
	u8 x_msb;
	u8 y_lsb;
	u8 y_msb;
	u8 z;
	u8 wx;
	u8 wy;
};

struct synaptics_rmidev_exp_fn {
	int (*init)(struct device *dev);
	void (*remove)(struct device *dev);
	void (*reset)(struct device *dev);
	void (*reinit)(struct device *dev);
	void (*early_suspend)(struct device *dev);
	void (*suspend)(struct device *dev);
	void (*resume)(struct device *dev);
	void (*late_resume)(struct device *dev);
	void (*attn)(struct device *dev,
			unsigned char intr_mask);
};

struct synaptics_rmidev_exp_fhandler {
	struct synaptics_rmidev_exp_fn *exp_fn;
	bool initialized;
	bool insert;
	bool remove;
};

struct synaptics_data {
	struct synaptics_function f01;
	struct synaptics_function f11;
	struct synaptics_function f12;
	struct synaptics_function f1a;
	struct synaptics_function f34;
	struct synaptics_function f51;
	struct synaptics_function f54;
	struct synaptics_function f55;
	struct synaptics_f12_reg f12_reg;
	struct synaptics_f51_reg f51_reg;
	struct synaptics_f54_reg f54_reg;
	struct synaptics_fw_info fw;
	u8 curr_page;
	u8 object_report;
	u8 num_of_fingers;
	u8 irq_mask;
	struct mutex io_lock;
};

static inline struct synaptics_data *to_synaptics_data(struct device *dev)
{
	return (struct synaptics_data *)touch_get_device(to_touch_core(dev));
}
extern int synaptics_fw_updater(struct device *dev,	const struct firmware *fw);
extern void synaptics_rmi4_fwu_attn(struct device *dev);
extern int synaptics_read (struct device *dev, u8 page, u16 addr, void *data, int size);
extern int synaptics_write(struct device *dev, u8 page, u16 addr, void *data, int size);
void synaptics_rmidev_function(struct synaptics_rmidev_exp_fn *exp_fn, bool insert);

#endif /* LGE_TOUCH_SYNAPTICS_H */
