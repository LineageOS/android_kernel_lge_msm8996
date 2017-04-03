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
#include <soc/qcom/lge/board_lge.h>
#include <linux/input/lge_touch_notify.h>

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	int ret = 0;

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return;
	}

	if ((pinfo->panel_type == LGD_SIC_LG4945_INCELL_CMD_PANEL) ||
	    (pinfo->panel_type == LGD_SIC_LG49407_INCELL_CMD_PANEL) ||
	    (pinfo->panel_type == LGD_SIC_LG49407_INCELL_VIDEO_PANEL)) {
		/* TODO: check power sequence for hplus */
		/* Shutdown sequence for LG4946
		 * LABIBB HiZ - 5ms - Reset L - VPNL off - VDDIO off
		 * - LABIBB Pulldown.
		 */
		if (mdss_dsi_is_right_ctrl(ctrl_pdata) &&
			mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
			pr_err("[Display] %s:right. skip here.\n", __func__);
			return;
		} else {
			usleep_range(5000, 5000);
			pr_err("[Display] %s. reset L\n", __func__);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			usleep_range(5000, 5000);

			pr_err("[Display] %s. vpnl off\n", __func__);

			lge_extra_gpio_set_value(ctrl_pdata, "vpnl", 0);
			usleep_range(1000, 1000);

			pr_err("[Display] %s. vddio off\n", __func__);
			lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
			usleep_range(1000, 1000);

			ret = msm_dss_set_vreg(
				ctrl_pdata->panel_power_data.vreg_config,
				ctrl_pdata->panel_power_data.num_vreg,
				REGULATOR_MODE_SHUTDOWN);
			if (ret)
				pr_err("[Display]%s failed : %d\n", __func__,
						ret);
		}
	}
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
extern int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		bool active);
#endif

#ifdef CONFIG_LGE_LCD_POWER_CTRL
extern int panel_not_connected;
extern int detect_factory_cable(void);

int lge_panel_power_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_err("[Display] %s+: ndx=%d\n", __func__, ctrl_pdata->ndx);

	if (!(panel_not_connected && detect_factory_cable() &&
						!lge_get_mfts_mode())) {
		if (mdss_dsi_is_right_ctrl(ctrl_pdata)) {
			pr_err("%s:%d, right ctrl configuration not needed\n",
				__func__, __LINE__);
			return ret;
		}
	}

	ret = msm_dss_set_vreg(ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg,
			REGULATOR_MODE_TTW_OFF);
	if(ret)
		pr_err("%s: fail to disable ttw modei : %d\n", __func__, ret);
	else
		pr_info("%s: diable ttw mode\n", __func__);

	if (touch_notifier_call_chain(NOTIFY_TOUCH_RESET, NULL))
		pr_err("Failt to send notify to touch\n");

	ret = msm_dss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
	else
		pr_info("%s: disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));


	usleep_range(5000,5000);

	ret = msm_dss_set_vreg(ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg,
			REGULATOR_MODE_SPARE_ON);
	if (ret)
		pr_err("%s fail to set mfts mode : %d\n", __func__, ret);
	else
		pr_info("%s: set spare on  mode\n", __func__);

	ret = mdss_dsi_panel_reset(pdata, 0);
	if (ret)
		pr_err("%s: Panel reset failed. rc=%d\n", __func__, ret);
	else
		pr_info("%s Panel reset off\n", __func__);

	usleep_range(5000,5000);

	lge_extra_gpio_set_value(ctrl_pdata, "vpnl", 0);
	pr_info("%s: disable LCD vpnl \n",__func__);

	usleep_range(3000,3000);

	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	pr_info("%s: disable LCD vddio \n",__func__);

	usleep_range(3000,3000);

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	ret = msm_dss_set_vreg(ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg,
			REGULATOR_MODE_TTW_ON);
	if(ret)
		pr_err("%s: fail to disable ttw modei : %d\n", __func__, ret);
	else
		pr_info("%s: enable ttw mode\n", __func__);

	pr_err("[Display] %s-: ndx=%d\n", __func__, ctrl_pdata->ndx);

	return ret;
}

int lge_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	pr_err("[Display] %s+: ndx=%d\n", __func__, ctrl_pdata->ndx);

	if (!(panel_not_connected && detect_factory_cable() &&
						!lge_get_mfts_mode())) {
		if (mdss_dsi_is_right_ctrl(ctrl_pdata)) {
			pr_err("%s:%d, right ctrl configuration not needed\n",
				__func__, __LINE__);
			return ret;
		}
	}

	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
	pr_info("%s: enable LCD vddio \n", __func__);

	usleep_range(3000,3000);

	lge_extra_gpio_set_value(ctrl_pdata, "vpnl", 1);
	pr_info("%s: enable LCD vpnl \n", __func__);

	usleep_range(3000,3000);

	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {

		ret = msm_dss_enable_vreg(
				ctrl_pdata->panel_power_data.vreg_config,
				ctrl_pdata->panel_power_data.num_vreg, 1);
		if (ret) {
			pr_err("%s: failed to enable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		} else
			pr_info("%s: enable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

		usleep_range(3000,3000);

		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
		else
			pr_info("%s: panel reset on\n",__func__);
	}

	pr_err("[Display] %s-: ndx=%d\n", __func__, ctrl_pdata->ndx);

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		pr_err("[Display] %s right ctrl gpio configuration not needed\n"
				, __func__);
		return ret;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
		pr_err("[Display] pinctrl not enabled\n");

#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	if (ctrl_pdata->panel_data.panel_info.aod_labibb_ctrl == true) {
		ret = msm_dss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 1);
		if (ret) {
			pr_err("%s: failed to enable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
			return ret;
		}
	}
	else
		pr_info("[AOD] Skip labibb enable\n");
#else
	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		return ret;
	}
#endif
	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		pr_debug("%s:%d, right ctrl gpio configuration not needed\n",
			__func__, __LINE__);
		return ret;
	}
#if defined(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	if (ctrl_pdata->panel_data.panel_info.aod_labibb_ctrl != true) {
		pr_info("[AOD] Skip labibb disable\n");
		return ret;
	}
#endif
	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
end:
	return ret;
}
#endif

