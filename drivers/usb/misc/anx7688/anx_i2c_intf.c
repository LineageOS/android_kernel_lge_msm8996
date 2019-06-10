/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * analogix register access by i2c
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

#include <linux/i2c.h>
#include "anx_i2c_intf.h"

struct i2c_client *ohio_client;

inline s32 OhioReadReg(unsigned short addr, u8 reg)
{
	s32 rc;
	int retry = 0;

	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;
	do {
		rc = i2c_smbus_read_byte_data(ohio_client, reg);
		if (IS_ERR_VALUE(rc))
			dev_err(&ohio_client->dev, "%s: error %d\n",
					__func__, rc);

	} while ((retry++ < 5) && (rc < 0));

	return rc;
}

inline s32 OhioReadWordReg(unsigned short addr, u8 reg)
{
	s32 rc;
	int retry = 0;

	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;
	do {
		rc = i2c_smbus_read_word_data(ohio_client, reg);
		if (IS_ERR_VALUE(rc))
			dev_err(&ohio_client->dev, "%s: error %d\n",
					__func__, rc);

	} while ((retry++ < 5) && (rc < 0));

	return rc;
}

inline s32 OhioReadBlockReg(unsigned short addr,
				u8 reg, u8 len, u8 *val)
{
	s32 rc;
	int retry = 0;

	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;
	do {
		rc = i2c_smbus_read_i2c_block_data(ohio_client, reg, len, val);
		if (IS_ERR_VALUE(rc))
			dev_err(&ohio_client->dev, "%s: error %d\n",
					__func__, rc);

	} while ((retry++ < 5) && (rc < 0));

	return rc;
}

inline s32 OhioWriteReg(unsigned short addr, u8 reg, u8 val)
{
	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;
	return i2c_smbus_write_byte_data(ohio_client, reg, val);
}

inline s32 OhioMaskWriteReg(unsigned short addr, u8 reg, u8 mask, u8 val)
{
	int rc;

	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;

	if (!mask) {
		return -EINVAL;
	}

	rc = i2c_smbus_read_byte_data(ohio_client, reg);
	if (!IS_ERR_VALUE(rc)) {
		rc = i2c_smbus_write_byte_data(ohio_client,
				reg, BITS_SET((u8)rc, mask, val));
	}

	return rc;
}

inline s32 OhioWriteWordReg(unsigned short addr, u8 reg, u16 val)
{
	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;
	return i2c_smbus_write_word_data(ohio_client, reg, val);
}

inline s32 OhioMaskWriteWordReg(unsigned short addr, u8 reg, u16 mask, u16 val)
{
	int rc;

	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;

	if (!mask) {
		return -EINVAL;
	}

	rc = i2c_smbus_read_word_data(ohio_client, reg);
	if (!IS_ERR_VALUE(rc)) {
		rc = i2c_smbus_write_word_data(ohio_client,
				reg, BITS_SET((u16)rc, mask, val));
	}

	return rc;
}

inline s32 OhioWriteBlockReg(unsigned short addr, u8 reg, u8 len, u8 *val)
{
	if (!ohio_client)
		return -ENODEV;

	ohio_client->addr = addr;
	return i2c_smbus_write_i2c_block_data(ohio_client, reg, len, val);
}
