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
#include <linux/kthread.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "mausb_common.h"
#include "stub.h"
#include "mausb.h"
#include "mausb_util.h"

extern ktime_t timer_start;

static int is_clear_halt_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;

	req = (struct usb_ctrlrequest *) urb->setup_packet;

	 return (req->bRequest == USB_REQ_CLEAR_FEATURE) &&
		 (req->bRequestType == USB_RECIP_ENDPOINT) &&
		 (req->wValue == USB_ENDPOINT_HALT);
}

static int is_set_interface_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;

	req = (struct usb_ctrlrequest *) urb->setup_packet;

	return (req->bRequest == USB_REQ_SET_INTERFACE) &&
		(req->bRequestType == USB_RECIP_INTERFACE);
}

static int is_set_configuration_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;

	req = (struct usb_ctrlrequest *) urb->setup_packet;

	return (req->bRequest == USB_REQ_SET_CONFIGURATION) &&
		(req->bRequestType == USB_RECIP_DEVICE);
}

static int is_reset_device_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;
	__u16 value;
	__u16 index;

	req = (struct usb_ctrlrequest *) urb->setup_packet;
	value = le16_to_cpu(req->wValue);
	index = le16_to_cpu(req->wIndex);

	if ((req->bRequest == USB_REQ_SET_FEATURE) &&
	    (req->bRequestType == USB_RT_PORT) &&
	    (value == USB_PORT_FEAT_RESET)) {
		mausb_dbg_stub_rx("reset_device_cmd, port %u\n", index);
		return 1;
	} else
		return 0;
}

static int tweak_clear_halt_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;
	int target_endp;
	int target_dir;
	int target_pipe;
	int ret;

	req = (struct usb_ctrlrequest *) urb->setup_packet;

	/*
	 * The stalled endpoint is specified in the wIndex value. The endpoint
	 * of the urb is the target of this clear_halt request (i.e., control
	 * endpoint).
	 */
	target_endp = le16_to_cpu(req->wIndex) & 0x000f;

	/* the stalled endpoint direction is IN or OUT?. USB_DIR_IN is 0x80.  */
	target_dir = le16_to_cpu(req->wIndex) & 0x0080;

	if (target_dir)
		target_pipe = usb_rcvctrlpipe(urb->dev, target_endp);
	else
		target_pipe = usb_sndctrlpipe(urb->dev, target_endp);

	ret = usb_clear_halt(urb->dev, target_pipe);
	if (ret < 0)
		dev_err(&urb->dev->dev, "usb_clear_halt error: devnum %d endp "
			"%d ret %d\n", urb->dev->devnum, target_endp, ret);
	else
		dev_info(&urb->dev->dev, "usb_clear_halt done: devnum %d endp "
			 "%d\n", urb->dev->devnum, target_endp);

	return ret;
}

static int tweak_set_interface_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;
	__u16 alternate;
	__u16 interface;
	int ret;

	req = (struct usb_ctrlrequest *) urb->setup_packet;
	alternate = le16_to_cpu(req->wValue);
	interface = le16_to_cpu(req->wIndex);

	mausb_dbg_stub_rx("set_interface: inf %u alt %u\n",
			  interface, alternate);

	ret = usb_set_interface(urb->dev, interface, alternate);
	if (ret < 0)
		dev_err(&urb->dev->dev, "usb_set_interface error: inf %u alt "
			"%u ret %d\n", interface, alternate, ret);
	else
		dev_info(&urb->dev->dev, "usb_set_interface done: inf %u alt "
			 "%u\n", interface, alternate);

	return ret;
}

static int tweak_set_configuration_cmd(struct urb *urb)
{
	struct usb_ctrlrequest *req;
	__u16 config;

	req = (struct usb_ctrlrequest *) urb->setup_packet;
	config = le16_to_cpu(req->wValue);

	/*
	 * I have never seen a multi-config device. Very rare.
	 * For most devices, this will be called to choose a default
	 * configuration only once in an initialization phase.
	 *
	 * set_configuration may change a device configuration and its device
	 * drivers will be unbound and assigned for a new device configuration.
	 * This means this mausb driver will be also unbound when called, then
	 * eventually reassigned to the device as far as driver matching
	 * condition is kept.
	 *
	 * Unfortunately, an existing mausb connection will be dropped
	 * due to this driver unbinding. So, skip here.
	 * A user may need to set a special configuration value before
	 * exporting the device.
	 */
	dev_info(&urb->dev->dev, "usb_set_configuration %d to %s... skip!\n",
		 config, dev_name(&urb->dev->dev));

	return 0;
}

static int tweak_reset_device_cmd(struct urb *urb)
{
	struct stub_mausb_pal *priv = (struct stub_mausb_pal *) urb->context;
	struct stub_device *sdev = priv->sdev;

	dev_info(&urb->dev->dev, "usb_queue_reset_device\n");

	/*
	 * With the implementation of pre_reset and post_reset the driver no
	 * longer unbinds. This allows the use of synchronous reset.
	 */

	if (usb_lock_device_for_reset(sdev->udev, sdev->interface) < 0) {
		dev_err(&urb->dev->dev, "could not obtain lock to reset device\n");
		return 0;
	}
	usb_reset_device(sdev->udev);
	usb_unlock_device(sdev->udev);

	return 0;
}

/*
 * clear_halt, set_interface, and set_configuration require special tricks.
 */
int tweak_special_requests(struct urb *urb)
{
	if (!urb || !urb->setup_packet)
		return 1;

	if (usb_pipetype(urb->pipe) != PIPE_CONTROL)
		return 1;

	if (is_clear_halt_cmd(urb))
		/* tweak clear_halt */
		 return tweak_clear_halt_cmd(urb);

	else if (is_set_interface_cmd(urb))
		/* tweak set_interface */
		return tweak_set_interface_cmd(urb);

	else if (is_set_configuration_cmd(urb))
		/* tweak set_configuration */
		return tweak_set_configuration_cmd(urb);

	else if (is_reset_device_cmd(urb))
		return tweak_reset_device_cmd(urb);
	else
	{
		//mausb_dbg_stub_rx("no need to tweak\n");
	}
	return 1;
}



int get_pipe(struct stub_device *sdev, int epnum, int dir)
{
	struct usb_device *udev = sdev->udev;
	struct usb_host_endpoint *ep;
	struct usb_endpoint_descriptor *epd = NULL;

	if (dir == MAUSB_DIR_IN)
		ep = udev->ep_in[epnum & 0x7f];
	else
		ep = udev->ep_out[epnum & 0x7f];
	if (!ep) {
		dev_err(&sdev->interface->dev, "no such endpoint?, %d\n",
			epnum);
		BUG();
	}

	epd = &ep->desc;
	if (usb_endpoint_xfer_control(epd)) {
		if (dir == MAUSB_DIR_OUT)
			return usb_sndctrlpipe(udev, epnum);
		else
			return usb_rcvctrlpipe(udev, epnum);
	}

	if (usb_endpoint_xfer_bulk(epd)) {
		if (dir == MAUSB_DIR_OUT)
			return usb_sndbulkpipe(udev, epnum);
		else
			return usb_rcvbulkpipe(udev, epnum);
	}

	if (usb_endpoint_xfer_int(epd)) {
		if (dir == MAUSB_DIR_OUT)
			return usb_sndintpipe(udev, epnum);
		else
			return usb_rcvintpipe(udev, epnum);
	}

	if (usb_endpoint_xfer_isoc(epd)) {
		if (dir == MAUSB_DIR_OUT)
			return usb_sndisocpipe(udev, epnum);
		else
			return usb_rcvisocpipe(udev, epnum);
	}

	/* NOT REACHED */
	dev_err(&sdev->interface->dev, "get pipe, epnum %d\n", epnum);
	return 0;
}

void masking_bogus_flags(struct urb *urb)
{
	int				xfertype;
	struct usb_device		*dev;
	struct usb_host_endpoint	*ep;
	int				is_out;
	unsigned int	allowed;
	struct usb_ctrlrequest *setup;
//	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"---> %s\n",__func__);
	if (!urb || urb->hcpriv || !urb->complete)
		return;
	dev = urb->dev;
	if ((!dev) || (dev->state < USB_STATE_UNAUTHENTICATED))
		return;

	ep = (usb_pipein(urb->pipe) ? dev->ep_in : dev->ep_out)
		[usb_pipeendpoint(urb->pipe)];
	if (!ep)
		return;
	//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"IN 2 %s\n",__func__);
	xfertype = usb_endpoint_type(&ep->desc);
	if (xfertype == USB_ENDPOINT_XFER_CONTROL) {
			//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"IN 3 %s\n",__func__);
			setup = (struct usb_ctrlrequest *) urb->setup_packet;

		if (!setup)
			return;
		is_out = !(setup->bRequestType & USB_DIR_IN) ||
			!setup->wLength;
	} else {
			//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "IN 4 %s\n",__func__);
		is_out = usb_endpoint_dir_out(&ep->desc);
	}

	/* enforce simple/standard policy */
	allowed = (URB_NO_TRANSFER_DMA_MAP | URB_NO_INTERRUPT |
		   URB_DIR_MASK | URB_FREE_BUFFER);
	switch (xfertype) {
	case USB_ENDPOINT_XFER_BULK:
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP, "USB_ENDPOINT_XFER_BULK\n");
		if (is_out)
			allowed |= URB_ZERO_PACKET;
		/* FALLTHROUGH */
	case USB_ENDPOINT_XFER_CONTROL:
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"USB_ENDPOINT_XFER_CONTROL\n");
		allowed |= URB_NO_FSBR;	/* only affects UHCI */
		/* FALLTHROUGH */
	default:			/* all non-iso endpoints */
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"default\n");
		if (!is_out)
			allowed |= URB_SHORT_NOT_OK;
		break;
	case USB_ENDPOINT_XFER_ISOC:
//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_DUMP,"USB_ENDPOINT_XFER_ISOC\n");
		allowed |= URB_ISO_ASAP;
		break;
	}
	urb->transfer_flags &= allowed;
}

/* recv a pdu */
static void stub_rx_pdu(struct mausb_device *ud)
{
	int ret;
	struct mausb_header pdu;
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);
	unsigned long  pkt_len = 0;
	int size = sizeof(struct mausb_header);
	void *packet;
	struct stub_mausb_pal *pal;
	unsigned long flags;
	struct mausb_header *pkt;
	__u8 *pdu_data;
#ifdef MAUSB_TIMER_LOG
	ktime_t timer_end;
	s64 actual_time;
#endif
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"-------->%s",__func__);
	memset(&pdu, 0, sizeof(pdu));

	ret = mausb_recv(ud->tcp_socket, &pdu, sizeof(struct mausb_header));
#ifdef MAUSB_TIMER_LOG
	timer_start = ktime_get();
#endif
	if (ret != sizeof(pdu)) {
		LG_PRINT(DBG_LEVEL_LOW,DATA_TRANS_RX,"recv a header, %d\n", ret);
		mausb_event_add(ud, SDEV_EVENT_ERROR_TCP);
		return;
	}

	pkt_len = pdu.u.non_iso_hdr.remsize_credit;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"pkt_len: %lu",pkt_len);

//	print_hex_dump(KERN_INFO, "DEV HEADER:", DUMP_PREFIX_ADDRESS, 16, 1,
//						   &(pdu.base), sizeof(struct mausb_header), false);
	if ( (pkt_len - size) > 0 ){

		if (pdu.base.type_subtype == 0)
			return;
		if ((!mausb_get_ep_number(&pdu)) || (mausb_is_out_data_pkt(&pdu)) || (mausb_is_mgmt_pkt(&pdu))) {

			packet = kzalloc(pkt_len, GFP_ATOMIC);
			if (packet == NULL )
			{
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"kzalloc FAILED length: %lu\n",pkt_len);
				return;
			}
			memcpy(packet,&pdu,size);
			pkt_len -= size;
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"Payload length: %lu\n",pkt_len);
			pdu_data = (__u8 *)packet;

			ret = mausb_recv(ud->tcp_socket, (void *)(&(pdu_data[size])), pkt_len);
			if (ret != pkt_len) {
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "Receive Payload FAILED, %d\n", ret);
				mausb_event_add(ud, SDEV_EVENT_ERROR_TCP);
				return;
			}
			//printk(KERN_INFO "Sequence no: %d\n",pdu.u.non_iso_hdr.reqid_seqno);
			//print_hex_dump(KERN_INFO, "PAYLOAD:", DUMP_PREFIX_ADDRESS, 16, 1,
			//				&(pdu_data[size]), pkt_len, false);
#ifdef MAUSB_TIMER_LOG
			timer_end = ktime_get();
			actual_time = ktime_to_ms(ktime_sub(timer_end, timer_start));
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, " Time taken for data receive: %u  milli seconds\n",(unsigned int)(actual_time));
#endif
		}
		else {
			packet = kzalloc(size, GFP_ATOMIC);
		if (packet == NULL )
			{
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"kzalloc packet FAILED \n");
				return;
			}
			memcpy(packet,&pdu,size);
		}
	}
	else {
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"pkt_len zero else case \n");
		packet = kzalloc(size, GFP_ATOMIC);
		if (packet == NULL )
			{
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"kzalloc packet FAILED \n");
				return;
			}
		memcpy(packet,&pdu,size);
	}
	pal = kzalloc(sizeof(struct stub_mausb_pal), GFP_ATOMIC);
	if (pal == NULL )
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"kzalloc stub_mausb_pal FAILED \n");
		if(packet)
		 kfree(packet);
		return;
	}

	pal->sdev = sdev;
	pal->pdu = packet;
	pal->length = pdu.u.non_iso_hdr.remsize_credit;
	if (mausb_is_mgmt_pkt(&pdu))
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"management packet\n");
		spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
		list_add_tail(&pal->list, &sdev->mausb_pal_mgmt_init);
		spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
		//wake_up(&sdev->pal_mgmt_waitq);
		mausbdev_mgmt_process_packect(ud);
	}
	else if (mausb_is_data_pkt(&pdu))
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"data packet");

		pkt = (struct mausb_header *)pal->pdu;
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX,"pkt->u.non_iso_hdr.reqid_seqno: %d\n",pkt->u.non_iso_hdr.reqid_seqno);
		pal->seqnum = pkt->u.non_iso_hdr.reqid_seqno;

		if (mausb_is_in_data_pkt(&pdu))
		{
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "in data packet");
			spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
			list_add_tail(&pal->list, &sdev->mausb_pal_in_init);
			spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
			//wake_up(&sdev->pal_in_waitq);
			mausbdev_in_process_packect(ud);
		}
		else if (mausb_is_out_data_pkt(&pdu))
		{
			//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "out data packet");
			spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
			list_add_tail(&pal->list, &sdev->mausb_pal_out_init);
			spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
			//wake_up(&sdev->pal_out_waitq);
			mausbdev_out_process_packect(ud);
		}
		else
		{
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "wrong data");
			if(packet)
				kfree(packet);
			if(pal)
				kfree(pal);
		}
	}
	else
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "wrong data");
		if(packet)
			kfree(packet);
		if(pal)
			kfree(pal);
	}

	return;
}

int stub_rx_loop(void *data)
{
	struct mausb_device *ud = data;

	while (!kthread_should_stop()) {
		if (mausb_event_happened(ud))
			break;
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "---> Start New Packet is receiving");
		stub_rx_pdu(ud);
		//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_RX, "<-- End New Packet is receiving");
	}

	return 0;
}
