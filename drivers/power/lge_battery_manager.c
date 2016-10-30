#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/string.h>
#include "lge_battery_manager.h"

//#define BM_CURRENT_PULSING
#define BM_LDB_LOG

#define BATT_MANAGER_NAME "lge,battery_manager"
#define BM_PSY_NAME "battery_manager"
#define BAT_PSY_NAME "battery"
#define BMS_PSY_NAME "bms"
#define USB_PSY_NAME "usb"
#define DC_PSY_NAME "dc"
#define BM_OCV_SET_TIME 10000
#define BM_MONITOR_WORK_TIME 10000
#ifdef SBM_TEST_MODE
#define BM_VT_MONITOR_A 2000 //2s
#else
#define BM_VT_MONITOR_A 10000 //10s
#endif
#define BM_VT_MONITOR_B 2000
#define OCV_COUNT 101
#define FAULT_RESISTANCE 300000
#define FAULT_VOLTAGE 4500000
#define FAULT_AGE 80
#define HOUR_TO_SEC 3600
#define OC_COUNT 5
#define DEFAULT_MAX_VOLTAGE 4400000
#define DEFAULT_FULL_DESIGN 2800000
#define ABNORMAL_COUNT_MAX 30
#define ADC_SCALE 1000
#define SOC_SCALE 100

enum sum_adc_value {
	RESIST = 0,
	CURR,
	OCV,
	VOL,
	SUM_ADC_VALUE_MAX,
};

enum battery_status {
	NORMAL = 0, //normal
	AGED, //aged
	WARN, //warning
	ABCV, //abnormal cv
	ABCC, //abnormal cc
	ABNOR, //abnormal
	MAX,
};

enum print_reason {
	PR_DEBUG		= BIT(0),
	PR_INFO 		= BIT(1),
	PR_ERR			= BIT(2),
};

enum adc_value {
	VOLTAGE_LOW     = 0,
	VOLTAGE_HIGH,
	CURRENT_LOW,
	CURRENT_HIGH
};

static int bm_debug_mask = PR_INFO|PR_ERR;

#define pr_bm(reason, fmt, ...)                                          \
    do {                                                                 \
	if (bm_debug_mask & (reason))                                    \
	    pr_info("[bm] " fmt, ##__VA_ARGS__);                         \
	else                                                             \
	    pr_debug("[bm] " fmt, ##__VA_ARGS__);                        \
    } while (0)

enum count_value{
	COUNT_OC = 0,
	COUNT_PULSING,
	COUNT_INFO,
	COUNT_ABNORMAL,
	COUNT_ABNORMAL_LOG,
	COUNT_MAX,
};

struct refer_param {
	int count[COUNT_MAX];
	int sum_adc_value[SUM_ADC_VALUE_MAX];
	int est_soc;
	int adc_value[4];
	unsigned long total_capacity_mAs;
	u64 cap_now_mAs;
	u64 past_time;
	bool check_aged;
};

struct batt_mngr {
	struct device		*dev;
	struct kobject		*kobj;
	struct power_supply batt_mngr_psy;
	struct power_supply *bm_psy;
	struct power_supply *cc_psy;
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
	struct power_supply *usb_psy;
	struct power_supply *dc_psy;
	struct delayed_work bm_monitor_work;
	struct delayed_work bm_ocv_set_work;
	struct delayed_work voltage_track_work;
#ifdef BM_CURRENT_PULSING
	struct delayed_work pulsing_work;
#endif
	struct refer_param	*ref;
	int max_voltage;
	int charge_full_design;
	int cycle_count;
	bool cycled;
};

static int ocv_table[OCV_COUNT + 1];
#define LOG_MAX_SIZE 32
#define LOG_MAX_LENGTH 256
#define BM_PROC_NAME "driver/sbm"
static char *log_buffer[LOG_MAX_SIZE];
static int log_index;
static bool work_started;

struct mutex mutex;

static char *status_map[MAX] = {
	"NORMAL, ",
	"AGED, ",
	"WARN, ",
	"ABCV, ",
	"ABCC, ",
	"ABNOR, ",
};

static enum power_supply_property batt_mngr_properties[] = {
	POWER_SUPPLY_PROP_HEALTH,
};

static int batt_mngr_get_fg_prop(struct power_supply *psy,
	enum power_supply_property prop, int *value)
{
	union power_supply_propval val = {0,};
	int rc = 0;

	if (unlikely(psy == NULL)) {
		pr_bm(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	rc = psy->get_property(psy, prop, &val);
	*value = val.intval;
	if (unlikely(rc))
		pr_bm(PR_ERR, "%s, rc: %d, intval: %d\n",
			__func__, rc, *value);

	return rc;
}

#ifdef BM_LDB_LOG
static void batt_mngr_flush_log(void)
{
	int i;

	mutex_lock(&mutex);
	for(i = 0; i < log_index; i++) {
		kfree(log_buffer[i]);
	}
	log_index = 0;
	mutex_unlock(&mutex);
}

static void batt_mngr_write_log(char *log, int size)
{
	if (size == 0)
		return;
	if (log_index >= LOG_MAX_SIZE) {
		return;
	}
	mutex_lock(&mutex);
	log_buffer[log_index] = kzalloc(sizeof(char) * size, GFP_KERNEL);
	if (log_buffer[log_index] == NULL) {
		pr_bm(PR_ERR, "fail to allocate log_buffer\n");
		return ;
	}

	strcpy(log_buffer[log_index], log);
	log_index++;
	mutex_unlock(&mutex);
}

static int batt_mngr_proc_show(struct seq_file *m, void *v)
{
	int i;
	mutex_lock(&mutex);
	for(i = 0; i < log_index; i++) {
		seq_printf(m,"%s",log_buffer[i]);
	}
	mutex_unlock(&mutex);
	batt_mngr_flush_log();
	return 0;
}

static int batt_mngr_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, batt_mngr_proc_show, NULL,
		(log_index + 1)*LOG_MAX_LENGTH * sizeof(char));
}

struct proc_dir_entry *proc_bm_file;
static const struct file_operations proc_file_fops = {
	.open = batt_mngr_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int batt_mngr_init_proc(void)
{
	int rv = 0;

	proc_bm_file = proc_create(BM_PROC_NAME, 0 , NULL, &proc_file_fops);
	mutex_init(&mutex);
	if (proc_bm_file == NULL) {
		rv = -ENOMEM;
		return rv;
	}

	return rv;
}

static void batt_mngr_cleanup_proc(void)
{
	remove_proc_entry(NULL, proc_bm_file);
	mutex_destroy(&mutex);
}
#endif

static int batt_mngr_set_fg_prop(struct power_supply *psy,
	enum power_supply_property prop, int value)
{
	union power_supply_propval val = {0,};
	int rc;

	if (unlikely(psy == NULL)) {
		pr_bm(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	val.intval = value;
	rc = psy->set_property(psy, prop, &val);
	if (unlikely(rc))
		pr_bm(PR_ERR, "%s, rc: %d, intval: %d\n",
			__func__, rc, value);

	return rc;
}

static void batt_mngr_reset_param(struct refer_param *r_param)
{
	memset(r_param, 0, sizeof(struct refer_param));

}

#ifdef BM_LDB_LOG
static void batt_mngr_battery_status(struct batt_mngr *bm,
	int status, char *batt_info)
{
	int size_status = strlen(status_map[status]);
	int size_batt_info = strlen(batt_info);
	char *log = (char *)kzalloc((size_status + size_batt_info), GFP_KERNEL);
	if (log == NULL) {
		pr_bm(PR_ERR, "fail to allocate log\n");
		return ;
	}

	strncat(log, status_map[status], size_status);
	strncat(log, batt_info, size_batt_info);

	batt_mngr_write_log(log, strlen(log));
	kfree(log);
}
#endif

static void batt_mngr_ocv_set_work(struct work_struct *work)
{
	struct batt_mngr *bm = container_of(work,
				struct batt_mngr,
				bm_ocv_set_work.work);

	if (ocv_table[OCV_COUNT] != -1) {
		batt_mngr_set_fg_prop(bm->bms_psy,
			POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, (int)(OCV_COUNT));
	}

	if(work_started == true) {
		schedule_delayed_work(&bm->voltage_track_work,
			round_jiffies_relative(msecs_to_jiffies(BM_VT_MONITOR_A)));
	}
}

static int batt_mngr_calc_current(struct batt_mngr *bm,
	int voltage, int resist, int curr, int ocv, int status)
{
	unsigned int time_interval = 0;
	u64 current_time = 0;
	char log[LOG_MAX_LENGTH] = {0};
	int fg_soc = 0;
	int fg_temp = 0;
	int fg_cycle = 0;
	int fg_full = 0;
	int i = 0;

	current_time = get_jiffies_64();
	if( ocv_table == NULL ) {
		return 0;
	}
	if(!bm->ref->past_time) {
		bm->ref->past_time = current_time - 200;
		while ((ocv > ocv_table[i]) && (ocv_table[i] != -1)) {
			i++;
		}
		for (i=0; i < SUM_ADC_VALUE_MAX; i++)
			bm->ref->sum_adc_value[i] = 0;
		if(i >= 100) {
			bm->ref->est_soc = 100;
			bm->ref->cap_now_mAs = bm->ref->total_capacity_mAs;
			return 0;
		}else if(i <= 0) {
			bm->ref->est_soc = 0;
			bm->ref->cap_now_mAs = 0;
			return 0;
		}

		bm->ref->est_soc = i * ADC_SCALE +
			(int)((ocv - ocv_table[i - 1]) * ADC_SCALE /
			(ocv_table[i] - ocv_table[i - 1]));
		bm->ref->cap_now_mAs =
			((bm->ref->total_capacity_mAs / ADC_SCALE) *
			bm->ref->est_soc) / SOC_SCALE;

	}

	time_interval = jiffies_to_msecs(current_time - bm->ref->past_time);
	bm->ref->cap_now_mAs -=
		(int)(time_interval  * (curr / (int)ADC_SCALE)) /
		(int)ADC_SCALE;
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_TEMP, &fg_temp);
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &fg_soc);
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &fg_cycle);
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &fg_full);

	bm->ref->sum_adc_value[RESIST] += resist;
	bm->ref->sum_adc_value[CURR] += curr;
	bm->ref->sum_adc_value[OCV] += ocv;
	bm->ref->sum_adc_value[VOL] += voltage;

	if (++bm->ref->count[COUNT_ABNORMAL_LOG] % 30 == 1) {
		snprintf(log, sizeof(log)-1, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			fg_soc, fg_temp, fg_cycle, fg_full/ADC_SCALE,
			(bm->ref->sum_adc_value[OCV]/30)/ADC_SCALE,
			(bm->ref->sum_adc_value[VOL]/30)/ADC_SCALE,
			(bm->ref->sum_adc_value[CURR]/30)/ADC_SCALE,
			(bm->ref->sum_adc_value[RESIST]/30)/ADC_SCALE,
			bm->ref->est_soc,(int)(bm->ref->cap_now_mAs));
			batt_mngr_battery_status(bm, NORMAL, log);
			for (i=0; i < SUM_ADC_VALUE_MAX; i++)
				bm->ref->sum_adc_value[i] = 0;
	}

	if (bm->ref->cap_now_mAs > bm->ref->total_capacity_mAs &&
		bm->ref->count[COUNT_ABNORMAL] > 0) {
		snprintf(log, sizeof(log)-1, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			fg_soc, fg_temp, fg_cycle, fg_full/ADC_SCALE,
			ocv/ADC_SCALE, voltage/ADC_SCALE,
			curr/ADC_SCALE, resist/ADC_SCALE, bm->ref->est_soc, (int)(bm->ref->cap_now_mAs));

		switch (status) {
			case ABNOR:
				if (bm->cycled == 1) {
#ifdef BM_LDB_LOG
					batt_mngr_battery_status(bm, WARN, log);
#else
					pr_bm(PR_INFO, "status(%d)\n", status);
#endif
				} else {
#ifdef BM_LDB_LOG
					batt_mngr_battery_status(bm, ABNOR, log);
#else
					pr_bm(PR_INFO, "status(%d)\n", status);
#endif
				}
				bm->ref->count[COUNT_ABNORMAL]--;
				break;
			case ABCC:
#ifdef BM_LDB_LOG
				batt_mngr_battery_status(bm, ABNOR, log);
#else
				pr_bm(PR_INFO, "status(%d)\n", status);
#endif
				bm->ref->count[COUNT_ABNORMAL]--;
				break;
			case ABCV:
				if (bm->ref->cap_now_mAs > (bm->ref->total_capacity_mAs * 11) / 10) {
#ifdef BM_LDB_LOG
					batt_mngr_battery_status(bm, ABNOR, log);
#else
					pr_bm(PR_INFO, "status(%d)\n", status);
#endif
					bm->ref->count[COUNT_ABNORMAL]--;
				}
				break;
			default:
				pr_bm(PR_ERR, "Invalid status(%d)\n", status);
				break;
			}
	}
	bm->ref->past_time = current_time;

	if (bm->ref->count[COUNT_ABNORMAL] <= 0) {
		bm->ref->count[COUNT_ABNORMAL_LOG] = 0;
		bm->ref->count[COUNT_OC] = -1;
		return 0;
	}

	return 1;
}


static void batt_mngr_impedance_monitor(struct batt_mngr *bm)
{
	int charge_full = 0;
	int age = 0;
	char log[LOG_MAX_LENGTH] = {0};

	if (!bm->ref->check_aged && bm->charge_full_design) {
		batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &charge_full);
		age = charge_full * 100 / bm->charge_full_design;
		if (age <= FAULT_AGE) {
			snprintf(log, sizeof(log)-1, "%d:%d:%d\n",
				charge_full, bm->charge_full_design, age);
#ifdef BM_LDB_LOG
			batt_mngr_battery_status(bm, AGED, log);
#endif
		}
		bm->ref->check_aged = 1;
	}
#ifdef BM_CURRENT_PULSING
	if (bm->ref->count[COUNT_PULSING] == 0) {
		schedule_delayed_work(&bm->pulsing_work,
			round_jiffies_relative(msecs_to_jiffies(BM_VT_MONITOR_B)));
	}
#endif
}


static int batt_mngr_abnormal_voltage_monitor(struct batt_mngr *bm)
{
	int voltage = 0;
	int resist = 0;
	int curr = 0;
	int ocv = 0;
	int chg_type = 0;
	int val = 0;
	int fg_soc = 0;
	int fg_temp = 0;
	int fg_cycle = 0;
	int fg_full = 0;
	int rc = 0;
	int i = 0;
	char log[LOG_MAX_LENGTH] = {0};

	if (batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_RESISTANCE, &resist) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &curr) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_OCV, &ocv) ||
			batt_mngr_get_fg_prop(bm->batt_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &chg_type) ||
			bm->ref->count[COUNT_OC] == -1) {
		pr_bm(PR_INFO, "Invalid adc or no count: %d\n", bm->ref->count[COUNT_OC]);
		return 0;
	}
	val = ocv - ((resist / ADC_SCALE) * (curr / ADC_SCALE));

	if (val > FAULT_VOLTAGE) {
		bm->ref->count[COUNT_OC]++;
		rc = 1;
	} else if(val < FAULT_VOLTAGE && bm->ref->count[COUNT_OC] <= OC_COUNT) {
		bm->ref->sum_adc_value[RESIST] += resist;
		bm->ref->sum_adc_value[CURR] += curr;
		bm->ref->sum_adc_value[OCV] += ocv;
		bm->ref->sum_adc_value[VOL] += voltage;

		if ((++bm->ref->count[COUNT_INFO]) >= 6) {
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_TEMP, &fg_temp);
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &fg_soc);
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &fg_cycle);
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &fg_full);

#ifdef BM_LDB_LOG
			snprintf(log, sizeof(log)-1, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
				fg_soc, fg_temp, fg_cycle, fg_full/ADC_SCALE,
				(bm->ref->sum_adc_value[OCV]/6)/ADC_SCALE,
				(bm->ref->sum_adc_value[VOL]/6)/ADC_SCALE,
				(bm->ref->sum_adc_value[CURR]/6)/ADC_SCALE,
				(bm->ref->sum_adc_value[RESIST]/6)/ADC_SCALE,
				bm->ref->est_soc, (int)(bm->ref->cap_now_mAs));

			batt_mngr_battery_status(bm, NORMAL, log);
#endif

			for (i = 0; i < SUM_ADC_VALUE_MAX; i++)
				bm->ref->sum_adc_value[i] = 0;
			bm->ref->count[COUNT_INFO] = 0;
		}
	}


	if (bm->ref->count[COUNT_OC] > OC_COUNT) {
		if (resist < FAULT_RESISTANCE) {
			switch (chg_type) {
				case POWER_SUPPLY_CHARGE_TYPE_FAST:
					if (voltage < 4350000) {
						rc = batt_mngr_calc_current(bm, voltage, resist,
							curr, ocv, ABNOR);
					} else {
						rc = batt_mngr_calc_current(bm, voltage, resist,
							curr, ocv, ABCC);
					}
					break;
				case POWER_SUPPLY_CHARGE_TYPE_TAPER:
						rc = batt_mngr_calc_current(bm, voltage, resist,
							curr, ocv, ABCV);
					break;
				default:
					pr_bm(PR_ERR, "Invalid type(%d)\n", chg_type);
					break;
			}
		} else {
			batt_mngr_impedance_monitor(bm);
		}
	}
	if (rc) {
		batt_mngr_set_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	}

	return rc;
}

static void batt_mngr_voltage_track_work(struct work_struct *work)
{
	struct batt_mngr *bm = container_of(work,
				struct batt_mngr,
				voltage_track_work.work);
	int rc = 0;

	pr_bm(PR_INFO, "monitoring\n");

	rc = batt_mngr_abnormal_voltage_monitor(bm);
	if (rc && work_started == true) {
		schedule_delayed_work(&bm->voltage_track_work,
			round_jiffies_relative(msecs_to_jiffies(BM_VT_MONITOR_B)));
	} else if(work_started == true) {
		schedule_delayed_work(&bm->voltage_track_work,
			round_jiffies_relative(msecs_to_jiffies(BM_VT_MONITOR_A)));
	}
}

#ifdef BM_CURRENT_PULSING
#define PULSING_PERIOD_1 4000
#define PULSING_PERIOD_2 1500
static void batt_mngr_pulsing_work(struct work_struct *work)
{
	struct batt_mngr *bm = container_of(work,
			struct batt_mngr,
			pulsing_work.work);
	int voltage = 0;
	int curr = 0;
	int est_resistance = 0;
	int i;
	int count = 0;
	int adc_value[4] = {0};

	count = bm->ref->count[COUNT_PULSING];
	memcpy(adc_value, bm->ref->adc_value, sizeof(bm->ref->adc_value));


	if (count == 0) {
		batt_mngr_set_fg_prop(bm->batt_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, 0);
		pr_bm(PR_INFO, "charging stop, count: %d\n", count);
		count++;
		schedule_delayed_work(&bm->pulsing_work,
			round_jiffies_relative(msecs_to_jiffies(PULSING_PERIOD_1)));
	} else if (count < 5) {
		batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage);
		batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &curr);
		adc_value[VOLTAGE_LOW] += voltage;
		adc_value[CURRENT_LOW] += curr;
		count++;
		schedule_delayed_work(&bm->pulsing_work,
			round_jiffies_relative(msecs_to_jiffies(PULSING_PERIOD_2)));
	} else if (count == 5) {
		batt_mngr_set_fg_prop(bm->batt_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, 1);
		batt_mngr_set_fg_prop(bm->batt_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
			1000 * ADC_SCALE);
		count++;
		schedule_delayed_work(&bm->pulsing_work,
			round_jiffies_relative(msecs_to_jiffies(PULSING_PERIOD_1)));
	} else if (count < 10) {
		batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage);
		batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &curr);
		adc_value[VOLTAGE_HIGH] += voltage;
		adc_value[CURRENT_HIGH] += curr;
		count++;
		schedule_delayed_work(&bm->pulsing_work,
			round_jiffies_relative(msecs_to_jiffies(PULSING_PERIOD_2)));
	} else if (count == 10) {
		est_resistance = (adc_value[VOLTAGE_HIGH] - adc_value[VOLTAGE_LOW]) /
			((adc_value[CURRENT_HIGH] - adc_value[CURRENT_LOW]) / ADC_SCALE);
		if (est_resistance > FAULT_RESISTANCE / ADC_SCALE) {
			batt_mngr_set_fg_prop(bm->batt_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, 1600 * ADC_SCALE);
		} else {
			batt_mngr_set_fg_prop(bm->batt_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, 1000 * ADC_SCALE);
		}
		count = 0;
		for (i = 0; i < 4; i++) {
			adc_value[i] = 0;
		}
	}

	bm->ref->count[COUNT_PULSING] = count;
	memcpy(bm->ref->adc_value, adc_value, sizeof(adc_value));
}
#endif

static void battery_mngr_init_parameter(struct batt_mngr *bm)
{
	int max_volt = 0;
	int charge_full_design = 0;

	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &max_volt);
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &charge_full_design);

	if (max_volt <= 0) {
		max_volt = DEFAULT_MAX_VOLTAGE;
	}

	if (charge_full_design <= 0) {
		charge_full_design = DEFAULT_FULL_DESIGN;
	}
	bm->ref->count[COUNT_ABNORMAL] = ABNORMAL_COUNT_MAX;
	bm->ref->check_aged = 0;
	bm->max_voltage = max_volt;
	bm->charge_full_design = charge_full_design;
	bm->ref->total_capacity_mAs = (unsigned long)((charge_full_design / 1000) * HOUR_TO_SEC);
}


static int batt_mngr_stop_work(struct batt_mngr *bm)
{
	if (bm == NULL || bm->ref == NULL) {
		pr_bm(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	batt_mngr_reset_param(bm->ref);

	cancel_delayed_work_sync(&bm->bm_monitor_work);
	cancel_delayed_work_sync(&bm->voltage_track_work);
	cancel_delayed_work_sync(&bm->bm_ocv_set_work);
	work_started = false;
	pr_bm(PR_INFO,"Stop work\n");
	return 1;
}


static int batt_mngr_start_work(struct batt_mngr *bm)
{
	int cycle = 0;

	if (bm == NULL) {
		return -ENODEV;
	}

	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &cycle);
	if (cycle == bm->cycle_count + 1) {
		bm->cycled = 1;
	}
	bm->cycle_count = cycle;

	battery_mngr_init_parameter(bm);

	if(work_started == true) {
		cancel_delayed_work_sync(&bm->bm_ocv_set_work);
		schedule_delayed_work(&bm->bm_ocv_set_work,
				round_jiffies_relative(msecs_to_jiffies(BM_OCV_SET_TIME)));
		pr_bm(PR_INFO,"Start work\n");
	}
	return 1;
}

static void batt_mngr_monitor_work(struct work_struct *work)
{
	struct batt_mngr *bm = container_of(work,
				struct batt_mngr,
				bm_monitor_work.work);
	int usb_present = 0;
	int dc_present = 0;

	batt_mngr_get_fg_prop(bm->usb_psy, POWER_SUPPLY_PROP_PRESENT, &usb_present);
	batt_mngr_get_fg_prop(bm->dc_psy, POWER_SUPPLY_PROP_PRESENT, &dc_present);

	if ( usb_present | dc_present ) {
		batt_mngr_start_work(bm);
	} else {
		work_started = false;
	}
}

void batt_mngr_ocv_table(int *ocv_tab, int size)
{
	int i;

	for(i = 0; i < size; i++) {
		ocv_table[i] = *(ocv_tab + i);
	}
	ocv_table[OCV_COUNT] = -1;
}

static int batt_mngr_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct batt_mngr *bm = container_of(psy, struct batt_mngr, batt_mngr_psy);

	if (bm == NULL) {
		pr_bm(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		if (val->intval == 1) {
			if(work_started == false) {
				work_started = true;
				batt_mngr_start_work(bm);
			} else {
				pr_bm(PR_INFO, "work started\n");
			}
		} else {
			batt_mngr_stop_work(bm);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static int batt_mngr_get_property(struct power_supply *psy,
	enum power_supply_property bm_property, union power_supply_propval *val)
{
	struct batt_mngr *bm = container_of(psy, struct batt_mngr, batt_mngr_psy);
	int rc = 0;

	if (bm == NULL) {
		pr_bm(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	switch (bm_property) {
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = 1;
		break;
	default:
		return -EINVAL;
	}
	return rc;
}

static int batt_mngr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct batt_mngr *bm = NULL;

	bm = kzalloc(sizeof(struct batt_mngr), GFP_KERNEL);
	if (bm == NULL) {
		pr_bm(PR_ERR, "Not Ready(bm)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	bm->ref = kzalloc(sizeof(struct refer_param), GFP_KERNEL);
	if (bm->ref == NULL) {
		pr_bm(PR_ERR, "Not Ready(ref)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	bm->batt_psy = power_supply_get_by_name(BAT_PSY_NAME);
	if (bm->batt_psy == NULL) {
		pr_bm(PR_ERR, "Not Ready(batt_psy)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}
	bm->bms_psy = power_supply_get_by_name(BMS_PSY_NAME);
	if (bm->bms_psy == NULL) {
		pr_bm(PR_ERR, "Not Ready(bms_psy)\n");
		ret =	-EPROBE_DEFER;
		goto error;
	}

	bm->batt_mngr_psy.name = BM_PSY_NAME;
	bm->batt_mngr_psy.properties = batt_mngr_properties;
	bm->batt_mngr_psy.num_properties = ARRAY_SIZE(batt_mngr_properties);
	bm->batt_mngr_psy.get_property = batt_mngr_get_property;
	bm->batt_mngr_psy.set_property = batt_mngr_set_property;

	ret = power_supply_register(bm->dev, &bm->batt_mngr_psy);
	if (ret < 0) {
		pr_bm(PR_ERR, "%s power_supply_register charger controller failed ret=%d\n",
			__func__, ret);
		goto error;
	}

	bm->bm_psy = power_supply_get_by_name(BM_PSY_NAME);
	if (bm->bm_psy == NULL) {
		pr_bm(PR_ERR, "Not Ready(cc_psy)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	bm->usb_psy = power_supply_get_by_name(USB_PSY_NAME);
	if (bm->usb_psy == NULL) {
		pr_bm(PR_ERR, "Not Ready(usb_psy)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	bm->dc_psy = power_supply_get_by_name(DC_PSY_NAME);
	if (bm->usb_psy == NULL) {
		pr_bm(PR_ERR, "Not Ready(dc_psy)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	INIT_DELAYED_WORK(&bm->bm_ocv_set_work, batt_mngr_ocv_set_work);
	INIT_DELAYED_WORK(&bm->voltage_track_work, batt_mngr_voltage_track_work);
	INIT_DELAYED_WORK(&bm->bm_monitor_work, batt_mngr_monitor_work);
#ifdef BM_CURRENT_PULSING
	INIT_DELAYED_WORK(&bm->pulsing_work, batt_mngr_pulsing_work);
#endif

	platform_set_drvdata(pdev, bm);

	schedule_delayed_work(&bm->bm_monitor_work,
			round_jiffies_relative(msecs_to_jiffies(BM_MONITOR_WORK_TIME)));
	work_started = true;

	pr_bm(PR_INFO, "Probe done\n");
	return ret;

error:
	kfree(bm);
	return ret;
}

static int batt_mngr_remove(struct platform_device *pdev)
{
	struct batt_mngr *bm = platform_get_drvdata(pdev);

	power_supply_unregister(&bm->batt_mngr_psy);
	kfree(bm);
	return 0;
}

#if defined(CONFIG_PM)
static int batt_mngr_suspend(struct device *dev)
{
	return 0;
}

static int batt_mngr_resume(struct device *dev)
{
	return 0;
}
static const struct dev_pm_ops batt_mngr_pm_ops = {
	.suspend	= batt_mngr_suspend,
	.resume		= batt_mngr_resume,
};
#endif

static struct of_device_id batt_mngr_match_table[] = {
		{ .compatible = BATT_MANAGER_NAME },
		{ },
};

static struct platform_driver batt_mngr_driver = {
	.probe = batt_mngr_probe,
	.remove = batt_mngr_remove,
	.driver = {
		.name = BM_PSY_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm     = &batt_mngr_pm_ops,
#endif
		.of_match_table = batt_mngr_match_table,
	},
};

static struct platform_device batt_mngr_dev = {
	.name = BM_PSY_NAME,
	.dev = {
		.platform_data = NULL,
	}
};

static int __init batt_mngr_init(void)
{
	platform_device_register(&batt_mngr_dev);
#ifdef BM_LDB_LOG
	batt_mngr_init_proc();
#endif
	return platform_driver_register(&batt_mngr_driver);
}

static void __exit batt_mngr_exit(void)
{
	platform_driver_unregister(&batt_mngr_driver);
#ifdef BM_LDB_LOG
	batt_mngr_cleanup_proc();
#endif
}

late_initcall(batt_mngr_init);
module_exit(batt_mngr_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Kensin");
MODULE_DESCRIPTION("LGE Battery Manager");
