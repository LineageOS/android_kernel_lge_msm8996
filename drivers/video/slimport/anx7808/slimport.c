/*
 * Copyright(c) 2012, LG Electronics Inc. All rights reserved.
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
#define pr_fmt(fmt)	"[7808] %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_data/slimport_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/async.h>
#include <linux/slimport.h>
#include "slimport_tx_drv.h"

#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <mach/board_lge.h>
#include <mach/msm_smem.h>

#include <linux/qpnp-misc.h>
static uint8_t anx7808_chip_detect(void);


/* Enable or Disable HDCP by default */
/* hdcp_enable = 1: Enable,  0: Disable */
static int hdcp_enable;

/* HDCP switch for external block*/
/* external_block_en = 1: enable, 0: disable*/
int external_block_en;


/* to access global platform data */
static struct anx7808_platform_data *g_pdata;


struct i2c_client *anx7808_client;

struct anx7808_data {
	struct anx7808_platform_data    *pdata;
	struct delayed_work    work;
	struct workqueue_struct    *workqueue;
	struct mutex    lock;
	struct wake_lock slimport_lock;
};

struct msm_hdmi_slimport_ops *hdmi_slimport_ops;


bool slimport_is_connected(void)
{
	struct anx7808_platform_data *pdata = NULL;
	bool result = false;

	if (!anx7808_client)
		return false;

#ifdef CONFIG_OF
	pdata = g_pdata;
#else
	pdata = anx7808_client->dev.platform_data;
#endif

	if (!pdata)
		return false;

	/* spin_lock(&pdata->lock); */

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			pr_info("Slimport Dongle is detected\n");
			result = true;
		}
	}
	/* spin_unlock(&pdata->lock); */

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);


/*sysfs interface : Enable or Disable HDCP by default*/
static ssize_t sp_hdcp_feature_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", hdcp_enable);
}

static ssize_t sp_hdcp_feature_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	hdcp_enable = val;
	pr_info(" hdcp_enable = %d\n", hdcp_enable);
	return count;
}

/*sysfs  interface : HDCP switch for external block*/
static ssize_t sp_external_block_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", external_block_en);
}

static ssize_t sp_external_block_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	external_block_en = val;
	return count;
}

/*sysfs  interface : i2c_master_read_reg, i2c_master_write_reg
anx7730 addr id:: DP_rx(0x50:0, 0x8c:1) HDMI_tx(0x72:5, 0x7a:6, 0x70:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static ssize_t anx7730_write_reg_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;

	if (sp_tx_system_state != STATE_PLAY_BACK) {
		pr_err("error!, Not STATE_PLAY_BACK\n");
		return -EINVAL;
	}

	if (sp_tx_rx_type != RX_HDMI) {
		pr_err("error!, rx is not anx7730\n");
		return -EINVAL;
	}

	if (count != 7 && count != 5) {
		pr_err("cnt:%d, invalid input!\n", count-1);
		pr_err("ex) 05df   -> op:0(read)  id:5 reg:0xdf \n");
		pr_err("ex) 15df5f -> op:1(wirte) id:5 reg:0xdf val:0x5f\n");
		return -EINVAL;
	}

	ret = snprintf(&op, 2, buf);
	ret = snprintf(&i, 2, buf+1);
	ret = snprintf(r, 3, buf+2);

	id = simple_strtoul(&i, NULL, 10);
	reg = simple_strtoul(r, NULL, 16);

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7)) {
		pr_err("invalid addr id! (id:0,1,5,6,7)\n");
		return -EINVAL;
	}

	switch (op) {

	case 0x30: /* "0" -> read */
		i2c_master_read_reg(id, reg, &tmp);
		pr_info("anx7730 read(%d,0x%x)= 0x%x \n", id, reg, tmp);
		break;

	case 0x31: /* "1" -> write */
		ret = snprintf(v, 3, buf+4);
		val = simple_strtoul(v, NULL, 16);

		i2c_master_write_reg(id, reg, val);
		i2c_master_read_reg(id, reg, &tmp);
		pr_info("anx7730 write(%d,0x%x,0x%x)\n", id, reg, tmp);
		break;

	default:
		pr_err("invalid operation code! (0:read, 1:write)\n");
		return -EINVAL;
	}

	return count;
}

/*sysfs  interface : sp_read_reg, sp_write_reg
anx7808 addr id:: HDMI_rx(0x7e:0, 0x80:1) DP_tx(0x72:5, 0x7a:6, 0x78:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static int anx7808_id_change(int id)
{
	int chg_id = 0;

	switch (id) {
	case 0:
		chg_id = 0x7e; /* RX_P0 */
		break;
	case 1:
		chg_id = 0x80; /* RX_P1 */
		break;
	case 5:
		chg_id = 0x72; /* TX_P2 */
		break;
	case 6:
		chg_id = 0x7a; /* TX_P1 */
		break;
	case 7:
		chg_id = 0x78; /* TX_P0 */
		break;
	}
	return chg_id;
}

static ssize_t anx7808_write_reg_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;

	if (sp_tx_system_state != STATE_PLAY_BACK) {
		pr_err("error!, Not STATE_PLAY_BACK\n");
		return -EINVAL;
	}

	if (count != 7 && count != 5) {
		pr_err("cnt:%d, invalid input!\n", count-1);
		pr_err("ex) 05df   -> op:0(read)  id:5 reg:0xdf \n");
		pr_err("ex) 15df5f -> op:1(wirte) id:5 reg:0xdf val:0x5f\n");
		return -EINVAL;
	}

	ret = snprintf(&op, 2, buf);
	ret = snprintf(&i, 2, buf+1);
	ret = snprintf(r, 3, buf+2);

	id = simple_strtoul(&i, NULL, 10);
	reg = simple_strtoul(r, NULL, 16);

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7)) {
		pr_err("invalid addr id! (id:0,1,5,6,7)\n");
		return -EINVAL;
	}

	id = anx7808_id_change(id); /* ex) 5 -> 0x72 */

	switch (op) {
	case 0x30: /* "0" -> read */
		sp_read_reg(id, reg, &tmp);
		pr_info("anx7808 read(0x%x,0x%x)= 0x%x \n", id, reg, tmp);
		break;

	case 0x31: /* "1" -> write */
		ret = snprintf(v, 3, buf+4);
		val = simple_strtoul(v, NULL, 16);

		sp_write_reg(id, reg, val);
		sp_read_reg(id, reg, &tmp);
		pr_info("anx7808 write(0x%x,0x%x,0x%x)\n", id, reg, tmp);
		break;

	default:
		pr_err("invalid operation code! (0:read, 1:write)\n");
		return -EINVAL;
	}

	return count;
}


/* for debugging */
static struct device_attribute slimport_device_attrs[] = {
	__ATTR(hdcp, S_IRUGO | S_IWUSR, sp_hdcp_feature_show, sp_hdcp_feature_store),
	__ATTR(hdcp_switch, S_IRUGO | S_IWUSR, sp_external_block_show, sp_external_block_store),
	__ATTR(anx7730, S_IRUGO | S_IWUSR, NULL, anx7730_write_reg_store),
	__ATTR(anx7808, S_IRUGO | S_IWUSR, NULL, anx7808_write_reg_store),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		if (device_create_file(dev, &slimport_device_attrs[i]))
			goto error;
	return 0;
error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, &slimport_device_attrs[i]);
	pr_err("Unable to create interface");
	return -EINVAL;
}


int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7808_client, offset);
	if (ret < 0) {
		pr_err("failed to read i2c addr=%x\n", slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(anx7808_client, offset, value);
	if (ret < 0) {
		pr_err("failed to write i2c addr=%x\n", slave_addr);
	}
	return ret;
}

void sp_tx_hardware_poweron(void)
{

	struct anx7808_platform_data *pdata = g_pdata;

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(10);
	gpio_set_value(pdata->gpio_p_dwn, 0);
	msleep(20);
	/* Enable 1.0V LDO */
	gpio_set_value(pdata->gpio_v10_ctrl, 1);
	msleep(30);
	gpio_set_value(pdata->gpio_reset, 1);

	pr_info("anx7808 power on\n");
}

void sp_tx_hardware_powerdown(void)
{

	struct anx7808_platform_data *pdata = g_pdata;

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(5);
	gpio_set_value(pdata->gpio_v10_ctrl, 0);
	msleep(10);
	gpio_set_value(pdata->gpio_p_dwn, 1);

	pr_info("anx7808 power down\n");
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bedid_firstblock, sizeof(bedid_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bedid_extblock, sizeof(bedid_extblock));
	} else {
		pr_err("block number %d is invalid\n", block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

static void slimport_cable_plug_proc(struct anx7808_data *anx7808)
{
	struct anx7808_platform_data *pdata = anx7808->pdata;

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(100);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			if (sp_tx_pd_mode) {
				sp_tx_pd_mode = 0;
				sp_tx_hardware_poweron();
				sp_tx_power_on(SP_TX_PWR_REG);
				sp_tx_power_on(SP_TX_PWR_TOTAL);
				hdmi_rx_initialization();
				sp_tx_initialization();
				sp_tx_vbus_poweron();
				msleep(200);
				if (!sp_tx_get_cable_type()) {
					pr_err("AUX ERR!\n");
					sp_tx_vbus_powerdown();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_rx_type = RX_NULL;
					sp_tx_rx_type_backup = RX_NULL;
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
					return;
				}
				sp_tx_rx_type_backup = sp_tx_rx_type;
			}
			switch (sp_tx_rx_type) {
			case RX_HDMI:
				if (sp_tx_get_hdmi_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_DP:
				if (sp_tx_get_dp_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_GEN:
				if (sp_tx_get_vga_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_9832:
				if (sp_tx_get_vga_connection()) {
					sp_tx_send_message(MSG_CLEAR_IRQ);
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				}
				break;
			case RX_NULL:
			default:
				break;
			}
		}
	} else if (sp_tx_pd_mode == 0) {
		sp_tx_vbus_powerdown();
		sp_tx_power_down(SP_TX_PWR_REG);
		sp_tx_power_down(SP_TX_PWR_TOTAL);
		sp_tx_hardware_powerdown();
		sp_tx_pd_mode = 1;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_rx_type = RX_NULL;
		sp_tx_rx_type_backup = RX_NULL;
		sp_tx_set_sys_state(STATE_CABLE_PLUG);
	}
}

static void slimport_edid_proc(void)
{
	sp_tx_edid_read();

	if (bedid_break)
		pr_err("EDID corruption!\n");
	hdmi_rx_set_hpd(1);
	hdmi_rx_set_termination(1);
	sp_tx_set_sys_state(STATE_CONFIG_HDMI);
}

static void slimport_config_output(void)
{
	sp_tx_clean_hdcp();
	sp_tx_set_colorspace();
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
	sp_tx_enable_video_input(1);
	sp_tx_set_sys_state(STATE_HDCP_AUTH);
}

static void slimport_playback_proc(void)
{
	if ((sp_tx_rx_type == RX_VGA_9832)
		|| (sp_tx_rx_type == RX_VGA_GEN)) {
		if ((sp_tx_hw_hdcp_en == 0) && (external_block_en == 1)) {
			sp_tx_video_mute(1);
			sp_tx_set_sys_state(STATE_HDCP_AUTH);
		} else if ((sp_tx_hw_hdcp_en == 1) && (external_block_en == 0))
			sp_tx_disable_slimport_hdcp();
	}
	if (external_block_en == 1)
		sp_tx_video_mute(1);
	else
		sp_tx_video_mute(0);
}

static void slimport_main_proc(struct anx7808_data *anx7808)
{
	mutex_lock(&anx7808->lock);

	if (!sp_tx_pd_mode) {
		sp_tx_int_irq_handler();
		hdmi_rx_int_irq_handler();
	}

	if (sp_tx_system_state == STATE_CABLE_PLUG)
		slimport_cable_plug_proc(anx7808);

	if (sp_tx_system_state == STATE_PARSE_EDID)
		slimport_edid_proc();

	if (sp_tx_system_state == STATE_CONFIG_HDMI)
		sp_tx_config_hdmi_input();

	if (sp_tx_system_state == STATE_LINK_TRAINING) {
		if (!sp_tx_lt_pre_config())
			sp_tx_hw_link_training();
	}

	if (sp_tx_system_state == STATE_CONFIG_OUTPUT)
		slimport_config_output();

	if (sp_tx_system_state == STATE_HDCP_AUTH) {
		if (hdcp_enable) {
			sp_tx_hdcp_process();
		} else {
			sp_tx_power_down(SP_TX_PWR_HDCP);
			sp_tx_video_mute(0);
			sp_tx_show_infomation();
			sp_tx_set_sys_state(STATE_PLAY_BACK);
		}
	}

	if (sp_tx_system_state == STATE_PLAY_BACK)
		slimport_playback_proc();

	mutex_unlock(&anx7808->lock);
}

static uint8_t anx7808_chip_detect(void)
{
	return sp_tx_chip_located();
}

static void anx7808_chip_initial(void)
{
#ifdef EYE_TEST
	sp_tx_eye_diagram_test();
#else
	sp_tx_variable_init();
	sp_tx_vbus_powerdown();
	sp_tx_hardware_powerdown();
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
#endif
}

static void anx7808_free_gpio(struct anx7808_data *anx7808)
{
	gpio_free(anx7808->pdata->gpio_cbl_det);
#if 0
	gpio_free(anx7808->pdata->gpio_int);
#endif
	gpio_free(anx7808->pdata->gpio_reset);
	gpio_free(anx7808->pdata->gpio_p_dwn);
	gpio_free(anx7808->pdata->gpio_v10_ctrl);
	gpio_free(anx7808->pdata->gpio_v33_ctrl);

}
static int anx7808_init_gpio(struct anx7808_data *anx7808)
{
	int ret = 0;

	pr_info("anx7808 init gpio\n");

	ret = gpio_request(anx7808->pdata->gpio_p_dwn, "anx_p_dwn_ctl");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->pdata->gpio_p_dwn);
		goto out;
	}
	gpio_direction_output(anx7808->pdata->gpio_p_dwn, 1);

	ret = gpio_request(anx7808->pdata->gpio_reset, "anx7808_reset_n");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->pdata->gpio_reset);
		goto err0;
	}
	gpio_direction_output(anx7808->pdata->gpio_reset, 0);
#if 0
	ret = gpio_request(anx7808->pdata->gpio_int, "anx7808_int_n");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->pdata->gpio_int);
		goto err1;
	}
	gpio_direction_input(anx7808->pdata->gpio_int);
#endif
	ret = gpio_request(anx7808->pdata->gpio_cbl_det, "anx7808_cbl_det");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->pdata->gpio_cbl_det);
		goto err2;
	}
	gpio_direction_input(anx7808->pdata->gpio_cbl_det);


	ret = gpio_request(anx7808->pdata->gpio_v10_ctrl, "anx7808_v10_ctrl");
		if (ret) {
			pr_err("failed to request gpio %d\n", anx7808->pdata->gpio_v10_ctrl);
		goto err3;
	}
	gpio_direction_output(anx7808->pdata->gpio_v10_ctrl, 0);

	ret = gpio_request(anx7808->pdata->gpio_v33_ctrl, "anx7808_v33_ctrl");
		if (ret) {
			pr_err("failed to request gpio %d\n", anx7808->pdata->gpio_v33_ctrl);
		goto err4;
	}
	gpio_direction_output(anx7808->pdata->gpio_v33_ctrl, 0);

	gpio_set_value(anx7808->pdata->gpio_v10_ctrl, 0);
	/* need to be check below */
	gpio_set_value(anx7808->pdata->gpio_v33_ctrl, 1);

	gpio_set_value(anx7808->pdata->gpio_reset, 0);
	gpio_set_value(anx7808->pdata->gpio_p_dwn, 1);


	goto out;

err4:
	gpio_free(anx7808->pdata->gpio_v10_ctrl);

err3:
	gpio_free(anx7808->pdata->gpio_cbl_det);
err2:
#if 0
	gpio_free(anx7808->pdata->gpio_int);
err1:
#endif
	gpio_free(anx7808->pdata->gpio_reset);
err0:
	gpio_free(anx7808->pdata->gpio_p_dwn);
out:
	return ret;
}

static int anx7808_system_init(void)
{
	int ret = 0;

	ret = anx7808_chip_detect();
	if (ret == 0) {
		pr_err("failed to detect anx7808\n");
		return -ENODEV;
	}

	anx7808_chip_initial();
	return 0;
}

static irqreturn_t anx7808_cbl_det_isr(int irq, void *data)
{
	struct anx7808_data *anx7808 = data;

	if (gpio_get_value(anx7808->pdata->gpio_cbl_det)) {
		wake_lock(&anx7808->slimport_lock);
		pr_info("detect cable insertion\n");
		queue_delayed_work(anx7808->workqueue, &anx7808->work, 0);
	} else {
		pr_info("detect cable removal\n");
		cancel_delayed_work_sync(&anx7808->work);
		wake_unlock(&anx7808->slimport_lock);
		wake_lock_timeout(&anx7808->slimport_lock, 2*HZ);
	}
	return IRQ_HANDLED;
}

static void anx7808_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7808_data *td = container_of(work, struct anx7808_data,
								work.work);
	slimport_main_proc(td);
	queue_delayed_work(td->workqueue, &td->work,
			msecs_to_jiffies(300));
#endif
}


#ifdef CONFIG_OF
static int anx7808_parse_dt(struct device *dev,
		struct anx7808_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->gpio_p_dwn = of_get_named_gpio_flags(np,
							"analogix,p-dwn-gpio", 0, NULL);

	pdata->gpio_reset = of_get_named_gpio_flags(np,
							"analogix,reset-gpio", 0, NULL);

	pdata->gpio_int = of_get_named_gpio_flags(np,
							"analogix,irq-gpio", 0, NULL);

	pdata->gpio_cbl_det = of_get_named_gpio_flags(np,
							"analogix,cbl-det-gpio", 0, NULL);

	pr_info("gpio p_dwn : %d, reset : %d, irq : %d, cbl_det : %d\n",
			pdata->gpio_p_dwn, pdata->gpio_reset,
			pdata->gpio_int, pdata->gpio_cbl_det);

	pdata->gpio_v10_ctrl = of_get_named_gpio_flags(np,
							"analogix,v10-ctrl-gpio", 0, NULL);

	pdata->gpio_v33_ctrl = of_get_named_gpio_flags(np,
							"analogix,v33-ctrl-gpio", 0, NULL);

	pr_info("gpio v10_ctrl: %d v33_ctrl: %d\n",
			pdata->gpio_v10_ctrl, pdata->gpio_v33_ctrl);

	return 0;
}
#endif


static int anx7808_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct anx7808_data *anx7808;
	struct anx7808_platform_data *pdata;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("i2c bus does not support the anx7808\n");
		ret = -ENODEV;
		goto exit;
	}

	anx7808 = kzalloc(sizeof(struct anx7808_data), GFP_KERNEL);
	if (!anx7808) {
		pr_err("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							 sizeof(struct anx7808_platform_data),
							 GFP_KERNEL);
		if (!pdata) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
		client->dev.platform_data = pdata;
	/* device tree parsing function call */
		ret = anx7808_parse_dt(&client->dev, pdata);
		if (ret != 0) /* if occurs error */
			goto err0;

		anx7808->pdata = pdata;
	} else {
		anx7808->pdata = client->dev.platform_data;
	}

	/* to access global platform data */
	g_pdata = anx7808->pdata;

	anx7808_client = client;

	mutex_init(&anx7808->lock);

	if (!anx7808->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = anx7808_init_gpio(anx7808);
	if (ret) {
		pr_err("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7808->work, anx7808_work_func);

	anx7808->workqueue = create_singlethread_workqueue("anx7808_work");
	if (anx7808->workqueue == NULL) {
		pr_err("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}

	ret = anx7808_system_init();
	if (ret) {
		pr_err("failed to initialize anx7808\n");
		goto err2;
	}

	client->irq = gpio_to_irq(anx7808->pdata->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("failed to get gpio irq\n");
		goto err2;
	}

	wake_lock_init(&anx7808->slimport_lock,	WAKE_LOCK_SUSPEND,
				"slimport_wake_lock");

	ret = request_threaded_irq(client->irq, NULL, anx7808_cbl_det_isr,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"anx7808", anx7808);
	if (ret  < 0) {
		pr_err("failed to request irq\n");
		goto err2;
	}

	ret = irq_set_irq_wake(client->irq, 1);
	if (ret  < 0) {
		pr_err("cable detect irq wake set fail\n");
		goto err3;
	}

	ret = enable_irq_wake(client->irq);
	if (ret  < 0) {
		pr_err("cable detect irq wake enable fail\n");
		goto err3;
	}


	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		pr_err("sysfs register failed");
		goto err3;
	}

	goto exit;

err3:
	free_irq(client->irq, anx7808);
err2:
	destroy_workqueue(anx7808->workqueue);
err1:
	anx7808_free_gpio(anx7808);
err0:
	anx7808_client = NULL;
	kfree(anx7808);
exit:
	return ret;
}

static int anx7808_i2c_remove(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);
	int i = 0;


	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		device_remove_file(&client->dev, &slimport_device_attrs[i]);
	free_irq(client->irq, anx7808);
	anx7808_free_gpio(anx7808);
	destroy_workqueue(anx7808->workqueue);
	wake_lock_destroy(&anx7808->slimport_lock);
	kfree(anx7808);
	return 0;
}

bool is_slimport_vga(void)
{
	return ((sp_tx_rx_type == RX_VGA_9832)
		|| (sp_tx_rx_type == RX_VGA_GEN)) ? TRUE : FALSE;
}
/* 0x01: hdmi device is attached
    0x02: DP device is attached
    0x03: Old VGA device is attached // RX_VGA_9832
    0x04: new combo VGA device is attached // RX_VGA_GEN
    0x00: unknow device            */
EXPORT_SYMBOL(is_slimport_vga);
bool is_slimport_dp(void)
{
	return (sp_tx_rx_type == RX_DP) ? TRUE : FALSE;
}
EXPORT_SYMBOL(is_slimport_dp);
unchar sp_get_link_bw(void)
{
	return slimport_link_bw;
}
EXPORT_SYMBOL(sp_get_link_bw);
void sp_set_link_bw(unchar link_bw)
{
	slimport_link_bw = link_bw;
}


static int anx7808_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	return 0;
}


static int anx7808_i2c_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id anx7808_id[] = {
	{ "anx7808", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7808_id);

#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
    { .compatible = "analogix,anx7808",},
    { },
};
#endif

static struct i2c_driver anx7808_driver = {
	.driver  = {
		.name  = "anx7808",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif
	},
	.probe  = anx7808_i2c_probe,
	.remove  = anx7808_i2c_remove,
	.suspend = anx7808_i2c_suspend,
	.resume = anx7808_i2c_resume,
	.id_table  = anx7808_id,
};

static void __init anx7808_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7808_driver);
	if (ret < 0)
		pr_err("failed to register anx7808 i2c driver\n");
}

static int __init anx7808_init(void)
{
	async_schedule(anx7808_init_async, NULL);
	return 0;
}

static void __exit anx7808_exit(void)
{
	i2c_del_driver(&anx7808_driver);
}

module_init(anx7808_init);
module_exit(anx7808_exit);

MODULE_DESCRIPTION("Slimport  transmitter ANX7808 driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.4");
