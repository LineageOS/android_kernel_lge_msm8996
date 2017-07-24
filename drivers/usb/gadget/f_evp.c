/*
 * Gadget Function Driver for Android USB EVP charger
 *
 * Copyright (C) 2014 MAXIM INTEGRATED.
 * Author: Clark Kim <clark.kim@maximintegrated.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef CONFIG_LGE_USB_MAXIM_EVP
/* verbose messages */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/types.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>

#undef dev_dbg
#define dev_dbg dev_info

#define USB_REQ_REVERVED 	0x02

#define DRIVER_VENDOR_ID	0x0B6A /* MAXIM */
#define DRIVER_PRODUCT_ID	0x003A /* undefined */

#define EVP_MAX_PACKET_SIZE	8

#define STRING_MANUFACTURER		1
#define STRING_PRODUCT			2
#define STRING_SERIALNUM		0

/*-------------------------------------------------------------------------*/
// EVP define
#define NO_COMMAND					0
#define GET_CONSUMER_COMMAND		1
#define GET_CONSUMER_PARAMETERS		2
#define SEND_PROVIDER_CAPABILITIES	3
#define SEND_PROVIDER_STATUS		4
#define SEND_PROVIDER_ERROR			5

// Request byte
enum {
	NO_REQUEST,
	SEND_PROVIDER_STATUS_PACKET,
	SEND_PROVIDER_ERROR_PACKET,
	SEND_CAPABILITIES_PACKET,
	GET_NEW_DEVICE_PARAMETERS,
	RESERVED,
};

// Timing byte
enum {
	POLL_TIME_20MS,
	POLL_TIME_100MS,
	POLL_TIME_500MS,
	POLL_TIME_1S,
	POLL_TIME_5S,
	POLL_TIME_10S,
	POLL_TIME_30S,
	POLL_TIME_1MIN,
};

#define EVP_DYNAMIC_BIT				BIT(2)
#define EVP_SUSPEND_BIT				BIT(3)

#define EVP_DYNAMIC_MODE			4
#define EVP_SIMPLE_MODE				0
#define EVP_SIMPLE_SUSPEND_MODE		8

#define EVP_DEFAULT_MAX_VOLTAGE 	9000
#define EVP_DEFAULT_MIN_VOLTAGE 	5000
#define EVP_DEFAULT_NEW_VOLTAGE 	8300
#define EVP_DEFAULT_FEATURES		EVP_SIMPLE_SUSPEND_MODE
#define EVP_DEFAULT_POLLING_TIME	POLL_TIME_1MIN

#define EVP_VOLTAGE(x) ((x >= 20000) ? 360 : (x-2000)*20/1000)

/*-------------------------------------------------------------------------*/

struct evp_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	/* synchronize access to our device file */
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req[RX_REQ_MAX];
	int rx_done;
	int bootmode;
};

/* temporary variable used between evp_open() and evp_gadget_bind() */
static struct evp_dev *_evp_dev;


static struct usb_device_descriptor evp_device_desc = {
	.bLength              = sizeof(evp_device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(DRIVER_VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(DRIVER_PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

/*-------------------------------------------------------------------------*/

/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX	0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer[255];
static char product[256];
static char serial[256];

static struct usb_string strings_evp[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer,
	[STRING_PRODUCT_IDX].s = product,
	[STRING_SERIAL_IDX].s = serial,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_evp = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_evp,
};

/*-------------------------------------------------------------------------*/
static struct usb_interface_descriptor evp_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = USB_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol     = 0,
};

static struct usb_endpoint_descriptor evp_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor evp_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_evp_descs[] = {
	(struct usb_descriptor_header *) &evp_interface_desc,
	(struct usb_descriptor_header *) &evp_fullspeed_in_desc,
	(struct usb_descriptor_header *) &evp_fullspeed_out_desc,
	NULL,
};

static struct usb_string evp_string_defs[] = {
	[INTERFACE_STRING_INDEX].s	= "Android EVP Interface",
	{  },	/* end of list */
};

static struct usb_gadget_strings evp_string_table = {
	.language		= 0x0409,	/* en-US */
	.strings		= evp_string_defs,
};

static struct usb_gadget_strings *evp_strings[] = {
	&evp_string_table,
	NULL,
};


/*-------------------------------------------------------------------------*/
static ushort evp_max_voltage = 0;
module_param(evp_max_voltage, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(evp_max_voltage, "EVP maximum voltage(mV) in the String descriptor");

static ushort evp_min_voltage = 0;
module_param(evp_min_voltage, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(evp_min_voltage, "EVP minimum voltage(mV) in the String descriptor");

static ushort evp_features = 5;
module_param(evp_features, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(evp_features, "EVP featrues in the String descriptor");

static ushort evp_new_voltage = 0;
module_param(evp_new_voltage, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(evp_new_voltage, "EVP new voltage in the dynamic mode");

static ushort evp_polling_time = 0;
module_param(evp_polling_time, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(evp_polling_time, "EVP consumer command polling time");

/*
 * 0bit : coming USB_REQ_GET_DESCRIPTOR & DT_DEVICE
 * 1bit : coming USB_REQ_GET_DESCRIPTOR & DT_STRING
 * 2bit : coming USB_REQ_GET_DESCRIPTOR & DT_CONFIG
 * 3bit : w_length is not 255 when DT_CONFIG (i.e. host is OSX)
 * 0011 is real evp.
 */
static int real_evp;

/*-------------------------------------------------------------------------*/
// EVP command //
static int get_evp_command(const struct usb_ctrlrequest *ctrl);
int evp_get_product_string(void);


/*-------------------------------------------------------------------------*/
static int check_real_evp(bool clear)
{
	if (clear)
		real_evp = 0;
	else if (real_evp == (BIT(0) | BIT(1))) {
		pr_debug("%s : yes, this is real evp \n", __func__);
		return 1;
	}
	pr_debug("%s : no, this isn't real evp \n", __func__);
	return 0;
}

static int evp_new_voltage_io(int *value, bool io)
{
	if (io)
		evp_new_voltage = *(ushort *)value;/*write*/
	else
		*(ushort *)value = evp_new_voltage;/*read*/
	return 0;
}

static int evp_mode_check(void)
{
	if (evp_features & EVP_DYNAMIC_BIT)
		return evp_features;
	else
		return 0;
}

static int evp_setting_voltage_read(void)
{
	if (evp_features & EVP_DYNAMIC_BIT)
		return (int)evp_new_voltage;
	else
		return (int)evp_max_voltage;
}

static inline struct evp_dev *evp_func_to_dev(struct usb_function *f)
{
	return container_of(f, struct evp_dev, function);
}

/* remove a request from the head of a list */
static struct usb_request *evp_req_get(struct evp_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void evp_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static void evp_setup_complete(struct usb_ep *ep,
				struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		printk(KERN_INFO "evp setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);;
}


static int evp_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	struct usb_request *req = cdev->req;
	int	value = -EOPNOTSUPP;
	u8 b_requestType = ctrl->bRequestType;
	u8 b_request = ctrl->bRequest;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);
	u8 data[EVP_MAX_PACKET_SIZE];
	int command = NO_COMMAND;
	int ret = 0;

	pr_debug("%s : %02x.%02x v%04x i%04x l%u\n",
			__func__, b_requestType, b_request,
			w_value, w_index, w_length);

	/* partial re-init of the response message; the function or the
	 * gadget might need to intercept e.g. a control-OUT completion
	 * when we delegate to it.
	 */
	req->zero = 0;
	req->complete = evp_setup_complete;
	req->length = 0;
	cdev->gadget->ep0->driver_data = cdev;

	if (b_requestType == USB_DIR_IN) {
		// setup device //
		if (b_request == USB_REQ_GET_DESCRIPTOR) {
			switch (w_value>>8) {
			case USB_DT_DEVICE:
				pr_debug("maxim, setup: desc device\n");
				real_evp |= BIT(0);
				evp_device_desc.bMaxPacketSize0 = cdev->gadget->ep0->maxpacket;
				value = min(w_length, (u16) sizeof evp_device_desc);
				memcpy(req->buf, &evp_device_desc, value);
				break;
			case USB_DT_STRING:
				pr_debug("maxim, setup: desc string\n");
				real_evp |= BIT(1);
				if (w_length != 0xFF)
					real_evp |= BIT(3);
				ret = evp_get_product_string();
				if (ret != 0)
					strlcpy(product, "Maxim EVP    gr8rFiV8US06403C00",
						sizeof(product) - 1);	// default - Simple mode : max 7V, min 5V

				value = usb_gadget_get_string(&stringtab_evp,
						w_value & 0xff, req->buf);
				if (value >= 0)
					value = min(w_length, (u16) value);
				break;
			case USB_DT_CONFIG:
				real_evp |= BIT(2);
				break;
			default:
				goto done;
			}
		} else {
			goto done;
		}
	} else if (b_requestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {
		// get command //
		command = get_evp_command(ctrl);

		if (command == GET_CONSUMER_COMMAND) {
			// get wValue and wLength
			pr_debug("%s: maxim, GET_CONSUMER_COMMAND, wVaule=0x%x, wLength=0x%x !! \n", __func__, ctrl->wValue, ctrl->wLength);
			// set Data Stage, total 4bytes
			value = 4;
			// byte 0 is the request byte
			data[0] = GET_NEW_DEVICE_PARAMETERS;
			// byte 1 is the timing byte
			data[1] = evp_polling_time;
			// byte 2, 3 are 0
			data[2] = 0;
			data[3] = 0;

			memcpy(req->buf, data, w_length);
		} else if (command == GET_CONSUMER_PARAMETERS) {
			// get wValue
			pr_debug("%s: maxim, GET_CONSUMER_PARAMETERS, wVaule=0x%x, wLength=0x%x !! \n", __func__, ctrl->wValue, ctrl->wLength);

			// set Data Stage, total 4bytes
			value = 4;
			// byte 0, 1 are the new voltage.
			data[0] = (EVP_VOLTAGE(evp_new_voltage) & 0xFF00)>>8;
			data[1] = EVP_VOLTAGE(evp_new_voltage) & 0xFF;

			pr_debug("%s: maxim, GET_CONSUMER_PARAMETERS, new_voltage, data[0]=0x%x, data[1]=0x%x !! \n", __func__, data[0], data[1]);

			// byte 2, 3 are 0
			data[2] = 0;
			data[3] = 0;

			memcpy(req->buf, data, w_length);
		} else {
			goto done;
		}
		value = min(w_length, (u16) value);
	} else if (b_requestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {
//		cdev->req->complete = dev_complete_send_provider;
	} else {
		goto done;
	}

	if (value >= 0) {
		req->zero = value < w_length;
		req->length = value;
		req->complete = evp_setup_complete;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			pr_info("%s setup response queue error, ep_queue-->%d\n",
				__func__, value);
			req->status = 0;
			evp_setup_complete(cdev->gadget->ep0, req);
		}
	}

done:
	pr_debug("evp_ctrlrequest: done %x v %x\n", b_request, w_value);
	return value;
}

static int
evp_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	pr_info("%s: \n", __func__);

	return 0;
}

static void
evp_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct evp_dev	*dev = evp_func_to_dev(f);
	struct usb_request *req;
	int i;

	pr_info("%s: \n", __func__);

	while ((req = evp_req_get(dev, &dev->tx_idle)))
		evp_request_free(req, dev->ep_in);
	for (i = 0; i < RX_REQ_MAX; i++)
		evp_request_free(dev->rx_req[i], dev->ep_out);
}

static void evp_function_disable(struct usb_function *f)
{
	struct evp_dev	*dev = evp_func_to_dev(f);

	pr_info("%s: \n", __func__);

	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);
}

static int evp_bind_config(struct usb_configuration *c)
{
	struct evp_dev *dev = _evp_dev;

	printk(KERN_INFO "evp_bind_config\n");
	pr_info("%s: \n", __func__);

	dev->cdev = c->cdev;
	dev->function.name = "evp";
	dev->function.strings = evp_strings,
	dev->function.fs_descriptors = fs_evp_descs;
	dev->function.bind = evp_function_bind;
	dev->function.unbind = evp_function_unbind;
	dev->function.disable = evp_function_disable;

	return usb_add_function(c, &dev->function);
}

int evp_get_product_string(void)
{
	int ret = 0;

	char evp_product[256];

	char string_maxV[50];
	char string_minV[50];
	char string_features[50];

	u16 maxV = 0;
	u16 minV = 0;
	u8 features = 0;

	if (_evp_dev->bootmode == LGE_BOOT_MODE_CHARGERLOGO)
		evp_features = EVP_SIMPLE_SUSPEND_MODE;
	else if (evp_features == 5)
		evp_features = EVP_DEFAULT_FEATURES;

	if (!(evp_features & EVP_DYNAMIC_BIT))
		evp_max_voltage = EVP_DEFAULT_NEW_VOLTAGE;
	else if (evp_max_voltage == 0)
		evp_max_voltage = EVP_DEFAULT_MAX_VOLTAGE;

	if (evp_min_voltage == 0)
		evp_min_voltage = EVP_DEFAULT_MIN_VOLTAGE;


	if (evp_new_voltage == 0)
		evp_new_voltage = EVP_DEFAULT_NEW_VOLTAGE;

	if (evp_polling_time == 0)
		evp_polling_time = EVP_DEFAULT_POLLING_TIME;

	pr_debug("%s: evp voltage, maxV=%d, minV=%d, feature=%d\n",
		__func__, evp_max_voltage, evp_min_voltage, evp_features);

	maxV = EVP_VOLTAGE(evp_max_voltage);
	minV = EVP_VOLTAGE(evp_min_voltage);
	features = evp_features;

	pr_debug("%s: evp convert voltage, maxV=%x, minV=%x, feature=%x\n",
		__func__, maxV, minV, features);

	sprintf(string_maxV,"%03X", maxV);
	sprintf(string_minV,"%03X", minV);
	sprintf(string_features,"%02X", features);

	pr_info("%s: evp convert string, maxV=%s, minV=%s, feature=%s\n",
		__func__, string_maxV, string_minV, string_features);

	strlcpy(evp_product, "Maxim EVP    gr8rFiV8US", sizeof(evp_product) - 1);
	pr_debug("%s: evp_product = %s\n", __func__, evp_product);
	strcat(evp_product, string_maxV);
	pr_debug("%s: evp_product(max) = %s\n", __func__, evp_product);
	strcat(evp_product, string_minV);
	pr_debug("%s: evp_product(min) = %s\n", __func__, evp_product);
	strcat(evp_product, string_features);
	pr_debug("%s: evp_product(fea) = %s\n", __func__, evp_product);

	strlcpy(product, evp_product, sizeof(product) -1);
	pr_debug("%s: product = %s\n", __func__, product);
	pr_info("%s: strings_evp = %s\n", __func__, strings_evp[STRING_PRODUCT_IDX].s);

	ret = strcmp(product, strings_evp[STRING_PRODUCT_IDX].s);

	return ret;
}

static int evp_setup(void)
{
	struct evp_dev *dev;
	int ret = 0;

	pr_info("%s: \n", __func__);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* _evp_dev must be set before calling usb_gadget_register_driver */
	_evp_dev = dev;

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	_evp_dev->bootmode = lge_get_boot_mode();
	/* maunfacture string */
	strings_evp[STRING_MANUFACTURER_IDX].id = STRING_MANUFACTURER;
	evp_device_desc.iManufacturer = STRING_MANUFACTURER;
	strlcpy(manufacturer, "Maxim Integrated", sizeof(manufacturer) - 1);

	/* product string */
	strings_evp[STRING_PRODUCT_IDX].id = STRING_PRODUCT;
	evp_device_desc.iProduct = STRING_PRODUCT;
	ret = evp_get_product_string();
	if (ret != 0)
		strlcpy(product, "Maxim EVP    gr8rFiV8US06403C00",
			sizeof(product) - 1);	// default - Simple mode : max 7V, min 5V

	/* serial string - don't care */
	strings_evp[STRING_SERIAL_IDX].id = STRING_SERIALNUM;
	evp_device_desc.iSerialNumber = STRING_SERIALNUM;
	strlcpy(serial, "1", sizeof(serial) - 1);	// don't need it

	return 0;
}

static void evp_cleanup(void)
{
	kfree(_evp_dev);
	_evp_dev = NULL;
	pr_info("%s: \n", __func__);
}


/*-------------------------------------------------------------------------*/
static int get_evp_command(const struct usb_ctrlrequest *ctrl)
{
	int command_type = NO_COMMAND;

	// get wValue and wLength
	pr_debug("%s: bRequestType=0x%x, bRequest=0x%x, wVaule=0x%x, wLength=0x%x !! \n",
		__func__, ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wLength);

	if (ctrl->bRequestType & 0x80) {
		if (ctrl->bRequest == USB_REQ_CLEAR_FEATURE)
			command_type = GET_CONSUMER_COMMAND;
		else if (ctrl->bRequest == 0x02)
			command_type = GET_CONSUMER_PARAMETERS;
		else
			command_type = NO_COMMAND;
	}

	return command_type;
}

/*-------------------------------------------------------------------------*/
#endif

MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_DESCRIPTION("MAX14675 EVP USB gadget driver");
MODULE_LICENSE("GPL");
