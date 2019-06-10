#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#include "anx7418.h"
#include "anx7418_pd.h"
#include "anx7418_charger.h"

static pdo_t pwr_snk_cap[] = {
	[0].fixed = {
		.curr = PD_CURR(3000),
		.volt = PD_VOLT(5000),
		.data_swap = 1,
		.comm_cap = 1,
		.dual_role = 1,
		.type = PDO_TYPE_FIXED,
	},
	[1].fixed = {
		.curr = PD_CURR(2000),
		.volt = PD_VOLT(9000),
		.data_swap = 1,
		.comm_cap = 1,
		.dual_role = 1,
		.type = PDO_TYPE_FIXED,
	}
};

static pdo_t pwr_src_cap[] = {
	[0].fixed = {
		.curr = PD_CURR(500),
		.volt = PD_VOLT(5000),
		.data_swap = 1,
		.comm_cap = 1,
		.dual_role = 1,
		.type = PDO_TYPE_FIXED,
	},
};

static u8 dp_snk_identity[] = {
	0x00, 0x00, 0x00, 0xEC, /*snk_id_hdr */
	0x00, 0x00, 0x00, 0x00, /*snk_cert */
	0x00, 0x00, 0x00, 0x00, /*snk_prd*/
	0x39, 0x00, 0x00, 0x51  /*5snk_ama*/
};

static u8 svid[] = {0x00, 0x00, 0x01, 0xFF};

static const char *pd_type_string(pd_type type)
{
	static const char *const names[] = {
		[PD_TYPE_PWR_SRC_CAP] = "Power source capability",
		[PD_TYPE_PWR_SNK_CAP] = "Power sink capability",
		[PD_TYPE_DP_SNK_IDENTITY] = "DP sink's identity",
		[PD_TYPE_SVID] = "SVID",
		[PD_TYPE_GET_DP_SNK_CFG] = "DP sink configuration",
		[PD_TYPE_ACCEPT] = "Accept a request",
		[PD_TYPE_REJECT] = "Reject a request",
		[PD_TYPE_PSWAP_REQ] = "Power role swap request",
		[PD_TYPE_DSWAP_REQ] = "Data role swap request",
		[PD_TYPE_GOTO_MIN_REQ] = "GoTo_Min request",
		[PD_TYPE_VCONN_SWAP_REQ] = "VCONN swap request",
		[PD_TYPE_VDM] = "VDM",
		[PD_TYPE_DP_SNK_CFG] = "DP pin assignment",
		[PD_TYPE_PWR_OBJ_REQ] = "Power object position request",
		[PD_TYPE_PD_STATUS_REQ] = "PD status",
		[PD_TYPE_DP_ALT_ENTER] = "entered DP Alt Mode",
		[PD_TYPE_DP_ALT_EXIT] = "exited DP Alt Mode",
		[PD_TYPE_RESPONSE_TO_REQ] = "Response to a prior request",
		[PD_TYPE_SOFT_RST] = "Soft reset",
		[PD_TYPE_HARD_RST] = "Hard reset",
		[PD_TYPE_RESTART] = "Restart",
	};

	if (type < 0 || type >= ARRAY_SIZE(names))
		return "UNDEFINED";

	return names[type];
}

static u8 make_checksum(const u8 *buf, size_t len)
{
	u8 sum = 0;
	while (len--) {
		sum += buf[len];
	}
	return 0 - sum;
}

static int send_pd_msg(struct i2c_client *client, const u8 *buf, size_t len)
{
	int front;
	int rear;
	const u8 *p = buf;
	int tmp_len;
	int remain = len;


	front = __anx7418_read_reg(client, TX_PTR_FRONT);
	rear = __anx7418_read_reg(client, TX_PTR_REAR);

	if (front != rear)
		return 0;

	tmp_len = (len < 7) ? len : 7;

	if ((rear + tmp_len) > 7) {
		tmp_len = 8 - rear;
		__anx7418_write_block_reg(client, TX_DATA0 + rear, tmp_len, p);
		p += tmp_len;
		remain -= tmp_len;

		front--;
		tmp_len = min(front, remain);
		if (tmp_len > 0) {
			__anx7418_write_block_reg(client, TX_DATA0, tmp_len, p);
			remain -= tmp_len;
		}
	} else {
		__anx7418_write_block_reg(client, TX_DATA0 + rear, tmp_len, p);
		remain -= tmp_len;
	}

	rear = (rear + (len - remain)) & 7;
	__anx7418_write_reg(client, TX_PTR_REAR, rear);

	return len - remain;
}

int anx7418_send_pd_msg(struct i2c_client *client, u8 type,
		const u8 *buf, size_t len, unsigned long timeout)
{
	struct device *cdev = &client->dev;
	unsigned long expire;
	pd_msg_t msg;
	const u8 *p;
	int remain;
	unsigned retry;
	int rc;

	dev_dbg(cdev, "send_pd: %s\n", pd_type_string(type));
	anx_dbg_print("PD SEND", type, pd_type_string(type));

	msg.hdr.length = len + 1;
	msg.hdr.type = type;
	memcpy(msg.data, buf, len);
	msg.data[len] = make_checksum((const u8 *)&msg,
			sizeof(pd_msg_hdr_t) + len);
	p = (const u8 *)&msg;

	remain = sizeof(msg.hdr) + msg.hdr.length;
	retry = 0;
	expire = timeout + jiffies;

	anx7418_i2c_lock(client);
	while (remain > 0) {
		rc = send_pd_msg(client, p, remain);
		p += rc;
		remain -= rc;

		if (time_before(expire, jiffies)) {
			if (remain == 0)
				break;

			anx_dbg_print("PD SEND", retry, "DATA TIMEOUT");
			dev_err(cdev, "send_pd: '%s' data timeout. remain %d, retry %d\n",
					pd_type_string(type), remain, retry);
			len = -ETIME;
			goto err;
		}
		retry++;
	}

	retry = 0;
	expire = timeout + jiffies;
	for(;;) {
		rc = __anx7418_read_reg(client, TX_STATUS);

		if (rc & STATUS_SUCCESS)
			break;
		else if (rc & STATUS_ERROR) {
			anx_dbg_print("PD SEND", rc, "ACK ERR");
			dev_err(cdev, "send_pd: '%s' ack err %x\n",
					pd_type_string(type), rc);
			__anx7418_write_reg(client, TX_STATUS, 0);
			len = -EREMOTEIO;
			goto err;
		}

		if (time_before(expire, jiffies)) {
			anx_dbg_print("PD SEND", retry, "STATUS TIMEOUT");
			dev_err(cdev, "send_pd: '%s' status timeout %x\n",
					pd_type_string(type), rc);
			len = -ETIME;
			goto err;
		}
		retry++;
	}

	anx_dbg_print("PD SEND", 0, "DONE");
err:
	anx7418_i2c_unlock(client);
#ifdef PD_DEBUG
	BUG_ON(len < 0);
#endif
	return len;
}
EXPORT_SYMBOL(anx7418_send_pd_msg);

static int recv_pd_msg(struct i2c_client *client, u8 *buf, size_t len)
{
	int front;
	int rear;
	u8 *p = buf;
	int tmp_len;
	int remain = len;
	int rc;

	front = __anx7418_read_reg(client, RX_PTR_FRONT);
	rear = __anx7418_read_reg(client, RX_PTR_REAR);

	if (front == rear) {
		return 0;

	} else if (front < rear) {
		tmp_len = min(rear - front, remain);
		rc = __anx7418_read_block_reg(client, RX_DATA0 + front, tmp_len, p);
		remain -= rc;

	} else {
		tmp_len = min(8 - front, remain);
		rc = __anx7418_read_block_reg(client, RX_DATA0 + front, tmp_len, p);
		p += rc;
		remain -= rc;

		tmp_len = min(rear, remain);
		if (tmp_len > 0) {
			rc = __anx7418_read_block_reg(client, RX_DATA0, tmp_len, p);
			remain -= rc;
		}
	}

	front = (front + (len - remain)) & 7;
	rc = __anx7418_write_reg(client, RX_PTR_FRONT, front);

	return len - remain;
}

int anx7418_recv_pd_msg(struct i2c_client *client,
		u8 *buf, size_t len, unsigned long timeout)
{
	struct device *cdev = &client->dev;
	unsigned long expire;
	pd_msg_t *msg = (pd_msg_t *)buf;
	u8 *p = buf;
	int remain = 0;
	int rear;
	unsigned retry;
	int rc = 0;

	anx_dbg_print("PD RECV", 0, "START");

	expire = timeout + jiffies;

	anx7418_i2c_lock(client);

	// read msg.hdr
	rc = recv_pd_msg(client, p, sizeof(pd_msg_hdr_t));
	if (rc == 0) {
		anx_dbg_print("PD RECV", 0, "EMPTY");
		anx7418_i2c_unlock(client);
		return 0;
	} else if (rc < sizeof(pd_msg_hdr_t)) {
		anx_dbg_print("PD RECV", rc, "HEADER TIEMOUT");
		__anx7418_write_reg(client, RX_STATUS, STATUS_ERROR);
		rc = -ETIME;
		goto err;
	} else if (msg->hdr.length > sizeof(msg->data)) {
		anx_dbg_print("PD RECV", msg->hdr.length, "INVALIDE HEADER");
		__anx7418_write_reg(client, RX_STATUS, STATUS_ERROR);
		rc = -EREMOTEIO;
		goto err;
	}

	p += sizeof(pd_msg_hdr_t);
	remain = msg->hdr.length;

	// read data
	retry = 0;
	while (remain > 0) {
		rc = recv_pd_msg(client, p, remain);
		p += rc;
		remain -= rc;

		if (time_before(expire, jiffies)) {
			if (remain == 0)
				break;

			anx_dbg_print("PD RECV", retry, "DATA TIMEOUT");
			dev_err(cdev, "recv_pd: data timeout. remain %d, retry %d\n",
					remain, retry);
			__anx7418_write_reg(client, TX_STATUS,
				STATUS_SUCCESS | STATUS_ERROR);
			rc = -ETIME;
			goto err_timeout;
		}
		retry++;
	}

	len = sizeof(pd_msg_hdr_t) + msg->hdr.length;
	rc = make_checksum(buf, len);
	if (rc != 0) {
		anx_dbg_print("PD RECV", rc, "CHECKSUM ERR");
		dev_err(cdev, "recv_pd: checksum err %x\n", rc);
		__anx7418_write_reg(client, RX_STATUS, STATUS_ERROR);
		rc = -EREMOTEIO;
		goto err_timeout;
	}

	__anx7418_write_reg(client, RX_STATUS, STATUS_SUCCESS);

	dev_dbg(cdev, "recv_pd: %s\n", pd_type_string(msg->hdr.type));
	anx_dbg_print("PD RECV", msg->hdr.type, pd_type_string(msg->hdr.type));

	anx7418_i2c_unlock(client);
	return len;
err_timeout:
#ifdef CONFIG_DEBUG_FS
	anx7418_i2c_unlock(client);
	len = sizeof(pd_msg_hdr_t) + msg->hdr.length - remain;
	if (len < 0 || len > sizeof(pd_msg_t))
		len = sizeof(pd_msg_t);

	print_hex_dump(KERN_INFO, "[recv_err]:", DUMP_PREFIX_OFFSET, 16, 1,
			msg, len, 0);
	{
		char tmp[len * 2 + 1];
		int i;
		for (i = 0; i < len; i++) {
			snprintf(tmp + (i * 2), 3, "%02X", buf[i]);
		}
		anx_dbg_print("PD RECV ERR", rc, tmp);
	}
	anx7418_i2c_lock(client);
#endif
err:
	rear = __anx7418_read_reg(client, RX_PTR_REAR);
	__anx7418_write_reg(client, RX_PTR_FRONT, rear);
	anx7418_i2c_unlock(client);
	return rc;
}
EXPORT_SYMBOL(anx7418_recv_pd_msg);

static int pwr_src_cap_parse(struct i2c_client *client, const pd_msg_t msg)
{
	struct device *cdev = &client->dev;
	struct anx7418 *anx = dev_get_drvdata(cdev);
	pdo_t *pdo = (pdo_t *)msg.data;
	rdo_t rdo = {
		.fixed = {
			.max_curr = pwr_snk_cap[0].fixed.curr,
			.op_curr = pwr_snk_cap[0].fixed.curr,
			.cap_mismatch = 1,
			.pos = 1,
		}
	};
	int i;
	int j;
	int rc;

	for (i = (msg.hdr.length - 1) / 4; i--;) {
		switch (pdo[i].common.type) {
		case PDO_TYPE_FIXED:
		{
			pdo_fixed_t *pdo_f = (pdo_fixed_t *)(pdo + i);
			rdo_fixed_t *rdo_f = (rdo_fixed_t *)&rdo;

			dev_info(cdev, "%s: [%d] fixed, %dmV, %dmA\n", __func__,
					i + 1,
					PD_VOLT_GET(pdo_f->volt),
					PD_CURR_GET(pdo_f->curr));

			for (j = sizeof(pwr_snk_cap) / sizeof(pdo_t); j--;) {
				if (pwr_snk_cap[i].common.type != PDO_TYPE_FIXED)
					continue;

				if (pdo_f->volt != pwr_snk_cap[j].fixed.volt)
					continue;

				rdo_f->max_curr = pwr_snk_cap[j].fixed.curr;
				if (rdo_f->max_curr <= pdo_f->curr) {
					rdo_f->op_curr = rdo_f->max_curr;
					rdo_f->cap_mismatch = 0;
				} else {
					rdo_f->op_curr = pdo_f->curr;
					rdo_f->cap_mismatch = 1;
				}
				rdo_f->pos = i + 1;

				anx->chg.volt_max = PD_VOLT_GET(pdo_f->volt);
				anx->chg.curr_max = PD_CURR_GET(pdo_f->curr);

				if (anx->chg.volt_max && anx->chg.curr_max) {
					anx->chg.ctype_charger =
							ANX7418_CTYPE_PD_CHARGER;
				}

				goto pdo_selected;
			}
		}
			break;

		case PDO_TYPE_BATTERY:
			/* FIXME */
			dev_err(cdev, "%s: PDO_TYPE_BATTERY is unimplemented\n",
					__func__);
			break;

		case PDO_TYPE_VARIABLE:
			/* FIXME */
			dev_err(cdev, "%s: PDO_TYPE_VARIABLE is unimplemented\n",
					__func__);
			break;

		default:
			dev_err(cdev, "%s: [%d] unknown pdo type: %x\n",
					__func__, i + 1, pdo[i].common.type);
			break;
		}

		if (rdo.common.cap_mismatch == 0)
			break;
	}

pdo_selected:
	dev_info(cdev, "%s: [%d] max(%dmA), op(%dmA), mismatch(%d)\n", __func__,
			rdo.common.pos,
			PD_CURR_GET(rdo.common.max),
			PD_CURR_GET(rdo.common.op),
			rdo.common.cap_mismatch);

	rc = anx7418_send_pd_msg(client, PD_TYPE_PWR_OBJ_REQ,
			(u8 *)&rdo, sizeof(rdo_t), PD_SEND_TIMEOUT);

	return 0;
}

static int pswap_req_parse(struct i2c_client *client, const pd_msg_t msg)
{
#ifdef CONFIG_LGE_USB_TYPE_C
	struct anx7418 *anx = dev_get_drvdata(&client->dev);
	struct device *cdev = &anx->client->dev;

	anx7418_send_pd_msg(client, PD_TYPE_ACCEPT, 0, 0, PD_SEND_TIMEOUT);

	if (IS_INTF_IRQ_SUPPORT(anx))
		return 0;

	switch (anx->pr) {
	case DUAL_ROLE_PROP_PR_SRC:
		dev_info(cdev, "Source to Sink\n");
		anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
		break;

	case DUAL_ROLE_PROP_PR_SNK:
		dev_info(cdev, "Sink to Source\n");
		anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SRC);
		break;

	default:
		dev_err(cdev, "pswap: invalid role. %d\n", anx->pr);
		break;
	}

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(anx->dual_role);
#endif
	return 0;
#else
	return anx7418_send_pd_msg(client, PD_TYPE_REJECT, 0, 0, PD_SEND_TIMEOUT);
#endif
}

static int dswap_req_parse(struct i2c_client *client, const pd_msg_t msg)
{
#ifdef CONFIG_LGE_USB_TYPE_C
	struct device *cdev = &client->dev;
	struct anx7418 *anx = dev_get_drvdata(cdev);
	union power_supply_propval prop;

	anx7418_send_pd_msg(client, PD_TYPE_ACCEPT, 0, 0, PD_SEND_TIMEOUT);

	if (IS_INTF_IRQ_SUPPORT(anx))
		return 0;

	switch (anx->dr) {
	case DUAL_ROLE_PROP_DR_HOST:
		dev_info(cdev, "Host to Device\n");
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);

		anx->usb_psy->get_property(anx->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &prop);
		if (prop.intval == POWER_SUPPLY_TYPE_UNKNOWN)
			power_supply_set_supply_type(anx->usb_psy,
					POWER_SUPPLY_TYPE_USB);
		break;

	case DUAL_ROLE_PROP_DR_DEVICE:
		dev_info(cdev, "Device to Host\n");
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);
		break;

	default:
		dev_err(cdev, "dswap: invalid role. %d\n", anx->dr);
		goto err;
	}

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(anx->dual_role);
#endif
	return 0;
err:
#endif
	return anx7418_send_pd_msg(client, PD_TYPE_REJECT, 0, 0, PD_SEND_TIMEOUT);
}

static int vconn_swp_req_parse(struct i2c_client *client, const pd_msg_t msg)
{
	return anx7418_send_pd_msg(client, PD_TYPE_ACCEPT, 0, 0, PD_SEND_TIMEOUT);
}

static int pwr_obj_req_parse(struct i2c_client *client, const pd_msg_t msg)
{
	struct device *cdev = &client->dev;
	rdo_fixed_t *rdo = (rdo_fixed_t *)msg.data;
	int pos = rdo->pos - 1;

	dev_info(cdev, "%s: [%d] max_curr(%dmA), op_curr(%dmA)\n", __func__,
			rdo->pos, rdo->max_curr, rdo->op_curr);

	if (pos < 0 || pos >= ARRAY_SIZE(pwr_src_cap))
		goto err;

	if (rdo->max_curr > pwr_src_cap[pos].fixed.curr ||
	    rdo->op_curr > pwr_src_cap[pos].fixed.curr)
		goto err;

	return anx7418_send_pd_msg(client, PD_TYPE_ACCEPT, 0, 0, PD_SEND_TIMEOUT);
err:
	return anx7418_send_pd_msg(client, PD_TYPE_REJECT, 0, 0, PD_SEND_TIMEOUT);
}

static int dp_alt_enter_parse(struct i2c_client *client, const pd_msg_t msg)
{
	struct anx7418 *anx = dev_get_drvdata(&client->dev);
#ifdef CONFIG_LGE_USB_TYPE_C
	union power_supply_propval prop;
	int rc;
#endif

	gpio_set_value(anx->sbu_sel_gpio, 0);
#ifdef CONFIG_LGE_USB_TYPE_C
	prop.intval = 1;
	rc = anx->batt_psy->set_property(anx->batt_psy,
			POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
	if (rc < 0)
		dev_err(&anx->client->dev,
			"set_property(DP_ALT_MODE) error %d\n", rc);
#endif

	return 0;
}

static int response_to_req_parse(struct i2c_client *client, const pd_msg_t msg)
{
	struct device *cdev = &client->dev;
	response_t *resp = (response_t *)msg.data;

	if (resp->status != RESPONSE_STATUS_SUCCESS) {
		dev_err(cdev, "%s: \"%s\" %s%s%s\n", __func__,
			pd_type_string(resp->req_type),
			resp->status == RESPONSE_STATUS_REJECT ? "reject" : "",
			resp->status == RESPONSE_STATUS_FAILURE ? "failure" : "",
			resp->status == RESPONSE_STATUS_BUSY ? "busy" : "");
	}

	return 0;
}

static int pd_parse(struct i2c_client *client, const pd_msg_t msg)
{
	int rc = -ENOSYS;

	switch (msg.hdr.type) {
	case PD_TYPE_PWR_SRC_CAP:
		rc = pwr_src_cap_parse(client, msg);
		break;

	case PD_TYPE_PWR_SNK_CAP:
		rc = 0;
		break;

	case PD_TYPE_DP_SNK_IDENTITY:
		rc = 0;
		break;

	case PD_TYPE_SVID:
		rc = 0;
		break;

	case PD_TYPE_GET_DP_SNK_CFG:
		break;

	case PD_TYPE_ACCEPT:
		break;

	case PD_TYPE_REJECT:
		break;

	case PD_TYPE_PSWAP_REQ:
		rc = pswap_req_parse(client, msg);
		break;

	case PD_TYPE_DSWAP_REQ:
		rc = dswap_req_parse(client, msg);
		break;

	case PD_TYPE_GOTO_MIN_REQ:
		break;

	case PD_TYPE_VCONN_SWAP_REQ:
		rc = vconn_swp_req_parse(client, msg);
		break;

	case PD_TYPE_VDM:
		break;

	case PD_TYPE_DP_SNK_CFG:
		break;

	case PD_TYPE_PWR_OBJ_REQ:
		rc = pwr_obj_req_parse(client, msg);
		break;

	case PD_TYPE_PD_STATUS_REQ:
		break;

	case PD_TYPE_DP_ALT_ENTER:
		rc = dp_alt_enter_parse(client, msg);
		break;

	case PD_TYPE_DP_ALT_EXIT:
		break;

	case PD_TYPE_RESPONSE_TO_REQ:
		rc = response_to_req_parse(client, msg);
		break;

	case PD_TYPE_SOFT_RST:
		rc = 0;
		break;

	case PD_TYPE_HARD_RST:
		rc = 0;
		break;

	case PD_TYPE_RESTART:
		rc = 0;
		break;

	default:
		rc = -ENOMSG;
		dev_err(&client->dev, "unknown msg type: %x\n", msg.hdr.type);
		break;
	}

	if (rc == -ENOSYS) {
		dev_err(&client->dev, "\"%s\" not implemented\n",
				pd_type_string(msg.hdr.type));
		print_hex_dump(KERN_ERR, "msg:", DUMP_PREFIX_OFFSET, 16, 1,
				msg.data, msg.hdr.length - 1, 0);
	}

	return rc;
}

int anx7418_pd_process(struct anx7418 *anx)
{
	struct i2c_client *client = anx->client;
	pd_msg_t msg;
	int rc;

	do {
		rc = anx7418_recv_pd_msg(client,
				(u8 *)&msg, sizeof(msg), PD_RECV_TIMEOUT);
		if (rc <= 0)
			break;

		pd_parse(client, msg);

		if (msg.hdr.type == PD_TYPE_RESTART) {
			if (anx->pd_restart) {
				rc = -ECANCELED;
				break;
			}
			anx->pd_restart = true;
		} else {
			anx->pd_restart = false;
		}
	} while (!anx->otp);

	return rc;
}
EXPORT_SYMBOL(anx7418_pd_process);

int anx7418_pd_init(struct anx7418 *anx)
{
	struct i2c_client *client = anx->client;
	int rc;

	anx->pd_restart = false;

	rc = anx7418_send_pd_msg(client, PD_TYPE_PWR_SRC_CAP,
			(u8 *)pwr_src_cap, sizeof(pwr_src_cap),
			PD_SEND_TIMEOUT);
	if (rc < 0)
		return -1;

	rc = anx7418_send_pd_msg(client, PD_TYPE_PWR_SNK_CAP,
			(u8 *)pwr_snk_cap, sizeof(pwr_snk_cap),
			PD_SEND_TIMEOUT);
	if (rc < 0)
		return -1;

	rc = anx7418_send_pd_msg(client, PD_TYPE_DP_SNK_IDENTITY,
			dp_snk_identity, sizeof(dp_snk_identity),
			PD_SEND_TIMEOUT);
	if (rc < 0)
		return -1;

	rc = anx7418_send_pd_msg(client, PD_TYPE_SVID,
			svid, sizeof(svid),
			PD_SEND_TIMEOUT);
	if (rc < 0)
		return -1;

	return 0;
}
EXPORT_SYMBOL(anx7418_pd_init);

int anx7418_pd_src_cap_init(struct anx7418 *anx)
{
	struct i2c_client *client = anx->client;
	return anx7418_send_pd_msg(client, PD_TYPE_PWR_SRC_CAP,
			(u8 *)pwr_src_cap, sizeof(pwr_src_cap),
			PD_SEND_TIMEOUT);
}
EXPORT_SYMBOL(anx7418_pd_src_cap_init);
