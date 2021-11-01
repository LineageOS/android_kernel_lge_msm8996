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

#include <linux/delay.h>
#include <soc/qcom/lge/board_lge.h>
#include "lge_mdss_watch.h"
#include "lge_mdss_aod.h"
#include "../mdss_fb.h"
#include "../mdss_dsi.h"
#include "../mdss_panel.h"


extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds, u32 flags);
extern void lge_watch_mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl,
		char cmd0, int cnt, char* ret_buf);
static struct msm_fb_data_type *mfd_base;

#if defined(CONFIG_LGE_LCD_TUNING)
void oem_mdss_watch_reg_print(struct mdss_dsi_ctrl_pdata *ctrl, unsigned int req_cmd)
{
	int i;
	u8 *param;
	int length;

	switch(req_cmd){
		case RTC_SET:
			printk("[Watch] RTC_SET : 90 ");
			param = &ctrl->watch_rtc_set_cmd.cmds->payload[1];
			length = 10;
			break;
		case WATCH_CTL:
			printk("[Watch] WATCH_CTL : 92 ");
			param = &ctrl->watch_ctl_cmd.cmds->payload[1];
			length = 3;
			break;
		case WATCH_SET:
			printk("[Watch] WATCH_SET : 93 ");
			param = &ctrl->watch_set_cmd.cmds->payload[1];
			length = 30;
			break;
		case FD_CTL:
			printk("[Watch] FD_CTL : 94 ");
			param = &ctrl->watch_fd_ctl_cmd.cmds->payload[1];
			length = 2;
			break;
		case FONT_SET:
			printk("[Watch] FONT_SET : 95 ");
			param = &ctrl->watch_font_set_cmd.cmds->payload[1];
			length = 31;
			break;
		case U2_SCR_FAD:
			printk("[Watch] U2_SCR_FAD : 98 ");
			param = &ctrl->watch_u2_scr_fad_cmd.cmds->payload[1];
			length = 9;
			break;
		case FONT_CRC:
			printk("[Watch] FONT_CRC : 99 ");
			param = &ctrl->watch_font_crc_cmd.cmds->payload[1];
			length = 7;
			break;
		case TCH_FIRMWR:
			return;
		default:
			return;
	}
	for(i=0;i<length;i++)
		printk("%02X ", param[i]);
	printk("\n");
}
#endif
static int oem_mdss_watch_reg_write(struct msm_fb_data_type *mfd, unsigned int req_cmd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (!ctrl) {
		pr_err("[Watch] ctrl is null\n");
		return  -EINVAL;
	}

	switch(req_cmd){
		case RTC_SET:
			memcpy(&ctrl->watch_rtc_set_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.time, sizeof(u8)*10);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_rtc_set_cmd, CMD_REQ_COMMIT);
			break;
		case WATCH_CTL:
			memcpy(&ctrl->watch_ctl_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.mode, sizeof(u8)*3);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_ctl_cmd, CMD_REQ_COMMIT);
			break;
		case WATCH_SET:
			memcpy(&ctrl->watch_set_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.position, sizeof(u8)*30);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_set_cmd, CMD_REQ_COMMIT);
			break;
		case FD_CTL:
			memcpy(&ctrl->watch_fd_ctl_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.font_ctl, sizeof(u8)*2);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_fd_ctl_cmd, CMD_REQ_COMMIT);
			break;
		case FONT_SET:
			memcpy(&ctrl->watch_font_set_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.font, sizeof(u8)*31);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_font_set_cmd, CMD_REQ_COMMIT);
			break;
		case U2_SCR_FAD:
			memcpy(&ctrl->watch_u2_scr_fad_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.u2_scr_fad, sizeof(u8)*9);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_u2_scr_fad_cmd, CMD_REQ_COMMIT);
			break;
		case FONT_CRC:
			memcpy(&ctrl->watch_font_crc_cmd.cmds->payload[1], (u8 *)&mfd->watch.wdata.font_crc, sizeof(u8)*7);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_font_crc_cmd, CMD_REQ_COMMIT);
			break;
		case TCH_FIRMWR:
			break;
		default:
			break;
	}
#if defined(CONFIG_LGE_LCD_TUNING)
	oem_mdss_watch_reg_print(ctrl, req_cmd);
#endif
	return 0;
}

static int oem_mdss_watch_reg_read(struct msm_fb_data_type *mfd,
		unsigned int req_cmd, char* ret_buf, int req_cnt)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	char cmd0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (!ctrl) {
		pr_err("[Watch] ctrl is null\n");
		return  -EINVAL;
	}

	cmd0 = req_cmd & 0xFF;
	lge_watch_mdss_dsi_panel_cmd_read(ctrl, cmd0, req_cnt, ret_buf); //cmd0 = register address to read, ret_buf = buffer address for saving read values, req_cnt : count how many values to read

	return 0;
}

void lcd_watch_rtc_start(struct msm_fb_data_type *mfd, u8 start)
{
	if (start == WATCH_RTC_START)
		mfd->watch.wdata.time.rtc_ctrl.rtc_en = 1;
	else if (start == WATCH_RTC_STOP) {
		mfd->watch.wdata.time.rtc_ctrl.rtc_stop = 1;
		mfd->watch.wdata.time.rtc_ctrl.rtc_update = 0;
	}
	else if (start == WATCH_RTC_UPDATE) {
		mfd->watch.wdata.time.rtc_ctrl.rtc_stop = 0;
		mfd->watch.wdata.time.rtc_ctrl.rtc_update = 1;
		mfd->watch.wdata.time.rtc_ctrl.rtc_en = 1;
	}
	else if (start == WATCH_CLEAR_RTC_UPDATE){
		mfd->watch.wdata.time.rtc_ctrl.rtc_update = 0;
	}
	else
		mfd->watch.wdata.time.rtc_ctrl.rtc_en = 0;

	oem_mdss_watch_reg_write(mfd, RTC_SET);
}

void lcd_watch_font_crc_check(struct msm_fb_data_type *mfd)
{
	// font crc pre check
	char rx_buf[7] = {0x0};
	struct watch_font_crc_cfg crc_read;
	int i = 0;

	for(i = 0; i < 10; i++){
		oem_mdss_watch_reg_read(mfd, FONT_CRC, rx_buf, 7);
		memcpy(&crc_read, (u8 *)rx_buf, sizeof(u8)*7);

		if (crc_read.crc_result == 0xFFFF)
			break;
		pr_info("[Watch] %s : crc result code is 0x%x. retry crc clear %d times.\n", __func__, crc_read.crc_result, i+1);
		//WATCH_FONT_CRC_CLEAR
		mfd->watch.wdata.font_crc.crc_en = 0;
		mfd->watch.wdata.font_crc.crc_clear = 1;
		oem_mdss_watch_reg_write(mfd, FONT_CRC);
		pr_info("[Watch] %s : font crc register cleared\n", __func__);

		//WATCH_FONT_CRC_ALL
		mfd->watch.wdata.font_crc.crc_en = 0;
		mfd->watch.wdata.font_crc.crc_clear = 0;
		oem_mdss_watch_reg_write(mfd, FONT_CRC);
		pr_info("[Watch] %s : font crc register clear all\n", __func__);
	}

	// WATCH_FONT_CRC_START
	mfd->watch.wdata.font_crc.crc_en = 1;
	mfd->watch.wdata.font_crc.crc_clear = 0;
	oem_mdss_watch_reg_write(mfd, FONT_CRC);
	pr_info("[Watch] %s : font crc check started\n", __func__);

	mdelay(1);

	//font crc read
	oem_mdss_watch_reg_read(mfd, FONT_CRC, rx_buf, 7);
	memcpy(&crc_read, (u8 *)rx_buf, sizeof(u8)*7);

	if (crc_read.crc_end == 0)
		pr_info("[Watch] %s : font crc check not finished\n",__func__);
	else {
		if (crc_read.crc_fail == 1) {
			pr_info("[Watch] %s : crc check error. expected code : 0x%x, result code : 0x%x\n",
						__func__, crc_read.crc_code, crc_read.crc_result);
			mfd->watch.current_font_type = FONT_NONE;
			mfd->watch.font_download_state = FONT_STATE_NONE;
		}
		else {
			pr_info("[Watch] %s : crc check ok. result code : 0x%x\n",__func__, crc_read.crc_result);
			mfd->watch.current_font_type = mfd->watch.requested_font_type;
			mfd->watch.font_download_state = FONT_DOWNLOAD_COMPLETE;
			mfd->need_to_init_watch = true;
		}
	}

	//WATCH_FONT_CRC_CLEAR
	mfd->watch.wdata.font_crc.crc_en = 0;
	mfd->watch.wdata.font_crc.crc_clear = 1;
	oem_mdss_watch_reg_write(mfd, FONT_CRC);
	pr_info("[Watch] %s : font crc register cleared\n", __func__);

	//WATCH_FONT_CRC_ALL
	mfd->watch.wdata.font_crc.crc_en = 0;
	mfd->watch.wdata.font_crc.crc_clear = 0;
	oem_mdss_watch_reg_write(mfd, FONT_CRC);
	pr_info("[Watch] %s : font crc register clear all\n", __func__);
}


void lcd_watch_set_fontproperty(struct msm_fb_data_type *mfd)
{
	oem_mdss_watch_reg_write(mfd, FONT_SET);
}

void lcd_watch_set_fontposition(struct msm_fb_data_type *mfd)
{
	oem_mdss_watch_reg_write(mfd, WATCH_SET);
}

void lcd_watch_display_onoff(struct msm_fb_data_type *mfd, u8 watch_on)
{
	if (watch_on == WATCH_ON)
		mfd->watch.wdata.mode.watch_en |= 0x1;
	else
		mfd->watch.wdata.mode.watch_en &= 0xE;
	if (mfd->watch.wdata.mode.dwch_24h)
		mfd->watch.wdata.mode.dwch_24_00 = 1;
	oem_mdss_watch_reg_write(mfd, WATCH_CTL);
	pr_info("[Watch] %s : %s\n", __func__,
			watch_on ? "On" : "Off");
}

void lcd_watch_set_fd_ctl(struct msm_fb_data_type *mfd, int enable)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	pr_info("[Watch] %s : font download control %s\n", __func__, enable ? "enable" : "disable");
	mfd->watch.wdata.font_ctl.aod_fd = enable;

	oem_mdss_watch_reg_write(mfd, FD_CTL);
}

void lcd_watch_set_btm_reset(struct msm_fb_data_type *mfd, int enable)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	pr_info("[Watch] %s : %s\n", __func__, enable ? "enable" : "disable");
	mfd->watch.wdata.mode.btm_reset = enable;

	oem_mdss_watch_reg_write(mfd, WATCH_CTL);
}

static ssize_t store_watch_fontonoff	(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	u8 value;
	u8 zero = '0';

	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl= NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[Watch] %s, no panel connected!\n", __func__);
		return 0;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	mutex_lock(&mfd->watch_lock);
	memcpy((char *)&value, buf, sizeof(u8));

	if (value == 0 || value == zero) {
		value = 0x00;
		mfd->watch.hw_clock_user_state = false;
	}
	else {
		mfd->watch.hw_clock_user_state = true;
		value = 0x01;
	}

	pr_info("[Watch] HW clock user setting : %s\n", mfd->watch.hw_clock_user_state ? "On" : "Off");
	lcd_watch_display_onoff(mfd, value);
	mutex_unlock(&mfd->watch_lock);

	return count;
}

static ssize_t show_watch_fontonoff(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pr_info("[Watch] %s : hw clock user state : %d\n",  __func__, mfd->watch.hw_clock_user_state);
	return sprintf(buf,"%d\n", mfd->watch.hw_clock_user_state);
}

static ssize_t store_watch_fonteffect_config (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct WatchFontEffectConfig cfg;
	char period[8] = {0};

	mutex_lock(&mfd->watch_lock);
	memcpy((char *)&cfg, buf, sizeof(struct WatchFontEffectConfig));

	mfd->watch.wdata.mode.dwch_24h = cfg.h24_en;
	mfd->watch.wdata.mode.dwch_z_disp_h = cfg.zero_disp;
	mfd->watch.wdata.mode.dwch_ms = cfg.clock_disp_type;
	mfd->watch.wdata.mode.dwch_24_00
		= cfg.midnight_hour_zero_en;
	mfd->watch.wdata.mode.dwch_blink_period = cfg.blink.blink_type;
	mfd->watch.wdata.position.dwch_blink_start_x = cfg.blink.bstartx;
	mfd->watch.wdata.position.dwch_blink_size_x = cfg.blink.bendx - cfg.blink.bstartx;

	switch (cfg.blink.blink_type) {
	default:
	case 0:
		snprintf(period, 8, "Off");
		break;
	case 1:
		snprintf(period, 8, "0.5 sec");
		break;
	case 2:
		snprintf(period, 8, "1 sec");
		break;
	case 3:
		snprintf(period, 8, "2 sec");
		break;
	}

	lcd_watch_set_fontposition(mfd);
	lcd_watch_display_onoff(mfd, mfd->watch.hw_clock_user_state ? WATCH_ON : WATCH_OFF);
	mutex_unlock(&mfd->watch_lock);

	pr_info("[Watch] %s : 24h mode %s, Zero Dispaly %s,%s Type %s mode "
		"Blink area [%d , %d] Period %s \n", __func__,
		cfg.h24_en ? "Enable" : "Disable",
		cfg.zero_disp ? "Enable" : "Disable",
		cfg.clock_disp_type ? "MM:SS" : "HH:MM",
		cfg.midnight_hour_zero_en ? "00:00" : "12:00",
		cfg.blink.bstartx, cfg.blink.bendx, period);

	return count;
}

static ssize_t store_watch_fontproperty_config (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct WatchFontPropertyConfig cfg;
	int idx = 0;
	char log[256] = {0};
	int len = 0;

	mutex_lock(&mfd->watch_lock);
	memcpy((char *)&cfg, buf, sizeof(struct WatchFontPropertyConfig));

	len += snprintf(log + len, 256 - len, "%s : LUT[%d] ",
		__func__, (int)cfg.max_num);

	for (idx = 0; idx < (int)cfg.max_num; idx++) {
		mfd->watch.wdata.font.lut[idx].b = cfg.LUT[idx].RGB_blue;
		mfd->watch.wdata.font.lut[idx].g = cfg.LUT[idx].RGB_green;
		mfd->watch.wdata.font.lut[idx].r = cfg.LUT[idx].RGB_red;

		len += snprintf(log + len, 256 - len, "%d:%02X%02X%02X ", idx + 1,
			cfg.LUT[idx].RGB_blue, cfg.LUT[idx].RGB_green,
			cfg.LUT[idx].RGB_red);
	}

	lcd_watch_set_fontproperty(mfd);
	mutex_unlock(&mfd->watch_lock);

	pr_info("[Watch] %s end %s \n", __func__, log);
	return count;
}

static ssize_t store_watch_fontposition_config(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct WatchFontPostionConfig cfg;
	struct WatchFontAnalogPositionConfig a_cfg;

	if (mfd->watch.requested_font_type == FONT_ANALOG) {
		mutex_lock(&mfd->watch_lock);
		memcpy((char *)&a_cfg, buf, sizeof(struct WatchFontAnalogPositionConfig));

		//Set Watch set reg 93h
		mfd->watch.wdata.position.awch_pos.start_x= a_cfg.a_watstartx - AOD_POS_X;
		mfd->watch.wdata.position.awch_pos.start_y = a_cfg.a_watstarty;
		mfd->watch.wdata.position.awch_center.start_x = a_cfg.a_watchcenterx;
		mfd->watch.wdata.position.awch_center.start_y = a_cfg.a_watchcentery;

		//Set Font size reg 95h
		mfd->watch.wdata.font.font_size_analog_x = a_cfg.a_fontx;
		mfd->watch.wdata.font.font_size_analog_y = a_cfg.a_fonty;
		mfd->watch.wdata.font.font_analog_center_x = a_cfg.a_font_centerx;
		mfd->watch.wdata.font.font_analog_center_y = a_cfg.a_font_centery;

		lcd_watch_set_fontposition(mfd);
		lcd_watch_set_fontproperty(mfd);
		mutex_unlock(&mfd->watch_lock);

		pr_info("[Watch] %s, awch_pos.start_x : %d awch_pos.start_y : %d\n", __func__, mfd->watch.wdata.position.awch_pos.start_x,
			mfd->watch.wdata.position.awch_pos.start_y);
		pr_info("[Watch] %s, awch_center.start_x : %d awch_center.start_y : %d\n", __func__, mfd->watch.wdata.position.awch_center.start_x,
			mfd->watch.wdata.position.awch_center.start_y);
		pr_info("[Watch] %s, awch_size.size_x : %d awch_size.size_y : %d\n", __func__, mfd->watch.wdata.position.awch_size.size_x,
			mfd->watch.wdata.position.awch_size.size_y);
		pr_info("[Watch] %s, font_size_analog_x : %dfont_size_analog_y : %d\n", __func__, mfd->watch.wdata.font.font_size_analog_x,
			mfd->watch.wdata.font.font_size_analog_y);
		pr_info("[Watch] %s, font_analog_center_x : %d font_analog_center_y : %d\n", __func__, mfd->watch.wdata.font.font_analog_center_x,
			mfd->watch.wdata.font.font_analog_center_y);
	}
	else if (mfd->watch.requested_font_type == FONT_DIGITAL || mfd->watch.requested_font_type == FONT_MINI) {
		mutex_lock(&mfd->watch_lock);
		memcpy((char *)&cfg, buf, sizeof(struct WatchFontPostionConfig));

		//Set Watch set reg 93h
		mfd->watch.wdata.position.dwch_pos.start_x = cfg.watstartx - AOD_POS_X;
		if (mfd->watch.requested_font_type == FONT_DIGITAL)
			mfd->watch.wdata.position.dwch_size.size_x = DWATCH_SIZE_X;
		else if (mfd->watch.requested_font_type == FONT_MINI)
			mfd->watch.wdata.position.dwch_size.size_x = MWATCH_SIZE_X;
		mfd->watch.wdata.position.dwch_pos.start_y = cfg.watstarty;
		mfd->watch.wdata.position.dwch_size.size_y = cfg.watendy - cfg.watstarty;
		mfd->watch.wdata.position.dwch_h_10_pos_x = cfg.h10x_pos;
		mfd->watch.wdata.position.dwch_h_1_pos_x = cfg.h1x_pos;
		mfd->watch.wdata.position.dwch_m_10_pos_x = cfg.m10x_pos;
		mfd->watch.wdata.position.dwch_m_1_pos_x = cfg.m1x_pos;
		mfd->watch.wdata.position.dwch_colon_pos_x = cfg.clx_pos;

		//Set Font size reg 95h
		mfd->watch.wdata.font.font_size_digit_x = cfg.d_fontx;
		mfd->watch.wdata.font.font_size_digit_y = cfg.d_fonty;
		mfd->watch.wdata.font.font_size_colon_x = cfg.d_fontx;

		lcd_watch_set_btm_reset(mfd, 1);
		lcd_watch_set_fontposition(mfd);
		lcd_watch_set_fontproperty(mfd);
		mdelay(5);
		lcd_watch_set_btm_reset(mfd, 0);
		mutex_unlock(&mfd->watch_lock);

		pr_info("[Watch] %s, dwch_pos.start_x : %d dwch_pos.start_y : %d\n", __func__, mfd->watch.wdata.position.dwch_pos.start_x,
			mfd->watch.wdata.position.dwch_pos.start_y);
		pr_info("[Watch] %s, dwch_size.size_x : %d dwch_size.size_y : %d\n", __func__, mfd->watch.wdata.position.dwch_size.size_x,
			mfd->watch.wdata.position.dwch_size.size_y);
		pr_info("[Watch] %s, 10h : %d 1h : %d colon : %d 10m : %d 1m : %d\n", __func__, mfd->watch.wdata.position.dwch_h_10_pos_x,
			mfd->watch.wdata.position.dwch_h_1_pos_x,
			mfd->watch.wdata.position.dwch_colon_pos_x,
			mfd->watch.wdata.position.dwch_m_10_pos_x,
			mfd->watch.wdata.position.dwch_m_1_pos_x
			);
		pr_info("[Watch] %s, font_size_digit_x : %d font_size_digit_y : %d font_size_colon_x : %d\n", __func__, mfd->watch.wdata.font.font_size_digit_x,
			mfd->watch.wdata.font.font_size_digit_y,
			mfd->watch.wdata.font.font_size_colon_x);
	}

	return count;
}

static ssize_t store_watch_timesync_config(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct WatchTimeSyncConfig cfg;
	u16 rtc_count = 305;

	pr_info("[Watch] %s\n", __func__);

	mutex_lock(&mfd->watch_lock);
	memcpy((char *)&cfg, buf, sizeof(struct WatchTimeSyncConfig));

	mfd->watch.wdata.time.rtc_cur_time.rtc_hour = cfg.rtc_cwhour;
	mfd->watch.wdata.time.rtc_cur_time.rtc_min = cfg.rtc_cwmin;
	mfd->watch.wdata.time.rtc_cur_time.rtc_sec = cfg.rtc_cwsec;
	mfd->watch.wdata.time.rtc_cur_time.rtc_undersec = cfg.rtc_cwmilli;
	mfd->watch.wdata.time.rtc_clk_freq = 32764;
	mfd->watch.wdata.time.rtc_cur_time.rtc_undersec = (int)((mfd->watch.wdata.time.rtc_cur_time.rtc_undersec * rtc_count) / 10);

	lcd_watch_rtc_start(mfd, WATCH_RTC_STOP);
	mdelay(1);
	lcd_watch_rtc_start(mfd, WATCH_RTC_UPDATE);

	mutex_unlock(&mfd->watch_lock);

	return count;
}

static ssize_t store_watch_font_download(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	u8 dl_start = 0;

	if (kstrtou8(buf, 10, &dl_start))
		return -EINVAL;

	lcd_watch_set_fd_ctl(mfd, dl_start);

	return count;
}

static ssize_t store_watch_font_type(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct WatchFontTypeSliceInfoConfig cfg;

	memcpy((char *)&cfg, buf, sizeof(struct WatchFontTypeSliceInfoConfig));

	//Set Watch CTRL reg 92h
	if (cfg.font_type == FONT_ANALOG) {
		mfd->watch.wdata.mode.watch_en = 0x03;
	}
	else if (cfg.font_type == FONT_DIGITAL || cfg.font_type == FONT_MINI) {
		mfd->watch.wdata.mode.watch_en = 0x01;
	}
	else if (cfg.font_type == FONT_NONE) {
		mfd->watch.current_font_type = FONT_NONE;
		mfd->watch.font_download_state = FONT_STATE_NONE;
		pr_info("[Watch] %s : Request font reset!!\n", __func__);
		mfd->ready_to_u2 = false;
		return count;
	}
	else {
		pr_err("[Watch] %s : font type error\n", __func__);
		return count;
	}

	//Set Font Download reg 94h
	mfd->watch.requested_font_type = cfg.font_type;
	if (mfd->watch.current_font_type != mfd->watch.requested_font_type)
		mfd->watch.font_download_state = FONT_STATE_NONE;
	else
		mfd->watch.font_download_state = FONT_DOWNLOAD_COMPLETE;
	mfd->watch.wdata.font_ctl.fdsidx = cfg.slice_idx;
	mfd->watch.wdata.font_ctl.fdscnt = cfg.slice_cnt;
	mfd->watch.wdata.font_crc.crc_code = cfg.font_crc_code;
	mfd->watch.wdata.font_crc.total_size = cfg.total_font_size;
	mfd->watch.wdata.mode.alpha_on = cfg.alpha_on;

	pr_info("[Watch] %s, set watch font type : %d, slice index : %d, slice count : %d\n",
				__func__, mfd->watch.requested_font_type, mfd->watch.wdata.font_ctl.fdsidx, mfd->watch.wdata.font_ctl.fdscnt);
	pr_info("[Watch] %s, set font crc code : 0x%x, total font size : %d, alpha blending mode : %d\n",
				__func__, mfd->watch.wdata.font_crc.crc_code, mfd->watch.wdata.font_crc.total_size, mfd->watch.wdata.mode.alpha_on);

	return count;
}


static ssize_t show_watch_font_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	if (mfd->panel_info->aod_cur_mode == AOD_PANEL_MODE_U3_UNBLANK) {
		pr_info("[Watch] Enable ready_to_u2 true!!");
		mfd->ready_to_u2 = true;
	}
	else
		mfd->ready_to_u2 = false;
	pr_info("[Watch] %s : current_font_type : %d\n",  __func__, mfd->watch.current_font_type);
	return sprintf(buf,"%d\n", mfd->watch.current_font_type);
}


static ssize_t show_watch_watch_reg_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd_leng=0, index=0, i=0;
	unsigned int value=0;
	char *param;
	char reg = ' ';
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	pr_info("[Watch] buf : %s\n", buf);
	sscanf(buf, "%d", &cmd_leng);
	if (cmd_leng <= 0) {
		pr_err("[Watch] cmd_leng error\n");
		return count;
	}

	param = kzalloc(sizeof(int) * cmd_leng, GFP_KERNEL);
	if (!param) {
		pr_err("[Watch] Error to get param memory\n");
		return count;
	}

	for(index=0,i=0;index<count;index++) {
		if (buf[index]==' ') {
			sscanf(&buf[index+1], "%x", &value);
			param[i++]=(char)value;
		}
	}

	if (i!=cmd_leng) {
		pr_err("[Watch] param parsing error\n");
		kfree(param);
		return count;
	}

	for(i=0;i<cmd_leng;i++) {
		printk("0x%02X ", param[i]);
	}

	reg = param[0];
	switch(reg)
	{
		case RTC_SET:
			memcpy(&ctrl->watch_rtc_set_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_rtc_set_cmd, CMD_REQ_COMMIT);
			break;
		case RTC_INFO:
			memcpy(&ctrl->watch_rtc_info_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_rtc_info_cmd, CMD_REQ_COMMIT);
			break;
		case WATCH_CTL:
			memcpy(&ctrl->watch_ctl_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_ctl_cmd, CMD_REQ_COMMIT);
			break;
		case WATCH_SET:
			memcpy(&ctrl->watch_set_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_set_cmd, CMD_REQ_COMMIT);
			break;
		case FD_CTL:
			memcpy(&ctrl->watch_fd_ctl_cmd.cmds->payload[0], param, cmd_leng);
			//mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_fd_ctl_cmd, CMD_REQ_COMMIT);
			break;
		case FONT_SET:
			memcpy(&ctrl->watch_font_set_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_font_set_cmd, CMD_REQ_COMMIT);
			break;
		case U2_SCR_FAD:
			memcpy(&ctrl->watch_u2_scr_fad_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_u2_scr_fad_cmd, CMD_REQ_COMMIT);
			break;
		case FONT_CRC:
			memcpy(&ctrl->watch_font_crc_cmd.cmds->payload[0], param, cmd_leng);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->watch_font_crc_cmd, CMD_REQ_COMMIT);
			break;
		case TCH_FIRMWR:
			break;
		default:
			pr_err("[Watch] Can't match 0x%02X param\n", reg);
			kfree(param);
			return count;
	}

	kfree(param);

	return count;
}

static ssize_t show_watch_watch_reg_read(struct device *dev, struct device_attribute *attr, char *buf)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	int i;
	char rx_buf[40] = {0x0};

	oem_mdss_watch_reg_read(mfd, RTC_SET, rx_buf, 5);
	printk("[Watch] 0x90h : ");
	for(i=0; i<10; i++)
		printk("%02Xh, ",rx_buf[i]);
	printk("\n");

	oem_mdss_watch_reg_read(mfd, RTC_INFO, rx_buf, 5);
	printk("[Watch] 0x91h : ");
	for(i=0; i<5; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");
	oem_mdss_watch_reg_read(mfd, WATCH_CTL, rx_buf, 3);
	printk("[Watch] 0x92h : ");
	for(i=0; i<3; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");
	oem_mdss_watch_reg_read(mfd, WATCH_SET, rx_buf, 30);
	printk("[Watch] 0x93h : ");
	for(i=0; i<30; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");
	oem_mdss_watch_reg_read(mfd, FD_CTL, rx_buf, 3);
	printk("[Watch] 0x94h : ");
	for(i=0; i<2; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");
	oem_mdss_watch_reg_read(mfd, FONT_SET, rx_buf, 31);
	printk("[Watch] 0x95h : ");
	for(i=0; i<31; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");
	oem_mdss_watch_reg_read(mfd, U2_SCR_FAD, rx_buf, 9);
	printk("[Watch] 0x98h : ");
	for(i=0; i<9; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");
	oem_mdss_watch_reg_read(mfd, FONT_CRC, rx_buf, 7);
	printk("[Watch] 0x99h : ");
	for(i=0; i<7; i++)
		printk("%02Xh, ", rx_buf[i]);

	printk("\n");

	/*oem_mdss_watch_reg_read(mfd, TCH_FIRMWR, rx_buf, 3);
	printk("[Watch] 0x90h : ");
	for(i=0; i<3; i++)
		printk("%02Xh, ", rx_buf[i]);
	printk("\n");*/
	return 0;
}


char reg_value[40];
int current_reg;
int current_size;
static ssize_t store_watch_watch_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value=0, reg=0, size=0, i=0;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	char rx_buf[40] = {0x0};
	memset(reg_value, 0x0, 40);

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (sscanf(buf, "%d", &value) != 1) {
		pr_err("[Watch] sccanf buf error!\n");
		return -EINVAL;
	}
	switch(value)
	{
		case 90:
			reg = RTC_SET;
			size = 10;
			break;
		case 91:
			reg = RTC_INFO;
			size = 5;
			break;
		case 92:
			reg = WATCH_CTL;
			size = 3;
			break;
		case 93:
			reg = WATCH_SET;
			size = 30;
			break;
		case 94:
			reg = FD_CTL;
			size = 3;
			break;
		case 95:
			reg = FONT_SET;
			size = 31;
			break;
		case 98:
			reg = U2_SCR_FAD;
			size = 9;
			break;
		case 99:
			reg = FONT_CRC;
			size = 7;
			break;
		default:
			pr_err("[Watch] Can't match watch reg %02X\n", reg);
			return count;
	}
	current_reg = reg;
	oem_mdss_watch_reg_read(mfd, reg, rx_buf, size);
	printk("[Watch] %02X", reg);
	if (reg == FD_CTL)
		size--;
	for(i=0; i<size; i++) {
		printk(" %02X",rx_buf[i]);
		reg_value[i] = rx_buf[i];
	}
	current_size = size;
	printk("\n");
	return count;
}

static ssize_t show_watch_watch_reg(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;

	sprintf(buf, "reg: %02X", current_reg);
	for(i=0;i<current_size;i++)
		sprintf(buf,"%s %02X",buf, reg_value[i]);
	pr_info("[Watch] %s\n", buf);
	return sprintf(buf,"%s\n",buf);
}

static ssize_t store_watch_scroll(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	int enable, up_down, upper, lower, sec, pixel;

	if (6 != sscanf(buf, "%d %d %d %d %d %d", &enable, &up_down, &upper, &lower, &sec, &pixel)) {
		pr_err("[Watch] %s : input error\n", __func__);
		return count;
	}
	pr_info("[Watch] enable : %d up_down : %d upper : %d lower : %d sec : %d pixel : %d\n", enable, up_down, upper, lower, sec, pixel);
	mfd->watch.wdata.u2_scr_fad.vsen = enable;
	mfd->watch.wdata.u2_scr_fad.vsks = 0;
	mfd->watch.wdata.u2_scr_fad.vsmode = up_down;
	mfd->watch.wdata.u2_scr_fad.vsposfix = 0;
	mfd->watch.wdata.u2_scr_fad.vsub = upper;
	mfd->watch.wdata.u2_scr_fad.vslb = lower;
	mfd->watch.wdata.u2_scr_fad.vsfi = sec * 30;
	mfd->watch.wdata.u2_scr_fad.vsli = pixel;
	oem_mdss_watch_reg_write(mfd, U2_SCR_FAD);

	return count;
}

static ssize_t store_watch_fade_in_out(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	int enable, fasd_step, full_off, middle_level;
	if (4 != sscanf(buf, "%d %d %d %d", &enable, &fasd_step, &full_off, &middle_level)) {
		pr_err("[Watch] %s : input error\n", __func__);
		return count;
	}
	mfd->watch.wdata.u2_scr_fad.fade_en = enable;
	mfd->watch.wdata.u2_scr_fad.fasd_step = fasd_step;
	mfd->watch.wdata.u2_scr_fad.fade_peak_time = full_off;
	mfd->watch.wdata.u2_scr_fad.fade_speed = middle_level;
	pr_info("[Watch] enable : %d fasd_step : %d fade_peak_time : %d fade_speed : %d\n", enable, fasd_step,full_off, middle_level);
	oem_mdss_watch_reg_write(mfd, U2_SCR_FAD);

	return count;
}

static ssize_t set_roi_for_hiddenmenu(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	u8 value;

	memcpy((char *)&value, buf, sizeof(u8));

	if (value == '1')
		mfd->watch.set_roi = 1;
	else
		mfd->watch.set_roi = 0;

	pr_info("[Watch] %s set_roi : %d\n", __func__, mfd->watch.set_roi);

	return count;
}

#if defined(CONFIG_LGE_LCD_TUNING)
static ssize_t show_watch_font_type_reset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	return sprintf(buf, "%d\n", mfd->watch.font_type_reset);
}


static ssize_t store_watch_font_type_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	int enable;

	sscanf(buf, "%d", &enable);

	if (enable == 1)
		mfd->watch.font_type_reset = 1;
	else
		mfd->watch.font_type_reset = 0;

	pr_info("[Watch] %s enable : %d\n", __func__, enable);
	return count;
}
#endif

static DEVICE_ATTR(config_fontonoff, S_IWUSR|S_IRUGO, show_watch_fontonoff, store_watch_fontonoff);
static DEVICE_ATTR(config_fonteffect, S_IWUSR|S_IRUGO, NULL, store_watch_fonteffect_config);
static DEVICE_ATTR(config_fontproperty, S_IWUSR|S_IRUGO, NULL, store_watch_fontproperty_config);
static DEVICE_ATTR(config_fontposition, S_IWUSR|S_IRUGO, NULL, store_watch_fontposition_config);
static DEVICE_ATTR(config_timesync, S_IWUSR|S_IRUGO, NULL, store_watch_timesync_config);
static DEVICE_ATTR(config_font_download, S_IWUSR|S_IRUGO, NULL, store_watch_font_download);
static DEVICE_ATTR(font_type, S_IWUSR|S_IRUGO, show_watch_font_type, store_watch_font_type);
static DEVICE_ATTR(set_watch, S_IWUSR|S_IRUGO, show_watch_watch_reg_read, show_watch_watch_reg_write);
static DEVICE_ATTR(get_watch, S_IWUSR|S_IRUGO, show_watch_watch_reg, store_watch_watch_reg);
static DEVICE_ATTR(scroll, S_IWUSR|S_IRUGO, NULL, store_watch_scroll);
static DEVICE_ATTR(fade_in_out, S_IWUSR|S_IRUGO, NULL, store_watch_fade_in_out);
static DEVICE_ATTR(set_roi, S_IWUSR|S_IRUGO, NULL, set_roi_for_hiddenmenu);

#if defined(CONFIG_LGE_LCD_TUNING)
static DEVICE_ATTR(font_type_reset, S_IWUSR|S_IRUGO, show_watch_font_type_reset, store_watch_font_type_reset);
#endif


static struct attribute *watch_attribute_list[] = {
	&dev_attr_config_fontonoff.attr,
	&dev_attr_config_fonteffect.attr,
	&dev_attr_config_fontproperty.attr,
	&dev_attr_config_fontposition.attr,
	&dev_attr_config_timesync.attr,
	&dev_attr_config_font_download.attr,
	&dev_attr_font_type.attr,
	&dev_attr_set_watch.attr,
	&dev_attr_get_watch.attr,
	&dev_attr_scroll.attr,
	&dev_attr_fade_in_out.attr,
	&dev_attr_set_roi.attr,
#if defined(CONFIG_LGE_LCD_TUNING)
	&dev_attr_font_type_reset.attr,
#endif
	NULL,
};

static const struct attribute_group watch_attribute_group = {
	.attrs = watch_attribute_list,
};

int lcd_watch_register_sysfs(struct  msm_fb_data_type *mfd)
{
	int ret = 0 ;

	ret = sysfs_create_group(&mfd->fbi->dev->kobj, &watch_attribute_group);
	if (ret < 0) {
		pr_err("[Watch] failed to create sysfs\n");
		return ret;
	}

	return ret;
}

int lcd_watch_wdata_init(struct  msm_fb_data_type *mfd)
{
	int ret = 0 ;

	memset(&(mfd->watch), 0x0, sizeof(struct watch_data));
	mfd->watch.wdata.position.aod_pos_x = AOD_POS_X;
	mfd->watch.wdata.position.awch_size.size_x = AWATCH_SIZE_X;
	mfd->watch.wdata.position.awch_size.size_y = AWATCH_SIZE_Y;
	mfd->watch.wdata.mode.dwch_z_disp_m = 1;
	mfd->watch.wdata.mode.update_u2_enter = 1;
	mfd->watch.wdata.mode.alpha_on = 0;
	mfd->watch.wdata.font_ctl.fderrfix = 1;
	mfd->watch.wdata.u2_scr_fad.vsen = 1;
	mfd->watch.wdata.u2_scr_fad.vsks = 0;
	mfd->watch.wdata.u2_scr_fad.vsmode = 1;
	mfd->watch.wdata.u2_scr_fad.vsposfix = 1;
	mfd->watch.wdata.u2_scr_fad.vsub = 320;
	mfd->watch.wdata.u2_scr_fad.vslb = 416;
	mfd->watch.wdata.u2_scr_fad.vsfi = 900;
	mfd->watch.wdata.u2_scr_fad.vsli = 1;
#if defined(CONFIG_LGE_LCD_TUNING)
	mfd->watch.font_type_reset = 0;
#endif
	mfd_base = mfd;

	return ret;
}

void lcd_watch_set_reg_after_fd(struct  msm_fb_data_type *mfd)
{
	mutex_lock(&mfd->watch_lock);
	if (mfd->need_to_init_watch) {
		lcd_watch_set_fontposition(mfd);
		lcd_watch_set_fontproperty(mfd);
		lcd_watch_display_onoff(mfd, mfd->watch.hw_clock_user_state);
		pr_info("[Watch] Init watch register after font download!!\n");
		mfd->need_to_init_watch = false;
	}
	else
		pr_info("[Watch] Font not downloaded!!\n");
	mutex_unlock(&mfd->watch_lock);
}

void lcd_watch_restore_reg_after_panel_reset(void)
{
	mutex_lock(&mfd_base->watch_lock);
	lcd_watch_set_fontposition(mfd_base);
	lcd_watch_set_fontproperty(mfd_base);
	lcd_watch_rtc_start(mfd_base, WATCH_RTC_STOP);
	mdelay(1);
	lcd_watch_rtc_start(mfd_base, WATCH_RTC_UPDATE);
	lcd_watch_display_onoff(mfd_base, mfd_base->watch.hw_clock_user_state);
	mutex_unlock(&mfd_base->watch_lock);
	pr_info("[Watch] Init watch regster after panel reset!!\n");
}

void lcd_watch_font_crc_check_after_panel_reset(void)
{
	if (mfd_base->watch.current_font_type)
		lcd_watch_font_crc_check(mfd_base);
}

void lcd_watch_deside_status(struct  msm_fb_data_type *mfd, unsigned int cur_mode, unsigned int next_mode)
{

	pr_info("[Watch] Current mode : %d, Next Mode : %d, User Setting %s\n", cur_mode, next_mode, mfd->watch.hw_clock_user_state ? "On" : "Off");
	mfd->ready_to_u2 = false;
	/* Next is U3 Case */
	/* 1. block AOD brightness set */
	if (next_mode == AOD_PANEL_MODE_U3_UNBLANK) {
		pr_info("[Watch] Block AOD backlight in U3!!\n");
		mfd->block_aod_bl = true;
#if defined(CONFIG_LGE_LCD_TUNING)
		if (mfd->watch.font_type_reset == 1) {
			mfd->watch.current_font_type = FONT_NONE;
			mfd->watch.font_download_state = FONT_STATE_NONE;
		}
#endif
		return;
	}

	/* Next is U0 Case */
	/* 1. block AOD brightness set */
	/* 2. reset current font type */
	if (next_mode == AOD_PANEL_MODE_U0_BLANK) {
		pr_info("[Watch] Block AOD backlight in U0 and need to init watch!!\n");
		mfd->block_aod_bl = true;
		mfd->need_to_init_watch = true;
		return;
	}

	/* HW clock don't control */
	/* 1. U2 blank -> U2 unblank*/
	if (cur_mode == AOD_PANEL_MODE_U2_BLANK && next_mode == AOD_PANEL_MODE_U2_UNBLANK) {
		pr_info("[Watch] Don't control hw clock!!\n");
		return;
	}

	/* Next is U2 blank */
	/* 1. U3 -> U2 blank */
	/* HW clock on and scroll enable*/
	if (cur_mode == AOD_PANEL_MODE_U3_UNBLANK && next_mode == AOD_PANEL_MODE_U2_BLANK) {
		lcd_watch_display_onoff(mfd, mfd->watch.hw_clock_user_state ? WATCH_ON : WATCH_OFF);
		pr_info("[Watch] HW clock User State :  %s\n", mfd->watch.hw_clock_user_state ? "On" : "Off");
		mfd->watch.wdata.u2_scr_fad.vsposfix = 0;
		oem_mdss_watch_reg_write(mfd, U2_SCR_FAD);
		oem_mdss_aod_cmd_send(mfd, AOD_CMD_DISPLAY_ON);
	}
	/*2. U2 unblank -> U2 blank after far */
	/* scroll enable */
	if (cur_mode == AOD_PANEL_MODE_U2_UNBLANK && next_mode == AOD_PANEL_MODE_U2_BLANK) {
		mfd->watch.wdata.u2_scr_fad.vsposfix = 0;
		oem_mdss_watch_reg_write(mfd, U2_SCR_FAD);
	}
}
