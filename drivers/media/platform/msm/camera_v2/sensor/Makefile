ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
ifdef CONFIG_MACH_LGE
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/tcs
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/ois
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/proxy
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/iris
obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/ eeprom/ ois/ proxy/ flash/ iris/
obj-$(CONFIG_LG_TCS) += tcs/
else
obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/ eeprom/ ois/ flash/ ir_led/ ir_cut/
endif
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor_init.o msm_sensor_driver.o msm_sensor.o