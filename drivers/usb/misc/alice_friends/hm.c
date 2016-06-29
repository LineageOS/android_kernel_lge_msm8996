#include <linux/fs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "hm.h"

void hm_earjack_changed(struct hm_instance *hm, bool earjack)
{
	char *attach[2] = { "EARJACK_STATE=ATTACH", NULL };
	char *detach[2] = { "EARJACK_STATE=DETACH", NULL };

	kobject_uevent_env(&hm->dev->kobj, KOBJ_CHANGE,
			earjack ? attach : detach);
	hm->earjack = earjack;
}
EXPORT_SYMBOL(hm_earjack_changed);

static ssize_t show_earjack(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hm_instance *hm = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", hm->earjack);
}
static DEVICE_ATTR(earjack, S_IRUGO, show_earjack, NULL);

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

	return hm;

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
	misc_deregister(&hm->mdev);
	kfree(hm);
}

static void devm_hm_release(struct device *dev, void *res)
{
	struct hm_instance **hm = res;

	hm_instance_unregister(*hm);
}

struct hm_instance *__must_check
devm_hm_instance_register(struct device *parent)
{
	struct hm_instance **ptr, *hm;

	ptr = devres_alloc(devm_hm_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	hm = __hm_register(parent);
	if (IS_ERR(hm)) {
		devres_free(ptr);
	} else {
		*ptr = hm;
		devres_add(parent, ptr);
	}
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
