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
#if defined(CONFIG_MACH_MSM8974_G2_LGU)
#define BTA_NOT_USE_ROOT_PERM
#define QOS_REQUEST_MSM
#define UART_CONTROL_MSM
#define BT_PORT_NUM		99
#elif defined(CONFIG_MACH_PDA)
#define QOS_REQUEST_TEGRA
#elif defined(CONFIG_MACH_MSM8926_JAGNM_GLOBAL_COM) || defined(CONFIG_MACH_MSM8926_JAGNM_KDDI_JP) || defined(CONFIG_MACH_MSM8226_JAG3GDS_GLOBAL_COM) || defined(CONFIG_MACH_MSM8226_JAG3GSS_GLOBAL_COM) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CMCC_CN) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CUCC_CN) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CTC_CN)
#define BTA_NOT_USE_ROOT_PERM
#define QOS_REQUEST_MSM
#define UART_CONTROL_MSM
#define BT_PORT_NUM		99
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
#elif defined(CONFIG_LGE_BLUETOOTH_PM)
//BT_S : [CONBT-1140] Remove QoS feature (It makes too much current consumption with octa-core AP)
//#define QOS_REQUEST_MSM
//BT_E : [CONBT-1140] Remove QoS feature (It makes too much current consumption with octa-core AP)
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
#define UART_CONTROL_MSM
#define BT_PORT_NUM		0
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
#endif/*CONFIG_BT_WITH_MSM*/


#ifdef QOS_REQUEST_MSM
#define REQUESTED		1
#define NOT_REQUESTED	2
#endif/*QOS_REQUEST_MSM*/


#ifdef UART_CONTROL_MSM
#include <linux/platform_data/msm_serial_hs.h>
#endif/*UART_CONTROL_MSM*/


#if defined(CONFIG_BCM4335BT)
/* +++BRCM 4335 AXI Patch */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
/* ---BRCM */
#endif /* defined(CONFIG_BCM4335BT) */


#define BT_RESET_GPIO  		25
#define BT_EXT_WAKE_GPIO    105
#define BT_HOST_WAKE_GPIO   95

#if 0
#if defined(CONFIG_MACH_MSM8974_G2_LGU)
#define BT_RESET_GPIO  		41
#define BT_EXT_WAKE_GPIO    62
#define BT_HOST_WAKE_GPIO   42
#elif defined(CONFIG_MACH_PDA)
#include "../gpio-names.h"

#define BT_RESET_GPIO  		25
#define BT_EXT_WAKE_GPIO    105
#define BT_HOST_WAKE_GPIO   95
#elif defined(CONFIG_MACH_MSM8926_JAGNM_GLOBAL_COM) || defined(CONFIG_MACH_MSM8926_JAGNM_KDDI_JP) || defined(CONFIG_MACH_MSM8226_JAG3GDS_GLOBAL_COM) || defined(CONFIG_MACH_MSM8226_JAG3GSS_GLOBAL_COM) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CMCC_CN) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CUCC_CN) || defined(CONFIG_MACH_MSM8926_JAGDSNM_CTC_CN)
#define BT_RESET_GPIO  		45
#define BT_EXT_WAKE_GPIO    47
#define BT_HOST_WAKE_GPIO   48
#endif
#endif

#ifdef BTA_NOT_USE_ROOT_PERM
#define AID_BLUETOOTH       1002  /* bluetooth subsystem */
#define AID_NET_BT_STACK  	3008  /* bluetooth: access config files */
#endif  //BTA_NOT_USE_ROOT_PERM


#endif/*__MACH_LGE_BLUETOOTH_H*/
