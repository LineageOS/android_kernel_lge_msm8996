/*
 * mausb_mgmt.c
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
#include "mausb.h"
#include "stub.h"
#include "mausb_util.h"

static inline void mausb_send_response(struct mausb_device *ud, struct mausb_header *header)
{
//	stub_send_mausb(ud,header);
}


static void mausb_dev_handle_req(struct mausb_device *ud, struct stub_mausb_pal *pal)
{
/*
	struct mausb_header *resp;
	resp = kzalloc(sizeof(struct mausb_header), GFP_ATOMIC);
	memcpy( resp,header,sizeof(struct mausb_header));
	resp->type = USBDevHandleResp;
	resp->status = MAUSB_STATUS_NO_ERROR;
	mausb_send_response(ud, resp);
*/
}



static void mausb_dev_reset_req(struct mausb_device *ud,	struct stub_mausb_pal *pal)
{
/*
	struct mausb_header *resp;
	resp = kzalloc(sizeof(struct mausb_header), GFP_ATOMIC);
	memcpy( resp,header,sizeof(struct mausb_header));
	resp->type = MAUSBDevResetResp;
	resp->status = MAUSB_STATUS_NO_ERROR;
	mausb_send_response(ud, resp);
*/
}
#if 0
static int mausb_mgmt_handle_ep_handle(char *buff, int offset)
{
    return MAUSB_SIZE_EP_HANDLE;
}
*/

/* gets the size of the endpoint descriptors in a EPHandleReq packet */
static int mausb_mgmt_get_size_ep_des(char *buff, int offset)
{
    int size_ep_des = 0;
    int temp_buffer = 0; /* for storing the offset data */

    /* grab the 2 bytes with the size field */
    temp_buffer = mausb_get_uint16(buff, offset, sizeof (struct mausb_header)) ;

    /* mask & shift the size field */
    temp_buffer = temp_buffer & MAUSB_MGMT_SIZE_EP_DES_MASK;
    size_ep_des = (temp_buffer >> MAUSB_MGMT_SIZE_EP_DES_OFFSET);

    return size_ep_des;
}
#endif
/* be in spin_lock_irqsave(&sdev->priv_lock, flags) */
void mausb_enqueue_ret_unlink(struct stub_device *sdev,
									 	 struct stub_mausb_pal *pal,
										 __u32 seqnum,
									     __u32 status)
{
	struct mausb_req_resp *unlink;
	struct stub_mausb_pal *unlink_pal;
	//struct mausb_header *pdu = pal->pdu;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\n ---> mausb_enqueue_ret_unlink");
	unlink = kzalloc(sizeof(struct mausb_req_resp), GFP_ATOMIC);
	if (!unlink) {
		mausb_event_add(&sdev->ud, VDEV_EVENT_ERROR_MALLOC);
		return;
	}
	memcpy(unlink,pal->pdu,sizeof(struct mausb_header));

	unlink->r.cancel_resp.resrved = seqnum;	//pdu->u.non_iso_hdr.reqid_seqno;
	unlink->header.u.non_iso_hdr.reqid_seqno = seqnum;	//pdu->u.non_iso_hdr.reqid_seqno;
	unlink->header.base.status = -status;
	unlink->header.base.type_subtype = MAUSB_PKT_TYPE_MGMT | 0x29;	//It will be incremented in tx path
	unlink->header.base.length = sizeof(struct mausb_req_resp);
	unlink->r.cancel_resp.cancel_status = -status;

	unlink_pal = kzalloc(sizeof(struct stub_mausb_pal), GFP_ATOMIC);
	if(!unlink_pal)
	{
		mausb_event_add(&sdev->ud, VDEV_EVENT_ERROR_MALLOC);
		return;
	}
		
	unlink_pal->pdu = unlink;
	unlink_pal->sdev = sdev;
	unlink_pal->length = sizeof(struct mausb_req_resp);
	unlink_pal->seqnum = seqnum;

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\n unlink_pal->seqnum : %u\n",unlink_pal->seqnum);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\n unlink->r.cancel_resp.resrved : %d\n",unlink->r.cancel_resp.resrved );
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\n unlink_pal address : %p\n",unlink_pal);
	list_add_tail(&unlink_pal->list, &sdev->mausb_unlink_tx);
}

static int mausb_mgmt_handle_pkt(struct mausb_device *ud,
											struct stub_mausb_pal *pal)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);
	struct mausb_req_resp *req;
	req = (struct mausb_req_resp *)pal->pdu;

    LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\n->%s",__func__);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\nreq->header.base.type_subtype:%d",req->header.base.type_subtype);
    switch (req->header.base.type_subtype) {

    /* subtypes with variable length additional data */
    case EPHandleReq:
      //  offset = handle_mausb_mgmt_pkt_ep_handle(buff,offset, TRUE, FALSE);
    break;

    case EPHandleResp:
      //  offset = handle_mausb_mgmt_pkt_ep_handle(buff,offset, FALSE, FALSE);
    break;

    /* TODO: Dissect type-specific managment packet fields */
    case EPActivateReq:
    case EPActivateResp:
    case EPInactivateReq:
    case EPInactivateResp:
    case EPRestartReq:
    case EPRestartResp:
    case EPClearTransferReq:
    case EPClearTransferResp:
    case EPHandleDeleteReq:
       // offset = handle_mausb_mgmt_pkt_ep_handle(buff,offset, TRUE, TRUE);
    break;
    case EPHandleDeleteResp:
      //  offset = handle_mausb_mgmt_pkt_ep_handle(buff,offset, FALSE, TRUE);
    break;
    case ModifyEP0Resp:
    case EPCloseStreamResp:
    case USBDevResetReq:
    case USBDevResetResp:
    case EPOpenStreamResp:
    case VendorSpecificReq:
    case VendorSpecificResp:
        /* FALLTHROUGH */

    /* subtypes with constant length additional data */
    case CapReq:
    case CapResp:
    case USBDevHandleReq:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\nUSBDevHandleReq \n");
		mausb_dev_handle_req(ud, pal);
		break;
    case USBDevHandleResp:
    case ModifyEP0Req:
    case SetDevAddrResp:
    case UpdateDevReq:
    case MAUSBSyncReq:
    case EPCloseStreamReq:
    case CancelTransferReq:
		{
			//struct mausb_req_resp *resp;
			struct mausb_header *hdr;
			struct stub_mausb_pal *mausb_pal, *tmp;
			int ret =0;
			//int unlink = 0;
			unsigned long flags;
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT," CancelTransferReq ");
			spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
			list_for_each_entry_safe(mausb_pal, tmp, &sdev->mausb_pal_submit, list) {
				hdr = (struct mausb_header *)mausb_pal->pdu;

		/*		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT," hdr->u.non_iso_hdr.reqid_seqno:%d ",hdr->u.non_iso_hdr.reqid_seqno );
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT," req->r.cancel_req.resrved2: %d",req->r.cancel_req.resrved2);
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT, " pal->seqnum: %d\n",mausb_pal->seqnum);
		*/
				if ( hdr->u.non_iso_hdr.reqid_seqno != req->r.cancel_req.resrved2)  //ToDo comparision should be with request id
					continue;
		//		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT, " submitted pkt is going to cancel ");
				mausb_pal->unlinking = 1;
				ret = usb_unlink_urb(mausb_pal->urb);
				spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
				if (ret != -EINPROGRESS)
						LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT," failed to unlink a urb %p, ret %d",
										mausb_pal->urb, ret);
				LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"urb %p, ret %d",	mausb_pal->urb, ret);

				stub_free_mausb_pal_and_urb(pal);
				return 0;
			}

			//LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT," sequence no: %d",req->r.cancel_req.resrved2);
			mausb_enqueue_ret_unlink(sdev,pal,req->r.cancel_req.resrved2,0);

			spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
			stub_free_mausb_pal_and_urb(pal);

			wake_up(&sdev->tx_waitq);
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"wakeup tx");
			break;
    	}
    case CancelTransferResp:
		 break;
    case EPOpenStreamReq:
         break;


    /* Managment packets with no additional data */
    case MAUSBDevResetReq:
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"\nMAUSBDevResetReq \n");
		mausb_dev_reset_req(ud, pal);
		break;
    case MAUSBDevResetResp:
    case SetDevAddrReq:
    case UpdateDevResp:
    case DisconnectDevReq:
    case DisconnectDevResp:
    case MAUSBDevSleepReq:
    case MAUSBDevSleepResp:
    case MAUSBDevWakeReq:
    case MAUSBDevWakeResp:
    case MAUSBDevInitSleepReq:
    case MAUSBDevInitSleepResp:
    case MAUSBDevRemoteWakeReq:
    case MAUSBDevRemoteWakeResp:
    case PingReq:
    case PingResp:
    case MAUSBDevDisconnectReq:
    case MAUSBDevDisconnectResp:
    case MAUSBDevInitDisconReq:
    case MAUSBDevInitDisconResp:
    case MAUSBSyncResp:
	    break;

    default:
        LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"invalid management request \n");
    break;

    }


    return 0;
}



/* Code to handle packets received from MA USB host */
static inline int mausb_mgmt_dissect_pkt(struct mausb_device *ud, struct stub_mausb_pal *pal)
{
    int offset = 0;
#if 0
	unsigned char *buff;
    struct mausb_header header;

    int payload_len;
	int i;

	buff =( unsigned char *)pal->pdu;

    memset(&header, 0, sizeof(struct mausb_header));

	printk(KERN_INFO "\nIN %s\n",__func__);
	for (i=0; i<16;i++)
		printk(KERN_INFO "%d: 0x%x\n",i, buff[i]);


    /* MAUSB Protocol Version */
    header.ver_flags = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));

 	printk(KERN_INFO "header.ver_flags: %x\n",header.ver_flags);
    /* Flags */
    offset += 1;

    /* Packet Type */
    header.type = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
    offset += 1;
	printk(KERN_INFO "header.type: %x\n",header.type);
    /* Packet Length */
    header.length = mausb_get_uint16(buff, offset, sizeof(struct mausb_header));
    offset += 2;

	printk(KERN_INFO "header.length: %x\n",header.length);
    /* Is the next field a device handle or an endpoint handle */
    header.handle = mausb_get_uint16(buff, offset, sizeof(struct mausb_header));

	printk(KERN_INFO "header.handle: %x\n",header.handle);
    if (mausb_is_mgmt_pkt(&header)) {

        offset += 2;

    } else {
        offset += mausb_mgmt_handle_ep_handle(buff, offset);

    }

    /* MA Device Address */
    header.ma_dev_addr = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
    offset += 1;
	printk(KERN_INFO "header.ma_dev_addr: %x\n",header.ma_dev_addr);
    /* SSID */
    header.mass_id = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
    offset += 1;
	printk(KERN_INFO "header.mass_id: %x\n",header.mass_id);
    /* Status */
    header.status = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
    offset += 1;
	printk(KERN_INFO "header.status: %x\n",header.base.status);

	mausb_mgmt_handle_pkt(ud, pal, header.type);
#endif
#if 0
    if (mausb_is_mgmt_pkt(&header)) {

        /* Dialog Token */
		printk(KERN_INFO "management packet\n");
        header.u.token = mausb_get_uint16(buff, 9,  sizeof(struct mausb_header)) & MAUSB_TOKEN_MASK;
        offset += 1; /* token */
		printk(KERN_INFO "header.u.token: %x\n",header.u.token);
        /* Padding to a DWORD */
        offset += 2; /* DWORD*/

        /* Dissect additional management fields (when applicable) */
        if (offset <= header.length) {

            offset = handle_mausb_mgmt_pkt(ud, &header, buff, offset);
        }


    }
    else if (mausb_is_data_pkt(&header)) {
        /* TODO: Isochronous Packet Fields */
		printk(KERN_INFO "data packet\n");
        /* EPS */
        header.u.s.eps_tflags = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
        if (mausb_is_from_host(&header)) {
        	printk(KERN_INFO "mausb is form host\n");
        } else {
        	printk(KERN_INFO "mausb is form device\n");
        }


        /* T-Flags */
        offset += 1;

        /* Stream ID (non-iso) */
        header.u.s.stream_id = mausb_get_uint16(buff, offset, sizeof(struct mausb_header));
        offset += 2;
       	printk(KERN_INFO "header.u.s.stream_id: %x \n",header.u.s.stream_id );
        /* Number of Headers (iso) */
        /* I-Flags (iso) */

        /* Sequence Number */
        header.u.s.seq_num = mausb_get_uint24(buff, offset, sizeof(struct mausb_header));
        offset += 3;
       	printk(KERN_INFO "header.u.s.seq_num: %x \n",header.u.s.seq_num );

        /* Request ID */
        header.u.s.req_id = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
        offset += 1;
       	printk(KERN_INFO " header.u.s.req_id: %x\n", header.u.s.req_id);

        /* Remaining Size/Credit (non-iso) */
        header.u.s.credit = mausb_get_uint8(buff, offset, sizeof(struct mausb_header));
        offset += 4;
       	printk(KERN_INFO " header.u.s.credit: %x\n", header.u.s.credit);

        /* Presentation Time (iso) */
        /* Number of Segments (iso) */

        /*
         * TODO: dissect MA USB Payload with USB class dissectors
         *       (ex: MBIM, USB Audio, etc.)
         */

        /* Everything after the header is payload */
        payload_len = header.length - offset;
       	printk(KERN_INFO "payload_len: %d \n",payload_len );

        if (0 < payload_len) {
             offset += payload_len;
        }
    }
#endif
    return offset;

}



static struct stub_mausb_pal *dequeue_from_mausb_pal_mgmt_init(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_mausb_pal *pal, *tmp;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"---> %s\n",__func__);

	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);
	list_for_each_entry_safe(pal, tmp, &sdev->mausb_pal_mgmt_init, list) {
//		list_move_tail(&pal->list, &sdev->mausb_pal_tx);
		spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
		//list_del(&pal->list);
		return pal;
	}
	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
	return NULL;
}

void mausbdev_mgmt_process_packect(struct mausb_device *ud)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);
	struct stub_mausb_pal *pal;
	char *buff;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MGMT,"IN %s\n",__func__);
	while ((pal = dequeue_from_mausb_pal_mgmt_init(sdev)) != NULL) {
			buff = pal->pdu;
//			print_hex_dump(KERN_INFO, "IN PDU:", DUMP_PREFIX_ADDRESS, 16, 1,
//			       buff, pal->length, false);

			mausb_mgmt_handle_pkt(ud, pal);
//			mausb_mgmt_dissect_pkt(sdev,pal);
/*			kfree(pal->pdu);
			list_del(&pal->list);
*/
		}
}

#if 0
int stub_pal_mgnt_loop(void *data)
{
	struct mausb_device *ud = data;
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);

	while (!kthread_should_stop()) {

		wait_event_interruptible(sdev->pal_mgmt_waitq,
					 (!list_empty(&sdev->mausb_pal_mgmt_init) ||
					  kthread_should_stop()));
		if (mausb_event_happened(ud))
			break;
		mausbdev_mgmt_process_packect(ud);
	}

	return 0;
}
#endif

