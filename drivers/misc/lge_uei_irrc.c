/*
UEI_IRRC_DRIVER_FOR_APQ8084
*/

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#define UEI_IRRC_NAME "uei_irrc"

struct uei_irrc_pdata_type {
	int reset_gpio;
	struct pinctrl *uart_pins;
};

struct uei_irrc_pdata_type irrc_data;

#ifdef CONFIG_OF
static int irrc_parse_dt(struct device *dev, struct uei_irrc_pdata_type *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->reset_gpio = of_get_named_gpio(np, "uei,reset-gpio", 0);
	printk(KERN_INFO "IrRC: uei,reset-gpio: %d\n", pdata->reset_gpio);

	return 0;
}
#endif

static int uei_irrc_probe(struct platform_device *pdev)
{
	struct pinctrl_state *set_state;
	int rc = 0;

	if (pdev->dev.of_node) {
		irrc_parse_dt(&pdev->dev, &irrc_data);
	}

	/* Get pinctrl if target uses pinctrl */
	irrc_data.uart_pins = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(irrc_data.uart_pins)) {
		dev_info(&pdev->dev, "IrRC: can't get pinctrl state\n");
		irrc_data.uart_pins = NULL;
	}

	if (irrc_data.uart_pins) {
		set_state = pinctrl_lookup_state(irrc_data.uart_pins, "ir_active");
		if (IS_ERR(set_state))
			dev_err(&pdev->dev, "IrRC: can't get pinctrl active state\n");
		else
			pinctrl_select_state(irrc_data.uart_pins, set_state);
	}

	if ((!gpio_is_valid(irrc_data.reset_gpio))) {
		printk(KERN_INFO "IrRC: IrRC reset_gpio is invalid\n");
		return 1;
	}

	rc = gpio_request(irrc_data.reset_gpio, "irrc_reset_n");
	if (rc) {
		printk(KERN_ERR "%s: irrc_reset_n %d request failed\n",
				__func__, irrc_data.reset_gpio);
		return rc;
	}

	rc = gpio_direction_output(irrc_data.reset_gpio, 0);
	if (rc) {
		printk(KERN_ERR "%s: gpio_direction %d config is failed\n",
				__func__, irrc_data.reset_gpio);
		return rc;
	}

	gpio_set_value(irrc_data.reset_gpio, 1);
	return rc;
}

static int uei_irrc_remove(struct platform_device *pdev)
{
	struct uei_irrc_pdata_type *pdata = platform_get_drvdata(pdev);

	gpio_free(irrc_data.reset_gpio);
	if (irrc_data.uart_pins)
		devm_pinctrl_put(irrc_data.uart_pins);
	pdata = NULL;
	return 0;
}

static void uei_irrc_shutdown(struct platform_device *pdev)
{
	return;
}

static int uei_irrc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pinctrl_state *set_state;

	if (irrc_data.uart_pins) {
		set_state = pinctrl_lookup_state(irrc_data.uart_pins, "ir_sleep");
		if (IS_ERR(set_state))
			dev_err(&pdev->dev, "IrRC: can't get pinctrl sleep state\n");
		else
			pinctrl_select_state(irrc_data.uart_pins, set_state);
	}
	gpio_set_value(irrc_data.reset_gpio, 0);
	dev_info(&pdev->dev, "IrRC: uei suspend\n");

	return 0;
}

static int uei_irrc_resume(struct platform_device *pdev)
{
	struct pinctrl_state *set_state;

	if (irrc_data.uart_pins) {
		set_state = pinctrl_lookup_state(irrc_data.uart_pins, "ir_active");
		if (IS_ERR(set_state))
			dev_err(&pdev->dev, "IrRC: can't get pinctrl active state\n");
		else
			pinctrl_select_state(irrc_data.uart_pins, set_state);
	}
	gpio_set_value(irrc_data.reset_gpio, 1);
	dev_info(&pdev->dev, "IrRC: uei resume\n");

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id irrc_match_table[] = {
	{ .compatible = "uei,irrc",},
	{ },
};
#endif

static struct platform_driver uei_irrc_driver = {
	.probe = uei_irrc_probe,
	.remove = uei_irrc_remove,
	.shutdown = uei_irrc_shutdown,
	.suspend = uei_irrc_suspend,
	.resume = uei_irrc_resume,
	.driver = {
		.name = UEI_IRRC_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = irrc_match_table,
#endif
	},
};

static int __init uei_irrc_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&uei_irrc_driver);

	if (ret) {
		printk(KERN_INFO "%s: init fail\n", __func__);
	}

	return ret;
}

static void __exit uei_irrc_exit(void)
{
	platform_driver_unregister(&uei_irrc_driver);
}

module_init(uei_irrc_init);
module_exit(uei_irrc_exit);

MODULE_AUTHOR("LG Electronics");
MODULE_DESCRIPTION("UEI IrRC Driver");
MODULE_LICENSE("GPL");
