/*
 * drivers/soc/qcom/lge/lge_handle_panic.c
 *
 * Copyright (C) 2015 LG Electronics, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <asm/setup.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/subsystem_restart.h>

#include <soc/qcom/lge/lge_handle_panic.h>
#include <soc/qcom/lge/board_lge.h>

#include <linux/input.h>

#define PANIC_HANDLER_NAME        "panic-handler"

#define RESTART_REASON_ADDR       0x65c

#define CRASH_HANDLER_MAGIC_NUM   0x4c474500
#define CRASH_HANDLER_MAGIC_ADDR  0x44
#define RAM_CONSOLE_ADDR_ADDR     0x48
#define RAM_CONSOLE_SIZE_ADDR     0x4c
#define FB_ADDR_ADDR              0x50

#define RESTART_REASON      (msm_imem_base + RESTART_REASON_ADDR)
#define CRASH_HANDLER_MAGIC (msm_imem_base + CRASH_HANDLER_MAGIC_ADDR)
#define RAM_CONSOLE_ADDR    (msm_imem_base + RAM_CONSOLE_ADDR_ADDR)
#define RAM_CONSOLE_SIZE    (msm_imem_base + RAM_CONSOLE_SIZE_ADDR)
#define FB_ADDR             (msm_imem_base + FB_ADDR_ADDR)

static void *msm_imem_base;
static int dummy_arg;
static int subsys_crash_magic;

static struct panic_handler_data *panic_handler;

#define KEY_CRASH_TIMEOUT 3000
static int gen_key_panic = 0;
static int key_crash_cnt = 0;
static unsigned long key_crash_last_time = 0;

void lge_set_subsys_crash_reason(const char *name, int type)
{
	const char *subsys_name[] =
		{ "adsp", "mba", "modem", "wcnss", "slpi", "venus" };
	int i = 0;

	if (!name)
		return;

	for (i = 0; i < ARRAY_SIZE(subsys_name); i++) {
		if (!strncmp(subsys_name[i], name, 40)) {
			subsys_crash_magic = LGE_RB_MAGIC | ((i+1) << 12)
					| type;
			break;
		}
	}
	return;
}

void lge_set_ram_console_addr(unsigned int addr, unsigned int size)
{
	writel_relaxed(addr, RAM_CONSOLE_ADDR);
	writel_relaxed(size, RAM_CONSOLE_SIZE);
}

void lge_set_fb_addr(unsigned int addr)
{
	writel_relaxed(addr, FB_ADDR);
}

void lge_set_restart_reason(unsigned int reason)
{
	writel_relaxed(reason, RESTART_REASON);
}

void lge_set_panic_reason(void)
{
	if (lge_get_download_mode() && gen_key_panic) {
		lge_set_restart_reason(LGE_RB_MAGIC | LGE_ERR_KERN | LGE_ERR_KEY);
		return;
	}

	if (subsys_crash_magic == 0)
		lge_set_restart_reason(LGE_RB_MAGIC | LGE_ERR_KERN);
	else
		lge_set_restart_reason(subsys_crash_magic);
}

int lge_get_restart_reason(void)
{
	if (msm_imem_base)
		return readl_relaxed(RESTART_REASON);
	else
		return 0;
}

inline static void lge_set_key_crash_cnt(int key, int* clear)
{
	unsigned long cur_time = 0;
	unsigned long key_crash_gap = 0;

	cur_time = jiffies_to_msecs(jiffies);
	key_crash_gap = cur_time - key_crash_last_time;

	if ((key_crash_cnt != 0) && (key_crash_gap > KEY_CRASH_TIMEOUT)) {
		pr_debug("%s: Ready to panic %d : over time %ld!\n", __func__, key, key_crash_gap);
		return;
	}

	*clear = 0;
	key_crash_cnt++;
	key_crash_last_time = cur_time;

	pr_info("%s: Ready to panic %d : count %d, time gap %ld!\n", __func__, key, key_crash_cnt, key_crash_gap);
}

void lge_gen_key_panic(int key)
{
	int clear = 1;
	int order = key_crash_cnt % 3;

	if(lge_get_download_mode() != 1)
		return;

	if(((key == KEY_VOLUMEDOWN) && (order == 0))
		|| ((key == KEY_POWER) && (order == 1))
		|| ((key == KEY_VOLUMEUP) && (order == 2)))
		lge_set_key_crash_cnt(key, &clear);

	if (clear == 1) {
		key_crash_cnt = 0;
		pr_debug("%s: Ready to panic %d : cleared!\n", __func__, key);
		return;
	}

	if (key_crash_cnt == 7) {
		gen_key_panic = 1;
		panic("%s: Generate panic by key!\n", __func__);
	}
}

static int gen_bug(const char *val, struct kernel_param *kp)
{
	BUG();
	return 0;
}
module_param_call(gen_bug, gen_bug, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_panic(const char *val, struct kernel_param *kp)
{
	panic("generate test-panic");
	return 0;
}
module_param_call(gen_panic, gen_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_adsp_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("adsp");
	return 0;
}
module_param_call(gen_adsp_panic, gen_adsp_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_mba_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("mba");
	return 0;
}
module_param_call(gen_mba_panic, gen_mba_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_modem_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("modem");
	return 0;
}
module_param_call(gen_modem_panic, gen_modem_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_wcnss_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("wcnss");
	return 0;
}
module_param_call(gen_wcnss_panic, gen_wcnss_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_slpi_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("slpi");
	return 0;
}
module_param_call(gen_slpi_panic, gen_slpi_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_venus_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("venus");
	return 0;
}
module_param_call(gen_venus_panic, gen_venus_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define WDT0_RST        0x04
#define WDT0_EN         0x08
#define WDT0_BARK_TIME  0x10
#define WDT0_BITE_TIME  0x14

extern void __iomem *wdt_timer_get_timer0_base(void);

static int gen_wdt_bark(const char *val, struct kernel_param *kp)
{
	void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	pr_info("%s\n", __func__);
	writel_relaxed(0, msm_tmr0_base + WDT0_EN);
	writel_relaxed(1, msm_tmr0_base + WDT0_RST);
	writel_relaxed(0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	writel_relaxed(5 * 0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	writel_relaxed(1, msm_tmr0_base + WDT0_EN);
	mb();
	mdelay(5000);

	pr_err("%s failed\n", __func__);

	return -1;
}
module_param_call(gen_wdt_bark, gen_wdt_bark, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_wdt_bite(const char *val, struct kernel_param *kp)
{
	void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	pr_info("%s\n", __func__);
	writel_relaxed(0, msm_tmr0_base + WDT0_EN);
	writel_relaxed(1, msm_tmr0_base + WDT0_RST);
	writel_relaxed(5 * 0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	writel_relaxed(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	writel_relaxed(1, msm_tmr0_base + WDT0_EN);
	mb();
	mdelay(5000);

	pr_err("%s failed\n", __func__);

	return -1;
}
module_param_call(gen_wdt_bite, gen_wdt_bite, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define REG_MPM2_WDOG_BASE             0xFC4AA000
#define REG_OFFSET_MPM2_WDOG_RESET     0x0
#define REG_OFFSET_MPM2_WDOG_BITE_VAL  0x10
#define REG_VAL_WDOG_RESET_DO_RESET    0x10
#define REG_VAL_WDOG_BITE_VAL          0x400

/* forced sec wdt bite can cause unexpected bus hang */
static int gen_sec_wdt_bite(const char *val, struct kernel_param *kp)
{
	void *sec_wdog_virt;
	sec_wdog_virt = ioremap(REG_MPM2_WDOG_BASE, SZ_4K);

	if (!sec_wdog_virt) {
		pr_err("unable to map sec wdog page\n");
		return -ENOMEM;
	}

	pr_info("%s\n", __func__);
	writel_relaxed(REG_VAL_WDOG_RESET_DO_RESET,
		sec_wdog_virt + REG_OFFSET_MPM2_WDOG_RESET);
	writel_relaxed(REG_VAL_WDOG_BITE_VAL,
		sec_wdog_virt + REG_OFFSET_MPM2_WDOG_BITE_VAL);
	mb();
	mdelay(5000);

	pr_err("%s failed\n", __func__);
	iounmap(sec_wdog_virt);

	return -1;
}
module_param_call(gen_sec_wdt_bite, gen_sec_wdt_bite, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define SCM_SVC_SEC_WDOG_TRIG  0x08

static int gen_sec_wdt_scm(const char *val, struct kernel_param *kp)
{
	struct scm_desc desc;
	desc.args[0] = 0;
	desc.arginfo = SCM_ARGS(1);

	lge_disable_watchdog();

	pr_info("%s\n", __func__);
	scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT,
			SCM_SVC_SEC_WDOG_TRIG), &desc);

	pr_err("%s failed\n", __func__);

	return -1;
}
module_param_call(gen_sec_wdt_scm, gen_sec_wdt_scm, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define MPM2_TSENS_Sn_MIN_MAX_STATUS_CTRL  0x004A8020

/* msm8996: apps isn't authorized for tsens. must use with no xpu tzbsp */
static int gen_tsense_reset(const char *val, struct kernel_param *kp)
{
	int status;
	int max_threshold = 0;
	void *mpm2_tsens_min_max_status_ctrl;

	mpm2_tsens_min_max_status_ctrl
		= ioremap(MPM2_TSENS_Sn_MIN_MAX_STATUS_CTRL, SZ_32);

	if (!mpm2_tsens_min_max_status_ctrl) {
		pr_err("unable to map mpm2 register space\n");
		return -ENOMEM;
	}

	status = readl_relaxed(mpm2_tsens_min_max_status_ctrl);
	max_threshold = (status & 0xFFF00) >> 12;

	while (max_threshold > 0) {
		max_threshold -= 0xF;

		pr_info("%s sensor0 threshold:0x%x\n", __func__, max_threshold);
		writel_relaxed((status & 0xFFF000FF) | (max_threshold << 12),
			mpm2_tsens_min_max_status_ctrl);
		mb();
		mdelay(100);
	}

	pr_err("%s failed\n", __func__);
	iounmap(mpm2_tsens_min_max_status_ctrl);

	return -1;
}
module_param_call(gen_tsense_reset, gen_tsense_reset, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define SCM_SVC_SEC_WDOG_DIS  0x07

void sec_watchdog_disable(void)
{
	int ret;
	struct scm_desc desc;
	desc.args[0] = 1;
	desc.arginfo = SCM_ARGS(1);

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT,
				SCM_SVC_SEC_WDOG_DIS), &desc);

	if (ret || desc.ret[0]) {
		pr_err("%s failed\n", __func__);
		return;
	}

	pr_info("%s\n", __func__);
}

void lge_disable_watchdog(void)
{
	static int once = 1;
	void __iomem *msm_tmr0_base;

	if (once > 1)
		return;

	msm_tmr0_base = wdt_timer_get_timer0_base();
	if (!msm_tmr0_base)
		return;

	writel_relaxed(0, msm_tmr0_base + WDT0_EN);
	mb();
	once++;

	pr_info("%s\n", __func__);
}

void lge_enable_watchdog(void)
{
	static int once = 1;
	void __iomem *msm_tmr0_base;

	if (once > 1)
		return;

	msm_tmr0_base = wdt_timer_get_timer0_base();
	if (!msm_tmr0_base)
		return;

	writel_relaxed(1, msm_tmr0_base + WDT0_EN);
	mb();
	once++;

	pr_info("%s\n", __func__);
}

void lge_pet_watchdog(void)
{
	void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	if (!msm_tmr0_base)
		return;

	writel_relaxed(1, msm_tmr0_base + WDT0_RST);
	mb();

	pr_info("%s\n", __func__);
}

void lge_panic_handler_fb_free_page(unsigned long mem_addr, unsigned long size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	pfn_start = mem_addr >> PAGE_SHIFT;
	pfn_end = (mem_addr + size) >> PAGE_SHIFT;
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));
}

void lge_panic_handler_fb_cleanup(void)
{
	static int free = 1;

	if (!panic_handler || free > 1)
		return;

	if (panic_handler->fb_addr && panic_handler->fb_size) {
		memblock_free(panic_handler->fb_addr, panic_handler->fb_size);
		lge_panic_handler_fb_free_page(
				panic_handler->fb_addr, panic_handler->fb_size);
		free++;

		pr_info("%s: free[@0x%lx+@0x%lx)\n", PANIC_HANDLER_NAME,
				panic_handler->fb_addr, panic_handler->fb_size);
	}
}

#ifdef CONFIG_LGE_BOOT_LOCKUP_DETECT

#define LOCKUP_WQ_NOT_START_YET (-1)
#define LOCKUP_WQ_PAUSED        (0)
#define LOCKUP_WQ_STARTED       (1)
#define LOCKUP_WQ_CANCELED      (2)

static unsigned long boot_deadline = 240 * 1000;
static struct delayed_work lge_boot_lockup_detect_work;
static int boot_lockup_detect_working = -1;

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_ADC_QCT
#include <soc/qcom/lge/power/lge_power_class.h>

static int lge_boot_lockup_check_xo_therm_too_hot(void)
{
	struct lge_power *lge_adc_lpc;
	union lge_power_propval lge_val = {0,};
	int xo_therm,rc;
	lge_adc_lpc = lge_power_get_by_name("lge_adc");

	if(!lge_adc_lpc) {
		pr_emerg("lge_adc is not connected\n");
        return -1;
	}

	rc = lge_adc_lpc->get_property(lge_adc_lpc,
			LGE_POWER_PROP_XO_THERM_PHY, &lge_val);

	if(rc < 0) {
		pr_emerg("lge_adc XO_THERM is not valid\n");
        return -1;
	}

	xo_therm = lge_val.intval;

	pr_emerg("boot_lockup_detect XO_THERM[%d]\n",xo_therm);

	if(xo_therm >= 70) // too hot
		return 1;

    return 0;
}
#endif
static void lge_boot_lockup_detect_func(struct work_struct *work)
{
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_ADC_QCT
	if(lge_boot_lockup_check_xo_therm_too_hot() == 1) { // too hot
		pr_emerg("==================================================================\n");
		pr_emerg("WARNING: detecting lockup during boot! skip panic by too hot\n");
		pr_emerg("==================================================================\n");
	} else
#endif
	{
		pr_emerg("==========================================================\n");
		pr_emerg("WARNING: detecting lockup during boot! forced panic......\n");
		pr_emerg("==========================================================\n");

		//panic("detecting lockup during boot!\n");
		gen_wdt_bite(NULL,NULL);
	}
}

static void lge_init_boot_lockup_detect(void)
{

	if( !strcmp(CONFIG_LOCALVERSION,"-perf") )
		boot_deadline = 120 * 1000;
	else
		boot_deadline = 240 * 1000;

	pr_info("%s boot_partition:%s boot_mode:%d fota:%d\n", __func__,
			lge_get_boot_partition(), lge_get_boot_mode(), lge_get_fota_mode());

	INIT_DELAYED_WORK(&lge_boot_lockup_detect_work, lge_boot_lockup_detect_func);

	if( strncmp(lge_get_boot_partition(),"boot",strlen("boot")) == 0
			&& lge_get_boot_mode() == LGE_BOOT_MODE_NORMAL
			&& lge_get_fota_mode() == 0) {
		boot_lockup_detect_working = LOCKUP_WQ_STARTED;
		pr_info("start boot_lockup_detect_work after %lds\n", (unsigned long)boot_deadline/1000);
		queue_delayed_work(system_highpri_wq, &lge_boot_lockup_detect_work, msecs_to_jiffies(boot_deadline));
	}
}

/* These sysfs node should be called by only init.rc files for mutual exclusion*/
static int cancel_boot_lockup_detect(const char *val, struct kernel_param *kp)
{
    if( boot_lockup_detect_working == LOCKUP_WQ_STARTED) {
		boot_lockup_detect_working = LOCKUP_WQ_CANCELED;
		pr_info("cancel boot_lockup_detect_work\n");
		cancel_delayed_work_sync(&lge_boot_lockup_detect_work);
	}

	return 0;
}
module_param_call(cancel_boot_lockup_detect, cancel_boot_lockup_detect, NULL, NULL, S_IWUSR);

static int pause_boot_lockup_detect(const char *val, struct kernel_param *kp)
{
	if (!strcmp(val, "1") && boot_lockup_detect_working == LOCKUP_WQ_STARTED) {
		boot_lockup_detect_working = LOCKUP_WQ_PAUSED;
		pr_info("pause boot_lockup_detect_work\n");
		cancel_delayed_work_sync(&lge_boot_lockup_detect_work);
	} else if (!strcmp(val, "0") && boot_lockup_detect_working == LOCKUP_WQ_PAUSED) {
		boot_lockup_detect_working = LOCKUP_WQ_STARTED;
		pr_info("restart boot_lockup_detect_work after %lds\n", (unsigned long)boot_deadline/1000);
		queue_delayed_work(system_highpri_wq, &lge_boot_lockup_detect_work, msecs_to_jiffies(boot_deadline));
    }

    return 0;
}
module_param_call(pause_boot_lockup_detect, pause_boot_lockup_detect, NULL, NULL, S_IWUSR);
#endif

#define REBOOT_DEADLINE msecs_to_jiffies(30 * 1000)

static struct delayed_work lge_panic_reboot_work;

static void lge_panic_reboot_work_func(struct work_struct *work)
{

    pr_emerg("==========================================================\n");
	pr_emerg("WARNING: detecting lockup during reboot! forcing panic....\n");
	pr_emerg("==========================================================\n");

	BUG();
}

static int lge_panic_reboot_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (lge_get_download_mode() != 1)
		return NOTIFY_DONE;

	INIT_DELAYED_WORK(&lge_panic_reboot_work, lge_panic_reboot_work_func);
	queue_delayed_work(system_highpri_wq, &lge_panic_reboot_work,
		   REBOOT_DEADLINE);
	return NOTIFY_DONE;
}

static struct notifier_block lge_panic_reboot_notifier = {
	lge_panic_reboot_handler,
	NULL,
	0
};

static int __init lge_panic_handler_early_init(void)
{
	struct device_node *np;
	int ret = 0;

	panic_handler = kzalloc(sizeof(*panic_handler), GFP_KERNEL);
	if (!panic_handler) {
		pr_err("could not allocate memory for panic_handler\n");
		return -ENOMEM;
	}

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem");
	if (!np) {
		pr_err("unable to find DT imem node\n");
		return -ENODEV;
	}

	msm_imem_base = of_iomap(np, 0);
	if (!msm_imem_base) {
		pr_err("unable to map imem\n");
		return -ENOMEM;
	}

	/* check struct boot_shared_imem_cookie_type is matched */
	if (readl_relaxed(CRASH_HANDLER_MAGIC) != CRASH_HANDLER_MAGIC_NUM) {
		pr_err("Check sbl's struct boot_shared_imem_cookie_type.\n"
			"Need to update lge_handle_panic's imem offset.\n");
	}

	/* Set default restart_reason to Unknown reset. */
	lge_set_restart_reason(LGE_RB_MAGIC | LGE_ERR_TZ);

	np = of_find_compatible_node(NULL, NULL, "crash_fb");
	if (!np) {
		pr_err("unable to find crash_fb node\n");
		return -ENODEV;
	}

	of_property_read_u32(np, "mem-addr", (u32*)&panic_handler->fb_addr);
	of_property_read_u32(np, "mem-size", (u32*)&panic_handler->fb_size);

	pr_info("%s: reserved[@0x%lx+@0x%lx)\n", PANIC_HANDLER_NAME,
			panic_handler->fb_addr, panic_handler->fb_size);

	lge_set_fb_addr(panic_handler->fb_addr);

#ifdef CONFIG_LGE_BOOT_LOCKUP_DETECT
	lge_init_boot_lockup_detect();
#endif

	/* register reboot notifier for detecting reboot lockup */
	ret = register_reboot_notifier(&lge_panic_reboot_notifier);
	if (ret) {
		pr_err("%s: Failed to register reboot notifier\n", __func__);
		return ret;
	}

	return 0;
}

early_initcall(lge_panic_handler_early_init);

static int __init lge_panic_handler_probe(struct platform_device *pdev)
{
	int ret = 0;

	return ret;
}

static int lge_panic_handler_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver panic_handler_driver __refdata = {
	.probe = lge_panic_handler_probe,
	.remove = lge_panic_handler_remove,
	.driver = {
		.name = PANIC_HANDLER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lge_panic_handler_init(void)
{
	return platform_driver_register(&panic_handler_driver);
}

static void __exit lge_panic_handler_exit(void)
{
	platform_driver_unregister(&panic_handler_driver);
}

module_init(lge_panic_handler_init);
module_exit(lge_panic_handler_exit);

MODULE_DESCRIPTION("LGE panic handler driver");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
