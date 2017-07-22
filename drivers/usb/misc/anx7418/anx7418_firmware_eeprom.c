static int _eeprom_read(struct i2c_client *client, u16 addr)
{
	int rc;

	anx7418_write_reg(client, R_EE_ADDR_L_BYTE, addr & 0xFF);
	anx7418_write_reg(client, R_EE_ADDR_H_BYTE, (addr >> 8) & 0xFF);
	anx7418_write_reg(client, R_EE_CTL_1, 0xC8 | R_EE_READ_EN);

	do {
		rc = anx7418_read_reg(client, R_EE_STATE);
		if (rc < 0)
			return rc;
	} while (!(rc & R_EE_RW_DONE));

	return anx7418_read_reg(client, R_EE_RD_DATA);
}

static int _eeprom_write(struct i2c_client *client, u16 addr, const u8 val)
{
	int rc;

	anx7418_write_reg(client, R_EE_ADDR_L_BYTE, addr & 0xFF);
	anx7418_write_reg(client, R_EE_ADDR_H_BYTE, (addr >> 8) & 0xFF);
	anx7418_write_reg(client, R_EE_WR_DATA, val);
	anx7418_write_reg(client, R_EE_CTL_1, 0xC8 | R_EE_WR_EN);

	do {
		rc = anx7418_read_reg(client, R_EE_STATE);
		if (rc < 0)
			return rc;
	} while (!(rc & R_EE_RW_DONE));

	return 0;
}

static ssize_t eeprom_read(struct anx7418_firmware *fw, u16 addr,
		u8 *buf, size_t count)
{
	struct i2c_client *client = fw->client;
	int i;
	int rc;

	for (i = 0; i < count; i++) {
		rc = _eeprom_read(client, addr + i);
		if (rc < 0)
			return rc;
		buf[i] = rc;
	}
	return count;
}

static ssize_t eeprom_write(struct anx7418_firmware *fw, u16 addr,
		const u8 *buf, size_t count)
{
	struct i2c_client *client = fw->client;
	int i;
	int rc;

	for (i = 0; i < count; i++) {
		rc = _eeprom_write(client, addr + i, buf[i]);
		if (rc < 0)
			return rc;
	}
	return count;
}

static int eeprom_open(struct anx7418_firmware *fw)
{
	struct i2c_client *client = fw->client;

	// reset OCM
	anx7418_write_reg(client, RESET_CTRL_0, R_OCM_RESET);

	// EEPROM access password
	anx7418_write_reg(client, EE_KEY_1, 0x28);
	anx7418_write_reg(client, EE_KEY_2, 0x5C);
	anx7418_write_reg(client, EE_KEY_3, 0x4E);

	return 0;
}

static void eeprom_release(struct anx7418_firmware *fw)
{
	anx7418_write_reg(fw->client, RESET_CTRL_0, 0);
}
