insmod ../../usb/gadget/udc-core.ko
insmod ../../../fs/configfs/configfs.ko
insmod ../../usb/gadget/libcomposite.ko
insmod ../../usb/gadget/dummy_hcd.ko
#insmod ../../usb/gadget/g_zero.ko loopdefault=1
#insmod ../../usb/gadget/g_mass_storage.ko file=/dev/sda7
insmod usbip-core.ko
insmod usbip-host.ko
insmod ../../usb/mon/usbmon.ko
