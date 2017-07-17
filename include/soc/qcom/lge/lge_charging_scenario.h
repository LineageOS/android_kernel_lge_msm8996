/*
 * LGE charging scenario Header file.
 *
 * Copyright (C) 2013 LG Electronics
 * mansu.lee <mansu.lee@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LGE_CHARGING_SCENARIO_H_
#define __LGE_CHARGING_SCENARIO_H_

#include <linux/kernel.h>

#define CONFIG_LGE_THERMALE_CHG_CONTROL
#define CONFIG_LGE_PSEUDO_CHG_UI

#define DC_IUSB_VOLTUV   4000000
#define DC_IUSB_CURRENT  450
#define DC_CURRENT_DEF   -1

#define HIGH_VBAT_THREHSOLD	3980000 // 4000 mV - float voltage margin 20 mV

#define DECCUR_FLOAT_VOLTAGE	4000 // Warm & Cool float voltage 4000 mV
#define DECCUR_CHG_CURRENT_DIVIDER	3 // Warm & Cool chg current 0.3C

/* Battery temperature states */

#ifdef CONFIG_LGE_PM_OTP_SCENARIO_FOR_SPRINT
enum lge_battemp_states {
	CHG_BATTEMP_BL_UT,
	CHG_BATTEMP_0_3,
	CHG_BATTEMP_3_10,
	CHG_BATTEMP_10_12,
	CHG_BATTEMP_12_42,
	CHG_BATTEMP_42_45,
	CHG_BATTEMP_45_50,
	CHG_BATTEMP_50_OT,
	CHG_BATTEMP_AB_OT,
};
#else
enum lge_battemp_states {
	CHG_BATTEMP_BL_UT,
	CHG_BATTEMP_0_3,
	CHG_BATTEMP_3_10,
	CHG_BATTEMP_10_12,
	CHG_BATTEMP_12_43,
	CHG_BATTEMP_43_45,
	CHG_BATTEMP_45_52,
	CHG_BATTEMP_52_OT,
	CHG_BATTEMP_AB_OT,
};
#endif

/* LGE charging states */
enum lge_charging_states {
	CHG_BATT_NORMAL_STATE,
	CHG_BATT_DECCUR_STATE,
	CHG_BATT_WARNIG_STATE,
	CHG_BATT_STPCHG_STATE,
};

/* LGE charging states change */
enum lge_states_changes {
	STS_CHE_NONE,
	STS_CHE_NORMAL_TO_DECCUR,
	STS_CHE_NORMAL_TO_STPCHG,
	STS_CHE_DECCUR_TO_NORMAL,
	STS_CHE_DECCUR_TO_STPCHG,
	STS_CHE_STPCHG_TO_NORMAL,
	STS_CHE_STPCHG_TO_DECCUR,
};

/* BTM status */
enum lge_btm_states {
	BTM_HEALTH_GOOD,
	BTM_HEALTH_OVERHEAT,
	BTM_HEALTH_COLD,
};

struct charging_info {
	int     batt_volt;
	int     batt_temp;
	int     is_charger;
	int     current_now;
	int		max_chg_volt;
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	int     chg_current_ma;
	int     chg_current_te;
#endif
	bool	is_charger_changed;
};

struct charging_rsp {
	enum lge_charging_states    state;
	enum lge_states_changes     change_lvl;
	bool                        force_update;
	bool                        disable_chg;
	int				chg_current;
	int				float_voltage;
	enum lge_btm_states         btm_state;
	int                         pseudo_chg_ui;
};

struct batt_temp_table {
	int                         min;
	int                         max;
	enum lge_battemp_states     battemp_state;
};

extern void
lge_monitor_batt_temp(struct charging_info req, struct charging_rsp *res);
#endif
/* __LGE_CHARGING_SCENARIO_H_ */

