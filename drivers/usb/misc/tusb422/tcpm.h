/*
 * TUSB422 Power Delivery
 *
 * Author: Brian Quach <brian.quach@ti.com>
 *
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __TCPM_H__
#define __TCPM_H__

#include "tusb422_common.h"
#include "tcpci.h"
#ifndef CONFIG_TUSB422
    #include <stdbool.h>
    #include <stdint.h>
#endif

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#include <linux/time.h>
#include <linux/power_supply.h>
#endif

/*
* Macros and definitions
*/

#define NUM_TCPC_DEVICES   1

#define ERR_NO_TI_DEV  (-77)

#define MV_TO_25MV(x) ((x)/25)
#define VSAFE0V_MAX   MV_TO_25MV(800)   /* 0.80V max in 25mV units */
#define VSAFE5V_MIN   MV_TO_25MV(4450)  /* 4.45V min in 25mV units */
#ifdef CONFIG_LGE_USB_TYPE_C
#define VDISCON_MAX   VSAFE0V_MAX
#else
#define VDISCON_MAX   MV_TO_25MV(4000)  /* 4.00V min in 25mV units */
#endif
#define VSTOP_DISCHRG MV_TO_25MV(500)   /* Stop discharge threshold in 25mV units */

/* assume default current of 500mA (USB2) */
#ifdef CONFIG_LGE_USB_TYPE_C
#define GET_SRC_CURRENT_MA(cc_adv) (((cc_adv) == CC_SNK_STATE_POWER30) ? 3000 : ((cc_adv) == CC_SNK_STATE_POWER15) ? 1500 : ((cc_adv) == CC_SNK_STATE_DEFAULT) ? 500 : 0)
#else
#define GET_SRC_CURRENT_MA(cc_adv) (((cc_adv) == CC_SNK_STATE_POWER30) ? 3000 : ((cc_adv) == CC_SNK_STATE_POWER15) ? 1500 : 500)
#endif

/* Sink with Accessory Support is NOT supported by the state machine */

typedef enum
{
	TCPC_STATE_UNATTACHED_SRC = 0,
	TCPC_STATE_UNATTACHED_SNK,
	TCPC_STATE_ATTACH_WAIT_SRC,
	TCPC_STATE_ATTACH_WAIT_SNK,
	TCPC_STATE_TRY_SNK,
	TCPC_STATE_TRY_SNK_LOOK4SRC,
	TCPC_STATE_TRY_SRC,
	TCPC_STATE_TRY_WAIT_SRC,
	TCPC_STATE_TRY_WAIT_SNK,
	TCPC_STATE_ATTACHED_SRC,
	TCPC_STATE_ATTACHED_SNK,
	TCPC_STATE_UNORIENTED_DEBUG_ACC_SRC,
	TCPC_STATE_ORIENTED_DEBUG_ACC_SRC,
	TCPC_STATE_DEBUG_ACC_SNK,
	TCPC_STATE_AUDIO_ACC,
	TCPC_STATE_ERROR_RECOVERY,
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	TCPC_STATE_CC_FAULT_CC_OV,
	TCPC_STATE_CC_FAULT_SWING,
	TCPC_STATE_CC_FAULT_SBU_OV,
	TCPC_STATE_CC_FAULT_SBU_ADC,
	TCPC_STATE_CC_FAULT_SBU_DRY_CHECK,
	TCPC_STATE_CC_FAULT_TEST,
#endif
	TCPC_STATE_DISABLED,  /* no CC terminations */
	TCPC_NUM_STATES
} tcpc_state_t;

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#define IS_STATE_CC_FAULT(state) \
	((state == TCPC_STATE_CC_FAULT_CC_OV) || \
	 (state == TCPC_STATE_CC_FAULT_SWING) || \
	 (state == TCPC_STATE_CC_FAULT_SBU_OV) || \
	 (state == TCPC_STATE_CC_FAULT_SBU_ADC) || \
	 (state == TCPC_STATE_CC_FAULT_SBU_DRY_CHECK) || \
	 (state == TCPC_STATE_CC_FAULT_TEST))
#endif

extern const char * const tcstate2string[TCPC_NUM_STATES];

typedef enum
{
	TC_FLAGS_TRY_SRC        = (1 << 0),	  /* Either Try.SRC or Try.SNK support but not both */
	TC_FLAGS_TRY_SNK        = (1 << 1),	  /* Either Try.SRC or Try.SNK support but not both */
	TC_FLAGS_TEMP_ROLE      = (1 << 2)
} tc_flags_t;


typedef enum
{
	PLUG_UNFLIPPED = 0,	 /* USB-PD comm on CC1 */
	PLUG_FLIPPED,		 /* USB-PD comm on CC2 */
} plug_polarity_t;


//typedef enum
//{
//	CC_STATE_OPEN        = 0,
//	CC_STATE_SRC_RA      = 1,
//	CC_STATE_SRC_RD      = 2,
//	CC_STATE_SNK_DEFAULT = (1 | CC_STATUS_CONNECT_RESULT), /* bit 2 is set if TCPC is presenting Rd */
//	CC_STATE_SNK_1P5A    = (2 | CC_STATUS_CONNECT_RESULT), /* bit 2 is set if TCPC is presenting Rd */
//	CC_STATE_SNK_3P0A    = (3 | CC_STATUS_CONNECT_RESULT), /* bit 2 is set if TCPC is presenting Rd */
//} cc_state_t;


typedef struct
{
	uint8_t             port;
	tcpc_state_t        state;
	tcpc_state_t        last_state;	   /* for debug */
	bool                state_change;

	struct tusb422_timer_t timer;
	struct tusb422_timer_t timer2;

	tc_role_t           role;
	tcpc_role_rp_val_t  rp_val;

	uint8_t             flags;

	uint8_t             cc_status;
#ifdef CONFIG_LGE_USB_TYPE_C
	uint8_t             last_cc_status;
#endif
	bool				src_detected;  /* source detected for debounce period */

	plug_polarity_t     plug_polarity;

	tcpc_cc_snk_state_t src_current_adv;
	bool                src_attach_notify;
	uint8_t             silicon_revision;
	uint8_t             rx_buff_overflow_cnt;
	uint8_t             vconn_ocp_cnt;
#ifdef CONFIG_LGE_USB_TYPE_C
	bool                debug_accessory_mode;
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	unsigned long       cc_swing_timeout;
	uint8_t             cc_swing_cnt;
	unsigned int        cc_swing_recheck_cnt;

	struct timespec     cc_fault_timeout;
	void                (*cc_fault_timeout_function)(unsigned int);
	struct mutex        cc_fault_timer_lock;

	bool                moisture_detect_disable;
	bool                moisture_detect_use_sbu;

	struct power_supply *typec_psy;
#endif
#endif
} tcpc_device_t;

typedef struct
{
	uint16_t      IE;
} tUSBPD_Intf_Context;

typedef enum
{
	TCPM_STATUS_OK = 0,
	TCPM_STATUS_SMBUS_ERROR,
	TCPM_STATUS_TIMEOUT,
	TCPM_STATUS_PARAM_ERROR

} tcpm_status_t;


typedef enum
{
	TX_STATUS_SUCCESS = 0,
	TX_STATUS_DISCARDED,
	TX_STATUS_FAILED
} tx_status_t;


typedef struct
{
	void (*conn_state_change_cbk)(unsigned int port, tcpc_state_t state);
	void (*current_change_cbk)(unsigned int port, tcpc_cc_snk_state_t cc_state);
	void (*volt_alarm_cbk)(unsigned int port, bool hi_volt);
	void (*pd_hard_reset_cbk)(unsigned int port);
	void (*pd_transmit_cbk)(unsigned int port, tx_status_t tx_status);
	void (*pd_receive_cbk)(unsigned int port);

} tcpm_callbacks_t;

//*****************************************************************************
//
//! \brief   Initializes the SMBus interface(s) used to talk to TCPCs.
//!
//!
//! \param   none
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************


void tcpm_register_callbacks(const tcpm_callbacks_t *callbacks);

void tcpm_get_msg_header_type(unsigned int port, uint8_t *frame_type, uint16_t *header);
void tcpm_read_message(unsigned int port, uint8_t *buf, uint8_t len);
void tcpm_transmit(unsigned int port, uint8_t *buf, tcpc_transmit_t sop_type);

void tcpm_enable_pd_receive(unsigned int port, bool enable_sop_p, bool enable_sop_pp);
void tcpm_disable_pd_receive(unsigned int port);

void tcpm_set_voltage_alarm_lo(unsigned int port, uint16_t threshold_25mv);
void tcpm_set_voltage_alarm_hi(unsigned int port, uint16_t threshold_25mv);

bool tcpm_is_vbus_present(unsigned int port);
uint16_t tcpm_get_vbus_voltage(unsigned int port);
void tcpm_enable_vbus_detect(unsigned int port);
void tcpm_disable_vbus_detect(unsigned int port);

void tcpm_set_autodischarge_disconnect(unsigned int port, bool enable);
void tcpm_set_sink_disconnect_threshold(unsigned int port, uint16_t threshold_25mv);
void tcpm_force_discharge(unsigned int port, uint16_t threshold_25mv);
void tcpm_enable_bleed_discharge(unsigned int port);

void tcpm_execute_error_recovery(unsigned int port);
#ifdef CONFIG_LGE_USB_TYPE_C
void tcpm_execute_shutdown(unsigned int port);
#endif

void tcpm_try_role_swap(unsigned int port);
void tcpm_change_role(unsigned int port, tc_role_t new_role);

void tcpm_src_vbus_disable(unsigned int port);
void tcpm_src_vbus_enable(unsigned int port, uint16_t mv);

void tcpm_snk_vbus_enable(unsigned int port);
void tcpm_snk_vbus_disable(unsigned int port);

void tcpm_set_vconn_enable(unsigned int port, bool enable);
void tcpm_vconn_pin_rp_control(unsigned int port, bool enable);
bool tcpm_is_vconn_enabled(unsigned int port);

void tcpm_set_bist_test_mode(unsigned int port);

tcpc_device_t* tcpm_get_device(unsigned int port);

void tcpm_cc_pin_control(unsigned int port, tc_role_t role);
void tcpm_handle_power_role_swap(unsigned int port);
void tcpm_update_msg_header_info(unsigned int port, uint8_t data_role, uint8_t power_role);
void tcpm_set_rp_value(unsigned int port, tcpc_role_rp_val_t rp_val);
void tcpm_snk_swap_standby(unsigned int port);

void tcpm_register_dump(unsigned int port);

#ifdef CONFIG_LGE_USB_TYPE_C
void tcpm_cc_fault_timer(unsigned int port, bool enable);
void tcpm_cc_fault_set(unsigned int port, tcpc_state_t state);
void tcpm_cc_fault_test(unsigned int port, bool enable);
#endif

#endif //__TCPM_H__

