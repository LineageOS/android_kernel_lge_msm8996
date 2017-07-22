/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
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

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/printk.h>
#include "anx7688_debugfs.h"
#include "anx7688_mi1.h"
#include "anx_i2c_intf.h"

#define DEBUG_BUF_SIZE 4096
#define FW_LENGTH 16
extern struct anx7688_firmware *__must_check
anx7688_firmware_alloc(struct anx7688_chip *chip);

static int anx7688_debugfs_open(struct inode *inode, struct file *file)
{
	struct anx7688_debugfs *dbgfs;

	dbgfs = kzalloc(sizeof(struct anx7688_debugfs), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(dbgfs)) {
		pr_err("%s : Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	dbgfs->chip = inode->i_private;
	file->private_data = dbgfs;
	return 0;
}

static int anx7688_debugfs_release(struct inode *inode, struct file *file)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	kfree(dbgfs);
	return 0;
}

/*
 * anx7688_debugfs_power
 */
static ssize_t anx7688_debugfs_power_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	struct anx7688_chip *chip = dbgfs->chip;
	char *buf;
	unsigned int buf_size;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = scnprintf(buf, buf_size, "%d\n", atomic_read(&chip->power_on));
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t anx7688_debugfs_power_write(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	struct anx7688_chip *chip = dbgfs->chip;
	char *buf;
	unsigned int buf_size;
	long is_on;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, PAGE_SIZE, ppos, ubuf, buf_size);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &is_on);
	if (rc < 0)
		goto err;

	if (is_on)
		anx7688_pwr_on(chip);
	else
		anx7688_pwr_down(chip);

	rc = count;
err:
	kfree(buf);
	return rc;
}

static const struct file_operations anx7688_debugfs_power_ops = {
	.open = anx7688_debugfs_open,
	.read = anx7688_debugfs_power_read,
	.write = anx7688_debugfs_power_write,
	.release = anx7688_debugfs_release,
};

/*
 * anx7688_debugfs_status
 */
static ssize_t anx7688_debugfs_status_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	struct anx7688_chip *chip = dbgfs->chip;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!atomic_read(&chip->power_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto done;
	}

	rc = OhioReadWordReg(USBC_ADDR, USBC_DEVICEID);
	rc <<= 8;
	rc |= OhioReadReg(USBC_ADDR, USBC_DEVVER);
	temp += scnprintf(buf + temp, buf_size - temp, "DEVICE: %X\n", rc);

	rc = OhioReadWordReg(USBC_ADDR, OCM_DEBUG_4);
	temp += scnprintf(buf + temp, buf_size - temp, "FW VER: %02X.%02X\n",
				(rc & 0x00FF) << 8, (rc & 0xFF00) >> 8 );
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = OhioReadReg(USBC_ADDR, USBC_PWRDN_CTRL);
	temp += scnprintf(buf + temp, buf_size - temp,
			"POWER_DOWN_CTRL(%02x)\n", rc);
	if (!(rc & (R_PWRDN_PD | R_PWRDN_OCM)))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tpower up\n");
	else {
		if (rc & R_PWRDN_PD)
			temp += scnprintf(buf + temp, buf_size - temp,
					"\tpower down power delivery logic\n");
		if (rc & R_PWRDN_OCM)
			temp += scnprintf(buf + temp, buf_size - temp,
					"\tpower down power OCM\n");
	}
	if (rc & (CC1_VRD_3P0 | CC2_VRD_3P0))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\t3.0A@5V\n");
	if (rc & (CC1_VRD_1P5 | CC2_VRD_1P5))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\t1.5A@5V\n");
	if (rc & (CC1_VRD_USB | CC2_VRD_USB))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tDefault USB Power\n");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = OhioReadReg(USBC_ADDR, 0x40); // ANALOG_STATUS
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_STATUS(%02x)\n", rc);
	temp += scnprintf(buf + temp, buf_size - temp,
			"\tDFP_OR_UFP: %s\n",
			(rc &  BIT(3)) ? "UFP" : "DFP");
	temp += scnprintf(buf + temp, buf_size - temp,
			"\tUFP_PLUG: %s\n",
			(rc & BIT(4)) ? "plug" : "unplug");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = OhioReadReg(USBC_ADDR, 0x41); // ANALOG_CTRL_0
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_CTRL_0(%02x)\n", rc);
	temp += scnprintf(buf + temp, buf_size - temp,
			"\t%s\n",
			(rc & BIT(7)) ? "cable plug" : "cable unplug");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = OhioReadReg(USBC_ADDR, 0x47); // ANALOG_CTRL_6
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_CTRL_6(%02x)\n", rc);
	switch (rc & 0x03) {
		case 0x00: // 36k
			temp += scnprintf(buf + temp, buf_size - temp,
						"\tRp 36k\n");
			break;
		case 0x01: // 12k
			temp += scnprintf(buf + temp, buf_size - temp,
						"\tRp 12k\n");
			break;
		case 0x10: // 4.7k
			temp += scnprintf(buf + temp, buf_size - temp,
						"\tRp 4.7k\n");
			break;
	}
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = OhioReadReg(USBC_ADDR, 0x48); // ANALOG_CTRL_7
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_CTRL_7(%02x)\n", rc);
	if (rc & BIT(0))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRd 5.1k connected to CC2\n");
	if (rc & BIT(1))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRa connected to CC2\n");
	if (rc & BIT(2))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRd 5.1k connected to CC1\n");
	if (rc & BIT(3))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRa connected to CC1\n");
	if (!(rc & (BIT(0) | BIT(2))))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRd not connected\n");
	if (!(rc & (BIT(1) | BIT(3))))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRa not connected\n");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	temp += scnprintf(buf + temp, buf_size - temp, "Crosspoint Switch\n");
	rc = OhioReadReg(USBC_ADDR, 0x42); // ANALOG_CTRL_1
	if (rc & BIT(0))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml0p --a11, ml0n --a10\n");
	if (rc & BIT(1))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml0p --b11, ml0n --b10\n");
	if (rc & BIT(2))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml3p --a11, ml3n --a10\n");
	if (rc & BIT(3))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml3p --b11, ml3n --b10\n");
	if (rc & BIT(4))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tSSRXp--a11, SSRXn--a10\n");
	if (rc & BIT(5))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tSSRXp--b11, SSRXn--b10\n");
	if (rc & BIT(6))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml1p --b3,  ml1n --b3\n");
	if (rc & BIT(7))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml1p --a2,  ml1n --a3\n");
	rc = OhioReadReg(USBC_ADDR, 0x46); // ANALOG_CTRL_5
	if (rc & BIT(4))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml2p --b2,  ml2n --b3\n");
	if (rc & BIT(5))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tml2p --a2,  ml2n --a3\n");
	if (rc & BIT(6))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tSSTXp--b2,  SSTXn--b3\n");
	if (rc & BIT(7))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tSSTXp--a2,  SSTXn--a3\n");

	rc = OhioReadReg(USBC_ADDR, 0x43); // ANALOG_CTRL_2
	if (rc & BIT(4))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\taux_p connect SBU1\n");
	if (rc & BIT(5))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\taux_n connect SBU2\n");
	if (rc & BIT(6))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\taux_p connect SBU2\n");
	if (rc & BIT(7))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\taux_n connect SBU1\n");

	temp += scnprintf(buf + temp, buf_size - temp, "\n");

done:
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return rc;
}

static const struct file_operations anx7688_debugfs_status_ops = {
	.open = anx7688_debugfs_open,
	.read = anx7688_debugfs_status_read,
	.release = anx7688_debugfs_release,
};

/*
 * anx7688_debugfs_dump
 */
static u8 dump_reg = 0;

static ssize_t anx7688_debugfs_dump_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	struct anx7688_chip *chip = dbgfs->chip;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int i, j = 0;
	int rc = 0;

	if(*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) *buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!atomic_read(&chip->power_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto done;
	}

	if (dump_reg < 1)
	{
		dump_reg = USBC_ADDR;
		temp += scnprintf(buf + temp, buf_size - temp, "  <0x%x>  ", dump_reg);

		for (i = 0; i < 16; i++) {
		temp += scnprintf(buf + temp, buf_size - temp,
					"%02x ", i);
		}
		temp += scnprintf(buf + temp, buf_size - temp, "\n");

		for (i = 0; i < 256; i += 16) {
			temp += scnprintf(buf + temp, buf_size - temp,
					"%.8x: ", i);
			for ( j = 0; j < 16; j++) {
				rc = OhioReadReg(dump_reg, i + j);
				temp += scnprintf(buf + temp,
						buf_size - temp,
						"%02x ", rc);
			}
			temp += scnprintf(buf + temp, buf_size - temp, "\n");
		}

		temp += scnprintf(buf + temp, buf_size - temp, "\n");

		dump_reg = TCPC_ADDR;
	}

	temp += scnprintf(buf + temp, buf_size - temp, "  <0x%x>  ", dump_reg);
	for (i = 0; i < 16; i++) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"%02x ", i);
	}
	temp += scnprintf(buf + temp, buf_size - temp, "\n");
	for (i = 0; i < 256; i += 16) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"%.8x: ", i);
		for ( j = 0; j < 16; j++) {
			rc = OhioReadReg(dump_reg, i + j);
			temp += scnprintf(buf + temp,
					buf_size - temp,
					"%02x ", rc);
		}
		temp += scnprintf(buf + temp, buf_size - temp, "\n");
	}
done:
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return rc;
}

static ssize_t anx7688_debugfs_dump_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long reg = 0;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) *buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, PAGE_SIZE, ppos, ubuf, buf_size);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	reg += hex_to_bin(buf[0]) * 16;
	reg += hex_to_bin(buf[1]);

	dump_reg = reg / 2;
	rc = count;
err:
	kfree(buf);
	return rc;

}

static const struct file_operations anx7688_debugfs_dump_ops = {
	.open = anx7688_debugfs_open,
	.read = anx7688_debugfs_dump_read,
	.write = anx7688_debugfs_dump_write,
	.release = anx7688_debugfs_release,
};

/*
 * anx7688_debugfs_address;
 */
static u8 dbgfs_address;

static ssize_t anx7688_debugfs_addr_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = scnprintf(buf, buf_size, "%02x\n", dbgfs_address);
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t anx7688_debugfs_addr_write(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long reg;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, PAGE_SIZE, ppos, ubuf, buf_size);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &reg);
	if (rc < 0)
		goto err;

	dbgfs_address = reg;
	rc = count;
err:
	kfree(buf);
	return rc;
}

static const struct file_operations anx7688_debugfs_addr_ops = {
	.read = anx7688_debugfs_addr_read,
	.write = anx7688_debugfs_addr_write,
};

/*
 * anx7688_debugfs_reg
 */
static u8 dbgfs_reg;

static ssize_t anx7688_debugfs_reg_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = scnprintf(buf, buf_size, "%02x\n", dbgfs_reg);
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t anx7688_debugfs_reg_write(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long reg;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, PAGE_SIZE, ppos, ubuf, buf_size);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &reg);
	if (rc < 0)
		goto err;

	dbgfs_reg = reg;
	rc = count;
err:
	kfree(buf);
	return rc;
}

static const struct file_operations anx7688_debugfs_reg_ops = {
	.read = anx7688_debugfs_reg_read,
	.write = anx7688_debugfs_reg_write,
};

/*
 * anx7688_debugfs_val
 */
static ssize_t anx7688_debugfs_val_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	int val;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!((dbgfs_address == USBC_ADDR) ||
			(dbgfs_address == TCPC_ADDR) ||
			(dbgfs_address == DP_CORE_ADDR) ||
			(dbgfs_address == DP_HDCP_ADDR) ||
			(dbgfs_address == DP_ANLG_ADDR) ||
			(dbgfs_address == DP_RX1_ADDR) ||
			(dbgfs_address == DP_RX2_ADDR))) {
		pr_err("%s: Error invalid address\n", __func__);
		goto err;
	}

	val = OhioReadReg(dbgfs_address, dbgfs_reg);
	if (val < 0)
		rc = scnprintf(buf, buf_size, "%d\n", val);
	else
		rc = scnprintf(buf, buf_size, "%02x\n", val);

err:
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t anx7688_debugfs_val_write(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	if (!((dbgfs_address == USBC_ADDR) ||
		(dbgfs_address == TCPC_ADDR) ||
		(dbgfs_address == DP_CORE_ADDR) ||
		(dbgfs_address == DP_HDCP_ADDR) ||
		(dbgfs_address == DP_ANLG_ADDR) ||
		(dbgfs_address == DP_RX1_ADDR) ||
		(dbgfs_address == DP_RX2_ADDR))) {
		pr_err("%s: Error invalid address\n", __func__);
		goto err;
	}

	rc = OhioWriteReg(dbgfs_address, dbgfs_reg, val);
	if (rc < 0)
		goto err;

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations anx7688_debugfs_val_ops = {
	.open = anx7688_debugfs_open,
	.read = anx7688_debugfs_val_read,
	.write = anx7688_debugfs_val_write,
	.release = anx7688_debugfs_release,
};

/*
 * anx7688_debugfs_events
 */

/* Maximum debug message length */
#define DBG_DATA_MSG   64UL
/* Maximum number of messages */
#define DBG_DATA_MAX   2048UL

static struct {
	char (buf[DBG_DATA_MAX])[DBG_DATA_MSG]; /* buffer */
	unsigned idx;   /* index */
	unsigned tty;   /* print to console? */
	rwlock_t lck;   /* lock */
} dbg_anx_data = {
	.idx = 0,
	.tty = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

static inline void __maybe_unused dbg_dec(unsigned *idx)
{
	*idx = (*idx - 1) % DBG_DATA_MAX;
}

static inline void dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) % DBG_DATA_MAX;
}

#define TIME_BUF_LEN  20
static char *get_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000)/1000;
	scnprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu] ", (unsigned long)t,
			nanosec_rem);
	return tbuf;
}

void anx_dbg_print(const char *name, int status, const char *extra)
{
	unsigned long flags;
	char tbuf[TIME_BUF_LEN];

	write_lock_irqsave(&dbg_anx_data.lck, flags);

	scnprintf(dbg_anx_data.buf[dbg_anx_data.idx], DBG_DATA_MSG,
			"%s\t? %-12.12s %4i ?\t%s\n",
			get_timestamp(tbuf), name, status, extra);

	dbg_inc(&dbg_anx_data.idx);

	write_unlock_irqrestore(&dbg_anx_data.lck, flags);

	if (dbg_anx_data.tty != 0)
		pr_notice("%s\t? %-7.7s %4i ?\t%s\n",
				get_timestamp(tbuf), name, status, extra);
}

void anx_dbg_event(const char *name, int status)
{
	anx_dbg_print(name, status, "");
}


static ssize_t anx7688_debugfs_events_store(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	unsigned tty;

	if (ubuf == NULL) {
		pr_err("[%s] EINVAL\n", __func__);
		goto done;
	}

	if (sscanf(ubuf, "%u", &tty) != 1 || tty > 1) {
		pr_err("<1|0>: enable|disable console log\n");
		goto done;
	}

	dbg_anx_data.tty = tty;
	pr_info("tty = %u", dbg_anx_data.tty);

done:
	return count;
}

static int anx7688_debugfs_events_show(struct seq_file *s, void *unused)
{
	unsigned long flags;
	unsigned i;

	read_lock_irqsave(&dbg_anx_data.lck, flags);

	i = dbg_anx_data.idx;
	if (strnlen(dbg_anx_data.buf[i], DBG_DATA_MSG))
		seq_printf(s, "%s\n", dbg_anx_data.buf[i]);
	for (dbg_inc(&i); i != dbg_anx_data.idx; dbg_inc(&i)) {
		if (!strnlen(dbg_anx_data.buf[i], DBG_DATA_MSG))
			continue;
		seq_printf(s, "%s\n", dbg_anx_data.buf[i]);
	}

	read_unlock_irqrestore(&dbg_anx_data.lck, flags);

	return 0;
}

static int anx7688_debugfs_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, anx7688_debugfs_events_show, inode->i_private);
}

static const struct file_operations anx7688_debugfs_events_ops = {
	.open = anx7688_debugfs_events_open,
	.read = seq_read,
	.write = anx7688_debugfs_events_store,
	.llseek = seq_lseek,
	.release = single_release,
};

static int anx7688_debugfs_fw_open(struct inode *inode, struct file *file)
{
	struct anx7688_debugfs *dbgfs;
	struct anx7688_chip *chip;
	int rc = 0;

	rc = anx7688_debugfs_open(inode, file);
	if (rc < 0)
		return rc;

	dbgfs = file->private_data;
	chip = dbgfs->chip;

	dbgfs->fw = anx7688_firmware_alloc(chip);
	if (ZERO_OR_NULL_PTR(dbgfs->fw))
		return -ENOMEM;

	disable_irq(chip->cdet_irq);
	disable_irq(chip->alter_irq);
	if (!atomic_read(&chip->power_on))
		anx7688_pwr_on(chip);

	return 0;
}

static int anx7688_debugfs_fw_release(struct inode *inode, struct file *file)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	struct anx7688_chip *chip = dbgfs->chip;

	anx7688_firmware_release(dbgfs->fw);
	kfree(dbgfs->fw);

	anx7688_pwr_down(chip);
	enable_irq(chip->cdet_irq);
	enable_irq(chip->alter_irq);

	return anx7688_debugfs_release(inode, file);
}

static ssize_t anx7688_debugfs_fw_update(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	const u8 *data = dbgfs->fw->entry->data;
	int size = dbgfs->fw->entry->size;
	//struct i2c_client *client = dbgfs->chip->client;
	char *buf;
	unsigned int buf_size;
	u8 reg;
	int output = 0;
	int rc = 0;
	int cmp_val = 0;
	int retry_count = 0;
	ktime_t diff;

	pr_err("%s : start\n", __func__);
	pr_err("%s : offset %d / %d\n", __func__, dbgfs->offset, size);

	if (dbgfs->offset >= size) {
		pr_err("%s : firmware done\n", __func__);
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) *buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (dbgfs->offset == 0) {
		output += scnprintf(buf + output, buf_size - output,
				"firmware update start\n");
		output += scnprintf(buf + output, buf_size - output,
				"chip ver: %02X\n", dbgfs->chip_ver);
		output += scnprintf(buf + output, buf_size - output,
				"fw ver: %02X\n", dbgfs->fw->ver);
		output += scnprintf(buf + output, buf_size - output,
				"fw size: %d\n", size);

		do {
			rc = OhioReadReg(USBC_ADDR, 0xE7);
			if (rc < 0)
				goto done;
		} while ((rc & 0x10) != 0x10);
		msleep(10);

		// reset OCM
		OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x10);

		//EEPROM access password
		//reg = OhioReadReg(USBC_ADDR, 0x3F);
		//reg |= 0x20; //KEY0
		OhioWriteReg(USBC_ADDR, 0x3F, 0x20 );

		//reg = OhioReadReg(USBC_ADDR, 0x44);
		//reg |= 0x81; //KEY1
		OhioWriteReg(USBC_ADDR, 0x44, 0x81 );

		//reg = OhioReadReg(USBC_ADDR, 0x66);
		//reg |= 0x08; //KEY3
		OhioWriteReg(USBC_ADDR, 0x66, 0x08 );

		dbgfs->start_time = ktime_get();

		dbgfs->offset += FW_LENGTH;
	}
retry:
	pr_err("%s : write start\n", __func__);
	rc = anx7688_firmware_write(dbgfs->fw, dbgfs->offset, (data + dbgfs->offset), FW_LENGTH);
	if (rc < 0) {
		goto done;
	}

	cmp_val = anx7688_firmware_verify(dbgfs->fw, dbgfs->offset, (data + dbgfs->offset), FW_LENGTH);
	if (cmp_val != 0) {
		retry_count++;
		if (retry_count > 3) {
			dbgfs->retry_count += retry_count;
			output += scnprintf(buf + output, buf_size - output, "X");
		} else {
			output += scnprintf(buf + output, buf_size - output, "R");
			goto retry;
		}
	} else
		output += scnprintf(buf + output, buf_size - output, "O");

	dbgfs->offset += FW_LENGTH;
	if ((size - dbgfs->offset) < FW_LENGTH)
		dbgfs->offset = size;

	if (dbgfs->offset >= size)
		dbgfs->fw->update_done = true;

	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, output);
	kfree(buf);

done:

	if (dbgfs->offset >= size) {
		//EEPROM close password
		buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
		buf = kmalloc(sizeof(char) *buf_size, GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(buf)) {
			pr_err("%s: Error allocating memory\n", __func__);
			return -ENOMEM;
		}

		reg = OhioReadReg(USBC_ADDR, 0x3F);
		reg &= 0xDF; //KEY0
		OhioWriteReg(USBC_ADDR, 0x3F, reg );

		reg = OhioReadReg(USBC_ADDR, 0x44);
		reg &= 0x7E; //KEY1
		OhioWriteReg(USBC_ADDR, 0x44, reg );

		reg = OhioReadReg(USBC_ADDR, 0x66);
		reg &= 0xE7; //KEY3
		OhioWriteReg(USBC_ADDR, 0x66, reg );

		diff = ktime_sub(ktime_get(), dbgfs->start_time);
		output += scnprintf(buf + output, buf_size - output,
				"\nupdate time : %lldms\n", ktime_to_ms(diff));
		rc = simple_read_from_buffer(ubuf, count, ppos, buf, output);
		kfree(buf);

		anx7688_firmware_release(dbgfs->fw);
	}
	return rc;
}

static const struct file_operations anx7688_debugfs_fw_update_ops = {
	.open = anx7688_debugfs_fw_open,
	//.read = anx7688_debugfs_fw_verify,
	.read = anx7688_debugfs_fw_update,
	.release = anx7688_debugfs_fw_release,
};

static ssize_t anx7688_debugfs_fw_verify(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	const u8 *data = dbgfs->fw->entry->data;
	int size = dbgfs->fw->entry->size;
	//struct i2c_client *client = dbgfs->chip->client;
	char *buf;
	unsigned int buf_size;
	int output = 0;
	int rc = 0;
	ktime_t diff;

	pr_err("%s : offset %d / %d\n", __func__, dbgfs->offset, size);

	if (dbgfs->offset >= size) {
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) *buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (dbgfs->offset == 0) {
		output += scnprintf(buf + output, buf_size - output,
				"firmware verify start\n");
		output += scnprintf(buf + output, buf_size - output,
				"chip ver: %02X\n", dbgfs->chip_ver);
		output += scnprintf(buf + output, buf_size - output,
				"fw ver: %02X\n", dbgfs->fw->ver);

		do {
			rc = OhioReadReg(USBC_ADDR, 0xE7);
			if (rc < 0) {
				output += scnprintf(buf + output, buf_size - output, "verify ready fail : %d\n", rc);
				goto done;
			}
		} while ((rc & 0x10) != 0x10);

		// reset OCM
		OhioWriteReg(USBC_ADDR, USBC_RESET_CTRL_0, 0x10);

		dbgfs->start_time = ktime_get();

		dbgfs->offset += FW_LENGTH;
	}
	rc = anx7688_firmware_verify(dbgfs->fw, dbgfs->offset, (data + dbgfs->offset), FW_LENGTH);
	if (rc == 0)
		output += scnprintf(buf + output, buf_size - output, "O");
	else
		output += scnprintf(buf + output, buf_size - output, "X");
	dbgfs->offset += FW_LENGTH;
	if ((size - dbgfs->offset) < FW_LENGTH)
		dbgfs->offset = size;

	if (dbgfs->offset >= size)
		dbgfs->fw->update_done = true;

	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, output);
done:
	if (dbgfs->offset >= size) {
		diff = ktime_sub(ktime_get(), dbgfs->start_time);
		output += scnprintf(buf + output, buf_size - output,
				"%lld ms\n", ktime_to_ms(diff));
		anx7688_firmware_release(dbgfs->fw);
	}
	return rc;
}

static const struct file_operations anx7688_debugfs_fw_verify_ops = {
	.open = anx7688_debugfs_fw_open,
	.read = anx7688_debugfs_fw_verify,
	.release = anx7688_debugfs_fw_release,
};

static ssize_t anx7688_debugfs_fw_dump_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7688_debugfs *dbgfs = file->private_data;
	const u8 *data = dbgfs->fw->entry->data;
	int size = dbgfs->fw->entry->size;
	char *buf;
	char *temp;
	unsigned int buf_size;
	int output = 0;
	int rc = 0;

	pr_err("%s : offset %d / %d\n", __func__, dbgfs->offset, size);

	if (dbgfs->offset >= size) {
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) *buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	temp = kmalloc(sizeof(char)*buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(temp)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}
	if(data!=NULL){
	hex_dump_to_buffer((data + dbgfs->offset), size, FW_LENGTH, 1, temp, buf_size, true);
	output += scnprintf(buf+output, buf_size-output, "%s\n", temp);
	dbgfs->offset += FW_LENGTH;
	}
	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, output);

done:
	return rc;
}

static const struct file_operations anx7688_debugfs_fw_dump_ops = {
	.open = anx7688_debugfs_fw_open,
	.read = anx7688_debugfs_fw_dump_read,
	.release = anx7688_debugfs_fw_release,
};

static struct dentry *anx7688_debugfs_dent;

int anx7688_debugfs_init(struct anx7688_chip *chip)
{
	struct dentry *entry = NULL;

	anx7688_debugfs_dent = debugfs_create_dir("anx7688", 0);
	if (IS_ERR(anx7688_debugfs_dent))
		return -ENODEV;

	entry = debugfs_create_file("power", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_power_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("status", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_status_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("dump", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_dump_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("addr", 0600, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_addr_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("reg", 0600, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_reg_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("val", 0600, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_val_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("events", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_events_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("fw", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_fw_update_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("verify", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_fw_verify_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	entry = debugfs_create_file("fw_dump", 0444, anx7688_debugfs_dent,
			chip, &anx7688_debugfs_fw_dump_ops);
	if (!entry) {
		debugfs_remove_recursive(anx7688_debugfs_dent);
		return -ENODEV;
	}

	return 0;
}

void anx7688_debugfs_cleanup(void)
{
	debugfs_remove_recursive(anx7688_debugfs_dent);
}
