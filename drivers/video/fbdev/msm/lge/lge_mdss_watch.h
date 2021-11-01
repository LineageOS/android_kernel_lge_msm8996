/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef OEM_MDSS_WATCH_H
#define OEM_MDSS_WATCH_H

#include <linux/of_device.h>
#include <linux/module.h>

#define WATCH_LUT_MAX               7

/********************************************************************
* FONT MODE SETTING
*********************************************************************/

struct WatchFontEffectBlinkConfig {
	u32	blink_type; /* 0:blink disable, 1:500ms, 2:1sec, 3:2sec. */
	u32	bstartx; /* blink startx.watstartx<=bstartx, bstartx<=bendx */
	u32	bendx; /* blink end position. bendx <= watendx */
};

struct WatchFontEffectConfig {
	u32	len;
	u32	watchon;	/* 0:watch off, 1:watch on */
	u32	h24_en;		/* 0:12 hour display, 1:24 hour display */
	u32	zero_disp;	/* 0:display off, 1:display on */
	u32	clock_disp_type; /* 0:hour and min, 1:min and sec */
	u32	midnight_hour_zero_en; /* 0: 12:00 mode, 1: 00:00 */
	struct WatchFontEffectBlinkConfig	blink;	/*for blink effect */
};

struct WatchFontLUTConfig {
	/* LUT */
	u32	RGB_blue;
	u32	RGB_green;
	u32	RGB_red;
};

struct WatchFontPropertyConfig {
	u32	len;
	u32	max_num;		/* The number of LUT */
	struct WatchFontLUTConfig	LUT[WATCH_LUT_MAX];
};

struct WatchFontPostionConfig {
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
	u32 d_fontx;
	u32 d_fonty;
	u32 d_font_colonx;
};

struct WatchFontAnalogPositionConfig {
	u32	a_watstartx;
	u32	a_watstarty;
	u32 a_watchcenterx;
	u32 a_watchcentery;
	u32 a_fontx;
	u32 a_fonty;
	u32 a_font_centerx;
	u32 a_font_centery;
};

struct WatchTimeSyncConfig {/* to sync with AP's current time */
	u32	len;
	u32	rtc_cwhour;	/* for hour */
	u32	rtc_cwmin;	/* for min */
	u32	rtc_cwsec;	/* for sec */
	u32	rtc_cwmilli;	/* for millisecond */
};

struct WatchFontTypeSliceInfoConfig {
	u32 font_type;
	u32 slice_idx;
	u32 slice_cnt;
	u32 font_crc_code;
	u32 total_font_size;
	u32 alpha_on;
};


#define WATCH_ON				1
#define WATCH_OFF				0

#define WATCH_RTC_START			1
#define WATCH_RTC_STOP			2
#define WATCH_RTC_UPDATE			3
#define WATCH_CLEAR_RTC_UPDATE		4

#define WATCH_LUT_NUM 			7

struct watch_rtc_ctrl_bits {
	u8 rtc_en:1;
	u8 rtc_stop:1;
	u8 rtc_update:1;
	u8 reserved0:3;
	u8 rtc_interval:2;
}__packed;

struct watch_rtc_time_bits {
	u8 rtc_hour:5;
	u8 reserved0:3;
	u8 rtc_min:6;
	u8 reserved1:2;
	u8 rtc_sec:6;
	u8 reserved2:2;
	u16 rtc_undersec;
}__packed;

//RTC_SET - 90h
struct watch_rtc_set_cfg {
	struct watch_rtc_ctrl_bits rtc_ctrl; //p1
	struct watch_rtc_time_bits rtc_cur_time; //p2 ~ p6
	u16 rtc_make_change_time; // p7 ~ p8
	u16 rtc_clk_freq; // p9 ~ p10
}__packed;

//RTC_INFO - 91h
struct watch_rtc_info_cfg {
	struct watch_rtc_time_bits rtc_time; // p1 ~ p5
}__packed;

//WATCH_CTL - 92h
struct watch_ctl_cfg {
	u8 watch_en:2;
	u8 afont_set:2;
	u8 dwch_blink_period:3;
	u8 dwch_blink_inv:1;  // ~ p1
	u8 dwch_z_disp_m:1;
	u8 dwch_z_disp_h:1;
	u8 dwch_24h:1;
	u8 dwch_ms:1;
	u8 dwch_24_00:1;
	u8 update_u2_enter:1;
	u8 alpha_on:1;
	u8 awch_sec_off:1; // ~ p2
	u8 btm_reset:1;
	u8 fix_black:1;
	u8 fix_color_en:1;
	u8 reserved0:5; // ~p3
}__packed;

//WATCH_SET - 93h

struct watch_x_y_pos_bits { // start position x,y -> 3πŸ¿Ã∆Æ
	u32	start_x:12;
	u32	start_y:12;
}__packed;

struct watch_x_y_size_bits { // start position x,y
	u32	size_x:12;
	u32	size_y:12;
}__packed;

struct watch_set_cfg {
	struct watch_x_y_pos_bits btm_pos;
	struct watch_x_y_size_bits awch_size;
	struct watch_x_y_pos_bits awch_pos;
	struct watch_x_y_pos_bits awch_center;
	struct watch_x_y_size_bits dwch_size;
	struct watch_x_y_pos_bits dwch_pos;
	u32 dwch_blink_start_x:12;
	u32 dwch_blink_size_x:12;
	u32 dwch_h_10_pos_x:12;
	u32 dwch_h_1_pos_x:12;
	u32 dwch_m_10_pos_x:12;
	u32 dwch_m_1_pos_x:12;
	u32 dwch_colon_pos_x:12;
	u32 aod_pos_x:12;
}__packed;

//FD_CTL - 94h

struct watch_fd_ctl_cfg {
	u8 aod_fd:1;
	u8 reserved0:3;
	u8 fdkfblk:1;
	u8 fdfblk:1;
	u8 reserved1:2; // ~ p1
	u8 fdsidx:5;
	u8 fderrfix:1;
	u8 fdscnt:1;
	u8 fdwget:1; // ~p2
}__packed;

//FONT_SET - 95h

struct watch_font_lut_bits {
	u8 r;
	u8 g;
	u8 b;
}__packed;

struct watch_font_set_cfg {
	u8 font_size_digit_x;
	u8 font_size_digit_y;
	u8 font_size_colon_x;
	u16 font_size_analog_x:6;
	u16 font_size_analog_y:10;
	u16 font_analog_center_x:5;
	u16 font_analog_center_y:9;
	u16 reserved0:2;
	struct watch_font_lut_bits lut[WATCH_LUT_NUM];
	struct watch_font_lut_bits alpha;
}__packed;

//U2_SCR_FAD - 98h
struct watch_u2_scr_fad_cfg {
	u8 vsen:1;
	u8 fade_en:1;
	u8 reserved0:2;
	u8 vsks:1;
	u8 vsmode:1;
	u8 reserved1:1;
	u8 vsposfix:1; // ~p1
	u16 vsli:6;
	u16 vsfi:10; // ~p3
	u32 vslb:12;
	u32 vsub:12;
	u32 fasd_step:3;
	u32 reserved2:5;
	u8 fade_speed;
	u8 fade_peak_time;
}__packed;

//FONT_CRC - 99h

struct watch_font_crc_cfg {
	u8 crc_en:1;
	u8 crc_clear:1;
	u8 crc_end:1;
	u8 crc_fail:1;
	u8 reserved0:4;
	u16 total_size:14;
	u16 reserved1:2;
	u16 crc_code;
	u16 crc_result;
}__packed;

//TCH_FIRMWR - D9h

struct watch_tch_firmwr_cfg {
	u8 reserved0:1;
	u8 font_wren:1;
	u8 reserved1:2;
	u8 byteswap:3;
	u8 reserved2:1;
	u8 reserved3;
	u8 fontwr_cmd;
}__packed;

struct watch_cfg {
	struct watch_rtc_set_cfg time; 				// RTC_SET - 90h
	struct watch_rtc_info_cfg dic_time;			// RTC_INFO - 91h
	struct watch_ctl_cfg mode;				// WATCH_CTL - 92h
	struct watch_set_cfg position; 				// WATCH_SET - 93h
	struct watch_fd_ctl_cfg font_ctl;			//FD_CTL - 94h
	struct watch_font_set_cfg font;				//FONT_SET - 95h
	struct watch_u2_scr_fad_cfg u2_scr_fad;	//U2SCR_FAD - 98h
	struct watch_font_crc_cfg font_crc;			//FONT_CRC - 99h
};

enum {
	RTC_SET = 0x90,			//144
	RTC_INFO,				//145
	WATCH_CTL,				//146
	WATCH_SET,				//147
	FD_CTL,					//148
	FONT_SET,				//149
	U2_SCR_FAD = 0x98,		//152
	FONT_CRC,				//153
	TCH_FIRMWR = 0xD9,
};


enum {
	FONT_NONE,
	FONT_ANALOG,
	FONT_DIGITAL,
	FONT_MINI
};

enum {
	FONT_STATE_NONE,
	FONT_LAYER_REQUESTED,
	FONT_DOWNLOAD_PROCESSING,
	FONT_DOWNLOAD_COMPLETE
};

struct watch_data {
	unsigned int requested_font_type;
	unsigned int current_font_type;
	bool hw_clock_user_state;
	unsigned int font_download_state;
	struct watch_cfg wdata;
	unsigned int set_roi;
#if defined(CONFIG_LGE_LCD_TUNING)
	unsigned int font_type_reset;
#endif
};

/* Default Clock/Font position and Size */
//Bitmap Pos
#define BTM_POS_X 0
#define BTM_POS_Y 0

//Awatch Size
#define AWATCH_SIZE_X 600
#define AWATCH_SIZE_Y 600

//Awatch Pos
#define AWATCH_POS_X 0
#define AWATCH_POS_Y 0

//Awatch Cen
#define AWATCH_CEN_X 300
#define AWATCH_CEN_Y 300

//Dwatch Size
#define DWATCH_SIZE_X 592
#define DWATCH_SIZE_Y 184

//Dwatch Pos
#define DWATCH_POS_X 42
#define DWATCH_POS_Y 0

//Dwatch Blinck
#define DWATCH_BLK_POS_X 232
#define DWATCH_BLK_SIZE 48

//Dwatch H Pos
#define DWATCH_H_10_POS_X 0
#define DWATCH_H_1_POS_X 116

//Dwatch H Pos
#define DWATCH_M_10_POS_X 280
#define DWATCH_M_1_POS_X 396

//Dwatch Colon
#define DWATCH_COLON_POS_X 232

//Miniwatch Size
#define MWATCH_SIZE_X 240
#define MWATCH_SIZE_Y 75

//AOD Pos
#define AOD_POS_X 420

#define FONT_D_X 116
#define FONT_D_Y 184
#define FONT_D_COLON 116

#define FONT_A_X 39
#define FONT_A_Y 330
#define FONT_A_C_X 19
#define FONT_A_C_Y 270

#endif
