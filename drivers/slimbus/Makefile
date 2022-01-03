#
# Makefile for kernel slimbus framework.
#
ccflags-$(CONFIG_DEBUG_SLIMBUS)	+= -DDEBUG

obj-$(CONFIG_SLIMBUS)			+= slimbus.o
obj-$(CONFIG_SLIMBUS_MSM_CTRL)		+= slim-msm.o slim-msm-ctrl.o
obj-$(CONFIG_SLIMBUS_MSM_NGD)		+= slim-msm.o slim-msm-ngd.o
