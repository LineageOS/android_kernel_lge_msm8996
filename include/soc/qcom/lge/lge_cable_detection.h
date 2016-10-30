#ifndef __LGE_CABLE_DETECTION_H
#define __LGE_CABLE_DETECTION_H

#include <linux/power_supply.h>

#define LT_CABLE_56K     6
#define LT_CABLE_130K    7
#define LT_CABLE_910K    11

enum acc_cable_type {
	NO_INIT_CABLE = 0,
	CABLE_MHL_1K,
	CABLE_U_28P7K,
	CABLE_28P7K,
	CABLE_56K,
	CABLE_100K,
	CABLE_130K,
	CABLE_180K,
	CABLE_200K,
	CABLE_220K,
	CABLE_270K,
	CABLE_330K,
	CABLE_620K,
	CABLE_910K,
	CABLE_NONE
};

struct chg_cable_info {
	enum acc_cable_type cable_type;
	unsigned ta_ma;
	unsigned usb_ma;
};

void get_cable_data_from_dt(void *of_node);

struct qpnp_vadc_chip;
int lge_pm_get_cable_info(struct qpnp_vadc_chip *, struct chg_cable_info *);
void lge_pm_read_cable_info(struct qpnp_vadc_chip *);
enum acc_cable_type lge_pm_get_cable_type(void);
unsigned lge_pm_get_ta_current(void);
unsigned lge_pm_get_usb_current(void);
int lge_smem_cable_type(void);

#ifdef CONFIG_LGE_PM_FACTORY_CABLE
bool lge_is_factory_cable(void);
#else
inline bool lge_is_factory_cable(void) { return false; }
#endif

#ifdef CONFIG_LGE_DOCK
void check_dock_connected(enum power_supply_type type);
#endif

#endif
