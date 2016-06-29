#ifndef MTSK_TTY_H_
#define MTSK_TTY_H_
/*
 * DIAG MTS for LGE MTS Kernel Driver
 *
 *  lg-msp TEAM <lg-msp@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include <linux/list.h>
#include "stddef.h"
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include <linux/diagchar.h>

#include <linux/usb/usbdiag.h>

#ifdef CONFIG_DIAG_BRIDGE_CODE
#include "diagfwd_hsic.h"
#include "diagfwd_bridge.h"
#endif
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <asm/current.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/suspend.h>

#define MTS_TTY_MODULE_NAME		"MTS_TTY"
#define DIAG_MTS_RX_MAX_PACKET_SIZE	9000     /* max size = 9000B */
#define DIAG_MTS_TX_SIZE		8192
#define MAX_DIAG_MTS_DRV		1

/* mts tty driver ioctl values */
#define MTS_TTY_IOCTL_MAGIC	'S'
#define MTS_TTY_START	_IOWR(MTS_TTY_IOCTL_MAGIC, 0x01, int)
#define MTS_TTY_STOP	_IOWR(MTS_TTY_IOCTL_MAGIC, 0x02, int)
#define MTS_TTY__READ	_IOWR(MTS_TTY_IOCTL_MAGIC, 0x03, int)
#define MTS_TTY_START_READY    _IOWR(MTS_TTY_IOCTL_MAGIC, 0x04, int)
#define MTS_TTY_STOP_READY    _IOWR(MTS_TTY_IOCTL_MAGIC, 0x05, int)

#define MTS_OFF         (0)
#define MTS_START_READY (1)
#define MTS_ON          (2)
#define MTS_STOP_READY  (3)
#define MTS_STOP_READY_COMPLETE  (4)

struct mts_tty {
	wait_queue_head_t waitq;
	struct tty_driver *tty_drv;
	struct tty_struct *tty_struct;
    struct tty_port *mts_tty_port;
//	struct notifier_block pm_notify;
//	int pm_notify_info;
//    int count;
	spinlock_t lock;
	int run;
};

extern struct mts_tty *mts_tty;

int mts_tty_process(char *, int);
//int mts_tty_send_mask(struct diag_request *diag_read_ptr);
#endif /* MTSK_TTY_H_ */

