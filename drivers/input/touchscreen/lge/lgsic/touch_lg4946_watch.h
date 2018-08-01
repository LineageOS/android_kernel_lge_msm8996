/* touch_lg4946_watch.h
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

#ifndef LGE_TOUCH_LG4946_WATCH_H
#define LGE_TOUCH_LG4946_WATCH_H

#define LGE_EXT_WATCH_NAME		"ext_watch"
#define MAX_FONT_SIZE		(87 * 1024) // 88,960 bytes will be used

/*******************************************************************
* Defines
********************************************************************/
#define EXT_WATCH_LUT_MAX               7

enum {
	NOT_SUPPORT = 0,
	SUPPORT,
};

enum {
	UNBLOCKED = 0,
	BLOCKED,
};


/******************************************************************
* CAPABILITY QUERY
*******************************************************************/
struct __packed ExtWatchFontDataQuery {
	bool Font_supported;	/* 0:not supported, 1:supported */
	u8 max_font_x_size;	/* 1~9 number X max size. */
	u8 max_font_y_size;	/* 1~9 number Y max size. */
	u8 max_cln_x_size;	/* ":" X max size. (ex. 23:47) */
	u8 max_cln_y_size;	/* ":" Y max size. (ex. 23:47) */
};

struct ExtWatchFontPositionQuery { /* 0:not supported, 1:supported */
	bool	vertical_position_supported;
	bool	horizontal_position_supported;
};

struct ExtWatchFontTimeQuery { /* 0:not supported, 1:supported */
	bool	h24_supported;
	bool	AmPm_supported;
};

struct __packed ExtWatchFontColorQuery { /* 0:not supported, 1:supported */
	u8	max_num;		/* The number of LUT */
	bool	LUT_supported;
	bool	alpha_supported;
	bool	gradation_supported;
};

struct __packed ExtWatchFontEffectQuery {
	bool	zero_supported;	/* 0:display off, 1:display on */
	u8	blink_type;	/* 0:blink disable, 1:500ms, 2:1sec, 3:2sec. */
};

/********************************************************************
* FONT MODE SETTING
*********************************************************************/

struct ExtWatchFontEffectBlinkConfig {
	u32	blink_type; /* 0:blink disable, 1:500ms, 2:1sec, 3:2sec. */
	u32	bstartx; /* blink startx.watstartx<=bstartx, bstartx<=bendx */
	u32	bendx; /* blink end position. bendx <= watendx */
};

struct ExtWatchFontEffectConfig {
	u32	len;
	u32	watchon;	/* 0:watch off, 1:watch on */
	u32	h24_en;		/* 0:12 hour display, 1:24 hour display */
	u32	zero_disp;	/* 0:display off, 1:display on */
	u32	clock_disp_type; /* 0:hour and min, 1:min and sec */
	u32	midnight_hour_zero_en; /* 0: 12:00 mode, 1: 00:00 */
	struct ExtWatchFontEffectBlinkConfig	blink;	/*for blink effect */
};

struct ExtWatchFontLUTConfig {
	/* LUT */
	u32	RGB_blue;
	u32	RGB_green;
	u32	RGB_red;
};

struct ExtWatchFontPropertyConfig {
	u32	len;
	u32	max_num;		/* The number of LUT */
	struct ExtWatchFontLUTConfig	LUT[EXT_WATCH_LUT_MAX];
};

struct ExtWatchFontPostionConfig {
	u32	len;
	u32	watstartx;	/* 420 <= watstartx <= 1020 */
	u32	watendx;	/* watch end positon. watendx <= 1020 */
	u32	watstarty;	/* 0 <= watstarty <= 680 */
	u32	watendy;	/* watch end positon. watendy <= 680*/
	u32	h1x_pos;	/* 1 ~ 9hour position */
	u32	h10x_pos;	/* 10, 20 hour position */
	u32	m1x_pos;	/* 1 ~ 9min position */
	u32	m10x_pos;	/* 10 ~ 50 min position */
	u32	clx_pos;	/* 1 ~ 60 second position */
};

struct ExtWatchTimeSyncConfig {/* to sync with AP's current time */
	u32	len;
	u32	rtc_cwhour;	/* for hour */
	u32	rtc_cwmin;	/* for min */
	u32	rtc_cwsec;	/* for sec */
	u32	rtc_cwmilli;	/* for millisecond */
};
struct ExtWatchFontDataConfig {
	u8	*Data;		/* Font Data (53120 bytes) */
};

#define EXT_WATCH_FONT_OFFSET		(0x07F)
#define EXT_WATCH_FONT_ADDR			(0x307)
#define EXT_WATCH_FONT_DN_ADDR_INFO	(0x2F7)
#define EXT_WATCH_FONT_CRC	0xC18
#define EXT_WATCH_DCS_CTRL	0xC19
#define EXT_WATCH_MEM_CTRL	0xC1A

#define EXT_WATCH_CTRL		0x2D2
#define EXT_WATCH_AREA_X	0x2D3
#define EXT_WATCH_AREA_Y	0x2D4
#define EXT_WATCH_BLINK_AREA 	0x2D5
#define EXT_WATCH_LUT		0x2D6

#define EXT_WATCH_DISPLAY_ON	0xC1B /* Watch display off :0, on :1*/
#define EXT_WATCH_DISPLAY_STATUS	0x039

#define EXT_WATCH_RTC_SCT	0x081	/* Synchronous current time */
/* Synchronous current time for milesec*/
#define EXT_WATCH_RTC_SCTCNT	0x082
/* Target time for occurring date change int */
#define EXT_WATCH_RTC_CAPTURE	0x084	/* Current time capture*/
#define EXT_WATCH_RTC_CTST	0x087	/* Current time */
#define EXT_WATCH_RTC_ECNT	0x088	/* end count */
#define EXT_WATCH_HOUR_DISP	0xC14	/* 0 :zerodisp, 1:h24en, 2:dispmode */
#define EXT_WATCH_BLINK_PRD	0xC15	/* Stop :0x00, 1s :0x01, 1.5s :0x10 */
#define EXT_WATCH_RTC_RUN	0xC10	/* Watch RTC Stop :0x10, Start 0x01 */

#define EXT_WATCH_POSITION	0xC11	/*Write only*/
#define EXT_WATCH_POSITION_R	0x271	/*Read only*/
#define EXT_WATCH_STATE		0x270 	/*Watch state, Read only*/

#define EXT_WATCH_LUT_NUM	7
#define SYS_DISPMODE_STATUS	0x021	/* DIC status */

#define EXT_WATCH_ON			1
#define EXT_WATCH_OFF			0

#define EXT_WATCH_RTC_START		1
#define EXT_WATCH_RTC_STOP		2

struct __packed ext_watch_ctrl_bits {	/* 0x2D2 */
	u32	reserved0:1;
	u32	dispmode:1;	/* 0:U2, 1:AnyMode */
	u32	reserved1:5;
	u32	alpha:9;
};

struct __packed ext_watch_area_bits { /* 0x2D3, 0x2D4 */
	u32	watstart:12;
	u32	watend:12;
};

struct __packed ext_watch_blink_area_bits { /* 0x2D5 */
	u32 bstartx:12;
	u32 bendx:12;
};

struct __packed  ext_watch_lut_bits {	/* 0x2D6 */
	u8 b;
	u8 g;
	u8 r;
	u8 reserved0;
};

struct __packed ext_watch_time_bits {
	u32 hour:5;
	u32 min:6;
	u32 sec:6;
	u32 reserved0:15;
};

struct __packed ext_watch_mode_cfg {	/* 36 bytes */
	struct ext_watch_ctrl_bits watch_ctrl;		/* 2 bytes */
	struct ext_watch_area_bits watch_area_x;		/* 3 bytes */
	struct ext_watch_area_bits watch_area_y;		/* 3 bytes */
	struct ext_watch_blink_area_bits blink_area;	/* 3 bytes */
	struct ext_watch_lut_bits lut[EXT_WATCH_LUT_NUM];	/* 21 bytes */
};

struct __packed ext_watch_time_cfg {	/* 36 bytes */
	u32 disp_waton;		/* 0xC10 watch display off:0, on:1*/
	struct ext_watch_time_bits rtc_sct;	/* 0x081 */
	u32 rtc_sctcnt;						/* 0x082 */
	u32 rtc_mit;							/* 0x083 */
	u32 rtc_capture;					/* 0x084 */
	struct ext_watch_time_bits rtc_ctst;	/* 0x087 */
	u32 rtc_ecnt;						/* 0x088 */
};

struct __packed ext_watch_position_cfg {	/* 0xC11 */
	u32 h10x_pos:9;
	u32 h1x_pos:9;
	u32 reserved0:14;
	u32 m10x_pos:9;
	u32 m1x_pos:9;
	u32 reserved1:14;
	u32 clx_pos:9;
	u32 reserved2:23;
	u32 zero_disp:1;
	u32 h24_en:1;
	u32 clock_disp_mode:1;
	u32 midnight_hour_zero_en:1;
	u32 reserved3:28;
	u32 bhprd:3;
	u32 reserved4:29;
	u32 num_width:8;
	u32 colon_width:8;
	u32 height:8;
	u32 reserved5:8;
};

struct __packed ext_watch_status_cfg {	/* 0x270*/
	u32 step:3;
	u32 en:1;
	u32 en_24:1;
	u32 zero_en:1;
	u32 disp_mode:1;
	u32 bhprd:3;
	u32 cur_hour:5;
	u32 cur_min:6;
	u32 cur_sec:6;
	u32 midnight_hour_zero_en:1;
	u32 reserved0:4;
};

struct ext_watch_font_header {
	u32 magic_code;
	u8 width_num;
	u8 width_colon;
	u8 height;
	u8 font_id;
	u32 size;
};

struct ext_watch_cfg {
	u8 *font_data;
	u32 font_crc;
	struct ext_watch_mode_cfg mode;
	struct ext_watch_time_cfg time;
	struct ext_watch_position_cfg position;
};

enum {
	FONT_EMPTY = 0,
	FONT_DOWNLOADING,
	FONT_READY,

};

enum {
	RTC_CLEAR = 0,
	RTC_RUN,
};

struct watch_state_info {
	atomic_t font_status;
	atomic_t rtc_status;
};

struct watch_data {
	struct watch_state_info state;
	struct bin_attribute fontdata_attr;
	u32 font_written_size;
	struct ext_watch_cfg ext_wdata;
};

struct watch_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, char *buf);
	ssize_t (*store)(struct device *idev, const char *buf, size_t count);
};

#define WATCH_ATTR(_name, _show, _store)		\
	struct watch_attribute watch_attr_##_name	\
	= __ATTR(_name, S_IRUGO | S_IWUSR, _show, _store)

extern int lg4946_watch_register_sysfs(struct device *dev);
extern int lg4946_xfer_msg(struct device *dev, struct touch_xfer_msg *xfer);
extern void lg4946_xfer_msg_ready(struct device *dev, u8 msg_cnt);
extern int lg4946_reg_read(struct device *dev, u16 addr, void *data, int size);
extern int lg4946_reg_write(struct device *dev, u16 addr, void *data, int size);
extern int lg4946_check_font_status(struct device *dev);
void lg4946_font_download(struct work_struct *font_download_work);
extern void lg4946_watch_remove(struct device *dev);
extern int lg4946_watch_init(struct device *dev);
extern int ext_watch_get_current_time(struct device *dev, char *buf, int *len);
extern void lg4946_watch_display_off(struct device *dev);

#endif
