/* arch/arm/mach-msm/lge/lge_pre_selfd.c
 *
 * Interface to calibrate display color temperature.
 *
 * Copyright (C) 2012 LGE
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <soc/qcom/lge/board_lge.h>

static struct pre_selfd_platform_data *ppre_selfd_pdata;
static char pre_selfd_buf[4096];
unsigned int selfd_cnt= 0;


int pre_selfd_set_values(int kcal_r, int kcal_g, int kcal_b)
{
	return 0;
}

static int pre_selfd_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	return 0;
}

static struct pre_selfd_platform_data pre_selfd_pdata = {
	.set_values = pre_selfd_set_values,
	.get_values = pre_selfd_get_values,
};


static struct platform_device pre_selfd_platrom_device = {
	.name   = "pre_selfd_ctrl",
	.dev = {
		.platform_data = &pre_selfd_pdata,
	}
};
/*
void __init lge_add_pre_selfd_devices(void)
{
	pr_info(" PRE_SELFD_DEBUG : %s\n", __func__);
	platform_device_register(&pre_selfd_platrom_device);
}
*/
static int __init pre_selfd_init(void)
{
	return platform_device_register(&pre_selfd_platrom_device);
}
//module_init(pre_selfd_init);



static ssize_t pre_selfd_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    char str[512];
    int str_len = 0;
    if ( !lge_get_factory_boot() ) return -EINVAL;

    if ( buf[0] =='$' )
    {
        memset(pre_selfd_buf, 0, 4096); selfd_cnt = 0;
        return -EINVAL;
    }
    str_len = sprintf(str, "%s", buf);
    if ( (selfd_cnt+str_len) > 4096 )
    {
        sprintf((char *) &pre_selfd_buf[selfd_cnt], "$");
        selfd_cnt++;
        return selfd_cnt;
    }

    sprintf((char *) &pre_selfd_buf[selfd_cnt], "%s", buf);
    selfd_cnt += str_len;

    return selfd_cnt;
}

static ssize_t pre_selfd_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    ssize_t ret_len = 0;
    ret_len = snprintf(buf, PAGE_SIZE, "%s", pre_selfd_buf);
    return ret_len;

}

static DEVICE_ATTR(pre_selfd, 0644, pre_selfd_show, pre_selfd_store);

int lge_pre_self_diagnosis(char *drv_bus_code, int func_code, char *dev_code, char *drv_code, int errno)
{
    char str[512];
    int str_len = 0;
    if ( !lge_get_factory_boot() ) return 1;

    str_len = sprintf(str, "%s|%d|%s|%s|%d\n", drv_bus_code, func_code, dev_code, drv_code, errno);
    if ( (selfd_cnt+str_len) > 4096 )
    {
        sprintf((char *) &pre_selfd_buf[selfd_cnt], "$");
        selfd_cnt++;
        return 1;
    }

    sprintf((char *) &pre_selfd_buf[selfd_cnt], "%s|%d|%s|%s|%d\n", drv_bus_code, func_code, dev_code, drv_code, errno);
    selfd_cnt += str_len;

    return 0;
}

static int pre_selfd_ctrl_probe(struct platform_device *pdev)
{
    int rc = 0;

    ppre_selfd_pdata = pdev->dev.platform_data;

    rc = device_create_file(&pdev->dev, &dev_attr_pre_selfd);
    if (rc != 0)
        return -1;
    return 0;
}

static struct platform_driver this_driver = {
    .probe  = pre_selfd_ctrl_probe,
    .driver = {
        .name   = "pre_selfd_ctrl",
    },
};

int __init pre_selfd_ctrl_init(void)
{
    return platform_driver_register(&this_driver);
}

device_initcall(pre_selfd_init);
device_initcall(pre_selfd_ctrl_init);


MODULE_DESCRIPTION("LGE Pre Self Diagnosis driver");
MODULE_LICENSE("GPL v2");
