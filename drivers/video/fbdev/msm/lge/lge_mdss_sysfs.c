#include "../mdss_fb.h"
#include "lge_mdss_display.h"
#include <linux/lge_display_debug.h>

extern int lge_mdss_sysfs_imgtune_init(struct class *panel, struct fb_info *fbi);

static struct class *panel = NULL;

static ssize_t mdss_fb_get_panel_type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int panel_type = lge_get_panel();

	if (panel_type == LGE_SIC_LG4946_INCELL_CND_PANEL)
		ret = snprintf(buf, PAGE_SIZE, "LGD - LG4946\n");
	else if (panel_type == LGD_SIC_LG49407_INCELL_CMD_PANEL)
		ret = snprintf(buf, PAGE_SIZE, "LGD - SW49407 cmd\n");
	else if (panel_type == LGD_SIC_LG49407_INCELL_VIDEO_PANEL)
		ret = snprintf(buf, PAGE_SIZE, "LGD - SW49407 video\n");
	else if (panel_type == LGD_SIC_LG49407_1440_2880_INCELL_VIDEO_PANEL)
		ret = snprintf(buf, PAGE_SIZE, "LGD - SW49407 1440 X 2880 video\n");
	else if (panel_type == LGD_SIC_LG49408_1440_2880_INCELL_CMD_PANEL)
		ret = snprintf(buf, PAGE_SIZE, "LGD - SW49408 1440 X 2880 cmd\n");
	else
		ret = snprintf(buf, PAGE_SIZE, "Unknown LCD TYPE\n");

	return ret;
}

__weak ssize_t mdss_fb_is_valid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = scnprintf(buf, PAGE_SIZE, "DDIC validation is not implemented\n");
	return ret;
}

static DEVICE_ATTR(panel_type, S_IRUGO,
		mdss_fb_get_panel_type, NULL);
static DEVICE_ATTR(valid_check, S_IRUGO, mdss_fb_is_valid, NULL);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED)
static ssize_t mdss_fb_get_mq_mode(struct device *dev,
		struct device_attribute *arrt, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	int ret;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if(!pdata) {
		pr_err("[MARQUEE] no panel connected!\n");
		return -EINVAL;
	}
	pinfo = &pdata->panel_info;

	ret = scnprintf(buf, PAGE_SIZE, "mode : %d, direction : %d, speed : %d, S_x : %u, E_x : %u, S_y : %u, E_y : %u\n",
			pinfo->mq_mode, pinfo->mq_direction, pinfo->mq_speed,
			(pinfo->xres-(pinfo->mq_pos.start_x*4)),(pinfo->xres-(pinfo->mq_pos.end_x*4)),
			pinfo->mq_pos.start_y, pinfo->mq_pos.end_y);

	return ret;
}
static ssize_t mdss_fb_set_mq_mode(struct device *dev,
			   struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_ctrl_pdata *ctrl;
	unsigned int mode_temp, direction_temp, speed_temp, start_x_temp, end_x_temp, start_y_temp, end_y_temp;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[MARQUEE] no panel connected!\n");
		return len;
	}

	pinfo = &pdata->panel_info;

	if (sscanf(buf,"%d %d %d %d %d %d %d",&mode_temp, &direction_temp, &speed_temp,
			&start_x_temp, &end_x_temp,
			&start_y_temp, &end_y_temp) != 7) {
		pr_err("sccanf buf error!\n");
		return len;
	}

	if (start_x_temp > pinfo->xres || end_x_temp > pinfo->xres){
		pr_err("xpos should be shorter than xres! start x = %u,end x = %u\n",start_x_temp,end_x_temp);
		return len;
	}

	if (start_x_temp > end_x_temp) {
		pr_err("start x pos should be shorter than end x pos! start x = %u,end x = %u\n",start_x_temp,end_x_temp);
		return len;
	}

	start_x_temp = (pinfo->xres - start_x_temp)/4; // x-pos is moved by 4 pixel & position is counted from right end.
	end_x_temp	= (pinfo->xres - end_x_temp)/4;

	if (mode_temp > 0x01) {
		pr_err("mq_mode max setting is 1!\n");
		mode_temp = 0x01;
	}
	if (direction_temp > 0x01) {
		pr_err("mq_direction max setting is 1!\n");
		direction_temp = 0x01;
	}
	if (speed_temp > 0x1F) {
		pr_err("mq_speed max setting is 1F!\n");
		speed_temp = 0x1F;
	}
	if (start_x_temp > 0x1FF) {
		pr_err("start x max setting is 0x1FF!\n");
		start_x_temp = 0x1FF;
	}
	if (end_x_temp > 0x1FF) {
		pr_err("end x max setting is 0x1FF!\n");
		end_x_temp = 0x1FF;
	}
	if (start_y_temp > 0xFF) {
		pr_err("start y max setting is 0xFF!\n");
		start_y_temp = 0xFF;
	}
	if (end_y_temp > 0xFF) {
		pr_err("end y max setting is 0xFF!\n");
		end_y_temp = 0xFF;
	}
	pinfo->mq_mode = mode_temp;
	pinfo->mq_direction = direction_temp;
	pinfo->mq_speed = speed_temp;
	pinfo->mq_pos.start_x = start_x_temp;
	pinfo->mq_pos.end_x = end_x_temp;
	pinfo->mq_pos.start_y = start_y_temp;
	pinfo->mq_pos.end_y = end_y_temp-1;//SignBoard App inputs the height of ex-panel(0~160) but postion is (0~159).

	pr_info("[MARQUEE] %d %d %d %u %u %u %u\n",
			pinfo->mq_mode, pinfo->mq_direction, pinfo->mq_speed,
			pinfo->mq_pos.start_x, pinfo->mq_pos.end_x,
			pinfo->mq_pos.start_y, pinfo->mq_pos.end_y);

	return len;
}
static DEVICE_ATTR(mq_mode, S_IWUSR|S_IRUGO, mdss_fb_get_mq_mode, mdss_fb_set_mq_mode);
#endif // CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED

#if IS_ENABLED(CONFIG_LGE_THERMAL_BL_MAX)
static ssize_t thermal_blmax_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	int ret;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if (!pdata) {
		pr_err("[thermal_blmax] no panel connected!\n");
		return -EINVAL;
	}
	pinfo = &pdata->panel_info;
    ret = scnprintf(buf, PAGE_SIZE, "%d\n", pinfo->thermal_maxblvalue);
	return ret;
}

static ssize_t thermal_blmax_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	int value = 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[thermal_blmax] no panel connected!\n");
		return len;
	}
	pinfo = &pdata->panel_info;

	if (sscanf(buf, "%d", &value) != 1) {
		pr_err("[thermal_blmax] sccanf buf error!\n");
		return len;
	}
	pinfo->thermal_maxblvalue = value;
	if( pinfo->thermal_maxblvalue != 0 ){
		pinfo->brightness_max = pinfo->thermal_maxblvalue;
		pr_info("[thermal_blmax] set brightness_max=%d\n",
				pinfo->brightness_max);
	}  else {
		pinfo->brightness_max = 255;
		pr_info("[thermal_blmax] unset brightness_max=%d\n",
				pinfo->brightness_max);
	}
	return len;
}
static DEVICE_ATTR(thermal_blmax, S_IWUSR|S_IRUGO, thermal_blmax_show , thermal_blmax_store);
#endif // CONFIG_LGE_THERMAL_BL_MAX

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DYNAMIC_LOG)
uint32_t display_debug_level = LEVEL_INFOR;
static ssize_t display_debug_level_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

    ret = scnprintf(buf, PAGE_SIZE, "%d\n", display_debug_level);
	return ret;
}

static ssize_t display_debug_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) != 1) {
		DISP_ERR(NONE, "sccanf buf error!\n");
		return len;
	}
	if (value < LEVEL_ERR || value >= LEVEL_MAX) {
		DISP_ERR(NONE, "Fail to set debug level!\n");
		return len;
	} else {
		DISP_INFO(NONE, "Change debug from %d to %d\n", display_debug_level, value);
		display_debug_level = value;
	}
	return len;
}
static DEVICE_ATTR(debug_level, S_IWUSR|S_IRUGO, display_debug_level_show , display_debug_level_store);
#endif // CONFIG_LGE_DISPLAY_DYNAMIC_LOG

#if IS_ENABLED(CONFIG_LGE_SP_MIRRORING_CTRL_BL)
extern ssize_t mdss_fb_get_bl_off_and_block(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t mdss_fb_set_bl_off_and_block(struct device *dev,
		struct device_attribute *attr,const char *buf,  size_t count);
static DEVICE_ATTR(sp_link_backlight_off, S_IRUGO | S_IWUSR,
	mdss_fb_get_bl_off_and_block, mdss_fb_set_bl_off_and_block);
#endif // CONFIG_LGE_SP_MIRRORING_CTRL_BL

/*---------------------------------------------------------------------------*/
/* High luminance function                                                   */
/*---------------------------------------------------------------------------*/
#if IS_ENABLED(CONFIG_LGE_HIGH_LUMINANCE_MODE)
static ssize_t hl_mode_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[hl_mode] no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", pinfo->hl_mode_on);
	return ret;
}

static ssize_t hl_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[hl_mode] no panel connected!\n");
		return len;
	}

	pinfo->hl_mode_on = simple_strtoul(buf, NULL, 10);

	if(pinfo->hl_mode_on == 1)
		pr_info("[hl_mode] hl_mode on\n");
	else
		pr_info("[hl_mode] hl_mode off\n");

	return len;
}

static DEVICE_ATTR(hl_mode, S_IWUSR|S_IRUGO, hl_mode_show, hl_mode_store);
#endif // CONFIG_LGE_HIGH_LUMINANCE_MODE

#if IS_ENABLED(CONFIG_LGE_PANEL_RECOVERY)
extern ssize_t get_recovery_mode(struct device *dev,
                struct device_attribute *attr, char *buf);
extern ssize_t set_recovery_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);
static DEVICE_ATTR(recovery_mode, S_IWUSR|S_IRUGO, get_recovery_mode, set_recovery_mode);
#endif // CONFIG_LGE_PANEL_RECOVERY

/*---------------------------------------------------------------------------*/
/* Alawys On display                                                         */
/*---------------------------------------------------------------------------*/
#if IS_ENABLED(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
static ssize_t mdss_fb_get_cur_panel_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int cur_panel_mode;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}
	if(mfd->index != 0)
		return -EINVAL;

	mutex_lock(&mfd->aod_lock);
	if (pinfo->aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK)
		cur_panel_mode = AOD_PANEL_MODE_U2_BLANK;
	else
		cur_panel_mode = pinfo->aod_cur_mode;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	if (cur_panel_mode == AOD_PANEL_MODE_U3_UNBLANK && pinfo->ext_off_temp)
		cur_panel_mode = AOD_PANEL_MODE_U1_UNBLANK;
#endif
	mutex_unlock(&mfd->aod_lock);
	pr_info("[AOD] %s : Current panel mode : %d\n", __func__ , cur_panel_mode);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
		cur_panel_mode);

	return ret;
}
static ssize_t mdss_fb_set_aod(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	int old_value, new_value;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &new_value) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}
	mutex_lock(&mfd->aod_lock);
	old_value = pdata->panel_info.aod_node_from_user;
	pdata->panel_info.aod_node_from_user = new_value;
	mutex_unlock(&mfd->aod_lock);
	pr_info("[AOD] old value : %d, new value : %d\n", old_value, new_value);
	return len;
}
static ssize_t mdss_fb_get_aod(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
		pinfo->aod_node_from_user);

	return ret;
}

static ssize_t mdss_fb_set_keep_aod(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	int old_value, new_value;
	int rc;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &new_value) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}
	mutex_lock(&mfd->aod_lock);
	old_value = pdata->panel_info.aod_keep_u2;
	pdata->panel_info.aod_keep_u2 = new_value;
	mutex_unlock(&mfd->aod_lock);
	pr_info("[AOD] keep_aod old value : %d, new value : %d\n", old_value, new_value);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
	if (new_value == AOD_MOVE_TO_U3)
		mfd->ready_to_u2 = false;
#endif
	/* Current mode is  AOD_PANEL_MODE_U2_UNBLANK and if set keep_aod by 0,
	     We have to U2-> U3 command only */
	if (pdata->panel_info.aod_cur_mode == AOD_PANEL_MODE_U2_UNBLANK &&
		new_value == AOD_MOVE_TO_U3) {
		struct mdss_dsi_ctrl_pdata *ctrl;

		ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

		mutex_lock(&mfd->aod_lock);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		// this flag will reset 0 in full frame kickoff(__validate_roi_and_set function).
		mfd->keep_aod_pending = true;
		pr_info("keep_aod_pending set to true\n");
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_AOD_WITH_MIPI)
		/* When Move to U3 mode from U2 unblank */
		/* 1. Turn off backlight	*/
		/* 2. Block aod bl control */
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, 0);
		mutex_unlock(&mfd->bl_lock);
		mfd->block_aod_bl = true;
#endif
		rc = oem_mdss_aod_cmd_send(mfd, AOD_CMD_DISABLE);
		if (rc)
			pr_err("[AOD] Fail to send U2->U3 command\n");

		oem_mdss_aod_set_backlight_mode(mfd);
		pdata->panel_info.aod_keep_u2 = AOD_NO_DECISION;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, mfd->br_lvl_ex);
		mutex_unlock(&mfd->bl_lock);
#endif
		mutex_unlock(&mfd->aod_lock);
	}
	return len;
}
static ssize_t mdss_fb_get_keep_aod(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[AOD] no panel connected!\n");
		return -EINVAL;
	}
	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
		pinfo->aod_keep_u2);

	return ret;
}

static DEVICE_ATTR(cur_panel_mode, S_IRUGO, mdss_fb_get_cur_panel_mode, NULL);
static DEVICE_ATTR(aod, S_IWUSR|S_IRUGO, mdss_fb_get_aod, mdss_fb_set_aod);
static DEVICE_ATTR(keep_aod, S_IWUSR|S_IRUGO, mdss_fb_get_keep_aod,
			mdss_fb_set_keep_aod);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
static ssize_t mdss_fb_toggle_u1(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ext_off;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &ext_off) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}

	pr_info("[AOD]%s called: ext_off: %d\n", __func__, ext_off);
	mutex_lock(&mfd->aod_lock);
	if (pinfo->ext_off_temp != ext_off) {
		pr_info("%d -> %d\n", pinfo->ext_off_temp, ext_off);
		pinfo->ext_off_temp = ext_off;
		oem_mdss_aod_set_backlight_mode(mfd);
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, mfd->br_lvl_ex);
		mutex_unlock(&mfd->bl_lock);
	}
	mutex_unlock(&mfd->aod_lock);
	return len;
}

static ssize_t mdss_fb_get_toggle_u1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			pinfo->ext_off_temp);

	return ret;
}

static ssize_t mdss_fb_set_ext_off(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ext_off;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &ext_off) != 1) {
		pr_err("sccanf buf error!\n");
		return -EINVAL;
	}

	pr_info("[AOD]%s called: ext_off: %d\n", __func__, ext_off);
	mutex_lock(&mfd->aod_lock);
	if (pinfo->ext_off != ext_off) {
		pr_info("%d -> %d\n", pinfo->ext_off, ext_off);
		pinfo->ext_off = ext_off;
		pinfo->ext_off_temp = ext_off;
		oem_mdss_aod_set_backlight_mode(mfd);
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_bl_brightness_aod_sub(mfd, mfd->br_lvl_ex);
		mutex_unlock(&mfd->bl_lock);
	}
	mutex_unlock(&mfd->aod_lock);
	return len;
}

static ssize_t mdss_fb_get_ext_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int ret;

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", pinfo->ext_off);

	return ret;
}

static DEVICE_ATTR(toggle_u1, S_IWUSR|S_IRUGO, mdss_fb_get_toggle_u1,
			mdss_fb_toggle_u1);
static DEVICE_ATTR(ext_off, S_IWUSR|S_IRUGO, mdss_fb_get_ext_off,
			mdss_fb_set_ext_off);
#endif // CONFIG_LGE_DISPLAY_BL_EXTENDED
#endif // CONFIG_LGE_DISPLAY_AOD_SUPPORTED

/*---------------------------------------------------------------------------*/
/* Register lge sysfs attributes                                             */
/*---------------------------------------------------------------------------*/
/* TODO: implement registering method for attributes in other lge files */
static struct attribute *lge_mdss_fb_attrs[] = {
	&dev_attr_panel_type.attr,
	&dev_attr_valid_check.attr,
#if IS_ENABLED(CONFIG_LGE_DISPLAY_MARQUEE_SUPPORTED)
	&dev_attr_mq_mode.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_THERMAL_BL_MAX)
  &dev_attr_thermal_blmax.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DYNAMIC_LOG)
	&dev_attr_debug_level.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_SP_MIRRORING_CTRL_BL)
	&dev_attr_sp_link_backlight_off.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_HIGH_LUMINANCE_MODE)
	&dev_attr_hl_mode.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_PANEL_RECOVERY)
	&dev_attr_recovery_mode.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_AOD_SUPPORTED)
	&dev_attr_aod.attr,
	&dev_attr_cur_panel_mode.attr,
	&dev_attr_keep_aod.attr,
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_EXTENDED)
	&dev_attr_toggle_u1.attr,
	&dev_attr_ext_off.attr,
#endif // CONFIG_LGE_DISPLAY_BL_EXTENDED
#endif // CONFIG_LGE_DISPLAY_AOD_SUPPORTED
	NULL,
};

static struct attribute_group lge_mdss_fb_attr_group = {
	.attrs = lge_mdss_fb_attrs,
};

/*---------------------------------------------------------------------------*/
/* Register lge sysfs attributes                                             */
/*---------------------------------------------------------------------------*/
/* TODO: implement registering method for attributes in other lge files */
int lge_mdss_sysfs_init(struct msm_fb_data_type *mfd)
{
	int rc = 0;

	if(!panel){
		panel = class_create(THIS_MODULE, "panel");
		if (IS_ERR(panel))
			pr_err("%s: Failed to create panel class\n", __func__);
	}

	rc = lge_mdss_sysfs_imgtune_init(panel, mfd->fbi);
	if (rc)
		pr_err("lge imgtune sysfs group creation failed, rc=%d\n", rc);

	rc += sysfs_create_group(&mfd->fbi->dev->kobj, &lge_mdss_fb_attr_group);
	if (rc)
		pr_err("lge fb sysfs group creation failed, rc=%d\n", rc);

	return rc;
}

void lge_mdss_sysfs_remove(struct msm_fb_data_type *mfd)
{
	sysfs_remove_group(&mfd->fbi->dev->kobj, &lge_mdss_fb_attr_group);
}

