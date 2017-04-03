/*
 * FIPS 200 support.
 *
 * Copyright (c) 2008 Neil Horman <nhorman@tuxdriver.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#include "internal.h"

int cc_mode;
int cc_mode_flag;

EXPORT_SYMBOL_GPL(cc_mode);
EXPORT_SYMBOL_GPL(cc_mode_flag);

int get_cc_mode_state(void)
{
	return cc_mode_flag;
}
EXPORT_SYMBOL_GPL(get_cc_mode_state);

static int cc_mode_enable(char *str)
{
	cc_mode_flag = simple_strtol(str, NULL, 10);
	if ((cc_mode_flag > 0x7f) || (cc_mode_flag < 0)) {
		cc_mode_flag = 0;
	}
	cc_mode = cc_mode_flag & 0x01;
	printk(KERN_INFO "CCMODE: CC mode %s\n",
		cc_mode ? "enabled" : "disabled");
	return 1;
}

__setup("cc_mode=", cc_mode_enable);
