/*
 * Copyright (c) 2016 -, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_UFS_LGE_CARD_RESET_DEBUGFS
#include <linux/debugfs.h>
#endif
#include "ufshcd.h"
#include "ufs-reset-gpio.h"

#ifdef CONFIG_UFS_LGE_CARD_RESET_DEBUGFS
struct ufs_card_reset_debugfs_files {
	struct dentry	*debugfs_root;
	struct dentry	*card_reset_control;
	struct dentry	*enable;
};
#endif

struct ufs_card_reset_info {
	#define UFSDEV_RESET_DELAY_MS 100
	struct delayed_work     reset_work;
	unsigned int            reset_count;
	spinlock_t              reset_lock;
	bool                    reset_enabled;

	int                     pin_num;
	unsigned int            active_conf;

	int                     use_pinctrl;
	struct pinctrl          *pctrl;
	struct pinctrl_state    *pins_reset_on;
	struct pinctrl_state    *pins_reset_off;
#ifdef CONFIG_UFS_LGE_CARD_RESET_DEBUGFS
	struct ufs_card_reset_debugfs_files debugfs_files;
#endif
};

#ifdef CONFIG_UFS_LGE_CARD_RESET_DEBUGFS
static int ufsdbg_card_reset_control_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	struct ufs_card_reset_info *card_reset_info
		= (struct ufs_card_reset_info *)hba->card_reset_info;

	seq_printf(file, "reset count : %u\n", card_reset_info->reset_count);
	seq_printf(file, "pin number : %d\n", card_reset_info->pin_num);
	seq_printf(file, "active conf : %u\n", card_reset_info->active_conf);
	seq_printf(file, "pin control : %s\n", card_reset_info->use_pinctrl ? "Y":"N");
	seq_puts(file, "\n");

	seq_puts(file, "echo 1 > /sys/kernel/debug/.../card_reset_control\n");
	seq_puts(file, "to resets the UFS device\n\n");
	return 0;
}

static int ufsdbg_card_reset_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_card_reset_control_show, inode->i_private);
}

static ssize_t ufsdbg_card_reset_control_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if(ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	if(val==1)
		ufs_card_reset(hba, true);

	return cnt;
}

static const struct file_operations ufsdbg_card_reset_control = {
	.open		= ufsdbg_card_reset_control_open,
	.read		= seq_read,
	.write		= ufsdbg_card_reset_control_write,
};

static int ufsdbg_card_reset_enable_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	struct ufs_card_reset_info *card_reset_info
		= (struct ufs_card_reset_info *)hba->card_reset_info;

	seq_printf(file, "reset enable : %d\n", card_reset_info->reset_enabled);
	seq_puts(file, "\n");

	return 0;
}

static int ufsdbg_card_reset_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_card_reset_enable_show, inode->i_private);
}

static ssize_t ufsdbg_card_reset_enable_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if(ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	if(val==0)
		ufs_card_reset_enable(hba, false);
	else
		ufs_card_reset_enable(hba, true);

	return cnt;
}

static const struct file_operations ufsdbg_card_reset_enable = {
	.open		= ufsdbg_card_reset_enable_open,
	.read		= seq_read,
	.write		= ufsdbg_card_reset_enable_write,
};

static void ufs_card_reset_remove_debugfs(struct ufs_hba *hba)
{
	struct ufs_card_reset_info *card_reset_info
		= (struct ufs_card_reset_info *)hba->card_reset_info;
	debugfs_remove_recursive(card_reset_info->debugfs_files.debugfs_root);
	card_reset_info->debugfs_files.debugfs_root = NULL;
}

void ufs_card_reset_add_debugfs(struct ufs_hba *hba)
{
	struct dentry *root = hba->debugfs_files.debugfs_root;
	struct ufs_card_reset_info *card_reset_info
		= (struct ufs_card_reset_info *)hba->card_reset_info;

	if (!hba || !root || !card_reset_info) {
		pr_err("%s: NULL hba info or card_reset_info\n", __func__);
		goto err_no_root;
	}

	card_reset_info->debugfs_files.debugfs_root = debugfs_create_dir("card_reset", root);
	if (IS_ERR(card_reset_info->debugfs_files.debugfs_root) ||
		!card_reset_info->debugfs_files.debugfs_root) {
		dev_err(hba->dev, "%s: failed to initialize reset debug fs\n", __func__);
		goto err_no_root;
	}

	card_reset_info->debugfs_files.card_reset_control =
		debugfs_create_file("card_reset_control", S_IRUSR | S_IWUSR,
			card_reset_info->debugfs_files.debugfs_root, hba,
			&ufsdbg_card_reset_control);
	if (!card_reset_info->debugfs_files.card_reset_control) {
		dev_err(hba->dev,
			"%s: failed create card_reset_control debugfs entry",
				__func__);
		goto err;
	}

	card_reset_info->debugfs_files.enable =
		debugfs_create_file("enable", S_IRUSR | S_IWUSR,
			card_reset_info->debugfs_files.debugfs_root, hba,
			&ufsdbg_card_reset_enable);
	if (!card_reset_info->debugfs_files.enable) {
		dev_err(hba->dev,
			"%s: failed create card_reset_enable debugfs entry",
				__func__);
		goto err;
	}
	return;

err:
	ufs_card_reset_remove_debugfs(hba);
err_no_root:
	return;
}
#endif

static int ufs_init_card_reset_pinctrl(struct ufs_hba *hba)
{
	struct pinctrl *pctrl;
	struct ufs_card_reset_info *card_reset_info
		= (struct ufs_card_reset_info *)hba->card_reset_info;
	int ret = 0;

	pctrl = devm_pinctrl_get(hba->dev);
	card_reset_info->pctrl = pctrl;

	card_reset_info->pins_reset_on = pinctrl_lookup_state(
		card_reset_info->pctrl, "ufs_res_in_on");
	if (IS_ERR(card_reset_info->pins_reset_on)) {
		ret = PTR_ERR(card_reset_info->pins_reset_on);
		dev_err(hba->dev, "Could not get ufs_res_in_on pinstates, err:%d\n", ret);
	}

	card_reset_info->pins_reset_off = pinctrl_lookup_state(
		card_reset_info->pctrl, "ufs_res_in_off");
	if (IS_ERR(card_reset_info->pins_reset_off)) {
		ret = PTR_ERR(card_reset_info->pins_reset_off);
		dev_err(hba->dev, "Could not get ufs_res_in_off pinstates, err:%d\n", ret);
	}

	return ret;

}

static int ufs_populate_card_reset_gpio(struct ufs_hba *hba)
{
	struct device_node *np;
	enum of_gpio_flags ufs_flags = OF_GPIO_ACTIVE_LOW;
	struct gpio_desc *gpiod;
	unsigned long gpio_flags;
	struct ufs_card_reset_info *card_reset_info;
	int err = -EINVAL;

	if(!hba) {
		pr_err("%s, init reset gpio : ufs_hba invalid!\n", __func__);
		err = -EINVAL;
		goto init_end;
	}

	card_reset_info = (struct ufs_card_reset_info *)hba->card_reset_info;
	card_reset_info->pin_num = -1;

	np = hba->dev->of_node;
	if (np) {

		card_reset_info->pin_num = of_get_named_gpio_flags(np, "ufsreset-gpios",
			0, &ufs_flags);

		if (!gpio_is_valid(card_reset_info->pin_num)) {
			dev_err(hba->dev, "ufs reset gpio not specified(%d)\n",
							card_reset_info->pin_num);
			err = -EINVAL;
			goto init_end;
		}

		dev_dbg(hba->dev, "reset gpio (%d), flag(%d), init_value(%d)\n",
						card_reset_info->pin_num, ufs_flags,
						gpio_get_value(card_reset_info->pin_num));

		gpiod = gpio_to_desc(card_reset_info->pin_num);
		if(gpiod_is_active_low(gpiod)) {
			gpio_flags = GPIOF_OUT_INIT_HIGH;
			card_reset_info->active_conf = 0;
		}
		else {
			gpio_flags = GPIOF_OUT_INIT_LOW;
			card_reset_info->active_conf = 1;
		}

		if(hba->dev)
			err = devm_gpio_request_one(hba->dev, card_reset_info->pin_num,
									gpio_flags, "ufs_res_in");
		else
			err = gpio_request_one(card_reset_info->pin_num, gpio_flags, "ufs_res_in");

		if(err) {
			dev_err(hba->dev, "request reset gpio failed, rc=%d\n", err);
			goto init_end;
		}

		dev_dbg(hba->dev, "request reset gpio (value(%d))\n",
				gpio_get_value(card_reset_info->pin_num));

		// even though pinctrl init failed, we can go with pin on/off
		if(ufs_init_card_reset_pinctrl(hba)) {
			dev_err(hba->dev, "reset pinctrl init failed, rc=%d\n", err);
			card_reset_info->use_pinctrl = 0;
		} else {
			card_reset_info->use_pinctrl = 1;
			pinctrl_select_state(card_reset_info->pctrl, card_reset_info->pins_reset_off);
		}
		//gpio_export(card_reset_info->pin_num, 0);
	}

init_end:
	if(err)
		card_reset_info->pin_num = -1;

	return err;
}

/**
* ufs_card_reset
* @
*
* execute ufs card reset : approve low signal to UFS_RESET_N
* JESD220C :: 7.1.2. Hardware reset
* The reset signal is active low. The UFS device shall not detect 100 ns or
* less of positive or negative RST_n pulse. The UFS device shall detect
* more than or equal to 1us of positive or negative RST_n pulse width.
*/
static void _ufs_do_card_reset(struct ufs_card_reset_info *card_reset_info)
{
	unsigned long flags;

	if (!card_reset_info || !gpio_is_valid(card_reset_info->pin_num)) {
		pr_err("%s, invalid param.\n", __func__);
		return;
	}

	pr_info("%s, _ufs_do_card_reset.\n", __func__);
	spin_lock_irqsave(&card_reset_info->reset_lock, flags);

	// make high to approve low signal to UFS_RESET_N
	if(card_reset_info->pctrl && card_reset_info->use_pinctrl) {
		pinctrl_select_state(card_reset_info->pctrl, card_reset_info->pins_reset_on);
		udelay(5);
		pinctrl_select_state(card_reset_info->pctrl, card_reset_info->pins_reset_off);
	}
	else {
		gpio_set_value(card_reset_info->pin_num, card_reset_info->active_conf);
		udelay(5);
		gpio_set_value(card_reset_info->pin_num, !card_reset_info->active_conf);
	}

	// skip count 0, except real init case that the card reset never happened yet.
	if (UINT_MAX <= card_reset_info->reset_count) {
		card_reset_info->reset_count = 0;
	}
	card_reset_info->reset_count++;

	spin_unlock_irqrestore(&card_reset_info->reset_lock, flags);
	usleep_range(6000, 6000+1000);

}

static void ufs_card_reset_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ufs_card_reset_info *card_reset_info
				= container_of(dwork, struct ufs_card_reset_info, reset_work);

	_ufs_do_card_reset(card_reset_info);
	return;
}

int ufs_card_reset_enable(struct ufs_hba *hba, bool enable)
{
	struct ufs_card_reset_info *card_reset_info;

	if(!hba)
		return -EINVAL;

	card_reset_info = (struct ufs_card_reset_info *)hba->card_reset_info;
	card_reset_info->reset_enabled = enable;

	return 0;
}

int ufs_card_reset(struct ufs_hba *hba, bool async)
{
	struct ufs_card_reset_info *card_reset_info;

	if(!hba) {
		pr_err("%s, ufs_hba invalid\n", __func__);
		return -EINVAL;
	}

	dev_info(hba->dev, "%s, card reset called, async(%d)\n", __func__, async);

	card_reset_info = (struct ufs_card_reset_info *)hba->card_reset_info;
	if(!card_reset_info) {
		dev_err(hba->dev, "%s, card reset info is not initialized\n", __func__);
		return -EINVAL;
	}

	if(!card_reset_info->reset_enabled) {
		dev_err(hba->dev, "%s, card reset disabled, return\n", __func__);
		return -EINVAL;
	}

	if(async) {
		schedule_delayed_work(&card_reset_info->reset_work,
						msecs_to_jiffies(UFSDEV_RESET_DELAY_MS));
	} else {
		_ufs_do_card_reset(card_reset_info);
	}

	return 0;
}

int ufs_card_reset_init(struct ufs_hba *hba)
{
	int err;
	struct ufs_card_reset_info *card_reset_info;

	if(!hba) {
		pr_err("%s, init reset gpio : ufs_hba invalid!\n", __func__);
		return -EINVAL;
	}

	card_reset_info = kzalloc(sizeof(struct ufs_card_reset_info), GFP_KERNEL);
	if(!card_reset_info)
		return -ENOMEM;

	card_reset_info->reset_count = 0;
	spin_lock_init(&card_reset_info->reset_lock);

	// set card reset interface to hba
	hba->card_reset_info = (void*)card_reset_info;
	err = ufs_populate_card_reset_gpio(hba);
	if(!err && gpio_is_valid(card_reset_info->pin_num)) {
		INIT_DELAYED_WORK(&card_reset_info->reset_work, ufs_card_reset_work);
		card_reset_info->reset_enabled = true;
	} else {
		dev_err(hba->dev, "%s, ufs card reset gpio init fail %d\n", __func__, err);
		kfree(card_reset_info);
		hba->card_reset_info = NULL;
	}

	return err;
}

