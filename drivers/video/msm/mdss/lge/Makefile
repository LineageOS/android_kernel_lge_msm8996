obj-$(CONFIG_LGE_DISPLAY_H1_COMMON) += h1/
obj-$(CONFIG_LGE_DISPLAY_HPLUS_COMMON) += hplus/

obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_mdss_aod.o
obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_mdss_dsi.o
obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_mdss_dsi_panel.o
obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_mdss_fb.o
ifeq ($(CONFIG_LGE_DISPLAY_READER_MODE),y)
obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_reader_mode.o
endif
