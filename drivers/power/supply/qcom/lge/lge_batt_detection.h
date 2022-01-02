/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>

/*Overrides the default battery type string with the values 
  from lge_battery_id.h to get the correct battery profile
  for the phones and enable battery metrics, iterated version
  now has an improved and more flexible naming scheme to enable
  further improvements*/
char *return_lge_battery_name(void);