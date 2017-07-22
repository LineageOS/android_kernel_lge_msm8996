/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
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

#ifndef __ANX7688_FIRMWARE_H__
#define __ANX7688_FIRMWARE_H__

#include <linux/firmware.h>
#include <linux/i2c.h>

#define MI1_FWVER_RC1           0x0030
#define MI1_FWVER_RC2           0x0032
#define MI1_FWVER_RC3           0x0211
#define MI1_FWVER_RC4           0x0221
#define MI1_FWVER_RC5           0x2040
#define MI1_FWVER_PRE_RC6       0x2060
#define MI1_FWVER_RC6           0x206c
#define MI1_FWVER_PRE_REL1      0x2080
#define MI1_FWVER_REL2          0x2082
#define MI1_FWVER_REL2_MR       0x2089
#define MI1_FWVER_REL2_MR2      0x208b
#define MI1_NEW_FWVER           MI1_FWVER_REL2_MR2

struct anx7688_firmware {
	struct i2c_client *client;
	const struct firmware *entry;

	int ver;
	bool otp;
	bool update_needed;
	bool update_done;

	ssize_t (*read) (struct anx7688_firmware *, u16, const u8 *, size_t);
	ssize_t (*write) (struct anx7688_firmware *, u16, const u8 *, size_t);
	int (*open) (struct anx7688_firmware *, int);
	void (*release) (struct anx7688_firmware *);

	int (*update) (struct anx7688_firmware *);
};

ssize_t anx7688_firmware_read(struct anx7688_firmware *fw, u16 addr,
				const void *buf, size_t count);
ssize_t anx7688_firmware_write(struct anx7688_firmware *fw, u16 addr,
				const void *buf, size_t count);

int anx7688_firmware_open(struct anx7688_firmware *fw, int ver);
void anx7688_firmware_release(struct anx7688_firmware *fw);
bool is_fw_update_need(struct anx7688_firmware *fw, int ver);
int anx7688_firmware_update(struct anx7688_firmware *fw);
ssize_t anx7688_firmware_verify(struct anx7688_firmware *fw, u16 addr,
			      const u8 *buf, size_t count);
void anx7688_firmware_free(struct anx7688_firmware *fw);
int anx7688_firmware_get_old_ver(struct anx7688_firmware *fw);
int anx7688_firmware_get_new_ver(struct anx7688_firmware *fw);

#endif /* __ANX7688_FIRMWARE_H__ */
