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

#ifndef LGE_MDSS_FB_H
#define LGE_MDSS_FB_H

int lge_br_to_bl(struct msm_fb_data_type *mfd, int br_lvl);
#if defined(CONFIG_LGE_SP_MIRRORING_CTRL_BL)
int lge_is_bl_update_blocked(int bl_lvl);
void lge_set_bl_update_blocked(bool enable);
#endif
#ifdef CONFIG_LGE_LCD_OFF_DIMMING
void lge_set_blank_called(void);
#endif
void lge_mdss_fb_init(struct msm_fb_data_type *mfd);
void lge_set_backlight(int bl_level);
void lge_backlight_register(struct msm_fb_data_type *mfd);
void lge_backlight_unregister(void);
void lge_aod_bl_ctrl_blank_blank(struct msm_fb_data_type *mfd);

#if defined(CONFIG_LGE_PP_AD_SUPPORTED)
void lge_mdss_fb_ad_set_brightness(struct msm_fb_data_type *mfd, u32 amb_light,
				int ad_on);
#endif

void lge_mdss_sysfs_remove(struct msm_fb_data_type *mfd);
int lge_mdss_sysfs_init(struct msm_fb_data_type *mfd);

#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
void lge_mdss_fb_aod_release(struct msm_fb_data_type *mfd);
void lge_mdss_fb_aod_recovery(struct msm_fb_data_type *mfd, char *envp[]);
void mdss_fb_set_bl_brightness_aod_sub(struct msm_fb_data_type *mfd,
				      enum led_brightness value);
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
int lge_is_bl_update_blocked_ex(int bl_lvl);
int lge_br_to_bl_ex(struct msm_fb_data_type *mfd, int br_lvl);
void mdss_fb_set_backlight_ex(struct msm_fb_data_type *mfd, u32 bkl_lvl);
void mdss_fb_update_backlight_ex(struct msm_fb_data_type *mfd);
#endif
#endif
#if defined(CONFIG_LGE_PANEL_RECOVERY)
bool lge_panel_recovery_mode(void);
#endif
#ifdef CONFIG_LGE_PM_SUPPORT_LG_POWER_CLASS
int lge_charger_present(void);
#endif
#endif /* LGE_MDSS_FB_H */
