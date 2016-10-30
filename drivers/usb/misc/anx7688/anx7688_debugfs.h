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

#ifndef __ANX7688_DEBUGFS_H__
#define __ANX7688_DEBUGFS_H__

#include "anx7688_core.h"
#include "anx7688_firmware.h"

struct anx7688_debugfs {
	struct anx7688_chip *chip;
	struct anx7688_firmware *fw;
	int offset;
	ktime_t start_time;
	int chip_ver;
	int retry_count;
};

int anx7688_debugfs_init(struct anx7688_chip *chip);
void anx7688_debugfs_cleanup(void);

#endif /* __ANX7688_DEBUGFS_H__ */
