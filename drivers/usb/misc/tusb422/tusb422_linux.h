/*
 * TUSB422 Power Delivery
 *
 * Author: Brian Quach <brian.quach@ti.com>
 *
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _TUSB422_LINUX_H_
#define _TUSB422_LINUX_H_

#include <linux/types.h>
#include <linux/delay.h>
#include "tusb422_common.h"

enum vbus_sel_t
{
	VBUS_SEL_SRC_5V       = (1 << 0),
	VBUS_SEL_SRC_HI_VOLT  = (1 << 1),
	VBUS_SEL_SNK          = (1 << 2)
};

int tusb422_read(int reg, void *data, int len);
int tusb422_write(int reg, const void *data, int len);
int tusb422_modify_reg(int reg, int clr_mask, int set_mask);
int tusb422_set_vbus(int vbus_sel);
int tusb422_clr_vbus(int vbus_sel);
#ifdef CONFIG_LGE_USB_TYPE_C
int tusb422_set_vconn_enable(int enable);
#endif
void tusb422_msleep(int msecs);

void tusb422_set_timer_func(void (*function)(unsigned int));
void tusb422_clr_timer_func(void);
int tusb422_start_timer(unsigned int timeout_ms);
int tusb422_stop_timer(void);

#ifdef CONFIG_WAKELOCK
void tusb422_wake_lock_attach(void);
void tusb422_wake_lock_detach(void);
#endif

#endif
