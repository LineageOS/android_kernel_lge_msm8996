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

#include <asm/byteorder.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <net/sock.h>

#include "mausb_common.h"
#include "mausb_util.h"

#define DRIVER_AUTHOR "Takahiro Hirofuchi <hirofuchi@users.sourceforge.net>"
#define DRIVER_DESC "USB/IP Core"

#ifdef CONFIG_MAUSB_DEBUG
unsigned long mausb_debug_flag=0xffffffff;
#else
unsigned long mausb_debug_flag=0;
#endif
EXPORT_SYMBOL_GPL(mausb_debug_flag);
module_param(mausb_debug_flag, ulong, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mausb_debug_flag, "debug flags (defined in mausb_common.h)");

/* FIXME */
struct device_attribute dev_attr_mausb_debug;
EXPORT_SYMBOL_GPL(dev_attr_mausb_debug);

static ssize_t mausb_debug_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%lx\n", mausb_debug_flag);
}

static ssize_t mausb_debug_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	sscanf(buf, "%lx", &mausb_debug_flag);
	return count;
}
DEVICE_ATTR_RW(mausb_debug);

static void mausb_dump_buffer(char *buff, int bufflen)
{
//	print_hex_dump(KERN_DEBUG, "mausb-core", DUMP_PREFIX_OFFSET, 16, 4,
//		       buff, bufflen, false);
}

static void mausb_dump_pipe(unsigned int p)
{
	unsigned char type = usb_pipetype(p);
	unsigned char ep   = usb_pipeendpoint(p);
	unsigned char dev  = usb_pipedevice(p);
	unsigned char dir  = usb_pipein(p);

	pr_debug("dev(%d) ep(%d) [%s] ", dev, ep, dir ? "IN" : "OUT");

	switch (type) {
	case PIPE_ISOCHRONOUS:
		pr_debug("ISO\n");
		break;
	case PIPE_INTERRUPT:
		pr_debug("INT\n");
		break;
	case PIPE_CONTROL:
		pr_debug("CTRL\n");
		break;
	case PIPE_BULK:
		pr_debug("BULK\n");
		break;
	default:
		pr_debug("ERR\n");
		break;
	}
}

static void mausb_dump_usb_device(struct usb_device *udev)
{
	struct device *dev = &udev->dev;
	int i;

	dev_dbg(dev, "       devnum(%d) devpath(%s) ",
		udev->devnum, udev->devpath);

	switch (udev->speed) {
	case USB_SPEED_HIGH:
		pr_debug("SPD_HIGH ");
		break;
	case USB_SPEED_FULL:
		pr_debug("SPD_FULL ");
		break;
	case USB_SPEED_LOW:
		pr_debug("SPD_LOW ");
		break;
	case USB_SPEED_UNKNOWN:
		pr_debug("SPD_UNKNOWN ");
		break;
	default:
		pr_debug("SPD_ERROR ");
		break;
	}

	pr_debug("tt %p, ttport %d\n", udev->tt, udev->ttport);

	dev_dbg(dev, "                    ");
	for (i = 0; i < 16; i++)
		pr_debug(" %2u", i);
	pr_debug("\n");

	dev_dbg(dev, "       toggle0(IN) :");
	for (i = 0; i < 16; i++)
		pr_debug(" %2u", (udev->toggle[0] & (1 << i)) ? 1 : 0);
	pr_debug("\n");

	dev_dbg(dev, "       toggle1(OUT):");
	for (i = 0; i < 16; i++)
		pr_debug(" %2u", (udev->toggle[1] & (1 << i)) ? 1 : 0);
	pr_debug("\n");

	dev_dbg(dev, "       epmaxp_in   :");
	for (i = 0; i < 16; i++) {
		if (udev->ep_in[i])
			pr_debug(" %2u",
			    le16_to_cpu(udev->ep_in[i]->desc.wMaxPacketSize));
	}

	dev_dbg(dev, "       epmaxp_out  :");
	for (i = 0; i < 16; i++) {
		if (udev->ep_out[i])
			pr_debug(" %2u",
			    le16_to_cpu(udev->ep_out[i]->desc.wMaxPacketSize));
	}
	pr_debug("\n");

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP, "parent %p, bus %p\n", udev->parent, udev->bus);

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP, "descriptor %p, config %p, actconfig %p, "
		"rawdescriptors %p\n", &udev->descriptor, udev->config,
		udev->actconfig, udev->rawdescriptors);

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP, "have_langid %d, string_langid %d\n",
		udev->have_langid, udev->string_langid);

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP, "maxchild %d\n", udev->maxchild);
}

static void mausb_dump_request_type(__u8 rt)
{
	switch (rt & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		pr_debug("DEVICE");
		break;
	case USB_RECIP_INTERFACE:
		pr_debug("INTERF");
		break;
	case USB_RECIP_ENDPOINT:
		pr_debug("ENDPOI");
		break;
	case USB_RECIP_OTHER:
		pr_debug("OTHER ");
		break;
	default:
		pr_debug("------");
		break;
	}
}

static void mausb_dump_usb_ctrlrequest(struct usb_ctrlrequest *cmd)
{
	if (!cmd) {
		pr_debug("       : null pointer\n");
		return;
	}

	pr_debug("       ");
	pr_debug("bRequestType(%02X) bRequest(%02X) wValue(%04X) wIndex(%04X) "
		 "wLength(%04X) ", cmd->bRequestType, cmd->bRequest,
		 cmd->wValue, cmd->wIndex, cmd->wLength);
	pr_debug("\n       ");

	if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		pr_debug("STANDARD ");
		switch (cmd->bRequest) {
		case USB_REQ_GET_STATUS:
			pr_debug("GET_STATUS\n");
			break;
		case USB_REQ_CLEAR_FEATURE:
			pr_debug("CLEAR_FEAT\n");
			break;
		case USB_REQ_SET_FEATURE:
			pr_debug("SET_FEAT\n");
			break;
		case USB_REQ_SET_ADDRESS:
			pr_debug("SET_ADDRRS\n");
			break;
		case USB_REQ_GET_DESCRIPTOR:
			pr_debug("GET_DESCRI\n");
			break;
		case USB_REQ_SET_DESCRIPTOR:
			pr_debug("SET_DESCRI\n");
			break;
		case USB_REQ_GET_CONFIGURATION:
			pr_debug("GET_CONFIG\n");
			break;
		case USB_REQ_SET_CONFIGURATION:
			pr_debug("SET_CONFIG\n");
			break;
		case USB_REQ_GET_INTERFACE:
			pr_debug("GET_INTERF\n");
			break;
		case USB_REQ_SET_INTERFACE:
			pr_debug("SET_INTERF\n");
			break;
		case USB_REQ_SYNCH_FRAME:
			pr_debug("SYNC_FRAME\n");
			break;
		default:
			pr_debug("REQ(%02X)\n", cmd->bRequest);
			break;
		}
		mausb_dump_request_type(cmd->bRequestType);
	} else if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		pr_debug("CLASS\n");
	} else if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		pr_debug("VENDOR\n");
	} else if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_RESERVED) {
		pr_debug("RESERVED\n");
	}
}

void mausb_dump_urb(struct urb *urb)
{
	struct device *dev;

	if (!urb) {
		pr_debug("urb: null pointer!!\n");
		return;
	}

	if (!urb->dev) {
		pr_debug("urb->dev: null pointer!!\n");
		return;
	}

	dev = &urb->dev->dev;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"---> %s",__func__);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   urb                   :%p\n", urb);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   dev                   :%p\n", urb->dev);

	mausb_dump_usb_device(urb->dev);

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   pipe                  :%08x ", urb->pipe);

	mausb_dump_pipe(urb->pipe);

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   status                :%d\n", urb->status);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   transfer_flags        :%08X\n", urb->transfer_flags);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   transfer_buffer       :%p\n", urb->transfer_buffer);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   transfer_buffer_length:%d\n",
						urb->transfer_buffer_length);
	mausb_dump_buffer(urb->transfer_buffer, urb->transfer_buffer_length);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   actual_length         :%d\n", urb->actual_length);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   setup_packet          :%p\n", urb->setup_packet);
 
	if (urb->setup_packet && usb_pipetype(urb->pipe) == PIPE_CONTROL)
		mausb_dump_usb_ctrlrequest(
			(struct usb_ctrlrequest *)urb->setup_packet);

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   start_frame           :%d\n", urb->start_frame);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   number_of_packets     :%d\n", urb->number_of_packets);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   interval              :%d\n", urb->interval);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   error_count           :%d\n", urb->error_count);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   context               :%p\n", urb->context);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"   complete              :%p\n", urb->complete);
}
EXPORT_SYMBOL_GPL(mausb_dump_urb);

/* Receive data over TCP/IP. */
int mausb_recv(struct socket *sock, void *buf, int size)
{
	int result;
	struct msghdr msg;
	struct kvec iov;
	int total = 0;

	/* for blocks of if (mausb_dbg_flag_xmit) */
	char *bp = buf;
	int osize = size;
 
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"--->%s",__func__);

	if (!sock || !buf || !size) {
		pr_err("invalid arg, sock %p buff %p size %d\n", sock, buf,
		       size);
		return -EINVAL;
	}

	do {
		sock->sk->sk_allocation = GFP_NOIO;
		iov.iov_base    = buf;
		iov.iov_len     = size;
		msg.msg_name    = NULL;
		msg.msg_namelen = 0;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_namelen    = 0;
		msg.msg_flags      = MSG_NOSIGNAL;

		result = kernel_recvmsg(sock, &msg, &iov, 1, size, MSG_WAITALL);
		if (result <= 0) {
			pr_debug("receive sock %p buf %p size %u ret %d total %d\n",
				 sock, buf, size, result, total);
			goto err;
		}

		size -= result;
		buf += result;
		total += result;
	} while (size > 0);

	if (mausb_dbg_flag_xmit) {
		if (!in_interrupt())
			pr_debug("%-10s:", current->comm);
		else
			pr_debug("interrupt  :");

		//pr_debug("receiving....\n");
		mausb_dump_buffer(bp, osize);
		//pr_debug("received, osize %d ret %d size %d total %d\n",
		//	 osize, result, size, total);
	}
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"<---%s",__func__);
	return total;

err:
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"<---error in %s",__func__);
	return result;
}
EXPORT_SYMBOL_GPL(mausb_recv);
#if 0
struct socket *sockfd_to_socket(unsigned int sockfd)
{
	struct socket *socket;
	struct file *file;
	struct inode *inode;

	file = fget(sockfd);
	if (!file) {
		pr_err("invalid sockfd\n");
		return NULL;
	}

//	inode = file_inode(file);
	inode = file->f_dentry->d_inode;

	if (!inode || !S_ISSOCK(inode->i_mode)) {
		fput(file);
		return NULL;
	}

	socket = SOCKET_I(inode);

	return socket;
}
EXPORT_SYMBOL_GPL(sockfd_to_socket);
#endif
#if 0
/* there may be more cases to tweak the flags. */
static unsigned int tweak_transfer_flags(unsigned int flags)
{
	flags &= ~URB_NO_TRANSFER_DMA_MAP;
	return flags;
}
#endif

/* some members of urb must be substituted before. */
int mausb_recv_xbuff(struct mausb_device *ud, struct urb *urb)
{
	int ret;
	int size;

	if (ud->side == MAUSB_STUB) {
		/* the direction of urb must be OUT. */
		if (usb_pipein(urb->pipe))
			return 0;

		size = urb->transfer_buffer_length;
	} else {
		/* the direction of urb must be IN. */
		if (usb_pipeout(urb->pipe))
			return 0;

		size = urb->actual_length;
	}

	/* no need to recv xbuff */
	if (!(size > 0))
		return 0;

	ret = mausb_recv(ud->tcp_socket, urb->transfer_buffer, size);
	if (ret != size) {
		dev_err(&urb->dev->dev, "recv xbuf, %d\n", ret);
		if (ud->side == MAUSB_STUB) {
			mausb_event_add(ud, SDEV_EVENT_ERROR_TCP);
		} else {
			mausb_event_add(ud, VDEV_EVENT_ERROR_TCP);
			return -EPIPE;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mausb_recv_xbuff);

static int __init mausb_core_init(void)
{
	pr_info(DRIVER_DESC " v" MAUSB_VERSION "\n");
	return 0;
}

static void __exit mausb_core_exit(void)
{
	return;
}

module_init(mausb_core_init);
module_exit(mausb_core_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(MAUSB_VERSION);
