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

#include <linux/kthread.h>
#include <linux/slab.h>

#include "mausb_common.h"
#include "vhci.h"
#include "mausb.h"


static int seqnum=1;
static int mgmt_dialog =1;

static void setup_cmd_submit_pdu(struct mausb_header *pdup,  struct urb *urb)
{
	struct vhci_priv *priv = ((struct vhci_priv *)urb->hcpriv);
	struct vhci_device *vdev = priv->vdev;
	__u16 flags;

	mausb_dbg_vhci_tx("URB, local devnum %u, remote devid %u\n",
			  usb_pipedevice(urb->pipe), vdev->devid);

	pdup->base.flags_version = 0x10;
	pdup->base.type_subtype =  MAUSB_PKT_TYPE_DATA | 0x00 ;
	pdup->base.ep_devhandle = (usb_pipedevice(urb->pipe)<< 5) | (usb_pipeendpoint(urb->pipe) << 1) | ((usb_pipein(urb->pipe))>>7);
	pdup->base.ssid_devaddr = 0x00; //TODO set after device enumeration
	pdup->base.status = MAUSB_STATUS_NO_ERROR;
	pdup->u.non_iso_hdr.remsize_credit = urb->transfer_buffer_length;

	pdup->u.non_iso_hdr.reqid_seqno =  seqnum++;

	
	flags = urb->transfer_flags;
	flags &= ~URB_NO_TRANSFER_DMA_MAP;
	pdup->u.non_iso_hdr.streamid = flags;	//Need to remove flags from stream id
//	printk(KERN_INFO "\n urb->transfer_flags: %d",urb->transfer_flags);
//	printk(KERN_INFO "\n urb->interval: %d",urb->interval);

	urb->transfer_flags = flags;
}

static struct vhci_priv *dequeue_from_priv_tx(struct vhci_device *vdev)
{
	struct vhci_priv *priv, *tmp;

	spin_lock(&vdev->priv_lock);

	list_for_each_entry_safe(priv, tmp, &vdev->priv_tx, list) {
		list_move_tail(&priv->list, &vdev->priv_rx);
		spin_unlock(&vdev->priv_lock);
		return priv;
	}

	spin_unlock(&vdev->priv_lock);

	return NULL;
}

static int vhci_send_cmd_submit(struct vhci_device *vdev)
{
	struct vhci_priv *priv = NULL;

	struct msghdr msg;
	struct kvec iov[3];
	size_t txsize;

	size_t total_size = 0;

	while ((priv = dequeue_from_priv_tx(vdev)) != NULL) {
		int ret;
		struct urb *urb = priv->urb;
		struct mausb_header pdu_header;
		struct mausb_iso_packet_descriptor *iso_buffer = NULL;
		int count_iov;
		txsize = 0;
		memset(&pdu_header, 0, sizeof(pdu_header));
		memset(&msg, 0, sizeof(msg));
		memset(&iov, 0, sizeof(iov));

		mausb_dbg_vhci_tx("setup txdata urb %p\n", urb);

		/* 1. setup mausb_header */
		setup_cmd_submit_pdu(&pdu_header, urb);
		pdu_header.u.non_iso_hdr.remsize_credit = 0;
		iov[0].iov_base = &pdu_header;
		iov[0].iov_len  = sizeof(pdu_header);
		txsize += sizeof(pdu_header);

		/* 2. setup transfer buffer */
		count_iov = 1;
		if( usb_pipecontrol(urb->pipe) && (urb->setup_packet) ){
				iov[count_iov].iov_base =  urb->setup_packet;
				iov[count_iov].iov_len  = 8;
				txsize += 8;
				count_iov++;
			printk(KERN_INFO "TX:CONTROL Transfer to MA USB Device \n" );
			print_hex_dump(KERN_INFO, "SETUP URB:", DUMP_PREFIX_ADDRESS, 16, 1,
                                                   iov[count_iov].iov_base,iov[count_iov].iov_len, false);
		}
		if (!usb_pipein(urb->pipe) && urb->transfer_buffer_length > 0) {
			iov[count_iov].iov_base = urb->transfer_buffer;
			iov[count_iov].iov_len  = urb->transfer_buffer_length;
			txsize += urb->transfer_buffer_length;
			if (count_iov == 2)
				printk(KERN_INFO "TX:CONTROL + OUT Transfer to MA USB Device \n" );
			count_iov++;
			printk(KERN_INFO "TX: OUT Transfer to MA USB Device \n" );
			
			print_hex_dump(KERN_INFO, "OUT URB:", DUMP_PREFIX_ADDRESS, 16, 1,
                                                   iov[count_iov].iov_base,iov[count_iov].iov_len, false);
		}

		
		if (usb_pipein(urb->pipe) &&  !usb_pipecontrol(urb->pipe)) {
//			printk(KERN_INFO "IN Transfer to MA USB Device \n" );
			//txsize += urb->transfer_buffer_length;
			pdu_header.u.non_iso_hdr.remsize_credit += urb->transfer_buffer_length;
			printk(KERN_INFO "IN Transfer length:%d \n",urb->transfer_buffer_length);
		}

		/* 3. setup iso_packet_descriptor */
		if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
			ssize_t len = 0;

			iso_buffer = mausb_alloc_iso_desc_pdu(urb, &len);
			if (!iso_buffer) {
				mausb_event_add(&vdev->ud,
						SDEV_EVENT_ERROR_MALLOC);
				return -1;
			}

			iov[2].iov_base = iso_buffer;
			iov[2].iov_len  = len;
			txsize += len;
		}

		//pdu_header.base.length = txsize ;
		pdu_header.u.non_iso_hdr.remsize_credit += txsize;
		priv->seqnum = mausb_generate_seqnum(&pdu_header);
		pdu_header.u.non_iso_hdr.reqid_seqno = priv->seqnum;
//		printk(KERN_INFO "pdu_header.base.length :%d\n",pdu_header.base.length );
//		print_hex_dump(KERN_INFO, "MAUSB HEADER:", DUMP_PREFIX_ADDRESS, 16, 1,
//						   &(pdu_header.base), sizeof(struct mausb_header), false);


		//print_hex_dump(KERN_INFO, "MAUSB URB:", DUMP_PREFIX_ADDRESS, 16, 1,
        //                                           urb->transfer_buffer,urb->transfer_buffer_length, false);

		printk(KERN_INFO "\n TX:priv->seqnum:%ld",priv->seqnum);
		//printk(KERN_INFO "\n transfer flags: 0x%x",urb->transfer_flags);
		ret = kernel_sendmsg(vdev->ud.tcp_socket, &msg, iov, 3, txsize);
		if (ret != txsize) {
			pr_err("sendmsg failed!, ret=%d for %zd\n", ret,
			       txsize);
			kfree(iso_buffer);
			mausb_event_add(&vdev->ud, VDEV_EVENT_ERROR_TCP);
			return -1;
		}

		kfree(iso_buffer);
		printk(KERN_INFO "send txdata: %d \n",(int)txsize);

		total_size += txsize;
	}

	return total_size;
}

static struct vhci_unlink *dequeue_from_unlink_tx(struct vhci_device *vdev)
{
	struct vhci_unlink *unlink, *tmp;

	spin_lock(&vdev->priv_lock);

	list_for_each_entry_safe(unlink, tmp, &vdev->unlink_tx, list) {
		list_move_tail(&unlink->list, &vdev->unlink_rx);
		spin_unlock(&vdev->priv_lock);
		return unlink;
	}

	spin_unlock(&vdev->priv_lock);

	return NULL;
}

static int vhci_send_cmd_unlink(struct vhci_device *vdev)
{
	struct vhci_unlink *unlink = NULL;

	struct msghdr msg;
	struct kvec iov[3];
	size_t txsize;
	struct mausb_cancel_req_descriptor *cancel_req = NULL;
	struct mausb_req_resp *req;
	size_t total_size = 0;
	__u8	*tmp;
	while ((unlink = dequeue_from_unlink_tx(vdev)) != NULL) {
		int ret;
		struct mausb_header pdu_header;

		txsize = 0;
		memset(&pdu_header, 0, sizeof(pdu_header));
		memset(&msg, 0, sizeof(msg));
		memset(&iov, 0, sizeof(iov));

		mausb_dbg_vhci_tx("setup cmd unlink, %lu\n", unlink->seqnum);
		printk(KERN_INFO "CANCEL Request to MA USB Device \n" );

		req = kzalloc(sizeof(struct mausb_req_resp), GFP_KERNEL);
		req->header.base.flags_version = 0x10;
		req->header.base.type_subtype =  MAUSB_PKT_TYPE_MGMT | 0x28 ;
		req->header.base.ep_devhandle = (usb_pipedevice(unlink->priv->urb->pipe)<< 5) | (usb_pipeendpoint(unlink->priv->urb->pipe) << 1) | ((usb_pipein(unlink->priv->urb->pipe))>>7);
		req->header.base.ssid_devaddr = 0x00; //TODO set after device enumeration
		req->header.base.status = MAUSB_STATUS_NO_ERROR;
		req->header.u.mgmt_hdr.dialog= mgmt_dialog++;
//		req->header.base.length = sizeof(struct mausb_req_resp);
		pdu_header.u.non_iso_hdr.remsize_credit = sizeof(struct mausb_req_resp);
		
//		req->r.cancel_req.request_id = unlink->unlink_seqnum; //TODO Change to request ID later 
		req->r.cancel_req.resrved2 = unlink->unlink_seqnum; //TODO Change to request ID later 
		req->r.cancel_req.ep_handle = usb_pipeendpoint(unlink->priv->urb->pipe);
		
		
		iov[0].iov_base = req;
		iov[0].iov_len  = sizeof(struct mausb_header);
		txsize += sizeof(struct mausb_header);

		tmp = (__u8 *)req;
		iov[1].iov_base = &(tmp[txsize]);
		//iov[1].iov_len = req->header.base.length - txsize;
		iov[1].iov_len = req->header.u.non_iso_hdr.remsize_credit - txsize;
		txsize += iov[1].iov_len;

		printk(KERN_INFO "req->header.u.non_iso_hdr.remsize_credit :%d\n",req->header.u.non_iso_hdr.remsize_credit);
		printk(KERN_INFO "req->r.cancel_req.resrved2 : %d\n",req->r.cancel_req.resrved2 );		
		
//		print_hex_dump(KERN_INFO, "UNLINK HEADER:", DUMP_PREFIX_ADDRESS, 16, 1,
//										   iov[0].iov_base, iov[0].iov_len, false);

//		print_hex_dump(KERN_INFO, "REQ HEADER:", DUMP_PREFIX_ADDRESS, 16, 1,
//								   iov[1].iov_base, iov[1].iov_len, false);

#if 0	
		pdu_header.base.flags_version = 0x10;
		pdu_header.base.type_subtype =  MAUSB_PKT_TYPE_MGMT | 0x28 ;
		pdu_header.base.ep_devhandle = (usb_pipedevice(unlink->priv->urb->pipe)<< 5) | (usb_pipeendpoint(unlink->priv->urb->pipe) << 1) | ((usb_pipein(unlink->priv->urb->pipe))>>7);
		pdu_header.base.ssid_devaddr = 0x00; //TODO set after device enumeration
		pdu_header.base.status = MAUSB_STATUS_NO_ERROR;
		pdu_header.u.mgmt_hdr.dialog= mgmt_dialog++;


		iov[0].iov_base = &pdu_header;
		iov[0].iov_len  = sizeof(pdu_header);
		txsize += sizeof(pdu_header);

		req = kzalloc(sizeof(struct mausb_req_resp), GFP_KERNEL);
		if (!req)
			return -1;

		req->r.cancel_req.request_id = unlink->unlink_seqnum; //TODO Change to request ID later 
		req->r.cancel_req.ep_handle = usb_pipeendpoint(unlink->priv->urb->pipe);

		//cancel_req->ep_handle = usb_pipeendpoint(unlink->priv->urb->pipe);
		//cancel_req->stream_id = 0x00;  //TODO
		
		
//		cancel_req->request_id = unlink->unlink_seqnum; //TODO Change to request ID later 
		printk(KERN_INFO "req->r.cancel_req.request_id : %d\n",req->r.cancel_req.request_id );		
		iov[1].iov_base = req;
		iov[1].iov_len = sizeof(struct mausb_req_resp);
		txsize += sizeof(struct mausb_req_resp);
		pdu_header.base.length = txsize;


		print_hex_dump(KERN_INFO, "UNLINK HEADER:", DUMP_PREFIX_ADDRESS, 16, 1,
								   req, sizeof(struct mausb_req_resp), false);

		printk(KERN_INFO"\n\n\n");
		print_hex_dump(KERN_INFO, "UNLINK REQ:", DUMP_PREFIX_ADDRESS, 16, 1,
								   &(pdu_header.base), sizeof(struct mausb_header), false);

		
		printk(KERN_INFO "pdu_header.base.length :%d\n",pdu_header.base.length );
#endif
		ret = kernel_sendmsg(vdev->ud.tcp_socket, &msg, iov, 2, txsize);
		if (ret != txsize) {
			pr_err("sendmsg failed!, ret=%d for %zd\n", ret,
			       txsize);
			mausb_event_add(&vdev->ud, VDEV_EVENT_ERROR_TCP);
			return -1;
		}

		kfree(cancel_req);

		mausb_dbg_vhci_tx("send txdata\n");

		total_size += txsize;
	}

	return total_size;
}

int vhci_tx_loop(void *data)
{
	struct mausb_device *ud = data;
	struct vhci_device *vdev = container_of(ud, struct vhci_device, ud);

	while (!kthread_should_stop()) {
		if (vhci_send_cmd_submit(vdev) < 0)
			break;

		if (vhci_send_cmd_unlink(vdev) < 0) 
			break;

		wait_event_interruptible(vdev->waitq_tx,
					 (!list_empty(&vdev->priv_tx) ||
					  !list_empty(&vdev->unlink_tx) ||
					  kthread_should_stop()));

		mausb_dbg_vhci_tx("pending urbs ?, now wake up\n");
	}

	return 0;
}
