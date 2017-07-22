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

#include "usb_pd_policy_engine.h"
#include "tcpm.h"
#include "tusb422.h"
#include "tusb422_common.h"
#include "usb_pd.h"
#include <linux/module.h>
#ifdef CONFIG_TUSB422_PAL
	#include "usb_pd_pal.h"
#endif
#include "usb_pd_protocol.h"
#ifndef CONFIG_TUSB422
	#include <string.h>
#endif
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	#include "tusb422_linux_dual_role.h"
	#include <linux/device.h>
	#include <linux/usb/class-dual-role.h>
#endif

#define VDM_DISCOVER_IDENTITY
#ifdef VDM_DISCOVER_IDENTITY
#include "vdm.h"
#endif

/* PD Counter */
#define N_CAPS_COUNT        50
#define N_HARD_RESET_COUNT  2
#define N_DISCOVER_IDENTITY_COUNT 3  /* 20 per PD3.0 spec, but using 3 to avoid violating Sink Wait Cap timeout */
#define N_BUSY_COUNT 1  /* 5 per PD3.0 spec but use 1 to meet USB-C v1.2 requirement for mode entry */

/* PD Time Values */
#define T_NO_RESPONSE_MS            5000   /* 4.5 - 5.5 s */
#define T_SENDER_RESPONSE_MS          25   /*  24 - 30 ms */
#define T_SWAP_SOURCE_START_MS        40   /*  20 - ? ms*/
#define T_TYPEC_SEND_SOURCE_CAP_MS   150   /* 100 - 200 ms */
#define T_TYPEC_SINK_WAIT_CAP_MS     500   /* 310 - 620 ms */
#define T_PS_HARD_RESET_MS            25   /* 25 - 35 ms */
#define T_PS_TRANSITION_MS           500   /* 450 - 550 ms */
#define T_SRC_RECOVER_MS             700   /* 660 - 1000 ms */
#define T_SRC_TRANSITION_MS           25   /* 25 - 35 ms */
#define T_SINK_REQUEST_MS            200   /* 100 - ? ms */
#define T_BIST_CONT_MODE_MS           50   /* 30 - 60 ms */
//#define T_SRC_TURN_ON_MS             275   /* tSrcTurnOn: 0 - 275ms */
//#define T_SRC_SETTLE_MS              275   /* tSrcSettle: 0 - 275ms */
#define T_PS_SOURCE_OFF_MS           880   /* 750 - 920 ms */
#define T_PS_SOURCE_ON_MS            440   /* 390 - 480 ms */
#define T_VCONN_SOURCE_ON_MS         100   /* ? - 100 ms */
#define T_SINK_TX_MS                  16   /* 16 - 20 ms */

#define T_VBUS_5V_STABLIZE_MS         20   /* delay for VBUS to stablize after vSafe5V-min is reached */

usb_pd_port_t pd[NUM_TCPC_DEVICES];
static uint8_t buf[32];

extern void usb_pd_pm_evaluate_src_caps(unsigned int port);
extern usb_pd_port_config_t* usb_pd_pm_get_config(unsigned int port);
extern void build_rdo(unsigned int port);
extern uint32_t get_data_object(uint8_t *obj_data);
#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
extern void tusb422_set_dp_notify_node(int val);
#endif

const char * const pdstate2string[PE_NUM_STATES] =
{
	"UNATTACHED",

	/* Source */
	"SRC_STARTUP",
	"SRC_DISCOVERY",
	"SRC_SEND_CAPS",
	"SRC_NEGOTIATE_CAPABILITY",
	"SRC_TRANSITION_SUPPLY",
	"SRC_TRANSITION_SUPPLY_EXIT",
	"SRC_READY",
	"SRC_DISABLED",
	"SRC_CAPABILITY_RESPONSE",
	"SRC_HARD_RESET",
	"SRC_HARD_RESET_RECEIVED",
	"SRC_TRANSITION_TO_DEFAULT",
	"SRC_GET_SINK_CAP",
	"SRC_WAIT_NEW_CAPS",
	"SRC_SEND_SOFT_RESET",
	"SRC_SOFT_RESET",
	"SRC_SEND_NOT_SUPPORTED",
	"SRC_NOT_SUPPORTED_RECEIVED",
	"SRC_PING",
	"SRC_SEND_SOURCE_ALERT",
	"SRC_SINK_ALERT_RECEIVED",
	"SRC_GIVE_SOURCE_CAP_EXT",
	"SRC_GIVE_SOURCE_STATUS",
	"SRC_GET_SINK_STATUS",

	/* Sink */
	"SNK_STARTUP",
	"SNK_DISCOVERY",
	"SNK_WAIT_FOR_CAPS",
	"SNK_EVALUATE_CAPABILITY",
	"SNK_SELECT_CAPABILITY",
	"SNK_TRANSITION_SINK",
	"SNK_READY",
	"SNK_HARD_RESET",
	"SNK_TRANSITION_TO_DEFAULT",
	"SNK_GIVE_SINK_CAP",
	"SNK_GET_SOURCE_CAP",
	"SNK_SEND_SOFT_RESET",
	"SNK_SOFT_RESET",
	"SNK_SEND_NOT_SUPPORTED",
	"SNK_NOT_SUPPORTED_RECEIVED",
	"SNK_SOURCE_ALERT_RECEIVED",
	"SNK_SEND_SINK_ALERT",
	"SNK_GET_SOURCE_CAP_EXT",
	"SNK_GET_SOURCE_STATUS",
	"SNK_GIVE_SINK_STATUS",

	/* Dual-role */
	"DR_SRC_GIVE_SINK_CAP",
	"DR_SNK_GIVE_SOURCE_CAP",

	/* BIST */
	"BIST_CARRIER_MODE",
	"BIST_TEST_MODE",

	/* Error Recovery */
	"ERROR_RECOVERY",

	/* Data Role Swap */
	"DRS_SEND_SWAP",
	"DRS_EVALUATE_SWAP",
	"DRS_REJECT_SWAP",
	"DRS_ACCEPT_SWAP",
	"DRS_CHANGE_ROLE",

	/* Power Role Swap */
	"PRS_SEND_SWAP",
	"PRS_EVALUATE_SWAP",
	"PRS_REJECT_SWAP",
	"PRS_ACCEPT_SWAP",
	"PRS_TRANSITION_TO_OFF",
	"PRS_ASSERT_RD",
	"PRS_WAIT_SOURCE_ON",
	"PRS_ASSERT_RP",
	"PRS_SOURCE_ON",
	"PRS_SOURCE_ON_EXIT",

	/* VCONN Swap */
	"VCS_SEND_SWAP",
	"VCS_EVALUATE_SWAP",
	"VCS_REJECT_SWAP",
	"VCS_ACCEPT_SWAP",
	"VCS_WAIT_FOR_VCONN",
	"VCS_TURN_OFF_VCONN",
	"VCS_TURN_ON_VCONN",
	"VCS_SEND_PS_RDY",

};

#ifndef CONFIG_TUSB422
#if DEBUG_LEVEL >= 1
void pe_debug_state_history(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];
	uint8_t i;

	PRINT("\nPolicy Engine State History:\n");
	for (i = 0; i < PD_STATE_HISTORY_LEN; i++)
	{
		if (dev->state[i] < PE_NUM_STATES)
		{
			PRINT("%s[%u] %s\n", (&dev->state[i] == dev->current_state) ? "->" : "  ", i, pdstate2string[dev->state[i]]);
		}
	}

	return;
}
#endif
#endif

static void pe_set_state(usb_pd_port_t *dev, usb_pd_pe_state_t new_state)
{
	CRIT("PE_%s\n", pdstate2string[new_state]);

	dev->state[dev->state_idx] = new_state;
	dev->current_state = &dev->state[dev->state_idx];
	dev->state_idx++;
	dev->state_idx &= PD_STATE_INDEX_MASK;
	dev->state_change = true;

	return;
}

static void timeout_no_response(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	DEBUG("%s\n", __func__);

	dev->no_response_timed_out = true;

	if (dev->hard_reset_cnt > N_HARD_RESET_COUNT)
	{
		if (!dev->pd_connected_since_attach)
		{
			if ((*dev->current_state == PE_SRC_SEND_CAPS) ||
				((*dev->current_state == PE_SRC_DISCOVERY) && (dev->caps_cnt > N_CAPS_COUNT)))
			{
				pe_set_state(dev, PE_SRC_DISABLED);
			}
		}
		else /* previously PD connected */
		{
			pe_set_state(dev, PE_ERROR_RECOVERY);
		}
	}

	return;
}

static void timer_start_no_response(usb_pd_port_t *dev)
{
	INFO("%s\n", __func__);

	dev->no_response_timed_out = false;

	tusb422_lfo_timer_start(&dev->timer2, T_NO_RESPONSE_MS, timeout_no_response);

	return;
}

static void timer_cancel_no_response(usb_pd_port_t *dev)
{
#ifdef CONFIG_LGE_USB_TYPE_C
	if (dev->timer2.function == timeout_no_response)
		return;
#endif

	INFO("%s\n", __func__);

	tusb422_lfo_timer_cancel(&dev->timer2);

	return;
}


usb_pd_port_t * usb_pd_pe_get_device(unsigned int port)
{
	return &pd[port];
}


void usb_pd_pe_init(unsigned int port, usb_pd_port_config_t *config)
{
	usb_pd_port_t *dev = &pd[port];
	unsigned int i;

	pd[port].state_idx = 0;

	for (i = 0; i < PD_STATE_HISTORY_LEN; i++)
	{
		pd[port].state[i] = (usb_pd_pe_state_t)0xEE;
	}

	dev->state_change = false;
	dev->current_state = &dev->state[0];

	dev->port = port;
	dev->timer.data = port;
	dev->timer2.data = port;

	dev->src_settling_time = config->src_settling_time_ms;

	return;
}


void usb_pd_pe_connection_state_change_handler(unsigned int port, tcpc_state_t state)
{
	usb_pd_port_t *dev = &pd[port];

	switch (state)
	{
		case TCPC_STATE_UNATTACHED_SRC:
		case TCPC_STATE_UNATTACHED_SNK:
			timer_cancel_no_response(dev);
			timer_cancel(&dev->timer);
			pe_set_state(dev, PE_UNATTACHED);
			break;

		case TCPC_STATE_ATTACHED_SRC:
			dev->data_role = PD_DATA_ROLE_DFP;
			dev->power_role = PD_PWR_ROLE_SRC;
			dev->vconn_source = true;
			pe_set_state(dev, PE_SRC_STARTUP);
			break;

		case TCPC_STATE_ATTACHED_SNK:
			dev->data_role = PD_DATA_ROLE_UFP;
			dev->power_role = PD_PWR_ROLE_SNK;
			dev->vbus_present = true;
			pe_set_state(dev, PE_SNK_STARTUP);
			break;

		case TCPC_STATE_ORIENTED_DEBUG_ACC_SRC:
		case TCPC_STATE_DEBUG_ACC_SNK:
			break;

		case TCPC_STATE_UNORIENTED_DEBUG_ACC_SRC:
		case TCPC_STATE_AUDIO_ACC:
			// Do nothing.
			break;

		default:
			break;
	}

	return;
}

static void usb_pd_pe_tx_data_msg(unsigned int port, msg_hdr_data_msg_type_t msg_type, tcpc_transmit_t sop_type)
{
	usb_pd_port_t *dev = &pd[port];
	usb_pd_port_config_t *config = usb_pd_pm_get_config(port);
	uint8_t *payload_ptr = &buf[3];
	uint8_t ndo = 0;
	uint32_t *pdo;
	uint8_t pdo_idx;

	if ((msg_type == DATA_MSG_TYPE_SRC_CAPS) ||
		(msg_type == DATA_MSG_TYPE_SNK_CAPS))
	{
		if (msg_type == DATA_MSG_TYPE_SRC_CAPS)
		{
			pdo = dev->src_pdo;
			ndo = config->num_src_pdos;
		}
		else /* SNK CAPS */
		{
			pdo = dev->snk_pdo;
			ndo = config->num_snk_pdos;
		}

		for (pdo_idx = 0; pdo_idx < ndo; pdo_idx++)
		{
			*payload_ptr++ = (uint8_t)(pdo[pdo_idx] & 0xFF);
			*payload_ptr++ = (uint8_t)((pdo[pdo_idx] & 0xFF00) >> 8);
			*payload_ptr++ = (uint8_t)((pdo[pdo_idx] & 0xFF0000) >> 16);
			*payload_ptr++ = (uint8_t)((pdo[pdo_idx] & 0xFF000000) >> 24);
		}
	}
	else if (msg_type == DATA_MSG_TYPE_REQUEST)
	{
		ndo = 1;

		payload_ptr[0] = (uint8_t)(dev->rdo & 0xFF);
		payload_ptr[1] = (uint8_t)((dev->rdo & 0xFF00) >> 8);
		payload_ptr[2] = (uint8_t)((dev->rdo & 0xFF0000) >> 16);
		payload_ptr[3] = (uint8_t)((dev->rdo & 0xFF000000) >> 24);
	}
	else
	{
		CRIT("%s: msg_type %u not supported.\n", __func__, msg_type);
	}

	if (ndo > 0)
	{
		usb_pd_prl_tx_data_msg(port, buf, msg_type, sop_type, ndo);
	}

	return;
}

#ifdef VDM_DISCOVER_IDENTITY
static void usb_pd_pe_tx_vdm_msg(unsigned int port, vdm_cmd_t command)
{
	vdm_hdr_t *vdm = (vdm_hdr_t *)(&buf[3]);
	uint8_t ndo = 0;

	switch (command) {
	case VDM_HDR_CMD_DISCOVER_IDENTITY:
		ndo = 1;

		vdm->CommandType = VDM_HDR_CMD_TYPE_REQ;
		vdm->ObjectPosition = 0;
		vdm->SVID = VDM_HDR_SVID_PD_SID;
		break;

	default:
		CRIT("%s: command %u not supported.\n", __func__, command);
		break;
	}

	if (ndo > 0)
	{
		vdm->Command = command;
		vdm->Reserved1 = 0;
		vdm->Reserved2 = 0;
		vdm->StructedVDMVersion = VDM_HDR_VERSION_10;
		vdm->VDMType = VDM_HDR_TYPE_STRUCTURED_VDM;

		usb_pd_prl_tx_data_msg(port, buf, DATA_MSG_TYPE_VENDOR, TCPC_TX_SOP, ndo);
	}
}
#endif

static void pe_send_accept_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_ACCEPT, TCPC_TX_SOP);
	return;
}

static void pe_send_reject_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_REJECT, TCPC_TX_SOP);
	return;
}

static void pe_send_soft_reset_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_SOFT_RESET, TCPC_TX_SOP);
	return;
}

static void pe_send_not_supported_entry(usb_pd_port_t *dev)
{
	// BQ - add option to send SOP'/SOP".
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_NOT_SUPPORTED, TCPC_TX_SOP);
	return;
}


static void usb_pd_pe_unhandled_rx_msg(usb_pd_port_t *dev)
{
	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		if (*dev->current_state == PE_SNK_TRANSITION_SINK)
		{
			pe_set_state(dev, PE_SNK_HARD_RESET);
		}
		else if (*dev->current_state == PE_SNK_READY)
		{
#if (PD_SPEC_REV == PD_REV20)
			pe_send_reject_entry(dev);
#else
			pe_set_state(dev, PE_SNK_SEND_NOT_SUPPORTED);
#endif
		}
		else if (dev->non_interruptable_ams)
		{
			pe_set_state(dev, PE_SNK_SEND_SOFT_RESET);
		}
	}
	else /* SRC */
	{
		if ((*dev->current_state == PE_SRC_NEGOTIATE_CAPABILITY) ||
			(*dev->current_state == PE_SRC_TRANSITION_SUPPLY))
		{
			pe_set_state(dev, PE_SRC_HARD_RESET);
		}
		else if (*dev->current_state == PE_SRC_READY)
		{
#if (PD_SPEC_REV == PD_REV20)
			pe_send_reject_entry(dev);
#else
			pe_set_state(dev, PE_SRC_SEND_NOT_SUPPORTED);
#endif
		}
		else if (dev->non_interruptable_ams)
		{
			pe_set_state(dev, PE_SRC_SEND_SOFT_RESET);
		}
	}

	return;
}

static void usb_pd_pe_ctrl_msg_rx_handler(usb_pd_port_t *dev)
{
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);

	switch (dev->rx_msg_type)
	{
		case CTRL_MSG_TYPE_GOTO_MIN:
			if (*dev->current_state == PE_SNK_READY)
			{
				if (config->giveback_flag)
				{
					dev->snk_goto_min = true;
					pe_set_state(dev, PE_SNK_TRANSITION_SINK);
				}
			}
			break;

		case CTRL_MSG_TYPE_ACCEPT:
			if (*dev->current_state == PE_SRC_SEND_SOFT_RESET)
			{
				// Stop sender response timer.
				timer_cancel(&dev->timer);
				dev->non_interruptable_ams = false;
				pe_set_state(dev, PE_SRC_SEND_CAPS);
			}
			else if (*dev->current_state == PE_SNK_SEND_SOFT_RESET)
			{
				// Stop sender response timer.
				timer_cancel(&dev->timer);
				dev->non_interruptable_ams = false;
				pe_set_state(dev, PE_SNK_WAIT_FOR_CAPS);
			}
			else if (*dev->current_state == PE_SNK_SELECT_CAPABILITY)
			{
				// Stop sender response timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_SNK_TRANSITION_SINK);
			}
			else if (*dev->current_state == PE_DRS_SEND_SWAP)
			{
				// Stop sender response timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_DRS_CHANGE_ROLE);
			}
			else if (*dev->current_state == PE_PRS_SEND_SWAP)
			{
				// Stop sender response timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_PRS_TRANSITION_TO_OFF);
			}
			else if (*dev->current_state == PE_VCS_SEND_SWAP)
			{
				// Stop sender response timer.
				timer_cancel(&dev->timer);

				if (dev->vconn_source)
				{
					pe_set_state(dev, PE_VCS_WAIT_FOR_VCONN);
				}
				else
				{
					pe_set_state(dev, PE_VCS_TURN_ON_VCONN);
				}
			}
			break;

		case CTRL_MSG_TYPE_WAIT:
		case CTRL_MSG_TYPE_REJECT:
			if (*dev->current_state == PE_SNK_SELECT_CAPABILITY)
			{
				if (dev->explicit_contract)
				{
					if (dev->rx_msg_type == CTRL_MSG_TYPE_WAIT)
					{
						dev->snk_wait = true;
					}

					// Restore previously selected PDO.
					dev->selected_pdo = dev->prev_selected_pdo;

					pe_set_state(dev, PE_SNK_READY);

				}
				else
				{
					pe_set_state(dev, PE_SNK_WAIT_FOR_CAPS);
				}
			}
			else if ((*dev->current_state == PE_DRS_SEND_SWAP) ||
					 (*dev->current_state == PE_PRS_SEND_SWAP))
			{
				if (dev->power_role == PD_PWR_ROLE_SNK)
				{
					pe_set_state(dev, PE_SNK_READY);
				}
				else
				{
					pe_set_state(dev, PE_SRC_READY);
				}
			}
			break;

		case CTRL_MSG_TYPE_PING:
			// Sink may receive ping msgs but should ignore them.
			break;

		case CTRL_MSG_TYPE_PS_RDY:
			if (*dev->current_state == PE_SNK_TRANSITION_SINK)
			{
				// Cancel PSTransition timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_SNK_READY);
			}
			else if (*dev->current_state == PE_PRS_WAIT_SOURCE_ON)
			{
				// Cancel PSSourceOn timer.
				timer_cancel(&dev->timer);

				// VBUS is now present.
				dev->vbus_present = true;

				// Enable VBUS present detection for disconnect.
				tcpm_enable_vbus_detect(dev->port);

				// Enable bleed discharge to ensure timely discharge of
				// VBUS bulk cap upon disconnect.
				tcpm_enable_bleed_discharge(dev->port);

				// Note: Sink VBUS will be enabled when current
				// change advertisement is reported.

				pe_set_state(dev, PE_SNK_STARTUP);
			}
			else if (*dev->current_state == PE_PRS_TRANSITION_TO_OFF)
			{
				if (dev->power_role == PD_PWR_ROLE_SNK)
				{
					// Cancel PSSourceOff timer.
					timer_cancel(&dev->timer);
					pe_set_state(dev, PE_PRS_ASSERT_RP);
				}
			}
			else if (*dev->current_state == PE_VCS_WAIT_FOR_VCONN)
			{
				// Cancel VCONNOn timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_VCS_TURN_OFF_VCONN);
			}
			break;

		case CTRL_MSG_TYPE_GET_SRC_CAP:
			if (*dev->current_state == PE_SRC_READY)
			{
				pe_set_state(dev, PE_SRC_SEND_CAPS);
			}
			else if (*dev->current_state == PE_SNK_READY)
			{
				pe_set_state(dev, PE_DR_SNK_GIVE_SOURCE_CAP);
			}
			break;

		case CTRL_MSG_TYPE_GET_SNK_CAP:
			if (*dev->current_state == PE_SNK_READY)
			{
				pe_set_state(dev, PE_SNK_GIVE_SINK_CAP);
			}
			else if (*dev->current_state == PE_SRC_READY)
			{
				pe_set_state(dev, PE_DR_SRC_GIVE_SINK_CAP);
			}
			break;

		case CTRL_MSG_TYPE_DR_SWAP:
			if ((*dev->current_state == PE_SNK_READY) ||
				(*dev->current_state == PE_SRC_READY))
			{
				if (!dev->modal_operation)
				{
					pe_set_state(dev, PE_DRS_EVALUATE_SWAP);
				}
				else
				{
					// Send hard reset.
					if (*dev->current_state == PE_SNK_READY)
					{
						pe_set_state(dev, PE_SNK_HARD_RESET);
					}
					else
					{
						pe_set_state(dev, PE_SRC_HARD_RESET);
					}
				}
			}
			break;

		case CTRL_MSG_TYPE_PR_SWAP:
			if ((*dev->current_state == PE_SNK_READY) ||
				(*dev->current_state == PE_SRC_READY))
			{
				pe_set_state(dev, PE_PRS_EVALUATE_SWAP);
			}
			break;

		case CTRL_MSG_TYPE_VCONN_SWAP:
			if ((*dev->current_state == PE_SNK_READY) ||
				(*dev->current_state == PE_SRC_READY))
			{
				pe_set_state(dev, PE_VCS_EVALUATE_SWAP);
			}
			break;

		case CTRL_MSG_TYPE_SOFT_RESET:
			if (dev->power_role == PD_PWR_ROLE_SNK)
			{
				pe_set_state(dev, PE_SNK_SOFT_RESET);
			}
			else
			{
				pe_set_state(dev, PE_SRC_SOFT_RESET);
			}
			break;

		case CTRL_MSG_TYPE_NOT_SUPPORTED:
			if (*dev->current_state == PE_SRC_READY)
			{
				pe_set_state(dev, PE_SRC_NOT_SUPPORTED_RECEIVED);
			}
			else if (*dev->current_state == PE_SNK_READY)
			{
				pe_set_state(dev, PE_SNK_NOT_SUPPORTED_RECEIVED);
			}
			break;

		case CTRL_MSG_TYPE_GET_SRC_CAP_EXT:
			break;

		case CTRL_MSG_TYPE_GET_STATUS:
			break;

		case CTRL_MSG_TYPE_FR_SWAP:
			break;

		default:
			CRIT("Invalid ctrl rx_msg_type 0x%x\n!", dev->rx_msg_type);
			break;
	}

	if (!dev->state_change && (dev->rx_msg_type != CTRL_MSG_TYPE_PING))
	{
		usb_pd_pe_unhandled_rx_msg(dev);
	}

	return;
}

#define BIST_CARRIER_MODE_REQUEST  5
#define BIST_TEST_DATA             8

static void usb_pd_pe_data_msg_rx_handler(usb_pd_port_t *dev)
{
	switch (dev->rx_msg_type)
	{
		case DATA_MSG_TYPE_SRC_CAPS:
			if (*dev->current_state == PE_SNK_WAIT_FOR_CAPS)
			{
				// Cancel SinkWaitCap timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_SNK_EVALUATE_CAPABILITY);
			}
			else if (*dev->current_state == PE_SNK_READY)
			{
				pe_set_state(dev, PE_SNK_EVALUATE_CAPABILITY);
			}
			else
			{
				// Hard reset for this particular protocol error.
				pe_set_state(dev, PE_SNK_HARD_RESET);
			}
			break;

		case DATA_MSG_TYPE_REQUEST:
			if ((*dev->current_state == PE_SRC_SEND_CAPS) ||
				(*dev->current_state == PE_SRC_READY))
			{
				// Cancel sender response timer.
				timer_cancel(&dev->timer);
				pe_set_state(dev, PE_SRC_NEGOTIATE_CAPABILITY);
			}
			break;

		case DATA_MSG_TYPE_BIST:
			if ((*dev->current_state == PE_SNK_READY) ||
				(*dev->current_state == PE_SRC_READY))
			{
				// Check we are operating at vSafe5V and power role is not swapped.
				if ((dev->object_position == 1) &&
					(dev->power_role == dev->data_role))
				{
					// Check BIST param, data_obj[31:28].
					if ((dev->rx_msg_buf[3] >> 4) == BIST_CARRIER_MODE_REQUEST)
					{
						pe_set_state(dev, PE_BIST_CARRIER_MODE);
					}
					else if ((dev->rx_msg_buf[3] >> 4) == BIST_TEST_DATA)
					{
						// Set BIST mode.
						tcpm_set_bist_test_mode(dev->port);

						pe_set_state(dev, PE_BIST_TEST_MODE);

						// This test mode shall be ended by a Hard Reset.
					}
				}
			}
			break;

		case DATA_MSG_TYPE_SNK_CAPS:
			if (*dev->current_state == PE_SRC_GET_SINK_CAP)
			{
				// Cancel sender response timer.
				timer_cancel(&dev->timer);
				// Pass sink caps to policy manager. - BQ

				pe_set_state(dev, PE_SRC_READY);
			}
			break;

		case DATA_MSG_TYPE_BATT_STATUS:
			// Response to Get_Battery_Status message.
			break;

		case DATA_MSG_TYPE_ALERT:
			break;

		case DATA_MSG_TYPE_VENDOR:
#ifdef VDM_DISCOVER_IDENTITY
		{
			vdm_hdr_t *vdm = (vdm_hdr_t *)dev->rx_msg_buf;

			if (vdm->Command == VDM_HDR_CMD_DISCOVER_IDENTITY &&
			    vdm->CommandType == VDM_HDR_CMD_TYPE_ACK)
			{
				vdm_id_hdr_t *hdr = (vdm_id_hdr_t *)(&vdm[1]);

				if (hdr->ProductTypeUFP == VDM_ID_HDR_PRODUCT_TYPE_UFP_AMC)
				{
					/* FIXME */
					PRINT("Alternate Mode Adapter detected!\n");
#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
					tusb422_set_dp_notify_node(1);
#endif
				}
				break;
			}
		}
#endif

#if (PD_SPEC_REV == PD_REV30) && !defined(CABLE_PLUG)
			// For USB_PD v3.0, DFP and UFP shall return Not_Supported msg if
			// VDM is not supported.
			if (dev->power_role == PD_PWR_ROLE_SNK)
			{
				pe_set_state(dev, PE_SNK_SEND_NOT_SUPPORTED);
			}
			else
			{
				pe_set_state(dev, PE_SRC_SEND_NOT_SUPPORTED);
			}
#endif
			break;

		default:
			CRIT("Invalid data rx_msg_type 0x%x\n!", dev->rx_msg_type);
			break;
	}

	// Certain Vendor and BIST msgs may be ignored if not supported so don't check here.
	// For PD r2.0, DFP or UFP should ignore the Vendor msg if not supported.
	// Unsupported BIST msgs are always ignored.
	if (!dev->state_change &&
		(dev->rx_msg_type != DATA_MSG_TYPE_BIST) &&
		(dev->rx_msg_type != DATA_MSG_TYPE_VENDOR))
	{
		usb_pd_pe_unhandled_rx_msg(dev);
	}

	return;
}


static void timeout_vbus_5v_stabilize(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	if (*dev->current_state == PE_SRC_STARTUP)
	{
		pe_set_state(dev, PE_SRC_SEND_CAPS);
	}
	else if (*dev->current_state == PE_PRS_SOURCE_ON)
	{
		pe_set_state(dev, PE_PRS_SOURCE_ON_EXIT);
	}

	return;
}

static void timeout_swap_source_start(unsigned int port)
{
	pe_set_state(&pd[port], PE_SRC_SEND_CAPS);
	return;
}

static void timeout_typec_send_source_cap(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	dev->caps_cnt++;

	INFO("CapsCnt = %u\n", dev->caps_cnt);

	if ((dev->caps_cnt > N_CAPS_COUNT) &&
		!dev->pd_connected_since_attach)
	{
		pe_set_state(dev, PE_SRC_DISABLED);
	}
	else
	{
		pe_set_state(&pd[port], PE_SRC_SEND_CAPS);
	}

	return;
}


static void timeout_sender_response(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	CRIT("%s\n", __func__);

	if ((*dev->current_state == PE_SRC_SEND_CAPS) ||
		(*dev->current_state == PE_SRC_SEND_SOFT_RESET))
	{
		pe_set_state(dev, PE_SRC_HARD_RESET);
	}
	else if (*dev->current_state == PE_SRC_GET_SINK_CAP)
	{
		pe_set_state(dev, PE_SRC_READY);
	}
	else if ((*dev->current_state == PE_SNK_SELECT_CAPABILITY) ||
			 (*dev->current_state == PE_SNK_SEND_SOFT_RESET))
	{
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
	else if ((*dev->current_state == PE_DRS_SEND_SWAP) ||
			 (*dev->current_state == PE_PRS_SEND_SWAP) ||
			 (*dev->current_state == PE_VCS_SEND_SWAP))
	{
		if (dev->power_role == PD_PWR_ROLE_SNK)
		{
			pe_set_state(dev, PE_SNK_READY);
		}
		else
		{
			pe_set_state(dev, PE_SRC_READY);
		}
	}
	else
	{
		CRIT("Error: %s - state %s unhandled!\n", __func__, pdstate2string[*dev->current_state]);
	}

	return;
}

static void timeout_ps_hard_reset(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	if ((*dev->current_state == PE_SRC_HARD_RESET) ||
		(*dev->current_state == PE_SRC_HARD_RESET_RECEIVED))
	{
		pe_set_state(dev, PE_SRC_TRANSITION_TO_DEFAULT);
	}

	return;
}

static void pe_src_startup_entry(usb_pd_port_t *dev)
{
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	usb_pd_prl_reset(dev->port);
	dev->non_interruptable_ams = false;
	dev->power_role_swap_in_progress = false;

	// Reset source caps count.
	dev->caps_cnt = 0;

	if (tcpm_is_vconn_enabled(dev->port))
	{
		// Enable PD receive for SOP/SOP'.
		tcpm_enable_pd_receive(dev->port, true, false);
	}
	else
	{
		// Enable PD receive for SOP.
		tcpm_enable_pd_receive(dev->port, false, false);
	}

	if (dev->swap_source_start)
	{
		// Wait tSwapSourceStart before transitioning to SRC_SEND_CAPS.
		timer_start(&dev->timer, T_SWAP_SOURCE_START_MS, timeout_swap_source_start);
		dev->swap_source_start = false;
	}
	else
	{
		// Wait for VBUS to stablize before transitioning to SRC_SEND_CAPS.
		timer_start(&dev->timer, T_VBUS_5V_STABLIZE_MS, timeout_vbus_5v_stabilize);
	}

	return;
}

static void pe_src_discovery_entry(usb_pd_port_t *dev)
{
	// Start SourceCapabilityTimer.
	timer_start(&dev->timer, T_TYPEC_SEND_SOURCE_CAP_MS, timeout_typec_send_source_cap);

	return;
}

static void pe_src_transition_to_default_entry(usb_pd_port_t *dev)
{
	// Disable VCONN.
	tcpm_set_vconn_enable(dev->port, false);

	// Remove Rp from VCONN pin.
	tcpm_vconn_pin_rp_control(dev->port, false);

	// Disable VBUS.
	tcpm_src_vbus_disable(dev->port);

	// Force VBUS discharge.
	tcpm_force_discharge(dev->port, VSTOP_DISCHRG);

	dev->explicit_contract = false;

	// Wait until VBUS drops to vSafe0V before checking data role and starting source recover timer.
	tcpm_set_voltage_alarm_lo(dev->port, VSAFE0V_MAX);

	return;
}

static void pe_src_transition_to_default_exit(usb_pd_port_t *dev)
{
	// Restore Rp on VCONN pin and enable VCONN if Ra is detected.
	tcpm_vconn_pin_rp_control(dev->port, true);

	// Restore vSafe5V.
	tcpm_src_vbus_5v_enable(dev->port);

	// After a Hard Reset, the sink must respond to SRC_CAPS within tNoResponse.
	timer_start_no_response(dev);

	// Wait for VBUS to reach vSafe5V before going to PE_SRC_STARTUP state.
	tcpm_set_voltage_alarm_hi(dev->port, VSAFE5V_MIN);

	return;
}

static void timeout_src_recover(unsigned int port)
{
	pe_src_transition_to_default_exit(&pd[port]);
	return;
}

static void pe_src_send_caps_entry(usb_pd_port_t *dev)
{
	usb_pd_pe_tx_data_msg(dev->port, DATA_MSG_TYPE_SRC_CAPS, TCPC_TX_SOP);
	return;
}


static void pe_src_negotiate_capability_entry(usb_pd_port_t *dev)
{
	uint16_t operating_current;
	uint16_t src_max_current;
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);
	uint32_t rdo = get_data_object(dev->rx_msg_buf);

	// BQ - Battery RDO not supported.

	dev->object_position = (rdo >> 28) & 0x07;
	operating_current = (rdo >> 10) & 0x3FF;

	if ((dev->object_position > 0) &&
		(dev->object_position <= config->num_src_pdos) &&
		(dev->object_position <= PD_MAX_PDO_NUM))
	{
		src_max_current = dev->src_pdo[dev->object_position - 1] & 0x3FF;

		DEBUG("PE_SRC_NEG_CAP: ObjPos = %u, req = %u mA, avail = %u mA\n",
			  dev->object_position, operating_current * 10, src_max_current * 10);

		if (operating_current <= src_max_current)
		{
			pe_set_state(dev, PE_SRC_TRANSITION_SUPPLY);
		}
		else
		{
			// Request cannot be met.
			pe_set_state(dev, PE_SRC_CAPABILITY_RESPONSE);
		}
	}
	else
	{
		DEBUG("PE_SRC_NEG_CAP: ObjPos = %u is invalid!\n", dev->object_position);

		// Request cannot be met.
		pe_set_state(dev, PE_SRC_CAPABILITY_RESPONSE);
	}

	return;
}


static void pe_src_transition_supply_entry(usb_pd_port_t *dev)
{
	if (dev->request_goto_min)
	{
		dev->request_goto_min = false;

		// Send GotoMin message.
		usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_GOTO_MIN, TCPC_TX_SOP);
	}
	else
	{
		// Send Accept message.
		usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_ACCEPT, TCPC_TX_SOP);
	}

	return;
}

static void pe_src_transition_supply_exit(usb_pd_port_t *dev)
{
	// Send PS_RDY.
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_PS_RDY, TCPC_TX_SOP);
	return;
}


#define V_SRC_VALID_MV  500  /* +/- 500 mV */

static uint16_t pd_power_enable_non_default_src_vbus(usb_pd_port_t *dev)
{
	uint16_t v_threshold;
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);

	// If supporting more than one voltage, check PDO to determine voltage.
	tcpm_src_vbus_hi_volt_enable(dev->port);

	// Get PDO min voltage in 50mV units.
	v_threshold = PDO_MIN_VOLTAGE(dev->src_pdo[dev->object_position - 1]);

	// If fixed PDO, multiply by 0.95 to get min vSrcNew.
	if (config->src_caps[dev->object_position - 1].SupplyType == SUPPLY_TYPE_FIXED)
	{
		// Note: If voltage is > 34V, this multiplication will overflow.
		v_threshold *= 95;
		v_threshold /= 100;
	}

	// Subtract vSrcValid to get min of valid range for new voltage.
	v_threshold -= (V_SRC_VALID_MV / 50);

	// Divide by two to convert to 25mV units.
	v_threshold = v_threshold << 1;

	return v_threshold;
}


static void pd_transition_power_supply(usb_pd_port_t *dev)
{
	uint16_t v_threshold;
	uint16_t v_src_new_max;
	uint16_t present_voltage;
	uint16_t requested_voltage;
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);

	if (config->num_src_pdos == 1)
	{
		// Only 5V is supported so no voltage transition required.
		pe_set_state(dev, PE_SRC_TRANSITION_SUPPLY_EXIT);
		return;
	}

	// Get present VBUS voltage and convert to millivolts.
	present_voltage = tcpm_get_vbus_voltage(dev->port) * 25;

	// Get requested VBUS voltage in millivolts.
	requested_voltage = PDO_VOLT_TO_MV(PDO_MIN_VOLTAGE(dev->src_pdo[dev->object_position - 1]));

	CRIT("%u mV requested, VBUS is %u mV\n", requested_voltage, present_voltage);

	v_src_new_max = requested_voltage;
	// If fixed PDO, multiply by 1.05 to get max vSrcNew.
	if (config->src_caps[dev->object_position - 1].SupplyType == SUPPLY_TYPE_FIXED)
	{
		v_src_new_max += (v_src_new_max / 100) * 5;
	}

	// Add vSrcValid to get max vSrcNew.
	v_src_new_max += V_SRC_VALID_MV;

	INFO("v_src_new_max = %u mV\n", v_src_new_max);

	if (v_src_new_max < present_voltage)
	{
		tcpm_src_vbus_disable(dev->port);

		v_threshold = MV_TO_25MV(requested_voltage);

		tcpm_set_voltage_alarm_lo(dev->port, v_threshold);

//        timer_start(&dev->timer, T_SRC_SETTLE_MS, timeout_src_settle);

		// Force VBUS discharge.
		tcpm_force_discharge(dev->port, v_threshold);
	}
	else
	{
		if (dev->object_position == 1)
		{
			// Default 5V. No voltage transition required.
			pe_set_state(dev, PE_SRC_TRANSITION_SUPPLY_EXIT);
		}
		else
		{
			v_threshold = pd_power_enable_non_default_src_vbus(dev);

			// Use Hi voltage alarm to determine when power supply is ready.
			tcpm_set_voltage_alarm_hi(dev->port, v_threshold);
		}
	}

	return;
}


static void timeout_src_transition(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	if (*dev->current_state == PE_SRC_TRANSITION_SUPPLY)
	{
		pd_transition_power_supply(&pd[port]);
	}
	else if (*dev->current_state == PE_PRS_TRANSITION_TO_OFF)
	{
		// Disable source VBUS.
		tcpm_src_vbus_disable(dev->port);
	}
	return;
}


static void pe_src_capability_response_entry(usb_pd_port_t *dev)
{
	// Send Reject.
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_REJECT, TCPC_TX_SOP);
#ifdef CONFIG_LGE_USB_TYPE_C
	pe_set_state(dev, PE_SNK_HARD_RESET);
#endif

	// BQ - Send Wait not supported.
}


static void pe_src_ready_entry(usb_pd_port_t *dev)
{
	// Notify PRL of end of AMS.
	dev->non_interruptable_ams = false;

	dev->pd_connected_since_attach = true;
	dev->explicit_contract = true;

	// Set Rp value to 3.0A for collision avoidance.
	tcpm_set_rp_value(dev->port, RP_HIGH_CURRENT);

#ifdef CONFIG_TUSB422_PAL
	usb_pd_pal_notify_pd_state(dev->port, PE_SRC_READY);
#endif

	if (tcpm_is_vconn_enabled(dev->port))
	{
		// Enable PD receive for SOP/SOP'/SOP".
		tcpm_enable_pd_receive(dev->port, true, true);

		// If VCONN source, start DiscoveryIdentity timer and negotiate PD with cable plug. - BQ
		//T_DISCOVER_IDENTITY_MS
	}


#ifdef VDM_DISCOVER_IDENTITY
	if (dev->discover_identity_cnt == 0)
	{
		// Send Discover Identity.
		usb_pd_pe_tx_vdm_msg(dev->port, VDM_HDR_CMD_DISCOVER_IDENTITY);
		dev->discover_identity_cnt++;
	}
#endif

	return;
}

static void pe_src_hard_reset_received_entry(usb_pd_port_t *dev)
{
	timer_start(&dev->timer, T_PS_HARD_RESET_MS, timeout_ps_hard_reset);
	return;
}

static void pe_src_hard_reset_entry(usb_pd_port_t *dev)
{
	dev->hard_reset_cnt++;
	tcpm_transmit(dev->port, NULL, TCPC_TX_HARD_RESET);
	timer_start(&dev->timer, T_PS_HARD_RESET_MS, timeout_ps_hard_reset);
	return;
}

static void pe_src_get_sink_cap_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_GET_SNK_CAP, TCPC_TX_SOP);
	return;
}

static void pe_src_disabled_entry(usb_pd_port_t *dev)
{
	tcpm_disable_pd_receive(dev->port);
	return;
}

static void pe_src_not_supported_received_entry(usb_pd_port_t *dev)
{
	// Inform policy manager.

	pe_set_state(dev, PE_SRC_READY);
	return;
}

/* can only be entered from PE_SRC_READY state */
static void pe_src_ping_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_PING, TCPC_TX_SOP);
	return;
}

static void pe_src_wait_new_caps_entry(usb_pd_port_t *dev)
{
	// Do nothing. Wait for policy manager to provide new caps.
}

static void pe_drs_send_swap_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_DR_SWAP, TCPC_TX_SOP);
	return;
}

static void pe_drs_evaluate_swap_entry(usb_pd_port_t *dev)
{
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);

	dev->non_interruptable_ams = true;

	if (dev->data_role == PD_DATA_ROLE_UFP)
	{
		if (config->auto_accept_swap_to_dfp)
		{
			pe_set_state(dev, PE_DRS_ACCEPT_SWAP);
		}
		else
		{
			// Ask platform policy manager or Reject.
			pe_set_state(dev, PE_DRS_REJECT_SWAP);
		}
	}
	else /* DFP */
	{
		if (config->auto_accept_swap_to_ufp)
		{
			pe_set_state(dev, PE_DRS_ACCEPT_SWAP);
		}
		else
		{
			// Ask platform policy manager or Reject.
			pe_set_state(dev, PE_DRS_REJECT_SWAP);
		}
	}

	return;
}

static void pe_drs_change_role_entry(usb_pd_port_t *dev)
{
	if (dev->data_role == PD_DATA_ROLE_UFP)
	{
#ifdef CONFIG_TUSB422_PAL
		usb_pd_pal_data_role_swap(dev->port, PD_DATA_ROLE_DFP);
#endif
		dev->data_role = PD_DATA_ROLE_DFP;
	}
	else /* DFP */
	{
#ifdef CONFIG_TUSB422_PAL
		usb_pd_pal_data_role_swap(dev->port, PD_DATA_ROLE_UFP);
#endif
		dev->data_role = PD_DATA_ROLE_UFP;
	}

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	tcpm_update_msg_header_info(dev->port, dev->data_role, dev->power_role);

	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		pe_set_state(dev, PE_SNK_READY);
	}
	else /* SRC */
	{
		pe_set_state(dev, PE_SRC_READY);
	}

	return;
}

static void pe_prs_send_swap_entry(usb_pd_port_t *dev)
{
	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		// Disable AutoDischargeDisconnect per TCPC spec.
		tcpm_set_autodischarge_disconnect(dev->port, false);

		// Disable sink VBUS per TCPC spec.
		tcpm_snk_vbus_disable(dev->port);

		// Disable VBUS present detection.
		tcpm_disable_vbus_detect(dev->port);
	}

	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_PR_SWAP, TCPC_TX_SOP);
	return;
}

static void pe_prs_evaluate_swap_entry(usb_pd_port_t *dev)
{
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);
	tcpc_device_t *tc_dev = tcpm_get_device(dev->port);

	dev->non_interruptable_ams = true;

	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		if (tc_dev->silicon_revision == 0)
		{
			// PG1.0 cannot support SNK->SRC swap because PD will be disabled.
			pe_set_state(dev, PE_PRS_REJECT_SWAP);
		}
		else
		{
			if (config->auto_accept_swap_to_source)
			{
				pe_set_state(dev, PE_PRS_ACCEPT_SWAP);
			}
			else
			{
				// Ask platform policy manager or Reject.
				pe_set_state(dev, PE_PRS_REJECT_SWAP);
			}
		}

		if (*dev->current_state == PE_PRS_ACCEPT_SWAP)
		{
			// Disable AutoDischargeDisconnect per TCPC spec.
			tcpm_set_autodischarge_disconnect(dev->port, false);

			// Disable VBUS present detection.
			tcpm_disable_vbus_detect(dev->port);
		}
	}
	else /* SRC */
	{
		// Only auto accept swap to sink if not externally powered.
		if (/*!config->externally_powered &&*/ config->auto_accept_swap_to_sink)
		{
			pe_set_state(dev, PE_PRS_ACCEPT_SWAP);
		}
		else
		{
			// Ask platform policy manager or Reject.
			pe_set_state(dev, PE_PRS_REJECT_SWAP);
		}
	}

	return;
}

static void timeout_ps_source(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	dev->power_role_swap_in_progress = false;
	pe_set_state(dev, PE_ERROR_RECOVERY);
	return;
}

static void pe_prs_transition_to_off_entry(usb_pd_port_t *dev)
{
	dev->power_role_swap_in_progress = true;

	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		// Start PSSourceOff timer.
		timer_start(&dev->timer, T_PS_SOURCE_OFF_MS, timeout_ps_source);

		// Disable sink VBUS.
		tcpm_snk_vbus_disable(dev->port);
	}
	else /* SRC */
	{
		// Accept msg was ACKed, transition from Attached.SRC to Attached.SNK according to Type-C spec.
		// This will prevent transistion to Unattached state when Rd-Rd condition is detected.
		tcpm_handle_power_role_swap(dev->port);

		// Wait tSrcTransition before disabling VBUS.
		timer_start(&dev->timer, T_SRC_TRANSITION_MS, timeout_src_transition);

		// Wait until VBUS drops to vSafe0V before changing CC to Rd.
		tcpm_set_voltage_alarm_lo(dev->port, VSAFE0V_MAX);
	}

	dev->explicit_contract = false;

	return;
}

static void pe_prs_assert_rd_entry(usb_pd_port_t *dev)
{
	// Rp -> Rd.
	tcpm_cc_pin_control(dev->port, ROLE_SNK);

	pe_set_state(dev, PE_PRS_WAIT_SOURCE_ON);

	return;
}

static void pe_prs_wait_source_on_entry(usb_pd_port_t *dev)
{
	// Set new power role to sink.
	dev->power_role = PD_PWR_ROLE_SNK;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	tcpm_update_msg_header_info(dev->port, dev->data_role, dev->power_role);

	// Send PS_RDY.
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_PS_RDY, TCPC_TX_SOP);

	// Start PSSourceOn timer.
	timer_start(&dev->timer, T_PS_SOURCE_ON_MS, timeout_ps_source);

	return;
}

static void pe_prs_assert_rp_entry(usb_pd_port_t *dev)
{
	// PS_RDY was received from original source, transition from Attached.SNK
	// to Attached.SRC according to Type-C spec.
	tcpm_handle_power_role_swap(dev->port);

	// Rd -> Rp.
	tcpm_cc_pin_control(dev->port, ROLE_SRC);

	pe_set_state(dev, PE_PRS_SOURCE_ON);

	return;
}

static void pe_prs_source_on_entry(usb_pd_port_t *dev)
{
	// Enable source VBUS.
	tcpm_src_vbus_5v_enable(dev->port);

	// Wait until VBUS rises to vSafe5V before sending PS_RDY.
	tcpm_set_voltage_alarm_hi(dev->port, VSAFE5V_MIN);

	return;
}

static void pe_prs_source_on_exit(usb_pd_port_t *dev)
{
	// Set new power role to source.
	dev->power_role = PD_PWR_ROLE_SRC;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	tcpm_update_msg_header_info(dev->port, dev->data_role, dev->power_role);

	// Send PS_RDY.
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_PS_RDY, TCPC_TX_SOP);

	dev->swap_source_start = true;

	return;
}

/* After VCONN swap, VCONN Source must issue SOP' or SOP" Soft Reset to cable plug before communication */
static void pe_vcs_send_swap_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_VCONN_SWAP, TCPC_TX_SOP);
	return;
}

static void pe_vcs_evaluate_swap_entry(usb_pd_port_t *dev)
{
	usb_pd_port_config_t *config = usb_pd_pm_get_config(dev->port);

	dev->non_interruptable_ams = true;

	if (dev->vconn_source || config->auto_accept_vconn_swap)
	{
		// Current VCONN source must accept swap request.
		pe_set_state(dev, PE_VCS_ACCEPT_SWAP);
	}
	else
	{
		pe_set_state(dev, PE_VCS_REJECT_SWAP);
	}

	return;
}

static void timeout_vconn_source_on(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];
	tcpc_device_t *tc_dev;

	// Hard reset based on CC state.
	tc_dev = tcpm_get_device(port);

	if (tc_dev->cc_status & CC_STATUS_CONNECT_RESULT)
	{
		// Rd asserted (Consumer/Provider)
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
	else
	{
		// Rp asserted (Provider/Consumer)
		pe_set_state(dev, PE_SRC_HARD_RESET);
	}

	return;
}


static void pe_vcs_wait_for_vconn_entry(usb_pd_port_t *dev)
{
	// Start VCONNOn timer.
	timer_start(&dev->timer, T_VCONN_SOURCE_ON_MS, timeout_vconn_source_on);
	return;
}

static void pe_vcs_turn_off_vconn_entry(usb_pd_port_t *dev)
{
	// Disable VCONN.
	tcpm_set_vconn_enable(dev->port, false);
	dev->vconn_source = false;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	// Disable PD receive for SOP'/SOP".
	tcpm_enable_pd_receive(dev->port, false, false);

	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		pe_set_state(dev, PE_SNK_READY);
	}
	else
	{
		pe_set_state(dev, PE_SRC_READY);
	}

	return;
}

static void timeout_vconn_enable(unsigned int port)
{
	pe_set_state(&pd[port], PE_VCS_SEND_PS_RDY);
	return;
}

static void pe_vcs_turn_on_vconn_entry(usb_pd_port_t *dev)
{
	// Enable VCONN.
	tcpm_set_vconn_enable(dev->port, true);
	dev->vconn_source = true;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	// Short delay for VCONN stabilization before sending PS_RDY.
	timer_start(&dev->timer, 10, timeout_vconn_enable);

	return;
}

static void pe_vcs_send_ps_rdy_entry(usb_pd_port_t *dev)
{
	// Send PS_RDY.
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_PS_RDY, TCPC_TX_SOP);
	return;
}


static void timeout_sink_wait_cap(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	DEBUG("%s: HR cnt = %u.\n", __func__, dev->hard_reset_cnt);

	if (dev->hard_reset_cnt <= N_HARD_RESET_COUNT)
	{
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}

	return;
}

static void timeout_ps_transition(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	if (dev->hard_reset_cnt <= N_HARD_RESET_COUNT)
	{
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}

	return;
}

static void timeout_sink_request(unsigned int port)
{
	pe_set_state(&pd[port], PE_SNK_SELECT_CAPABILITY);
	return;
}

static void pe_snk_startup_entry(usb_pd_port_t *dev)
{
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(tusb422_dual_role_phy);
#endif

	usb_pd_prl_reset(dev->port);
	dev->non_interruptable_ams = false;
	dev->power_role_swap_in_progress = false;

	// Enable PD receive for SOP.
	tcpm_enable_pd_receive(dev->port, false, false);

	pe_set_state(dev, PE_SNK_DISCOVERY);

	return;
}

static void pe_snk_discovery_entry(usb_pd_port_t *dev)
{
	if (dev->no_response_timed_out &&
		dev->pd_connected_since_attach &&
		(dev->hard_reset_cnt > N_HARD_RESET_COUNT))
	{
		pe_set_state(dev, PE_ERROR_RECOVERY);
	}
	else
	{
		if (dev->vbus_present)
		{
			pe_set_state(dev, PE_SNK_WAIT_FOR_CAPS);
		}
	}

	return;
}

static void pe_snk_wait_for_caps_entry(usb_pd_port_t *dev)
{
	// Disable sink disconnect threshold.
	tcpm_set_sink_disconnect_threshold(dev->port, 0);

	// Re-enable AutoDischargeDisconnect.
	// (May have been disabled due to hard reset or power role swap)
//	tcpm_set_autodischarge_disconnect(dev->port, true);  // BQ - removed so we don't disconnect after a hard reset.

	timer_start(&dev->timer, T_TYPEC_SINK_WAIT_CAP_MS, timeout_sink_wait_cap);
	return;
}

static void pe_snk_evaluate_capability_entry(usb_pd_port_t *dev)
{
	// Stop NoResponseTimer and reset HardResetCounter to zero.
	timer_cancel_no_response(dev);
	dev->hard_reset_cnt = 0;

	dev->non_interruptable_ams = true;

	if (dev->explicit_contract)
	{
		// Save current PDO in case request gets Reject or Wait response.
		dev->prev_selected_pdo = dev->selected_pdo;
	}

	// Ask policy manager to evaluate options based on supplied capabilities.
	usb_pd_pm_evaluate_src_caps(dev->port);

	pe_set_state(dev, PE_SNK_SELECT_CAPABILITY);

	return;
}

static void pe_snk_select_capability_entry(usb_pd_port_t *dev)
{
	if (dev->explicit_contract)
	{
		if (PDO_MIN_VOLTAGE(dev->selected_pdo) < PDO_MIN_VOLTAGE(dev->prev_selected_pdo))
		{
			// Disable sink disconnect threshold.
			tcpm_set_sink_disconnect_threshold(dev->port, 0);
		}
	}

	// Clear sink Wait and GotoMin flags.
	dev->snk_wait = false;
	dev->snk_goto_min = false;

	// Build RDO based on policy manager response.
	build_rdo(dev->port);

	// Send RDO.
	usb_pd_pe_tx_data_msg(dev->port, DATA_MSG_TYPE_REQUEST, TCPC_TX_SOP);

	return;
}

static void pe_snk_transition_sink_entry(usb_pd_port_t *dev)
{
#ifdef CONFIG_TUSB422_PAL
	uint16_t rdo_operational_curr_or_pwr;
#endif

	timer_start(&dev->timer, T_PS_TRANSITION_MS, timeout_ps_transition);

	// Request policy manager to transition to new power level and wait for PS_RDY from source.
#ifdef CONFIG_TUSB422_PAL

	if (dev->snk_goto_min)
	{
		rdo_operational_curr_or_pwr = RDO_MIN_OPERATIONAL_CURRENT_OR_POWER(dev->rdo);
	}
	else
	{
		rdo_operational_curr_or_pwr = RDO_OPERATIONAL_CURRENT_OR_POWER(dev->rdo);
	}

	if (PDO_SUPPLY_TYPE(dev->selected_pdo) == SUPPLY_TYPE_FIXED)
	{
		usb_pd_pal_sink_vbus(dev->port, true,
							 PDO_VOLT_TO_MV(PDO_MIN_VOLTAGE(dev->selected_pdo)),
							 PDO_CURR_TO_MA(rdo_operational_curr_or_pwr));
	}
	else if (PDO_SUPPLY_TYPE(dev->selected_pdo) == SUPPLY_TYPE_VARIABLE)
	{
		usb_pd_pal_sink_vbus_vari(dev->port,
								  PDO_VOLT_TO_MV(PDO_MIN_VOLTAGE(dev->selected_pdo)),
								  PDO_VOLT_TO_MV(PDO_MAX_VOLTAGE(dev->selected_pdo)),
								  PDO_CURR_TO_MA(rdo_operational_curr_or_pwr));
	}
	else /* Battery source */
	{
		usb_pd_pal_sink_vbus_batt(dev->port,
								  PDO_VOLT_TO_MV(PDO_MIN_VOLTAGE(dev->selected_pdo)),
								  PDO_VOLT_TO_MV(PDO_MAX_VOLTAGE(dev->selected_pdo)),
								  PDO_PWR_TO_MW(rdo_operational_curr_or_pwr));
	}
#endif

	return;
}

#define DEFAULT_5V  (5000 / 25)  /* 25mV LSB */

static void pe_snk_ready_entry(usb_pd_port_t *dev)
{
	uint16_t min_voltage;
	uint16_t threshold;

	// Notify PRL of end of AMS.
	dev->non_interruptable_ams = false;

	dev->pd_connected_since_attach = true;
	dev->explicit_contract = true;

	if (dev->snk_wait)
	{
		// Start SinkRequest timer.
		timer_start(&dev->timer, T_SINK_REQUEST_MS, timeout_sink_request);
	}

	// Get PDO min voltage (multiply by 2 to convert to 25mV LSB) so we can set discharge voltage threshold.
	min_voltage = PDO_MIN_VOLTAGE(dev->selected_pdo) << 1;

	if (min_voltage == DEFAULT_5V)
	{
#ifdef CONFIG_LGE_USB_TYPE_C
		// Set Sink Disconnect Threshold to VDISCON_MAX.
		threshold = VDISCON_MAX;
#else
		// Set Sink Disconnect Threshold to zero to disable.
		threshold = 0;
#endif
	}
	else
	{
		// Set Sink Disconnect Threshold to 80% of min voltage.
		threshold = (min_voltage * 8) / 10;
		DEBUG("SNK VBUS Disconn Thres = %u mV.\n", threshold * 25);
	}

	tcpm_set_sink_disconnect_threshold(dev->port, threshold);

	if (tcpm_is_vconn_enabled(dev->port))
	{
		// Enable PD receive for SOP/SOP'/SOP".
		tcpm_enable_pd_receive(dev->port, true, true);

		if (dev->data_role == PD_DATA_ROLE_DFP)
		{
			// On entry to the PE_SNK_Ready state if this is a DFP which needs to establish communication
			// with a Cable Plug, then the Policy Engine shall initialize and run the DiscoverIdentityTimer - BQ
			// T_DISCOVER_IDENTITY_MS
		}
	}

#ifdef CONFIG_TUSB422_PAL
	usb_pd_pal_notify_pd_state(dev->port, PE_SNK_READY);
#endif

	return;
}

static void pe_snk_hard_reset_entry(usb_pd_port_t *dev)
{
	// Disable AutoDischargeDisconnect per TCPC spec.
	tcpm_set_autodischarge_disconnect(dev->port, false);

	// Disable sink VBUS per TCPC spec. Sink must draw < 2.5mA.
//	tcpm_snk_vbus_disable(dev->port);  // BQ - Remove this to fix phone reboot issue with PD charger.

	dev->hard_reset_cnt++;
	tcpm_transmit(dev->port, NULL, TCPC_TX_HARD_RESET);

	return;
}

static void pe_snk_not_supported_received_entry(usb_pd_port_t *dev)
{
	// Notify policy manager.

	pe_set_state(dev, PE_SNK_READY);
	return;
}

static void pe_snk_transition_to_default_entry(usb_pd_port_t *dev)
{
#ifdef CONFIG_TUSB422_PAL
	tcpc_device_t* tc_dev = tcpm_get_device(dev->port);;
#endif

	// Notify policy manager sink shall transition to default.
	tcpm_hal_vbus_enable(dev->port, VBUS_SNK);

	// Request policy manager to set data role to UFP.
	if (dev->data_role != PD_DATA_ROLE_UFP)
	{
		dev->data_role = PD_DATA_ROLE_UFP;

		tcpm_update_msg_header_info(dev->port, dev->data_role, dev->power_role);

#ifdef CONFIG_TUSB422_PAL
		// BQ - should we notify in PE_SNK_Wait_for_Capabilities when VBUS is present instead?
		usb_pd_pal_notify_connect_state(dev->port, tc_dev->state, tc_dev->plug_polarity);
//        usb_pd_pal_data_role_swap(dev->port, PD_DATA_ROLE_UFP);
#endif
	}

	// Disable VCONN.
	tcpm_set_vconn_enable(dev->port, false);
	dev->explicit_contract = false;

	// After a Hard Reset, the sink must receive SRC_CAPS within tNoResponse.
	timer_start_no_response(dev);

	// During a Hard Reset the Source voltage will transition to vSafe0V and then
	// transition to vSafe5V. Sinks need to ensure that VBUS present is not indicated
	// until after the Source has completed the Hard Reset process by detecting both of these transitions.
	dev->vbus_present = false;
	tcpm_set_voltage_alarm_lo(dev->port, VSAFE0V_MAX);

	// Assume sink has reached default level and transition to SNK_STARTUP.
	pe_set_state(dev, PE_SNK_STARTUP);

	return;
}

static void pe_snk_give_sink_cap_entry(usb_pd_port_t *dev)
{
	// Request Sink Caps from policy manager.

	usb_pd_pe_tx_data_msg(dev->port, DATA_MSG_TYPE_SNK_CAPS, TCPC_TX_SOP);
	return;
}

static void pe_snk_get_source_cap_entry(usb_pd_port_t *dev)
{
	usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_GET_SRC_CAP, TCPC_TX_SOP);
	return;
}

static void timeout_bist_cont_mode(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	if (dev->power_role == PD_PWR_ROLE_SNK)
	{
		pe_set_state(dev, PE_SNK_TRANSITION_TO_DEFAULT);
	}
	else
	{
		pe_set_state(dev, PE_SRC_TRANSITION_TO_DEFAULT);
	}

	return;
}


static void pe_bist_carrier_mode_entry(usb_pd_port_t *dev)
{
	// Start BIST transmit.  TCPC HW will automatically stop transmit after tBISTContMode.
	tcpm_transmit(dev->port, NULL, TCPC_TX_BIST_MODE2);

	timer_start(&dev->timer, T_BIST_CONT_MODE_MS, timeout_bist_cont_mode);

	return;
}

static void pe_bist_test_mode_entry(usb_pd_port_t *dev)
{
	return;
}

static void pe_error_recovery_entry(usb_pd_port_t *dev)
{
	tcpm_execute_error_recovery(dev->port);
	return;
}

static void pe_dr_src_give_sink_caps_entry(usb_pd_port_t *dev)
{
	tcpc_device_t *tc_dev = tcpm_get_device(dev->port);

	if (tc_dev->role == ROLE_DRP)
	{
		usb_pd_pe_tx_data_msg(dev->port, DATA_MSG_TYPE_SNK_CAPS, TCPC_TX_SOP);
	}
	else
	{
		usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_REJECT, TCPC_TX_SOP);
	}

	return;
}


static void pe_dr_snk_give_source_caps_entry(usb_pd_port_t *dev)
{
	tcpc_device_t *tc_dev = tcpm_get_device(dev->port);

	if (tc_dev->role == ROLE_DRP)
	{
		usb_pd_pe_tx_data_msg(dev->port, DATA_MSG_TYPE_SRC_CAPS, TCPC_TX_SOP);
	}
	else
	{
		usb_pd_prl_tx_ctrl_msg(dev->port, buf, CTRL_MSG_TYPE_REJECT, TCPC_TX_SOP);
	}

	return;
}

static void pe_unattached_entry(usb_pd_port_t *dev)
{
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	tcpc_device_t *tcpc_dev = tcpm_get_device(dev->port);
#endif

	dev->swap_source_start = false;
	dev->pd_connected_since_attach = false;
	dev->explicit_contract = false;
	dev->no_response_timed_out = false;
	dev->vbus_present = false;
	dev->power_role_swap_in_progress = false;
	dev->modal_operation = false;

	dev->request_goto_min = false;
	dev->hard_reset_cnt = 0;
	dev->vconn_source = false;

	dev->high_pwr_cable = false;

	dev->discover_identity_cnt = 0;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	if (!(tcpc_dev->flags & TC_FLAGS_TEMP_ROLE))
	{
		dual_role_instance_changed(tusb422_dual_role_phy);
	}
#endif

	return;
}


static void pe_dummy_state_entry(usb_pd_port_t *dev)
{
	INFO("%s\n", __func__);
	return;
}

typedef void (*state_entry_fptr)(usb_pd_port_t *dev);

static const state_entry_fptr pe_state_entry[PE_NUM_STATES] =
{
	pe_unattached_entry,				  /* PE_UNATTACHED                 */
	pe_src_startup_entry,				  /* PE_SRC_STARTUP                */
	pe_src_discovery_entry,				  /* PE_SRC_DISCOVERY              */
	pe_src_send_caps_entry,				  /* PE_SRC_SEND_CAPS              */
	pe_src_negotiate_capability_entry,	  /* PE_SRC_NEGOTIATE_CAPABILITY   */
	pe_src_transition_supply_entry,		  /* PE_SRC_TRANSITION_SUPPLY      */
	pe_src_transition_supply_exit,		  /* PE_SRC_TRANSITION_SUPPLY_EXIT */
	pe_src_ready_entry,					  /* PE_SRC_READY                  */
	pe_src_disabled_entry,				  /* PE_SRC_DISABLED               */
	pe_src_capability_response_entry,	  /* PE_SRC_CAPABILITY_RESPONSE    */
	pe_src_hard_reset_entry,			  /* PE_SRC_HARD_RESET             */
	pe_src_hard_reset_received_entry,	  /* PE_SRC_HARD_RESET_RECEIVED    */
	pe_src_transition_to_default_entry,	  /* PE_SRC_TRANSITION_TO_DEFAULT  */
	pe_src_get_sink_cap_entry,			  /* PE_SRC_GET_SINK_CAP           */
	pe_src_wait_new_caps_entry,			  /* PE_SRC_WAIT_NEW_CAPS          */
	pe_send_soft_reset_entry,			  /* PE_SRC_SEND_SOFT_RESET        */
	pe_send_accept_entry,				  /* PE_SRC_SOFT_RESET             */
	pe_send_not_supported_entry,		  /* PE_SRC_SEND_NOT_SUPPORTED     */
	pe_src_not_supported_received_entry,  /* PE_SRC_NOT_SUPPORTED_RECEIVED */
	pe_src_ping_entry,					  /* PE_SRC_PING                   */
	pe_dummy_state_entry,				  /* PE_SRC_SEND_SOURCE_ALERT      */
	pe_dummy_state_entry,				  /* PE_SRC_SINK_ALERT_RECEIVED    */
	pe_dummy_state_entry,				  /* PE_SRC_GIVE_SOURCE_CAP_EXT    */
	pe_dummy_state_entry,				  /* PE_SRC_GIVE_SOURCE_STATUS     */
	pe_dummy_state_entry,				  /* PE_SRC_GET_SINK_STATUS        */

	pe_snk_startup_entry,				  /* PE_SNK_STARTUP                */
	pe_snk_discovery_entry,				  /* PE_SNK_DISCOVERY              */
	pe_snk_wait_for_caps_entry,			  /* PE_SNK_WAIT_FOR_CAPS          */
	pe_snk_evaluate_capability_entry,	  /* PE_SNK_EVALUATE_CAPABILITY    */
	pe_snk_select_capability_entry,		  /* PE_SNK_SELECT_CAPABILITY      */
	pe_snk_transition_sink_entry,		  /* PE_SNK_TRANSITION_SINK        */
	pe_snk_ready_entry,					  /* PE_SNK_READY                  */
	pe_snk_hard_reset_entry,			  /* PE_SNK_HARD_RESET             */
	pe_snk_transition_to_default_entry,	  /* PE_SNK_TRANSITION_TO_DEFAULT  */
	pe_snk_give_sink_cap_entry,			  /* PE_SNK_GIVE_SINK_CAP          */
	pe_snk_get_source_cap_entry,		  /* PE_SNK_GET_SOURCE_CAP         */
	pe_send_soft_reset_entry,			  /* PE_SNK_SEND_SOFT_RESET        */
	pe_send_accept_entry,				  /* PE_SNK_SOFT_RESET             */
	pe_send_not_supported_entry,		  /* PE_SNK_SEND_NOT_SUPPORTED     */
	pe_snk_not_supported_received_entry,  /* PE_SNK_NOT_SUPPORTED_RECEIVED */
	pe_dummy_state_entry,				  /* PE_SNK_SOURCE_ALERT_RECEIVED  */
	pe_dummy_state_entry,				  /* PE_SNK_SEND_SINK_ALERT        */
	pe_dummy_state_entry,				  /* PE_SNK_GET_SOURCE_CAP_EXT     */
	pe_dummy_state_entry,				  /* PE_SNK_GET_SOURCE_STATUS      */
	pe_dummy_state_entry,				  /* PE_SNK_GIVE_SINK_STATUS       */

	pe_dr_src_give_sink_caps_entry,		  /* PE_DR_SRC_GIVE_SINK_CAP       */
	pe_dr_snk_give_source_caps_entry,	  /* PE_DR_SNK_GIVE_SOURCE_CAP     */

	pe_bist_carrier_mode_entry,			  /* PE_BIST_CARRIER_MODE          */
	pe_bist_test_mode_entry,			  /* PE_BIST_TEST_MODE             */

	pe_error_recovery_entry,			  /* PE_ERROR_RECOVERY             */

	pe_drs_send_swap_entry,				  /* PE_DRS_SEND_SWAP              */
	pe_drs_evaluate_swap_entry,			  /* PE_DRS_EVALUATE_SWAP          */
	pe_send_reject_entry,				  /* PE_DRS_REJECT_SWAP            */
	pe_send_accept_entry,				  /* PE_DRS_ACCEPT_SWAP            */
	pe_drs_change_role_entry,			  /* PE_DRS_CHANGE_ROLE            */

	pe_prs_send_swap_entry,				  /* PE_PRS_SEND_SWAP              */
	pe_prs_evaluate_swap_entry,			  /* PE_PRS_EVALUATE_SWAP          */
	pe_send_reject_entry,				  /* PE_PRS_REJECT_SWAP            */
	pe_send_accept_entry,				  /* PE_PRS_ACCEPT_SWAP            */
	pe_prs_transition_to_off_entry,		  /* PE_PRS_TRANSITION_TO_OFF      */
	pe_prs_assert_rd_entry,				  /* PE_PRS_ASSERT_RD              */
	pe_prs_wait_source_on_entry,		  /* PE_PRS_WAIT_SOURCE_ON         */
	pe_prs_assert_rp_entry,				  /* PE_PRS_ASSERT_RP              */
	pe_prs_source_on_entry,				  /* PE_PRS_SOURCE_ON              */
	pe_prs_source_on_exit,				  /* PE_PRS_SOURCE_ON_EXIT         */

	pe_vcs_send_swap_entry,				  /* PE_VCS_SEND_SWAP              */
	pe_vcs_evaluate_swap_entry,			  /* PE_VCS_EVALUATE_SWAP          */
	pe_send_reject_entry,				  /* PE_VCS_REJECT_SWAP            */
	pe_send_accept_entry,				  /* PE_VCS_ACCEPT_SWAP            */
	pe_vcs_wait_for_vconn_entry,		  /* PE_VCS_WAIT_FOR_VCONN         */
	pe_vcs_turn_off_vconn_entry,		  /* PE_VCS_TURN_OFF_VCONN         */
	pe_vcs_turn_on_vconn_entry,			  /* PE_VCS_TURN_ON_VCONN          */
	pe_vcs_send_ps_rdy_entry,			  /* PE_VCS_SEND_PS_RDY            */

};


void usb_pd_pe_state_machine(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];

	if (!dev->state_change)
		return;

	while (dev->state_change)
	{
		dev->state_change = false;

		// Use branch table to execute "actions on entry or exit" for the current state.
		if (*dev->current_state < PE_NUM_STATES)
		{
			pe_state_entry[*dev->current_state](dev);
		}
	}

	return;
}


void usb_pd_pe_notify(unsigned int port, usb_pd_prl_alert_t prl_alert)
{
	usb_pd_port_t *dev = &pd[port];

	switch (prl_alert)
	{
		case PRL_ALERT_HARD_RESET_RECEIVED:
			// PD message passing is enabled in protocol layer.

			if (dev->power_role == PD_PWR_ROLE_SNK)
			{
				// Disable AutoDischargeDisconnect.
				tcpm_set_autodischarge_disconnect(port, false);

				// Disable sink VBUS.
				tcpm_snk_vbus_disable(port);

				pe_set_state(dev, PE_SNK_TRANSITION_TO_DEFAULT);
			}
			else /* SRC */
			{
				pe_set_state(dev, PE_SRC_HARD_RESET_RECEIVED);
			}
			break;

		case PRL_ALERT_MSG_TX_SUCCESS:	 /* GoodCRC received */
			switch (*dev->current_state)
			{
				case PE_SRC_SEND_CAPS:
					dev->caps_cnt = 0;
					dev->hard_reset_cnt = 0;
					timer_cancel_no_response(dev);
					dev->non_interruptable_ams = true;
					timer_start(&dev->timer, T_SENDER_RESPONSE_MS, timeout_sender_response);
					break;

				case PE_SRC_HARD_RESET:
					// Do nothing.  Wait for PSHardReset timer to expire to transition to default.
					break;

				case PE_SNK_HARD_RESET:
					pe_set_state(dev, PE_SNK_TRANSITION_TO_DEFAULT);
					break;

				case PE_SRC_GET_SINK_CAP:
				case PE_DRS_SEND_SWAP:
				case PE_PRS_SEND_SWAP:
				case PE_VCS_SEND_SWAP:
				case PE_SNK_SEND_SOFT_RESET:
				case PE_SRC_SEND_SOFT_RESET:
				case PE_SNK_SELECT_CAPABILITY:
					dev->non_interruptable_ams = true;
					timer_start(&dev->timer, T_SENDER_RESPONSE_MS, timeout_sender_response);
					break;

				case PE_SNK_SOFT_RESET:
					// Accept msg was successfully sent.
					dev->non_interruptable_ams = false;
					pe_set_state(dev, PE_SNK_WAIT_FOR_CAPS);
					break;

				case PE_SRC_SOFT_RESET:
					// Accept msg was successfully sent.
					dev->non_interruptable_ams = false;
					pe_set_state(dev, PE_SRC_SEND_CAPS);
					break;

				case PE_SRC_TRANSITION_SUPPLY:
					timer_start(&dev->timer, T_SRC_TRANSITION_MS, timeout_src_transition);
					break;

				case PE_SRC_CAPABILITY_RESPONSE:
					if (dev->explicit_contract /* || send_wait*/)
					{
						// Reject and Current contract is still valid or wait sent.
						pe_set_state(dev, PE_SRC_READY);

						// If explicit contract and current contract is invalid go to hard reset. - BQ
					}
					else /* No explicit contract */
					{
						pe_set_state(dev, PE_SRC_WAIT_NEW_CAPS);
					}
					break;

				case PE_SRC_TRANSITION_SUPPLY_EXIT:
				case PE_DR_SRC_GIVE_SINK_CAP:
				case PE_SRC_PING:
				case PE_SRC_SEND_NOT_SUPPORTED:
					pe_set_state(dev, PE_SRC_READY);
					break;

				case PE_SNK_GET_SOURCE_CAP:
				case PE_SNK_GIVE_SINK_CAP:
				case PE_DR_SNK_GIVE_SOURCE_CAP:
				case PE_SNK_SEND_NOT_SUPPORTED:
					pe_set_state(dev, PE_SNK_READY);
					break;

				case PE_PRS_SOURCE_ON_EXIT:
					pe_set_state(dev, PE_SRC_STARTUP);
					break;

				case PE_DRS_ACCEPT_SWAP:
					pe_set_state(dev, PE_DRS_CHANGE_ROLE);
					break;

				case PE_PRS_ACCEPT_SWAP:
					pe_set_state(dev, PE_PRS_TRANSITION_TO_OFF);
					break;

				case PE_VCS_ACCEPT_SWAP:
					if (dev->vconn_source)
					{
						pe_set_state(dev, PE_VCS_WAIT_FOR_VCONN);
					}
					else
					{
						pe_set_state(dev, PE_VCS_TURN_ON_VCONN);
					}
					break;

				case PE_PRS_REJECT_SWAP:
				case PE_DRS_REJECT_SWAP:
				case PE_VCS_REJECT_SWAP:
				case PE_VCS_SEND_PS_RDY:
					if (dev->power_role == PD_PWR_ROLE_SNK)
					{
						pe_set_state(dev, PE_SNK_READY);
					}
					else
					{
						pe_set_state(dev, PE_SRC_READY);
					}
					break;

				default:
					break;
			}
			break;

		case PRL_ALERT_MSG_TX_DISCARDED: /* Msg was received or Hard Reset issued before Tx */
			// Do nothing. Proceed to handle the received message or Hard Reset.
			break;

		case PRL_ALERT_MSG_TX_FAILED: /* No GoodCRC response */
			if (*dev->current_state == PE_PRS_WAIT_SOURCE_ON)
			{
				// Cancel PSSourceOn timer.
				timer_cancel(&dev->timer);

				// PS_RDY msg failed.
				pe_set_state(dev, PE_ERROR_RECOVERY);
			}
			else if (*dev->current_state == PE_PRS_SOURCE_ON_EXIT)
			{
				// PS_RDY msg failed.
				pe_set_state(dev, PE_ERROR_RECOVERY);
			}
			else if (dev->power_role == PD_PWR_ROLE_SNK)
			{
				if (*dev->current_state == PE_SNK_SEND_SOFT_RESET)
				{
					// Any failure in the Soft Reset process will trigger a Hard Reset.
					pe_set_state(dev, PE_SNK_HARD_RESET);
				}
				else if (*dev->current_state != PE_UNATTACHED)
				{
					// Failure to see a GoodCRC when a port pair
					// is connected will result in a Soft Reset.
					pe_set_state(dev, PE_SNK_SEND_SOFT_RESET);
				}
			}
			else /* SRC */
			{
				if (*dev->current_state == PE_SRC_SEND_SOFT_RESET)
				{
					// Any failure in the Soft Reset process will trigger a Hard Reset.
					pe_set_state(dev, PE_SRC_HARD_RESET);
				}
				else if ((*dev->current_state == PE_SRC_SEND_CAPS) &&
						 (!dev->pd_connected_since_attach))
				{
					pe_set_state(dev, PE_SRC_DISCOVERY);
				}
				else if (*dev->current_state != PE_UNATTACHED)
				{
					// Failure to see a GoodCRC when a port pair
					// is connected will result in a Soft Reset.
					pe_set_state(dev, PE_SRC_SEND_SOFT_RESET);
				}
			}
			break;

		case PRL_ALERT_MSG_RECEIVED:
			if (dev->rx_msg_data_len)
			{
				usb_pd_pe_data_msg_rx_handler(dev);
			}
			else
			{
				usb_pd_pe_ctrl_msg_rx_handler(dev);
			}
			break;

		default:
			break;
	}

	return;
}


int usb_pd_policy_manager_request(unsigned int port, pd_policy_manager_request_t req)
{
	usb_pd_port_t *dev = &pd[port];
	usb_pd_pe_status_t status = STATUS_OK;

	if (*dev->current_state == PE_SRC_READY)
	{
		if (req == PD_POLICY_MNGR_REQ_GET_SINK_CAPS)
		{
			pe_set_state(dev, PE_SRC_GET_SINK_CAP);
		}
		else if (req == PD_POLICY_MNGR_REQ_SRC_CAPS_CHANGE)
		{
			pe_set_state(dev, PE_SRC_SEND_CAPS);
		}
		else if (req == PD_POLICY_MNGR_REQ_GOTO_MIN)
		{
			dev->request_goto_min = true;
			pe_set_state(dev, PE_SRC_TRANSITION_SUPPLY);
		}
		else if (req == PD_POLICY_MNGR_REQ_PR_SWAP)
		{
			pe_set_state(dev, PE_PRS_SEND_SWAP);
		}
		else if (req == PD_POLICY_MNGR_REQ_DR_SWAP)
		{
			pe_set_state(dev, PE_DRS_SEND_SWAP);
		}
		else if (req == PD_POLICY_MNGR_REQ_VCONN_SWAP)
		{
			pe_set_state(dev, PE_VCS_SEND_SWAP);
		}
		else
		{
			status = STATUS_REQUEST_NOT_SUPPORTED_IN_CURRENT_STATE;
		}
	}
	else if (*dev->current_state == PE_SRC_WAIT_NEW_CAPS)
	{
		if (req == PD_POLICY_MNGR_REQ_SRC_CAPS_CHANGE)
		{
			pe_set_state(dev, PE_SRC_SEND_CAPS);
		}
		else
		{
			status = STATUS_REQUEST_NOT_SUPPORTED_IN_CURRENT_STATE;
		}
	}
	else if (*dev->current_state == PE_SNK_READY)
	{
		if (req == PD_POLICY_MNGR_REQ_UPDATE_REMOTE_CAPS)
		{
			pe_set_state(dev, PE_SNK_GET_SOURCE_CAP);
		}
		else if (req == PD_POLICY_MNGR_REQ_PR_SWAP)
		{
			pe_set_state(dev, PE_PRS_SEND_SWAP);
		}
		else if (req == PD_POLICY_MNGR_REQ_DR_SWAP)
		{
			pe_set_state(dev, PE_DRS_SEND_SWAP);
		}
		else if (req == PD_POLICY_MNGR_REQ_VCONN_SWAP)
		{
			pe_set_state(dev, PE_VCS_SEND_SWAP);
		}
		else
		{
			status = STATUS_REQUEST_NOT_SUPPORTED_IN_CURRENT_STATE;
		}
	}
	else
	{
		status = STATUS_REQUEST_NOT_SUPPORTED_IN_CURRENT_STATE;
	}

	return status;
}

bool usb_pd_pe_is_remote_externally_powered(unsigned int port)
{
	return pd[port].remote_externally_powered;
}

static void timeout_src_settling(unsigned int port)
{
	pe_set_state(&pd[port], PE_SRC_TRANSITION_SUPPLY_EXIT);
	return;
}

void usb_pd_pe_voltage_alarm_handler(unsigned int port, bool hi_voltage)
{
	usb_pd_port_t *dev = &pd[port];
	uint16_t v_threshold;
#ifdef CONFIG_TUSB422_PAL
	tcpc_device_t *tc_dev = tcpm_get_device(port);
#endif

	if (hi_voltage)
	{
		if (*dev->current_state == PE_SRC_TRANSITION_SUPPLY)
		{
			// Start power supply settling timeout.
			timer_start(&dev->timer, dev->src_settling_time, timeout_src_settling);
		}
		else if (*dev->current_state == PE_PRS_SOURCE_ON)
		{
			timer_start(&dev->timer, T_VBUS_5V_STABLIZE_MS, timeout_vbus_5v_stabilize);
		}
		else if (*dev->current_state == PE_SRC_TRANSITION_TO_DEFAULT)
		{
			pe_set_state(dev, PE_SRC_STARTUP);
		}
		else if ((*dev->current_state == PE_SNK_STARTUP) ||
				 (*dev->current_state == PE_SNK_DISCOVERY))
		{
			dev->vbus_present = true;

			if (*dev->current_state == PE_SNK_DISCOVERY)
			{
				pe_set_state(dev, PE_SNK_WAIT_FOR_CAPS);
			}
		}
	}
	else /* Low voltage */
	{
		if (*dev->current_state == PE_SRC_TRANSITION_TO_DEFAULT)
		{
			// Request policy manager to set the Port Data Role to DFP.
			if (dev->data_role != PD_DATA_ROLE_DFP)
			{
				dev->data_role = PD_DATA_ROLE_DFP;

				tcpm_update_msg_header_info(dev->port, dev->data_role, dev->power_role);

#ifdef CONFIG_TUSB422_PAL
				usb_pd_pal_notify_connect_state(dev->port, tc_dev->state, tc_dev->plug_polarity);
//                usb_pd_pal_data_role_swap(dev->port, PD_DATA_ROLE_DFP);
#endif
			}

			timer_start(&dev->timer, T_SRC_RECOVER_MS, timeout_src_recover);
		}
		else if (*dev->current_state == PE_PRS_TRANSITION_TO_OFF)
		{
			if (dev->power_role == PD_PWR_ROLE_SRC)
			{
				pe_set_state(dev, PE_PRS_ASSERT_RD);
			}
		}
		else if (*dev->current_state == PE_SRC_TRANSITION_SUPPLY)
		{
			// Negative voltage transition complete.  Enable vSrcNew.
			if (dev->object_position == 1)
			{
				// 5V.
				tcpm_src_vbus_5v_enable(dev->port);

				v_threshold = VSAFE5V_MIN;
			}
			else
			{
				v_threshold = pd_power_enable_non_default_src_vbus(dev);
			}

			// Use Hi voltage alarm to determine when power supply is ready.
			tcpm_set_voltage_alarm_hi(dev->port, v_threshold);
		}
		else if ((*dev->current_state == PE_SNK_STARTUP) ||
				 (*dev->current_state == PE_SNK_DISCOVERY))
		{
			// Set Hi voltage alarm to determine when VBUS is present.
			tcpm_set_voltage_alarm_hi(dev->port, VSAFE5V_MIN);
		}
	}

	return;
}


void usb_pd_pe_current_change_handler(unsigned int port, tcpc_cc_snk_state_t curr_adv)
{
	usb_pd_port_t *dev = &pd[port];

	// Used by SNK for collision avoidance.
	if (curr_adv == CC_SNK_STATE_POWER30)
	{
		// Sink Tx OK.  Sink can initiate an AMS.
	}
	else if (curr_adv == CC_SNK_STATE_POWER15)
	{
		// Sink Tx NG.
	}

	if (!dev->explicit_contract)
	{
		// Notify system of current change.
		tcpm_snk_vbus_enable(dev->port);
	}

	return;
}
