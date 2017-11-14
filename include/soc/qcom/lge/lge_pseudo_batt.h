/* Copyright (c) 2014 LG Electronics. All rights reserved.
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
#ifndef __LGE_PSEUDO_BATT_H
#define __LGE_PSEUDO_BATT_H

struct pseudo_batt_info_type {
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};

enum {
	PSEUDO_BATT_MODE = 0,
	PSEUDO_BATT_ID,
	PSEUDO_BATT_THERM,
	PSEUDO_BATT_TEMP,
	PSEUDO_BATT_VOLT,
	PSEUDO_BATT_CAPACITY,
	PSEUDO_BATT_CHARGING,
};

void set_pseudo_batt_info(struct pseudo_batt_info_type *);
int get_pseudo_batt_info(int type);

#endif
