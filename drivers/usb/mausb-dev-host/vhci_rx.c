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

/* get URB from transmitted urb queue. caller must hold vdev->priv_lock */
struct urb *pickup_urb_and_free_priv(struct vhci_device *vdev, __u32 seqnum)
{
	struct vhci_priv *priv, *tmp;
	struct urb *urb = NULL;
	int status;

	list_for_each_entry_safe(priv, tmp, &vdev->priv_rx, list) {
		if (priv->seqnum != seqnum)
			continue;

		urb = priv->urb;
		status = urb->status;

		mausb_dbg_vhci_rx("find urb %p vurb %p seqnum %u\n",
				urb, priv, seqnum);

		switch (status) {
		case -ENOENT:
			/* fall through */
		case -ECONNRESET:
			dev_info(&urb->dev->dev,
				 "urb %p was unlinked %ssynchronuously.\n", urb,
				 status == -ENOENT ? "" : "a");
			break;
		case -EINPROGRESS:
			/* no info output */
			break;
		default:
			dev_info(&urb->dev->dev,
				 "urb %p may be in a error, status %d\n", urb,
				 status);
		}

		list_del(&priv->list);
		kfree(priv);
		urb->hcpriv = NULL;

		break;
	}

	return urb;
}

__u32 mausb_generate_seqnum(struct mausb_header *pdu)
{
	return ( pdu->u.non_iso_hdr.reqid_seqno); //TODO how do we manage MGMT packets ? SONU
	//return ( pdu->base.ep_devhandle | pdu->u.non_iso_hdr.reqid_seqno); //TODO how do we manage MGMT packets ? SONU
}

static void vhci_recv_ret_submit(struct vhci_device *vdev,
				 struct mausb_header *pdu)
{
	struct mausb_device *ud = &vdev->ud;
	struct urb *urb;
	__u32 seqnum;
	__s32 status;
	//struct mausb_header_non_iso *non_iso_hdr;
	int transfer_length;

	printk(KERN_INFO"--> %s \n",__FUNCTION__);
	spin_lock(&vdev->priv_lock);
	seqnum = mausb_generate_seqnum(pdu);
	urb = pickup_urb_and_free_priv(vdev, seqnum);
	spin_unlock(&vdev->priv_lock);
	printk(KERN_INFO "\n RX:seqnum: %d",seqnum);
	if (!urb) {
		pr_err("cannot find a urb of seqnum %u\n", seqnum);
		pr_info("max seqnum %d\n",
			atomic_read(&the_controller->seqnum));
		mausb_event_add(ud, VDEV_EVENT_ERROR_TCP);
		return;
	}

	/* unpack the pdu to a urb */
	//non_iso_hdr = pdu->u.non_iso_hdr;
	//transfer_length = urb->transfer_length-non_iso_hdr->remsize_credit;
//	transfer_length = pdu->base.length - sizeof(struct mausb_header);

	transfer_length = pdu->u.non_iso_hdr.remsize_credit - sizeof(struct mausb_header);
	status = pdu->base.status;

	if (status == TRANSFER_SHORT_TRANSFER) {
		if (urb->transfer_flags & URB_SHORT_NOT_OK)
		{
			urb->status = -EREMOTEIO;
			printk(KERN_INFO "\n Remote IO Error");
		}
		else
		{
			urb->status = 0;
			printk(KERN_INFO "\n Short Packet flag is not set");
		}
	}		
	else if (status == TRANSFER_EP_STALL)
	{
		urb->status = -EPIPE;	
		printk(KERN_INFO "\n Broken Pipe");
	}	
	else
		urb->status = 0;

	//urb->status		= -status;
	//printk(KERN_INFO "*** urb->status: 0x%x\n",urb->status);
	urb->actual_length	= transfer_length;

	//printk(KERN_INFO"## Setting  URB actual length to:%d ,transfer_length:%d,pdu.base.length:%d , header length:%d \n", urb->actual_length,transfer_length,pdu->base.length,sizeof(struct mausb_header));

	
	/* recv transfer buffer */
	if (mausb_recv_xbuff(ud, urb) < 0)
		return;

	if(!(pdu->base.ep_devhandle & 0x1))
	{
		urb->actual_length = urb->transfer_buffer_length;
	}
	
	/* recv iso_packet_descriptor */
	if (mausb_recv_iso(ud, urb) < 0)
		return;

	/* restore the padding in iso packets */
	mausb_pad_iso(ud, urb);

	if (mausb_dbg_flag_vhci_rx)
		mausb_dump_urb(urb);

	//mausb_dbg_vhci_rx("now giveback urb %p\n", urb);

	spin_lock(&the_controller->lock);
	usb_hcd_unlink_urb_from_ep(vhci_to_hcd(the_controller), urb);
	spin_unlock(&the_controller->lock);

	usb_hcd_giveback_urb(vhci_to_hcd(the_controller), urb, urb->status);

	//mausb_dbg_vhci_rx("Leave\n");

	return;
}

static struct vhci_unlink *dequeue_pending_unlink(struct vhci_device *vdev,
						  struct mausb_header *pdu)
{
	struct vhci_unlink *unlink, *tmp;
//	struct mausb_req_resp *unlink_resp = (struct mausb_req_resp *)pdu;
	long seqnum;

	spin_lock(&vdev->priv_lock);
	seqnum = pdu->u.non_iso_hdr.reqid_seqno;		//mausb_generate_seqnum(pdu);
//	seqnum = unlink_resp->r.cancel_resp.resrved;

	list_for_each_entry_safe(unlink, tmp, &vdev->unlink_rx, list) {
		printk(KERN_INFO "unlink->seqnum %lu\n", unlink->seqnum);
		printk(KERN_INFO "unlink->unlink_seqnum %lu\n", unlink->unlink_seqnum);
		printk(KERN_INFO "seqnum %lu\n", seqnum);
		if (unlink->unlink_seqnum == seqnum) {
			mausb_dbg_vhci_rx("found pending unlink, %lu\n",
					  unlink->seqnum);
			list_del(&unlink->list);

			spin_unlock(&vdev->priv_lock);
			return unlink;
		}
	}

	spin_unlock(&vdev->priv_lock);

	return NULL;
}

static void vhci_recv_ret_unlink(struct vhci_device *vdev,
				 struct mausb_header *pdu)
{
	struct vhci_unlink *unlink;
	struct urb *urb;
	__s32	status;
//	mausb_dump_mausb_header(pdu);

	unlink = dequeue_pending_unlink(vdev, pdu);
	if (!unlink) {
		pr_info("cannot find the pending unlink %u\n",
	 		mausb_generate_seqnum(pdu));
		return;
	}

	spin_lock(&vdev->priv_lock);
	urb = pickup_urb_and_free_priv(vdev, unlink->unlink_seqnum);
	spin_unlock(&vdev->priv_lock);

	if (!urb) {
		/*
		 * I get the result of a unlink request. But, it seems that I
		 * already received the result of its submit result and gave
		 * back the URB.
		 */
		pr_info("the urb (seqnum %d) was already given back\n",
				mausb_generate_seqnum(pdu));
	} else {
		mausb_dbg_vhci_rx("now giveback urb %p\n", urb);

		/* If unlink is successful, status is -ECONNRESET */
	//	urb->status = pdu->u.ret_unlink.status; //TODO
		status = pdu->base.status;
		urb->status		= -status;
		printk(KERN_INFO"****urb->status %x\n", urb->status);

		spin_lock(&the_controller->lock);
		usb_hcd_unlink_urb_from_ep(vhci_to_hcd(the_controller), urb);
		spin_unlock(&the_controller->lock);

		usb_hcd_giveback_urb(vhci_to_hcd(the_controller), urb,
				     urb->status);
	}

	kfree(unlink);
}

static int vhci_priv_tx_empty(struct vhci_device *vdev)
{
	int empty = 0;

	spin_lock(&vdev->priv_lock);
	empty = list_empty(&vdev->priv_rx);
	spin_unlock(&vdev->priv_lock);

	return empty;
}

/* recv a pdu */
static void vhci_rx_pdu(struct mausb_device *ud)
{
	int ret;
	struct mausb_header pdu;
	struct vhci_device *vdev = container_of(ud, struct vhci_device, ud);

	printk(KERN_INFO "\n--->vhci_rx_pdu\n");

	memset(&pdu, 0, sizeof(pdu));

	/* receive a pdu header */
	ret = mausb_recv(ud->tcp_socket, &pdu, sizeof(pdu));
	if (ret < 0) {
		if (ret == -ECONNRESET)
			pr_info("connection reset by peer\n");
		else if (ret == -EAGAIN) {
			/* ignore if connection was idle */
			if (vhci_priv_tx_empty(vdev))
				return;
			pr_info("connection timed out with pending urbs\n");
		} else if (ret != -ERESTARTSYS)
			pr_info("xmit failed %d\n", ret);

		mausb_event_add(ud, VDEV_EVENT_ERROR_TCP);
		return;
	}
	if (ret == 0) {
		pr_info("connection closed");
		mausb_event_add(ud, VDEV_EVENT_DOWN);
		return;
	}
	if (ret != sizeof(pdu)) {
		pr_err("received pdu size is %d, should be %d\n", ret,
		       (unsigned int)sizeof(pdu));
		mausb_event_add(ud, VDEV_EVENT_ERROR_TCP);
		return;
	}

//	if (mausb_dbg_flag_vhci_rx)
//		mausb_dump_mausb_header(&pdu);

	//print_hex_dump(KERN_INFO, "HOST RX:", DUMP_PREFIX_ADDRESS, 16, 1,
	//		       &pdu, sizeof(struct mausb_header), false);
	switch (pdu.base.type_subtype >> 6) { //TODO should be shifted by 6 bits ...whats going wrong ??
	case 2://MAUSB_PKT_TYPE_DATA: //TODO switch based on Response or Ack
		vhci_recv_ret_submit(vdev, &pdu);
		break;
	case 0://MAUSB_PKT_TYPE_MGMT:
		//print_hex_dump(KERN_INFO, "UNLINK RX:", DUMP_PREFIX_ADDRESS, 16, 1,
		//	       &pdu, sizeof(struct mausb_header), false);
		printk(KERN_INFO"\n MAUSB_PKT_TYPE_MGMT\n");
		switch(pdu.base.type_subtype & 0x3f){
		case 0x29:
			vhci_recv_ret_unlink(vdev, &pdu);
			break;
		default:
			break;
		}
		break;
//	case MAUSB_PKT_TYPE_CTRL:
		//vhci_recv_ret_ctrl(vde,&pdu); //TODO
//		break;
		
	default:
		/* NOT REACHED */
		pr_err("unknown pdu type %u\n", pdu.base.type_subtype >> 6);
//		mausb_dump_mausb_header(&pdu);
		mausb_event_add(ud, VDEV_EVENT_ERROR_TCP);
		break;
	}
}

int vhci_rx_loop(void *data)
{
	struct mausb_device *ud = data;

	while (!kthread_should_stop()) {
		if (mausb_event_happened(ud))
			break;

		vhci_rx_pdu(ud);
	}

	return 0;
}
