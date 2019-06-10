/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * anx7688 USB Type-C PD interface driver
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

#include <linux/of_gpio.h>
#include <linux/delay.h>

#include "anx7688_core.h"
#include "anx7688_mi1.h"
#include "anx7688_pd.h"
#include "anx_i2c_intf.h"
#include "anx7688_firmware.h"

#define INTERFACE_TIMEOUT 30

/* init setting for TYPE_PWR_SRC_CAP */
static u32 init_src_caps[1] = {
	PDO_FIXED(PD_VOLTAGE_5V, PD_CURRENT_500MA, PDO_FIXED_FLAGS),
};

/* init setting for TYPE_PWR_SNK_CAP */
static u32 init_snk_cap[4] = {
	PDO_FIXED(PD_VOLTAGE_5V, PD_CURRENT_500MA, PDO_FIXED_FLAGS),
	PDO_FIXED(PD_VOLTAGE_5V, PD_CURRENT_3A, PDO_FIXED_FLAGS),
	PDO_FIXED(PD_VOLTAGE_9V, PD_CURRENT_2A, PDO_FIXED_FLAGS),
	PDO_FIXED(PD_VOLTAGE_9V, PD_CURRENT_3A, PDO_FIXED_FLAGS)
};

/* init setting for TYPE_SVID */
static u8 init_svid[4] = {0x00, 0x00, 0x01, 0xFF};

/* init setting for TYPE_DP_SNK_IDENTITY */
/*
 * Sink ID Header
 * Vendor ID 0x00, 0x00
 * Reserved  0x00,
 * Host Cap | Device Cap | Product Typ | Model Oper | Reserved
 *   7b          6b         5b~3b            2b         1b0b
 *
 * Sink Cert Stat VDO
 * 0x00, 0x00, 0x00, 0x00,
 * Sink Product VDO *
 * 0x00, 0xAB, 0x88, 0x76,
 */
#if defined(CONFIG_LGE_USB_TYPE_C)
static u8 init_snk_ident[12] = {
	0x04, 0x10, 0x00, 0xD0, /*snk_id HEADER */
	0x00, 0x00, 0x00, 0x00, /*snk_certify VDO */
	0x01, 0x00, 0x00, 0x00, /*snk_product VDO*/
};

#else
static u8 init_snk_ident[16] = {
	0x00, 0x00, 0x00, 0xec, /*snk_id_hdr */
	0x00, 0x00, 0x00, 0x00, /*snk_cert */
	0x00, 0x00, 0x00, 0x00, /*snk_prd*/
	0x39, 0x00, 0x00, 0x51  /*snk_ama*/
};
#endif

u8 pd_src_pdo_cnt = 2;
u8 pd_src_pdo[VDO_SIZE] = {
	/*5V 0.9A , 5V 1.5 */
	0x5A, 0x90, 0x01, 0x2A, 0x96, 0x90, 0x01, 0x2A
};

u8 pd_snk_pdo_cnt = 3;
u8 pd_snk_pdo[VDO_SIZE];
u8 pd_rdo[PD_ONE_DATA_OBJECT_SIZE];
u8 DP_caps[PD_ONE_DATA_OBJECT_SIZE];
u8 configure_DP_caps[PD_ONE_DATA_OBJECT_SIZE];
u8 src_dp_caps[PD_ONE_DATA_OBJECT_SIZE];

char *interface_to_str(unsigned char header_type)
{
	return (header_type == TYPE_PWR_SRC_CAP) ? "src cap" :
		(header_type == TYPE_PWR_SNK_CAP) ? "snk cap" :
		(header_type == TYPE_PWR_OBJ_REQ) ? "RDO" :
		(header_type == TYPE_DP_SNK_IDENTITY) ? "snk identity" :
		(header_type == TYPE_SVID) ? "svid" :
		(header_type == TYPE_PSWAP_REQ) ? "PR_SWAP" :
		(header_type == TYPE_DSWAP_REQ) ? "DR_SWAP" :
		(header_type == TYPE_GOTO_MIN_REQ) ? "GOTO_MIN" :
		(header_type == TYPE_DP_ALT_ENTER) ? "DPALT_ENTER" :
		(header_type == TYPE_DP_ALT_EXIT) ? "DPALT_EXIT" :
		(header_type == TYPE_VCONN_SWAP_REQ) ? "VCONN_SWAP" :
		(header_type == TYPE_GET_DP_SNK_CAP) ? "GET_SINK_DP_CAP" :
		(header_type == TYPE_DP_SNK_CFG) ? "dp cap" :
		(header_type == TYPE_SOFT_RST) ? "Soft Reset" :
		(header_type == TYPE_HARD_RST) ? "Hard Reset" :
		(header_type == TYPE_RESTART) ? "Restart" :
		(header_type == TYPE_PD_STATUS_REQ) ? "PD Status" :
		(header_type == TYPE_ACCEPT) ? "ACCEPT" :
		(header_type == TYPE_REJECT) ? "REJECT" :
		(header_type == TYPE_VDM) ? "VDM" :
		(header_type ==
		 TYPE_RESPONSE_TO_REQ) ? "Response to Request" : "Unknown";
}

inline unsigned char cac_checksum(unsigned char *pSendBuf, unsigned char len)
{
	unsigned char i;
	unsigned char sum = 0;

	for (i = 0; i < len; i++)
		sum += *(pSendBuf + i);

	return (u8) (0 - sum);
}

u8 interface_send_msg(struct anx7688_chip *chip, u8 type, u8 *pbuf,
						u8 len, int timeout_ms)
{
	struct device *cdev = &chip->client->dev;
	unsigned char c;
	unsigned char sending_len;
	unsigned char WriteDataBuf[32];

	/* full, return 0 */
	WriteDataBuf[0] = len + 1; /* cmd */
	WriteDataBuf[1] = type;
	memcpy(WriteDataBuf + 2, pbuf, len);

	/* cmd + checksum */
	WriteDataBuf[len + 2] = cac_checksum(WriteDataBuf, len + 1 + 1);

	sending_len = WriteDataBuf[0] + 2;

	c = OhioReadReg(TCPC_ADDR, InterfaceSendBuf_Addr);
	if (c == 0) {
		OhioWriteBlockReg(TCPC_ADDR, (InterfaceSendBuf_Addr + 0x01),
					(sending_len - 1), &WriteDataBuf[1]);
		OhioWriteReg(TCPC_ADDR, InterfaceSendBuf_Addr, WriteDataBuf[0]);
	} else {
		dev_err(cdev, "tx buf full\n");
	}

	return 0;
}

u8 send_rdo(struct anx7688_chip *chip, const u8 *rdo, u8 size)
{
	struct device *cdev = &chip->client->dev;
	u8 i;

	dev_dbg(cdev, "%s\n", __func__);
	if (rdo == NULL) {
		dev_err(cdev, "RDO NULL Check Fail\n");
		return CMD_FAIL;
	}
	if ((size % PD_ONE_DATA_OBJECT_SIZE) != 0 ||
			(size / PD_ONE_DATA_OBJECT_SIZE) > PD_MAX_DATA_OBJECT_NUM) {
		dev_err(cdev, "RDO Size Check Fail\n");
		return CMD_FAIL;
	}
	for (i = 0; i < size; i++)
		pd_rdo[i] = *rdo++;

	return interface_send_msg(chip, TYPE_PWR_OBJ_REQ, pd_rdo, size,
			INTERFACE_TIMEOUT);
}

u8 send_dp_snk_cfg(struct anx7688_chip *chip, const u8 *dp_snk_caps, u8 dp_snk_caps_size)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s\n", __func__);

	memcpy(configure_DP_caps, dp_snk_caps, dp_snk_caps_size);
	/*configure sink cap */
	return interface_send_msg(chip, TYPE_DP_SNK_CFG, configure_DP_caps,
			4, INTERFACE_TIMEOUT);
}

u8 send_vdm(struct anx7688_chip *chip, const u8 *vdm, u8 size)
{
	struct device *cdev = &chip->client->dev;
	u8 tmp[32] = {0 };

	dev_dbg(cdev, "%s\n", __func__);
	if (vdm == NULL) {
		dev_err(cdev, "VDM NULL Check Fail\n");
		return CMD_FAIL;
	}
	if (size > 3 && size < 32) {
		memcpy(tmp, vdm, size);
		if (tmp[2] == 0x01 && tmp[3] == 0x00) {
			tmp[3] = 0x40;
			return interface_send_msg(chip, TYPE_VDM, tmp, size,
					INTERFACE_TIMEOUT);
		}
	}

	return 1;
}

u8 send_src_cap(struct anx7688_chip *chip, const u8 *src_caps, u8 src_caps_size)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s\n", __func__);
	if (src_caps == NULL) {
		dev_err(cdev, " SRC Cap NULL Check Fail\n");
		return CMD_FAIL;
	}
	if ((src_caps_size % PD_ONE_DATA_OBJECT_SIZE) != 0 ||
			(src_caps_size / PD_ONE_DATA_OBJECT_SIZE) >
			PD_MAX_DATA_OBJECT_NUM) {
		dev_err(cdev, "SRC Cap NULL Check Fail\n");
		return CMD_FAIL;
	}
	memcpy(pd_src_pdo, src_caps, src_caps_size);
	pd_src_pdo_cnt = src_caps_size / PD_ONE_DATA_OBJECT_SIZE;

	/*send source capabilities message to Ohio really */
	return interface_send_msg(chip, TYPE_PWR_SRC_CAP, pd_src_pdo,
			pd_src_pdo_cnt *
			PD_ONE_DATA_OBJECT_SIZE,
			INTERFACE_TIMEOUT);
}

u8 send_svid(struct anx7688_chip *chip, const u8 *svid, u8 size)
{
	struct device *cdev = &chip->client->dev;

	u8 tmp[4] = {0 };
	dev_dbg(cdev, "%s\n", __func__);
	if (svid == NULL || size != 4) {
		dev_err(cdev, "SVID NULL Check Fail\n");
		return CMD_FAIL;
	}
	memcpy(tmp, svid, size);
	return interface_send_msg(chip, TYPE_SVID, tmp, size,
			INTERFACE_TIMEOUT);
}

u8 send_snk_cap(struct anx7688_chip *chip, const u8 *snk_caps,
						u8 snk_caps_size)
{
	struct device *cdev = &chip->client->dev;

	dev_dbg(cdev, "%s\n", __func__);
	memcpy(pd_snk_pdo, snk_caps, snk_caps_size);
	pd_snk_pdo_cnt = snk_caps_size / PD_ONE_DATA_OBJECT_SIZE;

	/*configure sink cap */
	return interface_send_msg(chip, TYPE_PWR_SNK_CAP, pd_snk_pdo,
			pd_snk_pdo_cnt * 4,
			INTERFACE_TIMEOUT);
}

u8 send_pd_msg(struct anx7688_chip *chip,PD_MSG_TYPE type,
				const char *buf, u8 size)
{
	struct device *cdev = &chip->client->dev;
	u8 rst = 0;

	dev_dbg(cdev,"%s: type = %s\n", __func__, interface_to_str(type));
	switch (type) {
	case TYPE_PWR_SRC_CAP:
		rst = send_src_cap(chip, buf, size);
		break;
	case TYPE_PWR_SNK_CAP:
		rst = send_snk_cap(chip, buf, size);
		break;
	case TYPE_DP_SNK_IDENTITY:
		rst = interface_send_msg(chip, TYPE_DP_SNK_IDENTITY,
				(u8 *)buf, size, INTERFACE_TIMEOUT);
		break;
	case TYPE_SVID:
		rst = send_svid(chip, buf, size);
		break;
	case TYPE_GET_DP_SNK_CAP:
		rst = interface_send_msg(chip, TYPE_GET_DP_SNK_CAP,
					NULL, 0, INTERFACE_TIMEOUT);
		break;
	case TYPE_PSWAP_REQ:
		rst = interface_pr_swap(chip);
		break;
	case TYPE_DSWAP_REQ:
		rst = interface_dr_swap(chip);
		break;
	case TYPE_GOTO_MIN_REQ:
		rst = interface_send_gotomin(chip);
		break;
	case TYPE_VDM:
		rst = send_vdm(chip, buf, size);
		break;
	case TYPE_DP_SNK_CFG:
		rst = send_dp_snk_cfg(chip, buf, size);
		break;
	case TYPE_PWR_OBJ_REQ:
		rst = send_rdo(chip, buf, size);
		break;
	case TYPE_ACCEPT:
		rst = interface_send_accept(chip);
		break;
	case TYPE_REJECT:
		rst = interface_send_reject(chip);
		break;
	case TYPE_SOFT_RST:
		rst = interface_send_soft_rst(chip);
		break;
	case TYPE_HARD_RST:
		rst = interface_send_hard_rst(chip);
		break;
	default:
		dev_dbg(cdev, "unknown type %x\n", type);
		rst = 0;
		break;
	}
	if (rst == CMD_FAIL) {
		dev_err(cdev, "cmd %x fail.\n", type);
		return CMD_FAIL;
	}

	return rst;
}

#define INIT_SET_MAXCOUNT 20
void anx7688_send_init_setting(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	unsigned char send_init_setting_state;
	unsigned char c;
	int try_cnt = 0;

	dev_dbg(cdev,"%s\n", __func__);
	send_init_setting_state = 1;

	do{
		if (!atomic_read(&chip->power_on)) {
			return;
		}

		switch(send_init_setting_state) {
		default:
		case 0:
			break;
		case 1:
			/* send TYPE_PWR_SRC_CAP init setting */
			send_pd_msg(chip, TYPE_PWR_SRC_CAP,
					(const char *)init_src_caps,
					sizeof(init_src_caps));
#ifdef CONFIG_LGE_USB_TYPE_C
			memcpy(chip->src_pdo, init_src_caps, sizeof(init_src_caps));
#endif
			send_init_setting_state++;
			break;
		case 2:
			c = OhioReadReg(TCPC_ADDR, InterfaceSendBuf_Addr);
			if (c == 0) {
				send_init_setting_state++;
				try_cnt = 0;
			} else {
				dev_dbg(cdev, "%s: state %d, failed value %d\n",
						__func__,
						send_init_setting_state, c);
				if (try_cnt > INIT_SET_MAXCOUNT) {
					dev_err(cdev, "timeout init set %d\n",
						send_init_setting_state);
					return;
				}

				try_cnt++;
				mdelay(1);
			}
			break;
		case 3:
			/* send TYPE_PWR_SNK_CAP init setting */
			send_pd_msg(chip, TYPE_PWR_SNK_CAP,
					(const char *)init_snk_cap,
					sizeof(init_snk_cap));
			send_init_setting_state++;
			break;
		case 4:
			c = OhioReadReg(TCPC_ADDR, InterfaceSendBuf_Addr);
			if (c == 0) {
				send_init_setting_state++;
				try_cnt = 0;
			} else {
				dev_dbg(cdev, "%s: state %d, failed value %d\n",
						__func__,
						send_init_setting_state, c);
				if (try_cnt > INIT_SET_MAXCOUNT) {
					dev_err(cdev, "timeout init set %d\n",
						send_init_setting_state);
					return;
				}

				try_cnt++;
				mdelay(1);
			}
			break;
		case 5:
			/* send TYPE_DP_SNK_IDENTITY init setting */
			send_pd_msg(chip, TYPE_DP_SNK_IDENTITY,
					init_snk_ident,
					sizeof(init_snk_ident));
			send_init_setting_state++;
			break;
		case 6:
			c = OhioReadReg(TCPC_ADDR, InterfaceSendBuf_Addr);
			if (c == 0){
				send_init_setting_state++;
				try_cnt = 0;
			} else {
				dev_dbg(cdev, "%s: state %d, failed value %d\n",
						__func__,
						send_init_setting_state, c);

				if (try_cnt > INIT_SET_MAXCOUNT) {
					dev_err(cdev, "timeout init set %d\n",
						send_init_setting_state);
					return;
				}

				try_cnt++;
				mdelay(1);
			}
			break;
		case 7:
			/* send TYPE_SVID init setting */
			send_pd_msg(chip, TYPE_SVID, init_svid,
						sizeof(init_svid));
			send_init_setting_state++;
			break;
		case 8:
			c = OhioReadReg(TCPC_ADDR, InterfaceSendBuf_Addr);
			if (c == 0){
				send_init_setting_state++;
				try_cnt = 0;
			} else {
				dev_dbg(cdev, "%s: state %d, failed value %d\n",
						__func__,
						send_init_setting_state, c);

				if (try_cnt > INIT_SET_MAXCOUNT) {
					dev_err(cdev, "timeout init set %d\n",
							send_init_setting_state);
					return;
				}

				try_cnt++;
				mdelay(1);
			}
			break;
		case 9:
			send_init_setting_state = 0;
			break;
		}
	} while(send_init_setting_state != 0);

}

static int handle_response_to_req(struct anx7688_chip *chip, void *data)
{
	struct device *cdev = &chip->client->dev;
	response_t *resp = (response_t *)data;

	if (resp->status == RESPONSE_STATUS_SUCCESS)
		dev_dbg(cdev, "%s: success\n", __func__);

	if (resp->status != RESPONSE_STATUS_SUCCESS) {
		dev_err(cdev, "%s: \"%s\" %s%s%s\n", __func__,
				interface_to_str(resp->req_type),
				resp->status == RESPONSE_STATUS_REJECT ?
				"reject" : "",
				resp->status == RESPONSE_STATUS_FAILURE ?
				"failure" : "",
				resp->status == RESPONSE_STATUS_BUSY ?
				"busy" : "");
	}

	return 0;
}

static int handle_dp_alt_enter(struct anx7688_chip *chip)
{
	anx7688_sbu_ctrl(chip, false);
#ifdef CONFIG_LGE_DP_ANX7688
	if (chip->dp_status == ANX_DP_DISCONNECTED) {
		chip->dp_status = ANX_DP_CONNECTED;
		wake_lock(&chip->dp_lock);
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
		slimport_set_hdmi_hpd(1);
		pr_info("%s:set hdmi hpd on\n", __func__);
#endif
	}
#endif
	return 0;
}

static int handle_dp_alt_exit(struct anx7688_chip *chip)
{
#ifdef CONFIG_LGE_DP_ANX7688
	if (chip->dp_status != ANX_DP_DISCONNECTED) {
		chip->dp_status = ANX_DP_DISCONNECTED;
		wake_unlock(&chip->dp_lock);
		wake_lock_timeout(&chip->dp_lock, 2*HZ);
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
		slimport_set_hdmi_hpd(0);
		pr_info("%s:set hdmi hpd off\n", __func__);
#endif
	}
#endif
	return 0;
}

#ifdef CONFIG_LGE_USB_TYPE_C
uint32_t get_data_object(uint8_t *obj_data)
{
	return((((uint32_t)obj_data[3]) << 24) |
		   (((uint32_t)obj_data[2]) << 16) |
		   (((uint32_t)obj_data[1]) << 8) |
		   (((uint32_t)obj_data[0])));
}
#endif

u8 dispatch_rcvd_pd_msg(struct anx7688_chip *chip, PD_MSG_TYPE type, void *data, u8 len)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
#ifdef CONFIG_LGE_USB_TYPE_C
	uint8_t *buf = (uint8_t *) data;
	int i;
#endif

	dev_dbg(cdev,"%s: msg type : %s\n", __func__, interface_to_str(type));

	switch (type) {
	case TYPE_PWR_SRC_CAP:
#ifdef CONFIG_LGE_USB_TYPE_C
		for (i = 0 ; i < len >> 2 ; i++)
		{
			if(i < PD_MAX_PDO_NUM)
				chip->offered_pdo[i] = get_data_object(&buf[i << 2]);
		}
#endif
		/* TODO: need to vbus off adjust */
		OhioWriteReg(USBC_ADDR, USBC_VBUS_DELAY_TIME, 0xA0);
		break;
	case TYPE_PWR_SNK_CAP:
		break;
	case TYPE_DP_SNK_IDENTITY:
		break;
	case TYPE_SVID:
		break;
	case TYPE_GET_DP_SNK_CAP:
		break;
	case TYPE_ACCEPT:
		break;
	case TYPE_REJECT:
		break;
	case TYPE_PWR_OBJ_REQ:
#ifdef CONFIG_LGE_USB_TYPE_C
		if(len == sizeof(chip->offered_rdo))
			chip->offered_rdo = get_data_object(buf);
#endif
		/* TODO: need to vbus offadjust */
		OhioWriteReg(USBC_ADDR, USBC_VBUS_DELAY_TIME, 0xA0);
		break;
	case TYPE_DSWAP_REQ:
		break;
	case TYPE_PSWAP_REQ:
		break;
	case TYPE_GOTO_MIN_REQ:
		break;
	case TYPE_VCONN_SWAP_REQ:
		break;
	case TYPE_VDM:
		break;
	case TYPE_DP_SNK_CFG:
		break;
	case TYPE_PD_STATUS_REQ:
		break;
	case TYPE_RESPONSE_TO_REQ:
		rc = handle_response_to_req(chip, data);
		break;
	case TYPE_DP_ALT_ENTER:
		rc = handle_dp_alt_enter(chip);
		break;
	case TYPE_DP_ALT_EXIT:
		rc = handle_dp_alt_exit(chip);
		break;
	case TYPE_SOFT_RST:
		break;
	case TYPE_HARD_RST:
		break;
	case TYPE_RESTART:
		break;
	default:
		rc = -ENOMSG;
		dev_err(cdev, "unknown msg type: %x\n", type);
		break;
	}

	if (rc == -ENOSYS) {
		dev_err(cdev, "\"%s\" not implemented\n",
				interface_to_str(type));
		print_hex_dump(KERN_ERR, "msg:",
				DUMP_PREFIX_OFFSET, 16, 1,
				data, len, 0);
	}

	return rc;
}

int anx7688_pd_process(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	pd_msg_t *msg;
	u8 read_data_buf[32];
	unsigned char receiving_len;
	unsigned char i;
	int rc;

	rc = OhioReadBlockReg(TCPC_ADDR, InterfaceRecvBuf_Addr,
				32, (unsigned char *)read_data_buf);
	msg = (pd_msg_t *)read_data_buf;

	dev_dbg(cdev, " msg_header length = %x\n", msg->hdr.length);
	if (msg->hdr.length != 0){
		OhioWriteReg(TCPC_ADDR, InterfaceRecvBuf_Addr, 0);

		receiving_len = 0;
		for(i = 0; i < msg->hdr.length + 2; i++) {
			receiving_len += read_data_buf[i];
		}
		if (receiving_len == 0) {
			dev_dbg(cdev, "%s: >>%s\n", __func__,
					interface_to_str(msg->hdr.type));
			dispatch_rcvd_pd_msg(chip,
					(PD_MSG_TYPE)msg->hdr.type,
					&(msg->data), msg->hdr.length - 1);
			return CMD_SUCCESS;
		} else {
			dev_err(cdev,"%s: checksum error! n", __func__);
			return CMD_FAIL;
		}
	}
	return CMD_FAIL;
}
