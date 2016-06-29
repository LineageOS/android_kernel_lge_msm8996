#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.

# Touch Core Driver Common Header Code..!!
ccflags-y += -I$(srctree)/drivers/input/touchscreen/lge/

obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_S3320)      += touch_s3320.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_S3320)		 += touch_s3320_prd.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_S3320)		 += touch_s3320_fw_upgrade.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_S3320)		 += touch_s3320_rmi_dev.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_S3320)		 += DS5/

obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4302)	+= touch_td4302.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4302)	+= touch_td4302_fw_upgrade.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4302)	+= touch_td4302_rmi_dev.o

