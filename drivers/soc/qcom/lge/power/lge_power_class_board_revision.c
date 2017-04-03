/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "[LGE-HW_REV] %s : " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <soc/qcom/lge/power/lge_board_revision.h>
#include <soc/qcom/lge/power/lge_power_class.h>
#include <asm/system_misc.h>


#define MODULE_NAME "lge_hw_revision"

static enum hw_rev_no lge_bd_rev = HW_REV_MAX;

/* CAUTION: These strings are come from LK. */
#if defined(CONFIG_MACH_MSM8996_LUCYE)
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_02", "rev_03", "rev_04",
	"rev_a", "rev_b", "rev_c", "rev_d", "rev_10", "rev_11", "rev_12", "rev_13",
	"reserved"};
#elif defined(CONFIG_MACH_MSM8996_ELSA)
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_02", "rev_a", "rev_b",
	"rev_c", "rev_d", "rev_e", "rev_f", "rev_10", "rev_11", "rev_12", "rev_13",
	"reserved"};
#else
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_01", "rev_f", "rev_b", "rev_c",
	"rev_d", "rev_e", "rev_a", "rev_g", "rev_10", "rev_11", "rev_12",
	"reserved"};
#endif

enum hw_rev_no lge_get_board_rev_no(void)
{
	return lge_bd_rev;
}

char *lge_get_board_revision(void)
{
	char *name;
	name = rev_str[lge_bd_rev];
	return name;
}

struct lge_hw_rev{
	struct lge_power lge_hw_rev_lpc;
};

static enum lge_power_property lge_power_lge_hw_rev_properties[] = {
	LGE_POWER_PROP_HW_REV,
	LGE_POWER_PROP_HW_REV_NO,
};

static int lge_power_lge_hw_rev_get_property(struct lge_power *lpc,
			enum lge_power_property lpp,
			union lge_power_propval *val)
{
	int ret_val = 0;

	switch (lpp) {
	case LGE_POWER_PROP_HW_REV:
		val->strval = lge_get_board_revision();
		break;

	case LGE_POWER_PROP_HW_REV_NO:
		val->intval = lge_get_board_rev_no();
		break;

	default:
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}

static int lge_hw_rev_probe(struct platform_device *pdev)
{
	struct lge_hw_rev *hw_rev;
	struct lge_power *lge_power_hw_rev;
	int ret;
	pr_info("Probe Start~!!\n");
	hw_rev = kzalloc(sizeof(struct lge_hw_rev), GFP_KERNEL);

	if(!hw_rev){
		pr_err("hw_rev memory allocation failed.\n");
		return -ENOMEM;
	}

	pr_info("HW_REV : %s\n", lge_get_board_revision());

	lge_power_hw_rev = &hw_rev->lge_hw_rev_lpc;

	lge_power_hw_rev->name = "lge_hw_rev";

	lge_power_hw_rev->properties = lge_power_lge_hw_rev_properties;
	lge_power_hw_rev->num_properties
		= ARRAY_SIZE(lge_power_lge_hw_rev_properties);
	lge_power_hw_rev->get_property
		= lge_power_lge_hw_rev_get_property;


	ret = lge_power_register(&pdev->dev, lge_power_hw_rev);
	if (ret < 0) {
		pr_err("Failed to register lge power class: %d\n",
			ret);
		goto err_free;
	}

	pr_err("Probe done~!!\n");

	return 0;
err_free:
	kfree(hw_rev);
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id lge_hw_rev_match_table[] = {
	{.compatible = "lge,hw_rev"},
	{},
};
#endif
static int lge_hw_rev_remove(struct platform_device *pdev)
{
	struct lge_hw_rev *hw_rev = platform_get_drvdata(pdev);

	lge_power_unregister(&hw_rev->lge_hw_rev_lpc);

	platform_set_drvdata(pdev, NULL);
	kfree(hw_rev);
	return 0;
}

static struct platform_driver lge_hw_rev_driver = {
	.probe = lge_hw_rev_probe,
	.remove = lge_hw_rev_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lge_hw_rev_match_table,
#endif
	},
};

static int __init lge_hw_rev_init(void)
{
	return platform_driver_register(&lge_hw_rev_driver);
}

static void lge_hw_rev_exit(void)
{
	platform_driver_unregister(&lge_hw_rev_driver);
}
module_init(lge_hw_rev_init);
module_exit(lge_hw_rev_exit);

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
	pr_info("[LGE-HW-REV] Rev.%s\n", rev_str[lge_bd_rev]);

	return 1;
}
__setup("lge.rev=", board_revno_setup);

MODULE_DESCRIPTION("LGE power monitor class");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Board rev driver");
