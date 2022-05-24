/* FPC1021a Area sensor driver
 *
 * Copyright (c) 2013 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/platform_device.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/of.h>
#endif

#include <linux/input.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>
#include <linux/irqchip/msm-gpio-irq.h>
#include <linux/irqchip/msm-mpm-irq.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include "qseecom_kernel.h"
#include <soc/qcom/scm.h>

#include "fpc_btp.h"
#include "fpc_btp_regs.h"
#include "fpc_log.h"

#define INTERRUPT_INPUT_REPORT

/* #define FEATURE_FPC_USE_XO */
/* #define FEATURE_FPC_USE_PINCTRL */

#ifdef FEATURE_FPC_USE_PINCTRL
#include <linux/pinctrl/consumer.h>
#endif

#if defined(SUPPORT_TRUSTZONE)
#include <soc/qcom/scm.h>
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC_BTP area sensor driver.");

/* -------------------------------------------------------------------- */
/* fpc_btp data types                                                   */
/* -------------------------------------------------------------------- */
struct fpc_test_result {
	int  pass;
	int  value;
};

struct fpc_btp_data_t {
	struct device          *dev;
	struct input_dev       *input;
	u32                    cs_gpio;
	u32                    reset_gpio;
	u32                    ldo_en;
	u32                    irq_gpio;
	u32                    qup_id;
	int                    irq;
	int		       pipe_owner;
	int                    select_checkerboard;
	int                    testmode;
	struct qseecom_handle  *qseecom_handle;
	struct mutex           mutex;

	/* factory result */
	struct fpc_test_result spi_result;
	struct fpc_test_result zone_result;
	struct fpc_test_result checkerbd_result;
	struct fpc_test_result rubber_result;
	bool                   power_on;
	struct fpc_btp_platform_data *platform_pdata;
	struct wake_lock       cmd_wake_lock;
};

#pragma pack(push, fpc, 1)
struct fpc_btp_qseecom_req {
	u32 cmd_id;
	u32 data;
	u32 data2;
	u32 len;
	u32 start_pkt;
	u32 end_pkt;
	u32 test_buf_size;
};

struct fpc_btp_qseecom_resp {
	int32_t data;
	int32_t status;
};
#pragma pack(pop, fpc)


/* -------------------------------------------------------------------- */
/* fpc_btp driver constants                                             */
/* -------------------------------------------------------------------- */
#define FPC_BTP_HWID_A           0x022a
#define FPC_BTP_HWID_B           0x0111

#define FPC_BTP_DEV_NAME         "btp"
#define FPC_BTP_RESET_RETRIES    2

#define FPC_BTP_SUPPLY_1V8       1800000UL
#define FPC_BTP_VOLTAGE_MIN      FPC_BTP_SUPPLY_1V8
#define FPC_BTP_VOLTAGE_MAX      FPC_BTP_SUPPLY_1V8
#define FPC_BTP_LOAD_UA          7000


#define FPC_BTP_TESTMODE_ENABLE                 1
#define FPC_BTP_TESTMODE_DISABLE                0
#define FPC_BTP_TEST_RESULT_FAIL                (-1)
#define FPC_BTP_TEST_RESULT_PASS                0
#define FPC_BTP_TEST_INVALID                    (-1)

/* qseecom cmd */
#define CLIENT_CMD_STARTUP                       10000
#define CLIENT_CMD_SHUTDOWN                      10001

/* predefined cmd with tz app */
#define CLIENT_CMD_WAIT_FINGER                   0
#define CLIENT_CMD_SLEEP_SENSOR                  1

#define CLIENT_CMD_SPI_TEST                      100
#define CLIENT_CMD_ZONE_TEST                     101
#define CLIENT_CMD_CHECKERBOARD_TEST             102
#define CLIENT_CMD_RUBBER_TEST                   103
#define CLIENT_CMD_GET_FINGER_COUNT              104
#define CLIENT_CMD_SET_HW_REVISION               1000
#define CLIENT_CMD_SET_QUP_ID                    1001


#define QSEECOM_SBUFF_SIZE                       64*10
#define FPC_TZAPP_NAME                           "csfp_app"

#define TZ_BLSP_MODIFY_OWNERSHIP_ID		3
#define AC_TZ					1
#define AC_HLOS					3
#ifdef INTERRUPT_INPUT_REPORT
#define FPC_BTP_INTERRUPT                        REL_MISC
#endif

/* -------------------------------------------------------------------- */
/* function prototypes                                                  */
/* -------------------------------------------------------------------- */

static int __init fpc_btp_init(void);
static void __exit fpc_btp_exit(void);
static int fpc_btp_probe(struct platform_device *pdev);
static int fpc_btp_remove(struct platform_device *spi);
static int fpc_btp_suspend(struct device *dev);
static int fpc_btp_resume(struct device *dev);

static int fpc_btp_cleanup(struct fpc_btp_data_t *fpc_btp,
					struct platform_device *spidev);
static int fpc_btp_reset_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata);
static int fpc_btp_ldo_en_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata);
static int fpc_btp_cs_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata);
static int fpc_btp_irq_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata);
static int fpc_btp_get_of_pdata(struct device *dev,
					struct fpc_btp_platform_data *pdata);
static int fpc_btp_gpio_reset(struct fpc_btp_data_t *fpc_btp);
static int fpc_btp_sleep(struct fpc_btp_data_t *fpc_btp, bool deep_sleep);
static irqreturn_t fpc_btp_interrupt(int irq, void *_fpc_btp);
static int fpc_btp_regulator_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata);
static int fpc_btp_regulator_set(struct fpc_btp_data_t *fpc_btp, bool enable);
static int fpc_btp_ldo_en_set(struct fpc_btp_data_t *fpc_btp, bool enable); 

static int spi_change_pipe_owner(struct fpc_btp_data_t *fpc_btp, bool to_tz);

/* -------------------------------------------------------------------- */
/* External interface                                                   */
/* -------------------------------------------------------------------- */
module_init(fpc_btp_init);
module_exit(fpc_btp_exit);

static const struct dev_pm_ops fpc_btp_pm = {
	.suspend = fpc_btp_suspend,
	.resume = fpc_btp_resume
};

#ifdef CONFIG_OF
static struct of_device_id fpc_btp_of_match[] = {
	{ .compatible = "fpc,btp", },
	{}
};

/* MODULE_DEVICE_TABLE(of, fpc_btp_of_match); */
#endif

static struct platform_driver fpc_btp_driver = {
	.driver = {
		.name	= FPC_BTP_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm     = &fpc_btp_pm,
#ifdef CONFIG_OF
		.of_match_table = fpc_btp_of_match,
#endif
	},
	.probe	= fpc_btp_probe,
	.remove	= fpc_btp_remove,
};


/* -------------------------------------------------------------------- */
/* function definitions                                                 */
/* -------------------------------------------------------------------- */
static int __init fpc_btp_init(void)
{
	if (platform_driver_register(&fpc_btp_driver))
		return -EINVAL;

	return 0;
}

/* -------------------------------------------------------------------- */
static void __exit fpc_btp_exit(void)
{
	PINFO("enter");

	platform_driver_unregister(&fpc_btp_driver);
}


static void get_cmd_rsp_buffers(struct qseecom_handle *handle,
	void **cmd, int *cmd_len, void **resp, int *resp_len)
{
	*cmd = handle->sbuf;
	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);

	*resp = handle->sbuf + *cmd_len;
	if (*resp_len & QSEECOM_ALIGN_MASK)
		*resp_len = QSEECOM_ALIGN(*resp_len);
}


static int fpc_btp_qseecom_cmd(struct fpc_btp_data_t *fpc_btp,
		u32 cmd, u32 data, struct fpc_test_result *result)
{
	int res = 0;
	struct fpc_btp_qseecom_req *req;
	struct fpc_btp_qseecom_resp *resp;
	int req_len, resp_len;

	mutex_lock(&fpc_btp->mutex);
	PINFO("qseecom cmd(%d)", cmd);

	if( (cmd != CLIENT_CMD_STARTUP)
		&& (cmd != CLIENT_CMD_SHUTDOWN) ) {
		if(fpc_btp->qseecom_handle == NULL) {
			PINFO("qseecom handle is NULL!!");
			res = -1;
			goto unlock;
		}
	}

	switch(cmd) {
	case CLIENT_CMD_STARTUP:
		if(fpc_btp->qseecom_handle != NULL) {
			PINFO("already opened qseecom");
			res = 0;
			goto unlock;
		}
		res = qseecom_start_app(&fpc_btp->qseecom_handle,
				FPC_TZAPP_NAME, QSEECOM_SBUFF_SIZE);
		break;

	case CLIENT_CMD_SHUTDOWN:
		if(fpc_btp->qseecom_handle == NULL) {
			PINFO("qseecom handle is NULL!!");
			res = 0;
			goto unlock;
		}
		res = qseecom_shutdown_app(&fpc_btp->qseecom_handle);
		break;

	case CLIENT_CMD_SPI_TEST:
	case CLIENT_CMD_ZONE_TEST:
	case CLIENT_CMD_CHECKERBOARD_TEST:
	case CLIENT_CMD_RUBBER_TEST:
	case CLIENT_CMD_SET_HW_REVISION:
	case CLIENT_CMD_SET_QUP_ID:
	case CLIENT_CMD_WAIT_FINGER:
	case CLIENT_CMD_GET_FINGER_COUNT:
		if(result == NULL) {
			PERR("response buf null");
			goto unlock;
		}
		req_len = sizeof(struct fpc_btp_qseecom_req);
		resp_len = sizeof(struct fpc_btp_qseecom_resp);
		get_cmd_rsp_buffers(fpc_btp->qseecom_handle,
				(void **)&req, &req_len,
				(void **)&resp, &resp_len);
		req->cmd_id = cmd;
		req->data = data;

		res = qseecom_send_command(fpc_btp->qseecom_handle,
				(void *)req, req_len,
				(void *)resp, resp_len);
		if(res < 0) {
			PERR("cmd(%d) qseecom fail", cmd);
			goto unlock;
		}
		result->value = resp->data;
		result->pass = resp->status;
		break;

	default:
		PERR("not suppport cmd(%d)!!", cmd);
		res = -1;
		break;
	}

unlock:
	mutex_unlock(&fpc_btp->mutex);
	return res;
}


/* -------------------------------------------------------------------- */
static int fpc_btp_testmode_enable(struct fpc_btp_data_t *fpc_btp)
{
	int res;

	/* Start the TZ app */
	res = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_STARTUP, 0, NULL);
	if (res < 0) {
		PERR("startup fail!!");
		return res;
	}
	fpc_btp->testmode = 1;
	PINFO("startup success!!");

	return res;
}

static int fpc_btp_testmode_disable(struct fpc_btp_data_t *fpc_btp)
{
	int res;

	/* shutdown the TZ app */
	res = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_SHUTDOWN, 0, NULL);
	if (res < 0) {
		PERR("shutdown fail!!");
		return res;
	}
	fpc_btp->qseecom_handle = NULL;
	fpc_btp->testmode = 0;

	PERR("shutdown success!!");

	return res;
}

/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_spitest(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	PINFO("start spi test");

	fpc_btp->spi_result.pass = FPC_BTP_TEST_RESULT_FAIL;
	fpc_btp->spi_result.value = FPC_BTP_TEST_INVALID;

	if(!fpc_btp->testmode) {
		PERR("not test mode");
		return count;
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_SPI_TEST,
				0, &fpc_btp->spi_result);
	if(error) {
		PERR("qseecom cmd failed.");
		return count;
	}

	if(fpc_btp->spi_result.pass < 0) {
		PERR("spi test failed, status(%d)", fpc_btp->spi_result.pass);
		return count;
	}

	if((fpc_btp->spi_result.value == FPC_BTP_HWID_A)
		|| (fpc_btp->spi_result.value == FPC_BTP_HWID_B)){
		fpc_btp->spi_result.pass = FPC_BTP_TEST_RESULT_PASS;
	}
	else {
		fpc_btp->spi_result.pass = FPC_BTP_TEST_RESULT_FAIL;
		PERR("spi test failed, hwid[0x%x]", fpc_btp->spi_result.value);
		return count;
	}

	PINFO("spi test success, hwid[0x%x]", fpc_btp->spi_result.value);

	return count;
}


static ssize_t fpc_btp_show_spitest(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d\n", fpc_btp->spi_result.pass,
				fpc_btp->spi_result.value);
}

static DEVICE_ATTR(spitest, S_IRUGO | S_IWUSR,
		   fpc_btp_show_spitest, fpc_btp_store_spitest);


/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_zonetest(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	PINFO("start zone test:%ld", val);
	fpc_btp->zone_result.pass = FPC_BTP_TEST_RESULT_FAIL;
	fpc_btp->zone_result.value = FPC_BTP_TEST_INVALID;

	if(!fpc_btp->testmode) {
		PERR("not test mode");
		return count;
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_ZONE_TEST,
					0, &fpc_btp->zone_result);
	if(error) {
		PERR("qseecom cmd failed");
		return count;
	}

	if(fpc_btp->zone_result.pass < 0) {
		PERR("zone test failed, status(%d)", fpc_btp->zone_result.pass);
		return count;
	}

	fpc_btp->zone_result.pass = FPC_BTP_TEST_RESULT_PASS;
	PINFO("zone test success, bed pixel count(%d)",
				fpc_btp->zone_result.value);

	return count;
}


static ssize_t fpc_btp_show_zonetest(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	PINFO("enter");

	return sprintf(buf, "%d %d\n", fpc_btp->zone_result.pass,
					fpc_btp->zone_result.value);
}


static DEVICE_ATTR(zonetest, S_IRUGO | S_IWUSR,
		   fpc_btp_show_zonetest, fpc_btp_store_zonetest);

/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_checkerbdtest(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	PINFO("start checkerboard test:%ld", val);
	fpc_btp->zone_result.pass = FPC_BTP_TEST_RESULT_FAIL;
	fpc_btp->zone_result.value = FPC_BTP_TEST_INVALID;

	if(!fpc_btp->testmode) {
		PERR("not test mode");
		return count;
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_CHECKERBOARD_TEST,
					0, &fpc_btp->checkerbd_result);
	if(error) {
		PERR("checkerboard qseecom cmd failed");
		return count;
	}

	if(fpc_btp->checkerbd_result.pass < 0) {
		PERR("checkerboard test failed, status(%d)",
			fpc_btp->checkerbd_result.pass);
		return count;
	}

	fpc_btp->checkerbd_result.pass = FPC_BTP_TEST_RESULT_PASS;
	PINFO("checkerbd test sucess, bed pixel count:%d",
			fpc_btp->checkerbd_result.value);

	return count;
}



static ssize_t fpc_btp_show_checkerbdtest(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	PINFO("enter");

	return sprintf(buf, "%d %d\n", fpc_btp->checkerbd_result.pass,
					fpc_btp->checkerbd_result.value);
}



static DEVICE_ATTR(checkerbdtest, S_IRUGO | S_IWUSR,
		   fpc_btp_show_checkerbdtest, fpc_btp_store_checkerbdtest);


/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_testmode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);


	PINFO("test mode:%ld", val);


	if(val == fpc_btp->testmode) {
		return count;
	}

	if(val==FPC_BTP_TESTMODE_ENABLE) {
		PINFO("test mode enable");
		fpc_btp_testmode_enable(fpc_btp);
	}
	else if(val==FPC_BTP_TESTMODE_DISABLE) {
		PINFO("test mode disable");
		fpc_btp_testmode_disable(fpc_btp);
	}
	else {
		PERR("please check testmode arg[%ld]", val);
	}
	return count;
}


static ssize_t fpc_btp_show_testmode(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", fpc_btp->testmode);
}


static DEVICE_ATTR(testmode, S_IRUGO | S_IWUSR,
		   fpc_btp_show_testmode, fpc_btp_store_testmode);


/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_qup_id(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	PINFO("blsp store");

	if(!fpc_btp->testmode) {
		PERR("not test mode");
		return count;
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_SET_QUP_ID,
					fpc_btp->qup_id, &fpc_btp->checkerbd_result);
	if(error) {
		PERR("qseecom cmd failed, cmd(%d)", CLIENT_CMD_SET_QUP_ID);
		return count;
	}
	return count;
}


static ssize_t fpc_btp_show_qup_id(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", fpc_btp->qup_id);
}


static DEVICE_ATTR(qup_id, S_IRUGO | S_IWUSR,
		   fpc_btp_show_qup_id, fpc_btp_store_qup_id);
/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_rubbertest(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	fpc_btp->rubber_result.pass = FPC_BTP_TEST_RESULT_FAIL;
	fpc_btp->rubber_result.value = FPC_BTP_TEST_INVALID;

	PINFO("rubber stamp test");

	if(!fpc_btp->testmode) {
		PERR("not test mode");
		return count;
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_RUBBER_TEST,
					0, &fpc_btp->rubber_result);
	if(error) {
		PERR("qseecom cmd failed, rubber test!!");
		return count;
	}

	if(fpc_btp->rubber_result.pass < 0) {
		PERR("rubber test failed, status(%d)",
			fpc_btp->rubber_result.pass);
		return count;
	}
	fpc_btp->rubber_result.pass = FPC_BTP_TEST_RESULT_PASS;
	PINFO("rubber test sucess, point(%d)",
			fpc_btp->rubber_result.value);

	return count;
}


static ssize_t fpc_btp_show_rubbertest(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d\n", fpc_btp->rubber_result.pass,
				fpc_btp->rubber_result.value);

}


static DEVICE_ATTR(rubbertest, S_IRUGO | S_IWUSR,
		   fpc_btp_show_rubbertest, fpc_btp_store_rubbertest);


/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_show_intstatus(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "not support\n");
}

static DEVICE_ATTR(intstatus, S_IRUGO,
		   fpc_btp_show_intstatus, NULL);


/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_waitfinger(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	struct fpc_test_result result;

	result.pass = FPC_BTP_TEST_RESULT_FAIL;
	result.value = FPC_BTP_TEST_INVALID;

	PINFO("wait finger");

	if(!fpc_btp->testmode) {
		PERR("not test mode");
		return count;
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_WAIT_FINGER,
					0, &result);
	if(error) {
		PERR("qseecom cmd failed, wait finger!!");
		return count;
	}

	return count;
}


static DEVICE_ATTR(waitfinger, S_IWUSR,
		   NULL, fpc_btp_store_waitfinger);

/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_show_fingercount(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	struct fpc_test_result result;

	result.pass = FPC_BTP_TEST_RESULT_FAIL;
	result.value = FPC_BTP_TEST_INVALID;

	PINFO("finger count");

	if (!fpc_btp->testmode) {
		PERR("not test mode");
		return sprintf(buf, "-1 -1\n");
	}

	error = fpc_btp_qseecom_cmd(fpc_btp, CLIENT_CMD_GET_FINGER_COUNT,
					0, &result);
	if (error)
        PINFO("qseecom cmd failed, wait finger!!");
	else
        PERR("finger down count:(%d), statsu(%d)", result.value, result.pass);

	return sprintf(buf, "%d %d\n", result.pass,
				result.value);
}

static DEVICE_ATTR(fingercount, S_IRUGO,
		   fpc_btp_show_fingercount, NULL);

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_spi_prepare_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t fpc_btp_show_spi_prepare(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0 \n");
}

static DEVICE_ATTR(spi_prepare, S_IRUGO | S_IWUSR,
		fpc_btp_show_spi_prepare, fpc_btp_store_spi_prepare_set);
#if 0
/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_regulator(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	bool enable;

	if(*buf == '1')
		enable = true;
	else
		enable = false;

	if(fpc_btp_regulator_set(fpc_btp,  enable) < 0)
		PERR("regulator (%d) fail", enable);

	return count;
}

static ssize_t fpc_btp_show_regulator(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	return sprintf(buf, "%d \n", fpc_btp->power_on);
}

static DEVICE_ATTR(regulator, S_IRUGO | S_IWUSR,
		fpc_btp_show_regulator, fpc_btp_store_regulator);

/* -------------------------------------------------------------------- */
static ssize_t fpc_btp_store_gpio(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);
	int error;

	if(*buf == '1' && !fpc_btp->power_on) {
		error = regulator_enable(fpc_btp->platform_pdata->vreg);
		if(error < 0) {
			PERR("regulator enable fail");
			return count;
		}

		if((error = fpc_btp_gpio_reset(fpc_btp)))
			PERR("reset gpio init fail");

		enable_irq(fpc_btp->irq);
		fpc_btp->power_on = true;
	}
	else if(*buf == '0' && fpc_btp->power_on) {
		disable_irq(fpc_btp->irq);
		gpio_direction_output(fpc_btp->reset_gpio, 0);
		error = regulator_disable(fpc_btp->platform_pdata->vreg);
		if(error < 0) {
			PERR("regulator disable fail");
			return count;
		}
		fpc_btp->power_on = false;
	}

	return count;
}

static ssize_t fpc_btp_show_gpio(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	return sprintf(buf, "%d \n", fpc_btp->power_on);
}

static DEVICE_ATTR(gpio, S_IRUGO | S_IWUSR,
		fpc_btp_show_gpio, fpc_btp_store_gpio);
#endif
/* -------------------------------------------------------------------- */

static struct attribute *fpc_btp_attributes[] = {
	&dev_attr_fingercount.attr,
	&dev_attr_waitfinger.attr,
	&dev_attr_intstatus.attr,
	&dev_attr_rubbertest.attr,
	&dev_attr_qup_id.attr,
	&dev_attr_spi_prepare.attr,
	&dev_attr_testmode.attr,
	&dev_attr_spitest.attr,
	&dev_attr_zonetest.attr,
	&dev_attr_checkerbdtest.attr,
/*	&dev_attr_regulator.attr,
	&dev_attr_gpio.attr,*/
	NULL
};


static const struct attribute_group fpc_btp_attr_group = {
	.attrs = fpc_btp_attributes,
};


/* -------------------------------------------------------------------- */
static int fpc_btp_probe(struct platform_device *pdev)
{
	struct fpc_btp_platform_data *fpc_btp_pdata;
	struct device *dev = &pdev->dev;
	int error = 0;
	struct fpc_btp_data_t *fpc_btp = NULL;

	PDEBUG("enter");

	fpc_btp = devm_kzalloc(dev, sizeof(*fpc_btp), GFP_KERNEL);
	if (!fpc_btp) {
		PERR("failed to allocate memory for struct fpc_btp_data");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, fpc_btp);
	fpc_btp->dev = dev;
	fpc_btp->reset_gpio = -EINVAL;
	fpc_btp->ldo_en     = -EINVAL;
	fpc_btp->irq_gpio   = -EINVAL;
	fpc_btp->cs_gpio    = -EINVAL;
	fpc_btp->irq        = -EINVAL;
	fpc_btp->qup_id     = -EINVAL;
	fpc_btp->testmode = 0;
	fpc_btp->qseecom_handle = NULL;
	mutex_init(&fpc_btp->mutex);
	wake_lock_init(&fpc_btp->cmd_wake_lock, WAKE_LOCK_SUSPEND, "csfp_wakelock");
	fpc_btp->platform_pdata = NULL;

	if(pdev->dev.of_node) {
		fpc_btp_pdata = devm_kzalloc(dev, sizeof(*fpc_btp_pdata), GFP_KERNEL);
		if (!fpc_btp_pdata) {
			PERR("Failed to allocate memory");
			return -ENOMEM;
		}

		error = fpc_btp_get_of_pdata(dev, fpc_btp_pdata);
		if (error)
			goto err;

		pdev->dev.platform_data = fpc_btp_pdata;
		fpc_btp->platform_pdata = fpc_btp_pdata;

	} else {
		fpc_btp_pdata = pdev->dev.platform_data;
		fpc_btp->platform_pdata = pdev->dev.platform_data;
	}

	fpc_btp->qup_id = fpc_btp_pdata->qup_id;

	if (fpc_btp->platform_pdata->use_regulator) {
		if((error = fpc_btp_regulator_init(fpc_btp, fpc_btp_pdata)) < 0)
			goto err;

		if((error = fpc_btp_regulator_set(fpc_btp, true)))
			goto err;
	} else {
		if((error = fpc_btp_ldo_en_init(fpc_btp, fpc_btp_pdata)))
			goto err;

		if((error = fpc_btp_ldo_en_set(fpc_btp, true)))
			goto err;
	}
	if((error = fpc_btp_reset_init(fpc_btp, fpc_btp_pdata)))
		goto err;

	if((error = fpc_btp_cs_init(fpc_btp, fpc_btp_pdata)))
		goto err;

	if((error = fpc_btp_irq_init(fpc_btp, fpc_btp_pdata)))
		goto err;

	if((error = fpc_btp_gpio_reset(fpc_btp)))
		goto err;


	/* register input device */
	fpc_btp->input = input_allocate_device();
	if(!fpc_btp->input) {
		PERR("input_allocate_deivce failed.");
		error = -ENOMEM;
		goto err;
	}

	fpc_btp->input->name = "fingerprint";
	fpc_btp->input->dev.init_name = "lge_fingerprint";

#ifdef INTERRUPT_INPUT_REPORT
	input_set_capability(fpc_btp->input, EV_REL, FPC_BTP_INTERRUPT);

	error = devm_request_threaded_irq(dev, fpc_btp->irq, NULL,
			fpc_btp_interrupt,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"fpc_btp", fpc_btp);

	if (error) {
		PERR("request_irq %i failed.", fpc_btp->irq);
		fpc_btp->irq = -EINVAL;
		goto err;
	}
	disable_irq(fpc_btp->irq);
	enable_irq(fpc_btp->irq);
#endif
	input_set_drvdata(fpc_btp->input, fpc_btp);
	error = input_register_device(fpc_btp->input);
	if(error) {
		PERR("input_register_device failed.");
		input_free_device(fpc_btp->input);
		goto err;
	}

	if(sysfs_create_group(&fpc_btp->input->dev.kobj, &fpc_btp_attr_group)) {
		PERR("sysfs_create_group failed.");
		goto err_sysfs;
	}

	spi_change_pipe_owner(fpc_btp, true);
	PINFO("done!!");

	return 0;

err_sysfs:
	input_free_device(fpc_btp->input);
	input_unregister_device(fpc_btp->input);

err:
	mutex_destroy(&fpc_btp->mutex);
	fpc_btp_cleanup(fpc_btp, pdev);
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_remove(struct platform_device *pdev)
{
	struct fpc_btp_data_t *fpc_btp = platform_get_drvdata(pdev);

	PINFO("enter");

	fpc_btp_sleep(fpc_btp, true);

	sysfs_remove_group(&fpc_btp->input->dev.kobj, &fpc_btp_attr_group);
	input_free_device(fpc_btp->input);
	input_unregister_device(fpc_btp->input);

	fpc_btp_cleanup(fpc_btp, pdev);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc_btp_suspend(struct device *dev)
{

	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	PINFO("enter");

	disable_irq(fpc_btp->irq);
	gpio_direction_output(fpc_btp->reset_gpio, 0);

	if(fpc_btp->platform_pdata->use_regulator) {
		if(fpc_btp_regulator_set(fpc_btp, false) < 0)
			PERR("reguator off fail");
	} else {
		if(fpc_btp_ldo_en_set(fpc_btp, false) < 0)
			PERR("ldo_en off fail");
	}

	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_resume(struct device *dev)
{
	struct fpc_btp_data_t *fpc_btp = dev_get_drvdata(dev);

	PINFO("enter");

	if(fpc_btp->platform_pdata->use_regulator) {
		if(fpc_btp_regulator_set(fpc_btp, true) < 0)
			PERR("reguator on fail");
	} else {
		if(fpc_btp_ldo_en_set(fpc_btp, true) < 0)
			PERR("ldo_en on fail");
	}

	if(fpc_btp_gpio_reset(fpc_btp))
		PINFO("reset gpio init fail");

	enable_irq(fpc_btp->irq);

	return 0;
}

/* -------------------------------------------------------------------- */
static int
fpc_btp_cleanup(struct fpc_btp_data_t *fpc_btp, struct platform_device *pdev)
{
	PINFO("enter");

	wake_lock_destroy(&fpc_btp->cmd_wake_lock);

#ifdef INTERRUPT_INPUT_REPORT
	if (fpc_btp->irq)
		devm_free_irq(&pdev->dev, fpc_btp->irq, fpc_btp);
#else
	if (fpc_btp->irq)
		free_irq(fpc_btp->irq, fpc_btp);
#endif
	if (gpio_is_valid(fpc_btp->irq_gpio))
		gpio_free(fpc_btp->irq_gpio);

	if (gpio_is_valid(fpc_btp->reset_gpio))
		gpio_free(fpc_btp->reset_gpio);

	if (fpc_btp->platform_pdata->use_regulator) {
		//regulator_put(fpc_btp->platform_pdata->vreg);
	} else {
		if (gpio_is_valid(fpc_btp->ldo_en))
			gpio_free(fpc_btp->ldo_en);
	}

	//spi_set_drvdata(spidev, NULL);

	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_ldo_en_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata)
{
	int error = 0;

	PINFO("enter");
	fpc_btp->power_on = false;

	if (gpio_is_valid(pdata->ldo_en)) {

		PINFO("Assign HW ldo_en -> GPIO%d",
				 pdata->ldo_en);

		error = gpio_request(pdata->ldo_en, "fpc_btp_ldo_en");

		if (error) {
			PERR("gpio_request (ldo_en) failed.");
			return error;
		}

		fpc_btp->ldo_en = pdata->ldo_en;

		error = gpio_direction_output(fpc_btp->ldo_en, 1);

		if (error) {
			PERR("gpio_direction_output(ldo_en) failed.");
			return error;
		}
	} else {
		PERR("failed");
	}

	return error;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_reset_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata)
{
	int error = 0;

	PINFO("enter");

	if (gpio_is_valid(pdata->reset_gpio)) {

		PINFO("Assign HW reset -> GPIO%d",
				 pdata->reset_gpio);

		error = gpio_request(pdata->reset_gpio, "fpc_btp_reset");

		if (error) {
			PERR("gpio_request (reset) failed.");
			return error;
		}

		fpc_btp->reset_gpio = pdata->reset_gpio;

		error = gpio_direction_output(fpc_btp->reset_gpio, 1);

		if (error) {
			PERR("gpio_direction_output(reset) failed.");
			return error;
		}
	} else {
		PERR("failed");
	}

	return error;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_cs_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata)
{
	int error = 0;

	PINFO("enter");

	if (gpio_is_valid(pdata->cs_gpio)) {

		PINFO("Assign CS -> GPIO%d", pdata->cs_gpio);

		error = gpio_request(pdata->cs_gpio, "fpc_btp_cs");

		if (error) {
			PERR("gpio_request (cs) failed.");
			return error;
		}

		fpc_btp->cs_gpio = pdata->cs_gpio;

		error = gpio_direction_output(fpc_btp->cs_gpio, 0);

		if (error) {
			PERR("gpio_direction_output(cs) failed.");
			return error;
		}

	} else {
		PERR("failed");
	}

	return error;
}

/* -------------------------------------------------------------------- */
static int
fpc_btp_irq_init(struct fpc_btp_data_t *fpc_btp,
				 struct fpc_btp_platform_data *pdata)
{
	int error = 0;

	PINFO("enter");

	if (gpio_is_valid(pdata->irq_gpio)) {

		PINFO("Assign IRQ -> GPIO%d", pdata->irq_gpio);

		error = gpio_request(pdata->irq_gpio, "fpc_btp_irq");
		if (error) {
			PERR("gpio_request (irq) failed.");

			return error;
		}
		fpc_btp->irq_gpio = pdata->irq_gpio;

		error = gpio_direction_input(fpc_btp->irq_gpio);

		if (error) {
			PERR("gpio_direction_input (irq) failed.");
			return error;
		}
	} else {
		return -EINVAL;
	}

	fpc_btp->irq = gpio_to_irq(fpc_btp->irq_gpio);

	if (fpc_btp->irq < 0) {
		PERR("gpio_to_irq failed.");
		error = fpc_btp->irq;
		return error;
	}

#ifndef INTERRUPT_INPUT_REPORT
	error = request_irq(fpc_btp->irq, fpc_btp_interrupt,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"fpc_btp", fpc_btp);

	if (error) {
		PERR("request_irq %i failed.", fpc_btp->irq);
		fpc_btp->irq = -EINVAL;
		return error;
	}
#endif
	return error;
}

/* -------------------------------------------------------------------- */
#ifdef FEATURE_FPC_USE_XO
static int fpc_btp_spi_clk(struct fpc_btp_data_t *fpc_btp)
{
	int error = 0;
	struct clk *btp_clk;

	PINFO("enter");

	btp_clk = clk_get(&fpc_btp->spi->dev, "fpc_xo");
	if (IS_ERR(btp_clk)) {
		error = PTR_ERR(btp_clk);
		PERR("could not get clock");
		goto out_err;
	}

	/* We enable/disable the clock only to assure it works */
	error = clk_prepare_enable(btp_clk);
	if (error) {
		PERR("could not enable clock");
		goto out_err;
	}
	/* clk_disable_unprepare(btp_clk); */

out_err:
	return error;
}
#endif

/* -------------------------------------------------------------------- */
static int fpc_btp_get_of_pdata(struct device *dev,
					struct fpc_btp_platform_data *pdata)
{
	struct device_node *node = dev->of_node;

	u32 irq_prop = of_get_named_gpio(node, "fpc,gpio_irq",   0);
	u32 rst_prop = of_get_named_gpio(node, "fpc,gpio_reset", 0);
	u32 cs_prop  = of_get_named_gpio(node, "fpc,gpio_cs",    0);
	u32 ldo_prop = of_get_named_gpio(node, "fpc,ldo_en",    0);

	PINFO("enter");

	if (node == NULL) {
		PERR("Could not find OF device node");
		goto of_err;
	}

	if (!irq_prop || !rst_prop || !cs_prop) {
		PINFO("Missing OF property");
		goto of_err;
	}

	pdata->irq_gpio   = irq_prop;
	pdata->reset_gpio = rst_prop;
	pdata->cs_gpio    = cs_prop;
	pdata->ldo_en     = ldo_prop;
	if (of_property_read_u32(node, "use_regulator", &pdata->use_regulator)) {
		PERR("Missing OF property");
		goto of_err;
	}
	PINFO("use_regulator: %d", pdata->use_regulator);
	if (of_property_read_u32(node, "qcom,qup-id", &pdata->qup_id)) {
		PERR("Missing OF property");
		goto of_err;
	}
	PINFO("qup_id: %d", pdata->qup_id);

	return 0;

of_err:
	pdata->reset_gpio = -EINVAL;
	pdata->irq_gpio   = -EINVAL;
	pdata->cs_gpio    = -EINVAL;
	pdata->ldo_en     = -EINVAL;
	pdata->use_regulator = -EINVAL;
	pdata->qup_id     = -EINVAL;

	return -ENODEV;
}
/* -------------------------------------------------------------------- */
static int fpc_btp_regulator_init(struct fpc_btp_data_t *fpc_btp,
					struct fpc_btp_platform_data *pdata)
{
	int error = 0;
	struct regulator *vreg;

	PINFO("enter!!");

	fpc_btp->power_on = false;
	pdata->vreg = NULL;
	vreg = devm_regulator_get(fpc_btp->dev, "fpc,vddio");
	if (IS_ERR(vreg)) {
		error = PTR_ERR(vreg);
		PERR("Regulator get failed, error=%d", error);
		return error;
	}

	if (regulator_count_voltages(vreg) > 0) {
		error = regulator_set_voltage(vreg,
			FPC_BTP_VOLTAGE_MIN, FPC_BTP_VOLTAGE_MAX);
		if (error) {
			PERR("regulator set_vtg failed error=%d", error);
			goto err;
		}
	}

/* block regulator_set_optimum_mode()
   because of rock-bottem issue (delta 10mA under) after Post-CS migration */
#if 0
	if(regulator_count_voltages(vreg) > 0) {
		error = regulator_set_optimum_mode(vreg, FPC_BTP_LOAD_UA);
		if(error < 0) {
			PERR("unable to set current");
			goto err;
		}
	}
#endif
	pdata->vreg = vreg;
	return error;
err:
	//regulator_put(vreg);
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc_btp_regulator_set(struct fpc_btp_data_t *fpc_btp, bool enable)
{
	int error = 0;
	struct fpc_btp_platform_data *pdata = fpc_btp->platform_pdata;

	PINFO("power %s!!", (enable) ? "on" : "off");

	if(enable) {
		if(!fpc_btp->power_on)
			error = regulator_enable(pdata->vreg);
	} else {
		if(fpc_btp->power_on)
			error = regulator_disable(pdata->vreg);
	}

	if(error < 0)
		PERR("can't set(%d) regulator, error(%d)", enable, error);
	else
		fpc_btp->power_on = enable;

	return error;
}

#ifdef FEATURE_FPC_USE_PINCTRL
static int fpc_btp_pinctrl_init(struct fpc_btp_data_t *fpc_btp)
{
	struct pinctrl *fpc_pinctrl;
	struct pinctrl_state *gpio_state_suspend;

	fpc_pinctrl = devm_pinctrl_get(&(fpc_btp->spi->dev));

	if (IS_ERR_OR_NULL(fpc_pinctrl)) {
		PERR("Getting pinctrl handle failed");
		return -EINVAL;
	}
	gpio_state_suspend
		= pinctrl_lookup_state(fpc_pinctrl, "gpio_fpc_suspend");

	if (IS_ERR_OR_NULL(gpio_state_suspend)) {
		PERR("Failed to get the suspend state pinctrl handle");
		return -EINVAL;
	}

	if (pinctrl_select_state(fpc_pinctrl, gpio_state_suspend)) {
		PERR("error on pinctrl_select_state");
		return -EINVAL;
	} else {
		PERR("success to set pinctrl_select_state");
	}

	return 0;
}
#endif

/* -------------------------------------------------------------------- */
/* this never fails.*/
static int fpc_btp_ldo_en_set(struct fpc_btp_data_t *fpc_btp, bool enable)
{
	PINFO("ldo_en %s!!",(enable)?"on":"off");

	if(enable) {
		if(!fpc_btp->power_on)
			gpio_set_value(fpc_btp->ldo_en, 1);
	} else {
		if(fpc_btp->power_on)
			gpio_set_value(fpc_btp->ldo_en, 0);
	}
	fpc_btp->power_on = enable;
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_gpio_reset(struct fpc_btp_data_t *fpc_btp)
{
	int error = 0;
	int counter = FPC_BTP_RESET_RETRIES;

	while (counter) {
		counter--;

		/* gpio_set_value(fpc_btp->reset_gpio, 0); */
		gpio_direction_output(fpc_btp->reset_gpio, 0);
		udelay(1000);
		/* mdelay(2); */

		/* gpio_set_value(fpc_btp->reset_gpio, 1); */
		gpio_direction_output(fpc_btp->reset_gpio, 1);
		udelay(1250);
		/* mdelay(3); */

		error = gpio_get_value(fpc_btp->irq_gpio) ? 0 : -EIO;
		if (!error) {
			counter = 0;
		} else {
			PINFO("timed out,retrying ...");

			udelay(1250);
		}
	}

	return error;
}

/* -------------------------------------------------------------------- */
static int fpc_btp_sleep(struct fpc_btp_data_t *fpc_btp, bool deep_sleep)
{
	if (deep_sleep &&  gpio_is_valid(fpc_btp->reset_gpio)) {
		/* hyojin.an */
		/* Kenel panic because gpio_set_value(fpc_btp->cs_gpio, 0); */
		/* gpio_set_value(fpc_btp->reset_gpio, 0); */
		gpio_direction_output(fpc_btp->reset_gpio, 0);
		PDEBUG("reset_gpio -> 0");
	}

	if (deep_sleep && gpio_is_valid(fpc_btp->cs_gpio)) {
		/* gpio_set_value(fpc_btp->cs_gpio, 0); */
		gpio_direction_output(fpc_btp->cs_gpio, 0);
		PDEBUG("cs_gpio -> 0");
	}

	PDEBUG("sleep OK");

	return 0;
}

/* -------------------------------------------------------------------- */
static irqreturn_t fpc_btp_interrupt(int irq, void *_fpc_btp)
{
	struct fpc_btp_data_t *fpc_btp = _fpc_btp;
	int gpio_value;

	gpio_value = gpio_get_value(fpc_btp->irq_gpio);
	PINFO("interrupt:(%d)", gpio_value);

#ifdef INTERRUPT_INPUT_REPORT
	if(gpio_value) {
		input_report_rel(fpc_btp->input, FPC_BTP_INTERRUPT, 1);
		input_sync(fpc_btp->input);
	}
#endif
	return IRQ_HANDLED;
}
/* -------------------------------------------------------------------- */
/* Example of a change in BAM Pipe ownership */
static int spi_change_pipe_owner(struct fpc_btp_data_t *fpc_btp, bool to_tz)
{
       struct scm_desc desc; /* scm call descriptor */
       int ret;

       /* CMD ID to change BAM PIPE Owner*/
       desc.arginfo = SCM_ARGS(2);
       desc.args[0] = fpc_btp->qup_id;
       desc.args[1] = (to_tz) ? AC_TZ : AC_HLOS;

       /* scm_call failed: func id 0x2000403, arginfo: 0x2, args:0:10.439 */
       ret = scm_call2(SCM_SIP_FNID(SCM_SVC_TZ,
                                       TZ_BLSP_MODIFY_OWNERSHIP_ID), &desc);

       if(ret || desc.ret[0]) {
               PERR("ownership change failed!!");
               return -1;
       }

       /* set spi ownership flag */
       fpc_btp->pipe_owner = to_tz;

       return 0;
}
