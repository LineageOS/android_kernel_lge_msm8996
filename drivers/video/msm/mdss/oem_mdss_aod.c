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

#include "oem_mdss_aod.h"
#include <linux/input/lge_touch_notify.h>
#include <soc/qcom/lge/board_lge.h>

extern int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds, u32 flags);

#if defined(CONFIG_LGE_DISPLAY_AOD_USE_QPNP_WLED)
extern int qpnp_wled_set_sink(int enable);
#endif

static char *aod_mode_cmd_dt[] = {
	"lge,mode-change-cmds-u3-to-u2",
	"lge,mode-change-cmds-u2-to-u3",
};

int oem_mdss_aod_init(struct device_node *node, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int i;
	struct mdss_panel_info *panel_info;
	pr_info("[AOD] init start\n");

	panel_info = &(ctrl_pdata->panel_data.panel_info);
	panel_info->aod_init_done = false;
	ctrl_pdata->aod_cmds = kzalloc(sizeof(struct dsi_panel_cmds)*AOD_PANEL_CMD_NUM, GFP_KERNEL);
	if (!ctrl_pdata->aod_cmds) {
		pr_err("[AOD] failed to get memory\n");
		return -AOD_RETURN_ERROR_MEMORY;
	}

	for (i = 0; i < AOD_PANEL_CMD_NUM ; i++ ) {
		mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->aod_cmds[i],
			aod_mode_cmd_dt[i], "qcom,mode-control-dsi-state");
	}

	panel_info->aod_cur_mode = AOD_PANEL_MODE_U3_UNBLANK;
	panel_info->aod_keep_u2 = AOD_NO_DECISION;
	panel_info->aod_node_from_user = 0;
	panel_info->aod_init_done = true;
	panel_info->aod_labibb_ctrl = true;

	return AOD_RETURN_SUCCESS;
}

int oem_mdss_aod_decide_status(struct msm_fb_data_type *mfd, int blank_mode)

{
	enum aod_panel_mode cur_mode, next_mode;
	enum aod_cmd_status cmd_status;
	int aod_node, aod_keep_u2, rc;
	bool labibb_ctrl = false;

	if (!mfd->panel_info->aod_init_done) {
		pr_err("[AOD] Not initialized!!!\n");
		rc = AOD_RETURN_ERROR_NOT_INIT;
		goto error;
	}

	if (lge_get_boot_mode() != LGE_BOOT_MODE_NORMAL) {
		pr_err("[AOD] AOD status decide only normal mode!!\n");
		rc = AOD_RETURN_ERROR_NOT_NORMAL_BOOT;
		goto error;
	}

	/* Mode set when recovery mode */
	if (mfd->recovery) {
		pr_err("[AOD] Received %d event when recovery mode\n", blank_mode);
		goto error;
	}
	cur_mode = mfd->panel_info->aod_cur_mode;
	aod_node = mfd->panel_info->aod_node_from_user;
	aod_keep_u2 = mfd->panel_info->aod_keep_u2;

	pr_info("[AOD][START]cur_mode : %d, blank mode : %d, aod_node : %d, keep_aod : %d\n", cur_mode, blank_mode, aod_node, aod_keep_u2);
	/* FB_BLANK_UNBLANK 0 */
	/* FB_BLANK_POWERDOWN 4 */
	switch (cur_mode) {
	case AOD_PANEL_MODE_U0_BLANK:
		/* U0_BLANK -> U2_UNBLANK*/
		if (blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 == AOD_NO_DECISION) {
			cmd_status = ON_AND_AOD;
			next_mode = AOD_PANEL_MODE_U2_UNBLANK;
			oem_mdss_aod_set_backlight_mode(AOD_BACKLIGHT_SECONDARY);
		}
		/* U0_BLANK -> U3_UNBLANK */
		else if ((blank_mode == FB_BLANK_UNBLANK && aod_node == 0) ||
				 (blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 == AOD_MOVE_TO_U3)) {
			cmd_status = ON_CMD;
			next_mode = AOD_PANEL_MODE_U3_UNBLANK;
			labibb_ctrl = true;
		}
		else {
			rc = AOD_RETURN_ERROR_NO_SCENARIO;
			pr_err("[AOD]  NOT Checked Scenario\n");
			goto error;
		}
		break;
	case AOD_PANEL_MODE_U2_UNBLANK:
		/* U2_UNBLANK -> U0_BLANK */
		if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 0) {
			cmd_status = OFF_CMD;
			next_mode = AOD_PANEL_MODE_U0_BLANK;
			oem_mdss_aod_set_backlight_mode(AOD_BACKLIGHT_PRIMARY);
		}
		/* U2_UNBLANK -> U2_BLANK */
		else if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 1) {
			cmd_status = CMD_SKIP;
			next_mode = AOD_PANEL_MODE_U2_BLANK;
		}
		else {
			rc = AOD_RETURN_ERROR_NO_SCENARIO;
			pr_err("[AOD]  NOT Checked Scenario\n");
			goto error;
		}
		break;
	case AOD_PANEL_MODE_U2_BLANK:
		/* U2_BLANK -> U3_UNBLANK */
		if ((blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 != AOD_KEEP_U2) ||
			 (blank_mode == FB_BLANK_UNBLANK && aod_node == 0 && aod_keep_u2 == AOD_MOVE_TO_U3)) {
			cmd_status = AOD_CMD_DISABLE;
			next_mode = AOD_PANEL_MODE_U3_UNBLANK;
			labibb_ctrl = true;
			oem_mdss_aod_set_backlight_mode(AOD_BACKLIGHT_PRIMARY);
		}
		/* U2_BLANK -> U2_UNBLANK */
		else if ((blank_mode == FB_BLANK_UNBLANK && aod_node == 0) ||
				(blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 == AOD_KEEP_U2)) {
			cmd_status = CMD_SKIP;
			next_mode = AOD_PANEL_MODE_U2_UNBLANK;
		}
		else {
			rc = AOD_RETURN_ERROR_NO_SCENARIO;
			pr_err("[AOD]  NOT Checked Scenario\n");
			goto error;
		}
		break;
	case AOD_PANEL_MODE_U3_UNBLANK:
		/* U3_UNBLANK -> U0_BLANK */
		if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 0) {
			cmd_status = OFF_CMD;
			next_mode = AOD_PANEL_MODE_U0_BLANK;
			labibb_ctrl = true;
		}
		/* U3_UNBLANK -> U2_BLANK */
		else if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 1) {
			cmd_status = AOD_CMD_ENABLE;
			next_mode = AOD_PANEL_MODE_U2_BLANK;
			labibb_ctrl = true;
			oem_mdss_aod_set_backlight_mode(AOD_BACKLIGHT_SECONDARY);
		}
		else {
			rc = AOD_RETURN_ERROR_NO_SCENARIO;
			pr_err("[AOD]  NOT Checked Scenario\n");
			goto error;
		}
		break;
	default:
		pr_err("[AOD] Unknown Mode : %d\n", blank_mode);
		rc = AOD_RETURN_ERROR_UNKNOWN;
		goto error;
	}
	pr_info("[AOD][END] cmd_status : %d, next_mode : %d labibb_ctrl : %s\n", cmd_status, next_mode, labibb_ctrl ? "ctrl" : "skip");
	mfd->panel_info->aod_cmd_mode = cmd_status;
	mfd->panel_info->aod_cur_mode = next_mode;
	mfd->panel_info->aod_keep_u2 = AOD_NO_DECISION;
	mfd->panel_info->aod_labibb_ctrl = labibb_ctrl;
	return AOD_RETURN_SUCCESS;
error:
	oem_mdss_aod_set_backlight_mode(AOD_BACKLIGHT_PRIMARY);
	mfd->panel_info->aod_labibb_ctrl = true;
	if (blank_mode == FB_BLANK_POWERDOWN) {
		mfd->panel_info->aod_cmd_mode = OFF_CMD;
		mfd->panel_info->aod_cur_mode = AOD_PANEL_MODE_U0_BLANK;
	}
	else if (blank_mode == FB_BLANK_UNBLANK) {
		mfd->panel_info->aod_cmd_mode = ON_CMD;
		mfd->panel_info->aod_cur_mode = AOD_PANEL_MODE_U3_UNBLANK;
	}
	else {
		mfd->panel_info->aod_cmd_mode = OFF_CMD;
		mfd->panel_info->aod_cur_mode = AOD_PANEL_MODE_U0_BLANK;
	}
	return rc;
}

int oem_mdss_aod_cmd_send(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int cmd)
{
	int cmd_index, param, ret;
	struct mdss_dsi_ctrl_pdata *ctrl;
	if (!ctrl_pdata) {
		pr_err("[AOD] ctrl_pdata is null\n");
		return AOD_RETURN_ERROR_SEND_CMD;
	}
	if (ctrl_pdata->ndx != DSI_CTRL_LEFT) {
		pr_err("[AOD] Change ctrl to left\n");
		ctrl = mdss_dsi_get_other_ctrl(ctrl_pdata);
	}
	else
		ctrl = ctrl_pdata;
	switch (cmd) {
	case AOD_CMD_ENABLE:
		cmd_index = AOD_PANEL_CMD_U3_TO_U2;
		ctrl_pdata->panel_data.panel_info.aod_cur_mode = AOD_PANEL_MODE_U2_UNBLANK;
		param = AOD_PANEL_MODE_U2_UNBLANK;
		break;
	case AOD_CMD_DISABLE:
		cmd_index = AOD_PANEL_CMD_U2_TO_U3;
		ctrl_pdata->panel_data.panel_info.aod_cur_mode = AOD_PANEL_MODE_U3_UNBLANK;
		param = AOD_PANEL_MODE_U3_UNBLANK;

		/* Need to enable 5V power when U2 unblank -> U3*/
		ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
		if (ret) {
			pr_err("[AOD] failed to enable vregs for %s\n", __mdss_dsi_pm_name(DSI_PANEL_PM));
			return AOD_RETURN_ERROR_SEND_CMD;
		}
		break;
	default:
		return AOD_RETURN_ERROR_SEND_CMD;
	}
	pr_info("[AOD] Send %d command to panel\n", cmd_index);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[cmd_index], CMD_REQ_COMMIT);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

	if(touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void *)&param))
		pr_err("[AOD] Failt to send notify to touch\n");
	return AOD_RETURN_SUCCESS;
}

void oem_mdss_aod_set_backlight_mode(int mode)
{
#if defined(CONFIG_LGE_DISPLAY_AOD_USE_QPNP_WLED)
	qpnp_wled_set_sink(mode);
#else
	/* Select Backlight IC in here */
#endif
}
