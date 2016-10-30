
#include <linux/printk.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/power/lge_friends_manager.h>
#include <linux/pinctrl/consumer.h>
#include <soc/qcom/lge/board_lge.h>

//typedef unsigned byte;
struct lge_friends_manager {
	bool			friends_connected;
	int			friends_type;
	int			friends_detect;	/* gpio */
	int			friends_detect_irq;
	int			friends_id;	/* gpio */
	int			friends_int;	/* gpio */
	int			friends_int_irq;
	int			vpwr_sw_en;	/* gpio */

	bool			usb_present;

	struct delayed_work	friends_det_work;
	struct delayed_work	friends_probe_work;

	struct device		*dev;
#ifdef CONFIG_OF
	struct device_node	*of_node;
#endif
	const char		*lge_friends_psy_name;
	struct power_supply 	lge_friends_psy;
	struct power_supply 	*usb_psy;
	struct power_supply 	*lge_friends_usb_psy;
	bool			lge_friends_psy_registered;
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	spinlock_t		*lock_time;
	struct power_supply	*batt_psy;
	int			friends_io;
	int			friends_io_pullup;
	int			io_irq;
	bool			receive_ack;
	byte			recieve_data;
	bool			start_flag;
	bool			end_flag;
	byte			connect_dev;
	byte			res_data;
	byte			command;
	bool			send_data;
#endif
	struct pinctrl		*friends_pinctrl;
	struct pinctrl_state	*gpio_state_active;
	struct pinctrl_state	*gpio_state_suspend;
};

#define FRIENDS_DET_DEBOUNCE_MS		(100)
#define FRIENDS_PROBE_TIMEOUT_MS	(5000)

#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
#define MAX_GPIO_NUM 	5
static void send_receive_ack(struct lge_friends_manager *lf);
int one_wire_com(struct lge_friends_manager *lf, byte command);
void recieve_data_from_client(struct lge_friends_manager *lf);
static void status_debug(struct lge_friends_manager *lf);
#endif

/* mode is that "in" is input mode and "out" is output mode.
   if you select output mode, the default set the value of low.
   strength is the driving current of port */
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
void change_io_mode(int gpio, int mode) {

	if (mode == GPIO_IN) {
		gpio_direction_input(gpio);
		pr_info("Gpio(%d) change input\n", gpio);
	} else if (mode == GPIO_OUT) {
		gpio_direction_output(gpio, GPIO_LOW);
		pr_info("Gpio(%d) change output\n", gpio);
	}
}

static byte read_one_bit(void) {
	byte value;

	change_io_mode(lf->friends_io, GPIO_OUT);
	udelay(10);
	change_io_mode(lf->friends_io, GPIO_IN);
	udelay(10);

	if (gpio_get_value(lf->friends_io))
		value = 1;
	else
		value = 0;

	udelay(40);

	return value;
}

static void write_ack_start(struct lge_friends_manager *lf) {
	change_io_mode(lf->friends_io, GPIO_OUT);
	gpio_set_value(lf->friends_io, GPIO_LOW);
	udelay(20);
	gpio_set_value(lf->friends_io, GPIO_HIGH);
	lf->start_flag = true;
}

static void write_ack_end(struct lge_friends_manager *lf) {
	change_io_mode(lf->friends_io, GPIO_OUT);
	gpio_set_value(lf->friends_io, GPIO_LOW);
	udelay(20);
	gpio_set_value(lf->friends_io, GPIO_HIGH);
	udelay(20);
}

static void write_zero(struct lge_friends_manager *lf) {
	change_io_mode(lf->friends_io, GPIO_OUT);
	gpio_set_value(lf->friends_io, GPIO_LOW);
	udelay(40);
	gpio_set_value(lf->friends_io, GPIO_HIGH);
	udelay(20);
	gpio_set_value(lf->friends_io, GPIO_LOW);
}

static void write_one_bit(voidstruct lge_friends_manager *lf) {
	gpio_set_value(lf->friends_io, GPIO_LOW);
	udelay(2);
	gpio_set_value(lf->friends_io, GPIO_HIGH);
	udelay(80);
}

static void write_byte(struct lge_friends_manager *lf,
		int data) {
	int bit;

	spin_lock(lf->lock_time);
	for(bit = 0; bit < 8; bit++) {
		if ((data & 1 << bit))
			write_one_bit();
		else
			write_zero();
	}
	spin_unlock(lf->lock_time);
}

static bool read_ack(struct lge_friends_manager *lf) {
	int loop;
	int response = 0;
	int check = 0;

	spin_lock(lf->lock_time);
	for (loop = 1; loop <= 4; loop++) {
		response = gpio_get_value(lf->friends_io);
		if (response == 0)
			check++;
		if (check >= 2)
			break;
		else if (check < 2 && loop < 4)
			pr_err("Not ready client\n");
		else if (check < 2 && loop == 4) {
			pr_err("Fail to read ack\n");
			spin_unlock(lf->lock_time);
			return false;
		} else
			udelay(10);
	}
	spin_unlock(lf->lock_time);

	return true;
}

static byte read_byte(struct lge_friends_manager *lf) {
	byte result = 0;
	int loop;
	byte temp_val = 0;

	spin_lock(lf->lock_time);
	for (loop = 0; loop < 8; loop++) {
		temp_val = read_one_bit();
		result = result | (temp_val << loop);
	}
	spin_unlock(lf->lock_time);

	return result;
}

#define MAX_COUNT 	3
static void send_receive_ack(struct lge_friends_manager *lf) {
	int ret = 0;
	static int loop;

	disable_irq_nosync(lf->io_irq);

	if (lf->receive_ack == false) {
		if (lf->start_flag == false &&
				lf->end_flag == false) {
			write_ack_start(lf);
		} else if (lf->start_flag == true &&
				lf->end_flag == false) {
			if (read_ack(lf)) {
				lf->start_flag = false;
				lf->receive_ack = true;
			} else {
				loop++;
				pr_err("Try to re-send count(%d)\n",
						loop);
				send_receive_ack(lf);
				if (loop > MAX_COUNT) {
					loop = 0;
					pr_err("No response client\n");
					return;
				}
			}
		}
/*	} else {
		ret = one_wire_com(lf, 0x00);
		if (ret < 0) {
			pr_err("Failed to communication\n");
			return;
		}*/
	}

	enable_irq(lf->io_irq);

	if (lf->receive_ack == true)
		lf->send_data = true;
}

static void update_power_class(struct lge_friends_manager *lf) {
	int rc;
	union power_supply_propval val = {0, };

	if (lf->batt_psy == NULL)
		lf->batt_psy = power_supply_get_by_name("battery");
	else {
		val.intval = lf->connect_dev;
		rc = lf->batt_psy->set_property(lf->batt_psy,
				POWER_SUPPLY_PROP_friends_type, &val);

		val.intval = lf->command;
		rc = lf->batt_psy->set_property(lf->batt_psy,
				POWER_SUPPLY_PROP_FRIENDS_COMMAND, &val);
	}
}

int check_connect_device(struct lge_friends_manager *lf) {
	return lf->connect_dev;
}

int check_friends_command(struct lge_friends_manager *lf) {
	return lf->command;
}

#define MAX_BYTE 	2
int one_wire_com(struct lge_friends_manager *lf,
		byte command) {

	disable_irq_nosync(lf->io_irq);
	lf->command = command;
	spin_lock(lf->lock_time);
	if (command == 0xF0) {
		write_byte(START_BYTE);
		udelay(10);
		write_byte(0x00);
		write_ack_end();
	} else {
		write_byte(lf->connect_dev);
		write_byte(command);
		write_ack_end();
	}
	spin_unlock(lf->lock_time);
	enable_irq(lf->io_irq);
	update_power_class(lf);

	return 0;
}

void recieve_data_from_client(struct lge_friends_manager *lf) {
	static int count;

	disable_irq_nosync(lf->io_irq);
	spin_lock(lf->lock_time);
	while(1) {
		lf->connect_dev = read_byte();
		udelay(10);
		lf->res_data = read_byte();
		if (read_ack(lf)) {
			lf->end_flag = true;
			break;
		}
		count++;
		if (count > 3) {
			lf->end_flag = false;
			pr_err("Failed to read data from client\n");
			break;
		}
	}
	spin_unlock(lf->lock_time);
	enable_irq(lf->io_irq);
	update_power_class(lf);
	status_debug(lf);

	pr_err("response1(0x%02x), response2(0x%02x)\n",
			lf->connect_dev, lf->res_data);
}

static void status_debug(struct lge_friends_manager *lf) {
	byte res_val;
	byte temp_val;
	int i = 7;

		res_val = lf->res_data;

		while (i == 4) {
			if ((res_val & (BIT(7) | BIT(6)))) {
				if (res_val == 0x80)
					pr_info ("Friend charge device\n");
				else
					pr_info ("Device charge friends\n");
			} else if ((res_val & BIT(5))) {
				temp_val = lf->res_data & (BIT(3) | BIT(2));
				switch (temp_val) {
					case LOW_TEMP:
						pr_info("Low temp status in friends\n");
						break;
					case NORMAL_TEMP:
						pr_info("Normal temp status in friends\n");
						break;
					case HIGH_TEMP:
						pr_info("HIGH temp status in friends\n");
						break;
					case WARN_TEMP:
						pr_err("the status of stop charging in friends\n");
						break;
				}
			} else if ((res_val & BIT(4))) {
				temp_val = lf->res_data & (BIT(1) | BIT(0));
				switch (temp_val) {
					case SHUTDOWN:
						pr_err("friends is shutdown\n");
						break;
					case LOW_LEVEL:
						pr_err("friends is the low-level status\n");
						break;
					case NORMAL_LEVEL:
						pr_info("friends is the normal status\n");
						break;
					case HIGH_LEVEL:
						pr_info("friends is the high-level status\n");
						break;
				}
			}
			i--;
		}
}
#endif
static int get_friends_detection(struct lge_friends_manager *lf) {
	bool connect = false;

	disable_irq_nosync(lf->friends_detect_irq);
	if (!gpio_get_value(lf->friends_detect))
		connect = true;
	else
		connect = false;
	enable_irq(lf->friends_detect_irq);

	return connect;
}

static void friends_set_vpwr_state(struct lge_friends_manager *lf, bool on)
{
	pr_info("friends vpwr turn %s\n", on ? "on" : "off");
	gpio_set_value(lf->vpwr_sw_en, on);
}

static int friends_get_vpwr_state(struct lge_friends_manager *lf)
{
	return gpio_get_value(lf->vpwr_sw_en);
}

static int friends_ecard_charge_done_check(struct lge_friends_manager *lf)
{
	if (lf->friends_type != FRIENDS_TYPE_ECARD)
		return 0;

	if (!gpio_get_value(lf->friends_int))
		return 0;

	if (!friends_get_vpwr_state(lf))
		return 0;

	pr_info("friends ecard charged.\n");
	friends_set_vpwr_state(lf, false);

	return 1;
}

static int friends_ecard_recharge_check(struct lge_friends_manager *lf)
{
	if (lf->friends_type != FRIENDS_TYPE_ECARD)
		return 0;

	if (friends_get_vpwr_state(lf))
		return 0;

	pr_info("friends ecard check recharge\n");
	friends_set_vpwr_state(lf, true);

	if (friends_ecard_charge_done_check(lf))
		return 0;

	return 1;
}

static int friends_get_charging_status(struct lge_friends_manager *lf)
{
	if (lf->friends_type == FRIENDS_TYPE_UNKNOWN)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	if (lf->friends_type == FRIENDS_TYPE_ECARD) {
		if (friends_get_vpwr_state(lf))
			return POWER_SUPPLY_STATUS_CHARGING;
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if (lf->friends_type == FRIENDS_TYPE_POWER_CONSUMER) {
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static bool is_usb_present(struct lge_friends_manager *lf)
{
	union power_supply_propval prop = {0,};
	if (!lf->usb_psy)
		lf->usb_psy = power_supply_get_by_name("usb");

	if (lf->usb_psy)
		lf->usb_psy->get_property(lf->usb_psy,
					    POWER_SUPPLY_PROP_PRESENT, &prop);
	return prop.intval != 0;
}

static int friends_parse_dt(struct lge_friends_manager *lf) {
	int rc = 0;
	struct device_node *node = lf->dev->of_node;

	if (!node) {
		pr_err("device tree info. missing\n");
		return -EINVAL;
	}

	lf->friends_detect = of_get_named_gpio(node, "lge,friends-detect-gpio", 0);
	if (lf->friends_detect < 0) {
		pr_err("Failed to get friends_detect. \n");
		rc = lf->friends_detect;
		goto friends_err;
	}

	lf->vpwr_sw_en = of_get_named_gpio(node, "lge,vpwr-sw-en", 0);
	if (lf->vpwr_sw_en < 0) {
		pr_err("Failed to get vpwr_sw_en. \n");
		rc = lf->vpwr_sw_en;
		goto friends_err;
	}


#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	if (lge_get_board_revno() <= HW_REV_0_1) {

		lf->friends_io = of_get_named_gpio(node,
				"lge,friends-io-gpio", 0);
		if (lf->friends_io < 0) {
			pr_err("Failed to get friends_io. \n");
			rc = lf->friends_io;
			goto friends_err;
		}

		lf->friends_io_pullup = of_get_named_gpio(node,
				"lge,friends-io-pullup-gpio", 0);
		if (lf->friends_io_pullup < 0) {
			pr_err("Failed to get friends_pullup. \n");
			rc = lf->friends_io_pullup;
			goto friends_err;
		}
	}
#endif
	lf->friends_id = of_get_named_gpio(node,
			"lge,friends-id-gpio", 0);
	if (lf->friends_id < 0) {
		pr_err("Failed to get friends_id\n");
		rc = lf->friends_id;
		goto friends_err;
	}
	lf->friends_int = of_get_named_gpio(node,
			"lge,friends-int-gpio", 0);
	if (lf->friends_int < 0) {
		pr_err("Failed to get friends_int\n");
		rc = lf->friends_int;
		goto friends_err;
	}

	pr_info("Success to get gpio on relevant friends'gpio\n");

	return 0;

friends_err:
	return rc;
}

static void friends_probe_wait_work(struct work_struct *work)
{
	struct lge_friends_manager *lf = container_of(to_delayed_work(work),
			struct lge_friends_manager, friends_probe_work);

	if (!lf->friends_connected) {
		lf->friends_type = FRIENDS_TYPE_UNKNOWN;
		return;
	}

	if (lf->friends_type == FRIENDS_TYPE_UNKNOWN) {
		pr_info("friends probe timeout. assume ecard\n");
		lf->friends_type = FRIENDS_TYPE_ECARD;
	}

	pr_info("friends probe device type : %d\n", lf->friends_type);
	if (friends_ecard_charge_done_check(lf))
		power_supply_changed(&lf->lge_friends_psy);

	enable_irq(lf->friends_int_irq);
}

static void friends_detect_work(struct work_struct *work)
{
	struct lge_friends_manager *lf = container_of(to_delayed_work(work),
			struct lge_friends_manager, friends_det_work);

	int connected;

	connected = get_friends_detection(lf);
	if (connected == lf->friends_connected) {
		pr_info("friends detect ignored\n");
		enable_irq(lf->friends_detect_irq);
		return;
	}

	lf->friends_connected = connected;
	pr_info("friends coneccted : %d\n", lf->friends_connected);

	friends_set_vpwr_state(lf, lf->friends_connected);

	if (lf->friends_connected) {
		pr_info("friend probe wait %dms\n", FRIENDS_PROBE_TIMEOUT_MS);
		schedule_delayed_work(&lf->friends_probe_work,
			round_jiffies_relative(msecs_to_jiffies(FRIENDS_PROBE_TIMEOUT_MS)));
	} else {
		cancel_delayed_work(&lf->friends_probe_work);

		if (lf->friends_type != FRIENDS_TYPE_UNKNOWN)
			disable_irq(lf->friends_int_irq);
		lf->friends_type = FRIENDS_TYPE_UNKNOWN;
	}
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	if (lf->connect_acc == true) {
		change_io_mode(lf->friends_io_pullup, GPIO_OUT);
		gpio_set_value(lf->friends_io_pullup, 1);
		if (!lf->receive_ack)
			send_receive_ack(lge_acc);
	} else {
		gpio_set_value(lf->friends_io_pullup, 0);
		change_io_mode(lf->friends_io_pullup, GPIO_IN);
		change_io_mode(lf->friends_io, GPIO_OUT);
		gpio_set_value(lf->friends_io, 0);
		change_io_mode(lf->friends_io, GPIO_IN);
	}
#endif
	power_supply_changed(&lf->lge_friends_psy);

	enable_irq(lf->friends_detect_irq);
}

static char *lge_friends_supplied_from[] = {
	"usb",
};

static enum power_supply_property lge_friends_properties[] = {
	POWER_SUPPLY_PROP_FRIENDS_DETECT,
	POWER_SUPPLY_PROP_FRIENDS_VPWR_SW,
	POWER_SUPPLY_PROP_FRIENDS_TYPE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	POWER_SUPPLY_PROP_FRIENDS_COMMAND,
#endif
};

static int lge_friends_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val) {
	struct lge_friends_manager *lf = container_of(psy,
			struct lge_friends_manager, lge_friends_psy);

	switch(prop) {
		case POWER_SUPPLY_PROP_FRIENDS_VPWR_SW:
			friends_set_vpwr_state(lf, val->intval);
			break;
		case POWER_SUPPLY_PROP_FRIENDS_TYPE:
			lf->friends_type = val->intval;
			cancel_delayed_work(&lf->friends_probe_work);
			if (lf->friends_connected)
				schedule_delayed_work(&lf->friends_probe_work, 0);
			break;
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
		case POWER_SUPPLY_PROP_FRIENDS_COMMAND:
			one_wire_com(lf, val->intval);
			break;
#endif
		default:
			return -EINVAL;
	}

	return 0;
}

static int lge_friends_is_writeable(struct power_supply *psy,
		enum power_supply_property prop) {
	int rc;

	switch (prop) {
		case POWER_SUPPLY_PROP_FRIENDS_VPWR_SW:
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
		case POWER_SUPPLY_PROP_FRIENDS_COMMAND:
#endif
			rc = 1;
			break;
		default:
			rc = 0;
			break;
	}
	return rc;
}

static int lge_friends_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val) {
	struct lge_friends_manager *lf = container_of(psy,
				struct lge_friends_manager, lge_friends_psy);

	switch(prop) {
		case POWER_SUPPLY_PROP_FRIENDS_DETECT:
			val->intval = lf->friends_connected;
			break;
		case POWER_SUPPLY_PROP_FRIENDS_VPWR_SW:
			val->intval = friends_get_vpwr_state(lf);
			break;
		case POWER_SUPPLY_PROP_FRIENDS_TYPE:
			val->intval = lf->friends_type;
			if (!lf->friends_connected) {
				val->intval = FRIENDS_TYPE_UNKNOWN;
			}
			break;
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
		case POWER_SUPPLY_PROP_FRIENDS_COMMAND:
			val->intval = check_friends_command();
			break;
#endif
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = friends_get_charging_status(lf);
			break;
		case POWER_SUPPLY_PROP_CHARGE_NOW:
			val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = 0;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

void lge_friends_external_power_changed(struct power_supply *psy)
{
	struct lge_friends_manager *lf = container_of(psy,
			struct lge_friends_manager, lge_friends_psy);
	bool usb_present;

	usb_present = is_usb_present(lf);
	if (lf->usb_present == usb_present)
		return;

	lf->usb_present = usb_present;

	if (!lf->friends_connected)
		return;

	if (usb_present && friends_ecard_recharge_check(lf))
		power_supply_changed(&lf->lge_friends_psy);
}

static irqreturn_t friends_det_irq(int irq, void *dev_id)
{
	struct lge_friends_manager *lf = dev_id;

	if (lf->lge_friends_psy_registered == false) {
		pr_info("friends det_irq too early. ignore\n");
		return IRQ_HANDLED;
	}
	disable_irq_nosync(lf->friends_detect_irq);

	pr_info("friends det_irq triggered\n");

	schedule_delayed_work(&lf->friends_det_work,
		msecs_to_jiffies(FRIENDS_DET_DEBOUNCE_MS));

	return IRQ_HANDLED;
}

static irqreturn_t friends_int_irq(int irq, void *dev_id)
{
	struct lge_friends_manager *lf = dev_id;

	if (!lf->friends_connected)
		return IRQ_HANDLED;

	pr_info("friends int_irq triggered\n");

	/* ignore interrupt when friends not probed */
	if (lf->friends_type == FRIENDS_TYPE_UNKNOWN) {
		pr_info("friends not probed yet\n");
		return IRQ_HANDLED;
	}

	if (friends_ecard_charge_done_check(lf))
		power_supply_changed(&lf->lge_friends_psy);

	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
static irqreturn_t friends_io_irq(int irq, void *dev_id) {
	struct lge_friends_manager *lf = dev_id;

	if (lf->receiver_ack) {
		if (lf->send_data == true)
			one_wire_com(lf, lf->command);
		else
			recieve_data_from_client(lf);
	}

	return IRQ_HANDLED;
}
#endif

int friends_enable_pinctrl(struct device *dev,
		struct lge_friends_manager *lf) {
	int rc = 0;

	lf->gpio_state_active = lf->gpio_state_suspend = 0;
	lf->friends_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(lf->friends_pinctrl)) {
		pr_err("Failed to use pinctrl\n");
		return -ENODEV;
	}

	lf->gpio_state_active = pinctrl_lookup_state
				(lf->friends_pinctrl, "friends_active_pins");
	if (IS_ERR_OR_NULL(lf->gpio_state_active)) {
		pr_err("Failed to use friends_detect_active\n");
		goto err_active_state;
	}

	lf->gpio_state_suspend = pinctrl_lookup_state
		(lf->friends_pinctrl, "friends_sleep_pins");
	if (IS_ERR_OR_NULL(lf->gpio_state_suspend)) {
		pr_err("Failed to use friends_detect_suspend\n");
		goto err_suspend_state;
	}

	if(pinctrl_select_state(lf-> friends_pinctrl,
				lf->gpio_state_active))
		pr_err("Error on pinctrl_select_state for host wake\n");
	else
		pr_info("Seccess on pinctrl_select_state for host wake\n");

	return rc;

err_suspend_state:
	lf->gpio_state_suspend = 0;
err_active_state:
	lf->gpio_state_active = 0;
	devm_pinctrl_put(lf->friends_pinctrl);
	lf->friends_pinctrl = 0;

	return rc;
}

static int friends_init_gpio(struct lge_friends_manager *lf)
{
	int rc;

	rc = gpio_request_one(lf->friends_detect, GPIOF_DIR_IN,
			"friends_detect_pin");
	if (rc) {
		pr_err("Failed to request friends_detect\n");
		return rc;
	}

	rc = gpio_request_one(lf->vpwr_sw_en,
			GPIOF_DIR_OUT | GPIOF_OUT_INIT_LOW,
			"enable-booster");
	if (rc) {
		pr_err("Failed to request friends_booster\n");
		return rc;
	}

	if (gpio_is_valid(lf->friends_id)) {
		rc = gpio_request_one(lf->friends_id,
				GPIOF_DIR_IN, "friends_id");
		if (rc) {
			pr_err("Failed to request friends_id\n");
			return rc;
		}
	}
	if (gpio_is_valid(lf->friends_int)) {
		rc = gpio_request_one(lf->friends_int,
			GPIOF_DIR_IN,"friends_int");
		if (rc) {
			pr_err("Failed to request friends_int\n");
			return rc;
		}
	}

	return rc;
}

static void friends_deinit_gpio(struct lge_friends_manager *lf)
{
	gpio_free(lf->friends_detect);
	gpio_free(lf->vpwr_sw_en);
	gpio_free(lf->friends_id);
	gpio_free(lf->friends_int);
}

static int friends_init_irq(struct lge_friends_manager *lf)
{
	int rc;

	lf->friends_detect_irq = -EINVAL;
	lf->friends_int_irq = -EINVAL;

	lf->friends_detect_irq = gpio_to_irq(lf->friends_detect);
	if (lf->friends_detect < 0) {
		pr_err("Failed to request friends_detect_irq\n");
		return -EINVAL;
	}

	rc = request_irq(lf->friends_detect_irq, friends_det_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"friends_detect_irq", lf);
	if (rc) {
		pr_err("Failed to request friends_detect_irq setting\n");
		return rc;
	}

	rc = enable_irq_wake(lf->friends_detect_irq);
	if (rc) {
		pr_err("Failed to wake friends_detect_irq\n");
		return rc;
	}
	disable_irq(lf->friends_detect_irq);

	lf->friends_int_irq = gpio_to_irq(lf->friends_int);
	if (lf->friends_int < 0) {
		pr_err("Failed to request friends_detect_irq\n");
		return -EINVAL;
	}
	rc = request_threaded_irq(lf->friends_int_irq, NULL, friends_int_irq,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
		"friends_int_irq", lf);
	if (rc) {
		pr_err("Failed to request friends_int_irq setting\n");
		return rc;
	}

	rc = enable_irq_wake(lf->friends_int_irq);
	if (rc) {
		pr_err("Failed to wake friends_int_irq\n");
		return rc;
	}
	disable_irq(lf->friends_int_irq);

	return rc;
}

static void friends_deinit_irq(struct lge_friends_manager *lf)
{
	if (lf->friends_detect_irq >= 0)
		free_irq(lf->friends_detect_irq, lf);
	if (lf->friends_int_irq >= 0)
		free_irq(lf->friends_int_irq, lf);
}

static int lge_friends_manager_probe(struct platform_device *pdev)
{
	int rc;
	struct lge_friends_manager *lf;

	lf = kzalloc(sizeof(struct lge_friends_manager), GFP_KERNEL);
	if (!lf) {
		pr_err("Failed to alloc memory for one_wire\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, lf);
	lf->dev = &pdev->dev;

	rc = friends_parse_dt(lf);
	if (rc) {
		pr_err("Failed to parse from dt : %d\n", rc);
		goto fail_gpio;
	}

	lf->lge_friends_psy_name = "friends";

	INIT_DELAYED_WORK(&lf->friends_det_work, friends_detect_work);
	INIT_DELAYED_WORK(&lf->friends_probe_work, friends_probe_wait_work);

	rc = friends_init_gpio(lf);
	if (rc) {
		pr_err("Failed to init gpio\n");
		goto fail_gpio;
	}
	rc = friends_init_irq(lf);
	if (rc) {
		pr_err("Failed to init irq\n");
		goto fail_irq;
	}

#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	if (lge_get_board_revno() <= HW_REV_0_1) {
		rc = gpio_request_one(lf->friends_io,
				GPIOF_DIR_IN, "friends_io_pin");
		if (rc) {
			pr_err("Failed to request friends_io\n");
			goto fail_gpio;
		}

		rc = gpio_request_one(lf->friends_io_pullup,
				GPIOF_DIR_OUT | GPIOF_OUT_INIT_HIGH,
				"friends_io_pin");
		if (rc) {
			pr_err("Failed to request friends_io_pullup\n");
			goto fail_gpio;
		}

		lf->io_irq = gpio_to_irq(lf->friends_io);
		if (lf->io_irq < 0) {
			pr_err("Failed to request io_irq\n");
			goto fail_irq;
		}

		rc = request_irq(lf->io_irq, friends_io_irq,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"friends_detect_irq", lf);
		if (rc) {
			pr_err("Failed to request io_irq setting\n");
			goto fail_irq;
		}

		rc = enable_irq_wake(lf->io_irq);
		if (rc) {
			pr_err("Failed to waek io_irq\n");
			goto fail_irq;
		}
	}
#endif
	rc = friends_enable_pinctrl(&pdev->dev, lf);
	if (rc) {
		pr_err("Failed to enable pinctrl\n");
		goto fail_irq;
	}

	lf->lge_friends_usb_psy = power_supply_get_by_name("friends_usb");
	if (!lf->lge_friends_usb_psy) {
		rc = -EPROBE_DEFER;
		pr_err("Failed to find friends_usb power class. try again.\n");
		goto fail_irq;
	}

#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	lf->batt_psy = power_supply_get_by_name("battery");
	if (lf->batt_psy == NULL) {
		rc = -EPROBE_DEFER;
		pr_err("Failed to find battery power class. try again.\n");
		goto fail_irq;
	}
#endif
	lf->friends_connected = 0;
	lf->friends_type = FRIENDS_TYPE_UNKNOWN;

	lf->lge_friends_psy.name = lf->lge_friends_psy_name;
	lf->lge_friends_psy.get_property = lge_friends_get_property;
	lf->lge_friends_psy.set_property = lge_friends_set_property;
	lf->lge_friends_psy.properties = lge_friends_properties;
	lf->lge_friends_psy.num_properties = ARRAY_SIZE(lge_friends_properties);
	lf->lge_friends_psy.property_is_writeable = lge_friends_is_writeable;
	lf->lge_friends_psy.external_power_changed = lge_friends_external_power_changed;
	lf->lge_friends_psy.supplied_from = lge_friends_supplied_from;
	lf->lge_friends_psy.num_supplies = ARRAY_SIZE(lge_friends_supplied_from);

	rc = power_supply_register(&pdev->dev, &lf->lge_friends_psy);
	if (rc < 0) {
		pr_err("lge_friends failed to register rc = %d\n", rc);
		goto fail_irq;
	}
	lf->lge_friends_psy_registered = true;

	/* get initial status */
	schedule_delayed_work(&lf->friends_det_work,
		round_jiffies_relative(msecs_to_jiffies(FRIENDS_DET_DEBOUNCE_MS)));

	pr_err("lge_friends_manager probe success\n");

	return rc;

fail_irq:
	friends_deinit_irq(lf);
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	if(lf->io_irq)
		free_irq(lf->io_irq, ow, lf);
#endif
fail_gpio:
	friends_deinit_gpio(lf);
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	if (lge_get_board_revno() <= HW_REV_0_1) {

		if (lf->friends_io >= 0)
			gpio_free(lf->friends_io);
		if (lf->friends_io_pullup >= 0)
			gpio_free(lf->friends_io_pullup);

	}
#endif
	kfree(lf);
	lf = NULL;

	pr_err("lge_friends_manager probe unsuccess\n");

	return rc;
}

static struct of_device_id lge_friends_match_table[] = {
	{
		.compatible 		= "lge,friends-manager",
	},
	{ },
};

static void lge_friends_manager_unregister(struct lge_friends_manager *lf)
{
	device_unregister(lf->dev);

	power_supply_unregister(&lf->lge_friends_psy);
//	sysfs_remove_link(&lf->dev->kobj, "lf");

	friends_deinit_irq(lf);
	friends_deinit_gpio(lf);
#ifdef CONFIG_LGE_PM_SUPPORT_ONE_WIRE
	if (lge_get_board_revno() <= HW_REV_0_1) {
		if (lf->friends_io >= 0)
			gpio_free(lf->friends_io);
		if (lf->friends_io_pullup >= 0)
			gpio_free(lf->friends_io_pullup);
	}
#endif
}

static int lge_friends_manager_remove(struct platform_device *dev)
{
	struct lge_friends_manager *lf = platform_get_drvdata(dev);

	cancel_delayed_work(&lf->friends_det_work);
	cancel_delayed_work(&lf->friends_probe_work);

	lge_friends_manager_unregister(lf);

	kfree(lf);
	lf = NULL;
	return 0;
}

static int lge_friends_manager_suspend(struct device *dev)
{
	struct lge_friends_manager *lf = dev_get_drvdata(dev);

	lf->friends_pinctrl = devm_pinctrl_get(dev);

	if (!IS_ERR(lf->friends_pinctrl)) {
	lf->gpio_state_suspend = pinctrl_lookup_state
		(lf->friends_pinctrl, "friends_sleep_pins");
		if (!IS_ERR(lf->gpio_state_suspend))
			pinctrl_select_state(lf->friends_pinctrl,
					lf->gpio_state_suspend);
	}

	if (lf && device_may_wakeup(dev))
		enable_irq_wake(lf->friends_detect_irq);

	pr_info("friends enter susend\n");

	return 0;
}

static int lge_friends_manager_resume(struct device *dev)
{
	struct lge_friends_manager *lf = dev_get_drvdata(dev);

	lf->friends_pinctrl = devm_pinctrl_get(dev);

	lf->gpio_state_active = pinctrl_lookup_state
		(lf->friends_pinctrl, "friends_active_pins");
		if (!IS_ERR(lf->gpio_state_active))
			pinctrl_select_state(lf->friends_pinctrl,
					lf->gpio_state_active);
	if (lf && device_may_wakeup(dev))
		enable_irq_wake(lf->friends_detect_irq);

	pr_info("friends enter resume\n");

	return 0;
}

static const struct dev_pm_ops lge_friends_pm_ops = {
	.suspend 			= lge_friends_manager_suspend,
	.resume 			= lge_friends_manager_resume,
};

static struct platform_driver lge_friends_manager_driver = {
	.probe 				= lge_friends_manager_probe,
	.remove 			= lge_friends_manager_remove,
	.driver = {
		.name 			= "lge_friends_manager_drv",
		.owner 			= THIS_MODULE,
		.of_match_table = lge_friends_match_table,
		.pm 			= &lge_friends_pm_ops,
	},
};

static int __init lge_friends_manager_init(void) {
	return platform_driver_register(&lge_friends_manager_driver);
}

static void __exit lge_friends_manager_exit(void) {
	return platform_driver_unregister(&lge_friends_manager_driver);
}

module_init(lge_friends_manager_init);
module_exit(lge_friends_manager_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ksue");
MODULE_DESCRIPTION("LGE Support friendsessary");
