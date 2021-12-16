## LG G5
ifeq ($(CONFIG_MACH_MSM8996_H1_LRA_US),y)
DTS_TARGET := g5/h1_us
endif
ifeq ($(CONFIG_MACH_MSM8996_H1_TMO_US),y)
DTS_TARGET := g5/h1_us
endif
ifeq ($(CONFIG_MACH_MSM8996_H1_GLOBAL_COM),y)
DTS_TARGET := g5/h1_global
endif

## LG G6
ifeq ($(CONFIG_MACH_MSM8996_LUCYE_NAO_US),y)
DTS_TARGET := g6/lucye_nao_us
endif
ifeq ($(CONFIG_MACH_MSM8996_LUCYE_TMO_US),y)
DTS_TARGET := g6/lucye_tmo_us
endif
ifeq ($(CONFIG_MACH_MSM8996_LUCYE_GLOBAL_COM),y)
DTS_TARGET := g6/lucye_global_com
endif

## LG V20
ifeq ($(CONFIG_MACH_MSM8996_ELSA_KR),y)
DTS_TARGET := v20/elsa_kr
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_VZW),y)
DTS_TARGET := v20/elsa_vzw
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_ATT_US),y)
DTS_TARGET := v20/elsa_att_us
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_SPR_US),y)
DTS_TARGET := v20/elsa_spr_us
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_TMO_US),y)
DTS_TARGET := v20/elsa_tmo_us
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_CLR_PR),y)
DTS_TARGET := v20/elsa_tmo_us
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_USC_US),y)
DTS_TARGET := v20/elsa_usc_us
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_NAO_US),y)
DTS_TARGET := v20/elsa_nao_us
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_CNO_CN),y)
DTS_TARGET := v20/elsa_cno_cn
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_DCM_JP),y)
DTS_TARGET := v20/elsa_dcm
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_KDDI_JP),y)
DTS_TARGET := v20/elsa_kddi
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_GLOBAL_CA),y)
DTS_TARGET := v20/elsa_global_ca
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_GLOBAL_COM),y)
DTS_TARGET := v20/elsa_global_com
endif
ifeq ($(CONFIG_MACH_MSM8996_ELSA_GLOBAL_LIVE_DUMMY),y)
DTS_TARGET := v20/elsa_global_livedummy
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
