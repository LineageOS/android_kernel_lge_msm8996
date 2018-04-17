/*
 * laf.c -- Composite driver support
 *
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 Nokia Corporation
 * Author: David Brownell
 * Modified: Klaus Schwarzkopf <schwarzkopf@sensortherm.de>
 *
 * Heavily based on multi.c and cdc2.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>


#include "f_laf_2.c"


#define DRIVER_DESC			"MAUSB Gadget LAF"
#define DRIVER_VERSION		"2016/05/12"

/*-------------------------------------------------------------------------*/

/*
 * DO NOT REUSE THESE IDs with a protocol-incompatible driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */
#define LAF_VENDOR_NUM	0x1004	//0x1d6b	/* LGE */
#define LAF_PRODUCT_NUM	0x6340	//0x0106	/* Composite Gadget:LAF*/

/*-------------------------------------------------------------------------*/
struct laf_data {
	bool opened;
	bool enabled;
};
static struct laf_data * plaf_data;

/*-------------------------------------------------------------------------*/
USB_GADGET_COMPOSITE_OPTIONS();

static struct usb_device_descriptor device_desc = {
	.bLength 			=		sizeof device_desc,
	.bDescriptorType 	=		USB_DT_DEVICE,
	.bcdUSB 			=		cpu_to_le16(0x0200),
	.bDeviceClass 		=		USB_CLASS_COMM,
	.bDeviceSubClass 	=		0,
	.bDeviceProtocol 	=		0,
	.idVendor 			=		cpu_to_le16(LAF_VENDOR_NUM),
	.idProduct 			=		cpu_to_le16(LAF_PRODUCT_NUM),
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength 			=		sizeof otg_descriptor,
	.bDescriptorType 	=		USB_DT_OTG,
	.bmAttributes 		=		USB_OTG_SRP | USB_OTG_HNP,
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};


/* string IDs are assigned dynamically */



static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "",
	[USB_GADGET_PRODUCT_IDX].s = DRIVER_DESC,
	[USB_GADGET_SERIAL_IDX].s = "",
	{  } /* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

/****************************** Configurations ******************************/

/*
 * LAF functions.
 */

static int __init laf_do_config(struct usb_configuration *c)
{
	int	status;
	printk(KERN_INFO "%s \n",__func__);
	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	plaf_data = kzalloc(sizeof(struct laf_data), GFP_KERNEL);
	if (!plaf_data)
	{
		printk(KERN_INFO " %s ENOMEM \n",__func__);
		return -ENOMEM;
	}
	
	laf_setup();	
	
	status = laf_bind_config(c);
	if (status < 0)
	{
		printk(KERN_INFO " %s laf_bind_config failed \n",__func__);
		return status;
	}
	
	return 0;
}


static struct usb_configuration laf_config_driver = {
	.label			= DRIVER_DESC,
	.bConfigurationValue	= 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

/*-------------------------------------------------------------------------*/

static int __init laf_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget	*gadget = cdev->gadget;
	int			status;
	printk(KERN_INFO "%s \n",__func__);



	/*
	 * Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */

	/* device descriptor strings: manufacturer, product */

	status = usb_string_ids_tab(cdev, strings_dev);
	if (status < 0)
		goto fail_string_ids;
	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = 		strings_dev[USB_GADGET_PRODUCT_IDX].id;


	//LAF
	
	/* register our configuration */
	status = usb_add_config(cdev, &laf_config_driver,laf_do_config);
	if (status < 0)
		goto fail_string_ids;
	
	usb_composite_overwrite_options(cdev, &coverwrite);
	
	dev_info(&gadget->dev, "%s, version: " DRIVER_VERSION "\n",DRIVER_DESC);
	printk(KERN_INFO "%s \n",__func__);
	return 0;

	/* error recovery */
fail_string_ids:
	if(plaf_data)
		kfree(plaf_data);
	return status;
}

static int __exit laf_unbind(struct usb_composite_dev *cdev)
{
	printk(KERN_INFO "%s \n",__func__);
	laf_cleanup();
	if(plaf_data)
	kfree(plaf_data);
	return 0;
}

static __refdata struct usb_composite_driver laf_driver = {
	.name		= "laf_mausb",
	.dev		= &device_desc,
	.max_speed	= USB_SPEED_HIGH,
	.strings	= dev_strings,
	.bind		= laf_bind,
	.unbind		= __exit_p(laf_unbind),
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Klaus Schwarzkopf <schwarzkopf@sensortherm.de>");
MODULE_LICENSE("GPL v2");

static void laf_ready_callback(void)
{
	printk(KERN_INFO "%s\n",__func__);
	plaf_data->opened = true;
	plaf_data->enabled =true;
}

static void laf_closed_callback(void)
{
	printk(KERN_INFO "%s\n",__func__);
	plaf_data->opened = false;
	plaf_data->enabled =false;
}

static int __init laf_mod_init(void)
{
	printk(KERN_INFO "%s\n",__func__);
	return usb_composite_probe(&laf_driver);
}
module_init(laf_mod_init);

static void __exit laf_mod_cleanup(void)
{
	printk(KERN_INFO "%s\n",__func__);
	usb_composite_unregister(&laf_driver);
}
module_exit(laf_mod_cleanup);

