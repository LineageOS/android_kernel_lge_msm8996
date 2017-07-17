/*
 * drivers/media/radio/si470x/radio-si470x-i2c.c
 *
 * I2C driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 * Copyright (c) 2009 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/* driver definitions */
#define DRIVER_CARD "Silicon Labs Si470x FM Radio Receiver"
#define DRIVER_DESC "I2C radio driver for Si470x FM Radio Receivers"
#define DRIVER_VERSION "1.0.2"

/* kernel includes */
#include <linux/version.h>
#include <linux/init.h>         /* Initdata                     */
#include <linux/delay.h>        /* udelay                       */
#include <linux/uaccess.h>      /* copy to/from user            */
#include <linux/kfifo.h>        /* lock free circular buffer    */
#include <linux/param.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include "radio-si470x.h"


/* I2C Device ID List */
static const struct i2c_device_id si470x_i2c_id[] = {
	/* Generic Entry */
	{ "si470x", 0 },
	/* Terminating entry */
	{ }
};
MODULE_DEVICE_TABLE(i2c, si470x_i2c_id);



/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Radio Nr */
static int radio_nr = -1;
module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio Nr");

/* RDS buffer blocks */
static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0444);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

/* RDS maximum block errors */
static unsigned short max_rds_errors = 1;
/* 0 means   0  errors requiring correction */
/* 1 means 1-2  errors requiring correction (used by original USBRadio.exe) */
/* 2 means 3-5  errors requiring correction */
/* 3 means   6+ errors or errors in checkword, correction not possible */
module_param(max_rds_errors, ushort, 0644);
MODULE_PARM_DESC(max_rds_errors, "RDS maximum block errors: *1*");

/*
 * si470x_get_register - read register
 */
int si470x_get_register(struct si470x_device *radio, int regnr)
{
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{
			.addr = radio->client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(u16) * READ_REG_NUM,
			.buf = (void *)buf
		},
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	radio->registers[regnr] = __be16_to_cpu(buf[READ_INDEX(regnr)]);

	return 0;
}


/*
 * si470x_set_register - write register
 */
int si470x_set_register(struct si470x_device *radio, int regnr)
{
	int i;
	u16 buf[WRITE_REG_NUM];
	struct i2c_msg msgs[1] = {
		{
			.addr = radio->client->addr,
			.len = sizeof(u16) * WRITE_REG_NUM,
			.buf = (void *)buf
		},
	};

	for (i = 0; i < WRITE_REG_NUM; i++)
		buf[i] = __cpu_to_be16(radio->registers[WRITE_INDEX(i)]);

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	return 0;
}



/**************************************************************************
 * General Driver Functions - ENTIRE REGISTERS
 **************************************************************************/

/*
 * si470x_get_all_registers - read entire registers
 */
int si470x_get_all_registers(struct si470x_device *radio)
{
	int i;
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{
			.addr = radio->client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(u16) * READ_REG_NUM,
			.buf = (void *)buf
		},
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	for (i = 0; i < READ_REG_NUM; i++)
		radio->registers[i] = __be16_to_cpu(buf[READ_INDEX(i)]);

	return 0;
}


/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/
int cancel_seek(struct si470x_device *radio)
{
	int retval = 0;

	pr_info("%s enter\n",__func__);
	mutex_lock(&radio->lock);

	/* stop seeking */
	radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
	retval = si470x_set_register(radio, POWERCFG);

	mutex_unlock(&radio->lock);
	radio->is_search_cancelled = true;

	return retval;

}


void si470x_search(struct si470x_device *radio, bool on)
{
	int current_freq_khz;

	current_freq_khz = radio->tuned_freq_khz;

	if (on) {
		pr_info("%s: Queuing the work onto scan work q\n", __func__);
		queue_delayed_work(radio->wqueue_scan, &radio->work_scan,
					msecs_to_jiffies(10));
	} else {
		cancel_seek(radio);
		si470x_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
	}
}

/*
 * si470x_vidioc_querycap - query device capabilities
 */
int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	pr_info("%s enter\n" , __func__);
	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	capability->device_caps = V4L2_CAP_HW_FREQ_SEEK | V4L2_CAP_READWRITE |
		V4L2_CAP_TUNER | V4L2_CAP_RADIO | V4L2_CAP_RDS_CAPTURE;
	capability->capabilities = capability->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

/*
 * si470x_i2c_interrupt - interrupt handler
 */
static void si470x_i2c_interrupt_handler(struct si470x_device *radio)
{
	unsigned char regnr;
	int retval = 0;
	//struct kfifo *data_b;

	pr_info("%s enter\n",__func__);
	/* check Seek/Tune Complete */
	retval = si470x_get_register(radio, STATUSRSSI);
	if (retval < 0)
		goto end;

	if (radio->registers[STATUSRSSI] & STATUSRSSI_STC)
		complete(&radio->completion);
	/* safety checks */
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		goto end;

	/* Update RDS registers */
	for (regnr = 1; regnr < RDS_REGISTER_NUM; regnr++) {
		retval = si470x_get_register(radio, STATUSRSSI + regnr);
		if (retval < 0)
			goto end;
	}

	/* get rds blocks */
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_RDSR) == 0){
		/* No RDS group ready, better luck next time */
		pr_err("%s No RDS group ready\n",__func__);
		goto end;
	} else {
		pr_info("%s start rds handler\n",__func__);
		schedule_work(&radio->rds_worker);
	}
end:
	pr_info("%s exit :%d\n",__func__, retval);
	return;
}

static irqreturn_t si470x_isr(int irq, void *dev_id)
{
	struct si470x_device *radio = dev_id;
	/*
	 * The call to queue_delayed_work ensures that a minimum delay
	 * (in jiffies) passes before the work is actually executed. The return
	 * value from the function is nonzero if the work_struct was actually
	 * added to queue (otherwise, it may have already been there and will
	 * not be added a second time).
	 */

	queue_delayed_work(radio->wqueue, &radio->work,
				msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static void si470x_handler(struct work_struct *work)
{
	struct si470x_device *radio;

	radio = container_of(work, struct si470x_device, work.work);

	si470x_i2c_interrupt_handler(radio);
}

static int si470x_pinctrl_select(struct si470x_device *radio, bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? radio->gpio_state_active
			: radio->gpio_state_suspend;

	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(radio->fm_pinctrl, pins_state);
		if (ret) {
			pr_err("%s: cannot set pin state\n", __func__);
			return ret;
		}
	} else {
		pr_err("%s: not a valid %s pin state\n", __func__,
				on ? "pmx_fm_active" : "pmx_fm_suspend");
	}

	return 0;
}

static int si470x_reg_cfg(struct si470x_device *radio, bool on)
{
	int rc = 0;
	struct si470x_vreg_data *vreg;

	vreg = radio->dreg;

	pr_info("%s enter : %d \n",__func__, on);

	if (!vreg) {
		pr_err("In %s, dreg is NULL\n", __func__);
		return rc;
	}

	if (on) {
		pr_info("%s vreg is : %s\n",__func__, vreg->name);
		if (vreg->set_voltage_sup) {
			rc = regulator_set_voltage(vreg->reg,
						vreg->low_vol_level,
						vreg->high_vol_level);
			if (rc < 0) {
				pr_err("set_vol(%s) fail %d\n", vreg->name, rc);
				return rc;
			}
		}

		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			pr_err("reg enable(%s) failed.rc=%d\n", vreg->name, rc);
			if (vreg->set_voltage_sup) {
				regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
			}
			return rc;
		}
			vreg->is_enabled = true;
	} else {
		pr_info("%s vreg is off: %s\n",__func__, vreg->name);
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			pr_err("reg disable(%s) fail. rc=%d\n", vreg->name, rc);
			return rc;
		}
		vreg->is_enabled = false;

		if (vreg->set_voltage_sup) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
			if (rc < 0) {
				pr_err("set_vol(%s) fail %d\n", vreg->name, rc);
				return rc;
			}
		}
	}
	return rc;
}

static int si470x_configure_gpios(struct si470x_device *radio, bool on)
{
	int rc = 0;
	int fm_reset_gpio = radio->reset_gpio;
	int fm_int_gpio = radio->int_gpio;

	if (on) {
		/*
		 * GPO/Interrupt gpio configuration.
		 * Keep the GPO to low till device comes out of reset.
		 */
		rc = gpio_direction_output(fm_int_gpio, 0);
		if (rc) {
			pr_err("%s unable to set the gpio %d direction(%d)\n",
			__func__,fm_int_gpio, rc);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		/*
		 * Reset pin configuration.
		 * write "0'' to make sure the chip is in reset.
		 */
		rc = gpio_direction_output(fm_reset_gpio, 0);
		if (rc) {
			pr_err("%s Unable to set direction\n",__func__);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);
		/* write "1" to bring the chip out of reset.*/
		rc = gpio_direction_output(fm_reset_gpio, 1);
		if (rc) {
			pr_err("%s Unable to set direction\n",__func__);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		rc = gpio_direction_input(fm_int_gpio);
		if (rc) {
			pr_err("%s unable to set the gpio %d direction(%d)\n",
					__func__,fm_int_gpio, rc);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

	} else {
		/*Turn OFF sequence */
		gpio_set_value(fm_reset_gpio, 0);

		rc = gpio_direction_input(fm_reset_gpio);
		if (rc)
			pr_err("Unable to set direction\n");
		/* Wait for some time for the value to take effect. */
		msleep(100);
	}
	return rc;
}


static int si470x_power_cfg(struct si470x_device *radio, bool on)
{
	int rc = 0;

	pr_info("%s enter : %d\n",__func__, on);

	if (on) {
		/* Turn ON sequence */
		rc = si470x_reg_cfg(radio, on);
		if (rc < 0) {
			pr_err("In %s, dreg cfg failed %x\n", __func__, rc);
			return rc;
		}

		/* If pinctrl is supported, select active state */
		if (radio->fm_pinctrl) {
			rc = si470x_pinctrl_select(radio, true);
			if (rc)
				pr_err("%s: error setting active pin state\n",
								__func__);
		}

		rc = si470x_configure_gpios(radio, on);
		if (rc < 0) {
			pr_err("%s fm_power gpio config failed\n",__func__);
			//si470x_reg_cfg(radio, false);
			return rc;
		}
	} else {
		/* Turn OFF sequence */
		rc = si470x_configure_gpios(radio, on);
		if (rc < 0)
			pr_err("%s fm_power gpio config failed\n",__func__);

		/* If pinctrl is supported, select suspend state */
		if (radio->fm_pinctrl) {
			rc = si470x_pinctrl_select(radio, false);
			if (rc)
				pr_err("%s: error setting suspend pin state\n",
								__func__);
		}
	/*	rc = si470x_reg_cfg(radio, on);
		if (rc < 0)
			pr_err("In %s, dreg cfg failed %x\n", __func__, rc); */
	}

	return rc;
}

static int si470x_pinctrl_init(struct si470x_device *radio)
{
	int retval = 0;

	radio->fm_pinctrl = devm_pinctrl_get(&radio->client->dev);
	if (IS_ERR_OR_NULL(radio->fm_pinctrl)) {
		pr_err("%s: target does not use pinctrl\n", __func__);
		retval = PTR_ERR(radio->fm_pinctrl);
		return retval;
	}

	radio->gpio_state_active =
			pinctrl_lookup_state(radio->fm_pinctrl,
						"pmx_fm_active");
	if (IS_ERR_OR_NULL(radio->gpio_state_active)) {
		pr_err("%s: cannot get FM active state\n", __func__);
		retval = PTR_ERR(radio->gpio_state_active);
		goto err_active_state;
	}

	radio->gpio_state_suspend =
				pinctrl_lookup_state(radio->fm_pinctrl,
							"pmx_fm_suspend");
	if (IS_ERR_OR_NULL(radio->gpio_state_suspend)) {
		pr_err("%s: cannot get FM suspend state\n", __func__);
		retval = PTR_ERR(radio->gpio_state_suspend);
		goto err_suspend_state;
	}

	return retval;

err_suspend_state:
	radio->gpio_state_suspend = 0;

err_active_state:
	radio->gpio_state_active = 0;

	return retval;
}

static int silabs_parse_dt(struct device *dev,
			struct si470x_device *radio)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	radio->reset_gpio = of_get_named_gpio(np, "silabs,reset-gpio", 0);
	if (radio->reset_gpio < 0) {
		pr_err("%s silabs-reset-gpio not provided in device tree",__func__);
		return radio->reset_gpio;
	}

	rc = gpio_request(radio->reset_gpio, "fm_rst_gpio_n");
	if (rc) {
		pr_err("%s unable to request gpio %d (%d)\n",__func__,
					radio->reset_gpio, rc);
		return rc;
	}

	radio->int_gpio = of_get_named_gpio(np, "silabs,int-gpio", 0);
	if (radio->int_gpio < 0) {
		pr_err("%s silabs-int-gpio not provided in device tree",__func__);
		rc = radio->int_gpio;
		goto err_int_gpio;
	}

	rc = gpio_request(radio->int_gpio, "silabs_fm_int_n");
	if (rc) {
		pr_err("%s unable to request gpio %d (%d)\n",__func__,
						radio->int_gpio, rc);
		goto err_int_gpio;
	}

	return rc;

err_int_gpio:
	gpio_free(radio->reset_gpio);
	gpio_free(radio->int_gpio);

	return rc;
}

static int silabs_dt_parse_vreg_info(struct device *dev,
			struct si470x_vreg_data *vreg, const char *vreg_name)
{
	int ret = 0;
	u32 vol_suply[2];
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32_array(np, vreg_name, vol_suply, 2);
	if (ret < 0) {
		pr_err("%s Invalid property name\n",__func__);
		ret =  -EINVAL;
	} else {
		vreg->low_vol_level = vol_suply[0];
		vreg->high_vol_level = vol_suply[1];
	}
	return ret;
}

static void si470x_disable_irq(struct si470x_device *radio)
{
	int irq;

	irq = radio->irq;
	disable_irq_wake(irq);
	free_irq(irq, radio);

	cancel_work_sync(&radio->rds_worker);
	flush_workqueue(radio->wqueue_rds);
	cancel_delayed_work_sync(&radio->work);
	flush_workqueue(radio->wqueue);
	cancel_delayed_work_sync(&radio->work_scan);
	flush_workqueue(radio->wqueue_scan);

}

static int si470x_enable_irq(struct si470x_device *radio)
{
	int retval;
	int irq;

	radio->irq = gpio_to_irq(radio->int_gpio);
	irq = radio->irq;

	if (radio->irq < 0) {
		pr_err("%s: gpio_to_irq returned %d\n", __func__, radio->irq);
		goto open_err_req_irq;
	}

	pr_info("%s irq number is = %d\n", __func__,radio->irq);

	retval = request_any_context_irq(radio->irq, si470x_isr,
			IRQF_TRIGGER_FALLING, DRIVER_NAME, radio);

	if (retval < 0) {
		pr_err("%s Couldn't acquire FM gpio %d, retval:%d\n", __func__,radio->irq,retval);
		return retval;
	} else {
		pr_info("%s FM GPIO %d registered\n", __func__, radio->irq);
	}
	retval = enable_irq_wake(irq);
	if (retval < 0) {
		pr_err("Could not wake FM interrupt\n ");
		free_irq(irq , radio);
	}
	return retval;

open_err_req_irq:
	si470x_power_cfg(radio, TURN_OFF);
	si470x_disable_irq(radio);

	return retval;
}

/*
 * si470x_fops_open - file open
 */
int si470x_fops_open(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = v4l2_fh_open(file);
	int version_warning = 0;
	int i = 0;

	pr_info("%s enter\n",__func__);
	if (retval){
		pr_err("%s fail to open v4l2\n", __func__);
		return retval;
	}

	INIT_DELAYED_WORK(&radio->work, si470x_handler);
	INIT_DELAYED_WORK(&radio->work_scan, si470x_scan);
	INIT_WORK(&radio->rds_worker, si470x_rds_handler);

	si470x_power_cfg(radio,TURN_ON);
	if (v4l2_fh_is_singular_file(file)) {
		/* start radio */
		retval = si470x_start(radio);
		if (retval < 0){
			pr_err("%s fail turn on\n",__func__);
			goto done;
		}
		/* enable RDS / STC interrupt */
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDSIEN;
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_STCIEN;
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_GPIO2;
		radio->registers[SYSCONFIG1] |= 0x1 << 2;
		retval = si470x_set_register(radio, SYSCONFIG1);

	}

	/* get device and chip versions */
	if (si470x_get_all_registers(radio) < 0) {
		retval = -EIO;
		pr_err("%s fail to get all registers\n",__func__);
		goto done;
	}

	pr_info("%s DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",__func__,
			radio->registers[DEVICEID], radio->registers[CHIPID]);
	if ((radio->registers[CHIPID] & CHIPID_FIRMWARE) < RADIO_FW_VERSION) {
		pr_err("%s This driver is known to work with "
				"firmware version %d\n", __func__,RADIO_FW_VERSION);
		version_warning = 1;
	}

	/* give out version warning */
	if (version_warning == 1) {
		pr_err("%s If you have some trouble using this driver,\n",__func__);
	}

	si470x_enable_irq(radio);

	/* reset last channel */
	retval = si470x_set_chan(radio,
		radio->registers[CHANNEL] & CHANNEL_CHAN);

	for(i = 0; i < 16; i++ )
		pr_info("%s radio->registers[%d] : %x\n",__func__,i,radio->registers[i]);

done:
	if (retval)
		v4l2_fh_release(file);
	return retval;
}

/*
 * si470x_fops_release - file release
 */
int si470x_fops_release(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);

	pr_info("%s enter \n",__func__);

	if (v4l2_fh_is_singular_file(file))
		/* stop radio */
		si470x_stop(radio);

	si470x_disable_irq(radio);
	si470x_power_cfg(radio,TURN_OFF);

	return v4l2_fh_release(file);
}

/*
 * si470x_i2c_probe - probe for the device
 */
static int si470x_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct si470x_device *radio;
	struct regulator *vreg = NULL;
	int retval = 0;
	int i = 0;
	int kfifo_alloc_rc = 0;

	pr_info("%s enter",__func__);
	vreg = regulator_get(&client->dev, "va");

	if (IS_ERR(vreg)) {
		/*
		 * if analog voltage regulator, VA is not ready yet, return
		 * -EPROBE_DEFER to kernel so that probe will be called at
		 * later point of time.
		 */
		if (PTR_ERR(vreg) == -EPROBE_DEFER) {
			pr_err("In %s, areg probe defer\n", __func__);
			return PTR_ERR(vreg);
		}
	}

	/* private data allocation and initialization */
	radio = kzalloc(sizeof(struct si470x_device), GFP_KERNEL);
	if (!radio) {
		pr_info("%s error radio allocation",__func__);
		retval = -ENOMEM;
		goto err_initial;
	}

	retval = silabs_parse_dt(&client->dev, radio);
	if (retval) {
		pr_err("%s: Parsing DT failed(%d)", __func__, retval);
		regulator_put(vreg);
		kfree(radio);
		return retval;
	}
	radio->client = client;
	radio->band = 0; /* Default to 87.5 - 108 MHz */
	radio->seek_tune_status = 0;
	mutex_init(&radio->lock);
	init_completion(&radio->completion);

	if (!IS_ERR(vreg)) {
		radio->areg = devm_kzalloc(&client->dev,
				sizeof(struct si470x_vreg_data),
				GFP_KERNEL);
		if (!radio->areg) {
			pr_err("%s: allocating memory for areg failed\n",
					__func__);
			regulator_put(vreg);
			kfree(radio);
			return -ENOMEM;
		}

		radio->areg->reg = vreg;
		radio->areg->name = "va";
		radio->areg->is_enabled = 0;
		retval = silabs_dt_parse_vreg_info(&client->dev,
				radio->areg, "silabs,va-supply-voltage");
		if (retval < 0) {
			pr_err("%s: parsing va-supply failed\n", __func__);
			goto mem_alloc_fail;
		}
	}

	vreg = regulator_get(&client->dev, "vdd");
	pr_info("%s regulator_get",__func__);

	if (IS_ERR(vreg)) {
		pr_err("In %s, vdd supply is not provided\n", __func__);
	} else {
		radio->dreg = devm_kzalloc(&client->dev,
				sizeof(struct si470x_vreg_data),
				GFP_KERNEL);
		if (!radio->dreg) {
			pr_err("%s: allocating memory for dreg failed\n",
					__func__);
			retval = -ENOMEM;
			regulator_put(vreg);
			goto mem_alloc_fail;
		}

		radio->dreg->reg = vreg;
		radio->dreg->name = "vdd";
		radio->dreg->is_enabled = 0;
		retval = silabs_dt_parse_vreg_info(&client->dev,
				radio->dreg, "silabs,vdd-supply-voltage");
		if (retval < 0) {
			pr_err("%s: parsing vdd-supply failed\n", __func__);
			goto err_dreg;
		}
	}

	/* Initialize pin control*/
	retval = si470x_pinctrl_init(radio);
	if (retval) {
		pr_err("%s: si470x_pinctrl_init returned %d\n",
							__func__, retval);
		/* if pinctrl is not supported, -EINVAL is returned*/
		if (retval == -EINVAL)
			retval = 0;
	} else {
		pr_info("%s si470x_pinctrl_init success\n",__func__);
	}

	radio->wqueue = NULL;
	radio->wqueue_scan = NULL;
	radio->wqueue_rds = NULL;

	/* video device allocation */
	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		pr_err("radio->videodev is NULL\n");
		goto err_dreg;
	}

	/* initial configuration */
	memcpy(radio->videodev, &si470x_viddev_template,
			sizeof(si470x_viddev_template));
	strlcpy(radio->v4l2_dev.name, DRIVER_NAME,
			sizeof(radio->v4l2_dev.name));
	retval = v4l2_device_register(NULL, &radio->v4l2_dev);
	if (retval) {
		pr_err("%s err v4l2_device_register",__func__);
		goto err_dreg;
	}
	radio->videodev->v4l2_dev = &radio->v4l2_dev;
	video_set_drvdata(radio->videodev, radio);

	/* rds buffer allocation */
	radio->buf_size = rds_buf * 3;
	radio->buffer = kmalloc(radio->buf_size, GFP_KERNEL);
	if (!radio->buffer) {
		retval = -EIO;
		goto err_radio;
	}

	for (i = 0; i < SILABS_FM_BUF_MAX; i++) {
		spin_lock_init(&radio->buf_lock[i]);

		kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE, GFP_KERNEL);

		if (kfifo_alloc_rc != 0) {
			pr_err("%s: failed allocating buffers %d\n",
					__func__, kfifo_alloc_rc);
			retval = -ENOMEM;
			goto err_fifo_alloc;
		}
	}

	init_waitqueue_head(&radio->event_queue);
	/* rds buffer configuration */
	radio->wr_index = 0;
	radio->rd_index = 0;
	init_waitqueue_head(&radio->read_queue);

	/*
	 * Start the worker thread for event handling and register read_int_stat
	 * as worker function
	 */
	radio->wqueue  = create_singlethread_workqueue("sifmradio");

	if (!radio->wqueue) {
		retval = -ENOMEM;
		goto err_wqueue;
	}
	pr_info("%s: creating work q for scan\n", __func__);
	radio->wqueue_scan  = create_singlethread_workqueue("sifmradioscan");

	if (!radio->wqueue_scan) {
		retval = -ENOMEM;
		goto err_wqueue_scan;
	}

	radio->wqueue_rds  = create_singlethread_workqueue("sifmradiords");

	if (!radio->wqueue_rds) {
		retval = -ENOMEM;
		goto err_all;
	}

	/* register video device */
	retval = video_register_device(radio->videodev, VFL_TYPE_RADIO,radio_nr);

	if (retval) {
		dev_warn(&client->dev, "Could not register video device\n");
		goto err_all;
	}
	i2c_set_clientdata(client, radio);

	return 0;

err_all:
	destroy_workqueue(radio->wqueue_rds);
err_wqueue_scan:
	destroy_workqueue(radio->wqueue_scan);
err_wqueue:
	destroy_workqueue(radio->wqueue);
err_fifo_alloc:
	video_device_release(radio->videodev);
err_dreg:
	if (radio->dreg && radio->dreg->reg) {
		regulator_put(radio->dreg->reg);
		devm_kfree(&client->dev, radio->dreg);
	}
mem_alloc_fail:
	if (radio->areg && radio->areg->reg) {
		regulator_put(radio->areg->reg);
		devm_kfree(&client->dev, radio->areg);
	}
err_radio:
	kfree(radio);
err_initial:
	return retval;
}


/*
 * si470x_i2c_remove - remove the device
 */

static int si470x_i2c_remove(struct i2c_client *client)
{
	struct si470x_device *radio = i2c_get_clientdata(client);

	si470x_disable_irq(radio);
	video_unregister_device(radio->videodev);
	kfree(radio);

	return 0;
}

/*
 * si470x_i2c_driver - i2c driver interface
 */
static const struct i2c_device_id i470x_i2c_id[] = {
	{ DRIVER_NAME, 0 },
	{ },
};

static const struct of_device_id silabs_fm_match[] = {
	{.compatible = "silabs,si470x"},
	{}
};

static struct i2c_driver si470x_i2c_driver = {
	.probe  = si470x_i2c_probe,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "si470x",
		.of_match_table = silabs_fm_match,
	},
	.remove  = si470x_i2c_remove,
	.id_table       = si470x_i2c_id,
};

static int __init radio_module_init(void)
{
	return i2c_add_driver(&si470x_i2c_driver);
}
module_init(radio_module_init);

static void __exit radio_module_exit(void)
{
	i2c_del_driver(&si470x_i2c_driver);
}
module_exit(radio_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRIVER_DESC);

