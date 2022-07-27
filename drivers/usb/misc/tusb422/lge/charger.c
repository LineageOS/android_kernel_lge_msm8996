#define TUSB422_LGE_DISABLE_USB_PSY

static char *chg_supplicants[] = {
	"battery",
};

static enum power_supply_property chg_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
};

static const char *chg_to_string(enum power_supply_type type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_TYPEC:
		return "USB Type-C Charger";
	case POWER_SUPPLY_TYPE_USB_PD:
		return "USB Type-C PD Charger";
	default:
		return "Generic USB Charger";
	}
}

int set_property_on_battery(struct hw_pd_dev *dev,
			enum power_supply_property prop)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!dev->batt_psy) {
		dev->batt_psy = power_supply_get_by_name("battery");
		if (!dev->batt_psy) {
			pr_err("no batt psy found\n");
			return -ENODEV;
		}
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		ret.intval = dev->curr_max;
		rc = power_supply_set_property(dev->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &ret);
		if (rc)
			pr_err("failed to set current max rc=%d\n", rc);
		else
			pr_info("current set on batt_psy = %d\n", dev->curr_max);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		/*
		 * Notify the typec mode to charger. This is useful in the DFP
		 * case where there is no notification of OTG insertion to the
		 * charger driver.
		 */
		if (dev->mode == DUAL_ROLE_PROP_MODE_UFP)
			ret.intval = POWER_SUPPLY_TYPE_UFP;
		else if (dev->mode == DUAL_ROLE_PROP_MODE_DFP)
			ret.intval = POWER_SUPPLY_TYPE_DFP;
		else
			ret.intval = POWER_SUPPLY_TYPE_UNKNOWN;

		rc = power_supply_set_property(dev->batt_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &ret);
		if (rc)
			pr_err("failed to set typec mode rc=%d\n", rc);
		else
			/* Since vbus is enabled some time after notifying this prop, rather than locally */
			msleep(30);
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		/*
		 * Just using this prop here, as the name seems the most appropriate.
		 * Still setting POWER_SUPPLY_PROP_TYPEC_MODE in the end.
		 */
		if (dev->is_otg)
			ret.intval = POWER_SUPPLY_TYPE_DFP;
		else
			ret.intval = POWER_SUPPLY_TYPE_UFP;

		rc = power_supply_set_property(dev->batt_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &ret);
		if (rc)
			pr_err("failed to set typec mode (otg) rc=%d\n", rc);
		else
			/* Since vbus is enabled some time after notifying this prop, rather than locally */
			msleep(30);
		break;
	default:
		pr_err("invalid request\n");
		rc = -EINVAL;
	}

	return rc;
}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
static int chg_get_sbu_adc(struct hw_pd_dev *dev)
{ // Function mostly disabled due to many lge tie-ins.
	//union lge_power_propval lge_val;
	int rc;

	if (!dev->lge_adc_lpc) {
		dev_err(dev->dev, "%s: lge_adc_lpc is NULL\n", __func__);
		return -ENODEV;
	}

	//rc = dev->lge_adc_lpc->get_property(dev->lge_adc_lpc,
	//				    LGE_POWER_PROP_USB_ID_PHY,
	//				    &lge_val);
	if (rc) {
		dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
		return rc;
	}

	//PRINT("SBU_ADC: %d\n", (int)lge_val.int64val);

	//return (int)lge_val.int64val;
	PRINT("SBU ADC DISABLED!!! MOISTURE DETECTION NOT IMPLEMENTED!!!");
	return -ENODEV;
}
#endif

static int chg_get_property(struct power_supply *psy,
			    enum power_supply_property prop,
			    union power_supply_propval *val)
{
	struct hw_pd_dev *dev = power_supply_get_drvdata(psy);

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
			chg_to_string(dev->chg_psy_d.type));
		val->intval = dev->chg_psy_d.type;
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
#if defined(CONFIG_LGE_USB_MOISTURE_DETECT) && defined(CONFIG_LGE_PM_WATERPROOF_PROTECTION)
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		dev->sbu_ov_cnt = 0;

		val->intval = (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) ? 1 : 0;
		DEBUG("%s: input_suspend(%d)\n", __func__, val->intval);
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
	struct hw_pd_dev *dev = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		if (dev->is_otg == val->intval)
			break;
		dev->is_otg = val->intval;
		DEBUG("%s: is_otg(%d)\n", __func__, dev->is_otg);

		set_property_on_battery(dev, POWER_SUPPLY_PROP_USB_OTG);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		if (dev->is_present == val->intval)
			break;
		dev->is_present = val->intval;
		DEBUG("%s: is_present(%d)\n", __func__, dev->is_present);

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		if (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) {
			tcpm_cc_fault_timer(0, dev->is_present ? false : true);
			break;
		}

		if (dev->moisture_detect_use_sbu && IS_CHARGERLOGO && val->intval) {
			int sbu_adc = chg_get_sbu_adc(dev);
			if (sbu_adc > SBU_WET_THRESHOLD) {
				PRINT("%s: VBUS/SBU SHORT!!! %d\n", __func__, sbu_adc);
				tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_ADC);
				tcpm_cc_fault_timer(0, false);
				break;
			}
		}
#endif

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
	struct power_supply_config chg_psy_cfg = {};
	union power_supply_propval prop;
	int rc;

#ifndef TUSB422_LGE_DISABLE_USB_PSY
	dev->usb_psy = power_supply_get_by_name("usb");
	if (!dev->usb_psy) {
		PRINT("usb power_supply_get failed\n");
		return -EPROBE_DEFER;
	}
	prop.intval = 0;
	power_supply_set_property(dev->usb_psy, 
					POWER_SUPPLY_PROP_PRESENT,&prop);
	//power_supply_set_present(dev->usb_psy, 0);
#else
	dev->usb_psy = NULL;
#endif

	dev->chg_psy_d.name = "usb_pd";
	dev->chg_psy_d.type = POWER_SUPPLY_TYPE_UNKNOWN;
	dev->chg_psy_d.get_property = chg_get_property;
	dev->chg_psy_d.set_property = chg_set_property;
	dev->chg_psy_d.property_is_writeable = chg_is_writeable;
	dev->chg_psy_d.properties = chg_properties;
	dev->chg_psy_d.num_properties = ARRAY_SIZE(chg_properties);
	
	chg_psy_cfg.drv_data = dev;
	chg_psy_cfg.supplied_to = chg_supplicants;
	chg_psy_cfg.num_supplicants = ARRAY_SIZE(chg_supplicants);

	dev->chg_psy = devm_power_supply_register(cdev, &dev->chg_psy_d, &chg_psy_cfg);
	if (IS_ERR(&dev->chg_psy)) {
		PRINT("Unable to register chg_psy rc = %d\n", rc);
		return -EPROBE_DEFER;
	}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	//dev->lge_adc_lpc = lge_power_get_by_name("lge_adc"); // We don't use LGE's files and structs anymore
#endif

	return 0;
}
