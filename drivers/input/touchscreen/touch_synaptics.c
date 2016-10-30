/* Touch_synaptics.c
 *
 * Copyright (C) 2013 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regulator/machine.h>
#include <linux/async.h>
#include <linux/atomic.h>
#include <linux/gpio.h>
#include <linux/file.h>     /*for file access*/
#include <linux/syscalls.h> /*for file access*/
#include <linux/uaccess.h>  /*for file access*/
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/input/lge_touch_core.h>
#include <linux/input/touch_synaptics.h>
#include <linux/firmware.h>
#include "./DS5/RefCode_F54.h"
//#include <soc/qcom/lge/board_lge.h>
/* RMI4 spec from 511-000405-01 Rev.D
 * Function	Purpose				See page
 * $01		RMI Device Control			45
 * $1A		0-D capacitive button sensors	61
 * $05		Image Reporting			68
 * $07		Image Reporting			75
 * $08		BIST				82
 * $09		BIST				87
 * $11		2-D TouchPad sensors		93
 * $19		0-D capacitive button sensors	141
 * $30		GPIO/LEDs			148
 * $31		LEDs				162
 * $34		Flash Memory Management		163
 * $36		Auxiliary ADC			174
 * $54		Test Reporting			176
 */
struct i2c_client *ds4_i2c_client;
static char power_state;
static char *productcode_parse(unsigned char *product);
static int get_ic_info(struct synaptics_ts_data *ts);
static int read_page_description_table(struct i2c_client *client);
static int get_type_bootloader(struct i2c_client *client);
static void synaptics_change_sleepmode(struct i2c_client *client);
static void synaptics_toggle_swipe(struct i2c_client *client);
static int synaptics_ts_im_test(struct i2c_client *client);
static int synaptics_ts_adc_test(struct i2c_client *client);
static int synaptics_ts_lpwg_adc_test(struct i2c_client *client);

int f54_window_crack_check_mode = 0;
int f54_window_crack;
int after_crack_check = 0;
static unsigned long im_sum;
static unsigned long cns_sum;
static unsigned long cid_im_sum;
static unsigned long freq_scan_im_sum;
static u16 im_aver;
static u16 cns_aver;
static u16 cid_im_aver;
static u16 freq_scan_im_aver;
static unsigned int cnt;
u8 int_mask_cust;
int is_sensing;
bool lpwg_by_lcd_notifier;
bool ghost_do_not_reset;
int sp_link_touch;
int incoming_call_state = 0;

/*static int ts_suspend = 0;
int thermal_status = 0;
extern int touch_thermal_mode;*/

/* Register Map & Register bit mask
 * - Please check "One time" this map before using this device driver
 */
/* RMI_DEVICE_CONTROL */
/* Manufacturer ID */
#define MANUFACTURER_ID_REG		(ts->f01.dsc.query_base)
/* CUSTOMER_FAMILY QUERY */
#define CUSTOMER_FAMILY_REG		(ts->f01.dsc.query_base + 2)
/* FW revision */
#define FW_REVISION_REG			(ts->f01.dsc.query_base + 3)
/* Product ID */
#define PRODUCT_ID_REG			(ts->f01.dsc.query_base + 11)
#define DEVICE_COMMAND_REG		(ts->f01.dsc.command_base)

/* Device Control */
#define DEVICE_CONTROL_REG		(ts->f01.dsc.control_base)
/* sleep mode : go to doze mode after 500 ms */
#define DEVICE_CONTROL_NORMAL_OP	0x00
/* sleep mode : go to sleep */
#define DEVICE_CONTROL_SLEEP		0x01
/* sleep mode : go to sleep. no-recalibration */
#define DEVICE_CONTROL_SLEEP_NO_RECAL	0x02
#define DEVICE_CONTROL_NOSLEEP		0x04
#define DEVICE_CHARGER_CONNECTED	0x20
#define DEVICE_CONTROL_CONFIGURED	0x80

/* Device Command */
#define DEVICE_COMMAND_RESET		0x01

/* Interrupt Enable 0 */
#define INTERRUPT_ENABLE_REG		(ts->f01.dsc.control_base + 1)
/* Doze Interval : unit 10ms */
#define DOZE_INTERVAL_REG               (ts->f01.dsc.control_base + 2)
#define DOZE_WAKEUP_THRESHOLD_REG       (ts->f01.dsc.control_base + 3)

/* Device Status */
#define DEVICE_STATUS_REG		(ts->f01.dsc.data_base)
#define DEVICE_FAILURE_MASK		0x03
#define DEVICE_CRC_ERROR_MASK		0x04
#define DEVICE_STATUS_FLASH_PROG	0x40
#define DEVICE_STATUS_UNCONFIGURED	0x80

/* Interrupt Status */
#define INTERRUPT_STATUS_REG		(ts->f01.dsc.data_base + 1)
#define INTERRUPT_MASK_FLASH		0x01
#define INTERRUPT_MASK_STATUS		0x02
#define INTERRUPT_MASK_ABS0		0x04
#define INTERRUPT_MASK_BUTTON		0x10
#define INTERRUPT_MASK_CUSTOM		0x40

/* TOUCHPAD_SENSORS */
#define FINGER_COMMAND_REG		(ts->f12.dsc.command_base)
#define MOTION_SUPPRESSION		(ts->f12.dsc.control_base + 5)
/* ts->f12_reg.ctrl[20] */
#define GLOVED_FINGER_MASK		0x20

/* Finger State */
#define OBJECT_TYPE_AND_STATUS_REG	(ts->f12.dsc.data_base)
#define OBJECT_ATTENTION_REG		(ts->f12.dsc.data_base + 2)
/* Finger Data Register */
#define FINGER_DATA_REG_START		(ts->f12.dsc.data_base)
#define REG_OBJECT_TYPE_AND_STATUS	0
#define REG_X_LSB			1
#define REG_X_MSB			2
#define REG_Y_LSB			3
#define REG_Y_MSB			4
#define REG_Z				5
#define REG_WX				6
#define REG_WY				7

#define MAXIMUM_XY_COORDINATE_REG	(ts->f12.dsc.control_base)

/* ANALOG_CONTROL */
#define ANALOG_COMMAND_REG		(ts->f54.dsc.command_base)
#define ANALOG_CONTROL_REG		(ts->f54.dsc.control_base)
#define SATURATION_CAP_LSB_REG		(ts->f54.dsc.control_base + 1)
#define SATURATION_CAP_MSB_REG		(ts->f54.dsc.control_base + 2)
#define THERMAL_UPDATE_INTERVAL_REG     0x2F      /* 1-page */

/* FLASH_MEMORY_MANAGEMENT */
/* Flash Control */
#define FLASH_CONFIG_ID_REG		(ts->f34.dsc.control_base)
#define FLASH_CONTROL_REG		(ts->f34.dsc.data_base + 2)
#define FLASH_STATUS_REG		(ts->f34.dsc.data_base + 3)
#define FLASH_STATUS_MASK		0xFF

/* Page number */
#define COMMON_PAGE			(ts->f01.page)
#define FINGER_PAGE			(ts->f12.page)
#define ANALOG_PAGE			(ts->f54.page)
#define FLASH_PAGE			(ts->f34.page)
#define DEFAULT_PAGE			0x00
#define LPWG_PAGE			(ts->f51.page)

/* Others */
#define LPWG_STATUS_REG			(ts->f51.dsc.data_base)
#define LPWG_DATA_REG			(ts->f51.dsc.data_base + 1)
#define LPWG_TAPCOUNT_REG		(ts->f51.dsc.control_base)
#define LPWG_MIN_INTERTAP_REG		(ts->f51.dsc.control_base + 1)
#define LPWG_MAX_INTERTAP_REG		(ts->f51.dsc.control_base + 2)
#define LPWG_TOUCH_SLOP_REG		(ts->f51.dsc.control_base + 3)
#define LPWG_TAP_DISTANCE_REG		(ts->f51.dsc.control_base + 4)
#define LPWG_INTERRUPT_DELAY_REG	(ts->f51.dsc.control_base + 6)
#define LPWG_BLKSIZ			7 /* 4-page */

#define LPWG_TAPCOUNT_REG2		(LPWG_TAPCOUNT_REG + LPWG_BLKSIZ)
#define LPWG_MIN_INTERTAP_REG2		(LPWG_MIN_INTERTAP_REG + LPWG_BLKSIZ)
#define LPWG_MAX_INTERTAP_REG2		(LPWG_MAX_INTERTAP_REG + LPWG_BLKSIZ)
#define LPWG_TOUCH_SLOP_REG2		(LPWG_TOUCH_SLOP_REG + LPWG_BLKSIZ)
#define LPWG_TAP_DISTANCE_REG2		(LPWG_TAP_DISTANCE_REG + LPWG_BLKSIZ)
#define LPWG_INTERRUPT_DELAY_REG2	(LPWG_INTERRUPT_DELAY_REG + LPWG_BLKSIZ)
#define LPWG_PARTIAL_REG		(LPWG_INTERRUPT_DELAY_REG2 + 35)
#define MISC_HOST_CONTROL_REG		\
	(ts->f51.dsc.control_base + 7 + LPWG_BLKSIZ)

/* finger_amplitude(0x80) = 0.5 */
#define THERMAL_HIGH_FINGER_AMPLITUDE	0x60

#define LPWG_HAS_DEBUG_MODULE		(ts->f51.dsc.query_base + 4)

#define LPWG_MAX_BUFFER			10

/* LPWG Control Value */
#define REPORT_MODE_CTRL	1
#define TCI_ENABLE_CTRL		2
#define TAP_COUNT_CTRL		3
#define MIN_INTERTAP_CTRL	4
#define MAX_INTERTAP_CTRL	5
#define TOUCH_SLOP_CTRL		6
#define TAP_DISTANCE_CTRL	7
#define INTERRUPT_DELAY_CTRL	8
#define PARTIAL_LPWG_ON		9

#define TCI_ENABLE_CTRL2	22
#define TAP_COUNT_CTRL2		23
#define MIN_INTERTAP_CTRL2	24
#define MAX_INTERTAP_CTRL2	25
#define TOUCH_SLOP_CTRL2	26
#define TAP_DISTANCE_CTRL2	27
#define INTERRUPT_DELAY_CTRL2   28

/* Palm / Hover */
#define PALM_TYPE		3
#define HOVER_TYPE		5
#define MAX_PRESSURE		255

#define I2C_DELAY		50
#define UEVENT_DELAY		200
#define REBASE_DELAY		100
#define CAP_DIFF_MAX		500
#define CAP_MIN_MAX_DIFF	1000
#define KNOCKON_DELAY		68 /* 700ms */
#define KNOCKCODE_DELAY		20 /* 200ms */

/* F/W calibration */
#define CALIBRATION_STATUS_REG	(ts->f54.dsc.data_base + 14)
#define CALIBRATION_FLAGS_REG	(ts->f54.dsc.control_base + 35)
#define F54_FIFO_INDEX_LSB		(ts->f54.dsc.data_base + 1)
#define F54_FIFO_INDEX_MSB		(ts->f54.dsc.data_base + 2)
#define F54_REPORT_DATA		(ts->f54.dsc.data_base + 3)
#define MAX_CAL_DATA_SIZE	(32*18*2)
#define MAX_ND_CAL_DATA_SIZE	(32*2*2)
#define MAX_DETAIL_SIZE         (32*18)
#define MAX_COARSE_SIZE         (32*18)
#define MAX_FINE_SIZE           (32*18)
#define MAX_ND_DETAIL_SIZE	(32*2)
#define MAX_ND_COARSE_SIZE	(32*2)
#define MAX_ND_FINE_SIZE	(32*2)
#define MAX_CAL_LOG_SIZE (MAX_CAL_DATA_SIZE*20)

/* Get user-finger-data from register.
 */
#define TS_POSITION(_msb_reg, _lsb_reg) \
	(((u16)((_msb_reg << 8)  & 0xFF00)  | (u16)((_lsb_reg) & 0xFF)))

#define TS_SNTS_GET_ORIENTATION(_width_y, _width_x) \
	(((_width_y - _width_x) > 0) ? 0 : 1)
#define TS_SNTS_GET_PRESSURE(_pressure) \
	_pressure
#define jitter_abs(x)	(x > 0 ? x : -x)

#define GET_OBJECT_REPORT_INFO(_reg, _type) \
	(((_reg) & ((u8)(1 << (_type)))) >> (_type))

#define GET_HIGH_U8_FROM_U16(_u16_data) \
	((u8)(((_u16_data) & 0xFF00) >> 8))

#define GET_LOW_U8_FROM_U16(_u16_data) \
	((u8)((_u16_data) & 0x00FF))

#define GET_U16_FROM_U8(_u8_hi_data, _u8_lo_data) \
	((u16)(((_u8_hi_data) << 8) | (_u8_lo_data)))

static int ref_chk_enable;
static int raw_cap_file_exist;
static bool touch_wake_test;
unsigned int  touch_wake_count;
#define TOUCH_WAKE_COUNTER_LOG_PATH		"/mnt/sdcard/wake_cnt.txt"
static enum error_type synaptics_ts_ic_ctrl(struct i2c_client *client,
		u8 code, u32 value, u32 *ret);
static int set_doze_param(struct synaptics_ts_data *ts, int value);
static bool need_scan_pdt = true;

bool is_product(struct synaptics_ts_data *ts,
		const char *product_id, size_t len)
{
	return strncmp(ts->fw_info.product_id, product_id, len)
			? false : true;
}

bool is_img_product(struct synaptics_ts_data *ts,
		const char *product_id, size_t len)
{
	return strncmp(ts->fw_info.img_product_id, product_id, len)
			? false : true;
}

void write_firmware_version_log(struct synaptics_ts_data *ts)
{
#define LOGSIZ 448
	char *version_string = NULL;
	int ret = 0;
	int rc = 0;
	version_string = kzalloc(LOGSIZ * sizeof(char), GFP_KERNEL);

	if (mfts_mode) {
		mutex_lock(&ts->pdata->thread_lock);
		read_page_description_table(ts->client);
		rc = get_ic_info(ts);
		mutex_unlock(&ts->pdata->thread_lock);
		if (rc < 0) {
			ret += snprintf(version_string + ret,
					LOGSIZ - ret, "-1\n");
			ret += snprintf(version_string + ret, LOGSIZ - ret,
					"Read Fail Touch IC Info\n");
			return;
		}
	}
	ret += snprintf(version_string + ret, LOGSIZ - ret,
			"===== Firmware Info =====\n");

	if (ts->fw_info.version[0] > 0x50) {
		ret += snprintf(version_string + ret, LOGSIZ - ret,
				"ic_version[%s]\n", ts->fw_info.version);
	} else {
		ret += snprintf(version_string + ret, LOGSIZ - ret,
				"version : v%d.%02d\n",
				((ts->fw_info.version[3] & 0x80) >> 7),
				(ts->fw_info.version[3] & 0x7F));
	}

	ret += snprintf(version_string + ret,
			LOGSIZ - ret,
			"IC_product_id[%s]\n",
			ts->fw_info.product_id);

	if (is_product(ts, "PLG349", 6)) {
		ret += snprintf(version_string + ret,
				LOGSIZ - ret,
				"Touch IC : s3528\n");
	} else if (is_product(ts, "s3320", 5)
		|| is_product(ts, "PLG446", 6)
		|| is_product(ts, "PLG468", 6)) {
		ret += snprintf(version_string + ret,
				LOGSIZ - ret,
				"Touch IC : s3320\n");
	} else {
		ret += snprintf(version_string + ret,
				LOGSIZ - ret,
				"Touch Product ID read error\n");
	}

	ret += snprintf(version_string + ret,
			LOGSIZ - ret,
			"=========================\n\n");

	write_log(NULL, version_string);
	msleep(30);

	kfree(version_string);

	return;
}


/* wrapper function for i2c communication - except defalut page
 * if you have to select page for reading or writing,
 * then using this wrapper function */
int synaptics_ts_set_page(struct i2c_client *client, u8 page)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	DO_SAFE(touch_i2c_write_byte(ts->client, PAGE_SELECT_REG, page), error);
	ts->curr_page = page;

	return 0;
error:
	TOUCH_E("%s, %d : read page failed\n", __func__, __LINE__);
	return -EPERM;
}

int synaptics_ts_page_data_read(struct i2c_client *client,
		u8 page, u8 reg, int size, u8 *data)
{
	DO_SAFE(synaptics_ts_set_page(client, page), error);
	DO_SAFE(touch_ts_i2c_read(client, reg, size, data), error);
	DO_SAFE(synaptics_ts_set_page(client, DEFAULT_PAGE), error);

	return 0;
error:
	TOUCH_E("%s, %d : read page failed\n", __func__, __LINE__);
	return -EPERM;
}

int synaptics_ts_page_data_write(struct i2c_client *client,
		u8 page, u8 reg, int size, u8 *data)
{
	DO_SAFE(synaptics_ts_set_page(client, page), error);
	DO_SAFE(touch_ts_i2c_write(client, reg, size, data), error);
	DO_SAFE(synaptics_ts_set_page(client, DEFAULT_PAGE), error);

	return 0;
error:
	TOUCH_E("%s, %d : write page failed\n", __func__, __LINE__);
	return -EPERM;
}


int synaptics_ts_page_data_write_byte(struct i2c_client *client,
		u8 page, u8 reg, u8 data)
{
	DO_SAFE(synaptics_ts_set_page(client, page), error);
	DO_SAFE(touch_i2c_write_byte(client, reg, data), error);
	DO_SAFE(synaptics_ts_set_page(client, DEFAULT_PAGE), error);

	return 0;
error:
	TOUCH_E("%s, %d : write page byte failed\n", __func__, __LINE__);
	return -EPERM;
}

const char *f_str[] = {
	"ERROR",
	"DISTANCE_INTER_TAP",
	"DISTANCE_TOUCHSLOP",
	"TIMEOUT_INTER_TAP",
	"MULTI_FINGER",
	"DELAY_TIME"
};

static int print_tci_debug_result(struct synaptics_ts_data *ts, int num)
{
	u8 count = 0;
	u8 index = 0;
	u8 buf = 0;
	u8 i = 0;
	u8 addr = 0;
	u8 offset = num ? LPWG_MAX_BUFFER + 2 : 0;

	DO_SAFE(synaptics_ts_page_data_read(ts->client,
		LPWG_PAGE, ts->f51_reg.lpwg_fail_count_reg + offset, 1,
		&count), error);
	DO_SAFE(synaptics_ts_page_data_read(ts->client,
		LPWG_PAGE, ts->f51_reg.lpwg_fail_index_reg + offset, 1,
		&index), error);

	for (i = 1; i <= count ; i++) {
		addr = ts->f51_reg.lpwg_fail_reason_reg + offset
			+ ((index + LPWG_MAX_BUFFER - i) % LPWG_MAX_BUFFER);
		DO_SAFE(synaptics_ts_page_data_read(ts->client, LPWG_PAGE,
			addr, 1, &buf), error);
		TOUCH_D(DEBUG_BASE_INFO, "TCI(%d)-Fail[%d/%d]: %s\n",
			num, count - i + 1, count,
			(buf > 0 && buf < 6) ? f_str[buf] : f_str[0]);
		if (i == LPWG_MAX_BUFFER)
			break;
	}

	return 0;
error:
	return -EPERM;
}

#define SWIPE_F_STR_SIZE 8
static const char *swipe_f_str[SWIPE_F_STR_SIZE] = {
	"SUCCESS",
	"FINGER_RELEASED",
	"MULTIPLE_FINGERS",
	"TOO_FAST",
	"TOO_SLOW",
	"OUT_OF_AREA",
	"RATIO_EXECESS",
	"UNKNOWN"
};

static int print_swipe_fail_reason(struct synaptics_ts_data *ts)
{
	struct swipe_data *swp = &ts->swipe;
	u8 buf = 0;
	u8 direction = 0;
	u8 fail_reason = 0;

	if (mfts_mode && !ts->pdata->role->mfts_lpwg) {
		TOUCH_E("do not print swipe fail reason - mfts\n");
		return -EPERM;
	} else {
		TOUCH_E("%s, %d : swipe fail reason\n", __func__, __LINE__);
	}

	if (swp->support_swipe == NO_SUPPORT_SWIPE) {
		TOUCH_E("support_swipe:0x%02X\n", swp->support_swipe);
		return -EPERM;
	}

	synaptics_ts_page_data_read(ts->client, LPWG_PAGE,
			swp->fail_reason_reg, 1, &buf);

	if (swp->support_swipe & SUPPORT_SWIPE_DOWN) {
		direction = SWIPE_DIRECTION_DOWN;
		fail_reason = buf;
	}

	if (swp->support_swipe & SUPPORT_SWIPE_UP) {
		direction = buf & 0x03;
		fail_reason = (buf & 0xfc) >> 2;
	}

	if (fail_reason >= SWIPE_F_STR_SIZE)
		fail_reason = SWIPE_F_STR_SIZE - 1;

	TOUCH_I("last swipe_%s fail reason:%d(%s)\n",
			direction ? "up" : "down",
			fail_reason, swipe_f_str[fail_reason]);

	return 0;
}

/**
 * Knock on
 *
 * Type		Value
 *
 * 1		WakeUp_gesture_only=1 / Normal=0
 * 2		TCI enable=1 / disable=0
 * 3		Tap Count
 * 4		Min InterTap
 * 5		Max InterTap
 * 6		Touch Slop
 * 7		Tap Distance
 * 8            Interrupt Delay
 */
static int tci_control(struct synaptics_ts_data *ts, int type, u8 value)
{
	struct i2c_client *client = ts->client;
	u8 buffer[3] = {0};
	u8 data = 0;

	switch (type) {
	case REPORT_MODE_CTRL:
		DO_SAFE(touch_ts_i2c_read(ts->client, INTERRUPT_ENABLE_REG,
					1, &data), error);

		if (value)
			data &= ~INTERRUPT_MASK_ABS0;
		else
			data |= INTERRUPT_MASK_ABS0;

		DO_SAFE(touch_i2c_write_byte(ts->client,
					INTERRUPT_ENABLE_REG,
					data), error);

		if (value) {
			buffer[0] = ts->min_finger_amplitude;
			buffer[1] = ts->min_finger_amplitude;
		} else {
			buffer[0] = ts->default_finger_amplitude;
			buffer[1] = ts->default_small_finger_amplitude;
		}

		TOUCH_I(
				"finger_amplitude(finger:0x%02X, small_finger:0x%02X)\n",
				buffer[0], buffer[1]);

		DO_SAFE(touch_ts_i2c_write(client,
					ts->f12_reg.ctrl[15],
					2, buffer), error);

		DO_SAFE(touch_ts_i2c_read(client, ts->f12_reg.ctrl[20],
					3, buffer), error);

		buffer[2] = (buffer[2] & 0xfc) | (value ? 0x2 : 0x0);

		DO_SAFE(touch_ts_i2c_write(client, ts->f12_reg.ctrl[20],
					3, buffer), error);
		break;

	case TCI_ENABLE_CTRL:
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg,
					1, buffer), error);
		buffer[0] = (buffer[0] & 0xfe) | (value & 0x1);
		DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg,
					1, buffer), error);
		break;
	case TCI_ENABLE_CTRL2:
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg2,
					1, buffer), error);
		buffer[0] = (buffer[0] & 0xfe) | (value & 0x1);
		DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg2,
					1, buffer), error);
		break;
	case TAP_COUNT_CTRL:
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg,
					1, buffer), error);
		buffer[0] = ((value << 3) & 0xf8) | (buffer[0] & 0x7);
		DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg,
					1, buffer), error);
		break;
	case TAP_COUNT_CTRL2:
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg2,
					1, buffer), error);
		buffer[0] = ((value << 3) & 0xf8) | (buffer[0] & 0x7);
		DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tapcount_reg2,
					1, buffer), error);
		break;
	case MIN_INTERTAP_CTRL:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_min_intertap_reg,
					value), error);
		break;
	case MIN_INTERTAP_CTRL2:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_min_intertap_reg2,
					value), error);
		break;
	case MAX_INTERTAP_CTRL:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_max_intertap_reg,
					value), error);
		break;
	case MAX_INTERTAP_CTRL2:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_max_intertap_reg2,
					value), error);
		break;
	case TOUCH_SLOP_CTRL:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_touch_slop_reg,
					value), error);
		break;
	case TOUCH_SLOP_CTRL2:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_touch_slop_reg2,
					value), error);
		break;
	case TAP_DISTANCE_CTRL:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tap_distance_reg,
					value), error);
		break;
	case TAP_DISTANCE_CTRL2:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_tap_distance_reg2,
					value), error);
		break;
	case INTERRUPT_DELAY_CTRL:
		DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_interrupt_delay_reg,
					value ?
					(buffer[0] = (KNOCKON_DELAY << 1) | 0x1)
					: (buffer[0] = 0)), error);
		break;
	case INTERRUPT_DELAY_CTRL2:
		if (ts->lpwg_ctrl.has_lpwg_overtap_module) {
			DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_interrupt_delay_reg2,
					buffer[0] = (KNOCKCODE_DELAY << 1) | 0x1),
					error);
		} else {
			DO_SAFE(synaptics_ts_page_data_write_byte(client, LPWG_PAGE,
					ts->f51_reg.lpwg_interrupt_delay_reg2,
					value), error);
		}
		break;
	case PARTIAL_LPWG_ON:
		if (is_product(ts, "PLG468", 6)) {
			DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_partial_reg,
					1, buffer), error);

			TOUCH_I("%s: partial lpwg, prev:0x%02X, next:0x%02X (value:%d)\n",
					__func__,
					buffer[0],
					value ? (buffer[0] & 0xfc) | value :
					(buffer[0] & 0xfe) | value,
					value);

			if (value)
				buffer[0] = (buffer[0] & 0xfc) | value;
			else
				buffer[0] = (buffer[0] & 0xfe) | value;

			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					ts->f51_reg.lpwg_partial_reg,
					1, buffer), error);
		} else {
			DO_SAFE(synaptics_ts_page_data_write_byte(client,
					LPWG_PAGE, ts->f51_reg.lpwg_partial_reg,
					value), error);
		}
		break;
	default:
		break;
	}

	return 0;
error:
	TOUCH_E("%s, %d : tci control failed\n", __func__, __LINE__);
	return -EPERM;

}

static int swipe_down_enable(struct synaptics_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	struct swipe_data *swp = &ts->swipe;
	struct swipe_ctrl_info *down = &swp->down;
	u8 buf[2] = {0};

	TOUCH_I("%s: (swipe_mode:0x%02X)\n", __func__, swp->swipe_mode);

	if (swp->swipe_mode & SWIPE_DOWN_BIT) {
		DO_SAFE(synaptics_ts_set_page(client,
					LPWG_PAGE), error);

		DO_SAFE(touch_ts_i2c_read(client,
					swp->enable_reg,
					1, buf), error);

		DO_SAFE(touch_i2c_write_byte(client,
					swp->enable_reg,
					buf[0] | down->enable_mask), error);

		DO_SAFE(touch_i2c_write_byte(client,
					down->min_distance_reg,
					down->min_distance), error);

		DO_SAFE(touch_i2c_write_byte(client,
					down->ratio_thres_reg,
					down->ratio_thres), error);

		DO_SAFE(touch_i2c_write_byte(client,
					down->ratio_chk_period_reg,
					down->ratio_chk_period), error);

		DO_SAFE(touch_i2c_write_byte(client,
					down->ratio_chk_min_distance_reg,
					down->ratio_chk_min_distance), error);

		buf[0] = GET_LOW_U8_FROM_U16(down->min_time_thres);
		buf[1] = GET_HIGH_U8_FROM_U16(down->min_time_thres);
		DO_SAFE(touch_ts_i2c_write(client,
					down->min_time_thres_reg,
					2, buf), error);

		buf[0] = GET_LOW_U8_FROM_U16(down->max_time_thres);
		buf[1] = GET_HIGH_U8_FROM_U16(down->max_time_thres);
		DO_SAFE(touch_ts_i2c_write(client,
					down->max_time_thres_reg,
					2, buf), error);

		if (swp->support_swipe & SUPPORT_SWIPE_UP) {
			buf[0] = GET_LOW_U8_FROM_U16(down->active_area_x0);
			buf[1] = GET_HIGH_U8_FROM_U16(down->active_area_x0);
			DO_SAFE(touch_ts_i2c_write(client,
						down->active_area_x0_reg,
						2, buf), error);

			buf[0] = GET_LOW_U8_FROM_U16(down->active_area_y0);
			buf[1] = GET_HIGH_U8_FROM_U16(down->active_area_y0);
			DO_SAFE(touch_ts_i2c_write(client,
						down->active_area_y0_reg,
						2, buf), error);

			buf[0] = GET_LOW_U8_FROM_U16(down->active_area_x1);
			buf[1] = GET_HIGH_U8_FROM_U16(down->active_area_x1);
			DO_SAFE(touch_ts_i2c_write(client,
						down->active_area_x1_reg,
						2, buf), error);

			buf[0] = GET_LOW_U8_FROM_U16(down->active_area_y1);
			buf[1] = GET_HIGH_U8_FROM_U16(down->active_area_y1);
			DO_SAFE(touch_ts_i2c_write(client,
						down->active_area_y1_reg,
						2, buf), error);
		}

		DO_SAFE(synaptics_ts_set_page(client,
					DEFAULT_PAGE), error);
	} else {
		TOUCH_I("swipe_down is not used.\n");
	}

	return 0;
error:
	synaptics_ts_set_page(client, DEFAULT_PAGE);
	TOUCH_E("%s failed\n", __func__);
	return -EPERM;
}

static int swipe_up_enable(struct synaptics_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	struct swipe_data *swp = &ts->swipe;
	struct swipe_ctrl_info *up = &swp->up;
	u8 buf[2] = {0};

	TOUCH_I("%s: (swipe_mode:0x%02X)\n", __func__, swp->swipe_mode);

	if (swp->swipe_mode & SWIPE_UP_BIT) {
		DO_SAFE(synaptics_ts_set_page(client,
					LPWG_PAGE), error);

		DO_SAFE(touch_ts_i2c_read(client,
					swp->enable_reg,
					1, buf), error);

		DO_SAFE(touch_i2c_write_byte(client,
					swp->enable_reg,
					buf[0] | up->enable_mask), error);

		DO_SAFE(touch_i2c_write_byte(client,
					up->min_distance_reg,
					up->min_distance), error);

		DO_SAFE(touch_i2c_write_byte(client,
					up->ratio_thres_reg,
					up->ratio_thres), error);

		DO_SAFE(touch_i2c_write_byte(client,
					up->ratio_chk_period_reg,
					up->ratio_chk_period), error);

		DO_SAFE(touch_i2c_write_byte(client,
					up->ratio_chk_min_distance_reg,
					up->ratio_chk_min_distance), error);

		buf[0] = GET_LOW_U8_FROM_U16(up->min_time_thres);
		buf[1] = GET_HIGH_U8_FROM_U16(up->min_time_thres);
		DO_SAFE(touch_ts_i2c_write(client,
					up->min_time_thres_reg,
					2, buf), error);

		buf[0] = GET_LOW_U8_FROM_U16(up->max_time_thres);
		buf[1] = GET_HIGH_U8_FROM_U16(up->max_time_thres);
		DO_SAFE(touch_ts_i2c_write(client,
					up->max_time_thres_reg,
					2, buf), error);

		buf[0] = GET_LOW_U8_FROM_U16(up->active_area_x0);
		buf[1] = GET_HIGH_U8_FROM_U16(up->active_area_x0);
		DO_SAFE(touch_ts_i2c_write(client,
					up->active_area_x0_reg,
					2, buf), error);

		buf[0] = GET_LOW_U8_FROM_U16(up->active_area_y0);
		buf[1] = GET_HIGH_U8_FROM_U16(up->active_area_y0);
		DO_SAFE(touch_ts_i2c_write(client,
					up->active_area_y0_reg,
					2, buf), error);

		buf[0] = GET_LOW_U8_FROM_U16(up->active_area_x1);
		buf[1] = GET_HIGH_U8_FROM_U16(up->active_area_x1);
		DO_SAFE(touch_ts_i2c_write(client,
					up->active_area_x1_reg,
					2, buf), error);

		buf[0] = GET_LOW_U8_FROM_U16(up->active_area_y1);
		buf[1] = GET_HIGH_U8_FROM_U16(up->active_area_y1);
		DO_SAFE(touch_ts_i2c_write(client,
					up->active_area_y1_reg,
					2, buf), error);

		DO_SAFE(synaptics_ts_set_page(client,
					DEFAULT_PAGE), error);
	} else {
		TOUCH_I("swipe_up is not used.\n");
	}

	return 0;
error:
	synaptics_ts_set_page(client, DEFAULT_PAGE);
	TOUCH_E("%s failed\n", __func__);
	return -EPERM;
}

static int swipe_enable(struct synaptics_ts_data *ts)
{
	struct swipe_data *swp = &ts->swipe;

	if (swp->support_swipe & SUPPORT_SWIPE_DOWN)
		swipe_down_enable(ts);

	if (swp->support_swipe & SUPPORT_SWIPE_UP)
		swipe_up_enable(ts);

	return 0;
}

static int swipe_down_disable(struct synaptics_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	struct swipe_data *swp = &ts->swipe;
	struct swipe_ctrl_info *down = &swp->down;
	u8 buf[1] = {0};

	TOUCH_I("%s: (swipe_mode:0x%02X)\n", __func__, swp->swipe_mode);

	DO_SAFE(synaptics_ts_set_page(client,
				LPWG_PAGE), error);

	DO_SAFE(touch_ts_i2c_read(client,
				swp->enable_reg,
				1, buf), error);

	DO_SAFE(touch_i2c_write_byte(client,
				swp->enable_reg,
				buf[0] & ~(down->enable_mask)), error);

	DO_SAFE(synaptics_ts_set_page(client,
				DEFAULT_PAGE), error);

	return 0;
error:
	synaptics_ts_set_page(client, DEFAULT_PAGE);
	TOUCH_E("%s failed\n", __func__);
	return -EPERM;
}

static int swipe_up_disable(struct synaptics_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	struct swipe_data *swp = &ts->swipe;
	struct swipe_ctrl_info *up = &swp->up;
	u8 buf[1] = {0};

	TOUCH_I("%s: (swipe_mode:0x%02X)\n", __func__, swp->swipe_mode);

	DO_SAFE(synaptics_ts_set_page(client,
				LPWG_PAGE), error);

	DO_SAFE(touch_ts_i2c_read(client,
				swp->enable_reg,
				1, buf), error);

	DO_SAFE(touch_i2c_write_byte(client,
				swp->enable_reg,
				buf[0] & ~(up->enable_mask)), error);

	DO_SAFE(synaptics_ts_set_page(client,
				DEFAULT_PAGE), error);

	return 0;
error:
	synaptics_ts_set_page(client, DEFAULT_PAGE);
	TOUCH_E("%s failed\n", __func__);
	return -EPERM;
}

static int swipe_disable(struct synaptics_ts_data *ts)
{
	struct swipe_data *swp = &ts->swipe;

	if (swp->support_swipe & SUPPORT_SWIPE_DOWN)
		swipe_down_disable(ts);

	if (swp->support_swipe & SUPPORT_SWIPE_UP)
		swipe_up_disable(ts);

	return 0;
}

static int get_tci_data(struct synaptics_ts_data *ts, int count)
{
	struct i2c_client *client = ts->client;
	u8 i = 0;
	u8 buffer[12][4] = {{0} };

	ts->pw_data.data_num = count;

	if (!count)
		return 0;

	DO_SAFE(synaptics_ts_page_data_read(client,
				LPWG_PAGE, ts->f51_reg.lpwg_data_reg, 4 * count,
				&buffer[0][0]), error);

	TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
			"%s : knock code coordinates, count = %d\n",
			__func__, count);

	for (i = 0; i < count; i++) {
		ts->pw_data.data[i].x = TS_POSITION(buffer[i][1],
				buffer[i][0]);
		ts->pw_data.data[i].y = TS_POSITION(buffer[i][3],
				buffer[i][2]);

		if (ts->pdata->role->use_security_mode) {
			if (ts->lpwg_ctrl.password_enable) {
				TOUCH_I("LPWG data xxxx, xxxx\n");
			} else {
				TOUCH_I("LPWG data %d, %d\n",
						ts->pw_data.data[i].x,
						ts->pw_data.data[i].y);
			}
		} else {
			TOUCH_I("LPWG data %d, %d\n",
					ts->pw_data.data[i].x,
					ts->pw_data.data[i].y);
		}
	}

	return 0;
error:
	TOUCH_E("%s, %d : get tci_control failed, count : %d\n",
			__func__, __LINE__, count);
	return -EPERM;
}

static void set_lpwg_mode(struct lpwg_control *ctrl, int mode)
{
	ctrl->double_tap_enable =
		((mode == LPWG_DOUBLE_TAP) | (mode == LPWG_PASSWORD)) ? 1 : 0;
	ctrl->password_enable = (mode == LPWG_PASSWORD) ? 1 : 0;
	ctrl->signature_enable = (mode == LPWG_SIGNATURE) ? 1 : 0;
	ctrl->lpwg_is_enabled = ctrl->double_tap_enable
		|| ctrl->password_enable || ctrl->signature_enable;
}

static int sleep_control(struct synaptics_ts_data *ts, int mode, int recal)
{
	u8 curr = 0;
	u8 next = 0;
	int ret = 0;

	/*
	 * NORMAL == 0 : resume & lpwg state
	 * SLEEP  == 1 : uevent reporting time - sleep
	 * NO_CAL == 2 : proxi near - sleep when recal is not needed
	 */

	DO_SAFE(touch_ts_i2c_read(ts->client, DEVICE_CONTROL_REG, 1, &curr),
			error);
	TOUCH_D(DEBUG_BASE_INFO, "%s : curr:0x%02x\n", __func__, curr);

	if (mode == 3) {
		/* in this case, reset Touch IC for sensor reset -> IC Reset */
		TOUCH_I("IC Soft reset for sensor reset.\n");
		if (synaptics_ts_ic_ctrl(ts->client, IC_CTRL_RESET,
					DEVICE_COMMAND_RESET, &ret) < 0)
			TOUCH_E("IC_RESET handling fail\n");

		return 0;
	}
	if (is_product(ts, "PLG446", 6)
			|| is_product(ts, "PLG468", 6)) {
		if (((curr & 0xF8) == DEVICE_CONTROL_SLEEP)
				&& !mode) {
			/* curr - sleep, next - sleep */
			TOUCH_D(DEBUG_BASE_INFO,
					"%s : It's odd case (sleep)(sleep)\n",
					__func__);
			return 0;
		}
		next = (curr & 0xF8) | (mode ?
			DEVICE_CONTROL_NOSLEEP :
			DEVICE_CONTROL_SLEEP);
		TOUCH_D(DEBUG_BASE_INFO,
			       "%s : next:0x%02x\n",
			       __func__, next);
	} else {
		next = (curr & 0xF8) | (mode ?
			DEVICE_CONTROL_NORMAL_OP :
			DEVICE_CONTROL_SLEEP);
		/*	(recal ? DEVICE_CONTROL_SLEEP
		 *	: DEVICE_CONTROL_SLEEP_NO_RECAL); */
		TOUCH_D(DEBUG_BASE_INFO,
				"%s : next:0x%02x\n",
				__func__, next);
	}

	/*TOUCH_D(DEBUG_BASE_INFO, "%s : curr = [%6s] next[%6s]\n", __func__,
			(curr == 0 ? "NORMAL" :
			 (curr == 1 ? "SLEEP" : "NOSLEEP")),
			(next == 0 ? "NORMAL" :
			 (next == 1 ? "SLEEP" : "NOSLEEP")));*/

	DO_SAFE(touch_i2c_write_byte(ts->client, DEVICE_CONTROL_REG, next),
			error);

	return 0;
error:
	TOUCH_E("%s, %d : sleep control failed\n", __func__, __LINE__);
	return -EPERM;
}

static int lpwg_control(struct synaptics_ts_data *ts, int mode)
{
	set_lpwg_mode(&ts->lpwg_ctrl, mode);

	synaptics_toggle_swipe(ts->client);

	switch (mode) {
	case LPWG_SIGNATURE:
		break;

	case LPWG_DOUBLE_TAP:                         /* Only TCI-1 */
		tci_control(ts, TCI_ENABLE_CTRL, 1);  /* Tci-1 enable */
		tci_control(ts, TAP_COUNT_CTRL, 2);   /* tap count = 2 */
		tci_control(ts, MIN_INTERTAP_CTRL, 0); /* min inter_tap
							  = 60ms */
		tci_control(ts, MAX_INTERTAP_CTRL, 70); /* max inter_tap
							   = 700ms */
		tci_control(ts, TOUCH_SLOP_CTRL, 100); /* touch_slop = 10mm */
		tci_control(ts, TAP_DISTANCE_CTRL, 10); /* tap distance
							   = 10mm */
		tci_control(ts, INTERRUPT_DELAY_CTRL, 0); /* interrupt delay
							     = 0ms */
		tci_control(ts, TCI_ENABLE_CTRL2, 0); /* Tci-2 disable */
		if (is_product(ts, "PLG349", 6)) {
			/* wakeup_gesture_only */
			tci_control(ts, REPORT_MODE_CTRL, 1);
		}
		if (is_product(ts, "PLG446", 6)
				|| is_product(ts, "PLG468", 6)) {
			if (lpwg_by_lcd_notifier)
				TOUCH_I(
						"Partial LPWG doens't work after LPWG ON command\n");
			else
				tci_control(ts, PARTIAL_LPWG_ON, 1);
		}
		break;

	case LPWG_PASSWORD:                           /* TCI-1 and TCI-2 */
		tci_control(ts, TCI_ENABLE_CTRL, 1);  /* Tci-1 enable */
		tci_control(ts, TAP_COUNT_CTRL, 2);   /* tap count = 2 */
		tci_control(ts, MIN_INTERTAP_CTRL, 0); /* min inter_tap
							  = 60ms */
		tci_control(ts, MAX_INTERTAP_CTRL, 70); /* max inter_tap
							   = 700ms */
		tci_control(ts, TOUCH_SLOP_CTRL, 100); /* touch_slop = 10mm */
		tci_control(ts, TAP_DISTANCE_CTRL, 7); /* tap distance = 7mm */
		tci_control(ts, INTERRUPT_DELAY_CTRL,
				(u8)ts->pw_data.double_tap_check);
		tci_control(ts, TCI_ENABLE_CTRL2, 1); /* Tci-2 ensable */
		tci_control(ts, TAP_COUNT_CTRL2,
				(u8)ts->pw_data.tap_count); /* tap count
							       = user_setting */
		tci_control(ts, MIN_INTERTAP_CTRL2, 0); /* min inter_tap
							   = 60ms */
		tci_control(ts, MAX_INTERTAP_CTRL2, 70); /* max inter_tap
							    = 700ms */
		tci_control(ts, TOUCH_SLOP_CTRL2, 100); /* touch_slop = 10mm */
		tci_control(ts, TAP_DISTANCE_CTRL2, 255); /* tap distance
							     = MAX */
		tci_control(ts, INTERRUPT_DELAY_CTRL2, 0); /* interrupt delay
							      = 0ms */
		/* wakeup_gesture_only */
		if (is_product(ts, "PLG349", 6))
			tci_control(ts, REPORT_MODE_CTRL, 1);
		if (is_product(ts, "PLG446", 6)
				|| is_product(ts, "PLG468", 6)) {
			if (lpwg_by_lcd_notifier)
				TOUCH_I(
						"Partial LPWG doens't work after LPWG ON command\n");
			else
				tci_control(ts, PARTIAL_LPWG_ON, 1);
		}
		break;

	default:
		if (is_product(ts, "PLG446", 6)
			|| is_product(ts, "PLG468", 6))
			tci_control(ts, PARTIAL_LPWG_ON, 0);
		tci_control(ts, TCI_ENABLE_CTRL, 0); /* Tci-1 disable */
		tci_control(ts, TCI_ENABLE_CTRL2, 0); /* tci-2 disable */
		if (is_product(ts, "PLG349", 6))
			tci_control(ts, REPORT_MODE_CTRL, 0); /* normal */
		break;
	}

	TOUCH_I("%s : lpwg_mode[%d]\n", __func__, mode);
	return 0;
}

struct synaptics_ts_exp_fhandler {
	struct synaptics_ts_exp_fn *exp_fn;
	bool inserted;
	bool initialized;
};

static struct synaptics_ts_exp_fhandler prox_fhandler;
static struct synaptics_ts_exp_fhandler rmidev_fhandler;

void synaptics_ts_prox_function(struct synaptics_ts_exp_fn *prox_fn,
		bool insert)
{
	prox_fhandler.inserted = insert;

	if (insert)
		prox_fhandler.exp_fn = prox_fn;
	else
		prox_fhandler.exp_fn = NULL;

	return;
}

void synaptics_ts_rmidev_function(struct synaptics_ts_exp_fn *rmidev_fn,
		bool insert)
{
	rmidev_fhandler.inserted = insert;

	if (insert)
		rmidev_fhandler.exp_fn = rmidev_fn;
	else
		rmidev_fhandler.exp_fn = NULL;

	return;
}

void matchUp_f51_regMap(struct synaptics_ts_data *ts)
{
	TOUCH_I("Start [%s]\n", __func__);

	if (is_product(ts, "PLG349", 6)
			|| is_product(ts, "s3320", 6)
			|| is_product(ts, "PLG446", 6)
			|| is_product(ts, "PLG468", 6)) {

		if (is_product(ts, "PLG349", 6))
			TOUCH_I("[%s] This is Z2\n", __func__);
		else
			TOUCH_I("[%s] This is P1\n", __func__);

		ts->f51_reg.lpwg_status_reg =
			LPWG_STATUS_REG;
		ts->f51_reg.lpwg_data_reg =
			LPWG_DATA_REG;
		ts->f51_reg.lpwg_tapcount_reg =
			LPWG_TAPCOUNT_REG;
		ts->f51_reg.lpwg_min_intertap_reg =
			LPWG_MIN_INTERTAP_REG;
		ts->f51_reg.lpwg_max_intertap_reg =
			LPWG_MAX_INTERTAP_REG;
		ts->f51_reg.lpwg_touch_slop_reg =
			LPWG_TOUCH_SLOP_REG;
		ts->f51_reg.lpwg_tap_distance_reg =
			LPWG_TAP_DISTANCE_REG;
		ts->f51_reg.lpwg_interrupt_delay_reg =
			LPWG_INTERRUPT_DELAY_REG;

		ts->f51_reg.lpwg_tapcount_reg2 =
			(LPWG_TAPCOUNT_REG + LPWG_BLKSIZ);
		ts->f51_reg.lpwg_min_intertap_reg2 =
			(LPWG_MIN_INTERTAP_REG + LPWG_BLKSIZ);
		ts->f51_reg.lpwg_max_intertap_reg2 =
			(LPWG_MAX_INTERTAP_REG + LPWG_BLKSIZ);
		ts->f51_reg.lpwg_touch_slop_reg2 =
			(LPWG_TOUCH_SLOP_REG + LPWG_BLKSIZ);
		ts->f51_reg.lpwg_tap_distance_reg2 =
			(LPWG_TAP_DISTANCE_REG + LPWG_BLKSIZ);
		ts->f51_reg.lpwg_interrupt_delay_reg2 =
			(LPWG_INTERRUPT_DELAY_REG + LPWG_BLKSIZ);

		if (is_product(ts, "PLG468", 6)) {
			if (ts->lpwg_ctrl.has_lpwg_overtap_module
				&& ts->lpwg_ctrl.has_request_reset_reg) {
				ts->f51_reg.overtap_cnt_reg =
					ts->f51.dsc.data_base + 57;
				ts->f51_reg.request_reset_reg =
					ts->f51.dsc.data_base + 69;
			}
			ts->f51_reg.lpwg_partial_reg =
			    LPWG_PARTIAL_REG + 71;
			ts->f51_reg.lpwg_fail_count_reg =
				ts->f51.dsc.data_base + 0x21;
			ts->f51_reg.lpwg_fail_index_reg =
				ts->f51.dsc.data_base + 0x22;
			ts->f51_reg.lpwg_fail_reason_reg =
				ts->f51.dsc.data_base + 0x23;
		} else {
			if (ts->lpwg_ctrl.has_lpwg_overtap_module) {
				ts->f51_reg.overtap_cnt_reg =
					ts->f51.dsc.data_base + 73;
				ts->f51_reg.lpwg_adc_offset_reg =
					ts->f51_reg.lpwg_interrupt_delay_reg2
				       + 44;
				ts->f51_reg.lpwg_adc_fF_reg1 =
					ts->f51_reg.lpwg_interrupt_delay_reg2
				       + 45;
				ts->f51_reg.lpwg_adc_fF_reg2 =
					ts->f51_reg.lpwg_interrupt_delay_reg2
				       + 46;
				ts->f51_reg.lpwg_adc_fF_reg3 =
					ts->f51_reg.lpwg_interrupt_delay_reg2
					+ 47;
				ts->f51_reg.lpwg_adc_fF_reg4 =
					ts->f51_reg.lpwg_interrupt_delay_reg2
					+ 48;
			}
			ts->f51_reg.lpwg_partial_reg =
			    LPWG_PARTIAL_REG;
			ts->f51_reg.lpwg_fail_count_reg =
				ts->f51.dsc.data_base + 0x31;
			ts->f51_reg.lpwg_fail_index_reg =
				ts->f51.dsc.data_base + 0x32;
			ts->f51_reg.lpwg_fail_reason_reg =
				ts->f51.dsc.data_base + 0x33;

			ts->f51_reg.lpwg_adc_offset_reg =
				ts->f51_reg.lpwg_interrupt_delay_reg2 + 45;
			ts->f51_reg.lpwg_adc_fF_reg1 =
				ts->f51_reg.lpwg_interrupt_delay_reg2 + 46;
			ts->f51_reg.lpwg_adc_fF_reg2 =
				ts->f51_reg.lpwg_interrupt_delay_reg2 + 47;
			ts->f51_reg.lpwg_adc_fF_reg3 =
				ts->f51_reg.lpwg_interrupt_delay_reg2 + 48;
			ts->f51_reg.lpwg_adc_fF_reg4 =
				ts->f51_reg.lpwg_interrupt_delay_reg2 + 49;
		}
	} else {
		TOUCH_I("[%s] No supported product.\n", __func__);
		return;
	}

	TOUCH_I("[%s] Complete to match-up regmap.\n", __func__);
	return;
}

void matchUp_f54_regMap(struct synaptics_ts_data *ts)
{
	if (is_product(ts, "PLG349", 6)) {
		TOUCH_I("[%s] This is Z2\n", __func__);

		ts->f54_reg.interference__metric_LSB = 0x04;
		ts->f54_reg.interference__metric_MSB = 0x05;
		ts->f54_reg.current_noise_status = 0x08;
		ts->f54_reg.cid_im = 0x09;
		ts->f54_reg.freq_scan_im = 0x0A;
	} else if (is_product(ts, "s3320", 5)
		 || is_product(ts, "PLG446", 6)) {
		TOUCH_I("[%s] This is P1\n", __func__);

		ts->f54_reg.interference__metric_LSB = 0x04;
		ts->f54_reg.interference__metric_MSB = 0x05;
		ts->f54_reg.current_noise_status = 0x08;
		ts->f54_reg.cid_im = 0x0A;
		ts->f54_reg.freq_scan_im = 0x0B;
		ts->f54_reg.incell_statistic = 0x10;
	} else if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] This is P1\n", __func__);

		ts->f54_reg.interference__metric_LSB = 0x05;
		ts->f54_reg.interference__metric_MSB = 0x06;
		ts->f54_reg.current_noise_status = 0x09;
		ts->f54_reg.freq_scan_im = 0x0A;
	} else {
		TOUCH_I("[%s] No supported product.\n", __func__);
		return;
	}

	TOUCH_I("[%s] Complete to match-up regmap.\n", __func__);

	return;
}


void get_f12_info(struct synaptics_ts_data *ts)
{
	int retval;
	struct synaptics_ts_f12_query_5 query_5;
	struct synaptics_ts_f12_query_8 query_8;
	struct synaptics_ts_f12_ctrl_23 ctrl_23;
	struct synaptics_ts_f12_ctrl_8 ctrl_8;

	int i;
	u8 offset;
	u32 query_5_present = 0;
	u16 query_8_present = 0;

	if (!ts) {
		TOUCH_E("ts is null\n");
		return;
	}

	/* ctrl_reg_info setting */
	retval = touch_ts_i2c_read(ts->client, (ts->f12.dsc.query_base + 5),
			sizeof(query_5.data), query_5.data);

	if (retval < 0) {
		TOUCH_E(
				"Failed to read from F12_2D_QUERY_05_Control_Presence register\n");
		return;
	}

	query_5_present = (query_5_present << 8) | query_5.data[4];
	query_5_present = (query_5_present << 8) | query_5.data[3];
	query_5_present = (query_5_present << 8) | query_5.data[2];
	query_5_present = (query_5_present << 8) | query_5.data[1];

	TOUCH_I("qeury_5_present=0x%08X [%02X %02X %02X %02X %02X]\n",
			query_5_present, query_5.data[0], query_5.data[1],
			query_5.data[2], query_5.data[3], query_5.data[4]);

	offset = 0;
	for (i = 0; i < 32; i++) {
		ts->f12_reg.ctrl[i] = ts->f12.dsc.control_base + offset;

		if (query_5_present & (1 << i)) {
			TOUCH_I(
					"ts->f12_reg.ctrl[%d]=0x%02X (0x%02x+%d)\n",
					i, ts->f12_reg.ctrl[i],
					ts->f12.dsc.control_base, offset);
			offset++;
		}
	}

	/* data_reg_info setting */
	retval = touch_ts_i2c_read(ts->client, (ts->f12.dsc.query_base + 8),
			sizeof(query_8.data), query_8.data);

	if (retval < 0) {
		TOUCH_E(
				"Failed to read from F12_2D_QUERY_08_Data_Presence register\n"
			     );
		return;
	}

	query_8_present = (query_8_present << 8) | query_8.data[2];
	query_8_present = (query_8_present << 8) | query_8.data[1];

	TOUCH_I("qeury_8_present=0x%08X [%02X %02X %02X]\n",
			query_8_present, query_8.data[0],
			query_8.data[1], query_8.data[2]);

	offset = 0;
	for (i = 0; i < 16; i++) {
		ts->f12_reg.data[i] = ts->f12.dsc.data_base + offset;

		if (query_8_present & (1 << i)) {
			TOUCH_I(
					"ts->f12_reg.data[%d]=0x%02X (0x%02x+%d)\n",
					i, ts->f12_reg.data[i],
					ts->f12.dsc.data_base, offset);
			offset++;
		}
	}

	retval = touch_ts_i2c_read(ts->client, ts->f12_reg.ctrl[23],
			sizeof(ctrl_23.data), ctrl_23.data);

	ts->object_report = ctrl_23.obj_type_enable;
	ts->num_of_fingers = min_t(u8, ctrl_23.max_reported_objects,
		       (u8) MAX_NUM_OF_FINGERS);

	TOUCH_I(
			"ts->object_report[0x%02X], ts->num_of_fingers[%d]\n",
			ts->object_report, ts->num_of_fingers);

	retval = touch_ts_i2c_read(ts->client, ts->f12_reg.ctrl[8],
			sizeof(ctrl_8.data), ctrl_8.data);

	TOUCH_I(
			"ctrl_8 - sensor_max_x[%d], sensor_max_y[%d]\n",
			((unsigned short)ctrl_8.max_x_coord_lsb << 0) |
			((unsigned short)ctrl_8.max_x_coord_msb << 8),
			((unsigned short)ctrl_8.max_y_coord_lsb << 0) |
			((unsigned short)ctrl_8.max_y_coord_msb << 8));

	return;
}

void get_finger_amplitude(struct synaptics_ts_data *ts)
{
	int retval = 0;
	u8 buf[2] = {0};
	u8 min_peak_amplitude = 0;
	u16 saturation_cap = 0;
	u8 temp_min_finger_amplitude = 0;

	retval = touch_ts_i2c_read(ts->client,
			ts->f12_reg.ctrl[15], sizeof(buf), buf);
	if (retval < 0) {
		TOUCH_E("Failed to read finger_amplitude data\n");
		return;
	}
	ts->default_finger_amplitude = buf[0];
	ts->default_small_finger_amplitude = buf[1];
	TOUCH_I(
			"default_finger_amplitude = 0x%02X, default_small_finger_amplitude = 0x%02X\n",
			ts->default_finger_amplitude,
			ts->default_small_finger_amplitude);

	retval = touch_ts_i2c_read(ts->client,
			ts->f12_reg.ctrl[10], sizeof(buf), buf);
	if (retval < 0) {
		TOUCH_E("Failed to read min_peak_amplitude data\n");
		return;
	}
	min_peak_amplitude = buf[1];
	TOUCH_I("min_peak_amplitude = 0x%02X\n",	min_peak_amplitude);

	retval = synaptics_ts_page_data_read(ts->client,
			ANALOG_PAGE, SATURATION_CAP_LSB_REG, 1, &buf[0]);
	if (retval < 0) {
		TOUCH_E("Failed to read saturation_cap_lsb data\n");
		return;
	}
	retval = synaptics_ts_page_data_read(ts->client,
			ANALOG_PAGE, SATURATION_CAP_MSB_REG, 1, &buf[1]);
	if (retval < 0) {
		TOUCH_E("Failed to read saturation_cap_msb data\n");
		return;
	}
	saturation_cap = (u16)((buf[1] << 8) & 0xff00) | (u16)(buf[0] & 0x00ff);
	TOUCH_I("saturation_cap = 0x%04X\n", saturation_cap);

	if (saturation_cap == 0)
		saturation_cap = 1;

	temp_min_finger_amplitude =
		1 + ((min_peak_amplitude * 0xff) / saturation_cap);
	ts->min_finger_amplitude = ts->default_finger_amplitude;

	TOUCH_I("min_finger_amplitude = 0x%02X\n",
			ts->min_finger_amplitude);

	return;
}

static int synaptics_get_cap_diff(struct synaptics_ts_data *ts)
{
	int t_diff = 0;
	int r_diff = 0;
	int x = 0;
	int y = 0;
	int ret = 0;
	s8 *rx_cap_diff = NULL;
	s8 *tx_cap_diff = NULL;
	unsigned short *raw_cap = NULL;
	char *f54_cap_wlog_buf = NULL;
	static int cap_outbuf;

	unsigned char txcnt = TxChannelCount;
	unsigned char rxcnt = RxChannelCount;

	/* allocation of cap_diff */
	rx_cap_diff = NULL;
	tx_cap_diff = NULL;
	raw_cap = NULL;
	cap_outbuf = 0;

	ASSIGN(rx_cap_diff = kzalloc(rxcnt * sizeof(u8),
				GFP_KERNEL), error_mem);
	ASSIGN(tx_cap_diff = kzalloc(txcnt * sizeof(u8),
				GFP_KERNEL), error_mem);
	ASSIGN(raw_cap = kzalloc(txcnt * rxcnt
				* sizeof(unsigned char) * 2,
				GFP_KERNEL), error_mem);
	ASSIGN(f54_cap_wlog_buf = kzalloc(DS5_BUFFER_SIZE, GFP_KERNEL),
			error_mem);

	memset(f54_cap_wlog_buf, 0, DS5_BUFFER_SIZE);

	if (diffnode(raw_cap) < 0) {
		TOUCH_I("check_diff_node fail!!\n");
		kfree(rx_cap_diff);
		kfree(tx_cap_diff);
		kfree(raw_cap);
		kfree(f54_cap_wlog_buf);
		return -EAGAIN;
	}

	ts->bad_sample = 0;
	for (y = 0; y < (int)rxcnt - 1; y++) {
		t_diff = 0;

		for (x = 0; x < (int)txcnt; x++)
			t_diff += (raw_cap[x * rxcnt + y + 1]
					- raw_cap[x * rxcnt + y]);

		t_diff = t_diff / (int)txcnt;

		if (jitter_abs(t_diff) > 1000)
			ts->bad_sample = 1;

		if (t_diff < -0x7F) /* limit diff max */
			rx_cap_diff[y + 1] = -0x7F;
		else if (t_diff > 0x7F)
			rx_cap_diff[y + 1] = 0x7F;
		else
			rx_cap_diff[y + 1] = (s8)t_diff;

		/*need to modify*/
		cap_outbuf += snprintf(f54_cap_wlog_buf+cap_outbuf,
				DS5_BUFFER_SIZE - cap_outbuf,
				"%5d\n",
				rx_cap_diff[y + 1]);
	}

	if (tx_cap_diff != NULL && ts->bad_sample == 0) {

		for (x = 0; x < (int)txcnt - 1; x++) {
			r_diff = 0;
			for (y = 0; y < (int)rxcnt; y++)
				r_diff += (raw_cap[(x + 1) * rxcnt + y]
						- raw_cap[x * rxcnt + y]);

			r_diff = r_diff / (int)rxcnt;

			/*need to tunning limit_value*/
			if (jitter_abs(r_diff) > 1000)
				ts->bad_sample = 1;

			 /* limit diff max */
			if (r_diff < -0x7F)
				tx_cap_diff[x + 1] = -0x7F;
			else if (r_diff > 0x7F)
				tx_cap_diff[x + 1] = 0x7F;
			else
				tx_cap_diff[x + 1] = (s8)r_diff;

			/*need to modify*/
			cap_outbuf += snprintf(f54_cap_wlog_buf+cap_outbuf,
					DS5_BUFFER_SIZE - cap_outbuf,
					"%5d\n",
					tx_cap_diff[x + 1]);
		}
	}
	/*need to modify*/
	if (write_log(CAP_FILE_PATH, f54_cap_wlog_buf) == 1)
		raw_cap_file_exist = 1;
	read_log(CAP_FILE_PATH, ts->pdata);

	/*average of Rx_line Cap Value*/
	kfree(rx_cap_diff);
	kfree(tx_cap_diff);
	kfree(raw_cap);
	kfree(f54_cap_wlog_buf);
	return ret;

error_mem:
	TOUCH_I("error_mem\n");
	return -ENOMEM;

}

static char *productcode_parse(unsigned char *product)
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
		len += snprintf(str + len, sizeof(str)-len,
				"%s\n", str_panel[i]);
	else
		len += snprintf(str + len, sizeof(str)-len,
				"Unknown\n");

	i = (product[0] & 0x0F);
	if (i < 5 && i != 1)
		len += snprintf(str + len, sizeof(str)-len,
				"%dkey\n", i);
	else
		len += snprintf(str + len, sizeof(str)-len,
				"Unknown\n");

	i = (product[1] & 0xF0) >> 4;
	if (i < 1)
		len += snprintf(str + len, sizeof(str)-len,
				"%s\n", str_ic[i]);
	else
		len += snprintf(str + len, sizeof(str)-len,
				"Unknown\n");

	inch[0] = (product[1] & 0x0F);
	inch[1] = ((product[2] & 0xF0) >> 4);
	len += snprintf(str+len, sizeof(str)-len,
			"%d.%d\n", inch[0], inch[1]);

	paneltype = (product[2] & 0x0F);
	len += snprintf(str+len, sizeof(str)-len,
			"PanelType %d\n", paneltype);

	version[0] = ((product[3] & 0x80) >> 7);
	version[1] = (product[3] & 0x7F);
	len += snprintf(str+len, sizeof(str)-len,
			"version : v%d.%02d\n", version[0], version[1]);

	return str;
}
static void lpwg_timer_func(struct work_struct *work_timer)
{
	struct synaptics_ts_data *ts = container_of(to_delayed_work(work_timer),
			struct synaptics_ts_data, work_timer);

	send_uevent_lpwg(ts->client, LPWG_PASSWORD);
	wake_unlock(&ts->timer_wake_lock);

	TOUCH_D(DEBUG_LPWG, "u-event timer occur!\n");
	return;
}

static void all_palm_released_func(struct work_struct *work_palm)
{
	struct synaptics_ts_data *ts = container_of(to_delayed_work(work_palm),
			struct synaptics_ts_data, work_palm);

	ts->palm.all_released = false;
	TOUCH_I("%s: ABS0 event disable time is expired.\n", __func__);

	return;
}

static void sleepmode_func(struct work_struct *work_sleep)
{
	struct synaptics_ts_data *ts = container_of(to_delayed_work(work_sleep),
			struct synaptics_ts_data, work_sleep);

	mutex_lock(&ts->pdata->thread_lock);
	synaptics_change_sleepmode(ts->client);
	mutex_unlock(&ts->pdata->thread_lock);

	return;
}

static int get_binFW_version(struct synaptics_ts_data *ts)
{
	const struct firmware *fw_entry = NULL;
	const u8 *firmware = NULL;
	int rc = 0;

	rc = request_firmware(&fw_entry,
		ts->pdata->inbuilt_fw_name,
		&ts->client->dev);
	if (rc != 0) {
		TOUCH_E("[%s] request_firmware() failed %d\n", __func__, rc);
		return -EIO;
	}

	firmware = fw_entry->data;

	memcpy(ts->fw_info.img_product_id,
			&firmware[ts->pdata->fw_pid_addr], 6);
	memcpy(ts->fw_info.img_version,
			&firmware[ts->pdata->fw_ver_addr], 4);

	release_firmware(fw_entry);

	return rc;
}

static ssize_t show_firmware(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int rc = 0;
	u8 crc_buffer = 0;

	mutex_lock(&ts->pdata->thread_lock);
	read_page_description_table(ts->client);
	rc = get_ic_info(ts);
	rc += get_binFW_version(ts);
	mutex_unlock(&ts->pdata->thread_lock);
	if (rc < 0) {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"-1\n");
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Read Fail Touch IC Info or Touch Bin Info.\n");
		return ret;
	}
	ret = snprintf(buf + ret,
			PAGE_SIZE - ret,
			"\n======== Firmware Info ========\n");

	if (ts->fw_info.version[0] > 0x50) {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"ic_version[%s]\n",
				ts->fw_info.version);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"ic_version RAW = %02X %02X %02X %02X\n",
				ts->fw_info.version[0],
				ts->fw_info.version[1],
				ts->fw_info.version[2],
				ts->fw_info.version[3]);
		ret += snprintf(buf+ret,
				PAGE_SIZE-ret,
				"=== ic_fw_version info ===\n%s",
				productcode_parse(ts->fw_info.version));
	}
	ret += snprintf(buf + ret,
			PAGE_SIZE - ret,
			"IC_product_id[%s]\n",
			ts->fw_info.product_id);

	if (is_product(ts, "PLG349", 6)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch IC : s3528\n\n");
	} else if (is_product(ts, "s3320", 5)
			|| is_product(ts, "PLG468", 6)) {
		rc = synaptics_ts_page_data_read(client,
			ANALOG_PAGE,
			CALIBRATION_STATUS_REG,
			1, &crc_buffer);
		if (rc < 0) {
			TOUCH_I("Can not read Calibration CRC Register\n");
			crc_buffer = -1;
		}
		crc_buffer &= 0x03;
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch IC : s3320(cal_crc : %u) \n\n", crc_buffer);
	} else if (is_product(ts, "PLG446", 6)) {
		if (ts->pdata->role->fw_index == BL_VER_HIGHER)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Touch IC : s3320 / BL 7.2\n\n");
		else
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Touch IC : s3320 / BL 6.0\n\n");
	} else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch product ID read fail\n\n");
	}

	if (ts->fw_info.img_version[0] > 0x50) {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"img_version[%s]\n",
				ts->fw_info.img_version);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"img_version RAW = %02X %02X %02X %02X\n",
				ts->fw_info.img_version[0],
				ts->fw_info.img_version[1],
				ts->fw_info.img_version[2],
				ts->fw_info.img_version[3]);
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"=== img_version info ===\n%s",
				productcode_parse(ts->fw_info.img_version));
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "Img_product_id[%s]\n",
			ts->fw_info.img_product_id);
	if (is_img_product(ts, "PLG349", 6)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch IC : s3528\n");
	} else if (is_img_product(ts, "s3320", 5)
			|| is_img_product(ts, "PLG446", 6)
			|| is_img_product(ts, "PLG468", 6)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch IC : s3320\n");
	} else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch product ID read fail\n");
	}

	return ret;
}

static ssize_t show_synaptics_fw_version(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int rc = 0;

	mutex_lock(&ts->pdata->thread_lock);
	read_page_description_table(ts->client);
	rc = get_ic_info(ts);
	mutex_unlock(&ts->pdata->thread_lock);
	if (rc < 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"-1\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Read Fail Touch IC Info.\n");
		return ret;
	}
	ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"\n======== Auto Touch Test ========\n");
	if (ts->fw_info.version[0] > 0x50) {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"ic_version[%s]\n",
				ts->fw_info.version);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"version : (v%d.%02d)\n",
				((ts->fw_info.version[3] & 0x80) >> 7),
				(ts->fw_info.version[3] & 0x7F));
	}

	ret += snprintf(buf + ret,
			PAGE_SIZE - ret,
			"IC_product_id[%s]\n",
			ts->fw_info.product_id);

	if (is_product(ts, "PLG349", 6)) {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Touch IC : s3528\n\n");
	} else if (is_product(ts, "s3320", 5)
		|| is_product(ts, "PLG446", 6)
		|| is_product(ts, "PLG468", 6)) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch IC : s3320\n\n");
	} else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Touch product ID read fail\n\n");
	}

	return ret;
}

static void check_incalibration_crc(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
			(struct synaptics_ts_data *)get_touch_handle(client);

	int rc = 0;
	u8 crc_buffer = 0;
	char crc_file_buf[100] = {0};
	int crc_file_buf_len = 0;

	rc = synaptics_ts_page_data_read(client,
		ANALOG_PAGE,
		CALIBRATION_STATUS_REG,
		1, &crc_buffer);

	if (rc < 0) {
		TOUCH_I("[%s] Can not read Calibration CRC Register\n",__func__);
		crc_file_buf_len += snprintf(crc_file_buf + crc_file_buf_len,
			sizeof(crc_file_buf) - crc_file_buf_len,
			"Can not read Calibration CRC Register\n\n");
		write_log(NULL, crc_file_buf);
		return;
	}
	crc_buffer &= 0x03;

	crc_file_buf_len += snprintf(crc_file_buf + crc_file_buf_len,
				sizeof(crc_file_buf) - crc_file_buf_len,
				"LGD_In_Calibration CRC = %d\n\n", crc_buffer);
	write_log(NULL, crc_file_buf);

}


ssize_t _show_sd(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int full_raw_cap = 0;
	int trx2trx = 0;
	int high_resistance = 0;
	int adc_range = 0;
	int sensor_speed = 0;
	int noise_delta = 0;
	int gnd = 0;
	int Rsp_grp = 0;
	int Rsp_short = 0;
	int Rsp_im = 0;
	int Rsp_coarse_cal = 0;
	int adc_test = 0;
	int lower_img = 0;
	int upper_img = 0;
	int lower_sensor = 0;
	int upper_sensor = 0;
	int lower_adc = 0;
	int upper_adc = 0;
	int noise_limit = 0;
	char *temp_buf = NULL;
	int len = 0;
	int upgrade = 0;
	if (power_state == POWER_ON || power_state == POWER_WAKE) {

		temp_buf = kzalloc(100, GFP_KERNEL);
		if (!temp_buf) {
			TOUCH_I("%s Failed to allocate memory\n",
					__func__);
			return 0;
		}

		if (!ts->pdata->panel_id && mfts_mode) {
			TOUCH_I("%s JDI MFTS FW UPGRADE\n",
					__func__);
			get_ic_info(ts);
			upgrade = firmware_upgrade_func_mfts(client);
			if (upgrade == NO_ERROR) {
				TOUCH_I("%s FW Upgrade Done\n",
						__func__);
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						"upgraded\n");
				return ret;
			} else if (upgrade == ERROR) {
				TOUCH_I("%s FW Upgrade Error\n",
						__func__);
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						"0\n");
				return ret;
			}
			msleep(30);

		}

		write_time_log(NULL, NULL, 0);
		msleep(30);
		write_firmware_version_log(ts);

		mutex_lock(&ts->pdata->thread_lock);
		touch_ts_disable_irq(ts->client->irq);

		SCAN_PDT();

		if (ts->pdata->panel_id) {
			/*RSP Product Test*/
			lower_img = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"RspLowerImageLimit",
					LowerImage);
			upper_img = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"RspUpperImageLimit",
					UpperImage);
			noise_limit = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"RspNoiseP2PLimit",
					RspNoise);
			check_incalibration_crc(client);
			if ((((ts->fw_info.version[3] & 0x80) >> 7) == 1)
				&& ((ts->fw_info.version[3] & 0x7F) > 18)) {
				Rsp_im = synaptics_ts_im_test(client);
				msleep(20);
			}

			Rsp_grp = F54Test('q', 0, buf);
			msleep(100);
			Rsp_short = F54Test('s', 0, buf);
			msleep(100);
			Rsp_coarse_cal = F54Test('q', 4, buf);
			msleep(100);

			if (lower_img < 0 || upper_img < 0) {
				TOUCH_I(
						"[%s] lower return = %d upper return = %d\n",
						__func__, lower_img, upper_img);
				TOUCH_I(
						"[%s][FAIL] Can not check the limit of raw cap\n",
						__func__);
				ret = snprintf(buf + ret, PAGE_SIZE - ret,
						"Can not check the limit of raw cap\n");
			} else {
				TOUCH_I(
						"Getting limit of raw cap is success\n");
			}

			if (noise_limit < 0) {
				TOUCH_I(
						"[%s] noise limit return = %d\n",
						__func__, noise_limit);
				TOUCH_I(
						"[%s][FAIL] Can not check the limit of noise\n",
						__func__);
				ret = snprintf(buf + ret, PAGE_SIZE - ret,
						"Can not check the limit of noise\n"
						);
			} else {
				TOUCH_I(
						"Getting limit of noise is success\n"
						);
			}

			synaptics_ts_init(ts->client);
			touch_ts_enable_irq(ts->client->irq);
			mutex_unlock(&ts->pdata->thread_lock);
			msleep(30);
			write_time_log(NULL, NULL, 0);
			msleep(20);

			ret = snprintf(buf,
					PAGE_SIZE,
					"========RESULT=======\n");

			ret += snprintf(buf + ret,
					PAGE_SIZE - ret,
					"Channel Status : %s",
					(Rsp_short == 1) ? "Pass\n" : "Fail\n");

			if ((((ts->fw_info.version[3] & 0x80) >> 7) == 1)
				&& ((ts->fw_info.version[3] & 0x7F) > 18)) {

				ret += snprintf(buf + ret,
					PAGE_SIZE - ret,
					"Raw Data : %s",
					(Rsp_grp == 1 && Rsp_im == 1
					 && Rsp_coarse_cal == 1)
					? "Pass\n" : "Fail");

				if (!(Rsp_grp && Rsp_im && Rsp_coarse_cal)) {
					ret += snprintf(buf + ret,
							PAGE_SIZE - ret,
							" (");
					ret += snprintf(buf + ret,
							PAGE_SIZE - ret,
							"%s /%s /%s",
							(Rsp_im == 0 ?
							" 0" : " 1"),
							(Rsp_grp == 0 ?
							" 0" : " 1"),
							(Rsp_coarse_cal == 0 ?
							" 0" : " 1"));
					ret += snprintf(buf + ret,
							PAGE_SIZE - ret,
							" )\n");
				}

			} else {

				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						"Raw Data : %s",
						(Rsp_grp == 1)
						? "Pass\n" : "Fail\n");
			}

		} else {
			lower_img = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"LowerImageLimit",
					LowerImage);
			upper_img = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"UpperImageLimit",
					UpperImage);
			lower_sensor = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"SensorSpeedLowerImageLimit",
					SensorSpeedLowerImage);
			upper_sensor = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"SensorSpeedUpperImageLimit",
					SensorSpeedUpperImage);
			lower_adc = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"ADCLowerImageLimit",
					ADCLowerImage);
			upper_adc = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"ADCUpperImageLimit",
					ADCUpperImage);

			adc_test = synaptics_ts_adc_test(client);
			msleep(20);

			if (lower_img < 0 || upper_img < 0) {
				TOUCH_I(
						"[%s] lower return = %d upper return = %d\n",
						__func__, lower_img, upper_img);
				TOUCH_I(
						"[%s][FAIL] Can not check the limit of raw cap\n",
						__func__);
				ret = snprintf(buf + ret, PAGE_SIZE - ret,
						"Can not check the limit of raw cap\n");
			} else {
				TOUCH_I(
						"Getting limit of raw cap is success\n");
				full_raw_cap = F54Test('a', 0, buf);
				if (ts->pdata->ref_chk_option[0]) {
					msleep(30);
					synaptics_get_cap_diff(ts);
				}
				msleep(30);
			}

			trx2trx = F54Test('f', 0, buf);
			msleep(50);

			high_resistance = F54Test('g', 0, buf);
			msleep(50);

			noise_delta = F54Test('x', 0, buf);
			msleep(100);

			gnd = F54Test('y', 0, buf);
			msleep(100);

			if (lower_sensor < 0 || upper_sensor < 0) {
				TOUCH_I(
						"[%s] lower return = %d upper return = %d\n"
						, __func__,
						lower_sensor,
						upper_sensor);
				TOUCH_I(
						"[%s][FAIL] Can not check the limit of sensor speed image\n"
						, __func__);
				ret = snprintf(buf + ret,
						PAGE_SIZE - ret,
						"Can not check the limit of sensor speed image limit\n");
			} else {
				TOUCH_I(
						"Getting limit of Sensor Speed Test is success\n");
				sensor_speed = F54Test('c', 0, buf);
			}
			msleep(50);

			if (lower_adc < 0 || upper_adc < 0) {
				TOUCH_I(
						"[%s] lower return = %d upper return = %d\n",
						__func__,
						lower_adc,
						upper_adc);
				TOUCH_I(
						"[%s][FAIL] Can not check the limit of ADC image\n",
						__func__);
				ret = snprintf(buf + ret,
						PAGE_SIZE - ret,
						"Can not check the limit of ADC image limit\n");
			} else {
				TOUCH_I(
						"Getting limit of ADC Range Test is success\n");
				adc_range = F54Test('b', 0, buf);
			}

			synaptics_ts_init(ts->client);
			touch_ts_enable_irq(ts->client->irq);

			mutex_unlock(&ts->pdata->thread_lock);
			msleep(30);


			if (ts->h_err_cnt || ts->v_err_cnt || ts->bad_sample)
				full_raw_cap = 0;

			len += snprintf(temp_buf+len,
					PAGE_SIZE-len,
					"Cap Diff : %s\n",
					ts->bad_sample == 0 ? "PASS" : "FAIL");

			len += snprintf(temp_buf+len,
					PAGE_SIZE - len,
					"Error node Check h_err_cnt: %s(err count:%d)\n",
					(ts->h_err_cnt == 0 ? "PASS" : "FAIL"),
					ts->h_err_cnt);
			len += snprintf(temp_buf+len,
					PAGE_SIZE-len,
					"Error node Check v_err_cnt: %s(err count:%d)\n\n",
					(ts->v_err_cnt == 0 ? "PASS" : "FAIL"),
					ts->v_err_cnt);

			write_log(NULL, temp_buf);
			msleep(30);
			write_time_log(NULL, NULL, 0);
			msleep(20);

			ret = snprintf(buf,
					PAGE_SIZE,
					"========RESULT=======\n");

			ret += snprintf(buf + ret,
					PAGE_SIZE - ret,
					"Channel Status : %s",
					(trx2trx == 1 && high_resistance == 1)
					? "Pass\n" : "Fail");

			if (!(trx2trx && high_resistance)) {
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						" (");
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						"%s /%s",
						(trx2trx == 0 ? " 0" : " 1"),
						(high_resistance == 0 ?
						 " 0" :
						 " 1"));
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						" )\n");
			}

			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Raw Data : %s",
					(full_raw_cap > 0 && adc_test == 1)
					? "Pass\n"
					: "Fail");

			if (!(full_raw_cap && adc_test)) {
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						" (");
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						"%s /%s",
						(full_raw_cap == 0 ? " 0" : " 1"),
						(adc_test == 0 ?
						 " 0" :
						 " 1"));
				ret += snprintf(buf + ret,
						PAGE_SIZE - ret,
						" )\n");
			}
		}

	} else {
		TOUCH_I("Power Suspend Can not Use I2C\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Power Suspend Can not Use I2C\n");
	}

	kfree(temp_buf);

	return ret;
}


ssize_t getsd(struct i2c_client *client, char *buf)
{
	ssize_t ret;

	ret = _show_sd(client, buf);

	return ret;
}
EXPORT_SYMBOL(getsd);

static ssize_t show_sd(struct i2c_client *client, char *buf)
{
	return _show_sd(client, buf);
}

static ssize_t show_rawdata(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int lower_ret = 0;
	int upper_ret = 0;

	if (power_state == POWER_ON || power_state == POWER_WAKE || ts->pdata->panel_id == 1) {
		mutex_lock(&ts->pdata->thread_lock);
		wake_lock(&ts->touch_rawdata);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}

		touch_ts_disable_irq(ts->client->irq);

		lower_ret = get_limit(TxChannelCount, RxChannelCount,
				*ts->client, ts->pdata,
				"LowerImageLimit", LowerImage);
		upper_ret = get_limit(TxChannelCount, RxChannelCount,
				*ts->client, ts->pdata,
				"UpperImageLimit", UpperImage);

		if (lower_ret < 0 || upper_ret < 0) {
			TOUCH_I(
					"[%s] lower return = %d upper return = %d\n",
					__func__, lower_ret, upper_ret);
			TOUCH_I(
					"[%s][FAIL] Can not check the limit of raw cap\n",
					__func__);
			ret = snprintf(buf + ret, PAGE_SIZE - ret,
					"Can not check the limit of raw cap\n");
		} else {
			TOUCH_I(
					"[%s] lower return = %d upper return = %d\n",
					__func__, lower_ret, upper_ret);
			TOUCH_I(
					"[%s][SUCCESS] Can check the limit of raw cap\n",
					__func__);

			if (is_product(ts, "PLG446", 6)
					|| is_product(ts, "PLG349", 6)) {
						TOUCH_I(
							"Display Rawdata Start PLG446\n");
						ret = F54Test('a', 1, buf);
				} else if (is_product(ts, "PLG468", 6)) {
						TOUCH_I(
							"Display Rawdata Start PLG468\n");
						ret = F54Test('q', 1, buf);
				} else {
						TOUCH_I(
							"Unknown Model : %s\n",
							ts->fw_info.product_id);
				}

		}

		touch_ts_enable_irq(ts->client->irq);

		synaptics_ts_init(ts->client);

		wake_unlock(&ts->touch_rawdata);
		mutex_unlock(&ts->pdata->thread_lock);
	} else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	if (ret == 0)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"ERROR: full_raw_cap failed.\n");

	return ret;
}

static ssize_t show_delta(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;

	if (power_state == POWER_ON || power_state == POWER_WAKE || ts->pdata->panel_id == 1) {
		mutex_lock(&ts->pdata->thread_lock);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}
		touch_ts_disable_irq(ts->client->irq);
		wake_lock(&ts->touch_rawdata);

		if (is_product(ts, "PLG446", 6)) {
			ret = F54Test('m', 0, buf);
		}
		else if(is_product(ts, "PLG468", 6)){
			ret = F54Test('q', 2, buf);
		} else {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		}

		touch_ts_enable_irq(ts->client->irq);
		wake_unlock(&ts->touch_rawdata);
		mutex_unlock(&ts->pdata->thread_lock);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	if (ret == 0)
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"ERROR: full_raw_cap failed.\n");

	return ret;
}

static ssize_t show_chstatus(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int high_resistance = 0;
	int trx2trx = 0;

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		mutex_lock(&ts->pdata->thread_lock);
		touch_ts_disable_irq(ts->client->irq);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}
		high_resistance = F54Test('g', 0, buf);
		trx2trx = F54Test('f', 0, buf);

		ret = snprintf(buf, PAGE_SIZE - ret, "========RESULT=======\n");

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"TRex Short Report : RESULT: %s",
				(trx2trx > 0) ? "Pass\n" : "Fail\n");

		/*High Resistance always return fail, you should see raw data.*/
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"High Resistance   : RESULT: %s",
				(high_resistance > 0) ? "Pass\n" : "Fail\n");

		synaptics_ts_init(ts->client);

		touch_ts_enable_irq(ts->client->irq);

		mutex_unlock(&ts->pdata->thread_lock);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}

static ssize_t show_abs_test(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int abs_raw_short = 0;
	int abs_raw_open = 0;

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		mutex_lock(&ts->pdata->thread_lock);
		touch_ts_disable_irq(ts->client->irq);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}
		abs_raw_short = F54Test('n', 1, buf);
		abs_raw_open = F54Test('o', 2, buf);

		ret = snprintf(buf,
				PAGE_SIZE, "========RESULT=======\n");

		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Absolute Sensing Short Test : RESULT: %s",
				(abs_raw_short > 0) ? "Pass\n" : "Fail\n");

		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Absolute Sensing Open Test  : RESULT: %s",
				(abs_raw_open > 0) ? "Pass\n" : "Fail\n");

		synaptics_ts_init(ts->client);

		touch_ts_enable_irq(ts->client->irq);

		mutex_unlock(&ts->pdata->thread_lock);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}
static ssize_t show_sensor_speed_test(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int sensor_speed = 0;
	int lower_ret = 0;
	int upper_ret = 0;

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		mutex_lock(&ts->pdata->thread_lock);
		touch_ts_disable_irq(ts->client->irq);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}

		lower_ret = get_limit(TxChannelCount,
				RxChannelCount,
				*ts->client,
				ts->pdata,
				"SensorSpeedLowerImageLimit",
				SensorSpeedLowerImage);
		upper_ret = get_limit(TxChannelCount,
				RxChannelCount,
				*ts->client,
				ts->pdata,
				"SensorSpeedUpperImageLimit",
				SensorSpeedUpperImage);

		if (lower_ret < 0 || upper_ret < 0) {
			TOUCH_I(
					"[%s] lower return = %d upper return = %d\n",
					__func__,
					lower_ret,
					upper_ret);
			TOUCH_I(
					"[%s][FAIL] Can not check the limit of sensor speed image\n",
					__func__);
			ret = snprintf(buf + ret,
					PAGE_SIZE - ret,
					"Can not check the limit of sensor speed image limit\n");
		} else {
			TOUCH_I(
					"[%s] lower return = %d upper return = %d\n",
					__func__,
					lower_ret,
					upper_ret);
			TOUCH_I(
					"[%s][SUCCESS] Can check the limit of sensor speed image limit\n",
					__func__);
			sensor_speed = F54Test('c', 0, buf);
		}

		ret = snprintf(buf,
				PAGE_SIZE,
				"========RESULT=======\n");

		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Sensor Speed Test : RESULT: %s",
				(sensor_speed > 0) ? "Pass\n" : "Fail\n");

		synaptics_ts_init(ts->client);

		touch_ts_enable_irq(ts->client->irq);

		mutex_unlock(&ts->pdata->thread_lock);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}
static ssize_t show_adc_range_test(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int adc_range = 0;
	int lower_ret = 0;
	int upper_ret = 0;

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		mutex_lock(&ts->pdata->thread_lock);
		touch_ts_disable_irq(ts->client->irq);


		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}
		lower_ret = get_limit(TxChannelCount,
				RxChannelCount,
				*ts->client,
				ts->pdata,
				"ADCLowerImageLimit",
				ADCLowerImage);
		upper_ret = get_limit(TxChannelCount,
				RxChannelCount,
				*ts->client,
				ts->pdata,
				"ADCUpperImageLimit",
				ADCUpperImage);

		if (lower_ret < 0 || upper_ret < 0) {
			TOUCH_I(
					"[%s] lower return = %d upper return = %d\n",
					__func__,
					lower_ret,
					upper_ret);
			TOUCH_I(
					"[%s][FAIL] Can not check the limit of ADC image\n",
					__func__);
			ret = snprintf(buf + ret,
					PAGE_SIZE - ret,
					"Can not check the limit of ADC image limit\n");
		} else {
			TOUCH_I(
					"[%s] lower return = %d upper return = %d\n",
					__func__,
					lower_ret,
					upper_ret);
			TOUCH_I(
					"[%s][SUCCESS] Can check the limit of ADC image limit\n",
					__func__);
			adc_range = F54Test('b', 0, buf);
		}

		ret = snprintf(buf, PAGE_SIZE, "========RESULT=======\n");

		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"ADC Range Test : RESULT: %s",
				(adc_range > 0) ? "Pass\n" : "Fail\n");

		synaptics_ts_init(ts->client);

		touch_ts_enable_irq(ts->client->irq);

		mutex_unlock(&ts->pdata->thread_lock);
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}
static ssize_t show_gnd_test(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int gnd = 0;

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		mutex_lock(&ts->pdata->thread_lock);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}
		touch_ts_disable_irq(ts->client->irq);

		gnd = F54Test('y', 0, buf);

		synaptics_ts_init(ts->client);

		touch_ts_enable_irq(ts->client->irq);

		mutex_unlock(&ts->pdata->thread_lock);

		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Gnd Test : RESULT: %s",
				(gnd > 0) ? "Pass\n" : "Fail\n");
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}
/*
 * show_atcmd_fw_ver
 *
 * show only firmware version.
 * It will be used for AT-COMMAND
 */
static ssize_t show_atcmd_fw_ver(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;

	if (ts->fw_info.version[0] > 0x50)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%s\n", ts->fw_info.version);
	else
		ret = snprintf(buf, PAGE_SIZE - ret,
				"V%d.%02d (0x%X/0x%X/0x%X/0x%X)\n",
				(ts->fw_info.version[3] & 0x80 ? 1 : 0),
				ts->fw_info.version[3] & 0x7F,
				ts->fw_info.version[0],
				ts->fw_info.version[1],
				ts->fw_info.version[2],
				ts->fw_info.version[3]);

	return ret;
}

static ssize_t store_tci(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u32 type = 0, value = 0;

	if (sscanf(buf, "%d %d", &type, &value) <= 0)
		return count;

	mutex_lock(&ts->pdata->thread_lock);
	tci_control(ts, type, (u8)value);
	mutex_unlock(&ts->pdata->thread_lock);

	return count;
}

static ssize_t show_tci(struct i2c_client *client, char *buf)
{
	int ret = 0;
	u8 buffer[5] = {0};
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	mutex_lock(&ts->pdata->thread_lock);
	touch_ts_i2c_read(client, ts->f12_reg.ctrl[20], 3, buffer);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"report_mode [%s]\n", (buffer[2] & 0x3) == 0x2 ?
			"WAKEUP_ONLY" : "NORMAL");
	touch_ts_i2c_read(client, ts->f12_reg.ctrl[27], 1, buffer);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"wakeup_gesture [%d]\n", buffer[0]);
	synaptics_ts_page_data_read(client, LPWG_PAGE,
			ts->f51_reg.lpwg_tapcount_reg, 5,
			buffer);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"TCI [%s]\n", (buffer[0] & 0x1) == 1 ?
			"enabled" : "disabled");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Tap Count [%d]\n", (buffer[0] & 0xf8) >> 3);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Min InterTap [%d]\n", buffer[1]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Max InterTap [%d]\n", buffer[2]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Touch Slop [%d]\n", buffer[3]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Tap Distance [%d]\n", buffer[4]);

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
}

static ssize_t store_reg_ctrl(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buffer[50] = {0};
	char command[6] = {0};
	int page = 0;
	int reg = 0;
	int offset = 0;
	int value = 0;

	if (sscanf(buf, "%s %d %d %d %d ",
				command, &page, &reg, &offset, &value) <= 0)
		return count;
	mutex_lock(&ts->pdata->thread_lock);

	if (!strcmp(command, "write")) {
		synaptics_ts_page_data_read(client, page, reg,
				offset+1, buffer);
		buffer[offset] = (u8)value;
		synaptics_ts_page_data_write(client, page, reg,
				offset+1, buffer);
	} else if (!strcmp(command, "read")) {
		synaptics_ts_page_data_read(client, page, reg,
				offset+1, buffer);
		TOUCH_D(DEBUG_BASE_INFO,
				"page[%d] reg[%d] offset[%d] = 0x%x\n",
				page, reg, offset, buffer[offset]);
	} else {
		TOUCH_D(DEBUG_BASE_INFO, "Usage\n");
		TOUCH_D(DEBUG_BASE_INFO, "Write page reg offset value\n");
		TOUCH_D(DEBUG_BASE_INFO, "Read page reg offset\n");
	}
	mutex_unlock(&ts->pdata->thread_lock);
	return count;
}

static ssize_t show_object_report(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	u8 object_report_enable_reg_addr = 0;
	u8 object_report_enable_reg = 0;

	object_report_enable_reg_addr = ts->f12_reg.ctrl[23];

	mutex_lock(&ts->pdata->thread_lock);
	ret = touch_ts_i2c_read(client, object_report_enable_reg_addr,
			sizeof(object_report_enable_reg),
			&object_report_enable_reg);
	mutex_unlock(&ts->pdata->thread_lock);

	if (ret < 0) {
		ret = snprintf(buf, PAGE_SIZE,
				"%s: Failed to read object_report_enable register\n",
				__func__);
	} else {
		u8 temp[8];
		int i;

		for (i = 0; i < 8; i++)
			temp[i] = (object_report_enable_reg >> i) & 0x01;

		ret = snprintf(buf, PAGE_SIZE,
				"\n============ read object_report_enable register ============\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Addr  Bit7  Bit6  Bit5  Bit4  Bit3  Bit2  Bit1  Bit0  HEX\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"------------------------------------------------------------\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" 0x%02X   %d     %d     %d     %d     %d     %d     %d     %d    0x%02X\n",
				object_report_enable_reg_addr,
				temp[7], temp[6], temp[5], temp[4],
				temp[3], temp[2], temp[1], temp[0],
				object_report_enable_reg);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"------------------------------------------------------------\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit0  :  [F]inger             -> %s\n",
				temp[0] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit1  : [S]tylus              -> %s\n",
				temp[1] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit2  : [P]alm                -> %s\n",
				temp[2] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit3  : [U]nclassified Object -> %s\n",
				temp[3] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit4  : [H]overing Finger     -> %s\n",
				temp[4] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit5  : [G]loved Finger       -> %s\n",
				temp[5] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit6  : [N]arrow Object Swipe -> %s\n",
				temp[6] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" Bit7  : Hand[E]dge            -> %s\n",
				temp[7] ? "Enable" : "Disable");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"============================================================\n\n");
	}

	return ret;
}

static ssize_t store_object_report(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret;
	char select[16];
	u8 value = 2;
	int select_cnt;
	int i;
	u8 bit_select = 0;
	u8 object_report_enable_reg_addr = 0;
	u8 object_report_enable_reg_old = 0;
	u8 object_report_enable_reg_new = 0;
	u8 old[8];
	u8 new[8];

	if (sscanf(buf, "%s %hhu", select, &value) <= 0)
		return count;

	if ((strlen(select) > 8) || (value > 1)) {
		TOUCH_I(
				"<writing object_report guide>\n");
		TOUCH_I(
				"echo [select] [value] > object_report\n");
		TOUCH_I(
				"select: [F]inger, [S]tylus, [P]alm, [U]nclassified Object, [H]overing Finger, [G]loved Finger, [N]arrow Object Swipe, Hand[E]dge\n");
		TOUCH_I(
				"select length: 1~8, value: 0~1\n");
		TOUCH_I(
				"ex) echo F 1 > object_report         (enable [F]inger)\n");
		TOUCH_I(
				"ex) echo s 1 > object_report         (enable [S]tylus)\n");
		TOUCH_I(
				"ex) echo P 0 > object_report         (disable [P]alm)\n");
		TOUCH_I(
				"ex) echo u 0 > object_report         (disable [U]nclassified Object)\n");
		TOUCH_I(
				"ex) echo HgNe 1 > object_report      (enable [H]overing Finger, [G]loved Finger, [N]arrow Object Swipe, Hand[E]dge)\n");
		TOUCH_I(
				"ex) echo eNGh 1 > object_report      (enable Hand[E]dge, [N]arrow Object Swipe, [G]loved Finger, [H]overing Finger)\n");
		TOUCH_I(
				"ex) echo uPsF 0 > object_report      (disable [U]nclassified Object, [P]alm, [S]tylus, [F]inger)\n");
		TOUCH_I(
				"ex) echo HguP 0 > object_report      (disable [H]overing Finger, [G]loved Finger, [U]nclassified Object, [P]alm)\n");
		TOUCH_I(
				"ex) echo HFnuPSfe 1 > object_report  (enable all object)\n");
		TOUCH_I(
				"ex) echo enghupsf 0 > object_report  (disbale all object)\n");
	} else {
		select_cnt = strlen(select);

		for (i = 0; i < select_cnt; i++) {
			switch ((char)(*(select + i))) {
			case 'F': case 'f':
				bit_select |= (0x01 << 0);
				break;   /* Bit0 : (F)inger*/
			case 'S': case 's':
				bit_select |= (0x01 << 1);
				break;   /* Bit1 : (S)tylus*/
			case 'P': case 'p':
				bit_select |= (0x01 << 2);
				break;   /* Bit2 : (P)alm*/
			case 'U': case 'u':
				bit_select |= (0x01 << 3);
				break;   /* Bit3 : (U)nclassified Object*/
			case 'H': case 'h':
				bit_select |= (0x01 << 4);
				break;   /* Bit4 : (H)overing Finger*/
			case 'G': case 'g':
				bit_select |= (0x01 << 5);
				break;   /* Bit5 : (G)loved Finger*/
			case 'N': case 'n':
				bit_select |= (0x01 << 6);
				break;   /* Bit6 : (N)arrow Object Swipe*/
			case 'E': case 'e':
				bit_select |= (0x01 << 7);
				break;   /* Bit7 : Hand(E)dge*/
			default:
				break;
			}
		}

		object_report_enable_reg_addr = ts->f12_reg.ctrl[23];

		mutex_lock(&ts->pdata->thread_lock);
		ret = touch_ts_i2c_read(client, object_report_enable_reg_addr,
				sizeof(object_report_enable_reg_old),
				&object_report_enable_reg_old);

		if (ret < 0) {
			TOUCH_E(
					"Failed to read object_report_enable_reg old value\n");
			mutex_unlock(&ts->pdata->thread_lock);
			return count;
		}

		object_report_enable_reg_new = object_report_enable_reg_old;

		if (value > 0)
			object_report_enable_reg_new |= bit_select;
		else
			object_report_enable_reg_new &= ~(bit_select);

		ret = touch_i2c_write_byte(client,
				object_report_enable_reg_addr,
				object_report_enable_reg_new);

		if (ret < 0) {
			TOUCH_E(
					"Failed to write object_report_enable_reg new value\n");
			mutex_unlock(&ts->pdata->thread_lock);
			return count;
		}

		ret = touch_ts_i2c_read(client, object_report_enable_reg_addr,
				sizeof(object_report_enable_reg_new),
				&object_report_enable_reg_new);
		mutex_unlock(&ts->pdata->thread_lock);

		if (ret < 0) {
			TOUCH_E(
					"Failed to read object_report_enable_reg new value\n");
			return count;
		}

		for (i = 0; i < 8; i++) {
			old[i] = (object_report_enable_reg_old >> i) & 0x01;
			new[i] = (object_report_enable_reg_new >> i) & 0x01;
		}

		TOUCH_I(
				"======= write object_report_enable register (before) =======\n");
		TOUCH_I(
				" Addr  Bit7  Bit6  Bit5  Bit4  Bit3  Bit2  Bit1  Bit0  HEX\n");
		TOUCH_I(
				"------------------------------------------------------------\n");
		TOUCH_I(
				" 0x%02X   %d     %d     %d     %d     %d     %d     %d     %d    0x%02X\n",
				object_report_enable_reg_addr, old[7], old[6],
				old[5], old[4], old[3], old[2], old[1], old[0],
				object_report_enable_reg_old);
		TOUCH_I(
				"============================================================\n");

		TOUCH_I(
				"======= write object_report_enable register (after) ========\n");
		TOUCH_I(
				" Addr  Bit7  Bit6  Bit5  Bit4  Bit3  Bit2  Bit1  Bit0  HEX\n");
		TOUCH_I(
				"------------------------------------------------------------\n");
		TOUCH_I(
				" 0x%02X   %d     %d     %d     %d     %d     %d     %d     %d    0x%02X\n",
				object_report_enable_reg_addr, new[7], new[6],
				new[5], new[4], new[3], new[2], new[1], new[0],
				object_report_enable_reg_new);
		TOUCH_I(
				"------------------------------------------------------------\n");
		TOUCH_I(
				" Bit0     :     [F]inger                  ->     %s\n",
				new[0] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit1     :     [S]tylus                  ->     %s\n",
				new[1] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit2     :     [P]alm                    ->     %s\n",
				new[2] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit3     :     [U]nclassified Object     ->     %s\n",
				new[3] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit4     :     [H]overing Finger         ->     %s\n",
				new[4] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit5     :     [G]loved Finger           ->     %s\n",
				new[5] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit6     :     [N]arrow Object Swipe     ->     %s\n",
				new[6] ? "Enable" : "Disable");
		TOUCH_I(
				" Bit7     :     Hand[E]dge                ->     %s\n",
				new[7] ? "Enable" : "Disable");
		TOUCH_I(
				"============================================================\n");
	}

	return count;
}

static ssize_t store_boot_mode(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buffer[1] = {0};

	if (sscanf(buf, "%d", &boot_mode) <= 0)
		return count;

	mutex_lock(&ts->pdata->thread_lock);
	switch (boot_mode) {
	case CHARGERLOGO_MODE:
		TOUCH_I("%s: Charger mode!!! Disable irq\n",
				__func__);

		if (is_product(ts, "PLG446", 6)) {
			DO_SAFE(synaptics_ts_page_data_read(client, ANALOG_PAGE,
					0x54, 1, buffer), error);
			buffer[0] = 0x00;
			DO_SAFE(synaptics_ts_page_data_write(client,
					ANALOG_PAGE, 0x54,
					1, buffer), error);
			DO_SAFE(synaptics_ts_page_data_read(client, ANALOG_PAGE,
					0x54, 1, buffer), error);
			if (!buffer[0]) {
				TOUCH_I(
						"%s: DDIC Control bit cleared.\n",
				__func__);
			}
			sleep_control(ts, 0, 1);
		}

		if (is_product(ts, "PLG349", 6))
			sleep_control(ts, 0, 1);
		break;
	case NORMAL_BOOT_MODE:
		TOUCH_I("%s: Normal boot mode!!! Enable irq\n",
				__func__);
		if (is_product(ts, "PLG446", 6)) {
			DO_SAFE(synaptics_ts_page_data_read(client, ANALOG_PAGE,
					0x54, 1, buffer), error);
			buffer[0] = 0x01;
			DO_SAFE(synaptics_ts_page_data_write(client,
					ANALOG_PAGE, 0x54,
					1, buffer), error);
			DO_SAFE(synaptics_ts_page_data_read(client, ANALOG_PAGE,
					0x54, 1, buffer), error);
			if (buffer[0]) {
				TOUCH_I(
						"%s: DDIC Control bit set 1 again.\n",
				__func__);
			}
		}
		sleep_control(ts, 1, 1);
		break;
	default:
		break;
	}
	mutex_unlock(&ts->pdata->thread_lock);

	return count;

error:
	mutex_unlock(&ts->pdata->thread_lock);
	TOUCH_E("%s, failed DDIC Control\n",
			__func__);
	return count;

}

static ssize_t store_sensing_test(struct i2c_client *client,
		const char *buf, size_t count)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	is_sensing = value;
	TOUCH_I("is_sensing:%d\n", is_sensing);

	return count;
}

static ssize_t show_noise_delta_test(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;
	int noise_delta = 0;

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] -- Not support this test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "Not support this test\n");
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		mutex_lock(&ts->pdata->thread_lock);

		if (need_scan_pdt) {
			SCAN_PDT();
			need_scan_pdt = false;
		}

		touch_ts_disable_irq(ts->client->irq);

		noise_delta = F54Test('x', 0, buf);

		touch_ts_enable_irq(ts->client->irq);
		synaptics_ts_init(ts->client);
		mutex_unlock(&ts->pdata->thread_lock);

		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"Noise Delta Test : RESULT: %s",
				(noise_delta > 0) ? "Pass\n" : "Fail\n");
	} else {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;

}


static ssize_t show_ts_noise(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret += snprintf(buf + ret,
			PAGE_SIZE - ret,
			"Test Count : %u\n",
			cnt);
	ret += snprintf(buf + ret,
			PAGE_SIZE - ret,
			"Current Noise State : %d\n",
			cns_aver);
	ret += snprintf(buf + ret,
			PAGE_SIZE - ret,
			"Interference Metric : %d\n",
			im_aver);
	if (!ts->pdata->panel_id) {
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret,
				"CID IM : %d\n",
				cid_im_aver);
	}
	ret += snprintf(buf + ret,
			PAGE_SIZE - ret,
			"Freq Scan IM : %d\n",
			freq_scan_im_aver);
	return ret;
}

static ssize_t store_ts_noise(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int value;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if ((ts->ts_state_flag.check_noise_menu == MENU_OUT)
			&& (value == MENU_ENTER)) {
		ts->ts_state_flag.check_noise_menu = MENU_ENTER;
	} else if ((ts->ts_state_flag.check_noise_menu == MENU_ENTER)
			&& (value == MENU_OUT)) {
		ts->ts_state_flag.check_noise_menu = MENU_OUT;
	} else {
		TOUCH_I("Already entered Check Noise menu .\n");
		TOUCH_I("check_noise_menu:%d, value:%d\n",
				ts->ts_state_flag.check_noise_menu, value);
		return count;
	}

	TOUCH_I("Check Noise = %s\n",
			(ts->ts_state_flag.check_noise_menu == MENU_OUT) ?
			"MENU_OUT" : "MENU_ENTER");
	TOUCH_I("TA state = %s\n",
		(touch_ta_status) ? "TA_CONNECTED" : "TA_DISCONNECTED");

	return count;
}

static ssize_t show_ts_noise_log_enable(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n",
			ts->ts_state_flag.ts_noise_log_flag);
	TOUCH_I("ts noise log flag = %s\n",
			(ts->ts_state_flag.ts_noise_log_flag
			 == TS_NOISE_LOG_DISABLE) ?
			"TS_NOISE_LOG_DISABLE" : "TS_NOISE_LOG_ENABLE");

	return ret;
}

static ssize_t store_ts_noise_log_enable(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int value;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if ((ts->ts_state_flag.ts_noise_log_flag == TS_NOISE_LOG_DISABLE)
			&& (value == TS_NOISE_LOG_ENABLE)) {
		ts->ts_state_flag.ts_noise_log_flag = TS_NOISE_LOG_ENABLE;
	} else if ((ts->ts_state_flag.ts_noise_log_flag == TS_NOISE_LOG_ENABLE)
			&& (value == TS_NOISE_LOG_DISABLE)) {
		ts->ts_state_flag.ts_noise_log_flag = TS_NOISE_LOG_DISABLE;
	} else {
		TOUCH_I("Already Enable TS Noise Log.\n");
		TOUCH_I("ts_noise_log_flag:%d, value:%d\n",
				ts->ts_state_flag.ts_noise_log_flag, value);
		return count;
	}

	TOUCH_I("ts noise log flag = %s\n",
			(ts->ts_state_flag.ts_noise_log_flag ==
			 TS_NOISE_LOG_DISABLE) ?
			"TS_NOISE_LOG_DISABLE" : "TS_NOISE_LOG_ENABLE");
	TOUCH_I("TA state = %s\n",
		(touch_ta_status) ? "TA_CONNECTED" : "TA_DISCONNECTED");

	return count;
}
static ssize_t show_diff_node(struct i2c_client *client, char *buf)
{

	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "show_diff_node: %d\n",  ref_chk_enable);

	return ret;
}


/* test code for operating ref chk code */
static ssize_t store_diff_node(struct i2c_client *client,
		const char *buf, size_t count)
{

	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	int ret = 0;

	if (sscanf(buf, "%d", &ref_chk_enable) <= 0)
		return count;

	mutex_lock(&ts->pdata->thread_lock);
	if (synaptics_ts_ic_ctrl(ts->client,
				IC_CTRL_BASELINE_REBASE, FORCE_CAL, &ret) < 0)
		TOUCH_E("IC_CTRL_REBASE handling fail\n");
	mutex_unlock(&ts->pdata->thread_lock);

	return count;
}

/* code for operating sp mirroring */
static ssize_t show_sp_link_touch_off(struct i2c_client *client, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "sp link touch status %d\n",
			sp_link_touch);
	return ret;
}

/* code for operating sp mirroing */
static ssize_t store_sp_link_touch_off(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	if (sscanf(buf, "%d", &sp_link_touch) <= 0) {
		TOUCH_E("Invalid value\n");
		return count;
	}
	TOUCH_D(DEBUG_BASE_INFO, "sp link touch off : %d\n",
			sp_link_touch);
	if (sp_link_touch) {
		if (is_product(ts, "PLG446", 6)
				|| is_product(ts, "PLG468", 6)) {
			touch_ts_disable_irq(ts->client->irq);
			/* tci_control(ts, PARTIAL_LPWG_ON, 1);
			DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
					DEVICE_CONTROL_SLEEP
					| DEVICE_CONTROL_CONFIGURED), error);
		*/
		}
	} else {
		if (is_product(ts, "PLG446", 6)
				|| is_product(ts, "PLG468", 6)) {
			touch_ts_enable_irq(ts->client->irq);
			/* tci_control(ts, PARTIAL_LPWG_ON, 0);
			DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
					DEVICE_CONTROL_NORMAL_OP
					| DEVICE_CONTROL_CONFIGURED), error);
		*/
		}
	}
	return count;
/*
error:
	TOUCH_E("Fail to change status\n");
	return count;
*/
}

static ssize_t show_lpwg_test_info(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
			(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"%d\n", atomic_read(&ts->lpwg_ctrl.is_suspend));

	return ret;
}

static ssize_t show_touch_wake_up_test(struct i2c_client *client, char *buf)
{
	int ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", touch_wake_count);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", touch_wake_test);

	return ret;
}

static ssize_t store_touch_wake_up_test(struct i2c_client *client,
		const char *buf, size_t count)
{
	int cmd = 0;

	if (sscanf(buf, "%d", &cmd) <= 0)
		return -EINVAL;

	switch (cmd) {
	case 0:
		if (touch_wake_test) {
			TOUCH_I("Stop touch wake test !\n");
			write_time_log(TOUCH_WAKE_COUNTER_LOG_PATH,
					"Stop touch wake test !\n", 1);
			touch_wake_test = false;
			touch_wake_count = 0;
		}
		break;
	case 1:
		if (!touch_wake_test) {
			TOUCH_I("Start touch wake test !\n");
			write_time_log(TOUCH_WAKE_COUNTER_LOG_PATH,
					"Start touch wake test !\n", 1);
			touch_wake_test = true;
		}
		break;
	case 2:
		TOUCH_I("Reset touch wake count !\n");
		touch_wake_count = 0;
		break;
	default:
		TOUCH_I("else case.\n");
	}

	return count;
}

static ssize_t show_pen_support(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int pen_support = 0;   /* 1: Support, 0: Not support */

	pen_support = GET_OBJECT_REPORT_INFO(ts->object_report,
			OBJECT_STYLUS_BIT);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pen_support);

	return ret;
}

static ssize_t show_palm_ctrl_mode(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", ts->pdata->role->palm_ctrl_mode);

	return ret;
}

static ssize_t store_palm_ctrl_mode(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int value;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value < PALM_REJECT_FW || value > PALM_REPORT) {
		TOUCH_I(
				"Invalid palm_ctrl_mode:%d (palm_ctrl_mode -> PALM_REJECT_FW)\n",
				value);
		value = PALM_REJECT_FW;
	}

	ts->pdata->role->palm_ctrl_mode = value;
	TOUCH_I("palm_ctrl_mode:%u\n", ts->pdata->role->palm_ctrl_mode);

	return count;
}

static ssize_t show_use_hover_finger(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE,
			"%u\n", ts->pdata->role->use_hover_finger);

	return ret;
}

static ssize_t store_use_hover_finger(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int value;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value < 0 || value > 1) {
		TOUCH_I("Invalid use_hover_finger value:%d\n", value);
		return count;
	}

	ts->pdata->role->use_hover_finger = value;
	TOUCH_I("use_hover_finger:%u\n",
			ts->pdata->role->use_hover_finger);

	return count;
}

static ssize_t show_use_rmi_dev(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", ts->pdata->role->use_rmi_dev);

	return ret;
}

static ssize_t store_use_rmi_dev(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int value;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value < 0 || value > 1) {
		TOUCH_I("Invalid use_rmi_dev value:%d\n", value);
		return count;
	}

	ts->pdata->role->use_rmi_dev = value;
	TOUCH_I("use_rmi_dev:%u\n", ts->pdata->role->use_rmi_dev);

	return count;
}


static ssize_t show_status_normal_calibration(struct i2c_client *client,
		char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	u8 crc_buffer = 0;
	u8 start_buffer = 0;
	u8 calibration_status = 0;
	u8 crc_status = 0;

	if (ts->pdata->panel_id != 1) {
		TOUCH_I("Panel id : %d, Not supproted f/w calibration\n",
				ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	mutex_lock(&ts->pdata->thread_lock);
	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		ret = synaptics_ts_page_data_read(client,
						ANALOG_PAGE,
						CALIBRATION_STATUS_REG,
						1, &crc_buffer);
		if (ret < 0) {
			TOUCH_E("Failed to read calibration_status_reg\n");
			goto error;
		}

		ret = synaptics_ts_page_data_read(client,
						ANALOG_PAGE,
						CALIBRATION_FLAGS_REG,
						1, &start_buffer);
		if (ret < 0) {
			TOUCH_E("Failed to read calibration start reg\n");
			goto error;
		}

		TOUCH_I(
			"[%s] start_buffer = 0x%02x, crc_buffer = 0x%02x\n",
			__func__, start_buffer, crc_buffer);

		calibration_status = (start_buffer & 0x01);
		crc_status = (crc_buffer & 0x02) >> 1;

		if (calibration_status == 0) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"calibration_status = %d\n",
					calibration_status);
			if (!crc_status) {
				TOUCH_I(
						"Checksum of calibration values is good, Calibration is not going on\n");
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"CRC_status = %d\n",
						crc_status);
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"Checksum of calibration values is good.\n");
			} else {
				TOUCH_E(
						"Checksum of calibration values is bad.\n");
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"CRC_status = %d\n",
						crc_status);
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"Checksum of calibration values is bad.\n");
			}
		} else if (calibration_status == 1) {
			TOUCH_I("Calibration is in progress\n");
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"calibration_status = %d\n",
					calibration_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Calibration is in progress\n");
		} else {
			TOUCH_E("Invalidated to calibration_status\n");
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"calibration_status = %d\n",
					calibration_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Invalidated to calibration_status\n");
		}
	} else {
		TOUCH_E(
				"state is suspend, Failed to read register because cannot use I2C\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"state is suspend, Failed to read register because cannot use I2C\n");
	}

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;

error:
	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
}

static ssize_t show_normal_calibration(struct i2c_client *client,
		char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int cal_exist = 0;
	u8 buffer = 0;
	u8 calibration_on = 0x01;

	if (ts->pdata->panel_id != 1) {
		TOUCH_I("Panel id : %d, Not supproted f/w calibration\n",
				ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	mutex_lock(&ts->pdata->thread_lock);

	cal_exist  = check_cal_magic_key();
	if (cal_exist  < 0) {
		TOUCH_I("[%s] In Cal MAGIC Key Not Exist\n", __func__);
		mutex_unlock(&ts->pdata->thread_lock);
		return ret;
	}

	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		ret = synaptics_ts_page_data_read(client,
					ANALOG_PAGE, CALIBRATION_FLAGS_REG,
					1, &buffer);

		if (ret < 0) {
			TOUCH_E("Failed to read calibration_flag_reg\n");
			goto error;
		}
		TOUCH_I("[%s] buffer = 0x%02x\n",
				__func__, buffer);

		if ((buffer & calibration_on)) {
			TOUCH_E("Now Running Calibration....\n");
			goto error;
		}

		buffer = buffer | calibration_on;

		ret = synaptics_ts_page_data_write_byte(client,
					ANALOG_PAGE, CALIBRATION_FLAGS_REG,
					buffer);
		if (ret < 0) {
			TOUCH_E("Failed to write calibration_flag_reg value\n");
			goto error;
		}
		TOUCH_I(
				"Start Normal Calibration\n");

	} else {
		TOUCH_E(
				"state is suspend, Failed to Calibration because cannot use I2C\n");
	}

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
error:
	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
}



static ssize_t show_status_lpwg_calibration(struct i2c_client *client,
		char *buf)
{
	struct synaptics_ts_data *ts
		= (struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	u8 crc_buffer = 0;
	u8 start_buffer = 0;
	u8 calibration_status = 0;
	u8 crc_status = 0;

	if (ts->pdata->panel_id != 1) {
		TOUCH_I(
				"Panel id : %d, Not supproted f/w calibration\n",
				ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	mutex_lock(&ts->pdata->thread_lock);

	ret = synaptics_ts_page_data_read(client,
			ANALOG_PAGE, CALIBRATION_STATUS_REG,
			1, &crc_buffer);
	if (ret < 0) {
		TOUCH_E("Failed to read calibration_status_reg\n");
		goto error;
	}

	ret = synaptics_ts_page_data_read(client,
			ANALOG_PAGE, CALIBRATION_FLAGS_REG,
			1, &start_buffer);
	if (ret < 0) {
		TOUCH_E("Failed to read calibration_start_reg\n");
		goto error;
	}
	TOUCH_I(
		"[%s] start_buffer = 0x%02x, crc_buffer = 0x%02x\n",
		__func__, start_buffer, crc_buffer);

	calibration_status = (start_buffer & 0x02) >> 1;
	crc_status = (crc_buffer & 0x01);

	if (calibration_status == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"calibration_status = %d\n",
				calibration_status);
		if (!crc_status) {
			TOUCH_I(
					"Checksum of calibration values is good, Calibration is not going on\n");
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"CRC_status = %d\n", crc_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Checksum of calibration values is good\n");
		} else {
			TOUCH_E(
					"Checksum of calibration values is bad, Retry calibration\n");
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"CRC_status = %d\n", crc_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"Checksum of calibration values is bad, Retry calibration\n");
		}
	} else if (calibration_status == 1) {
		TOUCH_I("Calibration is in progress\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"calibration_status = %d\n",
				calibration_status);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Calibration is in progress\n");
	} else {
		TOUCH_E("Invalidated to calibration_status\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"calibration_status = %d\n",
				calibration_status);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Invalidated to calibration_status\n");
	}

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
error:
	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
}

static ssize_t show_lpwg_calibration(struct i2c_client *client,
		char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int cal_exist = 0;
	u8 buffer = 0;
	u8 calibration_on = 0x02;

	if (ts->pdata->panel_id != 1) {
		TOUCH_I("Panel id : %d, Not supproted f/w calibration\n",
				ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	mutex_lock(&ts->pdata->thread_lock);

	cal_exist  = check_cal_magic_key();
	if (cal_exist  < 0) {
		TOUCH_I("[%s] In Cal MAGIC Key Not Exist\n", __func__);
		mutex_unlock(&ts->pdata->thread_lock);
		return ret;
	}

	if (power_state == POWER_SLEEP) {
		ret = synaptics_ts_page_data_read(client,
				ANALOG_PAGE, CALIBRATION_FLAGS_REG,
				1, &buffer);

		if (ret < 0) {
			TOUCH_E("Failed to read calibration_flag_reg\n");
			goto error;
		}
		TOUCH_I("[%s] buffer = 0x%02x\n",
				__func__, buffer);

		if ((buffer & calibration_on)) {
			TOUCH_E("Now Running Calibration....\n");
			goto error;
		}

		buffer = buffer | calibration_on;

		ret = synaptics_ts_page_data_write_byte(client,
				ANALOG_PAGE, CALIBRATION_FLAGS_REG,
				buffer);
		if (ret < 0) {
			TOUCH_E("Failed to write calibration_flag_reg value\n");
			goto error;
		}
		TOUCH_I("Start LPWG Calibration\n");

	} else {
		TOUCH_E(
				"state is suspend, Failed to Calibration because cannot use I2C\n");
	}

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
error:
	mutex_unlock(&ts->pdata->thread_lock);
	return ret;

}

static ssize_t show_get_calibration(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int ret_size = 0;
	int err = 0;
	u8 buffer = 0;
	u8 old_report_type = 0x0;
	u8 start_get_cal_data = 0x54;
	u8 fifo_init = 0x00;
	int retry_cnt = 300;
	u8 cal_data[MAX_CAL_DATA_SIZE] = {0,};
	u8 nd_cal_data[MAX_ND_CAL_DATA_SIZE] = {0,};
	u8 *save_buf = NULL;
	u8 freq = 0;
	int i = 0;
	int k = 0;
	int line = 0;
	u8 *detail;
	u8 *coarse;
	u8 *fine;
	u8 *nd_detail;
	u8 *nd_coarse;
	u8 *nd_fine;
	char *f_path = "/sdcard/touch_cal_data.txt";
	int cal_exist = 0;

	cal_exist  = check_cal_magic_key();
	if (cal_exist  < 0) {
		TOUCH_I("[%s] In Cal MAGIC Key Not Exist\n", __func__);
		return ret;
	}

	if (save_buf == NULL) {
		TOUCH_E("fail to allocate memory\n");
		return ret;
	}

	if (ts->pdata->panel_id != 1) {
		TOUCH_I("Panel id : %d, Not supproted f/w calibration\n",
				ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	save_buf = kzalloc(sizeof(u8)*MAX_CAL_LOG_SIZE, GFP_KERNEL);
	detail = kzalloc(sizeof(u8)*MAX_DETAIL_SIZE, GFP_KERNEL);
	coarse = kzalloc(sizeof(u8)*MAX_COARSE_SIZE, GFP_KERNEL);
	fine = kzalloc(sizeof(u8)*MAX_FINE_SIZE, GFP_KERNEL);
	nd_detail = kzalloc(sizeof(u8)*MAX_ND_DETAIL_SIZE, GFP_KERNEL);
	nd_coarse = kzalloc(sizeof(u8)*MAX_ND_COARSE_SIZE, GFP_KERNEL);
	nd_fine = kzalloc(sizeof(u8)*MAX_ND_FINE_SIZE, GFP_KERNEL);
	mutex_lock(&ts->pdata->thread_lock);

	/*Step 0. Backup Old Report Type*/
	err = synaptics_ts_page_data_read(client,
			ANALOG_PAGE, ts->f54.dsc.data_base,
			1, &buffer);
	if (err < 0) {
		TOUCH_E("Failed to read F54 Data Base\n");
		goto error;
	}

	old_report_type = buffer;

	/*Step 1. Set 84 to FIFO Index 1, FIFO Index 2 */
	err = synaptics_ts_page_data_write_byte(client,
			ANALOG_PAGE, ts->f54.dsc.data_base,
			start_get_cal_data);
	if (err < 0) {
		TOUCH_E("Failed to write F54 Data Base\n");
		goto error;
	}

	/*Step 2. Set 0 to FIFO Index 1, FIFO Index 2 */
	err = synaptics_ts_page_data_write_byte(client,
			ANALOG_PAGE, F54_FIFO_INDEX_LSB,
			fifo_init);
	if (err < 0) {
		TOUCH_E("Failed to write F54_FIFO_INDEX_LSB\n");
		goto error;
	}

	err = synaptics_ts_page_data_write_byte(client,
			ANALOG_PAGE, F54_FIFO_INDEX_MSB,
			fifo_init);
	if (err < 0) {
		TOUCH_E("Failed to write F54_FIFO_INDEX_MSB\n");
		goto error;
	}

	/*Step 3. Send Get Report Command*/
	write_time_log(f_path, NULL, 0);
	for (freq = 0; freq < 3; freq++) {
		err = synaptics_ts_page_data_read(client,
				ANALOG_PAGE, ts->f54.dsc.command_base,
				1, &buffer);
		if (err < 0) {
			TOUCH_E("Failed to read F54 CMD Base\n");
			goto error;
		}
		/*For collecting lpwg calibration Data*/
		if (power_state == POWER_SLEEP)
			freq = 3;

		buffer |= (freq << 2);
		buffer |= 0x01;

		err = synaptics_ts_page_data_write_byte(client,
				ANALOG_PAGE, ts->f54.dsc.command_base,
				buffer);
		if (err < 0) {
			TOUCH_E("Failed to write F54 CMD Base\n");
			goto error;
		}

		/*waiting clear get report bit*/
		retry_cnt = 300;
		do {
			err = synaptics_ts_page_data_read(client,
					ANALOG_PAGE, ts->f54.dsc.command_base,
					1, &buffer);
			buffer = (buffer & 0x01);
			usleep(10000);
			retry_cnt--;

			if (retry_cnt <= 0) {
				TOUCH_E("Fail to Read Get Report type.\n");
				goto error;
			}
		} while (buffer);

		memset(cal_data, 0, sizeof(cal_data));
		memset(cal_data, 0, sizeof(nd_cal_data));

		/*Step 4. Get Cal Data*/
		err = synaptics_ts_page_data_read(client,
				ANALOG_PAGE, F54_REPORT_DATA,
				MAX_CAL_DATA_SIZE, &cal_data[0]);
		if (err < 0) {
			TOUCH_E("Failed to read F54_REPORT_DATA\n");
			goto error;
		}

		memset(detail, 0x0, sizeof(u8)*MAX_DETAIL_SIZE);
		memset(coarse, 0x0, sizeof(u8)*MAX_COARSE_SIZE);
		memset(fine, 0x0, sizeof(u8)*MAX_FINE_SIZE);
		memset(nd_detail, 0x0, sizeof(u8)*MAX_ND_DETAIL_SIZE);
		memset(nd_coarse, 0x0, sizeof(u8)*MAX_ND_COARSE_SIZE);
		memset(nd_fine, 0x0, sizeof(u8)*MAX_ND_FINE_SIZE);


		ret_size += snprintf(save_buf + ret_size,
				MAX_CAL_LOG_SIZE - ret_size,
				"\n ============ Calibration Data [Freq = %d] ============\n",
				freq);
		TOUCH_I("Start Get Cal Data, Freq = %d", freq);
		k = 0;
		for (i = 0; i < MAX_CAL_DATA_SIZE; i += 2) {
			detail[k] = cal_data[i];
			coarse[k] = (cal_data[i+1] & 0xf0) >> 4;
			fine[k] = cal_data[i+1] & 0x0f;
			k++;
		}

		ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"==== detail ==\n");
		line = 0;
		for (i = 0; i < (MAX_CAL_DATA_SIZE/2); i++) {
			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"%d ", detail[i]);
			if (((i+1)%18 == 0) && (i != 0)) {
				TOUCH_I("\n");
				ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						" %d\n" , ++line);
				}
		}

		ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						"\n==== coarse ==\n");

		line = 0;
		for (i = 0; i < (MAX_CAL_DATA_SIZE/2); i++) {
			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"%d ", coarse[i]);
			if (((i+1)%18 == 0) && (i != 0)) {
				TOUCH_I("\n");
				ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						" %d\n" , ++line);
				}
		}

		ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						"\n==== fine ==\n");

		line = 0;
		for (i = 0; i < (MAX_CAL_DATA_SIZE/2); i++) {
			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"%d ", fine[i]);

			if (((i+1)%18 == 0) && (i != 0)) {
				TOUCH_I("\n");
				ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						" %d\n" , ++line);
				}
		}

		ret_size += snprintf(save_buf + ret_size,
				MAX_CAL_LOG_SIZE - ret_size,
				"\n");

		/*Get ND CAL*/
		err = synaptics_ts_page_data_read(client,
				ANALOG_PAGE,
				F54_REPORT_DATA,
				MAX_ND_CAL_DATA_SIZE,
				&nd_cal_data[0]);
		if (err < 0) {
			TOUCH_E("Failed to read calibration_status_reg\n");
			goto error;
		}
		ret_size += snprintf(save_buf + ret_size,
				MAX_CAL_LOG_SIZE - ret_size,
				"\n ============ ND Calibration Data [Freq = %d] ============\n",
				freq);
		TOUCH_I("Start Get ND Cal Data, Freq = %d", freq);

		k = 0;
		for (i = 0; i < MAX_ND_CAL_DATA_SIZE; i += 2) {
			nd_detail[k] = cal_data[i];
			nd_coarse[k] = (cal_data[i+1] & 0xf0) >> 4;
			nd_fine[k] = cal_data[i+1] & 0x0f;
			k++;
		}

			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"==== detail ==\n");
		line = 0;
		for (i = 0; i < (MAX_ND_CAL_DATA_SIZE/2); i++) {
			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"%d ", nd_detail[i]);
			if (((i+1)%2 == 0) && (i != 0)) {
				TOUCH_I("\n");
				ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						" %d\n" , ++line);
			}
		}

		ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"\n==== coarse ==\n");

		line = 0;
		for (i = 0; i < (MAX_ND_CAL_DATA_SIZE/2); i++) {
			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"%d ", nd_coarse[i]);
			if (((i+1)%2 == 0) && (i != 0)) {
				TOUCH_I("\n");
				ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						" %d\n" , ++line);
				}
			}

		ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"\n==== fine ==\n");

		line = 0;
		for (i = 0; i < (MAX_ND_CAL_DATA_SIZE/2); i++) {
			ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"%d ", nd_fine[i]);

			if (((i+1)%2 == 0) && (i != 0)) {
				TOUCH_I("\n");
				ret_size += snprintf(save_buf + ret_size,
						MAX_CAL_LOG_SIZE - ret_size,
						" %d\n" , ++line);
				}
			}

		ret_size += snprintf(save_buf + ret_size,
					MAX_CAL_LOG_SIZE - ret_size,
					"\n");
	}

	ret_size += snprintf(save_buf + ret_size,
			MAX_CAL_LOG_SIZE - ret_size,
			"\n");

	write_log(f_path, save_buf);

	/*Step 5. Restore Report Mode*/
	err = synaptics_ts_page_data_write_byte(client,
			ANALOG_PAGE, ts->f54.dsc.data_base,
			old_report_type);
	if (err < 0) {
		TOUCH_E("Failed to read calibration_status_reg\n");
		goto error;
	}

	mutex_unlock(&ts->pdata->thread_lock);
	TOUCH_I("Cal Data Extract Complete.");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Cal Data Extract Complete.\n");
	kfree(save_buf);
	kfree(detail);
	kfree(coarse);
	kfree(fine);
	kfree(nd_detail);
	kfree(nd_coarse);
	kfree(nd_fine);
	return ret;

error:
	TOUCH_E("Fail to get Calibration Data");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"Fail to get Calibration Data.\n");
	mutex_unlock(&ts->pdata->thread_lock);
	kfree(save_buf);
	kfree(detail);
	kfree(coarse);
	kfree(fine);
	kfree(nd_detail);
	kfree(nd_coarse);
	kfree(nd_fine);
	return ret;
}

static ssize_t show_swipe_param(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	struct swipe_data *swp = &ts->swipe;
	int ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"swipe_mode = 0x%02X\n",
			swp->swipe_mode);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"support_swipe = 0x%02X\n",
			swp->support_swipe);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"=================================================\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.min_distance = 0x%02X (%dmm)\n",
			swp->down.min_distance,
			swp->down.min_distance);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.ratio_thres = 0x%02X (%d%%)\n",
			swp->down.ratio_thres,
			swp->down.ratio_thres);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.ratio_chk_period = 0x%02X (%dframes)\n",
			swp->down.ratio_chk_period,
			swp->down.ratio_chk_period);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.ratio_chk_min_distance = 0x%02X (%dmm)\n",
			swp->down.ratio_chk_min_distance,
			swp->down.ratio_chk_min_distance);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.min_time_thres =  0x%02X (%d0ms)\n",
			swp->down.min_time_thres,
			swp->down.min_time_thres);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.max_time_thres = 0x%02X (%d0ms)\n",
			swp->down.max_time_thres,
			swp->down.max_time_thres);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"down.active_area = x0,y0(%d,%d) x1,y1(%d,%d)\n",
			swp->down.active_area_x0, swp->down.active_area_y0,
			swp->down.active_area_x1, swp->down.active_area_y1);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"=================================================\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.min_distance = 0x%02X (%dmm)\n",
			swp->up.min_distance,
			swp->up.min_distance);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.ratio_thres = 0x%02X (%d%%)\n",
			swp->up.ratio_thres,
			swp->up.ratio_thres);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.ratio_chk_period = 0x%02X (%dframes)\n",
			swp->up.ratio_chk_period,
			swp->up.ratio_chk_period);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.ratio_chk_min_distance = 0x%02X (%dmm)\n",
			swp->up.ratio_chk_min_distance,
			swp->up.ratio_chk_min_distance);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.min_time_thres =  0x%02X (%d0ms)\n",
			swp->up.min_time_thres,
			swp->up.min_time_thres);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.max_time_thres = 0x%02X (%d0ms)\n",
			swp->up.max_time_thres,
			swp->up.max_time_thres);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"up.active_area = x0,y0(%d,%d) x1,y1(%d,%d)\n",
			swp->up.active_area_x0, swp->up.active_area_y0,
			swp->up.active_area_x1, swp->up.active_area_y1);

	return ret;
}

static ssize_t store_swipe_param(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	struct swipe_data *swp = &ts->swipe;
	struct swipe_ctrl_info *swpd = NULL;
	char direction;
	char select;
	u16 value;

	if (swp->support_swipe == NO_SUPPORT_SWIPE) {
		TOUCH_E("support_swipe:0x%02X\n", swp->support_swipe);
		return count;
	}

	if (sscanf(buf, "%c %c %hu", &direction, &select, &value) <= 0)
		return count;

	if (((direction != 'd') && (direction != 'u'))
			|| (select < 'a')
			|| (select > 'j')) {
		TOUCH_I("<writing swipe_param guide>\n");
		TOUCH_I("echo [direction] [select] [value] > swipe_param\n");
		TOUCH_I("[direction]: d(down), u(up)\n");
		TOUCH_I("[select]:\n");
		TOUCH_I("a(min_distance),\n");
		TOUCH_I("b(ratio_thres),\n");
		TOUCH_I("c(ratio_chk_period),\n");
		TOUCH_I("d(ratio_chk_min_distance),\n");
		TOUCH_I("e(min_time_thres),\n");
		TOUCH_I("f(max_time_thres),\n");
		TOUCH_I("g(active_area_x0),\n");
		TOUCH_I("h(active_area_y0),\n");
		TOUCH_I("i(active_area_x1),\n");
		TOUCH_I("j(active_area_y1)\n");
		TOUCH_I("[value]: (0x00~0xFF) or (0x00~0xFFFF)\n");
		return count;
	}

	switch (direction) {
	case 'd':
		swpd = &swp->down;
		break;
	case 'u':
		swpd = &swp->up;
		break;
	default:
		TOUCH_I("unknown direction(%c)\n", direction);
		return count;
	}

	switch (select) {
	case 'a':
		swpd->min_distance = GET_LOW_U8_FROM_U16(value);
		break;
	case 'b':
		swpd->ratio_thres = GET_LOW_U8_FROM_U16(value);
		break;
	case 'c':
		swpd->ratio_chk_period = GET_LOW_U8_FROM_U16(value);
		break;
	case 'd':
		swpd->ratio_chk_min_distance = GET_LOW_U8_FROM_U16(value);
		break;
	case 'e':
		swpd->min_time_thres = value;
		break;
	case 'f':
		swpd->max_time_thres = value;
		break;
	case 'g':
		swpd->active_area_x0 = value;
		break;
	case 'h':
		swpd->active_area_y0 = value;
		break;
	case 'i':
		swpd->active_area_x1 = value;
		break;
	case 'j':
		swpd->active_area_y1 = value;
		break;
	default:
		break;
	}

	mutex_lock(&ts->pdata->thread_lock);
	swipe_enable(ts);
	mutex_unlock(&ts->pdata->thread_lock);

	return count;
}

static ssize_t store_swipe_mode(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	struct swipe_data *swp = &ts->swipe;
	int down = 0;
	int up = 0;
	u8 mode = 0;

	if (swp->support_swipe == NO_SUPPORT_SWIPE) {
		TOUCH_E("support_swipe:0x%02X\n", swp->support_swipe);
		return count;
	}

	if (sscanf(buf, "%d %d", &down, &up) <= 0)
		return count;

	if (down)
		mode |= SWIPE_DOWN_BIT;
	else
		mode &= ~(SWIPE_DOWN_BIT);

	if (up)
		mode |= SWIPE_UP_BIT;
	else
		mode &= ~(SWIPE_UP_BIT);

	swp->swipe_mode = mode;

	return count;
}
static ssize_t show_hidden_normal_cal_state(struct i2c_client *client,
		char *buf)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	u8 start_buffer = 0;
	u8 crc_buffer = 0;
	u8 calibration_status = 0;
	u8 crc_status = 0;

	if (ts->pdata->panel_id != 1) {
		TOUCH_I("[%s] Panel id : %d, Not supproted f/w calibration\n",
				__func__, ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	mutex_lock(&ts->pdata->thread_lock);
	if (power_state == POWER_ON || power_state == POWER_WAKE) {
		ret = synaptics_ts_page_data_read(client,
						ANALOG_PAGE,
						CALIBRATION_STATUS_REG,
						1, &crc_buffer);
		if (ret < 0) {
			TOUCH_E(
				"[%s] Failed to read calibration_status_reg\n",
				__func__);
			goto error;
		}

		ret = synaptics_ts_page_data_read(client,
						ANALOG_PAGE,
						CALIBRATION_FLAGS_REG,
						1, &start_buffer);
		if (ret < 0) {
			TOUCH_E(
				"[%s] Failed to read calibration_start_reg\n",
				__func__);
			goto error;
		}
		TOUCH_I(
			"[%s] Calibration start_buffer = 0x%02x, crc_buffer = 0x%02x\n",
			__func__, start_buffer, crc_buffer);

		calibration_status = (start_buffer & 0x01);
		crc_status = (crc_buffer & 0x02) >> 1;
		/*Calibration Result*/
		/*Bad = 0, Good = 1, In Progress = 2, Error = 99*/
		if (calibration_status == 0) {
			if (!crc_status) {
				TOUCH_I(
						"[%s] Normal CRC is Good. = %d\n",
						__func__, crc_status);
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"status = 1\n");
			} else {
				TOUCH_E(
						"[%s] Normal CRC Value is bad, crc = %d\n",
						__func__, crc_status);
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"status = 0\n");
			}
		} else if (calibration_status == 1) {
			TOUCH_I("Calibration is in progress = %d\n",
				calibration_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"status = 2\n");
		} else {
			TOUCH_E("Invalidated to calibration_status = %d\n",
				calibration_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"status = 99\n");
		}
	} else {
		TOUCH_E(
				"[%s] state is suspend, Failed to read register because cannot use I2C\n",
				__func__);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"state is suspend, Failed to read register because cannot use I2C\n");
	}

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;

error:
	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
}

static ssize_t show_hidden_lpwg_cal_state(struct i2c_client *client,
		char *buf)
{
	struct synaptics_ts_data *ts
		= (struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	u8 start_buffer = 0;
	u8 crc_buffer = 0;
	u8 calibration_status = 0;
	u8 crc_status = 0;

	if (ts->pdata->panel_id != 1) {
		TOUCH_I(
				"[%s] Panel id : %d, Not supproted f/w calibration\n",
				__func__, ts->pdata->panel_id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Not supproted f/w calibration\n");
		return ret;
	}

	mutex_lock(&ts->pdata->thread_lock);

	ret = synaptics_ts_page_data_read(client,
			ANALOG_PAGE, CALIBRATION_STATUS_REG,
			1, &crc_buffer);
	if (ret < 0) {
		TOUCH_E(
			"[%s] Failed to read calibration_status_reg\n",
			__func__);
		goto error;
	}

	ret = synaptics_ts_page_data_read(client,
			ANALOG_PAGE, CALIBRATION_FLAGS_REG,
			1, &start_buffer);
	if (ret < 0) {
		TOUCH_E(
			"[%s] Failed to read calibration_start_reg\n",
			__func__);
		goto error;
	}
	TOUCH_I(
		"[%s] start_buffer = 0x%02x, crc_buffer = 0x%02x\n",
		__func__, start_buffer, crc_buffer);

	calibration_status = (start_buffer & 0x02) >> 1;
	crc_status = (crc_buffer & 0x01);

	/*Calibration Result*/
	/*Bad = 0, Good = 1, In Progress = 2, Error = 99*/
	if (calibration_status == 0) {
		if (!crc_status) {
			TOUCH_I(
					"[%s] LPWG CRC Value is good = %d\n",
					__func__, crc_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"status = 1\n");
		} else {
			TOUCH_E(
					"[%s] LPWG CRC Value is Bad = %d\n",
					__func__, crc_status);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"status = 0\n");
		}
	} else if (calibration_status == 1) {
		TOUCH_I(
			"[%s] Calibration is in progress = %d\n",
			__func__, calibration_status);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"status = 2\n");
	} else {
		TOUCH_E(
			"[%s] Invalidated to calibration_status = %d\n",
			__func__, calibration_status);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"status = 99\n");
	}

	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
error:
	mutex_unlock(&ts->pdata->thread_lock);
	return ret;
}

static ssize_t show_lpwg_disable(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts
		= (struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n",
			ts->lpwg_ctrl.hidden_lpwg_disable);
	TOUCH_I("hidden_lpwg_disable = %s\n",
			ts->lpwg_ctrl.hidden_lpwg_disable ?
			"lpwg disable" : "lpwg enable");

	return ret;

}

static ssize_t store_lpwg_disable(struct i2c_client *client,
		const char *buf, size_t count)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int value;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if ((ts->lpwg_ctrl.hidden_lpwg_disable == 0)
			&& (value == 1)) {
		ts->lpwg_ctrl.hidden_lpwg_disable = 1;
	} else if ((ts->lpwg_ctrl.hidden_lpwg_disable == 1)
			&& (value == 0)) {
		ts->lpwg_ctrl.hidden_lpwg_disable = 0;
	} else {
		TOUCH_I("hidden_lpwg_disable: %d, value: %d\n",
				ts->lpwg_ctrl.hidden_lpwg_disable, value);
		return count;
	}

	TOUCH_I("hidden_lpwg_disable = %s\n",
			ts->lpwg_ctrl.hidden_lpwg_disable ?
			"set lpwg disable" : "set lpwg enable");

	return count;
}

static ssize_t show_lpwg_sd(struct i2c_client *client, char *buf)
{
	struct synaptics_ts_data *ts
		= (struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;
	int adc_test = 0;
        int rsp_test = 0;
	int lower_img = 0;
	int upper_img = 0;

	TOUCH_I("[%s] start.\n", __func__);

	if (power_state == POWER_SLEEP) {
		if (is_product(ts, "PLG446", 6)) {
			wake_lock(&ts->touch_rawdata);
			write_time_log(NULL, NULL, 0);
			msleep(30);
			write_firmware_version_log(ts);
			mutex_lock(&ts->pdata->thread_lock);
			adc_test = synaptics_ts_lpwg_adc_test(client);
			msleep(20);
			ret = snprintf(buf,
					PAGE_SIZE,
					"========RESULT=======\n");

			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"LPWG RawData : %s",
					(adc_test == 1)
					? "Pass\n"
					: "Fail\n");
			wake_unlock(&ts->touch_rawdata);
			mutex_unlock(&ts->pdata->thread_lock);
		} else {
			wake_lock(&ts->touch_rawdata);
			lower_img = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"RspLPWGLowerLimit",
					LowerImage);
			upper_img = get_limit(TxChannelCount,
					RxChannelCount,
					*ts->client,
					ts->pdata,
					"RspLPWGUpperLimit",
					UpperImage);
			if (lower_img < 0 || upper_img < 0) {
				TOUCH_I(
						"[%s] lower return = %d upper return = %d\n",
						__func__, lower_img, upper_img);
				TOUCH_I(
						"[%s][FAIL] Can not check the limit of raw cap\n",
						__func__);
				wake_unlock(&ts->touch_rawdata);
				return ret;

			} else {
				TOUCH_I(
						"Getting limit of LPWG raw cap is success\n");
			}

			/*Exception Handle of flat and curved state*/
			if(mfts_mode == 2 || mfts_mode == 3){
			    TOUCH_I("Can not execute lpwg_sd, mfts_mode = %d\n",mfts_mode);
			    ret += snprintf(buf + ret, PAGE_SIZE - ret,
			   "LPWG RawData : Not Support\n");
			    wake_unlock(&ts->touch_rawdata);
			    return ret;
			}

			msleep(1000);
			SCAN_PDT();
			touch_ts_disable_irq(ts->client->irq);
			mutex_lock(&ts->pdata->thread_lock);
			write_time_log(NULL, NULL, 0);
			msleep(30);
			rsp_test = F54Test('q', 5, buf);
			msleep(20);

			ret = snprintf(buf,
			PAGE_SIZE,
			   "========RESULT=======\n");


			ret += snprintf(buf + ret, PAGE_SIZE - ret,
			   "LPWG RawData : %s",
			   (rsp_test == 1)
			   ? "Pass\n"
			   : "Fail\n");
			mutex_unlock(&ts->pdata->thread_lock);
			wake_unlock(&ts->touch_rawdata);

			touch_ts_enable_irq(ts->client->irq);

		}
	} else {
		TOUCH_I("Can not execute lpwg_sd, power state = %d\n",
			       power_state);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Can not execute lpwg_sd, power state = %d\n",
				power_state);
	}

	return ret;

}


static LGE_TOUCH_ATTR(firmware, S_IRUGO | S_IWUSR, show_firmware, NULL);
static LGE_TOUCH_ATTR(sd, S_IRUGO | S_IWUSR, show_sd, NULL);
static LGE_TOUCH_ATTR(rawdata, S_IRUGO | S_IWUSR, show_rawdata, NULL);
static LGE_TOUCH_ATTR(delta, S_IRUGO | S_IWUSR, show_delta, NULL);
static LGE_TOUCH_ATTR(chstatus, S_IRUGO | S_IWUSR, show_chstatus, NULL);
static LGE_TOUCH_ATTR(testmode_ver, S_IRUGO | S_IWUSR, show_atcmd_fw_ver, NULL);
static LGE_TOUCH_ATTR(tci, S_IRUGO | S_IWUSR, show_tci, store_tci);
static LGE_TOUCH_ATTR(reg_ctrl, S_IRUGO | S_IWUSR, NULL, store_reg_ctrl);
static LGE_TOUCH_ATTR(object_report, S_IRUGO | S_IWUSR,
		show_object_report, store_object_report);
static LGE_TOUCH_ATTR(version, S_IRUGO | S_IWUSR,
		show_synaptics_fw_version, NULL);
static LGE_TOUCH_ATTR(bootmode, S_IRUGO | S_IWUSR, NULL, store_boot_mode);
static LGE_TOUCH_ATTR(ts_noise, S_IRUGO | S_IWUSR,
		show_ts_noise, store_ts_noise);
static LGE_TOUCH_ATTR(ts_noise_log_enable, S_IRUGO | S_IWUSR,
		show_ts_noise_log_enable, store_ts_noise_log_enable);
static LGE_TOUCH_ATTR(diff_node, S_IRUGO | S_IWUSR,
		show_diff_node, store_diff_node);
static LGE_TOUCH_ATTR(lpwg_test_info, S_IRUGO | S_IWUSR,
		show_lpwg_test_info, NULL);
static LGE_TOUCH_ATTR(touch_wake_up_test, S_IRUGO | S_IWUSR,
		show_touch_wake_up_test, store_touch_wake_up_test);
static LGE_TOUCH_ATTR(pen_support, S_IRUGO | S_IWUSR,
		show_pen_support, NULL);
static LGE_TOUCH_ATTR(palm_ctrl_mode, S_IRUGO | S_IWUSR,
		show_palm_ctrl_mode, store_palm_ctrl_mode);
static LGE_TOUCH_ATTR(use_hover_finger, S_IRUGO | S_IWUSR,
		show_use_hover_finger, store_use_hover_finger);
static LGE_TOUCH_ATTR(use_rmi_dev, S_IRUGO | S_IWUSR,
		show_use_rmi_dev, store_use_rmi_dev);
static LGE_TOUCH_ATTR(sensing_test, S_IRUGO | S_IWUSR,
		NULL, store_sensing_test);
static LGE_TOUCH_ATTR(abs_test, S_IRUGO | S_IWUSR,
		show_abs_test, NULL);
static LGE_TOUCH_ATTR(sensor_speed_test, S_IRUGO | S_IWUSR,
		show_sensor_speed_test, NULL);
static LGE_TOUCH_ATTR(adc_range_test, S_IRUGO | S_IWUSR,
		show_adc_range_test, NULL);
static LGE_TOUCH_ATTR(noise_delta_test, S_IRUGO | S_IWUSR,
		show_noise_delta_test, NULL);
static LGE_TOUCH_ATTR(gnd_test, S_IRUGO | S_IWUSR,
		show_gnd_test, NULL);
static LGE_TOUCH_ATTR(status_normal_calibration, S_IRUGO | S_IWUSR,
		show_status_normal_calibration, NULL);
static LGE_TOUCH_ATTR(normal_calibration, S_IRUGO | S_IWUSR,
		show_normal_calibration, NULL);
static LGE_TOUCH_ATTR(status_lpwg_calibration, S_IRUGO | S_IWUSR,
		show_status_lpwg_calibration, NULL);
static LGE_TOUCH_ATTR(lpwg_calibration, S_IRUGO | S_IWUSR,
		show_lpwg_calibration, NULL);
static LGE_TOUCH_ATTR(get_calibration, S_IRUGO | S_IWUSR,
		show_get_calibration, NULL);
static LGE_TOUCH_ATTR(swipe_param, S_IRUGO | S_IWUSR,
		show_swipe_param, store_swipe_param);
static LGE_TOUCH_ATTR(swipe_mode, S_IRUGO | S_IWUSR,
		NULL, store_swipe_mode);
static LGE_TOUCH_ATTR(hidden_normal_cal_state, S_IRUGO | S_IWUSR,
		show_hidden_normal_cal_state, NULL);
static LGE_TOUCH_ATTR(hidden_lpwg_cal_state, S_IRUGO | S_IWUSR,
		show_hidden_lpwg_cal_state, NULL);
static LGE_TOUCH_ATTR(sp_link_touch_off, S_IRUGO | S_IWUSR,
		show_sp_link_touch_off, store_sp_link_touch_off);
static LGE_TOUCH_ATTR(lpwg_disable, S_IRUGO | S_IWUSR,
		show_lpwg_disable, store_lpwg_disable);
static LGE_TOUCH_ATTR(lpwg_sd, S_IRUGO | S_IWUSR,
		show_lpwg_sd, NULL);

static struct attribute *synaptics_ts_attribute_list[] = {
	&lge_touch_attr_firmware.attr,
	&lge_touch_attr_sd.attr,
	&lge_touch_attr_rawdata.attr,
	&lge_touch_attr_delta.attr,
	&lge_touch_attr_chstatus.attr,
	&lge_touch_attr_testmode_ver.attr,
	&lge_touch_attr_tci.attr,
	&lge_touch_attr_reg_ctrl.attr,
	&lge_touch_attr_object_report.attr,
	&lge_touch_attr_version.attr,
	&lge_touch_attr_bootmode.attr,
	&lge_touch_attr_ts_noise.attr,
	&lge_touch_attr_ts_noise_log_enable.attr,
	&lge_touch_attr_diff_node.attr,
	&lge_touch_attr_lpwg_test_info.attr,
	&lge_touch_attr_touch_wake_up_test.attr,
	&lge_touch_attr_pen_support.attr,
	&lge_touch_attr_palm_ctrl_mode.attr,
	&lge_touch_attr_use_hover_finger.attr,
	&lge_touch_attr_use_rmi_dev.attr,
	&lge_touch_attr_sensing_test.attr,
	&lge_touch_attr_abs_test.attr,
	&lge_touch_attr_sensor_speed_test.attr,
	&lge_touch_attr_adc_range_test.attr,
	&lge_touch_attr_noise_delta_test.attr,
	&lge_touch_attr_gnd_test.attr,
	&lge_touch_attr_status_normal_calibration.attr,
	&lge_touch_attr_normal_calibration.attr,
	&lge_touch_attr_status_lpwg_calibration.attr,
	&lge_touch_attr_lpwg_calibration.attr,
	&lge_touch_attr_get_calibration.attr,
	&lge_touch_attr_swipe_param.attr,
	&lge_touch_attr_swipe_mode.attr,
	&lge_touch_attr_hidden_normal_cal_state.attr,
	&lge_touch_attr_hidden_lpwg_cal_state.attr,
	&lge_touch_attr_sp_link_touch_off.attr,
	&lge_touch_attr_lpwg_disable.attr,
	&lge_touch_attr_lpwg_sd.attr,
	NULL,
};

static const struct attribute_group synaptics_ts_attribute_group = {
	.attrs = synaptics_ts_attribute_list,
};

static int read_page_description_table(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	struct function_descriptor buffer;

	unsigned short u_address = 0;
	unsigned short page_num = 0;

	TOUCH_TRACE();

	memset(&buffer, 0x0, sizeof(buffer));
	memset(&ts->f01, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->f11, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->f12, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->f1a, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->f34, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->f51, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->f54, 0x0, sizeof(struct ts_ic_function));

	for (page_num = 0; page_num < PAGE_MAX_NUM; page_num++) {
		DO_SAFE(synaptics_ts_set_page(client, page_num), error);

		for (u_address = DESCRIPTION_TABLE_START; u_address > 10;
				u_address -= sizeof(buffer)) {

			DO_SAFE(touch_ts_i2c_read(client, u_address,
						sizeof(buffer),
						(unsigned char *)&buffer) < 0,
					error);

			if (buffer.id == 0)
				break;

			switch (buffer.id) {
			case 0x01:
				/* RMI device control */
				ts->f01.dsc = buffer;
				ts->f01.page = page_num;
				break;

			case 0x11:
				/* 2D sensors (finger) */
				ts->f11.dsc = buffer;
				ts->f11.page = page_num;
				break;

			case 0x12:
				/* 2D sensors (finger) */
				ts->f12.dsc = buffer;
				ts->f12.page = page_num;
				get_f12_info(ts);
				break;

			case 0x1a:
				/* capacitive button sensors */
				ts->f1a.dsc = buffer;
				ts->f1a.page = page_num;
				break;

			case 0x34:
				/* Flash memory management */
				ts->f34.dsc = buffer;
				ts->f34.page = page_num;
				break;

			case 0x51:
				/* lpwg */
				ts->f51.dsc = buffer;
				ts->f51.page = page_num;
				break;

			case 0x54:
				/* test report */
				ts->f54.dsc = buffer;
				ts->f54.page = page_num;
				break;

			default:
				break;
			}
		}
	}

	TOUCH_D(DEBUG_BASE_INFO,
			"common[%dP:0x%02x] finger_f12[%dP:0x%02x] flash[%dP:0x%02x] analog[%dP:0x%02x] lpwg[%dP:0x%02x]\n",
			ts->f01.page, ts->f01.dsc.id,
			ts->f12.page, ts->f12.dsc.id,
			ts->f34.page, ts->f34.dsc.id,
			ts->f54.page, ts->f54.dsc.id,
			ts->f51.page, ts->f51.dsc.id);

	/* means fw version before v1.12 */
	if (ts->f1a.dsc.id)
		int_mask_cust = 0x40;
	else
		int_mask_cust = 0x20;

	ERROR_IF(ts->f01.dsc.id == 0 ||
			ts->f12.dsc.id == 0 ||
			ts->f34.dsc.id == 0 ||
			ts->f54.dsc.id == 0 ||
			ts->f51.dsc.id == 0,
			"page_init_error", init_error);

	DO_SAFE(synaptics_ts_set_page(client, 0x00), error);

	get_finger_amplitude(ts);

	return 0;

init_error:
	TOUCH_E("%s, %d : read page failed\n", __func__, __LINE__);
	return -EINVAL;

error:
	TOUCH_E("%s, %d : read page failed\n", __func__, __LINE__);
	return -EIO;
}

static int get_swipe_info(struct synaptics_ts_data *ts)
{
	struct swipe_data *swp = &ts->swipe;
	bool is_official_fw = 0;
	u8 fw_ver = 0;
	u8 lpwg_properties_reg = ts->f51.dsc.query_base + 4;
	u8 has_swipe_mask = 0x10;
	u8 buf = 0;

	memset(swp, 0, sizeof(struct swipe_data));

	if (is_product(ts, "PLG446", 6)
		|| is_product(ts, "PLG468", 6)) {
		is_official_fw = ((ts->fw_info.version[3] & 0x80) >> 7);
		fw_ver = (ts->fw_info.version[3] & 0x7F);

		TOUCH_I("%s: is_official_fw=%d, fw_ver=%d\n",
				__func__, is_official_fw, fw_ver);

		swp->support_swipe = NO_SUPPORT_SWIPE;

		if (is_official_fw) {
			if (is_product(ts, "PLG446", 6)) {
				if (fw_ver >= 9)
					swp->support_swipe |= SUPPORT_SWIPE_DOWN;
				if (fw_ver >= 14)
					swp->support_swipe |= SUPPORT_SWIPE_UP;
			} else if (is_product(ts, "PLG468", 6)) {
				if (fw_ver >= 11)
					swp->support_swipe |= SUPPORT_SWIPE_DOWN;
				if (fw_ver >= 14)
					swp->support_swipe |= SUPPORT_SWIPE_UP;
			}
		} else {
			if (is_product(ts, "PLG446", 6)) {
				if (fw_ver >= 41)
					swp->support_swipe |= SUPPORT_SWIPE_DOWN;
				if (fw_ver >= 75)
					swp->support_swipe |= SUPPORT_SWIPE_UP;
			} else if (is_product(ts, "PLG468", 6)) {
				if (fw_ver >= 40)
					swp->support_swipe |= SUPPORT_SWIPE_DOWN;
				if (fw_ver >= 90)
					swp->support_swipe |= SUPPORT_SWIPE_UP;
			}
		}

		if (swp->support_swipe) {
			synaptics_ts_page_data_read(ts->client, LPWG_PAGE,
					lpwg_properties_reg, 1, &buf);

			TOUCH_I(
					"%s: lpwg_properties_reg [addr:0x%02X,value:0x%02X)\n",
					__func__, lpwg_properties_reg, buf);

			if (!(buf & has_swipe_mask)) {
				TOUCH_I("%s: Need to check Has Swipe bit\n",
						__func__);
				swp->support_swipe = NO_SUPPORT_SWIPE;
			}
		}
	} else {
		TOUCH_E("%s, %d : Unknown firmware\n", __func__, __LINE__);
		return 0;
	}

	TOUCH_I("%s: support_swipe:0x%02X\n", __func__, swp->support_swipe);

	if (swp->support_swipe == NO_SUPPORT_SWIPE)
		return 0;

	swp->gesture_mask = 0x04;
	swp->enable_reg = ts->f51.dsc.control_base + 15;

	if (is_product(ts, "PLG446", 6)) {
		if (ts->lpwg_ctrl.has_lpwg_overtap_module) {
			swp->coordinate_start_reg = ts->f51.dsc.data_base + 74;
			swp->coordinate_end_reg = ts->f51.dsc.data_base + 78;
			swp->fail_reason_reg = ts->f51.dsc.data_base + 82;
			swp->time_reg = ts->f51.dsc.data_base + 83;
		} else {
			swp->coordinate_start_reg = ts->f51.dsc.data_base + 73;
			swp->coordinate_end_reg = ts->f51.dsc.data_base + 77;
			swp->fail_reason_reg = ts->f51.dsc.data_base + 81;
			swp->time_reg = ts->f51.dsc.data_base + 82;
		}
	} else if (is_product(ts, "PLG468", 6)) {
		if (ts->lpwg_ctrl.has_lpwg_overtap_module
			&& ts->lpwg_ctrl.has_request_reset_reg) {
			swp->coordinate_start_reg = ts->f51.dsc.data_base + 58;
			swp->coordinate_end_reg = ts->f51.dsc.data_base + 62;
			swp->fail_reason_reg = ts->f51.dsc.data_base + 66;
			swp->time_reg = ts->f51.dsc.data_base + 67;
		} else {
			swp->coordinate_start_reg = ts->f51.dsc.data_base + 57;
			swp->coordinate_end_reg = ts->f51.dsc.data_base + 61;
			swp->fail_reason_reg = ts->f51.dsc.data_base + 65;
			swp->time_reg = ts->f51.dsc.data_base + 66;
		}
	} else {
		TOUCH_E("%s, %d : Unknown firmware\n", __func__, __LINE__);
		memset(swp, 0, sizeof(struct swipe_data));
		return 0;
	}

	if (swp->support_swipe & SUPPORT_SWIPE_DOWN) {
		swp->swipe_mode |= SWIPE_DOWN_BIT;
		swp->down.enable_mask = SWIPE_DOWN_BIT;
		swp->down.min_distance =  ts->pdata->swp_down_caps->min_distance;
		swp->down.ratio_thres = ts->pdata->swp_down_caps->ratio_thres;
		swp->down.ratio_chk_period = ts->pdata->swp_down_caps->ratio_chk_period;
		swp->down.ratio_chk_min_distance = ts->pdata->swp_down_caps->ratio_chk_min_distance;
		swp->down.min_time_thres = ts->pdata->swp_down_caps->min_time_thres;
		swp->down.max_time_thres = ts->pdata->swp_down_caps->max_time_thres;
		swp->down.active_area_x0 = ts->pdata->swp_down_caps->active_area_x0;
		swp->down.active_area_y0 = ts->pdata->swp_down_caps->active_area_y0;
		swp->down.active_area_x1 = ts->pdata->swp_down_caps->active_area_x1;
		swp->down.active_area_y1 = ts->pdata->swp_down_caps->active_area_y1;
		swp->down.min_distance_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->min_distance_reg_offset;
		swp->down.ratio_thres_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->ratio_thres_reg_offset;
		swp->down.ratio_chk_period_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->ratio_chk_period_reg_offset;
		swp->down.ratio_chk_min_distance_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->ratio_chk_min_distance_reg_offset;
		swp->down.min_time_thres_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->min_time_thres_reg_offset;
		swp->down.max_time_thres_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->max_time_thres_reg_offset;
		swp->down.active_area_x0_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->active_area_x0_reg_offset;
		swp->down.active_area_y0_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->active_area_y0_reg_offset;
		swp->down.active_area_x1_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->active_area_x1_reg_offset;
		swp->down.active_area_y1_reg = ts->f51.dsc.control_base + ts->pdata->swp_down_caps->active_area_y1_reg_offset;
	}

	if (swp->support_swipe & SUPPORT_SWIPE_UP) {
		/* swp->swipe_mode |= SWIPE_UP_BIT; */
		swp->up.enable_mask = SWIPE_UP_BIT;
		swp->up.min_distance = ts->pdata->swp_up_caps->min_distance;
		swp->up.ratio_thres = ts->pdata->swp_up_caps->ratio_thres;
		swp->up.ratio_chk_period = ts->pdata->swp_up_caps->ratio_chk_period;
		swp->up.ratio_chk_min_distance = ts->pdata->swp_up_caps->ratio_chk_min_distance;
		swp->up.min_time_thres = ts->pdata->swp_up_caps->min_time_thres;
		swp->up.max_time_thres = ts->pdata->swp_up_caps->max_time_thres;
		swp->up.active_area_x0 = ts->pdata->swp_up_caps->active_area_x0;
		swp->up.active_area_y0 = ts->pdata->swp_up_caps->active_area_y0;
		swp->up.active_area_x1 = ts->pdata->swp_up_caps->active_area_x1;
		swp->up.active_area_y1 = ts->pdata->swp_up_caps->active_area_y1;
		swp->up.min_distance_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->min_distance_reg_offset;
		swp->up.ratio_thres_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->ratio_thres_reg_offset;
		swp->up.ratio_chk_period_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->ratio_chk_period_reg_offset;
		swp->up.ratio_chk_min_distance_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->ratio_chk_min_distance_reg_offset;
		swp->up.min_time_thres_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->min_time_thres_reg_offset;
		swp->up.max_time_thres_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->max_time_thres_reg_offset;
		swp->up.active_area_x0_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->active_area_x0_reg_offset;
		swp->up.active_area_y0_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->active_area_y0_reg_offset;
		swp->up.active_area_x1_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->active_area_x1_reg_offset;
		swp->up.active_area_y1_reg = ts->f51.dsc.control_base + ts->pdata->swp_up_caps->active_area_y1_reg_offset;
	}

	return 0;
}

static void get_lpwg_module_enable(struct synaptics_ts_data *ts)
{
	bool is_official_fw = 0;
	u8 fw_ver = 0;

	is_official_fw = ((ts->fw_info.version[3] & 0x80) >> 7);
	fw_ver = (ts->fw_info.version[3] & 0x7F);

	if (is_product(ts, "PLG468", 6)) {
		if (is_official_fw && fw_ver >= 20) {
			ts->lpwg_ctrl.has_lpwg_overtap_module = 1;
			ts->lpwg_ctrl.has_request_reset_reg = 1;
		} else if (fw_ver >= 123) {
			ts->lpwg_ctrl.has_lpwg_overtap_module = 1;
			ts->lpwg_ctrl.has_request_reset_reg = 1;
		} else {
			ts->lpwg_ctrl.has_lpwg_overtap_module = 0;
			ts->lpwg_ctrl.has_request_reset_reg = 0;
		}
	} else if (is_product(ts, "PLG446", 6)) {
		if (is_official_fw && fw_ver >= 24)
			ts->lpwg_ctrl.has_lpwg_overtap_module = 1;
		else if (fw_ver >= 121)
			ts->lpwg_ctrl.has_lpwg_overtap_module = 1;
		else
			ts->lpwg_ctrl.has_lpwg_overtap_module = 0;
	} else {
		TOUCH_E("%s, %d : can't find matched product id \n", __func__, __LINE__);
	}
}

static int get_ic_info(struct synaptics_ts_data *ts)
{
	u8 buf = 0;
	memset(&ts->fw_info, 0, sizeof(struct synaptics_ts_fw_info));

	ts->pdata->role->fw_index = get_type_bootloader(ts->client);
	DO_SAFE(touch_ts_i2c_read(ts->client, FLASH_CONFIG_ID_REG,
				sizeof(ts->fw_info.version) - 1,
				ts->fw_info.version), error);
	DO_SAFE(touch_ts_i2c_read(ts->client, CUSTOMER_FAMILY_REG, 1,
				&(ts->fw_info.family)), error);
	DO_SAFE(touch_ts_i2c_read(ts->client, FW_REVISION_REG, 1,
				&(ts->fw_info.revision)), error);
	TOUCH_D(DEBUG_BASE_INFO, "CUSTOMER_FAMILY_REG = %d\n",
			ts->fw_info.family);
	TOUCH_D(DEBUG_BASE_INFO, "FW_REVISION_REG = %d\n",
			ts->fw_info.revision);

	DO_SAFE(synaptics_ts_page_data_read(ts->client,
		LPWG_PAGE, LPWG_HAS_DEBUG_MODULE, 1, &buf), error);
	ts->lpwg_ctrl.has_debug_module = (buf & 0x0C) ? 1 : 0;
	TOUCH_D(DEBUG_BASE_INFO, "addr[0x%x] buf[0x%x] has_d_module[%d]",
		LPWG_HAS_DEBUG_MODULE, buf, ts->lpwg_ctrl.has_debug_module);
	get_lpwg_module_enable(ts);
	get_swipe_info(ts);

	return 0;
error:
	TOUCH_E("%s, %d : get_ic_info failed\n", __func__, __LINE__);

	return -EIO;
}

static int check_firmware_status(struct synaptics_ts_data *ts)
{
	u8 device_status = 0;
	u8 flash_status = 0;

	DO_SAFE(touch_ts_i2c_read(ts->client, FLASH_STATUS_REG,
				sizeof(flash_status), &flash_status), error);
	DO_SAFE(touch_ts_i2c_read(ts->client, DEVICE_STATUS_REG,
				sizeof(device_status), &device_status), error);

	ts->fw_info.need_rewrite_firmware = 0;

	if ((device_status & DEVICE_STATUS_FLASH_PROG)
			|| (device_status & DEVICE_CRC_ERROR_MASK)
			|| (flash_status & FLASH_STATUS_MASK)) {
		TOUCH_E("FLASH_STATUS[0x%x] DEVICE_STATUS[0x%x]\n",
				(u32)flash_status, (u32)device_status);
		ts->fw_info.need_rewrite_firmware = 1;
	}
	return 0;
error:
	TOUCH_E("%s, %d : check_firmware_status failed\n",
			__func__, __LINE__);
	return -EIO;
}

enum error_type synaptics_ts_probe(struct i2c_client *client,
		struct touch_platform_data *lge_ts_data,
		struct state_info *state)
{
	struct synaptics_ts_data *ts;

	TOUCH_TRACE();

	ASSIGN(ts = devm_kzalloc(&client->dev, sizeof(struct synaptics_ts_data),
				GFP_KERNEL), error);
	set_touch_handle(client, ts);

	ts->client = client;
	ds4_i2c_client = client;
	ts->pdata = lge_ts_data;
	ts->state = state;
	/* Protocol 9 disable for sleep control */
	ts->lpwg_ctrl.protocol9_sleep_flag = false;

	if (ts->pdata->pwr->vio_control) {
		TOUCH_I(
				"%s: ts->pdata->vio_pin[%d]\n",
				__func__, ts->pdata->vio_pin);
		if (ts->pdata->vio_pin > 0) {
			DO_SAFE(gpio_request(ts->pdata->vio_pin,
						"touch_vio"), error);
			gpio_direction_output(ts->pdata->vio_pin, 0);
		}
	}

	if (ts->pdata->pwr->use_regulator) {
		DO_IF(IS_ERR(ts->vreg_vdd = regulator_get(&client->dev,
						ts->pdata->pwr->vdd)), error);
		DO_IF(IS_ERR(ts->vreg_vio = regulator_get(&client->dev,
						ts->pdata->pwr->vio)), error);
		if (ts->pdata->pwr->vdd_voltage > 0)
			DO_SAFE(regulator_set_voltage(ts->vreg_vdd,
						ts->pdata->pwr->vdd_voltage,
						ts->pdata->pwr->vdd_voltage),
					error);
		if (ts->pdata->pwr->vio_voltage > 0)
			DO_SAFE(regulator_set_voltage(ts->vreg_vio,
						ts->pdata->pwr->vio_voltage,
						ts->pdata->pwr->vio_voltage),
					error);
	}

	ts->is_probed = 0;
	ts->is_init = 0;
	ts->lpwg_ctrl.screen = 1;
	ts->lpwg_ctrl.sensor = 1;

	atomic_set(&ts->lpwg_ctrl.is_suspend, 0);
	INIT_DELAYED_WORK(&ts->work_timer, lpwg_timer_func);
	INIT_DELAYED_WORK(&ts->work_palm, all_palm_released_func);
	INIT_DELAYED_WORK(&ts->work_sleep, sleepmode_func);
	wake_lock_init(&ts->timer_wake_lock, WAKE_LOCK_SUSPEND, "touch_timer");
	wake_lock_init(&ts->touch_rawdata, WAKE_LOCK_SUSPEND, "touch_rawdata");
	return NO_ERROR;
error:
	TOUCH_E("%s, %d : synaptics_probe failed\n", __func__, __LINE__);
	return ERROR;
}

enum error_type synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts
		= (struct synaptics_ts_data *)get_touch_handle(client);

	TOUCH_TRACE();

	if (ts->pdata->role->use_hover_finger
			&& prox_fhandler.inserted
			&& prox_fhandler.initialized) {
		prox_fhandler.exp_fn->remove(ts);
		prox_fhandler.initialized = false;
	}

	if (ts->pdata->role->use_rmi_dev
			&& rmidev_fhandler.inserted
			&& rmidev_fhandler.initialized) {
		rmidev_fhandler.exp_fn->remove(ts);
		rmidev_fhandler.initialized = false;
	}

	if (ts->pdata->pwr->use_regulator) {
		regulator_put(ts->vreg_vio);
		regulator_put(ts->vreg_vdd);
	}

	wake_lock_destroy(&ts->timer_wake_lock);
	wake_lock_destroy(&ts->touch_rawdata);
	return NO_ERROR;
}

static int lpwg_update_all(struct synaptics_ts_data *ts, bool irqctrl)
{
	int sleep_status = 0;
	int lpwg_status = 0;
	bool req_lpwg_param = false;

	TOUCH_TRACE();

	if (ts->lpwg_ctrl.screen) {
		if (atomic_read(&ts->lpwg_ctrl.is_suspend) == 1) {
			if (power_state == POWER_OFF
					|| power_state == POWER_SLEEP)
				ts->is_init = 0;
			if (ts->lpwg_ctrl.has_debug_module) {
				DO_SAFE(print_tci_debug_result(ts, 0), error);
				DO_SAFE(print_tci_debug_result(ts, 1), error);
			}
		}
		atomic_set(&ts->lpwg_ctrl.is_suspend, 0);
		if (ts->pdata->panel_id)
			ghost_do_not_reset = false;
	} else {
		if (atomic_read(&ts->lpwg_ctrl.is_suspend) == 0) {
			atomic_set(&ts->lpwg_ctrl.is_suspend, 1);
			if (is_product(ts, "PLG349", 6))
				set_doze_param(ts, 3);
		}
		if (ts->pdata->swipe_pwr_ctr != SKIP_PWR_CON) {
			ts->pdata->swipe_pwr_ctr = WAIT_SWIPE_WAKEUP;
		}
		TOUCH_I("%s : swipe_pwr_ctr = %d\n", __func__,
				ts->pdata->swipe_pwr_ctr);
		if (ts->pdata->panel_id)
			ghost_do_not_reset = true;
	}

	if (ts->lpwg_ctrl.screen) { /* ON(1) */
		sleep_status = 1;
		lpwg_status = 0;
	} else if (!ts->lpwg_ctrl.screen /* OFF(0), CLOSED(0) */
			&& ts->lpwg_ctrl.qcover) {
		sleep_status = 1;
		lpwg_status = 1;
	} else if (!ts->lpwg_ctrl.screen /* OFF(0), OPEN(1), FAR(1) */
			&& !ts->lpwg_ctrl.qcover
			&& ts->lpwg_ctrl.sensor) {
		sleep_status = 1;
		lpwg_status = ts->lpwg_ctrl.lpwg_mode;
	} else if (!ts->lpwg_ctrl.screen /* OFF(0), OPEN(1), NEAR(0) */
			&& !ts->lpwg_ctrl.qcover
			&& !ts->lpwg_ctrl.sensor) {
		if (ts->pdata->role->crack->use_crack_mode) {
			if (!after_crack_check) {
				TOUCH_I(
						"%s : Crack check not done... use nonsleep mode to check Crack!!\n",
						__func__);
				sleep_status = 1;
				lpwg_status = ts->lpwg_ctrl.lpwg_mode;
			} else {
				sleep_status = 0;
				req_lpwg_param = true;
			}
		} else {
			sleep_status = 0;
			req_lpwg_param = true;
		}
	}

	if (is_product(ts, "PLG349", 6)) {
		DO_SAFE(sleep_control(ts, sleep_status, 0), error);
	} else {
		TOUCH_D(DEBUG_BASE_INFO,
				"Sensor Status: %d\n", ts->lpwg_ctrl.sensor);
		TOUCH_D(DEBUG_BASE_INFO,
				"lpwg_is_enabled: %d\n", ts->lpwg_ctrl.lpwg_is_enabled);
		ts->pdata->swipe_stat[0] = ts->lpwg_ctrl.sensor;
		if (!ts->lpwg_ctrl.lpwg_mode &&
				!ts->pdata->swipe_stat[1] && !mfts_mode) {
			touch_sleep_status(ts->client, 1);
			TOUCH_D(DEBUG_BASE_INFO,
			"[%s] LPWG Disable !\n", __func__);
		} else if (ts->lpwg_ctrl.qcover) {
			touch_sleep_status(ts->client, 0);
		} else if (ts->pdata->swipe_stat[1] == SWIPE_DONE) {
			touch_sleep_status(ts->client, !ts->lpwg_ctrl.sensor);
		} else if (ts->lpwg_ctrl.lpwg_is_enabled) {
			touch_swipe_status(ts->client,
					ts->pdata->swipe_stat[1]);
		}
	}

	if (req_lpwg_param == false)
		DO_SAFE(lpwg_control(ts, lpwg_status), error);

	ts->lpwg_ctrl.prev_screen = ts->lpwg_ctrl.screen;

	return NO_ERROR;
error:
	return ERROR;
}

/* temporary code for INCELL JDI (relaxation) */
static int set_rebase_param(struct synaptics_ts_data *ts, int value)
{
	u8 buf_array[9] = {0};

	DO_SAFE(synaptics_ts_set_page(ts->client, ANALOG_PAGE), error);

	touch_ts_i2c_read(ts->client, 0x45, 5, buf_array);
	if (value)
		buf_array[3] = 0x19;  /* hold fast transition */
	else
		buf_array[3] = 0x32;  /* hold fast transition */
	touch_ts_i2c_write(ts->client, 0x45, 5, buf_array);

	touch_ts_i2c_read(ts->client, 0x4C, 9, buf_array);
	if (value) {
		buf_array[6] = 0x1E;  /* Difference Threshold  */
		buf_array[8] = 0x46;  /* Negative Energy Threshold */
	} else {
		buf_array[6] = 0x32;  /* Difference Threshold  */
		buf_array[8] = 0x96;  /* Negative Energy Threshold */
	}
	touch_ts_i2c_write(ts->client, 0x4C, 9, buf_array);

	if (value)
		TOUCH_D(DEBUG_BASE_INFO, "%s : Set for Normal\n", __func__);
	else
		TOUCH_D(DEBUG_BASE_INFO, "%s : Set for LPWG\n", __func__);

	DO_SAFE(synaptics_ts_set_page(ts->client, DEFAULT_PAGE), error);

	return 0;
error:
	TOUCH_E("%s : failed to set rebase param\n", __func__);
	return -EPERM;
}

enum error_type synaptics_ts_init(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buf = 0;
	u8 buf_array[2] = {0};
	int exp_fn_retval;
	u8 motion_suppression_reg_addr;
	int rc = 0;
	u8 lpwg_mode = ts->lpwg_ctrl.lpwg_mode;
	int is_suspend = atomic_read(&ts->lpwg_ctrl.is_suspend);

	TOUCH_TRACE();

	if (ts->is_probed == 0) {
		rc = read_page_description_table(ts->client);
		DO_SAFE(check_firmware_status(ts), error);
		if (rc == -EIO)
			return ERROR;
		get_ic_info(ts);
		if (rc == -EINVAL) {
			TOUCH_I("%s : need to rewrite firmware !!",
				__func__);
			ts->fw_info.need_rewrite_firmware = 1;
		}
		ts->is_probed = 1;
	}

	if (ts->pdata->role->use_hover_finger && prox_fhandler.inserted) {
		if (!prox_fhandler.initialized) {
			exp_fn_retval = prox_fhandler.exp_fn->init(ts);

			if (exp_fn_retval < 0) {
				TOUCH_I(
						"[Touch Proximity] %s: Failed to init proximity settings\n",
						__func__);
			} else
				prox_fhandler.initialized = true;
		} else {
			prox_fhandler.exp_fn->reinit(ts);
		}
	}

	if (ts->pdata->role->use_rmi_dev && rmidev_fhandler.inserted) {
		if (!rmidev_fhandler.initialized) {
			exp_fn_retval = rmidev_fhandler.exp_fn->init(ts);

			if (exp_fn_retval < 0) {
				TOUCH_I(
						"[Touch RMI_Dev] %s: Failed to init rmi_dev settings\n",
						__func__);
			} else {
				rmidev_fhandler.initialized = true;
			}
		}
	}

	if (is_product(ts, "PLG468", 6)) {
		TOUCH_I("[%s] DEVICE_CONTROL_NORMAL_OP\n", __func__);
		DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
					DEVICE_CONTROL_NORMAL_OP
					| DEVICE_CONTROL_CONFIGURED), error);

		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, &buf), error);
		buf_array[0] = touch_ta_status ? 0x02 : 0x00;
		buf_array[1] = incoming_call_state ? 0x00 : 0x04;
		TOUCH_I("%s: prev:0x%02X, next:0x%02X (TA: %d / Call: %d)\n",
				__func__,
				buf,
				(buf & 0xF9) | (buf_array[0] | buf_array[1]),
				touch_ta_status, incoming_call_state);
		buf = (buf & 0xF9) | (buf_array[0] | buf_array[1]);
		DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, &buf), error);
	} else if (is_product(ts, "PLG446", 6)) {
		if (touch_ta_status == 2 || touch_ta_status == 3) {
			TOUCH_I("[%s] DEVICE_CONTROL_NOSLEEP\n", __func__);
			DO_SAFE(touch_ts_i2c_read(client, DEVICE_CONTROL_REG, 1,
						&buf), error);
			DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
						DEVICE_CONTROL_NOSLEEP
						| (buf & 0xF8)),
					error);

			buf = 0x01;
			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
						LPWG_PARTIAL_REG + 4, 1, &buf), error);
		} else {
			TOUCH_I("[%s] DEVICE_CONTROL_NORMAL_OP\n", __func__);
			DO_SAFE(touch_ts_i2c_read(client, DEVICE_CONTROL_REG, 1,
						&buf), error);
			DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
						DEVICE_CONTROL_NORMAL_OP
						| (buf & 0xF8)),
					error);

			buf = 0x00;
			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
						LPWG_PARTIAL_REG + 4, 1, &buf), error);
		}
		set_rebase_param(ts, 1);
	}

	DO_SAFE(touch_ts_i2c_read(client, INTERRUPT_ENABLE_REG, 1, &buf), error);
	if (is_product(ts, "PLG349", 6) ||
		is_product(ts, "s3320", 5) ||
		is_product(ts, "PLG446", 6) ||
		is_product(ts, "PLG468", 6)) {
		DO_SAFE(touch_i2c_write_byte(client, INTERRUPT_ENABLE_REG,
					buf | INTERRUPT_MASK_ABS0
					| int_mask_cust), error);

	} else {
		DO_SAFE(touch_i2c_write_byte(client, INTERRUPT_ENABLE_REG,
					buf | INTERRUPT_MASK_ABS0), error);
	}

	if (ts->pdata->role->report_mode == REDUCED_MODE
			&& !ts->pdata->role->ghost->long_press_chk) {
		buf_array[0] = buf_array[1] =
			ts->pdata->role->delta_pos_threshold;
	} else {
		buf_array[0] = buf_array[1] = 0;
		ts->pdata->role->ghost->force_continuous_mode = true;
	}

	motion_suppression_reg_addr = ts->f12_reg.ctrl[20];
	DO_SAFE(touch_ts_i2c_write(client, motion_suppression_reg_addr, 2,
				buf_array), error);

	if (ts->pdata->role->palm_ctrl_mode > PALM_REPORT) {
		TOUCH_I(
				"Invalid palm_ctrl_mode:%u (palm_ctrl_mode -> PALM_REJECT_FW)\n",
				ts->pdata->role->palm_ctrl_mode);
		ts->pdata->role->palm_ctrl_mode = PALM_REJECT_FW;
	}
	TOUCH_I("palm_ctrl_mode:%u\n", ts->pdata->role->palm_ctrl_mode);

	DO_SAFE(touch_ts_i2c_read(client, ts->f12_reg.ctrl[22],
				1, &buf), error);
	buf_array[0] = buf & 0x03;

	if ((ts->pdata->role->palm_ctrl_mode == PALM_REJECT_DRIVER)
			|| (ts->pdata->role->palm_ctrl_mode == PALM_REPORT)) {
		if (buf_array[0] != 0x00) {
			/* PalmFilterMode bits[1:0] (00:Disable palm filter */
			buf &= ~(0x03);
			DO_SAFE(touch_i2c_write_byte(client,
						ts->f12_reg.ctrl[22],
						buf), error);
		}
		memset(&ts->palm, 0, sizeof(struct palm_data));
	} else {
		if (buf_array[0] != 0x01) {
			/* PalmFilterMode bits[1:0] (01:Enable palm filter) */
			buf &= ~(0x02);
			buf |= 0x01;
			DO_SAFE(touch_i2c_write_byte(client,
						ts->f12_reg.ctrl[22],
						buf), error);
		}
	}

	if (ts->pdata->role->use_lpwg_all)
		DO_SAFE(lpwg_update_all(ts, 0), error);
	else
		DO_SAFE(lpwg_control(ts, is_suspend ? lpwg_mode : 0),
				error);

	/* To get register addr properly for each Environment*/
	matchUp_f51_regMap(ts);
	matchUp_f54_regMap(ts);

	/* It always should be done last. */
	DO_SAFE(touch_ts_i2c_read(client, INTERRUPT_STATUS_REG, 1, &buf), error);
	ts->is_init = 1;
	lpwg_by_lcd_notifier = false;

	return NO_ERROR;
error:
	TOUCH_E("%s, %d : synaptics init failed\n", __func__, __LINE__);
	return ERROR;
}

static int synaptics_ts_im_test(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	unsigned int retry_cnt = 0, im_fail_max = 150;
	u8 buf1 = 0, buf2 = 0, curr[2] = {0};
	u16 im = 0, im_test_max = 0, result = 0;

	int f54len = 0;
	char f54buf[1000] = {0};
	int im_result = 0;

	f54len = snprintf(f54buf + f54len,
			sizeof(f54buf) - f54len,
			"RSP IM Test Result\n");

	DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
				LPWG_PARTIAL_REG + 71,
				1, curr), error);
	curr[0] = (curr[0] & 0xff) | 0x02;
	DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
				LPWG_PARTIAL_REG + 71,
				1, curr), error);
	msleep(20);
	for (retry_cnt = 0; retry_cnt < 10; retry_cnt++) {
		DO_SAFE(synaptics_ts_set_page(client, ANALOG_PAGE), error);

		DO_SAFE(touch_ts_i2c_read(client,
					ts->f54_reg.interference__metric_LSB, 1,
					&buf1), error);
		DO_SAFE(touch_ts_i2c_read(client,
					ts->f54_reg.interference__metric_MSB, 1,
					&buf2), error);
		im = (buf2 << 8) | buf1;

		f54len += snprintf(f54buf + f54len,
			       sizeof(f54buf) - f54len,
			      "%d : Current IM value = %d\n", retry_cnt, im);
		if (im > im_test_max)
			im_test_max = im;
		TOUCH_I("%s : im_test_max : %u retry_cnt : %u\n",
			__func__, im_test_max, retry_cnt);
		mdelay(5);
	}
	result = im_test_max;
	TOUCH_I("%s : result : %u\n", __func__, result);
	curr[0] = (curr[0] & 0xff) & 0xfd;
	DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
				LPWG_PARTIAL_REG + 71,
				1, curr), error);

	f54len += snprintf(f54buf + f54len, sizeof(f54buf) - f54len,
			"\nMAX IM value=%d\n", result);

	if (result < im_fail_max) {
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"RSP IM TEST passed\n\n");
		im_result = 1;
	} else {
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"RSP IM TEST failed\n\n");
		im_result = 0;
	}

	write_log(NULL, f54buf);
	msleep(30);

	return im_result;

error:
	TOUCH_E("%s, %d : IM TEST failed\n", __func__, __LINE__);
	f54len += snprintf(f54buf + f54len,
			sizeof(f54buf) - f54len,
			"%s, %d : IM TEST failed\n", __func__, __LINE__);
	write_log(NULL, f54buf);
	msleep(30);
	return -EPERM;
}

static int synaptics_ts_adc_test(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	unsigned int i = 0, temp_cnt = 0;
	u8 buf[42] = {0};
	u16 result = 0, adc_result = 0, adc_fail_max = 3800, adc_fail_min = 400;
	u16 adc[20] = {0};

	int f54len = 0;
	char f54buf[1000] = {0};

	f54len = snprintf(f54buf + f54len,
			sizeof(f54buf) - f54len,
			"ADC Test Result\n");
	write_log(NULL, f54buf);
	f54len = 0;

	TOUCH_D(DEBUG_BASE_INFO, "JDI ADC Test start\n");

	DO_SAFE(synaptics_ts_set_page(client, ANALOG_PAGE), error);

	DO_SAFE(touch_ts_i2c_read(client,
				ts->f54_reg.incell_statistic, 50,
				buf), error);

	DO_SAFE(synaptics_ts_set_page(client, DEFAULT_PAGE), error);

	for(i = 0 ; i < 21 ; i++){
		if(i < 4)
			continue;

		temp_cnt  = i * 2;
		adc[i] = (buf[temp_cnt + 1] << 8) | buf[temp_cnt];
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"%d : Adc mesured value = %d\n",
				i, adc[i]);

		TOUCH_D(DEBUG_BASE_INFO, "Adc value adc[%d] = %d\n", i+1, adc[i]);

		if (adc[i] > adc_fail_max || adc[i] < adc_fail_min)
			adc_result++;

		write_log(NULL, f54buf);
		f54len = 0;
	}

	if (adc_result) {
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"ADC TEST Failed\n");
		TOUCH_D(DEBUG_BASE_INFO, "JDI ADC Test has failed!!\n");
		result = 0;
	} else {
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"ADC TEST Passed\n\n");
		TOUCH_D(DEBUG_BASE_INFO, "JDI ADC Test has passed\n");
		result = 1;
	}

	write_log(NULL, f54buf);
	msleep(30);

	TOUCH_D(DEBUG_BASE_INFO, "JDI ADC Test end\n");

	return result;
error:
	TOUCH_E("%s, %d : ADC TEST failed\n", __func__, __LINE__);
	f54len += snprintf(f54buf + f54len,
			sizeof(f54buf) - f54len,
			"%s, %d : ADC TEST failed\n", __func__, __LINE__);
	write_log(NULL, f54buf);
	msleep(30);
	return -EPERM;
}

static int synaptics_ts_lpwg_adc_test(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	unsigned int i = 0;
	u8 buf[5] = {0};
	u16 result = 0, adc_result = 0;
	unsigned int adc_fail_max = 54299, adc_fail_min = 20998;
	u32 adc[20] = {0};

	int f54len = 0;
	char f54buf[1000] = {0};

	f54len = snprintf(f54buf + f54len,
			sizeof(f54buf) - f54len,
			"LPWG ADC Test Result\n");
	write_log(NULL, f54buf);
	f54len = 0;

	TOUCH_D(DEBUG_BASE_INFO, "LPWG ADC Test start\n");

	DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					ts->f51_reg.lpwg_adc_offset_reg,
					1, buf), error);

	msleep(20);

	for (i = 0 ; i < 17 ; i++) {
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_adc_offset_reg,
					1, &buf[0]), error);

		TOUCH_D(DEBUG_BASE_INFO, "LPWG ADC Test offset_read : %d\n",
			       buf[0]);

		if (buf[0] != i) {
			TOUCH_D(DEBUG_BASE_INFO,
				      "LPWG ADC Test offset update error\n");
			goto error;
		}

		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_adc_fF_reg1,
					1, &buf[1]), error);
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_adc_fF_reg2,
					1, &buf[2]), error);
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_adc_fF_reg3,
					1, &buf[3]), error);
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					ts->f51_reg.lpwg_adc_fF_reg4,
					1, &buf[4]), error);
		adc[i] =
		(buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];
		TOUCH_D(DEBUG_BASE_INFO,
				"LPWG ADC Test value : %d\t"
				"(buf[4] 0x%02x, buf[3] 0x%02x\t"
				"buf[2] 0x%02x, buf[1] 0x%02x)\n",
				adc[i], buf[4], buf[3], buf[2], buf[1]);
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"%d : LPWG Adc mesured value = %d\n",
				i, adc[i]);

		TOUCH_D(DEBUG_BASE_INFO, "LPWG Adc value adc[%d] = %d\n",
				i + 1, adc[i]);

		if (adc[i] > adc_fail_max || adc[i] < adc_fail_min)
			adc_result++;

		write_log(NULL, f54buf);
		f54len = 0;
	}

	if (adc_result) {
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"LPWG ADC TEST Failed\n");
		TOUCH_D(DEBUG_BASE_INFO, "LPWG ADC Test has failed!!\n");
		result = 0;
	} else {
		f54len += snprintf(f54buf + f54len,
				sizeof(f54buf) - f54len,
				"LPWG ADC TEST Passed\n\n");
		TOUCH_D(DEBUG_BASE_INFO, "LPWG ADC Test has passed\n");
		result = 1;
	}

	write_log(NULL, f54buf);
	msleep(30);

	TOUCH_D(DEBUG_BASE_INFO, "LPWG ADC Test end\n");

	return result;
error:
	TOUCH_E("%s, %d : LPWG ADC TEST failed\n", __func__, __LINE__);
	f54len += snprintf(f54buf + f54len,
			sizeof(f54buf) - f54len,
			"%s, %d : LPWG ADC TEST failed\n", __func__, __LINE__);
	write_log(NULL, f54buf);
	msleep(30);
	return -EPERM;
}

static int synaptics_ts_noise_log(struct i2c_client *client,
		struct touch_data *curr_data,
		const struct touch_data *prev_data)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buffer[3] = {0};
	u8 buf1 = 0, buf2 = 0, cns = 0;
	u16 im = 0, cid_im = 0, freq_scan_im = 0;
	int i = 0;

	DO_SAFE(synaptics_ts_set_page(client, ANALOG_PAGE), error);

	DO_SAFE(touch_ts_i2c_read(client, ts->f54_reg.interference__metric_LSB, 1,
				&buf1), error);
	DO_SAFE(touch_ts_i2c_read(client, ts->f54_reg.interference__metric_MSB, 1,
				&buf2), error);
	im = (buf2 << 8) | buf1;
	im_sum += im;

	DO_SAFE(touch_ts_i2c_read(client, ts->f54_reg.current_noise_status, 1,
				&cns), error);
	cns_sum += cns;

	if (!ts->pdata->panel_id) {
		DO_SAFE(touch_ts_i2c_read(client, ts->f54_reg.cid_im, 2, buffer),
				error);
		cid_im = (buffer[1]<<8)|buffer[0];
		cid_im_sum += cid_im;
	}

	DO_SAFE(touch_ts_i2c_read(client, ts->f54_reg.freq_scan_im, 2, buffer),
			error);
	freq_scan_im = (buffer[1] << 8) | buffer[0];
	freq_scan_im_sum += freq_scan_im;

	DO_SAFE(synaptics_ts_set_page(client, DEFAULT_PAGE), error);

	cnt++;

	if ((ts->ts_state_flag.ts_noise_log_flag == TS_NOISE_LOG_ENABLE)
			|| (touch_ts_debug_mask & DEBUG_NOISE)) {
		if (prev_data->total_num != curr_data->total_num) {
			if (!ts->pdata->panel_id) {
				TOUCH_I(
						"Curr: CNS[%5d]   IM[%5d]   CID_IM[%5d] FREQ_SCAN_IM[%5d]\n",
						cns, im, cid_im, freq_scan_im);
			} else {
				TOUCH_I(
						"Curr: CNS[%5d]   IM[%5d]   FREQ_SCAN_IM[%5d]\n",
						cns, im, freq_scan_im);
			}
		}
	}

	for (i = 0; i < MAX_FINGER; i++) {
		if ((prev_data->report_id_mask & (1 << i))
				&& !(curr_data->id_mask & (1 << i))) {
			break;
		}
	}
	if (((i < MAX_FINGER) && curr_data->total_num == 0)
			|| (im_sum >= ULONG_MAX || cns_sum >= ULONG_MAX
				|| cid_im_sum >= ULONG_MAX
				|| freq_scan_im_sum >= ULONG_MAX
				|| cnt >= UINT_MAX)) {
		if ((ts->ts_state_flag.ts_noise_log_flag == TS_NOISE_LOG_ENABLE)
				|| (touch_ts_debug_mask & DEBUG_NOISE)) {
			if (!ts->pdata->panel_id) {
				TOUCH_I(
						"Aver: CNS[%5lu]   IM[%5lu]   CID_IM[%5lu]   FREQ_SCAN_IM[%5lu] (cnt:%u)\n",
						cns_sum/cnt,
						im_sum/cnt,
						cid_im_sum/cnt,
						freq_scan_im_sum/cnt,
						cnt);
			} else {
				TOUCH_I(
						"Aver: CNS[%5lu]   IM[%5lu]   FREQ_SCAN_IM[%5lu] (cnt:%u)\n",
						cns_sum/cnt,
						im_sum/cnt,
						freq_scan_im_sum/cnt,
						cnt);
			}
		}

		im_aver = im_sum/cnt;
		cns_aver = cns_sum/cnt;
		cid_im_aver = cid_im_sum/cnt;
		freq_scan_im_aver = freq_scan_im_sum/cnt;
	}

	if (prev_data->total_num == 0 && curr_data->total_num != 0) {
		cnt = im_sum = cns_sum = cid_im_sum = freq_scan_im_sum = 0;
		im_aver = cns_aver = cid_im_aver = freq_scan_im_aver = 0;
	}
	return 0;

error:
	 TOUCH_E("%s, %d : get ts noise failed\n", __func__, __LINE__);
	return -EPERM;
}

static int synaptics_ts_debug_noise(struct i2c_client *client,
		struct touch_data *curr_data,
		const struct touch_data *prev_data)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buffer[2] = {0};
	u8 report_rate_lsb = 0, sense_frq_select = 0;
	u8 data10_status = 0, cur_noise = 0;
	u16 freq_scan_im = 0;

	if (prev_data->total_num != curr_data->total_num) {
		DO_SAFE(synaptics_ts_set_page(client, ANALOG_PAGE), error);
		DO_SAFE(touch_ts_i2c_read(client, 0x06, 1, &report_rate_lsb),
				error);
		DO_SAFE(touch_ts_i2c_read(client, 0x0C, 1, &sense_frq_select),
				error);
		DO_SAFE(touch_ts_i2c_read(client, 0x09, 1, &data10_status), error);
		DO_SAFE(touch_ts_i2c_read(client, 0x08, 1, &cur_noise), error);
		DO_SAFE(touch_ts_i2c_read(client, ts->f54_reg.freq_scan_im,
					2, buffer), error);
		freq_scan_im = (buffer[1] << 8) | buffer[0];

		DO_SAFE(synaptics_ts_set_page(client, DEFAULT_PAGE), error);

		TOUCH_I("Report rate LSB    : [%5d],\n", report_rate_lsb);
		TOUCH_I("Sense Freq Select  : [%5d],\n", sense_frq_select);
		TOUCH_I("Analog data10 stats: [%5d],\n", data10_status);
		TOUCH_I("Current Noise stats: [%5d],\n", cur_noise);
		TOUCH_I("Freq Scan IM	    : [%5d]\n", freq_scan_im);
	}

	return 0;
error:
	TOUCH_E("%s : failed to get noise values\n", __func__);
	return -EPERM;
}


int synaptics_ts_get_object_count(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	/* determine finger count to process */
	u8 object_to_read = ts->num_of_fingers;
	u8 buf[2] = {0,};
	u16 object_attention_data = 0;
	DO_SAFE(touch_ts_i2c_read(ts->client, ts->f12_reg.data[15],
				sizeof(buf),
				(u8 *) buf), error);

	object_attention_data = (((u16)((buf[1] << 8)  & 0xFF00) |
				(u16)((buf[0]) & 0xFF)));

	for (; object_to_read > 0 ;) {
		if (object_attention_data & (0x1 << (object_to_read - 1)))
			break;
		else
			object_to_read--;
	}

	return object_to_read;
error:
	TOUCH_E(
			"%s, %d : get object_attention data failed\n",
			__func__, __LINE__);
	return -ERROR;
}

static int get_swipe_data(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	struct swipe_data *swp = &ts->swipe;
	u8 swipe_buf[11] = {0};
	u16 swipe_start_x = 0;
	u16 swipe_start_y = 0;
	u16 swipe_end_x = 0;
	u16 swipe_end_y = 0;
	u8 swipe_direction = 0;
	u8 swipe_fail_reason = 0;
	u16 swipe_time = 0;

	DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
				ts->swipe.coordinate_start_reg,
				11, swipe_buf), error);

	swipe_start_x = GET_U16_FROM_U8(swipe_buf[1], swipe_buf[0]);
	swipe_start_y = GET_U16_FROM_U8(swipe_buf[3], swipe_buf[2]);
	swipe_end_x = GET_U16_FROM_U8(swipe_buf[5], swipe_buf[4]);
	swipe_end_y = GET_U16_FROM_U8(swipe_buf[7], swipe_buf[6]);

	if (swp->support_swipe & SUPPORT_SWIPE_DOWN) {
		swipe_direction = SWIPE_DIRECTION_DOWN;
		swipe_fail_reason = swipe_buf[8];
	}

	if (swp->support_swipe & SUPPORT_SWIPE_UP) {
		swipe_direction = swipe_buf[8] & 0x03;
		swipe_fail_reason = (swipe_buf[8] & 0xfc) >> 2;
	}
	swipe_time = GET_U16_FROM_U8(swipe_buf[10], swipe_buf[9]);

	TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
			"LPWG Swipe Gesture: start(%4d,%4d) end(%4d,%4d) "
			"swipe_direction(%d) swipe_fail_reason(%d) "
			"swipe_time(%dms)\n",
			swipe_start_x, swipe_start_y,
			swipe_end_x, swipe_end_y,
			swipe_direction, swipe_fail_reason,
			swipe_time);

	if (swipe_fail_reason == 0) {
		ts->pw_data.data_num = 1;
		ts->pw_data.data[0].x = swipe_end_x;
		ts->pw_data.data[0].y = swipe_end_y;
		return swipe_direction;
	} else {
		TOUCH_I("swipe fail.\n");
		return -ERROR;
	}
error:
	TOUCH_E("failed to read swipe data.\n");
	return -ERROR;
}

enum error_type synaptics_ts_palm_control(struct synaptics_ts_data *ts)
{
	u8  i = 0;

	switch (ts->pdata->role->palm_ctrl_mode) {
	case PALM_REJECT_DRIVER:
	case PALM_REPORT:
		for (i = 0; i < ts->num_of_fingers; i++) {
			if (ts->palm.curr_mask[i]
					== ts->palm.prev_mask[i])
				continue;

			if (ts->palm.curr_mask[i]) {
				ts->palm.curr_num++;
				TOUCH_I(
						"Palm is detected : id[%d] pos[%4d,%4d] total palm:%u\n",
						i,
						ts->palm.coordinate[i].x,
						ts->palm.coordinate[i].y,
						ts->palm.curr_num);
			} else {
				ts->palm.curr_num--;
				TOUCH_I(
						"Palm is released : id[%d] pos[%4d,%4d] total palm:%u\n",
						i,
						ts->palm.coordinate[i].x,
						ts->palm.coordinate[i].y,
						ts->palm.curr_num);
			}
		}

		memcpy(ts->palm.prev_mask,
				ts->palm.curr_mask,
				sizeof(ts->palm.prev_mask));

		if (ts->pdata->role->palm_ctrl_mode
				== PALM_REJECT_DRIVER) {
			if (ts->palm.curr_num) {
				ts->palm.prev_num = ts->palm.curr_num;
				return NO_FILTER;
			}

			if (ts->palm.prev_num) {
				ts->palm.all_released = true;
				queue_delayed_work(touch_wq,
						&ts->work_palm,
						msecs_to_jiffies(50));
				TOUCH_I("All palm is released.\n");
				ts->palm.prev_num = ts->palm.curr_num;
				return NO_FILTER;
			}

			if (ts->palm.all_released) {
				ts->palm.all_released = true;
				cancel_delayed_work(&ts->work_palm);
				queue_delayed_work(touch_wq,
						&ts->work_palm,
						msecs_to_jiffies(50));
				return NO_FILTER;
			}

		}
		ts->palm.prev_num = ts->palm.curr_num;
		break;
	case PALM_REJECT_FW:
	default:
		break;
	}

	return NO_ERROR;
}

enum error_type synaptics_ts_get_data(struct i2c_client *client,
		struct touch_data *curr_data,
		const struct touch_data *prev_data)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	struct finger_data *finger = NULL;
	struct t_data *abs = NULL;
	enum error_type ret = NO_ERROR;
	u8  i = 0;
	u8  finger_index = 0, lpwg_fail = 0;
	u8 object_to_read;
	static u8 prev_object_to_read;
	bool odd_zvalue = false;
	int swipe_direction = 0;
	int swipe_uevent = 0;
	u8 buffer = 0;

	TOUCH_TRACE();

	if (!ts->is_init) {
		if (lpwg_by_lcd_notifier) {
			TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
					"ts->is_init = 0,"
					"lpwg_by_lcd_notifier = ture,"
					"handling lpwg event\n");
		} else {
			TOUCH_E("%s, %d : ts->is_init == 0, IGNORE_EVENT!!, s:\n",
					__func__, __LINE__);
			return IGNORE_EVENT;
		}
	}

	curr_data->total_num = 0;
	curr_data->id_mask = 0;

	if (ts->pdata->role->palm_ctrl_mode == PALM_REJECT_DRIVER ||
			ts->pdata->role->palm_ctrl_mode == PALM_REPORT) {
		memset(ts->palm.curr_mask, 0,
				sizeof(ts->palm.curr_mask));
	}

	DO_SAFE(touch_ts_i2c_read(client, DEVICE_STATUS_REG,
				sizeof(ts->ts_data.device_status_reg),
				&ts->ts_data.device_status_reg), error);

	DO_IF((ts->ts_data.device_status_reg & DEVICE_FAILURE_MASK)
			== DEVICE_FAILURE_MASK, error);

	DO_SAFE(touch_ts_i2c_read(client, INTERRUPT_STATUS_REG,
				sizeof(ts->ts_data.interrupt_status_reg),
				&ts->ts_data.interrupt_status_reg), error);

	if (ts->pdata->role->use_hover_finger && prox_fhandler.inserted
			&& prox_fhandler.initialized)
		prox_fhandler.exp_fn->attn(ts->ts_data.interrupt_status_reg);

	if (ts->ts_data.interrupt_status_reg & int_mask_cust) {
		u8 status = 0;

		DO_SAFE(synaptics_ts_page_data_read(client,
					LPWG_PAGE, ts->f51_reg.lpwg_status_reg,
					1, &status), error);

		if (ts->lpwg_ctrl.has_request_reset_reg) {
			DO_SAFE(synaptics_ts_page_data_read(client,
				LPWG_PAGE, ts->f51_reg.request_reset_reg,
				1, &lpwg_fail), error);
			if ((lpwg_fail & 0x01) && (!ts->lpwg_ctrl.screen)) {
				TOUCH_I("%s: LPWG Malfuction (lpwg_state 0x%02x) - goto reset\n",
					__func__, status);
				return ERROR_IN_LPWG;
			}
		}

		if ((status & 0x1)) {   /* TCI-1 Double-Tap */
			TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
					"LPWG Double-Tap mode\n");
			if (ts->lpwg_ctrl.double_tap_enable) {
				get_tci_data(ts, 2);
				send_uevent_lpwg(ts->client, LPWG_DOUBLE_TAP);
			}
		} else if ((status & 0x2)) { /* TCI-2 Multi-Tap */
			TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
					"LPWG Multi-Tap mode\n");
			if (ts->lpwg_ctrl.password_enable) {
				get_tci_data(ts, ts->pw_data.tap_count);
				wake_lock(&ts->timer_wake_lock);
				queue_delayed_work(touch_wq, &ts->work_timer,
						msecs_to_jiffies(UEVENT_DELAY
							- I2C_DELAY));
			}
		} else if ((ts->swipe.support_swipe)
				&& (status & ts->swipe.gesture_mask)) {
			swipe_direction = get_swipe_data(client);

			if (swipe_direction == SWIPE_DIRECTION_DOWN) {
				swipe_uevent = LPWG_SWIPE_DOWN;
			} else if (swipe_direction == SWIPE_DIRECTION_UP) {
				swipe_uevent = LPWG_SWIPE_UP;
			} else {
				return IGNORE_EVENT;
			}
			ts->pdata->swipe_stat[1] = DO_SWIPE;
			ts->pdata->swipe_pwr_ctr = SKIP_PWR_CON;
			send_uevent_lpwg(client, swipe_uevent);
			swipe_disable(ts);
		} else {
			if (ts->lpwg_ctrl.has_lpwg_overtap_module) {
				DO_SAFE(synaptics_ts_page_data_read(ts->client,
					LPWG_PAGE, ts->f51_reg.overtap_cnt_reg,
					1, &buffer), error);

				if (buffer > ts->pw_data.tap_count) {
					wake_lock(&ts->timer_wake_lock);
					ts->pw_data.data_num = 1;
					get_tci_data(ts, ts->pw_data.data_num);
					TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
						"knock code fail to over tap count(%u)\n",
						buffer);
					queue_delayed_work(touch_wq, &ts->work_timer,
						msecs_to_jiffies(0));
				}
			}
			TOUCH_D(DEBUG_BASE_INFO || DEBUG_LPWG,
					"LPWG status has problem\n");
		}
		return IGNORE_EVENT;
	} else if (ts->ts_data.interrupt_status_reg & INTERRUPT_MASK_ABS0) {
		object_to_read = synaptics_ts_get_object_count(client);
		ERROR_IF(unlikely(object_to_read < 0),
				"get_object_count error", error);
		object_to_read = object_to_read > prev_object_to_read ?
			object_to_read :
			prev_object_to_read;
		if (likely(object_to_read > 0)) {
			DO_SAFE(touch_ts_i2c_read(ts->client,
						FINGER_DATA_REG_START,
						sizeof(ts->ts_data.finger[0])
						* object_to_read,
						(u8 *) ts->ts_data.finger),
					error);
		}
		for (i = 0; i < object_to_read; i++) {
			finger = ts->ts_data.finger + i;
			abs = curr_data->touch + finger_index;

			if (finger->type == F12_NO_OBJECT_STATUS)
				continue;

			/* work around for wrong finger type*/
			/* by msm-i2c-v2 BAM mode*/
			if (finger->type > 6) {
				u8 *bb = (u8 *) finger;
				TOUCH_I(
						"wrong finger id:%d, type:%x\n",
						i, finger->type);
				TOUCH_I("x=%d y=%d\n",
						TS_POSITION(finger->x_msb,
							finger->x_lsb),
						TS_POSITION(finger->y_msb,
							finger->y_lsb));
				TOUCH_I(
						"%02x %02x %02x %02x %02x %02x %02x %02x\n",
						bb[0], bb[1], bb[2], bb[3],
						bb[4], bb[5], bb[6], bb[7]
				       );
				bb[0] = F12_FINGER_STATUS;
			}

			prev_object_to_read = i+1;
			abs->id = i;
			abs->type = finger->type;
			abs->raw_x = TS_POSITION(finger->x_msb, finger->x_lsb);
			abs->raw_y = TS_POSITION(finger->y_msb, finger->y_lsb);

			if (finger->wx > finger->wy) {
				abs->width_major = finger->wx;
				abs->width_minor = finger->wy;
				abs->orientation = 0;
			} else {
				abs->width_major = finger->wy;
				abs->width_minor = finger->wx;
				abs->orientation = 1;
			}
			abs->pressure = finger->z;

			abs->x = abs->raw_x;
			abs->y = abs->raw_y;

			if (abs->type == F12_PALM_STATUS) {
				switch (ts->pdata->role->palm_ctrl_mode) {
				case PALM_REJECT_DRIVER:
				case PALM_REPORT:
					abs->pressure
						= MAX_PRESSURE;
					ts->palm.curr_mask[i] = 1;
					ts->palm.coordinate[i].x = abs->x;
					ts->palm.coordinate[i].y = abs->y;
					break;
				case PALM_REJECT_FW:
				default:
					break;
				}
			}
			if (ts->pdata->role->ghost->pressure_zero_chk
					&& abs->pressure == 0)
				ts->pdata->role->ghost->pressure_zero = true;

			if (ts->pdata->role->ghost->pressure_high_chk
					&& ts->pdata->panel_id == 0
					&& abs->pressure >= 250)
				ts->pdata->role->ghost->pressure_high = true;

			curr_data->id_mask |= (0x1 << i);
			curr_data->total_num++;

			TOUCH_D(DEBUG_GET_DATA,
					"<%d> type[%d] pos(%4d,%4d) w_m[%2d] w_n[%2d] o[%2d] p[%2d]\n",
					i, abs->type,
					abs->x,
					abs->y,
					abs->width_major,
					abs->width_minor,
					abs->orientation,
					abs->pressure);

			finger_index++;

			if (curr_data->touch->pressure >= 250)
				odd_zvalue = true;
		}

		ret = synaptics_ts_palm_control(ts);

		if (ret == NO_FILTER) {
			memset(curr_data, 0, sizeof(struct touch_data));
			return NO_FILTER;
		}

		TOUCH_D(DEBUG_GET_DATA, "ID[0x%x] Total_num[%d]\n",
				curr_data->id_mask, curr_data->total_num);
		if (ts->lpwg_ctrl.password_enable &&
				wake_lock_active(&ts->timer_wake_lock)) {
			if (curr_data->id_mask &
					~(prev_data->id_mask)) {
				/* password-matching will be failed */
				if (cancel_delayed_work(&ts->work_timer)) {
					ts->pw_data.data_num = 1;
					queue_delayed_work(touch_wq,
						&ts->work_timer,
						msecs_to_jiffies(UEVENT_DELAY));
				}
			}
			return IGNORE_EVENT_BUT_SAVE_IT;
		}
		if (ts->lpwg_ctrl.password_enable &&
				atomic_read(&ts->lpwg_ctrl.is_suspend) == 1) {
			TOUCH_I("%s:ignore abs interrupt in suspend\n",
					__func__);
			return IGNORE_EVENT;
		}
	} else if (ts->ts_data.interrupt_status_reg & INTERRUPT_MASK_FLASH) {
		TOUCH_E("%s: INTERRUPT_MASK_FLASH!\n", __func__);
		if (ts->lpwg_ctrl.screen)
			return ERROR;
		else
			return ERROR_IN_LPWG;
	} else if (ts->ts_data.interrupt_status_reg & INTERRUPT_MASK_STATUS) {
		TOUCH_I("%s: INTERRUPT_MASK_STATUS!\n", __func__);
		TOUCH_I("(lpwg_mode:%d, screen:%d, power_state:%d)\n",
				ts->lpwg_ctrl.lpwg_mode,
				ts->lpwg_ctrl.screen, power_state);
		if (ts->lpwg_ctrl.screen)
			return IGNORE_EVENT;
		else
			return ERROR_IN_LPWG;
	} else {
		return IGNORE_EVENT;
	}

	if (odd_zvalue && is_product(ts, "PLG446", 6)) {
		odd_zvalue = false;
		DO_SAFE(synaptics_ts_debug_noise(client, curr_data, prev_data),
			error);
	}

	if ((ts->ts_state_flag.ts_noise_log_flag == TS_NOISE_LOG_ENABLE)
			|| (ts->ts_state_flag.check_noise_menu == MENU_ENTER))
		DO_SAFE(synaptics_ts_noise_log(client, curr_data, prev_data),
				error);

	return NO_ERROR;
error:
	TOUCH_E("%s, %d : get data failed\n", __func__, __LINE__);
	return ERROR;
}

enum error_type synaptics_ts_filter(struct i2c_client *client,
		struct touch_data *curr_data,
		const struct touch_data *prev_data)
{
	/* struct synaptics_ts_data *ts =
	  (struct synaptics_ts_data *)get_touch_handle(client);*/
	int i = 0;

	for (i = 0; i < curr_data->total_num; i++) {
		if (curr_data->touch[i].type == HOVER_TYPE)
			curr_data->touch[i].pressure = 0;
		else if (curr_data->touch[i].type == PALM_TYPE)
			curr_data->touch[i].pressure = MAX_PRESSURE;
		else if (curr_data->touch[i].pressure == MAX_PRESSURE)
			curr_data->touch[i].pressure = MAX_PRESSURE - 1;
	}

	return NO_ERROR;
}

enum error_type synaptics_ts_power(struct i2c_client *client, int power_ctrl)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int ret = 0;

	TOUCH_TRACE();

	if (ts->pdata->swipe_pwr_ctr == SKIP_PWR_CON) {
		TOUCH_I("%s : Skip power_control (swipe_pwr_ctr = %d)\n",
				__func__, ts->pdata->swipe_pwr_ctr);

		power_state = power_ctrl;
		TOUCH_I("%s : power_state[%d]\n", __func__, power_state);
		return NO_ERROR;
	}

	switch (power_ctrl) {
	case POWER_OFF:
		if (ts->swipe.support_swipe)
			print_swipe_fail_reason(ts);

		ts->is_init = 0;

		if (ts->pdata->reset_pin > 0)
			gpio_direction_output(ts->pdata->reset_pin, 0);

		if (ts->pdata->pwr->vio_control)
			if (ts->pdata->vio_pin > 0)
				gpio_direction_output(ts->pdata->vio_pin, 0);

		if (ts->pdata->pwr->use_regulator) {
			if (ts->pdata->pwr->vio_control) {
				if (regulator_is_enabled(ts->vreg_vio))
					regulator_disable(ts->vreg_vio);
			}
			if (regulator_is_enabled(ts->vreg_vdd))
				regulator_disable(ts->vreg_vdd);
		}
		break;
	case POWER_ON:
		ts->is_init = 0;
		if (ts->pdata->pwr->use_regulator) {
			if (!regulator_is_enabled(ts->vreg_vdd))
				ret = regulator_enable(ts->vreg_vdd);
			if (ts->pdata->pwr->vio_control) {
				if (!regulator_is_enabled(ts->vreg_vio))
					ret = regulator_enable(ts->vreg_vio);
			}
		}
		if (ts->pdata->pwr->vio_control) {
			if (ts->pdata->vio_pin > 0)
				gpio_direction_output(ts->pdata->vio_pin, 1);
		}
		if (ts->pdata->reset_pin > 0)
			gpio_direction_output(ts->pdata->reset_pin, 1);

		break;
	case POWER_SLEEP:
		if (!ts->lpwg_ctrl.lpwg_is_enabled
				&& is_product(ts, "PLG349", 6))
			sleep_control(ts, 0, 1);
		break;
	case POWER_WAKE:
		break;
	case POWER_SLEEP_STATUS:
		sleep_control(ts, ts->lpwg_ctrl.sensor, 0);
		power_ctrl = POWER_SLEEP;
		break;
	default:
		break;
	}
	power_state = power_ctrl;

	TOUCH_I("%s : power_state[%d]\n", __func__, power_state);
	return NO_ERROR;
}

enum error_type synaptics_ts_ic_ctrl(struct i2c_client *client,
		u8 code, u32 value, u32 *ret)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buf = 0;
	u8 buf_array[2] = {0};

	switch (code) {
	case IC_CTRL_READ:
		DO_SAFE(touch_ts_i2c_read(client, value, 1, &buf), error);
		*ret = (u32)buf;
		break;
	case IC_CTRL_WRITE:
		DO_SAFE(touch_i2c_write_byte(client, ((value & 0xFFF0) >> 8),
					(value & 0xFF)), error);
		break;
	case IC_CTRL_BASELINE_REBASE:
		DO_SAFE(synaptics_ts_page_data_write_byte(client,
					ANALOG_PAGE, ANALOG_COMMAND_REG, value),
				error);
		break;
	case IC_CTRL_REPORT_MODE:
		if (value == REDUCED_MODE)
			buf_array[0] = buf_array[1] =
				ts->pdata->role->delta_pos_threshold;
		DO_SAFE(touch_ts_i2c_write(client, ts->f12_reg.ctrl[20],
					2, buf_array), error);
		break;
	case IC_CTRL_THERMAL:
		TOUCH_I("Driver Thermal Control Skip... !!\n");
		break;
	case IC_CTRL_RESET:
		ts->is_init = 0;
		lpwg_by_lcd_notifier = false;
		DO_SAFE(touch_i2c_write_byte(client,
					DEVICE_COMMAND_REG, (value & 0xFF)),
				error);
		break;
	default:
		break;
	}

	return NO_ERROR;
error:
	TOUCH_E("%s, %d : IC control failed\n", __func__, __LINE__);
	return ERROR;
}

int compare_fw_version(struct i2c_client *client,
		struct touch_fw_info *fw_info) {
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int i = 0;

	if (ts->fw_info.version[0] > 0x50) {
		if (ts->fw_info.img_version[0] > 0x50) {
			TOUCH_D(DEBUG_BASE_INFO,
					"product_id[%s(ic):%s(fw)] version[%s(ic):%s(fw)]\n",
					ts->fw_info.product_id,
					ts->fw_info.img_product_id,
					ts->fw_info.version,
					ts->fw_info.img_version);
			if (strncmp(ts->fw_info.version,
						ts->fw_info.img_version,
						4)) {
				TOUCH_D(DEBUG_BASE_INFO,
						"version mismatch.\n");
				return 1;
			} else {
				goto no_upgrade;
			}
		} else {
			TOUCH_D(DEBUG_BASE_INFO,
					"product_id[%s(ic):%s(fw)] version[%s(ic):V%d.%02d(fw)]\n",
					ts->fw_info.product_id,
					ts->fw_info.img_product_id,
					ts->fw_info.version,
					(ts->fw_info.img_version[3]
					 & 0x80 ? 1 : 0),
					ts->fw_info.img_version[3] & 0x7F);
			if (strncmp(ts->fw_info.version,
						ts->fw_info.img_version,
						4)) {
				TOUCH_D(DEBUG_BASE_INFO,
						"version mismatch.\n");
				return 1;
			} else {
				goto no_upgrade;
			}
		}
	} else {
		if (!(ts->fw_info.version[3] & 0x80)) {
			if ((ts->fw_info.version[3] & 0x7F) == 0) {
				TOUCH_D(DEBUG_BASE_INFO,
						"FW version is someting wrong.[V%d.%02d], need to upgrade!\n",
						ts->fw_info.version[3] & 0x80 ?
						1 : 0,
						ts->fw_info.version[3] & 0x7F);
				return 1;
			} else if (((ts->fw_info.version[3] & 0x7F) == 40) &&
					is_product(ts, "PLG446", 6)) {
				TOUCH_D(DEBUG_BASE_INFO,
					"FW version is Test Version.[V%d.%02d]\n",
					(ts->fw_info.version[3] & 0x80 ?
					1 : 0),
					ts->fw_info.version[3] & 0x7F);
				TOUCH_D(DEBUG_BASE_INFO,
					"Need upgrade for DV Sample\n");
				return 1;
			} else {
				TOUCH_D(DEBUG_BASE_INFO,
						"FW version is Test Version.[V%d.%02d]\n",
						(ts->fw_info.version[3] & 0x80 ?
						 1 : 0),
						ts->fw_info.version[3] & 0x7F);
				goto no_upgrade;
			}
		}
		if (ts->fw_info.img_version[0] > 0x50) {
			TOUCH_D(DEBUG_BASE_INFO,
					"product_id[%s(ic):%s(fw)] fw_version[V%d.%02d(ic):%s(fw)]\n",
					ts->fw_info.product_id,
					ts->fw_info.img_product_id,
					(ts->fw_info.version[3]
					 & 0x80 ? 1 : 0),
					ts->fw_info.version[3] & 0x7F,
					ts->fw_info.img_version);
			if (strncmp(ts->fw_info.version,
						ts->fw_info.img_version,
						4)) {
				TOUCH_D(DEBUG_BASE_INFO,
						"version mismatch.\n");
				return 1;
			} else {
				goto no_upgrade;
			}
		} else {
			TOUCH_D(DEBUG_BASE_INFO,
					"product_id[%s(ic):%s(fw)]\n",
					ts->fw_info.product_id,
					ts->fw_info.img_product_id);
			TOUCH_D(DEBUG_BASE_INFO,
					"ic_version[V%d.%02d(0x%02X 0x%02X 0x%02X 0x%02X)]\n ",
					(ts->fw_info.version[3]
					 & 0x80 ? 1 : 0),
					ts->fw_info.version[3] & 0x7F,
					ts->fw_info.version[0],
					ts->fw_info.version[1],
					ts->fw_info.version[2],
					ts->fw_info.version[3]);
			TOUCH_D(DEBUG_BASE_INFO,
					"version[V%d.%02d(0x%02X 0x%02X 0x%02X 0x%02X)]\n",
					(ts->fw_info.img_version[3]
					 & 0x80 ? 1 : 0),
					ts->fw_info.img_version[3] & 0x7F,
					ts->fw_info.img_version[0],
					ts->fw_info.img_version[1],
					ts->fw_info.img_version[2],
					ts->fw_info.img_version[3]);
			for (i = 0; i < FW_VER_INFO_NUM; i++) {
				if (ts->fw_info.version[i] !=
						ts->fw_info.img_version[i]) {
					TOUCH_D(DEBUG_BASE_INFO,
						"version mismatch. ic_version[%d]:0x%02X != version[%d]:0x%02X\n",
						i, ts->fw_info.version[i],
						i, ts->fw_info.img_version[i]);
					return 1;
				}
			}
			goto no_upgrade;
		}
	}
no_upgrade:
	TOUCH_D(DEBUG_BASE_INFO | DEBUG_FW_UPGRADE,
			"need not fw version upgrade.\n");
	return 0;
}

enum error_type synaptics_ts_fw_upgrade(struct i2c_client *client,
		struct touch_fw_info *info, struct touch_firmware_module *fw)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	int need_upgrade = 0;
	int rc = 0;
	char path[256];
	const struct firmware *fw_entry = NULL;
	const u8 *firmware = NULL;

	if (info->fw_path) {
		TOUCH_D(DEBUG_BASE_INFO, "IC_product_id: %s\n",
			ts->fw_info.product_id);

		rc = request_firmware(&fw_entry,
			info->fw_path,
			&ts->client->dev);

		if (rc != 0) {
			TOUCH_E("request_firmware() failed %d\n", rc);
			goto error;
		}
	} else {
		TOUCH_E("error get fw_path\n");
		goto error;
	}

	firmware = fw_entry->data;

	memcpy(ts->fw_info.img_product_id,
			&firmware[ts->pdata->fw_pid_addr], 6);
	memcpy(ts->fw_info.img_version,
			&firmware[ts->pdata->fw_ver_addr], 4);

	if (info->force_upgrade) {
		TOUCH_D(DEBUG_BASE_INFO | DEBUG_FW_UPGRADE,
				"FW: need_upgrade[%d] force[%d] file[%s]\n",
				fw->need_upgrade, info->force_upgrade, path);
		goto firmware_up;
	}

	if (info->force_upgrade_cat) {
		TOUCH_D(DEBUG_BASE_INFO | DEBUG_FW_UPGRADE,
				"FW: need_upgrade[%d] force[%d] file[%s]\n",
				fw->need_upgrade,
				info->force_upgrade, info->fw_path);
		goto firmware_up;
	}

	need_upgrade = !strncmp(ts->fw_info.product_id,
			ts->fw_info.img_product_id,
			sizeof(ts->fw_info.product_id));

	/* Force Upgrade for P1 on 1st cut temporarily */
	if (is_product(ts, "s3320", 5) ||
		is_img_product(ts, "PLG468", 6))
		need_upgrade = 1;

	TOUCH_I("[%s] img_product_id : %s\n",
			__func__, ts->fw_info.img_product_id);

	rc = compare_fw_version(client, info);
	if (fw->need_upgrade)
		need_upgrade = need_upgrade && rc;
	else
		need_upgrade = need_upgrade && rc;

	TOUCH_I("ts_need_upgrade = %d, need_upgrade = %d\n",
			ts->pdata->fw->need_upgrade, need_upgrade);
	need_upgrade = ts->pdata->fw->need_upgrade & need_upgrade;

	if (need_upgrade || ts->fw_info.need_rewrite_firmware) {
		TOUCH_D(DEBUG_BASE_INFO | DEBUG_FW_UPGRADE,
				"FW: start-upgrade - need[%d] rewrite[%d]\n",
				need_upgrade,
				ts->fw_info.need_rewrite_firmware);

		if (info->fw_path != NULL) {
			TOUCH_I(
					"FW: need_upgrade[%d] force[%d] file[%s]\n",
				fw->need_upgrade,
				info->force_upgrade, info->fw_path);
			goto firmware_up;
		} else {
			goto firmware_up_error;
		}

		/* it will be reset and initialized
		 * automatically by lge_touch_core. */
	}
	release_firmware(fw_entry);
	return NO_UPGRADE;

firmware_up:
	ts->is_probed = 0;
	ts->is_init = 0; /* During upgrading, interrupt will be ignored. */
	info->force_upgrade = 0;
	info->force_upgrade_cat = 0;
	need_scan_pdt = true;
	DO_SAFE(FirmwareUpgrade(ts, fw_entry), error);
	release_firmware(fw_entry);
	return NO_ERROR;
firmware_up_error:
	release_firmware(fw_entry);
	return ERROR;
error:
	return ERROR;
}

enum error_type synaptics_ts_notify(struct i2c_client *client,
		u8 code, u32 value)
{
	 struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	switch (code) {
	case NOTIFY_TA_CONNECTION:
		queue_delayed_work(touch_wq,
			&ts->work_sleep, msecs_to_jiffies(0));
		break;
	case NOTIFY_TEMPERATURE_CHANGE:
		break;
	case NOTIFY_PROXIMITY:
		break;
	default:
		break;
	}

	return NO_ERROR;
}

enum error_type synaptics_ts_suspend(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	if (ts->pdata->role->use_hover_finger && prox_fhandler.inserted
			&& prox_fhandler.initialized)
		prox_fhandler.exp_fn->suspend(ts);

	if (!atomic_read(&ts->lpwg_ctrl.is_suspend)) {
		DO_SAFE(lpwg_control(ts, ts->lpwg_ctrl.lpwg_mode), error);
		atomic_set(&ts->lpwg_ctrl.is_suspend, 1);
	}
	ts->lpwg_ctrl.screen = 0;

	return NO_ERROR;
error:
	return ERROR;
}

enum error_type synaptics_ts_resume(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	if (ts->pdata->role->use_hover_finger && prox_fhandler.inserted
			&& prox_fhandler.initialized)
		prox_fhandler.exp_fn->resume(ts);

	cancel_delayed_work_sync(&ts->work_timer);

	if (wake_lock_active(&ts->timer_wake_lock))
		wake_unlock(&ts->timer_wake_lock);
	atomic_set(&ts->lpwg_ctrl.is_suspend, 0);
	ts->lpwg_ctrl.screen = 1;

	return NO_ERROR;
}
enum error_type synaptics_ts_lpwg(struct i2c_client *client,
		u32 code, int64_t value, struct point *data)
{
	int i;
	u8 buffer[50] = {0};
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 mode = ts->lpwg_ctrl.lpwg_mode;
	u8 doubleTap_area_reg_addr = ts->f12_reg.ctrl[18];

	switch (code) {
	case LPWG_READ:
		memcpy(data, ts->pw_data.data,
				sizeof(struct point) * ts->pw_data.data_num);
		data[ts->pw_data.data_num].x = -1;
		data[ts->pw_data.data_num].y = -1;
		/* '-1' should be assigned to the last data.
		   Each data should be converted to LCD-resolution.*/
		memset(ts->pw_data.data, -1,
				sizeof(struct point) * ts->pw_data.data_num);
		break;
	case LPWG_ENABLE:
		if (!atomic_read(&ts->lpwg_ctrl.is_suspend))
			ts->lpwg_ctrl.lpwg_mode = value;
		break;
	case LPWG_LCD_X:
	case LPWG_LCD_Y:
		/* If touch-resolution is not same with LCD-resolution,
		   position-data should be converted to LCD-resolution.*/
		break;
	case LPWG_ACTIVE_AREA_X1:
		for (i = 0; i < 2; i++) {
			synaptics_ts_page_data_read(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
			if (i == 0)
				buffer[i] = value;
			else
				buffer[i] = value >> 8;
			synaptics_ts_page_data_write(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
		}
		break;
	case LPWG_ACTIVE_AREA_X2:
		for (i = 4; i < 6; i++) {
			synaptics_ts_page_data_read(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
			if (i == 4)
				buffer[i] = value;
			else
				buffer[i] = value >> 8;
			synaptics_ts_page_data_write(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
		}
		break;
	case LPWG_ACTIVE_AREA_Y1:
		for (i = 2; i < 4; i++) {
			synaptics_ts_page_data_read(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
			if (i == 2)
				buffer[i] = value;
			else
				buffer[i] = value >> 8;
			synaptics_ts_page_data_write(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
		}
		break;
	case LPWG_ACTIVE_AREA_Y2:
		/* Quick Cover Area*/
		for (i = 6; i < 8; i++) {
			synaptics_ts_page_data_read(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
			if (i == 6)
				buffer[i] = value;
			else
				buffer[i] = value >> 8;
			synaptics_ts_page_data_write(client,
					COMMON_PAGE, doubleTap_area_reg_addr,
					i + 1, buffer);
		}
		break;
	case LPWG_TAP_COUNT:
		ts->pw_data.tap_count = value;
		if (ts->lpwg_ctrl.password_enable)
			tci_control(ts, TAP_COUNT_CTRL,
					(u8)ts->pw_data.tap_count);
		break;
	case LPWG_LENGTH_BETWEEN_TAP:
		if (ts->lpwg_ctrl.double_tap_enable
				|| ts->lpwg_ctrl.password_enable)
			tci_control(ts, TAP_DISTANCE_CTRL, value);
		break;
	case LPWG_EARLY_SUSPEND:
		if (!mode)
			break;

		/* wakeup gesture enable */
		if (value) {
			if (atomic_read(&ts->lpwg_ctrl.is_suspend) == 1
					&& (power_state == POWER_OFF
						|| power_state
						== POWER_SLEEP))
				ts->is_init = 0;

			DO_SAFE(lpwg_control(ts, 0), error);
			atomic_set(&ts->lpwg_ctrl.is_suspend, 0);
		} else {
			if (is_product(ts, "PLG349", 6))
				set_doze_param(ts, 3);
			DO_SAFE(lpwg_control(ts, ts->lpwg_ctrl.lpwg_mode),
					error);
			atomic_set(&ts->lpwg_ctrl.is_suspend, 1);
		}
		break;
	case LPWG_SENSOR_STATUS:
		if (!mode)
			break;

		if (value) { /* Far */
			DO_SAFE(lpwg_control(ts, mode), error);
		} else { /* Near */
			if (ts->lpwg_ctrl.password_enable &&
					wake_lock_active(
						&ts->timer_wake_lock)) {
				cancel_delayed_work_sync(&ts->work_timer);
				tci_control(ts, REPORT_MODE_CTRL, 1);
				wake_unlock(&ts->timer_wake_lock);
			}
		}
		break;
	case LPWG_DOUBLE_TAP_CHECK:
		ts->pw_data.double_tap_check = value;
		if (ts->lpwg_ctrl.password_enable)
			tci_control(ts, INTERRUPT_DELAY_CTRL, value);
		break;
	case LPWG_REPLY:
		if (ts->pdata->role->use_lpwg_all) {
			if (atomic_read(&ts->lpwg_ctrl.is_suspend) == 0) {
				TOUCH_I("%s : screen on\n", __func__);
				break;
			}
			DO_SAFE(lpwg_update_all(ts, 1), error);
		} else {
			if (ts->lpwg_ctrl.password_enable && !value)
				DO_SAFE(lpwg_control(ts, mode), error);
		}
		break;
	case LPWG_UPDATE_ALL:
		{
			int *v = (int *) value;
			int mode = *(v + 0);
			int screen = *(v + 1);
			int sensor = *(v + 2);
			int qcover = *(v + 3);

			ts->lpwg_ctrl.lpwg_mode = mode;
			ts->lpwg_ctrl.screen = screen;
			ts->lpwg_ctrl.sensor = sensor;
			ts->lpwg_ctrl.qcover = qcover;

			TOUCH_I(
					"LPWG_UPDATE_ALL: mode[%s], screen[%s], sensor[%s], qcover[%s]\n",
					ts->lpwg_ctrl.lpwg_mode ?
					"ENABLE" : "DISABLE",
					ts->lpwg_ctrl.screen ?
					"ON" : "OFF",
					ts->lpwg_ctrl.sensor ?
					"FAR" : "NEAR",
					ts->lpwg_ctrl.qcover ?
					"CLOSE" : "OPEN");
			DO_SAFE(lpwg_update_all(ts, 1), error);
			break;
		}
		/* LPWG On Sequence has to be */
		/* after Display off callback timing. */
	case LPWG_INCELL_LPWG_ON:
		if (is_product(ts, "PLG446", 6)) {
			lpwg_by_lcd_notifier = true;
			set_rebase_param(ts, 0);
			tci_control(ts, REPORT_MODE_CTRL, 1);
		} else if (is_product(ts, "PLG468", 6)) {
			TOUCH_I("[%s] CONTROL_REG : DEVICE_CONTROL_NOSLEEP\n",
					__func__);
			DO_SAFE(touch_i2c_write_byte(client,
				DEVICE_CONTROL_REG,
				DEVICE_CONTROL_NOSLEEP
				| DEVICE_CONTROL_CONFIGURED),
					error);

			tci_control(ts, REPORT_MODE_CTRL, 1);
			lpwg_by_lcd_notifier = true;
		}
		/* Protocol 9 enable for sleep control */
		ts->lpwg_ctrl.protocol9_sleep_flag = true;
		TOUCH_D(DEBUG_BASE_INFO, "Protocol 9 enable!\n");
		break;
	case LPWG_INCELL_LPWG_OFF:
		if (is_product(ts, "PLG446", 6)) {
			TOUCH_I("[%s] DEVICE_CONTROL_NORMAL_OP\n",
					__func__);
			DO_SAFE(touch_i2c_write_byte(client,
						DEVICE_CONTROL_REG,
						DEVICE_CONTROL_NORMAL_OP
						| DEVICE_CONTROL_CONFIGURED),
					error);
		}
		/* normal */
		tci_control(ts, REPORT_MODE_CTRL, 0);
		lpwg_by_lcd_notifier = false;
		/* Protocol 9 disable for sleep control */
		ts->lpwg_ctrl.protocol9_sleep_flag = false;
		TOUCH_D(DEBUG_BASE_INFO, "Protocol 9 disable!\n");
		break;
	case LPWG_INCELL_NO_SLEEP:
		msleep(20);
		TOUCH_I("[%s] CONTROL_REG : DEVICE_CONTROL_NOSLEEP\n",
				__func__);
		DO_SAFE(touch_i2c_write_byte(client,
			DEVICE_CONTROL_REG,
			DEVICE_CONTROL_NOSLEEP
			| DEVICE_CONTROL_CONFIGURED),
		error);
		if (is_product(ts, "PLG446", 6))
			mdelay(30);
		break;
	default:
		break;
	}

	return NO_ERROR;
error:
	return ERROR;
}

static void synapitcs_change_ime_status(struct i2c_client *client,
		int ime_status)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	u8 udata[5] = {0, };
	u8 drumming_address = ts->f12_reg.ctrl[10];

	TOUCH_I("%s : IME STATUS is [ %d ]!!!\n", __func__, ime_status);

	touch_ts_i2c_read(ts->client, drumming_address, 5, udata);

	if (ime_status) {
		TOUCH_I("%s : IME on !!\n", __func__);
		udata[3] = 0x08;/*Drumming Acceleration Threshold*/
		udata[4] = 0x05;/*Minimum Drumming Separation*/
		if (touch_ts_i2c_write(ts->client,
					drumming_address, 5, udata) < 0) {
			TOUCH_E("%s : Touch i2c write fail !!\n",
					__func__);
		}
	} else {
		udata[3] = 0x0f; /*Drumming Acceleration Threshold*/
		udata[4] = 0x0a; /*Minimum Drumming Separation*/
		if (touch_ts_i2c_write(ts->client,
					drumming_address, 5, udata) < 0) {
			TOUCH_E("%s : Touch i2c write fail !!\n",
					__func__);
		}
		TOUCH_I("%s : IME Off\n", __func__);
	}
	TOUCH_I("%s : Done !!\n", __func__);
	return;
}

static void synaptics_toggle_swipe(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	if (ts->swipe.support_swipe == NO_SUPPORT_SWIPE) {
		TOUCH_I("%s: support_swipe:0x%02X\n",
				__func__, ts->swipe.support_swipe);
		return;
	}

	if (power_state == POWER_OFF) {
		TOUCH_I("%s: power_state:%d\n",	__func__, power_state);
		return;
	}

	TOUCH_I("%s: [S/Q/P/L] = [%d/%d/%d/%d]\n", __func__,
			ts->lpwg_ctrl.screen, ts->lpwg_ctrl.qcover,
			power_state, ts->pdata->lockscreen_stat);

	if (!ts->lpwg_ctrl.screen
			&& !ts->lpwg_ctrl.qcover
			&& (power_state == POWER_SLEEP)
			&& ts->pdata->lockscreen_stat)
		swipe_enable(ts);
	else
		swipe_disable(ts);

	return;
}

static int get_type_bootloader(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	u8 temp_pid[11] = {0,};

	DO_SAFE(touch_ts_i2c_read(ts->client, PRODUCT_ID_REG,
			sizeof(ts->fw_info.product_id) - 1,
			ts->fw_info.product_id), error);
	TOUCH_I("[%s]IC_product_id: %s\n"
			, __func__, ts->fw_info.product_id);

	if (is_product(ts, "S332U", 5) || is_product(ts, "S3320T", 6)) {
		DO_SAFE(touch_ts_i2c_read(ts->client, FLASH_CONFIG_ID_REG,
				sizeof(temp_pid) - 1,
				temp_pid), error);
		memset(ts->fw_info.product_id, 0,
				sizeof(ts->fw_info.product_id));
		memcpy(ts->fw_info.product_id, &temp_pid[4], 6);

		TOUCH_I("[%s] Product_ID_Reset ! , addr = 0x%x, P_ID = %s\n",
				__func__,
				FLASH_CONFIG_ID_REG,
				ts->fw_info.product_id);

		return BL_VER_HIGHER;
	}

	return BL_VER_LOWER;
error:
	return -EPERM;
}

static int set_doze_param(struct synaptics_ts_data *ts, int value)
{
	u8 buf_array[6] = {0};

	if (ts->pdata->panel_id) {
		TOUCH_D(DEBUG_BASE_INFO, "panel_id = %d, ignore %s\n",
			ts->pdata->panel_id, __func__);
		return 0;
	}

	touch_ts_i2c_read(ts->client,
			ts->f12_reg.ctrl[27], 6, buf_array);

	/* max active duration */
	if (ts->pw_data.tap_count < 3)
		buf_array[3] = 3;
	else
		buf_array[3] = 3 + ts->pw_data.tap_count;

	buf_array[2] = 0x0C;  /* False Activation Threshold */
	buf_array[4] = 0x01;  /* Timer 1 */
	buf_array[5] = 0x01;  /* Max Active Duration Timeout */

	touch_ts_i2c_write(ts->client, ts->f12_reg.ctrl[27],
			6, buf_array);

	DO_SAFE(touch_i2c_write_byte(ts->client,
				DOZE_INTERVAL_REG, 3), error);
	DO_SAFE(touch_i2c_write_byte(ts->client,
				DOZE_WAKEUP_THRESHOLD_REG, 30), error);

	return 0;
error:
	TOUCH_E("%s : failed to set doze interval\n", __func__);
	return -EPERM;
}

enum window_status synapitcs_check_crack(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	char result[2] = {0x00, };

	if (need_scan_pdt) {
		SCAN_PDT();
		need_scan_pdt = false;
	}

	touch_ts_disable_irq(ts->client->irq);
	F54Test('l', (int)ts->pdata->role->crack->min_cap_value,
			result);
	touch_ts_enable_irq(ts->client->irq);

	TOUCH_I("%s : check window crack = %s\n",
			__func__, result);

	after_crack_check = 1; /* set crack check flag */

	if (strncmp(result, "1", 1) == 0)
		return CRACK;
	else
		return NO_CRACK;
}

static void synaptics_change_sleepmode(struct i2c_client *client)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);
	u8 curr[2] = {0};

	if (is_product(ts, "PLG468", 6)) {
		DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
				LPWG_PARTIAL_REG + 71,
				1, curr), error);
		TOUCH_I("%s: prev:0x%02X, next:0x%02X (TA :%d)\n",
				__func__,
				curr[0],
				touch_ta_status ? (curr[0] & 0xff) | 0x02 :
							(curr[0] & 0xff) & 0xfd,
				touch_ta_status);
		if (touch_ta_status)
			curr[0] = (curr[0] & 0xff) | 0x02;
		else
			curr[0] = (curr[0] & 0xff) & 0xfd;
		DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
				LPWG_PARTIAL_REG + 71,
				1, curr), error);
	} else if (is_product(ts, "PLG446", 6)) {
		if (touch_ta_status == 2 || touch_ta_status == 3) {
			curr[0] = 0x01;
			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
						LPWG_PARTIAL_REG + 4, 1, curr), error);
			DO_SAFE(touch_ts_i2c_read(client, DEVICE_CONTROL_REG, 1,
						curr), error);
			DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
						DEVICE_CONTROL_NOSLEEP
						| (curr[0] & 0xF8)),
					error);
		} else {
			curr[0] = 0x00;
			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
						LPWG_PARTIAL_REG + 4, 1, curr), error);
			DO_SAFE(touch_ts_i2c_read(client, DEVICE_CONTROL_REG, 1,
						curr), error);
			DO_SAFE(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
						DEVICE_CONTROL_NORMAL_OP
						| (curr[0] & 0xF8)),
					error);
		}
	}

	return;
error:
	TOUCH_E("%s : failed to set sleep_mode\n", __func__);
	return;
}

static void synaptics_ts_incoming_call(struct i2c_client *client, int value)
{
	struct synaptics_ts_data *ts =
		(struct synaptics_ts_data *)get_touch_handle(client);

	u8 curr[2] = {0};

	incoming_call_state = value;

	if (is_product(ts, "PLG468", 6)) {
		if (incoming_call_state) {
			DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, curr), error);
			curr[0] = (curr[0] & 0xff) & 0xfb;
			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, curr), error);
			DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, curr), error);
			TOUCH_I("%s : incoming_call(%d) = 0x%02x\n",
				__func__, incoming_call_state, curr[0]);
		} else {
			DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, curr), error);
			curr[0] = (curr[0] & 0xff) | 0x04;
			DO_SAFE(synaptics_ts_page_data_write(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, curr), error);
			DO_SAFE(synaptics_ts_page_data_read(client, LPWG_PAGE,
					LPWG_PARTIAL_REG + 71,
					1, curr), error);
			TOUCH_I("%s : incoming_call(%d) = 0x%02x\n",
				__func__, incoming_call_state, curr[0]);
		}
	} else {
		TOUCH_I("%s : Not incoming_call setting\n",
				__func__);
	}
	return;
error:
	TOUCH_E("%s : failed to set incoming_call_mode\n", __func__);
	return;
}

enum error_type synaptics_ts_shutdown(struct i2c_client *client)
{
	struct synaptics_ts_data *ts
		= (struct synaptics_ts_data *)get_touch_handle(client);

	TOUCH_TRACE();

	if (is_product(ts, "PLG468", 6)) {
		if (ts->pdata->reset_pin > 0)
			gpio_direction_output(ts->pdata->reset_pin, 0);
	}

	return NO_ERROR;
}

static int synapitcs_ts_register_sysfs(struct kobject *k)
{
	return sysfs_create_group(k, &synaptics_ts_attribute_group);
}

struct touch_device_driver synaptics_ts_driver = {
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
	.shutdown	= synaptics_ts_shutdown,
	.suspend	= synaptics_ts_suspend,
	.resume		= synaptics_ts_resume,
	.init		= synaptics_ts_init,
	.data		= synaptics_ts_get_data,
	.filter		= synaptics_ts_filter,
	.power		= synaptics_ts_power,
	.ic_ctrl	= synaptics_ts_ic_ctrl,
	.fw_upgrade	= synaptics_ts_fw_upgrade,
	.notify		= synaptics_ts_notify,
	.lpwg		= synaptics_ts_lpwg,
	.ime_drumming = synapitcs_change_ime_status,
	.toggle_swipe = synaptics_toggle_swipe,
	.inspection_crack = synapitcs_check_crack,
	.register_sysfs = synapitcs_ts_register_sysfs,
	.incoming_call = synaptics_ts_incoming_call,
};

static struct of_device_id match_table[] = {
	{ .compatible = "synap,s3320",},
	{ },
};
static void async_touch_init(void *data, async_cookie_t cookie)
{
	//int panel_type = lge_get_panel();
	int panel_type = 2; //force set LGD_INCELL_CMD_PANEL
	TOUCH_D(DEBUG_BASE_INFO, "panel type is %d\n", panel_type);

	if (panel_type == 3)
		return;
	touch_driver_register(&synaptics_ts_driver, match_table);
	return;
}


static int __init touch_init(void)
{
	TOUCH_TRACE();
	/* async_schedule(async_touch_init, NULL); */
	async_schedule(async_touch_init, NULL);

	return 0;
}

static void __exit touch_exit(void)
{
	TOUCH_TRACE();
	touch_driver_unregister();
}

module_init(touch_init);
module_exit(touch_exit);

MODULE_AUTHOR("yehan.ahn@lge.com, hyesung.shin@lge.com");
MODULE_DESCRIPTION("LGE Touch Driver");
MODULE_LICENSE("GPL");

