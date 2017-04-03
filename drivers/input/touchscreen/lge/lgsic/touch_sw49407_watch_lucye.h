/* touch_sw49407_watch.h
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

#ifndef LGE_TOUCH_SW49407_WATCH_H
#define LGE_TOUCH_SW49407_WATCH_H

#define LGE_EXT_WATCH_NAME		"ext_watch"
#define MAX_FONT_SIZE			 (43840) // 42.8KB bytes will be used

/*******************************************************************
* Defines
********************************************************************/
#define EXT_WATCH_LUT_MAX		7
#define EXT_WATCH_MAX_STARTX	1152

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
__packed struct ExtWatchFontDataQuery {
	bool Font_supported;	/* 0:not supported, 1:supported */
	u8 max_font_x_size;		/* 1~9 number X max size. */
	u8 max_font_y_size;		/* 1~9 number Y max size. */
	u8 max_cln_x_size;		/* ":" X max size. (ex. 23:47) */
	u8 max_cln_y_size;		/* ":" Y max size. (ex. 23:47) */
};

struct ExtWatchFontPositionQuery {	/* 0:not supported, 1:supported */
	bool	vertical_position_supported;
	bool	horizontal_position_supported;
};

struct ExtWatchFontTimeQuery {	/* 0:not supported, 1:supported */
	bool	h24_supported;
	bool	AmPm_supported;
};

__packed struct ExtWatchFontColorQuery {	/* 0:not supported, 1:supported */
	u8	max_num;					/* The number of LUT */
	bool	LUT_supported;
	bool	alpha_supported;
	bool	gradation_supported;
};

__packed struct ExtWatchFontEffectQuery {
	bool	zero_supported;		/* 0:display off, 1:display on */
	u8	blink_type;				/* 0:blink disable, 1:125ms, 2:250ms, 3:500ms
									4:1s, 5:2s, 6:3s, 7:8s*/
};

/********************************************************************
* FONT MODE SETTING
*********************************************************************/

struct ExtWatchFontEffectBlinkConfig {
	u32	blink_type;		/* 0:blink disable, 1:125ms, 2:250ms, 3:500ms, 4: 1s
							5:2s, 6:3s, 7:8s*/
	u32	bstartx;		/* blink startx.watstartx<=bstartx, bstartx<=bendx */
	u32	bendx;			/* blink end position. bendx <= watendx */
};

struct ExtWatchFontEffectConfig {
	u32	len;
	u32	watchon;	/* 0:watch off, 1:watch on */
	u32	h24_en;		/* 0:12 hour display, 1:24 hour display */
	u32	zero_disp;	/* 0:display off, 1:display on */
	u32	clock_disp_type;		/* 0:hour and min, 1:min and sec */
	u32	midnight_hour_zero_en;	/* 0: 12:00 mode, 1: 00:00 */
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
	u32	max_num;	/* The number of LUT */
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

struct ExtWatchTimeSyncConfig {		/* to sync with AP's current time */
	u32	len;
	u32	rtc_cwhour;		/* for hour */
	u32	rtc_cwmin;		/* for min */
	u32	rtc_cwsec;		/* for sec */
	u32	rtc_cwmilli;	/* for millisecond */
};
struct ExtWatchFontDataConfig {
	u8	*Data;
};

/* font download */
#define EXT_WATCH_FONT_OFFSET		0x086	// font_mem_oft
#define EXT_WATCH_FONT_ADDR			0xFD8	// font_mem_data

#define EXT_WATCH_FONT_CRC_TEST		0xC16	// font_crc_test

#define EXT_WATCH_FONT_DN_FLAG		0xC18	// font_dn_flag

/* watch disp and blink area, font lut*/
#define EXT_WATCH_DCST_OFFSET		0x085	// dcst_reg_oft
#define EXT_WATCH_DCST_ADDR			0xFDB	// dcst_reg_data

#define EXT_WATCH_CTRL_DCST_OFT		0x1020
#define EXT_WATCH_LUT_DCST_OFT		0x1040

/* watch display on/off */
#define EXT_WATCH_DISPLAY_ON		0xC17	/* disp_waton: display off :0, on :1*/
#define EXT_WATCH_DISPLAY_STATUS	0x039	/* display status. off :0, on : 1*/

/* watch rtc */
#define EXT_WATCH_RTC_SCTCNT		0x08D	/* rtc_sctcnt */
/* Target time for occurring date change int */
#define EXT_WATCH_RTC_CTST			0x092	/* rtc_ctst: Current time */
#define EXT_WATCH_RTC_ECNT			0x093	/* rtc_ecnt: rtc clk count */
#define EXT_WATCH_RTC_MIT			0x08E	/* rtc_mit: rtc 1 second target cnt */
#define EXT_WATCH_RTC_SCT			0x08C	/* rtc_sct: Synchronous current time */
/* Synchronous current time for milesec*/
#define EXT_WATCH_RTC_RUN			0xC10	/* rtc_en: Watch RTC Stop :0x10, Start 0x01 */

/* watch mode */
#define EXT_WATCH_POSITION			0xC11	/*Write only*/
#define EXT_WATCH_HOUR_DISP			0xC14	/* 0 :zerodisp, 1:h24en, 2:dispmode */

/* 0:blink disable, 1:125ms, 2:250ms, 3:500ms, 4: 1s
5:2s, 6:3s, 7:8s*/
#define EXT_WATCH_BLINK_PRD			0xC15	/*blk_prd*/

/* watch status */
#define EXT_WATCH_STATUS			0x33	/*Watch status, Read only*/
#define EXT_WATCH_POSITION_R		0x34	 /*Read only*/

#define EXT_WATCH_ON			1
#define EXT_WATCH_OFF			0
#define EXT_WATCH_RTC_START		1
#define EXT_WATCH_RTC_STOP		2

__packed struct ext_watch_ctrl_cfg {
	//watch ctrl1
	u8	dispmode:1;
	u8	u2_u3_disp_on:1;
	u8	reserved1_1:2;
	u8	grad_mode:1;
	u8	reserved1_2:2;
	u8	alpha_fg:1;
	u32 reserved1:24;
	//watch ctrl2
	u8	alpha;
	u32	reserved2_1:24;
	//watch ctrl3
	u8	watch_x_start_7to0;
	u32	reserved3_1:24;
	//watch ctrl4
	u8	watch_x_start_11to8:4;
	u8	watch_x_end_3to0:4;
	u32	reserved4:24;
	//watch ctrl5
	u8	watch_x_end_11to4;
	u32	reserved5:24;
	//watch ctrl6
	u8	watch_y_start_7to0;
	u32	reserved6:24;
	//watch ctrl7
	u8	watch_y_start_11to8:4;
	u8	watch_y_end_3to0:4;
	u32	reserved7:24;
	//watch ctrl8
	u8	watch_y_end_11to4;
	u32	reserved8:24;
	//watch ctrl9
	u8	blink_x_start_7to0;
	u32	reserved9:24;
	//watch ctrl10
	u8	blink_x_start_11to8:4;
	u8	blink_x_end_3to0:4;
	u32	reserved10:24;
	//watch ctrl11
	u8	blink_x_end_11to4;
	u32	reserved11:24;
};

__packed struct ext_watch_lut_bits {
	u8  b;
	u32 reserved0:24;
	u8  g;
	u32 reserved1:24;
	u8  r;
	u32 reserved2:24;
};

__packed struct ext_watch_lut_cfg {
	struct ext_watch_lut_bits lut[EXT_WATCH_LUT_MAX];
};

__packed struct ext_watch_time_bits {
	u32 hour:5;
	u32 min:6;
	u32 sec:6;
	u32 reserved0:15;
};

__packed struct ext_watch_time_cfg {
	u32 disp_waton;			/* watch display off:0, on:1*/
	struct ext_watch_time_bits rtc_sct;
	u32 rtc_sctcnt;
	u32 rtc_mit;
	struct ext_watch_time_bits rtc_ctst;
	u32 rtc_ecnt;
};

__packed struct ext_watch_position_cfg {
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

__packed struct ext_watch_status_cfg {
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
	u32 reserved;
};

struct ext_watch_cfg {
	u8 *font_data;
	struct ext_watch_ctrl_cfg mode;
	struct ext_watch_lut_cfg lut;
	struct ext_watch_time_cfg time;
	struct ext_watch_position_cfg position;
};

enum {
	FONT_EMPTY = 0,
	FONT_DOWNLOADING,
	FONT_READY,
	FONT_ERROR,
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

extern int sw49407_watch_register_sysfs(struct device *dev);
extern int sw49407_xfer_msg(struct device *dev, struct touch_xfer_msg *xfer);
extern void sw49407_xfer_msg_ready(struct device *dev, u8 msg_cnt);
extern int sw49407_check_font_status(struct device *dev);
void sw49407_font_download(struct work_struct *font_download_work);
extern void sw49407_watch_remove(struct device *dev);
extern int sw49407_watch_init(struct device *dev);
extern int ext_watch_get_current_time(struct device *dev, char *buf, int *len);
extern void sw49407_watch_display_off(struct device *dev);

#endif
