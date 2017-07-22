#include "hw_pd_dev.h"
#include "tcpm.h"
#include "usb_pd.h"

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "charger.c"
#ifdef CONFIG_LGE_USB_DEBUGGER
#include <soc/qcom/lge/board_lge.h>
#include "usb_debugger.c"
#endif

#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
struct blocking_notifier_head notifier;
void detect_water_on_usb(bool det_water);
void init_for_mbhc(void* data);
#endif

#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
extern void tusb422_set_dp_notify_node(int val);
#endif


static struct hw_pd_dev _hw_pd_dev;

int set_mode(struct hw_pd_dev *dev, int mode)
{
	static const char *const strings[] = {
		[DUAL_ROLE_PROP_MODE_UFP]	= "UFP",
		[DUAL_ROLE_PROP_MODE_DFP]	= "DFP",
		[DUAL_ROLE_PROP_MODE_FAULT]	= "FAULT",
		[DUAL_ROLE_PROP_MODE_NONE]	= "None",
	};

	if (dev->mode == mode)
		return 0;

	switch (mode) {
	case DUAL_ROLE_PROP_MODE_UFP:
	case DUAL_ROLE_PROP_MODE_DFP:
	case DUAL_ROLE_PROP_MODE_FAULT:
		break;
	case DUAL_ROLE_PROP_MODE_NONE:
#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
		tusb422_set_dp_notify_node(0);
#endif
		break;

	default:
		PRINT("%s: unknown mode %d\n", __func__, mode);
		return -1;
	}

	dev->mode = mode;

#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
    if( mode == DUAL_ROLE_PROP_MODE_FAULT )
        detect_water_on_usb(true);
    else
        detect_water_on_usb(false);
#endif

	PRINT("%s(%s)\n", __func__, strings[mode]);
	return 0;

}

int set_pr(struct hw_pd_dev *dev, int pr)
{
	static const char *const strings[] = {
		[DUAL_ROLE_PROP_PR_SRC]		= "Source",
		[DUAL_ROLE_PROP_PR_SNK]		= "Sink",
		[DUAL_ROLE_PROP_PR_NONE]	= "None",
	};

	if (dev->pr == pr)
		return 0;

	switch (pr) {
	case DUAL_ROLE_PROP_PR_SRC:
		power_supply_set_usb_otg(&dev->chg_psy, 1);
		break;

	case DUAL_ROLE_PROP_PR_SNK:
	case DUAL_ROLE_PROP_PR_NONE:
		power_supply_set_usb_otg(&dev->chg_psy, 0);
		break;

	default:
		PRINT("%s: unknown pr %d\n", __func__, pr);
		return -1;
	}

	dev->pr = pr;

	PRINT("%s(%s)\n", __func__, strings[pr]);
	return 0;

}

int set_dr(struct hw_pd_dev *dev, int dr)
{
	static const char *const strings[] = {
		[DUAL_ROLE_PROP_DR_HOST]	= "Host",
		[DUAL_ROLE_PROP_DR_DEVICE]	= "Device",
		[DUAL_ROLE_PROP_DR_NONE]	= "None",
	};

	if (dev->dr == dr)
		return 0;

	switch (dr) {
	case DUAL_ROLE_PROP_DR_HOST:
		set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
		power_supply_set_usb_otg(dev->usb_psy, 1);
		break;

	case DUAL_ROLE_PROP_DR_DEVICE:
		set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
		power_supply_set_present(dev->usb_psy, 1);
		break;

	case DUAL_ROLE_PROP_DR_NONE:
		if (dev->dr == DUAL_ROLE_PROP_DR_HOST)
			power_supply_set_usb_otg(dev->usb_psy, 0);
		if (dev->dr == DUAL_ROLE_PROP_DR_DEVICE)
			power_supply_set_present(dev->usb_psy, 0);
		break;

	default:
		PRINT("%s: unknown dr %d\n", __func__, dr);
		return -1;
	}

	dev->dr = dr;

	PRINT("%s(%s)\n", __func__, strings[dr]);
	return 0;
}

static const char *event_to_string(enum pd_dpm_pe_evt event)
{
	static const char *const names[] = {
		[PD_DPM_PE_EVT_SOURCE_VBUS]	= "Source VBUS",
		[PD_DPM_PE_EVT_DIS_VBUS_CTRL]	= "Disable VBUS",
		[PD_DPM_PE_EVT_SINK_VBUS]	= "Sink VBUS",
		[PD_DPM_PE_EVT_PD_STATE]	= "PD State",
		[PD_DPM_PE_EVT_TYPEC_STATE]	= "TypeC State",
		[PD_DPM_PE_EVT_DR_SWAP]		= "DataRole Swap",
		[PD_DPM_PE_EVT_PR_SWAP]		= "PowerRole Swap",
	};

	if (event < 0 || event >= ARRAY_SIZE(names))
		return "Undefined";

	return names[event];
}
EXPORT_SYMBOL(event_to_string);

int pd_dpm_handle_pe_event(enum pd_dpm_pe_evt event, void *state)
{
	struct hw_pd_dev *dev = &_hw_pd_dev;
	union power_supply_propval prop;

	CRIT("%s: event: %s\n", __func__, event_to_string(event));

	switch (event) {
	case PD_DPM_PE_EVT_SOURCE_VBUS:
		set_pr(dev, DUAL_ROLE_PROP_PR_SRC);
		dev->chg_psy.type = POWER_SUPPLY_TYPE_DFP;
		break;

	case PD_DPM_PE_EVT_DIS_VBUS_CTRL:
		if (dev->mode != DUAL_ROLE_PROP_MODE_NONE) {
			set_pr(dev, DUAL_ROLE_PROP_PR_SNK);
		} else {
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
		}

		dev->chg_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
		dev->curr_max = 0;
		dev->volt_max = 0;

		prop.intval = 0;
		set_property_to_battery(dev,
					POWER_SUPPLY_PROP_CTYPE_RP,
					&prop);
		break;

	case PD_DPM_PE_EVT_SINK_VBUS:
	{
		struct pd_dpm_vbus_state *vbus_state =
			(struct pd_dpm_vbus_state *)state;

		DEBUG("vbus_type(%d), mv(%d), ma(%d)\n",
		      vbus_state->vbus_type, vbus_state->mv, vbus_state->ma);

		set_pr(dev, DUAL_ROLE_PROP_PR_SNK);

		if (vbus_state->vbus_type) {
			if (dev->chg_psy.type == POWER_SUPPLY_TYPE_CTYPE_PD &&
			    dev->volt_max == vbus_state->mv &&
			    dev->curr_max == vbus_state->ma)
				goto print_vbus_state;

			dev->chg_psy.type = POWER_SUPPLY_TYPE_CTYPE_PD;
			dev->volt_max = vbus_state->mv;
			dev->curr_max = vbus_state->ma;

			prop.intval = (dev->curr_max > 500) ? 500 : dev->curr_max;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
						&prop);
		} else {
			switch (vbus_state->ma) {
			case 3000: // Rp10K
				if (dev->chg_psy.type == POWER_SUPPLY_TYPE_CTYPE &&
				    dev->volt_max == vbus_state->mv &&
#ifdef CONFIG_ARCH_MSM8996
				    dev->curr_max == 2000)
#else
				    dev->curr_max == vbus_state->ma)
#endif
					goto print_vbus_state;

				dev->chg_psy.type = POWER_SUPPLY_TYPE_CTYPE;
				dev->volt_max = vbus_state->mv;
#ifdef CONFIG_ARCH_MSM8996
				dev->curr_max = 2000;
#else
				dev->curr_max = vbus_state->ma;
#endif

				prop.intval = dev->curr_max;
				set_property_to_battery(dev,
							POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
							&prop);
				break;
			case 1500: // Rp22K
			case 500:  // Rp56K
			default:
				break;
			}

			prop.intval = (vbus_state->ma == 3000) ? 10 :
				(vbus_state->ma == 1500) ? 22 :
				(vbus_state->ma == 500) ? 56 : 0;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CTYPE_RP,
						&prop);
		}

print_vbus_state:
		PRINT("%s: %s, %dmV, %dmA\n", __func__,
		      chg_to_string(vbus_state->vbus_type ?
				    POWER_SUPPLY_TYPE_CTYPE_PD :
				    POWER_SUPPLY_TYPE_CTYPE),
		      vbus_state->mv,
		      vbus_state->ma);
		break;
	}

	case PD_DPM_PE_EVT_PD_STATE:
	{
		struct pd_dpm_pd_state *pd_state =
			(struct pd_dpm_pd_state *)state;

		DEBUG("connected(%d)\n", pd_state->connected);

		if (pd_state->connected == PD_CONNECT_PE_READY_SNK) {
			prop.intval = dev->curr_max;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
						&prop);
		}
		break;
	}

	case PD_DPM_PE_EVT_TYPEC_STATE:
	{
		struct pd_dpm_typec_state *tc_state =
			(struct pd_dpm_typec_state *)state;

		DEBUG("polarity(%d), new_state(%d)\n",
		      tc_state->polarity, tc_state->new_state);

		/* new_state */
		switch (tc_state->new_state) {
		case PD_DPM_TYPEC_UNATTACHED:
			gpiod_direction_output(dev->redriver_sel_gpio, 0);
			set_mode(dev, DUAL_ROLE_PROP_MODE_NONE);
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
			set_dr(dev, DUAL_ROLE_PROP_DR_NONE);

#ifdef CONFIG_LGE_PM_WATERPROOF_PROTECTION
			prop.intval = 0;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_INPUT_SUSPEND,
						&prop);
#endif
			break;

		case PD_DPM_TYPEC_ATTACHED_SRC:
			if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
				gpiod_direction_output(dev->redriver_sel_gpio,
						       !tc_state->polarity);
				set_mode(dev, DUAL_ROLE_PROP_MODE_DFP);
				set_pr(dev, DUAL_ROLE_PROP_PR_SRC);
				set_dr(dev, DUAL_ROLE_PROP_DR_HOST);
			}
			break;

		case PD_DPM_TYPEC_ATTACHED_SNK:
			if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
				gpiod_direction_output(dev->redriver_sel_gpio,
						       !tc_state->polarity);
				set_mode(dev, DUAL_ROLE_PROP_MODE_UFP);
				set_pr(dev, DUAL_ROLE_PROP_PR_SNK);
				set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			}
			break;

		case PD_DPM_TYPEC_CC_FAULT:
			gpiod_direction_output(dev->redriver_sel_gpio, 0);
			set_mode(dev, DUAL_ROLE_PROP_MODE_FAULT);
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
			set_dr(dev, DUAL_ROLE_PROP_DR_NONE);

#ifdef CONFIG_LGE_PM_WATERPROOF_PROTECTION
			prop.intval = 1;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_INPUT_SUSPEND,
						&prop);
#endif
			break;
		}
		break;
	}

	case PD_DPM_PE_EVT_DR_SWAP:
	{
		struct pd_dpm_swap_state *swap_state =
			(struct pd_dpm_swap_state *)state;
		switch (swap_state->new_role) {
		case PD_DATA_ROLE_UFP:
			power_supply_set_supply_type(dev->usb_psy, POWER_SUPPLY_TYPE_USB);
			set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			break;

		case PD_DATA_ROLE_DFP:
			set_dr(dev, DUAL_ROLE_PROP_DR_HOST);
			break;
		}
		break;
	}

	case PD_DPM_PE_EVT_PR_SWAP:
		break;

#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
	case PD_DPM_PE_EVT_DEBUG_ACCESSORY:
	{
		bool is_debug_accessory = *(bool *)state;

		if (dev->is_debug_accessory == is_debug_accessory)
			break;

		dev->is_debug_accessory = is_debug_accessory;

#ifdef CONFIG_LGE_USB_FACTORY
		dev->typec_mode = is_debug_accessory ?
			POWER_SUPPLY_TYPE_CTYPE_DEBUG_ACCESSORY :
			POWER_SUPPLY_TYPE_UNKNOWN;
#endif

#ifdef CONFIG_LGE_USB_DEBUGGER
		schedule_work(&dev->usb_debugger_work);
#endif
		break;
	}
#endif

	default:
		PRINT("%s: Unknown event: %d\n", __func__, event);
		return -EINVAL;
	}

	return 0;
}

int hw_pd_dev_init(struct device *dev)
{
	int rc;

	_hw_pd_dev.dev = dev;
	_hw_pd_dev.mode = DUAL_ROLE_PROP_MODE_NONE;
	_hw_pd_dev.pr = DUAL_ROLE_PROP_PR_NONE;
	_hw_pd_dev.dr = DUAL_ROLE_PROP_DR_NONE;

	dev_set_drvdata(dev, &_hw_pd_dev);

	rc = charger_init(&_hw_pd_dev);
	if (rc)
		return rc;

	_hw_pd_dev.redriver_sel_gpio = devm_gpiod_get(dev, "ti,redriver-sel",
						  GPIOD_OUT_LOW);
	if (IS_ERR(_hw_pd_dev.redriver_sel_gpio)) {
		PRINT("failed to allocate redriver_sel gpio\n");
		_hw_pd_dev.redriver_sel_gpio = NULL;
	}

#ifdef CONFIG_LGE_USB_DEBUGGER
	usb_debugger_init(&_hw_pd_dev);
#endif
#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
    BLOCKING_INIT_NOTIFIER_HEAD(&notifier);
    init_for_mbhc(&notifier);
#endif

	return 0;
}
