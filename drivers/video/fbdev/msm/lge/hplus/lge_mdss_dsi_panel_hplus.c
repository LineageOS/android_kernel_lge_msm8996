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
#endif

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
	if (gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio)) {
		rc = gpio_request(ctrl_pdata->lcd_mode_sel_gpio, "panel_mode");
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
extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds, u32 flags);

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

	pr_err("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("[Display] gpio request failed\n");
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
			if (pinfo->aod_cmd_mode != ON_CMD &&
				pinfo->aod_cmd_mode != ON_AND_AOD &&
							!pinfo->panel_dead) {
				pr_debug("[Display] reset skip..\n");
				return rc;
			}
#endif
			/* Notify to Touch when panel reset */
			if (touch_notifier_call_chain(NOTIFY_TOUCH_RESET, NULL))
				pr_err("[Display] Failt to send notify to touch\n");
			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
						pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000,
						pinfo->rst_seq[i] * 1000);
			}

#ifdef CONFIG_LGE_LCD_POWER_CTRL
			if (pinfo->power_ctrl || pinfo->panel_dead) {
				usleep_range(5000,5000);

				rc = msm_dss_enable_vreg(
						ctrl_pdata->panel_power_data.vreg_config,
						ctrl_pdata->panel_power_data.num_vreg, 1);
				if (rc) {
					pr_err("%s: failed to enable vregs for %s\n",
							__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
				} else {
					pr_debug("%s: enable vregs for %s\n",
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

		if (gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio)) {
			bool out = false;

			if (pinfo->mode_sel_state == MODE_GPIO_HIGH)
				out = true;
			else if (pinfo->mode_sel_state == MODE_GPIO_LOW)
				out = false;

			rc = gpio_direction_output(ctrl_pdata->lcd_mode_sel_gpio, out);
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
		if (gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio))
			gpio_free(ctrl_pdata->lcd_mode_sel_gpio);
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
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U2_TO_U3], CMD_REQ_COMMIT);
			goto notify;
		case ON_AND_AOD:
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
			lge_mdss_dsi_panel_send_on_cmds(ctrl, on_cmds, lge_get_reader_mode());
#else
			if (on_cmds->cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl, on_cmds, CMD_REQ_COMMIT);
#endif

#if defined(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
			if (ctrl->ie_on == 0)
			{
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
				if(lge_get_reader_mode()){
					pr_debug("[Display]%s: reader mode on\n",__func__);
					ctrl->ie_off_cmds.cmds[1].payload[1] = 0x81;
				}
				else
				{
					pr_debug("[Display]%s: reader mode off\n",__func__);
					ctrl->ie_off_cmds.cmds[1].payload[1] = 0x01;
				}
#endif
				mdss_dsi_panel_cmds_send(ctrl, &ctrl->ie_off_cmds, CMD_REQ_COMMIT);
			}
#endif
#if defined(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
			if (ctrl->sharpness_on_cmds.cmds[2].payload[3] == 0x29)
			{
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
				if(lge_get_reader_mode())
					ctrl->sharpness_on_cmds.cmds[1].payload[1] = 0x8A;
				else
					ctrl->sharpness_on_cmds.cmds[1].payload[1] = 0x82;
#endif
				mdss_dsi_panel_cmds_send(ctrl, &ctrl->sharpness_on_cmds, CMD_REQ_COMMIT);
			}
#endif
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U0_TO_U2], CMD_REQ_COMMIT);

			if (pinfo->compression_mode == COMPRESSION_DSC)
				mdss_dsi_panel_dsc_pps_send(ctrl, pinfo);
			if (ctrl->ds_registered && pinfo->is_pluggable)
				 mdss_dba_utils_video_on(pinfo->dba_data, pinfo);
			goto notify;
		case ON_CMD:
			break;
		case CMD_SKIP:
#if defined(CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED)
			if (pinfo->mq_mode)
				oem_mdss_mq_cmd_unset(ctrl);
#endif
			/*fps to 60 */
			pr_debug("[Display] FPS changed to 60\n");
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_FPS_60], CMD_REQ_COMMIT);
			goto notify;
		default:
			pr_err("[AOD] Unknown Mode : %d\n", pinfo->aod_cmd_mode);
			break;
	}
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
	lge_mdss_dsi_panel_send_on_cmds(ctrl, on_cmds, lge_get_reader_mode());
#else
	if (on_cmds->cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, on_cmds, CMD_REQ_COMMIT);
#endif
#if defined(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
	if (ctrl->ie_on == 0)
	{
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
				if(lge_get_reader_mode()){
					pr_debug("[Display]%s: reader mode on\n",__func__);
					ctrl->ie_off_cmds.cmds[1].payload[1] = 0x81;
				}
				else
				{
					pr_debug("[Display]%s: reader mode off\n",__func__);
					ctrl->ie_off_cmds.cmds[1].payload[1] = 0x01;
				}
#endif
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->ie_off_cmds, CMD_REQ_COMMIT);
	}
#endif
#if defined(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
	if (ctrl->sharpness_on_cmds.cmds[2].payload[3] == 0x29)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->sharpness_on_cmds, CMD_REQ_COMMIT);
#endif

	if (ctrl->display_on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->display_on_cmds, CMD_REQ_COMMIT);

	if (pinfo->compression_mode == COMPRESSION_DSC)
		mdss_dsi_panel_dsc_pps_send(ctrl, pinfo);

	if (ctrl->ds_registered && pinfo->is_pluggable)
		mdss_dba_utils_video_on(pinfo->dba_data, pinfo);
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
			/* fps to 30 */
			pr_debug("[Display] FPS changed to 30\n");
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_FPS_30], CMD_REQ_COMMIT);
#if defined(CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED)
			if (pinfo->mq_mode)
				oem_mdss_mq_cmd_set(ctrl);
#endif
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U3_TO_U2], CMD_REQ_COMMIT);
			goto notify;
		case AOD_CMD_DISABLE:
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U2_TO_U3], CMD_REQ_COMMIT);
			goto notify;
		case ON_AND_AOD:
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U2_TO_U0], CMD_REQ_COMMIT);

			if (ctrl->ds_registered && pinfo->is_pluggable) {
				mdss_dba_utils_video_off(pinfo->dba_data);
				mdss_dba_utils_hdcp_enable(pinfo->dba_data, false);
			}
			goto notify;
		case OFF_CMD:
			break;
		case CMD_SKIP:
			/* fps to 30 */
			pr_debug("[Display] FPS changed to 30\n");
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_FPS_30], CMD_REQ_COMMIT);
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
