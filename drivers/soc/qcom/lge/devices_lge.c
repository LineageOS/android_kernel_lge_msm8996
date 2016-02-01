#include <linux/kernel.h>
#include <linux/string.h>

#include <soc/qcom/lge/board_lge.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/platform_device.h>
#include <asm/system_misc.h>

#ifdef CONFIG_LGE_USB_FACTORY
#include <linux/platform_data/lge_android_usb.h>
#endif
#ifdef CONFIG_LGE_ALICE_FRIENDS
#include <soc/qcom/lge/lge_acc_nt_type.h>
#endif

static enum hw_rev_type lge_bd_rev = HW_REV_MAX;

#ifdef CONFIG_LGE_LCD_TUNING
#include "../drivers/video/msm/mdss/mdss_dsi.h"
int tun_lcd[128];

int lcd_set_values(int *tun_lcd_t)
{
	memset(tun_lcd,0,128*sizeof(int));
	memcpy(tun_lcd,tun_lcd_t,128*sizeof(int));
	printk("lcd_set_values ::: tun_lcd[0]=[%x], tun_lcd[1]=[%x], tun_lcd[2]=[%x] ......\n"
		,tun_lcd[0],tun_lcd[1],tun_lcd[2]);
	return 0;
}
static int lcd_get_values(int *tun_lcd_t)
{
	memset(tun_lcd_t,0,128*sizeof(int));
	memcpy(tun_lcd_t,tun_lcd,128*sizeof(int));
	printk("lcd_get_values\n");
	return 0;
}

static struct lcd_platform_data lcd_pdata ={
	.set_values = lcd_set_values,
	.get_values = lcd_get_values,
};
static struct platform_device lcd_ctrl_device = {
	.name = "lcd_ctrl",
	.dev = {
	.platform_data = &lcd_pdata,
	}
};

static int __init lge_add_lcd_ctrl_devices(void)
{
	return platform_device_register(&lcd_ctrl_device);
}
arch_initcall(lge_add_lcd_ctrl_devices);
#endif
#if defined(CONFIG_LGE_MIPI_H1_INCELL_QHD_CMD_PANEL)
int lk_panel_init_fail = 0;
int lge_use_external_dsv = 0;
int display_panel_type;
#endif

char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_f", "rev_b", "rev_c",
	"rev_d", "rev_e", "rev_a", "rev_g", "rev_10", "rev_11", "rev_12",
	"reserved"};

static int __init board_revno_setup(char *rev_info)
{
	int i;

	for (i = 0; i < HW_REV_MAX; i++) {
		if (!strncmp(rev_info, rev_str[i], 6)) {
			lge_bd_rev = i;
			system_rev = lge_bd_rev;
			break;
		}
	}
	pr_info("BOARD: LGE %s\n", rev_str[lge_bd_rev]);

	return 1;
}
__setup("lge.rev=", board_revno_setup);

enum hw_rev_type lge_get_board_revno(void)
{
	return lge_bd_rev;
}

#if defined(CONFIG_LGE_PANEL_MAKER_ID_SUPPORT)
static enum panel_maker_id_type lge_panel_maker_id = PANEL_MAKER_ID_MAX;

static int __init panel_maker_id_setup(char *panel_maker_id_info)
{
	int ret = 0;
	int idx = PANEL_MAKER_ID_MAX;
	if(panel_maker_id_info == NULL) {
		pr_info("UNKOWN PANEL MAKER ID: %d\n", lge_panel_maker_id);
		return 0;
	}

	ret = kstrtoint(panel_maker_id_info, 10, &idx);
	if(!ret && idx < PANEL_MAKER_ID_MAX && idx >= 0)
		lge_panel_maker_id = idx;
	else {
		pr_info("UNKOWN PANEL MAKER ID: %d\n", lge_panel_maker_id);
		return 0;
	}

	pr_info("PANEL MAKER ID: %d\n", lge_panel_maker_id);
	return 1;
}
__setup("lge.lcd=", panel_maker_id_setup);

enum panel_maker_id_type lge_get_panel_maker_id(void)
{
	return lge_panel_maker_id;
}
#endif

#if defined (CONFIG_LGE_MIPI_H1_INCELL_QHD_CMD_PANEL)
static enum panel_revision_id_type lge_panel_revision_id = PANEL_REVISION_ID_MAX;

static int __init panel_revision_id_setup(char *panel_revision_id_info)
{
	int ret = 0;
	int idx = PANEL_REVISION_ID_MAX;
	if(panel_revision_id_info == NULL) {
		pr_info("UNKOWN PANEL REVISION ID: %d\n", lge_panel_revision_id);
		return 0;
	}

	ret = kstrtoint(panel_revision_id_info, 10, &idx);
	if(!ret && idx < PANEL_REVISION_ID_MAX && idx >= 0)
		lge_panel_revision_id = idx;
	else {
		pr_info("UNKOWN PANEL MAKER ID: %d\n", lge_panel_revision_id);
		return 0;
	}

	pr_err("PANEL REVISION ID: %d\n", lge_panel_revision_id);
	return 1;
}
__setup("lge.lcd.rev=", panel_revision_id_setup);

enum panel_revision_id_type lge_get_panel_revision_id(void)
{
	return lge_panel_revision_id;
}
#endif

#if defined(CONFIG_LGE_MIPI_H1_INCELL_QHD_CMD_PANEL)
static int __init lk_panel_init_status(char *panel_init_cmd)
{
	if (strncmp(panel_init_cmd, "1", 1) == 0) {
		lk_panel_init_fail = 1;
		pr_info("lk panel init fail[%d]\n", lk_panel_init_fail);
	} else {
		lk_panel_init_fail = 0;
	}

	return 1;
}
__setup("lge.pinit_fail=", lk_panel_init_status);

int lge_get_lk_panel_status(void)
{
     return lk_panel_init_fail;
}

static int __init lge_use_dsv(char *use_external_dsv)
{
	if (strncmp(use_external_dsv, "1", 1) == 0) {
		lge_use_external_dsv = 1;
		pr_err("lge use external dsv[%d]\n", lge_use_external_dsv);
	} else {
		lge_use_external_dsv = 0;
	}

	return 1;
}
__setup("lge.with_external_dsv=", lge_use_dsv);

int lge_get_dsv_status(void)
{
     return lge_use_external_dsv;
}

void lge_set_panel(int panel_type)
{
	pr_info("panel_type is %d\n",panel_type);
	display_panel_type = panel_type;
}

int lge_get_panel(void)
{
	return display_panel_type;
}
#endif

#ifdef CONFIG_LGE_EARJACK_DEBUGGER
/* s_uart_console_status bits format
 * ------higher than bit4 are not used
 * bit5...: not used
 * ------bit4 indicates whenter uart console was ready(probed)
 * bit4: [UART_CONSOLE_READY]
 * ------current uart console status -----------------
 * bit3: [UART_CONSOLE_ENABLED]
 * ------configuration bit field -----------------
 * bit2: [UART_CONSOLE_ENABLE_ON_DEFAULT]
 * bit1; [UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER]
 * bit0: [UART_CONSOLE_ENABLE_ON_EARJACK]
 */
static unsigned int s_uart_console_status = 0;	/* disabling uart console */

unsigned int lge_uart_console_get_config(void)
{
	return (s_uart_console_status & UART_CONSOLE_MASK_CONFIG);
}

void lge_uart_console_set_config(unsigned int config)
{
	config &= UART_CONSOLE_MASK_CONFIG;
	s_uart_console_status |= config;
}

unsigned int lge_uart_console_get_enabled(void)
{
	return s_uart_console_status & UART_CONSOLE_MASK_ENABLED;
}

void lge_uart_console_set_enabled(int enabled)
{
	s_uart_console_status &= ~UART_CONSOLE_MASK_ENABLED;
	/* for caller conding convenience, regard no-zero as enabled also */
	s_uart_console_status |= (enabled ? UART_CONSOLE_ENABLED : 0);
}

unsigned int lge_uart_console_get_ready(void)
{
	return s_uart_console_status & UART_CONSOLE_MASK_READY;
}

void lge_uart_console_set_ready(unsigned int ready)
{
	s_uart_console_status &= ~UART_CONSOLE_MASK_READY;
	/* for caller side coding convenience, regard no-zero as ready also */
	s_uart_console_status |= (ready ? UART_CONSOLE_READY : 0);
}

#endif /* CONFIG_LGE_EARJACK_DEBUGGER */

#ifdef CONFIG_LGE_USB_FACTORY
/* get boot mode information from cmdline.
 * If any boot mode is not specified,
 * boot mode is normal type.
 */
static enum lge_boot_mode_type lge_boot_mode = LGE_BOOT_MODE_NORMAL;
int __init lge_boot_mode_init(char *s)
{
	if (!strcmp(s, "charger"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGER;
	else if (!strcmp(s, "chargerlogo"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGERLOGO;
	else if (!strcmp(s, "qem_56k"))
		lge_boot_mode = LGE_BOOT_MODE_QEM_56K;
	else if (!strcmp(s, "qem_130k"))
		lge_boot_mode = LGE_BOOT_MODE_QEM_130K;
	else if (!strcmp(s, "qem_910k"))
		lge_boot_mode = LGE_BOOT_MODE_QEM_910K;
	else if (!strcmp(s, "pif_56k"))
		lge_boot_mode = LGE_BOOT_MODE_PIF_56K;
	else if (!strcmp(s, "pif_130k"))
		lge_boot_mode = LGE_BOOT_MODE_PIF_130K;
	else if (!strcmp(s, "pif_910k"))
		lge_boot_mode = LGE_BOOT_MODE_PIF_910K;
	/* LGE_UPDATE_S for MINIOS2.0 */
	else if (!strcmp(s, "miniOS"))
		lge_boot_mode = LGE_BOOT_MODE_MINIOS;
	pr_info("ANDROID BOOT MODE : %d %s\n", lge_boot_mode, s);
	/* LGE_UPDATE_E for MINIOS2.0 */

	return 1;
}
__setup("androidboot.mode=", lge_boot_mode_init);

enum lge_boot_mode_type lge_get_boot_mode(void)
{
	return lge_boot_mode;
}

int lge_get_factory_boot(void)
{
	int res;

	/*   if boot mode is factory,
	 *   cable must be factory cable.
	 */
	switch (lge_boot_mode) {
	case LGE_BOOT_MODE_QEM_56K:
	case LGE_BOOT_MODE_QEM_130K:
	case LGE_BOOT_MODE_QEM_910K:
	case LGE_BOOT_MODE_PIF_56K:
	case LGE_BOOT_MODE_PIF_130K:
	case LGE_BOOT_MODE_PIF_910K:
	case LGE_BOOT_MODE_MINIOS:
		res = 1;
		break;
	default:
		res = 0;
		break;
	}
	return res;
}

int get_factory_cable(void)
{
	int res = 0;

	/* if boot mode is factory, cable must be factory cable. */
	switch (lge_boot_mode) {
	case LGE_BOOT_MODE_QEM_56K:
	case LGE_BOOT_MODE_PIF_56K:
		res = LGEUSB_FACTORY_56K;
		break;

	case LGE_BOOT_MODE_QEM_130K:
	case LGE_BOOT_MODE_PIF_130K:
		res = LGEUSB_FACTORY_130K;
		break;

	case LGE_BOOT_MODE_QEM_910K:
	case LGE_BOOT_MODE_PIF_910K:
		res = LGEUSB_FACTORY_910K;
		break;

	default:
		res = 0;
		break;
	}

	return res;
}

struct lge_android_usb_platform_data lge_android_usb_pdata = {
	.vendor_id = 0x1004,
	.factory_pid = 0x6000,
	.iSerialNumber = 0,
	.product_name = "LGE Android Phone",
	.manufacturer_name = "LG Electronics Inc.",
	.factory_composition = "acm,diag",
	.get_factory_cable = get_factory_cable,
};

static struct platform_device lge_android_usb_device = {
	.name = "lge_android_usb",
	.id = -1,
	.dev = {
		.platform_data = &lge_android_usb_pdata,
	},
};

static int __init lge_android_usb_devices_init(void)
{
	return platform_device_register(&lge_android_usb_device);
}
arch_initcall(lge_android_usb_devices_init);
#endif

#ifdef CONFIG_LGE_USB_DIAG_LOCK
static struct platform_device lg_diag_cmd_device = {
	.name = "lg_diag_cmd",
	.id = -1,
	.dev    = {
		.platform_data = 0, /* &lg_diag_cmd_pdata */
	},
};

static int __init lge_diag_devices_init(void)
{
	return platform_device_register(&lg_diag_cmd_device);
}
arch_initcall(lge_diag_devices_init);
#endif

#ifdef CONFIG_LGE_QFPROM_INTERFACE
static struct platform_device qfprom_device = {
	.name = "lge-qfprom",
	.id = -1,
};

static int __init lge_add_qfprom_devices(void)
{
	return platform_device_register(&qfprom_device);
}

arch_initcall(lge_add_qfprom_devices);
#endif

#ifdef CONFIG_LGE_USB_G_LAF
static enum lge_laf_mode_type lge_laf_mode = LGE_LAF_MODE_NORMAL;

int __init lge_laf_mode_init(char *s)
{
	if (strcmp(s, "") && strcmp(s, "MID"))
		lge_laf_mode = LGE_LAF_MODE_LAF;

	return 1;
}
__setup("androidboot.laf=", lge_laf_mode_init);

enum lge_laf_mode_type lge_get_laf_mode(void)
{
	return lge_laf_mode;
}
#endif

static int lge_boot_reason = -1;

static int __init lge_check_bootreason(char *reason)
{
	int ret = 0;

	/* handle corner case of kstrtoint */
	if (!strcmp(reason, "0xffffffff")) {
		lge_boot_reason = 0xffffffff;
		return 1;
	}

	ret = kstrtoint(reason, 16, &lge_boot_reason);
	if (!ret)
		pr_info("LGE BOOT REASON: 0x%x\n", lge_boot_reason);
	else
		pr_info("LGE BOOT REASON: Couldn't get bootreason - %d\n", ret);

	return 1;
}
__setup("lge.bootreason=", lge_check_bootreason);

int lge_get_bootreason(void)
{
	return lge_boot_reason;
}

int on_hidden_reset;

static int __init lge_check_hidden_reset(char *reset_mode)
{
	if (!strncmp(reset_mode, "on", 2))
		on_hidden_reset = 1;

	return 1;
}
__setup("lge.hreset=", lge_check_hidden_reset);

static int lge_mfts_mode = 0;

static int __init lge_check_mfts_mode(char *s)
{
	int ret = 0;

	ret = kstrtoint(s, 10, &lge_mfts_mode);
	if(!ret)
		pr_info("LGE MFTS MODE: %d\n", lge_mfts_mode);
	else
		pr_info("LGE MFTS MODE: faile to get mfts mode %d\n", lge_mfts_mode);


	return 1;
}
__setup("mfts.mode=", lge_check_mfts_mode);

int lge_get_mfts_mode(void)
{
	return lge_mfts_mode;
}

#ifdef CONFIG_LGE_LCD_OFF_DIMMING
int lge_get_bootreason_with_lcd_dimming(void)
{
	int ret = 0;

	if (lge_get_bootreason() == 0x77665560)
		ret = 1;
	else if (lge_get_bootreason() == 0x77665561)
		ret = 2;
	else if (lge_get_bootreason() == 0x77665562)
		ret = 3;
	return ret;
}
#endif

/*
   for download complete using LAF image
   return value : 1 --> right after laf complete & reset
 */

int android_dlcomplete = 0;

int __init lge_android_dlcomplete(char *s)
{
	if(strncmp(s,"1",1) == 0)
		android_dlcomplete = 1;
	else
		android_dlcomplete = 0;
	printk("androidboot.dlcomplete = %d\n", android_dlcomplete);

	return 1;
}
__setup("androidboot.dlcomplete=", lge_android_dlcomplete);

int lge_get_android_dlcomplete(void)
{
	return android_dlcomplete;
}

#ifdef CONFIG_LGE_ALICE_FRIENDS
static enum lge_alice_friends lge_alice_friends = LGE_ALICE_FRIENDS_NONE;
int __init lge_get_cable_type_init(char *s)
{
	if (!strcmp(s, "LT_270K"))
		lge_alice_friends = LGE_ALICE_FRIENDS_CM;
	else if (!strcmp(s, "LT_330K"))
		lge_alice_friends = LGE_ALICE_FRIENDS_HM;
	else
		lge_alice_friends = LGE_ALICE_FRIENDS_NONE;

	return 1;
}
__setup("bootcable.type=", lge_get_cable_type_init);

enum lge_alice_friends lge_get_alice_friends(void)
{
	nt_type_t nt_type = get_acc_nt_type();

	pr_info("[BSP-USB] nt_type(%d)\n", nt_type);

	if (nt_type == NT_TYPE_CM)
		lge_alice_friends = LGE_ALICE_FRIENDS_CM;
	else if (nt_type == NT_TYPE_HM)
		lge_alice_friends = LGE_ALICE_FRIENDS_HM_B;

	return lge_alice_friends;
}
#endif
