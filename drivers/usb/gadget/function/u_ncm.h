/*
 * u_ncm.h
 *
 * Utility definitions for the ncm function
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef U_NCM_H
#define U_NCM_H

#include <linux/usb/composite.h>

struct f_ncm_opts {
	struct usb_function_instance	func_inst;
	struct net_device		*net;
	bool				bound;

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};

#ifdef CONFIG_LGE_USB_G_NCM
int ncm_init(void);
void ncm_cleanup(void);
int ncm_ctrlrequest(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl);
int get_ncm_start_requested(void);
#endif

#endif /* U_NCM_H */
