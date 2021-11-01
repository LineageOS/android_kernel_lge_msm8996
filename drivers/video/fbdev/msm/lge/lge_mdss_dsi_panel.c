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

#include <linux/of_platform.h>
#include "../mdss_dsi.h"
#include "lge_mdss_dsi_panel.h"
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
#include "lge_reader_mode.h"
#include "../mdss_mdp.h"
struct dsi_panel_cmds reader_mode_step0_cmds;
struct dsi_panel_cmds reader_mode_step1_cmds;
struct dsi_panel_cmds reader_mode_step2_cmds;
struct dsi_panel_cmds reader_mode_step3_cmds;
struct dsi_panel_cmds reader_mode_step4_cmds;
struct dsi_panel_cmds reader_mode_step5_cmds;
struct dsi_panel_cmds reader_mode_step6_cmds;
struct dsi_panel_cmds reader_mode_step7_cmds;
struct dsi_panel_cmds reader_mode_step8_cmds;
struct dsi_panel_cmds reader_mode_step9_cmds;
struct dsi_panel_cmds reader_mode_step10_cmds;
#endif

char *lge_blmap_name[] = {
	"lge,blmap",
	"lge,blmap-hl",
	"lge,blmap-ex",
	"lge,blmap-ex-dim",
	"lge,blmap-ex-hl",
	"lge,blmap-ex-dim-hl",
};

extern int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);
extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds, u32 flags);

static int parse_dt_extra_dcs_cmds(struct device_node *np,
                        struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc;
	int i;
	const char *name;
	char buf1[256];
	char buf2[256];

	rc = of_property_count_strings(np, "lge,mdss-dsi-extra-command-names");
	if (rc > 0) {
		ctrl_pdata->lge_extra.num_extra_cmds = rc;
		pr_info("%s: num_extra_cmds=%d\n", __func__,
					ctrl_pdata->lge_extra.num_extra_cmds);
		ctrl_pdata->lge_extra.extra_cmds_array =
			kmalloc(sizeof(struct lge_cmds_entry) *
			ctrl_pdata->lge_extra.num_extra_cmds, GFP_KERNEL);
		if (NULL == ctrl_pdata->lge_extra.extra_cmds_array) {
			pr_err("%s: no memory\n", __func__);
			ctrl_pdata->lge_extra.num_extra_cmds = 0;
			return -ENOMEM;
		}
		for (i = 0; i < ctrl_pdata->lge_extra.num_extra_cmds; ++i) {
			of_property_read_string_index(np,
				"lge,mdss-dsi-extra-command-names", i, &name);
			strlcpy(ctrl_pdata->lge_extra.extra_cmds_array[i].name,
					name,
			sizeof(ctrl_pdata->lge_extra.extra_cmds_array[i].name));
			snprintf(buf1, sizeof(buf1),
					"lge,mdss-dsi-extra-command-%s", name);
			snprintf(buf2, sizeof(buf2),
				"lge,mdss-dsi-extra-command-state-%s", name);
			mdss_dsi_parse_dcs_cmds(np,
				&ctrl_pdata->lge_extra.extra_cmds_array[i].cmds,
					buf1, buf2);
		}
	} else {
		ctrl_pdata->lge_extra.num_extra_cmds = 0;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
int lge_mdss_dsi_parse_reader_mode_cmds(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step0_cmds,
			"qcom,panel-reader-mode-step0-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step1_cmds,
			"qcom,panel-reader-mode-step1-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step2_cmds,
			"qcom,panel-reader-mode-step2-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step3_cmds,
			"qcom,panel-reader-mode-step3-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step4_cmds,
			"qcom,panel-reader-mode-step4-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step5_cmds,
			"qcom,panel-reader-mode-step5-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step6_cmds,
			"qcom,panel-reader-mode-step6-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step7_cmds,
			"qcom,panel-reader-mode-step7-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step8_cmds,
			"qcom,panel-reader-mode-step8-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step9_cmds,
			"qcom,panel-reader-mode-step9-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_step10_cmds,
			"qcom,panel-reader-mode-step10-command", "qcom,mdss-dsi-reader-mode-command-state");
	return 0;
}

bool lge_change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int new_mode)
{
	switch(new_mode) {
		char mask;
	case READER_MODE_STEP_1:
		pr_info("%s: Reader Mode Step 1\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step1_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_2:
		pr_info("%s: Reader Mode Step 2\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step2_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_3:
		pr_info("%s: Reader Mode Step 3\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step3_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_4:
		pr_info("%s: Reader Mode Step 4\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step4_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_5:
		pr_info("%s: Reader Mode Step 5\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step5_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_6:
		pr_info("%s: Reader Mode Step 6\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step6_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_7:
		pr_info("%s: Reader Mode Step 7\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step7_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_8:
		pr_info("%s: Reader Mode Step 8\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step8_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_9:
		pr_info("%s: Reader Mode Step 9\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step9_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;
	case READER_MODE_STEP_10:
		pr_info("%s: Reader Mode Step 10\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step10_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= READER_GC_MASK;
#endif
		break;

	case READER_MODE_OFF:
	default:
		pr_info("%s: Reader Mode Step OFF\n",__func__);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_step0_cmds, CMD_REQ_COMMIT);
		mask = MONO_MASK;
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		mask = READER_GC_MASK;
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
#endif
	}
#if defined(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f0h_cmds, CMD_REQ_COMMIT);
	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);
#endif
	return true;
}

int lge_mdss_dsi_panel_send_on_cmds(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *default_on_cmds, int cur_mode)
{
	if (default_on_cmds->cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, default_on_cmds, CMD_REQ_COMMIT);

	lge_change_reader_mode(ctrl, cur_mode);

	return 0;
}
#endif

int lge_mdss_panel_parse_dt_extra(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc;
	u32 tmp;

	rc = of_property_read_u32(np, "lge,pre-on-cmds-delay", &tmp);
	ctrl_pdata->lge_extra.pre_on_cmds_delay = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "lge,pre-off-cmds-extra-delay", &tmp);
	ctrl_pdata->lge_extra.pre_off_cmds_extra_delay = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "lge,post-off-cmds-delay", &tmp);
	ctrl_pdata->lge_extra.post_off_cmds_delay = (!rc ? tmp : 0);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
	lge_mdss_dsi_parse_reader_mode_cmds(np, ctrl_pdata);
#endif

	parse_dt_extra_dcs_cmds(np, ctrl_pdata);

	return 0;
}

void lge_mdss_dsi_panel_extra_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
					const char *name)
{
	int i, index = -1;
	for (i = 0; i < ctrl->lge_extra.num_extra_cmds; ++i) {
		if (!strcmp(ctrl->lge_extra.extra_cmds_array[i].name, name)) {
			index = i;
			break;
		}
	}

	if (index != -1) {
		if (ctrl->lge_extra.extra_cmds_array[index].cmds.cmd_cnt)
			mdss_dsi_panel_cmds_send(ctrl,
			&ctrl->lge_extra.extra_cmds_array[index].cmds,
			CMD_REQ_COMMIT);
	} else {
		pr_err("%s: extra cmds %s not found\n", __func__, name);
	}
}

char *lge_get_blmapname(enum lge_bl_map_type  blmaptype)
{
	if (blmaptype >= 0 && blmaptype < LGE_BLMAPMAX)
		return lge_blmap_name[blmaptype];
	else
		return lge_blmap_name[LGE_BLDFT];
}

void lge_mdss_panel_parse_dt_blmaps(struct device_node *np,
				   struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int i, j, rc;
	u32 *array;

	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	pinfo->blmap_size = 256;
	array = kzalloc(sizeof(u32) * pinfo->blmap_size, GFP_KERNEL);

	if (!array)
		return;

	for (i = 0; i < LGE_BLMAPMAX; i++) {
		/* check if property exists */
		if (!of_find_property(np, lge_blmap_name[i], NULL))
			continue;

		pr_info("%s: found %s\n", __func__, lge_blmap_name[i]);

		rc = of_property_read_u32_array(np, lge_blmap_name[i], array,
						pinfo->blmap_size);
		if (rc) {
			pr_err("%s:%d, unable to read %s\n",
					__func__, __LINE__, lge_blmap_name[i]);
			goto error;
		}

		pinfo->blmap[i] = kzalloc(sizeof(int) * pinfo->blmap_size,
				GFP_KERNEL);

		if (!pinfo->blmap[i]){
			goto error;
		}

		for (j = 0; j < pinfo->blmap_size; j++)
			pinfo->blmap[i][j] = array[j];

	}
	kfree(array);
	return;

error:
	for (i = 0; i < LGE_BLMAPMAX; i++)
		if (pinfo->blmap[i])
			kfree(pinfo->blmap[i]);
	kfree(array);
}

#ifdef CONFIG_LGE_DISPLAY_BL_EXTENDED
int mdss_panel_parse_blex_settings(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	const char *data;

	ctrl_pdata->bkltex_ctrl = UNKNOWN_CTRL;
	data = of_get_property(np, "qcom,mdss-dsi-blex-pmic-control-type", NULL);
	if (data) {
		/* TODO: implement other types of backlight */
		if (!strcmp(data, "bl_ctrl_lge")) {
			ctrl_pdata->bkltex_ctrl = BL_OTHERS;
			pr_info("%s: Configured BL_OTHERS bkltex ctrl\n",
								__func__);
		} else {
			pr_info("%s: bkltex ctrl configuration fail\n",
								__func__);
		}
	}
	return 0;
}
#endif
