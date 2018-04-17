/*
 * mausb.h
 *
 * Copyright (C) 2015-2016 LGE Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#ifndef __MAUSB_H
#define __MAUSB_H

#include <linux/list.h>
#include "mausb_common.h"

#define aMaxRequestID  			255
#define aMaxSequenceNumber		16777215
#define aDataChannelDelay		1 //TODO change value
#define aManagementChannelDelay		2
#define aMaxIsochLinkDelay		3
#define MaxFrameDistance		895
#define aManagementResponseTime		5	
#define aManagementRequestTimeout	(aManagementResponseTime + 2 * aManagementChannelDelay)
#define aTransferResponseTime		10
#define aTransferTimeout		(aTransferResponseTime + 2 * aDataChannelDelay)
#define aTransferKeepAlive		(aTransferResponseTime + aDataChannelDelay)
#define aTransferTimerTick		1 //TODO SONU is this equal to one among the above predefines


/*MA USB error code */ /* TODO set values */
#define MAUSB_STATUS_INVALID_REQUEST		0 
#define MAUSB_STATUS_MISSING_REQUEST_ID		1
#define MAUSB_STATUS_TRANSFER_PENDING		2
#define MAUSB_STATUS_ERROR_XXX			3
#define MAUSB_STATUS_SUCCESS			0
#define MAUSB_STATUS_MISSING_SEQUENCE_NUMBER	4
#define MAUSB_STATUS_DROPPED_PACKET		5
#define MAUSB_STATUS_NO_ERROR			0

#define TransferRespRetries			1 
#define MAUSB_NO_ERROR				0 //TODO can we use MAUSB_STATUS_NO_ERROR


#define TRUE 	1
#define FALSE 	0

typedef struct _value_string {
	long value;
	const char *strptr;
} value_string;

typedef struct _true_false_string {
	const char *true_string;
	const char *false_string;
} true_false_string;

/* MAUSB Version, per 6.2.1.1 */
#define MAUSB_VERSION_1_0     0x0
#define MAUSB_VERSION_MASK    0x0F

/* for dissecting snap packets */
/*
*/
#define OUI_MAUSB 0xdead54
#define PID_MAUSB 0xf539

static const value_string mausb_pid_string[] = {
    { PID_MAUSB, "MAUSB" },
    { 0, NULL}
};

static const value_string mausb_version_string[] = {
    { MAUSB_VERSION_1_0, "MAUSB protocol version 1.0" },
    { 0, NULL}
};

/* Packet flags, per 6.2.1.2 */
#define MAUSB_FLAG_MASK       0xF0
#define MAUSB_FLAG_HOST       (1 << 0)
#define MAUSB_FLAG_RETRY      (1 << 1)
#define MAUSB_FLAG_TIMESTAMP  (1 << 2)
#define MAUSB_FLAG_RESERVED   (1 << 3)
#define MAUSB_FLAG_OFFSET     4


static const value_string mausb_flag_string[] = {
    { 0,                                                         "(None)"                   },
    { MAUSB_FLAG_HOST,                                           "(Host)"                   },
    { MAUSB_FLAG_RETRY,                                          "(Retry)"                  },
    { MAUSB_FLAG_TIMESTAMP,                                      "(Timestamp)"              },
    { MAUSB_FLAG_HOST  | MAUSB_FLAG_RETRY,                       "(Host, Retry)"            },
    { MAUSB_FLAG_HOST  | MAUSB_FLAG_TIMESTAMP,                   "(Host, Timestamp)"        },
    { MAUSB_FLAG_RETRY | MAUSB_FLAG_TIMESTAMP,                   "(Retry, Timestamp)"       },
    { MAUSB_FLAG_HOST | MAUSB_FLAG_RETRY | MAUSB_FLAG_TIMESTAMP, "(Host, Retry, Timestamp)" },
    { 0, NULL}
};

/* Packet Types, per 6.2.1.3 */
#define MAUSB_PKT_TYPE_MASK       0xC0
#define MAUSB_PKT_TYPE_MGMT       (0 << 6)
#define MAUSB_PKT_TYPE_CTRL       (1 << 6)
#define MAUSB_PKT_TYPE_DATA       (2 << 6)

/* Packet Subtypes, per 6.2.1.3 */
#define MAUSB_SUBTYPE_MASK        0x3F

enum mausb_pkt_type {
    /* Management packets */
    CapReq = 0x00 | MAUSB_PKT_TYPE_MGMT,
    CapResp               ,
    USBDevHandleReq       ,
    USBDevHandleResp      ,

    EPHandleReq           ,
    EPHandleResp          ,
    EPActivateReq         ,
    EPActivateResp        ,
    EPInactivateReq       ,
    EPInactivateResp      ,
    EPRestartReq          ,
    EPRestartResp         ,
    EPClearTransferReq    ,
    EPClearTransferResp   ,
    EPHandleDeleteReq     ,
    EPHandleDeleteResp    ,

    MAUSBDevResetReq      ,
    MAUSBDevResetResp     ,
    ModifyEP0Req          ,
    ModifyEP0Resp         ,
    SetDevAddrReq         ,
    SetDevAddrResp        ,
    UpdateDevReq          ,
    UpdateDevResp         ,
    DisconnectDevReq      ,
    DisconnectDevResp     ,

    MAUSBDevSleepReq      ,
    MAUSBDevSleepResp     ,
    MAUSBDevWakeReq       ,
    MAUSBDevWakeResp      ,
    MAUSBDevInitSleepReq  , /* Transmitted by Device */
    MAUSBDevInitSleepResp , /* Transmitted by Host   */
    MAUSBDevRemoteWakeReq , /* Transmitted by Device */
    MAUSBDevRemoteWakeResp, /* Transmitted by Host   */
    PingReq               , /* Transmitted by either */
    PingResp              , /* Transmitted by either */
    MAUSBDevDisconnectReq ,
    MAUSBDevDisconnectResp,
    MAUSBDevInitDisconReq , /* Transmitted by Device */
    MAUSBDevInitDisconResp, /* Transmitted by Host   */
    MAUSBSyncReq          ,
    MAUSBSyncResp         ,

    CancelTransferReq     ,
    CancelTransferResp    ,
    EPOpenStreamReq       ,
    EPOpenStreamResp      ,
    EPCloseStreamReq      ,
    EPCloseStreamResp     ,
    USBDevResetReq        ,
    USBDevResetResp       ,

    /* Vendor-Specific Management Packets */
    /* Transmitted by either */
    VendorSpecificReq = 0x3E | MAUSB_PKT_TYPE_MGMT,
    VendorSpecificResp    ,

    /* Control Packets */ /* Transmitter not defined! */
    TransferSetupReq = 0x00 | MAUSB_PKT_TYPE_CTRL,
    TransferSetupResp     ,
    TransferTearDownConf  ,

    /* Data Packets */
    TransferReq = 0x00 | MAUSB_PKT_TYPE_DATA,
    TransferResp = 0x01 | MAUSB_PKT_TYPE_DATA,
    TransferAck           , /* Transmitted by Host   */
    IsochTransferReq      , /* Transmitter not defined! */
    IsochTransferResp       /* Transmitter not defined! */
};


/**
 * Type & Subtype values for MAUSB packet variants, per 6.2.1.3, Table 5
 */
static const value_string mausb_type_string[] = {
    /* Management packets */
    { MAUSB_PKT_TYPE_MGMT | 0x00 , "CapReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x01 , "CapResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x02 , "USBDevHandleReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x03 , "USBDevHandleResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x04 , "EPHandleReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x05 , "EPHandleResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x06 , "EPActivateReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x07 , "EPActivateResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x08 , "EPInactivateReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x09 , "EPInactivateResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x0a , "EPResetReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x0b , "EPResetResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x0c , "EPClearTransferReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x0d , "EPClearTransferResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x0e , "EPHandleDeleteReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x0f , "EPHandleDeleteResp" },

    { MAUSB_PKT_TYPE_MGMT | 0x10 , "MADevResetReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x11 , "MADevResetResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x12 , "ModifyEP0Req" },
    { MAUSB_PKT_TYPE_MGMT | 0x13 , "ModifyEP0Resp" },
    { MAUSB_PKT_TYPE_MGMT | 0x14 , "SetDevAddrReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x15 , "SetDevAddrResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x16 , "UpdateDevReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x17 , "UpdateDevResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x18 , "DisconnectDevReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x19 , "DisconnectDevResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x1a , "USBSuspendReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x1b , "USBSuspendResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x1c , "USBResumeReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x1d , "USBResumeResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x1e , "RemoteWakeReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x1f , "RemoteWakeResp" },

    { MAUSB_PKT_TYPE_MGMT | 0x20 , "PingReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x21 , "PingResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x22 , "MADevDisconnectReq " },
    { MAUSB_PKT_TYPE_MGMT | 0x23 , "MADevDisconnectResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x24 , "MADevInitDisconReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x25 , "MADevInitDisconResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x26 , "SyncReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x27 , "SyncResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x28 , "CancelTransferReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x29 , "CancelTransferResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x2a , "EPOpenStreamReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x2b , "EPOpenStreamResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x2c , "EPCloseStreamReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x2d , "EPCloseStreamResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x2e , "USBDevResetReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x2f , "USBDevResetResp" },

    { MAUSB_PKT_TYPE_MGMT | 0x30 , "DevNotificationReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x31 , "DevNotificationResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x32 , "EPSetKeepAliveReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x33 , "EPSetKeepAliveResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x34 , "GetPortBWReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x35 , "GetPortBWResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x36 , "SleepReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x37 , "SleepResp" },
    { MAUSB_PKT_TYPE_MGMT | 0x38 , "WakeReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x39 , "WakeResp" },

    /* Vendor-Specific Management Packets */
    { MAUSB_PKT_TYPE_MGMT | 0x3e, "VendorSpecificReq" },
    { MAUSB_PKT_TYPE_MGMT | 0x3f, "VendorSpecificResp" },

    /* Control Packets */
    { MAUSB_PKT_TYPE_CTRL | 0x00, "TransferSetupReq" },
    { MAUSB_PKT_TYPE_CTRL | 0x01, "TransferSetupResp" },
    { MAUSB_PKT_TYPE_CTRL | 0x02, "TransferTearDownConf" },

    /* Data Packets */
    { MAUSB_PKT_TYPE_DATA | 0x00, "TransferReq" },
    { MAUSB_PKT_TYPE_DATA | 0x01, "TransferResp" },
    { MAUSB_PKT_TYPE_DATA | 0x02, "TransferAck" },
    { MAUSB_PKT_TYPE_DATA | 0x03, "IsochTransferReq" },
    { MAUSB_PKT_TYPE_DATA | 0x04, "IsochTransferResp" },
    { 0, NULL}
};

#define MAUSB_EP_HANDLE_D        0x0001
#define MAUSB_EP_HANDLE_EP_NUM   0x001e
#define MAUSB_EP_HANDLE_DEV_ADDR 0x0fe0
#define MAUSB_EP_HANDLE_BUS_NUM  0xf000

static const value_string mausb_status_string[] = {
    {   0, "SUCCESS (NO_ERROR)" },
    { 128, "UNSUCCESSFUL" },
    { 129, "INVALID_MA_USB_SESSION_STATE" },
    { 130, "INVALID_DEVICE_HANDLE" },
    { 131, "INVALID_EP_HANDLE" },
    { 132, "INVALID_EP_HANDLE_STATE" },
    { 133, "INVALID_REQUEST" },
    { 134, "MISSING_SEQUENCE_NUMBER" },
    { 135, "TRANSFER_PENDING" },
    { 136, "TRANSFER_EP_STALL" },
    { 137, "TRANSFER_SIZE_ERROR" },
    { 138, "TRANSFER_DATA_BUFFER_ERROR" },
    { 139, "TRANSFER_BABBLE_DETECTED" },
    { 140, "TRANSFER_TRANSACTION_ERROR" },
    { 141, "TRANSFER_SHORT_TRANSFER" },
    { 142, "TRANSFER_CANCELLED" },
    { 143, "INSUFFICENT_RESOURCES" },
    { 144, "NOT_SUFFICENT_BANDWIDTH" },
    { 145, "INTERNAL_ERROR" },
    { 146, "DATA_OVERRUN" },
    { 147, "DEVICE_NOT_ACCESSED" },
    { 148, "BUFFER_OVERRUN" },
    { 149, "BUSY" },
    { 150, "DROPPED_PACKET" },
    { 151, "ISOC_TIME_EXPIRED" },
    { 152, "ISOCH_TIME_INVALID" },
    { 153, "NO_USB_PING_RESPONSE" },
    { 154, "NOT_SUPPORTED" },
    { 155, "REQUEST_DENIED" },
    { 0, NULL}
};

#define MAUSB_TOKEN_MASK  0x03ff
#define MAUSB_MGMT_PAD_MASK  0xfffc
#define MAUSB_MGMT_NUM_EP_DES_MASK 0x001f
#define MAUSB_MGMT_SIZE_EP_DES_OFFSET 5
#define MAUSB_MGMT_SIZE_EP_DES_MASK (0x003f << MAUSB_MGMT_SIZE_EP_DES_OFFSET)

#define DWORD_MASK 0xffffffff
#define MAUSB_MGMT_NUM_EP_HANDLE_PAD_MASK \
            (DWORD_MASK & !(MAUSB_MGMT_NUM_EP_DES_MASK))
#define MAUSB_MGMT_EP_DES_PAD_MASK \
            ((DWORD_MASK & !(MAUSB_MGMT_NUM_EP_DES_MASK | \
                           MAUSB_MGMT_SIZE_EP_DES_MASK)) >> 8)


/* EPHandleResp Bitfield Masks */
#define MAUSB_EP_HANDLE_RESP_DIR_MASK   (1 << 0)
#define MAUSB_EP_HANDLE_RESP_ISO_MASK   (1 << 1)
#define MAUSB_EP_HANDLE_RESP_LMAN_MASK  (1 << 2)
#define MAUSB_EP_HANDLE_RESP_VALID_MASK (1 << 3)

static const value_string mausb_eps_string[] = {
    { 0, "Unassigned" },
    { 1, "Active" },
    { 2, "Inactive" },
    { 3, "Halted" },
    { 0, NULL}
};

#define MAUSB_EPS_MASK 0x03

#define MAUSB_TFLAG_MASK   0xfc

#define MAUSB_TX_TYPE_CTRL (0 << 3)
#define MAUSB_TX_TYPE_ISOC (1 << 3)
#define MAUSB_TX_TYPE_BULK (2 << 3)
#define MAUSB_TX_TYPE_INTR (3 << 3)

#define MAUSB_TFLAG_OFFSET     2
#define MAUSB_TFLAG_ARQ  (1 << 0)
#define MAUSB_TFLAG_NEG  (1 << 1)
#define MAUSB_TFLAG_EOT  (1 << 2)
#define MAUSB_TFLAG_TRANSFER_TYPE (3 << 3)
#define MAUSB_TFLAG_RSVD (1 << 5)

static const value_string mausb_transfer_type_string[] = {
    { 0, "Control" },
    { 1, "Isochronous" },
    { 2, "Bulk" },
    { 3, "Interrupt" },
    { 0, NULL},
};

static const value_string mausb_tflag_string[] = {
    { MAUSB_TX_TYPE_CTRL,                                     "Control"                 },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_ARQ,                   "Control (ARQ)"           },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_NEG,                   "Control (NEG)"           },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_EOT,                   "Control (EoT)"           },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_NEG, "Control (ARQ, NEG)"      },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_EOT, "Control (ARQ, EoT)"      },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT, "Control (NEG, EoT)"      },
    { MAUSB_TX_TYPE_CTRL | MAUSB_TFLAG_ARQ
       | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT,                   "Control (ARQ, NEG, EoT)" },

    { MAUSB_TX_TYPE_ISOC,                                     "Isochronous"                 },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_ARQ,                   "Isochronous (ARQ)"           },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_NEG,                   "Isochronous (NEG)"           },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_EOT,                   "Isochronous (EoT)"           },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_NEG, "Isochronous (ARQ, NEG)"      },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_EOT, "Isochronous (ARQ, EoT)"      },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT, "Isochronous (NEG, EoT)"      },
    { MAUSB_TX_TYPE_ISOC | MAUSB_TFLAG_ARQ
       | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT,                   "Isochronous (ARQ, NEG, EoT)" },

    { MAUSB_TX_TYPE_BULK,                                     "Bulk"                 },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_ARQ,                   "Bulk (ARQ)"           },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_NEG,                   "Bulk (NEG)"           },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_EOT,                   "Bulk (EoT)"           },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_NEG, "Bulk (ARQ, NEG)"      },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_EOT, "Bulk (ARQ, EoT)"      },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT, "Bulk (NEG, EoT)"      },
    { MAUSB_TX_TYPE_BULK | MAUSB_TFLAG_ARQ
       | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT,                   "Bulk (ARQ, NEG, EoT)" },

    { MAUSB_TX_TYPE_INTR,                                     "Interrupt"                 },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_ARQ,                   "Interrupt (ARQ)"           },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_NEG,                   "Interrupt (NEG)"           },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_EOT,                   "Interrupt (EoT)"           },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_NEG, "Interrupt (ARQ, NEG)"      },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_ARQ | MAUSB_TFLAG_EOT, "Interrupt (ARQ, EoT)"      },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT, "Interrupt (NEG, EoT)"      },
    { MAUSB_TX_TYPE_INTR | MAUSB_TFLAG_ARQ
       | MAUSB_TFLAG_NEG | MAUSB_TFLAG_EOT,                   "Interrupt (ARQ, NEG, EoT)" },
    { 0, NULL}
};


//const true_false_string tfs_ep_handle_resp_dir = { "IN", "OUT or Control" };

#define MAUSB_TRANSFER_TYPE_OFFSET 3 /* Offset from start of TFlags Field */
                                     /* (EPS not included) */
#define MAUSB_TRANSFER_TYPE_CTRL      (0 << MAUSB_TRANSFER_TYPE_OFFSET)
#define MAUSB_TRANSFER_TYPE_ISO       (1 << MAUSB_TRANSFER_TYPE_OFFSET)
#define MAUSB_TRANSFER_TYPE_BULK      (2 << MAUSB_TRANSFER_TYPE_OFFSET)
#define MAUSB_TRANSFER_TYPE_INTERRUPT (3 << MAUSB_TRANSFER_TYPE_OFFSET)

/*mausb header*/

#define MAUSB_COMMON_HEADER_LEN			12
#define MAUSB_NON_ISOCHRONUS_HEADER_LEN	20
#define MAUSB_ISOCHRONUS_HEADER_LEN		24
#define MAUSB_MANAGEMENT_HEADER_LEN		12


#if 0

/** Common header fields, per section 6.2.1 */
struct mausb_header {
    /* DWORD 0 */
    __u8	ver_flags;
    __u8	type;
    __u16	length;
    /* DWORD 1 */
    __u16	handle;
    __u8	ma_dev_addr;
    __u8	mass_id;
    /* DWORD 2 */
    __u8	status;
	__u8	flags;	
	__u16	stream_id; 
	/* DWORD 3 */
	__u16	sequ_no_req_id;
    /* DWORD 4 */
	__u16	rem_size;
}__attribute__((packed));

#endif

/**
 * struct mausb_header_basic - data pertinent to every request
 * @flag_version: identifies mausb protocol version
 * @type_subtype: identifies packet variant
 * @length: lenght of mausb packet including packet header
 * @ep_devhandle: handle of usb device or endpoint 
 * @ssid_devaddr: MA usb device address + MSS  to which MA USB device belongs
 * @status: indicates status of requested operation
 */
struct mausb_header_basic {
	__u8  flags_version;
	__u8  type_subtype;
	__u16 length;
	__u16 ep_devhandle;
	__u16 ssid_devaddr;
	__u8  status;
} __packed;

/**
 * struct mausb_header_mgmt - header for all management packets
 * @dialog: diaglog token no 
 * @reserved: fields not to be touched
 */

struct mausb_header_mgmt {
	__u16 dialog;
	__u8  reserved;
} __packed;

/**
 * struct mausb_header_non_iso - header for all non-iso data packets
 * @tflags_eps: flags and endpoint status 
 * @streamid: stream id for e-super speed protocol
 */

struct mausb_header_non_iso {
	__u8  tflags_eps;
	__u16 streamid;
	__u32 reqid_seqno;
	__u32 remsize_credit;
} __packed;

/**
 * struct mausb_header_iso - header for all iso data packets
 * @tflags_eps: flags and endpoint status 
 * @streamid: stream id for e-super speed protocol
 */

struct mausb_header_iso {
	__u8  tflags_eps;
	__u16 iflags_numheader;
	__u32 reqid_seqno;
	__u32 numseg_presentime;
	__u32 timestamp;
	__u32 mediatime_delay;
} __packed;
/**
 * struct mausb_header - common header for all mausb packets
 * @base: the basic header
 * @u: packet type dependent header
 */
struct mausb_header {
	struct mausb_header_basic base;
	union {
		struct mausb_header_mgmt	mgmt_hdr;
		struct mausb_header_non_iso	non_iso_hdr;
		//struct mausb_header_iso		iso_hdr;
	} u;
} __packed;

#if 0
/** Common header fields, per section 6.2.1 */
struct mausb_header {
    /* DWORD 0 */
    unsigned char     ver_flags;
    unsigned char     type;
    short int      length;
    /* DWORD 1 */
    short int      handle;
    unsigned char     ma_dev_addr;
    unsigned char   mass_id;
    /* DWORD 2 */
    char   status;
    union {
        short int	 token;
        struct {
            char   eps_tflags;
            short int    stream_id;
            /* DWORD 3 */
            unsigned long  seq_num; /* Note: only 24 bits used */
            unsigned char   req_id;
            /* DWORD 4 */
            unsigned long  credit;
        } s;
    } u;
}__attribute__((packed));

#endif
/* We need at least the first DWORD to determine the packet length (for TCP) */
#define MAUSB_MIN_LENGTH 4

#define MAUSB_MIN_MGMT_LENGTH 12
#define MAUSB_MIN_DATA_LENGTH 20
#define MAUSB_COMMON_LEN 9



#if 0 
/* returns the length field of the MAUSB packet */
static int mausb_get_pkt_len(packet_info *pinfo _U_, tvbuff_t *tvb, int offset)
{
    return tvb_get_letohs(tvb, offset + 2);
}
#endif 

/* Global Port Preference */
#define USB_DT_EP_SIZE		    7
#define USB_DT_SS_EP_COMP_SIZE      6
#define USB_DT_ISO_SSP_EP_COMP_SIZE 8

/* Size of EPHandleReq Descriptors */
#define MAUSB_EP_DES_SIZE 8
#define MAUSB_SS_EP_DES_SIZE 16
#define MAUSB_ISO_SSP_EP_DES_SIZE 24

/* EPHandleReq Descriptor Padding */
#define MAUSB_EP_DES_PAD         (MAUSB_EP_DES_SIZE - USB_DT_EP_SIZE)

#define MAUSB_SS_EP_DES_PAD      (MAUSB_SS_EP_DES_SIZE - \
        (USB_DT_EP_SIZE + USB_DT_SS_EP_COMP_SIZE))

#define MAUSB_ISO_SSP_EP_DES_PAD (MAUSB_ISO_SSP_EP_DES_SIZE - \
        (USB_DT_EP_SIZE + USB_DT_SS_EP_COMP_SIZE + USB_DT_ISO_SSP_EP_COMP_SIZE))


/* Size of EPHandleResp Descriptor */
#define MAUSB_SIZE_MAUSB_EP_DES 16


/* Size of EPHandleResp Descriptor */
#define MAUSB_SIZE_EP_HANDLE 2



#define INVALID_MA_USB_SESSION_STATE	129
#define INVALID_DEVICE_HANDLE			130
#define INVALID_EP_HANDLE				131
#define INVALID_EP_HANDLE_STATE			132
#define INVALID_REQUEST					133
#define MISSING_SEQUENCE_NUMBER			134 
#define TRANSFER_PENDING				135
#define TRANSFER_EP_STALL 				136
#define TRANSFER_SIZE_ERROR				137
#define TRANSFER_DATA_BUFFER_ERROR 		138
#define TRANSFER_BABBLE_DETECTED		139
#define TRANSFER_TRANSACTION_ERROR 		140
#define TRANSFER_SHORT_TRANSFER			141
#define TRANSFER_CANCELLED 				142
#define INSUFFICENT_RESOURCES			143
#define NOT_SUFFICENT_BANDWIDTH			144
#define INTERNAL_ERROR					145
#define DATA_OVERRUN					146
#define DEVICE_NOT_ACCESSED				147
#define BUFFER_OVERRUN					148
#define BUSY							149
#define DROPPED_PACKET					150
#define ISOC_TIME_EXPIRED				151
#define ISOCH_TIME_INVALID				152
#define NO_USB_PING_RESPONSE			153
#define NOT_SUPPORTED					154
#define REQUEST_DENIED					155



struct mausbreq
{
	int 	r;
	int 	SN;
	long 	RemSize;
	bool	EndOfTransferDetected;
	bool 	TransferError;
	int	LastTransferSN;
	bool    TransferComplete;
	bool    Delayed;
	int     ReqID;
	int   	RequestID; //Is this duplication. check above value ReqID SONU
	int     AckRequest;

}__attribute__((packed));
struct mausbdev
{
	/* MA USB IN transfer variables */
	int    RequestID;
	int    ActiveRequestID;
	struct mausbreq reqlist[aMaxRequestID];
	int    EarliestRequestID;
	int    SeqNumber;
	int    EarliestUnacknowledged;
	bool   Delayed;
	int    ResponseTimer;
	int    TransferRespRetryCounter;
	bool   ElasticBufferCap; // Update this based on device caps request response
	bool   PayloadDeliveryFail; 
	long   KeepAliveTimer;
	int    Error;

	/* MA USB OUT transfer variables */

	int    Occupancy; //TODO initialize this 
	int    RxBufSize;
	int    SequenceNo; // TODO can we not use SeqNumber ?
	bool   ActiveTransferReq; //TODO remember to initialize it 
	bool   DroppedNotificationCap;
	
}__attribute__((packed));

struct mausbres
{
	int RequestID;
	int SequenceNo;
	int RemainingSize;
	int Retry; //Use bitfields later
	int EndOfTransfer;
	int AckRequest;
	int Status;

}__attribute__((packed));;

struct mausback
{
	int r;
	int SN;
	int Status;

}__attribute__((packed));;


struct setup_packet
{
	__u8	BmRequest_Type;
	__u8	bRequest;
	__u16	wValue;
	__u16	wIndext;
	__u16	wLength;
}__attribute__((packed));

struct cancel_trans_resp
{
	__u16	ep_handle;
	__u16	stream_id;
	__u8	request_id;
	__u8	cancel_status;		//Only 2 bits used
	__u16	resrved;
	__u32	deliveryed_SN;		//MSB 8 bits are reserved
	__u32	deliveryed_BO;		//Deliveryed Byte Offset
}__attribute__((packed));

struct cancel_trans_req
{
	__u16	ep_handle;
	__u16	stream_id;
	__u8	request_id;
	__u8	resrved1;
	__u16	resrved2;
}__attribute__((packed));

struct mausb_mgmt_header
{
	struct mausb_header_basic 	base;
	struct mausb_header_mgmt	mgmt_hdr;
}__attribute__((packed));

struct mausb_req_resp
{
	struct mausb_header header;
	union
	{
		struct cancel_trans_req cancel_req;
		struct cancel_trans_resp cancel_resp;
	}r;
}__attribute__((packed));
int handle_mausb_pkt(struct mausb_device *ud, unsigned char *buff);
void stub_send_mausb(struct mausb_device *ud, struct mausb_header *pdu);
void mausbdev_in_stub_complete(struct urb *urb);
/*void mausb_enqueue_ret_unlink(struct stub_device *sdev,
									 	 struct stub_mausb_pal *pal,
										 __u32 seqnum,
									     __u32 status);

*/

__u32 mausb_generate_seqnum(struct mausb_header *pdu);
void mausb_dump_mausb_header(struct mausb_header *pdu);
void mausbdev_out_process_packect(struct mausb_device *ud);
void mausbdev_in_process_packect(struct mausb_device *ud);
void mausbdev_mgmt_process_packect(struct mausb_device *ud);
#endif /* __MAUSB_H */
