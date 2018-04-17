/*
 * mausb_main.c
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

#include <linux/module.h>
#include <linux/byteorder/generic.h>
#include <linux/kthread.h>
#include <linux/socket.h>
#include "stub.h"
#include "mausb.h"
#include "mausb_util.h"

/* Dissects a MAUSB endpoint handle */

/* dissects portions of a MA USB packet specific to Endpoint Handle Request packets */
static int handle_mausb_mgmt_pkt_ep_handle(char  *buff,
            int start, bool req, bool del)
{
    int offset = start;
    int loop_offset;
    char num_ep;
    char size_ep_des;
    int i;


    num_ep = mausb_get_uint8(buff, offset, sizeof (struct mausb_header)) & MAUSB_MGMT_NUM_EP_DES_MASK;

    if (!del) {
    } else {
    }

     if (req && !del) {

     //   size_ep_des = mausb_get_size_ep_des(buff, offset);
        offset += 1;

        /* Padding to DWORD */
        offset += 3;

    } else if (!req && !del) {
        size_ep_des = MAUSB_SIZE_MAUSB_EP_DES;
        /* Padding to DWORD */
        offset += 4;

    } else { /* If it is an EPHandleDelete Req or Resp */
        size_ep_des = MAUSB_SIZE_EP_HANDLE;
        /* Padding to DWORD */
        offset += 4; /* Padding to DWORD */

    }

    /* For every entry */
    for (i = 0; i < num_ep; ++i) {
        loop_offset = offset;

        /* If it is an EPHandleDelete Req or Resp */
        if (del) {
//loop_offset += handle_ep_handle(buff, loop_offset);

        } else if (req && !del) {

            /* Standard USB Endpoint Descriptor */
            loop_offset += USB_DT_EP_SIZE;

            /* If there are more descriptors to read */
            if (MAUSB_EP_DES_SIZE < size_ep_des) {
                /* TODO: Dissector for SS EP Companion Descriptors */
                loop_offset += USB_DT_SS_EP_COMP_SIZE;

                if (MAUSB_SS_EP_DES_SIZE < size_ep_des) {
                    /* TODO: Dissector for SSP ISO EP Companion Descriptors */
                    //loop_offset += dissect_usb_unknown_descriptor(
                      //      buff, loop_offset, &usb_trans_info, &usb_conv_info);

                    /* Pad to a DWORD */
                    loop_offset += MAUSB_ISO_SSP_EP_DES_PAD;

                } else {
                    /* Pad to a DWORD */
                    loop_offset += MAUSB_SS_EP_DES_PAD;
                }

            } else {
                /* Pad to a DWORD */
                loop_offset += MAUSB_EP_DES_PAD;
            }

        } else { /* IE: it's a EPHandleResp */
            /* EP Handle */
          //  loop_offset += handle_ep_handle(buff, loop_offset);

            /* direction */

            /* isochronous */

            /* L-managed transfers */

            /* valid handle bit */
            loop_offset += 2; /* 4 bit flags + 12 reserved bits */

            /* credit consumption unit */
            loop_offset += 2;

            loop_offset += 2; /* 2 bytes reserved */

            /* buffer size (in bytes) */
            loop_offset += 4;


            /* max iso programming delay (in uSec) */
            loop_offset += 2;

            /* max iso response delay (in uSec) */
            loop_offset += 2;
        }


        offset += size_ep_des;

        if (req && !del && loop_offset != offset){
        }

    }

return offset;

}






