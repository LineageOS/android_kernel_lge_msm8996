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
#ifdef CONFIG_LGE_PM_LGE_POWER_CORE
#include <soc/qcom/lge/power/lge_power_class.h>
#include <soc/qcom/smem.h>
#endif
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
#include <soc/qcom/lge/power/lge_cable_detect.h>
#else
#include <soc/qcom/lge/lge_cable_detection.h>
#endif
#include <linux/module.h>
#include <linux/power/lge_battery_id.h>
#include <linux/lge_display_debug.h>
#include "lge_mdss_display.h"

extern int get_factory_cable(void);
#define LGEUSB_FACTORY_56K 1
#define LGEUSB_FACTORY_130K 2
#define LGEUSB_FACTORY_910K 3

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
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
	cable_info = get_factory_cable();
#if !defined(CONFIG_LGE_PM_EMBEDDED_BATTERY)
		if (cable_info == LGEUSB_FACTORY_56K ||
			cable_info == LGEUSB_FACTORY_130K ||
			cable_info == LGEUSB_FACTORY_910K) {
			pr_info("%s : cable_type = factory(%d) \n",__func__, cable_info);
			return true;
		} else {
			return false;
		}
#else
		if (cable_info == LGEUSB_FACTORY_130K ||
			cable_info == LGEUSB_FACTORY_910K) {
			pr_info("%s : cable_type = factory(%d) \n",__func__, cable_info);
			return true;
		} else {
			return false;
		}
#endif
#elif defined (CONFIG_LGE_PM_CABLE_DETECTION)
	cable_info = lge_pm_get_cable_type();

#if !defined(CONFIG_LGE_PM_EMBEDDED_BATTERY)
	if (cable_info == CABLE_56K ||
		cable_info == CABLE_130K ||
		cable_info == CABLE_910K)
#else
	if (cable_info == CABLE_130K ||
		cable_info == CABLE_910K)
#endif
		return true;
	else
#else
	cable_info = NO_INIT_CABLE;
#endif
		return false;
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

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
static bool batt_present = false;
bool lge_battery_present(void){
	struct lge_power *lge_batt_id_lpc;
	union lge_power_propval	lge_val = {0,};
	uint *smem_batt = 0;
	uint _smem_batt_id = 0;
	int rc;
	if (batt_present == true){
		return true;
		}
	lge_batt_id_lpc = lge_power_get_by_name("lge_batt_id");
	if (lge_batt_id_lpc) {
		rc = lge_batt_id_lpc->get_property(lge_batt_id_lpc,
				LGE_POWER_PROP_PRESENT, &lge_val);
		batt_present = lge_val.intval;
	}else{
		pr_err("[MDSS] Failed to get batt presnet property\n");
		smem_batt = (uint *)smem_alloc(SMEM_BATT_INFO,
				sizeof(smem_batt), 0, SMEM_ANY_HOST_FLAG);
		if (smem_batt == NULL) {
			pr_err("[MDSS] smem_alloc returns NULL\n");
			batt_present  = false;
		} else {
			_smem_batt_id = *smem_batt;
			pr_err("[MDSS] Battery was read in sbl is = %d\n",
					_smem_batt_id);
			if (_smem_batt_id == BATT_NOT_PRESENT) {
				pr_err("[MDSS] Set batt_id as DEFAULT\n");
				batt_present = false;
			}else{
				batt_present = true;
			}
		}
	}
	return batt_present;
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
		pr_info("%s: lcd dimming mode. set value = %d\n",
							__func__, br_lvl);
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
	} else if (is_factory_cable()
#if !defined(CONFIG_LGE_PM_EMBEDDED_BATTERY)
					&& !lge_battery_present()
#endif
#elif defined (CONFIG_LGE_PM_BATTERY_ID_CHECKER)
	} else if (is_factory_cable() && !lge_battery_check()
#else
	} else if (is_factory_cable()
#endif
			&& !is_blank_called()) {
		br_lvl = 1;
		pr_info("%s: Detect factory cable. set value = %d\n",
							__func__, br_lvl);
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
		pr_info("[AOD] br_lvl(%d) -> bl_lvl(%d)\n", br_lvl, bl_lvl);
	else
		pr_info("%s: br_lvl(%d) -> bl_lvl(%d)\n", lge_get_blmapname(blmaptype), br_lvl, bl_lvl);
#else
	pr_info("%s: br_lvl(%d) -> bl_lvl(%d)\n", lge_get_blmapname(blmaptype), br_lvl, bl_lvl);
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
		pr_info("%s: do not control backlight (bl: %d)\n", __func__,
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
		pr_info("%s: do not control backlight (bl: %d)\n", __func__,
			lge_bl_lvl_unset_ex);
		return 1;
	}
	return 0;
}
#endif

static ssize_t mdss_fb_get_bl_off_and_block(struct device *dev,
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

static ssize_t mdss_fb_set_bl_off_and_block(struct device *dev,
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
		pr_info("[%s] status : %d, brightness : 0 \n", __func__,
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
		pr_info("[%s] status : %d, brightness : %d \n", __func__,
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

static DEVICE_ATTR(sp_link_backlight_off, S_IRUGO | S_IWUSR,
	mdss_fb_get_bl_off_and_block, mdss_fb_set_bl_off_and_block);
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
		pr_info("%s: RECOVERY when screen crack occured.\n",__func__);
		lge_panel_recovery_mode();
	} else {
		pr_info("%s: NO RECOVERY\n",__func__);
	}

	return count;
}
static DEVICE_ATTR(recovery_mode, S_IWUSR|S_IRUGO, get_recovery_mode, set_recovery_mode);

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

	pr_info("%s: RECOVERY when screen crack occured.\n",__func__);
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
			pr_info("[AOD] Skip ext-BL ctrl except U2 & U2unblank mode\n");
			return;
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
			DISP_DEBUG(BL, "backlight sent to panel ex:%d\n", temp);
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
		pr_info("[AOD] Save unset Level : %d\n", value);
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
#if defined (CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
	int current_bl;
#endif
	if (mfd->panel_info->aod_cur_mode ==
				AOD_PANEL_MODE_U2_BLANK && mfd->index == 0) {
		pr_info("[AOD] Don't off backlight when U2 Blank\n");
		mfd->unset_bl_level = U32_MAX;
#if defined (CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mfd->unset_bl_level_ex = U32_MAX;
#endif
	}
#if defined (CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
	else if(mfd->index == 0) {
		pr_info("%s: backlight backup: bl_level %d\n", __func__, mfd->bl_level);
		current_bl = mfd->bl_level;
		mfd->allow_bl_update = true;
		mdss_fb_set_backlight(mfd, 0);
		mfd->allow_bl_update = false;
		mfd->unset_bl_level = current_bl;
#if defined (CONFIG_LGE_DISPLAY_BL_EXTENDED)
		pr_info("%s: backlight backup: bl_level_ex %d\n", __func__, mfd->bl_level_ex);
		current_bl = mfd->bl_level_ex;
		mfd->allow_bl_update_ex = true;
		mdss_fb_set_backlight_ex(mfd, 0);
		mfd->allow_bl_update_ex = false;
		mfd->unset_bl_level_ex = current_bl;
#endif
	}
#else
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
#endif
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
		pr_info("%s: lcd dimming mode. set value = %d\n",
							__func__, br_lvl);
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER
	} else if (is_factory_cable()
#if !defined(CONFIG_LGE_PM_EMBEDDED_BATTERY)
					&& !lge_battery_present()
#endif
#elif defined (CONFIG_LGE_PM_BATTERY_ID_CHECKER)
	} else if (is_factory_cable() && !lge_battery_check()
#else
	} else if (is_factory_cable()
#endif
			&& !is_blank_called()) {
		br_lvl = 1;
		pr_info("%s: Detect factory cable. set value = %d\n",
							__func__, br_lvl);
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

	pr_info("%s: br_lvl(%d) -> bl_lvl(%d)\n", lge_get_blmapname(blmaptype),
						br_lvl, bl_lvl);
	return bl_lvl;
	return bl_lvl;
}
#endif
#endif
/*---------------------------------------------------------------------------*/
/* High luminance function                                                   */
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_LGE_HIGH_LUMINANCE_MODE)
static ssize_t hl_mode_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[hl_mode] no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", pinfo->hl_mode_on);
	return ret;
}

static ssize_t hl_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[hl_mode] no panel connected!\n");
		return len;
	}

	pinfo->hl_mode_on = simple_strtoul(buf, NULL, 10);

	if(pinfo->hl_mode_on == 1)
		pr_info("[hl_mode] hl_mode on\n");
	else
		pr_info("[hl_mode] hl_mode off\n");

	return len;
}

static DEVICE_ATTR(hl_mode, S_IWUSR|S_IRUGO, hl_mode_show, hl_mode_store);
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
			pr_info("Main Backlight already off. Or HL mode. No need to Set\n");
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
static ssize_t mdss_fb_get_cur_panel_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int cur_panel_mode;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}
	if(mfd->index != 0)
		return -EINVAL;

	mutex_lock(&mfd->aod_lock);
	if (pinfo->aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK)
		cur_panel_mode = AOD_PANEL_MODE_U2_BLANK;
	else
		cur_panel_mode = pinfo->aod_cur_mode;
#if defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	if (cur_panel_mode == AOD_PANEL_MODE_U3_UNBLANK && pinfo->ext_off_temp)
		cur_panel_mode = AOD_PANEL_MODE_U1_UNBLANK;
#endif
	mutex_unlock(&mfd->aod_lock);
	pr_info("[AOD] %s : Current panel mode : %d\n", __func__ , cur_panel_mode);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
		cur_panel_mode);

	return ret;
}
static ssize_t mdss_fb_set_aod(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	int old_value, new_value;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &new_value) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}
	mutex_lock(&mfd->aod_lock);
	old_value = pdata->panel_info.aod_node_from_user;
	pdata->panel_info.aod_node_from_user = new_value;
	mutex_unlock(&mfd->aod_lock);
	pr_info("[AOD] old value : %d, new value : %d\n", old_value, new_value);
	return len;
}
static ssize_t mdss_fb_get_aod(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
		pinfo->aod_node_from_user);

	return ret;
}

static ssize_t mdss_fb_set_keep_aod(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	int old_value, new_value;
	int rc;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &new_value) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}
	mutex_lock(&mfd->aod_lock);
	old_value = pdata->panel_info.aod_keep_u2;
	pdata->panel_info.aod_keep_u2 = new_value;
	mutex_unlock(&mfd->aod_lock);
	pr_info("[AOD] keep_aod old value : %d, new value : %d\n", old_value, new_value);
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	if (new_value == AOD_MOVE_TO_U3)
		mfd->ready_to_u2 = false;
#endif
	/* Current mode is  AOD_PANEL_MODE_U2_UNBLANK and if set keep_aod by 0,
	     We have to U2-> U3 command only */
#if defined(CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
	if ((pdata->panel_info.aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK ||
		 (pdata->panel_info.aod_cur_mode == AOD_PANEL_MODE_U3_UNBLANK &&
		  pdata->panel_info.mipi.mode == DSI_CMD_MODE)) &&
#else
	if (pdata->panel_info.aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK &&
#endif
		new_value == AOD_MOVE_TO_U3) {
		struct mdss_dsi_ctrl_pdata *ctrl;

		ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

#if defined(CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
		pr_info("[AOD] pdata->panel_info.aod_cur_mode : %d, pdata->panel_info.mipi.mode : %d\n",
						pdata->panel_info.aod_cur_mode, pdata->panel_info.mipi.mode);
		pdata->panel_info.mode_switch = CMD_TO_VIDEO;
		rc = mdss_fb_mode_switch(mfd, CMD_TO_VIDEO);
		if (rc)
			pr_err("[AOD] Fail to change mode from command to video\n");
#endif
		mutex_lock(&mfd->aod_lock);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		// this flag will reset 0 in full frame kickoff(__validate_roi_and_set function).
		mfd->keep_aod_pending = true;
		pr_info("keep_aod_pending set to true\n");
#endif
#if !defined(CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
		rc = oem_mdss_aod_cmd_send(mfd, AOD_CMD_DISABLE);
		if (rc)
			pr_err("[AOD] Fail to send U2->U3 command\n");
#endif
		oem_mdss_aod_set_backlight_mode(mfd);
		pdata->panel_info.aod_keep_u2 = AOD_NO_DECISION;
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
		/* When Move to U3 mode from U2 unblank */
		/* 1. Turn off backlight	*/
		/* 2. Block aod bl control */
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, 0);
		mutex_unlock(&mfd->bl_lock);
		mfd->block_aod_bl = true;
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, mfd->br_lvl_ex);
		mutex_unlock(&mfd->bl_lock);
#endif
		mutex_unlock(&mfd->aod_lock);
	}
	return len;
}
static ssize_t mdss_fb_get_keep_aod(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}
	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
		pinfo->aod_keep_u2);

	return ret;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
static ssize_t mdss_fb_lge_set_mode_switch(struct device *dev,
		     struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	int new_value;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &new_value) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}
	pdata->panel_info.mode_switch = new_value;

	pr_info("mode_switch : %d\n", new_value);

	if (new_value == CMD_TO_VIDEO) { //case1. CMD TO VIDEO
		pr_info("aod_mode = %d, mode_switch=%d\n",pdata->panel_info.aod_cur_mode,pdata->panel_info.mode_switch);
		mdss_fb_mode_switch(mfd,new_value);
	}
	else if (new_value == VIDEO_TO_CMD) { //case2. VIDEO TO CMD
		pr_info("aod_mode = %d, mode_switch=%d\n",pdata->panel_info.aod_cur_mode,pdata->panel_info.mode_switch);
		mdss_fb_mode_switch(mfd,new_value);
	}
	else{
		pr_info("unexpected mode switch");
	}

	return len;
}
static ssize_t mdss_fb_lge_get_mode_switch(struct device *dev,
		          struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}
	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			pinfo->mode_switch);

	return ret;
}
#endif

void lge_mdss_fb_aod_release(struct msm_fb_data_type *mfd)
{
		if (mfd->index == 0) {
			pr_info("[AOD] Disable AOD mode when shutdown\n");
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

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
static ssize_t mdss_fb_toggle_u1(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ext_off;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &ext_off) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}

	pr_info("[AOD]%s called: ext_off: %d\n", __func__, ext_off);
	mutex_lock(&mfd->aod_lock);
	if (pinfo->ext_off_temp != ext_off) {
		pr_info("%d -> %d\n", pinfo->ext_off_temp, ext_off);
		pinfo->ext_off_temp = ext_off;
		oem_mdss_aod_set_backlight_mode(mfd);
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, mfd->br_lvl_ex);
		mutex_unlock(&mfd->bl_lock);
	}
	mutex_unlock(&mfd->aod_lock);
	return len;
}

static ssize_t mdss_fb_get_toggle_u1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			pinfo->ext_off_temp);

	return ret;
}

static ssize_t mdss_fb_set_ext_off(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ext_off;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &ext_off) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}

	pr_info("[AOD]%s called: ext_off: %d\n", __func__, ext_off);
	mutex_lock(&mfd->aod_lock);
	if (pinfo->ext_off != ext_off) {
		pr_info("%d -> %d\n", pinfo->ext_off, ext_off);
		pinfo->ext_off = ext_off;
		pinfo->ext_off_temp = ext_off;
		oem_mdss_aod_set_backlight_mode(mfd);
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, mfd->br_lvl_ex);
		mutex_unlock(&mfd->bl_lock);
	}
	mutex_unlock(&mfd->aod_lock);
	return len;
}

static ssize_t mdss_fb_get_ext_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", pinfo->ext_off);

	return ret;
}
#endif

static DEVICE_ATTR(cur_panel_mode, S_IRUGO, mdss_fb_get_cur_panel_mode, NULL);
static DEVICE_ATTR(aod, S_IWUSR|S_IRUGO, mdss_fb_get_aod, mdss_fb_set_aod);
static DEVICE_ATTR(keep_aod, S_IWUSR|S_IRUGO, mdss_fb_get_keep_aod,
			mdss_fb_set_keep_aod);
#if defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
static DEVICE_ATTR(toggle_u1, S_IWUSR|S_IRUGO, mdss_fb_get_toggle_u1,
			mdss_fb_toggle_u1);
static DEVICE_ATTR(ext_off, S_IWUSR|S_IRUGO, mdss_fb_get_ext_off,
			mdss_fb_set_ext_off);
#if defined (CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
static DEVICE_ATTR(mode_switch, S_IWUSR|S_IRUGO, mdss_fb_lge_get_mode_switch,
			mdss_fb_lge_set_mode_switch);
#endif
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Register lge sysfs attributes                                             */
/*---------------------------------------------------------------------------*/
/* TODO: implement registering method for attributes in other lge files */
static struct attribute *lge_mdss_fb_attrs[] = {
#if defined(CONFIG_LGE_SP_MIRRORING_CTRL_BL)
	&dev_attr_sp_link_backlight_off.attr,
#endif
#if defined(CONFIG_LGE_HIGH_LUMINANCE_MODE)
	&dev_attr_hl_mode.attr,
#endif
#if defined(CONFIG_LGE_PANEL_RECOVERY)
	&dev_attr_recovery_mode.attr,
#endif
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	&dev_attr_aod.attr,
	&dev_attr_cur_panel_mode.attr,
	&dev_attr_keep_aod.attr,
#if defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	&dev_attr_toggle_u1.attr,
	&dev_attr_ext_off.attr,
#if defined (CONFIG_LGE_DISPLAY_DYN_DSI_MODE_SWITCH)
	&dev_attr_mode_switch.attr,
#endif
#endif
#endif
	NULL,
};

static struct attribute_group lge_mdss_fb_attr_group = {
	.attrs = lge_mdss_fb_attrs,
};

int lge_mdss_fb_create_sysfs(struct msm_fb_data_type *mfd)
{
	int rc;

	rc = sysfs_create_group(&mfd->fbi->dev->kobj, &lge_mdss_fb_attr_group);
	if (rc)
		pr_err("lge sysfs group creation failed, rc=%d\n", rc);
	return rc;
}

void lge_mdss_fb_remove_sysfs(struct msm_fb_data_type *mfd)
{
	sysfs_remove_group(&mfd->fbi->dev->kobj, &lge_mdss_fb_attr_group);
}
