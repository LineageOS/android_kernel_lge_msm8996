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

#include "lge_mdss_aod.h"
#include "lge_mdss_fb.h"
#include <linux/input/lge_touch_notify.h>
#include <linux/delay.h>

extern int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds, u32 flags);

#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
extern int lcd_watch_deside_status(struct  msm_fb_data_type *mfd, unsigned int cur_mode, unsigned int next_mode);
#endif

#if defined(CONFIG_LGE_DISPLAY_SRE_MODE)
void lge_set_sre_cmds(struct mdss_dsi_ctrl_pdata *ctrl);
#endif

static char *aod_mode_cmd_dt[] = {
	"lge,mode-change-cmds-u3-to-u2",
	"lge,mode-change-cmds-u2-to-u3",
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
	"lge,mode-change-cmds-u0-to-u2",
	"lge,mode-change-cmds-u2-to-u0",
	"lge,fps-change-to-30",
	"lge,fps-change-to-60",
	"lge,mode-change-cmds-u3-ready",
	"lge,mode-change-cmds-u2-ready",
#endif
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	"lge,mdss-dsi-display-on-command",
	"lge,mdss-dsi-display-off-command",
#endif
};

int oem_mdss_aod_init(struct device_node *node,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int i;
	struct mdss_panel_info *panel_info;

	/* Only parse aod command in DSI0 ctrl */
	if (ctrl_pdata->panel_data.panel_info.pdest != DISPLAY_1)
		return AOD_RETURN_SUCCESS;

	pr_info("[AOD] init start\n");
	panel_info = &(ctrl_pdata->panel_data.panel_info);
	panel_info->aod_init_done = false;
	ctrl_pdata->aod_cmds = kzalloc(sizeof(struct dsi_panel_cmds) *
					AOD_PANEL_CMD_NUM, GFP_KERNEL);
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

#if defined(CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED)
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->mq_column_row_cmds,
		"lge,mq-cmd-hs-column-row", "lge,mq-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->mq_control_cmds,
		"lge,mq-cmd-hs-control", "lge,mq-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->mq_access_cmds,
		"lge,mq-register-access-cmd", "lge,mq-register-access-control-dsi-state");
#endif

#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_rtc_set_cmd,
		"lge,watch-rtc-set", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_rtc_info_cmd,
		"lge,watch-rtc-info", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_ctl_cmd,
		"lge,watch-ctl", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_set_cmd,
		"lge,watch-set", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_fd_ctl_cmd,
		"lge,watch-fd-ctl", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_font_set_cmd,
		"lge,watch-font-set", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_u2_scr_fad_cmd,
		"lge,watch-u2-scr-fad", "lge,watch-cmd-control-dsi-state");
	mdss_dsi_parse_dcs_cmds(node, &ctrl_pdata->watch_font_crc_cmd,
		"lge,watch-font-crc", "lge,watch-cmd-control-dsi-state");
#endif

	return AOD_RETURN_SUCCESS;
}

int oem_mdss_aod_decide_status(struct msm_fb_data_type *mfd, int blank_mode)

{
	enum aod_panel_mode cur_mode = AOD_PANEL_MODE_NONE, next_mode;
	enum aod_cmd_status cmd_status;
	int aod_node, aod_keep_u2;
	int rc = AOD_RETURN_SUCCESS;
	bool labibb_ctrl = true;

#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
	if(blank_mode == FB_BLANK_UNBLANK)
		mfd->panel_info->ext_off_temp = mfd->panel_info->ext_off;
#endif
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
		pr_err("[AOD] Received %d event when recovery mode\n",
				blank_mode);
		goto error;
	}
	cur_mode = mfd->panel_info->aod_cur_mode;
	aod_node = mfd->panel_info->aod_node_from_user;
	aod_keep_u2 = mfd->panel_info->aod_keep_u2;

	pr_info("[AOD][START]cur_mode : %d, blank mode : %d, aod_node : %d, "
		"keep_aod : %d\n", cur_mode, blank_mode, aod_node, aod_keep_u2);
	/* FB_BLANK_UNBLANK 0 */
	/* FB_BLANK_POWERDOWN 4 */
	switch (cur_mode) {
	case AOD_PANEL_MODE_U0_BLANK:
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
		/* U0_BLANK -> U3_UNBLANK */
		if((blank_mode == FB_BLANK_UNBLANK && aod_node == 0) ||
			(blank_mode == FB_BLANK_UNBLANK && aod_node == 1 &&
			 (aod_keep_u2 == AOD_MOVE_TO_U3 || aod_keep_u2 == AOD_KEEP_U2))) {
			cmd_status = ON_CMD;
			next_mode = AOD_PANEL_MODE_U3_UNBLANK;
			labibb_ctrl = true;
		}
#else
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
			/* U0_BLANK -> U3_UNBLANK, When font download is Fail */
			if (blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && !mfd->watch.current_font_type) {
				cmd_status = ON_CMD;
				next_mode = AOD_PANEL_MODE_U3_UNBLANK;
				labibb_ctrl = true;
				break;
			}
#endif
			/* U0_BLANK -> U2_UNBLANK*/
			if (blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 == AOD_KEEP_U2) {
				cmd_status = ON_AND_AOD;
				next_mode = AOD_PANEL_MODE_U2_UNBLANK;
				labibb_ctrl = true;
			}
			/* U0_BLANK -> U3_UNBLANK */
			else if ((blank_mode == FB_BLANK_UNBLANK && aod_node == 0) ||
				 (blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 == AOD_MOVE_TO_U3)) {
				cmd_status = ON_CMD;
				next_mode = AOD_PANEL_MODE_U3_UNBLANK;
				labibb_ctrl = true;
			}
#endif
			else {
				rc = AOD_RETURN_ERROR_NO_SCENARIO;
				pr_err("[AOD]  NOT Checked Scenario\n");
				goto error;
			}

		break;
	case AOD_PANEL_MODE_U2_UNBLANK:
		/* U2_UNBLANK -> U0_BLANK */
		if (blank_mode == FB_BLANK_POWERDOWN && (aod_node == 0 || mfd->panel_info->dynamic_switch_pending == true)) {
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
			cmd_status = ON_AND_AOD;
			labibb_ctrl = false;
#else
			cmd_status = OFF_CMD;
#endif
			next_mode = AOD_PANEL_MODE_U0_BLANK;
		}
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
		/* U2_UNBLANK -> U0_BLANK, When font download is Fail */
		else if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 1 && !mfd->watch.current_font_type) {
			cmd_status = OFF_CMD;
			next_mode = AOD_PANEL_MODE_U0_BLANK;
			labibb_ctrl = true;
		}
#endif
		/* U2_UNBLANK -> U2_BLANK */
		else if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 1) {
			cmd_status = CMD_SKIP;
			next_mode = AOD_PANEL_MODE_U2_BLANK;
			labibb_ctrl = false;
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
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
			labibb_ctrl = true;
#else
			labibb_ctrl = false;
#endif
		}
		/* U2_BLANK -> U2_UNBLANK */
		else if ((blank_mode == FB_BLANK_UNBLANK && aod_node == 0) ||
				(blank_mode == FB_BLANK_UNBLANK && aod_node == 1 && aod_keep_u2 == AOD_KEEP_U2)) {
			cmd_status = CMD_SKIP;
			next_mode = AOD_PANEL_MODE_U2_UNBLANK;
			labibb_ctrl = false;
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
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
		/* AOD App didn't alive. U3_UNBLANK -> U0 */
		else if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 1 && !mfd->watch.current_font_type && !mfd->watch.set_roi) {
			cmd_status = OFF_CMD;
			next_mode = AOD_PANEL_MODE_U0_BLANK;
			labibb_ctrl = true;
		}
#endif
		/* U3_UNBLANK -> U2_BLANK */
		else if (blank_mode == FB_BLANK_POWERDOWN && aod_node == 1) {
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
			cmd_status = CMD_SKIP;
#else
			cmd_status = AOD_CMD_ENABLE;
#endif
			next_mode = AOD_PANEL_MODE_U2_BLANK;
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
			labibb_ctrl = true;
#else
			labibb_ctrl = false;
#endif
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
	pr_info("[AOD][END] cmd_status : %d, next_mode : %d labibb_ctrl : %s\n",
			cmd_status, next_mode, labibb_ctrl ? "ctrl" : "skip");
	mfd->panel_info->aod_cmd_mode = cmd_status;
	mfd->panel_info->aod_cur_mode = next_mode;
	mfd->panel_info->aod_labibb_ctrl = labibb_ctrl;

	/* set backlight mode as aod mode changes */
	oem_mdss_aod_set_backlight_mode(mfd);
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	if (mfd->panel_info->panel_type == LGD_SIC_LG49408_1440_2880_INCELL_CMD_PANEL) {
		mutex_lock(&mfd->watch_lock);
		lcd_watch_deside_status(mfd, cur_mode, next_mode);
		mutex_unlock(&mfd->watch_lock);
	}
#endif

	return AOD_RETURN_SUCCESS;
error:
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
	/* set backlight mode as aod mode changes */
	oem_mdss_aod_set_backlight_mode(mfd);
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	if (mfd->panel_info->panel_type == LGD_SIC_LG49408_1440_2880_INCELL_CMD_PANEL) {
		mutex_lock(&mfd->watch_lock);
		lcd_watch_deside_status(mfd, cur_mode, mfd->panel_info->aod_cur_mode);
		mutex_unlock(&mfd->watch_lock);
	}
#endif

	return rc;
}

int oem_mdss_aod_cmd_send(struct msm_fb_data_type *mfd, int cmd)
{
	int cmd_index, param, ret;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct mdss_panel_data *pdata;
	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (!ctrl) {
		pr_err("[AOD] ctrl is null\n");
		return AOD_RETURN_ERROR_SEND_CMD;
	}

	switch (cmd) {
	case AOD_CMD_ENABLE:
		cmd_index = AOD_PANEL_CMD_U3_TO_U2;
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
		if (ctrl->panel_data.panel_info.aod_cur_mode == AOD_PANEL_MODE_U3_UNBLANK) {
			param = AOD_PANEL_MODE_NONE;
		}
		else {
			ctrl->panel_data.panel_info.aod_cur_mode = AOD_PANEL_MODE_U2_UNBLANK;
			param = AOD_PANEL_MODE_U2_UNBLANK;
		}
#else
		ctrl->panel_data.panel_info.aod_cur_mode = AOD_PANEL_MODE_U2_UNBLANK;
		param = AOD_PANEL_MODE_U2_UNBLANK;
#endif
		break;
	case AOD_CMD_DISABLE:
		cmd_index = AOD_PANEL_CMD_U2_TO_U3;
		ctrl->panel_data.panel_info.aod_cur_mode = AOD_PANEL_MODE_U3_UNBLANK;
		param = AOD_PANEL_MODE_U3_UNBLANK;

#if defined(CONFIG_LGE_DISPLAY_SRE_MODE)
		lge_set_sre_cmds(ctrl);
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
#endif
		/* Need to enable 5V power when U2 unblank -> U3*/
		ret = msm_dss_enable_vreg(
				ctrl->panel_power_data.vreg_config,
				ctrl->panel_power_data.num_vreg, 1);
		if (ret) {
			pr_err("[AOD] failed to enable vregs for %s\n", __mdss_dsi_pm_name(DSI_PANEL_PM));
			return AOD_RETURN_ERROR_SEND_CMD;
		}
		break;
#if defined(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	case AOD_CMD_U3_READY:
	case AOD_CMD_U2_READY:
		mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
				MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
		if(cmd == AOD_CMD_U3_READY)
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U3_READY],
							CMD_REQ_COMMIT);
		if(cmd == AOD_CMD_U2_READY)
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[AOD_PANEL_CMD_U2_READY],
							CMD_REQ_COMMIT);
		mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
					MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
		return AOD_RETURN_SUCCESS;
#endif
#if defined(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	case AOD_CMD_DISPLAY_ON:
		cmd_index = AOD_PANEL_CMD_DISPALY_ON;
		param = AOD_PANEL_MODE_NONE;
		mfd->display_off = false;
		break;
	case AOD_CMD_DISPLAY_OFF:
		cmd_index = AOD_PANEL_CMD_DISPALY_OFF;
		param = AOD_PANEL_MODE_NONE;
		mfd->display_off = true;
		break;
#endif
	default:
		return AOD_RETURN_ERROR_SEND_CMD;
	}

	pr_info("[AOD] Send %d command to panel\n", cmd_index);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_cmds[cmd_index],
							CMD_REQ_COMMIT);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

	/* Don't send notify to touch when AOD_PANEL_MODE_NONE */
	if (param == AOD_PANEL_MODE_NONE)
		return AOD_RETURN_SUCCESS;
	if(touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void *)&param))
		pr_err("[AOD] Failt to send notify to touch\n");
	return AOD_RETURN_SUCCESS;
}

#if defined(CONFIG_LGE_DISPLAY_AOD_USE_QPNP_WLED)
extern int qpnp_wled_set_sink(int enable);
#endif
/* set backlight mode and level as proper aod mode
 * call it if backlight mode should be updated
 */
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
/* for extended backlight */
/* only change dimming mode */
static void aod_setblmode_ex(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_info *pinfo;
	struct mdss_panel_data *pdata;
	enum aod_panel_mode aod_mode;
	bool next_dimm_mode;
	int ext_off;

	if(mfd->index != 0) {
		pr_err("[AOD]fb[%d] is not for aod\n", mfd->index);
		return;
	}
	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[AOD]no panel connected!\n");
		return;
	}
	pinfo = &pdata->panel_info;

	/* ext_off_temp is the exact current extended aod mode */
	ext_off = pinfo->ext_off_temp;
	aod_mode = pinfo->aod_cur_mode;

	/* -------------------------------------------------------------------
	 * aod mode                | aod backlight mode
	 * -------------------------------------------------------------------
	 * u0, u2, u3              | normal mode
	 * u1(u3 with ext_off set) | dimming mode for main lcd bl compensation
	 * -------------------------------------------------------------------
	 */
	next_dimm_mode = (aod_mode == AOD_PANEL_MODE_U3_UNBLANK) && ext_off;

	if (pinfo->bl2_dimm != next_dimm_mode) {
		/* change bl with new ext aod mode */
		mutex_lock(&mfd->bl_lock);
		pinfo->bl2_dimm = next_dimm_mode;
		mutex_unlock(&mfd->bl_lock);
	}
}
#else /* for non extended backlight */
static void aod_setblmode(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_info *pinfo;
	struct mdss_panel_data *pdata;
	enum aod_panel_mode aod_mode;
#if defined(CONFIG_LGE_DISPLAY_AOD_USE_QPNP_WLED)
	bool next_dimm_mode;
#endif

	if(mfd->index != 0) {
		pr_err("[AOD]fb[%d] is not for aod\n", mfd->index);
		return;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[AOD]no panel connected!\n");
		return;
	}

	pinfo = &pdata->panel_info;
	aod_mode = pinfo->aod_cur_mode;

	/* --------------------------------------------
	 * aod mode          | aod backlight mode
	 * --------------------------------------------
	 * u0, u3            | normal mode
	 * u2                | dimming mode for aod
	 * --------------------------------------------
	 */
#if defined(CONFIG_LGE_DISPLAY_AOD_USE_QPNP_WLED)
	next_dimm_mode = (aod_mode == AOD_PANEL_MODE_U2_UNBLANK ||
					aod_mode == AOD_PANEL_MODE_U2_BLANK);
	if (pinfo->bl2_dimm != next_dimm_mode) {
		pinfo->bl2_dimm = next_dimm_mode;
		qpnp_wled_set_sink(next_dimm_mode);
	}
#else
	/* TODO: implement for others */
#endif
}
#endif /* CONFIG_LGE_DISPLAY_BL_EXTENDED */

void oem_mdss_aod_set_backlight_mode(struct msm_fb_data_type *mfd)
{
#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
	aod_setblmode_ex(mfd);
#else
	aod_setblmode(mfd);
#endif
}
#if defined(CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED)
void oem_mdss_mq_cmd_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 mq_reg[7];
	int i;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;

	pdata = &ctrl->panel_data;
	pinfo = &pdata->panel_info;
	pr_info("%s : marquee is set!!!\n",__func__);

	memset(mq_reg, 0, sizeof(u32)*7);
	mq_reg[0] = (pinfo->mq_pos.start_x) & 0xFF;
	mq_reg[1] = ((pinfo->mq_pos.start_x >> 8) & 0x01) | ((pinfo->mq_pos.end_x << 1) & 0xFE);
	mq_reg[2] = ((pinfo->mq_pos.end_x >> 7) & 0x03);
	//mq_reg[3] is not used
	mq_reg[4] = pinfo->mq_pos.start_y & 0xFF;
	mq_reg[5] = pinfo->mq_pos.end_y & 0xFF;
	mq_reg[6] = (pinfo->mq_mode & 0x01) |
		((pinfo->mq_direction << 1) & 0x02) | BIT(2)|((pinfo->mq_speed << 3) & 0xF8);//bit 2 is HSCR_GRANT

	for(i=3;i<(ctrl->mq_column_row_cmds.cmds->dchdr.dlen)-2;i++)//last 2 bytes are not used
	{
		ctrl->mq_column_row_cmds.cmds->payload[i]=mq_reg[i-3];
		pr_info("%s: mq_column_row_cmds.cmds->payload[%d]: %x\n",__func__,i,ctrl->mq_column_row_cmds.cmds->payload[i]);
	}
	ctrl->mq_control_cmds.cmds->payload[3] = mq_reg[6];
	pr_info("%s: mq_control_cmds.cmds->payload[3]: %x\n",__func__,ctrl->mq_control_cmds.cmds->payload[3]);

	oem_mdss_mq_access_set(ctrl, 1);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->mq_column_row_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->mq_control_cmds, CMD_REQ_COMMIT);
	oem_mdss_mq_access_set(ctrl, 0);
}
void oem_mdss_mq_cmd_unset(struct mdss_dsi_ctrl_pdata *ctrl)
{
	pr_info("%s : marquee is unset!!!\n",__func__);
	ctrl->mq_column_row_cmds.cmds->payload[7] = 0x00;
	ctrl->mq_column_row_cmds.cmds->payload[8] = 0x00;
	ctrl->mq_control_cmds.cmds->payload[3] = 0x00;//unset marquee

	oem_mdss_mq_access_set(ctrl,1);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->mq_column_row_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->mq_control_cmds, CMD_REQ_COMMIT);
	oem_mdss_mq_access_set(ctrl,0);
}
void oem_mdss_mq_access_set(struct mdss_dsi_ctrl_pdata *ctrl, bool enable)
{
	pr_info("%s : marquee is access set!!!\n",__func__);
	ctrl->mq_access_cmds.cmds->payload[1]= enable;
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->mq_access_cmds, CMD_REQ_COMMIT);
}
#endif

