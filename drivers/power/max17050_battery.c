/*
 * Fuel gauge driver for Maxim 17050 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2012 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/power/max17050_battery.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#ifdef CONFIG_LGE_PM
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/lge/board_lge.h>
#include <linux/moduleparam.h>
#endif

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
#include <linux/power/lge_battery_id.h>
#endif

#ifdef CONFIG_LGE_PM_MAX17050_POLLING
#define MAX17050_POLLING_PERIOD_20 20000
#define MAX17050_POLLING_PERIOD_10 10000
#define MAX17050_POLLING_PERIOD_5 5000
#endif

/* Factory cable type */
#define LT_CABLE_56K		6
#define LT_CABLE_130K		7
#define LT_CABLE_910K		11

/* Status register bits */
#define STATUS_POR_BIT      (1 << 1)
#define STATUS_BI_BIT       (1 << 11)
#define STATUS_BR_BIT       (1 << 15)

/* Interrupt config/status bits */
#define CFG_ALRT_BIT_ENBL	(1 << 2)
#define CFG_EXT_TEMP_BIT	(1 << 8)
#define STATUS_INTR_SOCMIN_BIT	(1 << 10)
#define STATUS_INTR_SOCMAX_BIT	(1 << 14)

#define FAKE_OCV_LGC 3502
#define FAKE_OCV_TCD 3451

#define FAIL_SAFE_SOC 77
#define FAIL_SAFE_VALUE 999
#define FAIL_SAFE_TEMP 25

enum print_reason {
	PR_DEBUG		= BIT(0),
	PR_INFO		= BIT(1),
	PR_ERR		= BIT(2),
};

static int fg_debug_mask = PR_INFO|PR_ERR;
module_param_named(
	debug_mask, fg_debug_mask, int, S_IRUSR | S_IWUSR
);

#define pr_max17050(reason, fmt, ...)                                \
	do {                                                             \
	if (fg_debug_mask & (reason))                                    \
		pr_info("[MAX17050] " fmt, ##__VA_ARGS__);                         \
	else                                                             \
		pr_debug("[MAX17050] " fmt, ##__VA_ARGS__);                        \
	} while (0)

#define trim_level(soc)	(soc)>100 ? 100 : ((soc)<0 ? 0 :( soc))

bool max17050_quick_start(int* new_ocv, int* new_soc);
struct max17050_chip {
	struct i2c_client *client;
	struct power_supply		*batt_psy;
	struct power_supply		ext_fg_psy;
	struct max17050_platform_data *pdata;
	struct work_struct work;
	struct mutex mutex;
	struct delayed_work max17050_post_init_work;
	struct delayed_work	max17050_model_data_write_work;
	struct delayed_work	max17050_monitor_work;
	struct delayed_work	max17050_dump_work;
	struct delayed_work temp_work;
	bool suspended;
	bool use_ext_temp;
	bool probe_done;
#ifdef MAX17050_CUTOFF_TRACKING
	int last_fake_soc;
	bool in_cutoff_tracking;
	bool force_cutoff_tracking;
	bool power_on_restore_done;
#endif
#ifdef CONFIG_LGE_PM_BATTERY_SWAP
	bool in_handling_swap;
#endif
	int prev_soc;
	int last_soc;

	int soc_rep;
	int soc_vf;
	int soc_rep_raw;
	int soc_vf_raw;
	int avg_ibat;
};

static struct max17050_chip *ref;
static struct i2c_client *max17050_i2c_client;

static unsigned int cable_smem_size;
int cell_info = 0;
#ifdef MAX17050_CUTOFF_TRACKING
static int por_state;
#endif

static int max17050_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "err %d\n", ret);

	return ret;
}

static int max17050_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "err %d\n", ret);

	return ret;
}

static int max17050_multi_write_data(struct i2c_client *client,
			int reg, const u8 *values, int length)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, length, values);

	if (ret < 0)
		dev_err(&client->dev, "err %d\n", ret);

	return ret;
}

static int max17050_multi_read_data(struct i2c_client *client,
			int reg, u8 *values, int length)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, length, values);

	if (ret < 0)
		dev_err(&client->dev, "err %d\n", ret);

	return ret;
}


bool max17050_i2c_write_and_verify(u8 addr, u16 value)
{
	u16 reg_val;

	max17050_write_reg(max17050_i2c_client, addr, value);
	msleep(4);
	reg_val = max17050_read_reg(max17050_i2c_client, addr);

	if (reg_val == value) {
		pr_max17050(PR_DEBUG, "%s() Addr = 0x%X,", __func__, addr);
		pr_max17050(PR_DEBUG, "%s() Value = 0x%X Success\n", __func__, value);
		return 1;
	}

	pr_max17050(PR_ERR, "%s : () Addr = 0x%X,", __func__, addr);
	pr_max17050(PR_ERR, "%s :  Value = 0x%X Fail to write.", __func__, value);
	pr_max17050(PR_ERR, "%s :  Write once more.\n", __func__);
	max17050_write_reg(max17050_i2c_client, addr, value);

	return 0;
}

int max17050_get_vbat_mv(void)
{
	u16 reg;
	int vbatt_mv;
	if (max17050_i2c_client == NULL) {
		pr_max17050(PR_ERR, "%s : i2c NULL vbatt = 800 mV\n", __func__);
		return FAIL_SAFE_VALUE;
	}
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_V_CELL);
	if (reg < 0)
		return FAIL_SAFE_VALUE;

	vbatt_mv = (reg >> 3);
	vbatt_mv = (vbatt_mv * 625) / 1000;

	pr_max17050(PR_DEBUG, "%s : vbatt = %d mV\n", __func__, vbatt_mv);

	return vbatt_mv;
}

int max17050_get_ocv_mv(void)
{
	u16 reg;
	int ocv_mv;
	if (max17050_i2c_client == NULL) {
		pr_max17050(PR_ERR, "%s : i2c NULL vbatt = 800 mV\n", __func__);
		return FAIL_SAFE_VALUE;
	}
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_V_FOCV);
	if (reg < 0)
		return FAIL_SAFE_VALUE;

	ocv_mv = (reg >> 4);
	ocv_mv = (ocv_mv * 125) / 100;

	pr_max17050(PR_DEBUG,"ocv = %d mV\n", ocv_mv);

	return ocv_mv;

}


bool  max17050_get_raw_soc(void)
{
	int reg_rep, reg_vf = 0;

	u8 upper_reg_rep, lower_reg_rep;
	u8 upper_reg_vf, lower_reg_vf;

	if (max17050_i2c_client == NULL)
		return false;

	reg_rep = max17050_read_reg(max17050_i2c_client,
		MAX17050_SOC_REP);

	if (reg_rep < 0) {
		pr_max17050(PR_ERR, "%s : reg_rep read fail \n",__func__);
		return false;
	}

	reg_vf = max17050_read_reg(max17050_i2c_client,
		MAX17050_SOC_VF);

	if (reg_vf < 0) {
		pr_max17050(PR_ERR, "%s : reg_vf read fail \n",__func__);
		return false;
	}

	upper_reg_rep = (reg_rep & 0xFF00) >> 8 ;
	lower_reg_rep = (reg_rep & 0xFF);

	pr_max17050(PR_DEBUG, "%s : SOC_REP : read_reg_rep = %X"
		"upper_reg_rep = %X lower_reg_rep = %X\n",
		__func__, reg_rep, upper_reg_rep, lower_reg_rep);

	upper_reg_vf = (reg_vf & 0xFF00) >> 8;
	lower_reg_vf = (reg_vf & 0xFF);

	pr_max17050(PR_DEBUG, "%s : SOC_VF : read_reg_vf = %X"
			"upper_reg_vf = %X lower_reg_vf = %X\n",
			__func__, reg_vf, upper_reg_vf, lower_reg_vf);

	ref->soc_rep_raw  = ((upper_reg_rep * 256) + lower_reg_rep) * 10 / 256;
	ref->soc_vf_raw = ((upper_reg_vf * 256) + lower_reg_vf) * 10 / 256;

	return true;
}

int max17050_get_soc(void)
{
	int soc_rep=0;
	int soc_vf = 0;
	int rc;
#ifdef CONFIG_LGE_PM_BATTERY_SWAP
	if(ref->in_handling_swap)
		return ref->last_soc;
#endif
	rc = max17050_get_raw_soc();
	if (!rc)
		return ref->last_soc;

	/* SOC scaling for stable max SOC and changed Cut-off */
	/* Adj SOC = (FG SOC - Emply) / (Full - Empty) * 100 */
	/* cut off vol 3.3V : (soc - 1.132%) * 100 / (94.28% - 1.132%) */
	/* full capacity SOC 106.5% , real battery SOC 100.7% */
	soc_rep = (ref->soc_rep_raw * 100) * 100;
	soc_rep = (soc_rep /
			(ref->pdata->rescale_soc)) - (ref->pdata->rescale_factor);
	/* 100 -> 105.8% scailing */

	if (cell_info == LGC_LLL) { /*LGC Battery*/
		if (8 <= soc_rep && soc_rep <= 20) {
			soc_rep = 20;
			pr_max17050(PR_DEBUG, "%s : cut off for LGC 2 per\n", __func__);
		}
		if (5 <= soc_rep && soc_rep < 8) {
			soc_rep = 10;
			pr_max17050(PR_DEBUG, "%s : cut off for LGC 1 per\n", __func__);
		}
	} else { /*Tocad battery*/
		if (3 <= soc_rep && soc_rep <= 20) {
			soc_rep = 20;
			pr_max17050(PR_DEBUG, "%s : cut off for tocad 2 per\n", __func__);
		}
		if (1 <= soc_rep && soc_rep < 3) {
			soc_rep = 10;
			pr_max17050(PR_DEBUG, "%s : cut off for tocad 1 per\n", __func__);
		}
	}

	soc_vf = (ref->soc_vf_raw  * 100) * 100;
	soc_vf = (soc_vf /
			(ref->pdata->rescale_soc)) - (ref->pdata->rescale_factor);
	/* 106.8% scailing */

	ref->soc_rep = soc_rep;
	ref->soc_vf = soc_vf;

	pr_max17050(PR_DEBUG, "%s rescale_soc %d,"
			"rescale_factor %d\n",__func__,
			ref->pdata->rescale_soc,ref->pdata->rescale_factor);

	pr_max17050(PR_DEBUG, "%s : After_rescailing SOC_REP  = %d : SOC_VF  = %d\n",
		__func__, soc_rep, soc_vf);

#ifdef CONFIG_LGE_PM_MAX17050_SOC_REP
	ref->last_soc = trim_level(soc_rep/10);
#else
	ref->last_soc = trim_level(soc_vf/10);
#endif

	return ref->last_soc;
}
int max17050_get_ibat(void)
{
	u16 reg;
	int ibat_ma;
	int avg_ibat_ma;
	u16 sign_bit;

	if (max17050_i2c_client == NULL) {
		pr_max17050(PR_ERR, "%s : i2c NULL", __func__);
		return FAIL_SAFE_VALUE;
	}
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_CURRENT);
	if (reg < 0)
		return FAIL_SAFE_VALUE;

	sign_bit = (reg & 0x8000)>>15;

	if (sign_bit == 1)
		ibat_ma = (15625 * (reg  - 65536))/100000;
	else
		ibat_ma = (15625 * reg) / 100000;
	ibat_ma *= -1;

	reg = max17050_read_reg(max17050_i2c_client,
		MAX17050_AVERAGE_CURRENT);
	if (reg < 0)
		return FAIL_SAFE_VALUE;/*Error Value return.*/

	sign_bit = (reg & 0x8000)>>15;

	if (sign_bit == 1)
		avg_ibat_ma = (15625 * (reg  - 65536)) / 100000;
	else
		avg_ibat_ma = (15625 * reg) / 100000;

	/* reverse (charging is negative by convention) */
	avg_ibat_ma *= -1;

	ref->avg_ibat = avg_ibat_ma;

	pr_max17050(PR_DEBUG, "%s : I_batt = %d mA avg_I_batt = %d mA\n",
		__func__, ibat_ma, avg_ibat_ma);

	return ibat_ma;
}

int max17050_get_batt_full_design(void)
{
	if (ref == NULL)
		return 2000;

	return ref->pdata->full_design;
}

#define DEFAULT_TEMP	25
int max17050_write_temp(void)
{
	int batt_temp;
	int batt_temp_raw;
	u16 temp_reg;

	union power_supply_propval val = {0,};

	if (!ref->use_ext_temp) {
		pr_max17050(PR_INFO, "%s : Not use batt temp"
				": defalult temp 25C\n", __func__);
		return  FAIL_SAFE_TEMP;
	}

	if (!ref->batt_psy)
		ref->batt_psy = power_supply_get_by_name("battery");

	if (!ref->batt_psy)
		return FAIL_SAFE_TEMP;

	ref->batt_psy->get_property(ref->batt_psy,
			POWER_SUPPLY_PROP_TEMP, &val);
	batt_temp_raw = val.intval;
	pr_max17050(PR_DEBUG, "%s : battery_temp from power_supply %d\n",
			__func__, batt_temp_raw);

	if(batt_temp_raw < 0)
		temp_reg = 0xFF00 & (u16)(batt_temp_raw * 256 / 10);
	else
		temp_reg = (u16)(batt_temp_raw * 256 / 10);

	max17050_write_reg(max17050_i2c_client,
				MAX17050_TEMPERATURE, temp_reg);

	/*At least 3mS of delay added between Write and Read functions*/
	msleep(4);

	temp_reg = max17050_read_reg(max17050_i2c_client,
			MAX17050_TEMPERATURE);
	pr_max17050(PR_DEBUG, "%s : battery_temp %X\n",
			__func__, temp_reg);
	batt_temp = (temp_reg * 10 / 256) / 10;
	if(batt_temp < 0)
		batt_temp = batt_temp_raw;

	pr_max17050(PR_DEBUG, "%s : battery_temp %d\n",
			__func__, batt_temp);
	return batt_temp;
}

int max17050_get_batt_age(void)
{
	u16 reg;
	int batt_age;

	if (max17050_i2c_client == NULL) {
		pr_max17050(PR_ERR, "%s : i2c NULL battery age: 800\n", __func__);
		return FAIL_SAFE_VALUE;
	}
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_AGE);
	if (reg < 0)
		return FAIL_SAFE_VALUE;

	batt_age = (reg >> 8);

	pr_max17050(PR_DEBUG, "%s : battery_age = %d\n", __func__, batt_age);

	return batt_age;
}

enum batt_condition{
	UNCALCULATED,
	VERY_GOOD,
	GOOD,
	BAD,
};
int max17050_get_batt_condition(void)
{
	int batt_age = max17050_get_batt_age();

	if (batt_age == FAIL_SAFE_VALUE)	return UNCALCULATED;
	if (batt_age >= 80)	return VERY_GOOD;
	if (batt_age >= 50)	return GOOD;
	if (batt_age >= 0)	return BAD;

	return UNCALCULATED;
}
#ifdef MAX17050_COMPENSATE_CAPACITY
#define REM_CAP_DECREASE_VAL 20
void max17050_compensate_capacity(void)
{
	u16 reg;
	int full_cap,rem_cap;

	reg = max17050_read_reg(max17050_i2c_client, MAX17050_FULL_CAP);
	full_cap = (5 * reg) / 10;
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_REM_CAP_REP);
	rem_cap= (5 * reg) / 10;

	if (full_cap > 3000) {
		pr_max17050(PR_ERR, "%s : recovery for full_cap(%d) > 3000"
			" , rem_cap(%d)\n",__func__, full_cap,rem_cap);

		max17050_i2c_write_and_verify(MAX17050_FULL_CAP,
				ref->pdata->capacity);
		max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP,
				ref->pdata->vf_fullcap);
		max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM,
				ref->pdata->vf_fullcap);

		reg = max17050_read_reg(max17050_i2c_client, MAX17050_FULL_CAP);
		full_cap = (5 * reg) / 10;

		if (rem_cap > full_cap) {
			max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP,
				ref->pdata->capacity);
			reg = max17050_read_reg(max17050_i2c_client, MAX17050_REM_CAP_REP);
			rem_cap = (5 * reg) / 10;
		} else {
			rem_cap = full_cap * (ref->soc_rep_raw)/ 1000;
			reg = (rem_cap * 10) / 5;
			max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP,
				reg);
		}

		pr_max17050(PR_ERR, "%s : rewrited! new full_cap(%d), rem_cap(%d)\n"
		,__func__, full_cap,rem_cap);

		return;
	}

	if (rem_cap > full_cap)	{
		pr_max17050(PR_ERR, "%s : recovery for rem_cap(%d) > full_cap(%d)"
			,__func__,rem_cap,full_cap);

		reg = max17050_read_reg(max17050_i2c_client, MAX17050_FULL_CAP);
		max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP,
				reg);
		reg = max17050_read_reg(max17050_i2c_client, MAX17050_REM_CAP_REP);
		rem_cap = (5 * reg) / 10;

		pr_max17050(PR_ERR, "%s : rewrited! new full_cap(%d), rem_cap(%d)\n"
		,__func__, full_cap,rem_cap);

		return;
	}

	pr_max17050(PR_DEBUG, "%s : no error in full_cap(%d), rem_cap(%d)\n"
		,__func__, full_cap,rem_cap);
	return;

}
#endif

#if defined (CONFIG_LGE_PM_BATTERY_SWAP) || defined(MAX17050_CUTOFF_TRACKING)
void max17050_recalculate_soc(int new_ocv)
{
	u16 reg;
	int vfocv;
	u16 vfsoc;
	u16 rem_cap;
	u16 rep_cap;
	u16 dQ_acc;
	u16 qh_reg;
	int ocv,soc;

	vfocv = (new_ocv * 100) / 125 ;
	vfocv = vfocv << 4;

	/* Set MiscCFG.VEX */
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg | 0x0004;
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, reg);
	msleep(175);
	/* Write VCell to previously saved VFOCV */
	max17050_write_reg(max17050_i2c_client, MAX17050_V_CELL, vfocv);
	/* Quick Start */
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg | 0x0400;
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, reg);
	msleep(350);
	/* Clear MiscCFG */
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg & 0xFBFB;
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, reg);

	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	pr_max17050(PR_ERR, "%s : Clear MiscCFG = 0x%X\n", __func__, reg);

	/*13. Delay at least 350mS*/
	msleep(350);

	/*14. Write VFSOC value to VFSOC 0 and QH0*/
	vfsoc = max17050_read_reg(max17050_i2c_client, MAX17050_SOC_VF);
	pr_max17050(PR_ERR, "%s : ()  vfsoc = 0x%X\n", __func__, vfsoc);
	max17050_write_reg(max17050_i2c_client, MAX17050_VFSOC0_LOCK, 0x0080);
	max17050_i2c_write_and_verify(MAX17050_VFSOC0, vfsoc);
	qh_reg = max17050_read_reg(max17050_i2c_client, MAX17050_QH);
	max17050_write_reg(max17050_i2c_client, MAX17050_QH0, qh_reg);
	max17050_write_reg(max17050_i2c_client, MAX17050_VFSOC0_LOCK, 0);

	/*15. Advance to Coulomb-Counter Mode */
	max17050_i2c_write_and_verify(MAX17050_CYCLES, 0x0060);

	/*16. Load New Capacity Parameters*/
	rem_cap = (vfsoc * ref->pdata->vf_fullcap) / 25600;
	pr_max17050(PR_ERR, "%s : ()  vf_full_cap = %d  = 0x%X\n",
			__func__, ref->pdata->vf_fullcap, ref->pdata->vf_fullcap);
	pr_max17050(PR_ERR, "%s : ()  rem_cap = %d  = 0x%X\n",
			__func__, rem_cap, rem_cap);
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_MIX, rem_cap);
	rep_cap = rem_cap;
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP, rep_cap);
	dQ_acc = (ref->pdata->capacity / 16);
	max17050_i2c_write_and_verify(MAX17050_D_PACC, 0x0C80);
	max17050_i2c_write_and_verify(MAX17050_D_QACC, dQ_acc);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP,
			ref->pdata->capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP,
			ref->pdata->vf_fullcap);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM,
			ref->pdata->vf_fullcap);
	max17050_write_reg(max17050_i2c_client, MAX17050_SOC_REP, vfsoc);

	/*17. Initialization Complete*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_STATUS);
	max17050_i2c_write_and_verify(MAX17050_STATUS, (reg & 0xFFFD));

	ocv = max17050_get_ocv_mv();
	soc = max17050_get_soc();

	pr_max17050(PR_ERR," final real_ocv = %d , soc = %d",ocv,soc);

	pr_max17050(PR_INFO, "%s : [CMP] End of the recalculate_soc.\n", __func__);

}
#endif

/*---------START-------- MAX17050_CUTOFF_TRACKING*/
#ifdef MAX17050_CUTOFF_TRACKING
extern void write_shutdown_soc(char *filename, int write_val, int pos);
extern int read_shutdown_soc(char *filename, int pos);
void max17050_battery_compare_soc(int soc, int backup_soc,
		int backup_vfocv, int backup_cell_type)
{
	int vfocv;
	int vfocv_1;
	int vfocv_2;
	int tolerance_hi;
	int tolerance_low;
	int fake_ocv;

	fake_ocv =  (cell_info == LGC_LLL) ? FAKE_OCV_LGC : FAKE_OCV_TCD;

	vfocv_1 = max17050_read_reg(max17050_i2c_client,0x14);
	vfocv_2 = max17050_read_reg(max17050_i2c_client,0x15);
	vfocv = vfocv_1 + (vfocv_2 << 8);
	tolerance_hi = max17050_read_reg(max17050_i2c_client,0x20);
	tolerance_low = tolerance_hi * (-1);

	pr_max17050(PR_INFO,"[CMP] soc=%d b_soc=%d "
		   "vfocv=%d b_vfocv=%d tol=%d,cell=%d,b_cell=%d\n"
			,soc, backup_soc, vfocv,backup_vfocv
			,tolerance_hi,cell_info,backup_cell_type);
	if ( por_state ){
		if(ref->in_cutoff_tracking) {
			ref->in_cutoff_tracking = 0;
			pr_max17050(PR_ERR, "%s : [CMP] comp fk_power_off(non-por)\n", __func__);
			max17050_recalculate_soc(fake_ocv);
			write_shutdown_soc("/persist/last_soc",ref->in_cutoff_tracking,12);
		}
		return;
	}

	if(backup_soc <=1)
		return;
	if(backup_cell_type != cell_info)
		return;
	if (abs(soc -backup_soc) <= 3)
		return;
	if ((vfocv-backup_vfocv)  > tolerance_hi  || (vfocv-backup_vfocv) < tolerance_low )
		return;
	if (ref->in_cutoff_tracking ) {
		ref->in_cutoff_tracking = 0;
		pr_max17050(PR_ERR, "%s : [CMP] comp fk_power_off(por).\n",
					__func__);
		max17050_recalculate_soc(fake_ocv);

		write_shutdown_soc("/persist/last_soc",ref->in_cutoff_tracking,12);
	} else {
		pr_max17050(PR_ERR, "%s : [CMP] removed/insert same battery.\n",
						__func__);
		max17050_recalculate_soc(backup_vfocv);

	}
	return;

}
void max17050_restore_soc_from_file(struct max17050_chip *chip)
{
	int soc_now;
	int backup_soc;
	int bakcup_vfocv_1,bakcup_vfocv_2;
	int backup_vfocv;
	int backup_cell_type;

	soc_now = max17050_get_soc();
	backup_soc = read_shutdown_soc("/persist/last_soc",0);
	bakcup_vfocv_1 = read_shutdown_soc("/persist/last_soc",3);
	bakcup_vfocv_2= read_shutdown_soc("/persist/last_soc",6);
	backup_vfocv = bakcup_vfocv_1 + (bakcup_vfocv_2 << 8);
	backup_cell_type= read_shutdown_soc("/persist/last_soc",9);
	chip->in_cutoff_tracking= read_shutdown_soc("/persist/last_soc",12);

	max17050_battery_compare_soc(soc_now, backup_soc,
			backup_vfocv, backup_cell_type);

	chip->power_on_restore_done= true;

}
void max17050_backup_soc_to_file(void) {

	int soc;
	int ocv;
	int vfocv_1,vfocv_2;

	soc = max17050_get_soc();
	ocv = max17050_get_ocv_mv();

	write_shutdown_soc("/persist/last_soc",soc,0);
	vfocv_1 = ocv & 0xFF;
	write_shutdown_soc("/persist/last_soc",vfocv_1,3);
	vfocv_2 = (ocv >> 8) & 0xFF;
	write_shutdown_soc("/persist/last_soc",vfocv_2,6);
	write_shutdown_soc("/persist/last_soc",cell_info,9);
}

static void max17050_lge_cutoff_tracking(void) {
	int vbat;

	if(!ref->in_cutoff_tracking ) {
		ref->last_fake_soc = max17050_get_soc();
		pr_max17050(PR_ERR, "%s : POLLING_FAKE_SOC :"
				"fist read real soc = %d\n",
				__func__, ref->last_fake_soc);
		ref->in_cutoff_tracking =true;
		write_shutdown_soc("/persist/last_soc",ref->in_cutoff_tracking,12);
		return;
	}

	ref->last_fake_soc -= 1;

	vbat = max17050_get_vbat_mv();
	if (vbat <= 3000)  {
		ref->force_cutoff_tracking= 1;
		ref->last_fake_soc = 0;
		pr_max17050(PR_ERR, "%s : Force shutdown "
				"at low voltage", __func__);
	}

	if (ref->last_fake_soc <= 2) {
		ref->force_cutoff_tracking = 1;
		if (ref->last_fake_soc  <= 0)
			ref->last_fake_soc= 0;
	}

	power_supply_changed(ref->batt_psy);

	pr_max17050(PR_ERR, "%s : POLLING_FAKE_SOC_FINAL= %d  \n"
				,__func__, ref->last_fake_soc );
}
#endif
/*---------END-------- MAX17050_CUTOFF_TRACKING*/

void max17050_battery_dump_print(void)
{
	u16 reg;
	int i;
	printk("Register Dump:");
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK1, 0x59);
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK2, 0xc4);
	for (i = 0; i <= 0xFF; i++) {
		if(i == 0x50)
			i = 0xE0;
		reg = max17050_read_reg(max17050_i2c_client, i);
		printk("%04xh,", reg);
	}
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK1, 0x0000);
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK2, 0x0000);
	printk("\n");
}
void max17050_print_all_batt_info(void)
{
	u16 reg;
	u16 learn_cfg, rcomp;
	int age,batt_tte_sec;
	int soc,vbat,ibat,ocv,temp;
	int full_cap,rem_cap;
	int fake_on;

	learn_cfg = max17050_read_reg(max17050_i2c_client, MAX17050_LEARN_CFG);
	rcomp = max17050_read_reg(max17050_i2c_client, MAX17050_RCOMP_0);

	age =   max17050_get_batt_age();
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_TTE);
	batt_tte_sec = (5625 * reg) / 1000;

	soc = max17050_get_soc();
	vbat = max17050_get_vbat_mv();
	ibat = max17050_get_ibat();
	ocv = max17050_get_ocv_mv();
	temp = max17050_write_temp();
	if (temp  > 127)
		temp -=256;

	reg = max17050_read_reg(max17050_i2c_client, MAX17050_FULL_CAP);
	full_cap = (5 * reg) / 10;
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_REM_CAP_REP);
	rem_cap= (5 * reg) / 10;

	pr_max17050(PR_INFO, "[MAX17050_PRINT_INFO]=F_cap=%d,recap=%d,Ibatt=%d,Iavg=%d,"
			"Empty=%d,Vbatt=%d,OCV=%d,SoC=%d,AfterSoCRep=%d,Raw=%d,AfterSocVF=%d,"
			"Raw=%d,Rcomp0=0x%X,temp=%d,learn=0x%X fake=%d\n",
			full_cap,rem_cap,ibat,ref->avg_ibat,
			batt_tte_sec/60,vbat,ocv,soc,ref->soc_rep, ref->soc_rep_raw,
			ref->soc_vf,ref->soc_vf_raw,rcomp,temp,learn_cfg,fake_on);
}

static void max17050_monitor_work(struct work_struct *work)
{
	struct max17050_chip *chip = container_of(work,
				struct max17050_chip,
				max17050_monitor_work.work);

#ifdef MAX17050_CUTOFF_TRACKING
	int vbat, ibat;
#endif

	max17050_print_all_batt_info();

#ifdef MAX17050_COMPENSATE_CAPACITY
	max17050_compensate_capacity();
#endif

#ifdef MAX17050_CUTOFF_TRACKING
	if (ref->power_on_restore_done)  {
		max17050_backup_soc_to_file();
		vbat = max17050_get_vbat_mv();
		ibat = max17050_get_ibat();
		if((vbat <= 3270 && ibat > 0) || ref->force_cutoff_tracking )  {
			max17050_lge_cutoff_tracking();
			pr_max17050(PR_DEBUG, "%s : POLLING_PERIOD_5_FAKE_SOC\n", __func__);
			schedule_delayed_work(&chip->max17050_monitor_work,
				msecs_to_jiffies(MAX17050_POLLING_PERIOD_5));
			return;
		}
		if (ref->in_cutoff_tracking)  {
			ref->in_cutoff_tracking  = 0;
			write_shutdown_soc("/persist/last_soc",ref->in_cutoff_tracking ,12);
		}
	}
	pr_max17050(PR_DEBUG, "%s : POLLING_NORMAL_WORK\n", __func__);
#endif

	pr_max17050(PR_DEBUG, "%s : prev_soc:%d, last_soc:%d\n",
			__func__, chip->prev_soc, chip->last_soc);

	if (chip->prev_soc != chip->last_soc) {
		pr_max17050(PR_ERR, "%s : Update PSY %d->%d\n"
					, __func__,chip->prev_soc,chip->last_soc);
		power_supply_changed(chip->batt_psy);
		chip->prev_soc = chip->last_soc;
	}

	if (chip->last_soc <= 15) {
		pr_max17050(PR_DEBUG, "%s : POLLING_PERIOD_5\n", __func__);
		schedule_delayed_work(&chip->max17050_monitor_work,
				msecs_to_jiffies(MAX17050_POLLING_PERIOD_5));
	} else {
		pr_max17050(PR_DEBUG, "%s : POLLING_PERIOD_20\n", __func__);
		schedule_delayed_work(&chip->max17050_monitor_work,
				msecs_to_jiffies(MAX17050_POLLING_PERIOD_20));
	}
	return;
}

#define DUMP_PRINT_TIME 40000
static void max17050_dump_work(struct work_struct *work)
{
	struct max17050_chip *chip = container_of(work,
				struct max17050_chip,
				max17050_dump_work.work);

	max17050_battery_dump_print();

	schedule_delayed_work(&chip->max17050_monitor_work,
			msecs_to_jiffies(DUMP_PRINT_TIME));

}

static int max17050_new_custom_model_write(void)
{
	/*u16 ret;*/
	u16 reg;

	/*u16 write_reg;*/
	u16 vfsoc;
	u16 rem_cap;
	u16 rep_cap;
	u16 dQ_acc;
	u16 qh_register;
	u16 i;

	u8 read_custom_model_80[MODEL_SIZE];
	u8 read_custom_model_90[MODEL_SIZE];
	u8 read_custom_model_A0[MODEL_SIZE];

	pr_max17050(PR_INFO, "%s : Model_data Start\n", __func__);

	/*1. Delay 500mS*/
	//msleep(500);

	/*1.1 Version Check*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_VERSION);
	pr_max17050(PR_INFO, "%s : MAX17050_VERSION = 0x%X\n", __func__, reg);
	if (reg != 0xAC) {
		pr_max17050(PR_ERR, "%s : Version Check Error.", __func__);
		pr_max17050(PR_ERR, "%s :  Version Check = 0x%x\n", __func__, reg);
		return 1; /*Version Check Error*/
	}

	/*2. Initialize Configuration*/
	/* External temp and enable alert function */
	max17050_write_reg(max17050_i2c_client, MAX17050_CONFIG,
						ref->pdata->config);
	max17050_write_reg(max17050_i2c_client, MAX17050_FILTER_CFG,
						ref->pdata->filtercfg);
	max17050_write_reg(max17050_i2c_client, MAX17050_RELAX_CFG,
						ref->pdata->relaxcfg);
	max17050_write_reg(max17050_i2c_client, MAX17050_LEARN_CFG,
						ref->pdata->learncfg);
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG,
						ref->pdata->misccfg);
	max17050_write_reg(max17050_i2c_client, MAX17050_FULL_SOC_THR,
						ref->pdata->fullsocthr);
	max17050_write_reg(max17050_i2c_client, MAX17050_I_AVG_EMPTY,
						ref->pdata->iavg_empty);

	/*4. Unlock Model Access*/
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK1, 0x59);
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK2, 0xc4);

	/*5. Write/Read/Verify the Custom Model*/
	max17050_multi_write_data(max17050_i2c_client,
				MAX17050_MODEL_TABLE_80, ref->pdata->model_80, MODEL_SIZE);
	max17050_multi_write_data(max17050_i2c_client,
				MAX17050_MODEL_TABLE_90, ref->pdata->model_90, MODEL_SIZE);
	max17050_multi_write_data(max17050_i2c_client,
				MAX17050_MODEL_TABLE_A0, ref->pdata->model_A0, MODEL_SIZE);

	/*For test only. Read back written-custom model data.*/
	max17050_multi_read_data(max17050_i2c_client,
				MAX17050_MODEL_TABLE_80, read_custom_model_80, MODEL_SIZE);
	max17050_multi_read_data(max17050_i2c_client,
				MAX17050_MODEL_TABLE_90, read_custom_model_90, MODEL_SIZE);
	max17050_multi_read_data(max17050_i2c_client,
				MAX17050_MODEL_TABLE_A0, read_custom_model_A0, MODEL_SIZE);

	/*Print read_custom_model print */
	for (i = 0; i < MODEL_SIZE; i++)
		pr_max17050(PR_DEBUG, "%s : Model_data_80 %d = 0x%x\n",
				__func__, i, read_custom_model_80[i]);

	for (i = 0; i < MODEL_SIZE; i++)
		pr_max17050(PR_DEBUG, "%s : Model_data_90 %d = 0x%x\n",
				__func__, i, read_custom_model_90[i]);

	for (i = 0; i < MODEL_SIZE; i++)
		pr_max17050(PR_DEBUG, "%s : Model_data_A0 %d = 0x%x\n",
				__func__, i, read_custom_model_A0[i]);

	/*Compare with original one.*/
	for (i = 0 ; i < MODEL_SIZE ; i++) {
		if (read_custom_model_80[i] != ref->pdata->model_80[i]) {
			pr_max17050(PR_INFO, "%s : [MAX17050] Custom Model",
					__func__);
			pr_max17050(PR_INFO, "%s :  1[%d]	Write Error\n",
					__func__, i);
		}
	}

	for (i = 0 ; i < MODEL_SIZE ; i++) {
		if (read_custom_model_90[i] != ref->pdata->model_90[i]) {
			pr_max17050(PR_INFO, "%s : [MAX17050] Custom Model", __func__);
			pr_max17050(PR_INFO, "%s :  2[%d] Write Error\n", __func__, i);
		}
	}

	for (i = 0 ; i < MODEL_SIZE ; i++) {
		if (read_custom_model_A0[i] != ref->pdata->model_A0[i]) {
			pr_max17050(PR_INFO, "%s : [MAX17050] Custom Model", __func__);
			pr_max17050(PR_INFO, "%s :  3[%d] Write Error\n", __func__, i);
		}
	}

	/*8. Lock Model Access*/
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK1, 0x0000);
	max17050_write_reg(max17050_i2c_client, MAX17050_MODEL_LOCK2, 0x0000);

	/*10. Write Custom Parameters*/
	max17050_i2c_write_and_verify(MAX17050_RCOMP_0,
					ref->pdata->rcomp0);
	max17050_i2c_write_and_verify(MAX17050_TEMP_CO,
						ref->pdata->tempco);
	max17050_i2c_write_and_verify(MAX17050_TEMP_NOM,
						ref->pdata->tempnom);
	max17050_i2c_write_and_verify(MAX17050_I_CHG_TERM,
						ref->pdata->ichgterm);
	max17050_i2c_write_and_verify(MAX17050_T_GAIN,
						ref->pdata->tgain);
	max17050_i2c_write_and_verify(MAX17050_T_OFF,
						ref->pdata->toff);
	max17050_i2c_write_and_verify(MAX17050_V_EMPTY,
						ref->pdata->vempty);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_00,
						ref->pdata->qrtable00);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_10,
						ref->pdata->qrtable10);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_20,
						ref->pdata->qrtable20);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_30,
						ref->pdata->qrtable30);

	/*11. Update Full Capacity Parameters*/
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP, 0x0);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP,
						ref->pdata->capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP,
						ref->pdata->vf_fullcap);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM,
						ref->pdata->vf_fullcap);

	/*13. Delay at least 350mS*/
	msleep(350);

	/*14. Write VFSOC value to VFSOC 0 and QH0*/
	vfsoc = max17050_read_reg(max17050_i2c_client, MAX17050_SOC_VF);
	pr_max17050(PR_ERR, "%s : ()  vfsoc = 0x%X\n", __func__, vfsoc);
	max17050_write_reg(max17050_i2c_client, MAX17050_VFSOC0_LOCK, 0x0080);
	max17050_i2c_write_and_verify(MAX17050_VFSOC0, vfsoc);
	qh_register = max17050_read_reg(max17050_i2c_client, MAX17050_QH);
	max17050_write_reg(max17050_i2c_client, MAX17050_QH0, qh_register);
	max17050_write_reg(max17050_i2c_client, MAX17050_VFSOC0_LOCK, 0);

	/*15. Advance to Coulomb-Counter Mode */
	max17050_i2c_write_and_verify(MAX17050_CYCLES, 0x0060);

	/*16. Load New Capacity Parameters*/
	rem_cap = (vfsoc * ref->pdata->vf_fullcap) / 25600;
	pr_max17050(PR_ERR, "%s : ()  vf_full_cap = %d  = 0x%X\n",
		__func__, ref->pdata->vf_fullcap, ref->pdata->vf_fullcap);
	pr_max17050(PR_ERR, "%s : ()  rem_cap = %d  = 0x%X\n",
		__func__, rem_cap, rem_cap);
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_MIX, rem_cap);
	rep_cap = rem_cap;
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP, rep_cap);
	dQ_acc = (ref->pdata->capacity / 16);
	max17050_i2c_write_and_verify(MAX17050_D_PACC, 0x0C80);
	max17050_i2c_write_and_verify(MAX17050_D_QACC, dQ_acc);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP,
						ref->pdata->capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP,
							ref->pdata->vf_fullcap);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM,
						ref->pdata->vf_fullcap);
	max17050_write_reg(max17050_i2c_client, MAX17050_SOC_REP, vfsoc);

	/*17. Initialization Complete*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_STATUS);
	max17050_i2c_write_and_verify(MAX17050_STATUS, (reg & 0xFFFD));

	pr_max17050(PR_INFO, "%s : Model_data End\n", __func__);
	return 0; /*Success to write.*/
}


enum pon_init_action_type {
NO_NEED_TO_WR_MODEL_DATA,
NEED_TO_WR_MODEL_DATA,
NEED_TO_WR_MODEL_DATA_N_QUICK_START,
};

#define INITIALIZED_REG_AT_LK_FOR_QS	0x33
#define INITIALIZED_REG_AT_LK_FOR_RESET_TYPE	0x34

static int max17050_check_pon_status(void)
{
	/*0. Check for POR or Battery Insertion*/
	u8 need_qs, is_hard_reset;
	u16 misc_reg, por_bit;

	need_qs = max17050_read_reg(max17050_i2c_client
					, INITIALIZED_REG_AT_LK_FOR_QS);
	is_hard_reset = max17050_read_reg(max17050_i2c_client
					, INITIALIZED_REG_AT_LK_FOR_RESET_TYPE);
	misc_reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	por_bit = max17050_read_reg(max17050_i2c_client, MAX17050_STATUS);
	por_bit= por_bit & (STATUS_POR_BIT | STATUS_BI_BIT);

	pr_max17050(PR_INFO, "%s :[CMP] need_qs = %d, hard_reset = %d,"
		"MISC_CFG = 0x%X POR = 0x%X\n", __func__, need_qs,
		is_hard_reset,misc_reg,por_bit);

	if ( misc_reg == 0x810 ) {
		if (is_hard_reset == 0 || (por_bit == 0 && need_qs == 0)) {
#ifdef MAX17050_CUTOFF_TRACKING
			por_state = 1;
#endif
			pr_max17050(PR_INFO, "%s : IC Non-Power-On-Reset state.", __func__);
			return NO_NEED_TO_WR_MODEL_DATA;
		}
	}

	if (por_bit== 0 && need_qs) {
#ifdef MAX17050_CUTOFF_TRACKING
		por_state = 1;	/*battery was not removed*/
#endif
		pr_max17050(PR_INFO, "%s :[CMP]IC Non-Power-On-Reset state."
				" But recal trigger, Start Custom Model Write.\n", __func__);
		return NEED_TO_WR_MODEL_DATA_N_QUICK_START;
	}
	pr_max17050(PR_INFO, "%s : IC Power-On-Reset state."
		" Start Custom Model Write.\n", __func__);
	return NEED_TO_WR_MODEL_DATA;
}

static void max17050_model_data_write_work(struct work_struct *work)
{
	int ret = 0;
	int result = 0;
	int vbat,soc;
	result = max17050_check_pon_status();

	if (result == NO_NEED_TO_WR_MODEL_DATA) {
		pr_max17050(PR_INFO, "%s : battery was NOT removed! Do not wirte model_data .\n", __func__);
	}  else {
		ret = max17050_new_custom_model_write();
		if( ret )  {
			pr_max17050(PR_INFO, "%s : model_data write fail, RETRY...\n", __func__);
			max17050_new_custom_model_write();
		} else {
			pr_max17050(PR_INFO, "%s : model_data wirte success \n", __func__);
		}

		if ( NEED_TO_WR_MODEL_DATA_N_QUICK_START ) {
			pr_max17050(PR_INFO, "%s : recalibration start! \n",
					__func__);
			max17050_quick_start(&vbat,&soc);
			pr_max17050(PR_INFO, "%s : new vbat = %d, new soc =%d \n",
					__func__,vbat,soc);
		}
	}

	/*Check to enable external battery temperature from CONFIG*/
	ref->use_ext_temp = (ref->pdata->config & CFG_EXT_TEMP_BIT);
	pr_max17050(PR_INFO, "%s : use_ext_temp = %d\n",
			__func__, ref->use_ext_temp);

#ifdef CONFIG_MACH_MSM8992_PPLUS_KR
	/*dQ_acc and dP_acc rewrite for drift current*/
	max17050_i2c_write_and_verify(MAX17050_D_QACC, 0x05AA);
	max17050_i2c_write_and_verify(MAX17050_D_PACC, 0x3200);
	pr_max17050(PR_INFO, "%s : D_QACC and D_PACC reset\n", __func__);

	/*Apply MAXIM new Implementation - MISC_CFG value = 0x0810 */
	max17050_i2c_write_and_verify(MAX17050_MISC_CFG, 0x0810);
	pr_max17050(PR_INFO, "%s : MISC_CFG initial value changed\n", __func__);

	/*Apply MAXIM new Implementation - Filter_CFG value = 0xCDA4 */
	max17050_i2c_write_and_verify(MAX17050_FILTER_CFG, 0xCDA4);
	pr_max17050(PR_INFO, "%s : FILTER_CFG initial value changed\n", __func__);

	/*Apply MAXIM new Implementation - Relax_CFG value = 0x000F */
	max17050_i2c_write_and_verify(MAX17050_RELAX_CFG, 0x000F);
	pr_max17050(PR_INFO, "%s : RELAX_CFG initial value changed\n", __func__);
#endif
}

static void control_charging(bool enable)
{
	union power_supply_propval val = {0,};
	val.intval = enable;
	ref->batt_psy->set_property(ref->batt_psy, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
}

#define MAX17050_QUICKSTART_VERIFY_CNT 2
bool max17050_quick_start_native(void)
{
	u16 reg;
	int retry_cnt = 0;

QUICK_STEP1:
	/*1. Set the QuickStart and Verify bits*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg | 0x1400;
#ifdef CONFIG_MACH_MSM8992_PPLUS_KR
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, 0x810);
#else
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, reg);
#endif
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	pr_max17050(PR_ERR, "%s : VFRemCap MiscCFG = 0x%X\n", __func__, reg);

	/*2. Verify no memory leaks during Quickstart writing*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg & 0x1000;
	if (reg != 0x1000) {
		if (retry_cnt <= MAX17050_QUICKSTART_VERIFY_CNT) {
			pr_max17050(PR_ERR, "%s : quick_start error STEP2 retry:%d\n",
					__func__, ++retry_cnt);
			goto QUICK_STEP1;
		} else {
		pr_max17050(PR_ERR, "%s : quick_start error !!!!\n",
					__func__);
		return 1;
		}
	}
	retry_cnt = 0;

QUICK_STEP3:
	/*3. Clean the Verify bit*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg & 0xefff;
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, reg);

	/*4. Verify no memory leaks during Verify bit clearing*/
	reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	reg = reg & 0x1000;
	if (reg != 0x0000) {
		if (retry_cnt <= MAX17050_QUICKSTART_VERIFY_CNT) {
			pr_max17050(PR_ERR, "%s : quick_start error STEP4 retry:%d\n",
					__func__, ++retry_cnt);
			goto QUICK_STEP3;
		} else {
		pr_max17050(PR_ERR, "%s :  [MAX17050] quick_start error !!!!\n",
					__func__);
		return 1;
		}
	}
	/*5. Delay 500ms*/
	msleep(500);

	/*6. Writing and Verify FullCAP Register Value*/
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP,
						ref->pdata->capacity);
	return 0;


}
bool max17050_quick_start(int* new_ocv, int* new_soc)
{	
	pr_max17050(PR_INFO, "%s : start\n", __func__);

	control_charging(0);
	msleep(200);

	if(new_ocv!=NULL)
		*new_ocv = max17050_get_vbat_mv();

	if (max17050_quick_start_native()){
		control_charging(1);
		return 1;
	}

	if(new_soc!=NULL) {
		msleep(500);
		*new_soc = max17050_get_soc();
	}

	control_charging(1);
	return 0;
}
static ssize_t at_fuel_guage_reset_show
	(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool fg_reset_result = 0;
	int soc = 0;
	int vbat = 0;

	pr_max17050(PR_INFO, "%s : [AT_CMD] start\n", __func__);

	fg_reset_result = max17050_quick_start(&vbat,&soc);

	pr_max17050(PR_ERR, "%s : Reset_SOC = %d %% vbat = %d mV\n",
				__func__, soc, vbat);

	r= snprintf(buf, PAGE_SIZE, "%d\n", fg_reset_result?false:true);
	return r;
}

static ssize_t at_fuel_guage_level_show
	(struct device *dev, struct device_attribute *attr, char *buf)
{
	int soc = 0;

	pr_max17050(PR_INFO, "%s :  [AT_CMD] start \n", __func__);

	max17050_quick_start(NULL,&soc);

	pr_max17050(PR_ERR, "%s :  [AT_CMD] BATT soc = %d  real_soc = %d\n"
			,__func__,soc, ref->soc_rep_raw);

	return snprintf(buf, PAGE_SIZE, "%d\n", soc);
}

static ssize_t at_batt_level_show
	(struct device *dev, struct device_attribute *attr, char *buf)
{
	int batt_level = 0;

	pr_max17050(PR_INFO, "%s :  [AT_CMD] start \n", __func__);

	max17050_quick_start(&batt_level,NULL);

	pr_max17050(PR_INFO, "%s : [AT_CMD] BATT LVL = %d\n"
			,__func__, batt_level);

	return snprintf(buf, PAGE_SIZE, "%d\n", batt_level);
}

DEVICE_ATTR(at_fuelrst, 0444, at_fuel_guage_reset_show, NULL);
DEVICE_ATTR(at_fuelval, 0444, at_fuel_guage_level_show, NULL);
DEVICE_ATTR(at_batl, 0444, at_batt_level_show, NULL);

static enum power_supply_property max17050_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_BATTERY_CONDITION,
	POWER_SUPPLY_PROP_BATTERY_AGE,
};

static int max17050_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17050_get_vbat_mv();
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
#ifdef MAX17050_CUTOFF_TRACKING
		if (ref->in_cutoff_tracking)
			val->intval = ref->last_fake_soc;
		else
#endif
		val->intval = max17050_get_soc();
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max17050_get_ibat();
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = max17050_get_batt_full_design();
		break;
	case POWER_SUPPLY_PROP_BATTERY_CONDITION:
		val->intval = max17050_get_batt_condition();
		break;
	case POWER_SUPPLY_PROP_BATTERY_AGE:
		val->intval = max17050_get_batt_age();
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int get_property_from_max17050(enum power_supply_property prop)
{
	union power_supply_propval ret = {0, };

	if ( ref ) {
		if (ref->probe_done) {
			ref->ext_fg_psy.get_property(&(ref->ext_fg_psy), prop, &ret);
			return ret.intval;
		}
	}

	if (prop == POWER_SUPPLY_PROP_CAPACITY) {
		pr_max17050(PR_ERR,"%s : report default soc %d%%\n "
				,__func__,FAIL_SAFE_SOC);
		return FAIL_SAFE_SOC;
	  } else {
		return FAIL_SAFE_VALUE;
	}
}
EXPORT_SYMBOL(get_property_from_max17050);

#ifdef CONFIG_LGE_PM_BATTERY_SWAP

#define NUM_SAMPLE_VBAT 	2
int v_new_bat[NUM_SAMPLE_VBAT];
static void max17050_set_interrupt(struct max17050_chip *chip);
void gauge_handle_batt_removal(void)
{

	pr_max17050(PR_ERR,"%s \n ",__func__);
	ref->in_handling_swap =true;

	/* Cancel any pending work, if necessary */
	cancel_delayed_work_sync(&ref->max17050_monitor_work);
	cancel_delayed_work_sync(&ref->max17050_dump_work);

	if (ref->client->irq) {
		disable_irq(ref->client->irq);
		disable_irq_wake(ref->client->irq);
	}

}
EXPORT_SYMBOL(gauge_handle_batt_removal);
void  gauge_detect_vbat_in_no_load(void)
{
	int vbat, cnt;

	control_charging(0);
	msleep(2000);

	for (cnt = 0;  cnt <NUM_SAMPLE_VBAT ; cnt ++)
	{
		vbat = max17050_get_vbat_mv();
		if ( vbat == FAIL_SAFE_VALUE) {
			pr_max17050(PR_ERR,"%s :  fail to get vbat \n ",__func__);
		}
		v_new_bat[cnt] = vbat;
	}

	control_charging(1);
	return ;
}
EXPORT_SYMBOL(gauge_detect_vbat_in_no_load);
void gauge_start_calculation (void) {

	int ocv;
	int v_new_bat_avg =0;
	int cnt;
	int ret = 1;

	pr_max17050(PR_ERR,"%s : --- START ---- \n",__func__);

	while (ret) {
		ret = max17050_new_custom_model_write();
		if (ret)
			pr_max17050(PR_ERR,"%s : fail to write model data",__func__);
	}

	ocv = max17050_get_ocv_mv();

	for (cnt = 0;  cnt <NUM_SAMPLE_VBAT ; cnt ++) {
		pr_max17050(PR_ERR," %s : vbat[%d] = %d \n"
			,__func__,cnt,v_new_bat[cnt]);
		if (v_new_bat[cnt] == FAIL_SAFE_VALUE)
			v_new_bat_avg+=ocv;
		else
			v_new_bat_avg+=v_new_bat[cnt];
	}

	v_new_bat_avg  = v_new_bat_avg/NUM_SAMPLE_VBAT;

	pr_max17050(PR_ERR," %s : ocv = %d, vbat(real_ocv) = %d diff = %d"
			,__func__,ocv,v_new_bat_avg,(int)abs (v_new_bat_avg - ocv));

	if (abs (v_new_bat_avg - ocv) >= 10)
		max17050_recalculate_soc(v_new_bat_avg);

	ref->in_handling_swap =false;

	schedule_delayed_work(&ref->max17050_monitor_work,0);
	schedule_delayed_work(&ref->max17050_dump_work, 0);

	max17050_set_interrupt(ref);
	if (ref->client->irq) {
		enable_irq(ref->client->irq);
		enable_irq_wake(ref->client->irq);
	}

	pr_max17050(PR_ERR,"%s : --- END ---- \n",__func__);
	return;
}
EXPORT_SYMBOL(gauge_start_calculation);
#endif

static void max17050_set_soc_thresholds(struct max17050_chip *chip,
								s16 threshold)
{
	s16 soc_now;
	s16 soc_max;
	s16 soc_min;

	soc_now = max17050_read_reg(chip->client, MAX17050_SOC_REP) >> 8;

	pr_max17050(PR_INFO, "%s : soc_now %d\n", __func__, soc_now);

	soc_max = soc_now + threshold;
	if (soc_max > 100)
		soc_max = 100;
	soc_min = soc_now - threshold;
	if (soc_min < 0)
		soc_min = 0;

	max17050_write_reg(chip->client, MAX17050_SOC_ALRT_THRESHOLD,
		(u16)soc_min | ((u16)soc_max << 8));
}

static irqreturn_t max17050_irq_handler(int id, void *dev)
{
	struct max17050_chip *chip = dev;
	struct i2c_client *client = chip->client;
	u16 val;

	pr_max17050(PR_INFO, "%s : Interrupt occured, ID = %d\n", __func__, id);

	val = max17050_read_reg(client, MAX17050_STATUS);

	/* Signal userspace when the capacity exceeds the limits */
	if ((val & STATUS_INTR_SOCMIN_BIT) || (val & STATUS_INTR_SOCMAX_BIT)) {
		/* Clear interrupt status bits */
		max17050_write_reg(client, MAX17050_STATUS, val &
			~(STATUS_INTR_SOCMIN_BIT | STATUS_INTR_SOCMAX_BIT));

		/* Reset capacity thresholds */
		max17050_set_soc_thresholds(chip, 5);

		power_supply_changed(chip->batt_psy);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
void max17050_check_factory_cable(void)
{
	int cable_type = 0;

	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size, 0, 0));

	if (p_cable_type)
		cable_type = *p_cable_type;

	if (cable_type == LT_CABLE_56K  ||
		cable_type == LT_CABLE_130K ||
		cable_type == LT_CABLE_910K)
	{
		max17050_quick_start(NULL,NULL);
		pr_max17050(PR_INFO, "%s : cable_type is = %d"
			" factory_mode quick start \n", __func__, cable_type);
	}

}
#endif
static void max17050_post_init_work(struct work_struct *work)
{
#ifdef MAX17050_CUTOFF_TRACKING
	struct max17050_chip *chip = container_of(work,
				struct max17050_chip,
				max17050_post_init_work.work);

	max17050_restore_soc_from_file(chip);
#endif
#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	max17050_check_factory_cable();
#endif
}
static void max17050_set_interrupt(struct max17050_chip *chip)
{

	struct i2c_client *client = chip->client;
	int val;

	if (client->irq) {
		max17050_set_soc_thresholds(chip, 5);

		val = max17050_read_reg(client, MAX17050_CONFIG);

		max17050_write_reg(client, MAX17050_CONFIG,
						val | CFG_ALRT_BIT_ENBL);
		pr_max17050(PR_INFO, "%s : MAX17050_CONFIG_val after write = 0x%X\n",
				__func__, val);
	}

}

static void max17050_init_work(struct work_struct *work)
{
	struct max17050_chip *chip = container_of(work,
				struct max17050_chip, work);

	max17050_set_interrupt(chip);
}

static int max17050_parse_dt(struct device *dev,
		struct max17050_platform_data *pdata)
{
	struct device_node *dev_node = dev->of_node;
	int i;
	int rc = 0;

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	/*Battery ID */
	pr_max17050(PR_INFO, "%s : <Battery ID> cell_info n = %d\n",
			__func__, cell_info);
#endif

	rc = of_property_read_u32(dev_node, "max17050,rsens-microohm",
			&pdata->r_sns);

	if (!pdata->r_sns)
		pdata->enable_current_sense = true;
	else
		pdata->r_sns = MAX17050_DEFAULT_SNS_RESISTOR;

	/* Load Battery cell*/
	if (cell_info == LGC_LLL) { /*LGC Battery*/
		rc = of_property_read_u8_array(dev_node, "max17050,model_80_l",
				pdata->model_80, MODEL_SIZE);

		rc = of_property_read_u8_array(dev_node, "max17050,model_90_l",
				pdata->model_90, MODEL_SIZE);

		rc = of_property_read_u8_array(dev_node, "max17050,model_A0_l",
				pdata->model_A0, MODEL_SIZE);

		rc = of_property_read_u32(dev_node, "max17050,rcomp0_l",
				&pdata->rcomp0);

		rc = of_property_read_u32(dev_node, "max17050,tempco_l",
				&pdata->tempco);

		rc = of_property_read_u32(dev_node, "max17050,ichgterm_l",
				&pdata->ichgterm);

		rc = of_property_read_u32(dev_node, "max17050,vempty_l",
				&pdata->vempty);

		rc = of_property_read_u32(dev_node, "max17050,qrtable00_l",
				&pdata->qrtable00);

		rc = of_property_read_u32(dev_node, "max17050,qrtable10_l",
				&pdata->qrtable10);

		rc = of_property_read_u32(dev_node, "max17050,qrtable20_l",
				&pdata->qrtable20);

		rc = of_property_read_u32(dev_node, "max17050,qrtable30_l",
				&pdata->qrtable30);

		rc = of_property_read_u32(dev_node, "max17050,capacity_l",
				&pdata->capacity);

		rc = of_property_read_u32(dev_node, "max17050,vf_fullcap_l",
				&pdata->vf_fullcap);

		rc = of_property_read_u32(dev_node, "max17050,iavg_empty_l",
			&pdata->iavg_empty);

		rc = of_property_read_u32(dev_node, "max17050,rescale_factor_l",
			&pdata->rescale_factor);
		rc = of_property_read_u32(dev_node, "max17050,rescale_soc_l",
			&pdata->rescale_soc);

	} else { /*Tocad battery*/
		rc = of_property_read_u8_array(dev_node, "max17050,model_80_t",
				pdata->model_80, MODEL_SIZE);

		rc = of_property_read_u8_array(dev_node, "max17050,model_90_t",
				pdata->model_90, MODEL_SIZE);

		rc = of_property_read_u8_array(dev_node, "max17050,model_A0_t",
				pdata->model_A0, MODEL_SIZE);

		rc = of_property_read_u32(dev_node, "max17050,rcomp0_t",
				&pdata->rcomp0);

		rc = of_property_read_u32(dev_node, "max17050,tempco_t",
				&pdata->tempco);

		rc = of_property_read_u32(dev_node, "max17050,ichgterm_t",
				&pdata->ichgterm);

		rc = of_property_read_u32(dev_node, "max17050,vempty_t",
				&pdata->vempty);

		rc = of_property_read_u32(dev_node, "max17050,qrtable00_t",
				&pdata->qrtable00);

		rc = of_property_read_u32(dev_node, "max17050,qrtable10_t",
				&pdata->qrtable10);

		rc = of_property_read_u32(dev_node, "max17050,qrtable20_t",
				&pdata->qrtable20);

		rc = of_property_read_u32(dev_node, "max17050,qrtable30_t",
				&pdata->qrtable30);

		rc = of_property_read_u32(dev_node, "max17050,capacity_t",
				&pdata->capacity);

		rc = of_property_read_u32(dev_node, "max17050,vf_fullcap_t",
				&pdata->vf_fullcap);

		rc = of_property_read_u32(dev_node, "max17050,iavg_empty_t",
			&pdata->iavg_empty);

		rc = of_property_read_u32(dev_node, "max17050,rescale_factor_t",
			&pdata->rescale_factor);

		rc = of_property_read_u32(dev_node, "max17050,rescale_soc_t",
			&pdata->rescale_soc);
	}
	rc = of_property_read_u32(dev_node, "max17050,config",
			&pdata->config);

	rc = of_property_read_u32(dev_node, "max17050,relaxcfg",
			&pdata->relaxcfg);

	rc = of_property_read_u32(dev_node, "max17050,filtercfg",
			&pdata->filtercfg);

	rc = of_property_read_u32(dev_node, "max17050,learncfg",
			&pdata->learncfg);

	rc = of_property_read_u32(dev_node, "max17050,misccfg",
			&pdata->misccfg);

	rc = of_property_read_u32(dev_node, "max17050,fullsocthr",
			&pdata->fullsocthr);

	rc = of_property_read_u32(dev_node, "max17050,tempnom",
				&pdata->tempnom);

	rc = of_property_read_u32(dev_node, "max17050,tgain",
			&pdata->tgain);

	rc = of_property_read_u32(dev_node, "max17050,toff",
			&pdata->toff);

	rc = of_property_read_u32(dev_node, "max17050,param-version",
			&pdata->param_version);

	rc = of_property_read_u32(dev_node, "max17050,full_design",
			&pdata->full_design);

	/* Debug log model data by dtsi parsing */
	for (i = 0; i < MODEL_SIZE; i++)
		pr_max17050(PR_DEBUG, "%s : Model_data_80 %d = 0x%x\n",
				__func__, i, pdata->model_80[i]);

	for (i = 0; i < MODEL_SIZE; i++)
		pr_max17050(PR_DEBUG, "%s : Model_data_90 %d = 0x%x\n",
				__func__, i, pdata->model_90[i]);

	for (i = 0; i < MODEL_SIZE; i++)
		pr_max17050(PR_DEBUG, "%s : Model_data_A0 %d = 0x%x\n",
				__func__, i, pdata->model_A0[i]);

	pr_max17050(PR_INFO, "%s : Platform data : "\
			"rcomp = 0x%x, "\
			"tempco = 0x%x, "\
			"ichgterm = 0x%x, "\
			"vempty = 0x%x, "\
			"full_design = 0x%x "\
			"rescale_soc =%d, "\
			"rescale_factor =%d\n",
			__func__,
			pdata->rcomp0,
			pdata->tempco,
			pdata->ichgterm,
			pdata->vempty,
			pdata->vf_fullcap,
			pdata->rescale_soc,
			pdata->rescale_factor);

	return rc;
}


bool max17050_get_cell_info(void)
{
	unsigned int smem_size = 0;
	unsigned int *batt_id = (unsigned int *)
		(smem_get_entry(SMEM_BATT_INFO, &smem_size, 0, 0));

	if (smem_size != 0 && batt_id) {
		if (*batt_id == BATT_NOT_PRESENT) {
			pr_max17050(PR_INFO, "%s : [MAX17050] probe : skip for no model data\n",
					__func__);
			ref = NULL;
			return false;
		}

		 if (*batt_id == BATT_ID_DS2704_L
			|| *batt_id == BATT_ID_ISL6296_C
			||  *batt_id == BATT_ID_RA4301_VC1
			||  *batt_id == BATT_ID_SW3800_VC0) {
			cell_info = LGC_LLL; /* LGC Battery */
			pr_max17050(PR_INFO, "%s : LGC profile\n", __func__);
			return true;
		 }

		if (*batt_id == BATT_ID_DS2704_C
			|| *batt_id == BATT_ID_ISL6296_L
			|| *batt_id == BATT_ID_RA4301_VC0
			|| *batt_id == BATT_ID_SW3800_VC1) {
			cell_info = TCD_AAC; /* Tocad, Hitachi Battery */
			pr_max17050(PR_INFO, "%s : TDC profile\n", __func__);
			return true;
		}
	}

	pr_max17050(PR_INFO, "%s : Unknown cell, Using LGC profile\n", __func__);
	cell_info = LGC_LLL;
	return true;

}

static char *pm_power_supplied_to[] = {
	"battery",
};
static int max17050_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17050_chip *chip = NULL;
	int ret = 0;
	int rc = 0;

	pr_max17050(PR_INFO, "%s : ----- Start-----\n", __func__);

	if (!max17050_get_cell_info()) {
		ret = -EINVAL;
		goto error;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		ret = -EIO;
		goto error;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto error;
	}
	chip->client = client;

	chip->pdata = devm_kzalloc(&client->dev,
					 sizeof(struct max17050_platform_data),
					 GFP_KERNEL);
	if (!chip->pdata) {
		pr_max17050(PR_ERR, "%s : missing platform data\n", __func__);
		ret = -ENODEV;
		goto error;
	}

	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy) {
		pr_max17050(PR_ERR, "%s : batt_psy supply not found, deferring probe\n", __func__);
		ret = -EPROBE_DEFER;
		goto error;
	}

#ifdef CONFIG_OF
	if (!(&client->dev.of_node)) {
		pr_max17050(PR_ERR, "%s : max17050_probe of_node err.\n", __func__);
		goto error;
	}

	ret = max17050_parse_dt(&client->dev, chip->pdata);

	if (ret != 0) {
		pr_max17050(PR_ERR, "%s : device tree parsing error\n", __func__);
		goto error;
	}
#else
	chip->pdata = client->dev.platform_data;
#endif

	i2c_set_clientdata(client, chip);
	max17050_i2c_client = client;

	chip->prev_soc = FAIL_SAFE_SOC;
	chip->last_soc = FAIL_SAFE_SOC;
	ref = chip;

	INIT_DELAYED_WORK(&chip->max17050_model_data_write_work, max17050_model_data_write_work);
	schedule_delayed_work(&chip->max17050_model_data_write_work, 0);
	INIT_WORK(&chip->work, max17050_init_work);

	mutex_init(&chip->mutex);

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	ret = device_create_file(&client->dev, &dev_attr_at_fuelrst);
	if (ret < 0) {
		pr_max17050(PR_ERR, "%s : File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_fuelrst_failed;
	}
	ret = device_create_file(&client->dev, &dev_attr_at_fuelval);
	if (ret < 0) {
		pr_max17050(PR_ERR, "%s : File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_fuelval_failed;
	}
	ret = device_create_file(&client->dev, &dev_attr_at_batl);
	if (ret < 0) {
		pr_max17050(PR_ERR, "%s : File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_batl_failed;
	}
#endif

	chip->ext_fg_psy.name = "ext_fg";
	chip->ext_fg_psy.type =  POWER_SUPPLY_TYPE_EXT_FG;
	chip->ext_fg_psy.supplied_to = pm_power_supplied_to;
	chip->ext_fg_psy.num_supplicants= ARRAY_SIZE(pm_power_supplied_to);
	chip->ext_fg_psy.properties= max17050_battery_props;
	chip->ext_fg_psy.num_properties = ARRAY_SIZE(max17050_battery_props);
	chip->ext_fg_psy.get_property = max17050_get_property;

	rc =  power_supply_register(&chip->client->dev, &chip->ext_fg_psy);
	if (rc < 0) {
		pr_max17050(PR_ERR, "%s : [MAX17050]failed to register a power_supply rc = %d\n"
			, __func__, ret);
		ret = -EPROBE_DEFER;
		goto error;
	}
	ref = chip;

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					max17050_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"max17050", chip);
		if (ret) {
			dev_err(&client->dev, "cannot enable irq");
			goto err_create_file_batl_failed;
		} else {
			enable_irq_wake(client->irq);
		}
	}

	INIT_DELAYED_WORK(&chip->max17050_post_init_work, max17050_post_init_work);
	INIT_DELAYED_WORK(&chip->max17050_dump_work, max17050_dump_work);
	INIT_DELAYED_WORK(&chip->max17050_monitor_work, max17050_monitor_work);

	schedule_delayed_work(&chip->max17050_post_init_work, 0);
	schedule_delayed_work(&chip->max17050_dump_work, 0);
	schedule_delayed_work(&chip->max17050_monitor_work
			,msecs_to_jiffies(MAX17050_POLLING_PERIOD_10));

	pr_max17050(PR_INFO, "%s : () ----- End -----\n", __func__);

	ref->probe_done = true;

	return 0;

err_create_file_fuelrst_failed:
	device_remove_file(&client->dev, &dev_attr_at_fuelrst);
	pr_max17050(PR_ERR, "%s : Probe err_create_file_fuelrst_failed\n ", __func__);
err_create_file_fuelval_failed:
	device_remove_file(&client->dev, &dev_attr_at_fuelval);
	pr_max17050(PR_ERR, "%s : Probe err_create_file_fuelval_failed!!!\n ", __func__);
err_create_file_batl_failed:
	device_remove_file(&client->dev, &dev_attr_at_batl);
	pr_max17050(PR_ERR, "%s : Probe err_create_file_batl_failed!!!\n ", __func__);
error:
	pr_max17050(PR_ERR, "%s : [MAX17050] Probe fail!!!\n", __func__);

	if (chip != NULL)
		kfree(chip);

	return ret;
}

static int max17050_remove(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_at_fuelrst);
	device_remove_file(&client->dev, &dev_attr_at_fuelval);
	device_remove_file(&client->dev, &dev_attr_at_batl);

	i2c_set_clientdata(client, NULL);
	kfree(chip);
	return 0;
}
static int max17050_pm_prepare(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17050_chip *chip = i2c_get_clientdata(client);

	/* Cancel any pending work, if necessary */
	cancel_delayed_work_sync(&ref->max17050_monitor_work);
	cancel_delayed_work_sync(&ref->max17050_dump_work);

	chip->suspended = true;
	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}
	return 0;
}

static void max17050_pm_complete(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17050_chip *chip = i2c_get_clientdata(client);

	chip->suspended = false;
	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
	}

	/* Schedule update, if needed */
	schedule_delayed_work(&ref->max17050_monitor_work, msecs_to_jiffies(HZ));
	schedule_delayed_work(&ref->max17050_dump_work, msecs_to_jiffies(HZ));
}

static const struct dev_pm_ops max17050_pm_ops = {
	.prepare = max17050_pm_prepare,
	.complete = max17050_pm_complete,
};

static struct of_device_id max17050_match_table[] = {
	{ .compatible = "maxim,max17050", },
	{ },
};

static const struct i2c_device_id max17050_id[] = {
	{ "max17050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17050_id);

static struct i2c_driver max17050_i2c_driver = {
	.driver	= {
		.name	= "max17050",
		.owner	= THIS_MODULE,
		.of_match_table = max17050_match_table,
		.pm = &max17050_pm_ops,

	},
	.probe		= max17050_probe,
	.remove		= max17050_remove,
	.id_table	= max17050_id,
};

static int __init max17050_init(void)
{
	return i2c_add_driver(&max17050_i2c_driver);
}
module_init(max17050_init);

static void __exit max17050_exit(void)
{
	i2c_del_driver(&max17050_i2c_driver);
}
module_exit(max17050_exit);

MODULE_AUTHOR("LG Power <lge.com>");
MODULE_DESCRIPTION("MAX17050 Fuel Gauge");
MODULE_LICENSE("GPL");
