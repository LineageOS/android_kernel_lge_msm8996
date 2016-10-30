#include <linux/err.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/termios.h>
#include <linux/bitops.h>
#include <linux/param.h>

//[S] Bluetooth Bring-up
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include "bluetooth_pm.h"
//[E] Bluetooth Bring-up

#undef BT_INFO
#define BT_INFO(fmt, arg...) printk(KERN_ERR "*[bluetooth-pm(%d)-%s()] " fmt "\n" , __LINE__, __FUNCTION__, ## arg)
#undef BT_ERR
#define BT_ERR(fmt, arg...)  printk(KERN_ERR "*[bluetooth-pm(%d)-%s()] " fmt "\n" , __LINE__, __FUNCTION__, ## arg)
#undef BT_DBG
#define BT_DBG(fmt, arg...)  printk(KERN_ERR "*[bluetooth-pm(%d)-%s()] " fmt "\n" , __LINE__, __FUNCTION__, ## arg)


struct bluetooth_pm_data {
    int bt_reset;
    int host_wake;
    int ext_wake;
    unsigned host_wake_irq;
    spinlock_t slock;

    struct rfkill *rfkill;
    struct wake_lock wake_lock;

#ifdef UART_CONTROL_MSM
    struct uart_port *uport;
#endif/*UART_CONTROL_MSM*/
};

//[S] Bluetooth Bring-up
struct bluetooth_pm_device_info {
    int gpio_bt_reset;
    int gpio_bt_host_wake;
    int gpio_bt_ext_wake;
    struct rfkill *rfk;
};
//[E] Bluetooth Bring-up

#define PROC_DIR    "bluetooth/sleep"


#define ASSERT      0
#define DEASSERT    1


/* work function */
static void bluetooth_pm_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluetooth_pm_sleep_work);

/* Macros for handling sleep work */
#define bluetooth_pm_rx_busy()         schedule_delayed_work(&sleep_workqueue, 0)
#define bluetooth_pm_tx_busy()         schedule_delayed_work(&sleep_workqueue, 0)
#define bluetooth_pm_rx_idle()         schedule_delayed_work(&sleep_workqueue, 0)
#define bluetooth_pm_tx_idle()         schedule_delayed_work(&sleep_workqueue, 0)


//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
#define TX_TIMER_INTERVAL   300//(Uint : ms)

#define RX_TIMER_INTERVAL   5//(Uint : sec)
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER


/* state variable names and bit positions */
#define BT_PROTO    0x01
#define BT_TXDATA   0x02
#define BT_ASLEEP   0x04
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
#define BT_PREPROTO 0x05
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);


static struct bluetooth_pm_data *bsi;


/** Global state flags */
static unsigned long flags;


/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
/** Transmission timer */
static struct timer_list tx_timer;
static struct timer_list rx_timer;
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER



//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
static struct timer_list uart_control_timer;
static unsigned long uart_off_jiffies;
static unsigned long uart_on_jiffies;

#define UART_CONTROL_BLOCK_TIME    50//(Uint : ms)
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT


/** Lock for state transitions */
static spinlock_t rw_lock;


struct proc_dir_entry *bluetooth_dir, *sleep_dir;


#ifdef UART_CONTROL_MSM
/*
 * Local functions
 */
static void hsuart_power(int on)
{
    if (on) {
        printk("%s, (1)\n", __func__);
        msm_hs_request_clock_on(bsi->uport);
        //printk("%s, (msm_hs_request_clock_on)\n", __func__);
        msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
        //printk("%s, (msm_hs_set_mctrl)\n", __func__);
    } else {
        printk("%s, (0)\n", __func__);
        msm_hs_set_mctrl(bsi->uport, 0);
        msm_hs_request_clock_off(bsi->uport);
    }
}
#endif/*UART_CONTROL_MSM*/


/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
static inline int bluetooth_pm_can_sleep(void)
{
    //printk("%s\n", __func__);

    /* check if MSM_WAKE_BT_GPIO and BT_WAKE_MSM_GPIO are both deasserted */
#ifdef UART_CONTROL_MSM
    return gpio_get_value(bsi->ext_wake) && gpio_get_value(bsi->host_wake) && (bsi->uport != NULL);
#else/*UART_CONTROL_MSM*/
    return gpio_get_value(bsi->ext_wake) && gpio_get_value(bsi->host_wake);
#endif/*UART_CONTROL_MSM*/
}


//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
//return TRUE : uart_control available
//return FALSE : uart control unavailable
bool check_uart_control_available(unsigned long diff_jeffies_0, unsigned long diff_jeffies_1)
{
    unsigned long diff_milli_second;
    bool ret = true;

    if(diff_jeffies_0 == 0 || diff_jeffies_1 == 0)
    {
        printk("%s, RETURN TRUE\n", __func__);
        return ret;
    }

    diff_milli_second = jiffies_to_msecs(diff_jeffies_1) - jiffies_to_msecs(diff_jeffies_0);

    //printk("=======================================\n");
    printk("diff_milli_second = %lu\n", diff_milli_second);
    //printk("=======================================\n");

    if(diff_milli_second <= UART_CONTROL_BLOCK_TIME)
    {
        //printk("%s, RETURN FALSE\n", __func__);
        ret = false;
    }
    //else
    //{
    //  printk("%s, RETURN TRUE\n", __func__);
    //}

    return ret;
}
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT



void bluetooth_pm_wakeup(void)
{
    unsigned long irq_flags;

    //printk("+++ %s\n", __func__);

    spin_lock_irqsave(&rw_lock, irq_flags);

    if (test_bit(BT_ASLEEP, &flags)) {
        printk("%s, waking up...\n", __func__);

//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
        uart_on_jiffies = jiffies;

        if(check_uart_control_available(uart_off_jiffies, uart_on_jiffies) == false)
        {
            mod_timer(&uart_control_timer, jiffies + msecs_to_jiffies(UART_CONTROL_BLOCK_TIME));
            spin_unlock_irqrestore(&rw_lock, irq_flags);
            printk("--- %s - UART control unavailable Return\n", __func__);
            return;
        }
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT

        clear_bit(BT_ASLEEP, &flags);

        spin_unlock_irqrestore(&rw_lock, irq_flags);

        //printk("%s, WAKE_LOCK_START\n", __func__);

        wake_lock(&bsi->wake_lock);

        //printk("%s, WAKE_LOCK_END\n", __func__);

#ifdef UART_CONTROL_MSM
        /*Activating UART */
        hsuart_power(1);
#endif/*UART_CONTROL_MSM*/
    }
    else
    {
        int wake, host_wake;
        wake = gpio_get_value(bsi->ext_wake);
        host_wake = gpio_get_value(bsi->host_wake);

        spin_unlock_irqrestore(&rw_lock, irq_flags);

        //printk("%s, not waking up...\n", __func__);

        if(wake == DEASSERT && host_wake == ASSERT)
        {
            printk("%s - Start Timer : check hostwake status when timer expired\n", __func__);
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
            mod_timer(&rx_timer, jiffies + (RX_TIMER_INTERVAL * HZ));
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
        }

#ifdef UART_CONTROL_MSM
        //printk("%s, Workaround for uart runtime suspend : Check UART Satus", __func__);

        //if(bsi->uport != NULL && msm_hs_get_bt_uport_clock_state(bsi->uport) == CLOCK_REQUEST_AVAILABLE)
        //{
        //  printk("%s, Enter abnormal status, HAVE to Call hsuart_power(1)!", __func__);
        //  hsuart_power(1);
        //}
#endif /*UART_CONTROL_MSM*/
    }

    //printk("--- %s\n", __func__);
}


void bluetooth_pm_sleep(void)
{
    unsigned long irq_flags;

    //printk("+++ %s\n", __func__);

    spin_lock_irqsave(&rw_lock, irq_flags);

    /* already asleep, this is an error case */
    if (test_bit(BT_ASLEEP, &flags)) {
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        printk("--- %s, already asleep return\n", __func__);
        return;
    }

//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
    uart_off_jiffies = jiffies;

    if(check_uart_control_available(uart_on_jiffies, uart_off_jiffies) == false)
    {
        mod_timer(&uart_control_timer, jiffies + msecs_to_jiffies(UART_CONTROL_BLOCK_TIME));
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        printk("--- %s - UART control unavailable Return\n", __func__);
        return;
    }
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT

    set_bit(BT_ASLEEP, &flags);

    spin_unlock_irqrestore(&rw_lock, irq_flags);

    printk("%s, going to sleep...\n", __func__);

    //printk("%s, WAKE_UNLOCK_START\n", __func__);

    wake_unlock(&bsi->wake_lock);

    //printk("%s, WAKE_UNLOCK_END\n", __func__);

#ifdef UART_CONTROL_MSM
    /*Deactivating UART */
    hsuart_power(0);
#endif /*UART_CONTROL_MSM*/

    //printk("--- %s\n", __func__);
}


static void bluetooth_pm_sleep_work(struct work_struct *work)
{
    //printk("+++ %s\n", __func__);

    if (bluetooth_pm_can_sleep()) {
        printk("%s, bluetooth_pm_can_sleep is true. Call BT Sleep\n", __func__);
        bluetooth_pm_sleep();
    } else {
        printk("%s, bluetooth_pm_can_sleep is false. Call BT Wake Up\n", __func__);
        bluetooth_pm_wakeup();
    }

    //printk("--- %s\n", __func__);
}


static void bluetooth_pm_hostwake_task(unsigned long data)
{
    printk("%s\n", __func__);

    spin_lock(&rw_lock);

    //printk("%s spin_lock Call rx_busy\n", __func__);

    //if(gpio_get_value(bsi->host_wake) == ASSERT)
    //{
        //printk("%s, hostwake GPIO ASSERT(Low)\n", __func__);

        bluetooth_pm_rx_busy();

//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
        mod_timer(&rx_timer, jiffies + (RX_TIMER_INTERVAL * HZ));
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
    //}
    //else
    //{
    //  printk("%s, hostwake GPIO DEASSERT(High)\n", __func__);
    //}

    spin_unlock(&rw_lock);

    //printk("%s spin_unlock\n", __func__);

    //printk("--- %s\n", __func__);
}

//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
static void bluetooth_pm_tx_timer_expire(unsigned long data)
{
    unsigned long irq_flags;

    //printk("+++ %s, Tx timer expired\n", __func__);

    //printk("%s, Spin Lock\n", __func__);

    spin_lock_irqsave(&rw_lock, irq_flags);


    if (test_bit(BT_ASLEEP, &flags)) {
        printk("%s, already asleep\n", __func__);
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        return;
    }

    if (!test_bit(BT_TXDATA, &flags)) {
        printk("%s, already deassert bt_wake\n", __func__);
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        return;
    }

    BT_DBG("BT WAKE Set to Sleep");
    clear_bit(BT_TXDATA,&flags);

    gpio_set_value(bsi->ext_wake, 1);

    //printk("%s, Call bluetooth_pm_tx_idle\n", __func__);

    bluetooth_pm_tx_idle();

    spin_unlock_irqrestore(&rw_lock, irq_flags);

    //printk("%s, Spin Unlock\n", __func__);

    //printk("--- %s, Tx timer expired\n", __func__);
}
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER


//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
static void bluetooth_pm_rx_timer_expire(unsigned long data)
{
    unsigned long irq_flags;

    printk("%s, Rx timer expired\n", __func__);

    //printk("%s, Spin Lock\n", __func__);

    spin_lock_irqsave(&rw_lock, irq_flags);

    /* already asleep, this is an error case */
    if (test_bit(BT_ASLEEP, &flags)) {
        printk("%s, already asleep\n", __func__);
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        return;
    }
    //printk("%s, Call bluetooth_pm_rx_idle\n", __func__);
    bluetooth_pm_rx_idle();

    spin_unlock_irqrestore(&rw_lock, irq_flags);

    //printk("%s, Spin Unlock\n", __func__);

    //printk("--- %s, Rx timer expired\n", __func__);
}
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER


//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
static void bluetooth_pm_uart_control_timer_expire(unsigned long data)
{
    printk("%s, UART Control Timer expired\n", __func__);

    bluetooth_pm_tx_busy();
}
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOU


static irqreturn_t bluetooth_pm_hostwake_isr(int irq, void *dev_id)
{
    /* schedule a tasklet to handle the change in the host wake line */
    int wake, host_wake;
    wake = gpio_get_value(bsi->ext_wake);
    host_wake = gpio_get_value(bsi->host_wake);
    printk("%s, wake : (%d), host_wake : (%d)\n", __func__, wake, host_wake);

    irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

    if(host_wake == ASSERT)
    {
        //printk("%s, bluesleep_hostwake_isr : Registration Tasklet\n", __func__);
        tasklet_schedule(&hostwake_task);
    }

    //printk("--- %s\n", __func__);

    return IRQ_HANDLED;
}


//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
/*static*/ int bluetooth_pm_sleep_start(void)
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
{
    int ret;
    unsigned long irq_flags;
    printk("%s\n", __func__);

    spin_lock_irqsave(&rw_lock, irq_flags);

    if (test_bit(BT_PROTO, &flags)) {
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        return 0;
    }
    spin_unlock_irqrestore(&rw_lock, irq_flags);

    if (!atomic_dec_and_test(&open_count)) {
        atomic_inc(&open_count);
        return -EBUSY;
    }

#ifdef UART_CONTROL_MSM
    /*Activate UART*/
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
    //hsuart_power(1); //hsuart power on timing is changed. When preproto is set, excute hsuart_power.
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver
#endif/*UART_CONTROL_MSM*/
    ret = request_irq(bsi->host_wake_irq, bluetooth_pm_hostwake_isr,
                IRQF_DISABLED | IRQF_TRIGGER_LOW,
                "bluetooth hostwake", NULL);
    if (ret  < 0) {
        printk("%s, Couldn't acquire BT_HOST_WAKE IRQ\n", __func__);
        goto fail;
    }
    ret = enable_irq_wake(bsi->host_wake_irq);
    if (ret < 0) {
        printk("%s, Couldn't enable BT_HOST_WAKE as wakeup interrupt\n", __func__);
        free_irq(bsi->host_wake_irq, NULL);
        goto fail;
    }
    set_bit(BT_PROTO, &flags);
    wake_lock(&bsi->wake_lock);

//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
    uart_off_jiffies = 0;
    uart_on_jiffies = 0;
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT

    return 0;
fail:
    atomic_inc(&open_count);

    return ret;
}
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
EXPORT_SYMBOL(bluetooth_pm_sleep_start);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER


/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
/*static*/ void bluetooth_pm_sleep_stop(void)
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
{
    unsigned long irq_flags;

    spin_lock_irqsave(&rw_lock, irq_flags);

    printk("%s\n", __func__);

    if (!test_bit(BT_PROTO, &flags)) {
        printk("%s BT_PROTO is not set. Return\n", __func__);
        spin_unlock_irqrestore(&rw_lock, irq_flags);
        return;
    }

//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
    del_timer(&rx_timer);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
    del_timer(&tx_timer);

//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
    uart_off_jiffies = 0;
    uart_on_jiffies = 0;

    del_timer(&uart_control_timer);
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT

    clear_bit(BT_PROTO, &flags);
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
    clear_bit(BT_TXDATA,&flags);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER

    if (test_bit(BT_ASLEEP, &flags)) {
        clear_bit(BT_ASLEEP, &flags);
    }
#ifdef UART_CONTROL_MSM
    else{
        /*Deactivate UART*/
        //hsuart_power(0); //block uart power off
    }
#endif/*UART_CONTROL_MSM*/
    atomic_inc(&open_count);

    spin_unlock_irqrestore(&rw_lock, irq_flags);

    if (disable_irq_wake(bsi->host_wake_irq))
        printk("%s, Couldn't disable hostwake IRQ wakeup mode\n", __func__);
    free_irq(bsi->host_wake_irq, NULL);

    wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
EXPORT_SYMBOL(bluetooth_pm_sleep_stop);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER


//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
void bluetooth_pm_outgoing_data(void)
{
    unsigned long irq_flags;

    //printk("+++ %s\n", __func__);

    //Always restart BT TX Timer
    mod_timer(&tx_timer, jiffies + msecs_to_jiffies(TX_TIMER_INTERVAL));

    //printk("%s, Spin Lock\n", __func__);

    spin_lock_irqsave(&rw_lock, irq_flags);

    if(!test_bit(BT_TXDATA,&flags)){
        printk("%s BT_TXDATA is not set. Launch BT TX Busy and Set BT_TXDATA\n", __func__);
        BT_DBG("BT WAKE Set to Wake");

        set_bit(BT_TXDATA, &flags);
        gpio_set_value(bsi->ext_wake, 0);

        bluetooth_pm_tx_busy();
    }

    spin_unlock_irqrestore(&rw_lock, irq_flags);

    //printk("%s, Spin Unlock\n", __func__);

    //printk("--- %s\n", __func__);
}
EXPORT_SYMBOL(bluetooth_pm_outgoing_data);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER


//BT_S : [CONBT-1572] LGC_BT_COMMON_IMP_KERNEL_UART_SHUTDOWN_EXCEPTION_HANDLING
void bluetooth_pm_sleep_stop_by_uart(void)
{
    printk("%s\n", __func__);
    bluetooth_pm_sleep_stop();
}
EXPORT_SYMBOL(bluetooth_pm_sleep_stop_by_uart);
//BT_E : [CONBT-1572] LGC_BT_COMMON_IMP_KERNEL_UART_SHUTDOWN_EXCEPTION_HANDLING



//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
static int check_proc_preproto_show(struct seq_file *m, void *v)
{
    unsigned int preproto;

    //BT_INFO("");
    preproto = test_bit(BT_PREPROTO, &flags) ? 1 : 0;
    //printk("%s: preproto = %u \n",__func__,preproto);
    seq_printf(m, "preproto:%u\n", preproto);
    return 0;
}

static int proc_bt_preproto_open(struct inode *inode, struct file  *file)
{
    //BT_INFO("");
    return single_open(file,check_proc_preproto_show,NULL);
}

static ssize_t proc_bt_preproto_write(struct file *file, const char __user *buf,
               size_t length, loff_t *ppos)
{
    char preproto;

    BT_INFO("");

    if (length < 1)
        return -EINVAL;

    if (copy_from_user(&preproto, buf, 1))
        return -EFAULT;

    if (preproto == '1') {
        if(bsi->uport == NULL) {
            BT_INFO("NULL");
            bsi->uport = msm_hs_get_bt_uport(BT_PORT_NUM);
        } else {
            BT_INFO("NOT NULL");
        }
        hsuart_power(1);
    }
    /* claim that we wrote everything */
    return length;

}
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver



#if defined(CONFIG_BCM4335BT)
/* +++BRCM 4335 AXI Patch */
#define BTLOCK_NAME     "btlock"
#define BTLOCK_MINOR    224
/* BT lock waiting timeout, in second */
#define BTLOCK_TIMEOUT  2


struct btlock {
    int lock;
    int cookie;
};
static struct semaphore btlock;
static int count = 1;
static int owner_cookie = -1;

int bcm_bt_lock(int cookie)
{
    int ret;
    char cookie_msg[5] = {0};

    ret = down_timeout(&btlock, msecs_to_jiffies(BTLOCK_TIMEOUT*1000));
    if (ret == 0) {
        memcpy(cookie_msg, &cookie, sizeof(cookie));
        owner_cookie = cookie;
        count--;
        printk("%s, btlock acquired cookie: %s\n", __func__, cookie_msg);
    }

    return ret;
}
EXPORT_SYMBOL(bcm_bt_lock);

void bcm_bt_unlock(int cookie)
{
    char owner_msg[5] = {0};
    char cookie_msg[5] = {0};

    memcpy(cookie_msg, &cookie, sizeof(cookie));
    if (owner_cookie == cookie) {
        owner_cookie = -1;
        if (count++ > 1)
            printk("%s, error, release a lock that was not acquired**\n", __func__);
        up(&btlock);
        printk("%s, btlock released, cookie: %s\n", __func__, cookie_msg);
    } else {
        memcpy(owner_msg, &owner_cookie, sizeof(owner_cookie));
        printk("%s, ignore lock release,  cookie mismatch: %s owner %s\n", __func__, cookie_msg,
                owner_cookie == 0 ? "NULL" : owner_msg);
    }
}
EXPORT_SYMBOL(bcm_bt_unlock);

static int btlock_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int btlock_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t btlock_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    struct btlock lock_para;
    ssize_t ret = 0;

    if (count < sizeof(struct btlock))
        return -EINVAL;

    if (copy_from_user(&lock_para, buffer, sizeof(struct btlock)))
        return -EFAULT;

    if (lock_para.lock == 0)
        bcm_bt_unlock(lock_para.cookie);
    else if (lock_para.lock == 1)
        ret = bcm_bt_lock(lock_para.cookie);
    else if (lock_para.lock == 2)
        ret = bcm_bt_lock(lock_para.cookie);

    return ret;
}

static long btlock_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static const struct file_operations btlock_fops = {
    .owner   = THIS_MODULE,
    .open    = btlock_open,
    .release = btlock_release,
    .write   = btlock_write,
    .unlocked_ioctl = btlock_unlocked_ioctl,
};

static struct miscdevice btlock_misc = {
    .name  = BTLOCK_NAME,
    .minor = BTLOCK_MINOR,
    .fops  = &btlock_fops,
};

static int bcm_btlock_init(void)
{
    int ret;

    printk("%s\n", __func__);

    ret = misc_register(&btlock_misc);
    if (ret != 0) {
        printk("%s, Error: failed to register Misc driver,  ret = %d\n", __func__, ret);
        return ret;
    }
    sema_init(&btlock, 1);

    return ret;
}

static void bcm_btlock_exit(void)
{
    printk("%s\n", __func__);

    misc_deregister(&btlock_misc);
}
/* ---BRCM */
#endif /* defined(CONFIG_BCM4335BT) */


static int bluetooth_pm_rfkill_set_power(void *data, bool blocked)
{
    struct bluetooth_pm_device_info *bdev = data;
//BT_S : [CONBT-1256] Fix to kernel crash happened when CONFIG_DEBUG_SPINLOCK is removed
//  struct pinctrl_state *pins_state;
//BT_E : [CONBT-1256] Fix to kernel crash happened when CONFIG_DEBUG_SPINLOCK is removed

    unsigned long irq_flags;

    printk("%s: blocked (%d)\n", __func__, blocked);

    if (gpio_get_value(bsi->bt_reset) == !blocked)
    {
        printk("%s: Receive same status(%d)\n", __func__, blocked);
        return 0;
    }

    // Check Proto...
    spin_lock_irqsave(&rw_lock, irq_flags);
    if (test_bit(BT_PROTO, &flags)) {
        spin_unlock_irqrestore(&rw_lock, irq_flags);

        printk("%s: proto is enabled. kill proto first..\n", __func__);
        bluetooth_pm_sleep_stop();

        mdelay(100);
    }
    else
    {
        spin_unlock_irqrestore(&rw_lock, irq_flags);
    }

//BT_S : [CONBT-1256] Fix to kernel crash happened when CONFIG_DEBUG_SPINLOCK is removed
    /*if (!IS_ERR_OR_NULL(bdev->bt_pinctrl)) {
        pins_state = blocked ? bdev->gpio_state_active : bdev->gpio_state_suspend;
        if (pinctrl_select_state(bdev->bt_pinctrl, pins_state)) {
            pr_err("%s: error on pinctrl_select_state for bt enable - %s\n", __func__,
                    blocked ? "bt_enable_active" : "bt_enable_suspend");
        } else {
            printk("%s: success to set pinctrl for bt enable - %s\n", __func__,
                    blocked ? "bt_enable_active" : "bt_enable_suspend");
        }
    }*/
//BT_E : [CONBT-1256] Fix to kernel crash happened when CONFIG_DEBUG_SPINLOCK is removed

    printk("%s:  bdev->gpio_bt_reset = %d\n",__func__, bdev->gpio_bt_reset);
    printk("%s:  bdev->gpio_bt_host_wake = %d \n",__func__,bdev->gpio_bt_host_wake);
    printk("%s:  bdev->gpio_bt_ext_wake = %d \n",__func__,bdev->gpio_bt_ext_wake);

    if (!blocked) { // BT ON
        gpio_direction_output(bdev->gpio_bt_reset, 0);
        msleep(30);
        gpio_direction_output(bdev->gpio_bt_reset, 1);
        printk("%s: Bluetooth RESET HIGH!!\n", __func__);
    } else {  // BT OFF
        gpio_direction_output(bdev->gpio_bt_reset, 0);
        printk("%s: Bluetooth RESET LOW!!\n", __func__);
    }

    return 0;
}


static const struct rfkill_ops bluetooth_pm_rfkill_ops = {
    .set_block = bluetooth_pm_rfkill_set_power,
};

//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
static const struct file_operations preproto_proc = {
.owner      = THIS_MODULE,
.open       = proc_bt_preproto_open,
.read       = seq_read,
.release        = single_release,
.write      = proc_bt_preproto_write,
};
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver

static int bluetooth_pm_suspend(struct platform_device *pdev, pm_message_t state)
{
    printk("%s:\n", __func__);

    return 0;
}

static int bluetooth_pm_resume(struct platform_device *pdev)
{
    printk("%s:\n", __func__);

    return 0;
}


static int bluetooth_pm_create_bt_proc_interface(void)
{
    int ret;
    struct proc_dir_entry *ent;
    printk("%s: create bt proc interface\n", __func__);

    bluetooth_dir = proc_mkdir("bluetooth", NULL);
    if (bluetooth_dir == NULL) {
        printk("%s, Unable to create /proc/bluetooth directory\n", __func__);
        return -ENOMEM;
    }

    sleep_dir = proc_mkdir("sleep", bluetooth_dir);
    if (sleep_dir == NULL) {
        printk("%s, Unable to create /proc/%s directory\n", __func__, PROC_DIR);
        return -ENOMEM;
    }

//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
// uart driver issue fixed
    ent = proc_create("preproto", S_IRUGO, sleep_dir, &preproto_proc);
    if (ent ==NULL) {
        printk("%s, Unable to create /proc/%s/preproto entry\n", __func__, PROC_DIR);
        ret = -ENOMEM;
        goto fail;
    }

#ifdef BTA_NOT_USE_ROOT_PERM
    ent->uid = AID_BLUETOOTH;
    ent->gid = AID_NET_BT_STACK;
#endif  //BTA_NOT_USE_ROOT_PERM
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver

    return 0;

fail:
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
    remove_proc_entry("preproto", sleep_dir);
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver
    remove_proc_entry("sleep", bluetooth_dir);
    remove_proc_entry("bluetooth", 0);
    return ret;
}


static int bluetooth_pm_parse_dt(struct device *dev) {
    struct device_node *np = dev->of_node;
    printk("%s:  bluetooth_pm_parse_dt  is started   \n", __func__);

    bsi->bt_reset = of_get_named_gpio_flags(np, "gpio-bt-reset", 0, NULL);
    printk("%s: bt_reset GPIO Number = %d \n", __func__, bsi->bt_reset);
    if(bsi->bt_reset <0)
        return -ENOMEM;

    bsi->host_wake = of_get_named_gpio_flags(np, "gpio-bt-host-wake", 0, NULL);
    printk("%s: host_wake GPIO Number = %d \n", __func__, bsi->host_wake);
    if(bsi->host_wake <0)
        return -ENOMEM;

    bsi->ext_wake = of_get_named_gpio_flags(np, "gpio-bt-ext-wake", 0, NULL);
    printk("%s: ext_wake GPIO Number = %d \n", __func__, bsi->ext_wake);
    if(bsi->ext_wake <0)
        return -ENOMEM;

    return 0;
}

//BT_S : [CONBT-3529] LGC_BT_COMMON_IMP_KERNEL_MODIFY_8996_PIN_CONTROL
int bt_pinctrl_init(struct device *dev)
{
    struct pinctrl *bt_pinctrl;
    struct pinctrl_state *gpio_default_config;

    bt_pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR_OR_NULL(bt_pinctrl)) {
        pr_err("%s: target does not use pinctrl for bt\n", __func__);
        return -ENODEV;
    }

    gpio_default_config = pinctrl_lookup_state(bt_pinctrl, "bt_default");
    if (IS_ERR_OR_NULL(gpio_default_config)) {
        pr_err("%s: can't get gpio_default_config for bt\n", __func__);
        return -ENODEV;
    }

    if (pinctrl_select_state(bt_pinctrl, gpio_default_config)) {
        pr_err("%s: error on gpio_default_config for bt\n", __func__);
        return -ENODEV;
    } else {
        printk("%s: success to set gpio_default_config for bt\n", __func__);
    }

    return 0;
}
//BT_E : [CONBT-3529] LGC_BT_COMMON_IMP_KERNEL_MODIFY_8996_PIN_CONTROL

static int bluetooth_pm_probe(struct platform_device *pdev)
{
    int ret;
    bool default_state = true;  /* off */

    struct bluetooth_pm_device_info *bdev;

    printk("%s:  bluetooth_pm_probe is called \n", __func__);
    bsi = kzalloc(sizeof(struct bluetooth_pm_data), GFP_KERNEL);
    if (!bsi) {
        printk("%s:  bsi is null \n", __func__);
        return -ENOMEM;
    }

    if (pdev->dev.of_node) {
        pdev->dev.platform_data = bsi;
        ret = bluetooth_pm_parse_dt(&pdev->dev);
        if (ret < 0) {
            printk("%s: failed to parse device tree\n",__func__);
            goto free_res;
        }
    } else {
        printk("%s: No Device Node\n", __func__);
        goto free_res;
    }

    if(!bsi) {
        printk("%s: no bluetooth_pm_data \n", __func__);
        return -ENODEV;
    }

    bdev = kzalloc(sizeof(struct bluetooth_pm_device_info), GFP_KERNEL);
    if (!bdev) {
        printk("%s: bdev is null  \n", __func__);
        return -ENOMEM;
    }

    bdev->gpio_bt_reset = bsi->bt_reset;
    bdev->gpio_bt_host_wake = bsi->host_wake;
    bdev->gpio_bt_ext_wake = bsi->ext_wake;

    platform_set_drvdata(pdev, bdev);

    //BT_RESET
    ret = gpio_request_one(bdev->gpio_bt_reset, GPIOF_OUT_INIT_LOW, "bt_reset");
    if (ret) {
        pr_err("%s: failed to request gpio(%d)\n", __func__, bdev->gpio_bt_reset);
        goto free_res;
    }

    //BT_HOST_WAKE
    ret = gpio_request_one(bdev->gpio_bt_host_wake, GPIOF_IN, "bt_host_wake");
    if (ret) {
        pr_err("%s: failed to request gpio(%d)\n", __func__, bdev->gpio_bt_host_wake);
        goto free_res;
    }

    //BT_EXT_WAKE
    ret = gpio_request_one(bdev->gpio_bt_ext_wake, GPIOF_OUT_INIT_LOW, "bt_ext_wake");
    if (ret) {
        pr_err("%s: failed to request gpio(%d)\n", __func__, bdev->gpio_bt_ext_wake);
        goto free_res;
    }

    bluetooth_pm_rfkill_set_power(bdev, default_state);

    bdev->rfk = rfkill_alloc(pdev->name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
            &bluetooth_pm_rfkill_ops, bsi);
    if(unlikely(!bdev->rfk)) {
        pr_err("%s:  failed to alloc rfkill \n", __func__);
        ret = -ENOMEM;
        goto free_res;
    }

    rfkill_set_states(bdev->rfk, default_state, false);

    ret = rfkill_register(bdev->rfk);

    if (unlikely(ret)) {
        pr_err("%s:  failed to register rfkill \n", __func__);
        rfkill_destroy(bdev->rfk);
        kfree(bdev->rfk);
        goto free_res;
    }
    bsi->rfkill = bdev->rfk;

    bsi->host_wake_irq = platform_get_irq_byname(pdev, "host_wake_interrupt");
    if (bsi->host_wake_irq < 0) {
        pr_err("%s: Couldn't find host_wake irq \n",__func__);
    }


    printk("%s :  hostwake GPIO : %d(%u),  btwake GPIO : %d(%u) \n",__func__,
            bsi->host_wake,gpio_get_value(bsi->host_wake),bsi->ext_wake,gpio_get_value(bsi->ext_wake));
    printk("%s: bsi->host_wake_irq: %d \n", __func__,bsi->host_wake_irq);

//BT_S : [CONBT-3529] LGC_BT_COMMON_IMP_KERNEL_MODIFY_8996_PIN_CONTROL
    ret = bt_pinctrl_init(&pdev->dev);
//BT_E : [CONBT-3529] LGC_BT_COMMON_IMP_KERNEL_MODIFY_8996_PIN_CONTROL
    
    if (ret) {
        pr_err("%s: failed to init pinctrl\n", __func__);
        goto free_res;
    }

#ifdef UART_CONTROL_MSM
    bsi->uport = NULL;//msm_hs_get_bt_uport(BT_PORT_NUM);
#endif/*UART_CONTROL_MSM*/

    wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluetooth_pm");

//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
    flags = 0; /* clear all status bits */

    /* Initialize spinlock. */
    spin_lock_init(&rw_lock);

    /* Initialize timer */
    init_timer(&rx_timer);
    rx_timer.function = bluetooth_pm_rx_timer_expire;
    rx_timer.data = 0;

    /* Initialize timer */
    init_timer(&tx_timer);
    tx_timer.function = bluetooth_pm_tx_timer_expire;
    tx_timer.data = 0;

//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
    init_timer(&uart_control_timer);
    uart_control_timer.function = bluetooth_pm_uart_control_timer_expire;
    uart_control_timer.data = 0;
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT

    /* initialize host wake tasklet */
    tasklet_init(&hostwake_task, bluetooth_pm_hostwake_task, 0);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER

    bluetooth_pm_create_bt_proc_interface();

    return 0;

free_res:
    if(bsi->ext_wake)
        gpio_free(bsi->ext_wake);
    if(bsi->host_wake)
        gpio_free(bsi->host_wake);
    if(bsi->bt_reset)
        gpio_free(bsi->bt_reset);
    if (bsi->rfkill) {
        rfkill_unregister(bsi->rfkill);
        rfkill_destroy(bsi->rfkill);
        kfree(bsi->rfkill);
    }
    kfree(bsi);
    return ret;
}


static void bluetooth_pm_remove_bt_proc_interface(void)
{
//BT_S : [CONBT-966] Fix to Bluetooth sleep & uart driver
    remove_proc_entry("preproto", sleep_dir);
//BT_E : [CONBT-966] Fix to Bluetooth sleep & uart driver
    remove_proc_entry("sleep", bluetooth_dir);
    remove_proc_entry("bluetooth", 0);
}


static int bluetooth_pm_remove(struct platform_device *pdev)
{
    /* assert bt wake */
    gpio_set_value(bsi->ext_wake, 0);
    if (test_bit(BT_PROTO, &flags)) {
        if (disable_irq_wake(bsi->host_wake_irq))
            printk("%s, Couldn't disable hostwake IRQ wakeup mode \n", __func__);
        free_irq(bsi->host_wake_irq, NULL);
//BT_S : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
        del_timer(&rx_timer);
//BT_E : [CONBT-2112] LGC_BT_COMMON_IMP_KERNEL_V4L2_SLEEP_DRIVER
        del_timer(&tx_timer);
//BT_S : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
        del_timer(&uart_control_timer);
//BT_E : [CONBT-1475] LGC_BT_COMMON_IMP_KERNEL_UART_HCI_COMMAND_TIMEOUT
    }

    bluetooth_pm_remove_bt_proc_interface();

    if (bsi->ext_wake)
        gpio_free(bsi->ext_wake);
    if (bsi->host_wake)
        gpio_free(bsi->host_wake);
    if (bsi->rfkill) {
        rfkill_unregister(bsi->rfkill);
        rfkill_destroy(bsi->rfkill);
        kfree(bsi->rfkill);
    }
    if (bsi->bt_reset)
        gpio_free(bsi->bt_reset);

    wake_lock_destroy(&bsi->wake_lock);

    kfree(bsi);

    return 0;
}

/*
static struct platform_device bluetooth_pm_device = {
    .name = "bluetooth_pm",
    .id             = -1,
};
*/
static struct of_device_id bluetooth_pm_match_table[] = {
        { .compatible = "lge,bluetooth_pm"  },
        { },
};

static struct platform_driver bluetooth_pm_driver = {
    .probe = bluetooth_pm_probe,
    .remove = bluetooth_pm_remove,
    .suspend = bluetooth_pm_suspend,
    .resume = bluetooth_pm_resume,
    .driver = {
           .name = "bluetooth_pm",
           .owner = THIS_MODULE,
           .of_match_table = bluetooth_pm_match_table,
    },
};

//BT_S : [CONBT-2025] LGC_BT_COMMON_IMP_MOS_V4L2
static struct platform_device bcm_ldisc_device = {
    .name = "bcm_ldisc",
    .id = -1,
    .dev = {

    },
};
//BT_E : [CONBT-2025] LGC_BT_COMMON_IMP_MOS_V4L2

static int __init bluetooth_pm_init(void)
{
    int ret;

    printk("+++bluetooth_pm_init\n");
    ret = platform_driver_register(&bluetooth_pm_driver);
    if (ret)
        printk("Fail to register bluetooth_pm platform driver\n");
    printk("---bluetooth_pm_init done\n");

//BT_S : [CONBT-2025] LGC_BT_COMMON_IMP_MOS_V4L2
    platform_device_register(&bcm_ldisc_device);
//BT_E : [CONBT-2025] LGC_BT_COMMON_IMP_MOS_V4L2

    //return platform_device_register(&bluetooth_pm_device);
    return ret;
}


static void __exit bluetooth_pm_exit(void)
{
    platform_driver_unregister(&bluetooth_pm_driver);
}

device_initcall(bluetooth_pm_init);
module_exit(bluetooth_pm_exit);

MODULE_DESCRIPTION("bluetooth PM");
MODULE_AUTHOR("UNKNOWN");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
