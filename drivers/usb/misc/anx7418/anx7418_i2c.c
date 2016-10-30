#include <linux/i2c.h>

#define POLY    (0x1070U << 3)
static u8 crc8(u16 data)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (data & 0x8000)
			data = data ^ POLY;
		data = data << 1;
	}
	return (u8)(data >> 8);
}

/* Incremental CRC8 over count bytes in the array pointed to by p */
static u8 i2c_smbus_pec(u8 crc, u8 *p, size_t count)
{
	int i;

	for (i = 0; i < count; i++)
		crc = crc8((crc ^ p[i]) << 8);
	return crc;
}

/* Assume a 7-bit address, which is reasonable for SMBus */
static u8 i2c_smbus_msg_pec(u8 pec, struct i2c_msg *msg)
{
	/* The address will be sent first */
	u8 addr = (msg->addr << 1) | !!(msg->flags & I2C_M_RD);
	pec = i2c_smbus_pec(pec, &addr, 1);

	/* The data buffer follows */
	return i2c_smbus_pec(pec, msg->buf, msg->len);
}

/* Used for write only transactions */
static inline void i2c_smbus_add_pec(struct i2c_msg *msg)
{
	msg->buf[msg->len] = i2c_smbus_msg_pec(0, msg);
	msg->len++;
}

/* Return <0 on CRC error
   If there was a write before this read (most cases) we need to take the
   partial CRC from the write part into account.
   Note that this function does modify the message (we need to decrease the
   message length to hide the CRC byte from the caller). */
static int i2c_smbus_check_pec(u8 cpec, struct i2c_msg *msg)
{
	u8 rpec = msg->buf[--msg->len];
	cpec = i2c_smbus_msg_pec(cpec, msg);

	if (rpec != cpec) {
		pr_debug("i2c-core: Bad PEC 0x%02x vs. 0x%02x\n",
				rpec, cpec);
		return -EBADMSG;
	}
	return 0;
}

static s32 anx7418_i2c_xfer(struct i2c_adapter *adapter, u16 addr,
		unsigned short flags,
		char read_write, u8 command, int size,
		union i2c_smbus_data *data)
{
	/* So we need to generate a series of msgs. In the case of writing, we
	   need to use only one message; when reading, we need two. We initialize
	   most things with sane defaults, to keep the code below somewhat
	   simpler. */
	unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX+3];
	unsigned char msgbuf1[I2C_SMBUS_BLOCK_MAX+2];
	int num = read_write == I2C_SMBUS_READ ? 2 : 1;
	int i;
	u8 partial_pec = 0;
	int status;
	struct i2c_msg msg[2] = {
		{
			.addr = addr,
			.flags = flags,
			.len = 1,
			.buf = msgbuf0,
		}, {
			.addr = addr,
			.flags = flags | I2C_M_RD,
			.len = 0,
			.buf = msgbuf1,
		},
	};

	msgbuf0[0] = command;
	switch (size) {
	case I2C_SMBUS_QUICK:
		msg[0].len = 0;
		/* Special case: The read/write field is used as data */
		msg[0].flags = flags | (read_write == I2C_SMBUS_READ ?
				I2C_M_RD : 0);
		num = 1;
		break;
	case I2C_SMBUS_BYTE:
		if (read_write == I2C_SMBUS_READ) {
			/* Special case: only a read! */
			msg[0].flags = I2C_M_RD | flags;
			num = 1;
		}
		break;
	case I2C_SMBUS_BYTE_DATA:
		if (read_write == I2C_SMBUS_READ)
			msg[1].len = 1;
		else {
			msg[0].len = 2;
			msgbuf0[1] = data->byte;
		}
		break;
	case I2C_SMBUS_WORD_DATA:
		if (read_write == I2C_SMBUS_READ)
			msg[1].len = 2;
		else {
			msg[0].len = 3;
			msgbuf0[1] = data->word & 0xff;
			msgbuf0[2] = data->word >> 8;
		}
		break;
	case I2C_SMBUS_PROC_CALL:
		num = 2; /* Special case */
		read_write = I2C_SMBUS_READ;
		msg[0].len = 3;
		msg[1].len = 2;
		msgbuf0[1] = data->word & 0xff;
		msgbuf0[2] = data->word >> 8;
		break;
	case I2C_SMBUS_BLOCK_DATA:
		if (read_write == I2C_SMBUS_READ) {
			msg[1].flags |= I2C_M_RECV_LEN;
			msg[1].len = 1; /* block length will be added by
					   the underlying bus driver */
		} else {
			msg[0].len = data->block[0] + 2;
			if (msg[0].len > I2C_SMBUS_BLOCK_MAX + 2) {
				dev_err(&adapter->dev,
						"Invalid block write size %d\n",
						data->block[0]);
				return -EINVAL;
			}
			for (i = 1; i < msg[0].len; i++)
				msgbuf0[i] = data->block[i-1];
		}
		break;
	case I2C_SMBUS_BLOCK_PROC_CALL:
		num = 2; /* Another special case */
		read_write = I2C_SMBUS_READ;
		if (data->block[0] > I2C_SMBUS_BLOCK_MAX) {
			dev_err(&adapter->dev,
					"Invalid block write size %d\n",
					data->block[0]);
			return -EINVAL;
		}
		msg[0].len = data->block[0] + 2;
		for (i = 1; i < msg[0].len; i++)
			msgbuf0[i] = data->block[i-1];
		msg[1].flags |= I2C_M_RECV_LEN;
		msg[1].len = 1; /* block length will be added by
				   the underlying bus driver */
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
		if (read_write == I2C_SMBUS_READ) {
			msg[1].len = data->block[0];
		} else {
			msg[0].len = data->block[0] + 1;
			if (msg[0].len > I2C_SMBUS_BLOCK_MAX + 1) {
				dev_err(&adapter->dev,
						"Invalid block write size %d\n",
						data->block[0]);
				return -EINVAL;
			}
			for (i = 1; i <= data->block[0]; i++)
				msgbuf0[i] = data->block[i];
		}
		break;
	default:
		dev_err(&adapter->dev, "Unsupported transaction %d\n", size);
		return -EOPNOTSUPP;
	}

	i = ((flags & I2C_CLIENT_PEC) && size != I2C_SMBUS_QUICK
			&& size != I2C_SMBUS_I2C_BLOCK_DATA);
	if (i) {
		/* Compute PEC if first message is a write */
		if (!(msg[0].flags & I2C_M_RD)) {
			if (num == 1) /* Write only */
				i2c_smbus_add_pec(&msg[0]);
			else /* Write followed by read */
				partial_pec = i2c_smbus_msg_pec(0, &msg[0]);
		}
		/* Ask for PEC if last message is a read */
		if (msg[num-1].flags & I2C_M_RD)
			msg[num-1].len++;
	}

	status = __i2c_transfer(adapter, msg, num);
	if (status < 0)
		return status;

	/* Check PEC if last message is a read */
	if (i && (msg[num-1].flags & I2C_M_RD)) {
		status = i2c_smbus_check_pec(partial_pec, &msg[num-1]);
		if (status < 0)
			return status;
	}

	if (read_write == I2C_SMBUS_READ)
		switch (size) {
		case I2C_SMBUS_BYTE:
			data->byte = msgbuf0[0];
			break;
		case I2C_SMBUS_BYTE_DATA:
			data->byte = msgbuf1[0];
			break;
		case I2C_SMBUS_WORD_DATA:
		case I2C_SMBUS_PROC_CALL:
			data->word = msgbuf1[0] | (msgbuf1[1] << 8);
			break;
		case I2C_SMBUS_I2C_BLOCK_DATA:
			for (i = 0; i < data->block[0]; i++)
				data->block[i+1] = msgbuf1[i];
			break;
		case I2C_SMBUS_BLOCK_DATA:
		case I2C_SMBUS_BLOCK_PROC_CALL:
			for (i = 0; i < msgbuf1[0] + 1; i++)
				data->block[i] = msgbuf1[i];
			break;
		}
	return 0;
}

s32 __anx7418_read_reg(const struct i2c_client *client, u8 command)
{
	union i2c_smbus_data data;
	int status;

	status = anx7418_i2c_xfer(client->adapter, client->addr, client->flags,
			I2C_SMBUS_READ, command,
			I2C_SMBUS_BYTE_DATA, &data);
	return (status < 0) ? status : data.byte;
}

s32 __anx7418_write_reg(const struct i2c_client *client, u8 command,
		u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return anx7418_i2c_xfer(client->adapter, client->addr, client->flags,
			I2C_SMBUS_WRITE, command,
			I2C_SMBUS_BYTE_DATA, &data);
}

s32 __anx7418_read_block_reg(const struct i2c_client *client, u8 command,
		u8 length, u8 *values)
{
	union i2c_smbus_data data;
	int status;

	if (length > I2C_SMBUS_BLOCK_MAX)
		length = I2C_SMBUS_BLOCK_MAX;
	data.block[0] = length;
	status = anx7418_i2c_xfer(client->adapter, client->addr, client->flags,
			I2C_SMBUS_READ, command,
			I2C_SMBUS_I2C_BLOCK_DATA, &data);
	if (status < 0)
		return status;

	memcpy(values, &data.block[1], data.block[0]);
	return data.block[0];
}

s32 __anx7418_write_block_reg(const struct i2c_client *client, u8 command,
		u8 length, const u8 *values)
{
	union i2c_smbus_data data;

	if (length > I2C_SMBUS_BLOCK_MAX)
		length = I2C_SMBUS_BLOCK_MAX;
	data.block[0] = length;
	memcpy(data.block + 1, values, length);
	return anx7418_i2c_xfer(client->adapter, client->addr, client->flags,
			I2C_SMBUS_WRITE, command,
			I2C_SMBUS_I2C_BLOCK_DATA, &data);
}

void anx7418_i2c_lock(const struct i2c_client *client)
{
	i2c_lock_adapter(client->adapter);
}

void anx7418_i2c_unlock(const struct i2c_client *client)
{
	i2c_unlock_adapter(client->adapter);
}

/*****/
int anx7418_read_reg(struct i2c_client *client, u8 reg)
{
	int rc;

	i2c_lock_adapter(client->adapter);
	rc = __anx7418_read_reg(client, reg);
	i2c_unlock_adapter(client->adapter);

	if (rc < 0)
		dev_err(&client->dev, "%s: reg 0x%02X, err %d\n",
				__func__, reg, rc);
	return rc;
}

int anx7418_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int rc;

	i2c_lock_adapter(client->adapter);
	rc = __anx7418_write_reg(client, reg, val);
	i2c_unlock_adapter(client->adapter);

	if (rc < 0)
		dev_err(&client->dev, "%s: reg 0x%02X, val 0x%02X, err %d\n",
				__func__, reg, val, rc);
	return rc;
}

int anx7418_read_block_reg(struct i2c_client *client,
		u8 reg, u8 len, u8 *data)
{
	int rc;

	i2c_lock_adapter(client->adapter);
	rc = __anx7418_read_block_reg(client, reg, len, data);
	i2c_unlock_adapter(client->adapter);

	if (rc < 0)
		dev_err(&client->dev, "%s: reg 0x%02X, err %d\n",
				__func__, reg, rc);
	return rc;
}

int anx7418_write_block_reg(struct i2c_client *client,
		u8 reg, u8 len, const u8 *data)
{
	int rc;

	i2c_lock_adapter(client->adapter);
	rc = __anx7418_write_block_reg(client, reg, len, data);
	i2c_unlock_adapter(client->adapter);

	if (rc < 0)
		dev_err(&client->dev, "%s: reg 0x%02X, err %d\n",
				__func__, reg, rc);
	return rc;
}

