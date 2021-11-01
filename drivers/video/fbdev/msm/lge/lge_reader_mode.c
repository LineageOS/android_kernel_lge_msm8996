/* Copyright (c) 2013-2015, LGE Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *   * Neither the name of The Linux Foundation, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/of_platform.h>
#include "../mdss_fb.h"
#include "../mdss_mdp.h"
#include "../mdss_dsi.h"

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *pcmds, u32 flags);

static int cur_reader_mode;

__weak int lge_mdss_dsi_parse_reader_mode_cmds(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	pr_err("%s is not implemented.\n", __func__);
	return 0;
}

__weak bool lge_change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int new_mode)
{
	pr_err("%s is not implemented.\n", __func__);
	return false;
}

__weak int lge_mdss_dsi_panel_send_on_cmds(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *default_on_cmds, int cur_mode)
{
	pr_err("%s is not implemented\n", __func__);
	if (default_on_cmds->cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, default_on_cmds, CMD_REQ_COMMIT);

	return 0;
}

int lge_get_reader_mode(void)
{
	return cur_reader_mode;
}

ssize_t set_reader_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl;
	int new_mode;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected!\n");
		return count;
	}

	if (mdss_dsi_is_panel_off(pdata)) {
		new_mode = simple_strtoul(buf, NULL, 10);
		new_mode %= 100;
		cur_reader_mode = new_mode;
		pr_err("%s: Panel is off\n", __func__);
		return count;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (!ctrl) {
		pr_err("%s: ctrl is null\n", __func__);
		return count;
	}

	new_mode = simple_strtoul(buf, NULL, 10);
	new_mode %= 100;
	if (lge_change_reader_mode(ctrl, new_mode))
		cur_reader_mode = new_mode;

	return count;
}

ssize_t get_reader_mode(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", cur_reader_mode);

	return ret;
}
