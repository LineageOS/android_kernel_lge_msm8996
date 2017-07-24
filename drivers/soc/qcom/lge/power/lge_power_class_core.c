/*
 *  Universal power supply monitor class
 *
 *  Copyright (C) 2014, Daeho Choi <daeho.choi@lge.com>
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <soc/qcom/lge/power/lge_power_class.h>
#include <linux/thermal.h>
#include "lge_power.h"

/* exported for the APM Power driver, APM emulation */
struct class *lge_power_class;
EXPORT_SYMBOL_GPL(lge_power_class);

static struct device_type lge_power_dev_type;

static int lge_power_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct lge_power *lpc = dev_get_drvdata(dev);

	return strcmp(lpc->name, name) == 0;
}

struct lge_power *lge_power_get_by_name(const char *name)
{
	struct device *dev = class_find_device(lge_power_class,
			NULL, name,	lge_power_match_device_by_name);

	return dev ? dev_get_drvdata(dev) : NULL;
}
EXPORT_SYMBOL_GPL(lge_power_get_by_name);

static bool __lge_power_is_supplied_by(struct lge_power *supplier,
			struct lge_power *supply)
{
	int i;

	if (!supply->lge_supplied_from && !supplier->lge_supplied_to)
		return false;

	/* Support both supplied_to and supplied_from modes */
	if (supply->lge_supplied_from) {
		if (!supplier->name)
			return false;
		for (i = 0; i < supply->num_lge_supplies; i++)
			if (!strcmp(supplier->name, supply->lge_supplied_from[i]))
				return true;
	}

	if (supplier->lge_supplied_to) {
		if (!supply->name)
			return false;
		for (i = 0; i < supplier->num_lge_supplicants; i++)
			if (!strcmp(supplier->lge_supplied_to[i], supply->name))
				return true;
	}

	return false;
}

static int __lge_power_changed_work(struct device *dev, void *data)
{
	struct lge_power *lpc = (struct lge_power *)data;
	struct lge_power *lpt = dev_get_drvdata(dev);

	if (__lge_power_is_supplied_by(lpc, lpt)) {
		if (lpt->external_lge_power_changed)
			lpt->external_lge_power_changed(lpt);
	}

	return 0;
}
static bool
__power_supply_is_supplied_by_for_lge_power(struct lge_power *supplier,
			struct power_supply *supply)
{
	int i;

	if (!supply->lge_power_supplied_from && !supplier->supplied_to)
		return false;

	/* Support both supplied_to and supplied_from modes */
	if (supply->lge_power_supplied_from) {
		if (!supplier->name)
			return false;
		for (i = 0; i < supply->num_lge_power_supplies; i++)
			if (!strcmp(supplier->name,
				supply->lge_power_supplied_from[i]))
				return true;
	}

	if (supplier->supplied_to) {
		if (!supply->name)
			return false;
		for (i = 0; i < supplier->num_supplicants; i++)
			if (!strcmp(supplier->supplied_to[i], supply->name))
				return true;
	}

	return false;
}


static bool
__power_supply_is_supplied_by_for_lge_power_psy(struct lge_power *supplier,
			struct power_supply *supply)
{
	int i;

	if (!supply->lge_psy_power_supplied_from &&
			!supplier->lge_psy_supplied_to)
		return false;

	/* Support both supplied_to and supplied_from modes */
	if (supply->lge_psy_power_supplied_from) {
		if (!supplier->name)
			return false;
		for (i = 0; i < supply->num_lge_psy_power_supplies; i++)
			if (!strcmp(supplier->name,
				supply->lge_psy_power_supplied_from[i]))
				return true;
	}

	if (supplier->lge_psy_supplied_to) {
		if (!supply->name)
			return false;
		for (i = 0; i < supplier->num_lge_psy_supplicants; i++)
			if (!strcmp(supplier->lge_psy_supplied_to[i], supply->name))
				return true;
	}

	return false;
}

static int
__lge_power_changed_for_power_supply_work(struct device *dev, void *data)
{
	struct lge_power *psy = (struct lge_power *)data;
	struct power_supply *pst = dev_get_drvdata(dev);

	if (__power_supply_is_supplied_by_for_lge_power(psy, pst)) {
		if (pst->external_lge_power_changed)
			pst->external_lge_power_changed(pst);
	}

	if (__power_supply_is_supplied_by_for_lge_power_psy(psy, pst)) {
		if (pst->external_power_changed)
			pst->external_power_changed(pst);
	}

	return 0;
}

static void lge_power_changed_work(struct work_struct *work)
{
	unsigned long flags;
	struct lge_power *lpc = container_of(work, struct lge_power, changed_work);

	dev_dbg(lpc->dev, "%s\n", __func__);
	pr_info("lge_power_changed_work\n");
	spin_lock_irqsave(&lpc->changed_lock, flags);
	if (lpc->changed) {
		lpc->changed = false;
		spin_unlock_irqrestore(&lpc->changed_lock, flags);

		class_for_each_device(lge_power_class, NULL, lpc,
				      __lge_power_changed_work);
		class_for_each_device(power_supply_class, NULL, lpc,
				      __lge_power_changed_for_power_supply_work);
		kobject_uevent(&lpc->dev->kobj, KOBJ_CHANGE);
		spin_lock_irqsave(&lpc->changed_lock, flags);
	}

	if (!lpc->changed)
		pm_relax(lpc->dev);
	spin_unlock_irqrestore(&lpc->changed_lock, flags);
}

void lge_power_changed(struct lge_power *lpc)
{
	unsigned long flags;

	dev_dbg(lpc->dev, "%s\n", __func__);

	spin_lock_irqsave(&lpc->changed_lock, flags);
	lpc->changed = true;
	pm_stay_awake(lpc->dev);
	spin_unlock_irqrestore(&lpc->changed_lock, flags);

	schedule_work(&lpc->changed_work);
}
EXPORT_SYMBOL_GPL(lge_power_changed);

static void lge_power_dev_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}

int lge_power_register(struct device *parent, struct lge_power *lpc)
{
	struct device *dev;
	int rc;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	device_initialize(dev);

	dev->class = lge_power_class;
	dev->type = &lge_power_dev_type;
	dev->parent = parent;
	dev->release = lge_power_dev_release;
	dev_set_drvdata(dev, lpc);
	lpc->dev = dev;
	INIT_WORK(&lpc->changed_work, lge_power_changed_work);

	rc = kobject_set_name(&dev->kobj, "%s", lpc->name);
	if (rc)
		goto kobject_set_name_failed;

	rc = device_add(dev);
	if (rc)
		goto device_add_failed;

	spin_lock_init(&lpc->changed_lock);

	rc = device_init_wakeup(dev, true);
	if (rc)
		goto wakeup_init_failed;

	lge_power_changed(lpc);

	goto success;

wakeup_init_failed:
	device_del(dev);

kobject_set_name_failed:

device_add_failed:
	put_device(dev);

success:

	return rc;
}
EXPORT_SYMBOL_GPL(lge_power_register);

void lge_power_unregister(struct lge_power *lpc)
{
	sysfs_remove_link(&lpc->dev->kobj, "powers");

	device_unregister(lpc->dev);
}
EXPORT_SYMBOL_GPL(lge_power_unregister);

static int __init lge_power_class_init(void)
{
	lge_power_class = class_create(THIS_MODULE, "lge_power");
	pr_err("[LGE_POWER] lge power!!!\n");
	if (IS_ERR(lge_power_class))
		return PTR_ERR(lge_power_class);
	lge_power_class->dev_uevent = lge_power_uevent;
	lge_power_init_attrs(&lge_power_dev_type);

	return 0;
}

static void __exit lge_power_class_exit(void)
{
	class_destroy(lge_power_class);
}

subsys_initcall(lge_power_class_init);
module_exit(lge_power_class_exit);

MODULE_DESCRIPTION("LGE power monitor class");
MODULE_AUTHOR("Daeho Choi <daeho.choi@lge.com>");
MODULE_LICENSE("GPL");
