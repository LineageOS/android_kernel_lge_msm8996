/*
 * Texas Instruments TUSB422 Power Delivery
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *         Brian Quach <brian.quach@ti.com>
 *
 * Copyright: (C) 2016 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "tusb422_linux.h"
#include "tcpci.h"
#include "tcpm.h"
#include "tusb422.h"
#include "tusb422_common.h"
#ifdef CONFIG_TUSB422_PAL
	#include "usb_pd_pal.h"
#endif
#include "usb_pd_policy_engine.h"

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#ifdef CONFIG_WAKELOCK
	#include <linux/wakelock.h>
#endif
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	#include "tusb422_linux_dual_role.h"
	#include <linux/usb/class-dual-role.h>
#endif

//#define TUSB422_USE_POLLING

/* Remove the following line to disable sysfs debug */
#define TUSB422_DEBUG
#define TUSB422_EXTRA_DEBUG

struct tusb422_pwr_delivery {
	struct device *dev;
	struct regmap *regmap;
	struct i2c_client *client;
	struct src_pdo_t *source_pwr;
	struct snk_pdo_t *sink_pwr;
	struct gpio_desc *alert_gpio;
	struct gpio_desc *vbus_src_gpio;
	struct gpio_desc *vbus_snk_gpio;
	struct gpio_desc *vconn_gpio;
	struct gpio_desc *vbus_hv_gpio;
	struct gpio_desc *vbus_5v_gpio;
	struct hrtimer timer;
	bool timer_expired;
	struct work_struct work;
#ifdef CONFIG_WAKELOCK
	struct wake_lock attach_wakelock;
	struct wake_lock detach_wakelock;
#endif
	void (*callback) (unsigned int);
	tcpc_config_t *configuration;
	usb_pd_port_config_t *port_config;
	int alert_irq;
#ifdef CONFIG_LGE_USB_TYPE_C
	struct workqueue_struct *wq;
#endif
};

static struct tusb422_pwr_delivery *tusb422_pd;


#ifdef TUSB422_DEBUG
/* Device registers can be dumped via:
 *   cat /sys/bus/i2c/devices/1-0020/registers
 * Individual registers can be written using echo:
 *   echo "ROLE_CTRL 0x0F" > /sys/bus/i2c/devices/1-0020/registers
 */
struct tusb422_reg {
	const char *name;
	const uint8_t reg;
};

static const struct tusb422_reg tusb422_regs[] = {
	{ "VENDOR_ID_0", TCPC_REG_VENDOR_ID},
	{ "VENDOR_ID_1", TCPC_REG_VENDOR_ID + 1},
	{ "PRODUCT_ID_0", TCPC_REG_PRODUCT_ID},
	{ "PRODUCT_ID_1", TCPC_REG_PRODUCT_ID + 1},
	{ "DEVICE_ID_0", TCPC_REG_DEVICE_ID},
	{ "DEVICE_ID_1", TCPC_REG_DEVICE_ID + 1},
	{ "TYPEC_REV_0",  TCPC_REG_USB_TYPEC_REV},
	{ "TYPEC_REV_1", TCPC_REG_USB_TYPEC_REV + 1},
	{ "USBPD_REV_0", TCPC_REG_PD_REV_VER},
	{ "USBPD_REV_1", TCPC_REG_PD_REV_VER + 1},
	{ "TCPC_REV_0", TCPC_REG_PD_INTERFACE_REV},
	{ "TCPC_REV_1", TCPC_REG_PD_INTERFACE_REV + 1},
	{ "ALERT_0", TCPC_REG_ALERT},
	{ "ALERT_1", TCPC_REG_ALERT + 1},
	{ "ALERT_MASK_0", TCPC_REG_ALERT_MASK},
	{ "ALERT_MASK_1", TCPC_REG_ALERT_MASK + 1},
	{ "POWER_STATUS_MASK", TCPC_REG_POWER_STATUS_MASK},
	{ "FAULT_STATUS_MASK", TCPC_REG_FAULT_STATUS_MASK},
	/*{ "CFG_STD_OUTPUT", TCPC_REG_CONFIG_STD_OUTPUT },*/
	{ "TCPC_CTRL", TCPC_REG_TCPC_CTRL},
	{ "ROLE_CTRL", TCPC_REG_ROLE_CTRL},
	{ "FAULT_CTRL", TCPC_REG_FAULT_CTRL},
	{ "POWER_CTRL", TCPC_REG_POWER_CTRL},
	{ "CC_STATUS", TCPC_REG_CC_STATUS},
	{ "POWER_STATUS", TCPC_REG_POWER_STATUS},
	{ "FAULT_STATUS", TCPC_REG_FAULT_STATUS},
	{ "MSG_HDR_INFO", TCPC_REG_MSG_HDR_INFO},
	{ "RX_DETECT", TCPC_REG_RX_DETECT},
	{ "VBUS_VOLTAGE_0", TCPC_REG_VBUS_VOLTAGE},
	{ "VBUS_VOLTAGE_1", TCPC_REG_VBUS_VOLTAGE + 1},
	{ "SNK_DISCON_THRES_0", TCPC_REG_VBUS_SINK_DISCONNECT_THRESH},
	{ "SNK_DISCON_THRES_1", TCPC_REG_VBUS_SINK_DISCONNECT_THRESH + 1},
	{ "STOP_DISCHRG_THRES_0", TCPC_REG_VBUS_STOP_DISCHARGE_THRESH},
	{ "STOP_DISCHRG_THRES_1", TCPC_REG_VBUS_STOP_DISCHARGE_THRESH + 1},
	{ "VBUS_ALARM_HI_0", TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG},
	{ "VBUS_ALARM_HI_1", TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG + 1},
	{ "VBUS_ALARM_LO_0", TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG},
	{ "VBUS_ALARM_LO_1", TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG + 1},
	{ "TI_INT_STATUS", TUSB422_REG_INT_STATUS},
	{ "TI_INT_STATUS_MASK", TUSB422_REG_INT_MASK},
	{ "TI_CC_GEN_CTRL", TUSB422_REG_CC_GEN_CTRL},
	{ "TI_BMC_TX_CTRL", TUSB422_REG_BMC_TX_CTRL},
	{ "TI_BMC_RX_CTRL", TUSB422_REG_BMC_RX_CTRL},
	{ "TI_BMC_RX_STATUS", TUSB422_REG_BMC_RX_STATUS}
};

static ssize_t tusb422_registers_show(struct device *dev,
									  struct device_attribute *attr,
									  char *buf)
{
	unsigned i, n, reg_count;
	unsigned int read_buf;
	unsigned int cc1, cc2;
	tcpc_device_t *tcpc_dev = tcpm_get_device(0);
	usb_pd_port_t *pd_dev = usb_pd_pe_get_device(0);

	reg_count = sizeof(tusb422_regs) / sizeof(tusb422_regs[0]);

	for (i = 0, n = 0; i < reg_count; i++) {
#ifdef CONFIG_REGMAP
		regmap_read(tusb422_pd->regmap, tusb422_regs[i].reg, &read_buf);
#else
		tusb422_read(tusb422_regs[i].reg, &read_buf, 1);
#endif
		n += scnprintf(buf + n, PAGE_SIZE - n,
					   "%-20s = 0x%02X\n",
					   tusb422_regs[i].name,
					   read_buf);
	}

	n += scnprintf(buf + n, PAGE_SIZE - n,
				   "----------------------------------------------\n");

	n += scnprintf(buf + n, PAGE_SIZE - n,
				   "Type-C State = %s\n",
				   tcstate2string[tcpc_dev->state]);

	if (*pd_dev->state < PE_NUM_STATES) {
		n += scnprintf(buf + n, PAGE_SIZE - n,
					   "USB PD State = %s\n",
					   pdstate2string[*pd_dev->current_state]);
	}

	cc1 = TCPC_CC1_STATE(tcpc_dev->cc_status);
	cc2 = TCPC_CC2_STATE(tcpc_dev->cc_status);

	if (tcpc_dev->cc_status & CC_STATUS_CONNECT_RESULT) {
		/* TCPC presenting Rd */
		n += scnprintf(buf + n, PAGE_SIZE - n,
					   "CC1 State = %s\n",
					   (cc1 == CC_SNK_STATE_DEFAULT) ? "500/900 mA" :
					   (cc1 == CC_SNK_STATE_POWER15) ? "1.5A" :
					   (cc1 == CC_SNK_STATE_POWER30) ? "3.0A" : "Open");

		n += scnprintf(buf + n, PAGE_SIZE - n,
					   "CC2 State = %s\n",
					   (cc2 == CC_SNK_STATE_DEFAULT) ? "500/900 mA" :
					   (cc2 == CC_SNK_STATE_POWER15) ? "1.5A" :
					   (cc2 == CC_SNK_STATE_POWER30) ? "3.0A" : "Open");
	}
	else {
		/* TCPC presenting Rp */

		n += scnprintf(buf + n, PAGE_SIZE - n,
					   "CC1 State = %s\n",
					   (cc1 == CC_SRC_STATE_OPEN) ? "Open" :
					   (cc1 == CC_SRC_STATE_RA) ? "Ra" :
					   (cc1 == CC_SRC_STATE_RD) ? "Rd" : "?");

		n += scnprintf(buf + n, PAGE_SIZE - n,
					   "CC2 State = %s\n",
					   (cc2 == CC_SRC_STATE_OPEN) ? "Open" :
					   (cc2 == CC_SRC_STATE_RA) ? "Ra" :
					   (cc2 == CC_SRC_STATE_RD) ? "Rd" : "?");
	}

	n += scnprintf(buf + n, PAGE_SIZE - n,
				   "----------------------------------------------\n");

#ifdef TUSB422_EXTRA_DEBUG
	n += scnprintf(buf + n, PAGE_SIZE - n, "Type-C Last State = %s\n",
				   tcstate2string[tcpc_dev->last_state]);
	n += scnprintf(buf + n, PAGE_SIZE - n, "Rx Buf Overflow Cnt = %u\n",
				   tcpc_dev->rx_buff_overflow_cnt);
	n += scnprintf(buf + n, PAGE_SIZE - n, "VCONN OCP Cnt = %u\n",
				   tcpc_dev->vconn_ocp_cnt);

	n += scnprintf(buf + n, PAGE_SIZE - n, "\nPolicy Engine State History:\n");

	for (i = 0; i < PD_STATE_HISTORY_LEN; i++) {
		if (pd_dev->state[i] < PE_NUM_STATES) {
			n += scnprintf(buf + n, PAGE_SIZE - n,
						   "%s[%u] %s\n",
						   (&pd_dev->state[i] == pd_dev->current_state) ? "->" : "  ",
						   i, pdstate2string[pd_dev->state[i]]);
		}
	}

	n += scnprintf(buf + n, PAGE_SIZE - n,
				   "----------------------------------------------\n");
#endif

	return n;
}

static ssize_t tusb422_registers_store(struct device *dev,
									   struct device_attribute *attr,
									   const char *buf, size_t count)
{
	unsigned i, reg_count, value;
	int error = 0;
	char name[30];

	if (count >= 30) {
		pr_err("%s: input too long\n", __func__);
		return -1;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s: unable to parse input\n", __func__);
		return -1;
	}

	if (!strcmp(name, "TEST_ROLE"))
	{
		if (value < 3)
		{
			tcpm_change_role(0, value);
		}
		else if (value == 3)
		{
			tcpm_try_role_swap(0);
		}

		return count;
	}

	reg_count = sizeof(tusb422_regs) / sizeof(tusb422_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, tusb422_regs[i].name)) {
#ifdef CONFIG_REGMAP
			error = regmap_write(tusb422_pd->regmap, tusb422_regs[i].reg, value);
#else
			error = tusb422_write(tusb422_regs[i].reg, &value, 1);
#endif
			if (error) {
				pr_err("%s: failed to write %s\n",
					   __func__, name);
				return -1;
			}
			return count;
		}
	}

	pr_err("%s: no such register %s\n", __func__, name);
	return -1;
}

static DEVICE_ATTR(registers, S_IWUSR | S_IRUGO,
				   tusb422_registers_show, tusb422_registers_store);

static struct attribute *tusb422_attrs[] = {
	&dev_attr_registers.attr,
	NULL
};

static const struct attribute_group tusb422_attr_group = {
	.attrs = tusb422_attrs,
};
#endif

#ifdef CONFIG_REGMAP

int tusb422_write(int reg, const void *data, int len)
{
	int ret;

	if (len == 1)
		ret = regmap_write(tusb422_pd->regmap, reg, *(unsigned int *)data);
	else
		ret = regmap_raw_write(tusb422_pd->regmap, reg, data, len);

	return ret;
}

int tusb422_read(int reg, void *data, int len)
{
	return regmap_raw_read(tusb422_pd->regmap, reg, data, len);
}

int tusb422_modify_reg(int reg, int clr_mask, int set_mask)
{
	return regmap_update_bits(tusb422_pd->regmap, reg, (clr_mask | set_mask), set_mask);
}

#else

int tusb422_write(int reg, const void *data, int len)
{
	int ret;

	if (len == 1)
		ret = i2c_smbus_write_byte_data(tusb422_pd->client, reg, *(uint8_t *)data);
	else if (len == 2)
		ret = i2c_smbus_write_word_data(tusb422_pd->client, reg, *(uint16_t *)data);
	else
		ret = i2c_smbus_write_i2c_block_data(tusb422_pd->client, reg, len, data);

	return ret;
}

/* max read length is 32-bytes */
int tusb422_read(int reg, void *data, int len)
{
	int ret;

	if (len == 1) {
		ret = i2c_smbus_read_byte_data(tusb422_pd->client, reg);
		if (ret >= 0)
			*(uint8_t *)data = (uint8_t)ret;
	} else if (len == 2) {
		ret = i2c_smbus_read_word_data(tusb422_pd->client, reg);
		if (ret >= 0)
			*(uint16_t *)data = (uint16_t)ret;
	} else {
		ret = i2c_smbus_read_i2c_block_data(tusb422_pd->client, reg, len, data);
	}

	return ret;
}

int tusb422_modify_reg(int reg, int clr_mask, int set_mask)
{
	int ret;
	uint8_t val;
	uint8_t new_val;

	ret = i2c_smbus_read_byte_data(tusb422_pd->client, reg);
	if (ret >= 0) {
		val = (uint8_t)ret;

		new_val = val & ~clr_mask;
		new_val |= set_mask;

		if (new_val != val)
			ret = i2c_smbus_write_byte_data(tusb422_pd->client, reg, new_val);
	}

	return ret;
}

#endif

void tusb422_set_timer_func(void (*function)(unsigned int))
{
	tusb422_pd->callback = function;
}

void tusb422_clr_timer_func(void)
{
	tusb422_pd->callback = NULL;
}

int tusb422_stop_timer(void)
{
	hrtimer_cancel(&tusb422_pd->timer);
	tusb422_pd->timer_expired = false;

	return 0;
}

int tusb422_start_timer(unsigned int timeout_ms)
{
	tusb422_stop_timer();

	hrtimer_start(&tusb422_pd->timer, ms_to_ktime(timeout_ms), HRTIMER_MODE_REL);

	return 0;
}

void tusb422_msleep(int msecs)
{
	/* Use udelay for short sleeps < 20ms */
	udelay(msecs * 1000);
}

static inline void tusb422_schedule_work(struct work_struct *work)
{
#ifdef CONFIG_LGE_USB_TYPE_C
	struct tusb422_pwr_delivery *tusb422_pwr = container_of(work,
		struct tusb422_pwr_delivery, work);

	queue_work_on(0, tusb422_pwr->wq, work);
#else
	queue_work(system_highpri_wq, work);
#endif
}

#ifdef CONFIG_WAKELOCK
void tusb422_wake_lock_attach(void)
{
	wake_unlock(&tusb422_pd->detach_wakelock);

#ifndef CONFIG_LGE_USB_TYPE_C
	if (!wake_lock_active(&tusb422_pd->attach_wakelock))
		wake_lock(&tusb422_pd->attach_wakelock);
#endif

	return;
}

#define WAKE_LOCK_TIMEOUT_MS  5000

void tusb422_wake_lock_detach(void)
{
	wake_lock_timeout(&tusb422_pd->detach_wakelock,
					  msecs_to_jiffies(WAKE_LOCK_TIMEOUT_MS));

#ifndef CONFIG_LGE_USB_TYPE_C
	wake_unlock(&tusb422_pd->attach_wakelock);
#endif

	return;
}
#endif

int tusb422_set_vbus(int vbus_sel)
{
	if (vbus_sel == VBUS_SRC_5V) {
		/* Disable high voltage. */
		gpiod_direction_output(tusb422_pd->vbus_hv_gpio, 0);
		/* Enable 5V. */
		gpiod_direction_output(tusb422_pd->vbus_5v_gpio, 1);
		/* Enable SRC switch. */
		gpiod_direction_output(tusb422_pd->vbus_src_gpio, 0);
	}
	else if (vbus_sel == VBUS_SRC_HI_VOLT) {
		/* Disable 5v */
		gpiod_direction_output(tusb422_pd->vbus_5v_gpio, 0);
		/* Enable high voltage. */
		gpiod_direction_output(tusb422_pd->vbus_hv_gpio, 1);
		/* Enable SRC switch. */
		gpiod_direction_output(tusb422_pd->vbus_src_gpio, 0);
	}
	else if (vbus_sel == VBUS_SNK) {
		/* Enable SNK switch. */
		gpiod_direction_output(tusb422_pd->vbus_snk_gpio, 0);
	}

	return 0;
}

int tusb422_clr_vbus(int vbus_sel)
{
	if (vbus_sel == VBUS_SRC_5V) {
		/* Disable SRC switch. */
		gpiod_direction_output(tusb422_pd->vbus_src_gpio, 1);
		/* Disable 5V. */
		gpiod_direction_output(tusb422_pd->vbus_5v_gpio, 0);
	}
	else if (vbus_sel == VBUS_SRC_HI_VOLT) {
		/* Disable high voltage. */
		gpiod_direction_output(tusb422_pd->vbus_hv_gpio, 0);
	}
	else if (vbus_sel == VBUS_SNK) {
		/* Disable SNK switch. */
		gpiod_direction_output(tusb422_pd->vbus_snk_gpio, 1);
	}

	return 0;
}

#ifdef CONFIG_LGE_USB_TYPE_C
int tusb422_set_vconn_enable(int enable)
{
	gpiod_direction_output(tusb422_pd->vconn_gpio, enable);
	return 0;
}
#endif

static irqreturn_t tusb422_event_handler(int irq, void *data)
{
	struct tusb422_pwr_delivery *tusb422_pwr = data;

#if defined(CONFIG_WAKELOCK) && defined(CONFIG_LGE_USB_TYPE_C)
	if (!tcpm_is_cc_fault(0)) {
		wake_lock_timeout(&tusb422_pd->attach_wakelock,
				  msecs_to_jiffies(WAKE_LOCK_TIMEOUT_MS));
	}
#endif

	tusb422_schedule_work(&tusb422_pwr->work);
	return IRQ_HANDLED;
}

static int tusb422_of_get_gpios(struct tusb422_pwr_delivery *tusb422_pd)
{
	tusb422_pd->alert_gpio = devm_gpiod_get(tusb422_pd->dev, "ti,alert",
											GPIOD_IN);
	if (IS_ERR(tusb422_pd->alert_gpio)) {
		dev_err(tusb422_pd->dev, "failed to allocate alert gpio\n");
		return PTR_ERR(tusb422_pd->alert_gpio);
	}

	tusb422_pd->alert_irq = gpiod_to_irq(tusb422_pd->alert_gpio);

	tusb422_pd->vbus_snk_gpio = devm_gpiod_get(tusb422_pd->dev, "ti,vbus-snk",
											   GPIOD_OUT_HIGH);
	if (IS_ERR(tusb422_pd->vbus_snk_gpio))
		tusb422_pd->vbus_snk_gpio = NULL;

	tusb422_pd->vbus_src_gpio = devm_gpiod_get(tusb422_pd->dev, "ti,vbus-src",
											   GPIOD_OUT_HIGH);
	if (IS_ERR(tusb422_pd->vbus_src_gpio))
		tusb422_pd->vbus_src_gpio = NULL;

#ifdef CONFIG_LGE_USB_TYPE_C
	tusb422_pd->vconn_gpio = devm_gpiod_get(tusb422_pd->dev,
						"ti,vconn-en",
						GPIOD_OUT_LOW);
#else
	tusb422_pd->vconn_gpio = devm_gpiod_get(tusb422_pd->dev, "ti,vconn-en",
											GPIOD_OUT_HIGH);
#endif
	if (IS_ERR(tusb422_pd->vconn_gpio))
		tusb422_pd->vconn_gpio = NULL;

	tusb422_pd->vbus_hv_gpio = devm_gpiod_get(tusb422_pd->dev, "ti,vbus-hv",
											  GPIOD_OUT_LOW);
	if (IS_ERR(tusb422_pd->vbus_hv_gpio))
		tusb422_pd->vbus_hv_gpio = NULL;

	tusb422_pd->vbus_5v_gpio = devm_gpiod_get(tusb422_pd->dev, "ti,vbus-5v",
											  GPIOD_OUT_LOW);
	if (IS_ERR(tusb422_pd->vbus_5v_gpio))
		tusb422_pd->vbus_5v_gpio = NULL;

	return 0;
}

static int tusb422_pd_init(struct tusb422_pwr_delivery *tusb422_pd)
{
	struct device_node *of_node = tusb422_pd->dev->of_node;
	struct device_node *pp;
	unsigned int supply_type;
	unsigned int min_volt, current_flow, peak_current, pdo;
	unsigned int max_volt, max_current, max_power, fast_role_swap_support;
	unsigned int op_current, min_current, op_power, min_power, pdo_priority;
	int ret;
	int num_of_sink = 0, num_of_src = 0;
	struct device *dev = tusb422_pd->dev;

	tusb422_pd->port_config = devm_kzalloc(dev, sizeof(*tusb422_pd->port_config), GFP_KERNEL);
	if (!tusb422_pd->port_config)
		return -ENOMEM;

	if (of_property_read_bool(of_node, "ti,usb-comm-capable"))
		tusb422_pd->port_config->usb_comm_capable = true;

	if (of_property_read_bool(of_node, "ti,usb-suspend-supported"))
		tusb422_pd->port_config->usb_suspend_supported = true;

	if (of_property_read_bool(of_node, "ti,externally-powered"))
		tusb422_pd->port_config->externally_powered = true;

	if (of_property_read_bool(of_node, "ti,dual-role-data"))
		tusb422_pd->port_config->dual_role_data = true;

	if (of_property_read_bool(of_node, "ti,unchunked-msg-support"))
		tusb422_pd->port_config->unchunked_msg_support = true;

	if (of_property_read_bool(of_node, "ti,higher-capability"))
		tusb422_pd->port_config->higher_capability = true;

	if (of_property_read_bool(of_node, "ti,giveback-flag"))
		tusb422_pd->port_config->giveback_flag = true;

	if (of_property_read_bool(of_node, "ti,no-usb-suspend"))
		tusb422_pd->port_config->no_usb_suspend = true;

	if (of_property_read_bool(of_node, "ti,auto-accept-swap-to-dfp"))
		tusb422_pd->port_config->auto_accept_swap_to_dfp = true;

	if (of_property_read_bool(of_node, "ti,auto-accept-swap-to-ufp"))
		tusb422_pd->port_config->auto_accept_swap_to_ufp = true;

	if (of_property_read_bool(of_node, "ti,auto-accept-swap-to-source"))
		tusb422_pd->port_config->auto_accept_swap_to_source = true;

	if (of_property_read_bool(of_node, "ti,auto-accept-swap-to-sink"))
		tusb422_pd->port_config->auto_accept_swap_to_sink = true;

	if (of_property_read_bool(of_node, "ti,auto-accept-vconn-swap"))
		tusb422_pd->port_config->auto_accept_vconn_swap = true;

	ret = of_property_read_u16(of_node, "ti,src-settling-time-ms",
							   &tusb422_pd->port_config->src_settling_time_ms);
	if (ret)
		pr_err("%s: Missing src-settling-time-ms\n", __func__);

	/* Mandate at least 50ms settling time */
	if (tusb422_pd->port_config->src_settling_time_ms < 50)
		tusb422_pd->port_config->src_settling_time_ms = 50;

	ret = of_property_read_u32(of_node, "ti,fast-role-swap-support",
							   &fast_role_swap_support);
	if (ret)
		pr_err("%s: Missing fast-role-swap-support\n", __func__);
	else
#if defined(CONFIG_LGE_USB_TYPE_C) && (PD_SPEC_REV == PD_REV30)
		tusb422_pd->port_config->fast_role_swap_support	= (fr_swap_current_t) fast_role_swap_support;
#else
		if (fast_role_swap_support)
			pr_err("%s: PD2.0 does not support fast-role-swap\n", __func__);
#endif

	ret = of_property_read_u32(of_node, "ti,pdo-priority", &pdo_priority);
	if (ret)
		pr_err("%s: Missing pdo-priority\n", __func__);
	else
		tusb422_pd->port_config->pdo_priority = (pdo_priority_t) pdo_priority;

	for_each_child_of_node(of_node, pp) {
		ret = of_property_read_u32(pp, "ti,current-flow", &current_flow);
		if (ret) {
			pr_err("%s: Missing current-flow\n", __func__);
			return ret;
		}

		ret = of_property_read_u32(pp, "ti,pdo-number", &pdo);
		if (ret) {
			pr_err("%s: Missing pdo-number\n", __func__);
			return ret;
		}

		ret = of_property_read_u32(pp, "ti,supply-type", &supply_type);
		if (ret) {
			pr_err("%s: Missing supply-type\n", __func__);
			return ret;
		}

		ret = of_property_read_u32(pp, "ti,min-voltage", &min_volt);
		if (ret) {
			pr_err("%s: Missing min-voltage\n", __func__);
			return ret;
		}

		switch (supply_type) {
			case SUPPLY_TYPE_BATTERY:
				ret = of_property_read_u32(pp, "ti,max-voltage", &max_volt);
				if (ret) {
					pr_err("%s: Missing max-voltage\n", __func__);
					return ret;
				}

				if (current_flow == 0) {
					num_of_src++;
					ret = of_property_read_u32(pp, "ti,max-power", &max_power);
					if (ret) {
						pr_err("%s: Missing max-power\n", __func__);
						return ret;
					}

					tusb422_pd->port_config->src_caps[pdo].SupplyType = supply_type;
					tusb422_pd->port_config->src_caps[pdo].MinV = PDO_VOLT(min_volt);
					tusb422_pd->port_config->src_caps[pdo].MaxV = PDO_VOLT(max_volt);
					tusb422_pd->port_config->src_caps[pdo].MaxPower = PDO_PWR(max_power);
				}
				else {
					num_of_sink++;

					ret = of_property_read_u32(pp, "ti,max-operating-pwr", &max_power);
					if (ret) {
						pr_err("%s: Missing max-operating-pwr\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,min-operating-pwr", &min_power);
					if (ret) {
						pr_err("%s: Missing min-operating-pwr\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,operational-pwr", &op_power);
					if (ret) {
						pr_err("%s: Missing operational-pwr\n", __func__);
						return ret;
					}

					tusb422_pd->port_config->snk_caps[pdo].SupplyType = supply_type;
					tusb422_pd->port_config->snk_caps[pdo].MinV = PDO_VOLT(min_volt);
					tusb422_pd->port_config->snk_caps[pdo].MaxV = PDO_VOLT(max_volt);
					tusb422_pd->port_config->snk_caps[pdo].MaxOperatingPower = PDO_PWR(max_power);
					tusb422_pd->port_config->snk_caps[pdo].MinOperatingPower = PDO_PWR(min_power);
					tusb422_pd->port_config->snk_caps[pdo].OperationalPower = PDO_PWR(op_power);
				}
				break;
			case SUPPLY_TYPE_FIXED:
				if (current_flow == 0) {
					num_of_src++;

					ret = of_property_read_u32(pp, "ti,peak-current", &peak_current);
					if (ret) {
						pr_err("%s: Missing peak-current\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,max-current", &max_current);
					if (ret) {
						pr_err("%s: Missing max-current\n", __func__);
						return ret;
					}

					tusb422_pd->port_config->src_caps[pdo].SupplyType = supply_type;
					tusb422_pd->port_config->src_caps[pdo].PeakI = peak_current;
					tusb422_pd->port_config->src_caps[pdo].MinV = PDO_VOLT(min_volt);
					tusb422_pd->port_config->src_caps[pdo].MaxI = PDO_CURR(max_current);
				}
				else {
					num_of_sink++;

					ret = of_property_read_u32(pp, "ti,max-operating-curr", &max_current);
					if (ret) {
						pr_err("%s: Missing max-operating-curr\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,min-operating-curr", &min_current);
					if (ret) {
						pr_err("%s: Missing min-operating-curr\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,operational-curr", &op_current);
					if (ret) {
						pr_err("%s: Missing operational-curr\n", __func__);
						return ret;
					}

					tusb422_pd->port_config->snk_caps[pdo].SupplyType = supply_type;
					tusb422_pd->port_config->snk_caps[pdo].MinV = PDO_VOLT(min_volt);
					tusb422_pd->port_config->snk_caps[pdo].MaxOperatingCurrent = PDO_CURR(max_current);
					tusb422_pd->port_config->snk_caps[pdo].MinOperatingCurrent = PDO_CURR(min_current);
					tusb422_pd->port_config->snk_caps[pdo].OperationalCurrent = PDO_CURR(op_current);
				}
				break;
			case SUPPLY_TYPE_VARIABLE:
				if (current_flow == 0) {
					num_of_src++;

					ret = of_property_read_u32(pp, "ti,peak-current", &peak_current);
					if (ret) {
						pr_err("%s: Missing peak-current\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,max-voltage", &max_volt);
					if (ret) {
						pr_err("%s: Missing max-voltage\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,peak-current", &peak_current);
					if (ret) {
						pr_err("%s: Missing peak-current\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,max-current", &max_current);
					if (ret) {
						pr_err("%s: Missing max-current\n", __func__);
						return ret;
					}

					tusb422_pd->port_config->src_caps[pdo].SupplyType = supply_type;
					tusb422_pd->port_config->src_caps[pdo].PeakI = peak_current;
					tusb422_pd->port_config->src_caps[pdo].MinV = PDO_VOLT(min_volt);
					tusb422_pd->port_config->src_caps[pdo].MaxV = PDO_VOLT(max_volt);
					tusb422_pd->port_config->src_caps[pdo].MaxI = PDO_CURR(max_current);
				}
				else {
					num_of_sink++;

					ret = of_property_read_u32(pp, "ti,max-voltage", &max_volt);
					if (ret) {
						pr_err("%s: Missing max-voltage\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,max-operating-curr", &max_current);
					if (ret) {
						pr_err("%s: Missing max-operating-curr\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,min-operating-curr", &min_current);
					if (ret) {
						pr_err("%s: Missing min-operating-curr\n", __func__);
						return ret;
					}

					ret = of_property_read_u32(pp, "ti,operational-curr", &op_current);
					if (ret) {
						pr_err("%s: Missing operational-curr\n", __func__);
						return ret;
					}

					tusb422_pd->port_config->snk_caps[pdo].SupplyType = supply_type;
					tusb422_pd->port_config->snk_caps[pdo].MinV = PDO_VOLT(min_volt);
					tusb422_pd->port_config->snk_caps[pdo].MaxV = PDO_VOLT(max_volt);
					tusb422_pd->port_config->snk_caps[pdo].MaxOperatingCurrent = PDO_CURR(max_current);
					tusb422_pd->port_config->snk_caps[pdo].MinOperatingCurrent = PDO_CURR(min_current);
					tusb422_pd->port_config->snk_caps[pdo].OperationalCurrent = PDO_CURR(op_current);
				}
				break;

			default:
				return -EINVAL;
				break;
		}
	}

	tusb422_pd->port_config->num_snk_pdos = num_of_sink;
	tusb422_pd->port_config->num_src_pdos = num_of_src;

	usb_pd_init(tusb422_pd->port_config);

	return 0;
}

static enum hrtimer_restart tusb422_timer_tasklet(struct hrtimer *hrtimer)
{
	struct tusb422_pwr_delivery *tusb422_pwr = container_of(hrtimer, struct tusb422_pwr_delivery, timer);

#if defined(CONFIG_WAKELOCK) && defined(CONFIG_LGE_USB_TYPE_C)
	if (!tcpm_is_cc_fault(0)) {
		wake_lock_timeout(&tusb422_pwr->attach_wakelock,
				  msecs_to_jiffies(WAKE_LOCK_TIMEOUT_MS));
	}
#endif

	tusb422_pwr->timer_expired = true;
	tusb422_schedule_work(&tusb422_pwr->work);

	return HRTIMER_NORESTART;
}

static void tusb422_work(struct work_struct *work)
{
	struct tusb422_pwr_delivery *tusb422_pwr = container_of(work, struct tusb422_pwr_delivery, work);

	while (!gpiod_get_raw_value(tusb422_pwr->alert_gpio)) {
		tcpm_alert_event(0);
		/* Run USB Type-C state machine */
		tcpm_connection_state_machine(0);
		/* Run USB PD state machine */
		usb_pd_pe_state_machine(0);
	}

	if (tusb422_pwr->timer_expired) {
		tusb422_pwr->timer_expired = false;

		if (tusb422_pwr->callback) {
			tusb422_pwr->callback(0);

			/* Run USB Type-C state machine */
			tcpm_connection_state_machine(0);
			/* Run USB PD state machine */
			usb_pd_pe_state_machine(0);
		}
	}

#ifdef TUSB422_USE_POLLING
	tusb422_schedule_work(&tusb422_pwr->work);
#endif
	return;
}


static const struct regmap_config tusb422_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xff,
	.cache_type = REGCACHE_NONE,
};

static int tusb422_tcpm_init(struct tusb422_pwr_delivery *tusb422_pd)
{
	struct device *dev = tusb422_pd->dev;
	struct device_node *of_node = tusb422_pd->dev->of_node;
	unsigned int role, flags, rp_val;
	int ret;

	tusb422_pd->configuration = devm_kzalloc(dev, sizeof(*tusb422_pd->configuration), GFP_KERNEL);
	if (!tusb422_pd->configuration)
		return -ENOMEM;

	ret = of_property_read_u32(of_node, "ti,role", &role);
	if (ret) {
		pr_err("%s: Missing ti,role\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(of_node, "ti,rp-val", &rp_val);
	if (ret) {
		pr_err("%s: Missing ti,rp-val\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(of_node, "ti,flags", &flags);
	if (ret)
		pr_err("%s: Missing ti,flags setting to 0\n", __func__);

	tusb422_pd->configuration->role = (tc_role_t) role;
	tusb422_pd->configuration->flags = (uint16_t) flags;
	tusb422_pd->configuration->rp_val = (tcpc_role_rp_val_t) rp_val;
	tusb422_pd->configuration->slave_addr = tusb422_pd->client->addr;
	tusb422_pd->configuration->intf = tusb422_pd->client->adapter->nr;

	ret = tcpm_init(tusb422_pd->configuration);
	if (ret)
		ret = -ENODEV;

	return ret;
}

static int tusb422_i2c_probe(struct i2c_client *client,
							 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	int ret;

	tusb422_pd = devm_kzalloc(dev, sizeof(*tusb422_pd), GFP_KERNEL);
	if (!tusb422_pd)
		return -ENOMEM;

	tusb422_pd->client = client;
	i2c_set_clientdata(client, tusb422_pd);
	tusb422_pd->dev = dev;

#if defined(CONFIG_LGE_USB_TYPE_C) && defined(CONFIG_TUSB422_PAL)
	ret = hw_pd_dev_init(tusb422_pd->dev);
	if (ret)
		goto err_hw_pd_dev_init;
#endif

#ifdef CONFIG_REGMAP
	tusb422_pd->regmap = devm_regmap_init_i2c(client, &tusb422_regmap_config);
	if (IS_ERR(tusb422_pd->regmap)) {
		ret = PTR_ERR(tusb422_pd->regmap);
		dev_err(dev, "failed to allocate register map: %d\n", ret);
		goto err_regmap;
	}
#endif

	if (!tusb422_is_present(0)) {
		dev_err(dev, "%s: no TUSB422 device found\n", __func__);
		ret = -ENODEV;
		goto err_nodev;
	}

	ret = tusb422_of_get_gpios(tusb422_pd);
	if (ret)
		goto err_of;

#ifndef TUSB422_USE_POLLING
	if (tusb422_pd->alert_irq > 0) {
		ret = devm_request_irq(dev,
							   tusb422_pd->alert_irq,
							   tusb422_event_handler,
#ifdef CONFIG_LGE_USB_TYPE_C
							   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
#else
							   IRQF_TRIGGER_FALLING | IRQF_NO_THREAD | IRQF_NO_SUSPEND,
#endif
							   "tusb422_event",
							   tusb422_pd);

		if (ret) {
			dev_err(dev, "unable to request IRQ\n");
			goto err_irq;
		}

		enable_irq_wake(tusb422_pd->alert_irq);
	} else {
		dev_err(dev, "no IRQ resource found\n");
		ret = tusb422_pd->alert_irq;
		goto err_irq;
	}
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
	tusb422_pd->wq = alloc_workqueue("tusb422_wq",
				 WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_CPU_INTENSIVE,
				 1);
#endif
	hrtimer_init(&tusb422_pd->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	INIT_WORK(&tusb422_pd->work, tusb422_work);

#ifdef CONFIG_WAKELOCK
	wake_lock_init(&tusb422_pd->attach_wakelock, WAKE_LOCK_SUSPEND, "typec_attach_wakelock");
	wake_lock_init(&tusb422_pd->detach_wakelock, WAKE_LOCK_SUSPEND, "typec_detach_wakelock");
#endif

	tusb422_pd->timer_expired = false;
	tusb422_pd->timer.function = tusb422_timer_tasklet;

	usb_pd_print_version();

	ret = tusb422_tcpm_init(tusb422_pd);
	if (ret == 0)
		ret = tusb422_pd_init(tusb422_pd);

	if (ret)
		goto err_init;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	ret = tusb422_linux_dual_role_init(dev);
	if (ret) {
		dev_err(dev, "failed to init dual role class: %d\n", ret);
		goto err_init;
	}
#endif

#ifdef TUSB422_DEBUG
	ret = sysfs_create_group(&client->dev.kobj, &tusb422_attr_group);
	if (ret) {
		dev_err(dev, "failed to create sysfs: %d\n", ret);
		goto err_sysfs;
	}
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
	/* Run USB Type-C init state machine */
	tcpm_connection_state_machine(0);
#endif

	tusb422_schedule_work(&tusb422_pd->work);

	return 0;

err_sysfs:
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	devm_dual_role_instance_unregister(dev, tusb422_dual_role_phy);
#endif

err_init:
	hrtimer_cancel(&tusb422_pd->timer);
	cancel_work_sync(&tusb422_pd->work);
#ifdef CONFIG_WAKELOCK
	wake_lock_destroy(&tusb422_pd->attach_wakelock);
	wake_lock_destroy(&tusb422_pd->detach_wakelock);
#endif

#ifdef CONFIG_REGMAP
err_regmap:
#endif
err_of:
err_irq:
err_nodev:
#if defined(CONFIG_LGE_USB_TYPE_C) && defined(CONFIG_TUSB422_PAL)
err_hw_pd_dev_init:
#endif
	i2c_set_clientdata(client, NULL);

	return ret;
}

static int tusb422_remove(struct i2c_client *client)
{
	disable_irq_wake(tusb422_pd->alert_irq);
	hrtimer_cancel(&tusb422_pd->timer);
	cancel_work_sync(&tusb422_pd->work);
#ifdef CONFIG_WAKELOCK
	wake_lock_destroy(&tusb422_pd->attach_wakelock);
	wake_lock_destroy(&tusb422_pd->detach_wakelock);
#endif
#ifdef TUSB422_DEBUG
	sysfs_remove_group(&client->dev.kobj, &tusb422_attr_group);
#endif
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	devm_dual_role_instance_unregister(&client->dev, tusb422_dual_role_phy);
#endif
	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id tusb422_id[] = {
	{ TUSB422_I2C_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tusb422_id);

#ifdef CONFIG_OF
static const struct of_device_id tusb422_pd_ids[] = {
	{ .compatible = "ti,tusb422-usb-pd" },
	{ }
};
MODULE_DEVICE_TABLE(of, tusb422_pd_ids);
#endif

static struct i2c_driver tusb422_i2c_driver = {
	.driver = {
		.name = TUSB422_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tusb422_pd_ids),
	},
	.probe = tusb422_i2c_probe,
	.remove = tusb422_remove,
	.id_table = tusb422_id,
};
module_i2c_driver(tusb422_i2c_driver);

MODULE_LICENSE("GPL v2");
