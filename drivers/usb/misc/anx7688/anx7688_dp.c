/*
  * Copyright(c) 2016, LG Electronics. All rights reserved.
  *
  * anx7688 DP Block Driver
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

#include "anx7688_dp.h"

struct anx7688_chip *g_pdata;

struct msm_hdmi_slimport_ops *hdmi_slimport_ops;

static unsigned char __i2c_read_byte(unsigned char dev, unsigned char offset)
{
    unsigned char temp;
    temp = OhioReadReg(dev, offset);
    return temp;
}

#define write_dpcd_addr(addrh, addrm, addrl) \
	do { \
		unchar temp; \
		if (__i2c_read_byte(DP_HDCP_ADDR, AUX_ADDR_7_0) != (unchar)addrl) \
			OhioWriteReg(DP_HDCP_ADDR, AUX_ADDR_7_0, (unchar)addrl); \
			if (__i2c_read_byte(DP_HDCP_ADDR, AUX_ADDR_15_8) != (unchar)addrm) \
			OhioWriteReg(DP_HDCP_ADDR, AUX_ADDR_15_8, (unchar)addrm); \
		temp = OhioReadReg(DP_HDCP_ADDR, AUX_ADDR_19_16); \
		if ((unchar)(temp & 0x0F)  != ((unchar)addrh & 0x0F)) \
			OhioWriteReg(DP_HDCP_ADDR, AUX_ADDR_19_16, (temp  & 0xF0) | ((unchar)addrh)); \
	} while (0)

void sp_tx_rst_aux(void)
{
    OhioWriteReg(DP_CORE_ADDR, RST_CTRL2, ((unsigned char)__i2c_read_byte(DP_CORE_ADDR, RST_CTRL2) | AUX_RST));
    OhioWriteReg(DP_CORE_ADDR, RST_CTRL2, ((unsigned char)__i2c_read_byte(DP_CORE_ADDR, RST_CTRL2) & (~AUX_RST)));
}


void wait_aux_op_finish(unchar * err_flag)
{
    unchar cnt;
    unchar c;

    *err_flag = 0;
    cnt = 150;
    while (__i2c_read_byte(DP_HDCP_ADDR, AUX_CTRL2) & AUX_OP_EN) {
                mdelay(2);
                if ((cnt--) == 0) {
                    pr_err("%s %s :aux operate failed!\n", LOG_TAG, __func__);
                    *err_flag = 1;
                    break;
                }
            }

    c = OhioReadReg(DP_HDCP_ADDR, SP_TX_AUX_STATUS);
    if (c & 0x0F) {
            pr_info("%s %s : wait aux operation status %.2x\n", LOG_TAG, __func__, (uint)c);
            *err_flag = 1;
    }
}

unchar sp_tx_dpcdread_bytes(unchar addrh, unchar addrm, unchar addrl, unchar cCount, unchar *pBuf)
{
	unchar c, c1, i;
	unchar aux_status;
	OhioWriteReg(DP_HDCP_ADDR, BUF_DATA_COUNT, 0x80);	/* Clear buffer */

	c = ((cCount - 1) << 4) | 0x09;
	OhioWriteReg(DP_HDCP_ADDR, AUX_CTRL, c);
	write_dpcd_addr(addrh, addrm, addrl);
	OhioWriteReg(DP_HDCP_ADDR, AUX_CTRL2, ((unsigned char)__i2c_read_byte(DP_HDCP_ADDR, AUX_CTRL2) | AUX_OP_EN));
	mdelay(2);

	wait_aux_op_finish(&aux_status);
    if (aux_status == AUX_ERR) {
        pr_err("%s %s : aux read failed\n", LOG_TAG, __func__);
        /* add by span 20130217. */
        c = OhioReadReg(DP_CORE_ADDR, SP_TX_INT_STATUS1);
        c1 = OhioReadReg(DP_HDCP_ADDR, TX_DEBUG1);
        /* if polling is enabled, wait polling error interrupt */
        if (c1&POLLING_EN) {
            if (c & POLLING_ERR)
                sp_tx_rst_aux();
        } else
            sp_tx_rst_aux();
        return AUX_ERR;
    }

    for (i = 0; i < cCount; i++) {
        c = OhioReadReg(DP_HDCP_ADDR, BUF_DATA_0 + i);
		*(pBuf + i) = c;
        if (i >= MAX_BUF_CNT)
            break;
    }
    return AUX_OK;
}

void slimport_set_hdmi_hpd(int on)
{
	static int hdmi_hpd_flag = 0;
	int rc = 0;

	pr_debug("%s %s:+\n", LOG_TAG, __func__);
	if (on && hdmi_hpd_flag != 1) {
		hdmi_hpd_flag = 1;
		rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 1);
		pr_info("%s %s:hpd on = %s\n", LOG_TAG, __func__,
				rc ? "failed" : "passed");
		if (rc) {
			msleep(2000);
			rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 1);
		}
	} else if (!on && hdmi_hpd_flag != 0) {
		hdmi_hpd_flag = 0;
		rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 0);
		pr_info("%s %s: hpd off = %s\n", LOG_TAG, __func__,
				rc ? "failed" : "passed");
	} else {
		pr_info("%s %s: hpd status is stupid.\n", LOG_TAG, __func__);
	}
	pr_debug("%s %s:-\n", LOG_TAG, __func__);
}

void rx_set_cable_type(void)
{
	struct anx7688_chip *chip = g_pdata;
	unchar dongle_type;
	unchar ret;
	ret = sp_tx_dpcdread_bytes(0x00, 0x00, 0x05, 1, &dongle_type);

	if(ret == AUX_OK) {
		switch((dongle_type & (BIT(1) | BIT(2))) >> 1)
		{
			case 0x00:
				chip->rx_cable_type = DP_TYPE;
				pr_info("%s %s: Downstream is DP dongle.\n", LOG_TAG, __func__);
				break;
			case 0x01:
			case 0x03:
				chip->rx_cable_type = VGA_TYPE;
				pr_info("%s %s: Downstream is VGA Dongle.\n", LOG_TAG, __func__);
				break;
			case 0x02:
				chip->rx_cable_type = HDMI_TYPE;
				pr_info("%s %s: Downstream is HDMI Dongle.\n", LOG_TAG, __func__);
				break;
			default:
				chip->rx_cable_type = NONE_TYPE;
				pr_info("%s %s: Downstream is NULL.\n", LOG_TAG, __func__);
				break;
		}
	} else
		chip->rx_cable_type = NONE_TYPE;
}
EXPORT_SYMBOL(rx_set_cable_type);

int rx_get_cable_type(void)
{
	struct anx7688_chip *chip = g_pdata;

	return chip->rx_cable_type;
}
EXPORT_SYMBOL(rx_get_cable_type);

bool is_vga_dongle(void)
{
	struct anx7688_chip *chip = g_pdata;

	if (chip->rx_cable_type == VGA_TYPE)
		return true;
	else
		return false;
}

static ssize_t anx7688_sysfs_rda_hdmi_vga(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	pr_info("%s %s : vga = %d\n", LOG_TAG, __func__, is_vga_dongle());
	return sprintf(buf, "%d\n", is_vga_dongle());
}

static struct device_attribute anx7688_device_attrs[] = {
	__ATTR(hdmi_vga, S_IRUGO, anx7688_sysfs_rda_hdmi_vga, NULL),
};

int dp_create_sysfs_interface(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(anx7688_device_attrs); i++)
		if (device_create_file(dev, &anx7688_device_attrs[i]))
			goto err;
	return 0;
err:
	for ( ; i >= 0; i--)
		device_remove_file(dev, &anx7688_device_attrs[i]);
	pr_err("%s %s: Unable to create interface", LOG_TAG, __func__);
	return -EINVAL;
}

bool slimport_is_connected(void)
{
	return g_pdata->dp_hpd_status > STATE_LINK_TRAINING ? 1 : 0;
}
EXPORT_SYMBOL(slimport_is_connected);

void sp_rx_cur_info(void)
{
	struct anx7688_chip *chip = g_pdata;

	chip->dp_rx_bandwidth = OhioReadReg(TCPC_ADDR, RX_BANDWIDTH);
	pr_debug("%s %s: rx_bandwidth : 0x%x\n", LOG_TAG,
				__func__, chip->dp_rx_bandwidth);

	chip->dp_rx_lanecount = OhioReadReg(TCPC_ADDR, RX_LANECOUNT);
	pr_debug("%s %s: rx_lanecount : %d\n", LOG_TAG,
				__func__, chip->dp_rx_lanecount);
}
EXPORT_SYMBOL(sp_rx_cur_info);

unsigned int sp_get_rx_lanecnt(void)
{
	struct anx7688_chip *chip = g_pdata;

	if(chip->dp_rx_lanecount < 0)
		chip->dp_rx_lanecount = OhioReadReg(TCPC_ADDR, RX_LANECOUNT);

	return chip->dp_rx_lanecount;
}
EXPORT_SYMBOL(sp_get_rx_lanecnt);

unsigned char sp_get_rx_bw(void)
{
	struct anx7688_chip *chip = g_pdata;

	if (chip->dp_rx_bandwidth <= 0)
		chip->dp_rx_bandwidth = OhioReadReg(TCPC_ADDR, RX_BANDWIDTH);

	return chip->dp_rx_bandwidth;
}
EXPORT_SYMBOL(sp_get_rx_bw);

void dp_init_variables(struct anx7688_chip *chip)
{
	chip->dp_hpd_status = STATE_LINK_TRAINING;
	chip->dp_rx_bandwidth = 0;
	chip->dp_rx_lanecount = 0;
	chip->rx_cable_type = NONE_TYPE;
}
EXPORT_SYMBOL(dp_init_variables);

void dp_read_hpd_status(struct anx7688_chip *chip)
{
	int ret;

	ret = OhioReadReg(TCPC_ADDR, 0x82);

	if (ret < 0) {
		pr_err("%s %s: failed to read status\n", LOG_TAG, __func__);
	} else {
		switch(chip->dp_hpd_status) {
		case STATE_LINK_TRAINING:
			if (ret & 0x1) {
				pr_info("%s %s: Link training Success\n",
						LOG_TAG, __func__);
				chip->dp_hpd_status++;
			}
			break;
		case STATE_HDMI_HPD:
			if (ret & 0x2) {
				pr_info("%s %s: HDMI_HPD enabled\n",
						LOG_TAG, __func__);
				chip->dp_hpd_status++;

			}
			break;
		case STATE_IRQ_PROCESS:
			if (ret & 0x4) {
				pr_info("%s %s: HPD_IRQ processed\n",
						LOG_TAG, __func__);
				chip->dp_hpd_status++;

			}
			break;
		case STATE_HPD_DONE:
			break;
		default:
			break;
		}
	}
}

void dp_print_status(int ret)
{
	const char *dp_system_status[] = {
		"NONE",
		"DP_HPD Assert High",
		"Get DP HPD IRQ & Link Trainning",
		"EDID is READY",
		"HDMI input video stable",
		"DP VIDEO STABLE",
		"DP AUDIO STABLE",
		"PLAYBACK"
	};
	pr_info("%s %s: %s\n", LOG_TAG, __func__,
			dp_system_status[ret]);
}

int dp_read_display_status(void)
{
	static int previous_status = -1;
	int ret;

	ret = OhioReadReg(TCPC_ADDR, 0x87);
	if (ret < 0) {
		pr_err("%s %s: failed to read status\n", LOG_TAG, __func__);
	} else {
		if (previous_status != ret) {
			dp_print_status(ret);
			previous_status = ret;
		}
	}
	return ret;
}

static void anx7688_dp_work(struct work_struct *work)
{
	struct anx7688_chip *chip = container_of(work,
				struct anx7688_chip, swork.work);
	int workqueue_timer = 0;

	if (chip->state <= STATE_UNATTACHED_SRC) {
		pr_info("%s: exit to dp queue\n", __func__);
		return;
	}

	pr_debug("%s : +\n", __func__);

	if(dp_read_display_status() >= DP_AUDIO_STABLE)
		workqueue_timer = 500;
	else
		workqueue_timer = 50;

	//mutex_lock(&chip->slock);
	dp_read_hpd_status(chip);
	dp_read_display_status();
	//mutex_unlock(&td->slock);
	queue_delayed_work(chip->dp_wq, &chip->swork,
			msecs_to_jiffies(workqueue_timer));

	pr_debug("%s : -\n", __func__);
}

int init_anx7688_dp(struct device *dev, struct anx7688_chip *chip)
{
	int ret = 0;
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	struct device_node *np = dev->of_node;
	struct platform_device *sp_pdev = NULL;
	struct device_node *sp_tx_node = NULL;

	/* to access anx7688 data */
	g_pdata = chip;

	sp_tx_node = of_parse_phandle(np, "qcom,hdmi-tx-map", 0);
	if (!sp_tx_node) {
		pr_err("%s %s: can't find hdmi phandle\n", LOG_TAG, __func__);
		return -ENODEV;
	}

	sp_pdev = of_find_device_by_node(sp_tx_node);
	if (!sp_pdev) {
		pr_err("%s %s: can't find the device by node\n",
				LOG_TAG, __func__);
		return -ENODEV;
	}
	chip->hdmi_pdev = sp_pdev;

	hdmi_slimport_ops = devm_kzalloc(dev, sizeof(struct msm_hdmi_slimport_ops),
			GFP_KERNEL);
	if (!hdmi_slimport_ops) {
		pr_err("%s %s: alloc hdmi slimport ops failed\n",
				LOG_TAG, __func__);
		return -ENOMEM;
	}

	if (chip->hdmi_pdev) {
		ret = msm_hdmi_register_slimport(chip->hdmi_pdev,
				hdmi_slimport_ops, chip);

		if(ret){
			pr_err("%s %s: register with hdmi failed\n",
					LOG_TAG, __func__);
			ret = -EPROBE_DEFER;
			return ret;
		}
	}

#endif
	INIT_DELAYED_WORK(&chip->swork, anx7688_dp_work);

	chip->dp_wq = create_singlethread_workqueue("anx7688-dp-work");
	if (chip->dp_wq == NULL) {
		pr_err("%s %s: failed to create work queue\n", LOG_TAG, __func__);
		ret = -ENOMEM;
	}

	pr_info("%s %s: init anx7688 dp block\n",LOG_TAG, __func__);
	return ret;
}

void dp_variables_remove(struct device *dev)
{
	g_pdata = 0;
	devm_kfree(dev, hdmi_slimport_ops);
}

MODULE_DESCRIPTION("I2C bus driver for Anx7688 DP block");
MODULE_AUTHOR("Hanwool Lee <hanwool.lee@lge.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.4");
