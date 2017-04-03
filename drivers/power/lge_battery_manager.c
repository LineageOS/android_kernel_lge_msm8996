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

#define BM_LDB_LOG

#define BATT_MANAGER_NAME "lge,battery_manager"
#define BM_PSY_NAME "battery_manager"
#define BAT_PSY_NAME "battery"
#define BMS_PSY_NAME "bms"
#define USB_PSY_NAME "usb"
#define DC_PSY_NAME "dc"
#define BM_MONITOR_SET_TIME 10000
#define BM_MONITOR_WORK_TIME 10000
#define BM_PARAM_MONITOR_NORMAL 10000
#define BM_PARAM_MONITOR_FAST 2000
#define FAULT_VOLTAGE 4500000
#define FAULT_AGE 70
#define FAULT_FACTOR 1100
#define ABCD_START_SOC 50
#define DEGRADE_FACTOR 90
#define HOUR_TO_SEC 3600
#define MSEC_TO_MINUTE 60000
#define MSEC_TO_SECOND 1000
#define OC_COUNT 5
#define DEFAULT_MAX_VOLTAGE 4400000
#define DEFAULT_FULL_DESIGN 3400000
#define ABNORMAL_COUNT_MAX 150
#define ADC_SCALE 1000
#define SOC_SCALE 100
#define SOURCE_TIME_PADDING 1000000
#define CONDITION_CAP_PADDING 100000


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
	DEGRADE, //DEGRADE
	MAX,
};

static char *status_map[MAX] = {
	"NORMAL, ",
	"AGED, ",
	"WARN, ",
	"ABCV, ",
	"ABCC, ",
	"ABNOR, ",
	"DEGRADE, ",
};

enum print_reason {
	PR_DEBUG		= BIT(0),
	PR_INFO 		= BIT(1),
	PR_ERR			= BIT(2),
};

static int bm_debug_mask = PR_INFO|PR_ERR;

#define pr_bm(reason, fmt, ...)                 \
	do {                                        \
		if ( bm_debug_mask & (reason) )         \
		pr_info("[bm] " fmt, ##__VA_ARGS__);    \
		else                                    \
		pr_debug("[bm] " fmt, ##__VA_ARGS__);   \
	} while (0)

enum count_value{
	COUNT_OC = 0,
	COUNT_INFO,
	COUNT_ABNORMAL,
	COUNT_MAX,
};

enum condition_bit{
	B_ABCD = 0,
	B_ABVD,
	B_IRB,
	B_AGE,
	B_DEGRADE,
	B_MAX,
};

struct charging_param {
	int voltage;
	int resist;
	int curr;
	int ocv;
	int chg_type;
	int source_type;
	int soc;
	int temp;
	int cycle;
	int cap_full;
	int cap_now;
	int cap_raw;
	int chg_enabled;
	int chg_time;
};
#define CAP_BUF_MAX 10
struct refer_param {
	int count[COUNT_MAX];
	int sum_adc_value[SUM_ADC_VALUE_MAX];
	int source_with_time;
	unsigned int charge_full_design;
	u64 cap_now_with_status;
	u64 cm_past_time;
	int calculated_cap;
	int max_cap;
	bool is_cap_now;
	bool check_abvd;
	int past_fcc;
	struct charging_param chg_data;
	int cap_buf[CAP_BUF_MAX];
	int cap_idx;
};

#define IRB_COUNT 10
struct irb_param {
	int irb_count;
	int abnormal_count;
	int irb_ready;
	int last_current;
	int irb_sum;
	int irb_buf[IRB_COUNT];
	int irb_flag;
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
	struct delayed_work bm_monitor_set_work;
	struct delayed_work parameter_track_work;
	struct refer_param	*ref;
	struct irb_param irbp;
	int condition;
};

#define ABS(X) ((X) < 0 ? (-1 * (X)) : (X))

#define ABCD_BIT (1 << B_ABCD)
#define ABVD_BIT (1 << B_ABVD)
#define IRB_BIT (1 << B_IRB)
#define AGE_BIT (1 << B_AGE)
#define DEGRADE_BIT (1 << B_DEGRADE)

#define LOG_MAX_SIZE 32
#define LOG_MAX_LENGTH 256
#define BM_PROC_NAME "driver/sbm"
static char *log_buffer[LOG_MAX_SIZE];
static int log_index;
static bool work_started;
static u64 start_time;
struct mutex mutex;

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
		LOG_MAX_SIZE*LOG_MAX_LENGTH * sizeof(char));
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

static void batt_mngr_monitor_set_work(struct work_struct *work)
{
	struct batt_mngr *bm = container_of(work,
				struct batt_mngr,
				bm_monitor_set_work.work);

	if (work_started == true) {
		schedule_delayed_work(&bm->parameter_track_work,
			round_jiffies_relative(msecs_to_jiffies(BM_PARAM_MONITOR_NORMAL)));
	}
}

static void batt_mngr_irb_reset(struct batt_mngr *bm) {
	bm->irbp.irb_flag = 0;
	bm->irbp.irb_count = 0;
	bm->irbp.irb_sum = 0;
	bm->irbp.irb_ready = 0;
	bm->irbp.abnormal_count = 0;
	bm->irbp.last_current = 0;
}

#define CUR_DELTA (25 * ADC_SCALE) // 25mA
#define VOL_DELTA (2  * ADC_SCALE)  // 2mV
#define ABNOR_COUNT 5
static void batt_mngr_irbounce_monitor(struct batt_mngr *bm)
{
	int volt_avg = 0;
	if (ABS(bm->irbp.last_current - bm->ref->chg_data.curr) > CUR_DELTA ||
			bm->ref->chg_data.chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE ||
			bm->ref->chg_data.curr > 0) {
		batt_mngr_irb_reset(bm);
		bm->irbp.last_current = bm->ref->chg_data.curr;
		return;
	} else if (bm->irbp.irb_flag == 1) {
		bm->irbp.irb_sum -= bm->irbp.irb_buf[bm->irbp.irb_count];
	} else if (bm->irbp.irb_count == (int)IRB_COUNT - 1) {
		bm->irbp.irb_flag = 1;
	}

	bm->irbp.irb_buf[bm->irbp.irb_count] = bm->ref->chg_data.voltage;
	bm->irbp.irb_sum += bm->ref->chg_data.voltage;
	bm->irbp.irb_count = (bm->irbp.irb_count + 1)%10;
	bm->irbp.last_current = bm->ref->chg_data.curr;

	if (bm->irbp.irb_flag == 1) {
		volt_avg = bm->irbp.irb_sum / (int)IRB_COUNT;
		if (bm->ref->chg_data.voltage + VOL_DELTA > volt_avg ) {
			bm->irbp.irb_ready++;
			return;
		}
		if (bm->ref->chg_data.voltage + VOL_DELTA < volt_avg &&
				bm->irbp.irb_ready > (int)ABNOR_COUNT) {
			bm->irbp.abnormal_count++;
		} else {
			bm->irbp.abnormal_count = 0;
		}
	}
	if(bm->irbp.abnormal_count > (int)ABNOR_COUNT) {
		bm->condition |= IRB_BIT;
		return;
	}
	return;
}

#define RESIST_MIN (50 * ADC_SCALE)
#define RESIST_MAX (1000 * ADC_SCALE)
#define VOLTAGE_MIN (3200 * ADC_SCALE)
#define VOLTAGE_MAX (4500 * ADC_SCALE)
bool check_data_validity(int type, int val){
	switch (type) {
		case RESIST:
			if (val < RESIST_MIN || val > RESIST_MAX) {
				return false;
			}
			break;
		case VOL:
			if (val <= VOLTAGE_MIN || val >= VOLTAGE_MAX) {
				return false;
			}
			break;
		default:
			return false;
	}
	return true;
}

static void batt_mngr_abnormal_cap_monitor(struct batt_mngr *bm){
	int abc_factor = 0;
	int cap;
	int fcc;

	cap = (bm->ref->calculated_cap)/ADC_SCALE;
	cap *= cap;
	fcc = (bm->ref->past_fcc)/ADC_SCALE;
	fcc *= fcc;
	if (cap == 0 || fcc == 0 || bm->ref->chg_data.soc == 0) {
		return;
	}
	abc_factor = (cap * SOC_SCALE) / ((fcc* bm->ref->chg_data.soc)/ADC_SCALE);
	if (abc_factor >= FAULT_FACTOR) {
		bm->condition |= ABCD_BIT;
	}
}

static void batt_mngr_aging_monitor(struct batt_mngr *bm){
	int age;
	int decrement;
	decrement = (bm->ref->max_cap * 100) / bm->ref->past_fcc;
	age = (bm->ref->max_cap * 100) / bm->ref->charge_full_design;

	if (age <= FAULT_AGE) {
		bm->condition |= AGE_BIT;
		if (decrement <= DEGRADE_FACTOR) {
			bm->condition |= DEGRADE_BIT;
		}
	}
}

static void batt_mngr_abnormal_voltage_monitor(struct batt_mngr *bm){
	int c_ocv;
	int rc = 0;
	c_ocv = bm->ref->chg_data.ocv - ((bm->ref->chg_data.resist / ADC_SCALE) * (bm->ref->chg_data.curr / ADC_SCALE));

	if (c_ocv > FAULT_VOLTAGE) {
		bm->ref->count[COUNT_OC]++;
		rc = 1;
	}
	if (bm->ref->count[COUNT_OC] > OC_COUNT) {
		if (bm->ref->count[COUNT_ABNORMAL] <= 0) {
			bm->ref->check_abvd = false;
			bm->ref->count[COUNT_ABNORMAL] = ABNORMAL_COUNT_MAX;
			bm->ref->count[COUNT_OC] = 0;
			return;
		}
		bm->ref->count[COUNT_ABNORMAL]--;
		bm->ref->check_abvd = true;
		bm->condition |= ABVD_BIT;
		rc = 1;
	}
	if (rc) {
		batt_mngr_set_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	}
}

static void batt_mngr_calculate_cap(struct batt_mngr *bm){
	u64 current_time = 0;
	int time_interval = 0;

	current_time = get_jiffies_64();
	if (!bm->ref->cm_past_time) {
		bm->ref->cm_past_time = current_time;
		if (bm->ref->max_cap != 0) {
			bm->ref->calculated_cap = (bm->ref->max_cap * bm->ref->chg_data.soc)/SOC_SCALE;
		} else {
			bm->ref->calculated_cap = (bm->ref->chg_data.cap_full * bm->ref->chg_data.soc)/SOC_SCALE;
		}
	}
	time_interval = jiffies_to_msecs(current_time - bm->ref->cm_past_time)/MSEC_TO_SECOND;
	bm->ref->calculated_cap -= (int)((time_interval * bm->ref->chg_data.curr) / HOUR_TO_SEC);
	bm->ref->cm_past_time = current_time;
}

static bool check_cap_validity(struct batt_mngr *bm){
	int i;
	int validity = 0;

	if (bm->ref->is_cap_now) {
		if (bm->ref->chg_data.cap_now == 0) {
			if (bm->ref->chg_data.chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE && 
					bm->ref->chg_data.chg_enabled) {
				bm->ref->max_cap = bm->ref->calculated_cap;
				batt_mngr_aging_monitor(bm);
				bm->ref->cap_idx = 0;
			}
			return false;
		}
		return true;
	}

	if (bm->ref->chg_data.cap_now == 0 || bm->ref->cap_idx == CAP_BUF_MAX) {
		return false;
	}

	bm->ref->cap_buf[bm->ref->cap_idx++] = bm->ref->chg_data.cap_now;
	if (bm->ref->cap_idx == CAP_BUF_MAX) {
		for (i = 1 ; i < CAP_BUF_MAX; i++) {
			validity += ABS(bm->ref->cap_buf[i] - bm->ref->cap_buf[i-1]);
		}
		if (validity != 0) {
			bm->ref->cm_past_time = 0;
			return true;
		}
		return false;
	}
	return false;
}

static void batt_mngr_parameter_monitor(struct batt_mngr *bm) {

	if( bm->ref->chg_data.cycle < 1 ) {
		return;
	}

	bm->ref->is_cap_now = check_cap_validity(bm);
	if (bm->ref->is_cap_now) {
		if (bm->ref->chg_data.curr < 0) {
			bm->ref->calculated_cap = max(bm->ref->chg_data.cap_now,bm->ref->calculated_cap);
		} else {
			bm->ref->calculated_cap = bm->ref->chg_data.cap_now;
		}
		bm->ref->is_cap_now = true;
	}
	else {
		batt_mngr_calculate_cap(bm);
	}
	if (bm->ref->chg_data.soc >= ABCD_START_SOC) {
		batt_mngr_abnormal_cap_monitor(bm);
	}

	if (bm->ref->chg_data.chg_type == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
		batt_mngr_abnormal_voltage_monitor(bm);
	}

	batt_mngr_irbounce_monitor(bm);
}

static int batt_mngr_get_condition(struct batt_mngr *bm){
	int i;
	int bit;
	int ret = 0 ;
	for (bit = B_ABCD; bit < B_MAX ; bit++) {
		int digit = 1;
		for (i = 0 ; i < bit; i++) {
			digit *= 10;
		}
		if (bm->condition & (1 << bit)) {
			ret += digit;
		}
	}
	return ret;
}
static bool batt_mngr_get_charging_data(struct batt_mngr *bm){
	if (batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &bm->ref->chg_data.voltage) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_RESISTANCE, &bm->ref->chg_data.resist) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &bm->ref->chg_data.curr) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_OCV, &bm->ref->chg_data.ocv) ||
			batt_mngr_get_fg_prop(bm->batt_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &bm->ref->chg_data.chg_type) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_TEMP, &bm->ref->chg_data.temp) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &bm->ref->chg_data.soc) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &bm->ref->chg_data.cycle) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &bm->ref->chg_data.cap_full) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_NOW, &bm->ref->chg_data.cap_now) ||
			batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_NOW_RAW, &bm->ref->chg_data.cap_raw) ||
			batt_mngr_get_fg_prop(bm->batt_psy, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &bm->ref->chg_data.chg_enabled) ||
			batt_mngr_get_fg_prop(bm->usb_psy, POWER_SUPPLY_PROP_TYPE, &bm->ref->chg_data.source_type) ||
			bm->ref->count[COUNT_OC] == -1) {
		pr_bm(PR_INFO, "Invalid adc or no count: %d\n", bm->ref->count[COUNT_OC]);
		return false;
	}
	batt_mngr_set_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	return true;
}

#define PRIOR_STAND 1000
static void batt_mngr_status_check(struct batt_mngr *bm, char *log){
	int status;
	int status_prior1;
	int status_prior2;
	status = bm->ref->cap_now_with_status / CONDITION_CAP_PADDING;
	status_prior1 = status % PRIOR_STAND;
	status_prior2 = status / PRIOR_STAND;
	switch (status_prior1) {
		case 1:
			batt_mngr_battery_status(bm, ABCC, log);
			return;
		case 10:
			batt_mngr_battery_status(bm, ABCV, log);
			return;
		case 11:
			batt_mngr_battery_status(bm, ABNOR,log);
			return;
		case 101:
			batt_mngr_battery_status(bm, WARN, log);
			return;
		case 110:
			batt_mngr_battery_status(bm, WARN, log);
			return;
		case 111:
			batt_mngr_battery_status(bm, WARN, log);
			return;
		default:
			break;
	}
	switch (status_prior2) {
		case 1:
			batt_mngr_battery_status(bm, AGED, log);
			return;
		case 10:
			batt_mngr_battery_status(bm, DEGRADE, log);
			return;
		case 11:
			batt_mngr_battery_status(bm, DEGRADE, log);
			return;
		default:
			break;
	}
	batt_mngr_battery_status(bm, NORMAL, log);
}

static void batt_mngr_parameter_logger(struct batt_mngr *bm){
	char log[LOG_MAX_LENGTH] = {0};
	int i;
	if (!batt_mngr_get_charging_data(bm)) {
		return;
	}

	bm->ref->chg_data.chg_time = jiffies_to_msecs(get_jiffies_64() - start_time) / MSEC_TO_MINUTE;

	if (check_data_validity(RESIST, bm->ref->chg_data.resist) &&
			check_data_validity(VOL, bm->ref->chg_data.ocv) &&
			check_data_validity(VOL, bm->ref->chg_data.voltage)) {
		batt_mngr_parameter_monitor(bm);
	}
	bm->ref->sum_adc_value[RESIST] += bm->ref->chg_data.resist;
	bm->ref->sum_adc_value[CURR] += bm->ref->chg_data.curr;
	bm->ref->sum_adc_value[OCV] += bm->ref->chg_data.ocv;
	bm->ref->sum_adc_value[VOL] += bm->ref->chg_data.voltage;

	if ((++bm->ref->count[COUNT_INFO]) >= 6) {
		bm->ref->source_with_time = bm->ref->chg_data.source_type * SOURCE_TIME_PADDING +bm->ref->chg_data.chg_time;
		bm->ref->cap_now_with_status = batt_mngr_get_condition(bm) * CONDITION_CAP_PADDING + bm->ref->chg_data.cap_now / ADC_SCALE;
#ifdef BM_LDB_LOG
		snprintf(log, sizeof(log)-1, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%llu\n",
			bm->ref->chg_data.soc,
			bm->ref->chg_data.temp,
			bm->ref->chg_data.cycle,
			bm->ref->chg_data.cap_full/ADC_SCALE,
			(bm->ref->sum_adc_value[OCV]/6)/ADC_SCALE,
			(bm->ref->sum_adc_value[VOL]/6)/ADC_SCALE,
			(bm->ref->sum_adc_value[CURR]/6)/ADC_SCALE,
			(bm->ref->sum_adc_value[RESIST]/6)/ADC_SCALE,
			bm->ref->source_with_time, bm->ref->cap_now_with_status);
		batt_mngr_status_check(bm, log);
#endif
		for (i = 0; i < SUM_ADC_VALUE_MAX; i++)
			bm->ref->sum_adc_value[i] = 0;
		bm->ref->count[COUNT_INFO] = 0;
	}
}


static void batt_mngr_parameter_track_work(struct work_struct *work)
{
	struct batt_mngr *bm = container_of(work,
				struct batt_mngr,
				parameter_track_work.work);
	pr_bm(PR_INFO, "monitoring\n");
	batt_mngr_parameter_logger(bm);

	if (bm->ref->check_abvd && work_started == true) {
		schedule_delayed_work(&bm->parameter_track_work,
			round_jiffies_relative(msecs_to_jiffies(BM_PARAM_MONITOR_FAST)));
	} else if (work_started == true) {
		schedule_delayed_work(&bm->parameter_track_work,
			round_jiffies_relative(msecs_to_jiffies(BM_PARAM_MONITOR_NORMAL)));
	}
}

static void battery_mngr_init_parameter(struct batt_mngr *bm)
{
	int max_volt = 0;
	int charge_full_design = 0;

	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &max_volt);
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &charge_full_design);
	batt_mngr_get_fg_prop(bm->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &bm->ref->past_fcc);

	if (max_volt <= 0) {
		max_volt = DEFAULT_MAX_VOLTAGE;
	}

	if (charge_full_design <= 0) {
		charge_full_design = DEFAULT_FULL_DESIGN;
	}
	if (bm->ref->past_fcc <= 0) {
		bm->ref->past_fcc = charge_full_design;
	}
	bm->ref->count[COUNT_ABNORMAL] = ABNORMAL_COUNT_MAX;
	bm->ref->check_abvd = false;
	bm->ref->calculated_cap = 0;
	bm->ref->cap_idx = 0;
	bm->ref->is_cap_now = false;
	bm->ref->cm_past_time = 0;
	bm->ref->charge_full_design = charge_full_design;

	bm->ref->chg_data.voltage = 0;
	bm->ref->chg_data.resist = 0;
	bm->ref->chg_data.curr = 0;
	bm->ref->chg_data.ocv = 0;
	bm->ref->chg_data.chg_type = 0;
	bm->ref->chg_data.source_type = POWER_SUPPLY_TYPE_UNKNOWN;
	bm->ref->chg_data.chg_time = 0;
	bm->ref->chg_data.soc = 0;
	bm->ref->chg_data.temp = 0;
	bm->ref->chg_data.cycle = 0;
	bm->ref->chg_data.cap_full = 0;
	bm->ref->chg_data.cap_now = 0;
}


static int batt_mngr_stop_work(struct batt_mngr *bm)
{
	if (bm == NULL || bm->ref == NULL) {
		pr_bm(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	batt_mngr_reset_param(bm->ref);

	cancel_delayed_work_sync(&bm->bm_monitor_work);
	cancel_delayed_work_sync(&bm->parameter_track_work);
	cancel_delayed_work_sync(&bm->bm_monitor_set_work);
	work_started = false;
	pr_bm(PR_INFO,"Stop work\n");
	return 1;
}


static int batt_mngr_start_work(struct batt_mngr *bm)
{
	start_time = get_jiffies_64();
	if (bm == NULL) {
		return -ENODEV;
	}

	battery_mngr_init_parameter(bm);

	if (work_started == true) {
		cancel_delayed_work_sync(&bm->bm_monitor_set_work);
		schedule_delayed_work(&bm->bm_monitor_set_work,
				round_jiffies_relative(msecs_to_jiffies(BM_MONITOR_SET_TIME)));
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

	if (usb_present | dc_present) {
		batt_mngr_start_work(bm);
	} else {
		work_started = false;
	}
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
			if (work_started == false) {
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
		pr_bm(PR_ERR, "Not Ready(bm_psy)\n");
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

	INIT_DELAYED_WORK(&bm->bm_monitor_set_work, batt_mngr_monitor_set_work);
	INIT_DELAYED_WORK(&bm->parameter_track_work, batt_mngr_parameter_track_work);
	INIT_DELAYED_WORK(&bm->bm_monitor_work, batt_mngr_monitor_work);

	platform_set_drvdata(pdev, bm);

	schedule_delayed_work(&bm->bm_monitor_work,
			round_jiffies_relative(msecs_to_jiffies(BM_MONITOR_WORK_TIME)));

	work_started = true;
	bm->condition = 0;
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
