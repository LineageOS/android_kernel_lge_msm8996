/* Copyright (c) 2012 LG Electronics. All rights reserved.
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

#ifndef __MACH_LGE_BLUETOOTH_H
#define __MACH_LGE_BLUETOOTH_H

// Basic Features...
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
#include <linux/platform_data/msm_serial_hs.h>
#define BT_PORT_NUM		0
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver

#define BT_RESET_GPIO		25
#define BT_EXT_WAKE_GPIO	105
#define BT_HOST_WAKE_GPIO	95

#endif /* __MACH_LGE_BLUETOOTH_H */
