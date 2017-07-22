#include <linux/slab.h>
#include <linux/delay.h>
#include "anx7418_firmware.h"
#include "anx7418_firmware_otp.c"
#include "anx7418_firmware_eeprom.c"

struct anx7418_firmware *__must_check
anx7418_firmware_alloc(struct anx7418 *anx)
{
	struct device *cdev = &anx->client->dev;
	struct anx7418_firmware *fw;
	char path[32];
	int rc;

	fw = kzalloc(sizeof(struct anx7418_firmware), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(fw)) {
		dev_err(cdev, "%s: Error allocating memory\n", __func__);
		return NULL;
	}

	fw->client = anx->client;
	fw->otp = anx->otp;

	if (fw->otp) {
		snprintf(path, sizeof(path), "anx7418/ocm_hamming_%02X.bin",
				ANX7418_OTP_FW_VER);

		fw->ver = ANX7418_OTP_FW_VER;
		fw->read = otp_read;
		fw->write = otp_write;
		fw->open = otp_open;
		fw->release = otp_release;
		fw->update_needed = otp_update_needed;
		fw->update = otp_update;
		fw->profile = otp_profile;
	} else {
		snprintf(path, sizeof(path), "anx7418/ocm_crc_%02X.bin",
				ANX7418_EEPROM_FW_VER);

		fw->ver = ANX7418_EEPROM_FW_VER;
		fw->read = eeprom_read;
		fw->write = eeprom_write;
		fw->open = eeprom_open;
		fw->release = eeprom_release;
	}

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

void anx7418_firmware_free(struct anx7418_firmware *fw)
{
	if (ZERO_OR_NULL_PTR(fw))
		return;

	release_firmware(fw->entry);
	kfree(fw);
}

ssize_t anx7418_firmware_read(struct anx7418_firmware *fw, u16 addr,
		void *buf, size_t count)
{
	return fw->read(fw, addr, buf, count);
}

ssize_t anx7418_firmware_write(struct anx7418_firmware *fw, u16 addr,
		const void *buf, size_t count)
{
	return fw->write(fw, addr, buf, count);
}

int anx7418_firmware_open(struct anx7418_firmware *fw)
{
	return fw->open(fw);
}

void anx7418_firmware_release(struct anx7418_firmware *fw)
{
	fw->release(fw);
}

bool anx7418_firmware_update_needed(struct anx7418_firmware *fw, u8 ver)
{
	if (fw->update_needed)
		return fw->update_needed(fw, ver);
	return false;
}

int anx7418_firmware_update(struct anx7418_firmware *fw)
{
#if 0 // for update test
	struct device *cdev = &fw->client->dev;
	int i;

	for (i = 0; i < 30; i++) {
		dev_info(cdev, "%s: delay %d\n", __func__, (i + 1) * 1000);
		msleep(1000);
	}
	return 0;
#else
	if (fw->update)
		return fw->update(fw);
	return -ENOSYS;
#endif
}

int anx7418_firmware_profile(struct anx7418_firmware *fw)
{
	if (fw->profile)
		return fw->profile(fw);
	return -ENOSYS;
}

int anx7418_firmware_get_old_ver(struct anx7418_firmware *fw)
{
	return anx7418_read_reg(fw->client, ANALOG_CTRL_3);
}

int anx7418_firmware_get_new_ver(struct anx7418_firmware *fw)
{
	return fw->ver;
}
