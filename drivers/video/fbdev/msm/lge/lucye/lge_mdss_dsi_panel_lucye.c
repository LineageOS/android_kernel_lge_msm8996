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

#include <linux/delay.h>
#include "../../mdss_dsi.h"
#include "../lge_mdss_display.h"
#include "../../mdss_dba_utils.h"
#include <linux/input/lge_touch_notify.h>
#include <soc/qcom/lge/board_lge.h>
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
#include "../lge_reader_mode.h"
extern struct dsi_panel_cmds reader_mode_step0_cmds;
extern struct dsi_panel_cmds reader_mode_step1_cmds;
extern struct dsi_panel_cmds reader_mode_step2_cmds;
extern struct dsi_panel_cmds reader_mode_step3_cmds;
extern struct dsi_panel_cmds reader_mode_step4_cmds;
extern struct dsi_panel_cmds reader_mode_step5_cmds;
extern struct dsi_panel_cmds reader_mode_step6_cmds;
extern struct dsi_panel_cmds reader_mode_step7_cmds;
extern struct dsi_panel_cmds reader_mode_step8_cmds;
extern struct dsi_panel_cmds reader_mode_step9_cmds;
extern struct dsi_panel_cmds reader_mode_step10_cmds;
#endif

#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
extern void lcd_watch_restore_reg_after_panel_reset(void);
extern void lcd_watch_font_crc_check_after_panel_reset(void);
#endif

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds, u32 flags);

/* when panel state is off, improve to ignore sre cmds */
void lge_set_sre_cmds(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char mask = 0x00;
	if(ctrl->sre_status == SRE_MID) {
		ctrl->reg_55h_cmds.cmds[0].payload[1] |= SRE_MASK_MID;
		pr_info("%s: SRE MID\n",__func__);
	} else if(ctrl->sre_status == SRE_HIGH) {
		ctrl->reg_55h_cmds.cmds[0].payload[1] |= SRE_MASK_HIGH;
		pr_info("%s: SRE HIGH\n",__func__);
	} else {
		mask = SRE_MASK;
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
	}
}
EXPORT_SYMBOL(lge_set_sre_cmds);

static void lge_set_image_quality_cmds(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char mask = 0x00;
#if defined(CONFIG_LGE_DISPLAY_READER_MODE)
	switch(lge_get_reader_mode()) {
		case READER_MODE_STEP_1:
			pr_info("%s: Reader STEP 1\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step1_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_2:
			pr_info("%s: Reader STEP 2\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step2_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_3:
			pr_info("%s: Reader STEP 3\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step3_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_4:
			pr_info("%s: Reader STEP 4\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step4_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_5:
			pr_info("%s: Reader STEP 5\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step5_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_6:
			pr_info("%s: Reader STEP 6\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step6_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_7:
			pr_info("%s: Reader STEP 7\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step7_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_8:
			pr_info("%s: Reader STEP 8\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step8_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_9:
			pr_info("%s: Reader STEP 9\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step9_cmds, CMD_REQ_COMMIT);
			break;
		case READER_MODE_STEP_10:
			pr_info("%s: Reader STEP 10\n",__func__);
			ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
			mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step10_cmds, CMD_REQ_COMMIT);
			break;
		default:
			pr_info("%s: Reader STEP OFF\n",__func__);
			break;
	}
#endif
#if defined(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
	if (ctrl->reg_f2h_cmds.cmds[0].payload[3] == SHARPNESS_VALUE) {
		pr_info("%s: Sharpness = 0x%02x \n",__func__, SHARPNESS_VALUE);
		ctrl->reg_f2h_cmds.cmds[0].payload[3] = SHARPNESS_VALUE;
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f2h_cmds, CMD_REQ_COMMIT);
		goto send;
	}
#endif
#if defined(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
	if (ctrl->ie_on == 0) {
		pr_info("%s: IE OFF => SAT:OFF, SH:OFF \n",__func__);
		mask = (SH_MASK | SAT_MASK);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
		goto send;
	}
#endif
#if defined(CONFIG_LGE_DISPLAY_DOLBY_MODE)
	if(ctrl->dolby_status > 0) {
		pr_info("%s: Dolby Mode ON\n", __func__);
		/* Dolby Setting : CABC OFF, SRE OFF */
		mask = (CABC_MASK | SRE_MASK);
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		mask = (SH_MASK | SAT_MASK);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_fbh_cmds.cmds[0].payload[4] = CABC_OFF_VALUE;
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f2h_cmds, CMD_REQ_COMMIT);
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f3h_cmds, CMD_REQ_COMMIT);
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_fbh_cmds, CMD_REQ_COMMIT);
		pr_info("%s: Dolby=%d 55h = 0x%02x, f0h = 0x%02x\n",__func__,ctrl->dolby_status,
			ctrl->reg_55h_cmds.cmds[0].payload[1],ctrl->reg_f0h_cmds.cmds[0].payload[1]);
		goto send;
	}
#endif
#if defined(CONFIG_LGE_DISPLAY_HDR_MODE)
	if(ctrl->hdr_status > 0) {
		pr_info("%s: HDR Mode ON\n", __func__);
		/* Dolby Setting : CABC OFF, SRE OFF, SAT OFF, SH OFF */
		mask = (CABC_MASK | SRE_MASK);
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		mask = (SH_MASK | SAT_MASK);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_fbh_cmds.cmds[0].payload[4] = CABC_OFF_VALUE;
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_fbh_cmds, CMD_REQ_COMMIT);
		pr_info("%s: HDR=%d 55h = 0x%02x, f0h = 0x%02x\n",__func__,ctrl->dolby_status,
				ctrl->reg_55h_cmds.cmds[0].payload[1],ctrl->reg_f0h_cmds.cmds[0].payload[1]);
		goto send;
	}
#endif
send:
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f0h_cmds, CMD_REQ_COMMIT);
	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);

}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_RESET)
static int mdss_dsi_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		rc = gpio_request(ctrl_pdata->disp_en_gpio,
				"disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
					rc);
			goto disp_en_gpio_err;
		}
	}
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
		rc = gpio_request(ctrl_pdata->bklt_en_gpio,
				"bklt_enable");
		if (rc) {
			pr_err("request bklt gpio failed, rc=%d\n",
					rc);
			goto bklt_en_gpio_err;
		}
	}
	if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
		rc = gpio_request(ctrl_pdata->mode_gpio, "panel_mode");
		if (rc) {
			pr_err("request panel mode gpio failed,rc=%d\n",
					rc);
			goto mode_gpio_err;
		}
	}

	return rc;

mode_gpio_err:
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		gpio_free(ctrl_pdata->bklt_en_gpio);
bklt_en_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
disp_en_gpio_err:
	return rc;
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (mdss_dsi_is_right_ctrl(ctrl_pdata) &&
			mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		pr_debug("%s:%d, right ctrl gpio configuration not needed\n",
				__func__, __LINE__);
		return rc;
	}

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
				__func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
				__func__, __LINE__);
		return rc;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
		if (!pinfo->cont_splash_enabled) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
				rc = gpio_direction_output(
					ctrl_pdata->disp_en_gpio, 1);
					if (rc) {
					pr_err("%s: unable to set dir for en gpio\n",
						__func__);
					goto exit;
				}
			}
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
			/* Only panel reset when U0 -> U3 or U0 -> U2 Unblank*/
			if (pinfo->aod_cmd_mode == ON_CMD ||
				pinfo->aod_cmd_mode == ON_AND_AOD ||
							pinfo->panel_dead) {
				/* Notify to Touch when panel reset */
				if (touch_notifier_call_chain(NOTIFY_TOUCH_RESET, NULL))
					pr_err("Failt to send notify to touch\n");
				for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
					gpio_set_value((ctrl_pdata->rst_gpio),
							pdata->panel_info.rst_seq[i]);
					if (pdata->panel_info.rst_seq[++i])
						usleep_range(pinfo->rst_seq[i] * 1000,
							pinfo->rst_seq[i] * 1000);
				}
			}

			if (pinfo->power_ctrl || pinfo->panel_dead ||
				pinfo->aod_labibb_ctrl) {
				usleep_range(5000,5000);

				rc = msm_dss_enable_vreg(
						ctrl_pdata->panel_power_data.vreg_config,
						ctrl_pdata->panel_power_data.num_vreg, 1);
				if (rc) {
					pr_err("%s: failed to enable vregs for %s\n",
							__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
				} else {
					pr_info("%s: enable vregs for %s\n",
							__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
				}
			}
#else
			/* Notify to Touch when panel reset */
			if (touch_notifier_call_chain(NOTIFY_TOUCH_RESET, NULL))
				pr_err("Failt to send notify to touch\n");
			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
						pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000,
						pinfo->rst_seq[i] * 1000);
			}

			if (pinfo->power_ctrl || pinfo->panel_dead) {
				usleep_range(5000,5000);

				rc = msm_dss_enable_vreg(
						ctrl_pdata->panel_power_data.vreg_config,
						ctrl_pdata->panel_power_data.num_vreg, 1);
				if (rc) {
					pr_err("%s: failed to enable vregs for %s\n",
							__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
				} else {
					pr_info("%s: enable vregs for %s\n",
							__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
				}
			}
#endif
			if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
				rc = gpio_direction_output(
					ctrl_pdata->bklt_en_gpio, 1);
				if (rc) {
					pr_err("%s: unable to set dir for bklt gpio\n",
						__func__);
					goto exit;
				}
			}
		}
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		if (pinfo->cont_splash_enabled) {
			usleep_range(5000,5000);
			rc = msm_dss_enable_vreg(
					ctrl_pdata->panel_power_data.vreg_config,
					ctrl_pdata->panel_power_data.num_vreg, 1);
			if (rc) {
				pr_err("%s: failed to enable vregs for %s\n",
						__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
			} else {
				pr_info("%s: enable vregs for %s\n",
						__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
			}
		}
#endif
		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			bool out = false;

			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				out = true;
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				out = false;

			rc = gpio_direction_output(ctrl_pdata->mode_gpio, out);
			if (rc) {
				pr_err("%s: unable to set dir for mode gpio\n",
					__func__);
				goto exit;
			}
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
					__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
			gpio_set_value((ctrl_pdata->bklt_en_gpio), 0);
			gpio_free(ctrl_pdata->bklt_en_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
			gpio_free(ctrl_pdata->disp_en_gpio);
		}
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		gpio_free(ctrl_pdata->rst_gpio);
		if (gpio_is_valid(ctrl_pdata->mode_gpio))
			gpio_free(ctrl_pdata->mode_gpio);
	}

exit:
	return rc;
}
#endif


#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_ON)
int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	struct dsi_panel_cmds *on_cmds;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	pr_err("[Display] %s: ndx=%d\n", __func__, ctrl->ndx);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

	on_cmds = &ctrl->on_cmds;
	if ((pinfo->mipi.dms_mode == DYNAMIC_MODE_SWITCH_IMMEDIATE) &&
			(pinfo->mipi.boot_mode != pinfo->mipi.mode))
		on_cmds = &ctrl->post_dms_on_cmds;

	pr_debug("%s: ndx=%d cmd_cnt=%d\n", __func__,
			ctrl->ndx, on_cmds->cmd_cnt);
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	switch (pinfo->aod_cmd_mode) {
		case AOD_CMD_ENABLE:
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U3_TO_U2], CMD_REQ_COMMIT);
			goto notify;
		case AOD_CMD_DISABLE:
			lge_set_sre_cmds(ctrl);
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
			lge_change_reader_mode(ctrl, lge_get_reader_mode());
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U2_TO_U3], CMD_REQ_COMMIT);
			goto notify;
		case ON_AND_AOD:
			lcd_watch_font_crc_check_after_panel_reset();
			lcd_watch_restore_reg_after_panel_reset();
			if (ctrl->display_on_and_aod_comds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl, &ctrl->display_on_and_aod_comds, CMD_REQ_COMMIT);
			lge_set_sre_cmds(ctrl);
			lge_set_image_quality_cmds(ctrl);

			if (pinfo->compression_mode == COMPRESSION_DSC)
				mdss_dsi_panel_dsc_pps_send(ctrl, pinfo);
			if (ctrl->ds_registered && pinfo->is_pluggable)
				 mdss_dba_utils_video_on(pinfo->dba_data, pinfo);
			goto notify;
		case ON_CMD:
			lcd_watch_font_crc_check_after_panel_reset();
			break;
		case CMD_SKIP:
			goto notify;
		default:
			pr_err("[AOD] Unknown Mode : %d\n", pinfo->aod_cmd_mode);
			break;
	}
#endif
	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds, CMD_REQ_COMMIT);
	lge_set_sre_cmds(ctrl);
	lge_set_image_quality_cmds(ctrl);

	if (pinfo->compression_mode == COMPRESSION_DSC)
		mdss_dsi_panel_dsc_pps_send(ctrl, pinfo);
	if (ctrl->ds_registered && pinfo->is_pluggable)
		mdss_dba_utils_video_on(pinfo->dba_data, pinfo);
	if (ctrl->display_on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->display_on_cmds, CMD_REQ_COMMIT);
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
notify:
	{
		int param;
		param = pinfo->aod_cur_mode;
		if(touch_notifier_call_chain(LCD_EVENT_LCD_MODE,
							(void *)&param))
			pr_err("[AOD] Failt to send notify to touch\n");
	}
#endif

end:
	pr_err("[Display] %s:-\n", __func__);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_OFF)
int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	pr_err("[Display] %s: ctrl=%pK ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	switch (pinfo->aod_cmd_mode) {
		case AOD_CMD_ENABLE:
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U3_TO_U2], CMD_REQ_COMMIT);
			goto notify;
		case AOD_CMD_DISABLE:
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U2_TO_U3], CMD_REQ_COMMIT);
			goto notify;
		case OFF_CMD:
			break;
		case CMD_SKIP:
			goto notify;
		default:
			pr_err("[AOD] Unknown Mode : %d\n", pinfo->aod_cmd_mode);
			break;
	}
#endif
	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds, CMD_REQ_COMMIT);

	if (ctrl->ds_registered && pinfo->is_pluggable) {
		mdss_dba_utils_video_off(pinfo->dba_data);
		mdss_dba_utils_hdcp_enable(pinfo->dba_data, false);
	}

#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
notify:
	{
		int param;
		param = pinfo->aod_cur_mode;
		if(touch_notifier_call_chain(LCD_EVENT_LCD_MODE,
						(void *)&param))
			pr_err("[AOD] Failt to send notify to touch\n");
	}
#endif
end:
	pr_err("[Display] %s:-\n", __func__);
	return 0;
}
#endif
