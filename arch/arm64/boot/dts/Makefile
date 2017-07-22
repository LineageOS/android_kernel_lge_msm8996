dts-dirs += apm
dts-dirs += arm
dts-dirs += cavium
ifneq ($(CONFIG_MACH_LGE), y)
dts-dirs += qcom
else
dts-dirs += lge
endif

subdir-y	:= $(dts-dirs)
