#include <linux/usb/class-dual-role.h>
#include <linux/i2c.h>

static enum dual_role_property drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

#define TRY_ROLE_TIMEOUT	600

static bool try_src(struct anx7418 *anx, unsigned long timeout)
{
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	unsigned long expire;

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	if (!(anx7418_read_reg(client, ANALOG_STATUS) & DFP_OR_UFP)) {
		dev_dbg(cdev, "Current role is DFP, no need Try source\n");
		return true;
	}

	power_supply_set_usb_otg(&anx->chg.psy, 0);
	anx->pr = DUAL_ROLE_PROP_PR_SNK;

	anx7418_write_reg(client, RESET_CTRL_0, R_OCM_RESET | R_PD_RESET);
	anx7418_write_reg(client, ANALOG_STATUS,
			anx7418_read_reg(client, ANALOG_STATUS) | R_TRY_DFP);
	anx7418_write_reg(client, ANALOG_CTRL_9,
			anx7418_read_reg(client, ANALOG_CTRL_9) | CC_SOFT_EN);

	expire = msecs_to_jiffies(timeout) + jiffies;
	while (!(anx7418_read_reg(client, ANALOG_CTRL_7) & 0x0F)) {
		if (time_before(expire, jiffies)) {
			dev_dbg(cdev, "Try source timeout. ANALOG_CTRL_7(%02X)\n",
					anx7418_read_reg(client, ANALOG_CTRL_7));
			goto try_src_fail;
		}
	}

	anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_DFP);
	power_supply_set_usb_otg(&anx->chg.psy, 1);
	anx->pr = DUAL_ROLE_PROP_PR_SRC;
	anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(anx->dual_role);
#endif

	mdelay(800);

	anx7418_write_reg(client, RESET_CTRL_0, 0);
	mdelay(50);
	anx7418_reg_init(anx);
	anx7418_pd_src_cap_init(anx);

	dev_dbg(cdev, "Try source swap success\n");
	return true;

try_src_fail:
	dev_dbg(cdev, "Try source fail\n");
	anx7418_write_reg(client, ANALOG_STATUS,
			anx7418_read_reg(client, ANALOG_STATUS) & ~R_TRY_DFP);
	anx7418_write_reg(client, RESET_CTRL_0, 0);
	mdelay(50);
	anx7418_reg_init(anx);

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(anx->dual_role);
#endif
	return false;
}

static void try_src_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w, struct anx7418, try_src_work);
	try_src(anx, TRY_ROLE_TIMEOUT * 3);
	up_read(&anx->rwsem);
}

static bool try_snk(struct anx7418 *anx, unsigned long timeout)
{
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	unsigned long expire;
	int intp_ctrl;

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	if (anx7418_read_reg(client, ANALOG_STATUS) & DFP_OR_UFP) {
		dev_dbg(cdev, "Current role is UFP, no need Try sink\n");
		return true;
	}

	anx->is_tried_snk = true;

	anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_NONE);
	anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_NONE);
	anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);

	anx7418_write_reg(client, RESET_CTRL_0, R_OCM_RESET | R_PD_RESET);
	anx7418_write_reg(client, ANALOG_STATUS,
			anx7418_read_reg(client, ANALOG_STATUS) & ~R_TRY_DFP);
	anx7418_write_reg(client, ANALOG_CTRL_9,
			anx7418_read_reg(client, ANALOG_CTRL_9) | CC_SOFT_EN);

	/* disable VCONN output */
	intp_ctrl = anx7418_read_reg(client, INTP_CTRL);
	anx7418_write_reg(client, INTP_CTRL, intp_ctrl & 0x0F);

	expire = msecs_to_jiffies(timeout) + jiffies;
	while (!(anx7418_read_reg(client, POWER_DOWN_CTRL) & 0xFC)) {
		if (time_before(expire, jiffies)) {
			dev_dbg(cdev, "Try sink timeout. POWER_DOWN_CTRL(%02X)\n",
					anx7418_read_reg(client, POWER_DOWN_CTRL));
			goto try_snk_fail;
		}
	}

	anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_UFP);
	anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
	anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	dual_role_instance_changed(anx->dual_role);
#endif

	anx7418_write_reg(client, RESET_CTRL_0, 0);
	mdelay(50);
	anx7418_reg_init(anx);

	dev_dbg(cdev, "Try sink swap success\n");
	return true;

try_snk_fail:
	dev_dbg(cdev, "Try sink fail\n");

	anx7418_write_reg(client, ANALOG_STATUS,
			anx7418_read_reg(client, ANALOG_STATUS) | R_TRY_DFP);
	anx7418_write_reg(client, RESET_CTRL_0, 0);

	mdelay(50);

	/* enable VCONN output */
	anx7418_write_reg(client, INTP_CTRL, intp_ctrl);

	anx7418_write_reg(client, 0x47, anx7418_read_reg(client, 0x47) | 1);
	anx7418_reg_init(anx);
	anx7418_pd_src_cap_init(anx);

	return false;
}

static void try_snk_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w, struct anx7418, try_snk_work);
	try_snk(anx, TRY_ROLE_TIMEOUT * 3);
	up_read(&anx->rwsem);
}

/* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
static int drp_get_property(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop,
		unsigned int *val)
{
	struct anx7418 *anx = dev_get_drvdata(dual_role->dev.parent);
	int rc = 0;

	if (!anx) {
		pr_err("%s: drvdata is NULL\n", __func__);
		return -EINVAL;
	}

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends == LGE_ALICE_FRIENDS_HM_B)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else
#endif
		*val = anx->mode;
		break;

	case DUAL_ROLE_PROP_PR:
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends == LGE_ALICE_FRIENDS_HM_B)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else
#endif
		*val = anx->pr;
		break;

	case DUAL_ROLE_PROP_DR:
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends == LGE_ALICE_FRIENDS_HM_B)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
#endif
		*val = anx->dr;
		break;

	default:
		pr_err("%s: unknown property. %d\n", __func__, prop);
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Callback for "echo <value> >
 *                      /sys/class/dual_role_usb/<name>/<property>"
 * Block until the entire final state is reached.
 * Blocking is one of the better ways to signal when the operation
 * is done.
 * This function tries to switched to Attached.SRC or Attached.SNK
 * by forcing the mode into SRC or SNK.
 * On failure, we fall back to Try.SNK state machine.
 */
static int drp_set_property(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop,
		const unsigned int *val)
{
	struct anx7418 *anx = dev_get_drvdata(dual_role->dev.parent);
	struct i2c_client *client;
	struct device *cdev;
	int rc = 0;

	if (!anx) {
		pr_err("%s: drvdata is NULL\n", __func__);
		return -EIO;
	}

	client = anx->client;
	cdev = &client->dev;

	down_read(&anx->rwsem);

	if (!atomic_read(&anx->pwr_on)) {
		dev_err(cdev, "%s: power down\n", __func__);
		goto out;
	}

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		if (*val == anx->mode)
			goto out;

		switch (*val) {
		case DUAL_ROLE_PROP_MODE_UFP:
			if (IS_INTF_IRQ_SUPPORT(anx))
				schedule_work(&anx->try_snk_work);
			goto out_prop_mode;

		case DUAL_ROLE_PROP_MODE_DFP:
			if (IS_INTF_IRQ_SUPPORT(anx))
				schedule_work(&anx->try_src_work);
			goto out_prop_mode;

		default:
			dev_err(cdev, "%s: unknown mode value. %d\n",
					__func__, *val);
			rc = -EINVAL;
			break;
		}
		break;

	default:
		dev_err(cdev, "%s: unknown property. %d\n", __func__, prop);
		rc = -EINVAL;
		break;
	}

out:
	up_read(&anx->rwsem);
out_prop_mode:
	return rc;
}

/* Decides whether userspace can change a specific property */
static int drp_is_writeable(struct dual_role_phy_instance *drp,
		enum dual_role_property prop)
{
	int rc;

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

int anx7418_drp_init(struct anx7418 *anx)
{
	struct device *cdev = &anx->client->dev;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;

	desc = devm_kzalloc(cdev, sizeof(struct dual_role_phy_desc),
			GFP_KERNEL);
	if (!desc) {
		dev_err(cdev, "unable to allocate dual role descriptor\n");
		return -ENOMEM;
	}

	desc->name = "otg_default";
	desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	desc->get_property = drp_get_property;
	desc->set_property = drp_set_property;
	desc->properties = drp_properties;
	desc->num_properties = ARRAY_SIZE(drp_properties);
	desc->property_is_writeable = drp_is_writeable;
	dual_role = devm_dual_role_instance_register(cdev, desc);
	anx->dual_role = dual_role;
	anx->desc = desc;

	INIT_WORK(&anx->try_src_work, try_src_work);
	INIT_WORK(&anx->try_snk_work, try_snk_work);

	return 0;
}
