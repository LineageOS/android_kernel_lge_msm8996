#include <linux/fs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "hm.h"

struct hm_instance *_hm = NULL;

static void hm_reset_work(struct work_struct *w)
{
	struct hm_instance *hm = container_of(w,
			struct hm_instance, reset_work);

	hm->desc->reset(hm);
}

int alice_friends_hm_reset(void)
{
	if (!_hm)
		return 0;

	schedule_work(&_hm->reset_work);
	return 1;
}
EXPORT_SYMBOL(alice_friends_hm_reset);

static void hm_earjack_changed_work(struct work_struct *w)
{
	struct hm_instance *hm = container_of(w,
			struct hm_instance, changed_work.work);

	char *attach[2] = { "EARJACK_STATE=ATTACH", NULL };
	char *detach[2] = { "EARJACK_STATE=DETACH", NULL };
	char **uevent_envp = NULL;

	if (hm->earjack == hm->uevent_earjack)
		return;

	uevent_envp = hm->earjack ? attach : detach;
	dev_info(hm->dev, "%s: %s\n", __func__, uevent_envp[0]);

	kobject_uevent_env(&hm->dev->kobj, KOBJ_CHANGE, uevent_envp);
	hm->uevent_earjack = hm->earjack;
}

void hm_earjack_changed(struct hm_instance *hm, bool earjack)
{
	hm->earjack = earjack;

	cancel_delayed_work(&hm->changed_work);
	schedule_delayed_work(&hm->changed_work,
			msecs_to_jiffies(800));
}
EXPORT_SYMBOL(hm_earjack_changed);

static ssize_t show_earjack(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hm_instance *hm = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", hm->earjack);
}
static DEVICE_ATTR(earjack, S_IRUGO, show_earjack, NULL);

static ssize_t store_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct hm_instance *hm = dev_get_drvdata(dev);

	hm->desc->reset(hm);

	return size;
}
static DEVICE_ATTR(reset, S_IWUSR, NULL, store_reset);

static struct hm_instance *__must_check __hm_register(struct device *parent)
{
	struct miscdevice *mdev;
	struct hm_instance *hm;
	int rc;

	hm = kzalloc(sizeof(*hm), GFP_KERNEL);
	if (!hm)
		return ERR_PTR(-ENOMEM);

	mdev = &hm->mdev;
	
	mdev->minor = MISC_DYNAMIC_MINOR;
	mdev->name = "alice_friends_hm";
	mdev->parent = parent;

	rc = misc_register(mdev);
	if (rc)
		goto misc_register_failed;
	hm->dev = mdev->this_device;
	dev_set_drvdata(hm->dev, hm);

	rc = device_init_wakeup(hm->dev, true);
	if (rc)
		goto wakeup_init_failed;

	rc = device_create_file(hm->dev, &dev_attr_earjack);
	if (rc)
		goto create_earjack_file_failed;

	rc = device_create_file(hm->dev, &dev_attr_reset);
	if (rc)
		goto create_reset_file_failed;

	hm->uevent_earjack = -1;
	INIT_DELAYED_WORK(&hm->changed_work, hm_earjack_changed_work);
	INIT_WORK(&hm->reset_work, hm_reset_work);

	return hm;

create_reset_file_failed:
	device_remove_file(hm->dev, &dev_attr_earjack);
create_earjack_file_failed:
	device_init_wakeup(hm->dev, false);
wakeup_init_failed:
	misc_deregister(mdev);
misc_register_failed:
	kfree(hm);

	return ERR_PTR(rc);
}

static void hm_instance_unregister(struct hm_instance *hm)
{
	device_init_wakeup(hm->dev, false);
	device_remove_file(hm->dev, &dev_attr_earjack);
	device_remove_file(hm->dev, &dev_attr_reset);
	misc_deregister(&hm->mdev);
	kfree(hm);
	_hm = NULL;
}

static void devm_hm_release(struct device *dev, void *res)
{
	struct hm_instance **hm = res;

	hm_instance_unregister(*hm);
}

struct hm_instance *__must_check
devm_hm_instance_register(struct device *parent, struct hm_desc *desc)
{
	struct hm_instance **ptr, *hm;

	ptr = devres_alloc(devm_hm_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	hm = __hm_register(parent);
	hm->desc = desc;
	if (IS_ERR(hm)) {
		devres_free(ptr);
	} else {
		*ptr = hm;
		devres_add(parent, ptr);
	}
	_hm = hm;
	return hm;
}
EXPORT_SYMBOL(devm_hm_instance_register);

static int devm_hm_match(struct device *dev, void *res, void *data)
{
	struct hm_instance **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

void devm_hm_instance_unregister(struct device *dev, struct hm_instance *hm)
{
	int rc;
	rc = devres_release(dev, devm_hm_release, devm_hm_match, hm);
	WARN_ON(rc);
}
EXPORT_SYMBOL(devm_hm_instance_unregister);
