#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/wakelock.h>
#include <linux/qpnp/qpnp-adc.h>


#define BATT_SWAP_NAME "lge,battery-swap"
#define BS_PSY_NAME "battery_swap"
#define BAT_PSY_NAME "battery"
#define BMS_PSY_NAME "bms"
#define USB_PSY_NAME "usb"
#define DC_PSY_NAME "dc"

#define BACKUP_VOLTAGE_CHK_TIME 10000
#define BACK_UP_CHG_WORK_TIME	20000
#define BAT_REMOVE_WORK_TIME	1
#define BAT_INSERT_WORK_TIME	100

enum battery_swap_status {
	SWAP_OFF = 0, //battery swap disabled
	SWAP_ON, //battery swap enabled
	BACK_UP_BATTERY_VOLTAGE_LOW, //backup battery voltge low
	MAIN_BATTERY_OUT_OF_TEMP, //use main battery temp, out of temp range
};

enum battery_swap_enable {
	SWAP_DISABLE = 0, //battery swap disabled
	SWAP_ENABLE, //battery swap enabled
};
enum signal_status {
	SIGNAL_LOW = 0,		//signal low status
	SIGNAL_HIGH,		//signal high status
};
enum present_status {
	MAIN_BAT_REMOVED = 0,	//Main Battery removed
	MAIN_BAT_INSERTED,	//Main Battery inserted
};

enum manual_battery_swap_action {
	SWAP_UNSET = 0, //manual battery swap unset
	SWAP_SET, //manual battery swap set, swap operation started
};

enum print_reason {
	PR_DEBUG		= BIT(0),
	PR_INFO 		= BIT(1),
	PR_ERR			= BIT(2),
};

enum back_up_voltage_adc_value {
	VOLTAGE_LOW     = 0,
	VOLTAGE_HIGH,
	CURRENT_LOW,
	CURRENT_HIGH
};

static int bs_debug_mask = PR_INFO|PR_ERR;

#define pr_bs(reason, fmt, ...)                                          \
    do {                                                                 \
	if (bs_debug_mask & (reason))                                    \
	    pr_info("[bs] " fmt, ##__VA_ARGS__);                         \
	else                                                             \
	    pr_debug("[bs] " fmt, ##__VA_ARGS__);                        \
    } while (0)


struct batt_swap {
	struct device			*dev;
	struct kobject			*kobj;
	struct power_supply 	batt_swap_psy;
	struct power_supply 	*bs_psy;
	struct power_supply 	*cc_psy;
	struct power_supply 	*batt_psy;
	struct power_supply 	*bms_psy;
	struct power_supply 	*usb_psy;
	struct power_supply 	*dc_psy;
	struct qpnp_vadc_chip 	*vadc_dev;
	struct delayed_work 	bs_bvbat_check_work;
	struct delayed_work		bs_backup_bat_chg_work;
	struct delayed_work		bs_battery_remove_work;
	struct delayed_work		bs_battery_insert_work;
	struct wake_lock 		swap_lock;
	int bvbat_voltage;
	bool bat_swap_enable;
	int bat_swap_status;
	bool manual_bat_swap;
	int		bat_missing_det;
	int		swap_ctrl_en1;
	int		swap_ctrl_en2;
	int		back_up_chg_disable;
	int		back_up_chg_stat;
	int		bat_missing_irqn;
	int		main_bat_present;
};


static enum power_supply_property batt_swap_properties[] = {
	POWER_SUPPLY_PROP_MANUAL_SWAP,
	POWER_SUPPLY_PROP_SWAP_ENABLE,
	POWER_SUPPLY_PROP_SWAP_STATUS,
};

#ifdef CONFIG_BATTERY_MAX17050
extern void gauge_handle_batt_removal(void);
extern void gauge_detect_vbat_in_no_load(void);
extern void gauge_start_calculation (void);
#endif

static irqreturn_t batt_missing_handler(int irq, void *_chip)
{
	struct batt_swap *bs = _chip;
	int val = 0;

	val = gpio_get_value(bs->bat_missing_det);
	pr_bs(PR_INFO, "triggered: val = %d\n", val);

	if(val == SIGNAL_LOW){ //Battery Remove
		pr_bs(PR_INFO, "Battery Remove: val = %d\n", val);
		wake_lock(&bs->swap_lock);

		gpio_direction_output(bs->swap_ctrl_en1, SIGNAL_HIGH);
		gpio_set_value(bs->swap_ctrl_en2, SIGNAL_HIGH);
		schedule_delayed_work(&bs->bs_battery_remove_work,
			round_jiffies_relative(
				msecs_to_jiffies(BAT_REMOVE_WORK_TIME)));

	}else { //Battery Inserted
		pr_bs(PR_INFO, "Battery Inserted: val = %d\n", val);
		wake_lock(&bs->swap_lock);

		schedule_delayed_work(&bs->bs_battery_insert_work,
			round_jiffies_relative(
				msecs_to_jiffies(BAT_INSERT_WORK_TIME)));

		if (bs->batt_psy)
			power_supply_changed(bs->batt_psy);
	}

	return IRQ_HANDLED;
}

static int batt_swap_get_prop(struct power_supply *psy,
	enum power_supply_property prop, int *value)
{
	union power_supply_propval val = {0,};
	int rc = 0;

	if (unlikely(psy == NULL)) {
		pr_bs(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	rc = psy->get_property(psy, prop, &val);
	*value = val.intval;
	if (unlikely(rc))
		pr_bs(PR_ERR, "%s, rc: %d, intval: %d\n",
			__func__, rc, *value);

	return rc;
}

#define DEFAULT_BACK_UP_VOLTAGE	3800
#define EXTERNAL_RESISTOR_DIVIDE	3
static int batt_swap_bvbat_votlage(struct batt_swap *bs)
{
	int rc = 0;
	int backup_voltage;
	struct qpnp_vadc_result results;

	if (IS_ERR_OR_NULL(bs->vadc_dev)) {
		/* Get adc device */
		pr_bs(PR_ERR, "vadc_dev is null\n");
		bs->vadc_dev = qpnp_get_vadc(bs->dev, "vbackup");
		if (IS_ERR(bs->vadc_dev)){
			rc = PTR_ERR(bs->vadc_dev);
			if (rc != -EPROBE_DEFER)
				pr_bs(PR_ERR, "Couldn't get vadc ret=%d\n",
						rc);
			//Default value return for exception
			return DEFAULT_BACK_UP_VOLTAGE;
			}
		}

	rc = qpnp_vadc_read(bs->vadc_dev, P_MUX1_1_1, &results);
	if (rc) {
		pr_err("Unable to read vbackup rc=%d\n", rc);
		return 0;

	} else {
		backup_voltage = (int)results.physical
					* EXTERNAL_RESISTOR_DIVIDE / 1000;
		return backup_voltage;
	}

	return DEFAULT_BACK_UP_VOLTAGE; //Default value return for exception
}

static bool batt_swap_get_enable(struct batt_swap *bs)
{
	pr_bs(PR_INFO, "get batt_swap_enable = %d \n", bs->bat_swap_enable);
	return bs->bat_swap_enable;
}

static bool batt_swap_get_manual_swap(struct batt_swap *bs)
{
	pr_bs(PR_INFO, "get manual_bat_swap = %d \n", bs->bat_swap_enable);
	return bs->manual_bat_swap;
}

static bool batt_swap_get_status(struct batt_swap *bs)
{
	if(bs->bat_swap_enable == SWAP_ENABLE){
		bs->bat_swap_status = SWAP_ON;
	}else{
		bs->bat_swap_status = SWAP_OFF;
	}
	pr_bs(PR_INFO, "batt_swap_get_status = %d \n", bs->bat_swap_status);
	return bs->bat_swap_status;
}


static void batt_swap_bvbat_check_work(struct work_struct *work)
{
	struct batt_swap *bs = container_of(work,
				struct batt_swap,
				bs_bvbat_check_work.work);
	int usb_present = 0;
	int dc_present = 0;
	int vbackup_voltage = 0;

	batt_swap_get_prop(bs->usb_psy, POWER_SUPPLY_PROP_PRESENT, &usb_present);
	batt_swap_get_prop(bs->dc_psy, POWER_SUPPLY_PROP_PRESENT, &dc_present);


	vbackup_voltage = batt_swap_bvbat_votlage(bs);
	pr_bs(PR_INFO,"Backup Battery Voltage  = %d mV\n", vbackup_voltage);

	schedule_delayed_work(&bs->bs_bvbat_check_work,
			round_jiffies_relative(msecs_to_jiffies(BACKUP_VOLTAGE_CHK_TIME)));
}

#define BACK_UP_BAT_CHG_THRESHOLD	3900
static void batt_swap_backup_bat_chg_work(struct work_struct *work)
{
	struct batt_swap *bs = container_of(work,
				struct batt_swap,
				bs_backup_bat_chg_work.work);

	int usb_present = 0;
	int dc_present = 0;
	int vbackup_voltage = 0;
	int chg_stat = 0;

	chg_stat = gpio_get_value(bs->back_up_chg_stat);
	batt_swap_get_prop(bs->usb_psy, POWER_SUPPLY_PROP_PRESENT,
								&usb_present);
	batt_swap_get_prop(bs->dc_psy, POWER_SUPPLY_PROP_PRESENT,
								&dc_present);

	if(usb_present || dc_present){ //Default charging when USB or TA inserted
		pr_bs(PR_DEBUG, "back_up_chg_stat: usb = %d dc = %d chg_stat = %d\n",
					usb_present,dc_present, chg_stat);
			if(chg_stat == SIGNAL_HIGH){ //In charging status
			pr_bs(PR_DEBUG,"Backup Battery Charging Status => Charging \n");
			goto out; //No additional work needs
		}else{
			pr_bs(PR_DEBUG,"Backup Battery Charging Enabled with Ext. Pwr.\n");
			gpio_direction_input(bs->back_up_chg_disable);
			goto out;
			}
	}else {

		if(chg_stat == SIGNAL_HIGH){ //In charging status
			pr_bs(PR_DEBUG,"Backup Battery Charging Status => Charging \n");
			goto out; //No additional work needs
		}else{
			vbackup_voltage = batt_swap_bvbat_votlage(bs);
			pr_bs(PR_DEBUG,"Backup Battery Voltage  = %d mV\n",
							vbackup_voltage);
			if (vbackup_voltage < BACK_UP_BAT_CHG_THRESHOLD){
				pr_bs(PR_DEBUG,"Backup Battery Charging Enabled.\n");
				gpio_direction_input(bs->back_up_chg_disable);
				goto out;
			}else{
				pr_bs(PR_DEBUG,"Backup Battery Voltage High => No Charging \n");
				gpio_direction_output(bs->back_up_chg_disable, 0);
				goto out; //No additional charging needs with Main BAT
			}
		}
	}
out:
	schedule_delayed_work(&bs->bs_backup_bat_chg_work,
			round_jiffies_relative(msecs_to_jiffies(BACK_UP_CHG_WORK_TIME)));
}

#define BACKUP_BAT_POWEROFF_THRESHOLD 3300
static void batt_swap_battery_remove_work(struct work_struct *work)
{
	struct batt_swap *bs = container_of(work,
				struct batt_swap,
				bs_battery_remove_work.work);

	int vbackup_voltage = 0;
	int usb_present = 0;
	int dc_present = 0;

	pr_bs(PR_INFO,"batt_swap_battery_remove_work \n");
	batt_swap_get_prop(bs->usb_psy, POWER_SUPPLY_PROP_PRESENT, &usb_present);
	batt_swap_get_prop(bs->dc_psy, POWER_SUPPLY_PROP_PRESENT, &dc_present);

	if(usb_present || dc_present){
		pr_bs(PR_INFO,"Main Battery removed with Ext. Pwr Src\n");
		}

	vbackup_voltage = batt_swap_bvbat_votlage(bs);
	pr_bs(PR_INFO,"Backup Battery Voltage  = %d mV\n", vbackup_voltage);

	if(vbackup_voltage < BACKUP_BAT_POWEROFF_THRESHOLD){
		pr_bs(PR_INFO,"Backup Battery Voltag is TOO low.\n");
	}

	wake_unlock(&bs->swap_lock);
	bs->main_bat_present = MAIN_BAT_REMOVED; //Main battery out
#ifdef CONFIG_BATTERY_MAX17050
	gauge_handle_batt_removal();
#endif
}

#define SWAP_CTRL_DELAY	20
static void batt_swap_battery_insert_work(struct work_struct *work)
{
	struct batt_swap *bs = container_of(work,
				struct batt_swap,
				bs_battery_insert_work.work);

	pr_bs(PR_INFO,"batt_swap_battery_insert_work \n");

	if(bs->main_bat_present == MAIN_BAT_INSERTED){
		pr_bs(PR_INFO,"Debounced Missing Interrupt signal.\n");
	}else{
#ifdef CONFIG_BATTERY_MAX17050
		gauge_detect_vbat_in_no_load();
#endif
		gpio_set_value(bs->swap_ctrl_en2, SIGNAL_LOW);
		msleep(SWAP_CTRL_DELAY);
		gpio_direction_input(bs->swap_ctrl_en1);
		bs->main_bat_present = MAIN_BAT_INSERTED; //Main battery out
#ifdef CONFIG_BATTERY_MAX17050
		gauge_start_calculation();
#endif
	}

	wake_unlock(&bs->swap_lock);

}


static int batt_swap_set_property(struct power_supply *psy,
	enum power_supply_property bs_property, const union power_supply_propval *val)
{
	struct batt_swap *bs = container_of(psy, struct batt_swap, batt_swap_psy);

	if (bs == NULL) {
		pr_bs(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	switch (bs_property) {
	case POWER_SUPPLY_PROP_MANUAL_SWAP:
			bs->manual_bat_swap = val->intval;
			pr_bs(PR_INFO, "Manual Battery Swap Set = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_SWAP_ENABLE:
			bs->bat_swap_enable = val->intval;
			pr_bs(PR_INFO, "Battery Swap Enable = %d\n", val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int batt_swap_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_MANUAL_SWAP:
	case POWER_SUPPLY_PROP_SWAP_ENABLE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}
static int batt_swap_get_property(struct power_supply *psy,
	enum power_supply_property bs_property, union power_supply_propval *val)
{
	struct batt_swap *bs = container_of(psy, struct batt_swap, batt_swap_psy);
	int rc = 0;

	if (bs == NULL) {
		pr_bs(PR_ERR, "%s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}

	switch (bs_property) {
	case POWER_SUPPLY_PROP_MANUAL_SWAP:
		val->intval = batt_swap_get_manual_swap(bs);
		break;
	case POWER_SUPPLY_PROP_SWAP_ENABLE:
		val->intval = batt_swap_get_enable(bs);
		break;
	case POWER_SUPPLY_PROP_SWAP_STATUS:
		val->intval = batt_swap_get_status(bs);
		break;
	default:
		return -EINVAL;
	}
	return rc;
}

static int batt_swap_hw_init(struct batt_swap *bs)
{
	int rc;

	rc = gpio_request(bs->bat_missing_det, "main_bat_missing_gpio");

	if (rc < 0)
		pr_bs(PR_ERR, "Couldn't get bat_missing_det rc = %d\n", rc);

	bs->bat_missing_irqn = gpio_to_irq(bs->bat_missing_det);

	rc = devm_request_threaded_irq(bs->dev, bs->bat_missing_irqn,
			NULL,
			batt_missing_handler, IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"battery_missing_det_irq", bs);
	if(rc<0)
		pr_bs(PR_ERR, "devm_request_threaded_irq\n");

	rc = enable_irq_wake(bs->bat_missing_irqn);
	if(rc<0)
		pr_bs(PR_ERR, "enable_irq_wake\n");

	rc = gpio_request(bs->swap_ctrl_en1, "swap_ctrl_en1_gpio");
	if (rc < 0)
		pr_bs(PR_ERR, "Couldn't get swap_ctrl_en1 rc = %d\n", rc);
	gpio_direction_input(bs->swap_ctrl_en1);

	rc = gpio_request(bs->swap_ctrl_en2, "swap_ctrl_en2_gpio");
	if (rc < 0)
		pr_bs(PR_ERR, "Couldn't get swap_ctrl_en2 rc = %d\n", rc);
	gpio_direction_output(bs->swap_ctrl_en2, 0);

	rc = gpio_request(bs->back_up_chg_disable, "back_up_chg_disable_gpio");
	if (rc < 0)
		pr_bs(PR_ERR, "Couldn't get back_up_chg_disable rc = %d\n", rc);
	gpio_direction_output(bs->back_up_chg_disable, 0);

	rc = gpio_request(bs->back_up_chg_stat, "back_up_chg_stat_gpio");
	if (rc < 0)
		pr_bs(PR_ERR, "Couldn't get back_up_chg_stat rc = %d\n", rc);
	gpio_direction_input(bs->back_up_chg_stat);

	return rc;
}

static int batt_swap_parse_dt(struct batt_swap *bs)
{
	int rc;
	struct device_node *node = bs->dev->of_node;

	if (!node) {
		pr_bs(PR_ERR, "device tree info. missing\n");
		return -EINVAL;
	}

	bs->vadc_dev = qpnp_get_vadc(bs->dev, "vbackup");
	if (IS_ERR(bs->vadc_dev)) {
		rc = PTR_ERR(bs->vadc_dev);
		if (rc != -EPROBE_DEFER)
			pr_bs(PR_ERR, "Couldn't get vbackup adc rc=%d\n",
					rc);
	}

	bs->bat_missing_det = of_get_named_gpio(node, "qcom,irq-gpio", 0);
	if((!gpio_is_valid(bs->bat_missing_det))){
		pr_bs(PR_ERR, "bat_missing_det gpio get name fail %d \n",
							bs->bat_missing_det);
		}

	bs->swap_ctrl_en1 = of_get_named_gpio(node,
					"qcom,swap_ctrl_en1-gpio", 0);
	if((!gpio_is_valid(bs->swap_ctrl_en1))){
		pr_bs(PR_ERR, "swap_ctrl_en1 gpio get name fail %d \n",
							bs->swap_ctrl_en1);
		}

	bs->swap_ctrl_en2 = of_get_named_gpio(node,
						"qcom,swap_ctrl_en2-gpio", 0);
	if((!gpio_is_valid(bs->swap_ctrl_en2))){
		pr_bs(PR_ERR, "swap_ctrl_en2 gpio get name fail %d \n",
							bs->swap_ctrl_en2);
		}

	bs->back_up_chg_disable = of_get_named_gpio(node,
						"qcom,backup_chg_disable-gpio", 0);
	if((!gpio_is_valid(bs->back_up_chg_disable))){
		pr_bs(PR_ERR, "back_up_chg_disable gpio get name fail %d \n",
						bs->back_up_chg_disable);
		}

	bs->back_up_chg_stat = of_get_named_gpio(node,
						"qcom,backup_chg_stat-gpio", 0);
	if((!gpio_is_valid(bs->back_up_chg_stat))){
		pr_bs(PR_ERR, "back_up_chg_stat gpio get name fail %d \n",
						bs->back_up_chg_stat);
		}

	return 0;
}

static int batt_swap_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct batt_swap *bs = NULL;

	bs = kzalloc(sizeof(struct batt_swap), GFP_KERNEL);
	if (bs == NULL) {
		pr_bs(PR_ERR, "Not Ready(bs)\n");
		rc = -EPROBE_DEFER;
		goto error;
	}

	platform_set_drvdata(pdev, bs);
	bs->dev = &pdev->dev;

	rc = batt_swap_parse_dt(bs);
	if (rc < 0) {
		pr_bs(PR_ERR,  "Unable to parse DT nodes: %d\n", rc);
		goto error;
	}

	bs->batt_psy = power_supply_get_by_name(BAT_PSY_NAME);
	if (bs->batt_psy == NULL) {
		pr_bs(PR_ERR, "Not Ready(batt_psy)\n");
		rc = -EPROBE_DEFER;
		goto error;
	}

	bs->bms_psy = power_supply_get_by_name(BMS_PSY_NAME);
	if (bs->bms_psy == NULL) {
		pr_bs(PR_ERR, "Not Ready(bms_psy)\n");
		rc =	-EPROBE_DEFER;
		goto error;
	}

	bs->batt_swap_psy.name = BS_PSY_NAME;
	bs->batt_swap_psy.properties = batt_swap_properties;
	bs->batt_swap_psy.num_properties = ARRAY_SIZE(batt_swap_properties);
	bs->batt_swap_psy.get_property = batt_swap_get_property;
	bs->batt_swap_psy.set_property = batt_swap_set_property;
	bs->batt_swap_psy.property_is_writeable = batt_swap_is_writeable;

	rc = power_supply_register(bs->dev, &bs->batt_swap_psy);
	if (rc < 0) {
		pr_bs(PR_ERR, "%s power_supply_register battery swap failed ret=%d\n",
			__func__, rc);
		goto error;
	}

	bs->usb_psy = power_supply_get_by_name(USB_PSY_NAME);
	if (bs->usb_psy == NULL) {
		pr_bs(PR_ERR, "Not Ready(usb_psy)\n");
		rc = -EPROBE_DEFER;
		goto error;
	}

	bs->dc_psy = power_supply_get_by_name(DC_PSY_NAME);
	if (bs->usb_psy == NULL) {
		pr_bs(PR_ERR, "Not Ready(dc_psy)\n");
		rc = -EPROBE_DEFER;
		goto error;
	}

	bs->bs_psy = power_supply_get_by_name(BS_PSY_NAME);
	if (bs->bs_psy == NULL) {
		pr_bs(PR_ERR, "Not Ready(battery_swap_psy)\n");
		rc = -EPROBE_DEFER;
		goto error;
	}

	INIT_DELAYED_WORK(&bs->bs_bvbat_check_work, batt_swap_bvbat_check_work);
	INIT_DELAYED_WORK(&bs->bs_backup_bat_chg_work, batt_swap_backup_bat_chg_work);
	INIT_DELAYED_WORK(&bs->bs_battery_remove_work, batt_swap_battery_remove_work);
	INIT_DELAYED_WORK(&bs->bs_battery_insert_work, batt_swap_battery_insert_work);

	wake_lock_init(&bs->swap_lock, WAKE_LOCK_SUSPEND, "battery_swap_wakelock");

	rc = batt_swap_hw_init(bs);
	if (rc < 0) {
		pr_bs(PR_ERR,
			"Unable to intialize hardware ret = %d\n", rc);
	}
	schedule_delayed_work(&bs->bs_bvbat_check_work,
			round_jiffies_relative(msecs_to_jiffies(BACKUP_VOLTAGE_CHK_TIME)));

	schedule_delayed_work(&bs->bs_backup_bat_chg_work,
			round_jiffies_relative(msecs_to_jiffies(BACK_UP_CHG_WORK_TIME)));

	pr_bs(PR_INFO, "Probe done\n");
	return rc;

error:
	kfree(bs);
	return rc;
}

static int batt_swap_remove(struct platform_device *pdev)
{
	struct batt_swap *bs = platform_get_drvdata(pdev);

	power_supply_unregister(&bs->batt_swap_psy);
	gpio_free(bs->bat_missing_det);
	gpio_free(bs->swap_ctrl_en1);
	gpio_free(bs->swap_ctrl_en2);
	gpio_free(bs->back_up_chg_disable);
	gpio_free(bs->back_up_chg_stat);
	kfree(bs);
	return 0;
}

#if defined(CONFIG_PM)
static int batt_swap_suspend(struct device *dev)
{
	return 0;
}

static int batt_swap_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops batt_swap_pm_ops = {
	.suspend	= batt_swap_suspend,
	.resume		= batt_swap_resume,
};
#endif

static struct of_device_id batt_swap_match_table[] = {
		{ .compatible = BATT_SWAP_NAME },
		{ },
};

static struct platform_driver batt_swap_driver = {
	.probe = batt_swap_probe,
	.remove = batt_swap_remove,
	.driver = {
		.name = BS_PSY_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm     = &batt_swap_pm_ops,
#endif
		.of_match_table = batt_swap_match_table,
	},
};

static int __init batt_swap_init(void)
{
	return platform_driver_register(&batt_swap_driver);
}

static void __exit batt_swap_exit(void)
{
	platform_driver_unregister(&batt_swap_driver);
}

late_initcall(batt_swap_init);
module_exit(batt_swap_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LGE Battery Swap");
