static char *chg_supplicants[] = {
	"battery",
};

static enum power_supply_property chg_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
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

static void set_property_to_battery(struct hw_pd_dev *dev,
				    enum power_supply_property property,
				    union power_supply_propval *prop)
{
	int rc;

	if (!dev->batt_psy) {
		dev->batt_psy = power_supply_get_by_name("battery");
		if (!dev->batt_psy) {
			PRINT("battery psy doesn't preapred\n");
			dev->batt_psy = 0;
			return;
		}
	}

	rc = dev->batt_psy->set_property(dev->batt_psy, property, prop);
	if (rc < 0)
		PRINT("battery psy doesn't support reading prop %d rc = %d\n",
		      property, rc);
}

#define OTG_WORK_DELAY 1000
static void otg_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     otg_work.work);
	struct device *cdev = dev->dev;
	union power_supply_propval prop;
	int rc;

	if (!dev->vbus_reg) {
		dev->vbus_reg = devm_regulator_get(cdev, "vbus");
		if (IS_ERR(dev->vbus_reg)) {
			PRINT("vbus regulator doesn't preapred\n");
			dev->vbus_reg = 0;
			schedule_delayed_work(&dev->otg_work,
					      msecs_to_jiffies(OTG_WORK_DELAY));
			return;
		}
	}

	if (dev->is_otg) {
		rc = regulator_enable(dev->vbus_reg);
		if (rc)
			PRINT("unable to enable vbus\n");

		prop.intval = POWER_SUPPLY_TYPE_DFP;
		set_property_to_battery(dev, POWER_SUPPLY_PROP_TYPEC_MODE,
				       &prop);
	} else {
		rc = regulator_disable(dev->vbus_reg);
		if (rc)
			PRINT("unable to disable vbus\n");

		prop.intval = POWER_SUPPLY_TYPE_UFP;
		set_property_to_battery(dev, POWER_SUPPLY_PROP_TYPEC_MODE,
				       &prop);
	}
}

static int chg_get_property(struct power_supply *psy,
			    enum power_supply_property prop,
			    union power_supply_propval *val)
{
	struct hw_pd_dev *dev = container_of(psy, struct hw_pd_dev, chg_psy);

	switch(prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		DEBUG("%s: is_otg(%d)\n", __func__,
			dev->is_otg);
		val->intval = dev->is_otg;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		DEBUG("%s: is_present(%d)\n", __func__,
			dev->is_present);
		val->intval = dev->is_present;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		DEBUG("%s: volt_max(%dmV)\n", __func__, dev->volt_max);
		val->intval = dev->volt_max;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		DEBUG("%s: curr_max(%dmA)\n", __func__, dev->curr_max);
		val->intval = dev->curr_max;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		DEBUG("%s: type(%s)\n", __func__,
			chg_to_string(dev->chg_psy.type));
		val->intval = dev->chg_psy.type;
		break;

	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		DEBUG("%s: current_capability(%dmA)\n", __func__,
		      dev->curr_max);
		val->intval = dev->curr_max;
		break;

	case POWER_SUPPLY_PROP_TYPEC_MODE:
		DEBUG("%s: typec_mode(%d)\n", __func__,
		      dev->typec_mode);
		val->intval = dev->typec_mode;
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
	struct hw_pd_dev *dev = container_of(psy, struct hw_pd_dev, chg_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		if (dev->is_otg == val->intval)
			break;
		dev->is_otg = val->intval;
		DEBUG("%s: is_otg(%d)\n", __func__, dev->is_otg);

		cancel_delayed_work(&dev->otg_work);
		otg_work(&dev->otg_work.work);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		if (dev->is_present == val->intval)
			break;
		dev->is_present = val->intval;
		DEBUG("%s: is_present(%d)\n", __func__, dev->is_present);

		if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
			if (val->intval) {
				PRINT("power on by charger\n");
				set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			} else {
				if (dev->dr == DUAL_ROLE_PROP_DR_DEVICE) {
					PRINT("power down by charger\n");
					set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
				}
			}
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int chg_is_writeable(struct power_supply *psy,
			    enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
	case POWER_SUPPLY_PROP_PRESENT:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

int charger_init(struct hw_pd_dev *dev)
{
	struct device *cdev = dev->dev;
	int rc;

	dev->usb_psy = power_supply_get_by_name("usb");
	if (!dev->usb_psy) {
		PRINT("usb power_supply_get failed\n");
		return -EPROBE_DEFER;
	}
	power_supply_set_present(dev->usb_psy, 0);

	dev->chg_psy.name = "usb_pd";
	dev->chg_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
	dev->chg_psy.get_property = chg_get_property;
	dev->chg_psy.set_property = chg_set_property;
	dev->chg_psy.property_is_writeable = chg_is_writeable;
	dev->chg_psy.properties = chg_properties;
	dev->chg_psy.num_properties = ARRAY_SIZE(chg_properties);
	dev->chg_psy.supplied_to = chg_supplicants;
	dev->chg_psy.num_supplicants = ARRAY_SIZE(chg_supplicants);

	INIT_DELAYED_WORK(&dev->otg_work, otg_work);

	rc = power_supply_register(cdev, &dev->chg_psy);
	if (rc < 0) {
		PRINT("Unalbe to register ctype_psy rc = %d\n", rc);
		return -EPROBE_DEFER;
	}

	power_supply_set_supply_type(&dev->chg_psy, POWER_SUPPLY_TYPE_UNKNOWN);

	return 0;
}
