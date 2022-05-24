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

ifeq ($(CONFIG_MACH_MSM8996_H1_GLOBAL_ESA),y)
DTS_TARGET := msm8996-h1_global_esa
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

ifeq ($(CONFIG_MACH_MSM8996_ELSA_ATT_US),y)
DTS_TARGET := msm8996-elsa_att_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_SPR_US),y)
DTS_TARGET := msm8996-elsa_spr_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_TMO_US),y)
DTS_TARGET := msm8996-elsa_tmo_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_CLR_PR),y)
DTS_TARGET := msm8996-elsa_tmo_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_USC_US),y)
DTS_TARGET := msm8996-elsa_usc_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_NAO_US),y)
DTS_TARGET := msm8996-elsa_nao_us
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_GLOBAL_COM),y)
DTS_TARGET := msm8996-elsa_global_com
endif

ifeq ($(CONFIG_MACH_MSM8996_ANNA_GLOBAL_COM),y)
DTS_TARGET := msm8996-anna_global_com
endif

ifeq ($(CONFIG_MACH_MSM8996_ANNA_KR),y)
DTS_TARGET := msm8996-anna_kr
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_GLOBAL_LIVE_DUMMY),y)
DTS_TARGET := msm8996-elsa_global_livedummy
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_GLOBAL_CA),y)
DTS_TARGET := msm8996-elsa_global_ca
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_CNO_CN),y)
DTS_TARGET := msm8996-elsa_cno_cn
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_KDDI_JP),y)
DTS_TARGET := msm8996-elsa_kddi
endif

ifeq ($(CONFIG_MACH_MSM8996_ELSA_DCM_JP),y)
DTS_TARGET := msm8996-elsa_dcm
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_KR),y)
DTS_TARGET := msm8996-lucye_kr
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_GLOBAL_COM),y)
DTS_TARGET := msm8996-lucye_global_com
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_GLOBAL_CA),y)
DTS_TARGET := msm8996-lucye_global_ca
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_VZW),y)
DTS_TARGET := msm8996-lucye_vzw
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_SPR_US),y)
DTS_TARGET := msm8996-lucye_spr_us
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_NAO_US),y)
DTS_TARGET := msm8996-lucye_nao_us
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_ATT_US),y)
DTS_TARGET := msm8996-lucye_att_us
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_TMO_US),y)
DTS_TARGET := msm8996-lucye_tmo_us
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_CLR_PR),y)
DTS_TARGET := msm8996-lucye_tmo_us
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_GLOBAL_LDU),y)
DTS_TARGET := msm8996-lucye_global_ldu
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_LGU_LDU),y)
DTS_TARGET := msm8996-lucye_lgu_ldu
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_ACG_US),y)
DTS_TARGET := msm8996-lucye_acg_us
endif

ifeq ($(CONFIG_MACH_MSM8996_LUCYE_TRF_US_VZW),y)
DTS_TARGET := msm8996-lucye_trf_us_vzw
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
