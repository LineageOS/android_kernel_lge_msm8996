/*
 * TUSB422 Power Delivery
 *
 * Author: Brian Quach <brian.quach@ti.com>
 *
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "usb_pd_pal.h"
#include "tcpm.h"
#include "tusb422_common.h"
#include "usb_pd_policy_engine.h"
#ifndef CONFIG_LGE_USB_TYPE_C
#include <huawei_platform/usb/hw_pd_dev.h>
#endif


#define TCP_VBUS_CTRL_PD_DETECT (1 << 7)

// TODO: add port number to device tree and use that for port.

void usb_pd_pal_source_vbus(unsigned int port, bool usb_pd, uint16_t mv, uint16_t ma)
{
	struct pd_dpm_vbus_state vbus_state;

	PRINT("%s: %u mV, %s\n", __func__, mv, (usb_pd) ? "USB_PD" : "TYPE-C");

	vbus_state.vbus_type = (usb_pd) ? TCP_VBUS_CTRL_PD_DETECT : 0;

	vbus_state.mv = mv;
	vbus_state.ma = ma;

	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_SOURCE_VBUS, (void *)&vbus_state);
	return;
}

void usb_pd_pal_disable_vbus(unsigned int port)
{
	PRINT("%s\n", __func__);

	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_DIS_VBUS_CTRL, NULL);
	return;
}

void usb_pd_pal_sink_vbus(unsigned int port, bool usb_pd, uint16_t mv, uint16_t ma)
{
	struct pd_dpm_vbus_state vbus_state;

#ifdef CONFIG_LGE_USB_TYPE_C
	DEBUG("%s: %u mV, %u mA %s\n", __func__, mv, ma, (usb_pd) ? "USB_PD" : "TYPE-C");
#else
	PRINT("%s: %u mV, %u mA %s\n", __func__, mv, ma, (usb_pd) ? "USB_PD" : "TYPE-C");
#endif

	vbus_state.vbus_type = (usb_pd) ? TCP_VBUS_CTRL_PD_DETECT : 0;

	if (usb_pd)
	{
		vbus_state.ext_power = usb_pd_pe_is_remote_externally_powered(port);
		PRINT("%s: ext_power = %u\n", __func__, vbus_state.ext_power);
	}

	vbus_state.mv = mv;
	vbus_state.ma = ma;

	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_SINK_VBUS, (void *)&vbus_state);
	return;
}

/* For battery supplies */
void usb_pd_pal_sink_vbus_batt(unsigned int port, uint16_t min_mv, uint16_t max_mv, uint16_t mw)
{
	PRINT("%s: %u - %u mV, %u mW\n", __func__, min_mv, max_mv, mw);

	return;
}

/* For variable supplies */
void usb_pd_pal_sink_vbus_vari(unsigned int port, uint16_t min_mv, uint16_t max_mv, uint16_t ma)
{
	PRINT("%s: %u - %u mV, %u mA\n", __func__, min_mv, max_mv, ma);

	return;
}

void usb_pd_pal_notify_pd_state(unsigned int port, usb_pd_pe_state_t state)
{
	struct pd_dpm_pd_state pd_state;

	PRINT("%s: %s\n", __func__,
		  (state == PE_SRC_READY) ? "PE_SRC_READY" :
		  (state == PE_SNK_READY) ? "PE_SNK_READY" : "?");

	switch (state)
	{
		case PE_SRC_READY:
			pd_state.connected = PD_CONNECT_PE_READY_SRC;
			pd_dpm_handle_pe_event(PD_DPM_PE_EVT_PD_STATE, (void *)&pd_state);
			break;

		case PE_SNK_READY:
			pd_state.connected = PD_CONNECT_PE_READY_SNK;
			pd_dpm_handle_pe_event(PD_DPM_PE_EVT_PD_STATE, (void *)&pd_state);
			break;

		default:
			break;
	}

	return;
}

void usb_pd_pal_notify_connect_state(unsigned int port, tcpc_state_t state, bool polarity)
{
	struct pd_dpm_typec_state tc_state;

	tc_state.polarity = polarity;

	switch (state)
	{
		case TCPC_STATE_UNATTACHED_SRC:
		case TCPC_STATE_UNATTACHED_SNK:
			PRINT("%s: TYPEC_UNATTACHED, polarity = 0x%x\n", __func__, polarity);
			tc_state.new_state = PD_DPM_TYPEC_UNATTACHED;
			pd_dpm_handle_pe_event(PD_DPM_PE_EVT_TYPEC_STATE, (void*)&tc_state);
			break;

		case TCPC_STATE_ATTACHED_SRC:
			PRINT("%s: TYPEC_ATTACHED_SRC, polarity = 0x%x\n", __func__, polarity);
			tc_state.new_state = PD_DPM_TYPEC_ATTACHED_SRC;
			pd_dpm_handle_pe_event(PD_DPM_PE_EVT_TYPEC_STATE, (void*)&tc_state);
			break;

		case TCPC_STATE_ATTACHED_SNK:
#ifdef CONFIG_LGE_USB_FACTORY
		case TCPC_STATE_DEBUG_ACC_SNK:
#endif
			PRINT("%s: TYPEC_ATTACHED_SNK, polarity = 0x%x\n", __func__, polarity);
			tc_state.new_state = PD_DPM_TYPEC_ATTACHED_SNK;
			pd_dpm_handle_pe_event(PD_DPM_PE_EVT_TYPEC_STATE, (void*)&tc_state);
			break;

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		case TCPC_STATE_CC_FAULT_CC_OV:
		case TCPC_STATE_CC_FAULT_SWING:
		case TCPC_STATE_CC_FAULT_SBU_OV:
		case TCPC_STATE_CC_FAULT_SBU_ADC:
		case TCPC_STATE_CC_FAULT_SBU_DRY_CHECK:
		case TCPC_STATE_CC_FAULT_TEST:
			PRINT("%s: TYPEC_CC_FAULT\n", __func__);
			tc_state.new_state = PD_DPM_TYPEC_CC_FAULT;
			pd_dpm_handle_pe_event(PD_DPM_PE_EVT_TYPEC_STATE, (void*)&tc_state);
			break;
#endif

		default:
			break;
	}

	return;
}

void usb_pd_pal_data_role_swap(unsigned int port, uint8_t new_role)
{
	struct pd_dpm_swap_state swap_state;

	PRINT("%s: new_role = %x\n", __func__, new_role);

	swap_state.new_role = new_role;

	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_DR_SWAP, (void *)&swap_state);

	return;
}

void usb_pd_pal_power_role_swap(unsigned int port, uint8_t new_role)
{
	//pd_dpm_handle_pe_event();

	return;
}

#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
void usb_pd_pal_debug_accessory_mode(unsigned int port, bool is_on)
{
	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_DEBUG_ACCESSORY, (void *)&is_on);
	return;
}
#endif

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
int usb_pd_pal_get_sbu_adc(unsigned int port)
{
	return pd_dpm_handle_pe_event(PD_DPM_PE_EVENT_GET_SBU_ADC, NULL);
}

int usb_pd_pal_set_moisture_detect_use_sbu(void)
{
	return pd_dpm_handle_pe_event(PD_DPM_PE_EVENT_SET_MOISTURE_DETECT_USE_SBU, NULL);
}
#endif
