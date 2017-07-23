/*
 *  Copyright (C) 2012 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17050_BATTERY_H_
#define __MAX17050_BATTERY_H_

#define MAX17050_STATUS_BATTABSENT (1 << 3)
#define MAX17050_BATTERY_FULL	100
#define MAX17050_DEFAULT_SNS_RESISTOR	10000

#define CONFIG_LGE_PM_MAX17050_POLLING

/* #define MAX17050_DEBUG */
/* Voltage Base */
/* #define CONFIG_LGE_PM_MAX17050_SOC_VF */
/* Current Base */
#define CONFIG_LGE_PM_MAX17050_SOC_REP

/* Number of words in model characterisation data */
#define MODEL_SIZE	32

/* Recharging for PPLUS max17050 */
#ifdef CONFIG_MACH_MSM8992_PPLUS
#define CONFIG_LGE_PM_MAX17050_RECHARGING
#endif

struct max17050_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);

#ifdef CONFIG_LGE_PM
	bool enable_current_sense;
	bool ext_batt_psy;
	int empty_soc;
	int full_soc;

	/*
	 * R_sns in micro-ohms.
	 * default 10000 (if r_sns = 0) as it is the recommended value by
	 * the datasheet although it can be changed by board designers.
	 */
	unsigned int r_sns;

	int config;
	int filtercfg;
	int relaxcfg;
	int learncfg;
	int misccfg;
	int fullsocthr;
	int iavg_empty;

	int rcomp0;
	int tempco;
	int tempnom;
	int ichgterm;
	int tgain;
	int toff;

	int vempty;
	int qrtable00;
	int qrtable10;
	int qrtable20;
	int qrtable30;

	int capacity;
	int vf_fullcap;

	int param_version;
	int full_design;

	int rescale_soc;
	int rescale_factor;

	/* model characterisation data */
	u8 model_80[MODEL_SIZE];
	u8 model_90[MODEL_SIZE];
	u8 model_A0[MODEL_SIZE];
#endif
};

#define MAX17050_STATUS				0x00
#define MAX17050_V_ALRT_THRESHOLD	0x01
#define MAX17050_T_ALRT_THRESHOLD	0x02
#define MAX17050_SOC_ALRT_THRESHOLD	0x03
#define MAX17050_AT_RATE			0x04
#define MAX17050_REM_CAP_REP		0x05
#define MAX17050_SOC_REP			0x06
#define MAX17050_AGE				0x07
#define MAX17050_TEMPERATURE		0x08
#define MAX17050_V_CELL				0x09
#define MAX17050_CURRENT			0x0A
#define MAX17050_AVERAGE_CURRENT	0x0B
#define MAX17050_SOC_MIX			0x0D
#define MAX17050_SOC_AV				0x0E
#define MAX17050_REM_CAP_MIX		0x0F
#define MAX17050_FULL_CAP			0x10
#define MAX17050_TTE				0x11
#define MAX17050_Q_RESIDUAL_00		0x12
#define MAX17050_FULL_SOC_THR		0x13
#define MAX17050_AVERAGE_TEMP		0x16
#define MAX17050_CYCLES				0x17
#define MAX17050_DESIGN_CAP			0x18
#define MAX17050_AVERAGE_V_CELL		0x19
#define MAX17050_MAX_MIN_TEMP		0x1A
#define MAX17050_MAX_MIN_VOLTAGE	0x1B
#define MAX17050_MAX_MIN_CURRENT	0x1C
#define MAX17050_CONFIG				0x1D
#define MAX17050_I_CHG_TERM			0x1E
#define MAX17050_REM_CAP_AV			0x1F
#define MAX17050_CUSTOMVER			0x20
#define MAX17050_VERSION			0x21
#define MAX17050_Q_RESIDUAL_10		0x22
#define MAX17050_FULL_CAP_NOM		0x23
#define MAX17050_TEMP_NOM			0x24
#define MAX17050_TEMP_LIM			0x25
#define MAX17050_AIN				0x27
#define MAX17050_LEARN_CFG			0x28
#define MAX17050_FILTER_CFG			0x29
#define MAX17050_RELAX_CFG			0x2A
#define MAX17050_MISC_CFG			0x2B
#define MAX17050_T_GAIN				0x2C
#define MAX17050_T_OFF				0x2D
#define MAX17050_C_GAIN				0x2E
#define MAX17050_C_OFF				0x2F
#define MAX17050_Q_RESIDUAL_20		0x32
#define MAX17050_FULLCAP0			0x35
#define MAX17050_I_AVG_EMPTY		0x36
#define MAX17050_F_CTC				0x37
#define MAX17050_RCOMP_0			0x38
#define MAX17050_TEMP_CO			0x39
#define MAX17050_V_EMPTY			0x3A
#define MAX17050_F_STAT				0x3D
#define MAX17050_TIMER				0x3E
#define MAX17050_SHDN_TIMER			0x3F
#define MAX17050_Q_RESIDUAL_30		0x42
#define MAX17050_D_QACC				0x45
#define MAX17050_D_PACC				0x46
#define MAX17050_VFSOC0				0x48
#define MAX17050_QH0				0x4C
#define MAX17050_QH					0x4D
#define MAX17050_VFSOC0_LOCK		0x60
#define MAX17050_MODEL_LOCK1		0x62
#define MAX17050_MODEL_LOCK2		0x63
#define MAX17050_V_FOCV				0xFB
#define MAX17050_SOC_VF				0xFF

#define MAX17050_MODEL_TABLE_80	0x80
#define MAX17050_MODEL_TABLE_90	0x90
#define MAX17050_MODEL_TABLE_A0	0xA0

#ifdef CONFIG_LGE_PM
int max17050_get_battery_capacity_percent(void);
int max17050_get_battery_mvolts(void);
int max17050_suspend_get_mvolts(void);
int max17050_read_battery_age(void);
int max17050_get_battery_age(void);
int max17050_get_battery_condition(void);
int max17050_get_battery_current(void);
int max17050_get_soc_for_charging_complete_at_cmd(void);
int max17050_get_full_design(void);
int max17050_write_battery_temp(void);
int max17050_write_temp(void);
bool max17050_battery_full_info_print(void);
void max17050_initial_quickstart_check(void);
bool max17050_recharging(void);
bool max17050_i2c_write_and_verify(u8 addr, u16 value);
#endif

#endif
