/* drivers/video/backlight/lm3630_bl.c
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>

#define I2C_BL_NAME                              "lm3697"
#define MAX_BRIGHTNESS_LM3697                    0xFF
#define MIN_BRIGHTNESS_LM3697                    0x0F
#define DEFAULT_BRIGHTNESS                       0xFF
#define DEFAULT_FTM_BRIGHTNESS                   0x0F

#define BL_ON        1
#define BL_OFF       0

/* LGE_CHANGE  - To turn backlight on by setting default brightness while kernel booting */
#define BOOT_BRIGHTNESS 1

static struct i2c_client *lm3697_i2c_client;

static int store_level_used = 0;

struct backlight_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int init_on_boot;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	int output_config;
	int brightness_config;
	int sink_feedback;
	int boost_ctrl;
};

struct lm3697_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	int output_config;
	int brightness_config;
	int sink_feedback;
	int boost_ctrl;
	struct mutex bl_mutex;
};

static const struct i2c_device_id lm3697_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
static int lm3697_read_reg(struct i2c_client *client, u8 reg, u8 *buf);
#endif

static int lm3697_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val);

static int cur_main_lcd_level = DEFAULT_BRIGHTNESS;
static int saved_main_lcd_level = DEFAULT_BRIGHTNESS;

static int backlight_status = BL_OFF;
static int lm3697_pwm_enable;
static struct lm3697_device *main_lm3697_dev;

static void lm3697_hw_reset(void)
{
	int gpio = main_lm3697_dev->gpio;
	/* LGE_CHANGE - Fix GPIO Setting Warning*/
	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		//gpio_set_value_cansleep(gpio, 1);
		mdelay(10);
	}
	else
		pr_err("%s: gpio is not valid !!\n", __func__);
}
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
static int lm3697_read_reg(struct i2c_client *client, u8 reg, u8 *buf)
{
	s32 ret;

	pr_debug("[LCD][DEBUG] reg: %x\n", reg);

	ret = i2c_smbus_read_byte_data(client, reg);

	if(ret < 0)
		pr_err("[LCD][DEBUG] error\n");

	*buf = ret;

	return 0;

}
#endif
static int lm3697_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int err;
	u8 buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = val;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		pr_info("i2c write error reg: %d, val: %d\n", buf[0], buf[1]  );

	return 0;
}

static int exp_min_value = 150;
static int cal_value;

/* TODO: rearange code */
static void lm3697_set_main_current_level(struct i2c_client *client, int level)
{
	struct lm3697_device *dev = i2c_get_clientdata(client);

	if (level == -BOOT_BRIGHTNESS)
		level = dev->default_brightness;

	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;

	store_level_used = 0;

	mutex_lock(&dev->bl_mutex);

	if (level != 0) {
		lm3697_write_reg(client, 0x21, level);
		//lm3697_write_reg(client, 0x23, level);
	} else {
		lm3697_write_reg(client, 0x24, 0x00);
	}

	mutex_unlock(&dev->bl_mutex);

	pr_info("[LCD][LM3697] %s : backlight level=%d, cal_value=%d \n", __func__, level, cal_value);
}

static void lm3697_set_main_current_level_no_mapping(
		struct i2c_client *client, int level)
{
	struct lm3697_device *dev;
	dev = (struct lm3697_device *)i2c_get_clientdata(client);

	if (level > 255)
		level = 255;
	else if (level < 0)
		level = 0;

	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;

	store_level_used = 1;

	mutex_lock(&main_lm3697_dev->bl_mutex);
	if (level != 0) {
		lm3697_write_reg(client, 0x21, level);
		//lm3697_write_reg(client, 0x23, level);
	} else {
		lm3697_write_reg(client, 0x24, 0x00);
	}
	mutex_unlock(&main_lm3697_dev->bl_mutex);
	pr_info("[LCD][LM3697] %s : backlight level=%d\n", __func__, level);
}

void lm3697_backlight_on(int level)
{
	if (backlight_status == BL_OFF) {

		pr_err("%s with level %d\n", __func__, level);
		lm3697_hw_reset();

		pr_err("[backlight] %s Enter lm3697 initial\n", __func__);

		lm3697_write_reg(main_lm3697_dev->client, 0x1A,
						main_lm3697_dev->boost_ctrl);	//OVP 32V, Freq 500kh
		lm3697_write_reg(main_lm3697_dev->client, 0x16,
						main_lm3697_dev->brightness_config);	//Exponential Mapping Mode
		lm3697_write_reg(main_lm3697_dev->client, 0x17,
						main_lm3697_dev->max_current);	//Bank A Full-scale current (20.2mA)
		lm3697_write_reg(main_lm3697_dev->client, 0x19,
						main_lm3697_dev->sink_feedback); //Sink Feedback Disable
		//lm3697_write_reg(main_lm3697_dev->client, 0x18, 0x13);	//Bank B Full-scale current (20.2mA)
		lm3697_write_reg(main_lm3697_dev->client, 0x10,
						main_lm3697_dev->output_config);	//HVLED 1, 2 bankA, HVLED 3 bank B enable
		lm3697_write_reg(main_lm3697_dev->client, 0x24, 0x01);	//Enable Bank A / Disable Bank B

		if( lm3697_pwm_enable ) {
			/* Enble Feedback , disable	PWM for BANK A,B */
		//	lm3697_write_reg(main_lm3697_dev->client, 0x01, 0x09);
		}
		else {
			/* Enble Feedback , disable	PWM for BANK A,B */
		//	lm3697_write_reg(main_lm3697_dev->client, 0x01, 0x08);
		}
	}
	mdelay(1);

	lm3697_set_main_current_level(main_lm3697_dev->client, level);
	backlight_status = BL_ON;

	return;
}

void lm3697_backlight_off(void)
{
	int gpio = main_lm3697_dev->gpio;

	if (backlight_status == BL_OFF)
		return;

	saved_main_lcd_level = cur_main_lcd_level;
	lm3697_set_main_current_level(main_lm3697_dev->client, 0);
	backlight_status = BL_OFF;

	gpio_direction_output(gpio, 0);
	msleep(6);

	pr_info("%s\n", __func__);
	return;
}

void lm3697_lcd_backlight_set_level(int level)
{
	if (level > MAX_BRIGHTNESS_LM3697)
		level = MAX_BRIGHTNESS_LM3697;

	pr_debug("### %s level = (%d) \n ", __func__, level);
	if (lm3697_i2c_client != NULL) {
		if (level == 0) {
			lm3697_backlight_off();
		} else {
			lm3697_backlight_on(level);
		}
	} else {
		pr_err("%s(): No client\n", __func__);
	}
}
EXPORT_SYMBOL(lm3697_lcd_backlight_set_level);

void lm3697_set_level_no_mapping(int level)
{
    lm3697_set_main_current_level_no_mapping(main_lm3697_dev->client, level);
}

void lm3697_set_ramp_time(u32 interval)
{
    u32 value;

    if(interval > 15)
        interval = 15;
    value = (interval << 4) | interval;
    pr_debug("%s() : set value : 0x%x\n", __func__, value);
    if (lm3697_i2c_client != NULL) {
        lm3697_write_reg(main_lm3697_dev->client, 0x13, value);
    } else {
        pr_err("%s(): No client\n", __func__);
    }
}

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_client *client = to_i2c_client(bd->dev.parent);

	/* LGE_CHANGE - if it's trying to set same backlight value, skip it.*/
	if(bd->props.brightness == cur_main_lcd_level){
		pr_debug("%s level is already set. skip it\n", __func__);
		return 0;
	}

	pr_debug("%s bd->props.brightness : %d     cur_main_lcd_level : %d \n", __func__, bd->props.brightness, cur_main_lcd_level);

	lm3697_set_main_current_level(client, bd->props.brightness);
	cur_main_lcd_level = bd->props.brightness;

	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	unsigned char val = 0;
	val &= 0x1f;

	return (int)val;
}

static ssize_t lcd_backlight_show_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	if(store_level_used == 0)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				cal_value);
	else if(store_level_used == 1)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				cur_main_lcd_level);

	return r;
}

static ssize_t lcd_backlight_store_level(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int level;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);

	lm3697_set_main_current_level_no_mapping(client, level);
	pr_info("[LCD][DEBUG] write %d direct to "
			"backlight register\n", level);

	return count;
}

static int lm3697_bl_resume(struct i2c_client *client)
{
	lm3697_lcd_backlight_set_level(saved_main_lcd_level);
	return 0;
}

static int lm3697_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("[LCD][DEBUG] %s: new state: %d\n",
			__func__, state.event);

	lm3697_lcd_backlight_set_level(saved_main_lcd_level);
	return 0;
}

static ssize_t lcd_backlight_show_on_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	pr_info("%s received (prev backlight_status: %s)\n",
			__func__, backlight_status ? "ON" : "OFF");

	return r;
}

static ssize_t lcd_backlight_store_on_off(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int on_off;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	pr_info("%s received (prev backlight_status: %s)\n",
			__func__, backlight_status ? "ON" : "OFF");

	on_off = simple_strtoul(buf, NULL, 10);

	pr_info("[LCD][DEBUG] %d", on_off);

	if (on_off == 1)
		lm3697_bl_resume(client);
	else if (on_off == 0)
		lm3697_bl_suspend(client, PMSG_SUSPEND);

	return count;

}
static ssize_t lcd_backlight_show_exp_min_value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;

	r = snprintf(buf, PAGE_SIZE, "LCD Backlight  : %d\n", exp_min_value);

	return r;
}

static ssize_t lcd_backlight_store_exp_min_value(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	exp_min_value = value;

	return count;
}
#if defined(CONFIG_BACKLIGHT_PARTIAL_MODE_SUPPORTED)
static ssize_t lcd_backlight_store_partial_on_off(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);

	if(value == 1){	// HVLED1
		lm3697_write_reg(main_lm3697_dev->client, 0x10,
			0x06);	//HVLED 1 Bank A
		lm3697_write_reg(main_lm3697_dev->client, 0x24, 0x01);	//Enable Bank A
	} else if(value == 2) {	// HVLED2
		lm3697_write_reg(main_lm3697_dev->client, 0x10,
			0x05);	//HVLED 2 Bank A
		lm3697_write_reg(main_lm3697_dev->client, 0x24, 0x01);	//Enable Bank A
	} else {	// HVLED1,2
		lm3697_write_reg(main_lm3697_dev->client, 0x10,
			main_lm3697_dev->output_config);	//HVLED 1,2 Bank A
		lm3697_write_reg(main_lm3697_dev->client, 0x24, 0x01);	//Enable Bank A
	}
	pr_info("[LM3697] %s EXT BL partial on off mode %d \n", __func__, value);
	return count;
}
#endif
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
static ssize_t lcd_backlight_show_pwm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	u8 level,pwm_low,pwm_high,config;

	mutex_lock(&main_lm3697_dev->bl_mutex);
	lm3697_read_reg(main_lm3697_dev->client, 0x01, &config);
	mdelay(3);
	lm3697_read_reg(main_lm3697_dev->client, 0x03, &level);
	mdelay(3);
	lm3697_read_reg(main_lm3697_dev->client, 0x12, &pwm_low);
	mdelay(3);
	lm3697_read_reg(main_lm3697_dev->client, 0x13, &pwm_high);
	mdelay(3);
	mutex_unlock(&main_lm3697_dev->bl_mutex);

	r = snprintf(buf, PAGE_SIZE, "Show PWM level: %d pwm_low: %d "
			"pwm_high: %d config: %d\n", level, pwm_low,
			pwm_high, config);

	return r;
}
static ssize_t lcd_backlight_store_pwm(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	return count;
}
#endif

DEVICE_ATTR(lm3697_level, 0644, lcd_backlight_show_level,
		lcd_backlight_store_level);
DEVICE_ATTR(lm3697_backlight_on_off, 0644, lcd_backlight_show_on_off,
		lcd_backlight_store_on_off);
DEVICE_ATTR(lm3697_exp_min_value, 0644, lcd_backlight_show_exp_min_value,
		lcd_backlight_store_exp_min_value);
#if defined(CONFIG_BACKLIGHT_PARTIAL_MODE_SUPPORTED)
DEVICE_ATTR(lm3697_partial_on_off, 0200, NULL,
		lcd_backlight_store_partial_on_off);
#endif
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
DEVICE_ATTR(lm3697_pwm, 0644, lcd_backlight_show_pwm, lcd_backlight_store_pwm);
#endif

//#ifdef CONFIG_OF
static int lm3697_parse_dt(struct device *dev,
		struct backlight_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	pdata->gpio = of_get_named_gpio_flags(np,
			"lm3697,lcd_bl_en", 0, NULL);
	rc = of_property_read_u32(np, "lm3697,max_current",
			&pdata->max_current);
	rc = of_property_read_u32(np, "lm3697,min_brightness",
			&pdata->min_brightness);
	rc = of_property_read_u32(np, "lm3697,default_brightness",
			&pdata->default_brightness);
	rc = of_property_read_u32(np, "lm3697,max_brightness",
			&pdata->max_brightness);
	rc = of_property_read_u32(np, "lm3697,output_config",
			&pdata->output_config);
	rc = of_property_read_u32(np, "lm3697,brightness_config",
			&pdata->brightness_config);
	rc = of_property_read_u32(np, "lm3697,sink_feedback",
			&pdata->sink_feedback);
	rc = of_property_read_u32(np, "lm3697,boost_ctrl",
			&pdata->boost_ctrl);

	rc = of_property_read_u32(np, "lm3697,enable_pwm",
			&lm3697_pwm_enable);
	if(rc == -EINVAL)
		lm3697_pwm_enable = 1;

	pr_info("[LM3697] %s gpio: %d, max_current: %d, min: %d, "
			"default: %d, max: %d, output_config: %d, brightness_config: %d, "
			"sink_feedback: %d, boost_ctrl: %d, pwm : %d \n",
			__func__, pdata->gpio,
			pdata->max_current,
			pdata->min_brightness,
			pdata->default_brightness,
			pdata->max_brightness,
			pdata->output_config,
			pdata->brightness_config,
			pdata->sink_feedback,
			pdata->boost_ctrl,
			lm3697_pwm_enable
			);

	return rc;
}
//#endif

static struct backlight_ops lm3697_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int lm3697_probe(struct i2c_client *i2c_dev,
		const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct lm3697_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err;

	pr_info("[LCD][LM3697] %s: i2c probe start\n", __func__);

	if (&i2c_dev->dev.of_node) {
		pdata = devm_kzalloc(&i2c_dev->dev,
				sizeof(struct backlight_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		err = lm3697_parse_dt(&i2c_dev->dev, pdata);
		if (err != 0)
			return err;
	} else {
		pdata = i2c_dev->dev.platform_data;
	}
	pr_info("[LCD][LM3697] %s: gpio = %d\n", __func__,pdata->gpio);
	if (pdata->gpio && gpio_request(pdata->gpio, "lm3697 reset") != 0) {
		return -ENODEV;
	}

	lm3697_i2c_client = i2c_dev;

	dev = kzalloc(sizeof(struct lm3697_device), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&i2c_dev->dev, "fail alloc for lm3697_device\n");
		return 0;
	}
	main_lm3697_dev = dev;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;

	props.max_brightness = MAX_BRIGHTNESS_LM3697;
	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev,
			NULL, &lm3697_bl_ops, &props);
	bl_dev->props.max_brightness = MAX_BRIGHTNESS_LM3697;
	bl_dev->props.brightness = DEFAULT_BRIGHTNESS;
	bl_dev->props.power = FB_BLANK_UNBLANK;

	dev->bl_dev = bl_dev;
	dev->client = i2c_dev;

	dev->gpio = pdata->gpio;
	dev->max_current = pdata->max_current;
	dev->min_brightness = pdata->min_brightness;
	dev->default_brightness = pdata->default_brightness;
	cur_main_lcd_level = dev->default_brightness;
	bl_dev->props.brightness = cur_main_lcd_level;
	dev->max_brightness = pdata->max_brightness;
	dev->output_config = pdata->output_config;
	dev->brightness_config = pdata->brightness_config;
	dev->sink_feedback = pdata->sink_feedback;
	dev->boost_ctrl = pdata->boost_ctrl;

	if (gpio_get_value(dev->gpio))
		backlight_status = BL_ON;
	else
		backlight_status = BL_OFF;

	i2c_set_clientdata(i2c_dev, dev);

	mutex_init(&dev->bl_mutex);

	err = device_create_file(&i2c_dev->dev,
			&dev_attr_lm3697_level);
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_lm3697_backlight_on_off);
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_lm3697_exp_min_value);
#if defined(CONFIG_BACKLIGHT_PARTIAL_MODE_SUPPORTED)
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_lm3697_partial_on_off);
#endif
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_lm3697_pwm);
#endif

#if 0 //Disable untill get the cont_splash_bootimg
	if (!lge_get_cont_splash_enabled())
		lm3697_lcd_backlight_set_level(0);
#endif
	return 0;
}

static int lm3697_remove(struct i2c_client *i2c_dev)
{
	struct lm3697_device *dev;
	int gpio = main_lm3697_dev->gpio;

	pr_info("[LCD][LM3697] %s: ++\n", __func__);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3697_level);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3697_backlight_on_off);
	dev = (struct lm3697_device *)i2c_get_clientdata(i2c_dev);
	backlight_device_unregister(dev->bl_dev);
	i2c_set_clientdata(i2c_dev, NULL);

	if (gpio_is_valid(gpio))
	{
		pr_info("[LCD][LM3697] %s: gpio %d free \n", __func__, gpio);
		gpio_free(gpio);
	}

	pr_info("[LCD][LM3697] %s: -- \n", __func__);
	return 0;
}

//#ifdef CONFIG_OF
static struct of_device_id lm3697_match_table[] = {
	{ .compatible = "backlight,lm3697",},
	{ },
};
//#endif

static struct i2c_driver main_lm3697_driver = {
	.probe = lm3697_probe,
	.remove = lm3697_remove,
	.suspend = NULL,
	.resume = NULL,
	.id_table = lm3697_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
//#ifdef CONFIG_OF
		.of_match_table = lm3697_match_table,
//#endif
	},
};

MODULE_DEVICE_TABLE(i2c, lm3697_bl_id);

static int __init lcd_backlight_init(void)
{
	static int err;

	err = i2c_add_driver(&main_lm3697_driver);

	return err;
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("LM3697 Backlight Control");
MODULE_AUTHOR("daewoo kwak");
MODULE_LICENSE("GPL");
