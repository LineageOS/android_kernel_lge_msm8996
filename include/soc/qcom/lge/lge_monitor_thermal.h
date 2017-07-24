/*
 * include/soc/qcom/lge/lge_monitor_thermal.h
 *
 * Copyright (C) 2016 LG Electronics, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef CONFIG_MACH_MSM8996_LUCYE
#define VTS_WEIGHT_XO_TEMP      (260)    /*  0.260     */
#define VTS_WEIGHT_BD2_TEMP     (380)    /*  0.380     */
#define VTS_CONST_1             (13940)  /* 13.940     */
#define VTS_WEIGHT_BY_PERCENT   (100)    /* scaling .0 */
#define VTS_UNIT_DECIDEGREE     (10)
#else
/* Alice default data
 * does not have meaning to other model & platform */
#define VTS_WEIGHT_XO_TEMP      (58)
#define VTS_WEIGHT_BD2_TEMP     (40)
#define VTS_WEIGHT_BY_PERCENT   (100)
#define VTS_UNIT_DECIDEGREE     (10)
#endif
