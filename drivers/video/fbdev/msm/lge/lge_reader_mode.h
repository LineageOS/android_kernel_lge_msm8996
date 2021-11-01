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

#ifndef READER_MODE_H
#define READER_MODE_H

#define READER_MODE_OFF		0
#define READER_MODE_STEP_1	1
#define READER_MODE_STEP_2	2
#define READER_MODE_STEP_3	3
#define READER_MODE_STEP_4	4
#define READER_MODE_STEP_5	5
#define READER_MODE_STEP_6	6
#define READER_MODE_STEP_7	7
#define READER_MODE_STEP_8	8
#define READER_MODE_STEP_9	9
#define READER_MODE_STEP_10	10


/* Implementions of below functions depend on DDIC */
int lge_mdss_dsi_parse_reader_mode_cmds(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata);
bool lge_change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int new_mode);
int lge_mdss_dsi_panel_send_on_cmds(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *default_on_cmds, int cur_mode);
/* END */

int lge_get_reader_mode(void);

ssize_t set_reader_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t get_reader_mode(struct device *dev, struct device_attribute *attr, char *buf);
#endif
