/*
 * mausb_out.c
 *
 * Copyright (C) 2015-2016 LGE Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/kthread.h>
#include <linux/slab.h>
#include "mausb.h"
#include "stub.h"
#include "mausb_util.h"

#ifdef MAUSB_TIMER_LOG
ktime_t out_timer_start;
#endif
extern void stub_free_mausb_pal_and_urb(struct stub_mausb_pal *pal);

static struct stub_mausb_pal *dequeue_from_mausb_pal_out_init(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_mausb_pal *pal, *tmp;
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"---> %s\n",__func__);

	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
	list_for_each_entry_safe(pal, tmp, &sdev->mausb_pal_out_init, list) {
		list_move_tail(&pal->list, &sdev->mausb_pal_submit);
		spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT," pal->seqnum: %d\n",pal->seqnum);
		return pal;
	}
	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
	return NULL;
}


static void mausbdev_out_stub_complete(struct urb *urb)
{
	struct stub_mausb_pal *pal = (struct stub_mausb_pal *) urb->context;
	struct stub_device *sdev = pal->sdev;
	unsigned long flags;
#ifdef MAUSB_TIMER_LOG
	ktime_t out_timer_end;
	s64 actual_time;
#endif
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "---> %s\n",__func__);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "complete! status %d\n", urb->status);
#ifdef MAUSB_TIMER_LOG
	out_timer_end = ktime_get();
	actual_time = ktime_to_ms(ktime_sub(out_timer_end, out_timer_start));
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "OUT: Time taken for out packet processing: %u  milli seconds\n",(unsigned int)(actual_time));
#endif
	switch (urb->status) {
	case 0:
		/* OK */
		break;
	case -ENOENT:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"stopped by a call to usb_kill_urb() "
			 "because of cleaning up a virtual connection\n");
		return;
	case -ECONNRESET:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "unlinked by a call to "
			 "usb_unlink_urb()\n");
		break;
	case -EPIPE:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "endpoint %d is stalled\n",
			 usb_pipeendpoint(urb->pipe));
		break;
	case -ESHUTDOWN:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "device removed?\n");
		break;
	default:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "urb completion with non-zero status "
			 "%d\n", urb->status);
		break;
	}


//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"\n OUT urb->transfer_buffer_length: %d \n urb->actual_length: %d",
//							urb->transfer_buffer_length ,
//							urb->actual_length);

	pal->urb->actual_length = urb->actual_length;

	/* link a urb to the queue of tx. */
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"pal->seqnum:%d",pal->seqnum);
	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
	if (pal->unlinking) {
		mausb_enqueue_ret_unlink(sdev,pal,pal->seqnum,urb->status);
	}
	else {
		list_move_tail(&pal->list, &sdev->mausb_pal_tx);
	}

//	list_add_tail(&pal->list, &sdev->mausb_pal_tx);

	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "\npal & urb freee\n");
	if (pal->unlinking) {
		stub_free_mausb_pal_and_urb(pal);
	}

	/* wake up tx_thread */
	wake_up(&sdev->tx_waitq);
}

static void mausbdev_out_submit_urb(struct stub_device *sdev,
				 struct stub_mausb_pal *pal)
{
	int ret;
	struct mausb_header header;
	struct mausb_device *ud = &sdev->ud;
	struct usb_device *udev = sdev->udev;
	int pipe, ep_number, dir;
	int ep_num;
	struct setup_packet *spacket = NULL;
	void *ptr = NULL;
	__u8	*pdu_data;
	unsigned long flags;
#ifdef MAUSB_TIMER_LOG
//	ktime_t timer_end;
//	s64 actual_time;
#endif
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "---> %s\n",__func__);
	memcpy(&header,pal->pdu,sizeof(struct mausb_header));
	ep_number = mausb_get_ep_number(&header);
	dir = mausb_is_in_data_pkt(&header);
	pipe = get_pipe(sdev, ep_number, dir);

	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "pipe: %d ep_number: %d  dir=%d\n",pipe,ep_number,dir);
	/* setup a urb */
	if (usb_pipeisoc(pipe))
	{
//		priv->urb = usb_alloc_urb(pdu->u.cmd_submit.number_of_packets, GFP_KERNEL);
	}
	else
	{
		pal->urb = usb_alloc_urb(0, GFP_KERNEL);
	}
	if (!pal->urb) {
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "malloc urb\n");
		mausb_event_add(ud, SDEV_EVENT_ERROR_MALLOC);
		return;
	}


	/* set other members from the base header of pdu */
	pal->urb->context                = (void *) pal;
	pal->urb->dev                    = udev;
	pal->urb->pipe                   = pipe;
	pal->urb->complete               = mausbdev_out_stub_complete;

	pdu_data = (__u8 *)(pal->pdu);
	ep_num = mausb_get_ep_number(&header);
	if (ep_num == USB_ENDPOINT_XFER_CONTROL)
	{
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "Control transfer\n");
		ptr = (void *)(&(pdu_data[MAUSB_NON_ISOCHRONUS_HEADER_LEN]));
		pal->urb->setup_packet = kmemdup(ptr, 8,  GFP_KERNEL);
		spacket = (struct setup_packet *)(&(pdu_data[MAUSB_NON_ISOCHRONUS_HEADER_LEN]));
		if (!pal->urb->setup_packet) {
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "Failed to allocate memory for setup_packet\n");
			mausb_event_add(ud, SDEV_EVENT_ERROR_MALLOC);
			return;
		}
		if ( pal->length > (MAUSB_NON_ISOCHRONUS_HEADER_LEN + 8))
		{
//			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "Control transfer more than 8 Bytes \n");
			pal->urb->transfer_buffer = kmemdup((void *)(&(pdu_data[MAUSB_NON_ISOCHRONUS_HEADER_LEN + 8])),
												pal->length - (MAUSB_NON_ISOCHRONUS_HEADER_LEN + 8),
												GFP_KERNEL);
			pal->urb->transfer_buffer_length  = pal->length - (MAUSB_NON_ISOCHRONUS_HEADER_LEN + 8);
		}

		/* no need to submit an intercepted request, but harmless? */
		if (!tweak_special_requests(pal->urb))
		{
			dev_info(&pal->urb->dev->dev, "No need to submit this request... skip!\n");
			spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
			list_move_tail(&pal->list, &sdev->mausb_pal_tx);
			spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
			wake_up(&sdev->tx_waitq);
			return;
		}

	}
	else if (!usb_pipeisoc(pipe))
	{
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"OUT 222 %s\n",__func__);
		pal->urb->transfer_buffer = kmemdup((void *)(&(pdu_data[MAUSB_NON_ISOCHRONUS_HEADER_LEN])),
											header.u.non_iso_hdr.remsize_credit - MAUSB_NON_ISOCHRONUS_HEADER_LEN,
											GFP_KERNEL);
		pal->urb->transfer_buffer_length  = header.u.non_iso_hdr.remsize_credit - MAUSB_NON_ISOCHRONUS_HEADER_LEN;

	}
	else
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"Isochronus %s\n",__func__);
	}

	pal->urb->transfer_flags = 00;
	pal->urb->interval = 2048;

	masking_bogus_flags(pal->urb);

	/* urb is inow ready to submit */
	// mausb_dump_urb(pal->urb);
	ret = usb_submit_urb(pal->urb, GFP_KERNEL);

	if (ret == 0){
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"submit urb ok. ");
#ifdef MAUSB_TIMER_LOG
//		timer_end = ktime_get();
//		actual_time = ktime_to_ms(ktime_sub(timer_end, timer_start));
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "OUT: Time taken for urb submission: %u  milli seconds\n",(unsigned int)(actual_time));
#endif
		}
	else {
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "submit_urb error, %d", ret);
	//	mausb_dump_header(pal->pdu);
		mausb_dump_urb(pal->urb);

		/*
		 * Pessimistic.
		 * This connection will be discarded.
		 */
		mausb_event_add(ud, SDEV_EVENT_ERROR_SUBMIT);
	}

	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "<-- %s\n",__func__);
	return;
}


void mausbdev_out_process_packect(struct mausb_device *ud)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);
	struct stub_mausb_pal *pal;
	char *buff;
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT, "---> %s\n",__func__);
	while ((pal = dequeue_from_mausb_pal_out_init(sdev)) != NULL) {
			buff = pal->pdu;
			//print_hex_dump(KERN_INFO, "OUT PDU:", DUMP_PREFIX_ADDRESS, 16, 1,
			  //     buff, pal->length, false);
#ifdef MAUSB_TIMER_LOG
			out_timer_start = ktime_get();
#endif
			mausbdev_out_submit_urb(sdev,pal);
		}
}


#if 0
int stub_pal_out_loop(void *data)
{
	struct mausb_device *ud = data;
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);

	while (!kthread_should_stop()) {

		wait_event_interruptible(sdev->pal_out_waitq,
					 (!list_empty(&sdev->mausb_pal_out_init) ||
					  kthread_should_stop()));
		if (mausb_event_happened(ud))
			break;
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"---> Process OUT Packet");
		mausbdev_out_process_packect(ud);
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_OUT,"<-- Process OUT Packet");
	}

	return 0;
}

#endif
