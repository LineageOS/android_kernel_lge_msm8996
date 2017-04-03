/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.


 *  Copyright (C) 2009-2016 Broadcom Corporation
 */


/*****************************************************************************/

/*******************************************************************************
 *
 *  Filename:      v4l2_logs.h
 *
 *  Description:   defines flags which enable various types of logs.
 *
 ******************************************************************************/

#include "v4l2_target.h"


#define V4L2_DBG_INIT     (1 << 0) /* enable logs for init and release in drivers */
#define V4L2_DBG_OPEN     (1 << 1) /* enable logs for open call to drivers */
#define V4L2_DBG_CLOSE    (1 << 2) /* enable logs for close call to drivers */
#define V4L2_DBG_TX       (1 << 3) /* enable logs for tx in drivers */
#define V4L2_DBG_RX       (1 << 4) /* enable logs for rx in drivers */

extern int ldisc_dbg_param;
extern int bt_dbg_param;
extern int fm_dbg_param;
extern int ant_dbg_param;

#if BTLDISC_DEBUG
#define BT_LDISC_DBG(flag, fmt, arg...) \
    do { \
        if (ldisc_dbg_param & flag) \
            printk(KERN_DEBUG "(brcmldisc):%s:%d  "fmt"\n" , \
                                            __func__, __LINE__, ## arg); \
    } while(0)
#else
#define BT_LDISC_DBG(flag, fmt, arg...)
#endif
#define BT_LDISC_ERR(fmt, arg...)  printk(KERN_ERR "(brcmldisc)ERR:%s:%d  "fmt"\n" , \
                                           __func__, __LINE__,## arg)

#if BTLDISC_DEBUG
#define BRCM_HCI_DBG(flag, fmt, arg...) \
    do { \
        if (ldisc_dbg_param & flag) \
            printk(KERN_DEBUG "(brcmhci):%s:%d  "fmt"\n" , \
                                            __func__, __LINE__, ## arg); \
    } while(0)
#else
#define BRCM_HCI_DBG(flag,fmt, arg...)
#endif
#define BRCM_HCI_ERR(fmt, arg...)  printk(KERN_ERR "(brcmhci):%s:%d  "fmt"\n" , \
                                           __func__, __LINE__,## arg)

/* Debugging for BT protocol driver */
#if BTDRV_DEBUG
#define BT_DRV_DBG(flag, fmt, arg...) \
    do { \
        if (bt_dbg_param & flag) \
            printk(KERN_DEBUG "(btdrv):%s:%d  "fmt"\n" , \
                                            __func__, __LINE__, ## arg); \
    } while(0)
#else
    #define BT_DRV_DBG(flag, fmt, arg...)
#endif
#define BT_DRV_ERR(fmt, arg...)  printk(KERN_ERR "(btdrv):%s:%d  "fmt"\n" , \
                                           __func__, __LINE__,## arg)

#if V4L2_FM_DEBUG
#define V4L2_FM_DRV_DBG(flag, fmt, arg...) \
    do { \
        if (fm_dbg_param & flag) \
            printk(KERN_DEBUG "(v4l2fmdrv):%s:%d  "fmt"\n" , \
                                            __func__, __LINE__, ## arg); \
    } while(0)
#else
#define V4L2_FM_DRV_DBG(flag, fmt, arg...)
#endif
#define V4L2_FM_DRV_ERR(fmt, arg...)  printk(KERN_ERR "(v4l2fmdrv):%s:%d  "fmt"\n" , \
                                           __func__, __LINE__,## arg)

#if V4L2_ANT_DEBUG
#define V4L2_ANT_DBG(flag, fmt, arg...) \
    do { \
        if (ant_dbg_param & flag) \
            printk(KERN_DEBUG "(v4l2ant):%s:%d  "fmt"\n" , \
                                            __func__, __LINE__,## arg); \
    } while(0)
#else
#define V4L2_ANT_DBG(flag, fmt, arg...)
#endif
#define V4L2_ANT_ERR(fmt, arg...)  printk(KERN_ERR "(v4l2ant):%s:%d "fmt"\n" , \
                                           __func__,__LINE__,## arg)

