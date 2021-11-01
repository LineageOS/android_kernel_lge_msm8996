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

#include "lge_dp_notify.h"

struct switch_dev dp_notify_sdev = {
	.name = "dp_notify",
};

void register_dp_notify_node()
{
	if (switch_dev_register(&dp_notify_sdev) < 0) {
		pr_err("%s: dp_notify switch registration failed\n", __func__);
	}
}
EXPORT_SYMBOL(register_dp_notify_node);

void tusb422_set_dp_notify_node(int val)
{
	int state = 0;

	state = dp_notify_sdev.state;

	if (val != state) {
		switch_set_state(&dp_notify_sdev, val);

		pr_err("%s: DisplayPort notify state is changed to %d\n", __func__,
				dp_notify_sdev.state);
	}
}
EXPORT_SYMBOL(tusb422_set_dp_notify_node);
