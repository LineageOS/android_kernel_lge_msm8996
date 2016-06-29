ifeq ($(CONFIG_MACH_MSM8996_H1_KR),y)
DTS_TARGET := msm8996-h1_kr
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_VZW),y)
DTS_TARGET := msm8996-h1_vzw
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_LRA_US),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_USC_US),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_SPR_US),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_ACG_US),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_ATT_US),y)
DTS_TARGET := msm8996-h1_att
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_TMO_US),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_GLOBAL_COM),y)
DTS_TARGET := msm8996-h1_global
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_CNO_CN),y)
DTS_TARGET := msm8996-h1_global
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_KDDI_JP),y)
DTS_TARGET := msm8996-h1_kddi
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_CA),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_H1_CLR_PR),y)
DTS_TARGET := msm8996-h1_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_KR),y)
DTS_TARGET := msm8996-elsa_kr
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_VZW),y)
DTS_TARGET := msm8996-elsa_vzw
endif

ALL_DTS := $(notdir $(wildcard echo $(srctree)/$(src)/$(DTS_TARGET)/*.dts))
dtb-y += $(patsubst %.dts,%.dtb,$(ALL_DTS))

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

always  := $(addprefix ../,$(DTB_LIST))

$(obj)/../%.dtb: $(src)/$(DTS_TARGET)/%.dts FORCE
	$(call if_changed_dep,dtc)

clean-files := *.dtb
