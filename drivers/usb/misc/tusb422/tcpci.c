/*
 * Texas Instruments TUSB422 Power Delivery
 *
 * Author: Brian Quach <brian.quach@ti.com>
 * Copyright: (C) 2016 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "tusb422_common.h"
#include "tcpci.h"

uint8_t tcpc_reg_role_ctrl_set(bool drp, tcpc_role_rp_val_t rp_val, tcpc_role_cc_t cc1, tcpc_role_cc_t cc2)
{
	return(((drp) ? TCPC_ROLE_CTRL_DRP : 0) |
		   ((rp_val) << TCPC_ROLE_CTRL_RP_VALUE_SHIFT) |
		   ((cc2) << TCPC_ROLE_CTRL_CC2_SHIFT) |
		   (cc1));
}
