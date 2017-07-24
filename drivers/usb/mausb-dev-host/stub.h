/*
 * Copyright (C) 2003-2008 Takahiro Hirofuchi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#ifndef __MAUSB_STUB_H
#define __MAUSB_STUB_H

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/ktime.h>

#define MAUSB_STUB_BUSID_OTHER 0
#define MAUSB_STUB_BUSID_REMOV 1
#define MAUSB_STUB_BUSID_ADDED 2
#define MAUSB_STUB_BUSID_ALLOC 3

struct stub_device {
	struct usb_interface *interface;
	struct usb_device *udev;

	struct mausb_device ud;

	__u32 devid;

	/*
	 * stub_mausb_pal preserves private data of each urb.
	 * It is allocated as stub_mausb_pal and assigned to urb->context.
	 *
	 * stub_priv is always linked to any one of 3 lists;
	 *	priv_init: linked to this until the comletion of a urb.
	 *	priv_tx  : linked to this after the completion of a urb.
	 *	priv_free: linked to this after the sending of the result.
	 *
	 * Any of these list operations should be locked by priv_lock.
	 */

	spinlock_t mausb_pal_lock;
	struct list_head mausb_pal_mgmt_init;
	struct list_head mausb_pal_in_init;
	struct list_head mausb_pal_out_init;
	struct list_head mausb_pal_submit;
	struct list_head mausb_pal_tx;
	struct list_head mausb_pal_free;

	struct list_head mausb_unlink_tx;
	struct list_head mausb_unlink_free;


	/* see comments for unlinking in stub_rx.c */
	wait_queue_head_t tx_waitq;
/*
	wait_queue_head_t pal_mgmt_waitq;
	wait_queue_head_t pal_in_waitq;
	wait_queue_head_t pal_out_waitq;
*/
};


struct stub_unlink {
	unsigned long seqnum;
	struct list_head list;
	__u32 status;
};

/* mausb  pal*/
struct stub_mausb_pal {
	unsigned int seqnum;
	unsigned long length;
	struct list_head list;
	struct stub_device *sdev;
	void *pdu;
	struct urb *urb;
	int unlinking;
};

/* same as SYSFS_BUS_ID_SIZE */
#define BUSID_SIZE 32

struct bus_id_priv {
	char name[BUSID_SIZE];
	char status;
	int interf_count;
	struct stub_device *sdev;
	char shutdown_busid;
};


/* stub_dev.c */
extern struct usb_driver stub_driver;

/* stub_main.c */
struct bus_id_priv *get_busid_priv(const char *busid);
int del_match_busid(char *busid);
void stub_device_cleanup_urbs(struct stub_device *sdev);

/* stub_rx.c */
int stub_rx_loop(void *data);

/* stub_tx.c */

int stub_tx_loop(void *data);
/*mausb_main.c*/
int stub_pal_mgnt_loop(void *data);
int stub_pal_in_loop(void *data);
int stub_pal_out_loop(void *data);
int get_pipe(struct stub_device *sdev, int epnum, int dir);
void masking_bogus_flags(struct urb *urb);
int mausb_bind_unbind(const char *buf);
void stub_free_mausb_pal_and_urb(struct stub_mausb_pal *pal);
void mausb_enqueue_ret_unlink(struct stub_device *sdev, struct stub_mausb_pal *pal, __u32 seqnum, __u32 status);
int tweak_special_requests(struct urb *urb);

#endif /* __MAUSB_STUB_H */
