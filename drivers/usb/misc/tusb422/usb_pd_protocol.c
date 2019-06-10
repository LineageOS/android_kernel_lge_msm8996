/*
 * TUSB422 Power Delivery
 *
 * Author: Brian Quach <brian.quach@ti.com>
 *
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "usb_pd_protocol.h"
#include "tcpm.h"
#include "usb_pd.h"
#include "usb_pd_policy_engine.h"

#define MSG_ID_CLEARED 0xCE

extern usb_pd_port_t pd[NUM_TCPC_DEVICES];

//#if DEBUG_LEVEL >= 3
#if DEBUG_LEVEL >= 3 || defined(CONFIG_LGE_USB_TYPE_C)

static const char * const ctrlmsg2string[]=
{
	"",						  /* 0x00 */
	"GOOD_CRC",				  /* 0x01 */
	"GOTO_MIN",				  /* 0x02 */
	"ACCEPT",				  /* 0x03 */
	"REJECT",				  /* 0x04 */
	"PING",					  /* 0x05 */
	"PS_RDY",				  /* 0x06 */
	"GET_SRC_CAP",			  /* 0x07 */
	"GET_SNK_CAP",			  /* 0x08 */
	"DR_SWAP",				  /* 0x09 */
	"PR_SWAP",				  /* 0x0A */
	"VCONN_SWAP",			  /* 0x0B */
	"WAIT",					  /* 0x0C */
	"SOFT_RESET",			  /* 0x0D */
	"",						  /* 0x0E */
	"",						  /* 0x0F */
	"NOT_SUPPORTED",		  /* 0x10 */
	"GET_SRC_CAP_EXT",		  /* 0x11 */
	"GET_STATUS",			  /* 0x12 */
	"FR_SWAP"				  /* 0x13 */
};

static const char * const datamsg2string[]=
{
	"",					  /* 0x00 */
	"SRC_CAPS",			  /* 0x01 */
	"REQUEST",			  /* 0x02 */
	"BIST",				  /* 0x03 */
	"SNK_CAPS",			  /* 0x04 */
	"BATT_STATUS",		  /* 0x05 */
	"ALERT",			  /* 0x06 */
	"", "", "", "", "", "", "", "",	 /* 0x07 - 0x0E */
	"VENDOR"			  /* 0x0F */
};
#endif

void usb_pd_prl_reset(unsigned int port)
{
	unsigned int i;

	// Reset all msg IDs.
	for (i = 0; i < MAX_SOP_NUM; i++)
	{
		pd[port].msg_id[i] = 0;
		pd[port].stored_msg_id[i] = MSG_ID_CLEARED;
	}

	return;
}

static void usb_pd_prl_transmit_alert_handler(unsigned int port, tx_status_t tx_status)
{
	// Increment message ID.
	pd[port].msg_id[pd[port].tx_sop_type]++;

	if (tx_status == TX_STATUS_SUCCESS)
	{
		// GoodCRC received.
		usb_pd_pe_notify(port, PRL_ALERT_MSG_TX_SUCCESS);
	}
	else if (tx_status == TX_STATUS_DISCARDED)
	{
		// Either we issued a Hard Reset or received a msg.
		usb_pd_pe_notify(port, PRL_ALERT_MSG_TX_DISCARDED);
	}
	else if (tx_status == TX_STATUS_FAILED)
	{
		// No valid GoodCRC received.
#ifdef CONFIG_LGE_USB_TYPE_C
		pd[port].msg_id[pd[port].tx_sop_type]--;
#endif
		usb_pd_pe_notify(port, PRL_ALERT_MSG_TX_FAILED);
	}

	return;
}


static void usb_pd_prl_receive_alert_handler(unsigned int port)
{
	usb_pd_port_t *dev = &pd[port];
	uint8_t sop;
	uint8_t msg_id;
	uint16_t hdr;

	if (*dev->current_state == PE_BIST_TEST_MODE)
	{
		return;
	}

	// Read Rx header and SOP type.
	tcpm_get_msg_header_type(port, &sop, &hdr);
	dev->rx_sop = sop;
	dev->rx_msg_type = USB_PD_HDR_GET_MSG_TYPE(hdr);
	dev->rx_msg_data_len = USB_PD_HDR_GET_DATA_LEN(hdr);

	if (sop == 0)
	{
		// Check port data role field for SOP pkts.
		if (dev->data_role == USB_PD_HDR_GET_DATA_ROLE(hdr))
		{
			PRINT("Msg hdr data role %u invalid! msg_type = 0x%x, len = %u.\n",
				  USB_PD_HDR_GET_DATA_ROLE(hdr), dev->rx_msg_type, dev->rx_msg_data_len);

			// Perform Type-C Error Recovery.
			tcpm_execute_error_recovery(port);
			return;
		}
	}

	INFO("Rx msg len: %u-bytes, type: %u (%s)\n", dev->rx_msg_data_len,
		 dev->rx_msg_type, (dev->rx_msg_data_len) ?
		 datamsg2string[dev->rx_msg_type] : ctrlmsg2string[dev->rx_msg_type]);

	// Check for Soft Reset.
	if ((dev->rx_msg_data_len == 0) &&
		(dev->rx_msg_type == CTRL_MSG_TYPE_SOFT_RESET))
	{
		CRIT("Soft Reset Rx'd. SOP=%u\n", sop);
		// Reset message ID.
		dev->msg_id[sop] = 0;
		dev->stored_msg_id[sop] = MSG_ID_CLEARED;

		usb_pd_pe_notify(port, PRL_ALERT_MSG_RECEIVED);
	}
	else
	{
		msg_id = USB_PD_HDR_GET_MSG_ID(hdr);
		INFO("MsgID: %u\n", msg_id);

		// Verify this is not a retry msg.
		if (msg_id != dev->stored_msg_id[sop])
		{
			// Store message ID.
			dev->stored_msg_id[sop] = msg_id;

			tcpm_read_message(port, pd[port].rx_msg_buf, dev->rx_msg_data_len);

			usb_pd_pe_notify(port, PRL_ALERT_MSG_RECEIVED);
		}
		else
		{
			CRIT("MsgID: %u matches stored!\n", msg_id);
		}
	}

	return;
}


static void usb_pd_prl_hard_reset_alert_handler(unsigned int port)
{
	usb_pd_pe_notify(port, PRL_ALERT_HARD_RESET_RECEIVED);
	return;
}


static void usb_pd_prl_tx_msg(unsigned int port, uint8_t *buf, tcpc_transmit_t sop_type)
{
	tcpm_transmit(port, buf, sop_type);

	// Save the SOP type.
	pd[port].tx_sop_type = sop_type;

	return;
}


void usb_pd_prl_tx_ctrl_msg(unsigned int port, uint8_t *buf, msg_hdr_ctrl_msg_type_t msg_type, tcpc_transmit_t sop_type)
{
	DEBUG("Tx msg_type: %u (%s), sop: %u\n", msg_type, ctrlmsg2string[msg_type], sop_type);

	if (msg_type == CTRL_MSG_TYPE_SOFT_RESET)
	{
		// Reset message ID counter.
		pd[port].msg_id[sop_type] = 0;
		pd[port].stored_msg_id[sop_type] = MSG_ID_CLEARED;
	}

	buf[0] = 2;	/* Tx byte cnt */
	buf[1] = USB_PD_HDR_GEN_BYTE0((sop_type == TCPC_TX_SOP) ? pd[port].data_role : 0, msg_type);
	buf[2] = USB_PD_HDR_GEN_BYTE1(0, 0, pd[port].msg_id[sop_type], pd[port].power_role);

	INFO("buf[0-2]: 0x%02x %02x %02x\n", buf[0], buf[1], buf[2]);

	usb_pd_prl_tx_msg(port, buf, sop_type);

	return;
}

void usb_pd_prl_tx_data_msg(unsigned int port, uint8_t *buf, msg_hdr_data_msg_type_t msg_type, tcpc_transmit_t sop_type, unsigned int ndo)
{
	DEBUG("Tx msg_type: %u (%s), sop: %u, ndo: %u\n", msg_type, datamsg2string[msg_type], sop_type, ndo);

	buf[0] = (ndo << 2) + 2; // Each data object is 4-bytes plus 2-byte header.
	buf[1] = USB_PD_HDR_GEN_BYTE0((sop_type == TCPC_TX_SOP) ? pd[port].data_role : 0, msg_type);
	buf[2] = USB_PD_HDR_GEN_BYTE1(0, ndo, pd[port].msg_id[sop_type], pd[port].power_role);

	INFO("buf[0-2]: 0x%02x %02x %02x\n", buf[0], buf[1], buf[2]);

	usb_pd_prl_tx_msg(port, buf, sop_type);

	return;
}


static const tcpm_callbacks_t tcpm_callbacks = {
	.conn_state_change_cbk = usb_pd_pe_connection_state_change_handler,
	.current_change_cbk = usb_pd_pe_current_change_handler,
	.pd_hard_reset_cbk = usb_pd_prl_hard_reset_alert_handler,
	.pd_receive_cbk = usb_pd_prl_receive_alert_handler,
	.pd_transmit_cbk = usb_pd_prl_transmit_alert_handler,
	.volt_alarm_cbk = usb_pd_pe_voltage_alarm_handler,
};

void usb_pd_prl_init(void)
{
	tcpm_register_callbacks(&tcpm_callbacks);

	return;
}
