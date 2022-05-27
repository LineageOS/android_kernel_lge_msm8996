#ifndef __ASM_ARCH_MSM_BOARD_LGE_H
#define __ASM_ARCH_MSM_BOARD_LGE_H

#if defined(CONFIG_MACH_MSM8996_LUCYE)
enum hw_rev_type {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_EVB3,
	HW_REV_0,
	HW_REV_0_1,
	HW_REV_0_2,
	HW_REV_0_3,
	HW_REV_0_4,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_1_3,
	HW_REV_1_4,
	HW_REV_1_5,
	HW_REV_1_6,
	HW_REV_MAX
};
#elif defined(CONFIG_MACH_MSM8996_ELSA) || defined(CONFIG_MACH_MSM8996_ANNA)
enum hw_rev_type {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_EVB3,
	HW_REV_0,
	HW_REV_0_1,
	HW_REV_0_2,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_1_3,
	HW_REV_MAX
};
#else
enum hw_rev_type {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_EVB3,
	HW_REV_0,
	HW_REV_0_1,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_1_3,
	HW_REV_MAX
};
#endif

extern char *rev_str[];
enum hw_rev_type lge_get_board_revno(void);

enum lge_boot_mode_type {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_QEM_56K,
	LGE_BOOT_MODE_QEM_130K,
	LGE_BOOT_MODE_QEM_910K,
	LGE_BOOT_MODE_PIF_56K,
	LGE_BOOT_MODE_PIF_130K,
	LGE_BOOT_MODE_PIF_910K,
	LGE_BOOT_MODE_MINIOS	/* LGE_UPDATE for MINIOS2.0 */
};

enum lge_boot_mode_type lge_get_boot_mode(void);
int lge_get_factory_boot(void);
extern int lge_get_bootreason(void);

#ifdef CONFIG_LGE_LCD_OFF_DIMMING
extern int lge_get_bootreason_with_lcd_dimming(void);
#endif

#if defined(CONFIG_LGE_DISPLAY_COMMON)
int lge_get_lk_panel_status(void);
int lge_get_panel(void);
void lge_set_panel(int);
#endif

#if defined(CONFIG_LGE_PANEL_MAKER_ID_SUPPORT)
enum panel_maker_id_type {
	LGD_LG4946 = 0,
	LGD_LG4945,
	LGD_S3320,
	LGD_TD4302,
	PANEL_MAKER_ID_MAX
};

enum panel_maker_id_type lge_get_panel_maker_id(void);
#endif

#if defined(CONFIG_LGE_DISPLAY_COMMON)
enum panel_revision_id_type {
	LGD_LG4946_REV0 = 0,
	LGD_LG4946_REV1,
	LGD_LG4946_REV2,
	LGD_LG4946_REV3,
	PANEL_REVISION_ID_MAX
};

enum panel_revision_id_type lge_get_panel_revision_id(void);
#endif

/* For debugging */
#ifdef CONFIG_LGE_LCD_TUNING
struct lcd_platform_data {
int (*set_values) (int *tun_lcd_t);
int (*get_values) (int *tun_lcd_t);
};

void __init lge_add_lcd_misc_devices(void);
#endif

#endif /* __ASM_ARCH_MSM_BOARD_LGE_H */
