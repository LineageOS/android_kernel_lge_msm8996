#ifndef __ANX7418_I2C_H__
#define __ANX7418_I2C_H__

s32 __anx7418_read_reg(const struct i2c_client *client, u8 command);
s32 __anx7418_write_reg(const struct i2c_client *client, u8 command,
		u8 value);
s32 __anx7418_read_block_reg(const struct i2c_client *client, u8 command,
		u8 length, u8 *values);
s32 __anx7418_write_block_reg(const struct i2c_client *client, u8 command,
		u8 length, const u8 *values);

void anx7418_i2c_lock(const struct i2c_client *client);
void anx7418_i2c_unlock(const struct i2c_client *client);

int anx7418_read_reg(struct i2c_client *client, u8 reg);
int anx7418_write_reg(struct i2c_client *client, u8 reg, u8 val);
int anx7418_read_block_reg(struct i2c_client *client,
		u8 reg, u8 len, u8 *data);
int anx7418_write_block_reg(struct i2c_client *client,
		u8 reg, u8 len, const u8 *data);

#endif /* __ANX7418_I2C_H__ */
