/* touch_lg4945_watch.h
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

#ifndef LGE_TOUCH_LG4945_WATCH_H
#define LGE_TOUCH_LG4945_WATCH_H

#define LGE_EXT_WATCH_NAME		"ext_watch"
#define MAX_WATCH_DATA_SIZE		(64 * 1024)
#define FONT_DATA_SIZE			(13280 * 4)
#define COMP_FONTM_MAX_SIZE		(4096)
#define COMP_FONT_TRANFER_CODE	(0x49455494U)

/*******************************************************************
* Defines
********************************************************************/
#define EXT_WATCH_LUT_MAX               7

enum {
	NOT_SUPPORT = 0,
	SUPPORT,
};
enum {
	E_COMP_1_NUM = 0x80,
	E_COMP_2_NUM = 0x08,
	E_COMP_255_NUM = 0x88
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
	u32	watstartx;	/* 400 <= watstartx, watstartx <= watendx */
	u32	watendx;	/* watch end positon. watendx <= 1440 */
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

/* 0 :disable, 1 :enable */
#define EXT_WATCH_FONT_ACC_EN	0xC010
/* 4'h 0~9:font '0'~'9', 4'hA :font ':' */
/* 0~9:D800-DCFF, A(:):D800-D9DF */
#define EXT_WATCH_FONT_SEL	0xD07A
/* Font memory for compressed data */
#define EXT_WATCH_FONT_COMP_ADDR	(0x8000u) /* 0x8C00 */
#define EXT_WATCH_FONT_OFFSET_ADDR	0xD800

#define EXT_WATCH_CTRL		0x8A00
#define EXT_WATCH_AREA		0x8A02
#define EXT_WATCH_BLINK_AREA	0x8A05
#define EXT_WATCH_GRAD		0x8A08
#define EXT_WATCH_LUT		0x8A0E

#define EXT_WATCH_RTC_CTRL	0xD027	/* Watch display off :0, on :1*/
#define EXT_WATCH_RTC_SCT	0xD08B	/* Synchronous current time */
/* Synchronous current time for milesec*/
#define EXT_WATCH_RTC_SCTCNT	0xD08C
/* Target time for occurring date change int */
#define EXT_WATCH_RTC_DIT	0xD08E
#define EXT_WATCH_RTC_CTST	0xD092	/* Current time */
#define EXT_WATCH_RTC_ECNT	0xD093	/* end count */
#define EXT_WATCH_HOUR_DISP	0xC014	/* 0 :zerodisp, 1:h24en, 2:dispmode */
#define EXT_WATCH_BLINK_PRD	0xC015	/* Stop :0x00, 1s :0x01, 1.5s :0x10 */
#define EXT_WATCH_RTC_RUN	0xC016	/* Watch RTC Stop :0x10, Start 0x01 */

#define EXT_WATCH_POSITION	0xC011	/*Write only*/
#define EXT_WATCH_POSITION_R	0x8A48	/*Read only*/
#define EXT_WATCH_SATATE	0x8A47	/*Watch state, Read only*/

#define EXT_WATCH_LUT_NUM	7
#define EXT_WATCH_FONT_NUM_SIZE	0x500	/* 1280	bytes per each '0' ~ '9' */
#define EXT_WATCH_FONT_CHAR_SIZE	0x1E0	/* 480 bytes for ':' */
#define SYS_DISPMODE_STATUS	0xD015	/* DIC status */


#define EXT_WATCH_ON			1
#define EXT_WATCH_OFF			0

#define EXT_WATCH_RTC_START		1
#define EXT_WATCH_RTC_STOP		2

struct __packed ext_watch_ctrl_bits {	/* 0xF9C0 */
	u32	dispmode:1;	/* 0:Alpha blending mode, 1:Gradation mode */
	u32	reserved0:3;
	u32	grad_mode:1;
	u32	reserved1:2;
	u32	alpha:9;
};

struct __packed ext_watch_area_bits { /* 0xF9C2 */
	u32	watstartx:11;
	u32	reserved0:1;
	u32	watendx:11;
	u32	reserved1:1;
};

struct __packed ext_watch_blink_area_bits { /* 0xF9C5 */
	u32 bstartx:11;
	u32	reserved0:1;
	u32 bendx:11;
	u32	reserved1:1;
};

struct __packed ext_watch_grad_bits {	/* 0xF9C8 */
	u32	grad_l:24;
	u32	reserved0:8;
	u32	grad_r:24;
	u32	reserved1:8;
};

struct __packed  ext_watch_lut_bits {	/* 0xF9E0 */
	u32 b;
	u32 g;
	u32 r;
};

struct __packed ext_watch_time_bits {
	u32 hour:5;
	u32 min:6;
	u32 sec:6;
	u32 reserved0:15;
};

struct __packed ext_watch_mode_cfg {	/* 36 bytes */
	u32 mcs_ctrl;					/* 1 bytes */
	struct ext_watch_ctrl_bits watch_ctrl;		/* 2 bytes */
	struct ext_watch_area_bits watch_area;		/* 3 bytes */
	struct ext_watch_blink_area_bits blink_area;	/* 3 bytes */
	struct ext_watch_grad_bits grad;		/* 6 bytes */
	struct ext_watch_lut_bits lut[EXT_WATCH_LUT_NUM];	/* 21 bytes */
};

struct __packed ext_watch_time_cfg {	/* 36 bytes */
	u32 disp_waton;		/* 0xD027 watch display off:0, on:1*/
	/* 0xD08B Synchronous current time */
	struct ext_watch_time_bits rtc_sct;
	/* 0xD08C Synchronous current time for millisecound */
	u32 rtc_sctcnt;
	u32 reserved0:16;
	/* 0xD08E Target time for occurring date change interrupt */
	struct ext_watch_time_bits rtc_dit;
	struct ext_watch_time_bits rtc_ctst;	/* 0xD092 Current time */
	u32 rtc_ecnt:16;	/* 0xD093 end count */
	u32 reserved:16;
};

struct __packed ext_watch_position_cfg {	/* 0xC011 20 bytes W-only*/
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
	u32 bhprd:2;
	u32 reserved4:30;
};

struct __packed ext_watch_status_cfg {	/* 0x8A47 4 bytes R-only*/
	u32 step:3;
	u32 en:1;
	u32 en_24:1;
	u32 zero_en:1;
	u32 disp_mode:1;
	u32 bhprd:2;
	u32 cur_hour:5;
	u32 cur_min:6;
	u32 cur_sec:6;
	u32 midnight_hour_zero_en:1;
	u32 reserved0:5;
};

struct ext_watch_cfg {
	u8 *font_data;
	u8 *comp_buf;
	struct ext_watch_mode_cfg mode;
	struct ext_watch_time_cfg time;
	struct ext_watch_position_cfg position;
};

enum {
	EMPTY = 0,
	DOWN_LOADING,
	DOWN_LOADED,

};
struct watch_state_info {
	atomic_t font_mem;
	atomic_t is_font;
};

struct watch_data {
	struct watch_state_info state;
	struct bin_attribute fontdata_attr;
	u32 fontdata_size;
	u32 font_written_size;
	u32 fontdata_comp_size;
	u32 font_written_comp_size;
	u8 font_width;
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

extern int lg4945_watch_register_sysfs(struct device *dev);
extern int lg4945_xfer_msg(struct device *dev, struct touch_xfer_msg *xfer);
extern int lg4945_reg_read(struct device *dev, u16 addr, void *data, int size);
extern int lg4945_reg_write(struct device *dev, u16 addr, void *data, int size);
extern int lg4945_ext_watch_check_font_download(struct device *dev);
void lg4945_ext_watch_font_download_func(struct work_struct *font_download_work);
extern void lg4945_watch_remove(struct device *dev);
extern int lg4945_watch_init(struct device *dev);


#endif
