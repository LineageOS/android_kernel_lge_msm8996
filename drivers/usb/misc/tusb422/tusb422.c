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

#include "tusb422.h"
#include "tusb422_common.h"
#include "tcpci.h"
#include "tcpm.h"

static struct tusb422_timer_t *lfo_timer[NUM_TCPC_DEVICES];

void tusb422_lfo_timer_start(struct tusb422_timer_t *timer, uint16_t timeout_ms, void (*function)(unsigned int))
{
	unsigned int port = timer->data;

	// Set timer function pointer.
	timer->function = function;

	// Save timer pointer.
	lfo_timer[port] = timer;

	// Start timer.
	tcpc_write16(port, TUSB422_REG_LFO_TIMER, timeout_ms);

	return;
}


void tusb422_lfo_timer_cancel(struct tusb422_timer_t *timer)
{
	unsigned int port = timer->data;

	// Stop timer.
	tcpc_write16(port, TUSB422_REG_LFO_TIMER, 0);

	// Clear timer function pointer.
	timer->function = NULL;

	return;
}

void tusb422_send_fast_role_swap(unsigned int port)
{
	// Send Fast role swap.
	tcpc_write8(port, TUSB422_REG_BMC_TX_CTRL, TUSB422_BMC_TX_FASTROLE_SWAP);

	return;
}


uint8_t tusb422_get_revision(unsigned int port)
{
	uint8_t rev;

	tcpc_write8(port, 0xFF, 1);	 /* Page 1 */
	tcpc_read8(port, 0xC0, &rev);
	tcpc_write8(port, 0xFF, 0);	 /* Page 0 */

	return rev;
}

bool tusb422_is_present(unsigned int port)
{
	uint16_t vid;
	uint16_t pid;

	// Read VID/PID/DID.
	tcpc_read16(port, TCPC_REG_VENDOR_ID, &vid);
#ifdef CONFIG_LGE_USB_TYPE_C
	PRINT("VID: 0x%04x\n", vid);
#else
	INFO("VID: 0x%04x\n", vid);
#endif
	tcpc_read16(port, TCPC_REG_PRODUCT_ID, &pid);
#ifdef CONFIG_LGE_USB_TYPE_C
	PRINT("PID: 0x%04x\n", pid);
#else
	INFO("PID: 0x%04x\n", pid);
#endif

	return ((vid != TI_VID) || (pid != TI_PID)) ? false : true;
}

#define EFUSE_REG_E7_TRIM_BIT  (1 << 7)  /* bit is set if chip is trimmed */

bool tusb422_is_trimmed(unsigned int port)
{
	uint8_t efuse;

	tcpc_write8(port, 0xFF, 1);	  /* Page 1 */
	tcpc_read8(port, 0xE7, &efuse);
	tcpc_write8(port, 0xFF, 0);	 /* Page 0 */

	return(efuse & EFUSE_REG_E7_TRIM_BIT) ? true : false;
}

#define OTSD1_EN_BIT  1

void tusb422_init(unsigned int port)
{
	// Check trim.
	if (!tusb422_is_trimmed(port))
	{
		// Write nominal soft trim values for pre-production samples.
		tcpc_write8(port, 0xFF, 1);	  /* Page 1 */
		tcpc_write8(port, 0xE0, 0xC0);
		tcpc_write8(port, 0xE1, 0x8);
		tcpc_write8(port, 0xE2, 0x20);
		tcpc_write8(port, 0xE3, 0x80);
		tcpc_write8(port, 0xE4, 0x96);
		tcpc_write8(port, 0xE5, 0x1A);
		tcpc_write8(port, 0xE6, 0x80);
		tcpc_write8(port, 0xE7, 0x70);
		tcpc_write8(port, 0xFF, 0);	 /* Page 0 */
	}

	// Enable OTSD1.
	tcpc_write8(port, TUSB422_REG_OTSD_CTRL, OTSD1_EN_BIT);

	// Clear interrupt status.
	tcpc_write8(port, TUSB422_REG_INT_STATUS, TUSB422_INT_MASK_ALL);

	// Unmask LFO timer interrupt.
#if defined(CONFIG_LGE_USB_TYPE_C) && !defined(CONFIG_LGE_USB_COMPLIANCE_TEST)
	tcpc_write8(port, TUSB422_REG_INT_MASK, TUSB422_INT_CC_FAULT | TUSB422_INT_LFO_TIMER);
#else
	tcpc_write8(port, TUSB422_REG_INT_MASK, TUSB422_INT_LFO_TIMER);
#endif

	// Unmask vendor alert.
#ifdef CONFIG_LGE_USB_TYPE_C
	tcpc_modify16(port, TCPC_REG_ALERT_MASK, TCPC_ALERT_POWER_STATUS, TUSB422_ALERT_IRQ_STATUS | TCPC_ALERT_CC_STATUS);
#else
	tcpc_modify16(port, TCPC_REG_ALERT_MASK, 0, TUSB422_ALERT_IRQ_STATUS);
#endif

	// Set 1ms sampling rate for better force discharge accuracy.
	tusb422_set_cc_sample_rate(port, CC_SAMPLE_RATE_1MS);

#ifdef CONFIG_LGE_USB_TYPE_C
	tcpc_modify8(port, TUSB422_REG_CC_GEN_CTRL, TUSB422_DRP_DUTY_CYCLE_MASK, DRP_DUTY_CYCLE_60PCT);
#endif
	return;
}

/* For PD v3.0 and higher */
void tusb422_set_fast_role_swap_detect(unsigned int port, bool enable)
{
	if (enable)
	{
		// Enable Fast role swap detect.
		tcpc_modify8(port, TUSB422_REG_BMC_RX_CTRL, 0, TUSB422_BMC_FASTROLE_RX_EN);
	}
	else
	{
		// Disable Fast role swap detect.
		tcpc_modify8(port, TUSB422_REG_BMC_RX_CTRL, TUSB422_BMC_FASTROLE_RX_EN, 0);
	}

	return;
}


void tusb422_isr(unsigned int port)
{
	uint8_t irq_status;

	tcpc_read8(port, TUSB422_REG_INT_STATUS, &irq_status);

	INFO("422IRQ = 0x%x\n", irq_status);

	if (irq_status & TUSB422_INT_LFO_TIMER)
	{
		DEBUG("LFO timer expired!\n");

		if (lfo_timer[port]->function)
		{
			lfo_timer[port]->function(lfo_timer[port]->data);
		}
	}

	if (irq_status & TUSB422_INT_CC_FAULT)
	{
		CRIT("CC fault (> 3.5V)!\n");
	}

	if (irq_status & TUSB422_INT_OTSD1_STAT)
	{
		CRIT("Over-temp detected!\n");
	}

	if (irq_status & TUSB422_INT_FAST_ROLE_SWAP)
	{
		DEBUG("FR Swap Rx'd!\n");
	}

	// Clear interrupt status.
	tcpc_write8(port, TUSB422_REG_INT_STATUS, irq_status);

	return;
}


/* may use larger sample rate when unattached. */
void tusb422_set_cc_sample_rate(unsigned int port, tusb422_sample_rate_t rate)
{
	// Set CC sample rate.
	tcpc_modify8(port, TUSB422_REG_CC_GEN_CTRL, TUSB422_CC_SAMPLE_RATE_MASK,
				 (rate << TUSB422_CC_SAMPLE_RATE_SHIFT));
	return;
}

/* Do not use PD reset for PG1.0/1.1/1.2 */
void tusb422_pd_reset(unsigned int port)
{
	uint8_t data;

#ifdef CONFIG_LGE_USB_TYPE_C
	DEBUG("%s\n", __func__);
#endif

	// Reset Tx/Rx PD state machines (write 0, then 1).
	tcpc_read8(port, TUSB422_REG_CC_GEN_CTRL, &data);
	data &= ~TUSB422_PD_TX_RX_RESET;
	tcpc_write8(port, TUSB422_REG_CC_GEN_CTRL, data);
	data |= TUSB422_PD_TX_RX_RESET;
	tcpc_write8(port, TUSB422_REG_CC_GEN_CTRL, data);

	return;
}

void tusb422_sw_reset(unsigned int port)
{
	uint8_t data;

#ifdef CONFIG_LGE_USB_TYPE_C
	DEBUG("%s\n", __func__);
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
	// Set pgage 0
	tcpc_write8(0, 0xFF, 0);
#endif

	// Perform global reset (write 0, then 1).
	tcpc_read8(port, TUSB422_REG_CC_GEN_CTRL, &data);
	data &= ~TUSB422_GLOBAL_SW_RESET;
	tcpc_write8(port, TUSB422_REG_CC_GEN_CTRL, data);
	data |= TUSB422_GLOBAL_SW_RESET;
	tcpc_write8(port, TUSB422_REG_CC_GEN_CTRL, data);

	// Wait for reset to complete.
	tcpm_msleep(2);

	return;
}

#define INT_VBUSDIS_DISABLE  1
#define INT_VCONNDIS_DISABLE (1 << 1)
void tusb422_stop_vbus_discharge(unsigned int port)
{
	// Stop VBUS discharge.
	tcpc_write8(port, TUSB422_REG_VBUS_VCONN_CTRL, INT_VBUSDIS_DISABLE);
	return;
}

void tusb422_set_vconn_discharge_enable(unsigned int port, bool enable)
{
	if (enable)
	{
		// Clear VCONN discharge disable.
		tcpc_write8(port, TUSB422_REG_VBUS_VCONN_CTRL, 0);
	}
	else
	{
		// Set VCONN discharge disable.
		tcpc_write8(port, TUSB422_REG_VBUS_VCONN_CTRL, INT_VCONNDIS_DISABLE);
	}

	return;

}
