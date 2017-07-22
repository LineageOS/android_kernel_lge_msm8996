/* touch_lg4945.h
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

#ifndef LGE_TOUCH_LG4945_H
#define LGE_TOUCH_LG4945_H

#define USE_ABT_MONITOR_APP		0

#include "touch_lg4945_watch.h"
/* report packet */

struct lg4945_touch_data {
	u8 tool_type:4;
	u8 event:4;
	s8 track_id;
	u16 x;
	u16 y;
	u8 pressure;
	u8 angle;
	u16 width_major;
	u16 width_minor;
} __packed;

struct lg4945_touch_info {
	u32 device_status;
	u32 wakeup_type:8;
	u32 touch_cnt:5;
	u32 button_cnt:3;
	u32 palm_bit:16;
	struct lg4945_touch_data data[10];
} __packed;

#define PALM_ID				15

/* device control */
#define tc_version			(0x8A40u)
#define tc_product_code			(0x8A41u)
#define tc_product_id1			(0x8A42u)
#define tc_product_id2			(0x8A43u)
#define tc_status			(0x8BFFu)

#define report_base			(0x8C00u)

#define tc_device_ctl			(0xC000u)
#define tc_interrupt_ctl		(0xC001u)
#define tc_drive_ctl			(0xC002u)
#define tc_interrupt_status		(0xC021u)

#define info_chip_revision	(0x8A7Au)
#define info_lot_num			(0x8A7Cu)
#define info_serial_num		(0x8A7Du)
#define info_date			(0x8A7Eu)
#define info_time			(0x8A7Fu)

#define TCI_ENABLE_W			(0xC200u)
#define TAP_COUNT_W			(0xC201u)
#define MIN_INTERTAP_W			(0xC202u)
#define MAX_INTERTAP_W			(0xC203u)
#define TOUCH_SLOP_W			(0xC204u)
#define TAP_DISTANCE_W			(0xC205u)
#define INT_DELAY_W			(0xC206u)
#define ACT_AREA_X1_W			(0xC207u)
#define ACT_AREA_Y1_W			(0xC208u)
#define ACT_AREA_X2_W			(0xC209u)
#define ACT_AREA_Y2_W			(0xC20Au)

#define SWIPE_ENABLE_W			(0xC210u)
#define SWIPE_DIST_W			(0xC211u)
#define SWIPE_RATIO_THR_W		(0xC212u)
#define SWIPE_RATIO_PERIOD_W		(0xC214u)
#define SWIPE_RATIO_DIST_W		(0xC213u)
#define SWIPE_TIME_MIN_W		(0xC215u)
#define SWIPE_TIME_MAX_W		(0xC216u)
#define SWIPE_ACT_AREA_X1_W		(0xC217u)
#define SWIPE_ACT_AREA_Y1_W		(0xC218u)
#define SWIPE_ACT_AREA_X2_W		(0xC219u)
#define SWIPE_ACT_AREA_Y2_W		(0xC21Au)

#define TCI_FAIL_DEBUG_R		(0x8BCB)
#define TCI_FAIL_BIT_R			(0x8BCC)
#define TCI_FAIL_DEBUG_W		(0xC20C)
#define TCI_FAIL_BIT_W			(0xC20D)
#define SWIPE_FAIL_DEBUG_R		(0x8BDB)
#define SWIPE_FAIL_DEBUG_W		(0xC21D)
#define TCI_DEBUG_R			(0x8BEC)
#define SWIPE_DEBUG_R			(0x8BF5)
#define REG_IME_STATE			(0xC261)
#define R_HEADER_SIZE			(6)
#define W_HEADER_SIZE			(4)

/* spi 0:All HW access, 2: FW access */
#define SPR_SPI_ACCESS			(0xD074u)
/* charger status */
#define SPR_CHARGER_STS			(0xC260u)

#define CONNECT_NONE			(0x00)
#define CONNECT_USB			(0x01)
#define CONNECT_TA			(0x02)
#define CONNECT_OTG			(0x03)
#define CONNECT_WIRELESS		(0x10)

enum {
	TOUCHSTS_IDLE = 0,
	TOUCHSTS_DOWN,
	TOUCHSTS_MOVE,
	TOUCHSTS_UP,
};

enum {
	ABS_MODE = 0,
	KNOCK_1,
	KNOCK_2,
	SWIPE_RIGHT,
	SWIPE_LEFT,
	CUSTOM_DEBUG = 200,
	KNOCK_OVERTAP = 201,
};

enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U1,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};

enum {
	SWIPE_R = 0,
	SWIPE_L,
};

enum {
	SWIPE_RIGHT_BIT	= 1,
	SWIPE_LEFT_BIT	= 1 << 16,
};

/* swipe */
enum {
	SWIPE_ENABLE_CTRL = 0,
	SWIPE_DISABLE_CTRL,
	SWIPE_DIST_CTRL,
	SWIPE_RATIO_THR_CTRL,
	SWIPE_RATIO_PERIOD_CTRL,
	SWIPE_RATIO_DIST_CTRL,
	SWIPE_TIME_MIN_CTRL,
	SWIPE_TIME_MAX_CTRL,
	SWIPE_AREA_CTRL,
};

/* SPR control */

/* Firmware control */
#define SYS_BASE_ADDR		(0xD000)
#define SYS_RST_CTL			(SYS_BASE_ADDR + 0x8)
#define SYS_CRC_CTL			(SYS_BASE_ADDR + 0xE)
#define SYS_CRC_STS			(SYS_BASE_ADDR + 0xF)
#define SYS_BOOT_CTL			(SYS_BASE_ADDR + 0x12)
#define SYS_SDRAM_CTL		(SYS_BASE_ADDR + 0x13)
#define SYS_DISPMODE_ST		(SYS_BASE_ADDR + 0x15)

#define FLASH_CTRL			0xC090
#define FLASH_DEST			0x8101
#define FLASH_STS			0x8102
#define FLASH_BOOTCHK			0x8103
#define FLASH_START			0x8200
#define FLASH_BOOTCHK_VALUE		0xA0A0A0A0
/* test control */

struct lg4945_fw_info {
	u8 version[2];
	u8 product_id[8];
	u8 image_version[2];
	u8 image_product_id[8];
	u8 revision;
};

struct swipe_info {
	u8	distance;
	u8	ratio_thres;
	u8	ratio_distance;
	u8	ratio_period;
	u16	min_time;
	u16	max_time;
	struct active_area area;
};

struct swipe_ctrl {
	u32 mode;
	struct swipe_info info[2]; /* down is 0, up 1 - LG4945 use up */
};

struct lg4945_data {
	struct device *dev;
	struct kobject kobj;
	struct lg4945_touch_info info;
	struct lg4945_fw_info fw;
	u8 lcd_mode;
	u8 u3fake;
	struct watch_data watch;
	struct swipe_ctrl swipe;
	struct mutex spi_lock;
	struct delayed_work font_download_work;
	u32 charger;
	u8 tci_debug_type;
	u8 swipe_debug_type;
	atomic_t block_watch_cfg;
};

#define TCI_MAX_NUM			2
#define SWIPE_MAX_NUM			2
#define TCI_DEBUG_MAX_NUM			16
#define SWIPE_DEBUG_MAX_NUM			8
#define DISTANCE_INTER_TAP		(0x1 << 1) /* 2 */
#define DISTANCE_TOUCHSLOP		(0x1 << 2) /* 4 */
#define TIMEOUT_INTER_TAP_LONG		(0x1 << 3) /* 8 */
#define MULTI_FINGER			(0x1 << 4) /* 16 */
#define DELAY_TIME			(0x1 << 5) /* 32 */
#define TIMEOUT_INTER_TAP_SHORT		(0x1 << 6) /* 64 */
#define PALM_STATE			(0x1 << 7) /* 128 */
#define TAP_TIMEOVER			(0x1 << 8) /* 256 */
#define TCI_DEBUG_ALL (DISTANCE_INTER_TAP | DISTANCE_TOUCHSLOP |\
	TIMEOUT_INTER_TAP_LONG | MULTI_FINGER | DELAY_TIME |\
	TIMEOUT_INTER_TAP_SHORT | PALM_STATE | TAP_TIMEOVER)

static inline struct lg4945_data *to_lg4945_data(struct device *dev)
{
	return (struct lg4945_data *)touch_get_device(to_touch_core(dev));
}

static inline struct lg4945_data *to_lg4945_data_from_kobj(struct kobject *kobj)
{
	return (struct lg4945_data *)container_of(kobj,
			struct lg4945_data, kobj);
}

int lg4945_reg_read(struct device *dev, u16 addr, void *data, int size);
int lg4945_reg_write(struct device *dev, u16 addr, void *data, int size);
int lg4945_ic_info(struct device *dev);
int lg4945_tc_driving(struct device *dev, int mode);

static inline int lg4945_read_value(struct device *dev,
					u16 addr, u32 *value)
{
	return lg4945_reg_read(dev, addr, value, sizeof(*value));
}

static inline int lg4945_write_value(struct device *dev,
					 u16 addr, u32 value)
{
	return lg4945_reg_write(dev, addr, &value, sizeof(value));
}

#endif /* LGE_TOUCH_LG4945_H */
