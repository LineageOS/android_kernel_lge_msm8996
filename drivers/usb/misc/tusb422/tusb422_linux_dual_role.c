/*
 * Texas Instruments TUSB422 Power Delivery
 *
 * Author: Brian Quach <brian.quach@ti.com>
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

#ifdef CONFIG_DUAL_ROLE_USB_INTF

#include "tusb422_linux_dual_role.h"
#include "tcpm.h"
#include "usb_pd.h"
#include "usb_pd_policy_engine.h"
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb/class-dual-role.h>


/* Uncomment the following line to use USB-PD messaging for changing the data or power role */
//#define USE_USB_PD_FOR_ROLE_CHANGE

struct dual_role_phy_instance *tusb422_dual_role_phy;

static enum dual_role_property tusb422_dual_role_props[] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	DUAL_ROLE_PROP_VCONN_SUPPLY,
#ifdef CONFIG_LGE_USB_TYPE_C
	DUAL_ROLE_PROP_CC1,
	DUAL_ROLE_PROP_CC2,
#endif
};


static int tusb422_dual_role_get_prop(struct dual_role_phy_instance *dual_role,
									  enum dual_role_property prop,
									  unsigned int *val)
{
	static uint8_t prop_mode = DUAL_ROLE_PROP_MODE_NONE;
	static uint8_t prop_pr = DUAL_ROLE_PROP_PR_NONE;
	static uint8_t prop_dr = DUAL_ROLE_PROP_DR_NONE;
	static uint8_t prop_vconn = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
	usb_pd_port_t *pd_dev = usb_pd_pe_get_device(0);
	tcpc_device_t *tcpc_dev = tcpm_get_device(0);
#ifdef CONFIG_LGE_USB_TYPE_C
	unsigned int cc;
#endif

	switch (prop) {
	case DUAL_ROLE_PROP_SUPPORTED_MODES:
		if ((tcpc_dev->role == ROLE_DRP) || (tcpc_dev->flags & TC_FLAGS_TEMP_ROLE))
			*val = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		else if (tcpc_dev->role == ROLE_SRC)
			*val = DUAL_ROLE_SUPPORTED_MODES_DFP;
		else
			*val = DUAL_ROLE_SUPPORTED_MODES_UFP;
		break;

	case DUAL_ROLE_PROP_MODE:
		if (tcpc_dev->flags & TC_FLAGS_TEMP_ROLE)
			*val = prop_mode;
		else if (tcpc_dev->state == TCPC_STATE_ATTACHED_SRC)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (tcpc_dev->state == TCPC_STATE_ATTACHED_SNK)
			*val = DUAL_ROLE_PROP_MODE_UFP;
#ifdef CONFIG_LGE_USB_TYPE_C
		else if (IS_STATE_CC_FAULT(tcpc_dev->state))
			*val = DUAL_ROLE_PROP_MODE_FAULT;
#endif
		else
			*val = DUAL_ROLE_PROP_MODE_NONE;
		prop_mode = *val;
		break;

	case DUAL_ROLE_PROP_PR:
		if (tcpc_dev->flags & TC_FLAGS_TEMP_ROLE)
			*val = prop_pr;
		else if ((tcpc_dev->state == TCPC_STATE_ATTACHED_SRC) ||
				 (tcpc_dev->state == TCPC_STATE_ATTACHED_SNK)) {
			if (pd_dev->power_role == PD_PWR_ROLE_SNK)
				*val = DUAL_ROLE_PROP_PR_SNK;
			else
				*val = DUAL_ROLE_PROP_PR_SRC;
#ifdef CONFIG_LGE_USB_TYPE_C
		} else if (IS_STATE_CC_FAULT(tcpc_dev->state)) {
			*val = DUAL_ROLE_PROP_PR_FAULT;
#endif
		} else
			*val = DUAL_ROLE_PROP_PR_NONE;
		prop_pr = *val;
		break;

	case DUAL_ROLE_PROP_DR:
		if (tcpc_dev->flags & TC_FLAGS_TEMP_ROLE)
			*val = prop_dr;
		else if ((tcpc_dev->state == TCPC_STATE_ATTACHED_SRC) ||
				 (tcpc_dev->state == TCPC_STATE_ATTACHED_SNK)) {
			if (pd_dev->data_role == PD_DATA_ROLE_UFP)
				*val = DUAL_ROLE_PROP_DR_DEVICE;
			else
				*val = DUAL_ROLE_PROP_DR_HOST;
#ifdef CONFIG_LGE_USB_TYPE_C
		} else if (IS_STATE_CC_FAULT(tcpc_dev->state)) {
			*val = DUAL_ROLE_PROP_DR_FAULT;
#endif
		} else
			*val = DUAL_ROLE_PROP_DR_NONE;
		prop_dr = *val;
		break;

	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		if (tcpc_dev->flags & TC_FLAGS_TEMP_ROLE)
			*val = prop_vconn;
		else if (tcpm_is_vconn_enabled(tcpc_dev->port))
			*val = DUAL_ROLE_PROP_VCONN_SUPPLY_YES;
		else
			*val = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		prop_vconn = *val;
		break;

#ifdef CONFIG_LGE_USB_TYPE_C
	case DUAL_ROLE_PROP_CC1:
	case DUAL_ROLE_PROP_CC2:
		if (tcpc_dev->debug_accessory_mode) {
			*val = DUAL_ROLE_PROP_CC_RD;
			break;
		}

		if (prop == DUAL_ROLE_PROP_CC1)
			cc = TCPC_CC1_STATE(tcpc_dev->cc_status);
		else
			cc = TCPC_CC2_STATE(tcpc_dev->cc_status);

		if (tcpc_dev->cc_status & CC_STATUS_CONNECT_RESULT) {
			switch (cc) {
			case CC_SNK_STATE_DEFAULT:
				*val = DUAL_ROLE_PROP_CC_RP_DEFAULT;
				break;
			case CC_SNK_STATE_POWER15:
				*val = DUAL_ROLE_PROP_CC_RP_POWER1P5;
				break;
			case CC_SNK_STATE_POWER30:
				*val = DUAL_ROLE_PROP_CC_RP_POWER3P0;
				break;
			default:
				*val = DUAL_ROLE_PROP_CC_OPEN;
				break;
			}
		} else {
			switch (cc) {
			case CC_SRC_STATE_RD:
				*val = DUAL_ROLE_PROP_CC_RD;
				break;
			case CC_SRC_STATE_RA:
				*val = DUAL_ROLE_PROP_CC_RA;
				break;
			case CC_SRC_STATE_OPEN:
			default:
				*val = DUAL_ROLE_PROP_CC_OPEN;
				break;
			}
		}
		break;
#endif

	default:
		return -EINVAL;
	}

	return 0;
}

static int tusb422_dual_role_set_prop(struct dual_role_phy_instance *dual_role,
									  enum dual_role_property prop,
									  const unsigned int *val)
{
	int ret = 0;
	usb_pd_port_t *pd_dev = usb_pd_pe_get_device(0);

	switch (prop) {
	case DUAL_ROLE_PROP_PR:
		if (((*val == DUAL_ROLE_PROP_PR_SNK) && (pd_dev->power_role == PD_PWR_ROLE_SRC)) ||
			((*val == DUAL_ROLE_PROP_PR_SRC) && (pd_dev->power_role == PD_PWR_ROLE_SNK))) {
#ifdef USE_USB_PD_FOR_ROLE_CHANGE
			if (usb_pd_policy_manager_request(pd_dev->port, PD_POLICY_MNGR_REQ_PR_SWAP))
				tcpm_try_role_swap(pd_dev->port);
#else
			tcpm_try_role_swap(pd_dev->port);
#endif
		}
#ifdef CONFIG_LGE_USB_TYPE_C
		if (*val == DUAL_ROLE_PROP_PR_FAULT)
			tcpm_cc_fault_test(0, true);
		else if (*val == DUAL_ROLE_PROP_PR_NONE)
			tcpm_cc_fault_test(0, false);
#endif
		break;

	case DUAL_ROLE_PROP_DR:
#ifdef USE_USB_PD_FOR_ROLE_CHANGE
		if (((*val == DUAL_ROLE_PROP_DR_HOST) && (pd_dev->data_role == PD_DATA_ROLE_UFP)) ||
			((*val == DUAL_ROLE_PROP_DR_DEVICE) && (pd_dev->data_role == PD_DATA_ROLE_DFP))) {
			if (usb_pd_policy_manager_request(pd_dev->port, PD_POLICY_MNGR_REQ_DR_SWAP))
				ret = -EBUSY;
		}
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
		if (*val == DUAL_ROLE_PROP_DR_FAULT)
			tcpm_cc_fault_test(0, true);
		else if (*val == DUAL_ROLE_PROP_DR_NONE)
			tcpm_cc_fault_test(0, false);
#endif
		break;

	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		if (((*val == DUAL_ROLE_PROP_VCONN_SUPPLY_NO) && tcpm_is_vconn_enabled(pd_dev->port))||
			((*val == DUAL_ROLE_PROP_VCONN_SUPPLY_YES) && !tcpm_is_vconn_enabled(pd_dev->port))) {

			if (usb_pd_policy_manager_request(pd_dev->port, PD_POLICY_MNGR_REQ_VCONN_SWAP))
				ret = -EBUSY;
		}
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int tusb422_dual_role_prop_is_writeable(struct dual_role_phy_instance *dual_role,
											   enum dual_role_property prop)
{
	tcpc_device_t *tcpc_dev = tcpm_get_device(0);
	int ret = 0;

	switch (prop) {
	case DUAL_ROLE_PROP_DR:
	case DUAL_ROLE_PROP_PR:
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		if (tcpc_dev->role == ROLE_DRP)
			ret = 1;
		break;

	default:
		break;
	}

	return ret;
}


int tusb422_linux_dual_role_init(struct device *dev)
{
	struct dual_role_phy_desc *drp_desc;

	tusb422_dual_role_phy = devm_kzalloc(dev, sizeof(*tusb422_dual_role_phy), GFP_KERNEL);
	if (!tusb422_dual_role_phy)
		return -ENOMEM;

	drp_desc = devm_kzalloc(dev, sizeof(*drp_desc), GFP_KERNEL);
	if (!drp_desc)
		return -ENOMEM;

	drp_desc->name = "otg_default";
	drp_desc->num_properties = ARRAY_SIZE(tusb422_dual_role_props);
	drp_desc->properties = tusb422_dual_role_props;
	drp_desc->get_property = tusb422_dual_role_get_prop;
	drp_desc->set_property = tusb422_dual_role_set_prop;
	drp_desc->property_is_writeable = tusb422_dual_role_prop_is_writeable;

	tusb422_dual_role_phy = devm_dual_role_instance_register(dev, drp_desc);
	if (IS_ERR(tusb422_dual_role_phy)) {
		dev_err(dev, "failed to register dual role instance\n");
		return -EINVAL;
	}

	return 0;
}

#endif /* CONFIG_DUAL_ROLE_USB_INTF */
