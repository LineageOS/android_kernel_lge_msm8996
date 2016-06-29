#ifndef __ALICE_FRIENDS_HM__
#define __ALICE_FRIENDS_HM__

#include <linux/miscdevice.h>
#include <linux/workqueue.h>

struct hm_instance {
	void *drv_data;

	struct miscdevice mdev;
	struct device *dev;

	bool earjack;
};

struct hm_instance *__must_check
devm_hm_instance_register(struct device *parent);
void devm_hm_instance_unregister(struct device *dev, struct hm_instance *hm);
void hm_earjack_changed(struct hm_instance *hm, bool earjack);
#endif /* __ALICE_FRIENDS_HM__ */
