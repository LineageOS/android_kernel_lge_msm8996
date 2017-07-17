#ifndef __HW_PD_DEV_H__
#define __HW_PD_DEV_H__

#include "../tusb422_linux.h"

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>

#ifdef CONFIG_LGE_USB_DEBUGGER
#include <soc/qcom/lge/power/lge_power_class.h>
#include <soc/qcom/lge/power/lge_cable_detect.h>
#endif

#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include <linux/usb/class-dual-role.h>
#else
enum {
	DUAL_ROLE_PROP_MODE_UFP = 0,
	DUAL_ROLE_PROP_MODE_DFP,
	DUAL_ROLE_PROP_MODE_FAULT,
	DUAL_ROLE_PROP_MODE_NONE,
	/*The following should be the last element*/
	DUAL_ROLE_PROP_MODE_TOTAL,
};

enum {
	DUAL_ROLE_PROP_PR_SRC = 0,
	DUAL_ROLE_PROP_PR_SNK,
	DUAL_ROLE_PROP_PR_FAULT,
	DUAL_ROLE_PROP_PR_NONE,
	/*The following should be the last element*/
	DUAL_ROLE_PROP_PR_TOTAL,

};

enum {
	DUAL_ROLE_PROP_DR_HOST = 0,
	DUAL_ROLE_PROP_DR_DEVICE,
	DUAL_ROLE_PROP_DR_FAULT,
	DUAL_ROLE_PROP_DR_NONE,
	/*The following should be the last element*/
	DUAL_ROLE_PROP_DR_TOTAL,
};
#endif

struct hw_pd_dev {
	struct device *dev;

	/* usb */
	struct power_supply *usb_psy;

	int mode;
	int pr;
	int dr;

	struct gpio_desc *redriver_sel_gpio;

	/* charger */
	struct regulator *vbus_reg;
	struct delayed_work otg_work;

	struct power_supply chg_psy;
	struct power_supply *batt_psy;

	bool is_otg;
	bool is_present;
	int curr_max;
	int volt_max;
	enum power_supply_type typec_mode;

#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
	bool is_debug_accessory;
#endif
#ifdef CONFIG_LGE_USB_DEBUGGER
	struct work_struct usb_debugger_work;
	struct lge_power *lge_power_cd;
	struct gpio_desc *sbu_sel_gpio;
#endif
};

enum pd_dpm_pe_evt {
	PD_DPM_PE_EVT_SOURCE_VBUS,
	PD_DPM_PE_EVT_DIS_VBUS_CTRL,
	PD_DPM_PE_EVT_SINK_VBUS,
	PD_DPM_PE_EVT_PD_STATE,
	PD_DPM_PE_EVT_TYPEC_STATE,
	PD_DPM_PE_EVT_DR_SWAP,
	PD_DPM_PE_EVT_PR_SWAP,
#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
	PD_DPM_PE_EVT_DEBUG_ACCESSORY,
#endif
};

struct pd_dpm_vbus_state {
	uint8_t vbus_type;
	uint16_t mv;
	uint16_t ma;
	bool ext_power;
};

enum pd_connect {
	PD_CONNECT_PE_READY_SRC,
	PD_CONNECT_PE_READY_SNK,
};

struct pd_dpm_pd_state {
	enum pd_connect connected;
};

enum pd_dpm_typec {
	PD_DPM_TYPEC_UNATTACHED,
	PD_DPM_TYPEC_ATTACHED_SRC,
	PD_DPM_TYPEC_ATTACHED_SNK,
	PD_DPM_TYPEC_CC_FAULT,
};

struct pd_dpm_typec_state {
	bool polarity;
	enum pd_dpm_typec new_state;
};

struct pd_dpm_swap_state {
	uint8_t new_role;
};

int pd_dpm_handle_pe_event(enum pd_dpm_pe_evt event, void *state);
int hw_pd_dev_init(struct device *dev);

int set_mode(struct hw_pd_dev *dev, int mode);
int set_pr(struct hw_pd_dev *dev, int pr);
int set_dr(struct hw_pd_dev *dev, int dr);

#endif /* __HW_PD_DEV_H__ */
