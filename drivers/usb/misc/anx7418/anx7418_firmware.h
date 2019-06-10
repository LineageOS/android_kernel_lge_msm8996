#ifndef __ANX7418_FIRMWARE_H__
#define __ANX7418_FIRMWARE_H__

#include <linux/firmware.h>
#include <linux/i2c.h>

#include "anx7418.h"

#define ANX7418_OTP_FW_VER	0x13
#define ANX7418_EEPROM_FW_VER	0x08

struct anx7418;

struct anx7418_firmware {
	struct i2c_client *client;
	const struct firmware *entry;

	int ver;
	bool otp;
	bool update_done;

	ssize_t (*read) (struct anx7418_firmware *, u16, u8 *, size_t);
	ssize_t (*write) (struct anx7418_firmware *, u16, const u8 *, size_t);
	int (*open) (struct anx7418_firmware *);
	void (*release) (struct anx7418_firmware *);

	bool (*update_needed) (struct anx7418_firmware *, u8 ver);
	int (*update) (struct anx7418_firmware *);
	int (*profile) (struct anx7418_firmware *);

	/* OTP */
	u32 otp_hdr;
	u32 otp_old_addr;
	u32 otp_old_size;
	u32 otp_new_addr;
	u32 otp_new_size;
	bool otp_fixed;
};

ssize_t anx7418_firmware_read(struct anx7418_firmware *fw, u16 addr,
		                void *buf, size_t count);
ssize_t anx7418_firmware_write(struct anx7418_firmware *fw, u16 addr,
		const void *buf, size_t count);

int anx7418_firmware_open(struct anx7418_firmware *fw);
void anx7418_firmware_release(struct anx7418_firmware *fw);

bool anx7418_firmware_update_needed(struct anx7418_firmware *fw, u8 ver);
int anx7418_firmware_update(struct anx7418_firmware *fw);
int anx7418_firmware_profile(struct anx7418_firmware *fw);

struct anx7418_firmware *__must_check
anx7418_firmware_alloc(struct anx7418 *anx);
void anx7418_firmware_free(struct anx7418_firmware *fw);

int anx7418_firmware_get_old_ver(struct anx7418_firmware *fw);
int anx7418_firmware_get_new_ver(struct anx7418_firmware *fw);

#endif /* __ANX7418_FIRMWARE_H__ */
