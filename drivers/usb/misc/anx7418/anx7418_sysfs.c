#include <linux/of_gpio.h>

#include "anx7418.h"
#include "anx7418_sysfs.h"

static ssize_t show_sbu_sel(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx7418 *anx = dev_get_drvdata(dev);
	int sbu_sel_gpio;
	
	sbu_sel_gpio = gpio_get_value(anx->sbu_sel_gpio);
	dev_info(dev, "read anx->sbu_sel_gpio:%d\n", sbu_sel_gpio);

	return sprintf(buf, "%d\n", sbu_sel_gpio); 
}
static ssize_t store_sbu_sel(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct anx7418 *anx = dev_get_drvdata(dev);
	int temp = simple_strtoul(buf, NULL, 10);

	if (temp)
		gpio_set_value(anx->sbu_sel_gpio, 1);
	else
		gpio_set_value(anx->sbu_sel_gpio, 0);

	dev_info(dev, "set anx->sbu_sel_gpio: (%d)\n", temp);
	return count;
}
static DEVICE_ATTR(sbu_sel, S_IRUGO | S_IWUSR | S_IWGRP, show_sbu_sel, store_sbu_sel);

static ssize_t show_sbu2(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx7418 *anx = dev_get_drvdata(dev);
	int sbu2_gpio;
	
	sbu2_gpio = gpio_get_value(anx->ext_acc_en_gpio);
	dev_info(dev, "read anx->sbu2_gpio:%d\n", sbu2_gpio);

	return sprintf(buf, "%d\n", sbu2_gpio); 
}
static ssize_t store_sbu2(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct anx7418 *anx = dev_get_drvdata(dev);
	int temp = simple_strtoul(buf, NULL, 10);

	if (anx->friends != LGE_ALICE_FRIENDS_CM) {
		dev_info(dev, "do not ext-acc_en pin write without alice-cm\n");
		return count;
	}

	if (temp)
		gpio_set_value(anx->ext_acc_en_gpio, 1);
	else
		gpio_set_value(anx->ext_acc_en_gpio, 0);

	dev_info(dev, "set anx->sbu2_gpio: (%d)\n", temp);
	return count;
}
static DEVICE_ATTR(sbu2, S_IRUGO | S_IWUSR | S_IWGRP, show_sbu2, store_sbu2);

int anx7418_sysfs_init(struct anx7418 *anx)
{
	int retval = 0;
	struct i2c_client *client = anx->client;

	retval = device_create_file(&client->dev, &dev_attr_sbu2);
	if (retval)
		goto err_sbu2;

	retval = device_create_file(&client->dev, &dev_attr_sbu_sel);
	if (retval)
		goto err_sbu_sel;

	return 0;
err_sbu_sel:
err_sbu2:
	device_remove_file(&client->dev, &dev_attr_sbu2);
	return -retval;
}
