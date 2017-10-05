ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
obj-$(CONFIG_MSMB_CAMERA) += msm_ois.o
obj-$(CONFIG_LG_OIS) += msm_ois_i2c.o
ifneq ($(CONFIG_MACH_MSM8996_LUCYE),y)
obj-$(CONFIG_LG_OIS) += lgit_imx234_onsemi_ois.o lgit_imx234_rohm_ois.o OIS_Init_IMT.o lgit_imx298_rohm_ois.o lgit_s5k2p7_rohm_ois.o
else
obj-$(CONFIG_LG_OIS) += lgit_imx258_rohm_ois.o lgit_imx258_claf_rohm_ois.o imt_imx258_rohm_ois.o
endif
