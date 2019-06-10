/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * anx7688 USB Type-C Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/async.h>
#include <linux/completion.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/class-dual-role.h>
#ifdef CONFIG_LGE_PM
#include <soc/qcom/lge/board_lge.h>
#endif

#include "anx_i2c_intf.h"
#include "anx7688_core.h"
#include "anx7688_firmware.h"
#include "anx7688_pd.h"
#ifdef CONFIG_LGE_DP_ANX7688
#include "anx7688_dp.h"
#endif
#include "anx7688_mi1.h"
#include "anx7688_debugfs.h"

#define ANX7688_WAKE_LOCK_TIMEOUT 5000
#define SET_ROLE_TIMEOUT 600
#define CDET_COUNT 5
#define TCCDEBOUNCETIME 200
#define PDINIT_DELAY (TCCDEBOUNCETIME - CDET_COUNT)
#define CDET_DEGLICH_DELAY 10
#define CDET_DEGLICH (80 - CDET_DEGLICH_DELAY - CDET_COUNT)
#define USBC_INT_MASK_SET (0xFF & \
                          ~(RECVD_MSG_INT_MASK | \
                          VCONN_CHG_INT_MASK | \
                          VBUS_CHG_INT_MASK | \
                          CC_STATUS_CHG_INT_MASK | \
                          DATA_ROLE_CHG_INT_MASK | \
                          PR_C_GOT_POWER))

#define anx_update_state(chip, st) \
	if (chip && st <= STATE_TRYWAIT_SRC) { \
		chip->state = st; \
		dev_info(&chip->client->dev, "%s: %s\n", __func__, #st); \
	}

extern struct i2c_client *ohio_client;
extern int anx7688_pd_process(struct anx7688_chip *chip);
extern void anx7688_send_init_setting(struct anx7688_chip *chip);
extern struct anx7688_firmware *__must_check
anx7688_firmware_alloc(struct anx7688_chip *chip);
static int anx7688_set_role(struct anx7688_chip *chip, u8 mode);
void anx7688_set_mode_role(struct anx7688_chip *chip, unsigned int val);
void anx7688_set_power_role(struct anx7688_chip *chip, unsigned int val);
void anx7688_set_data_role(struct anx7688_chip *chip, unsigned int val);

static enum dual_role_property drp_properties[] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	DUAL_ROLE_PROP_VCONN_SUPPLY,
#ifdef CONFIG_LGE_USB_TYPE_C
	DUAL_ROLE_PROP_CC1,
	DUAL_ROLE_PROP_CC2,
	DUAL_ROLE_PROP_PDO1,
	DUAL_ROLE_PROP_PDO2,
	DUAL_ROLE_PROP_PDO3,
	DUAL_ROLE_PROP_PDO4,
	DUAL_ROLE_PROP_RDO,
#endif
};

static int dual_role_get_prop(struct dual_role_phy_instance *dual_role,
				enum dual_role_property prop,
				unsigned int *val)
{
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	struct anx7688_chip *chip;

	if (!client)
		return -EINVAL;

	chip = i2c_get_clientdata(client);

	mutex_lock(&chip->mlock);
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		*val = chip->mode;
		break;
	case DUAL_ROLE_PROP_PR:
		*val = chip->power_role;
		break;
	case DUAL_ROLE_PROP_DR:
		*val = chip->data_role;
		break;

	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		*val = chip->is_vconn_on;
		break;
#ifdef CONFIG_LGE_USB_TYPE_C
	case DUAL_ROLE_PROP_CC1:
	case DUAL_ROLE_PROP_CC2:
		switch (prop == DUAL_ROLE_PROP_CC1 ? chip->cc1 : chip->cc2) {
			case CC_RPUSB:
				*val = DUAL_ROLE_PROP_CC_RP_DEFAULT;
				break;
			case CC_RP1P5:
				*val = DUAL_ROLE_PROP_CC_RP_POWER1P5;
				break;
			case CC_RP3P0:
				*val = DUAL_ROLE_PROP_CC_RP_POWER3P0;
				break;
			case CC_VRD:
				*val = DUAL_ROLE_PROP_CC_RD;
				break;
			case CC_VRA:
				*val = DUAL_ROLE_PROP_CC_RA;
				break;
			default:
				*val = DUAL_ROLE_PROP_CC_OPEN;
		}
		break;
	case DUAL_ROLE_PROP_PDO1:
	case DUAL_ROLE_PROP_PDO2:
	case DUAL_ROLE_PROP_PDO3:
	case DUAL_ROLE_PROP_PDO4:
		if (chip->state != STATE_UNATTACHED_SRC &&
			chip->state != STATE_UNATTACHED_SNK &&
			chip->state != STATE_UNATTACHED_DRP) {
			if (chip->power_role == DUAL_ROLE_PROP_PR_SRC)
				*val = chip->src_pdo[prop - DUAL_ROLE_PROP_PDO1];
			else
				*val = chip->offered_pdo[prop - DUAL_ROLE_PROP_PDO1];
		} else
			*val = 0;
		break;
	case DUAL_ROLE_PROP_RDO:
		if (chip->state != STATE_UNATTACHED_SRC &&
			chip->state != STATE_UNATTACHED_SNK &&
			chip->state != STATE_UNATTACHED_DRP) {
			if (chip->power_role == DUAL_ROLE_PROP_PR_SRC)
				*val = chip->offered_rdo;
			else
				*val = chip->rdo;
		} else
			*val = 0;
		break;
#endif
	default:
		dev_err(&chip->client->dev, "unknown property %d\n", prop);
		return -EINVAL;
	}
	mutex_unlock(&chip->mlock);

	return 0;
}

static int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
				enum dual_role_property prop,
				const unsigned int *val)
{
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
        struct anx7688_chip *chip;
	struct device *cdev;

	if (!client)
		return -EINVAL;

	chip = i2c_get_clientdata(client);
	cdev = &client->dev;

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		switch (*val) {
		case DUAL_ROLE_PROP_MODE_UFP:
			if (anx7688_set_role(chip, ANX_ROLE_SNK))
				chip->mode = DUAL_ROLE_PROP_MODE_UFP;
			break;
		case DUAL_ROLE_PROP_MODE_DFP:
			if (anx7688_set_role(chip, ANX_ROLE_SRC))
				chip->mode = DUAL_ROLE_PROP_MODE_DFP;
			break;
		case DUAL_ROLE_PROP_MODE_NONE:
			anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_NONE);
			anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_NONE);
			chip->mode = DUAL_ROLE_PROP_MODE_NONE;
			break;
		default:
			dev_err(cdev, "unknown mode\n");
			return -1;
		}
		break;
	case DUAL_ROLE_PROP_PR:
		switch (*val) {
		case DUAL_ROLE_PROP_PR_SRC:
			power_supply_set_usb_otg(&chip->usbpd_psy, 1);
			chip->power_role = DUAL_ROLE_PROP_PR_SRC;
			break;
		case DUAL_ROLE_PROP_PR_SNK:
			if (chip->power_role == DUAL_ROLE_PROP_PR_SRC)
				power_supply_set_usb_otg(&chip->usbpd_psy, 0);

			chip->power_role = DUAL_ROLE_PROP_PR_SNK;
			break;
		case DUAL_ROLE_PROP_PR_NONE:
			if (chip->power_role == DUAL_ROLE_PROP_PR_SRC)
				power_supply_set_usb_otg(&chip->usbpd_psy, 0);

			chip->power_role = DUAL_ROLE_PROP_PR_NONE;
			break;
		default:
			dev_err(cdev, "unknown power role\n");
			return -1;
		}
		break;
	case DUAL_ROLE_PROP_DR:
		switch (*val) {
		case DUAL_ROLE_PROP_DR_HOST:
			if (chip->data_role == DUAL_ROLE_PROP_DR_DEVICE) {
				power_supply_set_present(chip->usb_psy, 0);
				mdelay(100);
			}

			power_supply_set_usb_otg(chip->usb_psy, 1);
			chip->data_role = DUAL_ROLE_PROP_DR_HOST;
			break;
		case DUAL_ROLE_PROP_DR_DEVICE:
			if (chip->data_role == DUAL_ROLE_PROP_DR_HOST) {
				power_supply_set_usb_otg(chip->usb_psy, 0);
				mdelay(100);
			}

			power_supply_set_present(chip->usb_psy, 1);
			chip->data_role = DUAL_ROLE_PROP_DR_DEVICE;
			break;
		case DUAL_ROLE_PROP_DR_NONE:
			if (chip->data_role == DUAL_ROLE_PROP_DR_DEVICE) {
				power_supply_set_present(chip->usb_psy, 0);
				mdelay(100);
			}

			if (chip->data_role == DUAL_ROLE_PROP_DR_HOST) {
				power_supply_set_usb_otg(chip->usb_psy, 0);
				mdelay(100);
			}

			chip->data_role = DUAL_ROLE_PROP_DR_NONE;
			break;
		default:
			dev_err(cdev, "unknown data role\n");
			return -1;
		}
		break;
	default:
		dev_err(cdev, "unknown dual role\n");
		return -1;

	}

	return 0;
}

static int dual_role_is_writeable(struct dual_role_phy_instance *drp,
				enum dual_role_property prop)
{
	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}

void anx7688_set_mode_role(struct anx7688_chip *chip, unsigned int val)
{
	struct dual_role_phy_instance *dual_role = chip->dual_role;
	struct device *cdev = &chip->client->dev;
	int ret;

	if ((val == DUAL_ROLE_PROP_MODE_UFP) ||
		(val == DUAL_ROLE_PROP_MODE_DFP) ||
		(val == DUAL_ROLE_PROP_MODE_NONE)) {
		ret = dual_role_set_property(dual_role,
				DUAL_ROLE_PROP_MODE,
				&val);
		if (ret < 0) {
			dev_err(cdev, "fail to set mode\n");
			return;
		}
	}
}

void anx7688_set_power_role(struct anx7688_chip *chip, unsigned int val)
{
	struct dual_role_phy_instance *dual_role = chip->dual_role;
	struct device *cdev = &chip->client->dev;
	int ret;

	if ((val == DUAL_ROLE_PROP_PR_SRC) ||
		(val == DUAL_ROLE_PROP_PR_SNK) ||
		(val == DUAL_ROLE_PROP_PR_NONE)) {
		ret = dual_role_set_property(dual_role,
				DUAL_ROLE_PROP_PR,
				&val);
		if (ret < 0) {
			dev_err(cdev, "fail to set pr\n");
			return;
		}
	}
}

void anx7688_set_data_role(struct anx7688_chip *chip, unsigned int val)
{
	struct dual_role_phy_instance *dual_role = chip->dual_role;
	struct device *cdev = &chip->client->dev;
	int ret;

	if ((val == DUAL_ROLE_PROP_DR_HOST) ||
		(val == DUAL_ROLE_PROP_DR_DEVICE) ||
		(val == DUAL_ROLE_PROP_DR_NONE)) {
		ret = dual_role_set_property(dual_role,
				DUAL_ROLE_PROP_DR,
				&val);
		if (ret < 0) {
			dev_err(cdev, "fail to set dr\n");
			return;
		}
	}
}

static int anx7688_regulator_ctrl(struct anx7688_chip *chip, bool val)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	if (val) {
		rc = regulator_enable(chip->avdd10);
		if (rc) {
			dev_err(cdev, "unable to enable avdd10\n");
			return rc;
		}

		rc = regulator_enable(chip->avdd33);
		if (rc) {
			dev_err(cdev, "unable to enable avdd33\n");
			return rc;
		}

		if (chip->pdata->avdd33_ext_ldo) {
			if (gpio_is_valid(chip->pdata->avdd33_gpio)) {
				gpio_set_value(chip->pdata->avdd33_gpio, 1);
			}
		}
		atomic_set(&chip->vdd_on, 1);
		chip->state = STATE_UNATTACHED_DRP;
	} else {
		rc = regulator_disable(chip->avdd33);
		if (rc) {
			dev_err(cdev, "unable to disable avdd33\n");
			return rc;
		}

		rc = regulator_disable(chip->avdd10);
		if (rc) {
			dev_err(cdev, "unable to disable avdd33\n");
			return rc;
		}
		if (chip->pdata->avdd33_ext_ldo) {
			if (gpio_is_valid(chip->pdata->avdd33_gpio)) {
				gpio_set_value(chip->pdata->avdd33_gpio, 0);
			}
		}
		atomic_set(&chip->vdd_on, 0);
		chip->state = STATE_DISABLED;
	}

	return 0;
}

static int anx7688_power_reset(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = anx7688_regulator_ctrl(chip, true);
	if (rc) {
		dev_err(cdev, "unable to enable avdd10\n");
		return rc;
	}

	mdelay(10);

	rc = anx7688_regulator_ctrl(chip, false);
	if (rc) {
		dev_err(cdev, "unable to enable avdd10\n");
		return rc;
	}

	return 0;
}

inline static void anx7688_dump_register(void)
{
	int i;
	u8 buf[16] = {0,};

	pr_info("ANX7688 DUMP REGISTER\n");
	mdelay(5);
	pr_info("<0x%x> 00 01 02 03 04 05 06 07 08 09"
		"0A 0B 0C 0D 0E 0F\n", USBC_ADDR);

	for (i = 0; i < 256; i += 16) {
		OhioReadBlockReg(USBC_ADDR, i, 16, buf);
		pr_info("%.4x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x\n",
			i, buf[0], buf[1], buf[2], buf[3], buf[4],
			buf[5], buf[6], buf[7], buf[8], buf[9],
			buf[10], buf[11], buf[12], buf[13], buf[14],
			buf[15]);
		mdelay(5);
	}
	pr_info("\n");

	mdelay(5);
	pr_info("<0x%x> 00 01 02 03 04 05 06 07 08 09"
		"0A 0B 0C 0D 0E 0F\n", TCPC_ADDR);

	for (i = 0; i < 256; i += 16) {
		OhioReadBlockReg(TCPC_ADDR, i, 16, buf);
		pr_info("%.4x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x\n",
			i, buf[0], buf[1], buf[2], buf[3], buf[4],
			buf[5], buf[6], buf[7], buf[8], buf[9],
			buf[10], buf[11], buf[12], buf[13], buf[14],
			buf[15]);
		mdelay(5);
	}
	pr_info("END DUMP REGISTER\n");
}

#define PWR_DELAY    50
#define BOOT_TIMEOUT (1000/PWR_DELAY)
void anx7688_pwr_on(struct anx7688_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct device *cdev = &client->dev;
	int i;
	int ret;
	int timeout;
	int count = 0;

	if (atomic_read(&chip->power_on))
		return;

	init_completion(&chip->wait_pwr_ctrl);
pwron:
	timeout = 0;

	gpio_set_value(chip->pdata->pwren_gpio, 1);
	mdelay(10);

	gpio_set_value(chip->pdata->rstn_gpio, 1);
	mdelay(10);

	/*
	 * Current Advertizement to 1.5A
	 */
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_6, USBC_R_RP, 1);

	/*  mi1 firmware bug:
	 *  pd-message signal applied on cable det pin
	 *  this bug only shown firmware version under
         *  0032. it will fixed next firmware version.
	 */
	if (chip->pdata->fwver <= MI1_FWVER_RC2 && chip->pdata->fwver != 0x00) {
		OhioWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_0, 0xA0);
		OhioWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_2, 0x09);
	}

	for (i = 0; i < BOOT_TIMEOUT ; i++) {
		if ((chip->pdata->fwver >= MI1_FWVER_RC3) ||
			(chip->pdata->fwver == 0x00)) {
			ret = OhioReadReg(USBC_ADDR, OCM_DEBUG_1);
			if ((ret & 0x01) == 0x01) {
				dev_info(cdev, "boot load done\n");
				break;
			}
		} else {
			ret = OhioReadReg(USBC_ADDR, OCM_DEBUG_9);
			if ((ret & 0x03) == 0x03) {
				dev_info(cdev, "boot load done\n");
				break;
			}
		}
		mdelay(PWR_DELAY);
		timeout++;
	}

	if (timeout >= BOOT_TIMEOUT) {
		if (count++ < 3) {
			anx7688_power_reset(chip);
			gpio_set_value(chip->pdata->pwren_gpio, 0);
			mdelay(2);
			gpio_set_value(chip->pdata->rstn_gpio, 0);
			mdelay(100);
			goto pwron;
		}
		dev_err(cdev, "boot load failed\n");
	}

	complete(&chip->wait_pwr_ctrl);
	atomic_set(&chip->power_on, 1);
	return;
}

void anx7688_pwr_down(struct anx7688_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct device *cdev = &client->dev;

	if (!atomic_read(&chip->power_on))
		return;

	gpio_set_value(chip->pdata->pwren_gpio, 0);
	mdelay(2);

	gpio_set_value(chip->pdata->rstn_gpio, 0);
	mdelay(2);


	atomic_set(&chip->power_on, 0);
	dev_dbg(cdev, "power down\n");

	return;
}

#ifdef CONFIG_LGE_PM
static int usbpd_set_property_on_batt(struct anx7688_chip *chip,
				enum power_supply_property prop, int val)
{
	union power_supply_propval pval = {val, };

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy)
			return -ENODEV;
	}

	return chip->batt_psy->set_property(chip->batt_psy, prop, &pval);
}
#endif

static char *usbpd_supplicants[] = {
	"battery",
};

static enum power_supply_property usbpd_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_TYPE,
};

static const char *usbc_to_string(enum power_supply_type type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_CTYPE:
		return "USB Type-C Charger";
	case POWER_SUPPLY_TYPE_CTYPE_PD:
		return "USB Type-C PD Charger";
	default:
		return "Unknown Charger";
	}
}

static int usbpd_get_property(struct power_supply *psy,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
	struct anx7688_chip *chip = container_of(psy,
				struct anx7688_chip, usbpd_psy);

	switch(prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		val->intval = chip->is_otg;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->is_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chip->volt_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->curr_max;
		break;
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		val->intval = chip->curr_max;
		break;
#endif
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = chip->usbpd_psy.type;
		break;
#ifdef CONFIG_LGE_USB_ANX7688_OVP
	case POWER_SUPPLY_PROP_CTYPE_RP:
		dev_dbg(&chip->client->dev, "%s: Rp %dK\n", __func__,
			chip->rp.intval);
		val->intval = chip->rp.intval;
		break;
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_SIMPLE
	case POWER_SUPPLY_PROP_DP_ALT_MODE:
		val->intval = chip->dp_alt_mode;
#endif
#endif
	default:
		return -EINVAL;
	}

        return 0;
}


static int usbpd_set_property(struct power_supply *psy,
				enum power_supply_property prop,
				const union power_supply_propval *val)
{
	struct anx7688_chip *chip = container_of(psy,
				struct anx7688_chip, usbpd_psy);
	struct device *cdev = &chip->client->dev;
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		if (chip->is_otg == val->intval)
			break;

		chip->is_otg = val->intval;

		if (chip->is_otg) {
			rc = regulator_enable(chip->vbus_out);
			if (rc)
				dev_err(cdev, "unable to enable vbus\n");
		} else {
			rc = regulator_disable(chip->vbus_out);
			if (rc)
				dev_err(cdev, "unable to disable vbus\n");

			/* wait for vbus boost diacharging */
			mdelay(200);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval) {
			if (chip->mode == DUAL_ROLE_PROP_MODE_NONE) {
				dev_info(cdev, "power on by charger\n");
				anx7688_set_data_role(chip,
						DUAL_ROLE_PROP_DR_DEVICE);
#if defined(CONFIG_LGE_USB_TYPE_C)
				if (chip->pdata->fwver < MI1_FWVER_RC1)
					anx7688_pwr_on(chip);
#endif
			} else if (chip->state == STATE_DEBUG_ACCESSORY) {
				if (!atomic_read(&chip->power_on)) {
#if defined(CONFIG_LGE_USB_TYPE_C)
				anx7688_pwr_on(chip);
				anx7688_set_data_role(chip,
						DUAL_ROLE_PROP_DR_DEVICE);
#else
				/* Do nothing */
				;
#endif
#if defined(CONFIG_LGE_USB_TYPE_C)
				} else if (chip->data_role ==
						DUAL_ROLE_PROP_DR_NONE) {
					anx7688_set_data_role(chip,
							DUAL_ROLE_PROP_DR_DEVICE);
				}
#else
				}
#endif
			}
		} else if (chip->mode == DUAL_ROLE_PROP_MODE_NONE) {
			if (chip->data_role == DUAL_ROLE_PROP_DR_DEVICE) {
				dev_info(cdev, "power down by charger\n");
				anx7688_set_data_role(chip,
						DUAL_ROLE_PROP_DR_NONE);
			} else if (chip->data_role == DUAL_ROLE_PROP_DR_NONE) {
				union power_supply_propval prop;
				chip->usb_psy->get_property(chip->usb_psy,
						POWER_SUPPLY_PROP_PRESENT, &prop);
				if (prop.intval) {
					dev_info(cdev, "power down by charger\n");
					power_supply_set_present(chip->usb_psy, 0);
#if defined(CONFIG_LGE_USB_TYPE_C)
					if (chip->pdata->fwver < MI1_FWVER_RC1)
						anx7688_pwr_down(chip);
#endif
				}
                        }
		} else if (chip->state == STATE_DEBUG_ACCESSORY &&
					!val->intval) {
#if defined(CONFIG_LGE_USB_TYPE_C)
			anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_NONE);
			anx7688_pwr_down(chip);
#else
			/* Do nothing */
			;
#endif
		}

		if (chip->is_present == val->intval)
			break;

		chip->is_present = val->intval;

		if (chip->is_present) {
			schedule_delayed_work(&chip->cwork,
					msecs_to_jiffies(5000));
		} else {
			cancel_delayed_work(&chip->cwork);
			chip->curr_max = 0;
			chip->volt_max = 0;
			chip->charger_type = USBC_UNKNWON_CHARGER;
		}

		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chip->volt_max = val->intval;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		chip->curr_max = val->intval;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		switch (val->intval) {
		case POWER_SUPPLY_TYPE_CTYPE:
		case POWER_SUPPLY_TYPE_CTYPE_PD:
		case POWER_SUPPLY_TYPE_USB_HVDCP:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3:
			psy->type = val->intval;
			break;
		default:
			psy->type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		}
		break;
#ifdef CONFIG_LGE_USB_ANX7688_OVP
	case POWER_SUPPLY_PROP_CTYPE_RP:
		chip->rp.intval = val->intval;
		dev_dbg(cdev, "%s: Rp %dK\n", __func__, chip->rp.intval);
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_SIMPLE
		// do nothing
#else
		chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CTYPE_RP, &chip->rp);
#endif
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int usbpd_is_writeable(struct power_supply *psy,
				enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_TYPE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static void anx7688_ctype_work(struct work_struct *w)
{
	struct anx7688_chip *chip = container_of(w,
			struct anx7688_chip, cwork.work);
	struct device *cdev = &chip->client->dev;
	int cc1, cc2;

	if (!atomic_read(&chip->power_on))
		return;

	if (chip->usbpd_psy.type == POWER_SUPPLY_TYPE_USB_HVDCP ||
		chip->usbpd_psy.type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
		return;

	cc1 = chip->cc1;
	cc2 = chip->cc2;

	if (chip->charger_type != USBC_PD_CHARGER) {
		if ((cc1 == CC_RP3P0) || (cc2 == CC_RP3P0)) {
			chip->volt_max = USBC_VOLT_RP3P0;
			chip->curr_max = USBC_CURR_RP3P0;
			chip->charger_type = USBC_CHARGER;
		} else if ((cc1 == CC_RP1P5) || (cc2 == CC_RP1P5)) {
			if (chip->charger_type == USBC_CHARGER) {
				chip->volt_max = USBC_VOLT_RP1P5;
				chip->curr_max = USBC_CURR_RP1P5;
				chip->charger_type = USBC_CHARGER;
			}
		} else {
			dev_dbg(cdev, "%s: default usb Power\n", __func__);
			chip->volt_max = USBC_VOLT_RPUSB;
			chip->curr_max = 0;
			chip->charger_type = USBC_UNKNWON_CHARGER;
		}
	}

	/* update charger type*/
	switch (chip->charger_type) {
#if defined(CONFIG_LGE_USB_FLOATED_CHARGER_DETECT) && \
	defined(CONFIG_LGE_USB_TYPE_C)
	union power_supply_propval prop;
#endif
	case USBC_CHARGER:
		power_supply_set_supply_type(&chip->usbpd_psy,
				POWER_SUPPLY_TYPE_CTYPE);
#if defined(CONFIG_LGE_USB_FLOATED_CHARGER_DETECT) && \
	defined(CONFIG_LGE_USB_TYPE_C)
		chip->usb_psy->set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CTYPE_CHARGER, &prop);
#endif
#ifdef CONFIG_LGE_PM
		usbpd_set_property_on_batt(chip,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
				chip->curr_max);
#endif

		break;
	case USBC_PD_CHARGER:
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
#ifdef CONFIG_LGE_DP_ANX7688
		/*
		 * Current Restict Mode
		 * Some DP Support Type-C Display Device
		 * design to supply high current like 5V/2A
		 * with e-marked cable have no problem with
		 * charging with display. but when we connect
		 * non-compliance Type-C Cable connect each
		 * other, it make voltage drop at I/O port.
		 * to prevent UV handle interrupt, annouce
		 * current 5V/1A to chager power_sypply.
		 */
		if (chip->dp_status != ANX_DP_DISCONNECTED) {
			if ((chip->volt_max == USBC_VOLT_RP3P0) &&
				(chip->curr_max > USBC_CURR_RESTRICT))
				chip->curr_max = USBC_CURR_RESTRICT;
		}
#endif
#endif

		power_supply_set_supply_type(&chip->usbpd_psy,
				POWER_SUPPLY_TYPE_CTYPE_PD);
#if defined(CONFIG_LGE_USB_FLOATED_CHARGER_DETECT) && \
	defined(CONFIG_LGE_USB_TYPE_C)
		chip->usb_psy->set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CTYPE_CHARGER, &prop);
#endif
#ifdef CONFIG_LGE_PM
		usbpd_set_property_on_batt(chip,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
				chip->curr_max);
#endif
		break;
	default:
		/* unknown charger */
		break;
	}

	dev_info(cdev, "%s: %s, %dmV, %dmA\n", __func__,
			usbc_to_string(chip->usbpd_psy.type),
			chip->volt_max,
			chip->curr_max);

	power_supply_changed(&chip->usbpd_psy);

	return;
}

void anx7688_sbu_ctrl(struct anx7688_chip *chip, bool dir)
{
#ifdef CONFIG_LGE_USB_TYPE_C
#ifdef CONFIG_LGE_PM_LGE_POWER_CLASS_SIMPLE
	chip->dp_alt_mode = !dir;
#else
	struct device *cdev = &chip->client->dev;
	union power_supply_propval prop;
	int rc;

	prop.intval = !dir;
	rc = chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
	if (rc < 0)
		dev_err(cdev, "fail to dp alt set %d\n", rc);
#endif
#endif
	gpio_set_value(chip->pdata->sbu_gpio, dir);
	if (dir)
		chip->is_sbu_switched = true;
	else
		chip->is_sbu_switched = false;
}


static void anx7688_vconn_ctrl(struct anx7688_chip *chip, bool enable)
{
	struct device *cdev = &chip->client->dev;

	if (chip->pdata->vconn_always_on && chip->is_vconn_on)
		return;

	if (enable) {
		if (gpio_is_valid(chip->pdata->vconn_gpio)) {
			gpio_set_value(chip->pdata->vconn_gpio, 1);
			chip->is_vconn_on = true;
		}
	} else {
		if (gpio_is_valid(chip->pdata->vconn_gpio)) {
			gpio_set_value(chip->pdata->vconn_gpio, 0);
			chip->is_vconn_on = false;
		}
	}
	dev_dbg(cdev, "vconn boost %s\n", enable ? "on":"off");
}

static void anx7688_detach(struct anx7688_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct device *cdev = &client->dev;
#ifdef CONFIG_LGE_USB_TYPE_C
	uint8_t offered_pdo_idx;
#endif

	switch (chip->state) {
	case STATE_ATTACHED_SRC:
		anx7688_vconn_ctrl(chip, false);
		break;
	case STATE_ATTACHED_SNK:
		break;
	case STATE_AUDIO_ACCESSORY:
		break;
	case STATE_DEBUG_ACCESSORY:
		anx7688_sbu_ctrl(chip, false);
		break;
	case STATE_PWRED_ACCESSORY:
		anx7688_vconn_ctrl(chip, false);
		break;
	case STATE_PWRED_NOSINK:
		anx7688_vconn_ctrl(chip, false);
		break;
	default:
		dev_dbg(cdev, "%s: nothing do on state[0x%02x]\n",
			__func__, chip->state);
		break;
	}
#ifdef CONFIG_LGE_DP_ANX7688
	if (chip->dp_status != ANX_DP_DISCONNECTED) {
		chip->dp_status = ANX_DP_DISCONNECTED;
		cancel_delayed_work(&chip->swork);
		wake_unlock(&chip->dp_lock);
		wake_lock_timeout(&chip->dp_lock, 2*HZ);
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
		slimport_set_hdmi_hpd(0);
		pr_info("%s:set hdmi hpd off\n", __func__);
#endif
	}
#endif
	if (chip->is_vconn_on)
		anx7688_vconn_ctrl(chip, false);
	if (chip->is_sbu_switched)
		anx7688_sbu_ctrl(chip, false);

#ifdef CONFIG_LGE_USB_ANX7688_ADC
	chip->is_pd_connected = false;
#endif
	chip->cc1 = CC_OPEN;
	chip->cc2 = CC_OPEN;
#ifdef CONFIG_LGE_USB_TYPE_C
	for (offered_pdo_idx = 0; offered_pdo_idx < PD_MAX_PDO_NUM; offered_pdo_idx++)
		chip->offered_pdo[offered_pdo_idx] = 0;
	chip->offered_rdo = 0;
	chip->rdo = 0;
#endif
	anx_update_state(chip, STATE_UNATTACHED_DRP);
	anx7688_set_mode_role(chip, DUAL_ROLE_PROP_MODE_NONE);
	dual_role_instance_changed(chip->dual_role);
	return;
}

static void anx7688_snk_detect(struct anx7688_chip *chip)
{
	chip->mode = DUAL_ROLE_PROP_MODE_DFP;
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_6, USBC_R_RP, 0);
	/*
	 * mi1 firmware bug:
	 * when source detected datarole, vbus chanage interrupt
	 * does not triggered, to prevent usb connection set data,
	 * power role set to here. this bug fix at next fw version.
	 */
	if (chip->pdata->fwver <= MI1_FWVER_RC2) {
		anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SRC);
		anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_HOST);
	}
	anx_update_state(chip, STATE_ATTACHED_SRC);
}

#ifdef CONFIG_LGE_USB_ANX7688_ADC
#define ANX7688_MAX_ADC_READ  5
#define ANX7688_CC_ADC_SHORT  0x30	/* 2400mV */
#define ANX7688_CC_ADC_MIN    0x16	/* 1100mV */
#define ANX7688_MAX_ADC_CHECK 30
#define ANX7688_MAX_ERR_COUNT 2
static bool anx7688_cc_vadc_check(struct anx7688_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct device *cdev = &client->dev;
	u16 adc_out;
	u16 max_adc_out = 0;
	char error_cnt = 0;
	char timeout = 0;
	u8 adc_ctrl;
	bool cc_flip;
	int i;

	OhioMaskWriteReg(USBC_ADDR, USBC_AUTO_PD_MODE, VBUS_ADC_DISABLE, 1);
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, CC_SOFT_EN, 1);
	if (OhioReadReg(USBC_ADDR, USBC_ANALOG_CTRL_9) & BMC_ON_CC1) {
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, BMC_ON_CC1, 0);
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_4,
				PWR_SW_ON_CC1, 1);
		cc_flip = true;
	} else {
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, BMC_ON_CC1, 1);
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_4,
				PWR_SW_ON_CC2, 1);
		cc_flip = false;
	}

	OhioWriteReg(USBC_ADDR, SAR_ADC_CTRL, R_ADC_VCONN_SEL);
	OhioWriteReg(USBC_ADDR, SAR_ADC_CTRL,
			(R_ADC_VCONN_SEL | R_ADC_EN));

	for (i = 0; i < ANX7688_MAX_ADC_READ; i++) {
		udelay(200);

		timeout = 0;
		do {
			udelay(10);
			adc_ctrl = OhioReadReg(USBC_ADDR, SAR_ADC_CTRL);
			if (IS_ERR_VALUE(adc_ctrl)) {
				if (!atomic_read(&chip->power_on))
					return false;
				adc_ctrl = 0;
			}

			if (timeout++ > ANX7688_MAX_ADC_CHECK) {
				dev_err(cdev, "timeout adc read\n");
				error_cnt++;
				break;
			}
		} while (!(adc_ctrl & R_ADC_READY));

		adc_out = OhioReadReg(USBC_ADDR, SAR_ADC_OUT);
		adc_out = ((adc_ctrl << 3) & 0x100) | adc_out;
		dev_dbg(cdev, "cc%d %x adc 0x%x %dmv\n",
				cc_flip?2:1, adc_ctrl, adc_out, adc_out * 50);

		if (adc_out == 0)
			error_cnt++;

		if (adc_out > max_adc_out)
			max_adc_out = adc_out;
	}

	dev_info(cdev, "max adc %dmV, error count %d\n",
			max_adc_out * 50, error_cnt);

	/* rollback previous setting */
	OhioWriteReg(USBC_ADDR, SAR_ADC_CTRL, R_ADC_VBUS_SEL);
	OhioWriteReg(USBC_ADDR, SAR_ADC_CTRL, (R_ADC_VBUS_SEL | R_ADC_EN));

	OhioMaskWriteReg(USBC_ADDR, USBC_AUTO_PD_MODE, VBUS_ADC_DISABLE, 0);
	if (cc_flip) {
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_4,
				PWR_SW_ON_CC1, 0);
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, BMC_ON_CC1, 1);
	} else {
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_4,
				PWR_SW_ON_CC2, 0);
		OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, BMC_ON_CC1, 0);
	}
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, CC_SOFT_EN, 0);

	if ((max_adc_out > ANX7688_CC_ADC_SHORT) ||
			(max_adc_out < ANX7688_CC_ADC_MIN) ||
			(error_cnt > ANX7688_MAX_ERR_COUNT))
		return false;

	return true;
}
#endif

static void anx7688_src_detect(struct anx7688_chip *chip, int cc1, int cc2)
{
#if defined(CONFIG_LGE_USB_ANX7688_OVP)
	union power_supply_propval prop;
#endif
	chip->cc1 = cc1;
	chip->cc2 = cc2;

#ifdef CONFIG_LGE_USB_ANX7688_OVP
#ifdef CONFIG_LGE_USB_ANX7688_ADC
	if (!chip->is_pd_connected) {
		if ((cc1 == CC_RP3P0) || (cc2 == CC_RP3P0)) {
			if(anx7688_cc_vadc_check(chip))
				prop.intval = 1;
			else
				prop.intval = 0;
		} else {
			prop.intval = 1;
		}
	} else {
		 prop.intval = 0;
	}
	usbpd_set_property(&chip->usbpd_psy,
			POWER_SUPPLY_PROP_CTYPE_RP, &prop);
#else
	if (cc1 == CC_OPEN) {
		if (cc2 == CC_RPUSB) {
			prop.intval = 56;
		} else if (cc2 == CC_RP1P5) {
			prop.intval = 22;
		} else if (cc2 == CC_RP3P0) {
			prop.intval = 10;
		} else {
			prop.intval = 0;
		}
	} else if (cc2 == CC_OPEN) {
		if (cc1 == CC_RPUSB) {
			prop.intval = 56;
		} else if (cc1 == CC_RP1P5) {
			prop.intval = 22;
		} else if (cc1 == CC_RP3P0) {
			prop.intval = 10;
		} else {
			prop.intval = 0;
		}
	} else if ((cc1 == CC_RP3P0) && (cc2 == CC_RP3P0)) {
		prop.intval = 10;
	} else {
		prop.intval = 0;
	}
	usbpd_set_property(&chip->usbpd_psy,
			POWER_SUPPLY_PROP_CTYPE_RP, &prop);
#endif
#endif
	chip->mode = DUAL_ROLE_PROP_MODE_UFP;
	/*
	 * mi1 firmware bug:
	 * when source detected datarole, vbus chanage interrupt
	 * does not triggered, to prevent usb connection set data,
	 * power role set to here. this bug fix at next fw version.
	 */
	if (chip->pdata->fwver <= MI1_FWVER_RC2) {
		anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_DEVICE);
		anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SNK);
	}
	anx_update_state(chip, STATE_ATTACHED_SNK);
	schedule_delayed_work(&chip->cwork, 0);
}

static void anx7688_pwred_nosink_detect(struct anx7688_chip *chip)
{
	anx_update_state(chip, STATE_PWRED_NOSINK);
}

static void anx7688_pwred_accessory_detect(struct anx7688_chip *chip,
						int cc1, int cc2)
{
	chip->mode = DUAL_ROLE_PROP_MODE_DFP;
	/*
	 * mi1 firmware bug:
	 * when source detected datarole, vbus chanage interrupt
	 * does not triggered, to prevent usb connection set data,
	 * power role set to here. this bug fix at next fw version.
	 */
	if (chip->pdata->fwver <= MI1_FWVER_RC2) {
		anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_HOST);
	}
#ifdef CONFIG_LGE_DP_ANX7688
	if (chip->dp_status == ANX_DP_DISCONNECTED) {
		chip->dp_status = ANX_DP_CONNECTED;
		wake_lock(&chip->dp_lock);
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
		slimport_set_hdmi_hpd(1);
		pr_info("%s:set hdmi hpd on\n", __func__);
#endif
	}
#endif
	anx_update_state(chip, STATE_PWRED_ACCESSORY);
}

static void anx7688_audio_accessory_detect(struct anx7688_chip *chip)
{
	/* TODO: implement if this function need */
	anx_update_state(chip, STATE_AUDIO_ACCESSORY);
}

static void anx7688_debug_accessory_detect(struct anx7688_chip *chip)
{
	anx7688_sbu_ctrl(chip, true);
#ifdef CONFIG_LGE_USB_TYPE_C
	chip->mode = DUAL_ROLE_PROP_MODE_UFP;
	anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_DEVICE);
	anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SNK);
#endif

	/* TODO: implement if this function need */
	anx_update_state(chip, STATE_DEBUG_ACCESSORY);
}

static void usbc_chg_ccstatus(struct anx7688_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct device *cdev = &client->dev;
	int ret;
	int cc1, cc2;

	ret = OhioReadReg(USBC_ADDR, USBC_CC_STATUS);
	if (!ret) {
		int snk_cc, src_cc;
		dev_info(cdev, "retry read cc status 0x%x\n", ret);

		snk_cc = OhioReadReg(USBC_ADDR, USBC_ANALOG_CTRL_7) & 0x0F;
		src_cc = OhioReadReg(USBC_ADDR, USBC_PWRDN_CTRL) & 0xFC;
		if (src_cc & !snk_cc) {
			if ((src_cc & (CC1_VRD_3P0 |
					CC1_VRD_1P5 |
					CC1_VRD_USB)) &&
				!(src_cc & (CC1_VRD_3P0 |
						CC1_VRD_1P5 |
						CC1_VRD_USB))) {
				if (src_cc & CC1_VRD_USB) {
					ret = BIT(2);
				} else if (src_cc & CC1_VRD_1P5) {
					ret = BIT(3);
				} else if (src_cc & CC1_VRD_3P0) {
					ret = BIT(2) | BIT(3);
				}
			} else if ((src_cc & (CC1_VRD_3P0 |
						CC1_VRD_1P5 |
						CC1_VRD_USB)) &&
					!(src_cc & (CC1_VRD_3P0 |
							CC1_VRD_1P5 |
							CC1_VRD_USB))) {
				if (src_cc & CC1_VRD_USB) {
					ret = BIT(6);
				} else if (src_cc & CC1_VRD_1P5) {
					ret = BIT(7);
				} else if (src_cc & CC1_VRD_3P0) {
					ret = BIT(6) | BIT(7);
				}
			}
		} else if (snk_cc & !src_cc) {
			if ((snk_cc & CC1_RA_CONNECT) &&
					!(snk_cc & CC2_RA_CONNECT)) {
				if (snk_cc == CC1_RD_CONNECT) {
					ret = BIT(0);
				} else if (snk_cc == CC1_RA_CONNECT) {
					ret = BIT(1);
				}
			} else if ((snk_cc & CC2_RA_CONNECT) &&
					!(snk_cc & CC1_RA_CONNECT)) {
				if (snk_cc == CC2_RD_CONNECT) {
					ret = BIT(4);
				} else if (snk_cc == CC2_RA_CONNECT) {
					ret = BIT(5);
				}
			}
		} else {
			dev_err(cdev, "unknown snk cc:0x%x"
					"src cc:0x%x\n", snk_cc, src_cc);
		}
	}

	dev_info(cdev, "CC status 0x%x\n", ret);
	cc1 = ret & 0xF;
	cc2 = (ret >> 4) & 0xF;
	chip->cc1 = cc1;
	chip->cc2 = cc2;

	if (((cc1 >= CC_RPUSB) && (cc2 == CC_OPEN)) ||
		((cc2 >= CC_RPUSB) && (cc1 == CC_OPEN))) {
		/* USB Peripheral cable */
		anx7688_src_detect(chip, cc1, cc2);
	} else if (((cc1 == CC_VRD) && (cc2 == CC_OPEN)) ||
		((cc2 == CC_VRD) && (cc1 == CC_OPEN))) {
		/* USB Host cable */
		anx7688_snk_detect(chip);
	} else if (((cc1 == CC_VRA) && (cc2 == CC_VRD)) ||
		((cc2 == CC_VRA) && (cc1 == CC_VRD))) {
		/* VCONN Powered cable */
		anx7688_pwred_accessory_detect(chip, cc1, cc2);
	} else if (((cc1 == CC_VRA) && (cc2 == CC_OPEN)) ||
		((cc2 == CC_VRA) && (cc1 == CC_OPEN))) {
		/* Powered cable without sink */
		anx7688_pwred_nosink_detect(chip);
	} else if ((cc1 == CC_VRD) && (cc2 == CC_VRD)) {
		/* debug accessory cable */
		anx7688_debug_accessory_detect(chip);
	} else if ((cc1 == CC_VRA) && (cc2 == CC_VRA)) {
		/* audio accessory cable */
		anx7688_audio_accessory_detect(chip);
	} else if ((cc1 == CC_OPEN) && (cc2 == CC_OPEN)) {
		/* CC open cable */
	} else {
		/* spec out */
		dev_err(cdev, "unknwon cc status\n");
	}
}

static inline void usbc_chg_datarole(struct anx7688_chip *chip)
{
	if (OhioReadReg(USBC_ADDR, USBC_INTF_STATUS) & IDATA_ROLE) {
		anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_HOST);
	} else {
		anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_DEVICE);
	}
}

static void usbc_chg_vconn(struct anx7688_chip *chip)
{
	if (OhioReadReg(USBC_ADDR, USBC_INTF_STATUS) & IVCONN_STATUS) {
		anx7688_vconn_ctrl(chip, true);
	} else {
		anx7688_vconn_ctrl(chip, false);
	}
}

static inline void usbc_chg_vbus(struct anx7688_chip *chip)
{
	if (OhioReadReg(USBC_ADDR, USBC_INTF_STATUS) & IVBUS_STATUS) {
		anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SRC);
	} else {
		anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SNK);
	}
}

static void usbc_pd_recvd_msg(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = anx7688_pd_process(chip);
	if (rc != CMD_SUCCESS)
		dev_err(cdev, "Recieve Message not Success (%02X)\n", rc);
#ifdef CONFIG_LGE_USB_ANX7688_ADC
	if (!chip->is_pd_connected)
		chip->is_pd_connected = true;
#endif
}

static void usbc_pd_got_power(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int volt, power;
#ifdef CONFIG_LGE_USB_TYPE_C
	int i;
#endif

	volt = OhioReadReg(USBC_ADDR, USBC_RDO_MAX_VOLT);
	power = OhioReadReg(USBC_ADDR, USBC_RDO_MAX_POWER);

	if (volt > 0 && power > 0) {
		chip->curr_max = (power * 500 * 10) / volt;;
		chip->volt_max = volt * 100;;
		chip->charger_type = USBC_PD_CHARGER;
	}
#ifdef CONFIG_LGE_PM
	if (lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO) {
		cancel_delayed_work(&chip->cwork);
		schedule_delayed_work(&chip->cwork, msecs_to_jiffies(0));
	}
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
	for(i = 0 ; i < PD_MAX_PDO_NUM ; i++) {
		if(chip->offered_pdo[i] == 0 || GET_PDO_TYPE(chip->offered_pdo[i]) != 0)
			continue;
		if(GET_PDO_FIXED_VOLT(chip->offered_pdo[i]) == chip->volt_max
				&& GET_PDO_FIXED_CURR(chip->offered_pdo[i]) == chip->curr_max) {
			chip->rdo = RDO_FIXED(i + 1, chip->curr_max, chip->curr_max, 0);
			break;
		}
	}
#endif
	dev_info(cdev, "%s: volt(%dmV), CURR(%dmA)\n", __func__,
			chip->volt_max, chip->curr_max);
}

static void anx7688_alter_work(struct work_struct *work)
{
	struct anx7688_chip *chip =
		container_of(work, struct anx7688_chip, awork);
	struct i2c_client *client = chip->client;
	struct device *cdev = &client->dev;
	long rc;
	int alter = 0;

	if (!atomic_read(&chip->power_on)) {
		rc = wait_for_completion_timeout(&chip->wait_pwr_ctrl, 500);
		if (!rc) {
			dev_info(cdev, "pwr on completion timeout\n");
			/* TODO : power reset need? */
		}
		if (!atomic_read(&chip->power_on))
			return;
	}

	/*
	 * it have chance to fault detect of
	 * debug accessory. this is protection.
	 */
	if (chip->state == STATE_DEBUG_ACCESSORY) {
		anx7688_sbu_ctrl(chip, false);
	}

	mutex_lock(&chip->mlock);
	/*
	 * mi1 have a timeing issue related vbus
	 * wait until all interrupt triggered
	 * correctly.
	 */
	if (chip->pdata->fwver <= MI1_FWVER_RC4) {
		mdelay(100);
	}

	alter = OhioReadReg(USBC_ADDR, USBC_INT_STATUS);
	dev_info(cdev, "INTR:0x%x\n", alter);

	OhioWriteReg(USBC_ADDR, USBC_INT_STATUS, alter & ~alter);
	/* clear interrupt */
	OhioWriteReg(USBC_ADDR, USBC_IRQ_EXT_SOURCE_2, IRQ_EXT_SOFT_RESET_BIT);

	if (alter == 0xFF) {
		/* recovery abnormal status */
		OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0,
				R_OCM_RESET | R_PD_HARDWARE_RESET);
		mdelay(100);
		OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x00);
		return;
	}

	/* check interrupt masked */
	alter = alter & (~USBC_INT_MASK_SET);

	if (alter & CC_STATUS_CHG_INT) {
		usbc_chg_ccstatus(chip);
	}

	/*
	 * Timing about both VBUS and VBCONN interrupt happend
	 * it must enable vbus first, to avoid abormal work at
	 * accessory side
	 */

	if (alter & VBUS_CHG_INT) {
		usbc_chg_vbus(chip);
	}

	if (alter & VCONN_CHG_INT) {
		usbc_chg_vconn(chip);
	}

	if (alter & RECVD_MSG_INT) {
		usbc_pd_recvd_msg(chip);
	}

	if (alter & PR_C_GOT_POWER) {
		usbc_pd_got_power(chip);
	}

	if (alter & DATA_ROLE_CHG_INT) {
		usbc_chg_datarole(chip);
	}

	dual_role_instance_changed(chip->dual_role);
	mutex_unlock(&chip->mlock);
#ifdef CONFIG_LGE_DP_ANX7688
	if (chip->dp_status == ANX_DP_CONNECTED &&
			chip->dp_wq != NULL && &chip->swork != NULL) {
		dev_info(cdev, "start dp work\n");
		chip->dp_status = ANX_DP_RUNNING;
		dp_init_variables(chip);
		queue_delayed_work(chip->dp_wq, &chip->swork, 0);
	}
#endif

	return;
}

static void anx7688_register_init(struct anx7688_chip *chip)
{
	OhioMaskWriteReg(USBC_ADDR, USBC_AUTO_PD_MODE, TRY_SNK_EN, 1);

	/*
	 * USB PD CTS tunning value
	 * this value depend on hardware chariteristics
	 */
#if defined (CONFIG_MACH_MSM8996_ELSA) || defined (CONFIG_MACH_MSM8996_LUCYE) || defined (CONFIG_MACH_MSM8996_ANNA)
	OhioMaskWriteReg(USBC_ADDR, OCM_DEBUG_21, BIT(0)|BIT(1)|BIT(2), 2);
	OhioMaskWriteReg(USBC_ADDR, OCM_DEBUG_20, BIT(0)|BIT(1)|BIT(2), 3);
	OhioMaskWriteReg(USBC_ADDR, OCM_DEBUG_19, BIT(2)|BIT(3), 3);
	OhioMaskWriteReg(USBC_ADDR, OCM_DEBUG_19, BIT(0)|BIT(1), 3);
#endif

	/* unmask interrupt */
	OhioWriteReg(USBC_ADDR, USBC_IRQ_EXT_MASK_2, 0xFF);
	OhioWriteReg(USBC_ADDR, USBC_IRQ_EXT_MASK_2, 0xFB);
	OhioWriteReg(USBC_ADDR, USBC_IRQ_EXT_SOURCE_2, 0xFF);

	/*OHO-439, in AP side, for the interoperability,
	  set the try.UFP period to 0x96*2 = 300ms. */
	OhioWriteReg(USBC_ADDR, USBC_TRY_UFP_TIMER, 0x96);

	OhioWriteReg(USBC_ADDR, USBC_VBUS_DELAY_TIME, 0x19);

	OhioWriteReg(USBC_ADDR, USBC_INT_MASK, USBC_INT_MASK_SET);

	OhioWriteReg(USBC_ADDR, USBC_INT_STATUS, 0x00);

	if (chip->pdata->auto_pd_support) {
		OhioMaskWriteReg(USBC_ADDR, USBC_AUTO_PD_MODE, AUTO_PD_EN, 1);
		OhioWriteReg(USBC_ADDR, USBC_MAX_VOLT_SET,
				chip->pdata->pd_max_volt);
		OhioWriteReg(USBC_ADDR, USBC_MAX_PWR_SET,
				chip->pdata->pd_max_power);
		OhioWriteReg(USBC_ADDR, USBC_MIN_PWR_SET,
				chip->pdata->pd_min_power);
	}

	if (chip->pdata->fwver >= MI1_FWVER_RC5) {
		OhioMaskWriteReg(USBC_ADDR, USBC_AUTO_PD_MODE, SAFE0V_LEVEL,
				chip->pdata->vsafe0v_level);
	}
}

static void anx7688_pd_work(struct work_struct *work)
{
	struct anx7688_chip *chip =
		container_of(work, struct anx7688_chip, pdwork.work);

	anx7688_send_init_setting(chip);
	return;
}

static void anx7688_cd_work(struct work_struct *work)
{
	struct anx7688_chip *chip =
		container_of(work, struct anx7688_chip, dwork);
#ifdef CONFIG_LGE_USB_ANX7688_OVP
	union power_supply_propval prop;
#endif

	mutex_lock(&chip->mlock);
	if (chip->power_ctrl) {
		anx7688_pwr_on(chip);

		anx7688_register_init(chip);
		cancel_delayed_work(&chip->pdwork);
		schedule_delayed_work(&chip->pdwork,
				msecs_to_jiffies(PDINIT_DELAY));
	} else {
		anx7688_detach(chip);
#ifdef CONFIG_LGE_USB_ANX7688_OVP
		prop.intval = 0;
		usbpd_set_property(&chip->usbpd_psy,
				POWER_SUPPLY_PROP_CTYPE_RP, &prop);
#endif
		cancel_delayed_work(&chip->pdwork);
		anx7688_pwr_down(chip);
	}
	chip->deglich_check = false;
	mutex_unlock(&chip->mlock);
	return;
}

static irqreturn_t alter_irq(int irq, void *data)
{
	struct anx7688_chip *chip = (struct anx7688_chip *)data;
	struct device *cdev = &chip->client->dev;
	int ret;

	ret = OhioReadReg(TCPC_ADDR, 0x10);
	if (ret) OhioWriteReg(TCPC_ADDR, 0x10, ret);

	dev_dbg(cdev, "alter_irq\n");
	if (OhioReadReg(USBC_ADDR, USBC_IRQ_EXT_SOURCE_2) &
				IRQ_EXT_SOFT_RESET_BIT) {
		if (!queue_work(chip->cc_wq, &chip->awork))
			dev_err(cdev, "%s: can't alloc work\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t cable_det_irq(int irq, void *data)
{
	struct anx7688_chip *chip = (struct anx7688_chip *)data;
	struct device *cdev = &chip->client->dev;
	unsigned int count = 80;
	unsigned int cable_det_count = 0;

	if (!chip) {
		pr_err("%s : called before init\n", __func__);
		return IRQ_HANDLED;
	}

	wake_lock_timeout(&chip->wlock,
			msecs_to_jiffies(ANX7688_WAKE_LOCK_TIMEOUT));

	do {
		if (gpio_get_value_cansleep(chip->pdata->cdet_gpio)) {
			cable_det_count++;
		}
		if (!chip->deglich_check) {
			if (cable_det_count > CDET_COUNT) {
				chip->deglich_check = true;
				break;
			} else if (count < 70) {
				break;
			}
		} else {
			if (cable_det_count > CDET_DEGLICH)
				break;
		}
		mdelay(1);
	} while (count--);

	if (cable_det_count >= CDET_COUNT) {
		if (atomic_read(&chip->power_on))
                        return IRQ_HANDLED;

		chip->power_ctrl = true;
		dev_info_ratelimited(cdev, "cable plugged\n");
		if (!queue_work(chip->cc_wq, &chip->dwork))
			dev_err_ratelimited(cdev, "%s: can't alloc work\n",
								__func__);
	} else {
		if (!atomic_read(&chip->power_on))
			return IRQ_HANDLED;

		chip->power_ctrl = false;
		dev_info_ratelimited(cdev, "cable unplugged\n");
		if (!queue_work(chip->cc_wq, &chip->dwork))
			dev_err_ratelimited(cdev, "%s: can't alloc work\n",
								__func__);
	}

	mdelay(CDET_DEGLICH_DELAY);
	return IRQ_HANDLED;
}

static bool anx7688_set_src(struct anx7688_chip *chip, unsigned long timeout)
{
	struct device *cdev = &chip->client->dev;
	unsigned long expire;

	wake_lock_timeout(&chip->wlock, msecs_to_jiffies(2000));

	if (!(OhioReadReg(USBC_ADDR, USBC_ANALOG_STATUS) & DFP_OR_UFP)) {
		dev_info(cdev, "same with before role(dfp)\n");
		return true;
	}

	anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SNK);
	mdelay(100);

	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, R_OCM_RESET);
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, CC_SOFT_EN, 1);
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_STATUS, R_TRY_DRP, 1);

	expire = msecs_to_jiffies(timeout) + jiffies;
	while (!(OhioReadReg(USBC_ADDR, USBC_ANALOG_CTRL_7) & 0x0F)) {
		if (time_before(expire, jiffies)) {
			dev_dbg(cdev, "set src timeout\n");
			goto set_src_fail;
		}
	}

	anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_HOST);
	anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SRC);
	chip->mode = DUAL_ROLE_PROP_MODE_DFP;

	dual_role_instance_changed(chip->dual_role);

	mdelay(800);

	if ((OhioReadReg(USBC_ADDR, USBC_ANALOG_STATUS) & DFP_OR_UFP))
		goto set_src_fail;

	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x00);
	mdelay(50);
	anx7688_register_init(chip);
	anx_update_state(chip, STATE_ATTACHED_SRC);

	dev_info(cdev, "set src finished\n");
	return true;

set_src_fail:
	dev_info(cdev, "set src failed\n");
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_STATUS, R_TRY_DRP, 0);
	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x00);
	mdelay(50);
	anx7688_register_init(chip);

	anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_DEVICE);
	anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SNK);
	chip->mode = DUAL_ROLE_PROP_MODE_UFP;

	dual_role_instance_changed(chip->dual_role);

	return false;
}

static bool anx7688_set_snk(struct anx7688_chip *chip, unsigned long timeout)
{
	struct device *cdev = &chip->client->dev;
	unsigned long expire;
	int intp_ctrl;

	wake_lock_timeout(&chip->wlock, msecs_to_jiffies(2000));

	if (OhioReadReg(USBC_ADDR, USBC_ANALOG_STATUS) & DFP_OR_UFP) {
		dev_info(cdev, "same with before role(ufp)\n");
		return true;
	}

	anx7688_vconn_ctrl(chip, false);
	mdelay(100);

	/* disable vconn output */
	intp_ctrl = OhioReadReg(USBC_ADDR, USBC_INTP_CTRL);
	OhioWriteReg(USBC_ADDR, USBC_INTP_CTRL, (intp_ctrl & 0x0F));

	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0,
				R_OCM_RESET | R_PD_HARDWARE_RESET);
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_9, CC_SOFT_EN, 1);
	anx7688_set_mode_role(chip, DUAL_ROLE_PROP_MODE_NONE);
	dual_role_instance_changed(chip->dual_role);
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_STATUS, R_TRY_DRP, 0);

	expire = msecs_to_jiffies(timeout) + jiffies;
	while (!(OhioReadReg(USBC_ADDR, USBC_PWRDN_CTRL) & 0xFC)) {
		if (time_before(expire, jiffies)) {
			dev_dbg(cdev, "set snk timeout\n");
			goto set_snk_fail;
		}
	}

	mdelay(650);

	expire = msecs_to_jiffies(timeout) + jiffies;
	while (!(OhioReadReg(USBC_ADDR, USBC_ANALOG_STATUS) & UFP_PLUG)) {
		if (time_before(expire, jiffies)) {
			dev_dbg(cdev, "wait vbus timeout\n");
			goto set_snk_fail;
		}
	}

	if (!(OhioReadReg(USBC_ADDR, USBC_ANALOG_STATUS) & DFP_OR_UFP))
		goto set_snk_fail;

	anx7688_set_data_role(chip, DUAL_ROLE_PROP_DR_DEVICE);
	anx7688_set_power_role(chip, DUAL_ROLE_PROP_PR_SNK);
	chip->mode = DUAL_ROLE_PROP_MODE_UFP;

	dual_role_instance_changed(chip->dual_role);

	if (chip->pdata->vconn_always_on)
		anx7688_vconn_ctrl(chip, true);

	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x00);
	mdelay(50);
	anx7688_register_init(chip);
	anx_update_state(chip, STATE_ATTACHED_SNK);

	dev_info(cdev, "set snk finished\n");
	return true;

set_snk_fail:
	dev_info(cdev, "set snk failed\n");
	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_STATUS, R_TRY_DRP, 1);
	OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x00);

	OhioMaskWriteReg(USBC_ADDR, USBC_ANALOG_CTRL_6, USBC_R_RP, 1);
	anx7688_register_init(chip);

	return false;
}

static inline int anx7688_set_role(struct anx7688_chip *chip, u8 role)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (chip->role == role) {
		dev_err(cdev, "%s: could not set with same role\n", __func__);
		return 0;
	}

	dev_info(cdev, "%s: role change to %d\n", __func__, role);

	switch (role) {
	case ANX_ROLE_DRP:
		break;
	case ANX_ROLE_SRC:
		rc = anx7688_set_src(chip, SET_ROLE_TIMEOUT);
		break;
	case ANX_ROLE_SNK_ACC:
	case ANX_ROLE_SNK:
	case ANX_ROLE_SRC_OR_SNK:
		rc = anx7688_set_snk(chip, SET_ROLE_TIMEOUT);
		break;
	default:
		dev_err(cdev, "%s: unknown role%d\n", __func__, role);
		rc = -1;
		break;
	}

	return rc;
}

static inline void anx7688_reset_chip(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	gpio_set_value(chip->pdata->rstn_gpio, 0);

	mdelay(10);

	gpio_set_value(chip->pdata->rstn_gpio, 1);

	dev_info(cdev, "reset complete\n");
	return;
}

static int anx7688_check_firmware(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	struct anx7688_firmware *fw;
	ktime_t start_time;
	ktime_t diff;
	int rc = 0;
	int retry = 0;

	if (!atomic_read(&chip->vdd_on))
		anx7688_regulator_ctrl(chip, true);

	anx7688_pwr_on(chip);

	do {
		rc = OhioReadWordReg(USBC_ADDR, OCM_DEBUG_4);
		if (rc < 0) {
			dev_err(cdev, "cannot read fw ver skip update\n");
			goto out1;
		}
		chip->pdata->fwver = ((rc & 0xFF00) >> 8) |
					((rc & 0x00FF) << 8);

		mdelay(5);
	} while ((retry++ < 100) && (rc == 0x00) &&
			chip->pdata->fwver != MI1_NEW_FWVER);

	rc = 0;

	fw = anx7688_firmware_alloc(chip);
	if (ZERO_OR_NULL_PTR(fw)) {
		dev_err(cdev, "firmware alloc failed\n");
		rc = -ENOMEM;
		goto out1;
	}

	rc = anx7688_firmware_open(fw, chip->pdata->fwver);
	if (!is_fw_update_need(fw, chip->pdata->fwver)) {
                dev_info(cdev, "firmware update is not need\n");
                rc = 0;
		anx7688_firmware_release(fw);
		goto out2;
	}

	dev_info(cdev, "fw version: 0x%x"
			"(last version:0x%x)\n",
			chip->pdata->fwver,
			fw->ver);

	dev_info(cdev, "enter firmware update\n");
	start_time = ktime_get();


	rc = anx7688_firmware_update(fw);
	if (rc < 0)
		dev_err(cdev, "firmware update failed %d\n", rc);
	else
		chip->pdata->fwver = fw->ver;

	dev_dbg(cdev, "firmware release\n");
	anx7688_firmware_release(fw);

	diff = ktime_sub(ktime_get(), start_time);
	dev_info(cdev, "firmware update end (%lld ms)\n", ktime_to_ms(diff));

	anx7688_pwr_down(chip);
	mdelay(100);
	anx7688_pwr_on(chip);
out2:
	anx7688_firmware_free(fw);

	chip->pdata->fwver = OhioReadWordReg(USBC_ADDR, OCM_DEBUG_4);
	chip->pdata->fwver = ((chip->pdata->fwver & 0xFF00) >> 8) |
				((chip->pdata->fwver & 0x00FF) << 8);

out1:
	chip->pdata->vid = OhioReadWordReg(USBC_ADDR, USBC_VENDORID);
	chip->pdata->pid = OhioReadWordReg(USBC_ADDR, USBC_DEVICEID);
	chip->pdata->devver = OhioReadReg(USBC_ADDR, USBC_DEVVER);
	dev_info(cdev,  " VID: 0x%x"
			" PID: 0x%x"
			" HWREV: 0x%x"
			" STDALN FW: 0x%x\n",
			chip->pdata->vid,
			chip->pdata->pid,
			chip->pdata->devver,
			chip->pdata->fwver);

	/* clear interrupt status before enable irq */
	OhioWriteReg(USBC_ADDR, USBC_INT_STATUS, 0x00);
	OhioWriteReg(USBC_ADDR, USBC_IRQ_EXT_SOURCE_2, IRQ_EXT_SOFT_RESET_BIT);

	anx7688_pwr_down(chip);

	return rc;
}

static int anx7688_init_gpio(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (gpio_is_valid(chip->pdata->cdet_gpio)) {
		rc = gpio_request_one(chip->pdata->cdet_gpio,
				GPIOF_DIR_IN, "anx7688_cdet_gpio");
		if (rc)
			dev_err(cdev, "unable to request cdet_gpio %d\n",
					chip->pdata->cdet_gpio);
	} else {
		dev_err(cdev, "cdet_gpio %d is not valid\n",
				chip->pdata->cdet_gpio);
		rc = -EINVAL;
		goto err1;
	}

	if (gpio_is_valid(chip->pdata->alter_gpio)) {
		rc = gpio_request_one(chip->pdata->alter_gpio,
				GPIOF_DIR_IN, "anx7688_alter_gpio");
		if (rc)
			dev_err(cdev, "unable to request alter_gpio %d\n",
					chip->pdata->alter_gpio);
	} else {
		dev_err(cdev, "alter_gpio %d is not valid\n",
				chip->pdata->alter_gpio);
		rc = -EINVAL;
		goto err2;
	}

	if (gpio_is_valid(chip->pdata->pwren_gpio)) {
		rc = gpio_request_one(chip->pdata->pwren_gpio,
				GPIOF_OUT_INIT_LOW, "anx7688_pwren_gpio");
		if (rc)
			dev_err(cdev, "unable to request pwren_gpio %d\n",
					chip->pdata->pwren_gpio);
	} else {
		dev_err(cdev, "pwren_gpio %d is not valid\n",
				chip->pdata->pwren_gpio);
		rc = -EINVAL;
		goto err3;
	}

	if (gpio_is_valid(chip->pdata->rstn_gpio)) {
		rc = gpio_request_one(chip->pdata->rstn_gpio,
				GPIOF_OUT_INIT_LOW, "anx7688_rstn_gpio");
		if (rc)
			dev_err(cdev, "unable to request rstn_gpio %d\n",
					chip->pdata->rstn_gpio);
	} else {
		dev_err(cdev, "rstn_gpio %d is not valid\n",
				chip->pdata->rstn_gpio);
		rc = -EINVAL;
		goto err4;
	}

	if (gpio_is_valid(chip->pdata->vconn_gpio)) {
		rc = gpio_request_one(chip->pdata->vconn_gpio,
				GPIOF_DIR_OUT, "anx7688_vconn_gpio");
		if (rc)
			dev_err(cdev, "unable to request vconn_gpio %d\n",
					chip->pdata->vconn_gpio);
	} else {
		dev_err(cdev, "vconn_gpio %d is not valid\n",
				chip->pdata->vconn_gpio);
		rc = -EINVAL;
		goto err5;
	}

	if (gpio_is_valid(chip->pdata->sbu_gpio)) {
		rc = gpio_request_one(chip->pdata->sbu_gpio,
				GPIOF_DIR_OUT, "anx7688_sbu_gpio");
		if (rc)
			dev_err(cdev, "unable to request sbu_gpio %d\n",
					chip->pdata->sbu_gpio);
	} else {
		dev_err(cdev, "sbu_gpio %d is not valid\n",
				chip->pdata->sbu_gpio);
		rc = -EINVAL;
		goto err6;
	}

	return 0;

err6:
	gpio_free(chip->pdata->vconn_gpio);
err5:
	gpio_free(chip->pdata->rstn_gpio);
err4:
	gpio_free(chip->pdata->pwren_gpio);
err3:
	gpio_free(chip->pdata->alter_gpio);
err2:
	gpio_free(chip->pdata->cdet_gpio);
err1:
	if (rc < 0)
		dev_err(cdev, "gpio configure failed rc=%d\n", rc);
	return rc;
}

static void anx7688_free_gpio(struct anx7688_chip *chip)
{
	gpio_free(chip->pdata->sbu_gpio);
	gpio_free(chip->pdata->vconn_gpio);
	gpio_free(chip->pdata->rstn_gpio);
	gpio_free(chip->pdata->pwren_gpio);
	gpio_free(chip->pdata->alter_gpio);
	gpio_free(chip->pdata->cdet_gpio);
}

static int anx7688_init_regulator(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	chip->avdd33 = devm_regulator_get(cdev, "avdd33");
	if (IS_ERR(chip->avdd33)) {
		dev_err(cdev, "regulator avdd33 get failed\n");
		return -EPROBE_DEFER;
	}

	if (regulator_count_voltages(chip->avdd33) > 0) {
		rc = regulator_set_voltage(chip->avdd33, 3300000, 3300000);
		if (rc) {
			dev_err(cdev, "regulator set failed avdd33\n");
			return rc;
		}
	}

	if (chip->pdata->avdd33_ext_ldo) {
		if (gpio_is_valid(chip->pdata->avdd33_gpio)) {
			rc = gpio_request_one(chip->pdata->avdd33_gpio,
					GPIOF_DIR_OUT, "anx7688_avdd33_gpio");
			if (rc)
				dev_err(cdev, "unable to request avdd33_gpio %d\n",
					chip->pdata->avdd33_gpio);
		} else {
			dev_err(cdev, "avdd33_gpio %d is not valid\n",
				chip->pdata->avdd33_gpio);
		}
	}

	chip->avdd10 = devm_regulator_get(cdev, "avdd10");
	if (IS_ERR(chip->avdd10)) {
		dev_err(cdev, "regulator avdd10 get failed\n");
		return -EPROBE_DEFER;
	}

	if (regulator_count_voltages(chip->avdd10) > 0) {
		rc = regulator_set_voltage(chip->avdd10, 1000000, 1000000);
		if (rc) {
			dev_err(cdev, "regulator set failed avdd10");
			return rc;
		}
	}

	chip->vbus_out = devm_regulator_get(cdev, "vbus");
	if (IS_ERR(chip->vbus_out)) {
		dev_err(cdev, "regulator vbus get failed\n");
		return -EPROBE_DEFER;
	}

	if (regulator_count_voltages(chip->vbus_out) > 0) {
		rc = regulator_set_voltage(chip->vbus_out, 5000000, 5000000);
		if (rc) {
			dev_err(cdev, "regulator set failed vbus\n");
			return rc;
		}
	}

	rc = anx7688_regulator_ctrl(chip, true);
	if (rc) {
		dev_err(cdev, "unable to on regulator\n");
		return rc;
	}

	return 0;
}

static int anx7688_parse_dt(struct anx7688_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	struct device_node *np = cdev->of_node;
	struct anx7688_data *data = chip->pdata;
	int rc = 0;

	data->cdet_gpio = of_get_named_gpio(np, "anx7688,cdet-gpio", 0);
	if (data->cdet_gpio < 0) {
		dev_err(cdev, "cdet_gpio is not available\n");
		rc = data->cdet_gpio;
		goto out;
	}

	data->alter_gpio = of_get_named_gpio(np, "anx7688,alter-gpio", 0);
	if (data->alter_gpio < 0) {
		dev_err(cdev, "alter_gpio is not available\n");
		rc = data->alter_gpio;
		goto out;
	}

	data->pwren_gpio = of_get_named_gpio(np, "anx7688,pwren-gpio", 0);
	if (data->pwren_gpio < 0) {
		dev_err(cdev, "pwren_gpio is not available\n");
		rc = data->pwren_gpio;
		goto out;
	}

	data->rstn_gpio = of_get_named_gpio(np, "anx7688,rstn-gpio", 0);
	if (data->rstn_gpio < 0) {
		dev_err(cdev, "rstn_gpio is not available\n");
		rc = data->rstn_gpio;
		goto out;
	}

	data->sbu_gpio = of_get_named_gpio(np, "anx7688,sbu-gpio", 0);
	if (data->sbu_gpio < 0) {
		dev_err(cdev, "sbu_gpio %d is not available\n", data->sbu_gpio);
		rc = data->sbu_gpio;
		goto out;
	}

	data->vconn_gpio = of_get_named_gpio(np, "anx7688,vconn-gpio", 0);
	if (data->vconn_gpio < 0) {
		dev_err(cdev, "rstn_gpio is not available\n");
		rc = data->vconn_gpio;
		goto out;
	}

	data->avdd33_ext_ldo = of_property_read_bool(np,
			"anx7688,avdd33-ext-ldo");

	if (data->avdd33_ext_ldo) {
		data->avdd33_gpio = of_get_named_gpio(np, "anx7688,avdd33en-gpio", 0);
		if (data->avdd33_gpio < 0) {
			dev_err(cdev, "avdd33_gpio is not available\n");
			goto out;
		}
	}

	data->fw_force_update = of_property_read_bool(np,
			"anx7688,fw-force-update");

	data->vconn_always_on = of_property_read_bool(np,
			"anx7688,vconn-always-on");

	data->auto_pd_support = of_property_read_bool(np,
			"anx7688,auto-pd-support");

	/*
	 * power caculation
	 * max volt(100mV per bit): ex) 0x32: 0x32 * 100mV = 5V
	 * max power(500mW per bit) ex) 0x0F: 0x0F * 500mW = 7.5W
	 * min power(500mW per bit) ex) 0x02: 0x02 * 500mW = 1W
	 */
	rc = of_property_read_u32(np, "anx7688,pd-max-volt",
					&data->pd_max_volt);
	if (rc < 0) {
		dev_err(cdev, "max volt is not available\n");
		data->pd_max_volt = 0x32;
	} else {
		data->pd_max_volt = (data->pd_max_volt / 100);
	}

	rc = of_property_read_u32(np, "anx7688,pd-max-power",
					&data->pd_max_power);
	if (rc < 0) {
		dev_err(cdev, "max power is not available\n");
		data->pd_max_power = 0x0F;
	} else {
		data->pd_max_power = (data->pd_max_power / 500);
	}

	rc = of_property_read_u32(np, "anx7688,pd-min-power",
					&data->pd_min_power);
	if (rc < 0) {
		dev_err(cdev, "min power is not available\n");
		data->pd_min_power = 0x02;
	} else {
		data->pd_min_power = (data->pd_min_power / 500);
	}

	dev_info(cdev, " usb pd max volt: %dmV,"
			" max power: %dmW,"
			"  min power: %dmW",
			data->pd_max_volt * 100,
			data->pd_max_power * 500,
			data->pd_min_power * 500);

	rc = of_property_read_u32(np, "anx7688,vsafe0v-level",
					&data->vsafe0v_level);
	if (rc < 0) {
		data->vsafe0v_level = 0;
	}

	return 0;

out:
	return rc;
}

static int anx7688_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
	struct anx7688_chip *chip;
	struct device *cdev = &client->dev;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
	int ret = 0;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_err(cdev, "usb supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		dev_err(cdev, "battery supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(cdev, "smbus data not supported!\n");
		return -EIO;
	}

	chip = devm_kzalloc(cdev, sizeof(struct anx7688_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(cdev, "can't alloc anx7688_chip\n");
		return -ENOMEM;
	}

	chip->client = client;
	ohio_client = chip->client;
	i2c_set_clientdata(client, chip);

	if (&client->dev.of_node) {
		struct anx7688_data *data = devm_kzalloc(cdev,
				sizeof(struct anx7688_data), GFP_KERNEL);

		if (!data) {
			dev_err(cdev, "can't alloc anx7688_data\n");
			ret = -ENOMEM;
			goto err1;
		}

		chip->pdata = data;

		ret = anx7688_parse_dt(chip);
		if (ret) {
			dev_err(cdev, "can't parse dt\n");
			goto err2;
		}
	} else {
		chip->pdata = client->dev.platform_data;
	}

	ret = anx7688_init_regulator(chip);
	if (ret) {
		dev_err(cdev, "regulator init fail\n");
		goto err2;
	}

	ret = anx7688_init_gpio(chip);
	if (ret) {
		dev_err(cdev, "fail to init gpio\n");
		goto err2;
	}

	ret = anx7688_check_firmware(chip);
	if (ret) {
		dev_err(cdev, "fail to request firmware\n");
                goto err3;
	}

	ret = dp_create_sysfs_interface(&client->dev);
	if (ret < 0) {
		dev_err(cdev, "sysfs create failed\n");
		goto err3;
	}

	chip->usb_psy = usb_psy;
	chip->batt_psy = batt_psy;

	/* support DRP on standby mode */
	chip->role = ANX_ROLE_DRP;
	chip->cc1 = CC_OPEN;
	chip->cc2 = CC_OPEN;

	chip->is_vconn_on = false;
	chip->is_sbu_switched = false;
#ifdef CONFIG_LGE_USB_ANX7688_ADC
	chip->is_pd_connected = false;
#endif

	if (chip->pdata->vconn_always_on)
		anx7688_vconn_ctrl(chip, true);

	chip->cc_wq = alloc_ordered_workqueue("anx7688-usbc-wq", WQ_HIGHPRI);
	if (!chip->cc_wq) {
		dev_err(cdev, "unable to create workqueue anx7688-wq\n");
		goto err3;
	}
	INIT_WORK(&chip->dwork, anx7688_cd_work);
	INIT_WORK(&chip->awork, anx7688_alter_work);
	INIT_DELAYED_WORK(&chip->cwork, anx7688_ctype_work);
	INIT_DELAYED_WORK(&chip->pdwork, anx7688_pd_work);
	wake_lock_init(&chip->wlock, WAKE_LOCK_SUSPEND, "anx7688_wake");
#ifdef CONFIG_LGE_DP_ANX7688
	wake_lock_init(&chip->dp_lock, WAKE_LOCK_SUSPEND, "dp_wake");
#endif
	mutex_init(&chip->mlock);

#ifdef CONFIG_LGE_DP_ANX7688
	chip->dp_status = ANX_DP_DISCONNECTED;

	ret = init_anx7688_dp(cdev, chip);
	if (ret < 0)
		dev_err(cdev, "failed to init anx7688 dp\n");
#endif

	chip->cdet_irq = gpio_to_irq(chip->pdata->cdet_gpio);
	if (chip->cdet_irq < 0) {
		dev_err(cdev, "could not register cdet_irq\n");
		ret = -ENXIO;
		goto err3;
	}

	ret = devm_request_threaded_irq(&client->dev, chip->cdet_irq,
			NULL,
			cable_det_irq,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"cable_det_irq", chip);
	if (ret) {
		dev_err(cdev, "failed to reqeust cdet IRQ\n");
		goto err4;
	}

	chip->alter_irq = gpio_to_irq(chip->pdata->alter_gpio);
	if (chip->alter_irq < 0) {
		dev_err(cdev, "could not register alter_irq\n");
		ret = -ENXIO;
		goto err4;
	}

	ret= devm_request_threaded_irq(&client->dev, chip->alter_irq,
			NULL,
			alter_irq,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"alter_irq", chip);
	if (ret) {
		dev_err(cdev, "failed to reqeust alter IRQ\n");
		goto err5;
	}


	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		desc = devm_kzalloc(cdev, sizeof(struct dual_role_phy_desc),
				GFP_KERNEL);
		if (!desc) {
			dev_err(cdev, "unable to allocate dual role descriptor\n");
			goto err6;
		}

		desc->name = "otg_default";
		desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		desc->get_property = dual_role_get_prop;
		desc->set_property = dual_role_set_prop;
		desc->properties = drp_properties;
		desc->num_properties = ARRAY_SIZE(drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(cdev, desc);
		dual_role->drv_data = chip->client;
		chip->dual_role = dual_role;
		chip->desc = desc;

		chip->mode = DUAL_ROLE_PROP_MODE_NONE;
		chip->power_role = DUAL_ROLE_PROP_PR_NONE;
		chip->data_role = DUAL_ROLE_PROP_DR_NONE;
	}

	if (IS_ENABLED(CONFIG_POWER_SUPPLY)) {
		chip->usbpd_psy.name = "usb_pd";
		chip->usbpd_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
		chip->usbpd_psy.get_property = usbpd_get_property;
		chip->usbpd_psy.set_property = usbpd_set_property;
		chip->usbpd_psy.property_is_writeable = usbpd_is_writeable;
		chip->usbpd_psy.properties = usbpd_properties;
		chip->usbpd_psy.num_properties = ARRAY_SIZE(usbpd_properties);
		chip->usbpd_psy.supplied_to = usbpd_supplicants;
		chip->usbpd_psy.num_supplicants =ARRAY_SIZE(usbpd_supplicants);

		ret = power_supply_register(cdev, &chip->usbpd_psy);
		if (ret < 0) {
			dev_err(cdev, "unalbe to register psy rc = %d\n", ret);
			goto err7;
		}
	}

	ret = anx7688_debugfs_init(chip);
	if (ret)
		dev_dbg(cdev, "debugfs is not available\n");

	enable_irq_wake(chip->cdet_irq);
	enable_irq_wake(chip->alter_irq);

	if (!!gpio_get_value_cansleep(chip->pdata->cdet_gpio)) {
		chip->power_ctrl = true;
		queue_work(chip->cc_wq, &chip->dwork);
	}

	schedule_delayed_work(&chip->cwork, msecs_to_jiffies(5000));

	return 0;
err7:
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(cdev, chip->dual_role);
		devm_kfree(cdev, chip->desc);
	}
err6:
	if (chip->alter_irq > 0)
		devm_free_irq(cdev, chip->alter_irq, chip);
err5:
	if (chip->cdet_irq > 0)
		devm_free_irq(cdev, chip->cdet_irq, chip);
err4:
	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	wake_lock_destroy(&chip->wlock);
#ifdef CONFIG_LGE_DP_ANX7688
	wake_lock_destroy(&chip->dp_lock);
#endif
err3:
	anx7688_free_gpio(chip);
	if (chip->pdata->avdd33_ext_ldo) {
		if (gpio_is_valid(chip->pdata->avdd33_gpio)) {
			gpio_set_value(chip->pdata->avdd33_gpio, 0);
		}
		gpio_free(chip->pdata->avdd33_gpio);
	}
err2:
	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);
err1:
	i2c_set_clientdata(client, NULL);
	ohio_client = NULL;
	devm_kfree(cdev, chip);
	return ret;
}

static int anx7688_remove(struct i2c_client *client)
{
	struct anx7688_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	if (!chip) {
		pr_err("%s : chip is null\n", __func__);
		return -ENODEV;
	}

	if (chip->pdata->vconn_always_on) {
		chip->pdata->vconn_always_on = false;
		anx7688_vconn_ctrl(chip, false);
	}

	if (IS_ENABLED(CONFIG_POWER_SUPPLY)) {
		if (chip->usbpd_psy.dev)
			power_supply_unregister(&chip->usbpd_psy);
	}
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(cdev, chip->dual_role);
		devm_kfree(cdev, chip->desc);
	}

	anx7688_debugfs_cleanup();

	if (chip->alter_irq > 0)
		devm_free_irq(cdev, chip->alter_irq, chip);

	if (chip->cdet_irq > 0)
		devm_free_irq(cdev, chip->cdet_irq, chip);

	cancel_delayed_work(&chip->cwork);
	cancel_delayed_work(&chip->pdwork);
	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	wake_lock_destroy(&chip->wlock);
#ifdef CONFIG_LGE_DP_ANX7688
	wake_lock_destroy(&chip->dp_lock);
#endif
	anx7688_free_gpio(chip);
	regulator_put(chip->avdd33);
	regulator_put(chip->avdd10);

	if (chip->pdata->avdd33_ext_ldo) {
		if (gpio_is_valid(chip->pdata->avdd33_gpio)) {
			gpio_set_value(chip->pdata->avdd33_gpio, 0);
		}
		gpio_free(chip->pdata->avdd33_gpio);
	}

	if (&client->dev.of_node)
		devm_kfree(cdev, chip->pdata);

	ohio_client = NULL;
	i2c_set_clientdata(client, NULL);
#ifdef CONFIG_LGE_DP_ANX7688
	dp_variables_remove(cdev);
#endif
	devm_kfree(cdev, chip);

	return 0;
}

static void anx7688_shutdown(struct i2c_client *client)
{
	return;
}

#ifdef CONFIG_PM
static int anx7688_suspend(struct device *dev)
{
	return 0;
}

static int anx7688_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops anx7688_dev_pm_ops = {
	.suspend = anx7688_suspend,
	.resume  = anx7688_resume,
};
#endif

static const struct i2c_device_id anx7688_id_table[] = {
	{"anx7688-usbc", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, anx7688_id_table);

#ifdef CONFIG_OF
static struct of_device_id anx7688_match_table[] = {
	{ .compatible = "analogix,anx7688-usbc",},
	{ },
};
#else
#define anx7688_match_table NULL
#endif

static struct i2c_driver anx7688_i2c_driver = {
	.driver = {
		.name = "anx7688-usbc",
		.owner = THIS_MODULE,
		.of_match_table = anx7688_match_table,
#ifdef CONFIG_PM
		.pm = &anx7688_dev_pm_ops,
#endif
	},
	.probe = anx7688_probe,
	.remove = anx7688_remove,
	.shutdown = anx7688_shutdown,
	.id_table = anx7688_id_table,
};

static __init int anx7688_i2c_init(void)
{
	return i2c_add_driver(&anx7688_i2c_driver);
}

static __exit void anx7688_i2c_exit(void)
{
	i2c_del_driver(&anx7688_i2c_driver);
}

module_init(anx7688_i2c_init);
module_exit(anx7688_i2c_exit);

MODULE_DESCRIPTION("I2C bus driver for ANX7688 USB Type-C Standalone");
MODULE_AUTHOR("Bongju Kim <jude84.kim@lge.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.4");
