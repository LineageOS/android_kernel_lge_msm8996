/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/completion.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "wcd-mbhc-v2.h"
#include "wcdcal-hwdep.h"

#define HDSET_FACTORY

#if defined(CONFIG_SND_SOC_ES9018)|| defined(CONFIG_SND_SOC_ES9218P)
#ifdef CONFIG_SND_SOC_ES9218P
#define SKIP_RECALC_IMPED
#if defined(SKIP_RECALC_IMPED)  // defensive code
static bool skip_recalc_imped = false;
#endif
#endif
extern bool enable_es9218p;
extern int es9218_sabre_headphone_on(void);
extern int es9218_sabre_headphone_off(void);
extern int es9218_get_power_state(void);
#endif
#include <linux/debugfs.h>

#ifdef HDSET_FACTORY
#include <soc/qcom/lge/board_lge.h>
#endif

#define WCD_MBHC_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | \
			   SND_JACK_OC_HPHR | SND_JACK_LINEOUT | \
			   SND_JACK_MECHANICAL | SND_JACK_MICROPHONE2 | \
			   SND_JACK_UNSUPPORTED)

#define WCD_MBHC_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				  SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				  SND_JACK_BTN_4 | SND_JACK_BTN_5 | \
				  SND_JACK_BTN_6 | SND_JACK_BTN_7)
#define OCP_ATTEMPT 1
#define HS_DETECT_PLUG_TIME_MS (3 * 1000)
#define SPECIAL_HS_DETECT_TIME_MS (2 * 1000)
#define MBHC_BUTTON_PRESS_THRESHOLD_MIN 250
#define GND_MIC_SWAP_THRESHOLD 4
#define WCD_FAKE_REMOVAL_MIN_PERIOD_MS 100
#define HS_VREF_MIN_VAL 1400
#define FW_READ_ATTEMPTS 15
#define FW_READ_TIMEOUT 4000000
#define FAKE_REM_RETRY_ATTEMPTS 3
#define MAX_IMPED 60000

#define WCD_MBHC_BTN_PRESS_COMPL_TIMEOUT_MS  50
#define ANC_DETECT_RETRY_CNT 7
#define WCD_MBHC_SPL_HS_CNT  1

static int det_extn_cable_en;
module_param(det_extn_cable_en, int,
		S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(det_extn_cable_en, "enable/disable extn cable detect");

enum wcd_mbhc_cs_mb_en_flag {
	WCD_MBHC_EN_CS = 0,
	WCD_MBHC_EN_MB,
	WCD_MBHC_EN_PULLUP,
	WCD_MBHC_EN_NONE,
};

static int lge_extn_cable_flag;
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
static bool extn_cable;
#endif

#ifdef CONFIG_MACH_LGE

#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
static bool current_status_detect_water;
struct notifier_block nblock_for_usb;

struct blocking_notifier_head *notifier_from_usb;   // set from usb
struct wcd_mbhc *checkMBHC;                         // check if which current plug type is

enum wcd_mbhc_det_water_usb {
    WCD_MBHC_CLEARED_WATER =0,
    WCD_MBHC_DETECTED_WATER,
};

static int wcd_event_notify_from_usb(struct notifier_block *self, unsigned long val, void *data);
#endif // CONFIG_LGE_MBHC_DET_WATER_ON_USB

#if defined(CONFIG_SND_SOC_ES9218P)
// temporary code before applying tunning values of MBHC
#define LGE_NORMAL_HEADSET_THRESHOLD	50
#define LGE_ADVANCED_HEADSET_THRESHOLD	600
#else /* LGE original from ELSA*/
#define LGE_NORMAL_HEADSET_THRESHOLD	100
#define LGE_ADVANCED_HEADSET_THRESHOLD	400
#endif

#define LGE_SWITCH_NAME_NORMAL		"h2w"
#define LGE_SWITCH_NAME_ADVANCED	"h2w_advanced"
#define LGE_SWITCH_NAME_AUX			"h2w_aux"
#define LGE_SWITCH_NAME_AUX_HIDDEN 	"h2w_aux_hidden"

char *button_name[3] = {"Hook Key", "Voice Assist", "Volume Up"};

enum {
	NO_DEVICE   = 0,
	LGE_HEADSET = (1 << 0),
	LGE_HEADPHONE = (1 << 1),
	LGE_AUX_HIDDEN = (1 << 6),
};

#if defined (CONFIG_LGE_TOUCH_CORE)
void touch_notify_earjack(int value);
#endif

/* z floating defined in ohms */
#define TASHA_ZDET_FLOATING_IMPEDANCE 0x0FFFFFFE // aleady defined in wcd9335.h
static void lge_set_sdev_name(struct wcd_mbhc *mbhc, int status)
{
	int ess_threshold = -30;    // for diva w/o DAC
	pr_debug("%s: enter\n", __func__);

#if defined(CONFIG_SND_SOC_ES9218P)
	if(enable_es9218p)
		ess_threshold = 220;
#endif

#if defined(CONFIG_SND_SOC_ES9218P)
    /*
     * temporary code before applying tunning values of MBHC
     */
    if( (mbhc->zl == TASHA_ZDET_FLOATING_IMPEDANCE && mbhc->zr == TASHA_ZDET_FLOATING_IMPEDANCE) ||
            (mbhc->zl == 0 && mbhc->zr == TASHA_ZDET_FLOATING_IMPEDANCE) ||
                (mbhc->zl == TASHA_ZDET_FLOATING_IMPEDANCE && mbhc->zr == 0) ||
                    (mbhc->zl == 0 && mbhc->zr == 0) ) {
        pr_info("[LGE MBHC] one is floating, and the other one is 0, or both are floating or zero. it could be a Aux Cable(L:%d,R:%d)\n", mbhc->zl, mbhc->zr);
        mbhc->zl = mbhc->zr = TASHA_ZDET_FLOATING_IMPEDANCE;
    }else if( mbhc->zl == TASHA_ZDET_FLOATING_IMPEDANCE && mbhc->zr != TASHA_ZDET_FLOATING_IMPEDANCE ) {
        pr_info("[LGE MBHC] L's imped is flaoting, so L's imped will be assigned with R's (%d)\n", mbhc->zr);
        mbhc->zl = mbhc->zr;
    } else if( mbhc->zl != TASHA_ZDET_FLOATING_IMPEDANCE && mbhc->zr == TASHA_ZDET_FLOATING_IMPEDANCE ) {
        pr_info("[LGE MBHC] R's imped is flaoting, so R's imped will be assigned with L's (%d)\n", mbhc->zl);
        mbhc->zr = mbhc->zl;
    } else {
        pr_info("[LGE MBHC] L's imped is %d, R's imped is %d\n", mbhc->zl, mbhc->zr);
    }

    if ((mbhc->mbhc_cfg->detect_extn_cable) && (status == SND_JACK_LINEOUT))
		mbhc->sdev.name = LGE_SWITCH_NAME_AUX_HIDDEN;
	else if ((mbhc->zl > LGE_ADVANCED_HEADSET_THRESHOLD-ess_threshold) &&
		       (mbhc->zr > LGE_ADVANCED_HEADSET_THRESHOLD-ess_threshold))
		mbhc->sdev.name = LGE_SWITCH_NAME_AUX;
	else if (mbhc->zr < LGE_NORMAL_HEADSET_THRESHOLD)
		mbhc->sdev.name = LGE_SWITCH_NAME_NORMAL;
	else if ( (mbhc->zr >= LGE_NORMAL_HEADSET_THRESHOLD && mbhc->zr < LGE_ADVANCED_HEADSET_THRESHOLD-ess_threshold) ||
                (mbhc->zl >= LGE_NORMAL_HEADSET_THRESHOLD && mbhc->zl < LGE_ADVANCED_HEADSET_THRESHOLD-ess_threshold) )
		mbhc->sdev.name = LGE_SWITCH_NAME_ADVANCED;
	else
		mbhc->sdev.name = LGE_SWITCH_NAME_AUX;
#else   /* LGE original from ELSA*/
    if ((mbhc->mbhc_cfg->detect_extn_cable) && (status == SND_JACK_LINEOUT))
		mbhc->sdev.name = LGE_SWITCH_NAME_AUX_HIDDEN;
	else if ((mbhc->zl >= LGE_ADVANCED_HEADSET_THRESHOLD) ||
		       (mbhc->zr >= LGE_ADVANCED_HEADSET_THRESHOLD))
		mbhc->sdev.name = LGE_SWITCH_NAME_AUX;
	else if (mbhc->zr < (LGE_NORMAL_HEADSET_THRESHOLD - ess_threshold))
		mbhc->sdev.name = LGE_SWITCH_NAME_NORMAL;
	else if (mbhc->zr >= (LGE_NORMAL_HEADSET_THRESHOLD - ess_threshold) &&
				mbhc->zr < LGE_ADVANCED_HEADSET_THRESHOLD)
		mbhc->sdev.name = LGE_SWITCH_NAME_ADVANCED;
	else
		mbhc->sdev.name = LGE_SWITCH_NAME_AUX;
#endif

	pr_debug("[LGE MBHC] sdev.name: %s\n", mbhc->sdev.name);
	pr_debug("%s: leave\n", __func__);
}

static int lge_set_switch_device(struct wcd_mbhc *mbhc, int status)
{
	int result = NO_DEVICE;

	pr_debug("%s: enter. status:0x%x\n", __func__, status);

	status = status & ~SND_JACK_MECHANICAL;

	switch (status) {
		case 0:
		case SND_JACK_UNSUPPORTED:
			break;
		case SND_JACK_HEADPHONE:
		case SND_JACK_HEADSET:
		case SND_JACK_LINEOUT:
			lge_set_sdev_name(mbhc, status);
			if ((mbhc->mbhc_cfg->detect_extn_cable) && (status == SND_JACK_LINEOUT)) {
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
				extn_cable = true;
#endif
				result = LGE_AUX_HIDDEN;
				pr_info("[LGE MBHC] AUX HIDDEN jack is inserted.\n");
				break;
				}
			result = (status == SND_JACK_HEADPHONE || status == SND_JACK_LINEOUT)? LGE_HEADPHONE : LGE_HEADSET;
			pr_info("[LGE MBHC] %s %s is Inserted. jack_type=0x%x\n",
						(result-1)?"3 Pin":"4 Pin",
#if defined(CONFIG_SND_SOC_ES9218P)
						(mbhc->zr < (enable_es9218p?LGE_ADVANCED_HEADSET_THRESHOLD-220:LGE_ADVANCED_HEADSET_THRESHOLD))?"Headset":"AUX Cable",
#else
                        (mbhc->zr < LGE_ADVANCED_HEADSET_THRESHOLD)?"Headset":"AUX Cable",
#endif
						status);
#if defined (CONFIG_LGE_TOUCH_CORE)
			touch_notify_earjack(1);
#endif
			break;
		default:
			pr_err("[LGE MBHC] Unsupported jack type(0x%x) is reported.\n", status);
			break;
	}

	pr_debug("[LGE MBHC] sdev state: %d, sdev name: %s\n", result, mbhc->sdev.name);
	return result;
}
#endif

static int wcd_mbhc_get_button_mask(struct wcd_mbhc *mbhc);

static void wcd_mbhc_jack_report(struct wcd_mbhc *mbhc,
				struct snd_soc_jack *jack, int status, int mask)
{
#ifdef CONFIG_MACH_LGE
	int switch_device = lge_set_switch_device(mbhc, status);
	pr_debug("%s: status:0x%x, mask:0x%x\n", __func__, status, mask);

#if defined(CONFIG_SND_SOC_ES9218P) && defined(CONFIG_LGE_MBHC_DET_WATER_ON_USB)
    // detect water on USB AND 4 pole AND aux cable
    if( current_status_detect_water && switch_device == LGE_HEADSET && !strcmp(mbhc->sdev.name, LGE_SWITCH_NAME_AUX)) {
        pr_debug("%s: detect water on USB, and do not report an event to user space \n", __func__);
        es9218_sabre_headphone_off();
        return;
    }
#endif // CONFIG_LGE_MBHC_DET_WATER_ON_USB
//	if (!(switch_device == LGE_AUX_HIDDEN)) {
		snd_soc_jack_report(jack, status, mask);
//	}
	if(status & (SND_JACK_OC_HPHL | SND_JACK_OC_HPHR))
		pr_err("[LGE MBHC] HPH OCP IRQ(0x%x) is reported.\n", status);

	if ((mask == WCD_MBHC_JACK_MASK) &&
	    !(status & (SND_JACK_OC_HPHL | SND_JACK_OC_HPHR))){
		switch_set_state(&mbhc->sdev, switch_device);
#if defined(CONFIG_SND_SOC_ES9018)|| defined(CONFIG_SND_SOC_ES9218P)
		if(enable_es9218p) {
			if (status == 0)
				es9218_sabre_headphone_off();
			else if (status == SND_JACK_HEADPHONE
				|| status == SND_JACK_HEADSET
				|| status == SND_JACK_LINEOUT)
				es9218_sabre_headphone_on();
			else
				pr_debug("%s: not reported to switch_dev\n", __func__);
		}
#endif
	}
#else
	snd_soc_jack_report(jack, status, mask);
#endif
}

static void __hphocp_off_report(struct wcd_mbhc *mbhc, u32 jack_status,
				int irq)
{
	struct snd_soc_codec *codec = mbhc->codec;

	dev_dbg(codec->dev, "%s: clear ocp status %x\n",
		__func__, jack_status);

	if (mbhc->hph_status & jack_status) {
		mbhc->hph_status &= ~jack_status;
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				     mbhc->hph_status, WCD_MBHC_JACK_MASK);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_OCP_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_OCP_FSM_EN, 1);
		/*
		 * reset retry counter as PA is turned off signifying
		 * start of new OCP detection session
		 */
		if (mbhc->intr_ids->hph_left_ocp)
			mbhc->hphlocp_cnt = 0;
		else
			mbhc->hphrocp_cnt = 0;
		mbhc->mbhc_cb->irq_control(codec, irq, true);
	}
}

static void hphrocp_off_report(struct wcd_mbhc *mbhc, u32 jack_status)
{
	__hphocp_off_report(mbhc, SND_JACK_OC_HPHR,
			    mbhc->intr_ids->hph_right_ocp);
}

static void hphlocp_off_report(struct wcd_mbhc *mbhc, u32 jack_status)
{
	__hphocp_off_report(mbhc, SND_JACK_OC_HPHL,
			    mbhc->intr_ids->hph_left_ocp);
}

static void wcd_program_hs_vref(struct wcd_mbhc *mbhc)
{
	struct wcd_mbhc_plug_type_cfg *plug_type_cfg;
	struct snd_soc_codec *codec = mbhc->codec;
	u32 reg_val;

	plug_type_cfg = WCD_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);
	reg_val = ((plug_type_cfg->v_hs_max - HS_VREF_MIN_VAL) / 100);

	dev_dbg(codec->dev, "%s: reg_val  = %x\n", __func__, reg_val);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HS_VREF, reg_val);
}

static void wcd_program_btn_threshold(const struct wcd_mbhc *mbhc, bool micbias)
{
	struct wcd_mbhc_btn_detect_cfg *btn_det;
	struct snd_soc_codec *codec = mbhc->codec;
	struct snd_soc_card *card = codec->component.card;
	s16 *btn_low, *btn_high;

	if (mbhc->mbhc_cfg->calibration == NULL) {
		dev_err(card->dev, "%s: calibration data is NULL\n", __func__);
		return;
	}

	btn_det = WCD_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);
	btn_low = btn_det->_v_btn_low;
	btn_high = ((void *)&btn_det->_v_btn_low) +
			(sizeof(btn_det->_v_btn_low[0]) * btn_det->num_btn);

	mbhc->mbhc_cb->set_btn_thr(codec, btn_low, btn_high, btn_det->num_btn,
				   micbias);
}

static void wcd_enable_curr_micbias(const struct wcd_mbhc *mbhc,
				const enum wcd_mbhc_cs_mb_en_flag cs_mb_en)
{

	/*
	 * Some codecs handle micbias/pullup enablement in codec
	 * drivers itself and micbias is not needed for regular
	 * plug type detection. So if micbias_control callback function
	 * is defined, just return.
	 */
	if (mbhc->mbhc_cb->mbhc_micbias_control)
		return;

	pr_debug("%s: enter, cs_mb_en: %d\n", __func__, cs_mb_en);

	switch (cs_mb_en) {
	case WCD_MBHC_EN_CS:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MICB_CTRL, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 3);
		/* Program Button threshold registers as per CS */
		wcd_program_btn_threshold(mbhc, false);
		break;
	case WCD_MBHC_EN_MB:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);

		/* Disable PULL_UP_EN & enable MICBIAS */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MICB_CTRL, 2);
		/* Program Button threshold registers as per MICBIAS */
		wcd_program_btn_threshold(mbhc, true);
		break;
	case WCD_MBHC_EN_PULLUP:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 3);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MICB_CTRL, 1);
		/* Program Button threshold registers as per MICBIAS */
		wcd_program_btn_threshold(mbhc, true);
		break;
	case WCD_MBHC_EN_NONE:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MICB_CTRL, 0);
		break;
	default:
		pr_debug("%s: Invalid parameter", __func__);
		break;
	}

	pr_debug("%s: exit\n", __func__);
}

static const char *wcd_mbhc_get_event_string(int event)
{
	switch (event) {
	case WCD_EVENT_PRE_MICBIAS_2_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_MICBIAS_2_OFF);
	case WCD_EVENT_POST_MICBIAS_2_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_POST_MICBIAS_2_OFF);
	case WCD_EVENT_PRE_MICBIAS_2_ON:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_MICBIAS_2_ON);
	case WCD_EVENT_POST_MICBIAS_2_ON:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_POST_MICBIAS_2_ON);
	case WCD_EVENT_PRE_HPHL_PA_ON:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_HPHL_PA_ON);
	case WCD_EVENT_POST_HPHL_PA_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_POST_HPHL_PA_OFF);
	case WCD_EVENT_PRE_HPHR_PA_ON:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_HPHR_PA_ON);
	case WCD_EVENT_POST_HPHR_PA_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_POST_HPHR_PA_OFF);
	case WCD_EVENT_PRE_HPHR_PA_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_HPHR_PA_OFF);
	case WCD_EVENT_PRE_HPHL_PA_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_HPHL_PA_OFF);
	case WCD_EVENT_POST_DAPM_MICBIAS_2_ON:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_POST_DAPM_MICBIAS_2_ON);
	case WCD_EVENT_PRE_DAPM_MICBIAS_2_ON:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_DAPM_MICBIAS_2_ON);
	case WCD_EVENT_POST_DAPM_MICBIAS_2_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_POST_DAPM_MICBIAS_2_OFF);
	case WCD_EVENT_PRE_DAPM_MICBIAS_2_OFF:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_PRE_DAPM_MICBIAS_2_OFF);
	case WCD_EVENT_INVALID:
	default:
		return WCD_MBHC_STRINGIFY(WCD_EVENT_INVALID);
	}
}

static int wcd_event_notify(struct notifier_block *self, unsigned long val,
			    void *data)
{
	struct wcd_mbhc *mbhc = (struct wcd_mbhc *)data;
	enum wcd_notify_event event = (enum wcd_notify_event)val;
	struct snd_soc_codec *codec = mbhc->codec;
	bool micbias2 = false;
	bool micbias1 = false;

	pr_debug("%s: event %s (%d)\n", __func__,
		 wcd_mbhc_get_event_string(event), event);
	if (mbhc->mbhc_cb->micbias_enable_status) {
		micbias2 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_2);
		micbias1 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_1);
	}
	switch (event) {
	/* MICBIAS usage change */
	case WCD_EVENT_POST_DAPM_MICBIAS_2_ON:
		mbhc->is_hs_recording = true;
		pr_debug("%s: is_capture: %d\n", __func__,
			  mbhc->is_hs_recording);
		break;
	case WCD_EVENT_POST_MICBIAS_2_ON:
		if (!mbhc->micbias_enable)
			goto out_micb_en;
		if (mbhc->mbhc_cb->mbhc_common_micb_ctrl) {
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_PRECHARGE,
					true);
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_SET_VAL,
					true);
			/*
			 * Special headset needs MICBIAS as 2.7V so wait for
			 * 50 msec for the MICBIAS to reach 2.7 volts.
			 */
			msleep(50);
		}
		if (mbhc->mbhc_cb->set_auto_zeroing)
			mbhc->mbhc_cb->set_auto_zeroing(codec, true);
		if (mbhc->mbhc_cb->mbhc_common_micb_ctrl)
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_PRECHARGE,
					false);
out_micb_en:
		/* Disable current source if micbias enabled */
		if (!mbhc->mbhc_cb->mbhc_micbias_control) {
			mbhc->is_hs_recording = true;
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
		}
		/* configure cap settings properly when micbias is enabled */
		if (mbhc->mbhc_cb->set_cap_mode)
			mbhc->mbhc_cb->set_cap_mode(codec, micbias1, true);
		break;
	case WCD_EVENT_PRE_MICBIAS_2_OFF:
		break;
	/* MICBIAS usage change */
	case WCD_EVENT_POST_DAPM_MICBIAS_2_OFF:
		mbhc->is_hs_recording = false;
		pr_debug("%s: is_capture: %d\n", __func__,
			  mbhc->is_hs_recording);
		break;
	case WCD_EVENT_POST_MICBIAS_2_OFF:
		if (!mbhc->mbhc_cb->mbhc_micbias_control)
			mbhc->is_hs_recording = false;
		if (mbhc->micbias_enable) {
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
			break;
		}

		if (mbhc->mbhc_cb->set_auto_zeroing)
			mbhc->mbhc_cb->set_auto_zeroing(codec, false);
		if (mbhc->mbhc_cb->set_micbias_value && !mbhc->micbias_enable)
			mbhc->mbhc_cb->set_micbias_value(codec);
		/* Enable PULL UP if PA's are enabled */
		if ((test_bit(WCD_MBHC_EVENT_PA_HPHL, &mbhc->event_state)) ||
				(test_bit(WCD_MBHC_EVENT_PA_HPHR,
					  &mbhc->event_state)))
			/* enable pullup and cs, disable mb */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_PULLUP);
		else
			/* enable current source and disable mb, pullup*/
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_CS);

		/* configure cap settings properly when micbias is disabled */
		if (mbhc->mbhc_cb->set_cap_mode)
			mbhc->mbhc_cb->set_cap_mode(codec, micbias1, false);
		break;
	case WCD_EVENT_PRE_HPHL_PA_OFF:
		mutex_lock(&mbhc->hphl_pa_lock);
		break;
	case WCD_EVENT_POST_HPHL_PA_OFF:
		clear_bit(WCD_MBHC_HPHL_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
		if (mbhc->hph_status & SND_JACK_OC_HPHL)
			hphlocp_off_report(mbhc, SND_JACK_OC_HPHL);
		clear_bit(WCD_MBHC_EVENT_PA_HPHL, &mbhc->event_state);
		/* check if micbias is enabled */
		if (micbias2)
			/* Disable cs, pullup & enable micbias */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
		else
			/* Disable micbias, pullup & enable cs */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_CS);
		mutex_unlock(&mbhc->hphl_pa_lock);
		break;
	case WCD_EVENT_PRE_HPHR_PA_OFF:
		mutex_lock(&mbhc->hphr_pa_lock);
		break;
	case WCD_EVENT_POST_HPHR_PA_OFF:
		clear_bit(WCD_MBHC_HPHR_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
		if (mbhc->hph_status & SND_JACK_OC_HPHR)
			hphrocp_off_report(mbhc, SND_JACK_OC_HPHR);
		clear_bit(WCD_MBHC_EVENT_PA_HPHR, &mbhc->event_state);
		/* check if micbias is enabled */
		if (micbias2)
			/* Disable cs, pullup & enable micbias */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
		else
			/* Disable micbias, pullup & enable cs */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_CS);
		mutex_unlock(&mbhc->hphr_pa_lock);
		break;
	case WCD_EVENT_PRE_HPHL_PA_ON:
		set_bit(WCD_MBHC_EVENT_PA_HPHL, &mbhc->event_state);
		/* check if micbias is enabled */
		if (micbias2)
			/* Disable cs, pullup & enable micbias */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
		else
			/* Disable micbias, enable pullup & cs */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_PULLUP);
		break;
	case WCD_EVENT_PRE_HPHR_PA_ON:
		set_bit(WCD_MBHC_EVENT_PA_HPHR, &mbhc->event_state);
		/* check if micbias is enabled */
		if (micbias2)
			/* Disable cs, pullup & enable micbias */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
		else
			/* Disable micbias, enable pullup & cs */
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_PULLUP);
		break;
	default:
		break;
	}
	return 0;
}

static int wcd_cancel_btn_work(struct wcd_mbhc *mbhc)
{
	int r;

	r = cancel_delayed_work_sync(&mbhc->mbhc_btn_dwork);
	/*
	 * if scheduled mbhc.mbhc_btn_dwork is canceled from here,
	 * we have to unlock from here instead btn_work
	 */
	if (r)
		mbhc->mbhc_cb->lock_sleep(mbhc, false);
	return r;
}

static bool wcd_swch_level_remove(struct wcd_mbhc *mbhc)
{
	u16 result2;

	WCD_MBHC_REG_READ(WCD_MBHC_SWCH_LEVEL_REMOVE, result2);
	return (result2) ? true : false;
}

/* should be called under interrupt context that hold suspend */
static void wcd_schedule_hs_detect_plug(struct wcd_mbhc *mbhc,
					    struct work_struct *work)
{
	pr_debug("%s: scheduling correct_swch_plug\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);
	mbhc->hs_detect_work_stop = false;
	mbhc->mbhc_cb->lock_sleep(mbhc, true);
	schedule_work(work);
}

/* called under codec_resource_lock acquisition */
static void wcd_cancel_hs_detect_plug(struct wcd_mbhc *mbhc,
					 struct work_struct *work)
{
	pr_debug("%s: Canceling correct_plug_swch\n", __func__);
	mbhc->hs_detect_work_stop = true;
	WCD_MBHC_RSC_UNLOCK(mbhc);
	if (cancel_work_sync(work)) {
		pr_debug("%s: correct_plug_swch is canceled\n",
			 __func__);
		mbhc->mbhc_cb->lock_sleep(mbhc, false);
	}
	WCD_MBHC_RSC_LOCK(mbhc);
}

static void wcd_mbhc_clr_and_turnon_hph_padac(struct wcd_mbhc *mbhc)
{
	bool pa_turned_on = false;
	u8 wg_time;

	WCD_MBHC_REG_READ(WCD_MBHC_HPH_CNP_WG_TIME, wg_time);
	wg_time += 1;

	mutex_lock(&mbhc->hphr_pa_lock);
	if (test_and_clear_bit(WCD_MBHC_HPHR_PA_OFF_ACK,
			       &mbhc->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable PA\n", __func__);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHR_PA_EN, 1);
		pa_turned_on = true;
	}
	mutex_unlock(&mbhc->hphr_pa_lock);
	mutex_lock(&mbhc->hphl_pa_lock);
	if (test_and_clear_bit(WCD_MBHC_HPHL_PA_OFF_ACK,
			       &mbhc->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable PA\n", __func__);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHL_PA_EN, 1);
		pa_turned_on = true;
	}
	mutex_unlock(&mbhc->hphl_pa_lock);

	if (pa_turned_on) {
		pr_debug("%s: PA was turned on by MBHC and not by DAPM\n",
			 __func__);
		usleep_range(wg_time * 1000, wg_time * 1000 + 50);
	}
}

static bool wcd_mbhc_is_hph_pa_on(struct wcd_mbhc *mbhc)
{
	bool hph_pa_on = false;

	WCD_MBHC_REG_READ(WCD_MBHC_HPH_PA_EN, hph_pa_on);

	return (hph_pa_on) ? true : false;
}

static void wcd_mbhc_set_and_turnoff_hph_padac(struct wcd_mbhc *mbhc)
{
	u8 wg_time;

	WCD_MBHC_REG_READ(WCD_MBHC_HPH_CNP_WG_TIME, wg_time);
	wg_time += 1;

	/* If headphone PA is on, check if userspace receives
	* removal event to sync-up PA's state */
	if (wcd_mbhc_is_hph_pa_on(mbhc)) {
		pr_debug("%s PA is on, setting PA_OFF_ACK\n", __func__);
		set_bit(WCD_MBHC_HPHL_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
		set_bit(WCD_MBHC_HPHR_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
	} else {
		pr_debug("%s PA is off\n", __func__);
	}
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPH_PA_EN, 0);
	usleep_range(wg_time * 1000, wg_time * 1000 + 50);
}

int wcd_mbhc_get_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
			uint32_t *zr)
{
	*zl = mbhc->zl;
	*zr = mbhc->zr;

	if (*zl && *zr)
		return 0;
	else
		return -EINVAL;
}

static void wcd_mbhc_hs_elec_irq(struct wcd_mbhc *mbhc, int irq_type,
				 bool enable)
{
	int irq;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (irq_type == WCD_MBHC_ELEC_HS_INS)
		irq = mbhc->intr_ids->mbhc_hs_ins_intr;
	else if (irq_type == WCD_MBHC_ELEC_HS_REM)
		irq = mbhc->intr_ids->mbhc_hs_rem_intr;
	else {
		pr_debug("%s: irq_type: %d, enable: %d\n",
			__func__, irq_type, enable);
		return;
	}

	pr_debug("%s: irq: %d, enable: %d, intr_status:%lu\n",
		 __func__, irq, enable, mbhc->intr_status);
	if ((test_bit(irq_type, &mbhc->intr_status)) != enable) {
		mbhc->mbhc_cb->irq_control(mbhc->codec, irq, enable);
		if (enable)
			set_bit(irq_type, &mbhc->intr_status);
		else
			clear_bit(irq_type, &mbhc->intr_status);
	}
}

static void wcd_mbhc_report_plug(struct wcd_mbhc *mbhc, int insertion,
				enum snd_jack_types jack_type)
{
	struct snd_soc_codec *codec = mbhc->codec;
	bool is_pa_on = false;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	pr_debug("%s: enter insertion %d hph_status %x\n",
		 __func__, insertion, mbhc->hph_status);
	if (!insertion) {
		/* Report removal */
		mbhc->hph_status &= ~jack_type;
		/*
		 * cancel possibly scheduled btn work and
		 * report release if we reported button press
		 */
		if (wcd_cancel_btn_work(mbhc)) {
			pr_debug("%s: button press is canceled\n", __func__);
		} else if (mbhc->buttons_pressed) {
			pr_debug("%s: release of button press%d\n",
				 __func__, jack_type);
			wcd_mbhc_jack_report(mbhc, &mbhc->button_jack, 0,
					    mbhc->buttons_pressed);
			mbhc->buttons_pressed &=
				~WCD_MBHC_JACK_BUTTON_MASK;
		}
		if (mbhc->micbias_enable) {
			if (mbhc->mbhc_cb->mbhc_micbias_control)
				mbhc->mbhc_cb->mbhc_micbias_control(
						codec, MIC_BIAS_2,
						MICB_DISABLE);
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
				mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
						codec,
						MIC_BIAS_2, false);
			if (mbhc->mbhc_cb->set_micbias_value) {
				mbhc->mbhc_cb->set_micbias_value(codec);
				WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MICB_CTRL, 0);
			}
			mbhc->micbias_enable = false;
		}
		mbhc->hph_type = WCD_MBHC_HPH_NONE;
		mbhc->zl = mbhc->zr = 0;
		pr_debug("%s: Reporting removal %d(%x)\n", __func__,
			 jack_type, mbhc->hph_status);
		pr_info("[LGE MBHC] %s cable is Removed. jack_type=%x\n",
					(jack_type-1)?"4 Pin":"3 Pin",
					jack_type);
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				mbhc->hph_status, WCD_MBHC_JACK_MASK);
		wcd_mbhc_set_and_turnoff_hph_padac(mbhc);
		hphrocp_off_report(mbhc, SND_JACK_OC_HPHR);
		hphlocp_off_report(mbhc, SND_JACK_OC_HPHL);
		mbhc->current_plug = MBHC_PLUG_TYPE_NONE;
#ifdef CONFIG_MACH_LGE
		if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
			mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
							MIC_BIAS_2, false);
#endif
#if defined (CONFIG_MACH_LGE) && (CONFIG_LGE_TOUCH_CORE)
		touch_notify_earjack(0);
#endif
	} else {
		/*
		 * Report removal of current jack type.
		 * Headphone to headset shouldn't report headphone
		 * removal.
		 */
#ifndef CONFIG_MACH_LGE	/* Qualcomm Orig. */
		if (mbhc->mbhc_cfg->detect_extn_cable &&
		    (mbhc->current_plug == MBHC_PLUG_TYPE_HIGH_HPH ||
		    jack_type == SND_JACK_LINEOUT) &&
		    (mbhc->hph_status && mbhc->hph_status != jack_type)) {
#else
		if (mbhc->hph_status && mbhc->hph_status != jack_type) {
#endif

#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
			if (mbhc->micbias_enable &&
			    mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET && !mbhc->micbias_enable) {
#else
			if (mbhc->micbias_enable &&
			    mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET) {
#endif
				if (mbhc->mbhc_cb->mbhc_micbias_control)
					mbhc->mbhc_cb->mbhc_micbias_control(
						codec, MIC_BIAS_2,
						MICB_DISABLE);
				if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
					mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
						codec,
						MIC_BIAS_2, false);
				if (mbhc->mbhc_cb->set_micbias_value) {
					mbhc->mbhc_cb->set_micbias_value(
							codec);
					WCD_MBHC_REG_UPDATE_BITS(
							WCD_MBHC_MICB_CTRL, 0);
				}
				mbhc->micbias_enable = false;
			}
			mbhc->hph_type = WCD_MBHC_HPH_NONE;
#if defined(CONFIG_SND_SOC_ES9218P) && defined(SKIP_RECALC_IMPED)
            if( skip_recalc_imped ) {
                pr_info("[LGE MBHC] 2 skip_recalc_imped, skip mbhc->zl = mbhc->zr = 0\n");
            } else {
			    mbhc->zl = mbhc->zr = 0;
            }
#else /* Qualcomm Original */
            mbhc->zl = mbhc->zr = 0;
#endif
			pr_debug("%s: Reporting removal (%x)\n",
				 __func__, mbhc->hph_status);
			if ( !((mbhc->mbhc_cfg->detect_extn_cable) && (mbhc->hph_status == SND_JACK_LINEOUT))) {
				wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
					0, WCD_MBHC_JACK_MASK);
			}

			if (mbhc->hph_status == SND_JACK_LINEOUT) {

				pr_debug("%s: Enable micbias\n", __func__);
				/* Disable current source and enable micbias */
				wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
				pr_debug("%s: set up elec removal detection\n",
					  __func__);
				WCD_MBHC_REG_UPDATE_BITS(
						WCD_MBHC_ELECT_DETECTION_TYPE,
						0);
				usleep_range(200, 210);
				pr_info("[LGE MBHC] %s: WCD_MBHC_ELEC_HS_REM enable \n", __func__);
				wcd_mbhc_hs_elec_irq(mbhc,
						     WCD_MBHC_ELEC_HS_REM,
						     true);
			}
			mbhc->hph_status &= ~(SND_JACK_HEADSET |
						SND_JACK_LINEOUT |
						SND_JACK_ANC_HEADPHONE |
						SND_JACK_UNSUPPORTED);
		}

		if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET &&
			jack_type == SND_JACK_HEADPHONE)
			mbhc->hph_status &= ~SND_JACK_HEADSET;

		/* Report insertion */
		if (jack_type == SND_JACK_HEADPHONE)
			mbhc->current_plug = MBHC_PLUG_TYPE_HEADPHONE;
		else if (jack_type == SND_JACK_UNSUPPORTED)
			mbhc->current_plug = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		else if (jack_type == SND_JACK_HEADSET) {
			mbhc->current_plug = MBHC_PLUG_TYPE_HEADSET;
			mbhc->jiffies_atreport = jiffies;
		} else if (jack_type == SND_JACK_LINEOUT) {
			mbhc->current_plug = MBHC_PLUG_TYPE_HIGH_HPH;
		} else if (jack_type == SND_JACK_ANC_HEADPHONE)
			mbhc->current_plug = MBHC_PLUG_TYPE_ANC_HEADPHONE;

		if (mbhc->mbhc_cb->hph_pa_on_status)
			is_pa_on = mbhc->mbhc_cb->hph_pa_on_status(codec);

		if (mbhc->impedance_detect &&
			mbhc->mbhc_cb->compute_impedance &&
			(mbhc->mbhc_cfg->linein_th != 0) &&
			(!is_pa_on)) {
#if defined(CONFIG_SND_SOC_ES9218P) && defined(SKIP_RECALC_IMPED)
                if( enable_es9218p) { /* for models with DAC */
                    if( skip_recalc_imped ) {
                        pr_info("[LGE MBHC] 3 skip_recalc_imped, skip compute_impedance() \n");
                    } else {
                        mbhc->mbhc_cb->compute_impedance(mbhc, &mbhc->zl, &mbhc->zr);
                    }
                } else { /* for models without DAC */
                    mbhc->mbhc_cb->compute_impedance(mbhc, &mbhc->zl, &mbhc->zr);
                }
#else /* Qualcomm Original */
                    mbhc->mbhc_cb->compute_impedance(mbhc,
                        &mbhc->zl, &mbhc->zr);
#endif
#ifndef CONFIG_MACH_LGE
			if ((mbhc->zl > mbhc->mbhc_cfg->linein_th &&
				mbhc->zl < MAX_IMPED) &&
				(mbhc->zr > mbhc->mbhc_cfg->linein_th &&
				 mbhc->zr < MAX_IMPED) &&
				(jack_type == SND_JACK_HEADPHONE)) {
				jack_type = SND_JACK_LINEOUT;
				mbhc->current_plug = MBHC_PLUG_TYPE_HIGH_HPH;
				if (mbhc->hph_status) {
					mbhc->hph_status &= ~(SND_JACK_HEADSET |
							SND_JACK_LINEOUT |
							SND_JACK_UNSUPPORTED);
					wcd_mbhc_jack_report(mbhc,
							&mbhc->headset_jack,
							mbhc->hph_status,
							WCD_MBHC_JACK_MASK);
				}
				pr_debug("%s: Marking jack type as SND_JACK_LINEOUT\n",
				__func__);
			}
#endif
		}

		mbhc->hph_status |= jack_type;
#if defined(CONFIG_SND_SOC_ES9018)|| defined(CONFIG_SND_SOC_ES9218P)
		if (enable_es9218p)
			es9218_sabre_headphone_on();
#endif
		pr_debug("%s: Reporting insertion %d(%x)\n", __func__,
			 jack_type, mbhc->hph_status);
		pr_info("[LGE MBHC] Impedance zl: %d, zr:%d\n", mbhc->zl, mbhc->zr);
		if (mbhc->mbhc_cfg->detect_extn_cable) {
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD_MBHC_JACK_MASK);
		} else {
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				    (mbhc->hph_status | SND_JACK_MECHANICAL),
				    WCD_MBHC_JACK_MASK);
		}
		wcd_mbhc_clr_and_turnon_hph_padac(mbhc);
	}
	pr_debug("%s: leave hph_status %x\n", __func__, mbhc->hph_status);
}

static bool wcd_mbhc_detect_anc_plug_type(struct wcd_mbhc *mbhc)
{
	bool anc_mic_found = false;
	u16 val, hs_comp_res, btn_status = 0;
	unsigned long retry = 0;
	int valid_plug_cnt = 0, invalid_plug_cnt = 0;
	int btn_status_cnt = 0;
	bool is_check_btn_press = false;


	if (mbhc->mbhc_cfg->anc_micbias < MIC_BIAS_1 ||
	    mbhc->mbhc_cfg->anc_micbias > MIC_BIAS_4)
		return false;

	if (!mbhc->mbhc_cb->mbhc_micbias_control)
		return false;

	WCD_MBHC_REG_READ(WCD_MBHC_FSM_EN, val);

	if (val)
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);

	mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
					    mbhc->mbhc_cfg->anc_micbias,
					    MICB_ENABLE);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, 0x2);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ANC_DET_EN, 1);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	/*
	 * wait for button debounce time 20ms. If 4-pole plug is inserted
	 * into 5-pole jack, then there will be a button press interrupt
	 * during anc plug detection. In that case though Hs_comp_res is 0,
	 * it should not be declared as ANC plug type
	 */
	usleep_range(20000, 20100);

	/*
	 * After enabling FSM, to handle slow insertion scenarios,
	 * check hs_comp_result for few times to see if the IN3 voltage
	 * is below the Vref
	 */
	do {
		if (wcd_swch_level_remove(mbhc)) {
			pr_debug("%s: Switch level is low\n", __func__);
			goto exit;
		}
		pr_debug("%s: Retry attempt %lu\n", __func__, retry + 1);
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);

		if (!hs_comp_res) {
			valid_plug_cnt++;
			is_check_btn_press = true;
		} else
			invalid_plug_cnt++;
		/* Wait 1ms before taking another reading */
		usleep_range(1000, 1100);

		WCD_MBHC_REG_READ(WCD_MBHC_FSM_STATUS, btn_status);
		if (btn_status)
			btn_status_cnt++;

		retry++;
	} while (retry < ANC_DETECT_RETRY_CNT);

	pr_debug("%s: valid: %d, invalid: %d, btn_status_cnt: %d\n",
		 __func__, valid_plug_cnt, invalid_plug_cnt, btn_status_cnt);

	/* decision logic */
	if ((valid_plug_cnt > invalid_plug_cnt) && is_check_btn_press &&
	    (btn_status_cnt == 0))
		anc_mic_found = true;
exit:
	if (!val)
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ANC_DET_EN, 0);

	mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
					    mbhc->mbhc_cfg->anc_micbias,
					    MICB_DISABLE);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, 0x0);
	pr_debug("%s: anc mic %sfound\n", __func__,
		 anc_mic_found ? "" : "not ");
	return anc_mic_found;
}

static void wcd_mbhc_find_plug_and_report(struct wcd_mbhc *mbhc,
					 enum wcd_mbhc_plug_type plug_type)
{
	bool anc_mic_found = false;
	enum snd_jack_types jack_type;
	pr_info("[LGE MBHC] %s: enter current_plug(%d) new_plug(%d)\n",
		 __func__, mbhc->current_plug, plug_type);

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (mbhc->current_plug == plug_type) {
		pr_debug("%s: cable already reported, exit\n", __func__);
		goto exit;
	}

	if (plug_type == MBHC_PLUG_TYPE_HEADPHONE) {
		/*
		 * Nothing was reported previously
		 * report a headphone or unsupported
		 */
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_HEADPHONE);
	} else if (plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP) {
			if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADPHONE)
				wcd_mbhc_report_plug(mbhc, 0,
						SND_JACK_HEADPHONE);
			if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET)
				wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADSET);
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_UNSUPPORTED);
	} else if (plug_type == MBHC_PLUG_TYPE_HEADSET) {
		if (mbhc->mbhc_cfg->enable_anc_mic_detect)
			anc_mic_found = wcd_mbhc_detect_anc_plug_type(mbhc);

		jack_type = SND_JACK_HEADSET;
		if (anc_mic_found)
			jack_type = SND_JACK_ANC_HEADPHONE;

		/*
		 * If Headphone was reported previously, this will
		 * only report the mic line
		 */
		wcd_mbhc_report_plug(mbhc, 1, jack_type);
	} else if (plug_type == MBHC_PLUG_TYPE_HIGH_HPH) {
		if (mbhc->mbhc_cfg->detect_extn_cable) {
			/* High impedance device found. Report as LINEOUT */
			wcd_mbhc_report_plug(mbhc, 1, SND_JACK_LINEOUT);
			pr_debug("%s: setup mic trigger for further detection\n",
				 __func__);

			/* Disable HW FSM and current source */
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
			mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
							MIC_BIAS_2,
							MICB_PULLUP_DISABLE);
			/* Setup for insertion detection */
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE,
						 1);
			/*
			 * Enable HPHL trigger and MIC Schmitt triggers
			 * and request for elec insertion interrupts
			 */
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC,
						 3);
			pr_info("[LGE MBHC] %s: WCD_MBHC_ELEC_HS_INS enable \n", __func__);
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
					     true);
		} else {
			wcd_mbhc_report_plug(mbhc, 1, SND_JACK_LINEOUT);
		}
	} else {
		WARN(1, "Unexpected current plug_type %d, plug_type %d\n",
		     mbhc->current_plug, plug_type);
	}
exit:
	pr_debug("%s: leave\n", __func__);
}

/* To determine if cross connection occured */
static int wcd_check_cross_conn(struct wcd_mbhc *mbhc)
{
	u16 swap_res;
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_NONE;
	s16 reg1;
	bool hphl_sch_res, hphr_sch_res;

	if (wcd_swch_level_remove(mbhc)) {
		pr_info("[LGE MBHC] %s: Switch level is low\n", __func__);
		return -EINVAL;
	}

	/* If PA is enabled, dont check for cross-connection */
	if (mbhc->mbhc_cb->hph_pa_on_status)
		if (mbhc->mbhc_cb->hph_pa_on_status(mbhc->codec))
			return false;

	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_SCHMT_ISRC, reg1);
	/*
	 * Check if there is any cross connection,
	 * Micbias and schmitt trigger (HPHL-HPHR)
	 * needs to be enabled. For some codecs like wcd9335,
	 * pull-up will already be enabled when this function
	 * is called for cross-connection identification. No
	 * need to enable micbias in that case.
	 */
	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 2);

	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_RESULT, swap_res);
	pr_debug("%s: swap_res%x\n", __func__, swap_res);

	/*
	 * Read reg hphl and hphr schmitt result with cross connection
	 * bit. These bits will both be "0" in case of cross connection
	 * otherwise, they stay at 1
	 */
	WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch_res);
	WCD_MBHC_REG_READ(WCD_MBHC_HPHR_SCHMT_RESULT, hphr_sch_res);
	if (!(hphl_sch_res || hphr_sch_res)) {
		plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		pr_debug("%s: Cross connection identified\n", __func__);
	} else {
		pr_debug("%s: No Cross connection found\n", __func__);
	}

	/* Disable schmitt trigger and restore micbias */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, reg1);
	pr_debug("%s: leave, plug type: %d\n", __func__,  plug_type);

	return (plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP) ? true : false;
}

static bool wcd_is_special_headset(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	int delay = 0, rc;
	bool ret = false;
	u16 hs_comp_res;
	bool is_spl_hs = false;

	/*
	 * Increase micbias to 2.7V to detect headsets with
	 * threshold on microphone
	 */
	if (mbhc->mbhc_cb->mbhc_micbias_control &&
	    !mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic) {
		pr_debug("%s: callback fn micb_ctrl_thr_mic not defined\n",
			 __func__);
		return false;
	} else if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic) {
		rc = mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(codec,
							MIC_BIAS_2, true);
		if (rc) {
			pr_err("%s: Micbias control for thr mic failed, rc: %d\n",
				__func__, rc);
			return false;
		}
	}

	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);

	pr_debug("%s: special headset, start register writes\n", __func__);

	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);
	while (!is_spl_hs)  {
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			break;
		}
		delay = delay + 50;
		if (mbhc->mbhc_cb->mbhc_common_micb_ctrl) {
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_PRECHARGE,
					true);
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_SET_VAL,
					true);
		}
		/* Wait for 50msec for MICBIAS to settle down */
		msleep(50);
		if (mbhc->mbhc_cb->set_auto_zeroing)
			mbhc->mbhc_cb->set_auto_zeroing(codec, true);
		/* Wait for 50msec for FSM to update result values */
		msleep(50);
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);
		if (!(hs_comp_res)) {
			pr_debug("%s: Special headset detected in %d msecs\n",
					__func__, (delay * 2));
			is_spl_hs = true;
		}
		if (delay == SPECIAL_HS_DETECT_TIME_MS) {
			pr_debug("%s: Spl headset didnt get detect in 4 sec\n",
					__func__);
			break;
		}
	}
	if (is_spl_hs) {
		pr_debug("%s: Headset with threshold found\n",  __func__);
		mbhc->micbias_enable = true;
		ret = true;
	}
	if (mbhc->mbhc_cb->mbhc_common_micb_ctrl)
		mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
				MBHC_COMMON_MICB_PRECHARGE,
				false);
	if (mbhc->mbhc_cb->set_micbias_value && !mbhc->micbias_enable)
		mbhc->mbhc_cb->set_micbias_value(codec);
	if (mbhc->mbhc_cb->set_auto_zeroing)
		mbhc->mbhc_cb->set_auto_zeroing(codec, false);

	if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
	    !mbhc->micbias_enable)
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(codec, MIC_BIAS_2,
						      false);

	pr_debug("%s: leave, micb_enable: %d\n", __func__,
		  mbhc->micbias_enable);
	return ret;
}

static void wcd_mbhc_update_fsm_source(struct wcd_mbhc *mbhc,
				       enum wcd_mbhc_plug_type plug_type)
{
	switch (plug_type) {
	case MBHC_PLUG_TYPE_HEADPHONE:
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
	case MBHC_PLUG_TYPE_HIGH_HPH:
#endif
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 3);
		break;
	case MBHC_PLUG_TYPE_HEADSET:
	case MBHC_PLUG_TYPE_ANC_HEADPHONE:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
		if(extn_cable) {
			pr_info("[LGE MBHC] %s: micbias_enable is true, so maintain micbais enable.\n", __func__);
			break;
		}
#endif
		mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec, MIC_BIAS_2,
						MICB_PULLUP_ENABLE);
		break;
	default:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
		break;

	};
}

static void wcd_enable_mbhc_supply(struct wcd_mbhc *mbhc,
			enum wcd_mbhc_plug_type plug_type)
{

	struct snd_soc_codec *codec = mbhc->codec;

	/*
	 * Do not disable micbias if recording is going on or
	 * headset is inserted on the other side of the extn
	 * cable. If headset has been detected current source
	 * needs to be kept enabled for button detection to work.
	 * If the accessory type is invalid or unsupported, we
	 * dont need to enable either of them.
	 */
	if (mbhc->mbhc_cfg->detect_extn_cable && mbhc->is_extn_cable &&
		mbhc->mbhc_cb && mbhc->mbhc_cb->extn_use_mb &&
		mbhc->mbhc_cb->extn_use_mb(codec)) {
		if (plug_type == MBHC_PLUG_TYPE_HEADPHONE ||
		    plug_type == MBHC_PLUG_TYPE_HEADSET)
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
	} else {
		if (plug_type == MBHC_PLUG_TYPE_HEADSET) {
			if (mbhc->is_hs_recording || mbhc->micbias_enable)
				wcd_enable_curr_micbias(mbhc,
							WCD_MBHC_EN_MB);
			else if ((test_bit(WCD_MBHC_EVENT_PA_HPHL,
				&mbhc->event_state)) ||
				(test_bit(WCD_MBHC_EVENT_PA_HPHR,
				&mbhc->event_state)))
					wcd_enable_curr_micbias(mbhc,
							WCD_MBHC_EN_PULLUP);
			else
				wcd_enable_curr_micbias(mbhc,
							WCD_MBHC_EN_CS);
		} else if (plug_type == MBHC_PLUG_TYPE_HEADPHONE) {
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_CS);
		} else {
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_NONE);
		}
	}
}

static bool wcd_mbhc_check_for_spl_headset(struct wcd_mbhc *mbhc,
					   int *spl_hs_cnt)
{
	u16 hs_comp_res_1_8v = 0, hs_comp_res_2_7v = 0;
	bool spl_hs = false;

	if (!mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
		goto exit;

	/* Read back hs_comp_res @ 1.8v Micbias */
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res_1_8v);
	if (!hs_comp_res_1_8v) {
		spl_hs = false;
		goto exit;
	}

	/* Bump up MB2 to 2.7v */
	mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
				mbhc->mbhc_cfg->mbhc_micbias, true);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	usleep_range(10000, 10100);

	/* Read back HS_COMP_RESULT */
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res_2_7v);
	if (!hs_comp_res_2_7v && hs_comp_res_1_8v)
		spl_hs = true;

	if (spl_hs && spl_hs_cnt)
		*spl_hs_cnt += 1;

	/* MB2 back to 1.8v */
	if (spl_hs_cnt && (*spl_hs_cnt != WCD_MBHC_SPL_HS_CNT)) {
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
				mbhc->mbhc_cfg->mbhc_micbias, false);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
		usleep_range(10000, 10100);
	}

	if (spl_hs)
		pr_debug("%s: Detected special HS (%d)\n", __func__, spl_hs);

exit:
	return spl_hs;
}

static void wcd_correct_swch_plug(struct work_struct *work)
{
	struct wcd_mbhc *mbhc;
	struct snd_soc_codec *codec;
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_INVALID;
	unsigned long timeout;
	u16 hs_comp_res, hphl_sch, mic_sch, btn_result;
	bool wrk_complete = false;
	int pt_gnd_mic_swap_cnt = 0;
	int no_gnd_mic_swap_cnt = 0;
	bool is_pa_on = false, spl_hs = false;
	bool micbias2 = false;
	bool micbias1 = false;
	int ret = 0;
	int rc, spl_hs_count = 0;
	int cross_conn;
	int try = 0;

	pr_debug("%s: enter\n", __func__);
#if defined(CONFIG_SND_SOC_ES9218P) && defined(SKIP_RECALC_IMPED)
    skip_recalc_imped = false;
    pr_info("[LGE MBHC] 0 skip_recalc_imped, init. skip_recal_imped = false \n");
#endif

	mbhc = container_of(work, struct wcd_mbhc, correct_plug_swch);
	codec = mbhc->codec;

	/*
	 * Enable micbias/pullup for detection in correct work.
	 * This work will get scheduled from detect_plug_type which
	 * will already request for pullup/micbias. If the pullup/micbias
	 * is handled with ref-counts by individual codec drivers, there is
	 * no need to enabale micbias/pullup here
	 */

	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);


	/* Enable HW FSM */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	/*
	 * Check for any button press interrupts before starting 3-sec
	 * loop.
	 */
	rc = wait_for_completion_timeout(&mbhc->btn_press_compl,
			msecs_to_jiffies(WCD_MBHC_BTN_PRESS_COMPL_TIMEOUT_MS));

	WCD_MBHC_REG_READ(WCD_MBHC_BTN_RESULT, btn_result);
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);

	if (!rc) {
		pr_debug("%s No btn press interrupt\n", __func__);
		if (!btn_result && !hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HEADSET;
		else if (!btn_result && hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
		else
			plug_type = MBHC_PLUG_TYPE_INVALID;
	} else {
		if (!btn_result && !hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HEADPHONE;
		else if (btn_result && !hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HEADSET;
		else
			plug_type = MBHC_PLUG_TYPE_INVALID;
	}
	pr_info("[LGE MBHC] %s: Valid plug found, plug type is %d\n",
			 __func__, plug_type);

	do {
		cross_conn = wcd_check_cross_conn(mbhc);
		try++;
	} while (try < GND_MIC_SWAP_THRESHOLD);
	/*
	 * check for cross coneection 4 times.
	 * conisder the result of the fourth iteration.
	 */
	if (cross_conn > 0) {
		pr_debug("%s: cross con found, start polling\n",
			 __func__);
		plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		pr_debug("%s: Plug found, plug type is %d\n",
			 __func__, plug_type);
		goto correct_plug_type;
	}

#ifdef CONFIG_MACH_LGE
	if (mbhc->mbhc_cfg->detect_extn_cable == false &&
		plug_type == MBHC_PLUG_TYPE_HIGH_HPH) {
			mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
							MIC_BIAS_2, true);
			plug_type = MBHC_PLUG_TYPE_HEADSET;
			pr_info("[LGE MBHC] %s: HIGH_HPH is detected. Raise MIC Bias2 to 2.7v and report 4-pin headset.\n", __func__);
		WCD_MBHC_RSC_LOCK(mbhc);
		wcd_mbhc_find_plug_and_report(mbhc, plug_type);
		WCD_MBHC_RSC_UNLOCK(mbhc);
		goto correct_plug_type;
	} else if (plug_type == MBHC_PLUG_TYPE_HIGH_HPH && mbhc->mbhc_cfg->detect_extn_cable){
		pr_info("[LGE MBHC] %s: HIGH_HPH is detected. Raise MIC Bias2 to 2.7v and checking impedance .\n", __func__);
			mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,MIC_BIAS_2, true);
		if (mbhc->impedance_detect && mbhc->mbhc_cb->compute_impedance && (mbhc->mbhc_cfg->linein_th != 0)) {
			mbhc->mbhc_cb->compute_impedance(mbhc, &mbhc->zl, &mbhc->zr);
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
			extn_cable = true;
#endif
			if ((mbhc->zl < LGE_ADVANCED_HEADSET_THRESHOLD) || (mbhc->zr < LGE_ADVANCED_HEADSET_THRESHOLD)) {
				pr_info("[LGE MBHC] %s: High impedance found. Force 4pin Headset. plug_type:0x%x, zl=%d , zr=%d\n",
		                __func__, plug_type,mbhc->zl,mbhc->zr);
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
				mbhc->micbias_enable = true;
#endif
				plug_type = MBHC_PLUG_TYPE_HEADSET;
				WCD_MBHC_RSC_LOCK(mbhc);
				wcd_mbhc_find_plug_and_report(mbhc, plug_type);
				WCD_MBHC_RSC_UNLOCK(mbhc);
				goto correct_plug_type;
			}
		}
	}
#endif
	if ((plug_type == MBHC_PLUG_TYPE_HEADSET ||
	     plug_type == MBHC_PLUG_TYPE_HEADPHONE) &&
	    (!wcd_swch_level_remove(mbhc))) {
		WCD_MBHC_RSC_LOCK(mbhc);
		wcd_mbhc_find_plug_and_report(mbhc, plug_type);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}

#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
	if(extn_cable && (plug_type == MBHC_PLUG_TYPE_HEADSET) && !mbhc->micbias_enable) {
		pr_info("[LGE MBHC] %s: Gender + Headset(4 pole). Enable micbias to 2.7 \n", __func__);
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,MIC_BIAS_2, true);
		mbhc->micbias_enable = true;
	}
#endif

correct_plug_type:
#ifdef HDSET_FACTORY
	pr_info("%s lge_get_boot_mode:%d\n",__func__, lge_get_boot_mode());
	if((lge_get_boot_mode() == LGE_BOOT_MODE_QEM_56K) ||
		(lge_get_boot_mode() == LGE_BOOT_MODE_QEM_910K))
		 goto report;
#endif
	timeout = jiffies + msecs_to_jiffies(HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			wcd_enable_curr_micbias(mbhc,
						WCD_MBHC_EN_NONE);
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
				mbhc->micbias_enable && !extn_cable) {
#else
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
				mbhc->micbias_enable) {
#endif
				mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
					mbhc->codec, MIC_BIAS_2, false);
				if (mbhc->mbhc_cb->set_micbias_value)
					mbhc->mbhc_cb->set_micbias_value(
							mbhc->codec);
				mbhc->micbias_enable = false;
			}
			goto exit;
		}
		if (mbhc->btn_press_intr) {
			wcd_cancel_btn_work(mbhc);
			mbhc->btn_press_intr = false;
		}
		/* Toggle FSM */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);

		/* allow sometime and re-check stop requested again */
		msleep(20);
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			wcd_enable_curr_micbias(mbhc,
						WCD_MBHC_EN_NONE);
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
				mbhc->micbias_enable && !extn_cable) {
#else
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
				mbhc->micbias_enable) {
#endif
				mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
					mbhc->codec, MIC_BIAS_2, false);
				if (mbhc->mbhc_cb->set_micbias_value)
					mbhc->mbhc_cb->set_micbias_value(
							mbhc->codec);
				mbhc->micbias_enable = false;
			}
			goto exit;
		}
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);

		pr_debug("%s: hs_comp_res: %x\n", __func__, hs_comp_res);
		if (mbhc->mbhc_cb->hph_pa_on_status)
			is_pa_on = mbhc->mbhc_cb->hph_pa_on_status(codec);

		/*
		 * instead of hogging system by contineous polling, wait for
		 * sometime and re-check stop request again.
		 */
		msleep(180);
		if (hs_comp_res && (spl_hs_count < WCD_MBHC_SPL_HS_CNT)) {
			spl_hs = wcd_mbhc_check_for_spl_headset(mbhc,
								&spl_hs_count);

			if (spl_hs_count == WCD_MBHC_SPL_HS_CNT) {
				hs_comp_res = 0;
				spl_hs = true;
				mbhc->micbias_enable = true;
			}
		}

		if ((!hs_comp_res) && (!is_pa_on)) {
			/* Check for cross connection*/
			ret = wcd_check_cross_conn(mbhc);
			if (ret < 0) {
				continue;
			} else if (ret > 0) {
				pt_gnd_mic_swap_cnt++;
				no_gnd_mic_swap_cnt = 0;
				if (pt_gnd_mic_swap_cnt <
						GND_MIC_SWAP_THRESHOLD) {
					continue;
				} else if (pt_gnd_mic_swap_cnt >
						GND_MIC_SWAP_THRESHOLD) {
					/*
					 * This is due to GND/MIC switch didn't
					 * work,  Report unsupported plug.
					 */
					pr_debug("%s: switch didnt work\n",
						  __func__);
					plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
					goto report;
				} else {
					plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
				}
			} else {
				no_gnd_mic_swap_cnt++;
				pt_gnd_mic_swap_cnt = 0;
				plug_type = MBHC_PLUG_TYPE_HEADSET;
				if ((no_gnd_mic_swap_cnt <
				    GND_MIC_SWAP_THRESHOLD) &&
				    (spl_hs_count != WCD_MBHC_SPL_HS_CNT)) {
					continue;
				} else {
					no_gnd_mic_swap_cnt = 0;
				}
			}
			if ((pt_gnd_mic_swap_cnt == GND_MIC_SWAP_THRESHOLD) &&
				(plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP)) {
				/*
				 * if switch is toggled, check again,
				 * otherwise report unsupported plug
				 */
				if (mbhc->mbhc_cfg->swap_gnd_mic &&
					mbhc->mbhc_cfg->swap_gnd_mic(codec)) {
					pr_debug("%s: US_EU gpio present,flip switch\n"
						, __func__);
					continue;
				}
			}
		}

		WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch);
		WCD_MBHC_REG_READ(WCD_MBHC_MIC_SCHMT_RESULT, mic_sch);
		if (hs_comp_res && !(hphl_sch || mic_sch)) {
#ifdef CONFIG_MACH_LGE
			if (mbhc->mbhc_cfg->detect_extn_cable == false) {
				if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET) {
					pr_debug("[LGE MBHC] %s: cable is HIGH_HPH in correct-loop. Report 4-pin Headset.\n", __func__);
					if ((snd_soc_read(mbhc->codec, 0x0623) & 0x3f) != 0x22) {
						pr_info("[LGE MBHC] %s: Raise mic bias to 2.7v in correct-loop\n", __func__);
						mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
										MIC_BIAS_2, true);
					}
					plug_type = MBHC_PLUG_TYPE_HEADSET;
					if (mbhc->is_hs_recording == true)
						goto enable_supply;
					else
						wrk_complete = false;
				} else {
					pr_info("[LGE MBHC] %s: 3-pin is detected first and then changed to HIGH_HPH. Doing Nothing.\n", __func__);
				}
			} else {
				if (mbhc->impedance_detect && mbhc->mbhc_cb->compute_impedance && (mbhc->mbhc_cfg->linein_th != 0)) {
					pr_debug("[LGE MBHC] %s: cable is HIGH_HPH in correct-loop. checking Imp.\n", __func__);
					if ((snd_soc_read(mbhc->codec, 0x0623) & 0x3f) != 0x22) {
						pr_info("[LGE MBHC] %s: Raise mic bias to 2.7v in correct-loop\n", __func__);
						mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
										MIC_BIAS_2, true);
					}
					if ((mbhc->zl < LGE_ADVANCED_HEADSET_THRESHOLD) || (mbhc->zr < LGE_ADVANCED_HEADSET_THRESHOLD)) {
						pr_info("[LGE MBHC] %s: High Imp. Headset found. Force 4pin Headset. plug_type:0x%x, zl=%d , zr=%d\n",
									__func__, plug_type,mbhc->zl,mbhc->zr);
						plug_type = MBHC_PLUG_TYPE_HEADSET;
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
						mbhc->micbias_enable = true;
#endif
						wrk_complete = true;
					} else {
						pr_info("[LGE MBHC] %s: cable is extension cable found. line=%d, plug_type:0x%x, zl=%d(0x%x), zr=%d(0x%x)\n",
								__func__, __LINE__, plug_type, mbhc->zl, mbhc->zl, mbhc->zr, mbhc->zr);
						plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
						wrk_complete = true;
					}
				} else {
					pr_info("[LGE MBHC] %s: cable is extension cable found. line=%d, plug_type:0x%x, \n",
							__func__, __LINE__, plug_type);
					plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
					wrk_complete = true;
				}
			}
#else	/* Qualcomm Orig. */
			pr_debug("%s: cable is extension cable\n", __func__);
			plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
			wrk_complete = true;
#endif
		} else {
			pr_debug("%s: cable might be headset: %d\n", __func__,
					plug_type);
			if (!(plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP)) {
				plug_type = MBHC_PLUG_TYPE_HEADSET;
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
				if(extn_cable && !mbhc->micbias_enable) {
					pr_info("[LGE MBHC] %s: Gender + Headset(4 pole). Enable micbias to 2.7 later. \n", __func__);
					mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,MIC_BIAS_2, true);
					mbhc->micbias_enable = true;
				}
#endif
				/*
				 * Report headset only if not already reported
				 * and if there is not button press without
				 * release
				 */
				if (((mbhc->current_plug !=
				      MBHC_PLUG_TYPE_HEADSET) &&
				     (mbhc->current_plug !=
				      MBHC_PLUG_TYPE_ANC_HEADPHONE)) &&
				    !wcd_swch_level_remove(mbhc) &&
				    !mbhc->btn_press_intr) {
					pr_info("[LGE MBHC] %s: cable is %sheadset\n",
						__func__,
						((spl_hs_count ==
							WCD_MBHC_SPL_HS_CNT) ?
							"special ":""));
					goto report;
				}
			}
			wrk_complete = false;
		}
	}
	if (!wrk_complete && mbhc->btn_press_intr) {
		if (wcd_mbhc_get_button_mask(mbhc) & SND_JACK_BTN_0) {
			pr_info("[LGE MBHC] %s: Can be slow insertion of headphone\n",
				 __func__);
			wcd_cancel_btn_work(mbhc);
			plug_type = MBHC_PLUG_TYPE_HEADPHONE;
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
			if(extn_cable) {
				pr_info("[LGE MBHC] %s: Gender + Headphone(3 pole) later.\n", __func__);
				mbhc->micbias_enable = false;
			}
#endif
#if defined(CONFIG_SND_SOC_ES9218P) && defined(SKIP_RECALC_IMPED)
            if (enable_es9218p) {
                skip_recalc_imped = true;
                pr_info("[LGE MBHC] 1 skip_recalc_imped, skip_recalc_imped = true \n");
            }
#endif
		} else {
			pr_info("[LGE MBHC] %s: non-zero button pressed during detection\n",
				 __func__);
			if (mbhc->current_plug != MBHC_PLUG_TYPE_HEADSET) {
				plug_type = MBHC_PLUG_TYPE_HEADSET;
				goto report;
			}
		}
	}
	/*
	 * If plug_tye is headset, we might have already reported either in
	 * detect_plug-type or in above while loop, no need to report again
	 */
	if (!wrk_complete && ((plug_type == MBHC_PLUG_TYPE_HEADSET) ||
	    (plug_type == MBHC_PLUG_TYPE_ANC_HEADPHONE))) {
		pr_debug("%s: plug_type:0x%x already reported\n",
			 __func__, mbhc->current_plug);
		goto enable_supply;
	}

	if (plug_type == MBHC_PLUG_TYPE_HIGH_HPH &&
		(!mbhc->mbhc_cfg->detect_extn_cable)) {
		if (wcd_is_special_headset(mbhc)) {
			pr_debug("%s: Special headset found %d\n",
					__func__, plug_type);
			plug_type = MBHC_PLUG_TYPE_HEADSET;
			goto report;
		}
	}

report:
	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low\n", __func__);
		goto exit;
	}
	if (plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP && mbhc->btn_press_intr) {
		pr_debug("%s: insertion of headphone with swap\n", __func__);
		wcd_cancel_btn_work(mbhc);
		plug_type = MBHC_PLUG_TYPE_HEADPHONE;
	}
	pr_debug("%s: Valid plug found, plug type %d wrk_cmpt %d btn_intr %d\n",
			__func__, plug_type, wrk_complete,
			mbhc->btn_press_intr);
	WCD_MBHC_RSC_LOCK(mbhc);
	wcd_mbhc_find_plug_and_report(mbhc, plug_type);
	WCD_MBHC_RSC_UNLOCK(mbhc);
enable_supply:
	if (mbhc->mbhc_cb->mbhc_micbias_control)
		wcd_mbhc_update_fsm_source(mbhc, plug_type);
	else
		wcd_enable_mbhc_supply(mbhc, plug_type);
exit:
	if (mbhc->mbhc_cb->mbhc_micbias_control &&
	    !mbhc->micbias_enable)
		mbhc->mbhc_cb->mbhc_micbias_control(codec, MIC_BIAS_2,
						    MICB_DISABLE);
	if (mbhc->mbhc_cb->micbias_enable_status) {
		micbias1 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_1);
		micbias2 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_2);
	}

	if (mbhc->mbhc_cfg->detect_extn_cable &&
	    ((plug_type == MBHC_PLUG_TYPE_HEADPHONE) ||
	     (plug_type == MBHC_PLUG_TYPE_HEADSET)) && !hs_comp_res &&
	    !((mbhc->zl > 1000) || (mbhc->zr > 1000)) &&
	    !mbhc->hs_detect_work_stop) {
		WCD_MBHC_RSC_LOCK(mbhc);
		pr_info("[LGE MBHC] %s: WCD_MBHC_ELEC_HS_REM enable \n", __func__);
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM, true);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}
	if (mbhc->mbhc_cb->set_cap_mode)
		mbhc->mbhc_cb->set_cap_mode(codec, micbias1, micbias2);

	if (mbhc->mbhc_cb->hph_pull_down_ctrl)
		mbhc->mbhc_cb->hph_pull_down_ctrl(codec, true);

	mbhc->mbhc_cb->lock_sleep(mbhc, false);
	pr_info("[LGE MBHC] %s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void wcd_mbhc_detect_plug_type(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	bool micbias1 = false;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (mbhc->mbhc_cb->hph_pull_down_ctrl)
		mbhc->mbhc_cb->hph_pull_down_ctrl(codec, false);

	if (mbhc->mbhc_cb->micbias_enable_status)
		micbias1 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_1);

	if (mbhc->mbhc_cb->set_cap_mode)
		mbhc->mbhc_cb->set_cap_mode(codec, micbias1, true);

	if (mbhc->mbhc_cb->mbhc_micbias_control)
		mbhc->mbhc_cb->mbhc_micbias_control(codec, MIC_BIAS_2,
						    MICB_ENABLE);
	else
		wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);

	/* Re-initialize button press completion object */
	reinit_completion(&mbhc->btn_press_compl);
	wcd_schedule_hs_detect_plug(mbhc, &mbhc->correct_plug_swch);
	pr_debug("%s: leave\n", __func__);
}

static void wcd_mbhc_swch_irq_handler(struct wcd_mbhc *mbhc)
{
	bool detection_type;
	bool micbias1 = false;
	struct snd_soc_codec *codec = mbhc->codec;

	dev_dbg(codec->dev, "%s: enter\n", __func__);

	WCD_MBHC_RSC_LOCK(mbhc);

	mbhc->in_swch_irq_handler = true;

	/* cancel pending button press */
	if (wcd_cancel_btn_work(mbhc))
		pr_debug("%s: button press is canceled\n", __func__);

	WCD_MBHC_REG_READ(WCD_MBHC_MECH_DETECTION_TYPE, detection_type);

	/* Set the detection type appropriately */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MECH_DETECTION_TYPE,
				 !detection_type);

	pr_debug("%s: mbhc->current_plug: %d detection_type: %d\n", __func__,
			mbhc->current_plug, detection_type);
	wcd_cancel_hs_detect_plug(mbhc, &mbhc->correct_plug_swch);

	if (mbhc->mbhc_cb->micbias_enable_status)
		micbias1 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
						MIC_BIAS_1);

	if ((mbhc->current_plug == MBHC_PLUG_TYPE_NONE) &&
	    detection_type) {
		/* Make sure MASTER_BIAS_CTL is enabled */
		mbhc->mbhc_cb->mbhc_bias(codec, true);

		if (mbhc->mbhc_cb->mbhc_common_micb_ctrl)
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_TAIL_CURR, true);

		if (!mbhc->mbhc_cfg->hs_ext_micbias &&
		     mbhc->mbhc_cb->micb_internal)
			/*
			 * Enable Tx2 RBias if the headset
			 * is using internal micbias
			 */
			mbhc->mbhc_cb->micb_internal(codec, 1, true);

		/* Remove micbias pulldown */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_PULLDOWN_CTRL, 0);
		/* Apply trim if needed on the device */
		if (mbhc->mbhc_cb->trim_btn_reg)
			mbhc->mbhc_cb->trim_btn_reg(codec);
		/* Enable external voltage source to micbias if present */
		if (mbhc->mbhc_cb->enable_mb_source)
			mbhc->mbhc_cb->enable_mb_source(codec, true);
		mbhc->btn_press_intr = false;
		mbhc->is_btn_press = false;
		wcd_mbhc_detect_plug_type(mbhc);
	} else if ((mbhc->current_plug != MBHC_PLUG_TYPE_NONE)
			&& !detection_type) {
		/* Disable external voltage source to micbias if present */
		if (mbhc->mbhc_cb->enable_mb_source)
			mbhc->mbhc_cb->enable_mb_source(codec, false);
		/* Disable HW FSM */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
		extn_cable = false;
#endif
		mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec, MIC_BIAS_2,
						MICB_PULLUP_DISABLE);
		if (mbhc->mbhc_cb->mbhc_common_micb_ctrl)
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_TAIL_CURR, false);

		if (mbhc->mbhc_cb->set_cap_mode)
			mbhc->mbhc_cb->set_cap_mode(codec, micbias1, false);

		mbhc->btn_press_intr = false;
		mbhc->is_btn_press = false;
		if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADPHONE) {
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM,
					     false);
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
					     false);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE,
						 1);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADPHONE);
		} else if (mbhc->current_plug == MBHC_PLUG_TYPE_GND_MIC_SWAP) {
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_UNSUPPORTED);
		} else if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET) {
			/* make sure to turn off Rbias */
			if (mbhc->mbhc_cb->micb_internal)
				mbhc->mbhc_cb->micb_internal(codec, 1, false);

			/* Pulldown micbias */
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_PULLDOWN_CTRL, 1);
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM,
					     false);
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
					     false);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE,
						 1);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADSET);
		} else if (mbhc->current_plug == MBHC_PLUG_TYPE_HIGH_HPH) {
			mbhc->is_extn_cable = false;
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM,
					     false);
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
					     false);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE,
						 1);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_LINEOUT);
		} else if (mbhc->current_plug == MBHC_PLUG_TYPE_ANC_HEADPHONE) {
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM, false);
			wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS, false);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE,
						 0);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_ANC_HEADPHONE);
		}
	} else if (!detection_type) {
		/* Disable external voltage source to micbias if present */
		if (mbhc->mbhc_cb->enable_mb_source)
			mbhc->mbhc_cb->enable_mb_source(codec, false);
		/* Disable HW FSM */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS, false);
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM, false);

	}

	mbhc->in_swch_irq_handler = false;
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t wcd_mbhc_mech_plug_detect_irq(int irq, void *data)
{
	int r = IRQ_HANDLED;
	struct wcd_mbhc *mbhc = data;

	pr_info("[LGE MBHC] %s: enter\n", __func__);
	if (unlikely((mbhc->mbhc_cb->lock_sleep(mbhc, true)) == false)) {
		pr_warn("%s: failed to hold suspend\n", __func__);
		r = IRQ_NONE;
	} else {
		/* Call handler */
#if defined(CONFIG_SND_SOC_ES9018)|| defined(CONFIG_SND_SOC_ES9218P)
// Temp for ES9218 RevA
		if (enable_es9218p)
        		es9218_sabre_headphone_on();
#endif
		wcd_mbhc_swch_irq_handler(mbhc);
		mbhc->mbhc_cb->lock_sleep(mbhc, false);
	}
	pr_info("[LGE MBHC] %s: leave %d\n", __func__, r);
	return r;
}

static int wcd_mbhc_get_button_mask(struct wcd_mbhc *mbhc)
{
	int mask = 0;
	int btn;

	btn = mbhc->mbhc_cb->map_btn_code_to_num(mbhc->codec);

	switch (btn) {
	case 0:
		mask = SND_JACK_BTN_0;
		break;
	case 1:
		mask = SND_JACK_BTN_1;
		break;
	case 2:
		mask = SND_JACK_BTN_2;
		break;
	case 3:
		mask = SND_JACK_BTN_3;
		break;
	case 4:
		mask = SND_JACK_BTN_4;
		break;
	case 5:
		mask = SND_JACK_BTN_5;
		break;
	case 6:
		mask = SND_JACK_BTN_6;
		break;
	case 7:
		mask = SND_JACK_BTN_7;
		break;
	default:
		break;
	}

	return mask;
}

static irqreturn_t wcd_mbhc_hs_ins_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	bool detection_type, hphl_sch, mic_sch;
	u16 elect_result;
	static u16 hphl_trigerred;
	static u16 mic_trigerred;

	pr_info("[LGE MBHC] %s: enter\n", __func__);
	pr_debug("%s: enter\n", __func__);
	if (!mbhc->mbhc_cfg->detect_extn_cable) {
		pr_debug("%s: Returning as Extension cable feature not enabled\n",
			__func__);
		return IRQ_HANDLED;
	}
	WCD_MBHC_RSC_LOCK(mbhc);

	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_DETECTION_TYPE, detection_type);
	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_RESULT, elect_result);

	pr_debug("%s: detection_type %d, elect_result %x\n", __func__,
				detection_type, elect_result);
	if (detection_type) {
		/* check if both Left and MIC Schmitt triggers are triggered */
		WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch);
		WCD_MBHC_REG_READ(WCD_MBHC_MIC_SCHMT_RESULT, mic_sch);
		if (hphl_sch && mic_sch) {
			/* Go for plug type determination */
			pr_debug("%s: Go for plug type determination\n",
				  __func__);
			goto determine_plug;

		} else {
			if (mic_sch) {
				mic_trigerred++;
				pr_debug("%s: Insertion MIC trigerred %d\n",
					 __func__, mic_trigerred);
				WCD_MBHC_REG_UPDATE_BITS(
						WCD_MBHC_ELECT_SCHMT_ISRC,
						0);
				msleep(20);
				WCD_MBHC_REG_UPDATE_BITS(
						WCD_MBHC_ELECT_SCHMT_ISRC,
						1);
			}
			if (hphl_sch) {
				hphl_trigerred++;
				pr_debug("%s: Insertion HPHL trigerred %d\n",
					 __func__, hphl_trigerred);
			}
			if (mic_trigerred && hphl_trigerred) {
				/* Go for plug type determination */
				pr_debug("%s: Go for plug type determination\n",
					 __func__);
				goto determine_plug;
			}
		}
	}
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_info("[LGE MBHC] %s: leave\n", __func__);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;

determine_plug:
	/*
	 * Disable HPHL trigger and MIC Schmitt triggers.
	 * Setup for insertion detection.
	 */
	pr_debug("%s: Disable insertion interrupt\n", __func__);
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
			     false);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
	hphl_trigerred = 0;
	mic_trigerred = 0;
	mbhc->is_extn_cable = true;
	mbhc->btn_press_intr = false;
	mbhc->is_btn_press = false;
#ifdef CONFIG_MACH_LGE
        msleep(500);
#endif
	wcd_mbhc_detect_plug_type(mbhc);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_info("[LGE MBHC] %s: determine_plug leave\n", __func__);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_hs_rem_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	u8 hs_comp_result, hphl_sch, mic_sch;
	static u16 hphl_trigerred;
	static u16 mic_trigerred;
	unsigned long timeout;
	bool removed = true;
	int retry = 0;

	pr_info("[LGE MBHC] %s: enter\n", __func__);
	pr_debug("%s: enter\n", __func__);

	WCD_MBHC_RSC_LOCK(mbhc);

	timeout = jiffies +
		  msecs_to_jiffies(WCD_FAKE_REMOVAL_MIN_PERIOD_MS);
	do {
		retry++;
		/*
		 * read the result register every 10ms to look for
		 * any change in HS_COMP_RESULT bit
		 */
		usleep_range(10000, 10100);
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_result);
		pr_debug("%s: Check result reg for fake removal: hs_comp_res %x\n",
			 __func__, hs_comp_result);
		if ((!hs_comp_result) &&
		    retry > FAKE_REM_RETRY_ATTEMPTS) {
			removed = false;
			break;
		}
	} while (!time_after(jiffies, timeout));

	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low ", __func__);
		goto exit;
	}
	pr_debug("%s: headset %s actually removed\n", __func__,
		removed ? "" : "not ");

	WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch);
	WCD_MBHC_REG_READ(WCD_MBHC_MIC_SCHMT_RESULT, mic_sch);
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_result);

	if (removed) {
		if (!(hphl_sch && mic_sch && hs_comp_result)) {
			/*
			 * extension cable is still plugged in
			 * report it as LINEOUT device
			 */
			goto report_unplug;
		} else {
			if (!mic_sch) {
				mic_trigerred++;
				pr_debug("%s: Removal MIC trigerred %d\n",
					 __func__, mic_trigerred);
			}
			if (!hphl_sch) {
				hphl_trigerred++;
				pr_debug("%s: Removal HPHL trigerred %d\n",
					 __func__, hphl_trigerred);
			}
			if (mic_trigerred && hphl_trigerred) {
				/*
				 * extension cable is still plugged in
				 * report it as LINEOUT device
				 */
				goto report_unplug;
			}
		}
	}
exit:
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_info("[LGE MBHC] %s: leave\n", __func__);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;

report_unplug:

	/* cancel pending button press */
	if (wcd_cancel_btn_work(mbhc))
		pr_debug("%s: button press is canceled\n", __func__);
	/* cancel correct work function */
	wcd_cancel_hs_detect_plug(mbhc, &mbhc->correct_plug_swch);

	pr_debug("%s: Report extension cable\n", __func__);
	wcd_mbhc_report_plug(mbhc, 1, SND_JACK_LINEOUT);
	/*
	 * If PA is enabled HPHL schmitt trigger can
	 * be unreliable, make sure to disable it
	 */
	if (test_bit(WCD_MBHC_EVENT_PA_HPHL,
		&mbhc->event_state))
		wcd_mbhc_set_and_turnoff_hph_padac(mbhc);
	/*
	 * Disable HPHL trigger and MIC Schmitt triggers.
	 * Setup for insertion detection.
	 */
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM,
			     false);
	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_NONE);
	/* Disable HW FSM */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 3);

	/* Set the detection type appropriately */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE, 1);
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
			     true);
	hphl_trigerred = 0;
	mic_trigerred = 0;
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
	if (mbhc->mbhc_cb->mbhc_micbias_control) {
		mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec, MIC_BIAS_2,
			MICB_DISABLE);
		mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec, MIC_BIAS_2,
			MICB_PULLUP_DISABLE);
	}
	mbhc->micbias_enable = false;
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 3);
#endif
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_info("[LGE MBHC] %s: report_unplug leave\n", __func__);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

static void wcd_btn_lpress_fn(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct wcd_mbhc *mbhc;
	s16 btn_result;

	pr_debug("%s: Enter\n", __func__);

	dwork = to_delayed_work(work);
	mbhc = container_of(dwork, struct wcd_mbhc, mbhc_btn_dwork);

	WCD_MBHC_REG_READ(WCD_MBHC_BTN_RESULT, btn_result);
	if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET) {
		pr_debug("%s: Reporting long button press event, btn_result: %d\n",
			 __func__, btn_result);
		pr_info("[LGE MBHC] Long Button : %s is pressed.\n",
					mbhc->buttons_pressed==0x800000?"Volume Down":
					button_name[(mbhc->buttons_pressed>>24)*-1/2+2]);
		wcd_mbhc_jack_report(mbhc, &mbhc->button_jack,
				mbhc->buttons_pressed, mbhc->buttons_pressed);
	}
	pr_debug("%s: leave\n", __func__);
	mbhc->mbhc_cb->lock_sleep(mbhc, false);
}

static bool wcd_mbhc_fw_validate(const void *data, size_t size)
{
	u32 cfg_offset;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	struct firmware_cal fw;

	fw.data = (void *)data;
	fw.size = size;

	if (fw.size < WCD_MBHC_CAL_MIN_SIZE)
		return false;

	/*
	 * Previous check guarantees that there is enough fw data up
	 * to num_btn
	 */
	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(fw.data);
	cfg_offset = (u32) ((void *) btn_cfg - (void *) fw.data);
	if (fw.size < (cfg_offset + WCD_MBHC_CAL_BTN_SZ(btn_cfg)))
		return false;

	return true;
}

static irqreturn_t wcd_mbhc_btn_press_handler(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	int mask;
	unsigned long msec_val;

	pr_debug("%s: enter\n", __func__);
	complete(&mbhc->btn_press_compl);
	WCD_MBHC_RSC_LOCK(mbhc);
	wcd_cancel_btn_work(mbhc);
	if (wcd_swch_level_remove(mbhc)) {
		pr_info("[LGE MBHC] %s: Switch level is low\n", __func__);
		goto done;
	}

	mbhc->is_btn_press = true;
	msec_val = jiffies_to_msecs(jiffies - mbhc->jiffies_atreport);
	pr_debug("%s: msec_val = %ld\n", __func__, msec_val);
	if (msec_val < MBHC_BUTTON_PRESS_THRESHOLD_MIN) {
		pr_info("[LGE MBHC] %s: Too short, ignore button press\n", __func__);
		goto done;
	}

	/* If switch interrupt already kicked in, ignore button press */
	if (mbhc->in_swch_irq_handler) {
		pr_info("[LGE MBHC] %s: Swtich level changed, ignore button press\n",
			 __func__);
		goto done;
	}
	mask = wcd_mbhc_get_button_mask(mbhc);
	if (mask == SND_JACK_BTN_0)
		mbhc->btn_press_intr = true;

#ifdef CONFIG_MACH_LGE
        pr_info("[LGE MBHC] %s: enter\n", __func__);
        if (!(mask & (SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2| SND_JACK_BTN_3))) {
                pr_info("[LGE MBHC] %s: Unsupported button(0x%x). Ignore button event.\n",
                         __func__, mask);
                goto done;
        }
#endif

	if (mbhc->current_plug != MBHC_PLUG_TYPE_HEADSET) {
		pr_debug("%s: Plug isn't headset, ignore button press\n",
				__func__);
		goto done;
	}
	mbhc->buttons_pressed |= mask;
	mbhc->mbhc_cb->lock_sleep(mbhc, true);
	if (schedule_delayed_work(&mbhc->mbhc_btn_dwork,
				msecs_to_jiffies(400)) == 0) {
		WARN(1, "Button pressed twice without release event\n");
		mbhc->mbhc_cb->lock_sleep(mbhc, false);
	}
done:
	pr_debug("%s: leave\n", __func__);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_release_handler(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	int ret;

	pr_info("[LGE MBHC] %s: enter\n", __func__);
	WCD_MBHC_RSC_LOCK(mbhc);
	if (wcd_swch_level_remove(mbhc)) {
		pr_info("[LGE MBHC] %s: Switch level is low\n", __func__);
		goto exit;
	}

	if (mbhc->is_btn_press) {
		mbhc->is_btn_press = false;
	} else {
		pr_debug("%s: This release is for fake btn press\n", __func__);
		goto exit;
	}

	/*
	 * If current plug is headphone then there is no chance to
	 * get btn release interrupt, so connected cable should be
	 * headset not headphone.
	 */
	if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADPHONE) {
		wcd_mbhc_find_plug_and_report(mbhc, MBHC_PLUG_TYPE_HEADSET);
		goto exit;

	}
	if (mbhc->buttons_pressed & WCD_MBHC_JACK_BUTTON_MASK) {
		ret = wcd_cancel_btn_work(mbhc);
		if (ret == 0) {
			pr_debug("%s: Reporting long button release event\n",
				 __func__);
			pr_info("[LGE MBHC] Long Button : %s is released.\n",
						mbhc->buttons_pressed==0x800000?"Volume Down":
						button_name[(mbhc->buttons_pressed>>24)*-1/2+2]);
			wcd_mbhc_jack_report(mbhc, &mbhc->button_jack,
					0, mbhc->buttons_pressed);
		} else {
			if (mbhc->in_swch_irq_handler) {
				pr_debug("%s: Switch irq kicked in, ignore\n",
					__func__);
			} else {
				pr_debug("%s: Reporting btn press\n",
					 __func__);
			pr_info("[LGE MBHC] Short Button : %s is pressed.\n",
						mbhc->buttons_pressed==0x800000?"Volume Down":
						button_name[(mbhc->buttons_pressed>>24)*-1/2+2]);
				wcd_mbhc_jack_report(mbhc,
						     &mbhc->button_jack,
						     mbhc->buttons_pressed,
						     mbhc->buttons_pressed);
				pr_debug("%s: Reporting btn release\n",
					 __func__);
				pr_info("[LGE MBHC] Short Button : %s is released.\n",
							mbhc->buttons_pressed==0x800000?"Volume Down":
							button_name[(mbhc->buttons_pressed>>24)*-1/2+2]);
				wcd_mbhc_jack_report(mbhc,
						&mbhc->button_jack,
						0, mbhc->buttons_pressed);
			}
		}
		mbhc->buttons_pressed &= ~WCD_MBHC_JACK_BUTTON_MASK;
	}
exit:
	pr_debug("%s: leave\n", __func__);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_hphl_ocp_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;

	pr_debug("%s: received HPHL OCP irq\n", __func__);
	if (mbhc) {
		if ((mbhc->hphlocp_cnt < OCP_ATTEMPT) &&
		    (!mbhc->hphrocp_cnt)) {
			pr_debug("%s: retry\n", __func__);
			mbhc->hphlocp_cnt++;
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_OCP_FSM_EN, 0);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_OCP_FSM_EN, 1);
		} else {
			mbhc->mbhc_cb->irq_control(mbhc->codec,
						   mbhc->intr_ids->hph_left_ocp,
						   false);
			mbhc->hph_status |= SND_JACK_OC_HPHL;
			wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
					    mbhc->hph_status,
					    WCD_MBHC_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad wcd9xxx_spmi private data\n", __func__);
	}
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_hphr_ocp_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;

	pr_debug("%s: received HPHR OCP irq\n", __func__);
	if ((mbhc->hphrocp_cnt < OCP_ATTEMPT) &&
	    (!mbhc->hphlocp_cnt)) {
		pr_debug("%s: retry\n", __func__);
		mbhc->hphrocp_cnt++;
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_OCP_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_OCP_FSM_EN, 1);
	} else {
		mbhc->mbhc_cb->irq_control(mbhc->codec,
					   mbhc->intr_ids->hph_right_ocp,
					   false);
		mbhc->hph_status |= SND_JACK_OC_HPHR;
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD_MBHC_JACK_MASK);
	}
	return IRQ_HANDLED;
}

static void wcd_mbhc_moisture_config(struct wcd_mbhc *mbhc)
{
	if (mbhc->mbhc_cfg->moist_cfg.m_vref_ctl == V_OFF)
		return;

	/* Donot enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		pr_debug("%s: disable moisture detection for NC\n", __func__);
		return;
	}

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MOISTURE_VREF,
				 mbhc->mbhc_cfg->moist_cfg.m_vref_ctl);
	if (mbhc->mbhc_cb->hph_pull_up_control)
		mbhc->mbhc_cb->hph_pull_up_control(mbhc->codec,
				mbhc->mbhc_cfg->moist_cfg.m_iref_ctl);
}

static int wcd_mbhc_initialise(struct wcd_mbhc *mbhc)
{
	int ret = 0;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_LOCK(mbhc);

	/* enable HS detection */
	if (mbhc->mbhc_cb->hph_pull_up_control)
		mbhc->mbhc_cb->hph_pull_up_control(codec, I_DEFAULT);
	else
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HS_L_DET_PULL_UP_CTRL, 3);

	wcd_mbhc_moisture_config(mbhc);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHL_PLUG_TYPE, mbhc->hphl_swh);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_GND_PLUG_TYPE, mbhc->gnd_swh);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_SW_HPH_LP_100K_TO_GND, 1);
	if (mbhc->mbhc_cfg->gnd_det_en && mbhc->mbhc_cb->mbhc_gnd_det_ctrl)
		mbhc->mbhc_cb->mbhc_gnd_det_ctrl(codec, true);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL, 1);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_L_DET_EN, 1);

	/* Insertion debounce set to 96ms */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_INSREM_DBNC, 6);
#ifdef HDSET_FACTORY
	if((lge_get_boot_mode() == LGE_BOOT_MODE_QEM_56K) ||
		(lge_get_boot_mode() == LGE_BOOT_MODE_QEM_910K))
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_DBNC, 2);
	else {
		/* Button Debounce set to 32ms */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_DBNC, 3);
	}
#else
		/* Button Debounce set to 32ms */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_DBNC, 3);
#endif
	/* enable bias */
	mbhc->mbhc_cb->mbhc_bias(codec, true);
	/* enable MBHC clock */
	if (mbhc->mbhc_cb->clk_setup)
		mbhc->mbhc_cb->clk_setup(codec, true);

	/* program HS_VREF value */
	wcd_program_hs_vref(mbhc);

	wcd_program_btn_threshold(mbhc, false);

	INIT_WORK(&mbhc->correct_plug_swch, wcd_correct_swch_plug);

	init_completion(&mbhc->btn_press_compl);

	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return ret;
}

static void wcd_mbhc_fw_read(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct wcd_mbhc *mbhc;
	struct snd_soc_codec *codec;
	const struct firmware *fw;
	struct firmware_cal *fw_data = NULL;
	int ret = -1, retry = 0;
	bool use_default_cal = false;

	dwork = to_delayed_work(work);
	mbhc = container_of(dwork, struct wcd_mbhc, mbhc_firmware_dwork);
	codec = mbhc->codec;

	while (retry < FW_READ_ATTEMPTS) {
		retry++;
		pr_debug("%s:Attempt %d to request MBHC firmware\n",
			__func__, retry);
		if (mbhc->mbhc_cb->get_hwdep_fw_cal)
			fw_data = mbhc->mbhc_cb->get_hwdep_fw_cal(codec,
					WCD9XXX_MBHC_CAL);
		if (!fw_data)
			ret = request_firmware(&fw, "wcd9320/wcd9320_mbhc.bin",
				       codec->dev);
		/*
		 * if request_firmware and hwdep cal both fail then
		 * sleep for 4sec for the userspace to send data to kernel
		 * retry for few times before bailing out
		 */
		if ((ret != 0) && !fw_data) {
			usleep_range(FW_READ_TIMEOUT, FW_READ_TIMEOUT +
					WCD_MBHC_USLEEP_RANGE_MARGIN_US);
		} else {
			pr_debug("%s: MBHC Firmware read succesful\n",
					__func__);
			break;
		}
	}
	if (!fw_data)
		pr_debug("%s: using request_firmware\n", __func__);
	else
		pr_debug("%s: using hwdep cal\n", __func__);

	if (ret != 0 && !fw_data) {
		pr_err("%s: Cannot load MBHC firmware use default cal\n",
		       __func__);
		use_default_cal = true;
	}
	if (!use_default_cal) {
		const void *data;
		size_t size;

		if (fw_data) {
			data = fw_data->data;
			size = fw_data->size;
		} else {
			data = fw->data;
			size = fw->size;
		}
		if (wcd_mbhc_fw_validate(data, size) == false) {
			pr_err("%s: Invalid MBHC cal data size use default cal\n",
				__func__);
			if (!fw_data)
				release_firmware(fw);
		} else {
			if (fw_data) {
				mbhc->mbhc_cfg->calibration =
					(void *)fw_data->data;
				mbhc->mbhc_cal = fw_data;
			} else {
				mbhc->mbhc_cfg->calibration =
					(void *)fw->data;
				mbhc->mbhc_fw = fw;
			}
		}

	}

	(void) wcd_mbhc_initialise(mbhc);
}

int wcd_mbhc_set_keycode(struct wcd_mbhc *mbhc)
{
	enum snd_jack_types type;
	int i, ret, result = 0;
	int *btn_key_code;

	btn_key_code = mbhc->mbhc_cfg->key_code;

	for (i = 0 ; i < WCD_MBHC_KEYCODE_NUM ; i++) {
		if (btn_key_code[i] != 0) {
			switch (i) {
			case 0:
				type = SND_JACK_BTN_0;
				break;
			case 1:
				type = SND_JACK_BTN_1;
				break;
			case 2:
				type = SND_JACK_BTN_2;
				break;
			case 3:
				type = SND_JACK_BTN_3;
				break;
			case 4:
				type = SND_JACK_BTN_4;
				break;
			case 5:
				type = SND_JACK_BTN_5;
				break;
			case 6:
				type = SND_JACK_BTN_6;
				break;
			case 7:
				type = SND_JACK_BTN_7;
				break;
			default:
				WARN_ONCE(1, "Wrong button number:%d\n", i);
				result = -1;
				return result;
			}
			ret = snd_jack_set_key(mbhc->button_jack.jack,
							type,
							btn_key_code[i]);
			if (ret) {
				pr_err("%s: Failed to set code for %d\n",
					__func__, btn_key_code[i]);
				result = -1;
				return result;
			}
			input_set_capability(
				mbhc->button_jack.jack->input_dev,
				EV_KEY, btn_key_code[i]);
			pr_debug("%s: set btn%d key code:%d\n", __func__,
				i, btn_key_code[i]);
		}
	}
	if (btn_key_code[0])
		mbhc->is_btn_already_regd = true;
	return result;
}

int wcd_mbhc_start(struct wcd_mbhc *mbhc,
		       struct wcd_mbhc_config *mbhc_cfg)
{
	int rc = 0;

	pr_debug("%s: enter\n", __func__);
	/* update the mbhc config */
	mbhc->mbhc_cfg = mbhc_cfg;

	mbhc->mbhc_cfg->detect_extn_cable = lge_extn_cable_flag;

#ifdef CONFIG_LGE_ENABLE_HDSET_FACTORY
	if(lge_get_boot_mode() != LGE_BOOT_MODE_NORMAL && lge_get_boot_mode() != LGE_BOOT_MODE_QEM_130K){
		mbhc->mbhc_cfg->detect_extn_cable = 0;
	}
#endif
	/* Set btn key code */
	if ((!mbhc->is_btn_already_regd) && wcd_mbhc_set_keycode(mbhc))
		pr_err("Set btn key code error!!!\n");

	if (!mbhc->mbhc_cfg->read_fw_bin ||
	    (mbhc->mbhc_cfg->read_fw_bin && mbhc->mbhc_fw) ||
	    (mbhc->mbhc_cfg->read_fw_bin && mbhc->mbhc_cal)) {
		rc = wcd_mbhc_initialise(mbhc);
	} else {
		if (!mbhc->mbhc_fw || !mbhc->mbhc_cal)
			schedule_delayed_work(&mbhc->mbhc_firmware_dwork,
				      usecs_to_jiffies(FW_READ_TIMEOUT));
		else
			pr_err("%s: Skipping to read mbhc fw, 0x%pK %pK\n",
				 __func__, mbhc->mbhc_fw, mbhc->mbhc_cal);
	}
	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(wcd_mbhc_start);

void wcd_mbhc_stop(struct wcd_mbhc *mbhc)
{
	pr_debug("%s: enter\n", __func__);
	if (mbhc->current_plug != MBHC_PLUG_TYPE_NONE) {
		if (mbhc->mbhc_cb && mbhc->mbhc_cb->skip_imped_detect)
			mbhc->mbhc_cb->skip_imped_detect(mbhc->codec);
	}
	mbhc->current_plug = MBHC_PLUG_TYPE_NONE;
	mbhc->hph_status = 0;
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->irq_control) {
		mbhc->mbhc_cb->irq_control(mbhc->codec,
				mbhc->intr_ids->hph_left_ocp,
				false);
		mbhc->mbhc_cb->irq_control(mbhc->codec,
				mbhc->intr_ids->hph_right_ocp,
				false);
	}
	if (mbhc->mbhc_fw || mbhc->mbhc_cal) {
		cancel_delayed_work_sync(&mbhc->mbhc_firmware_dwork);
		if (!mbhc->mbhc_cal)
			release_firmware(mbhc->mbhc_fw);
		mbhc->mbhc_fw = NULL;
		mbhc->mbhc_cal = NULL;
	}
	pr_debug("%s: leave\n", __func__);
}
EXPORT_SYMBOL(wcd_mbhc_stop);

#ifdef CONFIG_DEBUG_FS
struct wcd_mbhc *debugMbhc;

static struct dentry *debugfs_mbhc_dent;
static struct dentry *debugfs_mic_type;
static struct dentry *debugfs_r_imped;
static struct dentry *debugfs_headset_type;
static struct dentry *debugfs_status;

static int mbhc_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
static char* mbhc_debug_get_current_plug(u8 current_plug)
{
	switch(current_plug)
	{
		case MBHC_PLUG_TYPE_NONE:
			return "None";
			break;
		case MBHC_PLUG_TYPE_HEADSET:
			return "HEADSET";
			break;
		case MBHC_PLUG_TYPE_HEADPHONE:
			return "HEADPHONE";
			break;
		case MBHC_PLUG_TYPE_HIGH_HPH:
			return "HIGH_HPH";
			break;
		case MBHC_PLUG_TYPE_GND_MIC_SWAP:
			return "GND_MIC_SWAP";
			break;
		default:
			return "Invalid Type";
			break;
	}
	return "None";

}
static ssize_t mbhc_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	int size = 1024;
	char lbuf[size];
	char *access_str = file->private_data;
	ssize_t ret_cnt = 0;
	enum wcd_mbhc_register_function mbhc_reg;
	s16 reg_value = 0;
	struct wcd_mbhc *mbhc = debugMbhc;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (!strcmp(access_str, "mic_type")) {
		scnprintf(lbuf,size,"%s\n",mbhc_debug_get_current_plug(debugMbhc->current_plug));
		ret_cnt = simple_read_from_buffer(ubuf, count, ppos, lbuf,strnlen(lbuf, size));
	} else if (!strcmp(access_str, "r_imped")) {
		scnprintf(lbuf,size,"r_imped = %d ohm\n",debugMbhc->zr);
		ret_cnt = simple_read_from_buffer(ubuf, count, ppos, lbuf,strnlen(lbuf, size));
	} else if (!strcmp(access_str, "headset_type")) {
		if(debugMbhc->current_plug != MBHC_PLUG_TYPE_NONE)
			scnprintf(lbuf,size,"%s\n",debugMbhc->sdev.name);
		else
			scnprintf(lbuf,size,"%s\n","None");
		ret_cnt = simple_read_from_buffer(ubuf, count, ppos, lbuf,strnlen(lbuf, size));
	} else if(!strcmp(access_str, "status")) {
		for(mbhc_reg = WCD_MBHC_L_DET_EN; mbhc_reg != WCD_MBHC_REG_FUNC_MAX; mbhc_reg++)
		{
			WCD_MBHC_REG_READ(mbhc_reg, reg_value);
			ret_cnt += scnprintf(lbuf + ret_cnt,size - ret_cnt,"%s : 0x%.2x\n",debugMbhc->wcd_mbhc_regs[mbhc_reg].id,reg_value);
		}
		lbuf[ret_cnt] = 0;
		ret_cnt = simple_read_from_buffer(ubuf, count, ppos, lbuf,ret_cnt);
	} else {
		pr_err("%s: %s not permitted to read\n", __func__, access_str);
		ret_cnt = -EPERM;
	}

	return ret_cnt;
}

static const struct file_operations codec_debug_ops = {
	.open = mbhc_debug_open,
	.read = mbhc_debug_read
};
#endif



/*
 * wcd_mbhc_init : initialize MBHC internal structures.
 *
 * NOTE: mbhc->mbhc_cfg is not YET configure so shouldn't be used
 */
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_codec *codec,
		      const struct wcd_mbhc_cb *mbhc_cb,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      struct wcd_mbhc_register *wcd_mbhc_regs,
		      bool impedance_det_en)
{
	int ret = 0;
	int hph_swh = 0;
	int gnd_swh = 0;
	struct snd_soc_card *card = codec->component.card;
	const char *hph_switch = "qcom,msm-mbhc-hphl-swh";
	const char *gnd_switch = "qcom,msm-mbhc-gnd-swh";
	const char *lge_extn_cable = "lge,msm-mbhc-extn-cable";

	pr_debug("%s: enter\n", __func__);

	ret = of_property_read_u32(card->dev->of_node, hph_switch, &hph_swh);
	if (ret) {
		dev_err(card->dev,
			"%s: missing %s in dt node\n", __func__, hph_switch);
		goto err;
	}

	ret = of_property_read_u32(card->dev->of_node, gnd_switch, &gnd_swh);
	if (ret) {
		dev_err(card->dev,
			"%s: missing %s in dt node\n", __func__, gnd_switch);
		goto err;
	}

	ret = of_property_read_u32(card->dev->of_node, lge_extn_cable, &lge_extn_cable_flag);
	if (ret) {
		pr_info("%s: missing %s in dt node\n", __func__, lge_extn_cable);
	}
#ifdef CONFIG_SND_SOC_ES9218P
	if (enable_es9218p)
		pr_info("%s: Enable enable_es9218p\n", __func__);
	else
		pr_info("%s: Disable enable_es9218p\n", __func__);
#endif

	mbhc->in_swch_irq_handler = false;
	mbhc->current_plug = MBHC_PLUG_TYPE_NONE;
	mbhc->is_btn_press = false;
	mbhc->codec = codec;
	mbhc->intr_ids = mbhc_cdc_intr_ids;
	mbhc->impedance_detect = impedance_det_en;
	mbhc->hphl_swh = hph_swh;
	mbhc->gnd_swh = gnd_swh;
	mbhc->micbias_enable = false;
	mbhc->mbhc_cb = mbhc_cb;
	mbhc->btn_press_intr = false;
	mbhc->is_hs_recording = false;
	mbhc->is_extn_cable = false;
	mbhc->hph_type = WCD_MBHC_HPH_NONE;
	mbhc->wcd_mbhc_regs = wcd_mbhc_regs;
#ifdef CONFIG_SND_USE_MBHC_EXTN_CABLE
	extn_cable = false;
#endif

	if (mbhc->intr_ids == NULL) {
		pr_err("%s: Interrupt mapping not provided\n", __func__);
		return -EINVAL;
	}
	if (!mbhc->wcd_mbhc_regs) {
		dev_err(codec->dev, "%s: mbhc registers are not defined\n",
			__func__);
		return -EINVAL;
	}

	/* Check if IRQ and other required callbacks are defined or not */
	if (!mbhc_cb || !mbhc_cb->request_irq || !mbhc_cb->irq_control ||
	    !mbhc_cb->free_irq || !mbhc_cb->map_btn_code_to_num ||
	    !mbhc_cb->lock_sleep || !mbhc_cb->mbhc_bias ||
	    !mbhc_cb->set_btn_thr) {
		dev_err(codec->dev, "%s: required mbhc callbacks are not defined\n",
			__func__);
		return -EINVAL;
	}

	if (mbhc->headset_jack.jack == NULL) {
		ret = snd_soc_jack_new(codec, "Headset Jack",
				WCD_MBHC_JACK_MASK, &mbhc->headset_jack);
		if (ret) {
			pr_err("%s: Failed to create new jack\n", __func__);
			return ret;
		}

		ret = snd_soc_jack_new(codec, "Button Jack",
				       WCD_MBHC_JACK_BUTTON_MASK,
				       &mbhc->button_jack);
		if (ret) {
			pr_err("Failed to create new jack\n");
			return ret;
		}

		ret = snd_jack_set_key(mbhc->button_jack.jack,
				       SND_JACK_BTN_0,
				       KEY_MEDIA);
		if (ret) {
			pr_err("%s: Failed to set code for btn-0\n",
				__func__);
			return ret;
		}

		set_bit(INPUT_PROP_NO_DUMMY_RELEASE,
			mbhc->button_jack.jack->input_dev->propbit);

		INIT_DELAYED_WORK(&mbhc->mbhc_firmware_dwork,
				  wcd_mbhc_fw_read);
		INIT_DELAYED_WORK(&mbhc->mbhc_btn_dwork, wcd_btn_lpress_fn);
	}
#ifdef CONFIG_MACH_LGE
	mbhc->sdev.name = "h2w";

	ret = switch_dev_register(&mbhc->sdev);
	if (ret < 0) {
		pr_err("[LGE MBHC] Failed to register switch device\n");
		switch_dev_unregister(&mbhc->sdev);
	}
#endif
#ifdef CONFIG_DEBUG_FS
	debugMbhc = mbhc;

	debugfs_mbhc_dent = debugfs_create_dir("mbhc_v2", 0);
	if (!IS_ERR(debugfs_mbhc_dent)) {
		debugfs_mic_type = debugfs_create_file("mic_type",
		S_IRUGO, debugfs_mbhc_dent,
		(void *) "mic_type", &codec_debug_ops);

		debugfs_r_imped = debugfs_create_file("r_imped",
		S_IRUGO, debugfs_mbhc_dent,
		(void *) "r_imped", &codec_debug_ops);

		debugfs_headset_type = debugfs_create_file("headset_type",
		S_IRUGO, debugfs_mbhc_dent,
		(void *) "headset_type", &codec_debug_ops);

		debugfs_status = debugfs_create_file("status",
		S_IRUGO, debugfs_mbhc_dent,
		(void *) "status", &codec_debug_ops);
	}
#endif
	mutex_init(&mbhc->hphl_pa_lock);
	mutex_init(&mbhc->hphr_pa_lock);

	/* Register event notifier */
	mbhc->nblock.notifier_call = wcd_event_notify;
	if (mbhc->mbhc_cb->register_notifier) {
		ret = mbhc->mbhc_cb->register_notifier(codec, &mbhc->nblock,
						       true);
		if (ret) {
			pr_err("%s: Failed to register notifier %d\n",
				__func__, ret);
			return ret;
		}
	}

#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
    current_status_detect_water=false;
    nblock_for_usb.notifier_call = wcd_event_notify_from_usb;
    checkMBHC = mbhc;
    if( notifier_from_usb != NULL ) {
        blocking_notifier_chain_register(notifier_from_usb, &nblock_for_usb);
        pr_debug("%s: notifier_from_usb register succeeded \n", __func__);
    } else
        pr_err("%s: notifier_from_usb register failed \n", __func__);
#endif // CONFIG_LGE_MBHC_DET_WATER_ON_USB

	init_waitqueue_head(&mbhc->wait_btn_press);
	mutex_init(&mbhc->codec_resource_lock);

	ret = mbhc->mbhc_cb->request_irq(codec, mbhc->intr_ids->mbhc_sw_intr,
				  wcd_mbhc_mech_plug_detect_irq,
				  "mbhc sw intr", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d, ret = %d\n", __func__,
		       mbhc->intr_ids->mbhc_sw_intr, ret);
		goto err_mbhc_sw_irq;
	}

	ret = mbhc->mbhc_cb->request_irq(codec,
					 mbhc->intr_ids->mbhc_btn_press_intr,
					 wcd_mbhc_btn_press_handler,
					 "Button Press detect",
					 mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->mbhc_btn_press_intr);
		goto err_btn_press_irq;
	}

	ret = mbhc->mbhc_cb->request_irq(codec,
					 mbhc->intr_ids->mbhc_btn_release_intr,
					 wcd_mbhc_release_handler,
					 "Button Release detect", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			mbhc->intr_ids->mbhc_btn_release_intr);
		goto err_btn_release_irq;
	}

	ret = mbhc->mbhc_cb->request_irq(codec,
					 mbhc->intr_ids->mbhc_hs_ins_intr,
					 wcd_mbhc_hs_ins_irq,
					 "Elect Insert", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->mbhc_hs_ins_intr);
		goto err_mbhc_hs_ins_irq;
	}
	mbhc->mbhc_cb->irq_control(codec, mbhc->intr_ids->mbhc_hs_ins_intr,
				   false);
	clear_bit(WCD_MBHC_ELEC_HS_INS, &mbhc->intr_status);

	ret = mbhc->mbhc_cb->request_irq(codec,
					 mbhc->intr_ids->mbhc_hs_rem_intr,
					 wcd_mbhc_hs_rem_irq,
					 "Elect Remove", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->mbhc_hs_rem_intr);
		goto err_mbhc_hs_rem_irq;
	}
	mbhc->mbhc_cb->irq_control(codec, mbhc->intr_ids->mbhc_hs_rem_intr,
				   false);
	clear_bit(WCD_MBHC_ELEC_HS_REM, &mbhc->intr_status);

	ret = mbhc->mbhc_cb->request_irq(codec, mbhc->intr_ids->hph_left_ocp,
				  wcd_mbhc_hphl_ocp_irq, "HPH_L OCP detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->hph_left_ocp);
		goto err_hphl_ocp_irq;
	}

	ret = mbhc->mbhc_cb->request_irq(codec, mbhc->intr_ids->hph_right_ocp,
				  wcd_mbhc_hphr_ocp_irq, "HPH_R OCP detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->hph_right_ocp);
		goto err_hphr_ocp_irq;
	}

	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;

err_hphr_ocp_irq:
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->hph_left_ocp, mbhc);
err_hphl_ocp_irq:
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_hs_rem_intr, mbhc);
err_mbhc_hs_rem_irq:
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_hs_ins_intr, mbhc);
err_mbhc_hs_ins_irq:
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_btn_release_intr,
				mbhc);
err_btn_release_irq:
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_btn_press_intr,
				mbhc);
err_btn_press_irq:
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_sw_intr, mbhc);
err_mbhc_sw_irq:
	if (mbhc->mbhc_cb->register_notifier)
		mbhc->mbhc_cb->register_notifier(codec, &mbhc->nblock, false);
	mutex_destroy(&mbhc->codec_resource_lock);
err:
	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(wcd_mbhc_init);

void wcd_mbhc_deinit(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
#ifdef CONFIG_MACH_LGE
	switch_dev_unregister(&mbhc->sdev);
#endif
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(debugfs_mbhc_dent);
#endif

	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_sw_intr, mbhc);
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_btn_press_intr,
				mbhc);
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_btn_release_intr,
				mbhc);
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_hs_ins_intr, mbhc);
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->mbhc_hs_rem_intr, mbhc);
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->hph_left_ocp, mbhc);
	mbhc->mbhc_cb->free_irq(codec, mbhc->intr_ids->hph_right_ocp, mbhc);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->register_notifier)
		mbhc->mbhc_cb->register_notifier(codec, &mbhc->nblock, false);

#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
    if( notifier_from_usb != NULL )
        blocking_notifier_chain_unregister(notifier_from_usb, &nblock_for_usb);
#endif // CONFIG_LGE_MBHC_DET_WATER_ON_USB

	mutex_destroy(&mbhc->codec_resource_lock);
}
EXPORT_SYMBOL(wcd_mbhc_deinit);

#ifdef CONFIG_LGE_MBHC_DET_WATER_ON_USB
static int wcd_event_notify_from_usb(struct notifier_block *self, unsigned long val, void *data)
{
//    struct wcd_mbhc *mbhc = (struct wcd_mbhc *)data;
	enum wcd_mbhc_det_water_usb event = (enum wcd_mbhc_det_water_usb)val;

    pr_debug("%s: enter, event=%d \n", __func__, event);

    if( current_status_detect_water == (bool)event ) {
        pr_debug("%s: do not update status[current_status_detect_water=%s]  \n", __func__, current_status_detect_water?"true":"false");
        return 0;
    }

    ///////////////////////////////////////////////////
    // A work_queue is better if code block below becomes heavy, or consume a lot of time slices because of handling interrupt handlers
    ///////////////////////////////////////////////////
    WCD_MBHC_RSC_LOCK(checkMBHC);

    switch(event) {
        case WCD_MBHC_CLEARED_WATER:
            current_status_detect_water = false;

            /*
             * case #1 : disable button irq handlers
             * case #2 : disable button irq handlers and detect irq handler
             * following commands depend on scenario.
            checkMBHC->mbhc_cb->irq_control(checkMBHC->codec, checkMBHC->intr_ids->mbhc_btn_press_intr, true);
            checkMBHC->mbhc_cb->irq_control(checkMBHC->codec, checkMBHC->intr_ids->mbhc_btn_release_intr, true);
            checkMBHC->mbhc_cb->irq_control(checkMBHC->codec, checkMBHC->intr_ids->mbhc_sw_intr, true);
            */
            break;
        case WCD_MBHC_DETECTED_WATER:
            // 0. check if a plug is inserted
            if( checkMBHC->current_plug == MBHC_PLUG_TYPE_HEADSET ||
                checkMBHC->current_plug == MBHC_PLUG_TYPE_HEADPHONE ||
                checkMBHC->current_plug == MBHC_PLUG_TYPE_HIGH_HPH )  {
                // 2-0. already inserted
                // 2-1. check if interrupt handlers or work_queues are working now
                // 2-2. if so, MAY cancel all interrupt handlers, if not, go to a next step
                wcd_cancel_btn_work(checkMBHC);

                /*
                 * case #1 : disable button irq handlers
                 * case #2 : disable button irq handlers and detect irq handler
                 * following commands depend on scenario.
                checkMBHC->mbhc_cb->irq_control(checkMBHC->codec, checkMBHC->intr_ids->mbhc_btn_press_intr, false);
                checkMBHC->mbhc_cb->irq_control(checkMBHC->codec, checkMBHC->intr_ids->mbhc_btn_release_intr, false);
                checkMBHC->mbhc_cb->irq_control(checkMBHC->codec, checkMBHC->intr_ids->mbhc_sw_intr, false);
                */

                // 2-3. send a removal event to user space
                if( checkMBHC->current_plug == MBHC_PLUG_TYPE_HEADPHONE ) {
                    //wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADPHONE);
                    //wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,	mbhc->hph_status, WCD_MBHC_JACK_MASK);
                    snd_soc_jack_report(&checkMBHC->headset_jack, checkMBHC->hph_status &= ~SND_JACK_HEADPHONE, WCD_MBHC_JACK_MASK);
                } else if( checkMBHC->current_plug == MBHC_PLUG_TYPE_HEADSET ) {
                    //wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADSET);
                    //wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,	mbhc->hph_status, WCD_MBHC_JACK_MASK);
                    snd_soc_jack_report(&checkMBHC->headset_jack, checkMBHC->hph_status &= ~SND_JACK_HEADSET, WCD_MBHC_JACK_MASK);
                } else if( checkMBHC->current_plug == MBHC_PLUG_TYPE_HIGH_HPH ) {
                    //wcd_mbhc_report_plug(mbhc, 0, SND_JACK_LINEOUT);
                    //wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,	mbhc->hph_status, WCD_MBHC_JACK_MASK);
                    snd_soc_jack_report(&checkMBHC->headset_jack, checkMBHC->hph_status &= ~SND_JACK_LINEOUT, WCD_MBHC_JACK_MASK);
                }

                switch_set_state(&checkMBHC->sdev, NO_DEVICE);
                pr_info("%s: sent a removal event to user space \n", __func__);
            } else {
                // 1-0. no plug
                // 1-1. go to a next step
            }

            // 3. skip to report an insertion event if detecting 4 pole aux cable, which means that there is water on an earajcke socket
            current_status_detect_water = true;
            break;
        default:
            pr_err("%s: unknown event[%d] \n", __func__, event);
            break;
    }

    WCD_MBHC_RSC_UNLOCK(checkMBHC);
    pr_debug("%s: leave, event=%d \n", __func__, event);

    return 0;
}

void detect_water_on_usb(bool det_water)
{
/* for debugging
    pr_debug("%s: enter, %s water on a usb port[det_water=%d] \n", __func__, det_water?"detected":"cleared", det_water);

    if( current_status_detect_water == det_water ) {
        pr_debug("%s: do not update status[current_status_detect_water=%s]  \n", __func__, current_status_detect_water?"true":"false");
        return;
    }

    current_status_detect_water = det_water;
*/
    if( det_water )
        blocking_notifier_call_chain(notifier_from_usb,
					                WCD_MBHC_DETECTED_WATER,
					                NULL);
    else
        blocking_notifier_call_chain(notifier_from_usb,
					                WCD_MBHC_CLEARED_WATER,
					                NULL);
    return;
}
EXPORT_SYMBOL(detect_water_on_usb);

void init_for_mbhc(void* data){
    pr_debug("%s: enter \n", __func__);
    notifier_from_usb = (struct blocking_notifier_head*)data;
}
EXPORT_SYMBOL(init_for_mbhc);
#endif // CONFIG_LGE_MBHC_DET_WATER_ON_USB

MODULE_DESCRIPTION("wcd MBHC v2 module");
MODULE_LICENSE("GPL v2");
