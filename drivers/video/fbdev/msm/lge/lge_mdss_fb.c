/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
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

#include <linux/msm_mdp.h>
#include "../mdss_fb.h"
#include <linux/module.h>
#include "lge_mdss_display.h"
#ifdef CONFIG_LGE_LCD_OFF_DIMMING
#include <soc/qcom/lge/board_lge.h>
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
extern int mdss_fb_mode_switch(struct msm_fb_data_type *mfd, u32 mode);
#endif
#if defined(CONFIG_LGE_PANEL_RECOVERY)
struct msm_fb_data_type *mfd_recovery = NULL;
#endif

#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
extern void register_dp_notify_node(void);
#endif

void lge_mdss_fb_init(struct msm_fb_data_type *mfd)
{
	if(mfd->index != 0)
		return;
	mfd->bl_level_scaled = -1;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	mfd->bl_level_scaled_ex = -1;
#endif
#if defined(CONFIG_LGE_HIGH_LUMINANCE_MODE)
	mfd->panel_info->hl_mode_on = 0;
#endif
#if defined(CONFIG_LGE_PP_AD_SUPPORTED)
	mfd->ad_info.is_ad_on = 0;
	mfd->ad_info.user_br_lvl = 0;
	mfd->ad_info.ad_weight = 0;
	mfd->ad_info.old_ad_br_lvl= -1;
#endif
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	mutex_init(&mfd->aod_lock);
#endif
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	mutex_init(&mfd->watch_lock);
#endif
#if defined(CONFIG_LGE_PANEL_RECOVERY)
	mfd_recovery = mfd;
#endif
#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
	register_dp_notify_node();
#endif
}

/*---------------------------------------------------------------------------*/
/* LCD off & dimming                                                         */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_LGE_LCD_OFF_DIMMING
static bool fb_blank_called;
static inline bool is_blank_called(void)
{
	return fb_blank_called;
}

static inline bool is_factory_cable(void)
{
	unsigned int cable_info;
	cable_info = lge_get_factory_cable();

#if !defined(CONFIG_LGE_PM_EMBEDDED_BATTERY)
		if (cable_info == CABLE_56K ||
			cable_info == CABLE_130K ||
			cable_info == CABLE_910K) {
			pr_debug("%s : cable_type = factory(%d) \n",__func__, cable_info);
			return true;
		} else {
			return false;
		}
#else
		if (cable_info == CABLE_130K ||
			cable_info == CABLE_910K) {
			pr_debug("%s : cable_type = factory(%d) \n",__func__, cable_info);
			return true;
		} else {
			return false;
		}
#endif
}

void lge_set_blank_called(void)
{
	fb_blank_called = true;
}
#else
static inline bool is_blank_called(void)
{
	return true;
}

static inline bool is_factory_cable(void)
{
	return false;
}
#endif

/*---------------------------------------------------------------------------*/
/* Brightness - Backlight mapping (main)                                     */
/*---------------------------------------------------------------------------*/
int lge_br_to_bl (struct msm_fb_data_type *mfd, int br_lvl)
{
	/* TODO: change default value more reasonablly */
	int bl_lvl = 100;
	enum lge_bl_map_type blmaptype;
	struct mdss_panel_info *pinfo;


	if(mfd->index != 0) {
		pr_err("[AOD]fb[%d] is not for aod\n", mfd->index);
		return bl_lvl ;
	}
	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}
	/* modify brightness level */
#if defined(CONFIG_LGE_PP_AD_SUPPORTED)
	mfd->ad_info.user_br_lvl = br_lvl;
	if (mfd->ad_info.is_ad_on) {
		if (br_lvl != 0 && mfd->ad_info.ad_weight > 0) {
			mfd->ad_info.user_br_lvl = br_lvl;
			br_lvl -= (br_lvl * mfd->ad_info.ad_weight) / 100;
			if (br_lvl < 10)
				br_lvl = 10;
		}
		mfd->ad_info.old_ad_br_lvl = br_lvl;
	}
#endif
	if (lge_get_bootreason_with_lcd_dimming() && !is_blank_called()) {
		br_lvl = 1;
		pr_debug("%s: lcd dimming mode. set value = %d\n",
							__func__, br_lvl);
#ifdef CONFIG_LGE_LCD_OFF_DIMMING
	} else if (is_factory_cable() && !is_blank_called()) {
		br_lvl = 1;
		pr_debug("%s: Detect factory cable. set value = %d\n",
							__func__, br_lvl);
#endif
	}
	/* modify brightness level */

	/* map brightness level to device backlight level */
#if defined(CONFIG_LGE_HIGH_LUMINANCE_MODE)
	blmaptype = pinfo->hl_mode_on ? LGE_BLHL : LGE_BL;
#else
	blmaptype = LGE_BL;
#endif

	if (pinfo->blmap[blmaptype])
		bl_lvl = pinfo->blmap[blmaptype][br_lvl];
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	if (mfd->panel_info->aod_cur_mode == AOD_PANEL_MODE_U2_BLANK ||
		mfd->panel_info->aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK)
		pr_debug("[AOD] br_lvl(%d) -> bl_lvl(%d)\n", br_lvl, bl_lvl);
	else
		pr_debug("%s: br_lvl(%d) -> bl_lvl(%d)\n", lge_get_blmapname(blmaptype), br_lvl, bl_lvl);
#else
	pr_debug("%s: br_lvl(%d) -> bl_lvl(%d)\n", lge_get_blmapname(blmaptype), br_lvl, bl_lvl);
#endif
	return bl_lvl;
}

#if defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
/* TODO: implement for other bl device */
extern void lm3697_lcd_backlight_set_level(int level);
#endif
void lge_set_backlight(int bl_level)
{
#if defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	/* TODO: implement general backlight set function registration  */
	lm3697_lcd_backlight_set_level(bl_level);
#endif
}

/*---------------------------------------------------------------------------*/
/* Backlight control blocking                                                */
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_LGE_SP_MIRRORING_CTRL_BL)
static bool lge_block_bl_update;
static int lge_bl_lvl_unset;
static bool lge_is_bl_ready;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
static int lge_bl_lvl_unset_ex;
static bool lge_is_bl_ready_ex;
#endif


/* must call this function within mfd->bl_lock */
int lge_is_bl_update_blocked(int bl_lvl)
{
	lge_is_bl_ready = true;
	if(lge_block_bl_update) {
		lge_bl_lvl_unset = bl_lvl;
		pr_debug("%s: do not control backlight (bl: %d)\n", __func__,
			lge_bl_lvl_unset);
		return true;
	}
	return false;
}

void lge_set_bl_update_blocked(bool enable)
{
	lge_block_bl_update = enable;
}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
/* must call this function within mfd->bl_lock */
int lge_is_bl_update_blocked_ex(int bl_lvl)
{
	lge_is_bl_ready_ex = 1;
	if (lge_block_bl_update) {
		lge_bl_lvl_unset_ex = bl_lvl;
		pr_debug("%s: do not control backlight (bl: %d)\n", __func__,
			lge_bl_lvl_unset_ex);
		return 1;
	}
	return 0;
}
#endif

ssize_t mdss_fb_get_bl_off_and_block(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	mutex_lock(&mfd->bl_lock);
	ret = snprintf(buf, PAGE_SIZE, "sp link backlight status : %d\n",
						lge_block_bl_update);
	mutex_unlock(&mfd->bl_lock);
	return ret;
}

ssize_t mdss_fb_set_bl_off_and_block(struct device *dev,
		struct device_attribute *attr,const char *buf,  size_t count)
{
	struct fb_info *fbi;
	struct msm_fb_data_type *mfd;
	int enable;

	if (!count || !lge_is_bl_ready ||!dev) {
		pr_warn("%s invalid value : %d, %d || NULL check\n",
		             __func__, (int) count, lge_is_bl_ready);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	if (!lge_is_bl_ready_ex) {
		pr_warn("%s invalid value : %d, %d || NULL check\n",
			__func__, (int) count, lge_is_bl_ready);
		return -EINVAL;
	}
#endif
	fbi = dev_get_drvdata(dev);
	mfd = fbi->par;

	enable = simple_strtoul(buf, NULL, 10);
	if (enable) {
		pr_debug("[%s] status : %d, brightness : 0 \n", __func__,
			lge_block_bl_update);
		mutex_lock(&mfd->bl_lock);
		lge_block_bl_update = false;
		lge_bl_lvl_unset = mfd->bl_level;
		mdss_fb_set_backlight(mfd, 0);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		lge_bl_lvl_unset_ex = mfd->bl_level_ex;
		mdss_fb_set_backlight_ex(mfd, 0);
#endif
		lge_block_bl_update = true;
		mutex_unlock(&mfd->bl_lock);
	} else {
		pr_debug("[%s] status : %d, brightness : %d \n", __func__,
			lge_block_bl_update, lge_bl_lvl_unset);
		mutex_lock(&mfd->bl_lock);
		lge_block_bl_update = false;
		mdss_fb_set_backlight(mfd, lge_bl_lvl_unset);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mdss_fb_set_backlight_ex(mfd, lge_bl_lvl_unset_ex);
#endif
		mutex_unlock(&mfd->bl_lock);
	}

	return count;
}
#endif

/*---------------------------------------------------------------------------*/
/* Recovery Mode - To recovery when screen crack occured					 */
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_LGE_PANEL_RECOVERY)
int recovery_val;
ssize_t get_recovery_mode(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", recovery_val);

	return ret;
}

ssize_t set_recovery_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected!\n");
		return count;
	}
	if (mfd->panel_power_state == MDSS_PANEL_POWER_OFF) {
		pr_err("%s: Panel is off\n", __func__);
		return count;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (!ctrl) {
		pr_err("%s: ctrl is null\n", __func__);
		return count;
	}

	recovery_val = simple_strtoul(buf, NULL, 10);

	if(recovery_val > 0) {
		pr_debug("%s: RECOVERY when screen crack occured.\n",__func__);
		lge_panel_recovery_mode();
	} else {
		pr_debug("%s: NO RECOVERY\n",__func__);
	}

	return count;
}

bool lge_panel_recovery_mode(void)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl;
	pdata = dev_get_platdata(&mfd_recovery->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected!\n");
		return false;
	}
	if (mfd_recovery->panel_power_state == MDSS_PANEL_POWER_OFF) {
		pr_err("%s: Panel is off\n", __func__);
		return false;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (!ctrl) {
		pr_err("%s: ctrl is null\n", __func__);
		return false;
	}

	pr_debug("%s: RECOVERY when screen crack occured.\n",__func__);
	mdss_fb_report_panel_dead(mfd_recovery);
	return true;
}
EXPORT_SYMBOL(lge_panel_recovery_mode);
#endif


/*---------------------------------------------------------------------------*/
/* AOD backlight                                                             */
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
extern void mdss_fb_bl_update_notify(struct msm_fb_data_type *mfd,
		uint32_t notification_type);


static int bklt_ctrl_backup;
/* must call this function within mfd->bl_lock */
void lge_set_to_blex(struct msm_fb_data_type *mfd)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;
	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	bklt_ctrl_backup = ctrl_pdata->bklt_ctrl;
	ctrl_pdata->bklt_ctrl = ctrl_pdata->bkltex_ctrl;
}
/* must call this function within mfd->bl_lock */
void lge_restore_from_blex(struct msm_fb_data_type *mfd)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;
	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	ctrl_pdata->bklt_ctrl = bklt_ctrl_backup;
}

/* must call this function from within mfd->bl_lock */
void mdss_fb_set_backlight_ex(struct msm_fb_data_type *mfd, u32 bkl_lvl)
{
	struct mdss_panel_data *pdata;
	u32 temp = bkl_lvl;
	bool ad_bl_notify_needed = false;
	bool bl_notify_needed = false;

#if defined(CONFIG_LGE_SP_MIRRORING_CTRL_BL)
	if(lge_is_bl_update_blocked_ex(bkl_lvl))
		return;
#endif

	if ((((mdss_fb_is_power_off(mfd) && mfd->dcm_state != DCM_ENTER)
		|| !mfd->allow_bl_update_ex) && !IS_CALIB_MODE_BL(mfd)) ||
		mfd->panel_info->cont_splash_enabled) {
		if ((mfd->panel_info->aod_cur_mode != AOD_PANEL_MODE_U2_BLANK)
			&& (mfd->panel_info->aod_cur_mode != AOD_PANEL_MODE_U2_UNBLANK)) {
			mfd->unset_bl_level_ex = bkl_lvl;
			pr_debug("[AOD] Skip ext-BL ctrl except U2 & U2unblank mode\n");
			return;
		}
		else
		{
			mfd->unset_bl_level_ex = U32_MAX;
		}
	} else if (mdss_fb_is_power_on(mfd) && mfd->panel_info->panel_dead) {
		mfd->unset_bl_level_ex = mfd->bl_level_ex;
	} else {
		mfd->unset_bl_level_ex = U32_MAX;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if ((pdata) && (pdata->set_backlight)) {
		if (mfd->mdp.ad_calc_bl)
			(*mfd->mdp.ad_calc_bl)(mfd, temp, &temp,
							&ad_bl_notify_needed);
		/* TODO: just for information of qct original code of main bl
		 *       function. delete below code if no concern
		 */

		/*
		if (!IS_CALIB_MODE_BL(mfd))
			mdss_fb_scale_bl(mfd, &temp);
		*/
		/*
		 * Even though backlight has been scaled, want to show that
		 * backlight has been set to bkl_lvl to those that read from
		 * sysfs node. Thus, need to set bl_level even if it appears
		 * the backlight has already been set to the level it is at,
		 * as well as setting bl_level to bkl_lvl even though the
		 * backlight has been set to the scaled value.
		 */
		if (mfd->bl_level_scaled_ex == temp) {
			mfd->bl_level_ex = bkl_lvl;
		} else {
			if (mfd->bl_level_ex != bkl_lvl)
				bl_notify_needed = true;
			pr_debug("backlight sent to panel ex:%d\n", temp);
			lge_set_to_blex(mfd);
			pdata->set_backlight(pdata, temp);
			lge_restore_from_blex(mfd);
			mfd->bl_level_ex = bkl_lvl;
			mfd->bl_level_scaled_ex = temp;
		}
		if (ad_bl_notify_needed)
			mdss_fb_bl_update_notify(mfd,
				NOTIFY_TYPE_BL_AD_ATTEN_UPDATE);
		if (bl_notify_needed)
			mdss_fb_bl_update_notify(mfd,
				NOTIFY_TYPE_BL_UPDATE);
	}
}
#endif

/* TODO: call in mutex lock bl_aod_bl?  make new mutex */
void mdss_fb_set_bl_brightness_aod_sub(struct msm_fb_data_type *mfd,
				      enum led_brightness value)
{
	int bl_lvl;

	if (value > mfd->panel_info->brightness_max)
		value = mfd->panel_info->brightness_max;

#ifndef CONFIG_LGE_DISPLAY_BL_EXTENDED
	if( mfd->panel_info->aod_cur_mode != AOD_PANEL_MODE_U3_UNBLANK )
		mfd->bl_isU3_mode = 0;
#endif
	/* This maps android backlight level 0 to 255 into
	   driver backlight level 0 to bl_max with rounding */
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
	mfd->br_lvl_ex = value;
	MDSS_BRIGHT_TO_BL_EX(bl_lvl, value, mfd->panel_info->bl_max,
				mfd->panel_info->brightness_max);
#else
	MDSS_BRIGHT_TO_BL(bl_lvl, value, mfd->panel_info->bl_max,
				mfd->panel_info->brightness_max);
#endif
	if (!bl_lvl && value)
		bl_lvl = 1;

	if (!IS_CALIB_MODE_BL(mfd) && (!mfd->ext_bl_ctrl || !value ||
							!mfd->bl_level)) {
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
		mdss_fb_set_backlight_ex(mfd, bl_lvl);
#else
		if ((mfd->bl_isU3_mode == 1 ) && ( bl_lvl == 0 ))
			pr_err("%s skip lcd-backlight-ex 0 setting\n", __func__);
		else
			mdss_fb_set_backlight(mfd, bl_lvl);
#endif
	}
}

static void mdss_fb_set_bl_brightness_aod(struct led_classdev *led_cdev,
				      enum led_brightness value)
{
	struct msm_fb_data_type *mfd = dev_get_drvdata(led_cdev->dev->parent);
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	if (mfd->block_aod_bl) {
		pr_debug("[AOD] Save unset Level : %d\n", value);
		mfd->unset_aod_bl = value;
		return;
	}
#endif
	mutex_lock(&mfd->bl_lock);
	mdss_fb_set_bl_brightness_aod_sub(mfd, value);
	mutex_unlock(&mfd->bl_lock);
}

static struct led_classdev backlight_led_aod = {
	/* TODO : rename suffix ex to aod in below string */
	.name           = "lcd-backlight-ex",
	.brightness     = MDSS_MAX_BL_BRIGHTNESS / 2,
	.usr_brightness_req = MDSS_MAX_BL_BRIGHTNESS,
	.brightness_set = mdss_fb_set_bl_brightness_aod,
	.max_brightness = MDSS_MAX_BL_BRIGHTNESS,
};

static int lge_lcd_backlight_registered;
void lge_backlight_register(struct msm_fb_data_type *mfd)
{
	if (lge_lcd_backlight_registered)
		return;
	backlight_led_aod.brightness = mfd->panel_info->default_brightness;
	backlight_led_aod.usr_brightness_req = mfd->panel_info->default_brightness;
	backlight_led_aod.max_brightness = mfd->panel_info->brightness_max;
	if (led_classdev_register(&mfd->pdev->dev, &backlight_led_aod))
		pr_err("led_classdev_ex_register failed\n");
	else
		lge_lcd_backlight_registered = 1;
}



void lge_backlight_unregister(void)
{
	if (lge_lcd_backlight_registered) {
		lge_lcd_backlight_registered = 0;
		led_classdev_unregister(&backlight_led_aod);
	}
}

#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
void mdss_fb_update_backlight_ex(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	u32 temp;
	if (mfd->unset_bl_level_ex == U32_MAX)
		return;
	mutex_lock(&mfd->bl_lock);
	if (!mfd->allow_bl_update_ex) {
		pdata = dev_get_platdata(&mfd->pdev->dev);
		if ((pdata) && (pdata->set_backlight)) {
			mfd->bl_level_ex = mfd->unset_bl_level_ex;
			temp = mfd->bl_level_ex;
			lge_set_to_blex(mfd);
			pr_debug("[Display] sub backlight sent to panel :%d in %s\n", temp, __func__);
			pdata->set_backlight(pdata, temp);
			lge_restore_from_blex(mfd);
			mfd->bl_level_scaled_ex = mfd->unset_bl_level_ex;
			mfd->allow_bl_update_ex = true;
		}
	}
	mutex_unlock(&mfd->bl_lock);
}
#endif

/* TODO: check whether backlight off should not be called in U2 blank */
void lge_aod_bl_ctrl_blank_blank(struct msm_fb_data_type *mfd)
{
	if (mfd->panel_info->aod_cur_mode ==
				AOD_PANEL_MODE_U2_BLANK && mfd->index == 0) {
		pr_debug("[AOD] Don't off backlight when U2 Blank\n");
		mfd->unset_bl_level = U32_MAX;
#if defined (CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mfd->unset_bl_level_ex = U32_MAX;
#endif
	}
	else {
		mfd->allow_bl_update = true;
		mdss_fb_set_backlight(mfd, 0);
		mfd->allow_bl_update = false;
		mfd->unset_bl_level = U32_MAX;
#if defined (CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mfd->allow_bl_update_ex = true;
		mdss_fb_set_backlight_ex(mfd, 0);
		mfd->allow_bl_update_ex = false;
		mfd->unset_bl_level_ex = U32_MAX;
#endif
	}
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
int lge_br_to_bl_ex (struct msm_fb_data_type *mfd, int br_lvl)
{
	/* TODO: change default value more reasonablly */
	int bl_lvl = 100;
	enum lge_bl_map_type blmaptype;
	struct mdss_panel_info *pinfo;

	if(mfd->index != 0) {
		pr_err("[AOD]fb[%d] is not for aod\n", mfd->index);
		return bl_lvl;
	}
	pinfo = mfd->panel_info;

	/* modify brightness level */
#if defined(CONFIG_LGE_PP_AD_SUPPORTED)
	/* TODO: delete below line if there is no concern */
	/* mfd->ad_info.user_br_lvl = br_lvl; */
	if (mfd->ad_info.is_ad_on) {
		if (br_lvl != 0 && mfd->ad_info.ad_weight > 0) {
			br_lvl -= (br_lvl * mfd->ad_info.ad_weight) / 100;
			if (br_lvl < 10)
				br_lvl = 10;
		}
	}
#endif
	if (lge_get_bootreason_with_lcd_dimming() && !is_blank_called()) {
		br_lvl = 1;
		pr_debug("%s: lcd dimming mode. set value = %d\n",
							__func__, br_lvl);
#ifdef CONFIG_LGE_LCD_OFF_DIMMING
	} else if (is_factory_cable() && !is_blank_called()) {
		br_lvl = 1;
		pr_debug("%s: Detect factory cable. set value = %d\n",
							__func__, br_lvl);
#endif
	}

	/* map brightness level to device backlight level */
#if defined(CONFIG_LGE_HIGH_LUMINANCE_MODE)
	blmaptype = pinfo->hl_mode_on ?
			(pinfo->bl2_dimm ? LGE_BL2DIMHL : LGE_BL2HL) :
			(pinfo->bl2_dimm ? LGE_BL2DIM : LGE_BL2);
#else
	blmaptype = (pinfo->bl2_dimm ? LGE_BL2DIM : LGE_BL2);
#endif
	if (pinfo->blmap[blmaptype])
		bl_lvl = pinfo->blmap[blmaptype][br_lvl];

	pr_debug("%s: br_lvl(%d) -> bl_lvl(%d)\n", lge_get_blmapname(blmaptype),
						br_lvl, bl_lvl);
	return bl_lvl;
	return bl_lvl;
}
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Assertive display                                                         */
/*---------------------------------------------------------------------------*/

#if defined(CONFIG_LGE_PP_AD_SUPPORTED)
extern void qpnp_wled_dimming(int dst_lvl,int current_lvl);
#if defined(CONFIG_BACKLIGHT_LM3697)
extern void lm3697_set_ramp_time(u32 interval);
#endif

static void mdss_dsi_panel_dimming_ctrl(int bl_level, int current_bl)
{
	qpnp_wled_dimming(bl_level,current_bl);
}

static void mdss_fb_ad_set_backlight(struct msm_fb_data_type *mfd, int brightness, int current_brightness)
{
	int main_bl, sub_bl;
	int current_bl;

	if(brightness <= 0)  // display off case
		return;

	if (mfd->panel_info->blmap[LGE_BL]) {
		main_bl = mfd->panel_info->blmap[LGE_BL][brightness];
		current_bl = mfd->panel_info->blmap[LGE_BL][current_brightness];
	} else {
		main_bl = brightness;
		current_bl = current_brightness;
	}
	sub_bl = brightness;

	pr_err("[AD_brightness] USER_BL[%d], OLD_BL[%d], NEW_BL[%d], weight[%d], ad_on[%d]\n",
		mfd->ad_info.user_br_lvl, mfd->ad_info.old_ad_br_lvl,
		brightness, mfd->ad_info.ad_weight, mfd->ad_info.is_ad_on);

	mfd->bl_level_scaled = main_bl ;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	mutex_lock(&mfd->bl_lock);
	mdss_fb_set_backlight_ex(mfd, sub_bl);
	mutex_unlock(&mfd->bl_lock);
#endif
	mutex_lock(&mfd->bl_lock);
	mdss_dsi_panel_dimming_ctrl(main_bl,current_bl);
	mutex_unlock(&mfd->bl_lock);
}

static int mdss_fb_ad_brightness_calc(struct msm_fb_data_type *mfd, u32 amb_light)
{
	int new_weight = 0;
	int ad_brightness = 0;
	// condition
	if (amb_light >= 30000)
		new_weight = 0;
	else if( amb_light >= 3100)
		new_weight = 8;
	else
		new_weight = 0;

	if(mfd->ad_info.ad_weight != new_weight) {
		mfd->ad_info.ad_weight = new_weight;
		if(new_weight == 0)  // wrong input or 0 weight, return the last user set value
			return mfd->ad_info.user_br_lvl;
	}

	if (new_weight > 0 && mfd->ad_info.user_br_lvl != 0) {
		ad_brightness = mfd->ad_info.user_br_lvl -
			mfd->ad_info.user_br_lvl * new_weight / 100;
		if (ad_brightness < 10)
			ad_brightness = 10;
	} else {
		ad_brightness = mfd->ad_info.user_br_lvl;
	}

	pr_debug("[AD_brightness] userBL : %d, weight : %d, AD_BL : %d\n",
			mfd->ad_info.user_br_lvl, new_weight, ad_brightness);
	return ad_brightness;
}

void lge_mdss_fb_ad_set_brightness(struct msm_fb_data_type *mfd, u32 amb_light, int ad_on)
{
	int dst_brightness = 0;
	int bl_lvl=0;

	if (mfd->ad_info.is_ad_on != ad_on) {
		mfd->ad_info.is_ad_on = ad_on;
/* TODO: implement ad dimming feature more generally */
#if defined(CONFIG_BACKLIGHT_LM3697) && defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		// dimming function enable
		if (ad_on)
			lm3697_set_ramp_time(2);
#endif
	}
	// AD on case
	if (ad_on) {
		// only re calculation when AD on
		dst_brightness = mdss_fb_ad_brightness_calc(mfd, amb_light);
		// AD off to AD on brightness setting
		if (mfd->ad_info.old_ad_br_lvl == -1) {
			if (mfd->ad_info.user_br_lvl != dst_brightness)
				mdss_fb_ad_set_backlight(mfd, dst_brightness,
						mfd->ad_info.user_br_lvl);
		} else {
			if(mfd->ad_info.old_ad_br_lvl== dst_brightness) {
				pr_debug("[AD_brightness] Same brightness setting,"
					 " no brightness update, brightness : %d\n",
						dst_brightness);
				return ;
			}
			mdss_fb_ad_set_backlight(mfd, dst_brightness,mfd->ad_info.old_ad_br_lvl);
		}
		mfd->ad_info.old_ad_br_lvl= dst_brightness;
	} else { // AD off case, back to user level
		bl_lvl = mfd->panel_info->blmap[LGE_BL][mfd->ad_info.user_br_lvl];
		//mdss_fb_ad_set_backlight(mfd, user_bl,user_bl);
#if defined(CONFIG_LGE_HIGH_LUMINANCE_MODE)
		if (bl_lvl && (mfd->panel_info->hl_mode_on == 0 )) {
#else
		if (bl_lvl) {
#endif
			mutex_lock(&mfd->bl_lock);
			mdss_fb_set_backlight(mfd, bl_lvl);
			pr_err("[Display] AD Off bl_lvl=%d\n",bl_lvl);
			mutex_unlock(&mfd->bl_lock);
		} else {
			pr_debug("Main Backlight already off. Or HL mode. No need to Set\n");
			// represent AD off state
			mfd->ad_info.old_ad_br_lvl = -1;
			mfd->ad_info.ad_weight = 0;
		}
/* TODO: implement ad dimming feature more generally */
#if defined(CONFIG_BACKLIGHT_LM3697) && defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		// dimming function disable
		lm3697_set_ramp_time(0);
#endif
	}
	pr_debug("[AD] lge_mdss_fb_ad_set_brightness : dst_brightness=%d user_bl=%d \n",
			dst_brightness,mfd->ad_info.old_ad_br_lvl);
}
#endif

/*---------------------------------------------------------------------------*/
/* Alawys On display                                                         */
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
void lge_mdss_fb_aod_release(struct msm_fb_data_type *mfd)
{
		if (mfd->index == 0) {
			pr_debug("[AOD] Disable AOD mode when shutdown\n");
			mfd->panel_info->aod_node_from_user = 0;
			mfd->panel_info->aod_keep_u2 = AOD_NO_DECISION;
		}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mdss_fb_set_backlight_ex(mfd, 0);
#endif
}

void lge_mdss_fb_aod_recovery(struct msm_fb_data_type *mfd, char *envp[])
{
	char *envp_aod[2] = {"PANEL_ALIVE=1", NULL};
	if ((mfd->panel_info->aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK
		|| mfd->panel_info->aod_cur_mode == AOD_PANEL_MODE_U2_BLANK))
		envp = envp_aod;
}
#endif // CONFIG_LGE_DISPLAY_AOD_SUPPORTED
