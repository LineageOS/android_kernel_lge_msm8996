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
#include <linux/regulator/consumer.h>

#include <soc/qcom/lge/board_lge.h>

#include "anx7418.h"
#include "anx7418_firmware.h"
#include "anx7418_pd.h"
#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include "anx7418_drp.c"
#endif
#include "anx7418_debugfs.h"
#include "anx7418_sysfs.h"

#define CHG_WORK_DELAY 5000
#define OCM_STARTUP_TIMEOUT 3200

unsigned int dfp;
module_param(dfp, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dfp, "FORCED DFP MODE");

static int intf_irq_mask = 0xFF;

static char *chg_supplicants[] = {
	"battery",
};

static enum power_supply_property chg_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_TYPE,
};

static const char *chg_to_string(enum power_supply_type type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_TYPEC:
		return "USB Type-C Charger";
	case POWER_SUPPLY_TYPE_USB_PD:
		return "USB Type-C PD Charger";
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		return "Quick Charge 2.0";
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		return "Quick Charge 3.0";
	default:
		return "Unknown Charger";
	}
}

int set_property_on_battery(struct anx7418 *anx,
			enum power_supply_property prop)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!anx->batt_psy) {
		anx->batt_psy = power_supply_get_by_name("battery");
		if (!anx->batt_psy) {
			pr_err("no batt psy found\n");
			return -ENODEV;
		}
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		ret.intval = anx->curr_max;
		rc = power_supply_set_property(anx->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &ret);
		if (rc)
			pr_err("failed to set current max rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		/*
		 * Notify the typec mode to charger. This is useful in the DFP
		 * case where there is no notification of OTG insertion to the
		 * charger driver.
		 */
		if (anx->mode == DUAL_ROLE_PROP_MODE_UFP)
			ret.intval = POWER_SUPPLY_TYPE_UFP;
		else if (anx->mode == DUAL_ROLE_PROP_MODE_DFP)
			ret.intval = POWER_SUPPLY_TYPE_DFP;
		else
			ret.intval = POWER_SUPPLY_TYPE_UNKNOWN;

		rc = power_supply_set_property(anx->batt_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &ret);
		if (rc)
			pr_err("failed to set typec mode rc=%d\n", rc);
		break;
	default:
		pr_err("invalid request\n");
		rc = -EINVAL;
	}

	return rc;
}

static int chg_get_property(struct power_supply *psy,
			enum power_supply_property prop,
			union power_supply_propval *val)
{
	struct anx7418 *anx = power_supply_get_drvdata(psy);
	struct device *cdev = &anx->client->dev;
	int rc;

	if (!anx) /* Don't check the psy if it doesn't exist yet. */
		return -ENODEV;

	switch(prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		val->intval = anx->is_otg;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = anx->is_present;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = anx->volt_max;
		break;

	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = anx->curr_max;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		if (anx->mode == DUAL_ROLE_PROP_MODE_UFP)
			val->intval = POWER_SUPPLY_TYPE_UFP;
		else if (anx->mode == DUAL_ROLE_PROP_MODE_DFP)
			val->intval = POWER_SUPPLY_TYPE_DFP;
		else
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;

	case POWER_SUPPLY_PROP_TYPEC_MODE:
		dev_dbg(cdev, "%s: get typec mode\n", __func__);
		val->intval = anx->pd_psy_d.type;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int chg_set_property(struct power_supply *psy,
			enum power_supply_property prop,
			const union power_supply_propval *val)
{
	struct anx7418 *anx = power_supply_get_drvdata(psy);
	struct device *cdev = &anx->client->dev;

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		if (anx->is_dbg_acc || anx->mode == DUAL_ROLE_PROP_MODE_NONE) {
			if (val->intval) {
				dev_dbg(cdev, "power on by charger\n");
				anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
			} else {
				if (anx->dr == DUAL_ROLE_PROP_DR_DEVICE) {
					dev_dbg(cdev, "power down by charger\n");
					anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);
				} else if (anx->dr == DUAL_ROLE_PROP_DR_NONE) {
					dev_dbg(cdev, "PRESENT=false, DR=none\n");
				}
			}
		}

		if (anx->is_present == val->intval)
			break;
		anx->is_present = val->intval;
		dev_dbg(cdev, "%s: is_present(%d)\n", __func__, anx->is_present);

		if (anx->is_present) {
			schedule_delayed_work(&anx->chg_work,
					msecs_to_jiffies(CHG_WORK_DELAY));
		} else {
			cancel_delayed_work(&anx->chg_work);
			anx->curr_max = 0;
			anx->volt_max = 0;
			anx->ctype_charger = ANX7418_UNKNOWN_CHARGER;
		}
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		anx->volt_max = val->intval;
		dev_dbg(cdev, "%s: volt_max(%dmV)\n", __func__, anx->volt_max);
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		anx->curr_max = val->intval;
		dev_dbg(cdev, "%s: curr_max(%dmA)\n", __func__, anx->curr_max);
		break;

	case POWER_SUPPLY_PROP_TYPE:
		if (val->intval == anx->pd_psy_d.type) {
			dev_dbg(cdev, "%s: type already set (%s)\n", __func__,
						chg_to_string(anx->pd_psy_d.type));
			break;
		}
		switch (val->intval) {
		case POWER_SUPPLY_TYPE_TYPEC:
		case POWER_SUPPLY_TYPE_USB_PD:
		case POWER_SUPPLY_TYPE_USB_HVDCP:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3:
			anx->pd_psy_d.type = val->intval;
			break;
		case POWER_SUPPLY_TYPE_USB:
			if (anx->pd_psy_d.type == POWER_SUPPLY_TYPE_UNKNOWN)
				break;
			dev_dbg(cdev, "%s: type USB - setting UNKNOWN\n", __func__);
		default:
			anx->pd_psy_d.type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		}
		dev_dbg(cdev, "%s: type(%s)\n", __func__, chg_to_string(anx->pd_psy_d.type));

		break;

	case POWER_SUPPLY_PROP_TYPEC_MODE:
		/* Fall through */
	default:
		return -EINVAL;
	}

	return 0;
}

int anx7418_set_mode(struct anx7418 *anx, int mode)
{
	struct device *cdev = &anx->client->dev;

	if (anx->mode == mode)
		return 0;

	switch (mode) {
	case DUAL_ROLE_PROP_MODE_UFP:
	case DUAL_ROLE_PROP_MODE_DFP:
		anx->is_tried_snk = false;

	case DUAL_ROLE_PROP_MODE_NONE:
		break;

	default:
		dev_err(cdev, "%s: unknown mode %d\n", __func__, mode);
		return -1;
	}

	anx->mode = mode;
	set_property_on_battery(anx, POWER_SUPPLY_PROP_TYPEC_MODE);

	dev_dbg(cdev, "%s(%d)\n", __func__, mode);
	return 0;
}

int anx7418_set_pr(struct anx7418 *anx, int pr)
{
	struct device *cdev = &anx->client->dev;

	if (anx->pr == pr)
		return 0;

	switch (pr) {
	case DUAL_ROLE_PROP_PR_SRC:
#ifdef CONFIG_LGE_USB_TYPE_C
		if (!IS_INTF_IRQ_SUPPORT(anx))
			anx->is_otg = 1;
#endif
		break;

	case DUAL_ROLE_PROP_PR_SNK:
#ifdef CONFIG_LGE_USB_TYPE_C
			anx->is_otg = 0;
#endif
		break;

	case DUAL_ROLE_PROP_PR_NONE:
#ifdef CONFIG_LGE_USB_TYPE_C
			anx->is_otg = 0;
#endif
		break;

	default:
		dev_err(cdev, "%s: unknown pr %d\n", __func__, pr);
		return -1;
	}

	anx->pr = pr;

	dev_dbg(cdev, "%s(%d)\n", __func__, pr);
	return 0;
}

int anx7418_set_dr(struct anx7418 *anx, int dr)
{
	struct device *cdev = &anx->client->dev;

	if (anx->dr == dr)
		return 0;

	switch (dr) {
	case DUAL_ROLE_PROP_DR_HOST:
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);

#ifdef CONFIG_LGE_USB_TYPE_C
		anx->is_otg = 1;
#endif
		break;

	case DUAL_ROLE_PROP_DR_DEVICE:
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);

/* #ifdef CONFIG_LGE_USB_TYPE_C
		power_supply_set_present(anx->usb_psy, 1);
#endif */
		break;

	case DUAL_ROLE_PROP_DR_NONE:
#ifdef CONFIG_LGE_USB_TYPE_C
		if (anx->dr == DUAL_ROLE_PROP_DR_HOST)
			anx->is_otg = 0;

		/* if (anx->dr == DUAL_ROLE_PROP_DR_DEVICE)
			power_supply_set_present(anx->usb_psy, 0); */
#endif
		break;

	default:
		dev_err(cdev, "%s: unknown dr %d\n", __func__, dr);
		return -1;
	}

	anx->dr = dr;

	dev_dbg(cdev, "%s(%d)\n", __func__, dr);
	return 0;
}

int anx7418_reg_init(struct anx7418 *anx)
{
	struct i2c_client *client = anx->client;

	if (!anx->otp)
		return 0;

	anx7418_i2c_lock(client);

	/* Interface and Status Interrupt Mask. Offset : 0x17
	 * 0 : RECVD_MSG_INT_MASK
	 * 1 : Reserved
	 * 2 : VCONN_CHG_INT_MASK
	 * 3 : VBUS_CHG_INT_MASK
	 * 4 : CC_STATUS_CHG_INT_MASK
	 * 5 : DATA_ROLE_CHG_INT_MASK
	 * 6 : Reserved
	 * 7 : Reserved
	 */
	if (IS_INTF_IRQ_SUPPORT(anx)) {
		if (anx->rom_ver >= 0x12) {
			intf_irq_mask = 0x83;

			/* AUTO-PD */
			__anx7418_write_reg(client, MAX_VOLT_RDO, 0x5A);
			__anx7418_write_reg(client, MAX_POWER_SYSTEM, 0x24);
			__anx7418_write_reg(client, MIN_POWER_SYSTEM, 0x06);
			__anx7418_write_reg(client, FUNCTION_OPTION,
					__anx7418_read_reg(client, FUNCTION_OPTION) | AUTO_PD_EN);

			/*
			 * 1:0 : To control the timing between PS_RDY and
			 *       vbus on during PR_SWAP.
			 *       0x00: 50ms; 0x01: 100ms; 0x02: 150ms; 0x03: 200ms
			 * 3:2 : To control the timing between PS_RDY and
			 *       vbus off during PR_SWAP.
			 *       0x00: 50ms; 0x01: 100ms; 0x02: 150ms; 0x03: 200ms
			 * 5:4 : To control the timing between the first cc message
			 *       and vbus on.
			 *       0x00: 10ms; 0x01: 40ms; 0x02: 70ms; 0x03: 100ms
			 */
			__anx7418_write_reg(client, TIME_CONTROL, 0x18);

			/* skip check vbus */
			__anx7418_write_reg(client, 0x6E,
					__anx7418_read_reg(client, 0x6E) | 1);
		} else {
			intf_irq_mask = 0xC2;
		}

		__anx7418_write_reg(client, IRQ_INTF_MASK, intf_irq_mask);

#ifndef PD_CTS_TEST
		/* in AP side, for the interoperability,
		 * set the try.UFP period to 0x96*2 = 300ms. */
		__anx7418_write_reg(client, TRY_UFP_TIMER, 0x96);
#endif
	}

	anx7418_i2c_unlock(client);

	return 0;
}

int __anx7418_pwr_on(struct anx7418 *anx)
{
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	int i;
	int rc;

	gpio_set_value(anx->vconn_gpio, 1);

	gpio_set_value(anx->pwr_en_gpio, 1);
	anx_dbg_event("PWR EN", 1);
	mdelay(10);

	gpio_set_value(anx->resetn_gpio, 1);
	anx_dbg_event("RESETN", 1);
	mdelay(5);

	for (i = 0; i < OCM_STARTUP_TIMEOUT; i++) {
		rc = i2c_smbus_read_byte_data(client, TX_STATUS);
		if (rc < 0) {
			/* it seemed to be EEPROM. */
			mdelay(OCM_STARTUP_TIMEOUT);
			rc = i2c_smbus_read_byte_data(client, TX_STATUS);
			if (rc < 0) {
				i = OCM_STARTUP_TIMEOUT;
				break;
			}
		}
		if (rc > 0 && rc & OCM_STARTUP) {
			break;
		}
		mdelay(1);
	}
	anx_dbg_event("OCM STARTUP", rc);

	atomic_set(&anx->pwr_on, 1);
	dev_info_ratelimited(cdev, "anx7418 power on\n");

	return i >= OCM_STARTUP_TIMEOUT ? -EIO : 0;
}

void __anx7418_pwr_down(struct anx7418 *anx)
{
	struct device *cdev = &anx->client->dev;

	atomic_set(&anx->pwr_on, 0);
	dev_info_ratelimited(cdev, "anx7418 power down\n");

	gpio_set_value(anx->resetn_gpio, 0);
	anx_dbg_event("RESETN", 0);
	mdelay(1);

	gpio_set_value(anx->pwr_en_gpio, 0);
	anx_dbg_event("PWR EN", 0);
	mdelay(1);

	gpio_set_value(anx->vconn_gpio, 0);
}

int anx7418_pwr_on(struct anx7418 *anx, int is_on)
{
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	int rc = 0;
#if 0 /* CONFIG_LGE_USB_TYPE_C START */
	union power_supply_propval prop;
#endif /* CONFIG_LGE_USB_TYPE_C END */

	dev_info_ratelimited(cdev, "%s(%d)\n", __func__, is_on);

	if (!is_on && anx->is_dbg_acc) {
		gpio_set_value(anx->sbu_sel_gpio, 0);

		anx->is_dbg_acc = false;
	}

	down_write(&anx->rwsem);

	if (atomic_read(&anx->pwr_on) == is_on) {
		dev_dbg(cdev, "anx7418 power is already %s\n",
				is_on ? "on" : "down");
		up_write(&anx->rwsem);
		return 0;
	}

	if (is_on) {
		rc = __anx7418_pwr_on(anx);
		if (rc < 0)
			goto set_as_ufp;

		anx_dbg_event("INIT START", 0);

		/* init reg */
		anx7418_reg_init(anx);

		/* init PD */
		anx7418_pd_init(anx);

		anx_dbg_event("INIT DONE", 0);

		if (dfp)
			goto set_as_dfp;

		if (IS_INTF_IRQ_SUPPORT(anx)) {
			goto out;
		}

		/*
		 * Check DFP or UFP
		 */
		rc = anx7418_read_reg(client, ANALOG_STATUS);

		if (rc & DFP_OR_UFP) { /* UFP */
set_as_ufp:
			dev_info(cdev, "%s: set as UFP\n", __func__);
			anx_dbg_event("UFP", 0);

			anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_UFP);
			anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
			anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
		} else { /* DFP */

			/* Todo fix Apple multiport adapter..
			 * If CC1/2 lines are connected with Rd/Rd,
			 * it's Debug Accesory Mode.
			 * By the way, Apple type-c multiport adapter has
			 * Rd/Rd and.. also Ra in CC line..
			 * It's non-standard about type-c from usb.org
			 */
			rc = anx7418_read_reg(client, ANALOG_CTRL_7);
			if ((rc & CC1_5P1K) && (rc & CC2_5P1K)) {
				if (rc & (CC1_RA | CC2_RA)) {
					dev_dbg(cdev, "Debug Accessory Mode with Ra\n");
				} else {
					dev_info(cdev, "Debug Accessory Mode\n");

					/* Connected Rd/Rd in CC1/2 is a factory cable.
					 * In this case, we need to check SBU line which
					 * is connected with a register for distinguish
					 * factory cables by switch SBU_SEL pin.
					 */
#if 0 /* CONFIG_LGE_USB_TYPE_C START */
					/* Check vbus on? */
					power_supply_get_property(anx->usb_psy,
							POWER_SUPPLY_PROP_DP_DM, &prop);
					if (prop.intval != POWER_SUPPLY_DP_DM_DPF_DMF) {
						/* vbus not detected */
						dev_err(cdev, "vbus is not detected. ignore it\n");
						__anx7418_pwr_down(anx);
						goto out;
					}
#endif /* CONFIG_LGE_USB_TYPE_C END */
					gpio_set_value(anx->sbu_sel_gpio, 1);

					anx->is_dbg_acc = true;
					goto set_as_ufp;
				}
			}

set_as_dfp:
			dev_info(cdev, "%s: set as DFP\n", __func__);
			anx_dbg_event("DFP", 0);

			anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_DFP);
			anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SRC);
			anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);
		}
#ifdef CONFIG_DUAL_ROLE_USB_INTF
		dual_role_instance_changed(anx->dual_role);
#endif
	} else {
		__anx7418_pwr_down(anx);
		anx->is_tried_snk = false;

		anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_NONE);
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);
		if (anx->mode != DUAL_ROLE_PROP_MODE_NONE) {
			anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_NONE);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
			dual_role_instance_changed(anx->dual_role);
#endif
		}
	}

out:
	up_write(&anx->rwsem);
	return 0;
}

static void i2c_irq_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w, struct anx7418, i2c_irq_work);
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	int irq;
	int status;
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	int dual_role_changed = false;
#endif
	int rc;

	down_read(&anx->rwsem);

	if (!atomic_read(&anx->pwr_on)) {
		anx_dbg_event("I2C IRQ", -1);
		goto out;
	}

	anx7418_i2c_lock(client);

	rc = __anx7418_read_reg(client, IRQ_EXT_SOURCE_2);
	anx_dbg_event("I2C IRQ", rc);

	if (!IS_INTF_IRQ_SUPPORT(anx)) {
		anx7418_i2c_unlock(client);
		rc = anx7418_pd_process(anx);
		if (rc == -ECANCELED)
			__anx7418_write_reg(client, IRQ_EXT_MASK_2, 0xFF);
		anx7418_i2c_lock(client);

		goto done;
	}

	irq = __anx7418_read_reg(client, IRQ_INTF_STATUS);
	status = __anx7418_read_reg(client, INTF_STATUS);
	dev_dbg(&client->dev, "IRQ_INTF_STATUS(%02X), INTF_STATUS(%02X)\n",
			irq, status);
	anx_dbg_event("INTF IRQ", irq);
	anx_dbg_event("INTF STATUS", status);

	if (!(intf_irq_mask & RECVD_MSG) && (irq & RECVD_MSG)) {
		anx7418_i2c_unlock(client);
		rc = anx7418_pd_process(anx);
		anx7418_i2c_lock(client);

		if (rc == -ECANCELED)
			__anx7418_write_reg(client, IRQ_EXT_MASK_2, 0xFF);

		irq &= ~RECVD_MSG;
	}

	if (!(intf_irq_mask & CC_STATUS_CHG) && (irq & CC_STATUS_CHG)) {
		rc = __anx7418_read_reg(client, CC_STATUS);
		dev_dbg(cdev, "%s: CC_STATUS(%02X)\n", __func__, rc);

		if (anx->is_tried_snk)
			__anx7418_write_reg(client, 0x47,
					__anx7418_read_reg(client, 0x47) & 0xFE);

		if (anx->mode == DUAL_ROLE_PROP_MODE_NONE) {
			if (rc & 0xCC) {
				/* UFP */
				dev_info(cdev, "%s: set as UFP\n", __func__);
				anx_dbg_event("UFP", 0);

				anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_UFP);
				anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
				anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
				dual_role_changed = true;
#endif
			} else if (rc == 0x11) {
				/* Debug Accessory Mode */
				dev_info(cdev, "%s: Debug Accessory Mode\n", __func__);
				anx_dbg_event("Debug Accessory", 0);
				gpio_set_value(anx->sbu_sel_gpio, 1);

				anx->is_dbg_acc = true;

				anx7418_i2c_unlock(client);
				__anx7418_pwr_down(anx);
				goto out;
			} else if (rc == 0x00) {
				/* No Mode Set = CC Open */
				dev_dbg(cdev, "%s: CC Open\n", __func__);
				anx_dbg_event("CC Open", 0);
				__anx7418_write_reg(client, IRQ_INTF_STATUS,
						irq & intf_irq_mask);
				goto done;
			} else if (rc == 0x22) {
				/* Audio Accessory Mode */
				dev_info(cdev, "%s: Audio Accessory Mode\n", __func__);
				anx_dbg_event("Audio Accessory", 0);
				__anx7418_write_reg(client, IRQ_INTF_STATUS,
						irq & intf_irq_mask);
				goto done;
			} else {
				/* Try using SNK (UFP) Mode First */
				if (!anx->is_tried_snk && !lge_get_factory_boot()) {
					dev_dbg(cdev, "%s: try_snk\n", __func__);

					anx7418_i2c_unlock(client);
					rc = try_snk(anx, TRY_ROLE_TIMEOUT);
					anx7418_i2c_lock(client);

					__anx7418_write_reg(client, IRQ_INTF_STATUS,
							irq & intf_irq_mask);
					anx->is_tried_snk = true;
					goto done;
				}
				/* DFP */
				dev_info(cdev, "%s: set as DFP\n", __func__);
				anx_dbg_event("DFP", 0);

				anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_DFP);
				anx->is_otg = 1;
				anx->pr = DUAL_ROLE_PROP_PR_SRC;

				anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
				dual_role_changed = true;
#endif
			}
		} else {
			if (anx->pr == DUAL_ROLE_PROP_PR_SNK) {
				if (rc & 0xCC) {
					/* Rp advertisement */
					schedule_delayed_work(&anx->chg_work, 0);
				}
			}
		}

		irq &= ~CC_STATUS_CHG;
	}

	if (!(intf_irq_mask & VBUS_CHG) && (irq & VBUS_CHG)) {
		if (status & VBUS_STATUS) {
			dev_dbg(cdev, "%s: VBUS ON\n", __func__);
			anx->is_otg = 1;
			anx->pr = DUAL_ROLE_PROP_PR_SRC;
		} else {
			dev_dbg(cdev, "%s: VBUS OFF\n", __func__);
			anx->is_otg = 0;
			anx->pr = DUAL_ROLE_PROP_PR_SNK;
		}
#ifdef CONFIG_DUAL_ROLE_USB_INTF
		dual_role_changed = true;
#endif
		irq &= ~VBUS_CHG;
	}

	if (!(intf_irq_mask & VCONN_CHG) && (irq & VCONN_CHG)) {
		if (status & VCONN_STATUS) {
			dev_dbg(cdev, "%s: VCONN ON\n", __func__);
			anx_dbg_event("VCONN", 1);
		} else {
			dev_dbg(cdev, "%s: VCONN OFF\n", __func__);
			anx_dbg_event("VCONN", 0);
		}
		irq &= ~VCONN_CHG;
	}

	if (!(intf_irq_mask & DATA_ROLE_CHG) && (irq & DATA_ROLE_CHG)) {
		if (anx->mode == DUAL_ROLE_PROP_MODE_NONE) {
			/*
			 * FIXME
			 * Ignore data role at abnormal case (Vbus + Rd)
			 */
			__anx7418_write_reg(client, IRQ_INTF_MASK, 0xFF);

		} else if (status & DATA_ROLE) {
			rc = __anx7418_read_reg(client, ANALOG_CTRL_7);
			if ( (rc & 0x0F) == 0x05) { /* CC1_Rd and CC2_Rd */

				dev_info(cdev, "%s: Debug Accessory Mode\n", __func__);
				anx_dbg_event("Debug Accessory", 0);
				gpio_set_value(anx->sbu_sel_gpio, 1);

				anx->is_dbg_acc = true;

				anx7418_i2c_unlock(client);
				__anx7418_pwr_down(anx);
				goto out;
			}

			dev_info(cdev, "%s: DFP\n", __func__);
			anx_dbg_event("DFP", 0);
			if (anx->mode == DUAL_ROLE_PROP_MODE_NONE)
				anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_DFP);
			if (anx->pr == DUAL_ROLE_PROP_PR_NONE)
				anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SRC);
			anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);

		} else {
			dev_info(cdev, "%s: UFP\n", __func__);
			anx_dbg_event("UFP", 0);
			if (anx->mode == DUAL_ROLE_PROP_MODE_NONE)
				anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_UFP);
			if (anx->pr == DUAL_ROLE_PROP_PR_NONE)
				anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
			anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
		}
#ifdef CONFIG_DUAL_ROLE_USB_INTF
		dual_role_changed = true;
#endif
		irq &= ~DATA_ROLE_CHG;
	}

	if (!(intf_irq_mask & PR_C_GOT_POWER) && (irq & PR_C_GOT_POWER)) {
		int volt = __anx7418_read_reg(client, RDO_MAX_VOLT);
		int power = __anx7418_read_reg(client, RDO_MAX_POWER);

		if (volt > 0 && power > 0) {
			anx->volt_max = volt * 100;
			anx->curr_max = (power * 500 * 10) / volt;
			anx->ctype_charger = ANX7418_CTYPE_PD_CHARGER;
		}

		dev_info(cdev, "%s: VOLT(%dmV), CURR(%dmA)\n", __func__,
				anx->volt_max, anx->curr_max);

		irq &= ~PR_C_GOT_POWER;
	}

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	if (dual_role_changed)
		dual_role_instance_changed(anx->dual_role);
#endif

	__anx7418_write_reg(client, IRQ_INTF_STATUS, irq);
done:
	__anx7418_write_reg(client, IRQ_EXT_SOURCE_2, SOFT_INTERRUPT);
	anx7418_i2c_unlock(client);
out:
	up_read(&anx->rwsem);
}

static irqreturn_t i2c_irq_event(int irq, void *_anx)
{
	struct anx7418 *anx = _anx;
	struct device *cdev = &anx->client->dev;

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	dev_dbg(cdev, "%s\n", __func__);
	queue_work_on(0, anx->wq, &anx->i2c_irq_work);
	return IRQ_HANDLED;
}

static void cbl_det_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w, struct anx7418, cbl_det_work);
	int det;

	det = gpio_get_value(anx->cbl_det_gpio);

	anx_dbg_event("CABLE DET", det);

	anx7418_pwr_on(anx, det);
}

#ifdef CABLE_DET_PIN_HAS_GLITCH
static int confirmed_cbl_det(struct anx7418 *anx)
{
	int count = 9;
	int cbl_det_count = 0;
	int cbl_det;

	do {
		cbl_det = gpio_get_value(anx->cbl_det_gpio);
		if (cbl_det)
			cbl_det_count++;
		mdelay(1);
	} while (count--);

	if (cbl_det_count > 7)
		return 1;
	else if (cbl_det_count < 3)
		return 0;
	else
		return atomic_read(&anx->pwr_on);
}
#endif

static irqreturn_t cbl_det_event(int irq, void *_anx)
{
	struct anx7418 *anx = _anx;
	struct device *cdev = &anx->client->dev;
#ifdef CABLE_DET_PIN_HAS_GLITCH
	int cbl_det;
#endif

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	dev_info_ratelimited(cdev, "%s\n", __func__);
#ifdef CABLE_DET_PIN_HAS_GLITCH
	cbl_det = confirmed_cbl_det(anx);
	if (cbl_det != atomic_read(&anx->pwr_on))
		cbl_det_work(&anx->cbl_det_work);
	else if (anx->is_dbg_acc && !cbl_det)
		cbl_det_work(&anx->cbl_det_work);
#else
	queue_work_on(0, anx->wq, &anx->cbl_det_work);
#endif
	return IRQ_HANDLED;
}

static int firmware_update(struct anx7418 *anx)
{
	struct anx7418_firmware *fw;
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	ktime_t start_time;
	ktime_t diff;
	int rc;

	__anx7418_pwr_on(anx);

	anx->rom_ver = anx7418_read_reg(client, ANALOG_CTRL_3);
	dev_info(cdev, "rom ver: %02X\n", anx->rom_ver);

	/* Check EEPROM or OTP */
	rc = anx7418_read_reg(client, DEBUG_EE_0);
	anx->otp = (rc & R_EE_DEBUG_STATE) == 1 ? true : false;
	if (anx->rom_ver == 0x00)
		anx->otp = true;

	if (!anx->otp) {
		dev_err(cdev, "EEPROM update not suppored\n");
		rc = 0;
		goto err;
	}

	fw = anx7418_firmware_alloc(anx);
	if (ZERO_OR_NULL_PTR(fw)) {
		dev_err(cdev, "anx7418_firmware_alloc failed\n");
		rc = -ENOMEM;
		goto err;
	}
	dev_info(cdev, "new ver: %02X\n", fw->ver);

	dev_info(cdev, "firmware update start\n");
	start_time = ktime_get();

	if (!anx7418_firmware_update_needed(fw, anx->rom_ver)) {
		dev_info(cdev, "firmware updata not needed\n");
		rc = 0;
#ifdef FIRMWARE_PROFILE
		if (anx->rom_ver != fw->ver)
#endif
		goto update_not_needed;
	}

	dev_info(cdev, "firmware open\n");
	rc = anx7418_firmware_open(fw);
#ifdef FIRMWARE_PROFILE
	if (anx->rom_ver == fw->ver) {
		dev_info(cdev, "update profile test\n");
	}
	else
#endif
	if (rc < 0) {
		if (rc == -EEXIST) {
			rc = 0;
			goto err_open;

		} else if (rc == -ENOSPC) {
			dev_err(cdev, "no space for update\n");
			rc = 0;
			goto err_open;
		}

		dev_err(cdev, "firmware open failed %d\n", rc);
		goto err_open;
	}


	dev_info(cdev, "firmware update\n");
#ifdef FIRMWARE_PROFILE
	if (anx->rom_ver == fw->ver)
		rc = anx7418_firmware_profile(fw);
	else
#endif
	rc = anx7418_firmware_update(fw);
	if (rc < 0)
		dev_err(cdev, "firmware update failed %d\n", rc);
	else
		anx->rom_ver = fw->ver;

	dev_info(cdev, "firmware release\n");
	anx7418_firmware_release(fw);

err_open:
update_not_needed:
	diff = ktime_sub(ktime_get(), start_time);
	dev_info(cdev, "firmware update end (%lld ms)\n", ktime_to_ms(diff));

	anx7418_firmware_free(fw);
err:
	__anx7418_pwr_down(anx);
	return rc;
}

static int anx7418_gpio_configure(struct anx7418 *anx, bool on)
{
	struct device *cdev = &anx->client->dev;
	int rc = 0;

	if (!on) {
		goto gpio_free_all;
	}

	if (gpio_is_valid(anx->pwr_en_gpio)) {
		/* configure anx7418 pwr_en gpio */
		rc = gpio_request_one(anx->pwr_en_gpio,
				GPIOF_OUT_INIT_LOW, "anx7418_pwr_en_gpio");
		if (rc)
			dev_err(cdev, "unable to request gpio[%d]\n",
					anx->pwr_en_gpio);
	} else {
		dev_err(cdev, "pwr_en gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_req;
	}

	if (gpio_is_valid(anx->resetn_gpio)) {
		/* configure anx7418 resetn gpio */
		rc = gpio_request_one(anx->resetn_gpio,
				GPIOF_OUT_INIT_LOW, "anx7418_resetn_gpio");
		if (rc)
			dev_err(cdev, "unable to request gpio[%d]\n",
					anx->resetn_gpio);
	} else {
		dev_err(cdev, "resetn gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_dir;
	}

	if (gpio_is_valid(anx->vconn_gpio)) {
		/* configure anx7418 vconn gpio */
		rc = gpio_request_one(anx->vconn_gpio,
				GPIOF_DIR_OUT, "anx7418_vconn_gpio");
		if (rc)
			dev_err(cdev, "unable to request gpio[%d]\n",
					anx->vconn_gpio);
	} else {
		dev_err(cdev, "vconn gpio not provided\n");
		rc = -EINVAL;
		goto err_resetn_gpio_dir;
	}

	if (gpio_is_valid(anx->sbu_sel_gpio)) {
		/* configure anx7418 sbu_sel gpio */
		rc = gpio_request_one(anx->sbu_sel_gpio,
				GPIOF_DIR_OUT, "anx7418_sbu_sel_gpio");
		if (rc)
			dev_err(cdev, "unable to request gpio[%d]\n",
					anx->sbu_sel_gpio);
	} else {
		dev_err(cdev, "sbu_sel gpio not provided\n");
		rc = -EINVAL;
		goto err_vconn_gpio_dir;
	}

	if (gpio_is_valid(anx->cbl_det_gpio)) {
		/* configure anx7418 cbl_det gpio */
		rc = gpio_request_one(anx->cbl_det_gpio,
				GPIOF_DIR_IN, "anx7418_cbl_det_gpio");
		if (rc)
			dev_err(cdev, "unable to request gpio[%d]\n",
					anx->cbl_det_gpio);
	} else {
		dev_err(cdev, "cbl_det gpio not provided\n");
		rc = -EINVAL;
		goto err_sbu_sel_gpio_dir;
	}

	if (gpio_is_valid(anx->i2c_irq_gpio)) {
		/* configure anx7418 irq gpio */
		rc = gpio_request_one(anx->i2c_irq_gpio,
				GPIOF_DIR_IN, "anx7418_i2c_irq_thread_gpio");
		if (rc)
			dev_err(cdev, "unable to request gpio[%d]\n",
					anx->i2c_irq_gpio);
	} else {
		dev_err(cdev, "irq gpio not provided\n");
		rc = -EINVAL;
		goto err_cbl_det_gpio_dir;
	}

	return 0;

gpio_free_all:
	if (gpio_is_valid(anx->i2c_irq_gpio))
		gpio_free(anx->i2c_irq_gpio);
err_cbl_det_gpio_dir:
	if (gpio_is_valid(anx->cbl_det_gpio))
		gpio_free(anx->cbl_det_gpio);
err_sbu_sel_gpio_dir:
	if (gpio_is_valid(anx->sbu_sel_gpio))
		gpio_free(anx->sbu_sel_gpio);
err_vconn_gpio_dir:
	if (gpio_is_valid(anx->vconn_gpio))
		gpio_free(anx->vconn_gpio);
err_resetn_gpio_dir:
	if (gpio_is_valid(anx->resetn_gpio))
		gpio_free(anx->resetn_gpio);
err_pwr_en_gpio_dir:
	if (gpio_is_valid(anx->pwr_en_gpio))
		gpio_free(anx->pwr_en_gpio);
err_pwr_en_gpio_req:

	if (rc < 0)
		dev_err(cdev, "gpio configure failed: rc=%d\n", rc);
	return rc;
}

static int chg_is_writeable(struct power_supply *psy,
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

static void chg_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w,
			struct anx7418, chg_work.work);
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	union power_supply_propval prop = {0,};
	int rc;

	down_read(&anx->rwsem);
	dev_dbg(cdev, "%s: starting\n", __func__);

	if (!atomic_read(&anx->pwr_on)) {
		dev_info(cdev, "%s: pwr is off, aborted\n", __func__);
		goto out;
	}

	if (anx->pd_psy_d.type == POWER_SUPPLY_TYPE_USB_HVDCP ||
		anx->pd_psy_d.type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
		goto out;

	if (anx->ctype_charger != ANX7418_CTYPE_PD_CHARGER) {
		/* check ctype charger */
		dev_dbg(cdev, "%s: checking charger capability\n", __func__);
		rc = anx7418_read_reg(client, POWER_DOWN_CTRL);

		if (rc & (CC1_VRD_3P0 | CC2_VRD_3P0)) {
			/* 5V@2A */
			anx->volt_max = 5000;
			anx->curr_max = 2000;
			anx->ctype_charger = ANX7418_CTYPE_CHARGER;

		} else if (rc & (CC1_VRD_1P5 | CC2_VRD_1P5)) {
			/* 5V@1.5A */
			anx->volt_max = 5000;
			anx->curr_max = 1500;
			anx->ctype_charger = ANX7418_CTYPE_CHARGER;

		} else {
			/* Default USB Current */
			dev_dbg(cdev, "%s: Default USB Power\n", __func__);
			anx->volt_max = 5000;
			anx->curr_max = 0;
			anx->ctype_charger = ANX7418_UNKNOWN_CHARGER;
		}
	}
	set_property_on_battery(anx, POWER_SUPPLY_PROP_CURRENT_CAPABILITY);

	/* Update ctype(ctype-pd) charger */
	switch (anx->ctype_charger) {
	case ANX7418_CTYPE_CHARGER:
		prop.intval = POWER_SUPPLY_TYPE_TYPEC;
		power_supply_set_property(anx->pd_psy, POWER_SUPPLY_PROP_TYPE, &prop);
		dev_dbg(cdev, "%s: charger type = typec\n", __func__);
		break;
	case ANX7418_CTYPE_PD_CHARGER:
		prop.intval = POWER_SUPPLY_TYPE_USB_PD;
		power_supply_set_property(anx->pd_psy, POWER_SUPPLY_PROP_TYPE, &prop);
		dev_dbg(cdev, "%s: charger type = usb_pd\n", __func__);
		break;
	default: /* unknown charger */
		goto out;
	}

	dev_dbg(cdev, "%s: %s, %dmV, %dmA\n", __func__,
			chg_to_string(anx->pd_psy_d.type),
			anx->volt_max,
			anx->curr_max);

	power_supply_changed(anx->pd_psy);
out:
	up_read(&anx->rwsem);
}

#ifdef CONFIG_OF
static int anx7418_parse_dt(struct device *dev, struct anx7418 *anx)
{
	struct device_node *np = dev->of_node;

	/* gpio */
	anx->pwr_en_gpio = of_get_named_gpio(np, "anx7418,pwr-en", 0);
	dev_dbg(dev, "pwr_en_gpio [%d]\n", anx->pwr_en_gpio);

	anx->resetn_gpio = of_get_named_gpio(np, "anx7418,resetn", 0);
	dev_dbg(dev, "resetn_gpio [%d]\n", anx->resetn_gpio);

	anx->vconn_gpio = of_get_named_gpio(np, "anx7418,vconn", 0);
	dev_dbg(dev, "vconn_gpio [%d]\n", anx->vconn_gpio);

	anx->sbu_sel_gpio = of_get_named_gpio(np, "anx7418,sbu-sel", 0);
	dev_dbg(dev, "sbu_sel_gpio [%d]\n", anx->sbu_sel_gpio);

	anx->cbl_det_gpio = of_get_named_gpio(np, "anx7418,cable-det", 0);
	dev_dbg(dev, "cbl_det_gpio [%d]\n", anx->cbl_det_gpio);

	anx->i2c_irq_gpio = of_get_named_gpio(np, "anx7418,i2c-irq", 0);
	dev_dbg(dev, "i2c_irq_gpio [%d]\n", anx->i2c_irq_gpio);

	return 0;
}
#else
static inline int anx7418_parse_dt(struct device *dev, struct anx7418 *anx)
{
	return 0;
}
#endif

static int anx7418_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct anx7418 *anx;
	struct device *cdev = &client->dev;
	struct power_supply_config pd_psy_cfg = {};
	int rc;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c_check_functionality failed\n", __func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		anx = devm_kzalloc(cdev, sizeof(struct anx7418), GFP_KERNEL);
		if (!anx) {
			pr_err("%s: devm_kzalloc\n", __func__);
			return -ENOMEM;
		}

		rc = anx7418_parse_dt(cdev, anx);
		if (rc)
			return rc;
	} else {
		anx = client->dev.platform_data;
	}

	if (!anx) {
		dev_err(cdev, "%s: No platform data found\n", __func__);
		return -EINVAL;
	}

	anx->client = client;
	i2c_set_clientdata(client, anx);

	/* avdd33 regulator */
	anx->avdd33 = devm_regulator_get(cdev, "avdd33");
	if (IS_ERR(anx->avdd33)) {
		dev_err(cdev, "avdd33: regulator_get failed\n");
		return -EPROBE_DEFER;
	}
	if (regulator_count_voltages(anx->avdd33) > 0) {
		rc = regulator_set_voltage(anx->avdd33, 3300000, 3300000);
		if (rc) {
			dev_err(cdev, "avdd33: set_vtg failed rc=%d\n", rc);
			return rc;
		}
	}
	rc = regulator_enable(anx->avdd33);
	if (rc) {
		dev_err(cdev, "failed to enable avdd33\n");
		return rc;
	}

#if 0 /* CONFIG_LGE_USB_TYPE_C START */
	anx->usb_psy = power_supply_get_by_name("usb");
	if (!anx->usb_psy) {
		dev_err(cdev, "usb power_supply_get failed\n");
		return -EPROBE_DEFER;
	}

	anx->batt_psy = power_supply_get_by_name("battery");
	if (!anx->batt_psy) {
		dev_err(cdev, "battery power_supply_get failed\n");
		return -EPROBE_DEFER;
	}
#endif /* CONFIG_LGE_USB_TYPE_C END */

	anx->wq = alloc_workqueue("anx_wq",
			WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_CPU_INTENSIVE,
			3);
	if (!anx->wq) {
		dev_err(cdev, "unable to create workqueue anx_wq\n");
		return -ENOMEM;
	}
	INIT_WORK(&anx->cbl_det_work, cbl_det_work);
	INIT_WORK(&anx->i2c_irq_work, i2c_irq_work);
	init_rwsem(&anx->rwsem);
	wake_lock_init(&anx->wlock, WAKE_LOCK_SUSPEND, "anx_wlock");

	anx->mode = DUAL_ROLE_PROP_MODE_NONE;
	anx->pr = DUAL_ROLE_PROP_PR_NONE;
	anx->dr = DUAL_ROLE_PROP_DR_NONE;

	rc = anx7418_gpio_configure(anx, true);
	if (rc) {
		dev_err(cdev, "gpio configure failed\n");
		goto err_gpio_config;
	}

	/* assign cbl_det irq */
	anx->cbl_det_irq = gpio_to_irq(anx->cbl_det_gpio);
	if (anx->cbl_det_irq < 0) {
		pr_err("%s : failed to get gpio irq, rc=%d\n", __func__,
					anx->cbl_det_irq);
		goto err_cbl_det_req_irq;
	} else {
		irq_set_status_flags(anx->cbl_det_irq, IRQ_NOAUTOEN);
#ifdef CABLE_DET_PIN_HAS_GLITCH
		rc = devm_request_threaded_irq(cdev, anx->cbl_det_irq,
				NULL,
				cbl_det_event,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"cbl_det_irq", anx);
#else
		rc = devm_request_irq(cdev, anx->cbl_det_irq,
				cbl_det_event,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"cbl_det_irq", anx);
#endif
		if (rc) {
			dev_err(cdev, "Failed to request irq for cbl_det\n");
			goto err_cbl_det_req_irq;
		}
		enable_irq_wake(anx->cbl_det_irq);
	}

	/* assign i2c_irq irq */
	client->irq = gpio_to_irq(anx->i2c_irq_gpio);
	if (client->irq < 0) {
		pr_err("%s : failed to get gpio irq, rc=%d\n", __func__,
					client->irq);
		goto err_req_irq;
	} else {
		irq_set_status_flags(client->irq, IRQ_NOAUTOEN);
		rc = devm_request_irq(cdev, client->irq,
				i2c_irq_event,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"i2c_irq", anx);
		if (rc) {
			dev_err(cdev, "Failed to request irq for i2c\n");
			goto err_req_irq;
		}
	}

	rc = firmware_update(anx);
	if (rc == -ENODEV)
		goto err_update;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	rc = anx7418_drp_init(anx);
	if (rc < 0)
		goto err_drp_init;
#endif

	anx->pd_psy_d.name = "usb_pd";
	anx->pd_psy_d.type = POWER_SUPPLY_TYPE_UNKNOWN;
	anx->pd_psy_d.get_property = chg_get_property;
	anx->pd_psy_d.set_property = chg_set_property;
	anx->pd_psy_d.property_is_writeable = chg_is_writeable;
	anx->pd_psy_d.properties = chg_properties;
	anx->pd_psy_d.num_properties = ARRAY_SIZE(chg_properties);

	pd_psy_cfg.drv_data = anx;
	pd_psy_cfg.supplied_to = chg_supplicants;
	pd_psy_cfg.num_supplicants = ARRAY_SIZE(chg_supplicants);

	INIT_DELAYED_WORK(&anx->chg_work, chg_work);

	anx->pd_psy = devm_power_supply_register(cdev, &anx->pd_psy_d, &pd_psy_cfg);
	if (IS_ERR(anx->pd_psy)) {
		dev_err(cdev, "Unable to register pd_psy rc = %ld\n", PTR_ERR(anx->pd_psy));
		return -EPROBE_DEFER;
	}

	rc = anx7418_sysfs_init(anx);
	if (rc < 0)
		goto err_sysfs_init;

	anx7418_debugfs_init(anx);

	enable_irq(anx->cbl_det_irq);
	enable_irq(client->irq);
	if (gpio_get_value(anx->cbl_det_gpio))
		queue_work_on(0, anx->wq, &anx->cbl_det_work);

	dev_info(cdev, "ANX7418 probe done");
	return 0;

err_sysfs_init:
#ifdef CONFIG_DUAL_ROLE_USB_INTF
err_drp_init:
#endif
err_update:
	devm_free_irq(cdev, client->irq, anx);
err_req_irq:
	devm_free_irq(cdev, anx->cbl_det_irq, anx);
err_cbl_det_req_irq:
	anx7418_gpio_configure(anx, false);
err_gpio_config:
	wake_lock_destroy(&anx->wlock);
	destroy_workqueue(anx->wq);
	i2c_set_clientdata(client, NULL);
	return rc;
}

static int anx7418_remove(struct i2c_client *client)
{
	struct anx7418 *anx = i2c_get_clientdata(client);
	struct device *cdev = &anx->client->dev;

	pr_info("%s\n", __func__);

	devm_free_irq(cdev, client->irq, anx);
	devm_free_irq(cdev, anx->cbl_det_irq, anx);

	if (atomic_read(&anx->pwr_on))
		__anx7418_pwr_down(anx);

	anx7418_gpio_configure(anx, false);
	wake_lock_destroy(&anx->wlock);
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, anx);

	return 0;
}

static const struct i2c_device_id anx7418_idtable[] = {
	{ "anx7418", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, anx7418_idtable);

#ifdef CONFIG_OF
static const struct of_device_id anx7418_of_match[] = {
	{ .compatible = "analogix,anx7418" },
	{},
};
MODULE_DEVICE_TABLE(of, anx7418_of_match);
#endif

static struct i2c_driver anx7418_driver = {
	.driver  = {
		.owner  = THIS_MODULE,
		.name  = "anx7418",
		.of_match_table = of_match_ptr(anx7418_of_match),
	},
	.id_table = anx7418_idtable,
	.probe  = anx7418_probe,
	.remove = anx7418_remove,
};

static void anx7418_async_init(void *data, async_cookie_t cookie)
{
	int rc;

	pr_info("%s\n", __func__);

	rc = i2c_add_driver(&anx7418_driver);
	if (rc < 0)
		pr_err("%s: i2c_add_driver failed %d\n", __func__, rc);
}

static int __init anx7418_init(void)
{
	pr_info("%s\n", __func__);
	async_schedule(anx7418_async_init, NULL);
	return 0;
}

static void __exit anx7418_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&anx7418_driver);
}

module_init(anx7418_init);
module_exit(anx7418_exit);

MODULE_AUTHOR("hansun.lee@lge.com");
MODULE_DESCRIPTION("ANX7418 driver");
MODULE_LICENSE("GPL");
