/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef _LGE_BOARD_REVISION_H
#define _LGE_BOARD_REVISION_H

#if defined(CONFIG_MACH_MSM8996_ELSA)
enum hw_rev_no {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_EVB3,
	HW_REV_0,
	HW_REV_0_1,
	HW_REV_0_2,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_1_3,
	HW_REV_MAX
};
#else
enum hw_rev_no {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_EVB3,
	HW_REV_0,
	HW_REV_0_1,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_MAX
};
#endif

extern char *rev_str[];

enum hw_rev_no lge_get_board_rev_no(void);
char *lge_get_board_revision(void);

#endif
