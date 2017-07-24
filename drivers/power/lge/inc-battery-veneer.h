#ifndef _INC_BATTERY_VENEER_H_
#define _INC_BATTERY_VENEER_H_

#include <linux/power_supply.h>

enum battery_health_psy {
	BATTERY_HEALTH_UNKNOWN = POWER_SUPPLY_HEALTH_UNKNOWN,

	BATTERY_HEALTH_COLD = POWER_SUPPLY_HEALTH_COLD,
	BATTERY_HEALTH_GOOD = POWER_SUPPLY_HEALTH_GOOD,
	BATTERY_HEALTH_OVERHEAT = POWER_SUPPLY_HEALTH_OVERHEAT,
};

inline static enum battery_health_psy battery_health_parse(int intval){
	if (intval == BATTERY_HEALTH_COLD || intval == BATTERY_HEALTH_GOOD ||
		intval == BATTERY_HEALTH_OVERHEAT) {
		return intval;
	}
	else
		return BATTERY_HEALTH_UNKNOWN;
}

#endif
