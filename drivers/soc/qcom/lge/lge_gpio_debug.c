/*
 * arch/arm/mach-msm/lge/lge_gpio_debug.c
 *
 * Copyright (C) 2014 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/spmi.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

/* MSM GPIOs */
#define GPIO_CONFIG(tlmm, gpio)        (tlmm->base + 0x0 + (0x1000 * (gpio)))
#define GPIO_IN_OUT(tlmm, gpio)        (tlmm->base + 0x4 + (0x1000 * (gpio)))
#define GPIO_INTR_CFG(tlmm, gpio)      (tlmm->base + 0x8 + (0x1000 * (gpio)))
#define GPIO_INTR_STATUS(tlmm, gpio)   (tlmm->base + 0xc + (0x1000 * (gpio)))

static char *pull[] = {"NO_PULL", "PULL_DOWN", "KEEPER", "PULL_UP"};

/* PMIC GPIOs */
#define PMIC_GPIO_REG            0xC004
#define PMIC_GPIO_MODE(gpio)     (PMIC_GPIO_REG + 0x3C + (0x100 * (gpio-1)))
#define PMIC_GPIO_INT(gpio)      (PMIC_GPIO_REG + 0x11 + (0x100 * (gpio-1)))

static char *gpio_mode[] = {"IN", "OUT", "IN/OUT", "Reserved"};
static char *gpio_pull[] = {"PULL_UP_30uA", "PULL_UP_1.5uA", "PULL_UP_31.5uA",
	"PULL_UP_1.5uA+30uA", "PULL_DOWN_10uA", "NO_PULL",
	"Reserved", "Reserved"};
static char *gpio_out[] = {"CMOS", "NMOS", "PMOS", "N/A"};
static char *gpio_drv[] = {"Reserved", "Low", "Medium", "High"};

/* PMIC MPPs */
#define PMIC_MPP_REG             0xA004
#define PMIC_MPP_MODE(gpio)      (PMIC_MPP_REG + 0x3C + (0x100 * (gpio-1)))
#define PMIC_MPP_INT(gpio)       (PMIC_MPP_REG + 0x11 + (0x100 * (gpio-1)))

static char *mpp_mode[] = {"D_IN", "D_OUT", "D_IN/OUT",
	"Bidirection", "A_IN", "A_OUT", "Current Sink", "Reserved"};
static char *mpp_pull[] = {"0.6kohm", "10 kohm", "30 kohm", "Open"};

static DEFINE_SPINLOCK(gpios_lock);
static struct dentry *debugfs_base;
struct dentry *tlmm_dentry;
static u32 debug_suspend;

struct lge_gpio_debug_data {
	unsigned int n_msm_gpio;
	unsigned int n_pm_gpio;
	unsigned int n_pm_mpp;
	void __iomem *base;
	struct device *dev;
};

static struct lge_gpio_debug_data *dbgdata;

/* GPIO values */
enum gpio_cfg_type {
	DEFAULT              = 0,
	INPUT_PULLDN         = 1,
	INPUT_PULLUP         = 2,
	INPUT_NOPULL         = 3,
	OUTPUT_PULLDN_LOW    = 4,
	OUTPUT_PULLUP_LOW    = 5,
	OUTPUT_NOPULL_LOW    = 6,
	OUTPUT_PULLDN_HIGH   = 7,
	OUTPUT_PULLUP_HIGH   = 8,
	OUTPUT_NOPULL_HIGH   = 9,
	TYPE_OVERFLOW        = 10,
};

static char *gpio_cfg_str[] = {
	"DEFAULT",
	"[DIR] INPUT,  [PULL] PULLDOWN",
	"[DIR] INPUT,  [PULL] PULLUP",
	"[DIR] INPUT,  [PULL] NOPULL",
	"[DIR] OUTPUT, [PULL] PULLDOWN, [VAL] LOW",
	"[DIR] OUTPUT, [PULL] PULLUP,   [VAL] LOW",
	"[DIR] OUTPUT, [PULL] NOPULL,   [VAL] LOW",
	"[DIR] OUTPUT, [PULL] PULLDOWN, [VAL] HIGH",
	"[DIR] OUTPUT, [PULL] PULLUP,   [VAL] HIGH",
	"[DIR] OUTPUT, [PULL] NOPULL,   [VAL] HIGH",
	"TYPE_OVERFLOW",
};

static int gpio_user_sel = -1;

void gpio_debug_print(void)
{
	unsigned cfg;
	unsigned out;
	unsigned intr;
	unsigned char d[6];
	int i = 0;
	struct spmi_controller *ctrl = spmi_busnum_to_ctrl(0);

	pr_cont("\nMSM GPIOs:\n");
	for (i = 0; i < dbgdata->n_msm_gpio; i++) {
		cfg = __raw_readl(GPIO_CONFIG(dbgdata, i));
		out = __raw_readl(GPIO_IN_OUT(dbgdata, i));
		intr = __raw_readl(GPIO_INTR_STATUS(dbgdata, i));

		pr_cont("GPIO[%-3d]: [FS] 0x%-3x  [DIR] %-5s  "
				"[PULL] %-12s  [DRV] %4dmA",
				i, (cfg&0x3C)>>2,
				((cfg&0x200)>>9) ? "OUT" : "IN",
				pull[(cfg&0x3)],
				(((cfg&0x1C0)>>6)<<1)+2);

		if ((cfg&0x200)>>9)
			pr_cont("  [VAL] %s", ((out>>1)&0x1) ? "HIGH" : "LOW");

		if (intr&0x1)
			pr_cont("  [INT] HIGH");
		pr_cont("\n");
	}

	pr_cont("\nPMIC GPIOs:\n");
	for (i = 1; i < dbgdata->n_pm_gpio+1; i++) {
		spmi_ext_register_readl(ctrl, 0, PMIC_GPIO_MODE(i), d, 6);

		pr_cont("GPIO[%-3d]: [DIR] %-5s  [PULL] %-20s  "
				"[OUT] %-8s  [DRV] %-8s",
				i, gpio_mode[(d[0]&0x70)>>4],
				gpio_pull[d[2]&0x7],
				gpio_out[d[5]&0x30>>4],
				gpio_drv[d[5]&0x3]);

		spmi_ext_register_readl(ctrl, 0, PMIC_GPIO_INT(i), d, 1);

		if (d[0])
			pr_cont("  [INT] Enable");
		pr_cont("\n");
	}

	pr_cont("\nPMIC MPPs:\n");
	for (i = 1; i < dbgdata->n_pm_mpp+1; i++) {
		spmi_ext_register_readl(ctrl, 0, PMIC_MPP_MODE(i), d, 3);

		pr_cont("MPP [%-3d]: [DIR] %-15s  [PULL] %-10s",
					i, mpp_mode[(d[0]&0x70)>>4],
					mpp_pull[d[2]&0x7]);

		spmi_ext_register_readl(ctrl, 0, PMIC_MPP_INT(i), d, 1);

		if (d[0])
			pr_cont("  [INT] Enable");
		pr_cont("\n");
	}

	return;
}

void gpio_debug_print_sel(int selection)
{
	unsigned cfg;
	unsigned out;
	unsigned intr;

	printk("gpio_user_sel = %d\n", selection);

	cfg = __raw_readl(GPIO_CONFIG(dbgdata, selection));
	out = __raw_readl(GPIO_IN_OUT(dbgdata, selection));
	intr = __raw_readl(GPIO_INTR_STATUS(dbgdata, selection));

	pr_cont("GPIO[%-3d]: [FS] 0x%-3x  [DIR] %-5s  "
			"[PULL] %-12s  [DRV] %4dmA",
			selection, (cfg&0x3C)>>2,
			((cfg&0x200)>>9) ? "OUT" : "IN",
			pull[(cfg&0x3)], (((cfg&0x1C0)>>6)<<1)+2);

	if ((cfg&0x200)>>9) pr_cont("  [VAL] %s",
			((out>>1)&0x1) ? "HIGH" : "LOW");
	if (intr&0x1) pr_cont("  [INT] HIGH");
	pr_cont("\n");
}

void gpio_configuration_set(int gpio, int setting)
{
	unsigned cfg;
	unsigned out;
	unsigned intr;

	printk("\nMSM GPIO : %-3d is changed to %s\n\n",
			gpio, gpio_cfg_str[setting]);

	cfg = __raw_readl(GPIO_CONFIG(dbgdata, gpio));
	out = __raw_readl(GPIO_IN_OUT(dbgdata, gpio));
	intr = __raw_readl(GPIO_INTR_STATUS(dbgdata, gpio));

	pr_cont("S : GPIO[%-3d]: [FS] 0x%-3x  [DIR] %-5s  "
			"[PULL] %-12s  [DRV] %4dmA",
			gpio, (cfg&0x3C)>>2,
			((cfg&0x200)>>9) ? "OUT" : "IN",
			pull[(cfg&0x3)],
			(((cfg&0x1C0)>>6)<<1)+2);

	if ((cfg&0x200)>>9) pr_cont("  [VAL] %s",
			((out>>1)&0x1) ? "HIGH" : "LOW");
	if (intr&0x1) pr_cont("  [INT] HIGH");
	pr_cont("\n");

	switch (setting) {
	case INPUT_PULLDN:
		cfg &= ~(0x200);
		cfg &= ~(0x3);
		cfg |= 0x1;
		break;
	case INPUT_PULLUP:
		cfg &= ~(0x200);
		cfg &= ~(0x3);
		cfg |= 0x3;
		break;
	case INPUT_NOPULL:
		cfg &= ~(0x200);
		cfg &= ~(0x3);
		cfg |= 0x0;
		break;
	case OUTPUT_PULLDN_LOW:
		cfg &= ~(0x200);
		cfg |= 0x200;
		cfg &= ~(0x3);
		cfg |= 0x1;
		out &= ~(0x2);
		out |= 0x0;
		break;
	case OUTPUT_PULLUP_LOW:
		cfg &= ~(0x200);
		cfg |= 0x200;
		cfg &= ~(0x3);
		cfg |= 0x3;
		out &= ~(0x2);
		out |= 0x0;
		break;
	case OUTPUT_NOPULL_LOW:
		cfg &= ~(0x200);
		cfg |= 0x200;
		cfg &= ~(0x3);
		cfg |= 0x0;
		out &= ~(0x2);
		out |= 0x0;
		break;
	case OUTPUT_PULLDN_HIGH:
		cfg &= ~(0x200);
		cfg |= 0x200;
		cfg &= ~(0x3);
		cfg |= 0x1;
		out &= ~(0x2);
		out |= 0x2;
		break;
	case OUTPUT_PULLUP_HIGH:
		cfg &= ~(0x200);
		cfg |= 0x200;
		cfg &= ~(0x3);
		cfg |= 0x3;
		out &= ~(0x2);
		out |= 0x2;
		break;
	case OUTPUT_NOPULL_HIGH:
		cfg &= ~(0x200);
		cfg |= 0x200;
		cfg &= ~(0x3);
		cfg |= 0x0;
		out &= ~(0x2);
		out |= 0x2;
		break;
	default :
		printk("default by something wrong\n");
		return;
	}

	__raw_writel(cfg, GPIO_CONFIG(dbgdata, gpio));
	udelay(350);

	__raw_writel(out, GPIO_IN_OUT(dbgdata, gpio));
	udelay(350);

	cfg = __raw_readl(GPIO_CONFIG(dbgdata, gpio));
	out = __raw_readl(GPIO_IN_OUT(dbgdata, gpio));
	intr = __raw_readl(GPIO_INTR_STATUS(dbgdata, gpio));

	pr_cont("E : GPIO[%-3d]: [FS] 0x%-3x  [DIR] %-5s  "
			"[PULL] %-12s  [DRV] %4dmA",
			gpio, (cfg&0x3C)>>2,
			((cfg&0x200)>>9) ? "OUT" : "IN",
			pull[(cfg&0x3)],
			(((cfg&0x1C0)>>6)<<1)+2);

	if ((cfg&0x200)>>9) pr_cont("  [VAL] %s",
			((out>>1)&0x1) ? "HIGH" : "LOW");
	if (intr&0x1) pr_cont("  [INT] HIGH");
	pr_cont("\n");
}

/* sysfs : /d/gpios/address */
static int gpio_address_set(void *data, u64 val)
{
	unsigned long flags;
	int gpio = 0;

	spin_lock_irqsave(&gpios_lock, flags);
	gpio = (int)val;

	if (gpio != -1) {
		if (gpio < 0 || gpio >= dbgdata->n_msm_gpio) {
			printk("Invalid GPIO, %d\n", gpio);
			spin_unlock_irqrestore(&gpios_lock, flags);
			return 0;
		}
		gpio_user_sel = gpio;
		pr_cont("Set MSM GPIO : %d\n", gpio_user_sel);
	} else {
		printk("Invalid GPIO, init\n");
	}

	spin_unlock_irqrestore(&gpios_lock, flags);
	return 0;
}

static int gpio_address_get(void *data, u64 *val)
{
	unsigned long flags;

	spin_lock_irqsave(&gpios_lock, flags);
	if (gpio_user_sel != -1)
		pr_cont("Current MSM GPIO : %d\n", gpio_user_sel);
	else
		pr_cont("GPIO is not set\n");

	spin_unlock_irqrestore(&gpios_lock, flags);
	return 0;
}

/* sysfs : /d/gpios/data */
static int gpio_data_set(void *data, u64 val)
{
	unsigned long flags;
	int cfg = 0;

	spin_lock_irqsave(&gpios_lock, flags);
	cfg = (int)val;

	if (cfg >= TYPE_OVERFLOW || cfg <= DEFAULT) {
		pr_cont("Invalid Configuration\n");
		spin_unlock_irqrestore(&gpios_lock, flags);
		return 0;
	}

	if (gpio_user_sel != -1)
		gpio_configuration_set(gpio_user_sel, cfg);
	else
		printk("Invalid GPIO, init\n");

	spin_unlock_irqrestore(&gpios_lock, flags);
	return 0;
}

static int gpio_data_get(void *data, u64 *val)
{
	unsigned long flags;

	spin_lock_irqsave(&gpios_lock, flags);

	if (gpio_user_sel < 0)
		pr_cont("Invalid GPIO, init\n");
	else
		gpio_debug_print_sel(gpio_user_sel);
	spin_unlock_irqrestore(&gpios_lock, flags);
	return 0;
}

/* sysfs : /d/gpios/data_all */
static int gpio_all_get(void *data, u64 *val)
{
	unsigned long flags;

	spin_lock_irqsave(&gpios_lock, flags);

	gpio_debug_print();

	spin_unlock_irqrestore(&gpios_lock, flags);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(gpio_address_fops,
		gpio_address_get, gpio_address_set, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(gpio_data_fops, gpio_data_get,
		gpio_data_set, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(gpio_all_fops, gpio_all_get,
		NULL, "0x%llx\n");

static int gpio_debug_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct device *dev = &pdev->dev;
	const struct device_node *node = pdev->dev.of_node;

	if (!pdev->dev.of_node)
		return -ENODEV;
	dbgdata = devm_kzalloc(dev, sizeof(struct lge_gpio_debug_data),
			GFP_KERNEL);
	if (!dbgdata)
		return -EIO;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tlmm-base");
	if (!res)
		goto err;

	dbgdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!dbgdata->base)
		goto err;

	ret = of_property_read_u32(node, "lge,n-msm-gpio",
			&dbgdata->n_msm_gpio);
	if (ret)
		dbgdata->n_msm_gpio = 0;

	ret = of_property_read_u32(node, "lge,n-pm-gpio",
			&dbgdata->n_pm_gpio);
	if (ret)
		dbgdata->n_pm_gpio = 0;

	ret = of_property_read_u32(node, "lge,n-pm-mpp",
			&dbgdata->n_pm_mpp);
	if (ret)
		dbgdata->n_pm_mpp = 0;

	platform_set_drvdata(pdev, dbgdata);

	debugfs_base = debugfs_create_dir("gpios", NULL);

	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_u32("debug_suspend", S_IRUGO | S_IWUSR,
				debugfs_base, &debug_suspend)) {
		debugfs_remove_recursive(debugfs_base);
		return -ENOMEM;
	}

	debugfs_create_file("address", 0600, debugfs_base,
			NULL, &gpio_address_fops);
	debugfs_create_file("data", 0600, debugfs_base,
			NULL, &gpio_data_fops);
	debugfs_create_file("data_all", 0600, debugfs_base,
			NULL, &gpio_all_fops);

	pr_err("LGE GPIO debugger init done\n");
	return 0;
err:
	return -ENODEV;
}

static int gpio_debug_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id gpio_debug_match[] = {
	{ .compatible = "lge,gpio-debug"},
	{}
};

static struct platform_driver gpio_debug_driver = {
	.probe          = gpio_debug_probe,
	.remove         = gpio_debug_remove,
	.driver         = {
		.name   = "lge-gpio-debug",
		.owner  = THIS_MODULE,
		.of_match_table = gpio_debug_match,
	},
};

static int __init gpio_debug_init(void)
{
	return platform_driver_register(&gpio_debug_driver);
}
module_init(gpio_debug_init);

static void __exit gpio_debug_exit(void)
{
	platform_driver_unregister(&gpio_debug_driver);
}
module_exit(gpio_debug_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPIOs dynamic debugger driver");
