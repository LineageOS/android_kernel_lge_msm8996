#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include "anx7418.h"
#include "anx7418_charger.h"

#define CHG_WORK_DELAY 5000

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
	case POWER_SUPPLY_TYPE_CTYPE:
		return "USB Type-C Charger";
	case POWER_SUPPLY_TYPE_CTYPE_PD:
		return "USB Type-C PD Charger";
	default:
		return "Unknown Charger";
	}
}

static int chg_get_property(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val)
{
	struct anx7418_charger *chg = container_of(psy,
			struct anx7418_charger, psy);
	struct device *cdev = &chg->anx->client->dev;

	switch(prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		dev_dbg(cdev, "%s: is_otg(%d)\n", __func__,
				chg->is_otg);
		val->intval = chg->is_otg;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		dev_dbg(cdev, "%s: is_present(%d)\n", __func__,
				chg->is_present);
		val->intval = chg->is_present;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		dev_dbg(cdev, "%s: volt_max(%dmV)\n", __func__, chg->volt_max);
		val->intval = chg->volt_max;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		dev_dbg(cdev, "%s: curr_max(%dmA)\n", __func__, chg->curr_max);
		val->intval = chg->curr_max;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		dev_dbg(cdev, "%s: type(%s)\n", __func__,
				chg_to_string(chg->psy.type));
		val->intval = chg->psy.type;
		break;

#if defined(CONFIG_LGE_USB_TYPE_C) && defined(CONFIG_LGE_PM_CHARGING_CONTROLLER)
	case POWER_SUPPLY_PROP_CTYPE_CHARGER:
		dev_dbg(cdev, "%s: Rp %dK\n", __func__, chg->rp.intval);
		val->intval = chg->rp.intval;
		break;
#endif

	default:
		return -EINVAL;
	}

	return 0;
}

static int chg_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct anx7418_charger *chg = container_of(psy,
			struct anx7418_charger, psy);
	struct anx7418 *anx = chg->anx;
	struct device *cdev = &chg->anx->client->dev;
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends == LGE_ALICE_FRIENDS_CM)
			goto out;
#endif
		if (chg->is_otg == val->intval)
			break;
		dev_dbg(cdev, "%s: is_otg(%d)\n", __func__, chg->is_otg);
		chg->is_otg = val->intval;

		anx_dbg_event("VBUS REG", chg->is_otg);
		if (chg->is_otg) {
			rc = regulator_enable(anx->vbus_reg);
			if (rc)
				dev_err(cdev, "unable to enable vbus\n");
			anx_dbg_event("VBUS", 1);
		} else {
			rc = regulator_disable(anx->vbus_reg);
			if (rc)
				dev_err(cdev, "unable to disable vbus\n");
			anx_dbg_event("VBUS", 0);
		}
		break;

	case POWER_SUPPLY_PROP_PRESENT:
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends == LGE_ALICE_FRIENDS_NONE)
#endif
		if (anx->is_dbg_acc || anx->mode == DUAL_ROLE_PROP_MODE_NONE) {
			if (val->intval) {
				dev_info(cdev, "power on by charger\n");
				anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
			} else {
				if (anx->dr == DUAL_ROLE_PROP_DR_DEVICE) {
					dev_info(cdev, "power down by charger\n");
					anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);
				} else if (anx->dr == DUAL_ROLE_PROP_DR_NONE) {
					union power_supply_propval prop;
					anx->usb_psy->get_property(anx->usb_psy,
							POWER_SUPPLY_PROP_PRESENT, &prop);
					if (prop.intval) {
						dev_info(cdev, "power down by charger\n");
						power_supply_set_present(anx->usb_psy, 0);
					}
				}
			}
		}

		if (chg->is_present == val->intval)
			break;
		chg->is_present = val->intval;
		dev_dbg(cdev, "%s: is_present(%d)\n", __func__, chg->is_present);

		if (chg->is_present) {
#ifdef CONFIG_LGE_ALICE_FRIENDS
			if (anx->friends == LGE_ALICE_FRIENDS_NONE)
#endif
			schedule_delayed_work(&chg->chg_work,
					msecs_to_jiffies(CHG_WORK_DELAY));
		} else {
			cancel_delayed_work(&chg->chg_work);
			chg->curr_max = 0;
			chg->volt_max = 0;
			chg->ctype_charger = 0;
		}

#ifdef CONFIG_LGE_ALICE_FRIENDS
		if ((anx->friends == LGE_ALICE_FRIENDS_HM ||
		     anx->friends == LGE_ALICE_FRIENDS_HM_B) &&
		     anx->dr != DUAL_ROLE_PROP_DR_HOST) {
			if (chg->is_present)
				power_supply_set_present(anx->usb_psy, 1);
			else
				power_supply_set_present(anx->usb_psy, 0);
		}
		else if (anx->friends == LGE_ALICE_FRIENDS_CM) {
			if (chg->is_present)
				schedule_delayed_work(&chg->vconn_work,
					msecs_to_jiffies(100));
			else
				cancel_delayed_work(&chg->vconn_work);
		}
#endif
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->volt_max = val->intval;
		dev_dbg(cdev, "%s: volt_max(%dmV)\n", __func__, chg->volt_max);
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		chg->curr_max = val->intval;
		dev_dbg(cdev, "%s: curr_max(%dmA)\n", __func__, chg->curr_max);
		break;

	case POWER_SUPPLY_PROP_TYPE:
		switch (val->intval) {
		case POWER_SUPPLY_TYPE_CTYPE:
		case POWER_SUPPLY_TYPE_CTYPE_PD:
			psy->type = val->intval;
			break;
		default:
			psy->type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		}
		dev_dbg(cdev, "%s: type(%s)\n", __func__, chg_to_string(psy->type));

		break;

#if defined(CONFIG_LGE_USB_TYPE_C) && defined(CONFIG_LGE_PM_CHARGING_CONTROLLER)
	case POWER_SUPPLY_PROP_CTYPE_CHARGER:
		chg->rp.intval = val->intval;
		dev_dbg(cdev, "%s: Rp %dK\n", __func__, chg->rp.intval);

		rc = anx->batt_psy->set_property(anx->batt_psy,
				POWER_SUPPLY_PROP_CTYPE_CHARGER, &chg->rp);
		if (rc < 0)
			dev_err(cdev, "set_property(CTYPE_CHARGER) error %d\n", rc);

		break;
#endif

	default:
		return -EINVAL;
	}

#ifdef CONFIG_LGE_ALICE_FRIENDS
out:
#endif
	return 0;
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

#ifdef CONFIG_LGE_ALICE_FRIENDS
#define CM_VCONN_DELAY		100

static void force_enable_vconn(struct anx7418 *anx)
{
	struct i2c_client *client = anx->client;
	int rc = 0;

	anx7418_write_reg(client, RESET_CTRL_0, R_OCM_RESET);

	gpio_set_value(anx->vconn_gpio, 1);

	rc = anx7418_read_reg(client, R_PULL_UP_DOWN_CTRL_1);
	rc |= R_VCONN1_EN_PULL_DOWN;
	anx7418_write_reg(client, R_PULL_UP_DOWN_CTRL_1, rc);

	mdelay(CM_VCONN_DELAY);

	rc = anx7418_read_reg(client, R_PULL_UP_DOWN_CTRL_1);
	rc &= ~R_VCONN1_EN_PULL_DOWN;
	anx7418_write_reg(client, R_PULL_UP_DOWN_CTRL_1, rc);

	gpio_set_value(anx->vconn_gpio, 0);

	anx7418_write_reg(client, RESET_CTRL_0, 0);
	mdelay(50);
	anx7418_reg_init(anx);
}

static void vconn_work(struct work_struct *w)
{
	struct anx7418_charger *chg = container_of(w,
			struct anx7418_charger, vconn_work.work);
	struct anx7418 *anx = chg->anx;
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;

	down_read(&anx->rwsem);

	dev_info(cdev, "%s is called\n", __func__);

	if (!atomic_read(&anx->pwr_on)) {
		goto out;
	}

	if (anx->dr != DUAL_ROLE_PROP_DR_DEVICE) {
		force_enable_vconn(anx);
		dev_info(cdev, "%s: Turn on CC1 Vconn for %dmsec\n",
				__func__, CM_VCONN_DELAY );
	}
out:
	up_read(&anx->rwsem);
}
#endif /* CONFIG_LGE_ALICE_FRIENDS */

static void chg_work(struct work_struct *w)
{
	struct anx7418_charger *chg = container_of(w,
			struct anx7418_charger, chg_work.work);
	struct anx7418 *anx = chg->anx;
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	int rc = 0;

	down_read(&anx->rwsem);

	if (!atomic_read(&anx->pwr_on))
		goto out;

	if (chg->ctype_charger != ANX7418_CTYPE_PD_CHARGER) {
		/* check ctype charger */
		rc = anx7418_read_reg(client, POWER_DOWN_CTRL);

		if (rc & (CC1_VRD_3P0 | CC2_VRD_3P0)) {
			// 5V@3A
			chg->volt_max = 5000;
			chg->curr_max = 3000;
			chg->ctype_charger = ANX7418_CTYPE_CHARGER;

		} else if (rc & (CC1_VRD_1P5 | CC2_VRD_1P5)) {
			/* From the power team request, do not update 5v@1.5A */

			// 5V@1.5A
			//chg->volt_max = 5000;
			//chg->curr_max = 1500;
			//chg->ctype_charger = ANX7418_CTYPE_CHARGER;

		} else {
			// Default USB Current
			dev_dbg(cdev, "%s: Default USB Power\n", __func__);
		}
	}

	/* Update ctype(ctype-pd) charger */
	switch (chg->ctype_charger) {
#if defined(CONFIG_LGE_USB_FLOATED_CHARGER_DETECT) && defined(CONFIG_LGE_USB_TYPE_C)
	union power_supply_propval prop;
#endif
	case ANX7418_CTYPE_CHARGER:
		power_supply_set_supply_type(&chg->psy,
					POWER_SUPPLY_TYPE_CTYPE);
#if defined(CONFIG_LGE_USB_FLOATED_CHARGER_DETECT) && defined(CONFIG_LGE_USB_TYPE_C)
		anx->usb_psy->set_property(anx->usb_psy, POWER_SUPPLY_PROP_CTYPE_CHARGER, &prop);
#endif
		break;
	case ANX7418_CTYPE_PD_CHARGER:
		power_supply_set_supply_type(&chg->psy,
					POWER_SUPPLY_TYPE_CTYPE_PD);
#if defined(CONFIG_LGE_USB_FLOATED_CHARGER_DETECT) && defined(CONFIG_LGE_USB_TYPE_C)
		anx->usb_psy->set_property(anx->usb_psy, POWER_SUPPLY_PROP_CTYPE_CHARGER, &prop);
#endif
		break;
	default: // unknown charger
		goto out;
	}

	dev_info(cdev, "%s: %s, %dmV, %dmA\n", __func__,
			chg_to_string(chg->psy.type),
			chg->volt_max,
			chg->curr_max);

	power_supply_changed(&chg->psy);
out:
	up_read(&anx->rwsem);
}

int anx7418_charger_init(struct anx7418 *anx)
{
	struct anx7418_charger *chg = &anx->chg;
	struct device *cdev = &anx->client->dev;
	int rc;

	chg->psy.name = "usb_pd";
	chg->psy.type = POWER_SUPPLY_TYPE_CTYPE;
	chg->psy.get_property = chg_get_property;
	chg->psy.set_property = chg_set_property;
	chg->psy.property_is_writeable = chg_is_writeable;
	chg->psy.properties = chg_properties;
	chg->psy.num_properties = ARRAY_SIZE(chg_properties);
	chg->psy.supplied_to = chg_supplicants;
	chg->psy.num_supplicants = ARRAY_SIZE(chg_supplicants);

	chg->anx = anx;
	INIT_DELAYED_WORK(&chg->chg_work, chg_work);

#ifdef CONFIG_LGE_ALICE_FRIENDS
	INIT_DELAYED_WORK(&chg->vconn_work, vconn_work);
#endif

	rc = power_supply_register(cdev, &chg->psy);
	if (rc < 0) {
		dev_err(cdev, "Unalbe to register ctype_psy rc = %d\n", rc);
		return -EPROBE_DEFER;
	}

	power_supply_set_supply_type(&chg->psy, POWER_SUPPLY_TYPE_UNKNOWN);

	return 0;
}
