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
#include <linux/socket.h>

#include "mausb_common.h"
#include "stub.h"
#include "mausb.h"
#include "mausb_util.h"
#ifdef MAUSB_TIMER_LOG
extern ktime_t timer_start;
#endif
extern void android_mausb_connect(int connect);

void stub_free_mausb_pal_and_urb(struct stub_mausb_pal *pal)
{
	struct urb *urb;
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"\n---> %s\n",__func__);
	if (pal)
	{
		if (pal->pdu)
			kfree(pal->pdu);
		urb = pal->urb;
		if (urb) {
			if (urb->transfer_buffer)
				kfree(urb->transfer_buffer);

			if (urb->setup_packet)
				kfree(urb->setup_packet);

			usb_free_urb(urb);
			urb = NULL;
		}
		list_del(&pal->list);
		kfree( pal);
		pal = NULL;
	}
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"<-- %s\n",__func__);
}

static struct stub_mausb_pal *dequeue_from_mausb_pal_tx(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_mausb_pal *pal, *tmp;
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"---> %s\n",__func__);

	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);

	list_for_each_entry_safe(pal, tmp, &sdev->mausb_pal_tx, list) {
		list_move_tail(&pal->list, &sdev->mausb_pal_free);
		spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
		return pal;
	}

	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);

	return NULL;
}

static int stub_send_ret_submit(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_mausb_pal *pal, *temp;
	struct msghdr msg;
	size_t txsize = 0;
	size_t total_size = 0;
#ifdef MAUSB_TIMER_LOG
	ktime_t timer_end;
	s64 actual_time;
#endif
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"---> %s\n",__func__);

	while ((pal = dequeue_from_mausb_pal_tx(sdev)) != NULL) {

		int ret;
		struct kvec *iov = NULL;
		int iovnum = 0;
		struct mausb_header *header;

		txsize = 0;
		memset(&msg, 0, sizeof(msg));
		header = (struct mausb_header *)pal->pdu;
		//if (mausb_is_in_data_pkt(header))

		iovnum = 2;

		iov = kzalloc(iovnum * sizeof(struct kvec), GFP_KERNEL);

		if (!iov) {
			mausb_event_add(&sdev->ud, SDEV_EVENT_ERROR_MALLOC);
			return -1;
		}

		iovnum = 0;
		header->base.type_subtype += 1;  	//MAUSB_PKT_TYPE_DATA | 0x01;
		header->base.length = sizeof(struct mausb_header);
		header->base.status = pal->urb->status;
		iov[iovnum].iov_base = pal->pdu;
		iov[iovnum].iov_len  = sizeof(struct mausb_header);
		iovnum++;
		txsize += sizeof(struct mausb_header);
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"\n pal->urb->status: %d",pal->urb->status);
		header->u.non_iso_hdr.reqid_seqno = pal->seqnum;
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"header->u.non_iso_hdr.reqid_seqno: %d",header->u.non_iso_hdr.reqid_seqno);

		if (usb_pipein(pal->urb->pipe) &&
		    usb_pipetype(pal->urb->pipe) != PIPE_ISOCHRONOUS &&
		    pal->urb->actual_length > 0) {
			iov[iovnum].iov_base = pal->urb->transfer_buffer;
			iov[iovnum].iov_len  = pal->urb->actual_length;
			iovnum++;
			txsize += pal->urb->actual_length;
			header->base.length += pal->urb->actual_length;
		}

		header->u.non_iso_hdr.remsize_credit = sizeof(struct mausb_header) + pal->urb->actual_length; //SONU added for length issues on host.

		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"send msg..txsize..%zu\n",txsize);
		ret = kernel_sendmsg(sdev->ud.tcp_socket, &msg,
								iov,  iovnum, txsize);
		if (ret != txsize) {
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"sendmsg failed!, retval %d for %zd\n",	ret, txsize);
			kfree(iov);
			mausb_event_add(&sdev->ud, SDEV_EVENT_ERROR_TCP);
			return -1;
		}
		else if (ret == txsize)
		{
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"sendmsg success, retval %d for %zd\n",
				ret, txsize);
		}
		else
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,				"sendmsg failed!, retval %d for %zd\n",
				ret, txsize);

		kfree(iov);
		if (pal->seqnum == 1)
		{
			android_mausb_connect(1);	//connect
		}

	}
	total_size += txsize;

	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
	list_for_each_entry_safe(pal, temp, &sdev->mausb_pal_free, list) {
		stub_free_mausb_pal_and_urb(pal);
	}
	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
#ifdef MAUSB_TIMER_LOG
	timer_end = ktime_get();
	actual_time = ktime_to_ms(ktime_sub(timer_end, timer_start));

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "Time taken for transaction: %u  milli seconds\n",(unsigned int)(actual_time));
#endif
	return total_size;
}

static struct stub_mausb_pal *dequeue_from_mausb_unlink_tx(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_mausb_pal *unlink, *tmp;
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"---> %s\n",__func__);
	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);

	list_for_each_entry_safe(unlink, tmp, &sdev->mausb_unlink_tx, list) {
		list_move_tail(&unlink->list, &sdev->mausb_unlink_free);
		spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
		return unlink;
	}

	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"<--Unlink tx is empty\n");
	return NULL;
}

static int stub_send_ret_unlink(struct stub_device *sdev)
{
	unsigned long flags;
//	struct stub_unlink *unlink;
	struct stub_mausb_pal *mausb_unlink, *mausb_tmp;
	struct msghdr msg;
	struct kvec iov[2];
	size_t txsize;

	size_t total_size = 0;

	while ((mausb_unlink = dequeue_from_mausb_unlink_tx(sdev)) != NULL) {
		int ret;
		//struct mausb_header header;
		__u8	*pdu = mausb_unlink->pdu;
		struct mausb_header *hdr= mausb_unlink->pdu;
		txsize = 0;
		//memset(&header, 0, sizeof(header));
		memset(&msg, 0, sizeof(msg));
		memset(&iov, 0, sizeof(iov));

		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"---> %s\n",__func__);
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"\n unlink_pal address : %p\n",mausb_unlink);
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"hdr->u.non_iso_hdr.reqid_seqno %u\n", hdr->u.non_iso_hdr.reqid_seqno);
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"mausb_unlink->seqnum %u\n", mausb_unlink->seqnum);

		iov[0].iov_base = pdu;
		iov[0].iov_len  = sizeof(struct mausb_header);
		txsize += sizeof(struct mausb_header);

		hdr->base.length = sizeof(struct mausb_header);	//Need to remove it
		 hdr->u.non_iso_hdr.remsize_credit = sizeof(struct mausb_header);
		ret = kernel_sendmsg(sdev->ud.tcp_socket, &msg, iov,
				     1, txsize);
		if (ret != txsize) {
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"sendmsg failed!, retval %d for %zd\n",
				ret, txsize);
			mausb_event_add(&sdev->ud, SDEV_EVENT_ERROR_TCP);
			return -1;
		}

		mausb_dbg_stub_tx("send txdata\n");
		total_size += txsize;
	}

	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);

	list_for_each_entry_safe(mausb_unlink, mausb_tmp, &sdev->mausb_unlink_free, list) {
		list_del(&mausb_unlink->list);
		kfree( mausb_unlink->pdu);
		kfree(mausb_unlink);
	}

	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);

	return total_size;
}

int stub_tx_loop(void *data)
{
	struct mausb_device *ud = data;
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);

	while (!kthread_should_stop()) {
		if (mausb_event_happened(ud))
			break;

		/*
		 * send_ret_submit comes earlier than send_ret_unlink.  stub_rx
		 * looks at only priv_init queue. If the completion of a URB is
		 * earlier than the receive of CMD_UNLINK, priv is moved to
		 * priv_tx queue and stub_rx does not find the target priv. In
		 * this case, vhci_rx receives the result of the submit request
		 * and then receives the result of the unlink request. The
		 * result of the submit is given back to the usbcore as the
		 * completion of the unlink request. The request of the
		 * unlink is ignored. This is ok because a driver who calls
		 * usb_unlink_urb() understands the unlink was too late by
		 * getting the status of the given-backed URB which has the
		 * status of usb_submit_urb().
		 */
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"---> Start Transfer a packet\n");
		if (stub_send_ret_submit(sdev) < 0)
			break;

		if (stub_send_ret_unlink(sdev) < 0)
			break;
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"<------- End Transfer a packet\n");
		wait_event_interruptible(sdev->tx_waitq,
					 (!list_empty(&sdev->mausb_unlink_tx) ||
					  !list_empty(&sdev->mausb_pal_tx) ||
					  kthread_should_stop()));
	}
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_TX,"<-- Exit stub_tx_loop\n");
	return 0;
}
