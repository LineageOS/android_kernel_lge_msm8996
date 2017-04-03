/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * anx7688 firmware update API
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

#include <linux/slab.h>
#include <linux/delay.h>
#include "anx7688_mi1.h"
#include "anx7688_core.h"
#include "anx7688_firmware.h"
#include "anx_i2c_intf.h"

#define FW_LENGTH 16
#define FWVER_ADDR_EEPROM 0x0800

int anx7688_fw_delay_ms = 4;

static bool is_factory_cable(void)
{
	unsigned int cable_info;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT
	unsigned int *p_cable_type = NULL;
	unsigned int cable_smem_size = 0;

	p_cable_type = smem_get_entry(SMEM_ID_VENDOR1,
			&cable_smem_size, 0, 0);
	if (p_cable_type)
		cable_info = *p_cable_type;
	else
		return  false;

	pr_info("[anx7688_firmware] cable %d\n", cable_info);
	if (cable_info == LT_CABLE_56K ||
			cable_info == LT_CABLE_130K ||
			cable_info == LT_CABLE_910K) {
		return true;
	}
#elif defined (CONFIG_LGE_PM_CABLE_DETECTION)
	cable_info = lge_pm_get_cable_type();
	pr_info("[anx7688_firmware] cable %d\n", cable_info);
	if (cable_info == CABLE_56K ||
			cable_info == CABLE_130K ||
			cable_info == CABLE_910K) {
		return true;
	}
#else
	cable_info = NO_INIT_CABLE;
#endif
	return false;
}

static ssize_t eeprom_read(struct anx7688_firmware *fw, u16 addr,
		const u8 *buf, size_t count)
{
	int rc;
	int len;
	int retry = 0;
	u8 data[FW_LENGTH] = {0,};

	OhioWriteReg(USBC_ADDR, EEPROM_ADDR_L, addr & 0xFF);
	OhioWriteReg(USBC_ADDR, EEPROM_ADDR_H, (addr >> 8) & 0xFF);
	OhioWriteReg(USBC_ADDR, EEPROM_WR_ENABLE, 0x06);

	do {
		rc = OhioReadReg(USBC_ADDR, EEPROM_WR_ENABLE);
		if (rc < 0)
			return rc;

		if (retry++ >= 20)
			return -EAGAIN;

	} while ((rc & 0x08) != 0x08);

	len = OhioReadBlockReg(USBC_ADDR, EEPROM_WR_DATA0, count, data);

	pr_debug("R:%x: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		addr, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
		data[7], data[8], data[9], data[10], data[11], data[12], data[13],
		data[14], data[15]);

	return memcmp(data, buf, count);
}

static ssize_t eeprom_write(struct anx7688_firmware *fw, u16 addr,
		const u8 *buf, size_t count)
{
	int rc;
	int retry = 0;
	u8 data[FW_LENGTH] = {0,};

	memcpy(data, buf, count);

	OhioWriteReg(USBC_ADDR, EEPROM_ADDR_L, addr & 0xFF);
	OhioWriteReg(USBC_ADDR, EEPROM_ADDR_H, (addr >> 8) & 0xFF);
	OhioWriteBlockReg(USBC_ADDR, EEPROM_WR_DATA0, count, data);
	OhioWriteReg(USBC_ADDR, EEPROM_WR_ENABLE, 0x01);

	do {
		rc = OhioReadReg(USBC_ADDR, EEPROM_WR_ENABLE);
		if (rc < 0)
			return rc;

		if (retry++ >= 20)
			return -EAGAIN;

	} while ((rc & 0x08) != 0x08);

	pr_debug("W:%x: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		addr, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
		data[7], data[8], data[9], data[10], data[11], data[12], data[13],
		data[14], data[15]);

	msleep(anx7688_fw_delay_ms++);

	memset(data, 0, count);
	OhioWriteBlockReg(USBC_ADDR, EEPROM_WR_DATA0, count, data);
	return rc;
}

static int eeprom_open(struct anx7688_firmware *fw, int ver)
{
	int rc;

	/* reset OCM */
	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, R_OCM_RESET);

	/* EEPROM access password */
	if (ver < MI1_FWVER_RC1) {
		OhioWriteReg(USBC_ADDR, USBC_EE_KEY_1, 0x20);
		OhioWriteReg(USBC_ADDR, USBC_EE_KEY_2, 0x81);
		OhioWriteReg(USBC_ADDR, USBC_EE_KEY_3, 0x08);
	} else {
		rc = OhioReadReg(USBC_ADDR, USBC_EE_KEY_1);
		OhioWriteReg(USBC_ADDR, USBC_EE_KEY_1, (rc | 0x20));

		rc = OhioReadReg(USBC_ADDR, USBC_EE_KEY_2);
		OhioWriteReg(USBC_ADDR, USBC_EE_KEY_2, (rc | 0x81));

		rc = OhioReadReg(USBC_ADDR, USBC_EE_KEY_3);
		OhioWriteReg(USBC_ADDR, USBC_EE_KEY_3, (rc | 0x08));
	}
	msleep(50);

	return 0;
}

static void eeprom_release(struct anx7688_firmware *fw)
{
	int rc;

	/* EEPROM close password */
	rc = OhioReadReg(USBC_ADDR, USBC_EE_KEY_1);
	OhioWriteReg(USBC_ADDR, USBC_EE_KEY_1, (rc & 0xDF));

	rc = OhioReadReg(USBC_ADDR, USBC_EE_KEY_2);
	OhioWriteReg(USBC_ADDR, USBC_EE_KEY_2, (rc & 0x7E));

	rc = OhioReadReg(USBC_ADDR, USBC_EE_KEY_3);
	OhioWriteReg(USBC_ADDR, USBC_EE_KEY_3, (rc & 0xE7));

	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0);
}

static int eeprom_update(struct anx7688_firmware *fw)
{
	struct device *cdev = &fw->client->dev;
	const u8 *data = fw->entry->data;
	int size = fw->entry->size;
	u16 offset = FW_LENGTH;
	int retry1 = 0;
	int retry2 = 0;
	int rc;
	bool need_finalize = false;;

	while(size > 0) {
retry:
		rc = eeprom_write(fw, offset, (data + offset), FW_LENGTH);
		if (rc < 0) {
			if (retry1 < 5) {
				retry1++;
				goto retry;
			} else {
				dev_err(cdev, "%s: max retired write\n",
						__func__);
				goto err;
			}
		}

		rc = eeprom_read(fw, offset, (data + offset), FW_LENGTH);
		if (rc != 0) {
			if (retry2 < 5) {
				retry2++;
				goto retry;
			} else {
				dev_err(cdev, "%s: max retired read\n",
						__func__);
				rc = -1;
				goto err;
			}
		}

		if (!((offset == FWVER_ADDR_EEPROM) && need_finalize))
			offset += FW_LENGTH;

		if (!(offset % 1024))
			dev_info(cdev, "size:%d, offet:%d\n", size, offset);

		if (offset == FWVER_ADDR_EEPROM) {
			if (need_finalize) {
				offset = size;
				need_finalize = false;
			} else {
				dev_info(cdev, "offet:%d skipped\n", offset);
				offset += FW_LENGTH;
				need_finalize = true;
			}
		}

		if ((size - offset) < FW_LENGTH)
			offset = size;

		if (offset >= size) {
			if (need_finalize) {
				offset = FWVER_ADDR_EEPROM;
				dev_info(cdev, "fw version offet:%d write\n", offset);
			} else {
				goto complete;
			}
		}

		retry1 = retry2 = 0;
		anx7688_fw_delay_ms = 4;
	}

complete:
err:
	return rc;
}

static bool anx7688_check_validate(struct anx7688_firmware *fw)
{
	struct device *cdev = &fw->client->dev;
	const u8 *data = fw->entry->data;
	int size = fw->entry->size;
	int retry = 0;
	int rc;
	u16 offset = FW_LENGTH;
	u16 interval;

	if (!(size % FW_LENGTH))
		interval = (size / 10);
	else
		interval = (FW_LENGTH * 200);

	while(size > 0) {
retry:
		rc = eeprom_read(fw, offset, (data + offset), FW_LENGTH);
		if (rc != 0) {
			if (retry < 10) {
				retry++;
				goto retry;
			} else {
				dev_err(cdev, "found not matched fw data"
						" at size:%d, offet:%d\n",
						size, offset);
				return false;
			}
		}

		if (offset == FW_LENGTH)
			offset += (interval - FW_LENGTH);
		else
			offset += interval;

		if ((size - offset) < FW_LENGTH)
			offset = size;

		if (offset >= size)
			goto complete;

		if (offset >= (size - interval))
			offset = size - FW_LENGTH;

		retry = 0;
	}
complete:
	return true;
}

struct anx7688_firmware *__must_check
anx7688_firmware_alloc(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	struct anx7688_firmware *fw;
	char path[32];
	int rc;

	fw = kzalloc(sizeof(struct anx7688_firmware), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(fw)) {
		dev_err(cdev, "%s: fail to alloc memory\n", __func__);
		return NULL;
	}

	fw->client = chip->client;

	snprintf(path, sizeof(path), "analogix/mi1_eeprom_%02x_%02x.img",
				((MI1_NEW_FWVER & 0xFF00) >> 8),
				(MI1_NEW_FWVER & 0x00FF));

	fw->ver = MI1_NEW_FWVER;
	fw->read = eeprom_read;
	fw->write = eeprom_write;
	fw->open = eeprom_open;
	fw->release = eeprom_release;
	fw->update = eeprom_update;
	fw->client = chip->client;
	fw->update_needed = chip->pdata->fw_force_update;

	dev_info(cdev, "request_firmware(%s)\n", path);
	rc = request_firmware(&fw->entry, path, cdev);
	if (rc < 0) {
		dev_err(cdev, "request_firmware failed %d\n", rc);
		goto err;
	}

	return fw;
err:
	kfree(fw);
	return NULL;
}

void anx7688_firmware_free(struct anx7688_firmware *fw)
{
	if (ZERO_OR_NULL_PTR(fw))
		return;

	release_firmware(fw->entry);
	kfree(fw);
}

ssize_t anx7688_firmware_read(struct anx7688_firmware *fw, u16 addr,
		const void *buf, size_t count)
{
	return fw->read(fw, addr, buf, count);
}

ssize_t anx7688_firmware_verify(struct anx7688_firmware *fw, u16 addr,
	      const u8 *buf, size_t count)
{
	//struct i2c_client *client = fw->client;
	u8 data[FW_LENGTH] = {0,};
	int rc;
	int len;

	OhioWriteReg(USBC_ADDR, EEPROM_ADDR_L, addr & 0xFF );
	OhioWriteReg(USBC_ADDR, EEPROM_ADDR_H, (addr >> 8) & 0xFF );
	OhioWriteReg(USBC_ADDR, EEPROM_WR_ENABLE, 0x06);

	do {
		rc = OhioReadReg(USBC_ADDR, EEPROM_WR_ENABLE);
		if (rc < 0)
			return rc;
	} while ((rc & 0x08) != 0x08);

	msleep(10);

	len = OhioReadBlockReg(USBC_ADDR, EEPROM_WR_DATA0, count, data);

	return memcmp(data, buf, count);
}

ssize_t anx7688_firmware_write(struct anx7688_firmware *fw, u16 addr,
		const void *buf, size_t count)
{
	return fw->write(fw, addr, buf, count);
}

int anx7688_firmware_open(struct anx7688_firmware *fw, int ver)
{
	return fw->open(fw, ver);
}

void anx7688_firmware_release(struct anx7688_firmware *fw)
{
	fw->release(fw);
}

bool is_fw_update_need(struct anx7688_firmware *fw, int ver)
{
	if (is_factory_cable())
		return false;
	if (fw->update_needed) {
		return true;
	} else {
		if (fw->ver != ver)
			return true;
		else if (!anx7688_check_validate(fw))
			return true;
	}

	return false;
}

int anx7688_firmware_update(struct anx7688_firmware *fw)
{
	if (fw->update)
		return fw->update(fw);
	return -ENOSYS;
}

int anx7688_firmware_get_old_ver(struct anx7688_firmware *fw)
{
	return OhioReadWordReg(USBC_ADDR, OCM_DEBUG_4);
}

int anx7688_firmware_get_new_ver(struct anx7688_firmware *fw)
{
	return fw->ver;
}
