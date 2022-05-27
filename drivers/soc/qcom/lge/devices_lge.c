#include <linux/kernel.h>
#include <linux/string.h>
#include <soc/qcom/lge/board_lge.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/platform_device.h>
#include <asm/system_misc.h>

static enum hw_rev_type lge_bd_rev = HW_REV_MAX;

/* For debugging */
#ifdef CONFIG_LGE_LCD_TUNING
#include "../../../video/fbdev/msm/msm_dsi.h"
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

static struct lcd_platform_data lcd_pdata = {
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

#ifdef CONFIG_LGE_DISPLAY_COMMON
int lk_panel_init_fail = 0;
int lge_use_external_dsv = 0;
int display_panel_type;
#endif

#if defined(CONFIG_MACH_MSM8996_LUCYE)
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_02", "rev_03", "rev_04",
	"rev_a", "rev_b", "rev_c", "rev_d", "rev_10", "rev_11", "rev_12", "rev_13", "rev_14", "rev_15", "rev_16",
	"reserved"};
#elif defined(CONFIG_MACH_MSM8996_ELSA) || defined(CONFIG_MACH_MSM8996_ANNA)
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_02", "rev_a", "rev_b",
	"rev_c", "rev_d", "rev_e", "rev_f", "rev_10", "rev_11", "rev_12", "rev_13",
	"reserved"};
#else
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_f", "rev_b", "rev_c",
	"rev_d", "rev_e", "rev_a", "rev_g", "rev_10", "rev_11", "rev_12", "rev_13",
	"reserved"};
#endif

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

#ifdef CONFIG_LGE_PANEL_MAKER_ID_SUPPORT
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

#ifdef CONFIG_LGE_DISPLAY_COMMON
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

/* get boot mode information from cmdline.
 * If any boot mode is not specified,
 * boot mode is normal type.
 */
static enum lge_boot_mode_type lge_boot_mode = LGE_BOOT_MODE_NORMAL;

int __init lge_boot_mode_init(char *s)
{
	if (!strcmp(s, "charger"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGERLOGO;
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
__setup("lge.bootreasoncode=", lge_check_bootreason);

int lge_get_bootreason(void)
{
	return lge_boot_reason;
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
