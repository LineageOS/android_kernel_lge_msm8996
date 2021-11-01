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
#include "../lge_mdss_aod.h"
#include "../../mdss_dba_utils.h"
#include <linux/input/lge_touch_notify.h>
#include <soc/qcom/lge/board_lge.h>

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

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if ((mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) ||
			pinfo->is_dba_panel) {
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

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
		if (!pinfo->cont_splash_enabled) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio)){
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
			if (pinfo->aod_cmd_mode != ON_CMD && pinfo->aod_cmd_mode != ON_AND_AOD && !pinfo->panel_dead)
				return rc;
#endif
			/* Notify to Touch when panel reset */
			if (touch_notifier_call_chain(NOTIFY_TOUCH_RESET, NULL))
				pr_err("Failt to send notify to touch\n");

			if (pdata->panel_info.rst_seq_len) {
				rc = gpio_direction_output(ctrl_pdata->rst_gpio,
					pdata->panel_info.rst_seq[0]);
				if (rc) {
					pr_err("%s: unable to set dir for rst gpio\n",
						__func__);
					goto exit;
				}
			}

			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
						pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
			}

			if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)){
				rc = gpio_direction_output(
					ctrl_pdata->bklt_en_gpio, 1);
				if (rc) {
					pr_err("%s: unable to set dir for bklt gpio\n",
						__func__);
					goto exit;
				}
			}
		}

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

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds, u32 flags);
extern void lge_force_mdss_dsi_panel_cmd_read(char cmd0, int cnt, char* ret_buf);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_ON)
int change_vcom_cmds_for_VNL(struct mdss_dsi_ctrl_pdata *ctrl, int restore)
{
	int i = 0;
	int ret = 0;
	int cnt = 7;
	char ret_buf[7] = {0x0};
	char cmd_addr[1] = {0xC5};

	lge_force_mdss_dsi_panel_cmd_read(cmd_addr[0], cnt, ret_buf);

	memcpy(&(ctrl->vcom_cmds.cmds[0].payload[1]), ret_buf, cnt);

	pr_info("[%s] vcom reg before writing: ", restore?"restore":"change");
	for ( i = 0; i < cnt + 1; i++) {
		pr_info("0x%x ", ctrl->vcom_cmds.cmds[0].payload[i]);
	}
	pr_info("\n");

	if (restore)
		ctrl->vcom_cmds.cmds[0].payload[6] = 0x34;
	else
		ctrl->vcom_cmds.cmds[0].payload[6] = 0x30;

	mdss_dsi_panel_cmds_send(ctrl, &ctrl->vcom_cmds, CMD_REQ_COMMIT);

	//memset(ret_buf, 0, cnt);
	lge_force_mdss_dsi_panel_cmd_read(cmd_addr[0], cnt, ret_buf);
	memcpy(&(ctrl->vcom_cmds.cmds[0].payload[1]), ret_buf, cnt);

	pr_info("[%s] vcom reg after writing: ", restore?"restore":"change");
	for ( i = 0; i < cnt + 1; i++) {
		pr_info("0x%x ", ctrl->vcom_cmds.cmds[0].payload[i]);
	}
	pr_info("\n");

	return ret;
}

int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	struct dsi_panel_cmds *on_cmds;
	struct dsi_panel_cmds *vcom_cmds;
	int ret = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	pr_debug("%s: ndx=%d\n", __func__, ctrl->ndx);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

	on_cmds = &ctrl->on_cmds;
	vcom_cmds = &ctrl->vcom_cmds;
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
			if (lge_get_panel_maker_id() == LGD_LG4946 &&
					lge_get_panel_revision_id() == LGD_LG4946_REV0 &&
					vcom_cmds->cmd_cnt != 0) {
				pr_err("[Display] send LG4946 REV0 Vcom Setting \n");
				mdss_dsi_panel_cmds_send(ctrl, vcom_cmds, CMD_REQ_COMMIT);
			}
			if (on_cmds->cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl, on_cmds, CMD_REQ_COMMIT);

			change_vcom_cmds_for_VNL(ctrl, 0);

#if defined(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
			if (ctrl->ie_on == 0)
				mdss_dsi_panel_cmds_send(ctrl, &ctrl->ie_off_cmds, CMD_REQ_COMMIT);
#endif
#if defined(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
			if (ctrl->sharpness_on_cmds.cmds[2].payload[3] == 0x23)
				mdss_dsi_panel_cmds_send(ctrl, &ctrl->sharpness_on_cmds, CMD_REQ_COMMIT);
#endif
			goto end;
		case ON_CMD:
			break;
		case CMD_SKIP:
			goto notify;
		default:
			pr_err("[AOD] Unknown Mode : %d\n", pinfo->aod_cmd_mode);
			break;
	}
#endif
	if (lge_get_panel_maker_id() == LGD_LG4946 &&
			lge_get_panel_revision_id() == LGD_LG4946_REV0 &&
			vcom_cmds->cmd_cnt != 0) {
		pr_err("[Display] send LG4946 REV0 Vcom Setting \n");
		mdss_dsi_panel_cmds_send(ctrl, vcom_cmds, CMD_REQ_COMMIT);
	}
	if (on_cmds->cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, on_cmds, CMD_REQ_COMMIT);

	change_vcom_cmds_for_VNL(ctrl, 0);

	if (pinfo->compression_mode == COMPRESSION_DSC)
		mdss_dsi_panel_dsc_pps_send(ctrl, pinfo);

	if (ctrl->ds_registered)

		mdss_dba_utils_video_on(pinfo->dba_data, pinfo);

#if defined(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
	if (ctrl->ie_on == 0)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->ie_off_cmds, CMD_REQ_COMMIT);
#endif
#if defined(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
	if (ctrl->sharpness_on_cmds.cmds[2].payload[3] == 0x23)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->sharpness_on_cmds, CMD_REQ_COMMIT);
#endif

#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
notify:
	{
		int param;
		param = pinfo->aod_cur_mode;
		if(touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void *)&param))
			pr_err("[AOD] Failt to send notify to touch\n");
	}
#endif

end:
	pr_debug("%s:-\n", __func__);
	return ret;
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

	pr_debug("%s: ctrl=%pK ndx=%d\n", __func__, ctrl, ctrl->ndx);

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
		if(touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void *)&param))
			pr_err("[AOD] Failt to send notify to touch\n");
	}
#endif

end:
	pr_debug("%s:-\n", __func__);
	return 0;
}
#endif

extern int lge_is_valid_U2_FTRIM_reg(void);
ssize_t mdss_fb_is_valid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int is_valid = lge_is_valid_U2_FTRIM_reg();
	if (is_valid < 0)
		ret = is_valid; //read error
	else
		ret = scnprintf(buf, PAGE_SIZE, "DDIC validation is %d\n",
				is_valid);

	return ret;
}
