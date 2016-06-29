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
#include <linux/cdev.h>
#include <linux/firmware.h>
#include <linux/input/lge_touch_notify.h>

#define MAX_POINT_SIZE_FOR_LPWG			256
#define FW_VER_INFO_NUM				4
#define MAX_NUM_OF_FINGERS			10
#define PAGE_SELECT_REG				0xff
#define PAGES_TO_SERVICE			10
#define PDT_START				0x00e9
#define PDT_END					0x00D0
#define TRX_MAX					32

#define DEFAULT_PAGE			0x00
#define COMMON_PAGE			(d->f01.page)
#define FINGER_PAGE			(d->f12.page)
#define ANALOG_PAGE			(d->f54.page)
#define FLASH_PAGE			(d->f34.page)
#define LPWG_PAGE			(d->f51.page)

#define DEVICE_CONTROL_REG		(d->f01.dsc.control_base + 0)
#define DEVICE_CONTROL_NORMAL_OP	0x00
#define DEVICE_CONTROL_SLEEP		0x01
#define	DEVICE_CONTROL_NOSLEEP		0x04
#define DEVICE_CONTROL_CONFIGURED	0x80

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
#define LPWG_BLKSIZ			7 /* 4-page */
#define LPWG_PARTIAL_REG		(LPWG_INTERRUPT_DELAY_REG2 + 35)

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

enum {
	SET_SLEEP_STATUS = 0,
	SET_NO_SLEEP,
	SET_EARLY_RESET,
	SET_POWER_OFF,
	SET_DEEP_ACTIVE,
};

enum {
	LPWG_ON = 0,
	LPWG_OFF,
	LPWG_NO_SLEEP,
};

enum{
	NOISE_DISABLE = 0,
	NOISE_ENABLE,
};

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
	u8 general_control;
};

struct synaptics_fw_info {
	u8 version[5];
	u8 product_id[11];
	u8 img_version[5];
	u8 img_product_id[11];
	unsigned char *fw_start;
	u8 family;
	u8 revision;
	unsigned long fw_size;
	u8 need_rewrite_firmware;
	u8 panel_id;
	const char *def_fw;
	u32 fw_index;
	u32 fw_ver_addr;
	u32 fw_pid_addr;
};

struct synaptics_noise_ctrl {
	u8 noise_log;
	u8 check_noise;
	u8 cnt;
	u8 cns_avg;
	u8 im_avg;
	u8 cid_im_avg;
	u8 freq_scan_im_avg;
};

struct function_descriptor {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count;
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
	struct synaptics_noise_ctrl noise;

	u8 curr_page;
	u8 object_report;
	u8 num_of_fingers;
	u8 lpwg_mask;
	u8 power_state;
	bool lpwg_by_lcd_notifier;
	bool need_scan_pdt;
};

struct synaptics_exp_fn {
	int (*init)(struct device *dev);
	void (*remove)(struct device *dev);
	void (*reset)(struct device *dev);
	void (*reinit)(struct device *dev);
	void (*early_suspend)(struct device *dev);
	void (*suspend)(struct device *dev);
	void (*resume)(struct device *dev);
	void (*late_resume)(struct device *dev);
	void (*attn)(unsigned char intr_status_reg);
};

struct synaptics_exp_fhandler {
	struct synaptics_exp_fn *exp_fn;
	bool inserted;
	bool initialized;
};

struct prd_handle {
	struct touch_core_data *c_data;
};

static inline struct synaptics_data *to_synaptics_data(struct device *dev)
{
	return (struct synaptics_data *)touch_get_device(to_touch_core(dev));
}

bool synaptics_is_product(struct synaptics_data *d,
					const char *product_id, size_t len);
bool synaptics_is_img_product(struct synaptics_data *d,
					const char *product_id, size_t len);
int synaptics_set_page(struct device *dev, u8 page);
int synaptics_init(struct device *dev);
int synaptics_ic_info(struct device * dev);
int synaptics_read(struct device *dev, u16 addr, void *data, int size);
int synaptics_write(struct device *dev, u16 addr, void *data, int size);
void synaptics_rmidev_function(struct synaptics_exp_fn *rmidev_fn, bool insert);
irqreturn_t touch_irq_handler(int irq, void *dev_id);
irqreturn_t touch_irq_thread(int irq, void *dev_id);

/* extern function */
extern int synaptics_prd_handle(struct device *dev);
extern int FirmwareUpgrade(struct device *dev, const struct firmware *fw);

#endif /* LGE_TOUCH_SYNAPTICS_H */
