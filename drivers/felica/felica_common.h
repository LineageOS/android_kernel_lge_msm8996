/*
 *  felicacommon.h
 *
 */

#ifndef __FELICACOMMON_H__
#define __FELICACOMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  INCLUDE FILES FOR MODULE
 */
#include <linux/module.h>/*THIS_MODULE*/
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>/* printk() */
#include <linux/types.h>/* size_t */
#include <linux/miscdevice.h>/*misc_register, misc_deregister*/
#include <linux/vmalloc.h>
#include <linux/fs.h>/*file_operations*/
#include <linux/delay.h>/*mdelay*/
#include <linux/irq.h>

#include <asm/uaccess.h>/*copy_from_user*/
#include <asm/io.h>/*static*/
#include <linux/gpio.h>
#include <soc/qcom/socinfo.h>

/*
 *  DEFINE
 */

enum{
  FELICA_UART_NOTAVAILABLE = 0,
  FELICA_UART_AVAILABLE,
};

/* debug message */
#define FEATURE_DEBUG_LOW
#define FELICA_DEBUG_MSG printk

/* felica_pon */
#define FELICA_PON_NAME    "control_pon"

/* minor number */
#define MINOR_NUM_FELICA_PON 250

#ifdef __cplusplus
}
#endif

#endif // __FELICACOMMON_H__
