/*
 * DIAG MTS for LGE MTS Kernel Driver
 *
 *  <LGMSP-MTS@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "mts_tty.h"

struct mts_tty *mts_tty = NULL;
static char *stop_push = "mts_tty_stop_push";

int mts_stop_ready_complete(void)
{
    int num_push = 0, total_push = 0;
    int left = strlen(stop_push);
	struct mts_tty *mts_tty_drv = mts_tty;

	if (mts_tty_drv == NULL)  {
		return -1;
	}

    //num_push = tty_insert_flip_string(mts_tty_drv->tty_struct,
    num_push = tty_insert_flip_string(mts_tty_drv->mts_tty_port,
            stop_push + total_push, left);
    total_push += num_push;
    left -= num_push;
    //tty_flip_buffer_push(mts_tty_drv->tty_struct);
    tty_flip_buffer_push(mts_tty_drv->mts_tty_port);
    printk("%s\n", __func__);
    return 0;
}

int mts_tty_process(char *buf, int left)
{
	struct mts_tty *mts_tty_drv = mts_tty;
	int num_push = 0;
	int total_push = 0;

	if (mts_tty_drv == NULL)  {
		return -1;
	}

    switch (mts_tty->run) {
        case MTS_START_READY:
            printk("%s MTS_START_READY\n", __func__);
            return -1;
        case MTS_STOP_READY_COMPLETE:
            printk("%s MTS_STOP_READY_COMPLETE\n", __func__);
            return -1;
        case MTS_STOP_READY:
            printk("%s MTS_STOP_READY\n", __func__);
            mts_tty->run = MTS_STOP_READY_COMPLETE;
            mts_stop_ready_complete();
            return -1;
        default:
            break;
    }

	//num_push = tty_insert_flip_string(mts_tty_drv->tty_struct,
	num_push = tty_insert_flip_string(mts_tty_drv->mts_tty_port,
			buf + total_push, left);
	total_push += num_push;
	left -= num_push;
	//tty_flip_buffer_push(mts_tty_drv->tty_struct);
	tty_flip_buffer_push(mts_tty_drv->mts_tty_port);

    //printk("mts_tty push %d\n", total_push);
	return 0;
}

static int mts_tty_open(struct tty_struct *tty, struct file *file)
{
	struct mts_tty *mts_tty_drv = NULL;

	if (!tty)
		return -ENODEV;

	mts_tty_drv = mts_tty;

	if (!mts_tty_drv)
		return -ENODEV;

    tty_port_tty_set(mts_tty_drv->mts_tty_port, tty);
    mts_tty_drv->mts_tty_port->low_latency = 0;

	tty->driver_data = mts_tty_drv;
	mts_tty_drv->tty_struct = tty;

	set_bit(TTY_NO_WRITE_SPLIT, &mts_tty_drv->tty_struct->flags);

	pr_debug(KERN_INFO "mts_tty_open TTY device open %d,%d\n", 0, 0);
	return 0;
}

static void mts_tty_close(struct tty_struct *tty, struct file *file)
{
	struct mts_tty *mts_tty_drv = NULL;

	if (!tty) {
		printk( "mts_tty_close FAIL."
				 "tty is Null %d,%d\n", 0, 0);
		return;
	}

	mts_tty_drv = tty->driver_data;
    tty_port_tty_set(mts_tty_drv->mts_tty_port, NULL);
	printk( "mts_tty_close TTY device close %d,%d\n", 0, 0);

	return;
}

static int mts_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
			  unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
     case MTS_TTY_START_READY:
		mts_tty->run = MTS_START_READY;
		printk("mts_tty->run: MTS_START_READY (%s)\n", __func__);
        break;
	case MTS_TTY_START:
		mts_tty->run = MTS_ON;
		printk("mts_tty->run: MTS_ON (%s)\n", __func__);
		break;
     case MTS_TTY_STOP_READY:
		printk("mts_tty->run: MTS_TTY_STOP_READY (%s)\n", __func__);
        mts_tty->run = MTS_STOP_READY;
        break;
	case MTS_TTY_STOP:
		mts_tty->run = MTS_OFF;
		printk("mts_tty->run: MTS_OFF (%s)\n", __func__);
		break;
	default:
		printk("mts_tty->run: unknown (%s)\n", __func__);
		break;
	}
	return ret;
}

static void mts_tty_unthrottle(struct tty_struct *tty) {
	return;
}

static int mts_tty_write_room(struct tty_struct *tty) {
	return DIAG_MTS_TX_SIZE;
}

static int mts_tty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct mts_tty *mts_tty_drv = NULL;
	mts_tty_drv = mts_tty;
	tty->driver_data = mts_tty_drv;
	mts_tty_drv->tty_struct = tty;

    print_hex_dump(KERN_DEBUG, "mts_tty_write ", 16, 1, DUMP_PREFIX_ADDRESS, buf+4, count-4, 1);

    return count;
}

static const struct tty_operations mts_tty_ops = {
	.open = mts_tty_open,
	.close = mts_tty_close,
	.write = mts_tty_write,
	.write_room = mts_tty_write_room,
	.unthrottle = mts_tty_unthrottle,
	.ioctl = mts_tty_ioctl,
};

#if 0
static int mts_pm_notify(struct notifier_block *b, unsigned long event, void *p)
{
	wake_up_interruptible(&mts_tty->waitq);

	switch (event) {
        case PM_SUSPEND_PREPARE:
		mts_tty->pm_notify_info = 3;
		printk("mts_pm_notify: PM_SUSPEND_PREPARE\n");
                break;
        case PM_POST_SUSPEND:
		mts_tty->pm_notify_info = 4;
		printk("mts_pm_notify: PM_POST_SUSPEND\n");
                break;
	default:
		break;
        }

        return 0;
}
#endif

static int __init mts_tty_init(void)
{
	int ret = 0;
	struct device *tty_dev =  NULL;
	struct mts_tty *mts_tty_drv = NULL;

	mts_tty_drv = kzalloc(sizeof(struct mts_tty), GFP_KERNEL);
	if (mts_tty_drv == NULL) {
		printk( "mts_tty_drv: memory alloc fail %d - %d\n", 0, 0);
		return 0;
	}

	mts_tty_drv->mts_tty_port = kzalloc(sizeof(struct tty_port), GFP_KERNEL);
	if (mts_tty_drv->mts_tty_port == NULL) {
		printk( "mts_tty_drv->mts_tty_port: memory alloc fail %d - %d\n", 0, 0);
		kfree(mts_tty_drv);
		return 0;
	}

	tty_port_init(mts_tty_drv->mts_tty_port);

	mts_tty = mts_tty_drv;
	mts_tty_drv->tty_drv = alloc_tty_driver(MAX_DIAG_MTS_DRV);

	if (!mts_tty_drv->tty_drv) {
		printk( "mts_tty_init: tty alloc driver fail %d - %d\n", 1, 0);
		kfree(mts_tty_drv);
		return 0;
	}

	mts_tty_drv->tty_drv->name = "mts_tty";
	mts_tty_drv->tty_drv->owner = THIS_MODULE;
	mts_tty_drv->tty_drv->driver_name = "mts_tty";

	/* uses dynamically assigned dev_t values */
	mts_tty_drv->tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	mts_tty_drv->tty_drv->subtype = SERIAL_TYPE_NORMAL;
	mts_tty_drv->tty_drv->flags = TTY_DRIVER_REAL_RAW |
				       TTY_DRIVER_DYNAMIC_DEV |
				       TTY_DRIVER_RESET_TERMIOS;

	/* initializing the mts driver */
	mts_tty_drv->tty_drv->init_termios = tty_std_termios;
	mts_tty_drv->tty_drv->init_termios.c_iflag = IGNBRK | IGNPAR;
	mts_tty_drv->tty_drv->init_termios.c_oflag = 0;
	mts_tty_drv->tty_drv->init_termios.c_cflag =
        B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	mts_tty_drv->tty_drv->init_termios.c_lflag = 0;

	spin_lock_init(&mts_tty_drv->lock);

	tty_set_operations(mts_tty_drv->tty_drv, &mts_tty_ops);
	tty_port_link_device(mts_tty_drv->mts_tty_port, mts_tty_drv->tty_drv, 0);
	ret = tty_register_driver(mts_tty_drv->tty_drv);

	if (ret) {
        printk("fail to mts tty_register_driver\n");
		put_tty_driver(mts_tty_drv->tty_drv);
		tty_port_destroy(mts_tty_drv->mts_tty_port);
		mts_tty_drv->tty_drv = NULL;
        kfree(mts_tty_drv->mts_tty_port);
		kfree(mts_tty_drv);
		return 0;
	}

	tty_dev = tty_register_device(mts_tty_drv->tty_drv, 0, NULL);

	if (IS_ERR(tty_dev)) {
        printk("fail to mts tty_register_device\n");
		tty_unregister_driver(mts_tty_drv->tty_drv);
		put_tty_driver(mts_tty_drv->tty_drv);
		tty_port_destroy(mts_tty_drv->mts_tty_port);
        kfree(mts_tty_drv->mts_tty_port);
		kfree(mts_tty_drv);
		return 0;
	}
/*
	mts_tty->pm_notify.notifier_call = mts_pm_notify;
	register_pm_notifier(&mts_tty->pm_notify);
	init_waitqueue_head(&mts_tty->waitq);
*/
	mts_tty->run = 0;
//	mts_tty->pm_notify_info = 0;

	printk( "mts_tty_init success\n");
	return 0;
}

static void __exit mts_tty_exit(void)
{
	int ret = 0;
	struct mts_tty *mts_tty_drv = NULL;

	mts_tty_drv = mts_tty;

	if (!mts_tty_drv) {
		printk(": %s:" "NULL mts_tty_drv", __func__);
		return;
	}
	tty_port_destroy(mts_tty_drv->mts_tty_port);
	mdelay(20);
	tty_unregister_device(mts_tty_drv->tty_drv, 0);
	ret = tty_unregister_driver(mts_tty_drv->tty_drv);
	put_tty_driver(mts_tty_drv->tty_drv);
	mts_tty_drv->tty_drv = NULL;
    kfree(mts_tty_drv->mts_tty_port);
	kfree(mts_tty_drv);
	mts_tty = NULL;

//	unregister_pm_notifier(&mts_tty->pm_notify);

	printk( "mts_tty_exit  SUCESS %d - %d\n", 0, 0);
	return;
}

module_init(mts_tty_init);
module_exit(mts_tty_exit);

MODULE_DESCRIPTION("LGE MTS TTY");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("LGMSP-MTS@lge.com>");

