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

/************************************************************************************
 *
 *  Filename:    antdrv.h
 *
 *  Description: Common header for all ANT+ driver sub-modules.
 *
 ***********************************************************************************/
#ifndef _ANT_DRV_H
#define _ANT_DRV_H
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include "include/ant.h"

/*******************************************************************************
**  Constants & Macros
*******************************************************************************/
#define ANT_DRV_VERSION            "1.01"

/* Should match with ANT_DRV_VERSION */
#define ANT_DRV_RADIO_VERSION      KERNEL_VERSION(1, 0, 1)
#define ANT_DRV_NAME               "brcm_antdrv"
#define ANT_DRV_CARD_SHORT_NAME    "BRCM ANT+ Radio"
#define ANT_DRV_CARD_LONG_NAME     "Broadcom corporation ANT+ Radio"

/* Flag info */
#define ANT_CORE_READY                 3

#define ANT_DRV_TX_TIMEOUT       (5*HZ)  /* 5 sec */
#define ANT_DRV_RX_SEEK_TIMEOUT       (20*HZ)  /* 20 sec */

#define SIZEA(array) (sizeof(array) / sizeof(array[0]))

#define ANT_READ_1_BYTE_DATA     1
#define ANT_READ_2_BYTE_DATA     2

#define WORKER_QUEUE TRUE

enum {
    ANT_MODE_OFF,
    ANT_MODE_TX,
    ANT_MODE_RX,
    ANT_MODE_ENTRY_MAX
};


/*******************************************************************************
**  Type definitions
*******************************************************************************/
struct ant_device_info {
    unsigned int capabilities;       /* Device capabilities */
    enum v4l2_tuner_type type;
};

/* ant driver operation structure */
struct antdrv_ops {
    struct video_device *radio_dev;   /* V4L2 video device pointer */

    wait_queue_head_t rx_wait;
    int rx_wake_flag;
    struct sk_buff_head rx_q;          /* RX queue */

    long (*bcm_write) (struct sk_buff *skb);

    wait_queue_head_t tx_wait;
    int tx_wake_flag;

    long flag;
    struct ant_device_info device_info; /* ANT Device info */
};

#endif
