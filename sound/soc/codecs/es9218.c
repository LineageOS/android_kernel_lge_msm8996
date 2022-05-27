/*
 * es9218.c -- es9218 ALSA SoC audio driver
 *  version = 2.05, date=6-10-2016, added comments on intial register settings and a new way of Sabre_headphone_on(),, look for '6-10-2016' below 
 *  versoin = 2.04, date=5-26-2016, modified sabre_bypass2hifi() to have 2 different HiFi modes and modified sabre_hifi2bypass()
 *  versoin = 2.03, date=5-19-2016, added PCM_2_DSD switch
 *  
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
//#include <linux/regulator/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <trace/events/asoc.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "es9218.h"
#include <linux/i2c.h>

#include <linux/fs.h>
#include <linux/string.h>

#include <soc/qcom/lge/board_lge.h>

#define ES9218_DEBUG

static struct es9218_priv *g_es9218_priv = NULL;
static int 	es9218_write_reg(struct i2c_client *client, int reg, u8 value);
static int 	es9218_read_reg(struct i2c_client *client, int reg);

static int es9218_sabre_hifi2bypass(void);
static int es9218_sabre_bypass2hifi(void);

static int	es9218_sabre_lpb2hifione(void);
static int	es9218_sabre_lpb2hifitwo(void);
static int	es9218_sabre_hifione2lpb(void);
static int	es9218_sabre_hifitwo2lpb(void);

static int 	es9218_sabre_amp_start(struct i2c_client *client, int headset);
static int 	es9218_sabre_amp_stop(struct i2c_client *client, int headset);
static int 	es9218_sabre_chargepump_start(void);
static int 	es9218_sabre_chargepump_stop(void);

struct es9218_reg {
	unsigned char num;
	unsigned char value;
};

/* We only include the analogue supplies here; the digital supplies
 * need to be available well before this driver can be probed.
 */

struct es9218_reg es9218_init_register[] = {
//	{ ESS9218_03, 			0x4F  },		// AVC vol -> 0x40 = 0dB -high impedance 600 ohm , 0x46 = -6db Aux, 0x4F = -15dB  32ohm
	{ ESS9218_SVOLCNTL1, 	0x20 },
	{ ESS9218_SVOLCNTL2, 	0x63 },
	{ ESS9218_SVOLCNTL3, 	0x22 },
	{ ESS9218_CHANNELMAP, 	0x00 },		// regisert 11 - headset amp , 32 ohm high 0x10 , 600ohm low 0x00
	{ ESS9218_THD_COMP, 	0x00 },		// 0x00 to enable
	{ ESS9218_VOL1, 		0x0  },		// Master vol: 0dB
	{ ESS9218_VOL2, 		0x0  },
	{ ESS9218_MASTERTRIM, 	0x78 },		// Trim: about -0.5dB
	{ ESS9218_2ND_HCOMP1, 	0x2b },		// needs to be tuned later
	{ ESS9218_2ND_HCOMP2, 	0xfd },
	{ ESS9218_3RD_HCOMP1, 	0x00 },
	{ ESS9218_3RD_HCOMP2, 	0xff },		// 0x00 = off
	{ ESS9218_GEN_CONFIG, 	0xe4 },
	{ ESS9218_29, 			0x30 },
	{ ESS9218_30, 			0x20 },
	{ ESS9218_CP_CLOCK, 	0x30 },
//	{ ESS9218_AMP_CONFIG, 	0x3   },		//register 32 , HiFi mode setting
	{ ESS9218_39, 			0xec },
	{ ESS9218_45,   		0x7c },
//	{ ESS9218_46, 			0xc0  },		 // HIFI SWITCH
	{ ESS9218_65, 			0x76 },
	{ ESS9218_DPLLRATIO1, 	0xc0 },
};


struct es9218_reg es9218_RevB_init_register[] = {
//	{ ESS9218_02,				0xF4 }, 		// #02	: Automute Config Enable & Mute & RAMP & AVC
	
	//{ ESS9218_SVOLCNTL1,		0x01 }, 	// #04	: automute_time 	- Running Change
	//{ ESS9218_SVOLCNTL2,		0x63 }, 	// #05	: automute_level (-99dB) 
	{ ESS9218_SVOLCNTL2,		0x75 }, 	// #05	: automute_level (-117dB) 
	//{ ESS9218_SVOLCNTL2,		0x7F }, 	// #05	: automute_level (-127dB) 
	//{ ESS9218_SVOLCNTL3,		0x45 }, 	// #06	: DoP Disable
	{ ESS9218_SETTING, 			0xA0 },		// #07	: Preset filter_shape

	{ ESS9218_CHANNELMAP,		0x90 },		// #11	: Over Current Protection
	{ ESS9218_DPLLASRC,        0x8a },  		// #12	:ASRC/DPLL Bandwidth
	{ ESS9218_THD_COMP, 		0x00 },		// #13	: 0x00 to enable
	//{ ESS9218_SOFT_START, 		0x07 },		// #14	: Soft Start time

	{ ESS9218_GEN_CONFIG,		0xC4 },		// #27	: asrc_en/avc_sel/res/res/ch1_vol/latch_vol/res
	{ ESS9218_29, 				0x05 },		// #29	: Auto_Clock_gear (0x04 : MCLK/1, 0x05 : MCLK/2, 0x06 : MLCK/4)
	//{ ESS9218_29, 				0x06 },		// #29	: Auto_Clock_gear (0x04 : MCLK/1, 0x05 : MCLK/2, 0x06 : MLCK/4)
};


struct es9218_reg es9218_RevB_DOP_init_register[] = {
	{ ESS9218_IN_CONFIG, 		0x80 },		// #01	: serial_mode 32bit-data word
//	{ ESS9218_SVOLCNTL1,		0x00 }, 	// #04	: automute_time (default)
//	{ ESS9218_SVOLCNTL2,		0x68 }, 	// #05	: automute_level (default)
	{ ESS9218_SVOLCNTL3,		0x4A }, 	// #06	: DoP Enable
//	{ ESS9218_SETTING,			0x80 }, 	// #07	: Preset filter_shape (default)
//	{ ESS9218_MASTERMODE,		0x82 }, 	// #10	: Master mode enable
	{ ESS9218_CHANNELMAP,		0x90 },		// #11	: Over Current Protection
	{ ESS9218_THD_COMP, 		0x00 },		// #13	: 0x00 to enable
//	{ ESS9218_2ND_HCOMP1, 		0x6E },		// #22	: needs to be tuned later
//	{ ESS9218_2ND_HCOMP2, 		0x02 },		// #23
//	{ ESS9218_3RD_HCOMP1, 		0x20 },		// #24
//	{ ESS9218_3RD_HCOMP2, 		0x00 },		// #25
	{ ESS9218_GEN_CONFIG, 		0xC4 },		// #27	: asrc_en/avc_sel/res/res/ch1_vol/latch_vol/res
//	{ ESS9218_29,				0x00 }, 	// #29	: Auto_Clock_gear
//	{ ESS9218_AMP_CONFIG, 		0x03 },		// #32 	: Amp mode setting (0x02  : 1Vrms / 0x03  : 2Vrms)
};


static const u32 master_trim_tbl[] = {
/*0	db */		0x7FFFFFFF,
/*-	0.5	db */	0x78D6FC9D,
/*-	1	db */	0x721482BF,
/*-	1.5	db */	0x6BB2D603,
/*-	2	db */	0x65AC8C2E,
/*-	2.5	db */	0x5FFC888F,
/*-	3	db */	0x5A9DF7AA,
/*-	3.5	db */	0x558C4B21,
/*-	4	db */	0x50C335D3,
/*-	4.5	db */	0x4C3EA838,
/*-	5	db */	0x47FACCEF,
/*-	5.5	db */	0x43F4057E,
/*-	6	db */	0x4026E73C,
/*-	6.5	db */	0x3C90386F,
/*-	7	db */	0x392CED8D,
/*-	7.5	db */	0x35FA26A9,
/*-	8	db */	0x32F52CFE,
/*-	8.5	db */	0x301B70A7,
/*-	9	db */	0x2D6A866F,
/*-	9.5	db */	0x2AE025C2,
/*-	10	db */	0x287A26C4,
/*-	10.5	db */	0x26368073,
/*-	11	db */	0x241346F5,
/*-	11.5	db */	0x220EA9F3,
/*-	12	db */	0x2026F30F,
/*-	12.5	db */	0x1E5A8471,
/*-	13	db */	0x1CA7D767,
/*-	13.5	db */	0x1B0D7B1B,
/*-	14	db */	0x198A1357,
/*-	14.5	db */	0x181C5761,
/*-	15	db */	0x16C310E3,
/*-	15.5	db */	0x157D1AE1,
/*-	16	db */	0x144960C5,
/*-	16.5	db */	0x1326DD70,
/*-	17	db */	0x12149A5F,
/*-	17.5	db */	0x1111AEDA,
/*-	18	db */	0x101D3F2D,
/*-	18.5	db */	0xF367BED,
/*-	19	db */	0xE5CA14C,
/*-	19.5	db */	0xD8EF66D,
/*-	20	db */	0xCCCCCCC,
/*-	20.5	db */	0xC157FA9,
/*-	21	db */	0xB687379,
/*-	21.5	db */	0xAC51566,
/*-	22	db */	0xA2ADAD1,
/*-	22.5	db */	0x99940DB,
/*-	23	db */	0x90FCBF7,
/*-	23.5	db */	0x88E0783,
/*-	24	db */	0x8138561,
/*-	24.5	db */	0x79FDD9F,
/*-	25	db */	0x732AE17,
/*-	25.5	db */	0x6CB9A26,
/*-	26	db */	0x66A4A52,
/*-	26.5	db */	0x60E6C0B,
/*-	27	db */	0x5B7B15A,
/*-	27.5	db */	0x565D0AA,
/*-	28	db */	0x518847F,
/*-	28.5	db */	0x4CF8B43,
/*-	29	db */	0x48AA70B,
/*-	29.5	db */	0x4499D60,
/*-	30	db */	0x40C3713,
/*-	30.5	db */	0x3D2400B,
/*-	31	db */	0x39B8718,
/*-	31.5	db */	0x367DDCB,
/*-	32	db */	0x337184E,
/*-	32.5	db */	0x3090D3E,
/*-	33	db */	0x2DD958A,
/*-	33.5	db */	0x2B48C4F,
/*-	34	db */	0x28DCEBB,
/*-	34.5	db */	0x2693BF0,
/*-	35	db */	0x246B4E3,
/*-	35.5	db */	0x2261C49,
/*-	36	db */	0x207567A,
/*-	36.5	db */	0x1EA4958,
/*-	37	db */	0x1CEDC3C,
/*-	37.5	db */	0x1B4F7E2,
/*-	38	db */	0x19C8651,
/*-	38.5	db */	0x18572CA,
/*-	39	db */	0x16FA9BA,
/*-	39.5	db */	0x15B18A4,
/*-	40	db */	0x147AE14,
};

static const u8 avc_vol_tbl[] = {
/*0 db */       0x40,
/*- 1   db */   0x41,
/*- 2   db */   0x42,
/*- 3   db */   0x43,
/*- 4   db */   0x44,
/*- 5   db */   0x45,
/*- 6   db */   0x46,
/*- 7   db */   0x47,
/*- 8   db */   0x48,
/*- 9   db */   0x49,
/*- 10  db */   0x4A,
/*- 11  db */   0x4B,
/*- 12  db */   0x4C,
/*- 13  db */   0x4D,
/*- 14  db */   0x4E,
/*- 15  db */   0x4F,
/*- 16  db */   0X50,
/*- 17  db */   0X51,
/*- 18  db */   0X52,
/*- 19  db */   0X53,
/*- 20  db */   0X54,
/*- 21  db */   0X55,
/*- 22  db */   0X56,
/*- 23  db */   0X57,
/*- 24  db */   0X58,
/*- 25  db */   0X59,
};

static const char *power_state[] = {
	"CLOSE",
	"OPEN",
	"BYPASS",
	"HIFI",
	"IDLE",
	"ACTIVE",
};

static unsigned int es9218_power_state = ESS_PS_CLOSE;
static unsigned int es9218_is_amp_on = 0;
static unsigned int es9218_bps = 16;
static unsigned int es9218_rate = 48000;

static int g_headset_type = 0;
static int g_avc_volume = 0;
static int g_volume = 0;
static int g_left_volume = 0;
static int g_right_volume = 0;
static int g_sabre_cf_num = 8; // default = 8
static int g_dop_flag = 0;
static int g_auto_mute_flag = 0;
static int g_skip_auto_mute = 0; //default enable
static u8  normal_harmonic_comp[4] ={0xF4, 0x01, 0xFF, 0xEB};
static u8  advance_harmonic_comp[4] ={0xDA, 0xFD, 0xA0, 0x04};
static u8  aux_harmonic_comp[4] ={0x6A, 0xFF, 0x9C, 0xFF};

enum {
    ESS_A = 1,
    ESS_B = 2,
};
int g_ess_rev = ESS_B;


#define ES9218_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |	\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |	\
		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |	\
		SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_176400) //|SNDRV_PCM_RATE_352800  TODO for dop128

#define ES9218_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE | \
		SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE | \
		SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)



#ifdef ES9218_DEBUG
struct es9218_regmap {
	const char *name;
	uint8_t reg;
	int writeable;
} es9218_regs[] = {
	{ "SYSTEMSETTINGS",			ESS9218_SYSTEM_SET,		1 },
	{ "INPUTCONFIG",			ESS9218_IN_CONFIG,		1 },
	{ "02",						ESS9218_02, 			1 },
	{ "03",						ESS9218_03, 			1 },
	{ "SOFTVOLUMECNTL1",		ESS9218_SVOLCNTL1, 		1 },
	{ "SOFTVOLUMECNTL2",		ESS9218_SVOLCNTL2, 		1 },
	{ "SOFTVOLUMECNTL3-DEEMPH",	ESS9218_SVOLCNTL3, 		1 },
	{ "GENERALSETTING",			ESS9218_SETTING, 		1 },
	{ "GPIOCONFIG", 			ESS9218_GPIOCFG, 		1 },
	{ "09",						ESS9218_09, 			1 },
	{ "MASTERMODE",				ESS9218_MASTERMODE, 	1 },
	{ "CHANNELMAP",				ESS9218_CHANNELMAP, 	1 },
	{ "DPLLASRC", 				ESS9218_DPLLASRC, 		1 },
	{ "THDCOMP", 				ESS9218_THD_COMP, 		1 },
	{ "SOFTSTART",				ESS9218_SOFT_START, 	1 },
	{ "VOL1",					ESS9218_VOL1, 			1 },
	{ "VOL2",					ESS9218_VOL2, 			1 },
	{ "MASTERTRIM3",			ESS9218_17, 			1 },
	{ "MASTERTRIM2",			ESS9218_18, 			1 },
	{ "MASTERTRIM1",			ESS9218_19, 			1 },
	{ "MASTERTRIM0", 			ESS9218_MASTERTRIM, 	1 },
	{ "GPIOINPUTSEL",			ESS9218_GPIO_INSEL, 	1 },
	{ "2NDHARMONICCOMP1",		ESS9218_2ND_HCOMP1, 	1 },
	{ "2NDHARMONICCOMP2",		ESS9218_2ND_HCOMP2, 	1 },
	{ "3RDHARMONICCOMP1",		ESS9218_3RD_HCOMP1, 	1 },
	{ "3RDHARMONICCOMP2",		ESS9218_3RD_HCOMP2, 	1 },
	{ "CPSSDELAY",				ESS9218_CP_SS_DELAY, 	1 },	
	{ "GENCONFIG", 				ESS9218_GEN_CONFIG, 	1 },
	{ "28", 					ESS9218_28, 			1 },
	{ "29", 					ESS9218_29, 			1 },
	{ "30", 					ESS9218_30, 			1 },
	{ "CPCLOCK", 				ESS9218_CP_CLOCK, 		1 }, 
	{ "AMPCONFIG", 				ESS9218_AMP_CONFIG, 	1 },
	{ "INTMASK", 				ESS9218_INT_MASK, 		1 },
	{ "34", 					ESS9218_34, 			1 },
	{ "35", 					ESS9218_35, 			1 },
	{ "36", 					ESS9218_36, 			1 }, 
	{ "NCONUM", 				ESS9218_NCO_NUM, 		1 },
	{ "39", 					ESS9218_39, 			1 },
	{ "FILTERADDR",				ESS9218_FILTER_ADDR, 	1 },
	{ "FILTERCOEF",				ESS9218_FILTER_COEF, 	1 },
	{ "FILTERCONT",				ESS9218_FILTER_CONT, 	1 },
	{ "45", 					ESS9218_45, 			1 }, 
	{ "46", 					ESS9218_46, 			1 },
	{ "47", 					ESS9218_47, 			1 },
	{ "48", 					ESS9218_48, 			1 },
	{ "CHIPSTATUS",				ESS9218_CHIPSTATUS, 	0 },
	{ "65", 					ESS9218_65, 			0 },
	{ "DPLLRATIO66", 			ESS9218_DPLLRATIO1, 	0 },
	{ "DPLLRATIO67", 			ESS9218_DPLLRATIO2, 	0 },
	{ "DPLLRATIO68", 			ESS9218_DPLLRATIO3, 	0 },
	{ "DPLLRATIO69", 			ESS9218_DPLLRATIO4, 	0 },
	{ "INPUTSELRD", 			ESS9218_INPUT_SEL_RD, 	0 },
	{ "73", 					ESS9218_73, 			0 }, 
	{ "74", 					ESS9218_74, 			0 },
	{ "RAMRD", 					ESS9218_RAM_RD, 		0 }
};


void es9218_check_dop(void) {
    //if(g_dop_flag > 0 ) {
        pr_info("%s() reg64[0]locked: 0x%x, reg72[3]:dop: 0x%x\n", __func__,
        es9218_read_reg(g_es9218_priv->i2c_client, ESS9218_CHIPSTATUS), //reg64[0] lock_status
        es9218_read_reg(g_es9218_priv->i2c_client, ESS9218_INPUT_SEL_RD)); //reg72[3] dop_status
    //}
    return;
}

static ssize_t es9218_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned i, n, reg_count;
	u8 read_buf;

	reg_count = sizeof(es9218_regs) / sizeof(es9218_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		read_buf = es9218_read_reg(g_es9218_priv->i2c_client, es9218_regs[i].reg);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s <#%d>= 0x%02X\n",
			       es9218_regs[i].name,	es9218_regs[i].reg,
			       read_buf);
	}

	return n;
}

static ssize_t es9218_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned i, reg_count, value;
	int error = 0;
	char name[45]; 

	if (count >= 45) {
		pr_err("%s:input too long\n", __func__);
		return -1;
	}

	if (sscanf(buf, "%25s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	pr_info("%s: %s %0xx",__func__,name,value);
	reg_count = sizeof(es9218_regs) / sizeof(es9218_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, es9218_regs[i].name)) {
			if (es9218_regs[i].writeable) {
				error = es9218_write_reg(g_es9218_priv->i2c_client,
											es9218_regs[i].reg, value);
				if (error) {
					pr_err("%s:Failed to write %s\n", __func__, name);
					return -1;
				}
			} 
			else {
				pr_err("%s:Register %s is not writeable\n", __func__, name);
				return -1;
			}
			
			return count;
		}
	}

	pr_err("%s:no such register %s\n", __func__, name);
	return -1;
}

static DEVICE_ATTR(registers, S_IWUSR | S_IRUGO,
		es9218_registers_show, es9218_registers_store);

static struct attribute *es9218_attrs[] = {
	&dev_attr_registers.attr,
	NULL
};

static const struct attribute_group es9218_attr_group = {
	.attrs = es9218_attrs,
};
/*
static void es9218_print_regdump(void)
{
	unsigned i, reg_count;
	u8 read_buf;

	reg_count = sizeof(es9218_regs) / sizeof(es9218_regs[0]);
	for (i = 0; i < reg_count; i++) {
		read_buf = es9218_read_reg(g_es9218_priv->i2c_client, es9218_regs[i].reg);
		pr_info("%-20s <0x%2X>= 0x%02X\n", es9218_regs[i].name, i, read_buf);
	}
}
*/
#endif


/*   ES9812's Power state / mode control signals
	reset_gpio;			//HIFI_RESET_N
	power_gpio;			//HIFI_LDO_SW
	hph_switch_gpio;		//HIFI_MODE2
	reset_gpio=H && hph_switch_gpio=L 	--> HiFi mode
	reset_gpio=L && hph_switch_gpio=H 	--> Bypass mode
	reset_gpio=L && hph_switch_gpio=L 	--> Shutdown mode
*/


static void es9218_power_gpio_H(void)
{
	gpio_set_value(g_es9218_priv->es9218_data->power_gpio, 1);
	pr_info("%s(): pa_gpio_level = %d\n", __func__, __gpio_get_value(g_es9218_priv->es9218_data->power_gpio));
}

static void es9218_power_gpio_L(void)
{
	gpio_set_value(g_es9218_priv->es9218_data->power_gpio, 0);
	pr_info("%s(): pa_gpio_level = %d\n", __func__,	__gpio_get_value(g_es9218_priv->es9218_data->power_gpio));
}

static void es9218_reset_gpio_H(void)
{
	gpio_set_value(g_es9218_priv->es9218_data->reset_gpio, 1);
	pr_info("%s(): pa_gpio_level = %d\n", __func__,	__gpio_get_value(g_es9218_priv->es9218_data->reset_gpio));
}

static void es9218_reset_gpio_L(void)
{
	gpio_set_value(g_es9218_priv->es9218_data->reset_gpio, 0);
	pr_info("%s(): pa_gpio_level = %d\n", __func__,	__gpio_get_value(g_es9218_priv->es9218_data->reset_gpio));
}

static void es9218_hph_switch_gpio_H(void)
{
	gpio_set_value(g_es9218_priv->es9218_data->hph_switch, 1);
	pr_info("%s(): hph_switch = %d\n", __func__, __gpio_get_value(g_es9218_priv->es9218_data->hph_switch));
}

static void es9218_hph_switch_gpio_L(void)
{
	gpio_set_value(g_es9218_priv->es9218_data->hph_switch, 0);
	pr_info("%s(): hph_switch = %d\n", __func__, __gpio_get_value(g_es9218_priv->es9218_data->hph_switch));
}

static int es9218_master_trim(struct i2c_client *client, int vol)
{
	int ret = 0;
	u32 value;

	if (vol >= sizeof(master_trim_tbl)/sizeof(master_trim_tbl[0])) {
		pr_err("%s() : Invalid vol = %d return \n", __func__, vol);
		return 0;
	}

	value = master_trim_tbl[vol];
	pr_info("%s(): MasterTrim = %08X \n", __func__, value);

	if	(es9218_power_state == ESS_PS_IDLE) {
		pr_err("%s() : Invalid vol = %d return \n", __func__, vol);
		return 0;
 	} 

	#if	(0)
	if (vol == 0) {
		pr_info("%s(): MasterTrim Volum = %d return \n", __func__, vol);
		return 0;
	}
	#endif

	ret |= es9218_write_reg(g_es9218_priv->i2c_client ,ESS9218_17,
						value&0xFF);

	ret |= es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_18,
						(value&0xFF00)>>8);

	ret |= es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_19,
						(value&0xFF0000)>>16);

	ret |= es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_MASTERTRIM,
						(value&0xFF000000)>>24);
	return ret;
}

static int es9218_set_avc_volume(struct i2c_client *client, int vol)
{
	int ret = 0;
	u8 value;

	if (vol >= sizeof(avc_vol_tbl)/sizeof(avc_vol_tbl[0])) {
		pr_err("%s() : Invalid vol = %d return \n", __func__, vol);
		return 0;
	}

	value = avc_vol_tbl[vol];
	pr_info("%s(): AVC Volume = %X \n", __func__, value);

	if	(es9218_power_state == ESS_PS_IDLE) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
 	}

	ret |= es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_03, value);
	return ret;
}

static int es9218_set_thd(struct i2c_client *client, int headset)
{
	int ret = 0;

    switch(headset) {
         case 1: // normal
			/*	Reg #22, #23	: THD_comp2	(-16dB)	*/
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_2ND_HCOMP1, normal_harmonic_comp[0]);
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_2ND_HCOMP2, normal_harmonic_comp[1]);

			/*	Reg #24, #25	: THD_comp3	(-16dB)	*/
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_3RD_HCOMP1, normal_harmonic_comp[2]);
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_3RD_HCOMP2, normal_harmonic_comp[3]); 
            break;

        case 2: // advance
			/*	Reg #22, #23	: THD_comp2	(-1dB)	*/
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_2ND_HCOMP1, advance_harmonic_comp[0]);
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_2ND_HCOMP2, advance_harmonic_comp[1]);

			/*	Reg #24, #25	: THD_comp3	(-1dB)	*/
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_3RD_HCOMP1, advance_harmonic_comp[2]);
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_3RD_HCOMP2, advance_harmonic_comp[3]); 
            break;

        case 3: // aux
			/*	Reg #22, #23	: THD_comp2	(-7dB)	*/
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_2ND_HCOMP1, aux_harmonic_comp[0]);
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_2ND_HCOMP2, aux_harmonic_comp[1]);

			/*	Reg #24, #25	: THD_comp3	(-7dB)	*/
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_3RD_HCOMP1, aux_harmonic_comp[2]);
			ret = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_3RD_HCOMP2, aux_harmonic_comp[3]); 
            break;

		default :
			//pr_err("%s() : Invalid headset = %d \n", __func__, headset);
			break;
    }

	pr_info("%s(): Headset Type = %d \n", __func__, headset);

	return ret;
}

static int es9218_sabre_amp_start(struct i2c_client *client, int headset)
{
	int ret = 0;
	pr_info("%s(): Headset Type = %d \n", __func__, headset);

	//NOTE  GPIO2 must already be HIGH as part of Chargepump_Start
 
    switch(headset) {
         case 1:
			//	normal
			//
			//	Low impedance 50RZ or less headphone detected
         	//	Use HiFi1 amplifier mode
         	//
         	es9218_sabre_lpb2hifione();
            break;

        case 2:
			//	advanced
			//
			//	High impedance >50RZ - <600RZ headphone detected (64RZ or 300RZ for example)
			//	Use HiFi2 amplifier mode
			//
         	es9218_sabre_lpb2hifitwo();
            break;

        case 3:
			//	aux
			//
			//	High impedance >600RZ line-out detected
			//	Use HiFi2 amplifier mode
			//
         	es9218_sabre_lpb2hifitwo();
            break;

		default :
			pr_err("%s() : Unknown headset = %d \n", __func__, headset);
			ret = 1;
			break;
    }

	return ret;
}

static int es9218_sabre_amp_stop(struct i2c_client *client, int headset)
{
	int ret = 0;
	switch(headset) {
				 case 1:
					//	normal
					//
					//	Low impedance 32RZ or less headphone detected
					//	Use HiFi1 amplifier mode
					//
					pr_err("%s() : 1 valid headset = %d \n", __func__, g_headset_type);
					es9218_sabre_hifione2lpb();
					
					break;
	
				case 2:
					//	advanced
					//
					//	High impedance >32RZ - <600RZ headphone detected (64RZ or 300RZ for example)
					//	Use HiFi2 amplifier mode
					//
					
					pr_err("%s() : 2 valid headset = %d \n", __func__, g_headset_type);
					es9218_sabre_hifitwo2lpb();
					break;
	
				case 3:
					//	aux
					//
					//	High impedance >600RZ line-out detected
					//	Use HiFi2 amplifier mode
					//
					pr_err("%s() : 3 valid headset = %d \n", __func__, g_headset_type);
					es9218_sabre_hifitwo2lpb();
					break;
	
				default :
					pr_err("%s() : Invalid headset = %d \n", __func__, g_headset_type);
					ret = 1;
					break;
			}
	return ret;
}

/*
Program stage1 and stage2 filter coefficients
*/
static int es9218_sabre_cfg_custom_filter(struct sabre_custom_filter *sabre_filter)
{
	int rc, i, *coeff;
	int count_stage1;
	u8 rv;
	pr_info("%s(): g_sabre_cf_num = %d \n", __func__, g_sabre_cf_num);

	if(g_sabre_cf_num < 0)
		return 0;

	if(g_sabre_cf_num > 3) {
		rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_FILTER_CONT, 0x00);
		switch(g_sabre_cf_num) {
			case 4:
			// 3b000: linear phase fast roll-off filter
				rv = 0x00;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 5:
			// 3b001: linear phase slow roll-off filter
				rv = 0x20;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 6:
			// 3b010: minimum phase fast roll-off filter #1
				rv = 0x40;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 7:
			// 3b011: minimum phase slow roll-off filter
				rv = 0x60;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 8:
			// 3b100: apodizing fast roll-off filter type 1 (default)
				rv = 0x80;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 9:
			// 3b101: apodizing fast roll-off filter type 2
				rv = 0xA0;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 10:
			// 3b110: hybrid fast roll-off filter
				rv = 0xC0;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			case 11:
			// 3b111: brick wall filter
				rv = 0xE0;
				rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
				break;
			default:
				pr_info("%s(): default = %d \n", __func__, g_sabre_cf_num);
				break;
		}
		return rc;
	}
	count_stage1 = sizeof(sabre_filter->stage1_coeff)/sizeof(sabre_filter->stage1_coeff[0]);
	pr_info("%s: count_stage1 : %d",__func__,count_stage1);

	rv = (sabre_filter->symmetry << 2) | 0x02;        // set the write enable bit
	rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_FILTER_CONT, rv);
	if (rc < 0) {
		pr_info("%s: rc = %d return ",__func__, rc);
		return rc;
	}

	rv = es9218_read_reg(g_es9218_priv->i2c_client, ESS9218_FILTER_ADDR-1);
	coeff = sabre_filter->stage1_coeff;
	for (i = 0; i < count_stage1 ; i++) { // Program Stage 1 filter coefficients
		u8 value[4];
		value[0] =  i;
		value[1] = (*coeff & 0xff);
		value[2] = ((*coeff>>8) & 0xff);
		value[3] = ((*coeff>>16) & 0xff);
		i2c_smbus_write_block_data(g_es9218_priv->i2c_client, ESS9218_FILTER_ADDR-1, 4, value);
		coeff++;
	}
	coeff = sabre_filter->stage2_coeff;
	for (i = 0; i < 16; i++) { // Program Stage 2 filter coefficients
		u8 value[4];
		value[0] =  128 + i;
		value[1] = (*coeff & 0xff);
		value[2] = ((*coeff>>8) & 0xff);
		value[3] = ((*coeff>>16) & 0xff);
		i2c_smbus_write_block_data(g_es9218_priv->i2c_client, ESS9218_FILTER_ADDR-1, 4, value);
		coeff++;
	}
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_FILTER_ADDR-1, rv);

	rv = (sabre_filter->shape << 5); // select the custom filter roll-off shape
	rv |= 0x80;
	rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SETTING, rv);
	if (rc < 0) {
		pr_info("%s: rc = %d return ",__func__, rc);
		return rc;
	}
	rv = (sabre_filter->symmetry << 2); // disable the write enable bit
	rv |= 0x1; // Use the custom oversampling filter.
	rc = es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_FILTER_CONT, rv);
	if (rc < 0) {
		pr_info("%s: rc = %d return ",__func__, rc);
		return rc;
	}
	return 0;
}

 
static int	es9218_sabre_lpb2hifione(void)
{
	pr_info("%s(): entry: state = %s\n", __func__, power_state[es9218_power_state]);

	//GOLDEN SEQUENCE.  DO NOT MODIFY
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x06);		//	ramp down fast, chip should already be ramped down, presetting for upcoming override enable
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SVOLCNTL3, 0x47);		//	speed up volume ramp rate

	//preset overrides to take control but stay in LowFi mode
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xCC);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x68);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x01);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x80); //enable overrides, amp input shunt and output shunt both engaged

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x7F); //CPL_ENS 1, SEL3v3 1 and ENSM_PS 1 for smooth transition
	mdelay(5);

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xDC); //CPH_ENS 1
	mdelay(2);

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xFC); //ENHPA 1

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x78); //supplies for 1.8V and smooth transition
	
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x81); //SHTINB 1 release input shunt

	
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG, 0x02);	//	AMP MODE HiFi1, Enable HiFi but partially blocked by overrides
	mdelay(8);

    //disengage the switch quietly
	es9218_hph_switch_gpio_L(); //GPIO2 LOW
	mdelay(8);
	
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x58); //ENAUX_OE 0 give switch control to GPIO2

    //finish enabling amplifier
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x41); //ENHPA_OUT 1 enable amp output stage
		
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x87); //Soft_Start 1, set ramp rate to 7 and ramp up
	mdelay(50);

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x83); //SHTOUTB 1 release output shunt

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x03); //disable overrides, rely on register 32 control now

	es9218_hph_switch_gpio_H(); //GPIO2 HIGH, this happens in the background, we're already in HiFi1 mode.

	return	0;
}

static int	es9218_sabre_lpb2hifitwo(void)
{

	pr_info("%s(): entry: state = %s\n", __func__, power_state[es9218_power_state]);

#if 0 //original
	//simplified sequence based on ESS recommendation with only settings strictly required by V20
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x07);		//	set soft start ramp rate to 7

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x00);				//	disable overrides

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG, 0x03);		//	set amp mode to HiFi2
#else
    //GOLDEN SEQUENCE.  DO NOT MODIFY
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x06);      //  ramp down fast, chip should already be ramped down, presetting for upcoming override enable
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SVOLCNTL3, 0x47);       //  speed up volume ramp rate

    //preset overrides to take control but stay in LowFi mode
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xCC);
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x68);
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x01);
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x80); //enable overrides, amp input shunt and output shunt both engaged

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x7F); //CPL_ENS 1, SEL3v3 1 and ENSM_PS 1 for smooth transition
    mdelay(5);

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xDC); //CPH_ENS 1
    mdelay(2);

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xFC); //ENHPA 1

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x7D); //disable ENSM_PS, leave 3v3 supply
                                                                   //once amp supply voltage change is complete this is not meant to be left on
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x81); //SHTINB 1 release input shunt


    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG, 0x03);  //AMP MODE HiFi2, Enable HiFi but partially blocked by overrides

    mdelay(8);

    //disengage the switch quietly
    es9218_hph_switch_gpio_L(); //GPIO2 LOW
    mdelay(8);
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x5D); //ENAUX_OE 0 give switch control to GPIO2

    //finish enabling amplifier
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x41); //ENHPA_OUT 1 enable amp output stage
        
    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x87); //Soft_Start 1, set ramp rate to 7 and ramp up
    mdelay(50);

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x83); //SHTOUTB 1 release output shunt

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x03); //disable overrides, rely on register 32 control now

    es9218_hph_switch_gpio_H(); //GPIO2 HIGH, this happens in the background, we're already in HiFi1 mode.
#endif
	return	0;
}

static int	es9218_sabre_chargepump_start(void)
{
	//
	// ESS recommended sequence for starting chargepumps quietly in ES9218VA
	// This function should only be used once after power-on
	//
	pr_info("%s(): entry: state = %s\n", __func__, power_state[es9218_power_state]);

	//NOTE, GPIO2 must already be LOW before power-on
    //es9218_hph_switch_gpio_L();	// GPIO2 LOW 

	es9218_reset_gpio_H(); 	// RESETb HIGH
 	mdelay(1);

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_CP_SS_DELAY, 0x01);		//	Speed up CPL chargepump charge rate 

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x80);		//	Enable overrides, amp input shunt and output shunt both enabled

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x18);		//	Configure CPL chargepump both strong mode and weak mode

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x58);		//	Enable chargepumps (CPL chargepump on, CPH chargepump still off)

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_52, 0x01);		//	Speed up CPH chargepump charge rate

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0x18);		//	Enable CPH chargepump, both strong mode and weak mode at the same time
	mdelay(3); //Allow chargepumps to stabilize.  Tune as needed.

	//	Now chargepumps are started
	//	Move to Low Power Bypass mode
    es9218_hph_switch_gpio_H(); // GPIO2 HIGH
	es9218_reset_gpio_L(); // RESETb LOW 
	
	return	0;
}

static int	es9218_sabre_chargepump_stop(void)
{
	//
	// ESS recommended sequence for stopping chargepumps quietly in ES9218VA
	// This function should only be used once before power-off
	//
	es9218_reset_gpio_H(); // RESETb HIGH
	mdelay(1);

	// restore override settings for LowFi mode before enabling overrides
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0x98);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x78);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x80);		//	Enable overrides, amp output shunt and input shunt both enabled

    es9218_hph_switch_gpio_L(); //GPIO2 LOW, relying on overrides to hold chargepumps on now

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x58);		//	Open switch using ENAUX_OE 0

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0x00);		//	Disable CPH chargepump and analog regulator
	mdelay(10); //Allow time for CPH chargepump to discharge before attempting to stop CPL chargepump
				//Exact time depends on decoupling cap size, but typical time for adequate discharge is ~10-20ms

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x18);		//	Disable CPL chargepump
	mdelay(1); 	//Allow time for CPL chargepump to discharge
				//Exact time depends on decoupling cap size but typical time for full discharge is ~3ms

	es9218_reset_gpio_L(); // RESETb LOW move into Standby mode.
	mdelay(10); // extra time for chargepumps to discharge, may not be necessary.  Tune as needed.

	return	0;
}


static int	es9218_sabre_hifione2lpb(void)
{

	pr_info("%s()\n", __func__);

	// GOLDEN SEQUENCE DO NOT MODIFY

	// preset overrides to take control for HiFi1 mode
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0x7C);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x78);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x41);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x86);	//	preset for ramp up fast, chip should already be ramped up at this point.  Should have no effect, just presetting for upcoming steps
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x81);			//	enable overrides, engage output shunt

	es9218_hph_switch_gpio_L(); //we're aiming for core-on mode before transition to LPB

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x07);		//	ramp down fast
	mdelay(50); //must leave enough time for ramp to complete even with slowest possible MCLK after clock gearing, MCLK = 45.1584MHz /2

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x01);		// ENHPA_OUT 0 *POP

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG, 0x00);		// amp mode core on, amp mode gpio set to trigger Core On

	es9218_reset_gpio_L();	//RESETb LOW move to Standby mode

	es9218_sabre_chargepump_start();//Quiet transition from Standby mode to Low Power Bypass mode

	return	0;
}

static int	es9218_sabre_hifitwo2lpb(void)
{

	pr_info("%s()\n", __func__);

	//GOLDEN SEQUENCE DO NOT MODIFY
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0x7C);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0x7F);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x41);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x86);
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x81);			//	enable overrides, amp output shunt enabled, amp input shunt disabled

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x07);		//	Register 14 - set soft start rate to 7 and trigger ramp down
	mdelay(50); // wait for soft start ramp down to finish, must leave enough time for ramp to complete even with slowest possible MCLK = 45.1584MHz /2
				// ramp down MUST finish before next step or there will be a loud *pop*
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_48, 0x01);		//	Disable amp output stage

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_47, 0xFD);		//	Disable analog regulator fast charge, disable power switch smooth-transition

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0x80);		//	Amp input shunt enabled

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_45, 0xFC);		//	Enable switch

	es9218_reset_gpio_L(); //RESETb LOW move to Low Power Bypass mode

	return	0;
}

static int es9218_sabre_bypass2hifi(void)
{
	int i;

	if ( es9218_power_state != ESS_PS_BYPASS ) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}
	pr_info("%s() : state = %s\n", __func__, power_state[es9218_power_state]);
	es9218_reset_gpio_H();
	mdelay(1);

    if(g_ess_rev == ESS_A) {
		
		if (g_dop_flag == 0) {
			pr_info("%s() : ESS_A Running !! \n", __func__);
			for(i = 0 ; i < sizeof(es9218_init_register)/sizeof(es9218_init_register[0]) ; i++) {
						es9218_write_reg(g_es9218_priv->i2c_client,
									es9218_init_register[i].num,
									es9218_init_register[i].value);
			}
		}
		else {
			pr_info("%s(): Rev-A DOP Format Test Running: NOT SUPPORTED!!!!! \n", __func__);
		}
    } 
	else if(g_ess_rev == ESS_B) {
		pr_info("%s() : ESS_B Running !! \n", __func__);

		if (g_dop_flag == 0) { //  PCM
			pr_info("%s(): Rev-B PCM Format Reg Initial in es9218_sabre_bypass2hifi() \n", __func__);

			/*  PCM Reg. Initial    */
			for (i = 0 ; i < sizeof(es9218_RevB_init_register)/sizeof(es9218_RevB_init_register[0]) ; i++) {
				es9218_write_reg(g_es9218_priv->i2c_client,
								es9218_RevB_init_register[i].num,
								es9218_RevB_init_register[i].value);
			}
            /*  BSP Setting     */
            switch (es9218_bps) {
                case 16 :
                    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_IN_CONFIG, 0x0);
                    break;
                case 24 :
                case 32 :
                default :
                    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_IN_CONFIG, 0x80);
                    /*  Automute Time Setting   */
                    if(!g_skip_auto_mute){
                    switch  (es9218_rate) {
                        case 48000  :
                            break;
                        case 96000  :
                            /*  Automute Time                   */
                            /*   0xFF : 0 msec ,    0x07 : 50 msec ,    0x04 : 100 msec ,   0x02 : 170 msec */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SVOLCNTL1, 0x0A);      // 0x02 : 170msec, 0x0A : 34msec
                            /*  Volume Rate */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SVOLCNTL3, 0x46);

                            /*  Soft Start  */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x0A);

                            /*  Auto Mute enable    */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_02, 0xF4);
                            break;
                        case 192000 :
                        default :
                            /*  Automute Time                   */
                            /*   0xFF : 0 msec ,    0x04 : 50 msec ,    0x02 : 100 msec ,   0x01 : 170 msec */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SVOLCNTL1, 0x05);      // 0x01 : 170msec, 0x05 : 34msec

                            /*  Volume Rate */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SVOLCNTL3, 0x45);

                            /*  Soft Start  */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_SOFT_START, 0x0B);

                            /*  Auto Mute enable    */
                            es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_02, 0xF4);
                            break;
                    }
                }
                    break;
            }
		}
		else if (g_dop_flag > 0) { //  DOP
			pr_info("%s(): Rev-B DOP Format Reg Initial in es9218_sabre_bypass2hifi() \n", __func__);

			/*  DOP Reg. Initial    */
			for (i = 0 ; i < sizeof(es9218_RevB_DOP_init_register)/sizeof(es9218_RevB_DOP_init_register[0]) ; i++) {
				es9218_write_reg(g_es9218_priv->i2c_client,
								es9218_RevB_DOP_init_register[i].num,
								es9218_RevB_DOP_init_register[i].value);
			}

			/*  BSP Setting     */
			switch (g_dop_flag) {
				case 64 :
					//es9218_write_reg(g_es9218_priv, ESS9218_SYSTEM_SET, 0x04);   //  Reg #0
					es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_MASTERMODE, 0xA2);   //  Reg #10
					break;

				case 128 :
				default :
					//es9218_write_reg(g_es9218_priv, ESS9218_SYSTEM_SET, 0x00);   //  Reg #0	: HW Default Value
					es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_MASTERMODE, 0x82);   //  Reg #10
					break;
			}
		}
	}
	else {
		pr_info("%s() : Unknown g_ess_rev = %d \n", __func__, g_ess_rev);
	}

	es9218_set_thd(g_es9218_priv->i2c_client, g_headset_type);

    if(g_ess_rev == ESS_A) {
		
		if (g_dop_flag == 0) {
			switch (es9218_bps) {
				case 16 :
					es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_IN_CONFIG, 0x0);
					break;
				case 24 :
				default :
					es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_IN_CONFIG, 0x80);
					break;
			}
		}
		else {
			pr_info("%s(): Rev-A DOP Format Test Running \n", __func__);
		}
   	}
	//	es9218_sabre_cfg_custom_filter(&es9218_sabre_custom_ft[g_sabre_cf_num]);

    es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_VOL1, g_left_volume); 	// set left channel digital volume level

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_VOL2, g_right_volume); 	// set right channel digital volume level

	pr_info("%s() : g_left_volume = %d, g_right_volume = %d \n", __func__, g_left_volume, g_right_volume);

	es9218_master_trim(g_es9218_priv->i2c_client, g_volume); 					// set master trim level

	es9218_set_avc_volume(g_es9218_priv->i2c_client, g_avc_volume); 			// set analog volume control, must happen before amp start

	es9218_sabre_amp_start(g_es9218_priv->i2c_client, g_headset_type); 			// move to HiFi mode

    if(g_ess_rev == ESS_A) {
		// switch , hifi amp mode setting
		es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG, 0x03);
		es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0xc0);
    } 
	
    es9218_power_state = ESS_PS_HIFI;

	return 0;
}

static int es9218_sabre_hifi2bypass(void)
{
	if ( es9218_power_state < ESS_PS_HIFI )	{
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	pr_info("%s() : state = %s\n", __func__, power_state[es9218_power_state]);

    if(g_ess_rev == ESS_A) {
		pr_info("%s() : ESS_A Running !! \n", __func__);
		es9218_power_gpio_H();
		es9218_reset_gpio_H();
		mdelay(1);
		es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG,0x01);
		es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0);
    } 

	else if(g_ess_rev == ESS_B)	{
		pr_info("%s() : ESS_B Running !! \n", __func__);
	    es9218_sabre_amp_stop(g_es9218_priv->i2c_client, g_headset_type); // places the chip in Low Power Bypass Mode
     }
	
	else {
		pr_err("%s() : Unknown g_ess_rev = %d \n", __func__, g_ess_rev);
	}

	es9218_power_state = ESS_PS_BYPASS;
	
    return 0;
}

/* HiFi mode, playback is stopped or paused
*/
static int es9218_sabre_audio_idle(void)
{
	if ( es9218_power_state != ESS_PS_HIFI ) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	pr_info("%s() : state = %s\n", __func__, power_state[es9218_power_state]);


	/*	Auto Mute disable	*/
	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_02, 0x34);

	es9218_power_state = ESS_PS_IDLE;
	return 0;
}

static int es9218_sabre_audio_active(void)
{
	if ( es9218_power_state != ESS_PS_IDLE ) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	pr_info("%s() : state = %s\n", __func__, power_state[es9218_power_state]);

	es9218_power_state = ESS_PS_HIFI;
	return 0;
}

/* Power up to bypass mode when headphone is plugged in
   Sabre DAC (ES9218) is still in power-down mode, but switch is on and sw position is at AUX/QC_CODEC.
   Thus, i2c link will not function as HiFi_RESET_N is still '0'.
*/
static int __es9218_sabre_headphone_on(void)
{

	pr_info("%s(): entry: state = %s\n", __func__, power_state[es9218_power_state]);

	if (es9218_power_state == ESS_PS_CLOSE)	{
		if(g_ess_rev == ESS_A) {
			pr_info("%s() : ESS_A Running !! \n", __func__);
			es9218_power_gpio_H();
			es9218_reset_gpio_H();
			mdelay(1);
			es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_AMP_CONFIG,0x01);
			es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_46, 0);
			es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_03, 0x40);     //avc max control , Need for checking L impedance

		}
		else if(g_ess_rev == ESS_B) {
			pr_info("%s() : ESS_B Running !! \n", __func__);

			es9218_hph_switch_gpio_L(); // GPIO2 LOW

			es9218_power_gpio_H(); // Power on, GPIO2 must be low before power on 

			es9218_sabre_chargepump_start();
		}
		else {
			pr_err("%s() : Unknown g_ess_rev = %d \n", __func__, g_ess_rev);
		}

		es9218_power_state = ESS_PS_BYPASS;
		return 0;
	} 
	else if (es9218_power_state == ESS_PS_BYPASS &&	es9218_is_amp_on) {
		pr_info("%s() : state = %s , is_amp_on = %d \n",	__func__, power_state[es9218_power_state], es9218_is_amp_on);
		es9218_sabre_bypass2hifi();
	}
	else {
		pr_err("%s() : state = %s , skip enabling EDO.\n",	__func__, power_state[es9218_power_state]);
		return 0;
	}

	pr_info("%s(): end \n", __func__);

	return 0;
}

/* Power down when headphone is plugged out. This state is the same as system power-up state. */
static int __es9218_sabre_headphone_off(void)
{
	if ( es9218_power_state == ESS_PS_CLOSE) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	if ( es9218_power_state != ESS_PS_BYPASS ||
		es9218_power_state != ESS_PS_IDLE) {
		es9218_sabre_hifi2bypass(); // if power state indicates chip is in HiFi mode, move to Low Power Bypass
	}
	pr_info("%s() : state = %d\n", __func__, es9218_power_state);
	
	es9218_sabre_chargepump_stop();

	// Remove this line to implement Always-On power mode.
	//es9218_power_gpio_L(); // power off

	es9218_power_state = ESS_PS_CLOSE;
	return 0;
}

/* Power up to bypass mode when headphone is plugged in
   Sabre DAC (ES9018) is still in power-down mode, but switch is on and sw position is at AUX/QC_CODEC.
   Thus, i2c link will not function as HiFi_RESET_N is still '0'.
*/
int es9218_sabre_headphone_on(void)
{
	pr_info("%s() Called !! \n", __func__);
	
	mutex_lock(&g_es9218_priv->power_lock);
	__es9218_sabre_headphone_on();
	mutex_unlock(&g_es9218_priv->power_lock);
	return 0;
}

/* Power down when headphone is plugged out. This state is the same as system power-up state.
*/
int es9218_sabre_headphone_off(void)
{
	pr_info("%s() Called !! \n", __func__);

	mutex_lock(&g_es9218_priv->power_lock);
	__es9218_sabre_headphone_off();
	mutex_unlock(&g_es9218_priv->power_lock);
	return 0;
}

int es9218_get_power_state(void){
	return es9218_power_state;
}

static void es9218_sabre_sleep_work (struct work_struct *work)
{
	wake_lock_timeout(&g_es9218_priv->sleep_lock, msecs_to_jiffies( 2000 ));
	mutex_lock(&g_es9218_priv->power_lock);
	if (es9218_power_state == ESS_PS_IDLE) {
		pr_info("%s(): sleep_work state is %s running \n", __func__, power_state[es9218_power_state]);
		
		es9218_sabre_hifi2bypass();
	}
	else {
		pr_info("%s(): sleep_work state is %s skip operation \n", __func__, power_state[es9218_power_state]);
	}
	mutex_unlock(&g_es9218_priv->power_lock);
	return;
}

static int es9218_power_state_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s(): power state = %d\n", __func__, es9218_power_state);
	
	ucontrol->value.enumerated.item[0] = es9218_power_state;
	
	pr_info("%s(): ucontrol = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	return 0;
}

static int es9218_power_state_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;

	pr_info("%s():ucontrol = %d, power state=%d\n", __func__, ucontrol->value.enumerated.item[0], es9218_power_state);

	if (es9218_power_state == ucontrol->value.enumerated.item[0]) {
		pr_info("%s():no power state change\n", __func__);
	}

	//"Open", "Close","Bypass","Hifi","Idle","Active","PowerHigh","PowerLow","HphHigh","HphLow"
	switch(ucontrol->value.enumerated.item[0]) {
		case 0:
			__es9218_sabre_headphone_on();
			break;
		case 1:
			__es9218_sabre_headphone_off();
			break;
		case 2:
			es9218_sabre_hifi2bypass();
			break;
		case 3:
			es9218_sabre_bypass2hifi();
			break;
		case 4:
			es9218_sabre_audio_idle();
			break;
		case 5:
			es9218_sabre_audio_active();
			break;
		case 6:
			es9218_reset_gpio_H();
			break;
		case 7:
			es9218_reset_gpio_L();
			break;
		case 8:
			es9218_hph_switch_gpio_H();
			break;
		case 9:
			es9218_hph_switch_gpio_L();
			break;
		default:
			break;

	}
	return ret;
}


static int es9218_normal_harmonic_comp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	int i =0 ;
	for( i =0  ;  i < 4  ; i ++ ) {
		normal_harmonic_comp[i] = (u8) ucontrol->value.integer.value[i];
	}
	return 0 ;
}
static int es9218_advance_harmonic_comp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	int i =0;
	for( i =0  ;  i < 4  ; i ++ ) {
		advance_harmonic_comp[i] = (u8) ucontrol->value.integer.value[i];
	}
	return 0;
}
static int es9218_aux_harmonic_comp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	int i =0;
	for( i =0  ;  i < 4  ; i ++ ) {
		aux_harmonic_comp[i] = (u8) ucontrol->value.integer.value[i];
	}
	return 0;
}
static int es9218_headset_type_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_headset_type;

	pr_info("%s(): type = %d \n", __func__, g_headset_type);

	return 0;
}

static int es9218_headset_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret =0 ;
	int is_headset_update =0;

    if((int)ucontrol->value.integer.value[0]!=0)
    {
		if (g_headset_type != (int)ucontrol->value.integer.value[0])
		is_headset_update=1;
    	g_headset_type = (int)ucontrol->value.integer.value[0];
    }

	
	pr_info("%s(): type = %d \n   state = %s headset type update = %d", __func__, g_headset_type , power_state[es9218_power_state], is_headset_update);
	if (es9218_power_state != ESS_PS_HIFI) {
		if (es9218_power_state == ESS_PS_IDLE && is_headset_update) {
			es9218_power_state = ESS_PS_BYPASS;
		}
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	es9218_set_thd(g_es9218_priv->i2c_client, g_headset_type);
	es9218_sabre_bypass2hifi();

	return ret;
}

static int es9218_auto_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_auto_mute_flag;

	pr_info("%s(): type = %d \n", __func__, g_auto_mute_flag);

	return 0;
}

static int es9218_auto_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret =0 ;

	g_auto_mute_flag = (int)ucontrol->value.integer.value[0];

	pr_info("%s(): g_auto_mute_flag = %d \n", __func__, g_auto_mute_flag);

	if (es9218_power_state < ESS_PS_HIFI) {
		pr_err("%s() : return = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

   if(g_auto_mute_flag || g_skip_auto_mute)
    {
        pr_info("%s(): Disable g_auto_mute_flag = %d \n", __func__, g_auto_mute_flag);
        es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_02,0x34); // #02	: Automute Config Disable
    }
    else
    {
        /*  BSP Setting     */
        switch (es9218_bps) {
            case 24 :
            case 32 :
                switch  (es9218_rate) {
                    case 96000  :
                    case 192000 :
                        /*  Auto Mute enable    */
                              pr_info("%s(): Enable g_auto_mute_flag = %d \n", __func__, g_auto_mute_flag);
                              es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_02,0xF4); // #02  : Automute Config Enable
                    break;
                }
        }
    }
	return ret;
}

static int es9218_avc_volume_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_avc_volume;
	
	pr_info("%s(): AVC Volume= -%d db\n", __func__, g_avc_volume);

	return 0;
}

static int es9218_avc_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;

	g_avc_volume = (int)ucontrol->value.integer.value[0];
	pr_info("%s(): AVC Volume= -%d db  state = %s\n", __func__, g_avc_volume , power_state[es9218_power_state]);

	if (es9218_power_state != ESS_PS_HIFI) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	es9218_set_avc_volume(g_es9218_priv->i2c_client, g_avc_volume);
	return ret;
}

static int es9218_master_volume_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_volume;
	pr_info("%s(): Master Volume= -%d db\n", __func__, g_volume/2);

	return 0;
}

static int es9218_master_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;

	g_volume = (int)ucontrol->value.integer.value[0];
	pr_info("%s(): Master Volume= -%d db\n", __func__, g_volume/2);


	if (es9218_power_state < ESS_PS_HIFI) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	es9218_master_trim(g_es9218_priv->i2c_client, g_volume);
	return ret;
}

static int es9218_left_volume_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_left_volume;
	pr_info("%s(): Left Volume= -%d db\n", __func__, g_left_volume/2);

	return 0;
}

static int es9218_left_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;

	g_left_volume = (int)ucontrol->value.integer.value[0];
	pr_info("%s(): Left Volume= -%d db\n", __func__, g_left_volume/2);

	if (es9218_power_state < ESS_PS_HIFI) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_VOL1, g_left_volume);
	return ret;
}

static int es9218_right_volume_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_right_volume;
	pr_info("%s(): Right Volume= -%d db\n", __func__, g_right_volume/2);

	return 0;
}

static int es9218_right_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;

	g_right_volume = (int)ucontrol->value.integer.value[0];
	pr_info("%s(): Right Volume= -%d db\n", __func__, g_right_volume/2);

	if (es9218_power_state < ESS_PS_HIFI) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	es9218_write_reg(g_es9218_priv->i2c_client, ESS9218_VOL2, g_right_volume);
	return ret;
}

static int es9218_filter_enum_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_sabre_cf_num;
	pr_info("%s(): ucontrol = %d\n", __func__, g_sabre_cf_num);
	return 0;
}

static int es9218_filter_enum_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;

	g_sabre_cf_num = (int)ucontrol->value.integer.value[0];
	pr_info("%s():filter num= %d\n", __func__, g_sabre_cf_num);
	es9218_sabre_cfg_custom_filter(&es9218_sabre_custom_ft[g_sabre_cf_num]);
	return ret;
}

static int es9218_dop_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_dop_flag;
	pr_info("%s() %d\n", __func__, g_dop_flag);
	return 0;
}

static int es9218_dop_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	g_dop_flag = (int)ucontrol->value.integer.value[0];
	pr_info("%s() dop_enable:%d, state:%d\n", __func__, g_dop_flag, es9218_power_state);
    if( !(g_dop_flag == 0 || g_dop_flag == 64 || g_dop_flag == 128 ) )
        pr_err("%s() dop_enable error:%d. invalid arg.\n", __func__, g_dop_flag);
	return 0;
}

static int es9218_chip_state_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret ;
	
	pr_info("%s(): enter\n", __func__);


	mutex_lock(&g_es9218_priv->power_lock);
	es9218_power_gpio_H();
	mdelay(1);
	es9218_reset_gpio_H();
	mdelay(1);
	ret = es9218_read_reg(g_es9218_priv->i2c_client,ESS9218_SYSTEM_SET);
	if(ret<0){
		pr_err("%s : i2_read fail : %d\n",__func__ ,ret);
		ucontrol->value.enumerated.item[0] = 0; // fail
	}else{
		pr_err("%s : i2_read success : %d\n",__func__ ,ret);
		ucontrol->value.enumerated.item[0] = 1; // true
	}

	if(es9218_power_state < ESS_PS_HIFI){
		es9218_reset_gpio_L();
	}
#if 0
	if(es9218_power_state == ESS_PS_CLOSE){
		es9218_power_gpio_L();
	}
#else
	if(es9218_power_state > ESS_PS_ACTIVE){
		es9218_power_gpio_L();
	}
#endif

	mutex_unlock(&g_es9218_priv->power_lock);

	pr_info("%s(): leave\n", __func__);
	return 0;
}

static int es9218_chip_state_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret=0;
//    ret= ucontrol->value.enumerated.item[0];
	pr_info("%s():ret = %d\n", __func__ ,ret );
	return 0;
}


static int es9218_sabre_wcdon2bypass_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s(): power state = %d\n", __func__, es9218_power_state);

//	ucontrol->value.enumerated.item[0] = es9218_power_state;	
//	pr_info("%s(): ucontrol = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	return 0;
}

static int es9218_sabre_wcdon2bypass_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0 ;

	mutex_lock(&g_es9218_priv->power_lock);


	ret = (int)ucontrol->value.integer.value[0];

	pr_info("%s(): entry wcd on : %d \n ", __func__ , ret);

	if(ret==0)
	{
        if(es9218_is_amp_on){
    		__es9218_sabre_headphone_on();
    		es9218_sabre_bypass2hifi();
            pr_info("%s() : state = %s : WCD On State ByPass -> HiFi !!\n", __func__, power_state[es9218_power_state]);
        }
        else{
            pr_info("%s() : state = %s : don't change\n", __func__, power_state[es9218_power_state]);
        }
    }else{
        if ( es9218_power_state > ESS_PS_BYPASS ) {
    //  if ( es9218_power_state == ESS_PS_IDLE ) {
            pr_info("%s() : state = %s : WCD On State HiFi -> ByPass !!\n", __func__, power_state[es9218_power_state]);
            cancel_delayed_work_sync(&g_es9218_priv->sleep_work);
            es9218_sabre_hifi2bypass();
            es9218_is_amp_on = 0;
        } 
        else {
            pr_info("%s() : Invalid state = %s !!\n", __func__, power_state[es9218_power_state]);
        }
	}
	pr_info("%s(): exit\n", __func__);

	mutex_unlock(&g_es9218_priv->power_lock);

	return 0;
}

static int es9218_clk_divider_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int err_check = -1;
	u8 reg_val;

	if (es9218_power_state < ESS_PS_HIFI) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	err_check = es9218_read_reg(g_es9218_priv->i2c_client,
				MASTER_MODE_CONTROL);
	if(err_check >= 0)
		reg_val = err_check;
	else
		return -1;

	reg_val = reg_val >> 5;
	ucontrol->value.integer.value[0] = reg_val;

	pr_info("%s: i2s_length = 0x%x\n", __func__, reg_val);

	return 0;
}

static int es9218_clk_divider_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int err_check = -1;
	u8 reg_val;

	if (es9218_power_state < ESS_PS_HIFI) {
		pr_err("%s() : invalid state = %s\n", __func__, power_state[es9218_power_state]);
		return 0;
	}

	pr_info("%s: ucontrol->value.integer.value[0]  = %ld\n", __func__, ucontrol->value.integer.value[0]);

	err_check = es9218_read_reg(g_es9218_priv->i2c_client,
				MASTER_MODE_CONTROL);
	if(err_check >= 0)
		reg_val = err_check;
	else
		return -1;

	reg_val &= ~(I2S_CLK_DIVID_MASK);
	reg_val |=  ucontrol->value.integer.value[0] << 5;

	es9218_write_reg(g_es9218_priv->i2c_client,
				MASTER_MODE_CONTROL, reg_val);
	return 0;
}


static const char * const es9218_power_state_texts[] = {
	"Close", "Open", "Bypass","Hifi","Idle","Active","ResetHigh","ResetLow","HphHigh","HphLow"
};

static const char * const es9218_clk_divider_texts[] = {
	"DIV4", "DIV8", "DIV16", "DIV16"
};

static const struct soc_enum es9218_power_state_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(es9218_power_state_texts),
		es9218_power_state_texts);

static const struct soc_enum es9218_clk_divider_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(es9218_clk_divider_texts),
		es9218_clk_divider_texts);

static struct snd_kcontrol_new es9218_digital_ext_snd_controls[] = {
    SOC_SINGLE_EXT("Es9018 Left Volume", SND_SOC_NOPM, 0, 256, 0,
                    es9218_left_volume_get,
                    es9218_left_volume_put),
    SOC_SINGLE_EXT("Es9018 Right Volume", SND_SOC_NOPM, 0, 256, 0,
                    es9218_right_volume_get,
                    es9218_right_volume_put),
    SOC_SINGLE_EXT("Es9018 Master Volume", SND_SOC_NOPM, 0, 100, 0,
                    es9218_master_volume_get,
                    es9218_master_volume_put),
    SOC_SINGLE_EXT("HIFI Custom Filter", SND_SOC_NOPM, 0, 12, 0, //current 0~3 custom filter num 4~11 built-in filter
                    es9218_filter_enum_get,
                    es9218_filter_enum_put),
    //SOC_SINGLE_EXT("HIFI THD Value", SND_SOC_NOPM, 0, 0xFFFFFF, 0, //current 0~3 filter num
    //                es9018_set_filter_enum,
    //                es9218_filter_enum_put),
    SOC_ENUM_EXT("Es9018 State", es9218_power_state_enum,
                    es9218_power_state_get,
                    es9218_power_state_put),
    SOC_ENUM_EXT("Es9018 CLK Divider", es9218_clk_divider_enum,
                    es9218_clk_divider_get,
                    es9218_clk_divider_put),
    SOC_SINGLE_EXT("Es9018 Chip State", SND_SOC_NOPM, 0, 1, 0,
                    es9218_chip_state_get,
                    es9218_chip_state_put),
    SOC_SINGLE_EXT("Es9018 AVC Volume", SND_SOC_NOPM, 0, 25, 0,
                    es9218_avc_volume_get,
                    es9218_avc_volume_put),
    SOC_SINGLE_EXT("Es9018 HEADSET TYPE", SND_SOC_NOPM, 0, 4, 0,
                    es9218_headset_type_get,
                    es9218_headset_type_put),
    SOC_SINGLE_EXT("Es9018 Dop", SND_SOC_NOPM, 0, 128, 0,
                    es9218_dop_get,
                    es9218_dop_put),
    SOC_SINGLE_EXT("Es9218 AUTO_MUTE", SND_SOC_NOPM, 0, 1, 0,
                    es9218_auto_mute_get,
                    es9218_auto_mute_put),
    SOC_SINGLE_EXT("Es9218 Bypass", SND_SOC_NOPM, 0, 1, 0,
                       es9218_sabre_wcdon2bypass_get,
                       es9218_sabre_wcdon2bypass_put),
   SOC_SINGLE_MULTI_EXT("Es9218 NORMAL_HARMONIC", SND_SOC_NOPM, 0, 256,
   0, 4, NULL, es9218_normal_harmonic_comp_put),
   SOC_SINGLE_MULTI_EXT("Es9218 ADVANCE_HARMONIC", SND_SOC_NOPM, 0, 256,
   0, 4, NULL, es9218_advance_harmonic_comp_put),
   SOC_SINGLE_MULTI_EXT("Es9218 AUX_HARMONIC", SND_SOC_NOPM, 0, 256,
   0, 4, NULL, es9218_aux_harmonic_comp_put),
};

static int es9218_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("%s: err %d\n", __func__, ret);
	}

	return ret;
}

static int es9218_write_reg(struct i2c_client *client, int reg, u8 value)
{

	int ret,i;

	//pr_info("%s(): %03d=0x%x\n", __func__, reg, value);
	for (i=0; i<3; i++)	{
		ret = i2c_smbus_write_byte_data(client, reg, value);
		if (ret < 0) {
			pr_err("%s: err %d,and try again\n", __func__, ret);
			mdelay(50);
		}
		else {
			break;
		}
	}

	if (ret < 0) {
		pr_err("%s: err %d\n", __func__, ret);
	}

	return ret;
}

static int es9218_populate_get_pdata(struct device *dev,
		struct es9218_data *pdata)
{

	pdata->reset_gpio = of_get_named_gpio(dev->of_node,
			"dac,reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		pr_err("Looking up %s property in node %s failed %d\n", "dac,reset-gpio", dev->of_node->full_name, pdata->reset_gpio);
		goto err;
	}
	pr_info("%s: reset gpio %d", __func__, pdata->reset_gpio);

	pdata->hph_switch = of_get_named_gpio(dev->of_node,
			"dac,hph-sw", 0);
	if (pdata->hph_switch < 0) {
		pr_err("Looking up %s property in node %s failed %d\n", "dac,hph-sw", dev->of_node->full_name, pdata->hph_switch);
		goto err;
	}
	pr_info("%s: hph switch %d", __func__, pdata->hph_switch);

#ifdef DEDICATED_I2C
	pdata->i2c_scl_gpio= of_get_named_gpio(dev->of_node,
			"dac,i2c-scl-gpio", 0);
	if (pdata->i2c_scl_gpio < 0) {
		pr_err("Looking up %s property in node %s failed %d\n", "dac,i2c-scl-gpio", dev->of_node->full_name, pdata->i2c_scl_gpio);
		goto err;
	}
	dev_dbg(dev, "%s: i2c_scl_gpio %d", __func__, pdata->i2c_scl_gpio);

	pdata->i2c_sda_gpio= of_get_named_gpio(dev->of_node,
			"dac,i2c-sda-gpio", 0);
	if (pdata->i2c_sda_gpio < 0) {
		pr_err("Looking up %s property in node %s failed %d\n", "dac,i2c-sda-gpio", dev->of_node->full_name, pdata->i2c_sda_gpio);
		goto err;
	}
	pr_info("%s: i2c_sda_gpio %d", __func__, pdata->i2c_sda_gpio);
#endif

	pdata->power_gpio= of_get_named_gpio(dev->of_node,
			"dac,power-gpio", 0);
	if (pdata->power_gpio < 0) {
		pr_err("Looking up %s property in node %s failed %d\n", "dac,power-gpio", dev->of_node->full_name, pdata->power_gpio);
		goto err;
	}
	pr_info("%s: power gpio %d\n", __func__, pdata->power_gpio);

    if(pdata->hw_rev == HW_REV_B) {
        pdata->ear_dbg = of_get_named_gpio(dev->of_node,
                "dac,ear-dbg", 0);
        if (pdata->ear_dbg < 0) {
            pr_err("Looking up %s property in node %s failed %d\n", "dac,ear-dbg", dev->of_node->full_name, pdata->ear_dbg);
            goto err;
        }
        pr_info("%s: ear_dbg gpio %d\n", __func__, pdata->ear_dbg);
    }

	if (of_property_read_u32(dev->of_node,"lge,skip-auto-mute", &g_skip_auto_mute)) {
		g_skip_auto_mute = 0;
	} else {
		pr_info("%s: g_skip_auto_mute = %d \n", __func__, g_skip_auto_mute);
	}

	return 0;
err:
	devm_kfree(dev, pdata);
	return -1;
}

static unsigned int es9218_codec_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	//struct es9218_priv *priv = codec->control_data;
	return 0;
}

static int es9218_codec_write(struct snd_soc_codec *codec, unsigned int reg,
		unsigned int value)
{
	//struct es9218_priv *priv = codec->control_data;
	return 0;
}

static int es9218_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	int ret = 0;

	/* dev_dbg(codec->dev, "%s(codec, level = 0x%04x): entry\n", __func__, level); */

	switch (level) {
		case SND_SOC_BIAS_ON:
			break;

		case SND_SOC_BIAS_PREPARE:
			break;

		case SND_SOC_BIAS_STANDBY:
			break;

		case SND_SOC_BIAS_OFF:
			break;
	}
	snd_soc_codec_force_bias_level(codec, level);

	/* dev_dbg(codec->dev, "%s(): exit\n", __func__); */
	return ret;
}

static int es9218_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *codec_dai)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es9218_priv *priv = codec->control_data;

	int ret = -1;
	u8 i2c_len_reg = 0;
	u8 in_cfg_reg = 0;


	es9218_bps = hw_param_interval(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS)->min;
	es9218_rate = params_rate(params);

	pr_info("%s(): entry , bps : %d , rate : %d\n", __func__, es9218_bps, es9218_rate);


    if(g_ess_rev == ESS_A) {
        pr_info("%s(): Rev-A Format Running \n", __func__);

        ret = 0;
    } 
    else if(g_ess_rev == ESS_B) {
        if(g_dop_flag == 0) { //  PCM
            pr_info("%s(): Rev-B PCM Format Running \n", __func__);


            /*  BSP Setting     */
            switch (es9218_bps) {
                case 16 :
                    i2c_len_reg = 0x0;
					in_cfg_reg |= i2c_len_reg;
					ret = es9218_write_reg(priv->i2c_client, ESS9218_IN_CONFIG, in_cfg_reg);
                    break;

                case 24 :
                case 32 :
                default :
                    i2c_len_reg = 0x80;
					in_cfg_reg |= i2c_len_reg;
					ret = es9218_write_reg(priv->i2c_client, ESS9218_IN_CONFIG, in_cfg_reg);

					if (!g_skip_auto_mute) {
					/*	Automute Time Setting	*/
					switch	(es9218_rate) {
						case 48000	:

							#if	(0)
							/*	Automute Time					*/
							/*	 0xFF : 0 msec ,	0x13 : 50 msec ,	0x07 : 100 msec ,	0x04 : 170 msec */
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SVOLCNTL1, 0x02);		// 0x04 : 170msec, 0x12 : 34msec

							/*	Volume Rate */
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SVOLCNTL3, 0x47);

							/*	Soft Start	*/
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SOFT_START, 0x0B);

							/*	Auto Mute enable	*/
							ret = es9218_write_reg(priv->i2c_client, ESS9218_02, 0xF4);
							#endif

							break;

						case 96000	:
							/*	Automute Time					*/
							/*	 0xFF : 0 msec ,	0x07 : 50 msec ,	0x04 : 100 msec ,	0x02 : 170 msec */
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SVOLCNTL1, 0x0A);		// 0x02 : 170msec, 0x0A : 34msec

							/*	Volume Rate */
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SVOLCNTL3, 0x46);

							/*	Soft Start	*/
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SOFT_START, 0x0A);

							/*	Auto Mute enable	*/
							ret = es9218_write_reg(priv->i2c_client, ESS9218_02, 0xF4);
							break;

						case 192000 :
						default :
							/*	Automute Time					*/
							/*	 0xFF : 0 msec ,	0x04 : 50 msec ,	0x02 : 100 msec ,	0x01 : 170 msec */
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SVOLCNTL1, 0x05);		// 0x01 : 170msec, 0x05 : 34msec

							/*	Volume Rate */
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SVOLCNTL3, 0x45);

							/*	Soft Start	*/
							ret = es9218_write_reg(priv->i2c_client, ESS9218_SOFT_START, 0x0B);

							/*	Auto Mute enable	*/
							ret = es9218_write_reg(priv->i2c_client, ESS9218_02, 0xF4);
							break;
					}
				}

                    break;
            }

			mdelay(10);

        }
        else if (g_dop_flag > 0) { //  DOP
            pr_info("%s(): Rev-B DOP Format Running \n", __func__);
            ret = 0;
            /*	2016-07-14	Update Start	*/
            #if	(0)	//	Register Initial Postion move to es9218_sabre_bypass2hifi

            /*  DOP Reg. Initial    */
            for(i = 0 ; i < sizeof(es9218_RevB_DOP_init_register)/sizeof(es9218_RevB_DOP_init_register[0]) ; i++) {
                ret = es9218_write_reg(g_es9218_priv->i2c_client,
                                es9218_RevB_DOP_init_register[i].num,
                                es9218_RevB_DOP_init_register[i].value);
            }
    
            /*  BSP Setting     */
            switch (g_dop_flag) {
                case 64 :
                    //es9218_write_reg(priv->i2c_client, ESS9218_SYSTEM_SET, 0x04);   //  Reg #0
                    ret = es9218_write_reg(priv->i2c_client, ESS9218_MASTERMODE, 0xA2);   //  Reg #10
                    break;
                    
                case 128 :
                default :
                    //es9218_write_reg(priv->i2c_client, ESS9218_SYSTEM_SET, 0x00);   //  Reg #0	: HW Default Value
                    ret = es9218_write_reg(priv->i2c_client, ESS9218_MASTERMODE, 0x82);   //  Reg #10
                    break;
            }
            #endif	//	End of	#if (0)
            /*	2016-07-14	Update Start	*/
        }
    }
    else {
        pr_err("%s() : Unknown g_ess_rev = %d \n", __func__, g_ess_rev);
    }

	pr_info("%s(): exit, ret=%d\n", __func__, ret);
	return ret;
}

static int es9218_mute(struct snd_soc_dai *dai, int mute)
{
	//struct snd_soc_codec *codec = dai->codec;
	//struct es9218_priv *priv = codec->control_data;

	//pr_info("%s(): entry\n", __func__);
	return 0;

}

static int es9218_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	//struct snd_soc_codec *codec = codec_dai->codec;
	//struct es9218_priv *priv = codec->control_data;
	//pr_info("%s(): entry\n", __func__);

	return 0;
}


static int es9218_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	//struct snd_soc_codec *codec = codec_dai->codec;
	//struct es9218_priv *priv = codec->control_data;
	//pr_info("%s(): entry\n", __func__);

	return 0;
}

static int es9218_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
//	struct snd_soc_codec *codec = dai->codec;
	mutex_lock(&g_es9218_priv->power_lock);

	pr_info("%s(): entry\n", __func__);
	if ( es9218_power_state == ESS_PS_IDLE ) {
		pr_info("%s() : state = %s : Audio Active !!\n", __func__, power_state[es9218_power_state]);
		cancel_delayed_work_sync(&g_es9218_priv->sleep_work);
		es9218_sabre_audio_active();
	} 

	else {
		pr_info("%s() : state = %s : goto HIFI !!\n", __func__, power_state[es9218_power_state]);
		__es9218_sabre_headphone_on();
		es9218_sabre_bypass2hifi();
	}
	es9218_is_amp_on = 1;

	pr_info("%s(): exit\n", __func__);

	mutex_unlock(&g_es9218_priv->power_lock);
	return 0;
}

static void es9218_shutdown(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	mutex_lock(&g_es9218_priv->power_lock);
	dev_info(codec->dev, "%s(): entry\n", __func__);
	es9218_sabre_audio_idle();

	wake_lock_timeout(&g_es9218_priv->shutdown_lock, msecs_to_jiffies( 5000 ));
	//schedule_delayed_work(&g_es9218_priv->sleep_work, msecs_to_jiffies(100));	//	100msec
	//schedule_delayed_work(&g_es9218_priv->sleep_work, msecs_to_jiffies(450));		//	450 msec
	//schedule_delayed_work(&g_es9218_priv->sleep_work, msecs_to_jiffies(500));		//	500 msec
	schedule_delayed_work(&g_es9218_priv->sleep_work, msecs_to_jiffies(3000));		//	3 Sec
	//schedule_delayed_work(&g_es9218_priv->sleep_work, msecs_to_jiffies(5000));	//	5 Sec

	es9218_is_amp_on = 0;
	mutex_unlock(&g_es9218_priv->power_lock);
}

static int es9218_hw_free(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "%s(): entry\n", __func__);

	return 0;
}


static const struct snd_soc_dai_ops es9218_dai_ops = {
	.hw_params		= es9218_pcm_hw_params,  //soc_dai_hw_params
	.digital_mute	= es9218_mute,
	.set_fmt		= es9218_set_dai_fmt,
	.set_sysclk		= es9218_set_dai_sysclk,
	.startup		= es9218_startup,
	.shutdown		= es9218_shutdown,
	.hw_free		= es9218_hw_free,
};

static struct snd_soc_dai_driver es9218_dai[] = {
	{
		.name 	= "es9218-hifi",
		.playback = {
			.stream_name 	= "Playback",
			.channels_min 	= 2,
			.channels_max 	= 2,
			.rates 			= ES9218_RATES,
			.formats 		= ES9218_FORMATS,
		},
		.capture = {
			.stream_name 	= "Capture",
			.channels_min 	= 2,
			.channels_max 	= 2,
			.rates 			= ES9218_RATES,
			.formats 		= ES9218_FORMATS,
		},
		.ops = &es9218_dai_ops,
	},
};

static  int es9218_codec_probe(struct snd_soc_codec *codec)
{
	struct es9218_priv *priv = snd_soc_codec_get_drvdata(codec);

	pr_info("%s(): entry\n", __func__);

	if (priv)
		priv->codec = codec;
	else
        pr_info("%s(): fail !!!!!!!!!!\n", __func__);
	codec->control_data = snd_soc_codec_get_drvdata(codec);

	es9218_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	pr_info("%s(): exit \n", __func__);

	return 0;
}

static int  es9218_codec_remove(struct snd_soc_codec *codec)
{
	es9218_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_es9218 = {
	.probe 			= es9218_codec_probe,
	.remove 		= es9218_codec_remove,
	.read 			= es9218_codec_read,
	.write 			= es9218_codec_write,
	.controls 		= es9218_digital_ext_snd_controls,
	.num_controls 	= ARRAY_SIZE(es9218_digital_ext_snd_controls),
};

static int es9218_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct es9218_priv *priv;
	struct es9218_data *pdata;
	int ret = 0;

    pr_info("%s", __func__);

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: no support for i2c read/write"
				"byte data\n", __func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct es9218_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
#if defined(CONFIG_MACH_MSM8996_ELSA_KDDI_JP) || defined(CONFIG_MACH_MSM8996_ELSA_DCM_JP) || \
			defined(CONFIG_MACH_MSM8996_ANNA_GLOBAL_COM) || defined(CONFIG_MACH_MSM8996_ANNA_KR)
		pdata->hw_rev = HW_REV_B; //[Temporary] For ANNA JP Board Rev.0 + ESS Rev.b
#else
		pdata->hw_rev = lge_get_board_revno();
		pr_info("%s() lg board1 rev_(%d)\n", __func__, pdata->hw_rev);
#endif
		ret = es9218_populate_get_pdata(&client->dev, pdata);
		if (ret) {
			pr_err("Parsing DT failed(%d)", ret);
			return ret;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("%s: no platform data\n", __func__);
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct es9218_priv),
			GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->i2c_client = client;
	priv->es9218_data = pdata;
	i2c_set_clientdata(client, priv);

	g_es9218_priv = priv;
	INIT_DELAYED_WORK(&g_es9218_priv->sleep_work, es9218_sabre_sleep_work);

	mutex_init(&g_es9218_priv->power_lock);

	wake_lock_init(&g_es9218_priv->sleep_lock, WAKE_LOCK_SUSPEND, "sleep_lock");
	wake_lock_init(&g_es9218_priv->shutdown_lock, WAKE_LOCK_SUSPEND, "shutdown_lock");

    if(pdata->hw_rev == HW_REV_B) {
        ret = gpio_request_one(pdata->ear_dbg, GPIOF_IN,"gpio_earjack_debugger");
        if (ret < 0) {
            pr_err("%s(): debugger gpio request failed\n", __func__);
            goto ear_dbg_gpio_request_error;
        }
        pr_info("%s: ear dbg. gpio num : %d, value : %d!\n",
            __func__, pdata->ear_dbg, gpio_get_value(pdata->ear_dbg));
    }

	ret = gpio_request(pdata->power_gpio, "ess_power");
	if (ret < 0) {
		pr_err("%s(): ess power request failed\n", __func__);
		goto power_gpio_request_error;
	}
	ret = gpio_direction_output(pdata->power_gpio, 1);
	if (ret < 0) {
		pr_err("%s: ess power set failed\n", __func__);
		goto power_gpio_request_error;
	}
	gpio_set_value(pdata->power_gpio, 0);

	ret = gpio_request(pdata->hph_switch, "ess_switch");
	if (ret < 0) {
		pr_err("%s(): ess switch request failed\n", __func__);
		goto switch_gpio_request_error;
	}
	ret = gpio_direction_output(pdata->hph_switch, 1);
	if (ret < 0) {
		pr_err("%s: ess switch set failed\n", __func__);
		goto switch_gpio_request_error;
	}
	gpio_set_value(pdata->hph_switch, 0);

	ret = gpio_request(pdata->reset_gpio, "ess_reset");
	if (ret < 0) {
		pr_err("%s(): ess reset request failed\n", __func__);
		goto reset_gpio_request_error;
	}
	ret = gpio_direction_output(pdata->reset_gpio, 1);
	if (ret < 0) {
		pr_err("%s: ess reset set failed\n", __func__);
		goto reset_gpio_request_error;
	}
	gpio_set_value(pdata->reset_gpio, 0);

    //rev.a             : ESS_A
    //rev.b + ESS_A': ESS_A
    //rev.b + ESS_B : ESS_B
    //rev.c~           : ESS_B
    if(pdata->hw_rev <= HW_REV_A) {
        g_ess_rev = ESS_A;
    } else if(pdata->hw_rev == HW_REV_B) {
        u8  readChipStatus;
        unsigned char   chipId = 0x00;
        int readCnt;

        es9218_power_gpio_H();
        mdelay(10);
        es9218_reset_gpio_H();
        mdelay(1);
        for (readCnt = 0; readCnt < 3; readCnt++) {
	        readChipStatus = es9218_read_reg(g_es9218_priv->i2c_client, ESS9218_CHIPSTATUS);
	        chipId = readChipStatus & 0xF8;

	        if ((chipId == 0xC8) || (chipId == 0xC0)) {
		        pr_info("%s: chipId:0x%x\n", __func__, chipId);
		        break;
	        }
	        mdelay(1);
        }

        es9218_reset_gpio_L();
        if(gpio_get_value(pdata->ear_dbg)) // if earjack debugger enable, do not operate power down.
            printk("go to skip \n");
            //es9218_power_gpio_L();
        else
            es9218_hph_switch_gpio_H();

        if(chipId != 0xC8)	//	not g_ess_rev_B
            g_ess_rev = ESS_A;
        es9218_power_gpio_H(); // earjack insert noise
    }

    pr_info("%s() g_ess_rev:%d\n", __func__, g_ess_rev);


	ret = snd_soc_register_codec(&client->dev,
				      &soc_codec_dev_es9218,
				      es9218_dai, ARRAY_SIZE(es9218_dai));
#ifdef ES9218_DEBUG
	ret = sysfs_create_group(&client->dev.kobj, &es9218_attr_group);
#endif
	printk("snd_soc_register_codec ret = %d\n",ret);
	return ret;

switch_gpio_request_error:
	gpio_free(pdata->hph_switch);
reset_gpio_request_error:
	gpio_free(pdata->reset_gpio);
power_gpio_request_error:
	gpio_free(pdata->power_gpio);
ear_dbg_gpio_request_error:
    if(pdata->hw_rev == HW_REV_B) {
        gpio_free(pdata->ear_dbg);
    }
	return ret;

}

static int es9218_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	mutex_destroy(&g_es9218_priv->power_lock);
	return 0;
}

static struct of_device_id es9218_match_table[] = {
	{ .compatible = "dac,es9218-codec", },
	{}
};

static const struct i2c_device_id es9218_id[] = {
	{ "es9218-codec", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, isa1200_id);

static struct i2c_driver es9218_i2c_driver = {
	.driver	= {
		.name			= "es9218-codec",
		.owner 			= THIS_MODULE,
		.of_match_table = es9218_match_table,
	},
	.probe		= es9218_probe,
	.remove		= es9218_remove,
	//.suspend	= es9218_suspend,
	//.resume	= es9218_resume,
	.id_table	= es9218_id,
};

static int __init es9218_init(void)
{
    pr_info("%s()\n", __func__);
	return i2c_add_driver(&es9218_i2c_driver);
}

static void __exit es9218_exit(void)
{
  	i2c_del_driver(&es9218_i2c_driver);
}

module_init(es9218_init);
module_exit(es9218_exit);

MODULE_DESCRIPTION("ASoC ES9218 driver");
MODULE_AUTHOR("ESS-LINshaodong");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es9218-codec");
