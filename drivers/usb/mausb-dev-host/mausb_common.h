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

#ifndef __MAUSB_COMMON_H
#define __MAUSB_COMMON_H

#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/net.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/kthread.h>


#define MAUSB_VERSION "1.0.1"

#undef pr_fmt

//#define DEBUG 1

#ifdef DEBUG
#define pr_fmt(fmt)     KBUILD_MODNAME ": %s:%d: " fmt, __func__, __LINE__
#else
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt
#endif

enum {
	mausb_debug_xmit	= (1 << 0),
	mausb_debug_sysfs	= (1 << 1),
	mausb_debug_urb		= (1 << 2),
	mausb_debug_eh		= (1 << 3),

	mausb_debug_stub_cmp	= (1 << 8),
	mausb_debug_stub_dev	= (1 << 9),
	mausb_debug_stub_rx	= (1 << 10),
	mausb_debug_stub_tx	= (1 << 11),

	mausb_debug_vhci_rh	= (1 << 8),
	mausb_debug_vhci_hc	= (1 << 9),
	mausb_debug_vhci_rx	= (1 << 10),
	mausb_debug_vhci_tx	= (1 << 11),
	mausb_debug_vhci_sysfs  = (1 << 12)
};

#define mausb_dbg_flag_xmit	(mausb_debug_flag & mausb_debug_xmit)
#define mausb_dbg_flag_vhci_rh	(mausb_debug_flag & mausb_debug_vhci_rh)
#define mausb_dbg_flag_vhci_hc	(mausb_debug_flag & mausb_debug_vhci_hc)
#define mausb_dbg_flag_vhci_rx	(mausb_debug_flag & mausb_debug_vhci_rx)
#define mausb_dbg_flag_vhci_tx	(mausb_debug_flag & mausb_debug_vhci_tx)
#define mausb_dbg_flag_stub_rx	(mausb_debug_flag & mausb_debug_stub_rx)
#define mausb_dbg_flag_stub_tx	(mausb_debug_flag & mausb_debug_stub_tx)
#define mausb_dbg_flag_vhci_sysfs  (mausb_debug_flag & mausb_debug_vhci_sysfs)

extern unsigned long mausb_debug_flag;
extern struct device_attribute dev_attr_mausb_debug;

#define mausb_dbg_with_flag(flag, fmt, args...)		\
	do {						\
		if (flag & mausb_debug_flag)		\
			printk(KERN_INFO " \n"fmt,##args); \
	} while (0)

/*
//pr_debug(fmt, ##args);
//printk(KERN_INFO " \n"fmt,##args); \
*/

#define mausb_dbg_sysfs(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_sysfs, fmt , ##args)
#define mausb_dbg_xmit(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_xmit, fmt , ##args)
#define mausb_dbg_urb(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_urb, fmt , ##args)
#define mausb_dbg_eh(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_eh, fmt , ##args)

#define mausb_dbg_vhci_rh(fmt, args...)	\
	mausb_dbg_with_flag(mausb_debug_vhci_rh, fmt , ##args)
#define mausb_dbg_vhci_hc(fmt, args...)	\
	mausb_dbg_with_flag(mausb_debug_vhci_hc, fmt , ##args)
#define mausb_dbg_vhci_rx(fmt, args...)	\
	mausb_dbg_with_flag(mausb_debug_vhci_rx, fmt , ##args)
#define mausb_dbg_vhci_tx(fmt, args...)	\
	mausb_dbg_with_flag(mausb_debug_vhci_tx, fmt , ##args)
#define mausb_dbg_vhci_sysfs(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_vhci_sysfs, fmt , ##args)

#define mausb_dbg_stub_cmp(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_stub_cmp, fmt , ##args)
#define mausb_dbg_stub_rx(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_stub_rx, fmt , ##args)
#define mausb_dbg_stub_tx(fmt, args...) \
	mausb_dbg_with_flag(mausb_debug_stub_tx, fmt , ##args)

/*
 * USB/IP request headers
 *
 * Each request is transferred across the network to its counterpart, which
 * facilitates the normal USB communication. The values contained in the headers
 * are basically the same as in a URB. Currently, four request types are
 * defined:
 *
 *  - MAUSB_CMD_SUBMIT: a USB request block, corresponds to usb_submit_urb()
 *    (client to server)
 *
 *  - MAUSB_RET_SUBMIT: the result of MAUSB_CMD_SUBMIT
 *    (server to client)
 *
 *  - MAUSB_CMD_UNLINK: an unlink request of a pending MAUSB_CMD_SUBMIT,
 *    corresponds to usb_unlink_urb()
 *    (client to server)
 *
 *  - MAUSB_RET_UNLINK: the result of MAUSB_CMD_UNLINK
 *    (server to client)
 *
 */
#define MAUSB_CMD_SUBMIT	0x0001
#define MAUSB_CMD_UNLINK	0x0002
#define MAUSB_RET_SUBMIT	0x0003
#define MAUSB_RET_UNLINK	0x0004

#define MAUSB_DIR_OUT	0x00
#define MAUSB_DIR_IN	0x01

/*
 * This is the same as usb_iso_packet_descriptor but packed for pdu.
 */
struct mausb_iso_packet_descriptor {
	__u32 offset;
	__u32 length;			/* expected length */
	__u32 actual_length;
	__u32 status;
} __attribute__((packed));;

enum mausb_side {
	MAUSB_HOST,
	MAUSB_STUB,
};

enum mausb_status {
	/* sdev is available. */
	MAUSB_SDEV_ST_AVAILABLE = 0x01,
	/* sdev is now used. */
	MAUSB_SDEV_ST_USED,
	/* sdev is unusable because of a fatal error. */
	MAUSB_SDEV_ST_ERROR,

	/* vdev does not connect a remote device. */
	MAUSB_VDEV_ST_NULL,
	/* vdev is used, but the USB address is not assigned yet */
	MAUSB_VDEV_ST_NOTASSIGNED,
	MAUSB_VDEV_ST_USED,
	MAUSB_VDEV_ST_ERROR
};


/* event handler */
#define MAUSB_EH_SHUTDOWN	(1 << 0)
#define MAUSB_EH_BYE		(1 << 1)
#define MAUSB_EH_RESET		(1 << 2)
#define MAUSB_EH_UNUSABLE	(1 << 3)

#define SDEV_EVENT_REMOVED   (MAUSB_EH_SHUTDOWN | MAUSB_EH_RESET | MAUSB_EH_BYE)
#define	SDEV_EVENT_DOWN		(MAUSB_EH_SHUTDOWN | MAUSB_EH_RESET)
#define	SDEV_EVENT_ERROR_TCP	(MAUSB_EH_SHUTDOWN | MAUSB_EH_RESET)
#define	SDEV_EVENT_ERROR_SUBMIT	(MAUSB_EH_SHUTDOWN | MAUSB_EH_RESET)
#define	SDEV_EVENT_ERROR_MALLOC	(MAUSB_EH_SHUTDOWN | MAUSB_EH_UNUSABLE)

#define	VDEV_EVENT_REMOVED	(MAUSB_EH_SHUTDOWN | MAUSB_EH_BYE)
#define	VDEV_EVENT_DOWN		(MAUSB_EH_SHUTDOWN | MAUSB_EH_RESET)
#define	VDEV_EVENT_ERROR_TCP	(MAUSB_EH_SHUTDOWN | MAUSB_EH_RESET)
#define	VDEV_EVENT_ERROR_MALLOC	(MAUSB_EH_SHUTDOWN | MAUSB_EH_UNUSABLE)

/* a common structure for stub_device and vhci_device */
struct mausb_device {
	enum mausb_side side;
	enum mausb_status status;

	/* lock for status */
	spinlock_t lock;

	struct socket *tcp_socket;

	struct task_struct *tcp_rx;
	struct task_struct *tcp_tx;
/*
	struct task_struct *tcp_pal_mgnt;
	struct task_struct *tcp_pal_in;
	struct task_struct *tcp_pal_out;
*/
	unsigned long event;
	struct task_struct *eh;
	wait_queue_head_t eh_waitq;

	struct mausb_eh_ops {
		void (*shutdown)(struct mausb_device *);
		void (*reset)(struct mausb_device *);
		void (*unusable)(struct mausb_device *);
	} mausb_eh_ops;
}__attribute__((packed));


#define kthread_get_run(threadfn, data, namefmt, ...)			   \
({									   \
	struct task_struct *__k						   \
		= kthread_create(threadfn, data, namefmt, ## __VA_ARGS__); \
	if (!IS_ERR(__k)) {						   \
		get_task_struct(__k);					   \
		wake_up_process(__k);					   \
	}								   \
	__k;								   \
})

#define kthread_stop_put(k)		\
	do {				\
		kthread_stop(k);	\
		put_task_struct(k);	\
	} while (0)

/* mausb_common.c */
void mausb_dump_urb(struct urb *purb);

int mausb_recv(struct socket *sock, void *buf, int size);
struct socket *sockfd_to_socket(unsigned int sockfd);

struct mausb_iso_packet_descriptor*
mausb_alloc_iso_desc_pdu(struct urb *urb, ssize_t *bufflen);

/* some members of urb must be substituted before. */
int mausb_recv_iso(struct mausb_device *ud, struct urb *urb);
void mausb_pad_iso(struct mausb_device *ud, struct urb *urb);
int mausb_recv_xbuff(struct mausb_device *ud, struct urb *urb);

/* mausb_event.c */
int event_handler(struct mausb_device *ud);
int mausb_start_eh(struct mausb_device *ud);
void mausb_stop_eh(struct mausb_device *ud);
void mausb_event_add(struct mausb_device *ud, unsigned long event);
int mausb_event_happened(struct mausb_device *ud);

static inline int interface_to_busnum(struct usb_interface *interface)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	return udev->bus->busnum;
}

static inline int interface_to_devnum(struct usb_interface *interface)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	return udev->devnum;
}

#endif /* __MAUSB_COMMON_H */
