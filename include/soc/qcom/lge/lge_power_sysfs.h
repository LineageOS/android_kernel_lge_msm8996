/*
 * LGE charging scenario Header file.
 *
 * Copyright (C) 2013 LG Electronics
 * sangwoo <sangwoo2.park@lge.com>
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

#ifndef __LGE_POWER_SYSFS_H_
#define __LGE_POWER_SYSFS_H_

#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
#define PWR_SYSFS_GROUPS_NUM 11
#else
#define PWR_SYSFS_GROUPS_NUM 10
#endif
#define PWR_SYSFS_MANDATORY_MAX_NUM 5

struct power_sysfs_array {
	const char *group;
	const char *user_node;
	const char *kernel_node;
};

const char *mandatory_paths[PWR_SYSFS_MANDATORY_MAX_NUM * 2] = {
	"adc", "xo_therm",
	"adc", "batt_therm",
	"battery", "status",
	"battery", "health",
	"charger", "online"
};

const char *group_names[PWR_SYSFS_GROUPS_NUM] = {
	"adc",
	"battery",
	"charger",
	"lcd",
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
	"lcd_ex",
#endif
	"key_led",
	"cpu",
	"gpu",
	"platform",
	"testmode",
	"psfeature"
};

/* Set sysfs node for non-using DT */
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
#define PWR_SYSFS_PATH_NUM 54
#else
#define PWR_SYSFS_PATH_NUM 51
#endif
const char *default_pwr_sysfs_path[PWR_SYSFS_PATH_NUM][3] = {
	/* ADC/MPP */
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
	{"adc", "thermal", "/sys/class/thermal/"},
	{"adc", "xo_therm", "/sys/class/hwmon/hwmon1/device/xo_therm"},
	{"adc", "batt_therm", "/sys/class/power_supply/battery/temp"},
	{"adc", "batt_id", "/sys/class/lge_power/lge_batt_id/valid_batt_id"},
	{"adc", "pa_therm0", "NULL"},
	{"adc", "pa_therm1", "sys/class/lge_power/lge_adc/pa1_therm_phy"},
	{"adc", "usb_in", "/sys/class/power_supply/usb/voltage_now"},
	{"adc", "vcoin", "/sys/class/hwmon/hwmon1/device/vcoin"},
	{"adc", "vph_pwr", "/sys/class/hwmon/hwmon1/device/vph_pwr"},
	{"adc", "usb_id", "/sys/class/hwmon/hwmon1/device/usb_id_lv"},
#else
	{"adc", "thermal", "/sys/class/thermal/"},
	{"adc", "xo_therm", "/sys/class/hwmon/hwmon0/device/xo_therm"},
	{"adc", "batt_therm", "/sys/class/hwmon/hwmon0/device/batt_therm"},
	{"adc", "batt_id", "/sys/class/power_supply/battery_id/valid_batt_id"},
	{"adc", "pa_therm0", "NULL"},
	{"adc", "pa_therm1", "NULL"},
	{"adc", "usb_in", "/sys/class/hwmon/hwmon0/device/mpp5_usbin"},
	{"adc", "vcoin", "/sys/class/hwmon/hwmon0/device/vcoin"},
	{"adc", "vph_pwr", "/sys/class/hwmon/hwmon0/device/vph_pwr"},
	{"adc", "usb_id", "/sys/class/hwmon/hwmon0/device/usb_id_lv"},
#endif
	/* Battery */
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
	{"battery", "capacity", "/sys/class/power_supply/battery/capacity"},
	{"battery", "health", "/sys/class/power_supply/battery/health"},
	{"battery", "present", "/sys/class/power_supply/battery/present"},
	{"battery", "pseudo_batt", "/sys/class/lge_power/lge_cc/pseudo_batt"},
	{"battery", "status", "/sys/class/power_supply/battery/status"},
	{"battery", "temp", "/sys/class/power_supply/battery/temp"},
	{"battery", "valid_batt_id", "/sys/class/lge_powre/lge_batt/valid_batt_id"},
	{"battery", "voltage_now", "/sys/class/power_supply/battery/voltage_now"},
#else
	{"battery", "capacity", "/sys/class/power_supply/battery/capacity"},
	{"battery", "health", "/sys/class/power_supply/battery/health"},
	{"battery", "present", "/sys/class/power_supply/battery/present"},
	{"battery", "pseudo_batt", "/sys/class/power_supply/battery/pseudo_batt"},
	{"battery", "status", "/sys/class/power_supply/battery/status"},
	{"battery", "temp", "/sys/class/power_supply/battery/temp"},
	{"battery", "valid_batt_id", "/sys/class/power_supply/battery/valid_batt_id"},
	{"battery", "voltage_now", "/sys/class/power_supply/battery/voltage_now"},
#endif
	/* Charger */
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
	{"charger", "ac_online", "/sys/class/power_supply/dc/present"},
	{"charger", "usb_online", "/sys/class/power_supply/usb/online"},
	{"charger", "present", "/sys/class/power_supply/usb/present"},
	{"charger", "wlc_online", "NULL"},
	{"charger", "type", "/sys/class/power_supply/usb/type"},
	{"charger", "time_out", "/sys/class/power_supply/battery/safety_timer_enabled"},
	{"charger", "charging_enabled", "/sys/class/power_supply/battery/charging_enabled"},
	{"charger", "ibat_current", "/sys/class/power_supply/battery/current_now"},
	{"charger", "ichg_current", "/sys/class/power_supply/usb/current_max"},
	{"charger", "iusb_control", "NULL"},
	{"charger", "thermal_mitigation", "/sys/class/power_supply/battery/system_temp_level"},
	{"charger", "wlc_thermal_mitigation", "NULL"},
	{"charger", "usb_parallel_chg_status", "/sys/class/power_supply/usb-parallel/status"},
	{"charger", "usb_parallel_charging_enabled", "/sys/class/power_supply/usb-parallel/charging_enabled"},
	{"charger", "quick_charging_state", "/sys/module/lge_charging_controller/parameters/quick_charging_state"},
#else
	{"charger", "ac_online", "/sys/class/power_supply/ac/online"},
	{"charger", "usb_online", "/sys/class/power_supply/usb/online"},
	{"charger", "present", "/sys/class/power_supply/usb/present"},
	{"charger", "wlc_online", "/sys/class/power_supply/wireless/online"},
	{"charger", "type", "/sys/class/power_supply/usb/type"},
	{"charger", "time_out", "/sys/class/power_supply/ac/charger_timer"},
	{"charger", "charging_enabled", "/sys/class/power_supply/battery/charging_enabled"},
	{"charger", "ibat_current", "/sys/class/power_supply/battery/current_now"},
	{"charger", "ichg_current", "/sys/class/power_supply/ac/input_current_max"},
	{"charger", "iusb_control", "/sys/module/bq24296_charger/parameters/iusb_control"},
	{"charger", "thermal_mitigation", "/sys/module/bq24296_charger/parameters/bq24296_thermal_mitigation"},
	{"charger", "wlc_thermal_mitigation", "/sys/module/unified_wireless_charger/parameters/wlc_thermal_mitigation"},
	{"charger", "usb_parallel_chg_status", "NULL"},
	{"charger", "usb_parallel_charging_enabled", "NULL"},
#endif
	/* LCD Backlight */
	{"lcd", "brightness", "/sys/class/leds/lcd-backlight/brightness"},
	{"lcd", "max_brightness", "/sys/class/leds/lcd-backlight/max_brightness"},
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
	{"lcd_ex", "brightness", "/sys/class/leds/lcd-backlight-ex/brightness"},
	{"lcd_ex", "max_brightness", "/sys/class/leds/lcd-backlight-ex/max_brightness"},
#endif
	/* KEY LED */
	{"key_led", "red_brightness", "/sys/class/leds/red/brightness"},
	{"key_led", "green_brightness", "/sys/class/leds/green/brightness"},
	{"key_led", "blue_brightness", "/sys/class/leds/blue/brightness"},
	/* CPU */
	{"cpu", "cpu_idle_modes", "/sys/module/msm_pm/modes"},
	/* GPU */
	{"gpu", "busy", "/sys/class/kgsl/kgsl-3d0/gpubusy"},
	/* PLATFORM */
	{"platform", "speed_bin", "/sys/devices/soc0/speed_bin"},
	{"platform", "pvs_bin", "/sys/devices/soc0/pvs_bin"},
	{"platform", "power_state", "/sys/power/autosleep"},
	{"platform", "poweron_alarm", "/sys/module/qpnp_rtc/parameters/poweron_alarm"},
	{"platform", "pcb_rev", "/sys/class/hwmon/hwmon1/device/pcb_rev"},
	/* testmode */
	{"testmode", "charge", "/sys/class/power_supply/battery/device/at_charge"},
	{"testmode", "chcomp", "/sys/class/power_supply/battery/device/at_chcomp"},
	{"testmode", "chgmodeoff", "/sys/class/power_supply/battery/charging_enabled"},
	{"testmode", "fuelrst", "/sys/bus/i2c/devices/0-0036/fuelrst"},
	{"testmode", "rtc_time", "/dev/rtc0"},
	{"testmode", "pmrst", "/sys/class/power_supply/battery/device/at_pmrst"},
	{"testmode", "battexit", "/sys/class/power_supply/battery/present"},
};
#endif
