#ifndef __ASM_ARCH_MSM_BOARD_LGE_H
#define __ASM_ARCH_MSM_BOARD_LGE_H

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BOARD_REVISION
#else
#if defined(CONFIG_MACH_MSM8996_ELSA)
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
#endif

extern char *rev_str[];

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_BOARD_REVISION
#else
enum hw_rev_type lge_get_board_revno(void);
#endif

#ifdef CONFIG_LGE_USB_G_LAF
enum lge_laf_mode_type {
	LGE_LAF_MODE_NORMAL = 0,
	LGE_LAF_MODE_LAF,
};

enum lge_laf_mode_type lge_get_laf_mode(void);
#endif

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
int lge_pre_self_diagnosis(char *drv_bus_code, int func_code, char *dev_code, char *drv_code, int errno);
#endif
#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
struct pre_selfd_platform_data {
	int (*set_values) (int r, int g, int b);
	int (*get_values) (int *r, int *g, int *b);
};
#endif
#ifdef CONFIG_LGE_USB_FACTORY
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
	LGE_BOOT_MODE_MINIOS    /* LGE_UPDATE for MINIOS2.0 */
};

enum lge_boot_mode_type lge_get_boot_mode(void);
int lge_get_android_dlcomplete(void);
int lge_get_factory_boot(void);
int get_lge_frst_status(void);
#endif

int lge_get_mfts_mode(void);

extern int lge_get_bootreason(void);

#ifdef CONFIG_LGE_LCD_OFF_DIMMING
extern int lge_get_bootreason_with_lcd_dimming(void);
#endif

#ifdef CONFIG_LGE_EARJACK_DEBUGGER
/* config */
#define UART_CONSOLE_ENABLE_ON_EARJACK		BIT(0)
#define UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER	BIT(1)
#define UART_CONSOLE_ENABLE_ON_DEFAULT		BIT(2)
/* current status
 * ENABLED | DISABLED : logical enable/disable
 * READY : It means whether device is ready or not.
 *         So even if in ENABLED state, console output will
 *         not be emitted on NOT-ready state.
 */
#define UART_CONSOLE_ENABLED		BIT(3)
#define UART_CONSOLE_DISABLED		!(BIT(3))
#define UART_CONSOLE_READY		BIT(4)
/* filter */
# define UART_CONSOLE_MASK_ENABLE_ON	(BIT(0) | BIT(1) | BIT(2))
# define UART_CONSOLE_MASK_CONFIG	UART_CONSOLE_MASK_ENABLE_ON
# define UART_CONSOLE_MASK_ENABLED	BIT(3)
# define UART_CONSOLE_MASK_READY	BIT(4)

/* util macro */
#define lge_uart_console_should_enable_on_earjack()	\
	(unsigned int)(lge_uart_console_get_config() &	\
			UART_CONSOLE_ENABLE_ON_EARJACK)

#define lge_uart_console_should_enable_on_earjack_debugger()	\
	(unsigned int)(lge_uart_console_get_config() &		\
			UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER)

#define lge_uart_console_should_enable_on_default()	\
	(unsigned int)(lge_uart_console_get_config() &	\
			UART_CONSOLE_ENABLE_ON_DEFAULT)

#define lge_uart_console_on_earjack_in()	\
	do {					\
		msm_serial_set_uart_console(	\
			lge_uart_console_should_enable_on_earjack());	\
	} while (0)

#define lge_uart_console_on_earjack_out()	\
	do {					\
		msm_serial_set_uart_console(	\
				lge_uart_console_should_enable_on_default()); \
	} while (0)

#define lge_uart_console_on_earjack_debugger_in()	\
	do {						\
		msm_serial_set_uart_console(		\
			lge_uart_console_should_enable_on_earjack_debugger()); \
	} while (0)

#define lge_uart_console_on_earjack_debugger_out()	\
	do {						\
		msm_serial_set_uart_console(		\
				lge_uart_console_should_enable_on_default()); \
	} while (0)

/* config =  UART_CONSOLE_ENABLE_ON_XXX [| UART_CONSOLE_ENABLE_ON_XXX]* */
extern unsigned int lge_uart_console_get_config(void);
extern void lge_uart_console_set_config(unsigned int config);

/* logical uart console status modifier
 * used as a flag to tell "I want to enable/disable uart console"
 * @RETURN or @PARAM::enabled
 * UART_CONSOLE_ENABLED  (non-zero): enabled
 * !UART_CONSOLE_ENABLED (zero): disabled
 */
extern unsigned int lge_uart_console_get_enabled(void);
extern void lge_uart_console_set_enabled(int enabled);
/* internal uart console device status tracker
 *
 * @RETURN or @PARAM::ready
 * UART_CONSOLE_READY (non-zero): device is ready
 * !UART_CONSOLE_READY (zero): device is not ready
 */
extern unsigned int lge_uart_console_get_ready(void);
extern void lge_uart_console_set_ready(unsigned int ready);

/* real device enabler (or disabler)
 * control uart console device to enable/disable
 * NOTE @PARAM::enable should be selected by uart console enable/disable policy
 * which can be known by lge_uart_console_should_enable_on_xxx.
 * @PARAM::enable
 * zero : disabled
 * non-zero : enable
 */
extern int msm_serial_set_uart_console(int enable);
extern int msm_serial_force_off(void);
#endif

#if defined(CONFIG_LGE_DISPLAY_COMMON)
int lge_get_lk_panel_status(void);
int lge_get_dsv_status(void);
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

#ifdef CONFIG_LGE_LCD_TUNING
struct lcd_platform_data {
int (*set_values) (int *tun_lcd_t);
int (*get_values) (int *tun_lcd_t);
};

void __init lge_add_lcd_misc_devices(void);
#endif

#ifdef CONFIG_LGE_ALICE_FRIENDS
enum lge_alice_friends {
	LGE_ALICE_FRIENDS_NONE = 0,
	LGE_ALICE_FRIENDS_CM,
	LGE_ALICE_FRIENDS_HM,
	LGE_ALICE_FRIENDS_HM_B,
};

enum lge_alice_friends lge_get_alice_friends(void);
#endif

extern int on_hidden_reset;
#endif
