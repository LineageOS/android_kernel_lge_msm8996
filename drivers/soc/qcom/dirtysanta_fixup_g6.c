/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * G6/Lucye DirtySanta-style modem fixup.
 */

#include <linux/err.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <soc/qcom/smem.h>

#define LG_G6_MODEL_NAME_SIZE	22
#define LG_G6_NT_CODE_SIZE	2048

struct lge_g6_smem_vendor0 {
	int hw_rev;
	char model_name[10];
	char reserved0[64];
	char nt_code[LG_G6_NT_CODE_SIZE];
	char lg_model_name[LG_G6_MODEL_NAME_SIZE];
	int sim_num;
	int svn_val;
	int flag_gpio;
};

static char g6_dev_name[LG_G6_MODEL_NAME_SIZE] __initdata =
	CONFIG_DIRTYSANTA_FIXUP_G6_DEVICENAME;
static char g6_nt_code[LG_G6_NT_CODE_SIZE] __initdata =
	CONFIG_DIRTYSANTA_FIXUP_G6_NTCODE;
static int g6_sim_num __initdata = CONFIG_DIRTYSANTA_FIXUP_G6_SIMCOUNT;

static void dirtysanta_g6_write_smem_string(char *dst, const char *src,
					    unsigned int size)
{
	unsigned int i;

	if (!size)
		return;

	for (i = 0; i < size - 1 && src[i]; i++)
		WRITE_ONCE(dst[i], src[i]);

	for (; i < size; i++)
		WRITE_ONCE(dst[i], '\0');
}

static int __init dirtysanta_g6_copy_cmdline_value(const char *key,
						   char *dst,
						   unsigned int size)
{
	char *match;
	unsigned int len;

	match = strnstr(saved_command_line, key, strlen(saved_command_line));
	if (!match)
		return -ENOENT;

	match += strlen(key);
	len = strchrnul(match, ' ') - match;
	if (len >= size) {
		pr_warn("DirtySanta G6: \"%s\" is too long, truncating\n",
			key);
		len = size - 1;
	}

	memcpy(dst, match, len);
	dst[len] = '\0';

	return 0;
}

static int __init dirtysanta_g6_load_config(void)
{
	const char model_key[] = "model.name=";
	const char sim_key[] = "lge.sim_num=";
	char *match;

	if (!g6_dev_name[0])
		dirtysanta_g6_copy_cmdline_value(model_key, g6_dev_name,
						 sizeof(g6_dev_name));

	if (!g6_sim_num) {
		match = strnstr(saved_command_line, sim_key,
				strlen(saved_command_line));
		if (match)
			g6_sim_num = match[strlen(sim_key)] - '0';
	}

	if (!g6_dev_name[0]) {
		pr_err("DirtySanta G6: missing VENDOR0 lg_model_name\n");
		return -EINVAL;
	}

	if (g6_sim_num <= 0) {
		pr_err("DirtySanta G6: missing VENDOR0 sim_num\n");
		return -EINVAL;
	}

	if (g6_sim_num > 2)
		pr_warn("DirtySanta G6: unusual SIM count %d\n", g6_sim_num);

	return 0;
}

static int __init dirtysanta_g6_fixup_msm_modem(void)
{
	struct lge_g6_smem_vendor0 *vendor0;
	unsigned int size;
	int ret;

	BUILD_BUG_ON(offsetof(struct lge_g6_smem_vendor0, nt_code) != 78);
	BUILD_BUG_ON(offsetof(struct lge_g6_smem_vendor0, lg_model_name) !=
		     2126);
	BUILD_BUG_ON(offsetof(struct lge_g6_smem_vendor0, sim_num) != 2148);
	BUILD_BUG_ON(offsetof(struct lge_g6_smem_vendor0, flag_gpio) != 2156);

	vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &size, 0,
				 SMEM_ANY_HOST_FLAG);
	if (IS_ERR_OR_NULL(vendor0)) {
		pr_info("DirtySanta G6: Qualcomm smem not initialized\n");
		return -EFAULT;
	}

	if (size < sizeof(*vendor0)) {
		pr_err("DirtySanta G6: VENDOR0 smem area is too small\n");
		return -ENOMEM;
	}

	ret = dirtysanta_g6_load_config();
	if (ret)
		return ret;

	pr_info("DirtySanta G6: applying VENDOR0 modem fixup\n");

	dirtysanta_g6_write_smem_string(vendor0->lg_model_name, g6_dev_name,
					sizeof(vendor0->lg_model_name));
	vendor0->sim_num = g6_sim_num;

	if (g6_nt_code[0])
		dirtysanta_g6_write_smem_string(vendor0->nt_code, g6_nt_code,
						sizeof(vendor0->nt_code));

	return 0;
}

/*
 * command-line is loaded at core_initcall(); smem is ready at arch_initcall()
 */
subsys_initcall(dirtysanta_g6_fixup_msm_modem);
