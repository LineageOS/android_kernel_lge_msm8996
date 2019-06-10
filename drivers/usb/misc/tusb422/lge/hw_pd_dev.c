#include "hw_pd_dev.h"
#include "tcpm.h"
#include "usb_pd.h"

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "charger.c"
#ifdef CONFIG_LGE_USB_DEBUGGER
#include "usb_debugger.c"
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#include "cc_protect.c"
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
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		[DUAL_ROLE_PROP_MODE_FAULT]	= "FAULT",
#endif
		[DUAL_ROLE_PROP_MODE_NONE]	= "None",
	};

	if (dev->mode == mode)
		return 0;

	switch (mode) {
	case DUAL_ROLE_PROP_MODE_UFP:
	case DUAL_ROLE_PROP_MODE_DFP:
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	case DUAL_ROLE_PROP_MODE_FAULT:
#endif
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

		if (dev->rp) {
			dev->rp = 0;
			prop.intval = 0;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CTYPE_RP,
						&prop);
		}
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
			uint16_t ma = vbus_state->ma;
			int rp = 0;

			if (dev->chg_psy.type == POWER_SUPPLY_TYPE_USB_HVDCP ||
			    dev->chg_psy.type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
				DEBUG("HVDCP is present. ignore Rp advertisement\n");
				if (dev->curr_max) {
					dev->curr_max = 0;
					dev->volt_max = 0;

					prop.intval = dev->curr_max;
					set_property_to_battery(dev,
								POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
								&prop);
				}
				break;
			}

			switch (ma) {
			case 3000: // Rp10K
#ifdef CONFIG_ARCH_MSM8996
				ma = 2000;
#endif
				rp = 10;
				break;
			case 1500: // Rp22K
				rp = 22;
				break;
			case 500:  // Rp56K
				ma = 0;
				rp = 56;
				break;
			default:
				ma = 0;
				rp = 0;
				break;
			}

			if (rp && !dev->rp) {
				dev->rp = rp;
				prop.intval = dev->rp;
				set_property_to_battery(dev,
							POWER_SUPPLY_PROP_CTYPE_RP,
							&prop);
			}

			if (dev->volt_max == vbus_state->mv &&
			    dev->curr_max == ma)
				goto print_vbus_state;

			dev->volt_max = vbus_state->mv;
			dev->curr_max = ma;

			prop.intval = dev->curr_max;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
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
			dev->typec_mode = POWER_SUPPLY_TYPE_UNKNOWN;

			if (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) {
#ifdef CONFIG_LGE_PM_WATERPROOF_PROTECTION
				prop.intval = 0;
				set_property_to_battery(dev,
							POWER_SUPPLY_PROP_INPUT_SUSPEND,
							&prop);
#endif
			}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
			if (dev->is_sbu_ov) {
				enable_irq(dev->cc_protect_irq);
				dev->is_sbu_ov = false;
			}
#endif

			gpiod_direction_output(dev->redriver_sel_gpio, 0);
			//if (dev->usb_ss_en_gpio)
			//	gpiod_direction_output(dev->usb_ss_en_gpio, 0);
			set_mode(dev, DUAL_ROLE_PROP_MODE_NONE);
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
			set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
			break;

		case PD_DPM_TYPEC_ATTACHED_SRC:
			if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
				gpiod_direction_output(dev->redriver_sel_gpio,
						       !tc_state->polarity);
				//if (dev->usb_ss_en_gpio)
				//	gpiod_direction_output(dev->usb_ss_en_gpio, 1);
				set_mode(dev, DUAL_ROLE_PROP_MODE_DFP);
				set_pr(dev, DUAL_ROLE_PROP_PR_SRC);
				set_dr(dev, DUAL_ROLE_PROP_DR_HOST);
			}
			break;

		case PD_DPM_TYPEC_ATTACHED_SNK:
			if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
				gpiod_direction_output(dev->redriver_sel_gpio,
						       !tc_state->polarity);
				//if (dev->usb_ss_en_gpio)
				//	gpiod_direction_output(dev->usb_ss_en_gpio, 1);
				set_mode(dev, DUAL_ROLE_PROP_MODE_UFP);
				set_pr(dev, DUAL_ROLE_PROP_PR_SNK);
				set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			}
			break;

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		case PD_DPM_TYPEC_CC_FAULT:
			gpiod_direction_output(dev->redriver_sel_gpio, 0);
			//if (dev->usb_ss_en_gpio)
			//	gpiod_direction_output(dev->usb_ss_en_gpio, 0);
			set_mode(dev, DUAL_ROLE_PROP_MODE_FAULT);
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
			set_dr(dev, DUAL_ROLE_PROP_DR_NONE);

			dev->chg_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
			dev->curr_max = 0;
			dev->volt_max = 0;

			if (dev->rp) {
				dev->rp = 0;
				prop.intval = 0;
				set_property_to_battery(dev,
							POWER_SUPPLY_PROP_CTYPE_RP,
							&prop);
			}

#ifdef CONFIG_LGE_PM_WATERPROOF_PROTECTION
			prop.intval = 1;
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_INPUT_SUSPEND,
						&prop);
#endif
			break;
#endif
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

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	case PD_DPM_PE_EVENT_GET_SBU_ADC:
		return chg_get_sbu_adc(dev);

	case PD_DPM_PE_EVENT_SET_MOISTURE_DETECT_USE_SBU:
		if (!(dev->moisture_detect_use_sbu && IS_CHARGERLOGO))
			break;

		if (dev->is_present) {
			int sbu_adc = chg_get_sbu_adc(dev);
			if (sbu_adc > SBU_WET_THRESHOLD) {
				PRINT("%s: VBUS/SBU SHORT!!! %d\n", __func__, sbu_adc);
				tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_ADC);
				tcpm_cc_fault_timer(0, false);
			}
		}

		enable_irq(dev->cc_protect_irq);
		break;
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
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#ifdef MOISTURE_DETECT_USE_SBU_TEST
	_hw_pd_dev.moisture_detect_use_sbu = true;
#else
#ifdef CONFIG_LGE_USB_FACTORY
	if (!IS_FACTORY_MODE)
#endif
	if (lge_get_board_rev_no() >= HW_REV_1_3)
		_hw_pd_dev.moisture_detect_use_sbu = true;
#endif
#ifndef CONFIG_MACH_MSM8996_LUCYE_KR
		_hw_pd_dev.moisture_detect_use_sbu = false;
#endif
#endif /* CONFIG_LGE_USB_MOISTURE_DETECT */

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

	_hw_pd_dev.usb_ss_en_gpio = devm_gpiod_get(dev, "ti,usb-ss-en",
						  GPIOD_OUT_HIGH);
	if (IS_ERR(_hw_pd_dev.usb_ss_en_gpio)) {
		PRINT("failed to allocate usb_ss_en gpio\n");
		_hw_pd_dev.usb_ss_en_gpio = NULL;
	}


#ifdef CONFIG_LGE_USB_DEBUGGER
	usb_debugger_init(&_hw_pd_dev);
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	cc_protect_init(&_hw_pd_dev);
#endif
#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
    BLOCKING_INIT_NOTIFIER_HEAD(&notifier);
    init_for_mbhc(&notifier);
#endif

	return 0;
}
