/* Touch_synaptics_rmi_dev.c
 *
 * Copyright (C) 2014 LGE.
 *
 * Author: daehyun.gil@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define TS_MODULE "[s3320_rmi_dev]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include <soc/qcom/lge/board_lge.h>

#include <touch_core.h>

#include "touch_s3320.h"

#define MASK_16BIT	(0xFFFF)
#define MASK_8BIT	(0xFF)
#define MASK_7BIT	(0x7F)
#define MASK_6BIT	(0x3F)
#define MASK_5BIT	(0x1F)
#define MASK_4BIT	(0x0F)
#define MASK_3BIT	(0x07)
#define MASK_2BIT	(0x03)
#define MASK_1BIT	(0x01)

#define CHAR_DEVICE_NAME "rmi"
#define DEVICE_CLASS_NAME "rmidev"
#define SYSFS_FOLDER_NAME "rmidev"
#define DEV_NUMBER 1
#define REG_ADDR_LIMIT 0xFFFF

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CONFIGURED (1 << 7)

#define TOUCH_RMIDEV_MSG(fmt, args...) \
	pr_info("[Touch-RMI] %s: " fmt, __func__, ##args)

typedef char *(*devnode_func)(struct device *dev, umode_t *mode);

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

static inline ssize_t rmidev_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	TOUCH_RMIDEV_MSG("Attempted to read from write-only attribute %s\n",
			attr->attr.name);
	return -EPERM;
}

static inline ssize_t rmidev_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	TOUCH_RMIDEV_MSG("Attempted to write to read-only attribute %s\n",
			attr->attr.name);
	return -EPERM;
}

struct rmidev_handle {
	dev_t dev_no;
	struct device dev;
	struct touch_core_data *c_data;
	struct synaptics_data *d_data;
	struct kobject *sysfs_dir;
	void *data;
	bool irq_enabled;
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
		.mode = (S_IRUSR | S_IWUSR),
	},
	.size = 0,
	.read = rmidev_sysfs_data_show,
	.write = rmidev_sysfs_data_store,
};

static struct device_attribute attrs[] = {
	__ATTR(open, S_IWUSR,
			rmidev_show_error, rmidev_sysfs_open_store),
	__ATTR(release, S_IWUSR,
			rmidev_show_error, rmidev_sysfs_release_store),
	__ATTR(attn_state, S_IRUSR,
			rmidev_sysfs_attn_state_show, rmidev_store_error),
};

static int rmidev_major_num;

static struct class *rmidev_device_class;

static struct rmidev_handle *rmidev;

DECLARE_COMPLETION(rmidev_remove_complete);

static int rmidev_i2c_read(struct device *dev,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int ret = 0;
	u8 page_old = 0;
	u8 page_new = 0;
	bool page_changed;

	/* page read */
	ret = synaptics_read(dev, PAGE_SELECT_REG, &page_old, sizeof(page_old));

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to read from Page_Select register\n");
		return ret;
	}

	page_new = (addr >> 8);

	/* page compare & change */
	if (page_old == page_new) {
		page_changed = false;
	} else {
		ret = synaptics_write(dev, PAGE_SELECT_REG,
					&page_new, sizeof(page_new));

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to change Page_Select register\n");
			return ret;
		}

		page_changed = true;
	}

	/* read register */
	ret = synaptics_read(dev, (u8)(addr & ~(MASK_8BIT << 8)), data, length);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to write register(addr=0x%04x)\n", addr);
		return ret;
	}

	/* page restore */
	if (page_changed) {
		ret = synaptics_write(dev, PAGE_SELECT_REG,
					&page_old, sizeof(page_old));

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to restore Page_Select register\n");
			return ret;
		}
	}

	return 0;
}

static int rmidev_i2c_write(struct device *dev,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int ret;
	u8 page_old = 0;
	u8 page_new = 0;
	bool page_changed;

	/* page read */
	ret = synaptics_read(dev, PAGE_SELECT_REG,
				&page_old, sizeof(page_old));

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to read from Page_Select register\n");
		return ret;
	}

	page_new = (addr >> 8);

	/* page compare & change */
	if (page_old == page_new) {
		page_changed = false;
	} else {
		ret = synaptics_write(dev, PAGE_SELECT_REG, &page_new, sizeof(page_new));

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to change Page_Select register\n");
			return ret;
		}

		page_changed = true;
	}

	/* write register */
	ret = synaptics_write(dev, (u8)(addr & ~(MASK_8BIT << 8)), data, length);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to write register(addr=0x%04x)\n", addr);
		return ret;
	}

	/* page restore */
	if (page_changed) {	
		ret = synaptics_write(dev, PAGE_SELECT_REG,
					&page_old, sizeof(page_old));

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to restore Page_Select register\n");
			return ret;
		}
	}

	return 0;
}

static int rmidev_reset_device(struct synaptics_data *d)
{
	struct touch_core_data *ts = rmidev->c_data;

	int ret;
	unsigned char command = 0x01;
	int reg_read_cnt;
	unsigned char manufacturer_ID;
	unsigned char interrupt_enable_0 = 0x3f;
	unsigned char device_ctrl;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (!ts) {
		TOUCH_RMIDEV_MSG("ts points to NULL\n");
		return -EACCES;
	}

	ret = rmidev_i2c_write(ts->dev, d->f01.dsc.command_base, &command, 1);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to issue reset command\n");
		return ret;
	}

	touch_report_all_event(ts);
	touch_msleep(10);

	for (reg_read_cnt = 1; reg_read_cnt <= 10; reg_read_cnt++) {
		ret = rmidev_i2c_read(ts->dev, d->f01.dsc.query_base,
				&manufacturer_ID, 1);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to read from manufacturer ID register\n");
			return ret;
		}

		TOUCH_RMIDEV_MSG("reg_read_cnt=%d , manufacturer ID=%d\n",
				reg_read_cnt, manufacturer_ID);

		if (manufacturer_ID == 0 || manufacturer_ID == 1)
			break;
		else
			touch_msleep(20);
	}

	ret = rmidev_i2c_write(ts->dev, d->f01.dsc.control_base + 1,
			&interrupt_enable_0, 1);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to enable interrupt\n");
		return ret;
	}

	ret = rmidev_i2c_read(ts->dev, d->f01.dsc.control_base, &device_ctrl, 1);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to read from device control register\n");
		return ret;
	}

	device_ctrl |= CONFIGURED;

	ret = rmidev_i2c_write(ts->dev, d->f01.dsc.control_base,
			&device_ctrl, 1);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to set configured\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	synaptics_init(ts->dev);
	mutex_unlock(&ts->lock);

	return 0;
}

static int rmidev_irq_enable(struct synaptics_data *d, bool enable)
{
	struct touch_core_data *ts = rmidev->c_data;

	int ret = 0;
	unsigned char intr_status;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (!ts) {
		TOUCH_RMIDEV_MSG("ts points to NULL\n");
		return -EACCES;
	}

	if (enable) {
		/* Clear interrupts first */
		ret = rmidev_i2c_read(ts->dev, d->f01.dsc.data_base + 1,
				&intr_status, 1);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to clear interrupts\n");
			return ret;
		}

		ret = touch_request_irq(ts->irq, touch_irq_handler,
				touch_irq_thread, ts->irqflags | IRQF_ONESHOT,
				LGE_TOUCH_NAME, ts);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to create irq thread\n");
			return ret;
		}
	} else {
		touch_disable_irq(ts->irq);
		free_irq(ts->irq, ts);
	}

	return ret;
}

static irqreturn_t rmidev_sysfs_irq(int irq, void *data)
{
	struct touch_core_data *ts = rmidev->c_data;
	struct synaptics_data *d = rmidev->d_data;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (!ts) {
		TOUCH_RMIDEV_MSG("ts points to NULL\n");
		return -EACCES;
	}

	sysfs_notify(&ts->input->dev.kobj, SYSFS_FOLDER_NAME,
			"attn_state");

	return IRQ_HANDLED;
}

static int rmidev_sysfs_irq_enable(struct synaptics_data *d, bool enable)
{
	struct touch_core_data *ts = rmidev->c_data;

	int ret = 0;
	unsigned char intr_status;
	unsigned long irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (!rmidev) {
		TOUCH_RMIDEV_MSG("rmidev points to NULL\n");
		return -EACCES;
	}

	if (enable) {
		if (rmidev->irq_enabled)
			return ret;

		/* Clear interrupts first */
		ret = rmidev_i2c_read(ts->dev, d->f01.dsc.data_base + 1,
				&intr_status, 1);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to clear interrupts\n");
			return ret;
		}

		ret = touch_request_irq(ts->irq, NULL, rmidev_sysfs_irq,
					irq_flags, "synaptics_dsx_rmidev", ts);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to create irq thread\n");
			return ret;
		}

		rmidev->irq_enabled = true;
	} else {
		if (rmidev->irq_enabled) {
			touch_disable_irq(ts->irq);
			free_irq(ts->irq, ts);
			rmidev->irq_enabled = false;
		}
	}

	return ret;
}

static ssize_t rmidev_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct touch_core_data *ts = rmidev->c_data;
	struct synaptics_data *d = rmidev->d_data;

	unsigned int length = (unsigned int)count;
	unsigned short address = (unsigned short)pos;
	int ret;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (length > (REG_ADDR_LIMIT - address)) {
		TOUCH_RMIDEV_MSG("Out of register map limit\n");
		return -EINVAL;
	}

	if (length) {
		ret = rmidev_i2c_read(ts->dev, address, (u8 *)buf, length);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to read data\n");
			return ret;
		}
	} else {
		TOUCH_RMIDEV_MSG("Invalid length value(length=0)\n");
		return -EINVAL;
	}

	return length;
}

static ssize_t rmidev_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct touch_core_data *ts = rmidev->c_data;
	struct synaptics_data *d = rmidev->d_data;

	unsigned int length = (unsigned int)count;
	unsigned short address = (unsigned short)pos;
	int ret;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (length > (REG_ADDR_LIMIT - address)) {
		TOUCH_RMIDEV_MSG("Out of register map limit\n");
		return -EINVAL;
	}

	if (length) {
		ret = rmidev_i2c_write(ts->dev, address, (u8 *)buf, length);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to write data\n");
			return ret;
		}
	} else {
		TOUCH_RMIDEV_MSG("Invalid length value(length=0)\n");
		return -EINVAL;
	}

	return length;
}

static ssize_t rmidev_sysfs_open_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_data *d = rmidev->d_data;

	unsigned int input;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (sscanf(buf, "%u", &input) != 1) {
		TOUCH_RMIDEV_MSG("The length of the input value is not 1\n");
		return -EINVAL;
	}

	if (input != 1) {
		TOUCH_RMIDEV_MSG("Invalid input value(input=%u)\n", input);
		return -EINVAL;
	}

	rmidev_irq_enable(d, false);
	rmidev_sysfs_irq_enable(d, true);
	TOUCH_RMIDEV_MSG("Attention interrupt disabled\n");

	return count;
}

static ssize_t rmidev_sysfs_release_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_data *d = rmidev->d_data;

	unsigned int input;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (sscanf(buf, "%u", &input) != 1) {
		TOUCH_RMIDEV_MSG("The length of the input value is not 1\n");
		return -EINVAL;
	}

	if (input != 1) {
		TOUCH_RMIDEV_MSG("Invalid input value(input=%u)\n", input);
		return -EINVAL;
	}

	rmidev_reset_device(d);

	rmidev_sysfs_irq_enable(d, false);
	rmidev_irq_enable(d, true);
	TOUCH_RMIDEV_MSG("Attention interrupt enabled\n");

	return count;
}

static ssize_t rmidev_sysfs_attn_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct touch_core_data *ts = rmidev->c_data;
	struct synaptics_data *d = rmidev->d_data;

	int attn_state;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	attn_state = gpio_get_value(ts->int_pin);

	return snprintf(buf, PAGE_SIZE, "%u\n", attn_state);
}

/*
 * rmidev_llseek - used to set up register address
 *
 * @filp: file structure for seek
 * @off: offset
 *   if whence == SEEK_SET,
 *     high 16 bits: page address
 *     low 16 bits: register address
 *   if whence == SEEK_CUR,
 *     offset from current position
 *   if whence == SEEK_END,
 *     offset from end position (0xFFFF)
 * @whence: SEEK_SET, SEEK_CUR, or SEEK_END
 */
static loff_t rmidev_llseek(struct file *filp, loff_t off, int whence)
{
	struct rmidev_data *dev_data = filp->private_data;

	loff_t newpos;

	if (IS_ERR(dev_data)) {
		TOUCH_RMIDEV_MSG("Pointer of char device data is invalid\n");
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
		TOUCH_RMIDEV_MSG("New position 0x%04x is invalid\n",
				(u32)newpos);
		newpos = -EINVAL;
		goto clean_up;
	}

	filp->f_pos = newpos;

clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	return newpos;
}

/*
 * rmidev_read: - use to read data from rmi device
 *
 * @filp: file structure for read
 * @buf: user space buffer pointer
 * @count: number of bytes to read
 * @f_pos: offset (starting register address)
 */
static ssize_t rmidev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct touch_core_data *ts = rmidev->c_data;
	struct rmidev_data *dev_data = filp->private_data;

	ssize_t ret;
	unsigned char *tmpbuf;

	if (IS_ERR(dev_data)) {
		TOUCH_RMIDEV_MSG("Pointer of char device data is invalid\n");
		return -EBADF;
	}

	if (count == 0)
		return 0;

	if (count > (REG_ADDR_LIMIT - *f_pos))
		count = REG_ADDR_LIMIT - *f_pos;

	tmpbuf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmpbuf)
		return -ENOMEM;

	mutex_lock(&(dev_data->file_mutex));

	ret = rmidev_i2c_read(ts->dev, *f_pos, tmpbuf, count);

	if (ret < 0)
		goto clean_up;

	if (copy_to_user(buf, tmpbuf, count))
		ret = -EFAULT;
	else
		*f_pos += ret;

clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	kfree(tmpbuf);
	return ret;
}

/*
 * rmidev_write: - used to write data to rmi device
 *
 * @filep: file structure for write
 * @buf: user space buffer pointer
 * @count: number of bytes to write
 * @f_pos: offset (starting register address)
 */
static ssize_t rmidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct touch_core_data *ts = rmidev->c_data;
	struct rmidev_data *dev_data = filp->private_data;

	ssize_t ret;
	unsigned char *tmpbuf;

	if (IS_ERR(dev_data)) {
		TOUCH_RMIDEV_MSG("Pointer of char device data is invalid\n");
		return -EBADF;
	}

	if (count == 0)
		return 0;

	if (count > (REG_ADDR_LIMIT - *f_pos))
		count = REG_ADDR_LIMIT - *f_pos;

	tmpbuf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmpbuf)
		return -ENOMEM;

	if (copy_from_user(tmpbuf, buf, count)) {
		kfree(tmpbuf);
		return -EFAULT;
	}

	mutex_lock(&(dev_data->file_mutex));

	ret = rmidev_i2c_write(ts->dev, *f_pos, tmpbuf, count);

	if (ret >= 0)
		*f_pos += ret;

	mutex_unlock(&(dev_data->file_mutex));
	kfree(tmpbuf);
	return ret;
}

/*
 * rmidev_open: enable access to rmi device
 * @inp: inode struture
 * @filp: file structure
 */
static int rmidev_open(struct inode *inp, struct file *filp)
{
	struct synaptics_data *d = rmidev->d_data;
	struct rmidev_data *dev_data = container_of(inp->i_cdev,
			struct rmidev_data, main_dev);

	int ret = 0;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (!dev_data) {
		TOUCH_RMIDEV_MSG("dev_data points to NULL\n");
		return -EACCES;
	}

	filp->private_data = dev_data;

	mutex_lock(&(dev_data->file_mutex));

	rmidev_irq_enable(d, false);
	TOUCH_RMIDEV_MSG("Attention interrupt disabled\n");

	if (dev_data->ref_count < 1)
		dev_data->ref_count++;
	else {
		TOUCH_RMIDEV_MSG("Invalid ref_count value(ref_count=%d)\n",
				dev_data->ref_count);
		ret = -EACCES;
	}

	mutex_unlock(&(dev_data->file_mutex));

	return ret;
}

/*
 * rmidev_release: - release access to rmi device
 * @inp: inode structure
 * @filp: file structure
 */
static int rmidev_release(struct inode *inp, struct file *filp)
{
	struct synaptics_data *d = rmidev->d_data;
	struct rmidev_data *dev_data = container_of(inp->i_cdev,
			struct rmidev_data, main_dev);

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		return -EACCES;
	}

	if (!dev_data) {
		TOUCH_RMIDEV_MSG("dev_data points to NULL\n");
		return -EACCES;
	}

	rmidev_reset_device(d);

	mutex_lock(&(dev_data->file_mutex));

	dev_data->ref_count--;
	if (dev_data->ref_count < 0)
		dev_data->ref_count = 0;

	rmidev_irq_enable(d, true);
	TOUCH_RMIDEV_MSG("Attention interrupt enabled\n");

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

	TOUCH_RMIDEV_MSG("\n");

	if (dev_data) {
		devno = dev_data->main_dev.dev;

		if (dev_data->device_class)
			device_destroy(dev_data->device_class, devno);

		cdev_del(&dev_data->main_dev);

		unregister_chrdev_region(devno, 1);

		TOUCH_RMIDEV_MSG("rmidev device removed\n");
	}

	return;
}

static char *rmi_char_devnode(struct device *dev, mode_t *mode)
{
	TOUCH_RMIDEV_MSG("\n");

	if (!mode) {
		TOUCH_RMIDEV_MSG("mode points to NULL\n");
		return NULL;
	}

	*mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	return kasprintf(GFP_KERNEL, "rmi/%s", dev_name(dev));
}

static int rmidev_create_device_class(void)
{
	TOUCH_RMIDEV_MSG("\n");

	rmidev_device_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);

	if (IS_ERR(rmidev_device_class)) {
		TOUCH_RMIDEV_MSG(
				"Failed to create /dev/%s\n", CHAR_DEVICE_NAME);
		return -ENODEV;
	}

	rmidev_device_class->devnode = (devnode_func)rmi_char_devnode;

	return 0;
}

static int rmidev_init_device(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct synaptics_data *d = to_synaptics_data(dev);

	int ret;
	dev_t dev_no;
	unsigned char attr_count;
	struct rmidev_data *dev_data;
	struct device *device_ptr;

	TOUCH_RMIDEV_MSG("\n");

	if (!d) {
		TOUCH_RMIDEV_MSG("device points to NULL\n");
		ret = -EACCES;
		goto err_rmidev;
	}

	if (!ts) {
		TOUCH_RMIDEV_MSG("ts points to NULL\n");
		ret = -EACCES;
		goto err_rmidev;
	}

	rmidev = kzalloc(sizeof(*rmidev), GFP_KERNEL);

	if (!rmidev) {
		TOUCH_RMIDEV_MSG("Failed to alloc mem for rmidev\n");
		ret = -ENOMEM;
		goto err_rmidev;
	}

	rmidev->c_data = ts;
	rmidev->d_data = d;

	ret = rmidev_create_device_class();

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to create device class\n");
		goto err_device_class;
	}

	if (rmidev_major_num) {
		dev_no = MKDEV(rmidev_major_num, DEV_NUMBER);
		ret = register_chrdev_region(dev_no, 1, CHAR_DEVICE_NAME);
	} else {
		ret = alloc_chrdev_region(&dev_no, 0, 1, CHAR_DEVICE_NAME);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to allocate char device region\n");
			goto err_device_region;
		}

		rmidev_major_num = MAJOR(dev_no);
		TOUCH_RMIDEV_MSG("Major number of rmidev = %d\n", rmidev_major_num);
	}

	dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);

	if (!dev_data) {
		TOUCH_RMIDEV_MSG("Failed to alloc mem for dev_data\n");
		ret = -ENOMEM;
		goto err_dev_data;
	}

	mutex_init(&dev_data->file_mutex);
	dev_data->rmi_dev = rmidev;
	rmidev->data = dev_data;

	cdev_init(&dev_data->main_dev, &rmidev_fops);

	ret = cdev_add(&dev_data->main_dev, dev_no, 1);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to add rmi char device\n");
		goto err_char_device;
	}

	dev_set_name(&rmidev->dev, "rmidev%d", MINOR(dev_no));
	dev_data->device_class = rmidev_device_class;

	device_ptr = device_create(dev_data->device_class, NULL, dev_no,
			NULL, CHAR_DEVICE_NAME"%d", MINOR(dev_no));

	if (IS_ERR(device_ptr)) {
		TOUCH_RMIDEV_MSG("Failed to create rmi char device\n");
		ret = -ENODEV;
		goto err_char_device;
	}

	ret = gpio_export(ts->int_pin, false);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to export attention gpio\n");
	} else {
		ret = gpio_export_link(&(ts->input->dev), "attn", ts->int_pin);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to create gpio symlink\n");
		} else {
			TOUCH_RMIDEV_MSG("Exported attention gpio %d\n",
					ts->int_pin);
		}
	}

	rmidev->sysfs_dir = kobject_create_and_add(SYSFS_FOLDER_NAME,
			&ts->input->dev.kobj);

	if (!rmidev->sysfs_dir) {
		TOUCH_RMIDEV_MSG("Failed to create sysfs directory\n");
		ret = -ENODEV;
		goto err_sysfs_dir;
	}

	ret = sysfs_create_bin_file(rmidev->sysfs_dir, &attr_data);

	if (ret < 0) {
		TOUCH_RMIDEV_MSG("Failed to create sysfs bin file\n");
		goto err_sysfs_bin;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		ret = sysfs_create_file(rmidev->sysfs_dir,
				&attrs[attr_count].attr);

		if (ret < 0) {
			TOUCH_RMIDEV_MSG("Failed to create sysfs attributes\n");
			ret = -ENODEV;
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
	class_destroy(rmidev_device_class);

err_device_class:
	kfree(rmidev);
	rmidev = NULL;

err_rmidev:
	return ret;
}

static void rmidev_remove_device(struct device *dev)
{
	struct rmidev_data *dev_data;

	unsigned char attr_count;

	TOUCH_RMIDEV_MSG("\n");

	if (!rmidev)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(rmidev->sysfs_dir, &attrs[attr_count].attr);

	sysfs_remove_bin_file(rmidev->sysfs_dir, &attr_data);

	kobject_put(rmidev->sysfs_dir);

	dev_data = rmidev->data;
	if (dev_data) {
		rmidev_device_cleanup(dev_data);
		kfree(dev_data);
	}

	unregister_chrdev_region(rmidev->dev_no, 1);

	class_destroy(rmidev_device_class);

	kfree(rmidev);
	rmidev = NULL;

exit:
	complete(&rmidev_remove_complete);

	return;
}

static struct synaptics_exp_fn rmidev_module = {
	.init = rmidev_init_device,
	.remove = rmidev_remove_device,
	.reset = NULL,
	.reinit = NULL,
	.early_suspend = NULL,
	.suspend = NULL,
	.resume = NULL,
	.late_resume = NULL,
	.attn = NULL,
};

static int __init rmidev_module_init(void)
{
	TOUCH_RMIDEV_MSG("\n");

	synaptics_rmidev_function(&rmidev_module, true);

	return 0;
}

static void __exit rmidev_module_exit(void)
{
	TOUCH_RMIDEV_MSG("\n");

	synaptics_rmidev_function(&rmidev_module, false);

	wait_for_completion(&rmidev_remove_complete);

	return;
}

module_init(rmidev_module_init);
module_exit(rmidev_module_exit);

MODULE_AUTHOR("daehyun.gil@lge.com");
MODULE_DESCRIPTION("LGE Touch RMI Dev Module");
MODULE_LICENSE("GPL");
