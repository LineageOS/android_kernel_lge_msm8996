/*
 * CAUTION! :
 * 		This file will be included at the end of "qpnp-fg.c".
 * 		So "qpnp-fg.c" should be touched before you start to build.
 * 		If not, your work will not be applied to built image
 * 		because the build system doesn't care the update time of this file.
 */

static int fg_power_get_property_pre(struct power_supply *psy,
		enum power_supply_property prop, union power_supply_propval *val) {
	struct fg_chip *chip = container_of(psy, struct fg_chip, bms_psy);
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_DONE :
		val->intval = chip->charge_done;
		break;

	default:
		rc = -ENOENT;
	}

	return rc;
}

static int fg_power_get_property_post(struct power_supply *psy,
		enum power_supply_property prop, union power_supply_propval *val,
		int rc) {
	return rc;
}

static int fg_power_set_property_pre(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val) {
	int rc = 0;

	switch (prop) {
	default:
		rc = -ENOENT;
	}

	return rc;
}

static int fg_power_set_property_post(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val,
		int rc) {

	switch (prop) {
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		if (!val->intval) // only when ZERO!
			power_supply_changed(psy);
		break;

	default:
		break;
	}
	return rc;
}

///////////////////////////////////////////////////////////////////////////////

int fg_power_get_property_ext(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val) {

	int rc = fg_power_get_property_pre(psy, prop, val);
	if (rc == -ENOENT || rc == -EAGAIN)
		rc = fg_power_get_property(psy, prop, val);
	rc = fg_power_get_property_post(psy, prop, val, rc);

	return rc;
}

int fg_power_set_property_ext(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val) {

	int rc = fg_power_set_property_pre(psy, prop, val);
	if (rc == -ENOENT || rc == -EAGAIN)
		rc = fg_power_set_property(psy, prop, val);
	rc = fg_power_set_property_post(psy, prop, val, rc);

	return rc;
}
