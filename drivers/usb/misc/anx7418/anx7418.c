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
#ifdef CONFIG_LGE_ALICE_FRIENDS
#include <linux/qpnp/pin.h>
#endif

#include "anx7418.h"
#include "anx7418_firmware.h"
#include "anx7418_pd.h"
#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include "anx7418_drp.c"
#endif
#include "anx7418_charger.h"
#include "anx7418_debugfs.h"
#include "anx7418_sysfs.h"

#define OCM_STARTUP_TIMEOUT 3200

unsigned int dfp;
module_param(dfp, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dfp, "FORCED DFP MODE");

static int intf_irq_mask = 0xFF;

#ifdef CONFIG_LGE_ALICE_FRIENDS
static const char *alice_friends_string(enum lge_alice_friends friends)
{
	static const char *const names[] = {
		[LGE_ALICE_FRIENDS_NONE] = "None",
		[LGE_ALICE_FRIENDS_CM] = "CM",
		[LGE_ALICE_FRIENDS_HM] = "HM",
		[LGE_ALICE_FRIENDS_HM_B] = "HM >= Rev.B",
	};

	if (friends < 0 || friends >= ARRAY_SIZE(names))
		return "Undefined";

	return names[friends];
}

enum {
	HM_EARJACK_DETACH = 0,
	HM_EARJACK_ATTACH = 1,
	HM_NO_INIT = 2,
};

static int ext_acc_en = HM_NO_INIT;

static irqreturn_t ext_acc_en_irq_thread(int irq, void *_anx)
{
	struct anx7418 *anx = _anx;
	struct device *cdev = &anx->client->dev;
	int en;

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	mutex_lock(&anx->hm_mutex);

	en = gpio_get_value(anx->ext_acc_en_gpio);
	dev_info(cdev, "ext_acc_en_gpio: old(%d), new(%d)\n", ext_acc_en, en);
	if (ext_acc_en == en)
		goto out;

	ext_acc_en = en;

	if (ext_acc_en == HM_EARJACK_ATTACH) {
		dev_info(cdev, "HM: Host\n");

		power_supply_set_present(anx->usb_psy, false);

		power_supply_set_usb_otg(anx->usb_psy, 1);
		anx->dr = DUAL_ROLE_PROP_DR_HOST;

		hm_earjack_changed(anx->hm, true);
	} else {
		dev_info(cdev, "HM: Device\n");

		power_supply_set_usb_otg(anx->usb_psy, 0);
		anx->dr = DUAL_ROLE_PROP_DR_DEVICE;

		hm_earjack_changed(anx->hm, false);
	}

out:
	mutex_unlock(&anx->hm_mutex);
	return IRQ_HANDLED;
}
#endif

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
			power_supply_set_usb_otg(&anx->chg.psy, 1);
#endif
		break;

	case DUAL_ROLE_PROP_PR_SNK:
#ifdef CONFIG_LGE_USB_TYPE_C
		power_supply_set_usb_otg(&anx->chg.psy, 0);
#endif
		break;

	case DUAL_ROLE_PROP_PR_NONE:
#ifdef CONFIG_LGE_USB_TYPE_C
		power_supply_set_usb_otg(&anx->chg.psy, 0);
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
		power_supply_set_usb_otg(anx->usb_psy, 1);
#endif
		break;

	case DUAL_ROLE_PROP_DR_DEVICE:
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);

#ifdef CONFIG_LGE_USB_TYPE_C
		power_supply_set_present(anx->usb_psy, 1);
#endif
		break;

	case DUAL_ROLE_PROP_DR_NONE:
#ifdef CONFIG_LGE_USB_TYPE_C
		if (anx->dr == DUAL_ROLE_PROP_DR_HOST)
			power_supply_set_usb_otg(anx->usb_psy, 0);

		if (anx->dr == DUAL_ROLE_PROP_DR_DEVICE)
			power_supply_set_present(anx->usb_psy, 0);
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

			// AUTO-PD
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

			// skip check vbus
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

#ifdef CONFIG_LGE_ALICE_FRIENDS
	if (anx->friends != LGE_ALICE_FRIENDS_CM)
#endif
	gpio_set_value(anx->vconn_gpio, 1);

	gpio_set_value(anx->pwr_en_gpio, 1);
	anx_dbg_event("PWR EN", 1);
	mdelay(10);

	gpio_set_value(anx->resetn_gpio, 1);
	anx_dbg_event("RESETN", 1);

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
#ifdef CONFIG_LGE_USB_TYPE_C
	union power_supply_propval prop;
#endif

	dev_info_ratelimited(cdev, "%s(%d)\n", __func__, is_on);

#ifdef CONFIG_LGE_ALICE_FRIENDS
	if (anx->friends == LGE_ALICE_FRIENDS_NONE)
#endif
	if (!is_on && anx->is_dbg_acc) {
#ifdef CONFIG_LGE_USB_TYPE_C
		prop.intval = 1;
		rc = anx->batt_psy->set_property(anx->batt_psy,
				POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
		if (rc < 0)
			dev_err(cdev, "set_property(DP_ALT_MODE) error %d\n", rc);
#endif
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

#ifdef CONFIG_LGE_ALICE_FRIENDS
		/* Turn on CC1 VCONN for HM */
		if (anx->friends == LGE_ALICE_FRIENDS_HM ||
		    anx->friends == LGE_ALICE_FRIENDS_HM_B) {
			ext_acc_en_irq_thread(anx->ext_acc_en_irq, anx);

			anx7418_write_reg(client, RESET_CTRL_0,
					R_OCM_RESET | R_PD_RESET);

			rc = anx7418_read_reg(client, R_PULL_UP_DOWN_CTRL_1);
			rc |= R_VCONN1_EN_PULL_DOWN;
			anx7418_write_reg(client, R_PULL_UP_DOWN_CTRL_1, rc);
			dev_info(cdev, "%s: Turn on CC1 VCONN for HM\n", __func__);

			anx7418_write_reg(client, POWER_DOWN_CTRL,
					R_POWER_DOWN_OCM | R_POWER_DOWN_PD);

			enable_irq(anx->ext_acc_en_irq);
			goto out;
		}
#endif

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
#ifdef CONFIG_LGE_ALICE_FRIENDS
					if (anx->friends == LGE_ALICE_FRIENDS_NONE) {
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
					// Check vbus on?
					anx->usb_psy->get_property(anx->usb_psy,
							POWER_SUPPLY_PROP_DP_DM, &prop);
					if (prop.intval != POWER_SUPPLY_DP_DM_DPF_DMF) {
						// vbus not detected
						dev_err(cdev, "vbus is not detected. ignore it\n");
						__anx7418_pwr_down(anx);
						goto out;
					}
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
					prop.intval = 0;
					rc = anx->batt_psy->set_property(anx->batt_psy,
							POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
					if (rc < 0)
						dev_err(cdev, "set_property(DP_ALT_MODE) error %d\n", rc);
#endif
					gpio_set_value(anx->sbu_sel_gpio, 1);

					anx->is_dbg_acc = true;
					goto set_as_ufp;
#ifdef CONFIG_LGE_ALICE_FRIENDS
					}
#endif
				}
			}

set_as_dfp:
			dev_info(cdev, "%s: set as DFP\n", __func__);
			anx_dbg_event("DFP", 0);

			anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_DFP);
#ifdef CONFIG_LGE_ALICE_FRIENDS
			if (anx->friends != LGE_ALICE_FRIENDS_NONE)
				anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
			else
#endif
			anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SRC);
			anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);
		}
#ifdef CONFIG_DUAL_ROLE_USB_INTF
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (!(anx->friends != LGE_ALICE_FRIENDS_NONE &&
		      anx->mode == DUAL_ROLE_PROP_MODE_DFP))
#endif
		dual_role_instance_changed(anx->dual_role);
#endif
	} else {
		__anx7418_pwr_down(anx);
		anx->is_tried_snk = false;

#if defined(CONFIG_LGE_USB_TYPE_C) && defined(CONFIG_LGE_PM_CHARGING_CONTROLLER)
		prop.intval = 0;
		rc = anx->chg.psy.set_property(&anx->chg.psy,
				POWER_SUPPLY_PROP_CTYPE_CHARGER, &prop);
		if (rc < 0)
			dev_err(cdev, "set_property(CTYPE_CHARGER) error %d\n", rc);
#endif

#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends != LGE_ALICE_FRIENDS_NONE &&
		    anx->mode == DUAL_ROLE_PROP_MODE_DFP) {
			anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_NONE);
			anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_NONE);
			anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);
		} else {
#endif
		anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_NONE);
		anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_NONE);
		if (anx->mode != DUAL_ROLE_PROP_MODE_NONE) {
			anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_NONE);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
			dual_role_instance_changed(anx->dual_role);
#endif
		}
#ifdef CONFIG_LGE_ALICE_FRIENDS
		}
#endif
	}

out:
	up_write(&anx->rwsem);
	return 0;
}

static void i2c_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w, struct anx7418, i2c_work);
	struct i2c_client *client = anx->client;
	struct device *cdev = &client->dev;
	int irq;
	int status;
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	int dual_role_changed = false;
#endif
#ifdef CONFIG_LGE_USB_TYPE_C
	union power_supply_propval prop;
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
				// UFP
				dev_info(cdev, "%s: set as UFP\n", __func__);
				anx_dbg_event("UFP", 0);

#if defined(CONFIG_LGE_USB_TYPE_C) && defined(CONFIG_LGE_PM_CHARGING_CONTROLLER)
				switch (rc) {
				case 0x04:
				case 0x40:
					prop.intval = 56; // Rp 56K
					break;
				case 0x08:
				case 0x80:
					prop.intval = 22; // Rp 22K
					break;
				case 0x0C:
				case 0xC0:
					prop.intval = 10; // Rp 10K
					break;
				default:
					prop.intval = 0;
					break;
				}
				dev_info(cdev, "%s: Rp %dK\n", __func__, prop.intval);

				rc = anx->chg.psy.set_property(&anx->chg.psy,
						POWER_SUPPLY_PROP_CTYPE_CHARGER, &prop);
				if (rc < 0)
					dev_err(cdev, "set_property(CTYPE_CHARGER) error %d\n", rc);
#endif

				anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_UFP);
				anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SNK);
				anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_DEVICE);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
				dual_role_changed = true;
#endif
			} else if (rc == 0x11) {
#ifdef CONFIG_LGE_ALICE_FRIENDS
				if (anx->friends == LGE_ALICE_FRIENDS_CM)
					goto set_dfp;
#endif
				// Debug Accerrosy Mode
				dev_info(cdev, "%s: Debug Accessory Mode\n", __func__);
				anx_dbg_event("Debug Accessory", 0);
#ifdef CONFIG_LGE_USB_TYPE_C
				prop.intval = 0;
				rc = anx->batt_psy->set_property(anx->batt_psy,
						POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
				if (rc < 0)
					dev_err(cdev, "set_property(DP_ALT_MODE) error %d\n", rc);
#endif
				gpio_set_value(anx->sbu_sel_gpio, 1);

				anx->is_dbg_acc = true;

				anx7418_i2c_unlock(client);
				__anx7418_pwr_down(anx);
				goto out;
			} else if (rc == 0x00) {
				dev_dbg(cdev, "%s: CC Open\n", __func__);
				anx_dbg_event("CC Open", 0);
				__anx7418_write_reg(client, IRQ_INTF_STATUS,
						irq & intf_irq_mask);
				goto done;
			} else if (rc == 0x22) {
				dev_info(cdev, "%s: Audio Accessory Mode\n", __func__);
				anx_dbg_event("Audio Accessory", 0);
				__anx7418_write_reg(client, IRQ_INTF_STATUS,
						irq & intf_irq_mask);
				goto done;
			} else {
#ifdef CONFIG_LGE_ALICE_FRIENDS
				if (anx->friends == LGE_ALICE_FRIENDS_NONE)
#endif
				if (!anx->is_tried_snk) {
					dev_dbg(cdev, "%s: try_snk\n", __func__);

					anx7418_i2c_unlock(client);
					rc = try_snk(anx, TRY_ROLE_TIMEOUT);
					anx7418_i2c_lock(client);

					__anx7418_write_reg(client, IRQ_INTF_STATUS,
							irq & intf_irq_mask);
					anx->is_tried_snk = true;
					goto done;
				}
#ifdef CONFIG_LGE_ALICE_FRIENDS
set_dfp:
#endif
				// DFP
				dev_info(cdev, "%s: set as DFP\n", __func__);
				anx_dbg_event("DFP", 0);

				anx7418_set_mode(anx, DUAL_ROLE_PROP_MODE_DFP);
				if (anx->pr == DUAL_ROLE_PROP_PR_NONE)
					anx7418_set_pr(anx, DUAL_ROLE_PROP_PR_SRC);
				anx7418_set_dr(anx, DUAL_ROLE_PROP_DR_HOST);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
				dual_role_changed = true;
#endif
			}
		}

		irq &= ~CC_STATUS_CHG;
	}

#ifdef CONFIG_LGE_ALICE_FRIENDS
	if (anx->friends != LGE_ALICE_FRIENDS_NONE) {
		if (!(intf_irq_mask & VBUS_CHG) && (irq & VBUS_CHG))
			irq &= ~VBUS_CHG;
	}
	else
#endif
	if (!(intf_irq_mask & VBUS_CHG) && (irq & VBUS_CHG)) {
		if (status & VBUS_STATUS) {
			dev_dbg(cdev, "%s: VBUS ON\n", __func__);
			power_supply_set_usb_otg(&anx->chg.psy, 1);
			anx->pr = DUAL_ROLE_PROP_PR_SRC;
		} else {
			dev_dbg(cdev, "%s: VBUS OFF\n", __func__);
			power_supply_set_usb_otg(&anx->chg.psy, 0);
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
		if (status & DATA_ROLE) {
			rc = __anx7418_read_reg(client, ANALOG_CTRL_7);
			if ( (rc & 0x0F) == 0x05 &&
			     (anx->friends == LGE_ALICE_FRIENDS_NONE)) { // CC1_Rd and CC2_Rd

				dev_info(cdev, "%s: Debug Accessory Mode\n", __func__);
				anx_dbg_event("Debug Accessory", 0);
#ifdef CONFIG_LGE_USB_TYPE_C
				prop.intval = 0;
				rc = anx->batt_psy->set_property(anx->batt_psy,
						POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
				if (rc < 0)
					dev_err(cdev, "set_property(DP_ALT_MODE) error %d\n", rc);
#endif
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
			anx->chg.volt_max = volt * 100;
			anx->chg.curr_max = (power * 500 * 10) / volt;
			anx->chg.ctype_charger = ANX7418_CTYPE_PD_CHARGER;
		}

		dev_info(cdev, "%s: VOLT(%dmV), CURR(%dmA)\n", __func__,
				anx->chg.volt_max, anx->chg.curr_max);

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

static irqreturn_t i2c_irq(int irq, void *_anx)
{
	struct anx7418 *anx = _anx;
	struct device *cdev = &anx->client->dev;

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	dev_dbg(cdev, "%s\n", __func__);
	queue_work_on(0, anx->wq, &anx->i2c_work);
	return IRQ_HANDLED;
}

static void cable_det_work(struct work_struct *w)
{
	struct anx7418 *anx = container_of(w, struct anx7418, cable_det_work);
	int det;

	det = gpio_get_value(anx->cable_det_gpio);

	anx_dbg_event("CABLE DET", det);

	anx7418_pwr_on(anx, det);
}

#ifdef CABLE_DET_PIN_HAS_GLITCH
static int confirmed_cable_det(struct anx7418 *anx)
{
	int count = 9;
	int cable_det_count = 0;
	int cable_det;

	do {
		cable_det = gpio_get_value(anx->cable_det_gpio);
		if (cable_det)
			cable_det_count++;
		mdelay(1);
	} while (count--);

	if (cable_det_count > 7)
		return 1;
	else if (cable_det_count < 3)
		return 0;
	else
		return atomic_read(&anx->pwr_on);
}
#endif

static irqreturn_t cable_det_irq(int irq, void *_anx)
{
	struct anx7418 *anx = _anx;
	struct device *cdev = &anx->client->dev;
#ifdef CABLE_DET_PIN_HAS_GLITCH
	int cable_det;
#endif

	wake_lock_timeout(&anx->wlock, msecs_to_jiffies(2000));

	dev_info_ratelimited(cdev, "%s\n", __func__);
#ifdef CABLE_DET_PIN_HAS_GLITCH
	cable_det = confirmed_cable_det(anx);
	if (cable_det != atomic_read(&anx->pwr_on))
		cable_det_work(&anx->cable_det_work);
	else if (anx->is_dbg_acc && !cable_det)
		cable_det_work(&anx->cable_det_work);
#else
	queue_work_on(0, anx->wq, &anx->cable_det_work);
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
	struct device *dev = &anx->client->dev;
	int rc = 0;

	if (!on) {
		goto gpio_free_all;
	}

	if (gpio_is_valid(anx->pwr_en_gpio)) {
		/* configure anx7418 pwr_en gpio */
		rc = gpio_request(anx->pwr_en_gpio, "anx7418_pwr_en_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->pwr_en_gpio);
			goto err_pwr_en_gpio_req;
		}
		rc = gpio_direction_output(anx->pwr_en_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->pwr_en_gpio);
			goto err_pwr_en_gpio_dir;
		}
	} else {
		dev_err(dev, "pwr_en gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_req;
	}

	if (gpio_is_valid(anx->resetn_gpio)) {
		/* configure anx7418 resetn gpio */
		rc = gpio_request(anx->resetn_gpio, "anx7418_resetn_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->resetn_gpio);
			goto err_pwr_en_gpio_dir;
		}
		rc = gpio_direction_output(anx->resetn_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->resetn_gpio);
			goto err_resetn_gpio_dir;
		}
	} else {
		dev_err(dev, "resetn gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_dir;
	}

	if (gpio_is_valid(anx->vconn_gpio)) {
		/* configure anx7418 vconn gpio */
		rc = gpio_request(anx->vconn_gpio, "anx7418_vconn_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->vconn_gpio);
			goto err_resetn_gpio_dir;
		}
		rc = gpio_direction_output(anx->vconn_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->vconn_gpio);
			goto err_vconn_gpio_dir;
		}
	} else {
		dev_err(dev, "vconn gpio not provided\n");
		rc = -EINVAL;
		goto err_resetn_gpio_dir;
	}

	if (gpio_is_valid(anx->sbu_sel_gpio)) {
		/* configure anx7418 sbu_sel gpio */
		rc = gpio_request(anx->sbu_sel_gpio, "anx7418_sbu_sel_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->sbu_sel_gpio);
			goto err_vconn_gpio_dir;
		}
#ifdef CONFIG_LGE_ALICE_FRIENDS
		if (anx->friends != LGE_ALICE_FRIENDS_NONE)
			rc = gpio_direction_output(anx->sbu_sel_gpio, 1);
		else
#endif
		rc = gpio_direction_output(anx->sbu_sel_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->sbu_sel_gpio);
			goto err_sbu_sel_gpio_dir;
		}
	} else {
		dev_err(dev, "sbu_sel gpio not provided\n");
		rc = -EINVAL;
		goto err_vconn_gpio_dir;
	}

	if (gpio_is_valid(anx->cable_det_gpio)) {
		/* configure anx7418 cable_det gpio */
		rc = gpio_request(anx->cable_det_gpio, "anx7418_cable_det_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->cable_det_gpio);
			goto err_sbu_sel_gpio_dir;
		}
		rc = gpio_direction_input(anx->cable_det_gpio);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->cable_det_gpio);
			goto err_cable_det_gpio_dir;
		}
	} else {
		dev_err(dev, "cable_det gpio not provided\n");
		rc = -EINVAL;
		goto err_sbu_sel_gpio_dir;
	}

	if (gpio_is_valid(anx->i2c_irq_gpio)) {
		/* configure anx7418 irq gpio */
		rc = gpio_request(anx->i2c_irq_gpio, "anx7418_i2c_irq_thread_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->i2c_irq_gpio);
			goto err_cable_det_gpio_dir;
		}
		rc = gpio_direction_input(anx->i2c_irq_gpio);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->i2c_irq_gpio);
			goto err_i2c_irq_gpio_dir;
		}
	} else {
		dev_err(dev, "irq gpio not provided\n");
		rc = -EINVAL;
		goto err_cable_det_gpio_dir;
	}

#ifdef CONFIG_LGE_ALICE_FRIENDS
	/* HM/CM has unique value in SBU1 line - HM:270K / CM:330K
	 * If HM or CM is connected to the phone, we need to control
	 * SBU2 line for external accessory power enable in HM/CM
	 */
	if (anx->friends == LGE_ALICE_FRIENDS_CM) {
		if (gpio_is_valid(anx->ext_acc_en_gpio)) {
			struct qpnp_pin_cfg cfg = {
				.mode = QPNP_PIN_MODE_DIG_OUT,
				.vin_sel = QPNP_PIN_VIN0,
				.out_strength = QPNP_PIN_OUT_STRENGTH_LOW,
				.src_sel = QPNP_PIN_SEL_FUNC_CONSTANT,
				.master_en = QPNP_PIN_MASTER_ENABLE,
			};

			rc = gpio_request(anx->ext_acc_en_gpio, "anx7418_ext_acc_en_gpio");
			if (rc) {
				dev_err(dev,
					"unable to request gpio[%d]\n",
					anx->ext_acc_en_gpio);
				goto err_i2c_irq_gpio_dir;
			}

			rc = qpnp_pin_config(anx->ext_acc_en_gpio, &cfg);
			if (rc) {
				dev_err(dev,
					"unable to set pin config for gpio[%d]\n",
					anx->ext_acc_en_gpio);
				goto err_ext_acc_en_gpio_dir;
			}

			rc = gpio_direction_output(anx->ext_acc_en_gpio, 0);
			if (rc) {
				dev_err(dev,
					"unable to set dir for gpio[%d]\n",
					anx->ext_acc_en_gpio);
				goto err_ext_acc_en_gpio_dir;
			}
		} else {
			dev_err(dev, "ext_acc_en gpio not provided\n");
			rc = -EINVAL;
			goto err_i2c_irq_gpio_dir;
		}

	} else if (anx->friends == LGE_ALICE_FRIENDS_HM_B) {
		if (gpio_is_valid(anx->ext_acc_en_gpio)) {
			struct qpnp_pin_cfg cfg = {
				.mode = QPNP_PIN_MODE_DIG_IN,
				.pull = QPNP_PIN_GPIO_PULL_NO,
				.vin_sel = QPNP_PIN_VIN2,
				.out_strength = QPNP_PIN_OUT_STRENGTH_LOW,
				.src_sel = QPNP_PIN_SEL_FUNC_CONSTANT,
				.master_en = QPNP_PIN_MASTER_ENABLE,
			};

			rc = gpio_request(anx->ext_acc_en_gpio, "anx7418_ext_acc_en_gpio");
			if (rc) {
				dev_err(dev,
					"unable to request gpio[%d]\n",
					anx->ext_acc_en_gpio);
				goto err_i2c_irq_gpio_dir;
			}

			rc = qpnp_pin_config(anx->ext_acc_en_gpio, &cfg);
			if (rc) {
				dev_err(dev,
					"unable to set pin config for gpio[%d]\n",
					anx->ext_acc_en_gpio);
				goto err_ext_acc_en_gpio_dir;
			}

			rc = gpio_direction_input(anx->ext_acc_en_gpio);
			if (rc) {
				dev_err(dev,
					"unable to set dir for gpio[%d]\n",
					anx->ext_acc_en_gpio);
				goto err_ext_acc_en_gpio_dir;
			}
		} else {
			dev_err(dev, "ext_acc_en gpio not provided\n");
			rc = -EINVAL;
			goto err_i2c_irq_gpio_dir;
		}
	}
#endif

	return 0;

gpio_free_all:
#ifdef CONFIG_LGE_ALICE_FRIENDS
err_ext_acc_en_gpio_dir:
	if (gpio_is_valid(anx->ext_acc_en_gpio))
		gpio_free(anx->ext_acc_en_gpio);
#endif
err_i2c_irq_gpio_dir:
	if (gpio_is_valid(anx->i2c_irq_gpio))
		gpio_free(anx->i2c_irq_gpio);
err_cable_det_gpio_dir:
	if (gpio_is_valid(anx->cable_det_gpio))
		gpio_free(anx->cable_det_gpio);
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
		dev_err(dev, "gpio configure failed: rc=%d\n", rc);
	return rc;
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

	anx->cable_det_gpio = of_get_named_gpio(np, "anx7418,cable-det", 0);
	dev_dbg(dev, "cable_det_gpio [%d]\n", anx->cable_det_gpio);

	anx->i2c_irq_gpio = of_get_named_gpio(np, "anx7418,i2c-irq", 0);
	dev_dbg(dev, "i2c_irq_gpio [%d]\n", anx->i2c_irq_gpio);

#ifdef CONFIG_LGE_ALICE_FRIENDS
	anx->ext_acc_en_gpio = of_get_named_gpio(np, "anx7418,ext-acc-en", 0);
	dev_dbg(dev, "ext_acc_en_gpio [%d]\n", anx->ext_acc_en_gpio);
#endif

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
	int rc;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c_check_functionality failed\n", __func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		anx = devm_kzalloc(&client->dev, sizeof(struct anx7418), GFP_KERNEL);
		if (!anx) {
			pr_err("%s: devm_kzalloc\n", __func__);
			return -ENOMEM;
		}

		rc = anx7418_parse_dt(&client->dev, anx);
		if (rc)
			return rc;
	} else {
		anx = client->dev.platform_data;
	}

	if (!anx) {
		dev_err(&client->dev,
				"%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	/* regulator */
	anx->vbus_reg = devm_regulator_get(&client->dev, "vbus");
	if (IS_ERR(anx->vbus_reg)) {
		dev_err(&client->dev, "vbus regulator_get failed\n");
		return -EPROBE_DEFER;
	}
	if (regulator_count_voltages(anx->vbus_reg) > 0) {
		rc = regulator_set_voltage(anx->vbus_reg, 5000000, 5000000);
		if (rc) {
			dev_err(&client->dev,
				"Regulator set_vtg failed vbus rc=%d\n",
				rc);
			return rc;
		}
	}

	anx->avdd33 = devm_regulator_get(&client->dev, "avdd33");
	if (IS_ERR(anx->avdd33)) {
		dev_err(&client->dev, "avdd33 regulator_get failed\n");
		return -EPROBE_DEFER;
	}
	if (regulator_count_voltages(anx->avdd33) > 0) {
		rc = regulator_set_voltage(anx->avdd33, 3300000, 3300000);
		if (rc) {
			dev_err(&client->dev,
				"Regulator set_vtg failed avdd33 rc=%d\n",
				rc);
			return rc;
		}
	}

#ifdef CONFIG_LGE_ALICE_FRIENDS
	anx->friends = lge_get_alice_friends();
	if (anx->friends != LGE_ALICE_FRIENDS_NONE)
		dev_info(&client->dev, "Alice Friends \"%s\" connected\n",
				alice_friends_string(anx->friends));
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
	anx->usb_psy = power_supply_get_by_name("usb");
	if (!anx->usb_psy) {
		dev_err(&client->dev, "usb power_supply_get failed\n");
		return -EPROBE_DEFER;
	}

	anx->batt_psy = power_supply_get_by_name("battery");
	if (!anx->batt_psy) {
		dev_err(&client->dev, "battery power_supply_get failed\n");
		return -EPROBE_DEFER;
	}
#endif

	rc = regulator_enable(anx->avdd33);
	if (rc) {
		dev_err(&client->dev, "unable to enable avdd33\n");
		return rc;
	}

	dev_set_drvdata(&client->dev, anx);
	anx->client = client;

	INIT_WORK(&anx->cable_det_work, cable_det_work);
	INIT_WORK(&anx->i2c_work, i2c_work);

	anx->wq = alloc_workqueue("anx_wq",
			WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_CPU_INTENSIVE,
			3);
	if (!anx->wq) {
		dev_err(&client->dev, "unable to create workqueue anx_wq\n");
		return -ENOMEM;
	}
	init_rwsem(&anx->rwsem);
	wake_lock_init(&anx->wlock, WAKE_LOCK_SUSPEND, "anx_wlock");

	anx->mode = DUAL_ROLE_PROP_MODE_NONE;
	anx->pr = DUAL_ROLE_PROP_PR_NONE;
	anx->dr = DUAL_ROLE_PROP_DR_NONE;

	rc = anx7418_gpio_configure(anx, true);
	if (rc) {
		dev_err(&client->dev, "gpio configure failed\n");
		goto err_gpio_config;
	}

	anx->cable_det_irq = gpio_to_irq(anx->cable_det_gpio);
	irq_set_status_flags(anx->cable_det_irq, IRQ_NOAUTOEN);
#ifdef CABLE_DET_PIN_HAS_GLITCH
	rc = devm_request_threaded_irq(&client->dev, anx->cable_det_irq,
			NULL,
			cable_det_irq,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"cable_det_irq", anx);
#else
	rc = devm_request_irq(&client->dev, anx->cable_det_irq,
			cable_det_irq,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"cable_det_irq", anx);
#endif
	if (rc) {
		dev_err(&client->dev, "Failed to request irq for cable_det\n");
		goto err_cable_det_req_irq;
	}
	enable_irq_wake(anx->cable_det_irq);

	client->irq = gpio_to_irq(anx->i2c_irq_gpio);
	irq_set_status_flags(client->irq, IRQ_NOAUTOEN);
	rc = devm_request_irq(&client->dev, client->irq,
			i2c_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"i2c_irq", anx);
	if (rc) {
		dev_err(&client->dev, "Failed to request irq for i2c\n");
		goto err_req_irq;
	}

	rc = firmware_update(anx);
	if (rc == -ENODEV)
		goto err_update;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	rc = anx7418_drp_init(anx);
	if (rc < 0)
		goto err_drp_init;
#endif

	rc = anx7418_charger_init(anx);
	if (rc < 0)
		goto err_charger_init;

	rc = anx7418_sysfs_init(anx);
	if (rc < 0)
		goto err_sysfs_init;

	anx7418_debugfs_init(anx);

#ifdef CONFIG_LGE_ALICE_FRIENDS
	if (anx->friends == LGE_ALICE_FRIENDS_HM ||
	    anx->friends == LGE_ALICE_FRIENDS_HM_B) {
		anx->ext_acc_en_irq = gpio_to_irq(anx->ext_acc_en_gpio);
		irq_set_status_flags(anx->ext_acc_en_irq, IRQ_NOAUTOEN);
		rc = devm_request_threaded_irq(&client->dev, anx->ext_acc_en_irq,
			NULL,
			ext_acc_en_irq_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"ext_acc_en", anx);
		if (rc) {
			dev_err(&client->dev, "Failed to request irq for ext_acc_en\n");
			goto err_ext_acc_en;
		}
		enable_irq_wake(anx->ext_acc_en_irq);

		mutex_init(&anx->hm_mutex);

		anx->hm = devm_hm_instance_register(&client->dev);
	}
#endif

	enable_irq(anx->cable_det_irq);
	enable_irq(anx->client->irq);
	if (gpio_get_value(anx->cable_det_gpio))
		queue_work_on(0, anx->wq, &anx->cable_det_work);

	return 0;

err_sysfs_init:
err_charger_init:
#ifdef CONFIG_DUAL_ROLE_USB_INTF
err_drp_init:
#endif
err_update:
	devm_free_irq(&client->dev, client->irq, anx);
err_req_irq:
	devm_free_irq(&client->dev, anx->cable_det_irq, anx);
#ifdef CONFIG_LGE_ALICE_FRIENDS
err_ext_acc_en:
#endif
err_cable_det_req_irq:
	anx7418_gpio_configure(anx, false);
err_gpio_config:
	wake_lock_destroy(&anx->wlock);
	destroy_workqueue(anx->wq);
	return rc;
}

static int anx7418_remove(struct i2c_client *client)
{
	struct device *cdev = &client->dev;
	struct anx7418 *anx = dev_get_drvdata(cdev);

	pr_info("%s\n", __func__);

	devm_free_irq(&client->dev, client->irq, anx);
	devm_free_irq(&client->dev, anx->cable_det_irq, anx);

	if (atomic_read(&anx->pwr_on))
		__anx7418_pwr_down(anx);

	anx7418_gpio_configure(anx, false);
	wake_lock_destroy(&anx->wlock);

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
