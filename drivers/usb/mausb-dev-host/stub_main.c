/*
 * Copyright (C) 2003-2008 Takahiro Hirofuchi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>



#include "mausb_common.h"
#include "stub.h"
#include "mausb_util.h"

#define DRIVER_AUTHOR "Takahiro Hirofuchi"
#define DRIVER_DESC "USB/IP Host Driver"

#define DEVICE_BUS_ID	"1-1"


#define LGFTM_MAUSB 204
#define LGFTM_MAUSB_SIZE 1
//extern int set_ftm_item(int id, int size, void *in);

/*
 * busid_tables defines matching busids that mausb can grab. A user can change
 * dynamically what device is locally used and what device is exported to a
 * remote host.
 */
#define MAX_BUSID 16
static struct bus_id_priv busid_table[MAX_BUSID];
static spinlock_t busid_table_lock;



static void init_busid_table(void)
{
	/*
	 * This also sets the bus_table[i].status to
	 * MAUSB_STUB_BUSID_OTHER, which is 0.
	 */
	memset(busid_table, 0, sizeof(busid_table));

//	strncpy(busid_table[0].name, DEVICE_BUS_ID, BUSID_SIZE);
//	busid_table[0].status = MAUSB_STUB_BUSID_ADDED;


	spin_lock_init(&busid_table_lock);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "lock3: %p",(void*)&busid_table_lock);
}

/*
 * Find the index of the busid by name.
 * Must be called with busid_table_lock held.
 */
static int get_busid_idx(const char *busid)
{
	int i;
	int idx = -1;

	for (i = 0; i < MAX_BUSID; i++)
		if (busid_table[i].name[0]) {
			LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN,  "busid: %s \n",busid_table[i].name);
			if (!strncmp(busid_table[i].name, busid, BUSID_SIZE)) {
				idx = i;
				break;
			}
		}
	return idx;
}

struct bus_id_priv *get_busid_priv(const char *busid)
{
	int idx;
	struct bus_id_priv *bid = NULL;

	spin_lock(&busid_table_lock);
	idx = get_busid_idx(busid);
	if (idx >= 0)
		bid = &(busid_table[idx]);
	spin_unlock(&busid_table_lock);

	return bid;
}

static int add_match_busid(char *busid)
{
	int i;
	int ret = -1;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN,  "match busid: %s \n",busid);
	spin_lock(&busid_table_lock);
	/* already registered? */
	if (get_busid_idx(busid) >= 0) {
		ret = 0;
		goto out;
	}

	for (i = 0; i < MAX_BUSID; i++)
		if (!busid_table[i].name[0]) {
			strncpy(busid_table[i].name, busid, BUSID_SIZE-1);
			if ((busid_table[i].status != MAUSB_STUB_BUSID_ALLOC) &&
			    (busid_table[i].status != MAUSB_STUB_BUSID_REMOV))
				busid_table[i].status = MAUSB_STUB_BUSID_ADDED;
			ret = 0;
			break;
		}

out:
	spin_unlock(&busid_table_lock);

	return ret;
}

int del_match_busid(char *busid)
{
	int idx;
	int ret = -1;

	spin_lock(&busid_table_lock);
	idx = get_busid_idx(busid);
	if (idx < 0)
		goto out;

	/* found */
	ret = 0;

	if (busid_table[idx].status == MAUSB_STUB_BUSID_OTHER)
		memset(busid_table[idx].name, 0, BUSID_SIZE);

	if ((busid_table[idx].status != MAUSB_STUB_BUSID_OTHER) &&
	    (busid_table[idx].status != MAUSB_STUB_BUSID_ADDED))
		busid_table[idx].status = MAUSB_STUB_BUSID_REMOV;

out:
	spin_unlock(&busid_table_lock);

	return ret;
}

static ssize_t show_match_busid(struct device_driver *drv, char *buf)
{
	int i;
	char *out = buf;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s", __func__);
	spin_lock(&busid_table_lock);
	for (i = 0; i < MAX_BUSID; i++)
		if (busid_table[i].name[0])
			out += sprintf(out, "%s ", busid_table[i].name);
	spin_unlock(&busid_table_lock);
	out += sprintf(out, "\n");

	return out - buf;
}

static ssize_t store_match_busid(struct device_driver *dev, const char *buf,
				 size_t count)
{
	int len;
	char busid[BUSID_SIZE];

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "\n---> %s",__func__);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "\n count %zu  Buffer: %s", count, buf);
	if (count < 5)
		return -EINVAL;

	/* strnlen() does not include \0 */
	len = strnlen(buf + 4, BUSID_SIZE);

	/* busid needs to include \0 termination */
	if (!(len < BUSID_SIZE))
		return -EINVAL;

	strncpy(busid, buf + 4, BUSID_SIZE-1);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "\n BusID: %s", busid);
	if (!strncmp(buf, "add ", 4)) {
		if (add_match_busid(busid) < 0)
			return -ENOMEM;

		pr_debug("add busid %s\n", busid);
		return count;
	}

	if (!strncmp(buf, "del ", 4)) {
		if (del_match_busid(busid) < 0)
			return -ENODEV;

		pr_debug("del busid %s\n", busid);
		return count;
	}

	return -EINVAL;
}
static DRIVER_ATTR(match_busid, S_IRUSR | S_IWUSR, show_match_busid,
		   store_match_busid);

static struct stub_mausb_pal *stub_priv_pop_from_listhead(struct list_head *listhead)
{
	struct stub_mausb_pal *priv, *tmp;

	list_for_each_entry_safe(priv, tmp, listhead, list) {
		list_del(&priv->list);
		return priv;
	}

	return NULL;
}


static struct stub_mausb_pal *stub_mausb_pal_pop(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_mausb_pal *pal;

	spin_lock_irqsave(&sdev->mausb_pal_lock, flags);

	pal = (struct stub_mausb_pal *)stub_priv_pop_from_listhead(&sdev->mausb_pal_in_init);
	if (pal)
		goto done1;

	pal = (struct stub_mausb_pal *)stub_priv_pop_from_listhead(&sdev->mausb_pal_out_init);
	if (pal)
		goto done1;

	pal = (struct stub_mausb_pal *)stub_priv_pop_from_listhead(&sdev->mausb_pal_mgmt_init);
	if (pal)
		goto done1;

	pal = (struct stub_mausb_pal *)stub_priv_pop_from_listhead(&sdev->mausb_pal_tx);
	if (pal)
		goto done1;

	pal = (struct stub_mausb_pal *)stub_priv_pop_from_listhead(&sdev->mausb_pal_free);
	if (pal)
		goto done1;

done1:
	spin_unlock_irqrestore(&sdev->mausb_pal_lock, flags);
	return pal;

}

void stub_device_cleanup_urbs(struct stub_device *sdev)
{
	struct stub_mausb_pal *pal;
	struct urb *urb;

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN,  " %s  \n",__func__);

	if(sdev==NULL)
	{
		return;
	}
	dev_dbg(&sdev->udev->dev, "free sdev %p\n", sdev);



	while ((pal = stub_mausb_pal_pop(sdev))) {
		urb = pal->urb;
		dev_dbg(&sdev->udev->dev, "free urb %p\n", urb);
		if (urb)
			usb_kill_urb(urb);
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, " %s  pal %p\n",__func__,pal);
		if (urb){
			if (urb->transfer_buffer)
				kfree(urb->transfer_buffer);
			if (urb->setup_packet)
				kfree(urb->setup_packet);
			usb_free_urb(urb);
		}
		kfree(pal);
	}


}

int task_mausb_enable_loop(void *data)
{
	unsigned int mausb_enable=0;

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);

	mausb_enable=1;
	//set_ftm_item(LGFTM_MAUSB,LGFTM_MAUSB_SIZE,&mausb_enable);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "<-- %s",__func__);
	return 0;
}

int mausb_enable_thread(void)
{
	struct task_struct *task_mausb_enable;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	task_mausb_enable =  kthread_get_run(task_mausb_enable_loop, NULL,  "task_mausb_enable");
	return 0;

}

int task_mausb_disable_loop(void *data)
{
	unsigned int mausb_enable=0;

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);

	mausb_enable=0;
	//set_ftm_item(LGFTM_MAUSB,LGFTM_MAUSB_SIZE,&mausb_enable);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "<-- %s",__func__);
	return 0;
}


int mausb_disable_thread(void)
{
	struct task_struct *task_mausb_disable;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	task_mausb_disable =  kthread_get_run(task_mausb_disable_loop, NULL,  "task_mausb_disable");
	return 0;

}

int mausb_bind_function(const char *buf)
{
	int ret = 0;
	char *argv_bind[] = { "/system/bin/mausb", "bind", "--busid=1-1", NULL };
	char *argv_upnp[] = {"/system/bin/upnp-server", "&", NULL};
	char *argv_mausbd[] = {"/system/bin/mausbd", "-D", NULL};

	char *argv_unbind[] = { "/system/bin/mausb", "unbind", "--busid=1-1", NULL };
	char *argv_kill_mausbd[] = {"/system/bin/pkill","mausbd", NULL};
	char *argv_kill_upnp[] = {"/system/bin/pkill", "upnp-server", NULL};

	static char *envp[] = {
		"HOME=/system/bin",
		"TERM=linux",
		"SHELL=/system/bin/sh",
		"LD_LIBRARY_PATH=/vendor/lib:/system/lib",
		"MKSH=/system/bin/sh",
		"PATH=/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin", NULL };

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	if (!strcmp(buf,"bind"))
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "bind command received \n");
		ret = call_usermodehelper( argv_bind[0], argv_bind, envp, UMH_WAIT_EXEC );
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "ret: %d\n",ret);
		ret = call_usermodehelper( argv_mausbd[0], argv_mausbd, envp, UMH_WAIT_EXEC );
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "ret: %d\n",ret);
		ret  = call_usermodehelper( argv_upnp[0], argv_upnp, envp, UMH_WAIT_EXEC );
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "ret: %d\n",ret);
	}
	else if (!strcmp(buf,"unbind"))
	{
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "unbind command received \n");
	ret  = call_usermodehelper( argv_kill_upnp[0], argv_kill_upnp, envp, UMH_WAIT_EXEC );
	ret  = call_usermodehelper( argv_unbind[0], argv_unbind, envp, UMH_WAIT_EXEC );
	ret  = call_usermodehelper( argv_kill_mausbd[0], argv_kill_mausbd, envp, UMH_WAIT_EXEC );
	}
	else if (!strcmp(buf,"mausb_enable"))
	{
	printk(KERN_INFO "mausb enable command received \n");
	mausb_enable_thread();
	}

	else if (!strcmp(buf,"mausb_disable"))
	{
		printk(KERN_INFO "mausb disable command received \n");
		mausb_disable_thread();
	}

	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "<-- %s",__func__);
	return ret;
}

int task_bind_loop(void *data)
{
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	msleep(400);
	mausb_bind_function("bind");
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "<-- %s",__func__);
	return 0;
}

int mausb_bind_unbind(const char *buf)
{
	struct task_struct *task_bind;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	  if (!strcmp(buf,"bind")) {
		task_bind =  kthread_get_run(task_bind_loop, NULL,  "task_bind");
	 	return 0;
	  }
	  return 0;
}

EXPORT_SYMBOL_GPL(mausb_bind_unbind);

static ssize_t mausb_bind_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	char buffer[16];
	sprintf(buffer,"%s",buf);
	return mausb_bind_function(buf);
}

static int mausb_bind_open(struct inode *ip, struct file *fp)
{
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "<-- %s",__func__);
	return 0;
}
static int mausb_bind_release(struct inode *ip, struct file *fp)
{
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "---> %s",__func__);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN, "<-- %s",__func__);
	return 0;
}

static const char mausb_shortname[] = "mausb_bind";

/* file operations for /dev/mausb_bind */
static const struct file_operations mausb_bind_ops = {
	.owner = THIS_MODULE,
	.write = mausb_bind_write,
	.open = mausb_bind_open,
	.release = mausb_bind_release,
};

static struct miscdevice mausb_bind_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = mausb_shortname,
	.fops = &mausb_bind_ops,
};

static int __init mausb_device_init(void)
{
	int ret;

	init_busid_table();

	ret = usb_register(&stub_driver);
	if (ret) {
		pr_err("usb_register failed %d\n", ret);
		goto err_usb_register;
	}

	ret = driver_create_file(&stub_driver.drvwrap.driver,
				 &driver_attr_match_busid);
	if (ret) {
		pr_err("driver_create_file failed\n");
		goto err_create_file;
	}

	ret = misc_register(&mausb_bind_device);

	if (ret)
			goto err_create_file;

	pr_info(DRIVER_DESC " v" MAUSB_VERSION "\n");

	return ret;

err_create_file:
	usb_deregister(&stub_driver);
err_usb_register:
	return ret;
}

static void __exit mausb_device_exit(void)
{
	//david kernel panic NW model
	misc_deregister(&mausb_bind_device);
	driver_remove_file(&stub_driver.drvwrap.driver,
			   &driver_attr_match_busid);

	/*
	 * deregister() calls stub_disconnect() for all devices. Device
	 * specific data is cleared in stub_disconnect().
	 */
	usb_deregister(&stub_driver);
}

module_init(mausb_device_init);
module_exit(mausb_device_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(MAUSB_VERSION);
