#ifdef CONFIG_DEBUG_FS

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include "anx7418_debugfs.h"
#include "anx7418_firmware.h"

/*
 * anx7418_dbgfs common open/release
 */
#define DEBUG_BUF_SIZE 4096
struct anx7418_dbgfs {
	struct anx7418 *anx;
	void *private_data;
};

static int anx7418_dbgfs_open(struct inode *inode, struct file *file)
{
	struct anx7418_dbgfs *dbgfs;

	dbgfs = kzalloc(sizeof(struct anx7418_dbgfs), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(dbgfs)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	dbgfs->anx = inode->i_private;
	file->private_data = dbgfs;
	return 0;
}

static int anx7418_dbgfs_release(struct inode *inode, struct file *file)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	kfree(dbgfs);
	return 0;
}

/*
 * anx7418_dbgfs_power
 */
static ssize_t anx7418_dbgfs_power_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
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

	rc = scnprintf(buf, buf_size, "%d\n", atomic_read(&anx->pwr_on));
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t anx7418_dbgfs_power_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
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

	anx7418_pwr_on(anx, is_on);
	rc = count;
err:
	kfree(buf);
	return rc;
}

static const struct file_operations anx7418_dbgfs_power_ops = {
	.open = anx7418_dbgfs_open,
	.read = anx7418_dbgfs_power_read,
	.write = anx7418_dbgfs_power_write,
	.release = anx7418_dbgfs_release,
};

/*
 * anx7418_dbgfs_status
 */
static ssize_t anx7418_dbgfs_status_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
	struct i2c_client *client = anx->client;
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

	if (!atomic_read(&anx->pwr_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto done;
	}

	rc = anx7418_read_reg(client, DEVICE_ID_H);
	rc <<= 8;
	rc |= anx7418_read_reg(client, DEVICE_ID_L);
	rc <<= 8;
	rc |= anx7418_read_reg(client, DEVICE_VERSION);
	temp += scnprintf(buf + temp, buf_size - temp, "DEVICE: %X\n", rc);

	rc = anx7418_read_reg(client, ANALOG_CTRL_3);
	temp += scnprintf(buf + temp, buf_size - temp, "FW VER: %02X\n", rc);
	temp += scnprintf(buf + temp, buf_size - temp, "ROM TYPE: ");
	if (anx->otp)
		temp += scnprintf(buf + temp, buf_size - temp, "OTP\n");
	else
		temp += scnprintf(buf + temp, buf_size - temp, "EEPROM\n");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = anx7418_read_reg(client, POWER_DOWN_CTRL);
	temp += scnprintf(buf + temp, buf_size - temp,
			"POWER_DOWN_CTRL(%02x)\n", rc);
	if (!(rc & (R_POWER_DOWN_PD | R_POWER_DOWN_OCM)))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tpower up\n");
	else {
		if (rc & R_POWER_DOWN_PD)
			temp += scnprintf(buf + temp, buf_size - temp,
					"\tpower down power delivery logic\n");
		if (rc & R_POWER_DOWN_OCM)
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

	rc = anx7418_read_reg(client, ANALOG_STATUS);
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_STATUS(%02x)\n", rc);
	temp += scnprintf(buf + temp, buf_size - temp,
			"\tDFP_OR_UFP: %s\n",
			(rc & DFP_OR_UFP) ? "UFP" : "DFP");
	temp += scnprintf(buf + temp, buf_size - temp,
			"\tUFP_PLUG: %s\n",
			(rc & UFP_PLUG) ? "plug" : "unplug");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = anx7418_read_reg(client, ANALOG_CTRL_0);
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_CTRL_0(%02x)\n", rc);
	temp += scnprintf(buf + temp, buf_size - temp,
			"\t%s\n",
			(rc & R_PIN_CABLE_DET) ? "cable plug" : "cable unplug");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = anx7418_read_reg(client, ANALOG_CTRL_6);
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_CTRL_6(%02x)\n", rc);
	switch (rc & R_RP) {
	case 0x00: // 36k
		temp += scnprintf(buf + temp, buf_size - temp, "\tRp 36k\n");
		break;
	case 0x01: // 12k
		temp += scnprintf(buf + temp, buf_size - temp, "\tRp 12k\n");
		break;
	case 0x10: // 4.7k
		temp += scnprintf(buf + temp, buf_size - temp, "\tRp 4.7k\n");
		break;
	}
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	rc = anx7418_read_reg(client, ANALOG_CTRL_7);
	temp += scnprintf(buf + temp, buf_size - temp,
			"ANALOG_CTRL_7(%02x)\n", rc);
	if (rc & CC2_5P1K)
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRd 5.1k connected to CC2\n");
	if (rc & CC2_RA)
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRa connected to CC2\n");
	if (rc & CC1_5P1K)
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRd 5.1k connected to CC1\n");
	if (rc & CC1_RA)
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRa connected to CC1\n");
	if (!(rc & (CC2_5P1K | CC1_5P1K)))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRd not connected\n");
	if (!(rc & (CC2_RA | CC1_RA)))
		temp += scnprintf(buf + temp, buf_size - temp,
				"\tRa not connected\n");
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	temp += scnprintf(buf + temp, buf_size - temp, "Crosspoint Switch\n");
	rc = anx7418_read_reg(client, ANALOG_CTRL_1);
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
	rc = anx7418_read_reg(client, ANALOG_CTRL_5);
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

	rc = anx7418_read_reg(client, ANALOG_CTRL_2);
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

static const struct file_operations anx7418_dbgfs_status_ops = {
	.open = anx7418_dbgfs_open,
	.read = anx7418_dbgfs_status_read,
	.release = anx7418_dbgfs_release,
};

/*
 * anx7418_dbgfs_dump
 */
static ssize_t anx7418_dbgfs_dump_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int i, j;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!atomic_read(&anx->pwr_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto done;
	}

	temp += scnprintf(buf + temp, buf_size - temp, "          ");
	for (i = 0; i < 16; i++) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"%02x ", i);
	}
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	for (i = 0; i < 256; i += 16) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"%.8x: ", i);
		for (j = 0; j < 16; j++) {
			rc = anx7418_read_reg(anx->client, i + j);
#if 0
			if (rc < 0) {
				temp += scnprintf(buf + temp,
						buf_size - temp,
						"err %d", rc);
				goto err;
			}
#endif
			temp += scnprintf(buf + temp,
					buf_size - temp,
					"%02x ", rc);
		}
		temp += scnprintf(buf + temp, buf_size - temp, "\n");
	}
done:
#if 0
err:
#endif
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return rc;
}

static const struct file_operations anx7418_dbgfs_dump_ops = {
	.open = anx7418_dbgfs_open,
	.read = anx7418_dbgfs_dump_read,
	.release = anx7418_dbgfs_release,
};

/*
 * anx7418_dbgfs_reg
 */
static u8 dbgfs_reg;

static ssize_t anx7418_dbgfs_reg_read(struct file *file, char __user *ubuf,
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

static ssize_t anx7418_dbgfs_reg_write(struct file *file, const char __user *ubuf,
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

static const struct file_operations anx7418_dbgfs_reg_ops = {
	.read = anx7418_dbgfs_reg_read,
	.write = anx7418_dbgfs_reg_write,
};

/*
 * anx7418_dbgfs_val
 */
static ssize_t anx7418_dbgfs_val_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
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

	val = anx7418_read_reg(anx->client, dbgfs_reg);
	if (val < 0)
		rc = scnprintf(buf, buf_size, "%d\n", val);
	else
		rc = scnprintf(buf, buf_size, "%02x\n", val);

	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t anx7418_dbgfs_val_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
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

	rc = anx7418_write_reg(anx->client, dbgfs_reg, val);
	if (rc < 0)
		goto err;

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations anx7418_dbgfs_val_ops = {
	.open = anx7418_dbgfs_open,
	.read = anx7418_dbgfs_val_read,
	.write = anx7418_dbgfs_val_write,
	.release = anx7418_dbgfs_release,
};

/*
 * anx7418_dbgfs_fw common open/release
 */
struct dbgfs_fw {
	struct anx7418_firmware *fw;
	loff_t offset;
	ktime_t start_time;
	int rom_ver;
};

static int anx7418_dbgfs_fw_open(struct inode *inode, struct file *file)
{
	struct anx7418_dbgfs *dbgfs;
	struct anx7418 *anx;
	struct dbgfs_fw *d_fw;
	int rc = 0;

	rc = anx7418_dbgfs_open(inode, file);
	if (rc < 0)
		return rc;

	dbgfs = file->private_data;
	anx = dbgfs->anx;

	d_fw = kzalloc(sizeof(struct dbgfs_fw), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(d_fw)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}
	dbgfs->private_data = d_fw;

	d_fw->fw = anx7418_firmware_alloc(anx);
	if (ZERO_OR_NULL_PTR(d_fw->fw))
		return -ENOMEM;

	disable_irq(anx->cable_det_irq);
	disable_irq(anx->client->irq);
	if (!atomic_read(&anx->pwr_on))
		anx7418_pwr_on(anx, 1);

	rc = anx7418_firmware_get_old_ver(d_fw->fw);
	if (rc < 0)
		goto err;

	d_fw->rom_ver = rc;

	return 0;
err:
	kfree(d_fw);

	anx7418_pwr_on(anx, 0);
	enable_irq(anx->cable_det_irq);
	enable_irq(anx->client->irq);
	if (gpio_get_value(anx->cable_det_gpio))
		queue_work_on(0, anx->wq, &anx->cable_det_work);

	return rc;
}

static int anx7418_dbgfs_fw_release(struct inode *inode, struct file *file)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct anx7418 *anx = dbgfs->anx;
	struct dbgfs_fw *d_fw = dbgfs->private_data;

	anx7418_firmware_free(d_fw->fw);
	kfree(d_fw);

	anx7418_pwr_on(anx, 0);
	enable_irq(anx->cable_det_irq);
	enable_irq(anx->client->irq);
	if (gpio_get_value(anx->cable_det_gpio))
		queue_work_on(0, anx->wq, &anx->cable_det_work);

	return anx7418_dbgfs_release(inode, file);;
}

/*
 * anx7418_dbgfs_fw_update
 */
static ssize_t anx7418_dbgfs_fw_update(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct dbgfs_fw *d_fw = dbgfs->private_data;
	struct anx7418_firmware *fw = d_fw->fw;
	const u8 *data = fw->entry->data;
	int size = fw->entry->size;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int word = fw->otp ? 9 : 8;
	int rc = 0;

	if (d_fw->offset >= size) {
		rc = 0;
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (d_fw->offset == 0) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"rom ver: %02X\n", d_fw->rom_ver);
		temp += scnprintf(buf + temp, buf_size - temp,
				"fw ver : %02X\n", fw->ver);

		rc = anx7418_firmware_open(fw);
		if (rc < 0) {
			if (rc == -ENOSPC)
				temp += scnprintf(buf + temp, buf_size - temp,
						"no space for update\n");
			temp += scnprintf(buf + temp, buf_size - temp,
					"err: %d\n", rc);
			d_fw->offset = size;
			goto err_open;
		}

		if (fw->otp)
			d_fw->offset += 9 * 3;

		d_fw->start_time = ktime_get();
	}

	rc = anx7418_firmware_write(fw, d_fw->offset, (data + d_fw->offset), word);
	if (rc < 0) {
		temp += scnprintf(buf + temp, buf_size - temp, "err: %d\n", rc);
		d_fw->offset = size;
		goto err;
	}
	temp += scnprintf(buf + temp, buf_size - temp, ".");

	d_fw->offset += rc;
	if ((size - d_fw->offset) < word)
		d_fw->offset = size;

	if (d_fw->offset >= size)
		fw->update_done = true;

err:
	if (d_fw->offset >= size) {
		ktime_t diff = ktime_sub(ktime_get(), d_fw->start_time);
		temp += scnprintf(buf + temp, buf_size - temp,
				"%lld ms\n", ktime_to_ms(diff));

		anx7418_firmware_release(fw);
	}

err_open:
	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
done:
	return rc;
}

static const struct file_operations anx7418_dbgfs_fw_update_ops = {
	.open = anx7418_dbgfs_fw_open,
	.read = anx7418_dbgfs_fw_update,
	.release = anx7418_dbgfs_fw_release,
};

/*
 * anx7418_dbgfs_fw_verify
 */
static ssize_t anx7418_dbgfs_fw_verify(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct dbgfs_fw *d_fw = dbgfs->private_data;
	struct anx7418_firmware *fw = d_fw->fw;
	const u8 *data = fw->entry->data;
	int size = fw->entry->size;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	char tmp[9];
	int word = fw->otp ? 9 : 8;
	int i;
	int rc = 0;

	if (d_fw->offset >= size) {
		rc = 0;
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (d_fw->offset == 0) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"rom ver: %02X\n", d_fw->rom_ver);
		temp += scnprintf(buf + temp, buf_size - temp,
				"fw ver : %02X\n", fw->ver);

		rc = anx7418_firmware_open(fw);
		if (rc < 0 && rc != -ENOSPC) {
			temp += scnprintf(buf + temp, buf_size - temp,
					"err: %d\n", rc);
			d_fw->offset = size;
			goto err_open;
		}

		if (fw->otp)
			d_fw->offset += 9 * 3;

		d_fw->start_time = ktime_get();
	}

	temp += scnprintf(buf + temp, buf_size - temp, "%.8llx: ",
			fw->otp ? d_fw->offset / word : d_fw->offset);

	rc = anx7418_firmware_read(fw, d_fw->offset, tmp, word);
	if (rc < 0) {
		temp += scnprintf(buf + temp, buf_size - temp, "err: %d\n", rc);
		d_fw->offset = size;
		goto err;
	}

	for (i = 0; i < word; i++)
		temp += scnprintf(buf + temp, buf_size - temp, "%02x ", tmp[i]);
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	if (memcmp(tmp, (data + d_fw->offset), word)) {
		temp += scnprintf(buf + temp, buf_size - temp, "%-8s: ", "fw");
		for (i = 0; i < word; i++)
			temp += scnprintf(buf + temp, buf_size - temp, "%02x ",
					data[d_fw->offset + i]);
		temp += scnprintf(buf + temp, buf_size - temp, "\nNG\n");
		d_fw->offset = size;
		goto err;
	}

	d_fw->offset += rc;
	if ((size - d_fw->offset) < word)
		d_fw->offset = size;

err:
	if (d_fw->offset >= size) {
		ktime_t diff = ktime_sub(ktime_get(), d_fw->start_time);
		temp += scnprintf(buf + temp, buf_size - temp,
				"%lld ms\n", ktime_to_ms(diff));

		anx7418_firmware_release(fw);
	}

err_open:
	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
done:
	return rc;
}

static const struct file_operations anx7418_dbgfs_fw_verify_ops = {
	.open = anx7418_dbgfs_fw_open,
	.read = anx7418_dbgfs_fw_verify,
	.release = anx7418_dbgfs_fw_release,
};

/*
 * anx7418_dbgfs_fw_profile
 */
static ssize_t anx7418_dbgfs_fw_profile(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct dbgfs_fw *d_fw = dbgfs->private_data;
	struct anx7418_firmware *fw = d_fw->fw;
	const u8 *data = fw->entry->data;
	int size = fw->entry->size;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int word = fw->otp ? 9 : 8;
	int rc = 0;

	if (d_fw->offset >= size) {
		rc = 0;
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (d_fw->offset == 0) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"rom ver: %02X\n", d_fw->rom_ver);
		temp += scnprintf(buf + temp, buf_size - temp,
				"fw ver : %02X\n", fw->ver);

		if (d_fw->rom_ver != fw->ver) {
			temp += scnprintf(buf + temp, buf_size - temp,
					"version not match\n");
			d_fw->offset = size;
			goto err_ver;
		}

		rc = anx7418_firmware_open(fw);
		if (rc < 0 && rc != -ENOSPC) {
			temp += scnprintf(buf + temp, buf_size - temp,
					"err: %d\n", rc);
			d_fw->offset = size;
			goto err_open;
		}

		if (fw->otp) {
			d_fw->offset += 9 * 3;
			fw->otp_new_addr = fw->otp_old_addr;
		}

		d_fw->start_time = ktime_get();
	}

	while (d_fw->offset < size) {
		rc = anx7418_firmware_write(fw, d_fw->offset,
				(data + d_fw->offset), word);
		if (rc < 0) {
			temp += scnprintf(buf + temp, buf_size - temp,
					"err: %d\n", rc);
			d_fw->offset = size;
			goto err;
		}

		d_fw->offset += rc;
		if ((size - d_fw->offset) < word)
			d_fw->offset = size;
	}

err:
	if (d_fw->offset >= size) {
		ktime_t diff = ktime_sub(ktime_get(), d_fw->start_time);
		temp += scnprintf(buf + temp, buf_size - temp,
				"%lld ms\n", ktime_to_ms(diff));

		anx7418_firmware_release(fw);
	}

err_ver:
err_open:
	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
done:
	return rc;
}

static const struct file_operations anx7418_dbgfs_fw_profile_ops = {
	.open = anx7418_dbgfs_fw_open,
	.read = anx7418_dbgfs_fw_profile,
	.release = anx7418_dbgfs_fw_release,
};

/*
 * anx7418_dbgfs_rom_dump
 */
static ssize_t anx7418_dbgfs_rom_dump(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct anx7418_dbgfs *dbgfs = file->private_data;
	struct dbgfs_fw *d_fw = dbgfs->private_data;
	struct anx7418_firmware *fw = d_fw->fw;
	int size = fw->entry->size;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	char tmp[9];
	int word = fw->otp ? 9 : 8;
	int i;
	int rc = 0;

	if (fw->otp) {
		size = 100 * 8; // 100 bytes
		fw->otp_old_addr = 0;
		fw->otp_old_size = 0x20000;
	}

	if (d_fw->offset >= size) {
		rc = 0;
		goto done;
	}

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (d_fw->offset == 0) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"rom ver: %02X\n", d_fw->rom_ver);

		rc = anx7418_firmware_open(fw);
		if (rc < 0 && rc != -ENOSPC) {
			temp += scnprintf(buf + temp, buf_size - temp,
					"err: %d\n", rc);
			d_fw->offset = size;
			goto err_open;
		}

		d_fw->start_time = ktime_get();
	}

	temp += scnprintf(buf + temp, buf_size - temp, "%.8llx: ",
			fw->otp ? d_fw->offset / 9 : d_fw->offset);

	rc = anx7418_firmware_read(fw, d_fw->offset, tmp, word);
	if (rc < 0) {
		temp += scnprintf(buf + temp, buf_size - temp, "err: %d\n", rc);
		d_fw->offset = size;
		goto err;
	}
	for (i = 0; i < word; i++) {
		temp += scnprintf(buf + temp, buf_size - temp, "%02x, ", tmp[i]);
	}
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	d_fw->offset += rc;

err:
	if (d_fw->offset >= size) {
		ktime_t diff = ktime_sub(ktime_get(), d_fw->start_time);
		temp += scnprintf(buf + temp, buf_size - temp,
				"%lld ms\n", ktime_to_ms(diff));

		anx7418_firmware_release(fw);
	}

err_open:
	*ppos = 0;
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
done:
	return rc;
}

static const struct file_operations anx7418_dbgfs_rom_dump_ops = {
	.open = anx7418_dbgfs_fw_open,
	.read = anx7418_dbgfs_rom_dump,
	.release = anx7418_dbgfs_fw_release,
};

/*
 * anx7418_dbgfs_events
 */

/* Maximum debug message length */
#define DBG_DATA_MSG   64UL
/* Maximum number of messages */
#define DBG_DATA_MAX   2048UL

static struct {
	char (buf[DBG_DATA_MAX])[DBG_DATA_MSG];	/* buffer */
	unsigned idx;	/* index */
	unsigned tty;	/* print to console? */
	rwlock_t lck;	/* lock */
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

static ssize_t anx7418_dbgfs_events_store(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
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

static int anx7418_dbgfs_events_show(struct seq_file *s, void *unused)
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

static int anx7418_dbgfs_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, anx7418_dbgfs_events_show, inode->i_private);
}

static const struct file_operations anx7418_dbgfs_events_ops = {
	.open = anx7418_dbgfs_events_open,
	.read = seq_read,
	.write = anx7418_dbgfs_events_store,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * anx7418_debugfs_init
 */
static struct dentry *anx7418_dbgfs_dent;

int anx7418_debugfs_init(struct anx7418 *anx)
{
	struct dentry *entry = NULL;

	anx7418_dbgfs_dent = debugfs_create_dir("anx7418", 0);
	if (IS_ERR(anx7418_dbgfs_dent))
		return -ENOMEM;

	entry = debugfs_create_file("power", 0600, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_power_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("status", 0444, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_status_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dump", 0444, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_dump_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("reg", 0600, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_reg_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("val", 0600, anx7418_dbgfs_dent,
		       	anx, &anx7418_dbgfs_val_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("fw_update", 0400, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_fw_update_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("fw_verify", 0400, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_fw_verify_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("fw_profile", 0400, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_fw_profile_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("rom_dump", 0400, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_rom_dump_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("events", 0444, anx7418_dbgfs_dent,
			anx, &anx7418_dbgfs_events_ops);
	if (!entry)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(anx7418_dbgfs_dent);
	return -ENOMEM;
}
#else
int anx7418_debugfs_init(struct anx7418 *anx) { return 0; }
#endif
