#ifndef TCS_I2C_COMMON_H
#define TCS_I2C_COMMON_H

#include <linux/i2c.h>

int32_t tcs_i2c_write(uint32_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type);
int32_t tcs_i2c_read(uint32_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type);
int32_t tcs_i2c_write_seq(uint32_t addr, uint8_t *data, uint16_t num_byte);
int32_t tcs_i2c_read_seq(uint32_t addr, uint8_t *data, uint16_t num_byte);

int32_t TCSWrite16bit( uint32_t RamAddr, uint16_t RamData );
int32_t TCSRead16bit( uint32_t RamAddr, uint16_t *ReadData );
int32_t TCSWrite32bit( uint32_t RamAddr, uint32_t RamData );
int32_t TCSRead32bit(uint32_t RamAddr, uint32_t *ReadData );
int32_t TCSWrite8bit(uint32_t RegAddr, uint8_t RegData);
int32_t TCSRead8bit(uint32_t RegAddr, uint8_t *RegData);

#define MAX_TCS_BIN_FILENAME 64
#define MAX_TCS_BIN_BLOCKS 4
#define MAX_TCS_BIN_FILES 3

struct tcs_i2c_bin_addr {
	uint16_t bin_str_addr;
	uint16_t bin_end_addr;
	uint16_t reg_str_addr;
};

struct tcs_i2c_bin_entry {
	char  filename[MAX_TCS_BIN_FILENAME];
	uint32_t filesize;
	uint16_t blocks;
	struct tcs_i2c_bin_addr addrs[MAX_TCS_BIN_BLOCKS];
};

struct tcs_i2c_bin_list {
	uint16_t files;
	struct tcs_i2c_bin_entry entries[MAX_TCS_BIN_FILES];
	uint32_t checksum;
};
#endif