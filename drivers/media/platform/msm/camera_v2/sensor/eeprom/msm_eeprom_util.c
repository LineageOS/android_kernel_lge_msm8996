/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include "msm_eeprom_util.h"

static struct class *camera_vendor_id_class = NULL;
static uint8_t module_maker_id = 0;

static ssize_t show_LGCameraMainID(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	switch (module_maker_id) {
		case 0x01:
		case 0x02:
		case 0x05:
		case 0x06:
		case 0x07:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "LGIT");
		case 0x03:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "Fujifilm");
		case 0x04:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "Minolta");
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "Cowell");
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "IM-tech");
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "Sunny");
		case 0x25:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "Mitsumi");
		default:
			return sprintf(buf, "id:0x%x, %s\n", module_maker_id, "Reserved for future");
	}
}
static DEVICE_ATTR(vendor_id, S_IRUGO, show_LGCameraMainID, NULL);

void msm_eeprom_set_maker_id(uint8_t v_id)
{
	if(!module_maker_id)
		module_maker_id = v_id;
	return;
}

void msm_eeprom_create_sysfs(void)
{
	struct device*  camera_vendor_id_dev;
	if(!camera_vendor_id_class) {
		camera_vendor_id_class = class_create(THIS_MODULE, "camera");
		camera_vendor_id_dev   = device_create(camera_vendor_id_class, NULL,
											   0, NULL, "vendor_id");
		device_create_file(camera_vendor_id_dev, &dev_attr_vendor_id);
	}
}

void msm_eeprom_destroy_sysfs(void)
{
	if(camera_vendor_id_class) {
		class_destroy(camera_vendor_id_class);
		camera_vendor_id_class = NULL;
		module_maker_id = 0;
	}
}
