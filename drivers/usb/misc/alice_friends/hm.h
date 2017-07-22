#ifndef __ALICE_FRIENDS_HM__
#define __ALICE_FRIENDS_HM__

#include <linux/miscdevice.h>
#include <linux/workqueue.h>

struct hm_instance;

struct hm_desc {
	int (*reset)(struct hm_instance *hm);
};

struct hm_instance {
	void *drv_data;
	const struct hm_desc *desc;

	struct miscdevice mdev;
	struct device *dev;

	bool earjack;
	int uevent_earjack;

	struct delayed_work changed_work;
	struct work_struct reset_work;
};

struct hm_instance *__must_check
devm_hm_instance_register(struct device *parent, struct hm_desc *desc);
void devm_hm_instance_unregister(struct device *dev, struct hm_instance *hm);
void hm_earjack_changed(struct hm_instance *hm, bool earjack);
#endif /* __ALICE_FRIENDS_HM__ */
