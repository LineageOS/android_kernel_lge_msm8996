/*
 * mausb_util.c
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
#include "mausb_util.h"

/*** Packet parsing helper functions ***/

int mausb_is_from_host(struct mausb_header *header)
{
    return (MAUSB_FLAG_HOST << MAUSB_FLAG_OFFSET) & header->base.flags_version;
}

int mausb_is_mgmt_pkt(struct mausb_header *header)
{
    return MAUSB_PKT_TYPE_MGMT == (header->base.type_subtype & MAUSB_PKT_TYPE_MASK);
}

int mausb_is_data_pkt(struct mausb_header *header)
{
    return MAUSB_PKT_TYPE_DATA == (header->base.type_subtype & MAUSB_PKT_TYPE_MASK);
}

int mausb_is_in_data_pkt(struct mausb_header *header)
{
    return (header->base.ep_devhandle & 0x0001);
}
int mausb_is_out_data_pkt(struct mausb_header *header)
{
    return !(header->base.ep_devhandle & 0x0001);
}

unsigned short int mausb_get_ep_number(struct mausb_header *header)
{
	return ((header->base.ep_devhandle & 0x001E) >> 1);
}

unsigned short int mausb_get_usb_device_address(struct mausb_header *header)
{
	return ((header->base.ep_devhandle & 0x0FE0) >> 5);
}

unsigned short int mausb_get_usb_bus_number(struct mausb_header *header)
{
	return ((header->base.ep_devhandle & 0xF000) >> 12);
}

/*unsigned short int mausb_get_transfer_type(struct mausb_header *header)
{
	return (header->);
}
*/
unsigned char mausb_get_request_id(struct mausb_header *header)
{
	return (header->u.non_iso_hdr.reqid_seqno >> 24);
}

unsigned char mausb_get_sequence_number(struct mausb_header *header)
{
	return (header->u.non_iso_hdr.reqid_seqno & 0x00FFFFFF);
}

unsigned char mausb_get_uint8(unsigned char *buff, const int offset, const int length)
{


	if (offset < 0 || !buff) {
		return 0;
	}

	if (offset < length) {
		return *(buff + offset);
	}

	return 0;
}

unsigned short mausb_get_uint16(unsigned char *buff, const int offset, const int length)
{
	unsigned short *ptr;

	if (offset < 0 || !buff) {
		return 0;
	}

	if (offset >= length) {
		return 0;
	}
	ptr = (unsigned short *)(buff + offset);

	return *ptr;
}


unsigned int mausb_get_uint24(unsigned char *buff, const int offset, const int length)
{
	unsigned int *ptr;
	unsigned int val;

	if (offset < 0 || !buff) {
		return 0;
	}

	if (offset >= length) {
		return 0;
	}
	ptr = (unsigned int *)(buff + offset);
	val = *ptr;
	val = val & 0x0FFF;

	return val;
}


