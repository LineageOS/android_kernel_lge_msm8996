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

#ifndef LGE_MDSS_DSI_H
#define LGE_MDSS_DSI_H

struct lge_supply_entry {
        char name[32];
};

struct lge_gpio_entry {
	char name[32];
	int gpio;
};

struct lge_cmds_entry {
	char name[32];
	struct dsi_panel_cmds cmds;
};

struct lge_mdss_dsi_ctrl_pdata {
	/* gpio */
	int num_gpios;
	struct lge_gpio_entry *gpio_array;

	/* delay */
	int pre_on_cmds_delay;
	int pre_off_cmds_extra_delay;
	int post_off_cmds_delay;

	/* cmds */
	int num_extra_cmds;
	struct lge_cmds_entry *extra_cmds_array;

	/* extra power */
	struct dss_module_power extra_power_data;
	int extra_power_state;
};

#define LGE_MDELAY(m) do { if ( m > 0) usleep_range((m)*1000,(m)*1000); } while(0)
#define LGE_OVERRIDE_VALUE(x, v) do { if ((v)) (x) = (v); } while(0)

#include "lge_mdss_dsi_panel.h"

int lge_mdss_dsi_parse_extra_params(
	struct platform_device *ctrl_pdev,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int lge_mdss_dsi_init_extra_pm(struct platform_device *ctrl_pdev,
        struct device_node *pan_node, struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void lge_mdss_dsi_deinit_extra_pm(struct platform_device *pdev,
        struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void lge_extra_gpio_set_value(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		const char *name, int value);

int lge_panel_power_on(struct mdss_panel_data *pdata);
int lge_panel_power_off(struct mdss_panel_data *pdata);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
void mdss_dsi_ctrl_shutdown(struct platform_device *pdev);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_ON)
int mdss_dsi_panel_on(struct mdss_panel_data *pdata);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_OFF)
int mdss_dsi_panel_off(struct mdss_panel_data *pdata);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata);
#endif

#endif /* LGE_MDSS_DSI_H */
