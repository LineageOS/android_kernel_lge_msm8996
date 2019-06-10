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

#ifndef __ANX7688_CORE_H
#define __ANX7688_CORE_H

#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/completion.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/class-dual-role.h>

#ifdef CONFIG_LGE_DP_ANX7688
#include <linux/slimport.h>
#endif

#ifdef CONFIG_LGE_PM_LGE_POWER_CORE
#include <soc/qcom/lge/power/lge_power_class.h>
#include <soc/qcom/smem.h>
#endif
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
#include <soc/qcom/lge/power/lge_cable_detect.h>
#elif defined (CONFIG_LGE_PM_CABLE_DETECTION)
#include <soc/qcom/lge/lge_cable_detection.h>
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
#define PD_MAX_PDO_NUM 7
#endif

enum DP_HPD_STATUS {
	STATE_LINK_TRAINING = 0,
	STATE_HDMI_HPD,
	STATE_IRQ_PROCESS,
	STATE_HPD_DONE,
};

struct anx7688_data {
	int vid;
	int pid;
	int devver;
	int fwver;
	int cdet_gpio;
	int alter_gpio;
	int pwren_gpio;
	int rstn_gpio;
	int sbu_gpio;
	int vconn_gpio;
	int avdd33_gpio;
	u32 pd_max_volt;
	u32 pd_max_power;
	u32 pd_min_power;
	int fw_force_update;
	bool vconn_always_on;
	int vsafe0v_level;
	bool auto_pd_support;
	bool avdd33_ext_ldo;
};

struct anx7688_chip {
	struct i2c_client *client;
	struct anx7688_data *pdata;
	struct workqueue_struct *cc_wq;
	struct work_struct dwork;
	struct work_struct awork;
	struct delayed_work twork;
	struct delayed_work cwork;
	struct delayed_work pdwork;
	struct wake_lock wlock;
	struct mutex mlock;

	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply usbpd_psy;

	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc *desc;
	struct completion wait_pwr_ctrl;

	struct regulator *avdd33;
	struct regulator *avdd10;
	struct regulator *vbus_out;

	int cdet_irq;
	int alter_irq;

	int role;
	u8 mode;
	u8 power_role;
	u8 data_role;

	u16 state;
	int cc1;
	int cc2;

	int is_otg;
	int is_present;
	int volt_max;
	int curr_max;
	int charger_type;
	bool is_vconn_on;
	bool is_sbu_switched;
	bool deglich_check;
	bool power_ctrl;
#ifdef CONFIG_LGE_USB_ANX7688_ADC
	bool is_pd_connected;
#endif
#ifdef CONFIG_LGE_DP_ANX7688
	struct platform_device *hdmi_pdev;
	struct delayed_work     swork;
	struct workqueue_struct *dp_wq;
	int dp_status;
	enum DP_HPD_STATUS dp_hpd_status;
	unsigned char dp_rx_bandwidth;
	int dp_rx_lanecount;
	struct wake_lock dp_lock;
	int rx_cable_type;
#endif
	atomic_t power_on;
	atomic_t vdd_on;

#ifdef CONFIG_LGE_USB_ANX7688_OVP
	union power_supply_propval rp;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_SIMPLE
	int dp_alt_mode;
#endif
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
	u32 src_pdo[PD_MAX_PDO_NUM];
	u32 offered_pdo[PD_MAX_PDO_NUM];
	u32 rdo;
	u32 offered_rdo;
#endif
};

void anx7688_sbu_ctrl(struct anx7688_chip *chip, bool dir);
void anx7688_pwr_on(struct anx7688_chip *chip);
void anx7688_pwr_down(struct anx7688_chip *chip);
/*#ifdef CONFIG_LGE_DP_ANX7688
extern int init_anx7688_dp(struct device *dev, struct anx7688_chip *chip);
#endif*/
#endif /* __ANX7688_CORE_H */
