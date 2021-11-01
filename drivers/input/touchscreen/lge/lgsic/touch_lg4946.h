/* touch_lg4946.h
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

#ifndef LGE_TOUCH_LG4946_H
#define LGE_TOUCH_LG4946_H

#include "touch_lg4946_watch.h"
#include "touch_lg4946_asc.h"	/* ASC */
#include <linux/pm_qos.h>

/* debug info */
#define DEBUG_BUF_SIZE 1024

struct lg4946_touch_debug {
	u32 padding[3];	/* packet structure 4+4+4+12*10+12+112 */
	u8 protocol_ver;
	u8 reserved_1;
	u32 frame_cnt;
	u8 rn_max_bfl;
	u8 rn_max_afl;
	u8 rn_min_bfl;
	u8 rn_min_afl;
	u8 rn_max_afl_x;
	u8 rn_max_afl_y;
	s8 lf_oft[18];
	u8 seg1_cnt:4;
	u8 seg2_cnt:4;
	u8 seg1_thr;
	u8 rebase[8];
	u8 rn_pos_cnt;
	u8 rn_neg_cnt;
	u8 rn_pos_sum;
	u8 rn_neg_sum;
	u8 rn_stable;
	u8 track_bit[10];
	u8 rn_max_tobj[12];
	u8 palm[8];
	u8 noise_detect[8];
	u8 reserved_2[21];
	u32 ic_debug[3];
	u32 ic_debug_info;
} __packed;

/* report packet */
struct lg4946_touch_data {
	u8 tool_type:4;
	u8 event:4;
	s8 track_id;
	u16 x;
	u16 y;
	u8 pressure;
	s8 angle;
	u16 width_major;
	u16 width_minor;
} __packed;

struct lg4946_touch_info {
	u32 ic_status;
	u32 device_status;
	u32 wakeup_type:8;
	u32 touch_cnt:5;
	u32 button_cnt:3;
	u32 palm_bit:16;
	struct lg4946_touch_data data[10];
	/* debug info */
	struct lg4946_touch_debug debug;
} __packed;

#define PALM_ID				15

/* device control */
#define tc_version				(0x242)
#define tc_product_id1			(0x244)
#define tc_product_id2			(0x245)
#define tc_ic_status			(0x200) /* lg4946_touch_info base addr*/
#define tc_status				(0x201)

#define spr_subdisp_st			(0x021)
#define spr_boot_st				(0x011)

#define tc_device_ctl			(0xC00)
#define tc_interrupt_ctl		(0xC01)
#define tc_interrupt_status		(0xC02)
#define tc_drive_ctl			(0xC03)

#define rtc_te_interval_cnt		(0x275)
#define info_fpc_type			(0x278)
#define info_wfr_type			(0x27B)
#define info_chip_revision		(0x27C)
#define info_cg_type			(0x27D)
#define info_lot_num			(0x27E)
#define info_serial_num			(0x27F)
#define info_date				(0x280)
#define info_time				(0x281)

#define TCI_ENABLE_W			(0xC20)
#define TAP_COUNT_W				(0xC21)
#define MIN_INTERTAP_W			(0xC22)
#define MAX_INTERTAP_W			(0xC23)
#define TOUCH_SLOP_W			(0xC24)
#define TAP_DISTANCE_W			(0xC25)
#define INT_DELAY_W				(0xC26)
#define ACT_AREA_X1_W			(0xC27)
#define ACT_AREA_Y1_W			(0xC28)
#define ACT_AREA_X2_W			(0xC29)
#define ACT_AREA_Y2_W			(0xC2A)

#define SWIPE_ENABLE_W			(0xC30)
#define SWIPE_DIST_W			(0xC31)
#define SWIPE_RATIO_THR_W		(0xC32)
#define SWIPE_RATIO_DIST_W		(0xC33)
#define SWIPE_RATIO_PERIOD_W	(0xC34)
#define SWIPE_TIME_MIN_W		(0xC35)
#define SWIPE_TIME_MAX_W		(0xC36)
#define SWIPE_ACT_AREA_X1_W		(0xC37)
#define SWIPE_ACT_AREA_Y1_W		(0xC38)
#define SWIPE_ACT_AREA_X2_W		(0xC39)
#define SWIPE_ACT_AREA_Y2_W		(0xC3A)

#define TCI_FAIL_DEBUG_R		(0x28D)
#define TCI_FAIL_BIT_R			(0x28E)
#define TCI_FAIL_DEBUG_W		(0xC2C)
#define TCI_FAIL_BIT_W			(0xC2D)
#define SWIPE_FAIL_DEBUG_R		(0x29D)
#define SWIPE_FAIL_DEBUG_W		(0xC3D)
#define TCI_DEBUG_R				(0x2AE)
#define SWIPE_DEBUG_R			(0x2B6)
#define QCOVER_SENSITIVITY		(0xC4F)
#define REG_IME_STATE			(0xC51)
#define REG_CALL_STATE			(0xC54)
#define R_HEADER_SIZE			(4)
#define W_HEADER_SIZE			(2)

#define CMD_DIS			0xAA
#define CMD_ENA			0xAB
#define CMD_CLK_ON		0x83
#define CMD_CLK_OFF		0x82
#define CMD_OSC_ON		0x81
#define CMD_OSC_OFF		0x80
#define CMD_RESET_LOW	0x84
#define CMD_RESET_HIGH	0x85

#define PRODUCTION_INFO_W	0xC04
#define PRODUCTION_INFO_R	0x265

enum {
	REVISION_0 = 0,
	REVISION_1 = 1,
	REVISION_2 = 2,
	REVISION_ERASED = 0xFF,
};

#define REVISION_FINAL	REVISION_2

/* charger status */
#define SPR_CHARGER_STS			(0xC50)

#define CONNECT_NONE			(0x00)
#define CONNECT_USB			(0x01)
#define CONNECT_TA			(0x02)
#define CONNECT_OTG			(0x03)
#define CONNECT_WIRELESS		(0x10)

enum {
	SW_RESET = 0,
	HW_RESET,
};

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
	SWIPE_DOWN,
	SWIPE_UP,
	CUSTOM_DEBUG = 200,
	KNOCK_OVERTAP = 201,
};

enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};

enum {
	SWIPE_D = 0,
	SWIPE_U,
};

enum {
	SWIPE_DOWN_BIT	= 1,
	SWIPE_UP_BIT	= 1 << 16,
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

enum {
	IC_INIT_NEED = 0,
	IC_INIT_DONE,
};

enum {
	LOG_WRITE_DONE = 0,
	DO_WRITE_LOG,
};

/* SPR control */

/* Firmware control */
#define spr_rst_ctl			(0x006)
#define spr_boot_ctl		(0x00F)
#define spr_sram_ctl		(0x010)
#define spr_code_offset		(0x078)
#define spr_data_offset		(0x07B)
#define tc_flash_dn_ctl		(0xC05)
#define tc_flash_dn_sts		(0x247)
#define tc_confdn_base_addr	(0x2F9)
#define code_access_addr	(0x300)
#define data_access_addr	(0x301)

#define MAX_RW_SIZE		(60 * 1024)
#define FLASH_FW_SIZE			(72 * 1024)
#define FLASH_CONF_SIZE			(1 * 1024)

#define FLASH_KEY_CODE_CMD		0xDFC1
#define FLASH_KEY_CONF_CMD		0xE87B
#define FLASH_BOOTCHK_VALUE		0x0A0A0000
#define FLASH_CODE_DNCHK_VALUE	0x42
#define FLASH_CONF_DNCHK_VALUE	0x84

#define TC_DRIVING_TIMEOUT_MS	300

struct lg4946_version {
	u8 build : 4;
	u8 major : 4;
	u8 minor;
};

struct lg4946_ic_info {
	struct lg4946_version version;
	u8 product_id[8];
	u8 image_version[2];
	u8 image_product_id[8];
	u8 revision;
	u32 wfr;
	u32 cg;
	u32 fpc;
	u32 date[2];
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
	struct swipe_info info[2]; /* down is 0, up 1 - LG4946 use up */
};

struct lg4946_data {
	struct device *dev;
	struct kobject kobj;
	struct lg4946_touch_info info;
	struct lg4946_ic_info ic_info;
	struct lg4946_asc_info asc;	/* ASC */
	struct workqueue_struct *wq_log;
	u8 lcd_mode;
	u8 prev_lcd_mode;
	u8 driving_mode;
	u8 u3fake;
	struct watch_data watch;
	struct swipe_ctrl swipe;
	struct mutex spi_lock;
	struct delayed_work font_download_work;
	struct delayed_work fb_notify_work;
	struct delayed_work debug_info_work;
	struct delayed_work te_test_work;
	struct delayed_work reset_work;
	u32 reset_work_cnt;
	u32 charger;
	u32 earjack;
	u32 frame_cnt;
	u8 tci_debug_type;
	u8 swipe_debug_type;
	atomic_t block_watch_cfg;
	atomic_t init;
	struct pm_qos_request pm_qos_req;
	u32 q_sensitivity;
	char te_test_log[64];
	int te_ret;
	u8 te_write_log;
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

static inline struct lg4946_data *to_lg4946_data(struct device *dev)
{
	return (struct lg4946_data *)touch_get_device(to_touch_core(dev));
}

static inline struct lg4946_data *to_lg4946_data_from_kobj(struct kobject *kobj)
{
	return (struct lg4946_data *)container_of(kobj,
			struct lg4946_data, kobj);
}

int lg4946_init(struct device *dev);
int lg4946_reg_read(struct device *dev, u16 addr, void *data, int size);
int lg4946_reg_write(struct device *dev, u16 addr, void *data, int size);
int lg4946_ic_info(struct device *dev);
int lg4946_te_info(struct device *dev, char *buf);
int lg4946_tc_driving(struct device *dev, int mode);
int lg4946_irq_abs(struct device *dev);
int lg4946_irq_abs_data(struct device *dev);
int lg4946_irq_lpwg(struct device *dev);
int lg4946_irq_handler(struct device *dev);
int lg4946_check_status(struct device *dev);
int lg4946_debug_info(struct device *dev, int mode);

static inline int lg4946_read_value(struct device *dev,
					u16 addr, u32 *value)
{
	return lg4946_reg_read(dev, addr, value, sizeof(*value));
}

static inline int lg4946_write_value(struct device *dev,
					 u16 addr, u32 value)
{
	return lg4946_reg_write(dev, addr, &value, sizeof(value));
}

#endif /* LGE_TOUCH_LG4946_H */
