/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * anx7688 USB Type-C Controller driver
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
#ifndef __ANX7688_PD_H__
#define __ANX7688_PD_H__

#define InterfaceSendBuf_Addr 0x30
#define InterfaceRecvBuf_Addr 0x51

typedef enum {
	TYPE_PWR_SRC_CAP = 0x00,
	TYPE_PWR_SNK_CAP = 0x01,
	TYPE_DP_SNK_IDENTITY = 0x02,
	TYPE_SVID = 0x03,
	TYPE_GET_DP_SNK_CAP = 0x04,
	TYPE_ACCEPT = 0x05,
	TYPE_REJECT = 0x06,
	TYPE_PSWAP_REQ = 0x10,
	TYPE_DSWAP_REQ = 0x11,
	TYPE_GOTO_MIN_REQ = 0x12,
	TYPE_VCONN_SWAP_REQ = 0x13,
	TYPE_VDM = 0x14,
	TYPE_DP_SNK_CFG = 0x15,
	TYPE_PWR_OBJ_REQ = 0x16,
	TYPE_PD_STATUS_REQ = 0x17,
	TYPE_DP_ALT_ENTER = 0x19,
	TYPE_DP_ALT_EXIT = 0x1A,
	TYPE_RESPONSE_TO_REQ = 0xF0,
	TYPE_SOFT_RST = 0xF1,
	TYPE_HARD_RST = 0xF2,
	TYPE_RESTART = 0xF3
} PD_MSG_TYPE;

typedef struct {
	u8 length;
	u8 type;
} pd_msg_hdr_t;

typedef struct {
	pd_msg_hdr_t hdr;
	u8 data[30];
} pd_msg_t;

/* Response */
typedef enum {
	RESPONSE_STATUS_SUCCESS = 0x00,
	RESPONSE_STATUS_REJECT = 0x01,
	RESPONSE_STATUS_FAILURE = 0x02,
	RESPONSE_STATUS_BUSY = 0x03
} response_status_t;

typedef struct {
	u8 req_type;
	u8 status;
} response_t;

/*Comands status*/
enum interface_status {
	CMD_SUCCESS,
	CMD_REJECT,
	CMD_FAIL,
	CMD_BUSY,
	CMD_STATUS
};

#define PD_VOLTAGE_5V          5000
#define PD_VOLTAGE_9V          9000

#define PD_MAX_VOLTAGE_20V     20000
#define PD_MAX_VOLTAGE_21V     21000

#define PD_CURRENT_500MA       500
#define PD_CURRENT_900MA       900
#define PD_CURRENT_1500MA      1500
#define PD_CURRENT_2A          2000
#define PD_CURRENT_3A          3000

#define PD_POWER_15W           15000
#define PD_POWER_60W           60000

/* RDO : Request Data Object */
#define RDO_OBJ_POS(n)             (((u32)(n) & 0x7) << 28)
#define RDO_POS(rdo)               ((((32)rdo) >> 28) & 0x7)
#define RDO_GIVE_BACK              ((u32)1 << 27)
#define RDO_CAP_MISMATCH           ((u32)1 << 26)
#define RDO_COMM_CAP               ((u32)1 << 25)
#define RDO_NO_SUSPEND             ((u32)1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  (((((u32)ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) (((((u32)ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      (((((u32)mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     (((((u32)mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags)	\
	(RDO_OBJ_POS(n) | (flags) |		\
	RDO_FIXED_VAR_OP_CURR(op_ma) |		\
	RDO_FIXED_VAR_MAX_CURR(max_ma))

#define PDO_TYPE_FIXED ((u32)0 << 30)
#define PDO_TYPE_BATTERY ((u32)1 << 30)
#define PDO_TYPE_VARIABLE ((u32)2 << 30)
#define PDO_TYPE_MASK ((u32)3 << 30)
#define PDO_FIXED_DUAL_ROLE ((u32)1 << 29)      /* Dual role device */
#define PDO_FIXED_SUSPEND ((u32)1 << 28)        /* USB Suspend supported */
#define PDO_FIXED_EXTERNAL ((u32)1 << 27)       /* Externally powered */
#define PDO_FIXED_COMM_CAP ((u32)1 << 26)       /* USB Communications Capable */
#define PDO_FIXED_DATA_SWAP ((u32)1 << 25)      /* Data role swap command */
#define PDO_FIXED_PEAK_CURR ((u32)1 << 20)      /* [21..20] Peak current */
/* Voltage in 50mV units */
#define PDO_FIXED_VOLT(mv) (u32)((((u32)mv)/50) << 10)
/* Max current in 10mA units */
#define PDO_FIXED_CURR(ma) (u32)((((u32)ma)/10))

/*build a fixed PDO packet*/
#define PDO_FIXED(mv, ma, flags) \
        (PDO_FIXED_VOLT(mv)\
        | PDO_FIXED_CURR(ma)\
        | (flags))

#define GET_PDO_TYPE(PDO) ((PDO & PDO_TYPE_MASK) >> 30)
#define GET_PDO_FIXED_DUAL_ROLE(PDO) ((PDO & PDO_FIXED_DUAL_ROLE) >> 29)
#define GET_PDO_FIXED_SUSPEND(PDO) ((PDO & PDO_FIXED_SUSPEND) >> 28)
#define GET_PDO_FIXED_EXTERNAL(PDO) ((PDO & PDO_FIXED_EXTERNAL) >> 27)
#define GET_PDO_FIXED_COMM_CAP(PDO) ((PDO & PDO_FIXED_COMM_CAP) >> 26)
#define GET_PDO_FIXED_DATA_SWAP(PDO) ((PDO & PDO_FIXED_DATA_SWAP) >> 25)
#define GET_PDO_FIXED_PEAK_CURR(PDO) ((PDO >> 20) & 0x03)

#define GET_PDO_FIXED_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_PDO_FIXED_CURR(PDO) ((PDO & 0x3FF) * 10)
#define GET_VAR_MAX_VOLT(PDO) (((PDO >> 20) & 0x3FF) * 50)
#define GET_VAR_MIN_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_VAR_MAX_CURR(PDO) ((PDO & 0x3FF) * 10)
#define GET_BATT_MAX_VOLT(PDO) (((PDO >> 20) & 0x3FF) * 50)
#define GET_BATT_MIN_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_BATT_OP_POWER(PDO) (((PDO) & 0x3FF) * 250)

#define PD_ONE_DATA_OBJECT_SIZE  4
#define PD_MAX_DATA_OBJECT_NUM  7
#define VDO_SIZE (PD_ONE_DATA_OBJECT_SIZE * PD_MAX_DATA_OBJECT_NUM)

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP)

#define interface_pr_swap(chip) \
        interface_send_msg(chip, TYPE_PSWAP_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_dr_swap(chip) \
        interface_send_msg(chip, TYPE_DSWAP_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_vconn_swap(chip) \
        interface_send_msg(chip, TYPE_VCONN_SWAP_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_get_dp_caps(chip) \
        interface_send_msg(chip, TYPE_GET_DP_SNK_CAP, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_gotomin(chip) \
        interface_send_msg(chip, TYPE_GOTO_MIN_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_soft_rst(chip) \
        interface_send_msg(chip, TYPE_SOFT_RST, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_hard_rst(chip) \
        interface_send_msg(chip, TYPE_HARD_RST, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_restart(chip) \
        interface_send_msg(chip, TYPE_RESTART, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_accept(chip) \
        interface_send_msg(chip, TYPE_ACCEPT, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_reject(chip) \
        interface_send_msg(chip, TYPE_REJECT, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_dp_enter(chip) \
        interface_send_msg(chip, TYPE_DP_ALT_ENTER, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_dp_exit(chip) \
        interface_send_msg(chip, TYPE_DP_ALT_EXIT, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_src_cap(chip) \
        interface_send_msg(chip, TYPE_PWR_SRC_CAP, pd_src_pdo,\
        pd_src_pdo_cnt * 4, INTERFACE_TIMEOUT)
#define interface_send_snk_cap(chip) \
        interface_send_msg(chip, TYPE_PWR_SNK_CAP, pd_snk_pdo,\
        pd_snk_pdo_cnt * 4, INTERFACE_TIMEOUT)
#define interface_send_src_dp_cap(chip) \
        interface_send_msg(chip, TYPE_DP_SNK_IDENTITY, src_dp_caps,\
        4, INTERFACE_TIMEOUT)
#define interface_config_dp_caps(chip) \
        interface_send_msg(chip, TYPE_DP_SNK_CFG, configure_DP_caps,\
        4, INTERFACE_TIMEOUT)
#define interface_send_request(chip) \
        interface_send_msg(chip, TYPE_PWR_OBJ_REQ, pd_rdo,\
        4, INTERFACE_TIMEOUT)
#define interface_send_vdm_data(chip, buf, len)       \
        interface_send_msg(chip, TYPE_VDM, buf, len, INTERFACE_TIMEOUT)

#endif /* __ANX7688_CORE_H__ */
