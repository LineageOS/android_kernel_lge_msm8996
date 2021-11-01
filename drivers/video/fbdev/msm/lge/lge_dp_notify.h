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

#ifndef LGE_DP_NOTIFY_H
#define LGE_DP_NOTIFY_H

#include <linux/module.h>
#include <linux/switch.h>

void register_dp_notify_node(void);
void tusb422_set_dp_notify_node(int val);

#endif /*LGE_MDSS_FB_H */
