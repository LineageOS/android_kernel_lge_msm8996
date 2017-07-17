/*
 * IDTP9223 Wireless Power Receiver driver
 *
 * Copyright (C) 2016 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "IDTP9223: %s: " fmt, __func__

#define pr_idt(reason, fmt, ...)			\
do {							\
	if (idtp9223_debug & (reason))			\
		pr_info(fmt, ##__VA_ARGS__);		\
	else						\
		pr_debug(fmt, ##__VA_ARGS__);		\
} while (0)

#define pr_assert(exp)					\
do {							\
	if ((idtp9223_debug & ASSERT) && !(exp)) {	\
		pr_idt(ASSERT, "Assertion failed\n");	\
		dump_stack();				\
	}						\
} while (0)

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>

#define IDTP9223_NAME_COMPATIBLE	"idt,p9223-charger"
#define IDTP9223_NAME_DRIVER		"idtp9223-charger"
#define IDTP9223_NAME_PSY		"dc-wireless"

// Register addresses
#define REG_OLD_ADDR_CHGSTAT	0x3A
#define REG_OLD_ADDR_EPT	0x3B
#define REG_OLD_ADDR_VOUT	0x3E
#define REG_OLD_ADDR_OPMODE	0x4D
#define REG_OLD_ADDR_COMMAND	0x4E
//----------------------------------
#define REG_NEW_ADDR_CHGSTAT	0x3E
#define REG_NEW_ADDR_EPT	0x3F
#define REG_NEW_ADDR_VOUT	0x3C
#define REG_NEW_ADDR_OPMODE	0x4C
#define REG_NEW_ADDR_COMMAND	0x4E

// For EPT register
#define EPT_BY_EOC		1
#define EPT_BY_OVERTEMP		3
// For VOUT register
#define VOUT_V50		0x0F
#define VOUT_V55		0x14
#define VOUT_V60		0x19
#define VOUT_V65		0x1E
// For Operation mode register
#define OLD_WPC			BIT(0)
#define OLD_PMA			BIT(1)
#define NEW_WPC			BIT(5)
#define NEW_PMA			BIT(6)
// For command register
#define SEND_CHGSTAT		BIT(4)
#define SEND_EPT		BIT(3)

#define SET_PROPERTY_NO_EFFECTED -EINVAL
#define SET_PROPERTY_DO_EFFECTED 0

enum idtp9223_print {
	ASSERT = BIT(0),
	ERROR = BIT(1),
	INTERRUPT = BIT(2),
	MONITOR = BIT(3),
	REGISTER = BIT(4),
	RETURN = BIT(5),
	UPDATE = BIT(6),

	VERBOSE = BIT(7),
};

enum idtp9223_opmode {
	UNKNOWN, WPC, PMA,
};

struct idtp9223_chip {
	/* Chip descripters */
	struct i2c_client* 	wlc_client;
	struct device* 		wlc_device;
	struct mutex 		wlc_mutex;
	struct power_supply	wlc_psy;
	struct delayed_work 	wlc_monitor;

	/* shadow status */
	enum idtp9223_opmode	status_opmode;	// WPC or PMA
	bool			status_alive;	// matched to gpio_alive
	bool			status_enabled;	// opposited to gpio_disabling
	bool			status_full; 	// it means EoC, not 100%
	int 			status_capacity;
	int			status_temperature;

	/* for controling GPIOs */
	int gpio_alive;		// MSM #91, DIR_IN
	int gpio_interrupt;	// MSM #123, DIR_IN
	int gpio_disabling;	// PMI #4, DIR_OUT

	/* configuration from DT */
	int  configure_overheat;	// shutdown threshold for overheat
	bool configure_sysfs;		// making sysfs or not (for debug)

	/* Register addresses */
	u16 REG_ADDR_CHGSTAT;
	u16 REG_ADDR_EPT;
	u16 REG_ADDR_VOUT;
	u16 REG_ADDR_OPMODE;
	u16 REG_ADDR_COMMAND;
};

static int idtp9223_regs [] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x1C, 0x1D, 0x1E, 0x1F,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61,
};

static inline const char* idtp9223_modename(enum idtp9223_opmode modetype) {
	switch (modetype) {
	case WPC :
		return "WPC";
	case PMA :
		return "PMA";

	case UNKNOWN :
	default :
		return "UNKNOWN";
	}
}

static int idtp9223_debug = ERROR | INTERRUPT | MONITOR | REGISTER | UPDATE;

static int idtp9223_reg_read(struct i2c_client* client, u16 reg, u8* val);
static int idtp9223_reg_write(struct i2c_client* client, u16 reg, u8 val);
static void idtp9223_reg_dump(struct idtp9223_chip* chip);
static bool idtp9223_is_online(struct idtp9223_chip* chip);
static bool idtp9223_is_enabled(struct idtp9223_chip* chip);
static bool idtp9223_is_full(struct idtp9223_chip* chip);

static bool psy_set_online(struct idtp9223_chip* chip, bool online);
static bool psy_set_enable(struct idtp9223_chip* chip, bool enable);
static bool psy_set_full(struct idtp9223_chip* chip, bool full);

static char* psy_external_suppliers [] = { "dc", "usb", "battery" };
static void psy_external_changed(struct power_supply* external_supplier);
static enum power_supply_property psy_property_list [] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,	// Wireless charging should be disabled on wired charging
	POWER_SUPPLY_PROP_CHARGE_DONE,
};
static int psy_property_set(struct power_supply* psy,
	enum power_supply_property prop,
	const union power_supply_propval* val);
static int psy_property_get(struct power_supply* psy,
	enum power_supply_property prop,
	union power_supply_propval* val);
static int psy_property_writeable(struct power_supply* psy,
	enum power_supply_property prop);

static ssize_t sysfs_i2c_show(struct device* dev,
	struct device_attribute* attr, char* buffer);
static ssize_t sysfs_i2c_store(struct device* dev,
	struct device_attribute* attr, const char* buf, size_t size);

static DEVICE_ATTR(register, S_IWUSR|S_IRUGO, sysfs_i2c_show, sysfs_i2c_store);

static struct attribute* idtp9223_sysfs_attrs [] = {
	&dev_attr_register.attr,
	NULL
};

static const struct attribute_group idtp9223_sysfs_files = {
	.attrs  = idtp9223_sysfs_attrs,
};

static int idtp9223_reg_read(struct i2c_client* client, u16 reg, u8* value) {

	u8 address [] = {
		reg >> 8,
		reg & 0xff
	};

	struct i2c_msg message [] = {
		{	.addr	= client->addr,
			.flags	= 0,
			.buf	= address,
			.len	= 2
		},
		{	.addr	= client->addr,
			.flags	= I2C_M_RD,
			.buf	= value,
			.len	= 1
		}
	};

	return (i2c_transfer(client->adapter, message, 2)!=2)
		? -ENODATA : 0;
}

static int idtp9223_reg_write(struct i2c_client* client, u16 reg, u8 val) {

	u8 address [] = {
		reg >> 8,
		reg & 0xff,
		val
	};

	struct i2c_msg message = {
		.addr	= client->addr,
		.flags	= 0,
		.buf	= address,
		.len	= 3
	};

	return (i2c_transfer(client->adapter, &message, 1)!=1) ? -ENODATA : 0;
}

static void idtp9223_reg_dump(struct idtp9223_chip* chip) {
	if (idtp9223_is_online(chip)) {
		u8 val;
		int i;

		for (i=0; i<sizeof(idtp9223_regs)/sizeof(idtp9223_regs[0]); i++) {
			val = -1;
			idtp9223_reg_read(chip->wlc_client, idtp9223_regs[i], &val);
			pr_idt(REGISTER, "%02x : %02x\n", idtp9223_regs[i], val);
		}
	}
	else
		pr_idt(VERBOSE, "idtp9223 is off\n");
}

static unsigned int sysfs_i2c_register = -1;
static ssize_t sysfs_i2c_show(struct device* dev,
	struct device_attribute* attr, char* buffer) {

	struct idtp9223_chip* chip = dev->platform_data;

	u8 value = -1;
	if (sysfs_i2c_register != -1) {
		if (!idtp9223_reg_read(chip->wlc_client, sysfs_i2c_register, &value))
			return snprintf(buffer, PAGE_SIZE, "0x%03x", value);
		else
			return snprintf(buffer, PAGE_SIZE, "I2C read fail for 0x%04x\n", sysfs_i2c_register);
	}
	else
		return snprintf(buffer, PAGE_SIZE, "Address should be set befor reading\n");
}
static ssize_t sysfs_i2c_store(struct device* dev,
	struct device_attribute* attr, const char* buf, size_t size) {

	struct idtp9223_chip* chip = dev->platform_data;

	u8 value = -1;
	if (sscanf(buf, "0x%04x-0x%02x", &sysfs_i2c_register, (unsigned int*)&value) == 2) {
		if (idtp9223_reg_write(chip->wlc_client, sysfs_i2c_register, value))
			pr_idt(ERROR, "I2C write fail for 0x%04x\n", sysfs_i2c_register);
	}
	else if (sscanf(buf, "0x%04x", &sysfs_i2c_register) == 1) {
		pr_idt(ERROR, "I2C address 0x%04x is stored\n", sysfs_i2c_register);
	}
	else {
		pr_idt(ERROR, "Usage : echo 0x%%04x-0x%%02x\n > register");
	}

	return size;
}

static bool idtp9223_is_online(struct idtp9223_chip* chip) {
	bool status = chip->status_alive;
	pr_assert(status==gpio_get_value(chip->gpio_alive));

	return status;
}

static bool idtp9223_is_enabled(struct idtp9223_chip* chip) {
	bool status = chip->status_enabled;
	pr_assert(status==!gpio_get_value(chip->gpio_disabling));

	return status;
}

static bool idtp9223_is_full(struct idtp9223_chip* chip) {
	bool status = chip->status_full;

	if (idtp9223_is_online(chip)) {
		u8 val;

		// TODO : check EPT bit
		pr_assert(status==(idtp9223_reg_read(chip->wlc_client, chip->REG_ADDR_EPT, &val)>0));
		return status;
	}
	else {
		pr_idt(VERBOSE, "idtp9223 is off now\n");

		pr_assert(status==false); // The status should be false on offline
		return false;
	}
}

static bool psy_set_online(struct idtp9223_chip* chip, bool online) {
	extern void touch_notify_wireless(u32 type);

	if (chip->status_alive == online) {
		pr_idt(VERBOSE, "status_alive is already set to %d\n", online);
		return false;
	}

	if (online) {
		u8 value = -1;
		u8 FLAG_WPC, FLAG_PMA;

		// Check firmware version and update register addresses
		#define REG_ADDR_FIRMWARE_LO 0x06
		#define REG_ADDR_FIRMWARE_HI 0x07
		idtp9223_reg_read(chip->wlc_client, REG_ADDR_FIRMWARE_HI, &value);
		pr_idt(REGISTER, "REG_ADDR_FIRMWARE_HI :%02x\n", value);
		idtp9223_reg_read(chip->wlc_client, REG_ADDR_FIRMWARE_LO, &value);
		pr_idt(REGISTER, "REG_ADDR_FIRMWARE_LO :%02x\n", value);

		pr_idt(REGISTER, "Resister set :%s\n", (value >= 0x09) ? "New" : "Old");
		chip->REG_ADDR_CHGSTAT 	= (value >= 0x09) ? REG_NEW_ADDR_CHGSTAT : REG_OLD_ADDR_CHGSTAT;
		chip->REG_ADDR_EPT 	= (value >= 0x09) ? REG_NEW_ADDR_EPT     : REG_OLD_ADDR_EPT;
		chip->REG_ADDR_VOUT 	= (value >= 0x09) ? REG_NEW_ADDR_VOUT    : REG_OLD_ADDR_VOUT;
		chip->REG_ADDR_OPMODE 	= (value >= 0x09) ? REG_NEW_ADDR_OPMODE  : REG_OLD_ADDR_OPMODE;
		chip->REG_ADDR_COMMAND 	= (value >= 0x09) ? REG_NEW_ADDR_COMMAND : REG_OLD_ADDR_COMMAND;
		FLAG_WPC = (value >= 0x09) ? NEW_WPC : OLD_WPC;
		FLAG_PMA = (value >= 0x09) ? NEW_PMA : OLD_PMA;

		// Update system's operating mode {WPC, PMA}
		idtp9223_reg_read(chip->wlc_client, chip->REG_ADDR_OPMODE, &value);
		if (value & FLAG_WPC)
			chip->status_opmode = WPC;
		else if (value & FLAG_PMA)
			chip->status_opmode = PMA;
		else
			chip->status_opmode = UNKNOWN;
		pr_idt(REGISTER, "TX mode = %s(%02x)\n", idtp9223_modename(chip->status_opmode), value);

		// Notify capacity only for WPC
		if (chip->status_opmode == WPC) {
			idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_CHGSTAT, chip->status_capacity);
			idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_COMMAND, SEND_CHGSTAT);
		}

		// Set Vout to 5.5V by default
		idtp9223_reg_read(chip->wlc_client, chip->REG_ADDR_VOUT, &value);
		pr_idt(REGISTER, "Before Vout = %02x\n", value);
		idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_VOUT, VOUT_V55);
		idtp9223_reg_read(chip->wlc_client, chip->REG_ADDR_VOUT, &value);
		pr_idt(REGISTER, "After Vout = %02x\n", value);

		touch_notify_wireless(1);
	}
	else {
		chip->status_opmode = UNKNOWN;
		chip->status_full = false;
		touch_notify_wireless(0);
	}

	chip->status_alive = online;
	return true;
}

static bool psy_set_enable(struct idtp9223_chip* chip, bool enable) {
	if (idtp9223_is_enabled(chip) == enable) {
		pr_idt(VERBOSE, "status_enabled is already set to %d\n", enable);
		return false;
	}

	gpiod_set_value(gpio_to_desc(chip->gpio_disabling), !enable);
	chip->status_enabled = enable;
	return true;
}

static bool psy_set_full(struct idtp9223_chip* chip, bool full) {
	if (idtp9223_is_full(chip) == full) {
		pr_idt(VERBOSE, "status full is already set to %d\n", full);
		return false;
	}

	if (!idtp9223_is_online(chip)) {
		pr_idt(VERBOSE, "idtp9223 is off now\n");
		return false;
	}

	if (full) {
		idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_EPT, EPT_BY_EOC);
		idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_COMMAND, SEND_EPT);

		pr_idt(UPDATE, "Sending EPT (reason is EoC)\n");
	}
	// TODO: check when the 'full' is 0

	chip->status_full = full;
	return true;
}

static bool psy_set_capacity(struct idtp9223_chip* chip, int capacity) {
	bool changed = false;

	if (chip->status_capacity != capacity) {
		chip->status_capacity = capacity;
		changed = true;
	}

	// Notify capacity only for WPC
	if (changed && chip->status_opmode == WPC) {
		idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_CHGSTAT, chip->status_capacity);
		idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_COMMAND, SEND_CHGSTAT);
	}

	return changed;
}

static bool psy_set_temperature(struct idtp9223_chip* chip, int temperature) {
	bool changed = false;

	if (chip->status_temperature != temperature) {
		chip->status_temperature = temperature;
		changed = true;

	/* On shutdown by overheat during wireless charging, send EPT by OVERHEAT */
		if (idtp9223_is_online(chip) &&
			chip->status_temperature > chip->configure_overheat) {

			pr_idt(MONITOR, "If the system is about to shutdown, "
				"Turn off with EPT_BY_OVERTEMP\n");
			if (!idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_EPT, EPT_BY_OVERTEMP) &&
				!idtp9223_reg_write(chip->wlc_client, chip->REG_ADDR_COMMAND, SEND_EPT)) {
				pr_idt(MONITOR, "System will be turned off by overheat, "
					"wait for 500msecs to settle TX\n");
				msleep(500);
			}
			else
				pr_idt(ERROR, "Failed to turning off by EPT_BY_OVERTEMP\n");
		}
	}

	return changed;
}

static int psy_property_set(struct power_supply* psy,
	enum power_supply_property prop, const union power_supply_propval* val) {

	int rc = SET_PROPERTY_NO_EFFECTED;
	struct idtp9223_chip* chip = container_of(psy,
		struct idtp9223_chip, wlc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy_set_online(chip, val->intval)) {
			pr_idt(UPDATE, "set POWER_SUPPLY_PROP_ONLINE : %d\n", val->intval);
			rc = SET_PROPERTY_DO_EFFECTED;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (psy_set_enable(chip, val->intval)) {
			pr_idt(UPDATE, "set POWER_SUPPLY_PROP_CHARGING_ENABLED : %d\n", val->intval);
			rc = SET_PROPERTY_DO_EFFECTED;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		if (psy_set_full(chip, val->intval)) {
			pr_idt(UPDATE, "set POWER_SUPPLY_PROP_CHARGE_DONE : %d\n", val->intval);
			rc = SET_PROPERTY_DO_EFFECTED;
		}
		break;
	default:
		break;
	}

	return rc;
}

static int psy_property_get(struct power_supply* psy,
	enum power_supply_property prop, union power_supply_propval* val) {

	struct idtp9223_chip* chip = container_of(psy,
		struct idtp9223_chip, wlc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = idtp9223_is_online(chip);
		pr_idt(RETURN, "get POWER_SUPPLY_PROP_ONLINE : %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = idtp9223_is_enabled(chip);
		pr_idt(RETURN, "get POWER_SUPPLY_PROP_CHARGING_ENABLED : %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		val->intval = idtp9223_is_full(chip);
		pr_idt(RETURN, "get POWER_SUPPLY_PROP_CHARGE_DONE : %d\n", val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int psy_property_writeable(struct power_supply* psy,
	enum power_supply_property prop) {

	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static void psy_external_changed(struct power_supply* wlc_psy) {
	struct idtp9223_chip* chip = container_of(wlc_psy,
		struct idtp9223_chip, wlc_psy);

	union power_supply_propval value = {0, };
	struct power_supply* psy_me = wlc_psy;
	struct power_supply* psy_dc = power_supply_get_by_name("dc");
	struct power_supply* psy_usb = power_supply_get_by_name("usb");
	struct power_supply* psy_batt = power_supply_get_by_name("battery");

	bool effected = false;

	mutex_lock(&chip->wlc_mutex);

	// Check 'online' at first
	// TODO : Remove this check routine after checking the new H/W.
	if (psy_dc) {
		psy_dc->get_property(psy_dc, POWER_SUPPLY_PROP_PRESENT, &value);
		effected |= (SET_PROPERTY_DO_EFFECTED ==
			psy_me->set_property(psy_me, POWER_SUPPLY_PROP_ONLINE, &value));
	}

	if (psy_usb) {
		psy_usb->get_property(psy_usb, POWER_SUPPLY_PROP_PRESENT, &value);
		if (!value.intval)
			value.intval = 1;
		else
			value.intval = 0;

		effected |= (SET_PROPERTY_DO_EFFECTED ==
			psy_me->set_property(psy_me, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value));
	}

	if (psy_batt) {
		psy_batt->get_property(psy_batt, POWER_SUPPLY_PROP_TEMP, &value);
		psy_set_temperature(chip, value.intval);
		psy_batt->get_property(psy_batt, POWER_SUPPLY_PROP_CAPACITY, &value);
		psy_set_capacity(chip, value.intval);

		psy_batt->get_property(psy_batt, POWER_SUPPLY_PROP_CHARGE_DONE, &value);
		effected |= (SET_PROPERTY_DO_EFFECTED ==
			psy_me->set_property(psy_me, POWER_SUPPLY_PROP_CHARGE_DONE, &value));
	}

	if (effected) {
		pr_idt(UPDATE, "%s will affect to system\n", IDTP9223_NAME_PSY);
		power_supply_changed(psy_me);
	}

	mutex_unlock(&chip->wlc_mutex);
}

static void idtp9223_monitor(struct work_struct* work) {
	struct idtp9223_chip* chip = container_of(work, struct idtp9223_chip,
		wlc_monitor.work);

	int enabled = idtp9223_is_enabled(chip);
	int alive = idtp9223_is_online(chip);
	int od2 = gpio_get_value(chip->gpio_interrupt);

	pr_idt(MONITOR, "[ENA]%d [ALV]%d [OD2]%d\n", enabled, alive, od2);
	if (false) {
		/* for debugging */
		idtp9223_reg_dump(chip);
	}

	schedule_delayed_work(&chip->wlc_monitor, round_jiffies_relative
		(msecs_to_jiffies(1000*30)));
}

static irqreturn_t idtp9223_isr_alive(int irq, void* data) {
	struct idtp9223_chip* idtp9223_me = data;
	struct power_supply psy_me = idtp9223_me->wlc_psy;
	const union power_supply_propval alive = {
		.intval = !!gpio_get_value(idtp9223_me->gpio_alive) };

	psy_me.set_property(&psy_me, POWER_SUPPLY_PROP_ONLINE, &alive);

	pr_idt(INTERRUPT, "idtp9223_isr_alive is triggered\n");
	return IRQ_HANDLED;
}

static irqreturn_t idtp9223_isr_notify(int irq, void* data) {
	/* not defined yet */

	pr_idt(INTERRUPT, "idtp9223_isr_notify is triggered\n");
	return IRQ_HANDLED;
}

static int idtp9223_probe_devicetree(struct device_node* dev_node,
	struct idtp9223_chip* chip) {
	int ret = -EINVAL;
	int buf;

	if (!dev_node) {
		pr_idt(ERROR, "dev_node is null\n");
		goto out;
	}

/* Parse GPIOs */
	chip->gpio_alive = of_get_named_gpio(dev_node,
		"idt,gpio-alive", 0);
	if (chip->gpio_alive < 0) {
		pr_idt(ERROR, "Fail to get gpio-alive\n");
		goto out;
	}
	else {
		pr_idt(RETURN, "Get gpio-alive : %d\n", chip->gpio_alive);
	}

	chip->gpio_interrupt = of_get_named_gpio(dev_node,
		"idt,gpio-interrupt", 0);
	if (chip->gpio_interrupt < 0) {
		pr_idt(ERROR, "Fail to get gpio-interrupt\n");
		goto out;
	}
	else {
		pr_idt(RETURN, "Get gpio-interrupt : %d\n", chip->gpio_interrupt);
	}

	chip->gpio_disabling = of_get_named_gpio(dev_node,
		"idt,gpio-disabling", 0);
	if (chip->gpio_disabling < 0) {
		pr_idt(ERROR, "Fail to get gpio-disabling\n");
		goto out;
	}
	else {
		pr_idt(RETURN, "Get gpio-disabling : %d\n", chip->gpio_disabling);
	}

/* Parse misc */
	if (of_property_read_u32(dev_node, "idt,configure-overheat", &buf) < 0) {
		pr_idt(ERROR, "Fail to get configure-overheat\n");
		goto out;
	}
	else {
		chip->configure_overheat = buf;
	}

	if (of_property_read_u32(dev_node, "idt,configure-sysfs", &buf) < 0) {
		pr_idt(ERROR, "Fail to get configure-sysfs\n");
		goto out;
	}
	else {
		chip->configure_sysfs = !!buf;
	}

	ret = 0;
out:
	return ret;
}

static int idtp9223_probe_gpios(struct idtp9223_chip* chip) {
	int ret;

	ret = gpio_request_one(chip->gpio_alive, GPIOF_DIR_IN,
		"gpio_alive");
	if (ret < 0) {
		pr_idt(ERROR, "Fail to request gpio_alive\n");
		return ret;
	}

	ret = gpio_request_one(chip->gpio_interrupt, GPIOF_DIR_IN,
		"gpio_interrupt");
	if (ret < 0) {
		pr_idt(ERROR, "Fail to request gpio_interrupt\n");
		return ret;
	}

	ret = gpio_request_one(chip->gpio_disabling, GPIOF_DIR_OUT,
		"gpio_disabling");
	if (ret < 0) {
		pr_idt(ERROR, "Fail to request gpio_disabling\n");
		return ret;
	}

	return ret;
}

static int idtp9223_probe_irqs(struct idtp9223_chip* chip) {
	int ret = 0;
	goto out;

	/* GPIO 91 : Alive */
	ret = request_threaded_irq(gpio_to_irq(chip->gpio_alive),
		NULL, idtp9223_isr_alive,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"IDTP9223-Alive", chip);
	if (ret) {
		pr_idt(ERROR, "Cannot request irq %d (%d)\n",
			gpio_to_irq(chip->gpio_alive), ret);
		goto out;
	}

	/* GPIO 123 : OD2 */
	ret = request_threaded_irq(gpio_to_irq(chip->gpio_interrupt),
		NULL, idtp9223_isr_notify,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"IDTP9223-Notify", chip);
	if (ret) {
		pr_idt(ERROR, "Cannot request irq %d (%d)\n",
			gpio_to_irq(chip->gpio_interrupt), ret);
		goto out;
	}
out:
	return ret;
}

static int idtp9223_remove(struct i2c_client* client) {
	struct idtp9223_chip* chip_me = i2c_get_clientdata(client);
	pr_idt(VERBOSE, "idt9223 is about to be removed from system\n");

	if (chip_me) {
	/* Clear descripters */
		cancel_delayed_work_sync(&chip_me->wlc_monitor);
		power_supply_unregister(&chip_me->wlc_psy);
		mutex_destroy(&chip_me->wlc_mutex);

	/* Clear gpios */
		if (chip_me->gpio_alive)
			gpio_free(chip_me->gpio_alive);
		if (chip_me->gpio_interrupt)
			gpio_free(chip_me->gpio_interrupt);
		if (chip_me->gpio_disabling)
			gpio_free(chip_me->gpio_disabling);

	/* Finally, make me free */
		kfree(chip_me);
		return 0;
	}
	else
		return -EINVAL;
}

static int idtp9223_probe(struct i2c_client* client,
	const struct i2c_device_id* id) {

	struct idtp9223_chip* chip = kzalloc(sizeof(struct idtp9223_chip), GFP_KERNEL);
	int ret = -EINVAL;

	pr_idt(VERBOSE, "Start\n");

	if (!chip) {
		pr_idt(ERROR, "Failed to alloc memory\n");
		goto error;
	}

	// For wlc_client
	chip->wlc_client = client;
	// For wlc_device
	chip->wlc_device = &client->dev;
	chip->wlc_device->platform_data = chip;
	// For wlc_psy
	chip->wlc_psy.name = IDTP9223_NAME_PSY;
	chip->wlc_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wlc_psy.get_property = psy_property_get;
	chip->wlc_psy.set_property = psy_property_set;
	chip->wlc_psy.properties = psy_property_list;
	chip->wlc_psy.property_is_writeable = psy_property_writeable;
	chip->wlc_psy.num_properties = ARRAY_SIZE(psy_property_list);
	chip->wlc_psy.supplied_from = psy_external_suppliers;
	chip->wlc_psy.num_supplies = ARRAY_SIZE(psy_external_suppliers);
	chip->wlc_psy.external_power_changed = psy_external_changed;
	// For wlc_mutex
	mutex_init(&chip->wlc_mutex);
	// For wlc_monitor
	INIT_DELAYED_WORK(&chip->wlc_monitor, idtp9223_monitor);

	// At first, store the platform_data to drv_data
	i2c_set_clientdata(client, chip);

	// For remained preset
	ret = idtp9223_probe_devicetree(chip->wlc_device->of_node, chip);
	if (ret < 0) {
		pr_idt(ERROR, "Fail to read parse_dt\n");
		goto error;
	}

	// For GPIOs
	ret = idtp9223_probe_gpios(chip);
	if (ret < 0) {
		pr_idt(ERROR, "Fail to request gpio at probe\n");
		goto error;
	}
	ret = idtp9223_probe_irqs(chip);
	if (ret < 0) {
		pr_idt(ERROR, "Fail to request irqs at probe\n");
		goto error;
	}

	// Create sysfs if it is configured
	if (chip->configure_sysfs &&
		sysfs_create_group(&chip->wlc_device->kobj, &idtp9223_sysfs_files) < 0) {
		pr_idt(ERROR, "unable to create sysfs\n");
		goto error;
	}

	// Start to update from psy friends
	ret = power_supply_register(chip->wlc_device, &chip->wlc_psy);
	if (ret < 0) {
		pr_idt(ERROR, "Unable to register wlc_psy ret = %d\n", ret);
		goto error;
	}

	// Start polling to debug
	schedule_delayed_work(&chip->wlc_monitor, 0);

	pr_idt(VERBOSE, "Complete probing IDTP9223\n");
	ret = 0;
	goto success;

error:
	idtp9223_remove(client);
success:
	return ret;
}

//Compatible node must be matched to dts
static struct of_device_id idtp9223_match [] = {
	{ .compatible = IDTP9223_NAME_COMPATIBLE, },
	{ },
};

//I2C slave id supported by driver
static const struct i2c_device_id idtp9223_id [] = {
	{ IDTP9223_NAME_DRIVER, 0 },
	{ }
};

//I2C Driver Info
static struct i2c_driver idtp9223_driver = {
	.driver = {
		.name = IDTP9223_NAME_DRIVER,
		.owner = THIS_MODULE,
		.of_match_table = idtp9223_match,
	},
	.id_table = idtp9223_id,

	.probe = idtp9223_probe,
	.remove = idtp9223_remove,
};

//Easy wrapper to do driver init
module_i2c_driver(idtp9223_driver);

MODULE_DESCRIPTION(IDTP9223_NAME_DRIVER);
MODULE_LICENSE("GPL v2");
