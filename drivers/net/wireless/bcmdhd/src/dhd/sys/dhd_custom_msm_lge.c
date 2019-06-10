/*
 * Linux DHD Bus Module for PCIE
 *
 * Copyright (C) 1999-2016, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: dhd_pcie_linux.c 599277 2015-11-13 05:41:14Z $
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <asm/io.h>
#ifdef CONFIG_WIFI_CONTROL_FUNC
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#endif /* CONFIG_WIFI_CONTROL_FUNC */
#ifdef CONFIG_BCMDHD_PCIE
#include <linux/pci.h>
#include <linux/msm_pcie.h>
#endif /* CONFIG_BCMDHD_PCIE */

#include <linux/pm_qos.h>

/* This header file need to be managed by LGE */
//#include CONFIG_BCMDHD_OEM_HEADER_PATH

/* use async_schedule to reduce boot-up time */
/* #define ASYNC_INIT */
#ifdef ASYNC_INIT
#include <linux/async.h>
#endif /* ASYNC_INIT */
/* should be updated from dts */
static int gpio_wlan_power = 0;
#ifdef BCMPCIE_OOB_HOST_WAKE
static int gpio_wlan_hostwake = 0;
#endif /* BCMPCIE_OOB_HOST_WAKE */

static int pwr_up_on_boot_time = 0;
static struct pinctrl *wifi_reg_on_pinctrl = NULL;

//#define LGE_BCM_WIFI_DMA_QOS_CONTROL

/*
	Memory allocation is done at dhd_attach
	so static allocation is only necessary in module type driver
*/
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern int dhd_init_wlan_mem(void);
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */


#ifdef LGE_BCM_WIFI_DMA_QOS_CONTROL
/* wifi_dma_state: 0-INATIVE, 1-INIT, 2-IDLE, 3-ACTIVE */
static int wifi_dma_state;
static struct pm_qos_request wifi_dma_qos;
static struct delayed_work req_dma_work;
static uint32_t packet_transfer_cnt = 0;

static void bcm_wifi_req_dma_work(struct work_struct * work)
{
	switch (wifi_dma_state) {
		case 2: /* IDLE State */
			if (packet_transfer_cnt < 100) {
				/* IDLE -> INIT */
				wifi_dma_state = 1;
			}
			else {
				/* IDLE -> ACTIVE */
				wifi_dma_state = 3;
				pm_qos_update_request(&wifi_dma_qos, 7);
				schedule_delayed_work(&req_dma_work, msecs_to_jiffies(50));
			}
			break;

		case 3: /* ACTIVE State */
			if (packet_transfer_cnt < 10) {
				/* ACTIVE -> IDLE */
				wifi_dma_state = 2;
				pm_qos_update_request(&wifi_dma_qos, PM_QOS_DEFAULT_VALUE);
				schedule_delayed_work(&req_dma_work, msecs_to_jiffies(1000));
			}
			else {
				/* Keep ACTIVE */
				schedule_delayed_work(&req_dma_work, msecs_to_jiffies(50));
			}
			break;

		default:
			break;
	}

	packet_transfer_cnt = 0;
}

void bcm_wifi_req_dma_qos(int vote)
{
	if (vote) {
		packet_transfer_cnt++;
	}

	/* INIT -> IDLE */
	if (wifi_dma_state == 1 && vote) {
		wifi_dma_state = 2; /* IDLE */
		schedule_delayed_work(&req_dma_work, msecs_to_jiffies(1000));
	}
}
#endif /* LGE_BCM_WIFI_DMA_QOS_CONTROL */

int bcm_wifi_set_power(int enable)
{
	int ret = 0;

	if (enable) {
		ret = gpio_direction_output(gpio_wlan_power, 1);
		printk("[BRCM]: gpio_direction_output(%d,1)! ret = %d\n", gpio_wlan_power, ret);
		if (ret) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			ret = -EIO;
			goto out;
		}

		/* WLAN chip to reset */
		mdelay(150);

		printk(KERN_ERR "%s: wifi power successed to pull up\n", __func__);
	} else {
		ret = gpio_direction_output(gpio_wlan_power, 0);
		printk("[BRCM]: gpio_direction_output(%d,0)! ret = %d\n", gpio_wlan_power, ret);
		if (ret) {
			printk(KERN_ERR "%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			ret = -EIO;
			goto out;
		}
		/* WLAN chip down */
		printk(KERN_ERR "%s: wifi power successed to pull down\n", __func__);
	}

	return ret;

out :
	return ret;
}

static int bcm_wifi_reset(int on)
{
	return 0;
}

#define PCIE_RC_IDX	0
static int bcm_wifi_carddetect(int val)
{
	int ret = 0;
	int retry_cnt = 10;

	while (retry_cnt > 0) {
		ret = msm_pcie_enumerate(PCIE_RC_IDX);

		if (!ret)
			break;

		printk("[BRCM] Enumeration failed, retry_cnt is %d left\n", retry_cnt);
		msleep(100);
		retry_cnt--;
	};

	if (!ret)
		printk("[BRCM] Card detect done\n");
	else
		printk("[BRCM] Card detect failed\n");

	return ret;
}

static int bcm_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;
	static unsigned char mymac[6] = {0, };
	const unsigned char nullmac[6] = {0, };
	pr_debug("%s: %p\n", __func__, buf);

	if (buf == NULL) return -EAGAIN;

	if (memcmp(mymac, nullmac, 6) != 0) {
		/* Mac displayed from UI are never updated..
		 * So, mac obtained on initial time is used
		 */
		memcpy(buf, mymac, 6);
		return 0;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0))
	prandom_seed((uint)jiffies);
	rand_mac = prandom_u32();
#else
	srandom32((uint)jiffies);
	rand_mac = random32();
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)) */
	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy(mymac, buf, 6);

	printk(KERN_INFO "[%s] Exiting. MyMac :  %x : %x : %x : %x : %x :%x\n",
			__func__, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

	return 0;
}

#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

/* Customized Locale table */
//CONFIG_BCM4358 and CONFIG_BCM43455
const struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"CK", "XZ", 11},	/* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XZ", 11},	/* Universal if Country code is Cuba (13.4.27)*/
	{"FO", "XZ", 11},	/* Universal if Country code is Faroe Island (13.4.27)*/
	{"IM", "XZ", 11},	/* Universal if Country code is Isle of Man (13.4.27)*/
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"JE", "XZ", 11},	/* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XZ", 11},	/* Universal if Country code is North Korea (13.4.27)*/
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"NF", "XZ", 11},	/* Universal if Country code is Norfolk Island (13.4.27)*/
	{"NU", "XZ", 11},	/* Universal if Country code is Niue (13.4.27)*/
	{"PM", "XZ", 11},	/* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"PN", "XZ", 11},	/* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SS", "XZ", 11},	/* Universal if Country code is South_Sudan (13.4.27)*/
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"AD", "AD", 0},
	{"AE", "AE", 6},
	{"AF", "AF", 0},
	{"AG", "AG", 2},
	{"AI", "AW", 2}, /*updated 2015.04.01*/
	{"AL", "AL", 2},
	{"AM", "AM", 0},
	{"AN", "GD", 2}, /*updated 2015.04.01*/
	{"AO", "AO", 0},
	{"AR", "AU", 6},
	{"AS", "AU", 6},
	{"AT", "AT", 4},
	{"AU", "AU", 6},
	{"AW", "AW", 2},
	{"AZ", "AZ", 2},
	{"BA", "BA", 2},
	{"BB", "BB", 0},
	{"BD", "AU", 6},
	{"BE", "BE", 4},
	{"BF", "BF", 0},
	{"BG", "BG", 4},
	{"BH", "BH", 4},
	{"BI", "BI", 0},
	{"BJ", "BJ", 0},
	{"BM", "AU", 6},
	{"BN", "BN", 4},
	{"BO", "NG", 0}, /*updated 2015.04.01*/
	{"BR", "BR", 15},
	{"BS", "BS", 2},
	{"BT", "BJ", 0}, /*updated 2015.04.01*/
	{"BW", "BJ", 0}, /*updated 2015.04.01*/
	{"BY", "BY", 3},
	{"BZ", "BZ", 0},
	{"CA", "US", 988},
	{"CD", "CD", 0},
	{"CF", "CF", 0},
	{"CG", "CG", 0},
	{"CH", "CH", 4},
	{"CI", "CI", 0},
	{"CL", "CL", 0},
	{"CM", "CM", 0},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"CV", "CV", 0},
	{"CX", "CX", 0},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DJ", "DJ", 0},
	{"DK", "DK", 4},
	{"DM", "DM", 0},
	{"DO", "DO", 0},
	{"DZ", "DZ", 1},
	{"EC", "EC", 21},
	{"EE", "EE", 4},
	{"EG", "EG", 13}, /*updated 2015.04.01*/
	{"ER", "ER", 0},
	{"ES", "ES", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FJ", "FJ", 0},
	{"FK", "FK", 0},
	{"FM", "FM", 0},
	{"FR", "FR", 5},
	{"GA", "GA", 0},
	{"GB", "GB", 6},
	{"GD", "GD", 2},
	{"GE", "GE", 0},
	{"GF", "GF", 2},
	{"GH", "GH", 0},
	{"GI", "GI", 0}, /*updated 2015.04.01*/
	{"GL", "GR", 4}, /*updated 2015.04.01*/
	{"GM", "GM", 0},
	{"GN", "GN", 0},
	{"GP", "GP", 2},
	{"GQ", "GQ", 0},
	{"GR", "GR", 4},
	{"GT", "GT", 1},
	{"GU", "GU", 12},
	{"GW", "GW", 0},
	{"GY", "GY", 0},
	{"HK", "HK", 2},
	{"HN", "HN", 0},
	{"HR", "HR", 4},
	{"HT", "HT", 0},
	{"HU", "HU", 4},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 7},
	{"IN", "IN", 3},
	{"IQ", "IQ", 0},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"JM", "JM", 0},
	{"JO", "JO", 3},
	{"JP", "JP", 58},
	{"KE", "SA", 0},
	{"KG", "KG", 0},
	{"KH", "KH", 2},
	{"KI", "KI", 0},
	{"KM", "KM", 0},
	{"KN", "KN", 0},
#if defined(CONFIG_MACH_MSM8996_ANNA) || defined(CONFIG_MACH_MSM8996_ELSA_DCM_JP) || defined(CONFIG_MACH_MSM8996_ELSA_KDDI_JP)
	{"KR", "KR", 962}, /* updated 2017.06.08 */
#else // h1, elsa
	{"KR", "KR", 990},
#endif
	{"KW", "KW", 5},
	{"KY", "KY", 3},
	{"KZ", "KZ", 0},
	{"LA", "LA", 2},
	{"LB", "LB", 5},
	{"LC", "LC", 0},
	{"LI", "LI", 4},
	{"LK", "LK", 1},
	{"LR", "LR", 0},
	{"LS", "LS", 2},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"LY", "LI", 4},/*updated 2015.04.01*/
	{"MA", "MA", 2},
	{"MC", "MC", 1},
	{"MD", "MD", 2},
	{"ME", "ME", 2},
	{"MF", "MF", 0},
	{"MG", "MG", 0},
	{"MK", "MK", 2},
	{"ML", "ML", 0},
	{"MM", "MM", 0},
	{"MN", "MN", 1},
	{"MO", "SG", 0}, /*updated 2015.04.01*/
	{"MP", "MP", 0},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MS", "MS", 0},
	{"MT", "MT", 4},
	{"MU", "MU", 2},
	{"MV", "MV", 3},
	{"MW", "MW", 1},
	{"MX", "MX", 20},
	{"MY", "MY", 3},
	{"MZ", "MZ", 0},
	{"NA", "NA", 0},
	{"NC", "NC", 0},
	{"NE", "NE", 0},
	{"NG", "NG", 0},
	{"NI", "NI", 2},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NP", "NP", 3}, /*updated 2015.04.01*/
	{"NR", "NR", 0},
	{"NZ", "NZ", 4}, /*updated 2015.04.01*/
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PE", "PE", 20},
	{"PF", "PF", 0},
	{"PG", "AU", 6},
	{"PH", "PH", 5},
	{"PK", "PK", 0},
	{"PL", "PL", 4},
	{"PR", "US", 988}, /*changed US/118 -> US/988*/
	{"PT", "PT", 4},
	{"PW", "PW", 0},
	{"PY", "PY", 2},
	{"QA", "QA", 0},
	{"RE", "RE", 2},
	{"RKS", "KG", 0},
	{"RO", "RO", 4},
	{"RS", "RS", 2},
	{"RU", "RU", 13},
	{"RW", "RW", 0},
	{"SA", "SA", 0},
	{"SB", "SB", 0},
	{"SC", "SC", 0},
	{"SE", "SE", 4},
	{"SG", "SG", 0},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"SL", "SL", 0},
	{"SM", "SM", 0},
	{"SN", "SN", 2},
	{"SO", "SO", 0},
	{"SR", "SR", 0},
	{"ST", "ST", 0},
	{"SV", "SV", 25},
	{"SZ", "SZ", 0},
	{"TC", "TC", 0},
	{"TD", "TD", 0},
	{"TF", "TF", 0},
	{"TG", "TG", 0},
	{"TH", "TH", 5},
	{"TJ", "TJ", 0},
	{"TM", "TM", 0},
	{"TN", "TN", 1}, /*updated 2015.04.01*/
	{"TO", "TO", 0},
	{"TR", "TR", 7},
	{"TT", "TT", 3},
	{"TV", "TV", 0},
	{"TW", "TW", 1},
	{"TZ", "TZ", 0},
	{"UA", "UA", 8},
	{"UG", "UG", 2},
	{"UM", "US", 988},
	{"US", "US", 988},
	{"UY", "UY", 1},
	{"UZ", "MA", 2},
	{"VA", "VA", 2},
	{"VC", "VC", 0},
	{"VE", "VE", 3},
	{"VG", "VG", 2},
	{"VI", "US", 988},
	{"VN", "VN", 4},
	{"VU", "VU", 0},
	{"WS", "SA", 0},/*updated 2015.04.01*/
	{"YE", "YE", 0},
	{"YT", "YT", 2},
	{"ZA", "ZA", 6},
	{"ZM", "LA", 2},
	{"ZW", "ZW", 0},
	{"DC", "XZ", 999},
};

// modifed bcm4358 country code, BCM4358A3 7.112.34.3 - Dec-04-2015
#if defined (CONFIG_BCM4358)
const struct cntry_locales_custom bcm_wifi_translate_custom_table_bcm4358[] = {
	{"AM", "AM", 1},
	{"MY", "MY", 19},
};
#endif

static void *bcm_wifi_get_country_code(char *ccode, u32 flags)
{
	int size, i;
	static struct cntry_locales_custom country_code;

// modifed bcm4358 country code, BCM4358A3 7.112.34.3 - Dec-04-2015
#if defined (CONFIG_BCM4358)
	size = ARRAY_SIZE(bcm_wifi_translate_custom_table_bcm4358);
	if ((size == 0) || (ccode == NULL))
		return NULL;
	for (i = 0; i < size; i++) {
		if (strcmp(ccode, bcm_wifi_translate_custom_table_bcm4358[i].iso_abbrev) == 0) {
			return (void *)&bcm_wifi_translate_custom_table_bcm4358[i];
		}
	}
#endif

	size = ARRAY_SIZE(bcm_wifi_translate_custom_table);

	if ((size == 0) || (ccode == NULL))
		return NULL;

	for (i = 0; i < size; i++) {
		if (strcmp(ccode, bcm_wifi_translate_custom_table[i].iso_abbrev) == 0) {
			return (void *)&bcm_wifi_translate_custom_table[i];
		}
	}

	memset(&country_code, 0, sizeof(struct cntry_locales_custom));
	strlcpy(country_code.custom_locale, ccode, COUNTRY_BUF_SZ);

	return (void *)&country_code;
}

static struct wifi_platform_data bcm_wifi_control = {
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	.set_power		= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr,
	.get_country_code = bcm_wifi_get_country_code,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  /* dummy */
		.end   = 0,  /* dummy */
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE | IORESOURCE_IRQ_HIGHLEVEL,
		},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
				.platform_data = &bcm_wifi_control,
				},
};

int bcm_wifi_init_mem(struct platform_device *platdev)
{
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	dhd_init_wlan_mem();
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	printk(KERN_INFO "bcm_wifi_init_mem successfully\n");

	return 0;
}

int bcm_wifi_init_gpio(struct platform_device *platdev)
{
	int ret = 0;
	struct device_node *np = platdev->dev.of_node;

	wifi_reg_on_pinctrl = devm_pinctrl_get(&platdev->dev);
	if (IS_ERR_OR_NULL(wifi_reg_on_pinctrl)) {
		printk("%s: target does not use pinctrl for wifi reg on\n", __func__);
	}

	gpio_wlan_power = of_get_named_gpio(np, "wlan-en-gpio", 0);
	printk(KERN_INFO "%s: gpio_wlan_power=%d\n", __func__, gpio_wlan_power);
#ifdef BCMPCIE_OOB_HOST_WAKE
	gpio_wlan_hostwake = of_get_named_gpio(np, "wlan-hostwake-gpio", 0);
	printk(KERN_INFO "%s: gpio_hostwake=%d\n", __func__, gpio_wlan_hostwake);
#endif /* BCMPCIE_OOB_HOST_WAKE */
	/* WLAN_POWER */
	if ((ret = gpio_request_one(gpio_wlan_power, GPIOF_OUT_INIT_LOW, "wifi_reg_on")) < 0)
		printk("%s: Failed to request gpio %d for bcmdhd_wifi_reg_on:[%d]\n",
			__func__, gpio_wlan_power, ret);
	msleep(10);

#ifdef BCMPCIE_OOB_HOST_WAKE
	if ((ret = gpio_request_one(gpio_wlan_hostwake, GPIOF_IN, "wifi_hostwakeup")) < 0)
		printk("Failed to request gpio %d for wifi_hostwakeup:ret[%d]\n",
			gpio_wlan_hostwake, ret);
	if (gpio_is_valid(gpio_wlan_hostwake)) {
		wifi_resource[0].start = wifi_resource[0].end = gpio_to_irq(gpio_wlan_hostwake);
	}
#endif /* BCMPCIE_OOB_HOST_WAKE */
	printk(KERN_INFO "bcm_wifi_init_gpio successfully\n");
	if (pwr_up_on_boot_time) {
		printk(KERN_INFO "bcm_wifi_power up\n");
		gpio_direction_output(gpio_wlan_power, 1);
		/* WLAN chip to reset */
		msleep(WIFI_TURNON_DELAY);
	} else {
		printk(KERN_INFO "skip bcm_wifi_power up here\n");
		gpio_direction_output(gpio_wlan_power, 0);
	}
	return 0;
}

static int bcm_wifi_probe(struct platform_device *pdev)
{
	bcm_wifi_init_mem(pdev);
	bcm_wifi_init_gpio(pdev);

	return 0;
}

static int bcm_wifi_remove(struct platform_device *pdev)
{

	return 0;
}

static struct of_device_id bcm_wifi_match_table[] = {
	{ .compatible = "lge,bcmdhd_wlan" },
	{ },
};

static struct platform_driver bcm_wifi_driver = {
	.probe = bcm_wifi_probe,
	.remove = bcm_wifi_remove,
	.driver = {
		.name = "wifi_bcm_lge",
		.owner = THIS_MODULE,
		.of_match_table = bcm_wifi_match_table,
		},
};

#ifdef ASYNC_INIT
static void __init init_bcm_wifi_async(void *data, async_cookie_t cookie)
{
#ifdef CONFIG_WIFI_CONTROL_FUNC

#ifdef LGE_BCM_WIFI_DMA_QOS_CONTROL
	INIT_DELAYED_WORK(&req_dma_work, bcm_wifi_req_dma_work);
	pm_qos_add_request(&wifi_dma_qos, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	wifi_dma_state = 1; //INIT
	printk("%s: wifi_dma_qos is added\n", __func__);
#endif /* LGE_BCM_WIFI_DMA_QOS_CONTROL */

	platform_device_register(&bcm_wifi_device);
	platform_driver_register(&bcm_wifi_driver);
#endif /* CONFIG_WIFI_CONTROL_FUNC */
}
#endif /* ASYNC_INIT */

static int __init init_bcm_wifi(void)
{
#ifdef ASYNC_INIT
	async_schedule(init_bcm_wifi_async, NULL);
#else
#ifdef CONFIG_WIFI_CONTROL_FUNC

#ifdef LGE_BCM_WIFI_DMA_QOS_CONTROL
	INIT_DELAYED_WORK(&req_dma_work, bcm_wifi_req_dma_work);
	pm_qos_add_request(&wifi_dma_qos, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	wifi_dma_state = 1; /* INIT */
	printk("%s: wifi_dma_qos is added\n", __func__);
#endif /* LGE_BCM_WIFI_DMA_QOS_CONTROL */
	platform_driver_register(&bcm_wifi_driver);
	platform_device_register(&bcm_wifi_device);
#endif /* CONFIG_WIFI_CONTROL_FUNC */
#endif /* ASYNC_INIT */
	return 0;
}

module_init(init_bcm_wifi);
