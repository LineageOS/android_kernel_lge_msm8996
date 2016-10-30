/*
 *  HID driver for LG Electronics Friends devices.
 *
 *  Copyright (c) 2016 LG Electronics Inc.
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/power_supply.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/idr.h>
#include <linux/input/mt.h>

#include "hid-ids.h"

#define LGE_FRIENDS_KEY_SUPPORT			BIT(1)
#define LGE_FRIENDS_LED_SUPPORT			BIT(2)
#define LGE_FRIENDS_BATTERY_SUPPORT		BIT(3)
#define LGE_FRIENDS_FLASH_SUPPORT		BIT(4)

#define LGE_FRIENDS_DEVICE_CFC		(LGE_FRIENDS_KEY_SUPPORT | LGE_FRIENDS_LED_SUPPORT | LGE_FRIENDS_BATTERY_SUPPORT)

static enum power_supply_property lge_friends_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_STATUS,
};

struct lge_friends_data {
	struct hid_device *hdev;
	unsigned long quirks;
	struct led_classdev led;
	struct led_classdev flash;
	struct power_supply battery;
	__u8 battery_charging;
	__u8 battery_capacity;
	__u8 cable_state;
};

static void lge_friends_parse_report(struct lge_friends_data *lfd, __u8 *rd, int size)
{
	struct hid_device *hdev = lfd->hdev;

	hid_info(hdev, "%s\n", __func__);
	return;
}

static int lge_friends_raw_event(struct hid_device *hdev, struct hid_report *report,
		__u8 *rd, int size)
{
	struct lge_friends_data *lfd = hid_get_drvdata(hdev);

	hid_info(hdev, "%s\n", __func__);
	lge_friends_parse_report(lfd, rd, size);
	return 0;
}

static int lge_friends_mapping(struct hid_device *hdev, struct hid_input *hi,
			struct hid_field *field, struct hid_usage *usage,
			unsigned long **bit, int *max)
{
	hid_info(hdev, "%s - hid_usage : 0x%x\n", __func__, usage->hid);
	return 0;
}

static int lge_friends_input_configured(struct hid_device *hdev,
					struct hid_input *hidinput)
{
	hid_info(hdev, "%s\n", __func__);
	return 0;
}

static void lge_friends_led_brightness_set(struct led_classdev *led,
				    enum led_brightness value)
{
	return;
}

static void lge_friends_flash_brightness_set(struct led_classdev *led,
				    enum led_brightness value)
{
	return;
}

static void lge_friends_leds_remove(struct lge_friends_data *lfd)
{
	led_classdev_unregister(&lfd->led);
	led_classdev_unregister(&lfd->flash);
}

static int lge_friends_leds_init(struct lge_friends_data *lfd)
{
	struct hid_device *hdev = lfd->hdev;

	hid_info(hdev, "%s\n", __func__);
	lfd->led.name = "friends:led";
	lfd->led.brightness_set = lge_friends_led_brightness_set;
	led_classdev_register(&hdev->dev, &lfd->led);

	lfd->flash.name = "friends:flash";
	lfd->flash.brightness_set = lge_friends_flash_brightness_set;
	led_classdev_register(&hdev->dev, &lfd->flash);
	return 0;
}

static int lge_friends_battery_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	struct lge_friends_data *lfd = container_of(psy, struct lge_friends_data, battery);
	struct hid_device *hdev = lfd->hdev;
	int ret = 0;
	u8 battery_charging, battery_capacity, cable_state;

	hid_info(hdev, "%s - psp : %d\n", __func__, psp);
	battery_charging = lfd->battery_charging;
	battery_capacity = lfd->battery_capacity;
	cable_state = lfd->cable_state;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_DEVICE;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = battery_capacity;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (battery_charging)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			if (battery_capacity == 100 && cable_state)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int lge_friends_battery_probe(struct lge_friends_data *lfd)
{
	struct hid_device *hdev = lfd->hdev;
	int ret;

	hid_info(hdev, "%s\n", __func__);
	/*
	 * Set the default battery level to 100% to avoid low battery warnings
	 * if the battery is polled before the first device report is received.
	 */
	lfd->battery_capacity = 100;

	lfd->battery.properties = lge_friends_battery_props;
	lfd->battery.num_properties = ARRAY_SIZE(lge_friends_battery_props);
	lfd->battery.get_property = lge_friends_battery_get_property;
	lfd->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	lfd->battery.name = kasprintf(GFP_KERNEL, "hid-lge_friends_battery");
	if (!lfd->battery.name)
		return -ENOMEM;

	ret = power_supply_register(&hdev->dev, &lfd->battery);
	if (ret) {
		hid_err(hdev, "Unable to register battery device\n");
		goto err_free;
	}

	power_supply_powers(&lfd->battery, &hdev->dev);
	return 0;

err_free:
	kfree(lfd->battery.name);
	lfd->battery.name = NULL;
	return ret;
}

static void lge_friends_battery_remove(struct lge_friends_data *lfd)
{
	struct hid_device *hdev = lfd->hdev;

	hid_info(hdev, "%s\n", __func__);
	if (!lfd->battery.name)
		return;

	power_supply_unregister(&lfd->battery);
	kfree(lfd->battery.name);
	lfd->battery.name = NULL;
}

static int lge_friends_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	unsigned long quirks = id->driver_data;
	struct lge_friends_data *lfd;
	unsigned int connect_mask = HID_CONNECT_DEFAULT;

	hid_info(hdev, "%s\n", __func__);
	lfd = devm_kzalloc(&hdev->dev, sizeof(*lfd), GFP_KERNEL);
	if (lfd == NULL) {
		hid_err(hdev, "can't alloc lge_friends data\n");
		return -ENOMEM;
	}

	lfd->quirks = quirks;
	hid_set_drvdata(hdev, lfd);
	lfd->hdev = hdev;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	if (lfd->quirks & LGE_FRIENDS_DEVICE_CFC)
		connect_mask |= HID_CONNECT_HIDDEV_FORCE;

	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	if (lfd->quirks & LGE_FRIENDS_LED_SUPPORT) {
		ret = lge_friends_leds_init(lfd);
		if (ret < 0)
			goto err_stop;
	}

	if (lfd->quirks & LGE_FRIENDS_BATTERY_SUPPORT) {
		ret = lge_friends_battery_probe(lfd);
		if (ret < 0)
			goto err_stop;

		/* Open the device to receive reports with battery info */
		ret = hid_hw_open(hdev);
		if (ret < 0) {
			hid_err(hdev, "hw open failed\n");
			goto err_stop;
		}
	}

	return 0;
err_stop:
	if (lfd->quirks & LGE_FRIENDS_LED_SUPPORT)
		lge_friends_leds_remove(lfd);
	if (lfd->quirks & LGE_FRIENDS_BATTERY_SUPPORT)
		lge_friends_battery_remove(lfd);
	hid_hw_stop(hdev);
err_free:
	kfree(lfd);
	return ret;
}

static void lge_friends_remove(struct hid_device *hdev)
{
	struct lge_friends_data *lfd = hid_get_drvdata(hdev);

	hid_info(hdev, "%s\n", __func__);
	if (lfd->quirks & LGE_FRIENDS_LED_SUPPORT)
		lge_friends_leds_remove(lfd);

	if (lfd->quirks & LGE_FRIENDS_BATTERY_SUPPORT) {
		hid_hw_close(hdev);
		lge_friends_battery_remove(lfd);
	}

	hid_hw_stop(hdev);
}

static const struct hid_device_id lge_friends_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_LGE, USB_DEVICE_ID_LGE_FRIENDS_CFC),
		.driver_data = LGE_FRIENDS_DEVICE_CFC },
	{ }
};
MODULE_DEVICE_TABLE(hid, lge_friends_devices);

static struct hid_driver lge_friends_driver = {
	.name             = "lge_friends",
	.id_table         = lge_friends_devices,
	.input_mapping    = lge_friends_mapping,
	.input_configured = lge_friends_input_configured,
	.probe            = lge_friends_probe,
	.remove           = lge_friends_remove,
	.raw_event        = lge_friends_raw_event
};

static int __init lge_friends_init(void)
{
	dbg_hid("LGE Friends:%s\n", __func__);

	return hid_register_driver(&lge_friends_driver);
}

static void __exit lge_friends_exit(void)
{
	dbg_hid("LGE Friends:%s\n", __func__);

	hid_unregister_driver(&lge_friends_driver);
}
module_init(lge_friends_init);
module_exit(lge_friends_exit);

MODULE_LICENSE("GPL");
