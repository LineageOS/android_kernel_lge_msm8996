/*
 *  Functions private to lge power class
 *
 *  Copyright (C) 2014, Daeho Choi <daeho.choi@lge.com>
 *
 *  You may use this code as per GPL version 2
 */

/* BTM status */
enum lge_power_btm_states {
	LGE_BTM_HEALTH_GOOD,
	LGE_BTM_HEALTH_OVERHEAT,
	LGE_BTM_HEALTH_COLD,
};


extern void lge_power_init_attrs(struct device_type *dev_type);
extern int lge_power_uevent(struct device *dev, struct kobj_uevent_env *env);
