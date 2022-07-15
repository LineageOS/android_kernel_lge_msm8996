#
# Makefile for Broadcom shared uart transport solution
#
obj-$(CONFIG_SH_V4L2)		+= brcm_hci_ldisc.o
brcm_hci_ldisc-objs := brcm_hci.o brcm_sh_ldisc.o brcm_bluesleep.o

obj-$(CONFIG_BT_V4L2)		+= brcm_bt_drv.o

obj-$(CONFIG_FM_V4L2)		+= fm_drv.o
fm_drv-objs := fmdrv_main.o fmdrv_v4l2.o fmdrv_rx.o

obj-$(CONFIG_ANT_V4L2)		+= ant_drv.o
ant_drv-objs := antdrv_main.o antdrv_v4l2.o

ccflags-y := -DVERSION="\"$(BUILD_MODULE_VERSION)\"" -DLPM_BLUESLEEP
ccflags-y += -DDEF_V4L2_FM_AUDIO_PATH=FM_AUDIO_I2S

ifneq ($(CONFIG_FM_V4L2),y)
ccflags-y += -DV4L2_FM_DEBUG=FALSE
endif
