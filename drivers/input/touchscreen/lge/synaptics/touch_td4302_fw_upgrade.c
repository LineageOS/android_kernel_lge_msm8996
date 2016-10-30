/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */
#define TS_MODULE "[upgrade]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_td4302.h"

/*
#define DO_STARTUP_FW_UPDATE
*/
/*
#ifdef DO_STARTUP_FW_UPDATE
#ifdef CONFIG_FB
#define WAIT_FOR_FB_READY
#define FB_READY_WAIT_MS 100
#define FB_READY_TIMEOUT_S 30
#endif
#endif
*/

#define PRODUCT_ID_SIZE 10
#define PRODUCT_INFO_SIZE 2

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define sstrtoul(...) kstrtoul(__VA_ARGS__)

#define FORCE_UPDATE false
#define DO_LOCKDOWN false

#define MAX_IMAGE_NAME_LEN 256
#define MAX_FIRMWARE_ID_LEN 10

#define IMAGE_HEADER_VERSION_06 0x06

#define IMAGE_AREA_OFFSET 0x100
#define LOCKDOWN_SIZE 0x50

#define V5V6_BOOTLOADER_ID_OFFSET 0
#define V5V6_CONFIG_ID_SIZE 4

#define V6_PROPERTIES_OFFSET 1
#define V6_BLOCK_SIZE_OFFSET 2
#define V6_BLOCK_COUNT_OFFSET 3
#define V6_PROPERTIES_2_OFFSET 4
#define V6_GUEST_CODE_BLOCK_COUNT_OFFSET 5
#define V6_BLOCK_NUMBER_OFFSET 0
#define V6_BLOCK_DATA_OFFSET 1
#define V6_FLASH_COMMAND_OFFSET 2
#define V6_FLASH_STATUS_OFFSET 3

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)

#define MIN_SLEEP_TIME_US 50
#define MAX_SLEEP_TIME_US 100

#define INT_DISABLE_WAIT_MS 20
#define ENTER_FLASH_PROG_WAIT_MS 20

enum f34_version {
	F34_V0 = 0,
	F34_V1,
	F34_V2,
};

enum bl_version {
	BL_V5 = 5,
	BL_V6 = 6,
	BL_V7 = 7,
	BL_V8 = 8,
};

enum flash_area {
	NONE = 0,
	UI_FIRMWARE,
	UI_CONFIG,
};

enum update_mode {
	NORMAL = 1,
	FORCE = 2,
	LOCKDOWN = 8,
};

enum config_area {
	UI_CONFIG_AREA = 0,
	PM_CONFIG_AREA,
	BL_CONFIG_AREA,
	DP_CONFIG_AREA,
	FLASH_CONFIG_AREA,
};

enum v7_status {
	SUCCESS = 0x00,
	DEVICE_NOT_IN_BOOTLOADER_MODE,
	INVALID_PARTITION,
	INVALID_COMMAND,
	INVALID_BLOCK_OFFSET,
	INVALID_TRANSFER,
	NOT_ERASED,
	FLASH_PROGRAMMING_KEY_INCORRECT,
	BAD_PARTITION_TABLE,
	CHECKSUM_FAILED,
	FLASH_HARDWARE_FAILURE = 0x1f,
};

enum v5v6_flash_command {
	CMD_V5V6_IDLE = 0x0,
	CMD_V5V6_WRITE_FW = 0x2,
	CMD_V5V6_ERASE_ALL = 0x3,
	CMD_V5V6_WRITE_LOCKDOWN = 0x4,
	CMD_V5V6_READ_CONFIG = 0x5,
	CMD_V5V6_WRITE_CONFIG = 0x6,
	CMD_V5V6_ERASE_UI_CONFIG = 0x7,
	CMD_V5V6_ERASE_BL_CONFIG = 0x9,
	CMD_V5V6_ERASE_DISP_CONFIG = 0xa,
	CMD_V5V6_ERASE_GUEST_CODE = 0xb,
	CMD_V5V6_WRITE_GUEST_CODE = 0xc,
	CMD_V5V6_ENABLE_FLASH_PROG = 0xf,
};

enum flash_command {
	CMD_IDLE = 0,
	CMD_WRITE_FW,
	CMD_WRITE_CONFIG,
	CMD_WRITE_LOCKDOWN,
	CMD_WRITE_GUEST_CODE,
	CMD_READ_CONFIG,
	CMD_ERASE_ALL,
	CMD_ERASE_UI_FIRMWARE,
	CMD_ERASE_UI_CONFIG,
	CMD_ERASE_BL_CONFIG,
	CMD_ERASE_DISP_CONFIG,
	CMD_ERASE_FLASH_CONFIG,
	CMD_ERASE_GUEST_CODE,
	CMD_ENABLE_FLASH_PROG,
};

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT entry
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_version: version of function
 * @fn_number: function number
 */

struct partition_table {
	unsigned char partition_id:5;
	unsigned char byte_0_reserved:3;
	unsigned char byte_1_reserved;
	unsigned char partition_length_7_0;
	unsigned char partition_length_15_8;
	unsigned char start_physical_address_7_0;
	unsigned char start_physical_address_15_8;
	unsigned char partition_properties_7_0;
	unsigned char partition_properties_15_8;
} __packed;

struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_v5v6_flash_properties {
	union {
		struct {
			unsigned char reg_map:1;
			unsigned char unlocked:1;
			unsigned char has_config_id:1;
			unsigned char has_pm_config:1;
			unsigned char has_bl_config:1;
			unsigned char has_disp_config:1;
			unsigned char has_ctrl1:1;
			unsigned char has_query4:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_v5v6_flash_properties_2 {
	union {
		struct {
			unsigned char has_guest_code:1;
			unsigned char reserved:7;
		} __packed;
		unsigned char data[1];
	};
};

struct register_offset {
	unsigned char properties;
	unsigned char properties_2;
	unsigned char block_size;
	unsigned char block_count;
	unsigned char gc_block_count;
	unsigned char flash_status;
	unsigned char partition_id;
	unsigned char block_number;
	unsigned char transfer_length;
	unsigned char flash_cmd;
	unsigned char payload;
};

struct block_count {
	unsigned short ui_firmware;
	unsigned short ui_config;
	unsigned short dp_config;
	unsigned short pm_config;
	unsigned short fl_config;
	unsigned short bl_image;
	unsigned short bl_config;
	unsigned short lockdown;
	unsigned short guest_code;
	unsigned short total_count;
};

struct physical_address {
	unsigned short ui_firmware;
	unsigned short ui_config;
	unsigned short dp_config;
	unsigned short fl_config;
	unsigned short guest_code;
};


struct image_header_10 {
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char minor_header_version;
	unsigned char major_header_version;
	unsigned char reserved_08;
	unsigned char reserved_09;
	unsigned char reserved_0a;
	unsigned char reserved_0b;
	unsigned char top_level_container_start_addr[4];
};

struct image_header_05_06 {
	/* 0x00 - 0x0f */
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char options_firmware_id:1;
	unsigned char options_bootloader:1;
	unsigned char options_guest_code:1;
	unsigned char options_tddi:1;
	unsigned char options_reserved:4;
	unsigned char header_version;
	unsigned char firmware_size[4];
	unsigned char config_size[4];
	/* 0x10 - 0x1f */
	unsigned char product_id[PRODUCT_ID_SIZE];
	unsigned char package_id[2];
	unsigned char package_id_revision[2];
	unsigned char product_info[PRODUCT_INFO_SIZE];
	/* 0x20 - 0x2f */
	unsigned char bootloader_addr[4];
	unsigned char bootloader_size[4];
	unsigned char ui_addr[4];
	unsigned char ui_size[4];
	/* 0x30 - 0x3f */
	unsigned char ds_id[16];
	/* 0x40 - 0x4f */
	union {
		struct {
			unsigned char cstmr_product_id[PRODUCT_ID_SIZE];
			unsigned char reserved_4a_4f[6];
		};
		struct {
			unsigned char dsp_cfg_addr[4];
			unsigned char dsp_cfg_size[4];
			unsigned char reserved_48_4f[8];
		};
	};
	/* 0x50 - 0x53 */
	unsigned char firmware_id[4];
};

struct block_data {
	unsigned int size;
	const unsigned char *data;
};

struct image_metadata {
	bool contains_firmware_id;
	bool contains_bootloader;
	bool contains_guest_code;
	bool contains_disp_config;
	bool contains_perm_config;
	bool contains_flash_config;
	unsigned int firmware_id;
	unsigned int checksum;
	unsigned int bootloader_size;
	unsigned int disp_config_offset;
	unsigned char bl_version;
	unsigned char product_id[PRODUCT_ID_SIZE + 1];
	unsigned char cstmr_product_id[PRODUCT_ID_SIZE + 1];
	struct block_data bootloader;
	struct block_data ui_firmware;
	struct block_data ui_config;
	struct block_data dp_config;
	struct block_data pm_config;
	struct block_data fl_config;
	struct block_data bl_image;
	struct block_data bl_config;
	struct block_data lockdown;
	struct block_data guest_code;
	struct block_count blkcount;
	struct physical_address phyaddr;
};

struct synaptics_rmi4_fwu_handle {
	enum bl_version bl_version;
	bool initialized;
	bool in_bl_mode;
	bool force_update;
	bool do_lockdown;
	bool has_guest_code;
	bool new_partition_table;
	unsigned int data_pos;
	unsigned char *ext_data_source;
	unsigned char *read_config_buf;
	unsigned char intr_mask;
	unsigned char command;
	unsigned char bootloader_id[2];
	unsigned char config_id[32];
	unsigned char flash_status;
	unsigned char partitions;
	unsigned short block_size;
	unsigned short config_size;
	unsigned short config_area;
	unsigned short config_block_count;
	unsigned short flash_config_length;
	unsigned short payload_length;
	unsigned short partition_table_bytes;
	unsigned short read_config_buf_size;
	const unsigned char *config_data;
	const unsigned char *image;
	unsigned char *image_name;
	unsigned int image_size;
	struct image_metadata img;
	struct register_offset off;
	struct block_count blkcount;
	struct physical_address phyaddr;
	struct f34_v5v6_flash_properties flash_properties;
};

static struct synaptics_rmi4_fwu_handle *fwu;

static unsigned int le_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
			(unsigned int)ptr[1] * 0x100 +
			(unsigned int)ptr[2] * 0x10000 +
			(unsigned int)ptr[3] * 0x1000000;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static void fwu_parse_image_header_05_06(void)
{
	const unsigned char *image = NULL;
	struct image_header_05_06 *header;

	TOUCH_TRACE();

	image = fwu->image;
	header = (struct image_header_05_06 *)image;

	fwu->img.checksum = le_to_uint(header->checksum);

	fwu->img.bl_version = header->header_version;

	fwu->img.contains_bootloader = header->options_bootloader;
	if (fwu->img.contains_bootloader)
		fwu->img.bootloader_size = le_to_uint(header->bootloader_size);

	fwu->img.ui_firmware.size = le_to_uint(header->firmware_size);
	if (fwu->img.ui_firmware.size) {
		fwu->img.ui_firmware.data = image + IMAGE_AREA_OFFSET;
		if (fwu->img.contains_bootloader)
			fwu->img.ui_firmware.data += fwu->img.bootloader_size;
	}

	if ((fwu->img.bl_version == BL_V6) && header->options_tddi)
		fwu->img.ui_firmware.data = image + IMAGE_AREA_OFFSET;

	fwu->img.ui_config.size = le_to_uint(header->config_size);
	if (fwu->img.ui_config.size) {
		fwu->img.ui_config.data = fwu->img.ui_firmware.data +
				fwu->img.ui_firmware.size;
	}

	if ((fwu->img.bl_version == BL_V5 && fwu->img.contains_bootloader) ||
			(fwu->img.bl_version == BL_V6 && header->options_tddi))
		fwu->img.contains_disp_config = true;
	else
		fwu->img.contains_disp_config = false;

	if (fwu->img.contains_disp_config) {
		fwu->img.disp_config_offset = le_to_uint(header->dsp_cfg_addr);
		fwu->img.dp_config.size = le_to_uint(header->dsp_cfg_size);
		fwu->img.dp_config.data = image + fwu->img.disp_config_offset;
	} else {
		memcpy(fwu->img.cstmr_product_id, header->cstmr_product_id, PRODUCT_ID_SIZE);
		fwu->img.cstmr_product_id[PRODUCT_ID_SIZE] = 0;
	}

	fwu->img.contains_firmware_id = header->options_firmware_id;
	if (fwu->img.contains_firmware_id)
		fwu->img.firmware_id = le_to_uint(header->firmware_id);

	memcpy(fwu->img.product_id, header->product_id, PRODUCT_ID_SIZE);
	fwu->img.product_id[PRODUCT_ID_SIZE] = 0;

	fwu->img.lockdown.size = LOCKDOWN_SIZE;
	fwu->img.lockdown.data = image + IMAGE_AREA_OFFSET - LOCKDOWN_SIZE;

	return;
}

static int fwu_parse_image_info(struct device *dev)
{
	struct image_header_10 *header;

	TOUCH_TRACE();

	header = (struct image_header_10 *)fwu->image;

	memset(&fwu->img, 0x00, sizeof(fwu->img));

	if (header->major_header_version == IMAGE_HEADER_VERSION_06) {
		fwu_parse_image_header_05_06();
	} else {
		TOUCH_E("%s: Unsupported image file format (0x%02x)\n",
				__func__, header->major_header_version);
		return -EINVAL;
	}
	fwu->new_partition_table = false;

	return 0;
}

static int fwu_read_flash_status(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;
	unsigned char status = 0;
	unsigned char command = 0;

	TOUCH_TRACE();

	retval = synaptics_read(dev, FLASH_PAGE,
			d->f34.dsc.data_base + fwu->off.flash_status,
			&status,
			sizeof(status));
	if (retval < 0) {
		TOUCH_E("%s: Failed to read flash status\n", __func__);
		return retval;
	}

	fwu->in_bl_mode = status >> 7;

	if (fwu->bl_version == BL_V6) {
		fwu->flash_status = status & MASK_3BIT;
	} else {
		TOUCH_E("%s: Failed to get flash status mached bl_version\n",
				__func__);
		return EINVAL;
	}

	retval = synaptics_read(dev, FLASH_PAGE,
			d->f34.dsc.data_base + fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		TOUCH_E("%s: Failed to read flash command\n", __func__);
		return retval;
	}

	if (fwu->bl_version == BL_V6) {
		fwu->command = command & MASK_6BIT;
	} else {
		TOUCH_E("%s: Failed to get command matched bl_version\n",
				__func__);
		return EINVAL;
	}

	return 0;
}

void synaptics_rmi4_fwu_attn(struct device *dev)
{
	if (!fwu)
		return;

    //check interrupt mask, test for interrupt mode
	fwu_read_flash_status(dev);

	return;
}

static int fwu_wait_for_idle(struct device *dev, int timeout_ms, bool poll)
{
	int count = 0;
	int timeout_count = ((timeout_ms * 1000) / MAX_SLEEP_TIME_US) + 1;

	TOUCH_TRACE();

	do {
		usleep_range(MIN_SLEEP_TIME_US, MAX_SLEEP_TIME_US);
		count++;
		if (poll || (count == timeout_count))
			fwu_read_flash_status(dev);

		if ((fwu->command == CMD_IDLE) && (fwu->flash_status == 0x00))
			return 0;
	} while (count < timeout_count);

	TOUCH_E("%s: Timed out waiting for idle status\n", __func__);

	return -ETIMEDOUT;
}

static int fwu_write_f34_v5v6_command(struct device *dev, unsigned char cmd)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;
	unsigned char base = d->f34.dsc.data_base;
	unsigned char command = 0;

	TOUCH_TRACE();

	switch (cmd) {
	case CMD_IDLE:
		command = CMD_V5V6_IDLE;
		break;
	case CMD_WRITE_FW:
		command = CMD_V5V6_WRITE_FW;
		break;
	case CMD_WRITE_CONFIG:
		command = CMD_V5V6_WRITE_CONFIG;
		break;
	case CMD_WRITE_LOCKDOWN:
		command = CMD_V5V6_WRITE_LOCKDOWN;
		break;
	case CMD_WRITE_GUEST_CODE:
		command = CMD_V5V6_WRITE_GUEST_CODE;
		break;
	case CMD_READ_CONFIG:
		command = CMD_V5V6_READ_CONFIG;
		break;
	case CMD_ERASE_ALL:
		command = CMD_V5V6_ERASE_ALL;
		break;
	case CMD_ERASE_UI_CONFIG:
		command = CMD_V5V6_ERASE_UI_CONFIG;
		break;
	case CMD_ERASE_DISP_CONFIG:
		command = CMD_V5V6_ERASE_DISP_CONFIG;
		break;
	case CMD_ERASE_GUEST_CODE:
		command = CMD_V5V6_ERASE_GUEST_CODE;
		break;
	case CMD_ENABLE_FLASH_PROG:
		command = CMD_V5V6_ENABLE_FLASH_PROG;
		break;
	default:
		TOUCH_E("%s: Invalid command 0x%02x\n", __func__, cmd);
		return -EINVAL;
	}

	switch (cmd) {
	case CMD_ERASE_ALL:
	case CMD_ERASE_UI_CONFIG:
	case CMD_ERASE_DISP_CONFIG:
	case CMD_ERASE_GUEST_CODE:
	case CMD_ENABLE_FLASH_PROG:
		retval = synaptics_write(dev, FLASH_PAGE,
				base + fwu->off.payload,
				fwu->bootloader_id,
				sizeof(fwu->bootloader_id));
		if (retval < 0) {
			TOUCH_E("%s: Failed to write bootloader ID\n",
					__func__);
			return retval;
		}
		break;
	default:
		break;
	};

	fwu->command = command;

	retval = synaptics_write(dev, FLASH_PAGE,
			base + fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		TOUCH_E("%s: Failed to write command 0x%02x\n",
				__func__, command);
		return retval;
	}

	return 0;
}

static int fwu_write_f34_command(struct device *dev, unsigned char cmd)
{
	int retval = 0;

	TOUCH_TRACE();

	retval = fwu_write_f34_v5v6_command(dev, cmd);

	return retval;
}

static int fwu_read_f34_v5v6_queries(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;
	unsigned char count = 0;
	unsigned char base = d->f34.dsc.query_base;
	unsigned char buf[10] = {0, };
	struct f34_v5v6_flash_properties_2 properties_2;

	TOUCH_TRACE();

	retval = synaptics_read(dev,FLASH_PAGE,
			base + V5V6_BOOTLOADER_ID_OFFSET,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		TOUCH_E("%s: Failed to read bootloader ID\n", __func__);
		return retval;
	}

	if (fwu->bl_version == BL_V6) {
	       fwu->off.properties = V6_PROPERTIES_OFFSET;
	       fwu->off.properties_2 = V6_PROPERTIES_2_OFFSET;
	       fwu->off.block_size = V6_BLOCK_SIZE_OFFSET;
	       fwu->off.block_count = V6_BLOCK_COUNT_OFFSET;
	       fwu->off.gc_block_count = V6_GUEST_CODE_BLOCK_COUNT_OFFSET;
	       fwu->off.block_number = V6_BLOCK_NUMBER_OFFSET;
	       fwu->off.payload = V6_BLOCK_DATA_OFFSET;
	       fwu->off.flash_cmd = V6_FLASH_COMMAND_OFFSET;
	       fwu->off.flash_status = V6_FLASH_STATUS_OFFSET;
	} else {
		TOUCH_E("%s: Failed to get offset\n", __func__);
		return -EINVAL;
	}

	retval = synaptics_read(dev, FLASH_PAGE,
			base + fwu->off.block_size,
			buf,
			2);
	if (retval < 0) {
		TOUCH_E("%s: Failed to read block size info\n", __func__);
		return retval;
	}

	batohs(&fwu->block_size, &(buf[0]));

	retval = synaptics_read(dev, FLASH_PAGE,
			base + fwu->off.properties,
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		TOUCH_E("%s: Failed to read flash properties\n", __func__);
		return retval;
	}

	count = 4;

	if (fwu->flash_properties.has_pm_config)
		count += 2;

	if (fwu->flash_properties.has_bl_config)
		count += 2;

	if (fwu->flash_properties.has_disp_config)
		count += 2;

	retval = synaptics_read(dev, FLASH_PAGE,
			base + fwu->off.block_count,
			buf,
			count);
	if (retval < 0) {
		TOUCH_E("%s: Failed to read block count info\n", __func__);
		return retval;
	}

	batohs(&fwu->blkcount.ui_firmware, &(buf[0]));
	batohs(&fwu->blkcount.ui_config, &(buf[2]));

	count = 4;

	if (fwu->flash_properties.has_pm_config) {
		batohs(&fwu->blkcount.pm_config, &(buf[count]));
		count += 2;
	}

	if (fwu->flash_properties.has_bl_config) {
		batohs(&fwu->blkcount.bl_config, &(buf[count]));
		count += 2;
	}

	if (fwu->flash_properties.has_disp_config)
		batohs(&fwu->blkcount.dp_config, &(buf[count]));

	fwu->has_guest_code = false;

	if (fwu->flash_properties.has_query4) {
		retval = synaptics_read(dev, FLASH_PAGE,
				base + fwu->off.properties_2,
				properties_2.data,
				sizeof(properties_2.data));
		if (retval < 0) {
			TOUCH_E("%s: Failed to read flash properties 2\n",
					__func__);
			return retval;
		}

		if (properties_2.has_guest_code) {
			retval = synaptics_read(dev, FLASH_PAGE,
					base + fwu->off.gc_block_count,
					buf,
					2);
			if (retval < 0) {
				TOUCH_E("%s: Failed to read guest code block count\n",
						__func__);
				return retval;
			}

			batohs(&fwu->blkcount.guest_code, &(buf[0]));
			fwu->has_guest_code = true;
		}
	}

	return 0;
}

static int fwu_read_f34_queries(struct device *dev)
{
	int retval = 0;

	TOUCH_TRACE();

	memset(&fwu->blkcount, 0x00, sizeof(fwu->blkcount));
	memset(&fwu->phyaddr, 0x00, sizeof(fwu->phyaddr));

	retval = fwu_read_f34_v5v6_queries(dev);

	return retval;
}

static int fwu_write_f34_v5v6_blocks(struct device *dev, unsigned char *block_ptr,
		unsigned short block_cnt, unsigned char command)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;
	unsigned char base = d->f34.dsc.data_base;
	unsigned char block_number[] = {0, 0};
	unsigned short blk = 0;

	TOUCH_TRACE();
	block_number[1] |= (fwu->config_area << 5);

	retval = synaptics_write(dev, FLASH_PAGE,
			base + fwu->off.block_number,
			block_number,
			sizeof(block_number));
	if (retval < 0) {
		TOUCH_E("%s: Failed to write block number\n", __func__);
		return retval;
	}

	for (blk = 0; blk < block_cnt; blk++) {
		if ((blk % 100) == 0)
			TOUCH_I("%s: blk : (%d/%d)\n", __func__, blk, block_cnt);
		retval = synaptics_write(dev, FLASH_PAGE,
				base + fwu->off.payload,
				block_ptr,
				fwu->block_size);
		if (retval < 0) {
			TOUCH_E("%s: Failed to write block data (block %d)\n",
					__func__, blk);
			return retval;
		}

		retval = fwu_write_f34_command(dev, command);
		if (retval < 0) {
			TOUCH_E("%s: Failed to write command for block %d\n",
					__func__, blk);
			return retval;
		}

		retval = fwu_wait_for_idle(dev, WRITE_WAIT_MS, false);
		if (retval < 0) {
			TOUCH_E("%s: Failed to wait for idle status (block %d)\n",
					__func__, blk);
			return retval;
		}
		block_ptr += fwu->block_size;
	}

    return 0;
}

static int fwu_write_f34_blocks(struct device *dev, unsigned char *block_ptr,
		unsigned short block_cnt, unsigned char cmd)
{
	int retval = 0;

	TOUCH_TRACE();

	retval = fwu_write_f34_v5v6_blocks(dev, block_ptr, block_cnt, cmd);

	return retval;
}

static int fwu_get_device_config_id(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;
	unsigned char config_id_size = 0;

	TOUCH_TRACE();

	config_id_size = V5V6_CONFIG_ID_SIZE;

	retval = synaptics_read(dev, FLASH_PAGE,
				d->f34.dsc.control_base,
				fwu->config_id,
				config_id_size);
	if (retval < 0)
		return retval;

	return 0;
}

static int fwu_enter_flash_prog(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;
	struct f01_device_control f01_device_control;

	TOUCH_TRACE();

	retval = fwu_read_flash_status(dev);
	if (retval < 0)
		return retval;

	if (fwu->in_bl_mode)
		return 0;

	msleep(INT_DISABLE_WAIT_MS);

	retval = fwu_write_f34_command(dev, CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(dev, ENABLE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	if (!fwu->in_bl_mode) {
		TOUCH_E("%s: BL mode not entered\n", __func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries(dev);
	if (retval < 0)
		return retval;

	retval = synaptics_read(dev, COMMON_PAGE,
			d->f01.dsc.control_base,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		TOUCH_E("%s: Failed to read F01 device control\n",
				__func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = synaptics_write(dev, COMMON_PAGE,
			d->f01.dsc.control_base,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		TOUCH_E("%s: Failed to write F01 device control\n",
				__func__);
		return retval;
	}

	msleep(ENTER_FLASH_PROG_WAIT_MS);

	return retval;
}

static int fwu_check_ui_firmware_size(struct device *dev)
{
	unsigned short block_count =0;

	TOUCH_TRACE();
	block_count = fwu->img.ui_firmware.size / fwu->block_size;

	if (block_count != fwu->blkcount.ui_firmware) {
		TOUCH_E("%s: UI firmware size mismatch(%d - %d)\n",
				__func__,block_count,fwu->blkcount.ui_firmware);
		return -EINVAL;
	}

	return 0;
}

static int fwu_check_ui_configuration_size(struct device *dev)
{
	unsigned short block_count = 0;

	TOUCH_TRACE();

	block_count = fwu->img.ui_config.size / fwu->block_size;

	if (block_count != fwu->blkcount.ui_config) {
		TOUCH_E("%s: UI configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

static int fwu_check_dp_configuration_size(struct device *dev)
{
	unsigned short block_count = 0;
	TOUCH_TRACE();

	block_count = fwu->img.dp_config.size / fwu->block_size;

	if (block_count != fwu->blkcount.dp_config) {
		TOUCH_E("%s: Display configuration size mismatch\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int fwu_check_guest_code_size(struct device *dev)
{
	unsigned short block_count = 0;

	TOUCH_TRACE();

	block_count = fwu->img.guest_code.size / fwu->block_size;
	if (block_count != fwu->blkcount.guest_code) {
		TOUCH_E("%s: Guest code size mismatch\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int fwu_write_firmware(struct device *dev)
{
	unsigned short firmware_block_count = 0;

	TOUCH_TRACE();

	firmware_block_count = fwu->img.ui_firmware.size / fwu->block_size;

	return fwu_write_f34_blocks(dev, (unsigned char *)fwu->img.ui_firmware.data,
			firmware_block_count, CMD_WRITE_FW);
}

static int fwu_erase_configuration(struct device *dev)
{
	int retval = 0;
	TOUCH_TRACE();

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = fwu_write_f34_command(dev, CMD_ERASE_UI_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case DP_CONFIG_AREA:
		retval = fwu_write_f34_command(dev, CMD_ERASE_DISP_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case BL_CONFIG_AREA:
		retval = fwu_write_f34_command(dev, CMD_ERASE_BL_CONFIG);
		if (retval < 0)
			return retval;
		break;
	}

	TOUCH_I("%s: Erase command written\n", __func__);

	retval = fwu_wait_for_idle(dev, ERASE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	TOUCH_I("%s: Idle status detected\n", __func__);

	return retval;
}


static int fwu_erase_guest_code(struct device *dev)
{
	int retval = 0;

	TOUCH_TRACE();
	retval = fwu_write_f34_command(dev, CMD_ERASE_GUEST_CODE);
	if (retval < 0)
		return retval;

	TOUCH_I("%s: Erase command written\n", __func__);

	retval = fwu_wait_for_idle(dev, ERASE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	TOUCH_I("%s: Idle status detected\n", __func__);

	return 0;
}

static int fwu_erase_all(struct device *dev)
{
	int retval = 0;

	TOUCH_TRACE();
	retval = fwu_write_f34_command(dev, CMD_ERASE_ALL);
	if (retval < 0)
		return retval;

	TOUCH_I("%s: Erase all command written\n", __func__);

	retval = fwu_wait_for_idle(dev, ERASE_WAIT_MS, false);
	if (!(fwu->bl_version == BL_V8 &&
			fwu->flash_status == BAD_PARTITION_TABLE)) {
		if (retval < 0)
			return retval;
	}

	TOUCH_I("%s: Idle status detected\n",__func__);

	if (fwu->flash_properties.has_disp_config &&
			fwu->img.contains_disp_config) {
		fwu->config_area = DP_CONFIG_AREA;
		retval = fwu_erase_configuration(dev);
		if (retval < 0)
			return retval;
	}

	if (fwu->has_guest_code && fwu->img.contains_guest_code) {
		retval = fwu_erase_guest_code(dev);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static int fwu_write_configuration(struct device *dev)
{
	return fwu_write_f34_blocks(dev, (unsigned char *)fwu->config_data,
			fwu->config_block_count, CMD_WRITE_CONFIG);
}

static int fwu_write_ui_configuration(struct device *dev)
{
	TOUCH_TRACE();

	fwu->config_area = UI_CONFIG_AREA;
	fwu->config_data = fwu->img.ui_config.data;
	fwu->config_size = fwu->img.ui_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;

	return fwu_write_configuration(dev);
}

static int fwu_write_dp_configuration(struct device *dev)
{
	TOUCH_TRACE();

	fwu->config_area = DP_CONFIG_AREA;
	fwu->config_data = fwu->img.dp_config.data;
	fwu->config_size = fwu->img.dp_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;

	return fwu_write_configuration(dev);
}

static int fwu_write_guest_code(struct device *dev)
{
	int retval = 0;
	unsigned short guest_code_block_count = 0;

	TOUCH_TRACE();
	guest_code_block_count = fwu->img.guest_code.size / fwu->block_size;

	retval = fwu_write_f34_blocks(dev, (unsigned char *)fwu->img.guest_code.data,
			guest_code_block_count, CMD_WRITE_GUEST_CODE);
	if (retval < 0)
		return retval;

	return 0;
}

static int fwu_write_lockdown(struct device *dev)
{
	unsigned short lockdown_block_count;

	TOUCH_TRACE();
	lockdown_block_count = fwu->img.lockdown.size / fwu->block_size;

	return fwu_write_f34_blocks(dev, (unsigned char *)fwu->img.lockdown.data,
			lockdown_block_count, CMD_WRITE_LOCKDOWN);

}

static int fwu_do_reflash(struct device *dev)
{
	int retval = 0;

	TOUCH_TRACE();

	if (!fwu->new_partition_table) {
		TOUCH_I("%s new partition table\n", __func__);
		retval = fwu_check_ui_firmware_size(dev);
		if (retval < 0)
			return retval;

		retval = fwu_check_ui_configuration_size(dev);
		if (retval < 0)
			return retval;

		if (fwu->flash_properties.has_disp_config &&
				fwu->img.contains_disp_config) {
			retval = fwu_check_dp_configuration_size(dev);
			if (retval < 0)
				return retval;
		}

		if (fwu->has_guest_code && fwu->img.contains_guest_code) {
			retval = fwu_check_guest_code_size(dev);
			if (retval < 0)
				return retval;
		}
	}

	retval = fwu_erase_all(dev);
	if (retval < 0)
		return retval;

	retval = fwu_write_firmware(dev);
	if (retval < 0)
		return retval;
	TOUCH_I("%s: Firmware programmed\n", __func__);

	fwu->config_area = UI_CONFIG_AREA;
	retval = fwu_write_ui_configuration(dev);
	if (retval < 0)
		return retval;
	TOUCH_I("%s: Configuration programmed\n", __func__);

	if (fwu->flash_properties.has_disp_config &&
			fwu->img.contains_disp_config) {
		retval = fwu_write_dp_configuration(dev);
		if (retval < 0)
			return retval;
		TOUCH_I("%s: Display configuration programmed\n", __func__);
	}

	if (fwu->has_guest_code && fwu->img.contains_guest_code) {
		retval = fwu_write_guest_code(dev);
		if (retval < 0)
			return retval;
		TOUCH_I("%s: Guest code programmed\n", __func__);
	}

	return retval;
}

static int fwu_do_lockdown_v5v6(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;

	TOUCH_TRACE();
	retval = fwu_enter_flash_prog(dev);
	if (retval < 0)
		return retval;

	retval = synaptics_read(dev, FLASH_PAGE,
			d->f34.dsc.query_base + fwu->off.properties,
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		TOUCH_E("%s: Failed to read flash properties\n", __func__);
		return retval;
	}

	if (fwu->flash_properties.unlocked == 0) {
		TOUCH_E("%s: Device already locked down\n", __func__);
		return 0;
	}

	retval = fwu_write_lockdown(dev);
	if (retval < 0)
		return retval;

	TOUCH_E("%s: Lockdown programmed\n", __func__);

	return retval;
}

static int fwu_start_reflash(struct device *dev, const struct firmware *fw)
{
	int retval = 0;
	enum flash_area flash_area;

	TOUCH_TRACE();

	fwu->image = fw->data;

	retval = fwu_parse_image_info(dev);
	if (retval < 0)
		goto exit;

	if (fwu->blkcount.total_count != fwu->img.blkcount.total_count) {
		TOUCH_E("%s: Flash size mismatch\n", __func__);
		retval = -EINVAL;
		goto exit;
	}

	if (fwu->bl_version != fwu->img.bl_version) {
		TOUCH_E("%s: Bootloader version mismatch\n", __func__);
		retval = -EINVAL;
		goto exit;
	}

	if (!fwu->force_update && fwu->new_partition_table) {
		TOUCH_E("%s: Partition table mismatch\n", __func__);
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_read_flash_status(dev);
	if (retval < 0)
		goto exit;

	if (fwu->in_bl_mode) {
		TOUCH_E("%s: Device in bootloader mode\n", __func__);
	}


	flash_area = UI_FIRMWARE;
	retval = fwu_enter_flash_prog(dev);
	if (retval < 0) {
		TOUCH_E("%s: enter flash prog error\n",
				__func__);
		goto exit;
	}

	retval = fwu_do_reflash(dev);
	if (retval < 0) {
		TOUCH_E("%s: Failed to do reflash\n",
				__func__);
		goto exit;
	}

	if (fwu->do_lockdown && (fwu->img.lockdown.data != NULL)) {
		retval = fwu_do_lockdown_v5v6(dev);
		if (retval < 0) {
			TOUCH_E("%s: Failed to do lockdown\n",__func__);
		}
	}
exit:
	TOUCH_I("%s: End of reflash process\n", __func__);
	return retval;
}

static int synaptics_rmi4_fwu_init(struct device *dev)
{
	struct synaptics_data *d = to_synaptics_data(dev);
	int retval = 0;

	TOUCH_TRACE();

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu) {
		TOUCH_E("%s: Failed to alloc mem for fwu\n", __func__);
		retval = -ENOMEM;
		goto exit;
	}

	switch (d->f34.dsc.fn_version) {
	case F34_V0:
		fwu->bl_version = BL_V5;
		break;
	case F34_V1:
		fwu->bl_version = BL_V6;
		break;
	case F34_V2:
		fwu->bl_version = BL_V7;
		break;
	default:

		return -EINVAL;
	}
	TOUCH_I("%s: bl_version:%d\n", __func__, fwu->bl_version);


	retval = fwu_read_f34_queries(dev);
	if (retval < 0)
		goto exit_free_fwu;

	retval = fwu_get_device_config_id(dev);
	if (retval < 0) {
		TOUCH_E("%s: Failed to read device config ID\n",
				__func__);
		goto exit_free_fwu;
	}

	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = DO_LOCKDOWN;
	fwu->initialized = true;

	return 0;

exit_free_fwu:
	kfree(fwu);
	fwu = NULL;
exit:
	return retval;
}

int synaptics_fw_updater(struct device *dev,
				const struct firmware *fw)
{
	int retval = 0;

	TOUCH_TRACE();

	retval = synaptics_rmi4_fwu_init(dev);
	if (retval < 0) {
		TOUCH_E("%s: Failed to init for fw upgrade\n", __func__);
		return retval;
	}

	retval = fwu_start_reflash(dev, fw);
	if (retval < 0) {
		TOUCH_E("%s: Failed to upgrade\n", __func__);
		return retval;
	}

	return retval;
}


