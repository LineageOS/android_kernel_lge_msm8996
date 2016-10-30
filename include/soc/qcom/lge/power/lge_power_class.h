/*
 *  Universal power supply monitor class
 *
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#ifndef __LINUX_LGE_POWER_CLASS_H__
#define __LINUX_LGE_POWER_CLASS_H__

#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/types.h>
#include <linux/power_supply.h>
struct device;

/*
 * All voltages, currents, charges, energies, time and temperatures in uV,
 * µA, µAh, µWh, seconds and tenths of degree Celsius unless otherwise
 * stated. It's driver's job to convert its raw values to units in which
 * this class operates.
 */

/*
 * For systems where the charger determines the maximum battery capacity
 * the min and max fields should be used to present these values to user
 * space. Unused/unknown fields will not appear in sysfs.
 */

enum lge_power_property {
	/* Properties of type `int' */
	LGE_POWER_PROP_STATUS = 0,
	LGE_POWER_PROP_HEALTH,
	LGE_POWER_PROP_PRESENT,
	LGE_POWER_PROP_CHARGING_ENABLED,
	LGE_POWER_PROP_CURRENT_MAX,
	LGE_POWER_PROP_INPUT_CURRENT_MAX,
	LGE_POWER_PROP_TEMP,
	LGE_POWER_PROP_TYPE,
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
	LGE_POWER_PROP_PSEUDO_BATT,
	LGE_POWER_PROPS_PSEUDO_BATT_MODE,
	LGE_POWER_PROPS_PSEUDO_BATT_ID,
	LGE_POWER_PROPS_PSEUDO_BATT_THERM,
	LGE_POWER_PROPS_PSEUDO_BATT_TEMP,
	LGE_POWER_PROPS_PSEUDO_BATT_VOLT,
	LGE_POWER_PROPS_PSEUDO_BATT_CAPACITY,
	LGE_POWER_PROPS_PSEUDO_BATT_CHARGING,
#endif
#if defined (CONFIG_LGE_PM_BATTERY_ID_CHECKER) \
		|| defined (CONFIG_LGE_PM_LGE_POWER_CLASS_BATTERY_ID_CHECKER)
	LGE_POWER_PROP_BATTERY_ID_CHECKER,
	LGE_POWER_PROP_VALID_BATT,
	LGE_POWER_PROP_CHECK_BATT_ID_FOR_AAT,
#endif
#ifdef CONFIG_LGE_PM
	LGE_POWER_PROP_SAFETY_TIMER,
#endif
#if defined (CONFIG_LGE_PM_CHARGING_BQ24296_CHARGER)
	LGE_POWER_PROP_EXT_PWR_CHECK,
	LGE_POWER_PROP_BAT_REMOVED,
#elif defined (CONFIG_LGE_PM_CHARGING_BQ24262_CHARGER)
	LGE_POWER_PROP_EXT_PWR_CHECK,
#endif
#ifdef CONFIG_LGE_PM_CHARGING_VZW_POWER_REQ
	LGE_POWER_PROP_VZW_CHG,
#endif

#if defined (CONFIG_LGE_PM_CHARGING_BQ24296_CHARGER) \
	|| defined (CONFIG_LGE_PM_CHARGING_BQ24262_CHARGER)
	LGE_POWER_PROP_SAFETY_CHARGER_TIMER,
	LGE_POWER_PROP_CHARGING_COMPLETE,
#endif
#ifdef CONFIG_LGE_PM_BATTERY_EXTERNAL_FUELGAUGE
	LGE_POWER_PROP_USE_FUELGAUGE,
#endif
#ifdef CONFIG_CHG_DETECTOR_MAX14656
	LGE_POWER_PROP_USB_CHG_DETECT_DONE,
	LGE_POWER_PROP_USB_CHG_TYPE,
	LGE_POWER_PROP_USB_DCD_TIMEOUT,
#endif
#if defined(CONFIG_LGE_PM_LLK_MODE)
	LGE_POWER_PROP_STORE_DEMO_ENABLED,
#endif
	LGE_POWER_PROP_HW_REV,
	LGE_POWER_PROP_HW_REV_NO,
#ifdef CONFIG_LGE_PM
	LGE_POWER_PROP_CALCULATED_SOC,
#endif
	LGE_POWER_PROP_CHG_TEMP_STATUS,
	LGE_POWER_PROP_CHG_TEMP_CURRENT_STATUS,
	LGE_POWER_PROP_BATT_TEMP_STATUS,
	LGE_POWER_PROP_TEST_CHG_SCENARIO,
	LGE_POWER_PROP_TEST_BATT_THERM_VALUE,
	LGE_POWER_PROP_IS_FACTORY_CABLE,
	LGE_POWER_PROP_IS_FACTORY_MODE_BOOT,
	LGE_POWER_PROP_CABLE_TYPE,
	LGE_POWER_PROP_CABLE_TYPE_BOOT,
	LGE_POWER_PROP_USB_CURRENT,
	LGE_POWER_PROP_TA_CURRENT,
	LGE_POWER_PROP_IBAT_CURRENT,
	LGE_POWER_PROP_UPDATE_CABLE_INFO,
	LGE_POWER_PROP_XO_THERM_PHY,
	LGE_POWER_PROP_XO_THERM_RAW,
	LGE_POWER_PROP_BATT_THERM_PHY,
	LGE_POWER_PROP_BATT_THERM_RAW,
	LGE_POWER_PROP_USB_ID_PHY,
	LGE_POWER_PROP_USB_ID_RAW,
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
	LGE_POWER_PROP_HVDCP_PRESENT,
	LGE_POWER_PROP_TYPE_USB_HVDCP,
	LGE_POWER_PROP_TYPE_USB_HVDCP_3,
#endif
	LGE_POWER_PROP_PA0_THERM_PHY,
	LGE_POWER_PROP_PA0_THERM_RAW,
	LGE_POWER_PROP_PA1_THERM_PHY,
	LGE_POWER_PROP_PA1_THERM_RAW,
	LGE_POWER_PROP_BD1_THERM_PHY,
	LGE_POWER_PROP_BD1_THERM_RAW,
	LGE_POWER_PROP_BD2_THERM_PHY,
	LGE_POWER_PROP_BD2_THERM_RAW,
#if defined(CONFIG_LGE_PM_CHARGING_SUPPORT_PHIHONG)
	LGE_POWER_PROP_CHECK_PHIHONG,
#endif
	LGE_POWER_PROP_PSEUDO_BATT_UI,
	LGE_POWER_PROP_BTM_STATE,
	LGE_POWER_PROP_OTP_CURRENT,
	LGE_POWER_PROP_IS_CHG_LIMIT,
	LGE_POWER_PROP_CHG_PRESENT,
	LGE_POWER_PROP_FLOATED_CHARGER,
	LGE_POWER_PROP_VZW_CHG,
	LGE_POWER_PROP_BATT_PACK_NAME,
	LGE_POWER_PROP_BATT_CAPACITY,
	LGE_POWER_PROP_BATT_CELL,
	LGE_POWER_PROP_CHARGING_USB_ENABLED,
	LGE_POWER_PROP_CHARGING_CURRENT_MAX,
	LGE_POWER_PROP_BATT_INFO,
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX_MODE
	LGE_POWER_PROP_USB_CURRENT_MAX_MODE,
#endif
	LGE_POWER_PROP_QC_IBAT_CURRENT,
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TDMB_MODE
	LGE_POWER_PROP_TDMB_MODE_ON,
#endif
	LGE_POWER_PROP_CHARGE_DONE,
	LGE_POWER_PROP_VOLTAGE_NOW,
};
enum lge_power_type {
	LGE_POWER_TYPE_UNKNOWN = 0,
};

union lge_power_propval {
	int intval;
	const char *strval;
	int64_t int64val;
};

struct covert_psp_lpp_type {
	enum lge_power_property dest;
	enum power_supply_property src;
};

struct lge_power {
	const char *name;
	enum lge_power_type type;
	enum lge_power_property *properties;
	size_t num_properties;

	enum lge_power_property *uevent_properties;
	size_t num_uevent_properties;

	char **supplied_to;
	size_t num_supplicants;

	char **supplied_from;
	size_t num_supplies;

	char **lge_supplied_to;
	size_t num_lge_supplicants;

	char **lge_supplied_from;
	size_t num_lge_supplies;

	char **lge_psy_supplied_to;
	size_t num_lge_psy_supplicants;

#ifdef CONFIG_OF
	struct device_node *of_node;
#endif

	int (*get_property)(struct lge_power *psy,
			    enum lge_power_property psp,
			    union lge_power_propval *val);
	int (*set_property)(struct lge_power *psy,
			    enum lge_power_property psp,
			    const union lge_power_propval *val);

	int (*property_is_writeable)(struct lge_power *psy,
				     enum lge_power_property psp);
	void (*external_power_changed)(struct lge_power *psy);
	void (*external_lge_power_changed)(struct lge_power *psy);
	void (*set_charged)(struct lge_power *psy);
	int (*check_convert_lpp)(enum power_supply_property psp);
	enum lge_power_property
			(*convert_lpp_from_psp)(enum power_supply_property psp);

	/* For APM emulation, think legacy userspace. */
	int use_for_apm;

	/* private */
	struct device *dev;
	struct work_struct changed_work;
	spinlock_t changed_lock;
	bool changed;
	void (*external_power_changed_with_psy)(struct lge_power *lpc,
				struct power_supply *psy);
};

/*
 * This is recommended structure to specify static power supply parameters.
 * Generic one, parametrizable for different power supplies. Power supply
 * class itself does not use it, but that's what implementing most platform
 * drivers, should try reuse for consistency.
 */

struct lge_power_info {
	const char *name;
	int technology;
	int voltage_max_design;
	int voltage_min_design;
	int charge_full_design;
	int charge_empty_design;
	int energy_full_design;
	int energy_empty_design;
	int use_for_apm;
};

/* For APM emulation, think legacy userspace. */
extern struct class *lge_power_class;
extern void lge_power_unregister(struct lge_power *psy);
extern int
lge_power_register(struct device *parent, struct lge_power *psy);
extern struct lge_power *lge_power_get_by_name(const char *name);
extern void lge_power_changed(struct lge_power *lpc);

#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_TYPE_HVDCP
extern int lgcc_get_effective_fcc_result(void);
#endif

#endif /* __LINUX_LGE_POWER_CLASS_H__ */
