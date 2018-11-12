ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
# BabyBear and current implementation below
#obj-$(CONFIG_LG_PROXY)   += msm_proxy.o msm_proxy_i2c.o vl6180x_api.o vl6180x_i2c.o vl6180x_LG_G3_i2c.o

#Ewok iimplementation below
obj-$(CONFIG_LG_PROXY)   += msm_proxy.o msm_proxy_i2c.o vl53l0_api.o vl53l010_api.o vl53l0_i2c.o vl53l0_api_calibration.o vl53l0_api_core.o vl53l0_api_histogram.o vl53l0_api_ranging.o vl53l0_api_strings.o vl53l0_LG_G3_i2c.o vl53l010_tuning.o