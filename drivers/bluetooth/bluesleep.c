/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.


 * Copyright (C) 2006-2007 - Motorola
 * Copyright (c) 2008-2010, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013, LGE Inc.
 * Copyright (C) 2009-2014 Broadcom Corporation
 * Copyright (C) 2015 Sony Mobile Communications Inc.
 * Copyright (c) 2014, HTC Corporation.

 * Date         Author           Comment
 * -----------  --------------   --------------------------------
 * 2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
 *                               Sleep-Mode Protocol from the Host side
 *  2006-Sep-08  Motorola        Added workqueue for handling sleep work.
 *  2007-Jan-24  Motorola        Added mbm_handle_ioi() call to ISR.
 *  2009-Aug-10  Motorola        Changed "add_timer" to "mod_timer" to solve
 *                               race when flurry of queued work comes in.
 */

#ifndef pr_fmt
#define pr_fmt(fmt)	"Bluetooth: %s: " fmt, __func__
#endif

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/serial_core.h>
#include <linux/platform_data/msm_serial_hs.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"

#define BT_SLEEP_DBG
#ifndef BT_SLEEP_DBG
#define BT_DBG(fmt, arg...)
#endif

/*
 * Defines
 */

#define VERSION		"1.1"
#define PROC_DIR	"bluetooth/sleep"

#define POLARITY_LOW 0
#define POLARITY_HIGH 1

#define BT_UART_PORT_ID 0

/* enable/disable wake-on-bluetooth */
#define BT_ENABLE_IRQ_WAKE 1

#define BT_BLUEDROID_SUPPORT 1

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 1,
	DEBUG_BTWAKE = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
};

static int debug_mask = 0; // DEBUG_USER_STATE | DEBUG_SUSPEND | DEBUG_BTWAKE | DEBUG_VERBOSE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct bluesleep_info {
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	struct uart_port *uport;
	struct wake_lock wake_lock;
	int irq_polarity;
	int has_ext_wake;
	struct mutex state_mutex; /* mutex to guarantee hsuart_power when LPM enabled */
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_hsuart_clk_check()     schedule_delayed_work(&sleep_workqueue, msecs_to_jiffies(10))

/* 1 second timeout */
#define TX_TIMER_INTERVAL  1

/* state variable names and bit positions */
#define BT_PROTO		0x01    // Set / unset when start / stop Sleep-Mode Protocol
#define BT_TXDATA		0x02    // There is incoming data host/stack wants to send out
#define BT_ASLEEP		0x04    // Set / unset when UART deactivated / activated
#define BT_EXT_WAKE		0x08    // same with BT_WAKE PIN
#define BT_SUSPEND		0x10
#define BT_ASLEEPING	0x20    // Set / unset when deactivating UART / UART deactivated

#if BT_BLUEDROID_SUPPORT
static bool has_lpm_enabled = false;
static int bluesleep_reset_lpm_internal(void);
#else
/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;
#endif

/* for rfkill driver to notify BT power state */
static bool bt_pwr_enabled = false;

static struct platform_device *bluesleep_uart_dev;
static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */
#if !BT_BLUEDROID_SUPPORT
static int bluesleep_hci_event(struct notifier_block *this,
			unsigned long event, void *data);
#endif
static int bluesleep_start(void);
static void bluesleep_stop(void);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Transmission timer */
static void bluesleep_tx_timer_expire(unsigned long data);
static DEFINE_TIMER(tx_timer, bluesleep_tx_timer_expire, 0, 0);

/** Lock for state transitions */
static spinlock_t rw_lock;

#if !BT_BLUEDROID_SUPPORT
/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
	.notifier_call = bluesleep_hci_event,
};
#endif

/* CLK state sync from uart driver */
enum msm_hs_states {
	MSM_HS_PORT_OFF,       /* port not in use */
	MSM_HS_PORT_ON,         /*port in use*/
};


struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/** extern functions */
extern unsigned int msm_hs_tx_empty_brcmbt(struct uart_port *uport);
extern void msm_hs_request_clock_off_brcmbt(struct uart_port *uport);
extern void msm_hs_request_clock_on_brcmbt(struct uart_port *uport);
extern struct uart_port *msm_hs_get_uart_port_brcmbt(int port_index);
extern void msm_hs_set_mctrl_brcmbt(struct uart_port *uport,
				    unsigned int mctrl);
extern int msm_hs_uart_get_clk_state(void);

/*
 * Local functions
 */

static void hsuart_power(int on)
{
	// not Sleep-Mode Protocol and LPM disabled
	if (!test_bit(BT_PROTO, &flags) && !has_lpm_enabled) {
		pr_err("not bluesleep (0x%lx)\n", flags);
		return;
	}

	if (test_bit(BT_SUSPEND, &flags)) {
		pr_info("suspend already\n");
		return;
	}

	if (!bt_pwr_enabled)
		pr_warn("control UART under bt off\n");

	if (bsi->uport == NULL) {
		pr_err("NULL UART\n");
		return;
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("(%d)+\n", on);

	if (on) {
		msm_hs_request_clock_on_brcmbt(bsi->uport);
		msm_hs_set_mctrl_brcmbt(bsi->uport, TIOCM_RTS);
	} else {
		msm_hs_set_mctrl_brcmbt(bsi->uport, 0);
		msm_hs_request_clock_off_brcmbt(bsi->uport);
	}
	pr_devel("(%d)-\n", on);
}
/**
 *  Called from rfkill to notify BT power state
 */
void bluesleep_set_bt_pwr_state(int on)
{
	if (on) {
		bt_pwr_enabled = true;
	} else {
		bt_pwr_enabled = false;
#if BT_BLUEDROID_SUPPORT
		bluesleep_reset_lpm_internal();
#endif
	}
}
EXPORT_SYMBOL(bluesleep_set_bt_pwr_state);

/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
int bluesleep_can_sleep(void)
{
	/* check if WAKE_BT_GPIO and BT_WAKE_GPIO are both deasserted */
	return ((gpio_get_value(bsi->host_wake) != bsi->irq_polarity) &&
		(test_bit(BT_EXT_WAKE, &flags)) &&  // current Tx is all sent out
		(!test_bit(BT_TXDATA, &flags)) &&   // no incoming Tx from host/stack
		(bsi->uport != NULL));
}

void bluesleep_sleep_wakeup(void)
{
	static int clk_retry = 0;

	mutex_lock(&bsi->state_mutex);
	if (test_bit(BT_ASLEEPING, &flags) || test_bit(BT_ASLEEP, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("waking up...\n");

		wake_lock(&bsi->wake_lock);

		/* Don't access uart while state is REQUEST_OFF, wait at most 100 ms */
		if ((test_bit(BT_ASLEEPING, &flags)) && clk_retry < 10) {

			pr_info("not access UART when REQUEST_OFF retry:%d\n", clk_retry);
			clk_retry++;

			/* Workqueue to wakeup case so check clk state latter (about 10ms) */
			bluesleep_hsuart_clk_check();
		} else {
			if (clk_retry != 0)
				pr_info("clk state changed retry:%d\n", clk_retry);
			clk_retry = 0;
			/* Start the timer */
			mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
			if (debug_mask & DEBUG_BTWAKE)
				pr_devel("BT WAKE: set to wake\n");
			if (bsi->has_ext_wake == 1)
				gpio_set_value(bsi->ext_wake, 0);
			clear_bit(BT_EXT_WAKE, &flags);
			clear_bit(BT_ASLEEP, &flags);
			/* Activating UART */
			hsuart_power(1);
		}
	} else if (test_bit(BT_EXT_WAKE, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("waking up without power up UART\n");
		wake_lock(&bsi->wake_lock);
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (debug_mask & DEBUG_BTWAKE)
			pr_devel("BT WAKE: set to wake\n");
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
	}
	mutex_unlock(&bsi->state_mutex);
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			pr_info("already asleep (0x%lx)\n", flags);
			return;
		}

		/* check uart port open/close state */
		if (msm_hs_uart_get_clk_state() == MSM_HS_PORT_OFF) {
			pr_info("UART port already off and stop lpm\n");
#if BT_BLUEDROID_SUPPORT
			bluesleep_reset_lpm_internal();
#endif
			return;
		}

		mutex_lock(&bsi->state_mutex);
		set_bit(BT_ASLEEPING, &flags);
		if (msm_hs_tx_empty_brcmbt(bsi->uport)) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("going to sleep\n");
			set_bit(BT_ASLEEP, &flags);
			/* Deactivating UART */
			hsuart_power(0);
			/* UART clk is not turned off immediately. Release
			 * wakelock after 500 ms.
			 */
			wake_lock_timeout(&bsi->wake_lock, HZ / 2);
			clear_bit(BT_ASLEEPING, &flags);
		} else {
			clear_bit(BT_ASLEEPING, &flags);
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("UART tx empty false\n");
			mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		}
		mutex_unlock(&bsi->state_mutex);
	} else if (test_bit(BT_EXT_WAKE, &flags)
			&& !test_bit(BT_ASLEEP, &flags)) {
		// still Rx and UART activated
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (debug_mask & DEBUG_BTWAKE)
			pr_devel("BT WAKE: keep waking\n");
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
	} else {
		//if (debug_mask & DEBUG_SUSPEND)
		//	pr_info("going to wake up anyway...\n");
		bluesleep_sleep_wakeup();
	}
}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
	//if (debug_mask & DEBUG_SUSPEND)
	//	pr_info("hostwake line change\n");

	spin_lock(&rw_lock);
	if ((gpio_get_value(bsi->host_wake) == bsi->irq_polarity))
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
static void bluesleep_outgoing_data(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	/* log data passing by */
	set_bit(BT_TXDATA, &flags);

	/* if the tx side is sleeping... */
	if (test_bit(BT_EXT_WAKE, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("wake up the sleeping Tx\n");
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		bluesleep_sleep_wakeup();
	} else
		spin_unlock_irqrestore(&rw_lock, irq_flags);
}

#if BT_BLUEDROID_SUPPORT
static int bluesleep_lpm_enable (int en)
{
	mutex_lock(&bsi->state_mutex);
	if (!en) {
		/* HCI_DEV_UNREG */
		bluesleep_stop();
		has_lpm_enabled = false;
		bsi->uport = NULL;
	} else {
		/* HCI_DEV_REG */
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			bsi->uport = msm_hs_get_uart_port_brcmbt(BT_UART_PORT_ID);
			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
	}
	mutex_unlock(&bsi->state_mutex);
	return 0;
}

static int bluesleep_reset_lpm_internal()
{
	if (has_lpm_enabled) {
		pr_warn("no bluesleep_write_proc_lpm\n");
		bluesleep_lpm_enable(0);
	}
	return 0;
}

static ssize_t bluesleep_read_proc_lpm(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return 0;
}

static ssize_t bluesleep_write_proc_lpm(struct file *file, const char __user *input,
				size_t size, loff_t *ppos)
{
	char b = '1';

	if (size < 1) {
		pr_info("wrong size: %ld\n", size);
		return -EINVAL;
	}

	if (copy_from_user(&b, input, 1)) {
		pr_info("copy_from_user failed\n");
		return -EFAULT;
	}

	pr_warn("%c\n", b);

	if (b == '0') {
		bluesleep_lpm_enable(0);
	} else {
		bluesleep_lpm_enable(1);
	}

	return size;
}

static ssize_t bluesleep_read_proc_btwrite(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return 0;
}

static ssize_t bluesleep_write_proc_btwrite(struct file *file, const char __user *input,
				size_t size, loff_t *ppos)
{
	char b = '0';
	int i = 0;

	if (size < 1) {
		pr_info("wrong size: %ld\n", size);
		return -EINVAL;
	}

	if (copy_from_user(&b, input, 1)) {
		pr_info("copy_from_user failed\n");
		return -EFAULT;
	}

	pr_info("%c\n", b);

	/* HCI_DEV_WRITE */
	if (b != '0') {
		bluesleep_outgoing_data();

		/* make sure clk is on before return */
		while (i <= 20) {
			if (!test_bit(BT_ASLEEPING, &flags)) {
				if (i != 0)
					pr_info("clk ready count:%d\n", i);
				break;
			}
			if ((i % 5) == 0)
				pr_info("clk not yet on count:%d\n", i);
			i++;
			msleep(5);
		}
	}

	return size;
}
#else
/**
 * Handles HCI device events.
 * @param this Not used.
 * @param event The event that occurred.
 * @param data The HCI device associated with the event.
 * @return <code>NOTIFY_DONE</code>.
 */
static int bluesleep_hci_event(struct notifier_block *this,
				unsigned long event, void *data)
{
	struct hci_dev *hdev = (struct hci_dev *) data;
	struct hci_uart *hu;
	struct uart_state *state;

	if (!hdev)
		return NOTIFY_DONE;

	switch (event) {
	case HCI_DEV_REG:
		if (!bluesleep_hdev) {
			bluesleep_hdev = hdev;
			hu  = (struct hci_uart *) hdev->driver_data;
			state = (struct uart_state *) hu->tty->driver_data;
			bsi->uport = state->uart_port;
			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
		break;
	case HCI_DEV_UNREG:
		bluesleep_stop();
		bluesleep_hdev = NULL;
		bsi->uport = NULL;
		/* if bluetooth stopped, stop bluesleep also */
		break;
	case HCI_DEV_WRITE:
		bluesleep_outgoing_data();
		break;
	}

	return NOTIFY_DONE;
}
#endif

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);
	if (debug_mask & DEBUG_VERBOSE)
		pr_devel("Tx timer expired\n");

	/* were we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("Tx has been idle\n");
		if (debug_mask & DEBUG_BTWAKE)
			pr_devel("BT WAKE: set to sleep\n");
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);
		set_bit(BT_EXT_WAKE, &flags);
		bluesleep_tx_idle();
	} else {
		/* a) UART is writing data to chip if BT_EXT_WAKE pin activated, or
		   b) BT_TXDATA indicates host/stack starts writing data to UART.
		   This function is for a) only.
		 */
		if (debug_mask & DEBUG_SUSPEND)
			pr_devel("Tx data during last period\n");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag */
	clear_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
       if (test_bit(BT_ASLEEPING, &flags) || test_bit(BT_ASLEEP, &flags)) {
          pr_info("hostwake_isr\n");
       }
	/* schedule a tasklet to handle the change in the host wake line */
	tasklet_schedule(&hostwake_task);
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
	int retval;
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (test_bit(BT_PROTO, &flags)) {
		pr_err("already (0x%lx)\n", flags);
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return 0;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (!atomic_dec_and_test(&open_count)) {
		atomic_inc(&open_count);
		return -EBUSY;
	}

	/* start the timer */
	mod_timer(&tx_timer, jiffies + (5*TX_TIMER_INTERVAL*HZ));
	//This time UART should already wake up. Mask it.
	//after timeout bluesleep will auto clock off UART.
	//hsuart_power(1);

	/* assert BT_WAKE */
	if (debug_mask & DEBUG_BTWAKE)
		pr_devel("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
#if BT_ENABLE_IRQ_WAKE
	retval = enable_irq_wake(bsi->host_wake_irq);
	if (retval < 0) {
		pr_err("Couldn't enable BT_HOST_WAKE as wakeup interrupt\n");
		goto fail;
	}
#endif
	set_bit(BT_PROTO, &flags);
	wake_lock(&bsi->wake_lock);
	return 0;
fail:
	del_timer(&tx_timer);
	atomic_inc(&open_count);

	return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		pr_err("already (0x%lx)\n", flags);
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	/* assert BT_WAKE */
	if (debug_mask & DEBUG_BTWAKE)
		pr_devel("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		hsuart_power(1);
	}

	atomic_inc(&open_count);

#if BT_ENABLE_IRQ_WAKE
	if (disable_irq_wake(bsi->host_wake_irq))
		pr_err("Couldn't disable hostwake IRQ wakeup mode\n");
#endif
	wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}
/**
 * Read the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the
 * pin is high, 0 otherwise.
 */
static ssize_t bluepower_read_proc_btwake(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return 0;
}

/**
 * Write the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 */
static ssize_t bluepower_write_proc_btwake(struct file *file, const char __user *input,
				size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return size;
}

/**
 * Read the <code>BT_HOST_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the pin
 * is high, 0 otherwise.
 */
static ssize_t bluepower_read_proc_hostwake(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return 0;
}

/**
 * Read the low-power status of the Host via the proc interface.
 * When this function returns, <code>page</code> contains a 1 if the Host
 * is asleep, 0 otherwise.
 */
static ssize_t bluesleep_read_proc_asleep(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return 0;
}

/**
 * Read the low-power protocol being used by the Host via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the Host
 * is using the Sleep Mode Protocol, 0 otherwise.
 */
static ssize_t bluesleep_read_proc_proto(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	pr_info("not supported\n");
	return 0;
}

/**
 * Modify the low-power protocol used by the Host via the proc interface.
 */
static ssize_t bluesleep_write_proc_proto(struct file *file, const char __user *input,
				size_t size, loff_t *ppos)
{
	pr_info("not supported\n");

/*
	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&proto, buffer, 1))
		return -EFAULT;

	if (proto == '0')
		bluesleep_stop();
	else
		bluesleep_start();
*/

	/* claim that we wrote everything */
	return size;
}

void bluesleep_setup_uart_port(struct platform_device *uart_dev)
{
	bluesleep_uart_dev = uart_dev;
}

static int bluesleep_populate_dt_pinfo(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int tmp;

	tmp = of_get_named_gpio(np, "bcm,bt_host_wake", 0);
	if (tmp < 0) {
		pr_err("couldn't find host_wake gpio\n");
		return -ENODEV;
	}
	bsi->host_wake = tmp;

	tmp = of_get_named_gpio(np, "bcm,bt_wake_dev", 0);
	if (tmp < 0)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake)
		bsi->ext_wake = tmp;

	pr_info("bt_host_wake %d, bt_ext_wake %d\n",
			bsi->host_wake,
			bsi->ext_wake);
	return 0;
}

static int bluesleep_populate_pinfo(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		pr_err("couldn't find host_wake gpio\n");
		return -ENODEV;
	}
	bsi->host_wake = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake)
		bsi->ext_wake = res->start;

	return 0;
}

static int bluesleep_probe(struct platform_device *pdev)
{
//	int res;
	int ret;

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		ret = bluesleep_populate_dt_pinfo(pdev);
		if (ret < 0) {
			pr_err("couldn't populate info from dt\n");
			return ret;
		}
	} else {
		ret = bluesleep_populate_pinfo(pdev);
		if (ret < 0) {
			pr_err("couldn't populate info\n");
			return ret;
		}
	}

	/* configure host_wake as input */
	ret = gpio_request_one(bsi->host_wake, GPIOF_IN, "bt_host_wake");
	if (ret < 0) {
		pr_err("failed to configure input"
				" direction for GPIO %d, error %d\n",
				bsi->host_wake, ret);
		goto free_bsi;
	}

	if (debug_mask & DEBUG_BTWAKE)
		pr_devel("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake) {
		/* configure ext_wake as output mode*/
		ret = gpio_request_one(bsi->ext_wake,
				GPIOF_OUT_INIT_LOW, "bt_ext_wake");
		if (ret < 0) {
			pr_err("failed to configure output"
				" direction for GPIO %d, error %d\n",
				  bsi->ext_wake, ret);
			goto free_bt_host_wake;
		}
	}
	clear_bit(BT_EXT_WAKE, &flags);
#if 0
	res = platform_get_irq_byname(pdev, "host_wake");
	if (!res) {
		pr_err("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_host_wake;
	}
	bsi->host_wake_irq = res;
#else
        bsi->host_wake_irq = gpio_to_irq(bsi->host_wake);
#endif
	if (bsi->host_wake_irq < 0) {
		pr_err("couldn't find host_wake irq res\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	bsi->irq_polarity = POLARITY_LOW;/*low edge (falling edge)*/

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");
	clear_bit(BT_SUSPEND, &flags);

	pr_info("host_wake_irq %d, polarity %d\n",
			bsi->host_wake_irq,
			bsi->irq_polarity);

	/* Initialize spinlock befor request IRQ */
	spin_lock_init(&rw_lock);

	mutex_init(&bsi->state_mutex);

	/* initialize host wake tasklet before request IRQ */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	/* Request IRQ */
	ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
			IRQF_DISABLED | IRQF_TRIGGER_FALLING,
			"bluetooth hostwake", NULL);
	if (ret  < 0) {
		pr_err("Couldn't acquire BT_HOST_WAKE IRQ\n");
		goto free_bt_ext_wake;
	}

	return 0;

free_bt_ext_wake:
	gpio_free(bsi->ext_wake);
	mutex_destroy(&bsi->state_mutex);
free_bt_host_wake:
	gpio_free(bsi->host_wake);
free_bsi:
	kfree(bsi);
	return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	free_irq(bsi->host_wake_irq, NULL);
	gpio_free(bsi->host_wake);
	gpio_free(bsi->ext_wake);
	wake_lock_destroy(&bsi->wake_lock);
	mutex_destroy(&bsi->state_mutex);
	kfree(bsi);
	return 0;
}


static int bluesleep_resume(struct platform_device *pdev)
{
	if (test_bit(BT_SUSPEND, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("bluesleep resuming...\n");
		if ((bsi->uport != NULL) &&
			(gpio_get_value(bsi->host_wake) == bsi->irq_polarity)) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("from BT event\n");

			if (!bt_pwr_enabled)
				pr_info("control UART under bt off\n");
			msm_hs_request_clock_on_brcmbt(bsi->uport);
			msm_hs_set_mctrl_brcmbt(bsi->uport, TIOCM_RTS);
		}
		clear_bit(BT_SUSPEND, &flags);
	}
	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("bluesleep suspending...\n");
	set_bit(BT_SUSPEND, &flags);
	return 0;
}

static struct of_device_id bluesleep_match_table[] = {
	{ .compatible = "bcm,bluesleep" },
	{}
};

static struct platform_driver bluesleep_driver = {
	.probe = bluesleep_probe,
	.remove = bluesleep_remove,
	.suspend = bluesleep_suspend,
	.resume = bluesleep_resume,
	.driver = {
		.name = "bluesleep",
		.owner = THIS_MODULE,
		.of_match_table = bluesleep_match_table,
	},
};


static const struct file_operations bluesleep_proc_fops_btwake = {
	.read = bluepower_read_proc_btwake,
	.write = bluepower_write_proc_btwake,
};

static const struct file_operations bluesleep_proc_fops_hostwake = {
	.read = bluepower_read_proc_hostwake,
};

static const struct file_operations bluesleep_proc_fops_proto = {
	.read = bluesleep_read_proc_proto,
	.write = bluesleep_write_proc_proto,
};

static const struct file_operations bluesleep_proc_fops_asleep = {
	.read = bluesleep_read_proc_asleep,
};

#if BT_BLUEDROID_SUPPORT
static const struct file_operations bluesleep_proc_fops_lpm = {
	.read = bluesleep_read_proc_lpm,
	.write = bluesleep_write_proc_lpm,
};

static const struct file_operations bluesleep_proc_fops_btwrite = {
	.read = bluesleep_read_proc_btwrite,
	.write = bluesleep_write_proc_btwrite,
};
#endif

/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	pr_info("BlueSleep Mode Driver Ver %s\n", VERSION);

	retval = platform_driver_register(&bluesleep_driver);
	if (retval)
		return retval;

	if (bsi == NULL)
		return 0;

#if !BT_BLUEDROID_SUPPORT
	bluesleep_hdev = NULL;
#endif

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		pr_err("Unable to create /proc/bluetooth directory\n");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		pr_err("Unable to create /proc/%s directory\n", PROC_DIR);
		return -ENOMEM;
	}

#if BT_BLUEDROID_SUPPORT
	/* read/write proc entries */
	ent = proc_create("lpm", S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH,
			sleep_dir, &bluesleep_proc_fops_lpm);
	if (ent == NULL) {
		pr_err("Unable to create /proc/%s/lpm entry\n", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create("btwrite", S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH,
			sleep_dir, &bluesleep_proc_fops_btwrite);
	if (ent == NULL) {
		pr_err("Unable to create /proc/%s/btwrite entry\n", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
#endif

	flags = 0; /* clear all status bits */

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

#if !BT_BLUEDROID_SUPPORT
	hci_register_notifier(&hci_event_nblock);
#endif

	pr_info("BlueSleep Mode Driver Initialized\n");

	return 0;

fail:
#if BT_BLUEDROID_SUPPORT
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
#endif
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	if (bsi == NULL)
		return;

	/* assert bt wake */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
	if (test_bit(BT_PROTO, &flags)) {
		if (disable_irq_wake(bsi->host_wake_irq))
			pr_err("Couldn't disable hostwake IRQ wakeup mode\n");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&tx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(1);
	}

#if !BT_BLUEDROID_SUPPORT
	hci_unregister_notifier(&hci_event_nblock);
#endif
	platform_driver_unregister(&bluesleep_driver);

#if BT_BLUEDROID_SUPPORT
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
#endif
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
