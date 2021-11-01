
#include "../mdss_fb.h"
#include "lge_mdss_display.h"

#if defined(CONFIG_LGE_DISPLAY_READER_MODE)
#include "lge_reader_mode.h"
#endif

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds, u32 flags);

#if IS_ENABLED(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
static ssize_t sharpness_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	return sprintf(buf, "%x\n", ctrl->reg_f2h_cmds.cmds[0].payload[3]);
#else
	return sprintf(buf, "%x\n", ctrl->sharpness_on_cmds.cmds[2].payload[3]);
#endif
}

static ssize_t sharpness_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int param;
	ssize_t ret = strnlen(buf, PAGE_SIZE);

	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore sharpness enhancement cmd\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	sscanf(buf, "%x", &param);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	ctrl->reg_f2h_cmds.cmds[0].payload[3] = param;
	pr_info("%s: Sharpness = 0x%02x \n", __func__, param);

	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f2h_cmds, CMD_REQ_COMMIT);

	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);
#else
	ctrl->sharpness_on_cmds.cmds[2].payload[3] = param;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
	if(lge_get_reader_mode())
		ctrl->sharpness_on_cmds.cmds[1].payload[1] = 0x8A;
	else
		ctrl->sharpness_on_cmds.cmds[1].payload[1] = 0x82;
#endif
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->sharpness_on_cmds, CMD_REQ_COMMIT);

	pr_info("%s: sent sharpness enhancement cmd : 0x%02X\n",
			__func__, ctrl->sharpness_on_cmds.cmds[2].payload[3]);
#endif
	return ret;
}
static DEVICE_ATTR(sharpness, S_IWUSR|S_IRUGO, sharpness_get, sharpness_set);
#endif // CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS

#if IS_ENABLED(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
static ssize_t image_enhance_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	return sprintf(buf, "%d\n", ctrl->ie_on);
}

static ssize_t image_enhance_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	char mask = 0x00;
#endif
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore image enhancement cmd\n", __func__);
		return -EINVAL;
	}

	ctrl =	container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	sscanf(buf, "%d", &ctrl->ie_on);
	pr_info("%s: IE = %d \n", __func__, ctrl->ie_on);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	if (ctrl->ie_on == 1) {
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= SAT_MASK | SH_MASK;
	} else if (ctrl->ie_on == 0) {
		mask = SAT_MASK | SH_MASK;
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
	} else {
		pr_info("%s: set = %d, wrong set value\n", __func__, ctrl->ie_on);
	}

	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f0h_cmds, CMD_REQ_COMMIT);
	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);
#else
	if(ctrl->ie_on == 1){
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->ie_on_cmds, CMD_REQ_COMMIT);
		pr_info("%s: set = %d, image enhance function on\n", __func__, ctrl->ie_on);
	}
	else if(ctrl->ie_on == 0){
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
		if(lge_get_reader_mode()){
			pr_info("[Display]%s: reader on\n",__func__);
			ctrl->ie_off_cmds.cmds[1].payload[1] = 0x81;
		}
		else
		{
			pr_info("[Display]%s: reader off\n",__func__);
			ctrl->ie_off_cmds.cmds[1].payload[1] = 0x01;
		}
#endif
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->ie_off_cmds, CMD_REQ_COMMIT);
		pr_info("%s: set = %d, image enhance funtion off\n", __func__, ctrl->ie_on);
	}
	else
		pr_info("%s: set = %d, wrong set value\n", __func__, ctrl->ie_on);
#endif

	return ret;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
static int cabc_on_off = 1;
#else
unsigned int cabc_ctrl;
#endif
static ssize_t cabc_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	return sprintf(buf, "%d\n", cabc_on_off);
#else
	return sprintf(buf, "%d\n", cabc_ctrl);
#endif
}

static ssize_t cabc_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);

	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore cabc set cmd\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	sscanf(buf, "%d", &cabc_on_off);
#else
	sscanf(buf, "%d", &cabc_ctrl);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	if (cabc_on_off == 0) {
		char mask = CABC_MASK;
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_fbh_cmds.cmds[0].payload[4] |= CABC_OFF_VALUE;
	} else if (cabc_on_off == 1) {
		ctrl->reg_55h_cmds.cmds[0].payload[1] |= CABC_MASK;
		ctrl->reg_fbh_cmds.cmds[0].payload[4] |= CABC_ON_VALUE;
	} else {
		return -EINVAL;
	}

	pr_info("%s: CABC = %d, 55h = 0x%02x, fbh = 0x%02x\n",__func__,cabc_on_off,
		ctrl->reg_55h_cmds.cmds[0].payload[1],ctrl->reg_f0h_cmds.cmds[0].payload[1]);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_fbh_cmds, CMD_REQ_COMMIT);
#endif
	return ret;
}

static DEVICE_ATTR(image_enhance_set, S_IWUSR|S_IRUGO, image_enhance_get, image_enhance_set);
static DEVICE_ATTR(cabc, S_IWUSR|S_IRUGO, cabc_get, cabc_set);
#endif //CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL

#if IS_ENABLED(CONFIG_LGE_DISPLAY_LINEAR_GAMMA)
static ssize_t linear_gamma_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	return sprintf(buf, "%s\n", buf);
}

static ssize_t linear_gamma_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore color enhancement cmd\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	sscanf(buf, "%d", &input);
	if (input == 0) {
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->linear_gamma_default_cmds, CMD_REQ_COMMIT);
	} else {
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->linear_gamma_tuning_cmds, CMD_REQ_COMMIT);
	}
	return ret;
}

static DEVICE_ATTR(linear_gamma, S_IWUSR|S_IRUGO, linear_gamma_get, linear_gamma_set);
#endif // CONFIG_LGE_DISPLAY_LINEAR_GAMMA

#if IS_ENABLED(CONFIG_LGE_DISPLAY_SRE_MODE)
static ssize_t sre_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	return sprintf(buf, "%d\n", ctrl->sre_status);
}

static ssize_t sre_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input;
	char mask = SRE_MASK;

	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	sscanf(buf, "%d", &input);

	if (pdata->panel_info.panel_power_state == 0) {
		ctrl->sre_status = input;
		pr_err("%s: Panel off state. Ignore sre_set cmd\n", __func__);
		return -EINVAL;
	}

	if(ctrl->hdr_status > 0 || ctrl->dolby_status > 0) {
		pr_info("%s : HDR or Dolby on, so disable SRE \n", __func__);
		return ret;
	}

	ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
	if (input == 0) {
		ctrl->sre_status = 0;
		pr_info("%s : SRE OFF \n",__func__);
	} else {
		if (input == SRE_LOW) {
			ctrl->sre_status = SRE_LOW;
			pr_info("%s : SRE LOW \n",__func__);
			ctrl->reg_55h_cmds.cmds[0].payload[1] |= SRE_MASK_LOW;
		} else if (input == SRE_MID) {
			ctrl->sre_status = SRE_MID;
			pr_info("%s : SRE MID \n",__func__);
			ctrl->reg_55h_cmds.cmds[0].payload[1] |= SRE_MASK_MID;
		} else if (input == SRE_HIGH) {
			ctrl->sre_status = SRE_HIGH;
			pr_info("%s : SRE HIGH \n",__func__);
			ctrl->reg_55h_cmds.cmds[0].payload[1] |= SRE_MASK_HIGH;
		} else {
			return -EINVAL;
		}
	}
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);

	return ret;
}

static DEVICE_ATTR(sre_mode, S_IWUSR|S_IRUGO, sre_get, sre_set);
static DEVICE_ATTR(daylight_mode, S_IWUSR|S_IRUGO, sre_get, sre_set);
#endif // CONFIG_LGE_DISPLAY_SRE_MODE

#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
static ssize_t SW49408_MUX_gate_voltage_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	return sprintf(buf, "%d\n", ctrl->mux_gate_voltage_status);
}

static ssize_t SW49408_MUX_gate_voltage_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore MUX gate voltage set\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	sscanf(buf, "%d", &input);
	ctrl->mux_gate_voltage_status = input;

	if (input == 0) {
		pr_info("%s: Set MUX gate voltage to +8.7V/-8.8V\n", __func__);
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->vgho_vglo_8p8v_cmd, CMD_REQ_COMMIT);
	} else if (input == 1) {
		pr_info("%s: Set MUX gate voltage to +11.6V/-11.6V\n", __func__);
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->vgho_vglo_11p6v_cmd, CMD_REQ_COMMIT);
	} else {
		pr_info("%s: Invalid setting\n", __func__);
	}

	return ret;
}
static DEVICE_ATTR(mux_gate_voltage, S_IWUSR|S_IRUGO, SW49408_MUX_gate_voltage_get, SW49408_MUX_gate_voltage_set);
#endif //CONFIG_LGE_DISPLAY_LUCYE_COMMON

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DOLBY_MODE)
static ssize_t dolby_mode_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	return sprintf(buf, "%d\n", ctrl->dolby_status);
}

static ssize_t dolby_mode_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input;
	char mask = 0x00;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore color enhancement cmd\n", __func__);
		return -EINVAL;
	}
	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	sscanf(buf, "%d", &input);
	ctrl->dolby_status = input;

	if (input == 0) {
		pr_info("%s: Dolby Mode OFF\n", __func__);
		/* Retore 55h Reg */
		ctrl->reg_55h_cmds.cmds[0].payload[1] |= CABC_MASK;
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= SH_MASK | SAT_MASK;
		ctrl->reg_fbh_cmds.cmds[0].payload[4] = CABC_ON_VALUE;
	} else {
		pr_info("%s: Dolby Mode ON\n", __func__);
		/* Dolby Setting : CABC OFF, SRE OFF*/
		mask = (CABC_MASK | SRE_MASK);
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		mask = (SH_MASK | SAT_MASK);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_fbh_cmds.cmds[0].payload[4] = CABC_OFF_VALUE;
	}
	/* Send 55h, f0h cmds in lge_change_reader_mode function */
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_fbh_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f0h_cmds, CMD_REQ_COMMIT);

	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);
	return ret;
}
static DEVICE_ATTR(dolby_mode, S_IWUSR|S_IRUGO, dolby_mode_get, dolby_mode_set);
#endif // CONFIG_LGE_DISPLAY_DOLBY_MODE

#if IS_ENABLED(CONFIG_LGE_DISPLAY_HDR_MODE)
static ssize_t HDR_mode_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	return sprintf(buf, "%d\n", ctrl->hdr_status);
}

static ssize_t HDR_mode_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input;
	char mask = 0x00;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_state == 0) {
		pr_err("%s: Panel off state. Ignore color enhancement cmd\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	sscanf(buf, "%d", &input);
	ctrl->hdr_status = input;

	if (input == 0) {
		pr_info("%s: HDR Mode OFF\n", __func__);
		/* Retore 55h & F0h Reg */
		ctrl->reg_55h_cmds.cmds[0].payload[1] |= CABC_MASK;
		ctrl->reg_f0h_cmds.cmds[0].payload[1] |= SAT_MASK | SH_MASK;
		ctrl->reg_fbh_cmds.cmds[0].payload[4] = CABC_ON_VALUE;
	} else {
		pr_info("%s: HDR Mode ON\n", __func__);
		/* Dolby Setting : CABC OFF, SRE OFF, SAT OFF, SH OFF */
		mask = (CABC_MASK | SRE_MASK);
		ctrl->reg_55h_cmds.cmds[0].payload[1] &= (~mask);
		mask = (SH_MASK | SAT_MASK);
		ctrl->reg_f0h_cmds.cmds[0].payload[1] &= (~mask);
		ctrl->reg_fbh_cmds.cmds[0].payload[4] = CABC_OFF_VALUE;
	}
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_fbh_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_55h_cmds, CMD_REQ_COMMIT);
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->reg_f0h_cmds, CMD_REQ_COMMIT);

	pr_info("%s : 55h:0x%02x, f0h:0x%02x, f2h(SH):0x%02x, fbh(CABC):0x%02x \n",__func__,
		ctrl->reg_55h_cmds.cmds[0].payload[1],	ctrl->reg_f0h_cmds.cmds[0].payload[1],
		ctrl->reg_f2h_cmds.cmds[0].payload[3], ctrl->reg_fbh_cmds.cmds[0].payload[4]);

	return ret;
}
static DEVICE_ATTR(hdr_mode, S_IWUSR|S_IRUGO, HDR_mode_get, HDR_mode_set);
#endif // CONFIG_LGE_DISPLAY_HDR_MODE

#if defined(CONFIG_LGE_DISPLAY_READER_MODE)
static DEVICE_ATTR(comfort_view, S_IRUGO | S_IWUSR, get_reader_mode, set_reader_mode);
#endif // CONFIG_LGE_DISPLAY_READER_MODE

static struct attribute *lge_mdss_imgtune_attrs[] = {
#if IS_ENABLED(CONFIG_LGE_ENHANCE_GALLERY_SHARPNESS)
	&dev_attr_sharpness.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_LCD_DYNAMIC_CABC_MIE_CTRL)
	&dev_attr_image_enhance_set.attr,
	&dev_attr_cabc.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LINEAR_GAMMA)
	&dev_attr_linear_gamma.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_SRE_MODE)
	&dev_attr_sre_mode.attr,
	&dev_attr_daylight_mode.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_LUCYE_COMMON)
	&dev_attr_mux_gate_voltage.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DOLBY_MODE)
	&dev_attr_dolby_mode.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_HDR_MODE)
	&dev_attr_hdr_mode.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
	&dev_attr_comfort_view.attr,
#endif
	NULL,
};

static struct attribute_group lge_mdss_imgtune_attr_group = {
	.attrs = lge_mdss_imgtune_attrs,
};

static struct device *lge_panel_sysfs_imgtune = NULL;

int lge_mdss_sysfs_imgtune_init(struct class *panel, struct fb_info *fbi)
{
	int rc = 0;

	if(!lge_panel_sysfs_imgtune){
		lge_panel_sysfs_imgtune = device_create(panel, NULL, 0, fbi, "img_tune");
		if (IS_ERR(lge_panel_sysfs_imgtune)) {
			pr_err("%s: Failed to create dev(lge_panel_sysfs_imgtune)!", __func__);
		}
		else {
			rc = sysfs_create_group(&lge_panel_sysfs_imgtune->kobj, &lge_mdss_imgtune_attr_group);
			if (rc)
				pr_err("lge sysfs group creation failed, rc=%d\n", rc);
		}
	}
	return rc;
}

void lge_mdss_sysfs_imgtune_remove(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &lge_mdss_imgtune_attr_group);
}
