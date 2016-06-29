/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */
#define TS_MODULE "[td4302_rmi_dev]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_td4302.h"

#define CHAR_DEVICE_NAME "rmi"
#define DEVICE_CLASS_NAME "rmidev"
#define SYSFS_FOLDER_NAME "rmidev"
#define DEV_NUMBER 1
#define REG_ADDR_LIMIT 0xFFFF

static ssize_t rmidev_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t rmidev_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t rmidev_sysfs_open_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_release_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_attn_state_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidev_sysfs_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidev_sysfs_pid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_term_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_intr_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidev_sysfs_intr_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_concurrent_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidev_sysfs_concurrent_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

struct rmidev_handle {
	dev_t dev_no;
	pid_t pid;
	unsigned char intr_mask;
	unsigned char *tmpbuf;
	unsigned int tmpbuf_size;
	struct device dev;
	struct device *ts_dev;
	struct synaptics_data *d;
	struct kobject *sysfs_dir;
	struct siginfo interrupt_signal;
	struct siginfo terminate_signal;
	struct task_struct *task;
	void *data;
	bool irq_enabled;
	bool concurrent;
};

struct rmidev_data {
	int ref_count;
	struct cdev main_dev;
	struct class *device_class;
	struct mutex file_mutex;
	struct rmidev_handle *rmi_dev;
};

static struct bin_attribute attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUGO),
	},
	.size = 0,
	.read = rmidev_sysfs_data_show,
	.write = rmidev_sysfs_data_store,
};

static struct device_attribute attrs[] = {
	__ATTR(open, S_IWUSR | S_IWGRP/*S_IWUGO*/,
			NULL,
			rmidev_sysfs_open_store),
	__ATTR(release, S_IWUSR | S_IWGRP/*S_IWUGO*/,
			NULL,
			rmidev_sysfs_release_store),
	__ATTR(attn_state, S_IRUGO,
			rmidev_sysfs_attn_state_show,
			NULL),
	__ATTR(pid, S_IRUGO | S_IWUSR | S_IWGRP/*S_IWUGO*/,
			rmidev_sysfs_pid_show,
			rmidev_sysfs_pid_store),
	__ATTR(term, S_IWUSR | S_IWGRP/*S_IWUGO*/,
			NULL,
			rmidev_sysfs_term_store),
	__ATTR(intr_mask, S_IRUGO | S_IWUSR | S_IWGRP/*S_IWUGO*/,
			rmidev_sysfs_intr_mask_show,
			rmidev_sysfs_intr_mask_store),
	__ATTR(concurrent, S_IRUGO | S_IWUSR | S_IWGRP/*S_IWUGO*/,
			rmidev_sysfs_concurrent_show,
			rmidev_sysfs_concurrent_store),
};

static int rmidev_major_num;

static struct class *rmidev_device_class;

static struct rmidev_handle *rmidev;

DECLARE_COMPLETION(rmidev_remove_complete);

static irqreturn_t rmidev_sysfs_irq(int irq, void *dev_id)
{
	struct touch_core_data *ts = (struct touch_core_data *) dev_id;

	TOUCH_TRACE();

	sysfs_notify(&ts->input->dev.kobj,
			SYSFS_FOLDER_NAME, "attn_state");

	return IRQ_HANDLED;
}

static int rmidev_sysfs_irq_enable(struct device *ts_dev,
		bool enable)
{
	struct touch_core_data *ts = to_touch_core(ts_dev);
	struct synaptics_data *d = to_synaptics_data(ts_dev);
	int retval = 0;
	unsigned char intr_status;
	unsigned long irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
			IRQF_ONESHOT;

	TOUCH_TRACE();

	if (enable) {
		if (rmidev->irq_enabled)
			return retval;

		/* Clear interrupts first */
		retval = synaptics_read(ts_dev,
				(d->f01.dsc.data_base + 1) >> 8,
				d->f01.dsc.data_base + 1,
				&intr_status,
				1);
		if (retval < 0)
			return retval;

		retval = request_threaded_irq(ts->irq, NULL,
				rmidev_sysfs_irq, irq_flags,
				PLATFORM_DRIVER_NAME, ts);
		if (retval < 0) {
			TOUCH_E("%s: Failed to create irq thread\n", __func__);
			return retval;
		}

		enable_irq(ts->irq);
		rmidev->irq_enabled = true;
	} else {
		if (rmidev->irq_enabled) {
			disable_irq(ts->irq);
			free_irq(ts->irq, ts);
			rmidev->irq_enabled = false;
		}
	}

	return retval;
}

static ssize_t rmidev_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
/*	unsigned char intr_status = 0;	*/
	unsigned int length = (unsigned int)count;
	unsigned short address = (unsigned short)pos;

	TOUCH_TRACE();

	if (length > (REG_ADDR_LIMIT - address)) {
		TOUCH_E("%s: Out of register map limit\n", __func__);
		return -EINVAL;
	}

	if (length) {
		retval = synaptics_read(rmidev->ts_dev,
				address >> 8,
				address,
				(unsigned char *)buf,
				length);
		if (retval < 0) {
			TOUCH_E("%s: Failed to read data\n", __func__);
			return retval;
		}
	} else {
		return -EINVAL;
	}

/*
	if (!rmidev->concurrent)
		goto exit;

	if (address != rmi4_data->f01_data_base_addr)
		goto exit;

	if (length <= 1)
		goto exit;

	intr_status = buf[1];

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask & intr_status) {
					rmi4_data->report_touch(rmi4_data,
							fhandler);
				}
			}
		}
	}
exit:
*/
	return length;
}

static ssize_t rmidev_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int length = (unsigned int)count;
	unsigned short address = (unsigned short)pos;

	TOUCH_TRACE();

	if (length > (REG_ADDR_LIMIT - address)) {
		TOUCH_E("%s: Out of register map limit\n", __func__);
		return -EINVAL;
	}

	if (length) {
		retval = synaptics_write(rmidev->ts_dev,
				(address) >> 8,
				address,
				(unsigned char *)buf,
				length);
		if (retval < 0) {
			TOUCH_E("%s: Failed to write data\n", __func__);
			return retval;
		}
	} else {
		return -EINVAL;
	}

	return length;
}

static ssize_t rmidev_sysfs_open_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(rmidev->ts_dev);
	unsigned int input;

	TOUCH_TRACE();

	if (sscanf(buf, "%u", &input) != 1) {
		TOUCH_I("%s : sscanf error\n", __func__);
		return -EINVAL;
	}

	if (input != 1) {
		TOUCH_I("%s : input = %d\n", __func__, input);
		return -EINVAL;
	}

	/*
	if (rmi4_data->sensor_sleep) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Sensor sleeping\n",
				__func__);
		return -ENODEV;
	}

	rmi4_data->stay_awake = true;
	*/

	touch_interrupt_control(rmidev->ts_dev, INTERRUPT_DISABLE);
	TOUCH_I("%s: touch_interrupt_control error!\n", __func__);
	free_irq(ts->irq, ts);
	TOUCH_I("%s: free_irq error!\n", __func__);
	rmidev_sysfs_irq_enable(rmidev->ts_dev, true);
	TOUCH_I("%s: rmidev_sysfs_irq_enable error!\n", __func__);

	TOUCH_I("%s: Attention interrupt disabled\n", __func__);

	return count;
}

static ssize_t rmidev_sysfs_release_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(rmidev->ts_dev);
	unsigned int input;

	TOUCH_TRACE();

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	rmidev_sysfs_irq_enable(rmidev->ts_dev, false);
	touch_request_irq(ts->irq, touch_irq_handler,
			touch_irq_thread, ts->irqflags | IRQF_ONESHOT,
			LGE_TOUCH_NAME, ts);
	touch_interrupt_control(rmidev->ts_dev, INTERRUPT_ENABLE);

	TOUCH_I("%s: Attention interrupt enabled\n" ,__func__);

	//rmi4_data->reset_device(rmi4_data, false);
	//rmi4_data->stay_awake = false;

	return count;
}

static ssize_t rmidev_sysfs_attn_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct touch_core_data *ts = to_touch_core(rmidev->ts_dev);
	int attn_state;

	TOUCH_TRACE();

	attn_state = gpio_get_value(ts->int_pin);

	return snprintf(buf, PAGE_SIZE, "%u\n", attn_state);
}

static ssize_t rmidev_sysfs_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	TOUCH_TRACE();

	return snprintf(buf, PAGE_SIZE, "%u\n", rmidev->pid);
}

static ssize_t rmidev_sysfs_pid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	TOUCH_TRACE();

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmidev->pid = input;

	if (rmidev->pid) {
		rmidev->task = pid_task(find_vpid(rmidev->pid), PIDTYPE_PID);
		if (!rmidev->task) {
			TOUCH_E("%s: Failed to locate PID of data logging tool\n", __func__);
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t rmidev_sysfs_term_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	TOUCH_TRACE();

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	if (rmidev->pid)
		send_sig_info(SIGTERM, &rmidev->terminate_signal, rmidev->task);

	return count;
}

static ssize_t rmidev_sysfs_intr_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	TOUCH_TRACE();

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", rmidev->intr_mask);
}

static ssize_t rmidev_sysfs_intr_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	TOUCH_TRACE();

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmidev->intr_mask = (unsigned char)input;

	return count;
}

static ssize_t rmidev_sysfs_concurrent_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	TOUCH_TRACE();

	return snprintf(buf, PAGE_SIZE, "%d\n", rmidev->concurrent);
}

static ssize_t rmidev_sysfs_concurrent_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	TOUCH_TRACE();

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmidev->concurrent = input > 0 ? true : false;

	return count;
}

static int rmidev_allocate_buffer(int count)
{
	TOUCH_TRACE();

	if (count + 1 > rmidev->tmpbuf_size) {
		if (rmidev->tmpbuf_size)
			kfree(rmidev->tmpbuf);
		rmidev->tmpbuf = kzalloc(count + 1, GFP_KERNEL);
		if (!rmidev->tmpbuf) {
			TOUCH_E("%s: Failed to alloc mem for buffer\n", __func__);
			rmidev->tmpbuf_size = 0;
			return -ENOMEM;
		}
		rmidev->tmpbuf_size = count + 1;
	}

	return 0;
}

/*
 * rmidev_llseek - set register address to access for RMI device
 *
 * @filp: pointer to file structure
 * @off:
 *	if whence == SEEK_SET,
 *		off: 16-bit RMI register address
 *	if whence == SEEK_CUR,
 *		off: offset from current position
 *	if whence == SEEK_END,
 *		off: offset from end position (0xFFFF)
 * @whence: SEEK_SET, SEEK_CUR, or SEEK_END
 */
static loff_t rmidev_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	struct rmidev_data *dev_data = filp->private_data;

	TOUCH_TRACE();

	if (IS_ERR(dev_data)) {
		TOUCH_E("%s: Pointer of char device data is invalid\n",
				__func__);
		return -EBADF;
	}

	mutex_lock(&(dev_data->file_mutex));

	switch (whence) {
	case SEEK_SET:
		newpos = off;
		break;
	case SEEK_CUR:
		newpos = filp->f_pos + off;
		break;
	case SEEK_END:
		newpos = REG_ADDR_LIMIT + off;
		break;
	default:
		newpos = -EINVAL;
		goto clean_up;
	}

	if (newpos < 0 || newpos > REG_ADDR_LIMIT) {
		TOUCH_E("%s: New position 0x%04x is invalid\n",
				__func__, (unsigned int)newpos);
		newpos = -EINVAL;
		goto clean_up;
	}

	filp->f_pos = newpos;

clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	return newpos;
}

/*
 * rmidev_read: read register data from RMI device
 *
 * @filp: pointer to file structure
 * @buf: pointer to user space buffer
 * @count: number of bytes to read
 * @f_pos: starting RMI register address
 */
static ssize_t rmidev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	ssize_t retval;
/*	unsigned char intr_status = 0;	*/
	unsigned short address;
	struct rmidev_data *dev_data = filp->private_data;
/*	struct synaptics_rmi4_fn *fhandler;	*/

	TOUCH_TRACE();

	if (IS_ERR(dev_data)) {
		TOUCH_E("%s: Pointer of char device data is invalid\n",
				__func__);
		return -EBADF;
	}

	if (count == 0)
		return 0;

	if (count > (REG_ADDR_LIMIT - *f_pos))
		count = REG_ADDR_LIMIT - *f_pos;

	address = (unsigned short)(*f_pos);

	rmidev_allocate_buffer(count);

	mutex_lock(&(dev_data->file_mutex));

	retval = synaptics_read(rmidev->ts_dev,
			(*f_pos) >> 8,
			*f_pos,
			rmidev->tmpbuf,
			count);
	if (retval < 0)
		goto clean_up;

	if (copy_to_user(buf, rmidev->tmpbuf, count))
		retval = -EFAULT;
	else
		*f_pos += retval;

/*
	if (!rmidev->concurrent)
		goto clean_up;

	if (address != rmi4_data->f01_data_base_addr)
		goto clean_up;

	if (count <= 1)
		goto clean_up;

	intr_status = rmidev->tmpbuf[1];

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask & intr_status) {
					rmi4_data->report_touch(rmi4_data,
							fhandler);
				}
			}
		}
	}
*/
clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	return retval;
}

/*
 * rmidev_write: write register data to RMI device
 *
 * @filp: pointer to file structure
 * @buf: pointer to user space buffer
 * @count: number of bytes to write
 * @f_pos: starting RMI register address
 */
static ssize_t rmidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	ssize_t retval;
	struct rmidev_data *dev_data = filp->private_data;

	TOUCH_TRACE();

	if (IS_ERR(dev_data)) {
		TOUCH_E("%s: Pointer of char device data is invalid\n",
				__func__);
		return -EBADF;
	}

	if (count == 0)
		return 0;

	if (count > (REG_ADDR_LIMIT - *f_pos))
		count = REG_ADDR_LIMIT - *f_pos;

	rmidev_allocate_buffer(count);

	if (copy_from_user(rmidev->tmpbuf, buf, count))
		return -EFAULT;

	mutex_lock(&(dev_data->file_mutex));

	retval = synaptics_write(rmidev->ts_dev,
			(*f_pos) >> 8,
			*f_pos,
			rmidev->tmpbuf,
			count);
	if (retval >= 0)
		*f_pos += retval;

	mutex_unlock(&(dev_data->file_mutex));

	return retval;
}

static int rmidev_open(struct inode *inp, struct file *filp)
{
	int retval = 0;
	struct touch_core_data *ts = to_touch_core(rmidev->ts_dev);
	struct rmidev_data *dev_data =
			container_of(inp->i_cdev, struct rmidev_data, main_dev);

	TOUCH_TRACE();

	TOUCH_I("%s : before dev_data\n", __func__);

	if (!dev_data) {
		TOUCH_I("%s : dev_data error\n", __func__);
		return -EACCES;
	}
	/*
	if (rmi4_data->sensor_sleep) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Sensor sleeping\n",
				__func__);
		return -ENODEV;
	}

	rmi4_data->stay_awake = true;
	*/
	filp->private_data = dev_data;

	mutex_lock(&(dev_data->file_mutex));

	touch_interrupt_control(rmidev->ts_dev, INTERRUPT_DISABLE);
	free_irq(ts->irq, ts);
	TOUCH_I("%s: Attention interrupt disabled\n", __func__);

	if (dev_data->ref_count < 1)
		dev_data->ref_count++;
	else
		retval = -EACCES;

	mutex_unlock(&(dev_data->file_mutex));

	return retval;
}

static int rmidev_release(struct inode *inp, struct file *filp)
{
	struct touch_core_data *ts = to_touch_core(rmidev->ts_dev);
	struct rmidev_data *dev_data =
			container_of(inp->i_cdev, struct rmidev_data, main_dev);
	int retval = 0;

	TOUCH_TRACE();

	if (!dev_data)
		return -EACCES;

	mutex_lock(&(dev_data->file_mutex));

	dev_data->ref_count--;
	if (dev_data->ref_count < 0)
		dev_data->ref_count = 0;

	retval = request_threaded_irq(ts->irq, touch_irq_handler,
			touch_irq_thread, ts->irqflags | IRQF_ONESHOT,
			LGE_TOUCH_NAME, ts);

	if (retval < 0) {
		TOUCH_E("%s: Failed to create irq thread\n", __func__);
		return retval;
	}

	touch_interrupt_control(rmidev->ts_dev, INTERRUPT_ENABLE);
	TOUCH_I("%s: Attention interrupt enabled\n", __func__);

	mutex_unlock(&(dev_data->file_mutex));

	return 0;
}

static const struct file_operations rmidev_fops = {
	.owner = THIS_MODULE,
	.llseek = rmidev_llseek,
	.read = rmidev_read,
	.write = rmidev_write,
	.open = rmidev_open,
	.release = rmidev_release,
};

static void rmidev_device_cleanup(struct rmidev_data *dev_data)
{
	dev_t devno;

	TOUCH_TRACE();

	if (dev_data) {
		devno = dev_data->main_dev.dev;

		if (dev_data->device_class)
			device_destroy(dev_data->device_class, devno);

		cdev_del(&dev_data->main_dev);

		unregister_chrdev_region(devno, 1);

		TOUCH_I("%s: rmidev device removed\n",
				__func__);
	}

	return;
}

static char *rmi_char_devnode(struct device *dev, umode_t *mode)
{
	TOUCH_TRACE();

	if (!mode)
		return NULL;

	*mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	return kasprintf(GFP_KERNEL, "rmi/%s", dev_name(dev));
}

static int rmidev_create_device_class(void)
{
	TOUCH_TRACE();

	if (rmidev_device_class != NULL)
		return 0;

	rmidev_device_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);

	if (IS_ERR(rmidev_device_class)) {
		TOUCH_E("%s: Failed to create /dev/%s\n",
				__func__, CHAR_DEVICE_NAME);
		return -ENODEV;
	}

	rmidev_device_class->devnode = rmi_char_devnode;

	return 0;
}

static void rmidev_attn(struct device *ts_dev,
		unsigned char intr_mask)
{
	TOUCH_TRACE();

	if (!rmidev)
		return;

	if (rmidev->pid && (rmidev->intr_mask & intr_mask))
		send_sig_info(SIGIO, &rmidev->interrupt_signal, rmidev->task);

	return;
}

static int rmidev_init_device(struct device *ts_dev)
{
	struct touch_core_data *ts = to_touch_core(ts_dev);
	struct synaptics_data *d = to_synaptics_data(ts_dev);
	int retval;
	struct rmidev_data *dev_data;
	unsigned char attr_count;
	struct device *device_ptr;
	dev_t dev_no;

	TOUCH_TRACE();

	if (rmidev) {
		TOUCH_I("%s: Handle already exists\n", __func__);
		return 0;
	}

	rmidev = kzalloc(sizeof(*rmidev), GFP_KERNEL);
	if (!rmidev) {
		TOUCH_E("%s: Failed to alloc mem for rmidev\n", __func__);
		retval = -ENOMEM;
		goto err_rmidev;
	}

	rmidev->d = d;
	rmidev->ts_dev = ts_dev;

	memset(&rmidev->interrupt_signal, 0, sizeof(rmidev->interrupt_signal));
	rmidev->interrupt_signal.si_signo = SIGIO;
	rmidev->interrupt_signal.si_code = SI_USER;

	memset(&rmidev->terminate_signal, 0, sizeof(rmidev->terminate_signal));
	rmidev->terminate_signal.si_signo = SIGTERM;
	rmidev->terminate_signal.si_code = SI_USER;

	retval = rmidev_create_device_class();
	if (retval < 0) {
		TOUCH_E("%s: Failed to create device class\n", __func__);
		goto err_device_class;
	}

	if (rmidev_major_num) {
		dev_no = MKDEV(rmidev_major_num, DEV_NUMBER);
		retval = register_chrdev_region(dev_no, 1, CHAR_DEVICE_NAME);
	} else {
		retval = alloc_chrdev_region(&dev_no, 0, 1, CHAR_DEVICE_NAME);
		if (retval < 0) {
			TOUCH_E("%s: Failed to allocate char device region\n",
					__func__);
			goto err_device_region;
		}

		rmidev_major_num = MAJOR(dev_no);
		TOUCH_I("%s: Major number of rmidev = %d\n", __func__,
				rmidev_major_num);
	}

	dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data) {
		TOUCH_E("%s: Failed to alloc mem for dev_data\n", __func__);
		retval = -ENOMEM;
		goto err_dev_data;
	}

	mutex_init(&dev_data->file_mutex);
	dev_data->rmi_dev = rmidev;
	rmidev->data = dev_data;

	cdev_init(&dev_data->main_dev, &rmidev_fops);

	retval = cdev_add(&dev_data->main_dev, dev_no, 1);
	if (retval < 0) {
		TOUCH_E("%s: Failed to add rmi char device\n", __func__);
		goto err_char_device;
	}

	dev_set_name(&rmidev->dev, "rmidev%d", MINOR(dev_no));
	dev_data->device_class = rmidev_device_class;

	device_ptr = device_create(dev_data->device_class, NULL, dev_no,
			NULL, CHAR_DEVICE_NAME"%d", MINOR(dev_no));
	if (IS_ERR(device_ptr)) {
		TOUCH_E("%s: Failed to create rmi char device\n", __func__);
		retval = -ENODEV;
		goto err_char_device;
	}

	TOUCH_I("ts->int_pin : %d\n", ts->int_pin);
	retval = gpio_export(ts->int_pin, false);
	if (retval < 0) {
		TOUCH_E("%s: Failed to export attention gpio\n", __func__);
	} else {
		retval = gpio_export_link(&(ts->input->dev),
				"attn", ts->int_pin);
		if (retval < 0) {
			TOUCH_E("%s Failed to create gpio symlink\n", __func__);
		} else {
			TOUCH_I("%s: Exported attention gpio %d\n",
					__func__, ts->int_pin);
		}
	}

	rmidev->sysfs_dir = kobject_create_and_add(SYSFS_FOLDER_NAME,
			&ts->input->dev.kobj);
	if (!rmidev->sysfs_dir) {
		TOUCH_E("%s: Failed to create sysfs directory\n", __func__);
		retval = -ENODEV;
		goto err_sysfs_dir;
	}

	retval = sysfs_create_bin_file(rmidev->sysfs_dir,
			&attr_data);
	if (retval < 0) {
		TOUCH_E("%s: Failed to create sysfs bin file\n", __func__);
		goto err_sysfs_bin;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(rmidev->sysfs_dir,
				&attrs[attr_count].attr);
		if (retval < 0) {
			TOUCH_E("%s: Failed to create sysfs attributes\n", __func__);
			retval = -ENODEV;
			goto err_sysfs_attrs;
		}
	}

	return 0;

err_sysfs_attrs:
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(rmidev->sysfs_dir, &attrs[attr_count].attr);

	sysfs_remove_bin_file(rmidev->sysfs_dir, &attr_data);

err_sysfs_bin:
	kobject_put(rmidev->sysfs_dir);

err_sysfs_dir:
err_char_device:
	rmidev_device_cleanup(dev_data);
	kfree(dev_data);

err_dev_data:
	unregister_chrdev_region(dev_no, 1);

err_device_region:
	if (rmidev_device_class != NULL) {
		class_destroy(rmidev_device_class);
		rmidev_device_class = NULL;
	}

err_device_class:
	kfree(rmidev);
	rmidev = NULL;

err_rmidev:
	return retval;
}

static void rmidev_remove_device(struct device *ts_dev)
{
	struct touch_core_data *ts = to_touch_core(ts_dev);
	unsigned char attr_count;
	struct rmidev_data *dev_data;

	TOUCH_TRACE();

	if (!rmidev)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(rmidev->sysfs_dir, &attrs[attr_count].attr);

	sysfs_remove_bin_file(rmidev->sysfs_dir, &attr_data);

	kobject_put(rmidev->sysfs_dir);

	gpio_unexport(ts->int_pin);

	dev_data = rmidev->data;
	if (dev_data) {
		rmidev_device_cleanup(dev_data);
		kfree(dev_data);
	}

	unregister_chrdev_region(rmidev->dev_no, 1);

	if (rmidev_device_class != NULL) {
		class_destroy(rmidev_device_class);
		rmidev_device_class = NULL;
	}

	kfree(rmidev->tmpbuf);

	kfree(rmidev);
	rmidev = NULL;

exit:
	complete(&rmidev_remove_complete);

	return;
}

static struct synaptics_rmidev_exp_fn rmidev_module = {
	.init = rmidev_init_device,
	.remove = rmidev_remove_device,
	.reset = NULL,
	.reinit = NULL,
	.early_suspend = NULL,
	.suspend = NULL,
	.resume = NULL,
	.late_resume = NULL,
	.attn = rmidev_attn,
};

static int __init rmidev_module_init(void)
{
	TOUCH_TRACE();

	synaptics_rmidev_function(&rmidev_module, true);

	return 0;
}

static void __exit rmidev_module_exit(void)
{
	TOUCH_TRACE();

	synaptics_rmidev_function(&rmidev_module, false);

	wait_for_completion(&rmidev_remove_complete);

	return;
}

module_init(rmidev_module_init);
module_exit(rmidev_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX RMI Dev Module");
MODULE_LICENSE("GPL v2");
