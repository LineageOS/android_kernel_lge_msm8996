#define OTP_UPDATE_MAX 16

static u8 blank[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static u8 inact[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//static u8 act[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

#define FIXED_HDR_SIZE 5
static u8 fixed_hdr[FIXED_HDR_SIZE][9] = {
	{0x03, 0x00, 0xA9, 0x07, 0x00, 0x00, 0x00, 0x00, 0x7A},
	{0x03, 0x0A, 0xAB, 0x07, 0x00, 0x00, 0x00, 0x00, 0xFF},
	{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x14, 0x00, 0x00, 0xFF},
	{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x1E, 0x00, 0x00, 0xFF},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x28, 0xFF},
};

static u32 fixed_new_addr[FIXED_HDR_SIZE] = {
	0x0003, 0x0A03, 0x1400, 0x1E00, 0x2800
};

static u8 hamming_table[] = {
	0x0b, 0x3B, 0x37, 0x07, 0x19, 0x29, 0x49, 0x89, // 0
	0x16, 0x26, 0x46, 0x86, 0x13, 0x23, 0x43, 0x83, // 1
	0x1C, 0x2C, 0x4C, 0x8C, 0x15, 0x25, 0x45, 0x85, // 2
	0x1A, 0x2A, 0x4A, 0x8A, 0x0D, 0xCD, 0xCE, 0x0E, // 3
	0x70, 0x73, 0xB3, 0xB0, 0x51, 0x52, 0x54, 0x58, // 4
	0xA1, 0xA2, 0xA4, 0xA8, 0x31, 0x32, 0x34, 0x38, // 5
	0xC1, 0xC2, 0xC4, 0xC8, 0x61, 0x62, 0x64, 0x68, // 6
	0x91, 0x92, 0x94, 0x98, 0xE0, 0xEC, 0xDC, 0xD0  // 7
};

static u8 hamming_encoder(const u8 *data)
{
	u8 c;
	int hamming = 0;
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		c = data[i];
		for (j = 0; j < 8; j++) {
			if (c & 0x01)
				hamming ^= hamming_table[(i << 3) + j];
			c >>= 1;
		}
	}

	return hamming;
}

static int ___otp_read_ecc(struct i2c_client *client, u16 word_addr, u8 *buf, bool ecc)
{
	int rc;

	anx7418_i2c_lock(client);

	__anx7418_write_reg(client, R_OTP_CTL_1, 0xA0);
	__anx7418_write_reg(client, R_OTP_ADDR_HIGH, (word_addr >> 8) & 0xFF);
	__anx7418_write_reg(client, R_OTP_ADDR_LOW, word_addr & 0xFF);
	__anx7418_write_reg(client, R_OTP_CTL_1, 0xA1);

	do {
		rc = __anx7418_read_reg(client, R_OTP_STATE_1);
		if (rc < 0)
			goto out;
	} while (rc & R_OTP_READ_WORD_STATE);

	if (ecc)
		rc = __anx7418_read_block_reg(client, R_OTP_DATA_OUT_0, 9, buf);
	else
		rc = __anx7418_read_block_reg(client, R_OTP_DATA_OUT_0, 8, buf);

out:
	anx7418_i2c_unlock(client);
	return rc;
}

static int __otp_read_ecc(struct i2c_client *client, u16 word_addr, u8 *buf)
{
	return ___otp_read_ecc(client, word_addr, buf, true);
}

static int __otp_read(struct i2c_client *client, u16 word_addr, u8 *buf)
{
	return ___otp_read_ecc(client, word_addr, buf, false);
}

static int ___otp_write_ecc(struct i2c_client *client,
	u16 word_addr, const u8 *buf, bool ecc)
{
	int rc;
	u8 tmp[9];

	/*
	 * hdr protector
	 */
	BUG_ON(word_addr < 2);

	anx7418_i2c_lock(client);

	__anx7418_write_reg(client, R_OTP_ADDR_HIGH, (word_addr >> 8) & 0xFF);
	__anx7418_write_reg(client, R_OTP_ADDR_LOW, word_addr & 0xFF);

	__anx7418_write_block_reg(client, R_OTP_DATA_IN_0, 8, buf);
	if (ecc)
		__anx7418_write_reg(client, R_OTP_ECC_IN, buf[8]);
	else
		__anx7418_write_reg(client, R_OTP_ECC_IN, hamming_encoder(buf));

	__anx7418_write_reg(client, R_OTP_CTL_1, 0xAA);
	do {
		rc = __anx7418_read_reg(client, R_OTP_STATE_1);
	} while (rc & R_OTP_WRITE_WORD_STATE);

	anx7418_i2c_unlock(client);

	__otp_read(client, word_addr, tmp);
	if (memcmp(buf, tmp, 8)) {
		dev_err(&client->dev, "%s: word %04X verify failed\n",
				__func__, word_addr);
		print_hex_dump(KERN_ERR, "fw :", DUMP_PREFIX_OFFSET, 16, 1, buf, 8, 0);
		print_hex_dump(KERN_ERR, "rom:", DUMP_PREFIX_OFFSET, 16, 1, tmp, 8, 0);

		return -EIO;
	}

	return 0;
}

static int __otp_write_ecc(struct i2c_client *client, u16 word_addr, const u8 *buf)
{
	return ___otp_write_ecc(client, word_addr, buf, true);
}

static int __otp_write(struct i2c_client *client, u16 word_addr, const u8 *buf)
{
	return ___otp_write_ecc(client, word_addr, buf, false);
}

static int __otp_write_invalid_rom(struct i2c_client *client)
{
	int rc;
	int i;

	for (i = 0; i < 6; i++) {
		anx7418_write_reg(client, R_OTP_ADDR_HIGH, 0);
		anx7418_write_reg(client, R_OTP_ADDR_LOW, 0);

		anx7418_write_block_reg(client, R_OTP_DATA_IN_0, 8, inact);
		anx7418_write_reg(client, R_OTP_ECC_IN, 0xFF);

		anx7418_write_reg(client, R_OTP_CTL_1, 0xAA);
		do {
			rc = anx7418_read_reg(client, R_OTP_STATE_1);
		} while (rc & R_OTP_WRITE_WORD_STATE);
	}

	return 0;
}

static ssize_t otp_read(struct anx7418_firmware *fw, u16 addr,
		u8 *buf, size_t count)
{
	u16 word_addr;
	int rc;

	if (addr % 9 || count != 9)
		return -EINVAL;

	word_addr = addr / 9;
	if (word_addr > 2 && fw->otp_old_addr > 3) {
		word_addr += fw->otp_old_addr;
		word_addr -= 3;
	}

	rc = __otp_read_ecc(fw->client, word_addr, buf);
	if (rc < 0)
		return rc;
	return 9;
}

static ssize_t otp_write(struct anx7418_firmware *fw, u16 addr,
		const u8 *buf, size_t count)
{
	u16 word_addr;
	int rc;

	if (addr % 9 || count != 9) {
		dev_err(&fw->client->dev, "addr(%d) or count(%zu) invalid\n",
				addr, count);
		return -EINVAL;
	}

	word_addr = addr / 9;
	if (word_addr < 3) {
		dev_err(&fw->client->dev, "word_addr(%d) invalid\n",
				word_addr);
		return -EINVAL;
	}

	word_addr += fw->otp_new_addr;
	word_addr -= 3;

	rc = __otp_write_ecc(fw->client, word_addr, buf);
	if (rc < 0)
		return rc;
	return 9;
}

static int otp_open(struct anx7418_firmware *fw)
{
	struct i2c_client *client = fw->client;
	struct device *cdev = &client->dev;
	const struct firmware *entry = fw->entry;
	u8 buf[9];
	int count;
	int i;
	bool inval_fw = true;
	bool inval_hdr = false;
	int rc;
	const char *data;
	u32 addr;
	int j;

	anx7418_i2c_lock(client);
	__anx7418_write_reg(client, POWER_DOWN_CTRL, R_POWER_DOWN_OCM);
	__anx7418_write_reg(client, R_OTP_CTL_1, 0xA8);
	__anx7418_write_reg(client, R_OTP_ACC_PROTECT, 0x7A);
	anx7418_i2c_unlock(client);

	// word 0
	__otp_read_ecc(client, 0, buf);
	if (memcmp(buf, blank, 9)) {
		dev_err(cdev, "invalid ROM, please change a new chip\n");
		print_hex_dump(KERN_ERR, "rom: ", DUMP_PREFIX_OFFSET,
				16, 1, buf, 9, 0);
		return -ENODEV;
	}

	// word 1
	__otp_read_ecc(client, 1, buf);
	if (!memcmp(buf, blank, 9)) {
		dev_err(cdev, "Word 1 is blank\n");
		return -ENODEV;

	} else if (buf[4] == 0x00) {
		__otp_read_ecc(client, 2, buf);
		if ((buf[0] | (buf[1] << 8)) == 0x12 || !memcmp(buf, inact, 9))
			goto find_hdr;

		// Wrong firmware was written. ANX7418 B-lot
		dev_info(cdev, "ANX7418 B-lot\n");
		for (i = 0; i < FIXED_HDR_SIZE; i++) {
			if (!memcmp(buf, fixed_hdr[i], 9))
				break;
		}

		if (i >= (FIXED_HDR_SIZE - 1)) {
			dev_err(cdev, "worng fw: no space for update\n");
			fw->otp_old_addr = fixed_new_addr[FIXED_HDR_SIZE - 1];
			return -ENOSPC;
		}

		fw->otp_hdr = i;
		fw->otp_old_addr = fixed_new_addr[i];
		fw->otp_old_size = entry->data[2 * 9 + 2];
		fw->otp_old_size |= entry->data[2 * 9 + 3] << 8;
		fw->otp_new_addr = fixed_new_addr[i + 1];
		fw->otp_fixed = true;

	} else {
find_hdr:
		// find header
		for (i = 2; i < (2 + OTP_UPDATE_MAX); i++) {
			__otp_read_ecc(client, i, buf);
			if (memcmp(buf, inact, 9)) {
				if (buf[5] != 0x00 || buf[6] != 0x00 || buf[7] != 0x00) {
					for (count = 0; count < 6; count++)
						rc = __otp_write_ecc(client, i, inact);
					if (rc < 0) {
						dev_err(cdev, "hdr_err1: chip damage, "
							"please change a new chip\n");
						return -ENODEV;
					}
					inval_hdr = true;
					continue;
				}
				if (buf[4] == 0x01) {
					// valid fw
					inval_fw= false;
				}
				break;
			}
		}

		if (i >= (2 + OTP_UPDATE_MAX))
			goto err_nospc;

		fw->otp_hdr = i;
		fw->otp_old_addr = buf[0] | (buf[1] << 8);
		fw->otp_old_size = buf[2] | (buf[3] << 8);

		// if old addr is 0, must search new addr.
		if (fw->otp_old_addr < (2 + OTP_UPDATE_MAX)) {
			fw->otp_old_addr = 2 + OTP_UPDATE_MAX;
			fw->otp_old_size = 0;
		} else {
			if (inval_hdr) {
				dev_info(cdev, "invalid header. recovery it\n");
				return -EEXIST;
			}
		}

		fw->otp_new_addr = fw->otp_old_addr + fw->otp_old_size;
	}
	fw->otp_new_size = entry->data[2 * 9 + 2];
	fw->otp_new_size |= entry->data[2 * 9 + 3] << 8;

	dev_info(cdev, "hdr: %d\n", fw->otp_hdr);
	dev_info(cdev, "old: addr(%04X), size(%04X)\n",
			fw->otp_old_addr, fw->otp_old_size);
	dev_info(cdev, "calc new: addr(%04X), size(%04X)\n",
			fw->otp_new_addr, fw->otp_new_size);
	dev_info(cdev, "fixed: %s\n",
			fw->otp_fixed ? "true" : "false");

	if ((fw->otp_new_addr + fw->otp_new_size) > 0x3FFF)
		goto err_nospc;

	/*
	 * find new addr
	 */
	dev_info(cdev, "find new addr\n");

	/*
	 * first: did power turned off during update fw?
	 *
	 *  1. find blank. if found, go to second_find and check space.
	 *  2. find first data.
	 *  3. if found, check shutdown during fw update or not.
	 */

	data = entry->data + 3 * 9;

	for (;fw->otp_new_addr < 0x3FFF; fw->otp_new_addr++) {

		__otp_read_ecc(client, fw->otp_new_addr, buf);

		// 1.
		if (!memcmp(buf, blank, 9))
			goto second_find;

		// 2.
		for (i = 0; i < 9; i++) {
			if (data[i] != (buf[i] | data[i])) {
				// is not first data.
				break;
			}
		}
		if (i >= 9) {

			// 3.

			// start with seoncd data
			data += 9;
			addr = fw->otp_new_addr + 1;

			for (i = 1; i < fw->otp_new_size; i++, addr++) {

				__otp_read_ecc(client, addr, buf);

				// is not blank
				if (memcmp(buf, blank, 9)) {

					for (j = 0; j < 9; j++) {

						if (data[j] != (buf[j] | data[j])) {
							// is not i-th data
							fw->otp_new_addr = addr + 1;
							goto second_find;
						}
					}
				}
				data += 9;
			}

			if (i >= fw->otp_new_size)
				goto found_new_addr;

			goto second_find;
		}
	}

second_find:
	if ((fw->otp_new_addr + fw->otp_new_size) > 0x3FFF)
		goto err_nospc;

	/* second: check available space
	 *
	 * 1. find blank.
	 * 2. check space.
	 * 3. if found non-blank, go to 1
	 */

	for (;fw->otp_new_addr < 0x3FFF; fw->otp_new_addr++) {

		__otp_read_ecc(client, fw->otp_new_addr, buf);

		// 1.
		if (!memcmp(buf, blank, 9)) {

			// 2.
			addr = fw->otp_new_addr + 1;

			for (i = 1;
			     i < fw->otp_new_size && addr < 0x3FFF;
			     i++, addr++) {

				__otp_read_ecc(client, addr, buf);
				if (memcmp(buf, blank, 9)) {
					// 3.
					break;
				}
			}

			if (i >= fw->otp_new_size)
				goto found_new_addr;
		}
	}

	if ((fw->otp_new_addr + fw->otp_new_size) > 0x3FFF)
		goto err_nospc;

found_new_addr:
	dev_info(cdev, "found new: addr(%04X), size(%04X)\n",
			fw->otp_new_addr, fw->otp_new_size);
	return 0;

err_nospc:
	if (inval_fw) {
		__otp_write_invalid_rom(client);
		dev_err(cdev, "chip damage, please change a new chip\n");
		return -ENODEV;
	}

	dev_err(cdev, "no space for update\n");
	return -ENOSPC;
}

static void otp_release(struct anx7418_firmware *fw)
{
	struct i2c_client *client = fw->client;
	struct device *cdev = &client->dev;
	u8 buf[9];
	int count;
	int rc;

	if (!fw->update_done)
		goto done;

	if (fw->otp_new_addr == fw->otp_old_addr)
		goto done;

	// update header
	if (fw->otp_fixed) {
		for (count = 0; count < 6; count++)
			rc = __otp_write_ecc(client, 2, fixed_hdr[fw->otp_hdr + 1]);
		if (rc < 0)
			dev_err(cdev, "fixed: chip damage, "
				"please change a new chip\n");
	} else {
		/* header update
		 *
		 * 1. find new header
		 * 2. write new header
		 * 3. inactivate old header
		 */

		// 1.
		if (fw->otp_hdr < 2)
			fw->otp_hdr = 2;

		while (fw->otp_hdr < (2 + OTP_UPDATE_MAX)) {
			__otp_read_ecc(client, fw->otp_hdr, buf);

			pr_info("%s: otp_hdr(%d)\n", __func__, fw->otp_hdr);
			print_hex_dump(KERN_ERR, "hdr:", DUMP_PREFIX_OFFSET, 16, 1, buf, 9, 0);

			if (!memcmp(buf, blank, 9))
				break;

			fw->otp_hdr++;
		}

		if (fw->otp_hdr >= (2 + OTP_UPDATE_MAX)) {
			dev_err(cdev, "no space for update\n");
			goto done;
		}

		// 2.
		dev_info(cdev, "new otp_hdr(%d)", fw->otp_hdr);
		memcpy(buf, &fw->entry->data[2 * 9], 9);
		buf[0] = fw->otp_new_addr & 0xFF;
		buf[1] = (fw->otp_new_addr >> 8) & 0xFF;

		// valid value
		buf[4] = 0x01;

		for (count = 0; count < 6; count++)
			rc = __otp_write(client, fw->otp_hdr, buf);
		if (rc < 0)
			dev_err(cdev, "hdr: chip damage, "
				"please change a new chip\n");

		// 3.
		while (--fw->otp_hdr >= 2) {
			for (count = 0; count < 6; count++)
				rc = __otp_write_ecc(client, fw->otp_hdr, inact);
			if (rc < 0) {
				dev_err(cdev, "inact: chip damage, "
					"please change a new chip\n");
				goto done;
			}
		}
	}

done:
	anx7418_write_reg(client, R_OTP_ACC_PROTECT, 0x00);
}

static bool otp_update_needed(struct anx7418_firmware *fw, u8 ver)
{
	if (ver == 0x16 || ver == 0xB2 || ver == 0x10)
		return true;
#ifdef CONFIG_LGE_PM_CABLE_DETECTION
	else if (lge_is_factory_cable()) {
		pr_info("%s: skip fw update for factory process\n", __func__);
		return false;
	}
#endif
	else if (ver < ANX7418_OTP_FW_VER)
		return true;

	return false;
}

static int otp_update(struct anx7418_firmware *fw)
{
	struct device *cdev = &fw->client->dev;
	const u8 *data = fw->entry->data;
	int size = fw->entry->size;
	int count;
	int i;
	int rc;

	for (count = 0; count < 6; count++) {
		dev_info(cdev, "[%d] firmware write\n", count + 1);
		for (i = 9 * 3; i < size; i += 9) {
			rc = otp_write(fw, i, (data + i), 9);
			if (rc <= 0) {
				dev_err(cdev, "firmware write(%d) failed %d\n", i, rc);
				if (count == 5) {
					dev_err(cdev, "programming Fail,please re-program\n");
					return rc;
				}
			}

			if (!(i % 0xFFF))
				dev_info(cdev, "%s: %d/%d\n", __func__, i, size);

			if ((size - i) < 9)
				break;
		}
	}

	fw->update_done = true;
	return 0;
}

static int otp_profile(struct anx7418_firmware *fw)
{
	if (fw->otp_old_size == 0)
		return 0;

	fw->otp_new_addr = fw->otp_old_addr;
	return otp_update(fw);
}
