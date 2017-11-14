/* Copyright (c) 2016 LG Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <soc/qcom/scm.h>

#include <soc/qcom/lge/lge_mme_drv.h>

/* for debug */
#define DEBUG 0

/* predefined cmd with tz app */
#define CMD_MME_SEND_TRACK1_ONCE        33
#define CMD_MME_SEND_TRACK2_ONCE        34
#define CMD_MME_SEND_TRACK3_ONCE        35
#define CMD_MME_TEST_MODE               38
#define CMD_MME_SEND_STOP               39
#define CMD_START_MME_QSAPP             40
#define CMD_SHUTDOWN_MME_QSAPP          41

/* TZ app name */
#define MME_TZAPP_NAME                  "mme"
#define MAX_APP_NAME                    25

#define QSEECOM_SBUFF_SIZE              64*10
#define QSEECOM_ALIGN_SIZE              0x40
#define QSEECOM_ALIGN_MASK              (QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)                ((x + QSEECOM_ALIGN_MASK) & (~QSEECOM_ALIGN_MASK))

#define DEVICE_NAME                     "lge_mme"

enum track_type {
	DONT_CARE = -1,
	MME_TRACK_TYPE_2,
	MME_TRACK_TYPE_1,
	MME_TRACK_TYPE_2_1,
	MME_TRACK_TYPE_1_2,
	MME_TRACK_TYPE_3,
	MME_TRACK_TYPE_HIGH,
};

/* structures */
struct qseecom_handle {
	void *dev; /* in/out */
	unsigned char *sbuf; /* in/out */
	uint32_t sbuf_len; /* in/out */
};

/* request */
struct mme_send_cmd {
	uint32_t cmd_id;
	uint32_t data;
	uint32_t data2;
	uint32_t data3;
	uint32_t len;
	uint32_t lenION;
	unsigned char ucAppID[20];
};

#if 0
struct mme_ion_info {
	int32_t ion_fd;
	int32_t ifd_data_fd;
	struct ion_handle_data ion_alloc_handle;
	unsigned char * ion_sbuffer;
	uint32_t sbuf_len;
};
#endif

/* respense */
struct mme_send_cmd_rsp {
	uint32_t data;
	int32_t status;
};

/* global variables
   should be updated from dts */
static struct class *mme_class;
static struct device *mme_dev;
static int mme_major;

static unsigned int command_type = 0;       // send_commnad (odd: once, even: repeat)
static bool is_repeat = 0;                  // for repeat command
static struct wake_lock   mme_wakelock;     // wake_lock used for HW test
static struct qseecom_handle *q_handle;     // handler for TA

/* extern function declarations */
extern int qseecom_start_app(struct qseecom_handle **handle, char *app_name, uint32_t size);
extern int qseecom_shutdown_app(struct qseecom_handle **handle);
extern int qseecom_send_command(struct qseecom_handle *handle, void *send_buf, uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len);

static DEFINE_MUTEX(mme_mutex);
static DEFINE_MUTEX(repeat_mutex);


static int qsc_app_start(void)
{
	int ret = 0;

	mutex_lock(&mme_mutex);
	if(q_handle != NULL) {
		pr_err("[MME] already opened qseecom\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = qseecom_start_app(&q_handle, MME_TZAPP_NAME, QSEECOM_SBUFF_SIZE);
	if(ret) {
		pr_err("[MME] Failed to load MME qsapp, ret(%d)\n", ret);
		goto exit;
	}

exit:
	mutex_unlock(&mme_mutex);
	return ret;
}

static int qsc_app_shutdown(void)
{
	int ret = 0;

	mutex_lock(&mme_mutex);
	if(q_handle == NULL) {
		pr_err("[MME] qseecom handle is NULL\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = qseecom_shutdown_app(&q_handle);
	if(ret) {
		pr_err("[MME] Failed to shutdown MME qsapp, ret(%d)\n", ret);
		goto exit;
	}

exit:
	mutex_unlock(&mme_mutex);
	return ret;
}

static int qsc_send_req_command(uint32_t cmd, uint32_t uTrackType)
{
	int ret = 0;
	int req_len = 0;
	int rsp_len = 0;
	struct mme_send_cmd *q_req;      // request data sent to QSEE
	struct mme_send_cmd_rsp *q_rsp;  // response data sent from QSEE

	mutex_lock(&mme_mutex);

	if(q_handle == NULL) {
		pr_err("[MME] already closed qseecom\n");
		ret = -EINVAL;
		goto exit;
	}
	req_len = sizeof(struct mme_send_cmd);
	rsp_len = sizeof(struct mme_send_cmd_rsp);

	q_req = (struct mme_send_cmd*)q_handle->sbuf;
	if(req_len & QSEECOM_ALIGN_MASK)
		req_len = QSEECOM_ALIGN(req_len);

	q_rsp = (struct mme_send_cmd_rsp*)q_handle->sbuf + req_len;
	if(rsp_len & QSEECOM_ALIGN_MASK)
		rsp_len = QSEECOM_ALIGN(rsp_len);

	q_req->cmd_id = cmd;
	if (uTrackType != -1) {
		q_req->data = uTrackType;
	}
	memset(q_req->ucAppID, 0x01, 20);

	ret = qseecom_send_command(q_handle, q_req, req_len, q_rsp, rsp_len);
	if(ret) {
		pr_err("[MME] Failed to send cmd_id(%d), ret(%d)\n", command_type, ret);
		goto exit;
	}

exit:
	mutex_unlock(&mme_mutex);
	return ret;
}

int transmit_data(int type)
{
	int ret = 0;

	ret = qsc_send_req_command(20, type);
	if (ret < 0) {
		pr_err("[MME] Failed to set a type(%d)\n", type);
		goto exit;
	}
	ret = qsc_send_req_command(16, DONT_CARE);
	if (ret < 0) {
		pr_err("[MME] Failed to power on\n");
		goto exit;
	}
	ret = qsc_send_req_command(111, DONT_CARE);
	if (ret < 0) {
		pr_err("[MME] Failed to transmit the data\n");
		goto exit;
	}
	ret = qsc_send_req_command(17, DONT_CARE);
	if (ret < 0) {
		pr_err("[MME] Failed to power off\n");
		goto exit;
	}

exit:
	return ret;
}

/*
 * [mme_command] node read/write function
 */
static ssize_t lge_show_mme_command (struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", command_type);
}

static ssize_t lge_store_mme_command (struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	sscanf(buf, "%d", &command_type);
	pr_err("[MME] %s: Setup qsee command_type=%d\n", __func__, command_type);

	switch(command_type) {

		case CMD_START_MME_QSAPP:
			pr_err("[MME] CMD_START_MME_QSAPP\n");
			ret = qsc_app_start();
			break;

		case CMD_SHUTDOWN_MME_QSAPP:
			pr_err("[MME] CMD_SHUTDOWN_MME_QSAPP\n");
			ret = qsc_app_shutdown();
			break;

		case CMD_MME_SEND_TRACK1_ONCE:
			pr_err("[MME] CMD_MME_SEND_TRACK1_ONCE\n");
			ret = transmit_data(MME_TRACK_TYPE_1);
			break;

		case CMD_MME_SEND_TRACK2_ONCE:
			pr_err("[MME] CMD_MME_SEND_TRACK2_ONCE\n");
			ret = transmit_data(MME_TRACK_TYPE_2);
			break;

		case CMD_MME_SEND_TRACK3_ONCE:
			pr_err("[MME] CMD_MME_SEND_TRACK3_ONCE\n");
			ret = transmit_data(MME_TRACK_TYPE_3);
			break;

		case CMD_MME_TEST_MODE:
			pr_err("[MME] CMD_MME_TEST_MODE\n");

			mutex_lock(&repeat_mutex);
			if(is_repeat == 1) {
				pr_err("[MME] repeat command is already busy\n");
				mutex_unlock(&repeat_mutex);
				return -EBUSY;
			}
			else if(is_repeat == 0) {
				wake_lock_init(&mme_wakelock, WAKE_LOCK_SUSPEND, "mme_wakelock");
				wake_lock(&mme_wakelock);
				is_repeat = 1;
			}
			else {
				pr_err("[MME] invalid value\n");
				mutex_unlock(&repeat_mutex);
				return -EINVAL;
			}
			mutex_unlock(&repeat_mutex);

			while (is_repeat == 1) {
				ret = transmit_data(MME_TRACK_TYPE_HIGH);
				if(ret)
					pr_err("[MME] HIGH_SIGNAL: Failed to transmit card data\n");
				mdelay(1000);
			}
			break;

		case CMD_MME_SEND_STOP:
			pr_err("[MME] CMD_MME_SEND_STOP\n");
			mutex_lock(&repeat_mutex);
			if (is_repeat == 1)
				wake_lock_destroy(&mme_wakelock);
			is_repeat = 0;
			mutex_unlock(&repeat_mutex);
			pr_err("[MME] CMD_MME_SEND_STOP - end\n");
			break;

		default:
			pr_err("[MME] Not suppported cmd_id(%d)\n", command_type);
			return -EINVAL;
			break;
	}

	return count;
}

/*           		  name	    perm	   cat function			echo function   */
static DEVICE_ATTR(mme_command, 0664, lge_show_mme_command, lge_store_mme_command);

static struct attribute *lge_mme_attrs[] = {
	&dev_attr_mme_command.attr,
	NULL
};

static const struct attribute_group lge_mme_files = {
	.attrs  = lge_mme_attrs,
};

static int __init lge_mme_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[MME] %s: probe enter\n", __func__);

	mme_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(mme_class)) {
		pr_err("[MME] %s: class_create() failed ENOMEM\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	mme_dev = device_create(mme_class, NULL, MKDEV(mme_major, 0), NULL, "mme_ctrl");
	if (IS_ERR(mme_dev)) {
		pr_err("[MME] %s: device_create() failed\n", __func__);
		ret = PTR_ERR(mme_dev);
		goto exit;
	}

	/* create /sys/class/lge_mme/mme_ctrl/mme_command */
	ret = device_create_file(mme_dev, &dev_attr_mme_command);
	if (ret < 0) {
		pr_err("[MME] %s: device create file fail\n", __func__);
		goto exit;
	}

	pr_info("[MME] %s: probe done\n", __func__);
	return 0;

exit:
	pr_err("[MME] %s: probe fail - %d\n", __func__, ret);
	return ret;
}

static int lge_mme_remove(struct platform_device *pdev)
{
	return 0;
}

/* device driver structures */
static struct of_device_id mme_match_table[] = {
	{ .compatible = "lge-mme-drv",},
	{},
};

static struct platform_driver lge_mme_driver __refdata = {
	.probe = lge_mme_probe,
	.remove = lge_mme_remove,
	.driver = {
		.name = "lge_mme_drv",
		.owner = THIS_MODULE,
		.of_match_table = mme_match_table,
	},
};

/* driver init funcion */
static int __init lge_mme_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&lge_mme_driver);
	if (ret < 0)
		pr_err("[MME] %s: platform_driver_register() err=%d\n", __func__, ret);

	return ret;
}

/* driver exit function */
static void __exit lge_mme_exit(void)
{
	platform_driver_unregister(&lge_mme_driver);
}

module_init(lge_mme_init);
module_exit(lge_mme_exit);

MODULE_DESCRIPTION("LGE MME kernel driver for LG pay");
MODULE_AUTHOR("jinsol.jo <jinsol.jo@lge.com>");
MODULE_LICENSE("GPL");
