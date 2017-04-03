/*
 * earjack debugger trigger
 *
 * Copyright (C) 2012 LGE, Inc.
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

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>

#include <soc/qcom/lge/board_lge.h>

static struct workqueue_struct *earjack_dbg_wq;

#if defined (CONFIG_LGE_TOUCH_CORE)
void touch_notify_earjack(int value);
#endif

struct earjack_debugger_device {
	int gpio;
#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
	int irq;
#endif
	int saved_detect;
	int (*set_uart_console)(int enable);
	int (*force_off)(void);
	struct delayed_work	detect_work;
};

struct earjack_debugger_platform_data {
	int gpio_trigger;
};

static int earjack_debugger_detected(void *dev)
{
	struct earjack_debugger_device *adev = dev;
	/* earjack debugger detecting by gpio 77 is changed
	 * from
	 *  G4: rev.A <= rev
	 *  Z2: rev.B <= rev
	 * as like
	 *  low  => uart enable
	 *  high => uart disable
	 */

	return !gpio_get_value(adev->gpio);
}

static void earjack_debugger_set_uart(struct work_struct *work)
{
	struct earjack_debugger_device *adev;
	int detect;

	printk(KERN_INFO "Calling %s\n", __func__);

	adev = container_of(work, struct earjack_debugger_device, detect_work.work);

	if (adev->force_off != NULL) {
		adev->force_off();
		adev->force_off = NULL;
		return;
	}

	detect = earjack_debugger_detected(adev);

	if (detect) {
		pr_debug("%s() : in!!\n", __func__);
		printk(KERN_INFO "%s() : in!!\n", __func__);
		adev->set_uart_console(
			lge_uart_console_should_enable_on_earjack_debugger());
	} else {
		/* restore uart console status to default mode */
		pr_debug("%s() : out!!\n", __func__);
		printk(KERN_INFO "%s() : out!!\n", __func__);
		adev->set_uart_console(
				lge_uart_console_should_enable_on_default());
	}
#if defined (CONFIG_LGE_TOUCH_CORE)
	if (detect)
		touch_notify_earjack(2);
	else

		touch_notify_earjack(0);
#endif
}

#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
static irqreturn_t earjack_debugger_irq_handler(int irq, void *_dev)
{
	struct earjack_debugger_device *adev = _dev;

	adev->force_off = NULL;
	cancel_delayed_work(&adev->detect_work);
	queue_delayed_work(earjack_dbg_wq, &adev->detect_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}
#endif

static void earjack_debugger_parse_dt(struct device *dev,
		struct earjack_debugger_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->gpio_trigger = of_get_named_gpio_flags(np, "serial,irq-gpio",
			0, NULL);
}

static int earjack_debugger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct earjack_debugger_device *adev;
	struct earjack_debugger_platform_data *pdata;

	lge_uart_console_set_config(UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER);

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct earjack_debugger_platform_data),
				GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: no pdata\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		earjack_debugger_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENOMEM;
	}

	adev = kzalloc(sizeof(struct earjack_debugger_device), GFP_KERNEL);
	if (!adev) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	earjack_dbg_wq = create_singlethread_workqueue("earjack_dbg_wq");
	if (!earjack_dbg_wq) {
		kfree(adev);
		pr_err("%s: no workqueue\n", __func__);
		return -ENOMEM;
	}

	adev->gpio = pdata->gpio_trigger;
#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
	adev->irq = gpio_to_irq(pdata->gpio_trigger);
#endif
	adev->set_uart_console = msm_serial_set_uart_console;
	adev->force_off = NULL;
	INIT_DELAYED_WORK(&adev->detect_work, earjack_debugger_set_uart);

	platform_set_drvdata(pdev, adev);

	ret = gpio_request_one(adev->gpio, GPIOF_IN,
			"gpio_earjack_debugger");
	if (ret < 0) {
		pr_err("%s: failed to request gpio %d\n", __func__,
				adev->gpio);
		goto err_gpio_request;
	}

#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
	ret = request_threaded_irq(adev->irq, NULL,
			earjack_debugger_irq_handler,
			IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT,
			"earjack_debugger_trigger", adev);
	if (ret < 0) {
		pr_err("%s: failed to request irq\n", __func__);
		goto err_request_irq;
	}
#endif

	if (earjack_debugger_detected(adev)) {
		pr_info("[UART CONSOLE][%s] %s uart console\n",
			__func__,
			lge_uart_console_should_enable_on_earjack_debugger() ?
				"enable" : "disable");
		adev->set_uart_console(
			lge_uart_console_should_enable_on_earjack_debugger());
	} else {
		pr_info("[UART CONSOLE][%s] %s uart console\n",
			__func__,"disable");
		adev->force_off = msm_serial_force_off;
		queue_delayed_work(earjack_dbg_wq, &adev->detect_work, msecs_to_jiffies(30000));
	}

	pr_info("earjack debugger probed\n");
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE)
	gpio_free(adev->gpio);
#endif
	return ret;

#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
err_request_irq:
	gpio_free(adev->gpio);
#endif
err_gpio_request:
	kfree(adev);
	destroy_workqueue(earjack_dbg_wq);

	return ret;
}

static int earjack_debugger_remove(struct platform_device *pdev)
{
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);
#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
	free_irq(adev->irq, adev);
#endif
	gpio_free(adev->gpio);
	kfree(adev);
	destroy_workqueue(earjack_dbg_wq);

	return 0;
}

#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
static void earjack_debugger_shutdown(struct platform_device *pdev)
{
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);
	disable_irq(adev->irq);
}

static int earjack_debugger_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	disable_irq(adev->irq);

	return 0;
}

static int earjack_debugger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	enable_irq(adev->irq);

	return 0;
}

static const struct dev_pm_ops earjack_debugger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(earjack_debugger_suspend,
			earjack_debugger_resume)
};
#endif

#ifdef CONFIG_OF
static struct of_device_id earjack_debugger_match_table[] = {
	{ .compatible = "serial,earjack-debugger", },
	{ },
};
#endif

static struct platform_driver earjack_debugger_driver = {
	.probe = earjack_debugger_probe,
	.remove = earjack_debugger_remove,
#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
	.shutdown = earjack_debugger_shutdown,
#endif
	.driver = {
		.name = "earjack-debugger",
#if !defined (CONFIG_MACH_MSM8996_ELSA) && !defined (CONFIG_MACH_MSM8996_LUCYE)
		.pm = &earjack_debugger_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = earjack_debugger_match_table,
#endif
	},
};

static int __init earjack_debugger_init(void)
{
	return platform_driver_register(&earjack_debugger_driver);
}

static void __exit earjack_debugger_exit(void)
{
	platform_driver_unregister(&earjack_debugger_driver);
}

module_init(earjack_debugger_init);
module_exit(earjack_debugger_exit);
