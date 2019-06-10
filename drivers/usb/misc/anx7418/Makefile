# defines
# FIRMWARE_PROFILE : test for firmware wrting time
# PD_DEBUG : test for pd send timeout
# PD_CTS_TEST : for PD CTS
# CABLE_DET_PIN_HAS_GLITCH : prevent cable_det glitch
ccflags-y := -DCABLE_DET_PIN_HAS_GLITCH

obj-$(CONFIG_LGE_USB_ANX7418) := anx7418.o
obj-$(CONFIG_LGE_USB_ANX7418) += anx7418_i2c.o
obj-$(CONFIG_LGE_USB_ANX7418) += anx7418_pd.o
obj-$(CONFIG_LGE_USB_ANX7418) += anx7418_charger.o
obj-$(CONFIG_LGE_USB_ANX7418) += anx7418_firmware.o
obj-$(CONFIG_LGE_USB_ANX7418) += anx7418_debugfs.o
obj-$(CONFIG_LGE_USB_ANX7418) += anx7418_sysfs.o
