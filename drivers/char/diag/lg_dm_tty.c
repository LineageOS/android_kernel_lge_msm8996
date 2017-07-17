/*
 * ODM TTY Driver for LGE DM router
 *
 * Youngjin Park <jin.park@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
*/

#ifdef CONFIG_LGE_DM_APP
#include <linux/list.h>
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include <linux/diagchar.h>
#ifdef CONFIG_DIAG_SDIO_PIPE
#include "diagfwd_sdio.h"
#endif

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
#include "diagfwd_hsic.h"
#include "diagfwd_smux.h"
#include "diagfwd_bridge.h"
#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/

#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <asm/current.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "diag_usb.h"
#include "diag_memorydevice.h"

#include "lg_dm_tty.h"
#include <soc/qcom/subsystem_restart.h>

#define CONFIG_TTY_PORT    /* used tty_port*/

#define DM_TTY_IOCTL_MAGIC        'J'
#define DM_TTY_MODEM_OPEN_SDM        _IOWR(DM_TTY_IOCTL_MAGIC, 0x01, short)
#define DM_TTY_MODEM_CLOSE_SDM        _IOWR(DM_TTY_IOCTL_MAGIC, 0x02, short)
#define DM_TTY_MODEM_OPEN_ODM        _IOWR(DM_TTY_IOCTL_MAGIC, 0x03, short)
#define DM_TTY_MODEM_CLOSE_ODM        _IOWR(DM_TTY_IOCTL_MAGIC, 0x04, short)
#define DM_TTY_MODEM_RESET            _IOWR(DM_TTY_IOCTL_MAGIC, 0x05, short)
#define DM_TTY_MODEM_CRASH            _IOWR(DM_TTY_IOCTL_MAGIC, 0x06, short)
#define DM_TTY_MODEM_DEBUGGER   _IOWR(DM_TTY_IOCTL_MAGIC, 0x07, char[300])

#define DM_TTY_MODULE_NAME        "DM_APP"
#define MAX_DM_TTY_DRV        1

#define TRUE 1
#define FALSE 0
#define MAX_SSR_REASON_LEN 81U

#define USER_SPACE_RAW_DATA_TYPE	0x00000080

/* packet header structure */
struct dm_router_header {
    short        dm_router_size;    /* size = header + body */
    char        dm_router_cmd;    /* command */
    char        dm_router_type;    /* type */
} __packed;

/* modem_response body structure */
struct dm_router_modem_response_body {
    short    modem_chip;        /* modem chip number */
    long long    local_time;        /* Linux local time */
} __packed;

/* modem chip */
enum {
    Primary_modem_chip = 1,
    Secondary_modem_chip = 2
};

/* TTY driver status */
enum {
    DM_TTY_INITIAL = 0,
    DM_TTY_REGISTERED = 1,
    DM_TTY_OPEN = 2,
    DM_TTY_CLOSED = 3,
};

/* packet type */
enum {
    DM_APP_REQUEST            = 0x01,
    DM_APP_RESPONSE        = 0x02,
    DM_APP_NOTICE            = 0x03
};

/* packet command */
enum {
    DM_APP_AUTH                        = 0x01,
    DM_APP_OPEN                        = 0x02,
    DM_APP_CLOSE                    = 0x03,
    DM_APP_MODEM_REQUEST        = 0x10,
    DM_APP_MODEM_RESPONSE        = 0x11
};

enum {
    DM_APP_ODM                    = 1,
    DM_APP_SDM                     = 2
};

struct dm_tty *lge_dm_tty;

#define DM_TTY_TX_MAX_PACKET_SIZE        40000     /*max size = 40000B */
#define DM_TTY_RX_MAX_PACKET_SIZE        9000     /* max size = 9000B */

/* modem_request packet */
char            *dm_modem_response;
int            dm_modem_response_length;

/* modem_response packet */
char            *dm_modem_request;
int            dm_modem_request_length;

/* modem_response header */
struct dm_router_header    *dm_modem_response_header;
int            dm_modem_response_header_length;

/* modem_request header */
struct dm_router_header    *dm_modem_request_header;
int            dm_modem_request_header_length;

/* body excluding real data */
struct dm_router_modem_response_body        *dm_modem_response_body;
int                    dm_modem_response_body_length;

short dm_rx_start_flag;
short dm_rx_end_flag;
#ifdef CONFIG_TTY_PORT /* Fu4.5 use tty_port */
static struct tty_port dm_tty_port;
#endif /* CONFIG_TTY_PORT */

char empty_mask_buff[] = {29, 28, 59, 126, 0, 120, -16, 126, 124, -109,
    73, 126, 28, -107, 42, 126, 12, 20, 58, 126, 99, -27, -95, 126,
    75, 15, 0, 0, -69, 96, 126, 75, 9, 0, 0, 98, -74, 126, 75, 8,
    0, 0, -66, -20, 126, 75, 8, 1, 0, 102, -11, 126, 75, 4, 0, 0,
    29, 73, 126, 75, 4, 15, 0, -43, -54, 126, 125, 93, 5, 0, 0, 0,
    0, 0, 0, 116, 65, 126, 115, 0, 0, 0, 0, 0, 0, 0, -38, -127, 126,
    96, 0, 18, 106, 126};

void lge_dm_dload_fn(struct work_struct *work)
{

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
    short modem_chip = Secondary_modem_chip;
    int err = 0;
    int index = 0;
#else
    short modem_chip = Primary_modem_chip;
#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/

    int count = 0, length = 0;
    int size = sizeof(empty_mask_buff)/sizeof(char);
    char mask_buf[20];

    while (count < size) {
        mask_buf[length++] = empty_mask_buff[count];
        if (empty_mask_buff[count] == CONTROL_CHAR) {
            if (modem_chip == Primary_modem_chip) {
                diag_process_hdlc_pkt((unsigned char *)mask_buf, length, NULL);
            } else if (modem_chip == Secondary_modem_chip) {
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
                /* send masks to All 9k */
                for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
                    if (diag_hsic[index].hsic_ch && (count > 0)){
                        /* wait sending mask updates
                         * if HSIC ch not ready */
                        if (diag_hsic[index].in_busy_hsic_write)
                            wait_event_interruptible(driver->wait_q,
                                (diag_hsic[index].
                                 in_busy_hsic_write != 1));
                        diag_hsic[index].in_busy_hsic_write = 1;
                        diag_hsic[index].in_busy_hsic_read_on_device =
                                            0;
                        err = diag_bridge_write(index, (unsigned char *)mask_buf, length);

                        if (err) {
                            pr_err("diag: err sending mask to MDM: %d\n",
                                   err);
                            /*
                            * If the error is recoverable, then
                            * clear the write flag, so we will
                            * resubmit a write on the next frame.
                            * Otherwise, don't resubmit a write
                            * on the next frame.
                            */
                            if ((-ESHUTDOWN) != err)
                                diag_hsic[index].in_busy_hsic_write = 0;
                         }
                     }
                }
#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/
            }
            length = 0;
        }
        count++;
    }
}

void lge_dm_usb_fn(struct work_struct *work)
{
    int err = 0;
    struct diag_usb_info *usb_info = NULL;

#ifndef CONFIG_DIAGFWD_BRIDGE_CODE
    usb_info = &diag_usb[DIAG_MUX_LOCAL];
#else
    usb_info = &diag_usb[DIAG_MUX_MDM];
#endif

    err = usb_diag_write(usb_info->hdl, lge_dm_tty->dm_usb_req);
    if (err)
        diagmem_free(driver, lge_dm_tty->dm_usb_req, usb_info->mempool);
}

/*  Modem_request command */
static int lge_dm_tty_modem_request(struct dm_tty *lge_dm_tty_drv, const unsigned char *buf, int count)
{

    int length;
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
    short modem_chip = Secondary_modem_chip;
    int err = 0;
    int index = 0;
#else
    short modem_chip = Primary_modem_chip;
#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/


    if(lge_dm_tty_drv->logging_mode == DM_APP_SDM)
    {
        length = dm_modem_request_header_length + sizeof(modem_chip);

        if (modem_chip == Primary_modem_chip) {
            /* send masks to modem */
            if(buf != NULL)
                diag_process_hdlc_pkt((void *)buf + length, count - length, NULL);
            else
                pr_info("[DM_APP]buf is null , lge_dm_tty_modem_request \n");
        } else if (modem_chip == Secondary_modem_chip) {

            /* send masks to 9k */
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
                /* send masks to All 9k */
                for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
        //            if (diag_hsic[index].hsic_ch && (payload_size > 0) &&
        //                             (remote_proc == MDM)) {
                    if (diag_hsic[index].hsic_ch && (count > 0)){
                        /* wait sending mask updates
                         * if HSIC ch not ready */
                        if (diag_hsic[index].in_busy_hsic_write)
                            wait_event_interruptible(driver->wait_q,
                                (diag_hsic[index].
                                 in_busy_hsic_write != 1));
                        diag_hsic[index].in_busy_hsic_write = 1;
                        diag_hsic[index].in_busy_hsic_read_on_device =
                                            0;
        //                err = diag_bridge_write(index,
        //                        driver->user_space_data +
        //                        token_offset, payload_size);
                        err = diag_bridge_write(index, (void *)buf + length, count - length);

                        if (err) {
                            pr_err("diag: err sending mask to MDM: %d\n",
                                   err);
                            /*
                            * If the error is recoverable, then
                            * clear the write flag, so we will
                            * resubmit a write on the next frame.
                            * Otherwise, don't resubmit a write
                            * on the next frame.
                            */
                            if ((-ESHUTDOWN) != err)
                                diag_hsic[index].in_busy_hsic_write = 0;
                         }
                     }
                }

#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/

        } else {
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_write"
                "modem_number %d "
                "error count = %d length = %d\n",
                __func__, modem_chip, count, length);
        }
    }
    else if(lge_dm_tty_drv->logging_mode == DM_APP_ODM)
    {
        int dm_pkt_type = 0;
        const unsigned char* payload_buf;
        int payload_len;
        if (modem_chip == Primary_modem_chip) {
            memcpy(&dm_pkt_type, buf, sizeof(int));
            payload_buf = buf + sizeof(int);
            payload_len = count - sizeof(int);
            if(dm_pkt_type == USER_SPACE_RAW_DATA_TYPE)
                diag_process_apps_pkt((unsigned char *)payload_buf, payload_len, NULL);
            else
                diag_process_hdlc_pkt((void *)buf, count, NULL);
        } else if (modem_chip == Secondary_modem_chip) {
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
                /* send masks to All 9k */
                for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
        //            if (diag_hsic[index].hsic_ch && (payload_size > 0) &&
        //                             (remote_proc == MDM)) {
                    if (diag_hsic[index].hsic_ch && (count > 0)){
                        /* wait sending mask updates
                         * if HSIC ch not ready */
                        if (diag_hsic[index].in_busy_hsic_write)
                            wait_event_interruptible(driver->wait_q,
                                (diag_hsic[index].
                                 in_busy_hsic_write != 1));
                        diag_hsic[index].in_busy_hsic_write = 1;
                        diag_hsic[index].in_busy_hsic_read_on_device =
                                            0;
        //                err = diag_bridge_write(index,
        //                        driver->user_space_data +
        //                        token_offset, payload_size);
//                        err = diag_bridge_write(index, (void *)buf + length, count - length);
                        err = diag_bridge_write(index, (void *)buf, count);

                        if (err) {
                            pr_err("diag: err sending mask to MDM: %d\n",
                                   err);
                            /*
                            * If the error is recoverable, then
                            * clear the write flag, so we will
                            * resubmit a write on the next frame.
                            * Otherwise, don't resubmit a write
                            * on the next frame.
                            */
                            if ((-ESHUTDOWN) != err)
                                diag_hsic[index].in_busy_hsic_write = 0;
                         }
                     }
                }

#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/
        }
    }

    return count;
}

/* Modem_response command */
static int lge_dm_tty_modem_response(struct dm_tty *lge_dm_tty_drv,
            short modem_number, const unsigned char *buf, int count)
{
    int num_push = 0;
    int left = 0;
    int total_push = 0;

    struct timeval time;
    int start_flag_length;
    int end_flag_length;

    if ((count == 0) || (buf == NULL))
        return 0;

    if(lge_dm_tty_drv->logging_mode == DM_APP_SDM)
    {

        /* make start flag */
        memcpy(dm_modem_response, &dm_rx_start_flag,
            sizeof(dm_rx_start_flag));

        start_flag_length = sizeof(dm_rx_start_flag);

        /* make header */
        dm_modem_response_header->dm_router_size =
            dm_modem_response_header_length +
                dm_modem_response_body_length + count;

        memcpy(dm_modem_response + start_flag_length,
            dm_modem_response_header,
                dm_modem_response_header_length);

        /* make body */
    dm_modem_response_body->modem_chip = modem_number;

        do_gettimeofday(&time);

        memcpy(&(dm_modem_response_body->local_time), &time,
            sizeof(struct timeval));

        memcpy(dm_modem_response + start_flag_length +
            dm_modem_response_header_length,
                dm_modem_response_body,
                    dm_modem_response_body_length);

        if(buf != NULL){
        memcpy(dm_modem_response + start_flag_length +
            dm_modem_response_header_length +
                dm_modem_response_body_length, buf, count);
        }else{
            printk("[DM_APP]buf is null!\n");
        }

        dm_modem_response_length =
            dm_modem_response_header->dm_router_size +
                start_flag_length;

        /* make end flag */
        memcpy(dm_modem_response + dm_modem_response_length,
            &dm_rx_end_flag, sizeof(dm_rx_end_flag));

        end_flag_length = sizeof(dm_rx_end_flag);

        dm_modem_response_length = dm_modem_response_length +
            end_flag_length;

        /* send modem_response packet to DM router */
        total_push = 0;
        left = dm_modem_response_length;

        do {
#ifndef CONFIG_TTY_PORT
            num_push = tty_insert_flip_string(lge_dm_tty_drv->tty_str,
                dm_modem_response + total_push, left);
#else
            num_push = tty_insert_flip_string(&dm_tty_port,
                dm_modem_response + total_push, left);
#endif /* CONFIG_TTY_PORT */

            total_push += num_push;
            left -= num_push;
#ifndef CONFIG_TTY_PORT
            tty_flip_buffer_push(lge_dm_tty_drv->tty_str);
#else
            tty_flip_buffer_push(&dm_tty_port);
#endif /* CONFIG_TTY_PORT */
        } while (left != 0);
    }
    else if(lge_dm_tty_drv->logging_mode == DM_APP_ODM)
    {
        total_push = 0;
        left = count;

        do {
            if (buf == NULL)
                return total_push;

#ifndef    CONFIG_TTY_PORT
            num_push = tty_insert_flip_string(lge_dm_tty_drv->tty_str,
                buf + total_push, left);
#else
            num_push = tty_insert_flip_string(&dm_tty_port,
                buf + total_push, left);
#endif /* CONFIG_TTY_PORT */

            total_push += num_push;
            left -= num_push;
#ifndef CONFIG_TTY_PORT
            tty_flip_buffer_push(lge_dm_tty_drv->tty_str);
#else
            tty_flip_buffer_push(&dm_tty_port);
#endif /* CONFIG_TTY_PORT */


        } while (left != 0);
    }

    return total_push;
}

static int lge_dm_tty_read_thread(void *data)
{
    int j = 0;
    struct dm_tty *lge_dm_tty_drv = NULL;
    struct diag_buf_tbl_t *entry = NULL;
    unsigned long flags;

//    int i = 0;

    lge_dm_tty_drv = lge_dm_tty;

    while (1) {

        wait_event_interruptible(lge_dm_tty->waitq,
            lge_dm_tty->set_logging);

        mutex_lock(&driver->diagchar_mutex);

        if ((lge_dm_tty->set_logging == 1)
                && (driver->logging_mode == DM_APP_MODE)) {

            lge_dm_tty->set_logging = 0;

            for (j = 0; j < lge_dm_tty->num_tbl_entries; j++) {
                entry = &lge_dm_tty->tbl[j];
                if ((entry->len <= 0) || ((void *)entry->buf == NULL))
                    continue;

                lge_dm_tty_modem_response(
                    lge_dm_tty_drv,
                    Primary_modem_chip,
                    (void *)entry->buf,
                    entry->len);

                lge_dm_tty->ops->write_done(entry->buf, entry->len,
                            entry->ctx, lge_dm_tty->ctx);
				
                diag_ws_on_copy(DIAG_WS_MUX);
                spin_lock_irqsave(&lge_dm_tty->lock, flags);
                entry->buf = NULL;
                entry->len = 0;
                entry->ctx = 0;
                spin_unlock_irqrestore(&lge_dm_tty->lock, flags);
            }

//           for (i = 0; i < NUM_PERIPHERALS; i++) {
//                /* Poll SMD data channels to check for data */
//                queue_work(driver->smd_data[i].wq,
//                    &(driver->smd_data[i].diag_read_smd_work));
//            }

//            for (i = 0; i < NUM_PERIPHERALS; i++)
//                flush_workqueue(driver->smd_data[i].wq);
//            wake_up(&driver->smd_wait_q);

            diag_ws_on_copy_complete(DIAG_WS_MUX);

        }

        mutex_unlock(&driver->diagchar_mutex);

        if (kthread_should_stop())
            break;
        mdelay(1);

    }

    return 0;

}


static void lge_dm_tty_unthrottle(struct tty_struct *tty)
{
    return;
}

static int lge_dm_tty_write_room(struct tty_struct *tty)
{

    return DM_TTY_TX_MAX_PACKET_SIZE;
}

static int lge_dm_tty_write(struct tty_struct *tty, const unsigned char *buf,
    int count)
{
    int result;
    struct dm_tty *lge_dm_tty_drv = NULL;

    lge_dm_tty_drv = lge_dm_tty;
    tty->driver_data = lge_dm_tty_drv;
    lge_dm_tty_drv->tty_str = tty;

    /* check the packet size */
    if (count > DM_TTY_RX_MAX_PACKET_SIZE) {
        pr_info(DM_TTY_MODULE_NAME ": %s:"
        "lge_dm_tty_write error count = %d\n",
            __func__, count);
        return -EPERM;
    }
    result = lge_dm_tty_modem_request(lge_dm_tty_drv, buf, count);
    return result;

}

static int lge_dm_tty_open(struct tty_struct *tty, struct file *file)
{
    struct dm_tty *lge_dm_tty_drv = NULL;

    if (!tty) {
        pr_err(DM_TTY_MODULE_NAME ": %s: NULL tty", __func__);
        return -ENODEV;
    }

    lge_dm_tty_drv = lge_dm_tty;

    if (!lge_dm_tty_drv) {
        pr_err(DM_TTY_MODULE_NAME ": %s:"
        "NULL lge_dm_tty_drv", __func__);
        return -ENODEV;
    }
#ifdef CONFIG_TTY_PORT
    tty_port_tty_set(&dm_tty_port, tty);
    dm_tty_port.low_latency = 1;
#endif /* CONFIG_TTY_PORT */
    tty->driver_data = lge_dm_tty_drv;
    lge_dm_tty_drv->tty_str = tty;

    if (lge_dm_tty_drv->tty_state == DM_TTY_OPEN) {
        pr_err(DM_TTY_MODULE_NAME ": %s:"
        "tty is already open", __func__);
        return -EBUSY;
    }

    /* support max = 64KB */
    set_bit(TTY_NO_WRITE_SPLIT, &lge_dm_tty_drv->tty_str->flags);

    lge_dm_tty_drv->tty_ts = kthread_run(lge_dm_tty_read_thread, NULL,
        "lge_dm_tty_thread");

    lge_dm_tty_drv->tty_state = DM_TTY_OPEN;

    pr_info(DM_TTY_MODULE_NAME ": %s: TTY device open\n", __func__);

    lge_dm_tty_drv->logging_mode = DM_APP_ODM;
    lge_dm_tty_drv->set_logging= 0;

    dm_modem_response_length = 0;
    dm_modem_request_length = 0;

    dm_rx_start_flag = 0x2B1A;
    dm_rx_end_flag = 0x7E6D;

    dm_modem_response_header->dm_router_cmd = DM_APP_MODEM_RESPONSE;
    dm_modem_response_header->dm_router_type = DM_APP_NOTICE;

    lge_dm_tty_drv->dm_wq = create_singlethread_workqueue("dm_wq");
    INIT_WORK(&(lge_dm_tty_drv->dm_usb_work), lge_dm_usb_fn);
    INIT_WORK(&(lge_dm_tty_drv->dm_dload_work), lge_dm_dload_fn);

    return 0;

}

static void lge_dm_tty_close(struct tty_struct *tty, struct file *file)
{
    struct dm_tty *lge_dm_tty_drv = NULL;

    lge_dm_tty_drv = lge_dm_tty;
    tty->driver_data = lge_dm_tty_drv;

    if (!tty) {
        pr_err(DM_TTY_MODULE_NAME ": %s: NULL tty", __func__);
        return;
    }

    lge_dm_tty_drv->tty_str = tty;

    clear_bit(TTY_NO_WRITE_SPLIT, &lge_dm_tty_drv->tty_str->flags);

    lge_dm_tty_drv = tty->driver_data;

    if (!lge_dm_tty_drv) {
        pr_err(DM_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv", __func__);
        return;
    }

    if (lge_dm_tty_drv->tty_state != DM_TTY_OPEN) {
        pr_err(DM_TTY_MODULE_NAME ": %s: TTY device was not opened\n",
            __func__);
        return;
    }

    lge_dm_tty_drv->logging_mode = DM_APP_ODM;
    lge_dm_tty_drv->set_logging = 1;
    wake_up_interruptible(&lge_dm_tty_drv->waitq);

    kthread_stop(lge_dm_tty_drv->tty_ts);

    lge_dm_tty_drv->tty_state = DM_TTY_CLOSED;

    pr_info(DM_TTY_MODULE_NAME ": %s: TTY device closed\n", __func__);

    cancel_work_sync(&(lge_dm_tty_drv->dm_usb_work));
    cancel_work_sync(&(lge_dm_tty_drv->dm_dload_work));
    destroy_workqueue(lge_dm_tty_drv->dm_wq);
    lge_dm_tty_drv->dm_wq = NULL;
#ifdef CONFIG_TTY_PORT
    tty_port_tty_set(&dm_tty_port, NULL);
#endif /* CONFIG_TTY_PORT */
    return;

}

static int lge_dm_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
    unsigned long arg)
{
    short result;
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
    short modem_number = Secondary_modem_chip;
#else
    short modem_number = Primary_modem_chip;
#endif /*CONFIG_DIAGFWD_BRIDGE_CODE*/


    struct dm_tty *lge_dm_tty_drv = NULL;
    int status = 0;
//    int i = 0;
    unsigned long flags;

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
    int index=0;
    unsigned long spin_lock_flags;
#endif

    int j = 0;
    struct diag_buf_tbl_t *entry = NULL;

    result = 0;
    lge_dm_tty_drv = lge_dm_tty;
    tty->driver_data = lge_dm_tty_drv;
    lge_dm_tty_drv->tty_str = tty;

    if (_IOC_TYPE(cmd) != DM_TTY_IOCTL_MAGIC)
        return -EINVAL;

    switch (cmd) {
    case DM_TTY_MODEM_OPEN_SDM:
        if(lge_dm_tty_drv->logging_mode == DM_APP_SDM)
        {
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
            "already DM_TTY_MODEM_OPEN_SDM\n", __func__);

            result = TRUE;

            if (copy_to_user((void *)arg, (const void *)&result,
                sizeof(result)) == 0)
                pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                    "already DM_TTY_MODEM_OPEN_SDM"
                    "result = %d\n", __func__, result);

            break;
        }

        lge_dm_tty_drv->logging_mode = DM_APP_SDM;

        pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
        "DM_TTY_MODEM_OPEN_SDM\n", __func__);

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
        for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
            if ((diag_bridge[index].usb_connected == 1) && (diag_hsic[index].count_hsic_pool == N_MDM_WRITE)) {
                spin_lock_irqsave(&diag_hsic[index].hsic_spinlock,spin_lock_flags);
                diag_hsic[index].count_hsic_pool = 0;
                spin_unlock_irqrestore(&diag_hsic[index].hsic_spinlock,spin_lock_flags);
            }

            diag_hsic[index].num_hsic_buf_tbl_entries = 0;
            for (i = 0; i < diag_hsic[index].poolsize_hsic_write; i++) {
                if (diag_hsic[index].hsic_buf_tbl[index].buf) {
                    /* Return the buffer to the pool */
                    diagmem_free(driver, (unsigned char *)
                        (diag_hsic[index].hsic_buf_tbl[index].buf),
                        index+POOL_TYPE_HSIC);
                    diag_hsic[index].hsic_buf_tbl[index].buf = 0;
                }
                diag_hsic[index].hsic_buf_tbl[index].length = 0;
            }
        }

        diagfwd_cancel_hsic(REOPEN_HSIC); // QCT 161032 migration - NEED TO CHECK
        diagfwd_connect_bridge(0);
#endif /* CONFIG_DIAGFWD_BRIDGE_CODE */

        /* change path to DM APP */
        mutex_lock(&driver->diagchar_mutex);
        lge_dm_tty->ops->open(lge_dm_tty->ctx, DIAG_USB_MODE);
        driver->logging_mode = DM_APP_MODE;
        mutex_unlock(&driver->diagchar_mutex);

        if (modem_number == Primary_modem_chip) {
            for (j = 0; j < lge_dm_tty->num_tbl_entries; j++) {
                entry = &lge_dm_tty->tbl[j];
                if (entry->len <= 0)
                    continue;

                spin_lock_irqsave(&lge_dm_tty->lock, flags);
                entry->buf = NULL;
                entry->len = 0;
                entry->ctx = 0;
                spin_unlock_irqrestore(&lge_dm_tty->lock, flags);
            }

            diag_ws_reset(DIAG_WS_MUX);

            if (driver->rsp_buf_busy) {
                spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
                driver->rsp_buf_busy = 0;
                spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
            }

//            for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
//                spin_lock_irqsave(&driver->smd_data[i].in_busy_lock, flags);
//                driver->smd_data[i].in_busy_1 = 0;
//                driver->smd_data[i].in_busy_2 = 0;
//                spin_unlock_irqrestore(&driver->smd_data[i].in_busy_lock, flags);
//
//                /* Poll SMD data channels to check for data */
//                queue_work(driver->smd_data[i].wq,
//                    &(driver->smd_data[i].diag_read_smd_work));
//            }

//            for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++) {
//                spin_lock_irqsave(&driver->smd_cmd[i].in_busy_lock, flags);
//                driver->smd_cmd[i].in_busy_1 = 0;
//                driver->smd_cmd[i].in_busy_2 = 0;
//                spin_unlock_irqrestore(&driver->smd_cmd[i].in_busy_lock, flags);
//
//                /* Poll SMD data channels to check for data */
//                queue_work(driver->diag_wq,
//                    &(driver->smd_cmd[i].diag_read_smd_work));
//            }
        } else if (modem_number == Secondary_modem_chip) {
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
                    for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
                        /* Read data from the hsic */
                        if (diag_hsic[index].hsic_ch)
                            queue_work(diag_bridge[index].wq,
                                   &(diag_hsic[index].
                                     diag_read_hsic_work));
                    }
#endif /* CONFIG_DIAGFWD_BRIDGE_CODE */
        } else {
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_MODEM_OPEN_SDM"
                "error modem_number = %d\n",
                    __func__, modem_number);
        }

        result = TRUE;

        if (copy_to_user((void *)arg, (const void *)&result,
            sizeof(result)) == 0)
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_MODEM_OPEN_SDM"
                "result = %d\n", __func__, result);

        break;

    case DM_TTY_MODEM_CLOSE_SDM:
        lge_dm_tty_drv->logging_mode = DM_APP_ODM;
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_MODEM_CLOSE_SDM, modem_number = %d\n",
                    __func__, modem_number);

        lge_dm_tty_drv->set_logging = 0;

        /* change path to USB driver */
        mutex_lock(&driver->diagchar_mutex);
        driver->logging_mode = USB_MODE;
        if (lge_dm_tty && lge_dm_tty->ops && lge_dm_tty->ops->close)
            lge_dm_tty->ops->close(lge_dm_tty->ctx, DIAG_USB_MODE);
        mutex_unlock(&driver->diagchar_mutex);

        result = TRUE;

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
        for (index = 0; index < MAX_HSIC_DATA_CH; index++) {

            diag_hsic[index].num_hsic_buf_tbl_entries = 0;
            for (i = 0; i < diag_hsic[index].poolsize_hsic_write; i++) {
                if (diag_hsic[index].hsic_buf_tbl[index].buf) {
                    /* Return the buffer to the pool */
                    diagmem_free(driver, (unsigned char *)
                        (diag_hsic[index].hsic_buf_tbl[index].buf),
                        index+POOL_TYPE_HSIC);
                    diag_hsic[index].hsic_buf_tbl[index].buf = 0;
                }
                diag_hsic[index].hsic_buf_tbl[index].length = 0;
            }
        }
        diagfwd_cancel_hsic(REOPEN_HSIC); // QCT 161032 migration - NEED TO CHECK
        diagfwd_connect_bridge(0);
#endif

        if (copy_to_user((void *)arg, (const void *)&result,
            sizeof(result)) == 0)
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_MODEM_CLOSE_SDM"
                "result = %d\n", __func__, result);

        break;
    case DM_TTY_MODEM_OPEN_ODM:
        lge_dm_tty_drv->logging_mode = DM_APP_ODM;

        pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                        "DM_TTY_MODEM_OPEN_ODM, modem_number = %d\n", __func__, modem_number);

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
        for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
            if ((diag_bridge[index].usb_connected == 1) && (diag_hsic[index].count_hsic_pool == N_MDM_WRITE)) {
                spin_lock_irqsave(&diag_hsic[index].hsic_spinlock,spin_lock_flags);
                diag_hsic[index].count_hsic_pool = 0;
                spin_unlock_irqrestore(&diag_hsic[index].hsic_spinlock,spin_lock_flags);
            }

            diag_hsic[index].num_hsic_buf_tbl_entries = 0;
            for (i = 0; i < diag_hsic[index].poolsize_hsic_write; i++) {
                if (diag_hsic[index].hsic_buf_tbl[index].buf) {
                    /* Return the buffer to the pool */
                    diagmem_free(driver, (unsigned char *)
                        (diag_hsic[index].hsic_buf_tbl[index].buf),
                        index+POOL_TYPE_HSIC);
                    diag_hsic[index].hsic_buf_tbl[index].buf = 0;
                }
                diag_hsic[index].hsic_buf_tbl[index].length = 0;
            }
        }

        diagfwd_cancel_hsic(REOPEN_HSIC); // QCT 161032 migration - NEED TO CHECK
        diagfwd_connect_bridge(0);
#endif /* CONFIG_DIAGFWD_BRIDGE_CODE */
        /* change path to DM DEV */
        mutex_lock(&driver->diagchar_mutex);
        lge_dm_tty->ops->open(lge_dm_tty->ctx, DIAG_USB_MODE);
        driver->logging_mode = DM_APP_MODE;
        mutex_unlock(&driver->diagchar_mutex);

        if (modem_number == Primary_modem_chip) {
            for (j = 0; j < lge_dm_tty->num_tbl_entries; j++) {
                entry = &lge_dm_tty->tbl[j];
                if (entry->len <= 0)
                    continue;

                spin_lock_irqsave(&lge_dm_tty->lock, flags);
                entry->buf = NULL;
                entry->len = 0;
                entry->ctx = 0;
                spin_unlock_irqrestore(&lge_dm_tty->lock, flags);
            }

            diag_ws_reset(DIAG_WS_MUX);

            if (driver->rsp_buf_busy) {
                spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
                driver->rsp_buf_busy = 0;
                spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
            }

//            for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
//                spin_lock_irqsave(&driver->smd_data[i].in_busy_lock, flags);
//                driver->smd_data[i].in_busy_1 = 0;
//                driver->smd_data[i].in_busy_2 = 0;
//                spin_unlock_irqrestore(&driver->smd_data[i].in_busy_lock, flags);
//
//                /* Poll SMD data channels to check for data */
//                queue_work(driver->smd_data[i].wq,
//                    &(driver->smd_data[i].diag_read_smd_work));
//            }

//            for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++) {
//                spin_lock_irqsave(&driver->smd_cmd[i].in_busy_lock, flags);
//                driver->smd_cmd[i].in_busy_1 = 0;
//                driver->smd_cmd[i].in_busy_2 = 0;
//                spin_unlock_irqrestore(&driver->smd_cmd[i].in_busy_lock, flags);
//
//                /* Poll SMD data channels to check for data */
//                queue_work(driver->diag_wq,
//                    &(driver->smd_cmd[i].diag_read_smd_work));
//            }
        } else if (modem_number == Secondary_modem_chip) {

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
                    for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
                        /* Read data from the hsic */
                        if (diag_hsic[index].hsic_ch)
                            queue_work(diag_bridge[index].wq,
                                   &(diag_hsic[index].
                                     diag_read_hsic_work));
                    }
#endif /* CONFIG_DIAGFWD_BRIDGE_CODE */


        } else {
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_IOCTL_MODEM_OPEN"
                "error modem_number = %d\n",
                    __func__, modem_number);
        }

        break;

    case DM_TTY_MODEM_CLOSE_ODM:
        lge_dm_tty_drv->logging_mode = DM_APP_ODM;

        pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
            "DM_TTY_MODEM_CLOSE_ODM, modem_number = %d\n",
                __func__, modem_number);
        result = TRUE;

        lge_dm_tty_drv->set_logging = 0;

        /* change path to USB driver */
        mutex_lock(&driver->diagchar_mutex);
        driver->logging_mode = USB_MODE;
        if (lge_dm_tty && lge_dm_tty->ops && lge_dm_tty->ops->close)
            lge_dm_tty->ops->close(lge_dm_tty->ctx, DIAG_USB_MODE);
        mutex_unlock(&driver->diagchar_mutex);

        if (modem_number == Primary_modem_chip) {

        } else if (modem_number == Secondary_modem_chip) {
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
        for (index = 0; index < MAX_HSIC_DATA_CH; index++) {
            diag_hsic[index].num_hsic_buf_tbl_entries = 0;
            for (i = 0; i < diag_hsic[index].poolsize_hsic_write; i++) {
                if (diag_hsic[index].hsic_buf_tbl[index].buf) {
                    /* Return the buffer to the pool */
                    diagmem_free(driver, (unsigned char *)
                        (diag_hsic[index].hsic_buf_tbl[index].buf),
                        index+POOL_TYPE_HSIC);
                    diag_hsic[index].hsic_buf_tbl[index].buf = 0;
                }
                diag_hsic[index].hsic_buf_tbl[index].length = 0;
            }
        }
        diagfwd_cancel_hsic(REOPEN_HSIC); // QCT 161032 migration - NEED TO CHECK
        diagfwd_connect_bridge(0);
#endif
        }

        if (copy_to_user((void *)arg, (const void *)&result,
            sizeof(result)) == 0)
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_MODEM_CLOSE_ODM"
                "result = %d\n", __func__, result);

      break;

    case DM_TTY_MODEM_RESET:
        // not supported
        //status = subsys_modem_restart();
        result = TRUE;

        if (copy_to_user((void *)arg, (const void *)&status,
            sizeof(result)) == 0)
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
                "DM_TTY_MODEM_RESET"
                "result = %d\n", __func__, result);
        break;

    case DM_TTY_MODEM_CRASH:
#ifdef CONFIG_DIAG_BRIDGE_CODE
            subsystem_restart("external_modem");
            result = TRUE;
#else
            subsystem_restart("modem");
            result = TRUE;
#endif

        if (copy_to_user((void *)arg, (const void *)&result,
        sizeof(result)) == 0)
        pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
            "DM_TTY_MODEM_CRASH"
            "result = %d\n", __func__, result);
    break;
  case DM_TTY_MODEM_DEBUGGER:
/*
    memset(rw_buf, 0, sizeof(rw_buf));
    strcpy(rw_buf,ssr_noti);
    if (copy_to_user((void *)arg, &rw_buf, sizeof(rw_buf))){
            pr_info(DM_TTY_MODULE_NAME ": %s: lge_dm_tty_ioctl "
            "DM_TTY_MODEM_DEBUGGER error! "
            "rw_buf = %s\n", __func__, rw_buf);
            return -EFAULT;
      }

      printk("rw_buf = %s\n", rw_buf);
*/
  break;

    default:
        pr_info(DM_TTY_MODULE_NAME ": %s:"
        "lge_dm_tty_ioctl error\n", __func__);
        break;

    }

    return 0;

}

static const struct tty_operations lge_dm_tty_ops = {
    .open = lge_dm_tty_open,
    .close = lge_dm_tty_close,
    .write = lge_dm_tty_write,
    .write_room = lge_dm_tty_write_room,
    .unthrottle = lge_dm_tty_unthrottle,
    .ioctl = lge_dm_tty_ioctl,
};

static int __init lge_dm_tty_init(void)
{
    int ret = 0;
    struct device *tty_dev;
    struct dm_tty *lge_dm_tty_drv;
#ifdef CONFIG_TTY_PORT
    tty_port_init(&dm_tty_port);
#endif /* CONFIG_TTY_PORT */
    pr_info(DM_TTY_MODULE_NAME ": %s\n", __func__);

    if(lge_dm_tty != NULL)
    {
        lge_dm_tty_drv = lge_dm_tty;
    }
    else
    {
        lge_dm_tty_drv = kzalloc(sizeof(struct dm_tty), GFP_KERNEL);
        if (lge_dm_tty_drv == NULL) {
            pr_info(DM_TTY_MODULE_NAME "%s:"
            "failed to allocate lge_dm_tty", __func__);
            return 0;
        }

        lge_dm_tty = lge_dm_tty_drv;
    }

    lge_dm_tty_drv->tty_drv = alloc_tty_driver(MAX_DM_TTY_DRV);

    if (!lge_dm_tty_drv->tty_drv) {
        pr_info(DM_TTY_MODULE_NAME ": %s - tty_drv is NULL", __func__);
        kfree(lge_dm_tty_drv);
        return 0;
    }

    lge_dm_tty_drv->tty_drv->name = "lge_dm_tty";
    lge_dm_tty_drv->tty_drv->owner = THIS_MODULE;
    lge_dm_tty_drv->tty_drv->driver_name = "lge_dm_tty";
    /* uses dynamically assigned dev_t values */
    lge_dm_tty_drv->tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
    lge_dm_tty_drv->tty_drv->subtype = SERIAL_TYPE_NORMAL;
    lge_dm_tty_drv->tty_drv->flags = TTY_DRIVER_REAL_RAW
        | TTY_DRIVER_DYNAMIC_DEV
        | TTY_DRIVER_RESET_TERMIOS;

    /* initializing the tty driver */
    lge_dm_tty_drv->tty_drv->init_termios = tty_std_termios;
    lge_dm_tty_drv->tty_drv->init_termios.c_iflag = IGNBRK | IGNPAR;
    lge_dm_tty_drv->tty_drv->init_termios.c_oflag = 0;
    lge_dm_tty_drv->tty_drv->init_termios.c_cflag =
        B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    lge_dm_tty_drv->tty_drv->init_termios.c_lflag = 0;

    tty_set_operations(lge_dm_tty_drv->tty_drv, &lge_dm_tty_ops);

#ifdef CONFIG_TTY_PORT
    tty_port_link_device(&dm_tty_port, lge_dm_tty_drv->tty_drv, 0);
#endif /* CONFIG_TTY_PORT */
    ret = tty_register_driver(lge_dm_tty_drv->tty_drv);

    if (ret) {
        put_tty_driver(lge_dm_tty_drv->tty_drv);
#ifdef CONFIG_TTY_PORT
        tty_port_destroy(&dm_tty_port);
#endif /* CONFIG_TTY_PORT */
        pr_info(DM_TTY_MODULE_NAME ": %s:"
        "tty_register_driver() ""failed\n",
            __func__);

        lge_dm_tty_drv->tty_drv = NULL;
        kfree(lge_dm_tty_drv);
        return 0;
    }

    tty_dev = tty_register_device(lge_dm_tty_drv->tty_drv, 0, NULL);

    if (IS_ERR(tty_dev)) {
        pr_info(DM_TTY_MODULE_NAME ": %s:"
        "tty_register_device() " "failed\n",
            __func__);
        tty_unregister_driver(lge_dm_tty_drv->tty_drv);
        put_tty_driver(lge_dm_tty_drv->tty_drv);
#ifdef CONFIG_TTY_PORT
        tty_port_destroy(&dm_tty_port);
#endif /* CONFIG_TTY_PORT */
        kfree(lge_dm_tty_drv);
        return 0;
    }

    init_waitqueue_head(&lge_dm_tty_drv->waitq);

    //fix to spinlock crash
    spin_lock_init(&lge_dm_tty_drv->lock);

    lge_dm_tty_drv->tty_state = DM_TTY_REGISTERED;

    /* data initialization */
    dm_modem_response = kzalloc(DM_TTY_TX_MAX_PACKET_SIZE, GFP_KERNEL);
    if (dm_modem_response == NULL)
        pr_info(DM_TTY_MODULE_NAME ": %s: dm_modem_response ""failed\n",
            __func__);

    dm_modem_request = kzalloc(DM_TTY_RX_MAX_PACKET_SIZE, GFP_KERNEL);
    if (dm_modem_request == NULL)
        pr_info(DM_TTY_MODULE_NAME ": %s: dm_modem_request ""failed\n",
            __func__);

    dm_modem_response_header_length = sizeof(struct dm_router_header);
    dm_modem_response_header = kzalloc(dm_modem_response_header_length,
        GFP_KERNEL);
    if (dm_modem_response_header == NULL)
        pr_info(DM_TTY_MODULE_NAME ": %s: dm_modem_response_header "
            "failed\n", __func__);

    dm_modem_request_header_length = sizeof(struct dm_router_header);
    dm_modem_request_header = kzalloc(dm_modem_request_header_length,
        GFP_KERNEL);
    if (dm_modem_request_header == NULL)
        pr_info(DM_TTY_MODULE_NAME ": %s: dm_modem_request_header "
            "failed\n", __func__);

    dm_modem_response_body_length =
        sizeof(struct dm_router_modem_response_body);
    dm_modem_response_body = kzalloc(dm_modem_response_body_length,
        GFP_KERNEL);
    if (dm_modem_response_body == NULL)
        pr_info(DM_TTY_MODULE_NAME ": %s: dm_modem_response_body "
            "failed\n", __func__);

    return 0;

}

static void __exit lge_dm_tty_exit(void)
{
    int ret = 0;
    struct dm_tty *lge_dm_tty_drv = NULL;

    lge_dm_tty_drv = lge_dm_tty;

    if (!lge_dm_tty_drv) {
        pr_err(DM_TTY_MODULE_NAME ": %s:"
        "NULL lge_dm_tty_drv", __func__);
        return;
    }

    if (lge_dm_tty_drv->tty_state != DM_TTY_INITIAL) {
        tty_unregister_device(lge_dm_tty_drv->tty_drv, 0);

        ret = tty_unregister_driver(lge_dm_tty_drv->tty_drv);

        if (ret) {
            pr_err(DM_TTY_MODULE_NAME ": %s: "
                "tty_unregister_driver() failed\n", __func__);
        }

        put_tty_driver(lge_dm_tty_drv->tty_drv);
        lge_dm_tty_drv->tty_state = DM_TTY_INITIAL;
        lge_dm_tty_drv->tty_drv = NULL;
    }

    kfree(dm_modem_response);
    kfree(dm_modem_request);
    kfree(dm_modem_response_header);
    kfree(dm_modem_response_body);

    pr_info(DM_TTY_MODULE_NAME ": %s: Freeing dm_tty structure", __func__);
    kfree(lge_dm_tty_drv);

    return;

}

module_init(lge_dm_tty_init);
module_exit(lge_dm_tty_exit);

MODULE_DESCRIPTION("LGE DM TTY");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Youngjin Park <jin.park@lge.com>");

#endif
