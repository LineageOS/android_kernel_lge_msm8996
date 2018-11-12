/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#ifndef MSM_EEPROM_UTIL_H
#define MSM_EEPROM_UTIL_H

#ifdef CONFIG_MACH_MSM8996_LUCYE
#define EEPROM_OFFSET_MODULE_MAKER		0xBE0//0x700
#define EEPROM_OFFSET_MODULE_MAKER1		0x0
#else
#define EEPROM_OFFSET_MODULE_MAKER		0x700
#endif

void msm_eeprom_set_maker_id(uint8_t);
void msm_eeprom_create_sysfs(void);
void msm_eeprom_destroy_sysfs(void);

#endif
