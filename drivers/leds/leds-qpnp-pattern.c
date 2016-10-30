#include "leds-qpnp.c"
#include "leds-qpnp-pattern.h"

/* Valueless constants just for indication
 */
#define PWM_PERIOD_US                   1000
#define LPG_NEED_TO_SET                 -1

/* Features used internally
 */
#define PATTERN_FOR_HIDDEN_MENU         true;

/* Factors for brightness tuning
 */
#define BRIGHTNESS_BASE_RGB             255
#define BRIGHTNESS_BASE_LUT             511
#define BRIGHTNESS_BASE_PCT             100



#if defined(CONFIG_MACH_MSM8996_H1)
#define TUNING_LUT_SCALE                 51
#define TUNING_PCT_RED                  200
#define TUNING_PCT_GREEN                100
#define TUNING_PCT_BLUE                  32
#else
#define TUNING_LUT_SCALE                BRIGHTNESS_BASE_LUT
#define TUNING_PCT_RED                  BRIGHTNESS_BASE_PCT
#define TUNING_PCT_GREEN                BRIGHTNESS_BASE_PCT
#define TUNING_PCT_BLUE                 BRIGHTNESS_BASE_PCT
#endif
/* The brightness scale value should be in [0, BRIGHTNESS_MAX_FOR_LUT].
 *
 * The Maximum scale value depends on the maximum PWM size.
 * In the case of QPNP PMI8994, the supported PWM sizes are listed like
 *     qcom,supported-sizes = <6>, <7>, <9>;
 * and BRIGHTNESS_MAX_FOR_LUT is defined as (2^9)-1 = 511
 *
 * When it needs to define the maximum LED brightness,
 *     test it with qpnp_pattern_scale().
 * And when maximum LED brightness is defined,
 *     set the qpnp_brightness_scale to BRIGHTNESS_MAX_SCALE
 */
static int                    qpnp_brightness_scale = TUNING_LUT_SCALE;

static struct qpnp_led_data*  qpnp_led_red     = NULL;
static struct qpnp_led_data*  qpnp_led_green   = NULL;
static struct qpnp_led_data*  qpnp_led_blue    = NULL;

static struct led_pattern_ops qpnp_pattern_ops = {
	.select = qpnp_pattern_select,
	.input  = qpnp_pattern_input,
	.blink  = qpnp_pattern_blink,
	.onoff  = qpnp_pattern_onoff,
	.scale  = qpnp_pattern_scale
};

static void qpnp_pattern_resister(void)
{
	led_pattern_register(&qpnp_pattern_ops);
}

static void qpnp_pattern_config(struct qpnp_led_data *led)
{
	if (led->id == QPNP_ID_RGB_RED)
		qpnp_led_red    = led;
	else if (led->id == QPNP_ID_RGB_GREEN)
		qpnp_led_green  = led;
	else if (led->id == QPNP_ID_RGB_BLUE)
		qpnp_led_blue   = led;
	else
		pr_err("Invalide QPNP_ID_RGB %d\n", led->id);

	if( qpnp_led_red && qpnp_led_green && qpnp_led_blue )
		qpnp_pattern_resister();
}

static int qpnp_pattern_scenario_index(enum qpnp_pattern_scenario scenario)
{
	switch (scenario)
	{
	case PATTERN_SCENARIO_DEFAULT_OFF                   :   return 0;

	case PATTERN_SCENARIO_POWER_ON                      :   return 1;
	case PATTERN_SCENARIO_POWER_OFF                     :   return 2;
	case PATTERN_SCENARIO_LCD_ON                        :   return 3;

	case PATTERN_SCENARIO_CHARGING                      :   return 4;
	case PATTERN_SCENARIO_CHARGING_FULL                 :   return 5;

	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_FAVORITE   :   return 6;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_URGENT     :   return 7;

	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_GREEN      :   return 8;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_BLUE       :   return 9;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_PINK       :   return 10;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_YELLOW     :   return 11;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_ORANGE     :   return 12;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_TURQUOISE  :   return 13;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_PURPLE     :   return 14;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_RED        :   return 15;
	case PATTERN_SCENARIO_MISSED_NOTI_REPEAT_LIME       :   return 16;

	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_GREEN     :   return 17;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_BLUE      :   return 18;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_PINK      :   return 19;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_YELLOW    :   return 20;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_ORANGE    :   return 21;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_TURQUOISE :   return 22;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_PURPLE    :   return 23;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_RED       :   return 24;
	case PATTERN_SCENARIO_MISSED_NOTI_ONESHOT_LIME      :   return 25;
	case PATTERN_SCENARIO_MISSED_NOTI_SECRETMODE_REPEAT_CYAN : return 26;
	case PATTERN_SCENARIO_MISSED_NOTI_SECRETMODE_ONESHOT_CYAN : return 27;

#ifdef PATTERN_FOR_HIDDEN_MENU
/* If PATTERN_FOR_HIDDEN_MENU is enabled,
 * the pattern numbers 0~15 of seek-bar are mapped to ...
 *       0 -> PATTERN_SCENARIO_DEFAULT_OFF
 *       1 -> PATTERN_SCENARIO_POWER_ON
 *       2 -> PATTERN_SCENARIO_LCD_ON
 *       3 -> PATTERN_SCENARIO_CHARGING
 *       4 -> PATTERN_SCENARIO_CHARGING_FULL
 *       5 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_URGENT
 *       6 -> PATTERN_SCENARIO_POWER_OFF
 *       7 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_GREEN
 *       8 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_BLUE
 *       9 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_PINK
 *      10 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_YELLOW
 *      11 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_ORANGE
 *      12 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_TURQUOISE
 *      13 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_PURPLE
 *      14 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_RED
 *      15 -> PATTERN_SCENARIO_MISSED_NOTI_REPEAT_LIME
 */
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_5           :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_URGENT);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_8           :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_BLUE);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_9           :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_PINK);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_10          :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_YELLOW);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_11          :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_ORANGE);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_12          :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_TURQUOISE);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_13          :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_PURPLE);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_14          :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_RED);
	case PATTERN_SCENARIO_HIDDEN_MENU_BLANK_15          :
		return qpnp_pattern_scenario_index(PATTERN_SCENARIO_MISSED_NOTI_REPEAT_LIME);
#endif
	default :
		break;
	}

	return -1;
}

static int* qpnp_pattern_scenario_parameter(enum qpnp_pattern_scenario scenario)
{
	int parameter_index = qpnp_pattern_scenario_index(scenario);

	if (parameter_index > -1)
		return qpnp_pattern_parameter[parameter_index];
	else
		return NULL;
}

static void qpnp_pattern_print(int parameter_pattern [])
{
	int     i = 0;

	printk("[RGB LED] LUT TABLE is \n");
	for (i = 0; i < PATTERN_SIZE_LUT; i++)
		printk("%d ", parameter_pattern[i]);
	printk("\n");

	printk("[RGB LED][RED] START:%d, LENGTH:%d\n",
		parameter_pattern[PATTERN_INDEX_RED_START], parameter_pattern[PATTERN_INDEX_RED_LENGTH]);
	printk("[RGB LED][GRN] START:%d, LENGTH:%d\n",
		parameter_pattern[PATTERN_INDEX_GREEN_START], parameter_pattern[PATTERN_INDEX_GREEN_LENGTH]);
	printk("[RGB LED][BLU] START:%d, LENGTH:%d\n",
		parameter_pattern[PATTERN_INDEX_BLUE_START], parameter_pattern[PATTERN_INDEX_BLUE_LENGTH]);
	printk("[RGB LED][COM] PAUSE_LO:%d, PAUSE_HI:%d, PAUSE_STEP:%d, FLAGS:%x\n",
		parameter_pattern[PATTERN_INDEX_PAUSE_LO], parameter_pattern[PATTERN_INDEX_PAUSE_HI],
		parameter_pattern[PATTERN_INDEX_PAUSE_STEP], parameter_pattern[PATTERN_INDEX_FLAGS]);
}

static int qpnp_pattern_play(int parameter_pattern [])
{
	struct lut_params       parameter_lut;
	int                     parameter_scaled[PATTERN_SIZE_LUT];

	struct pwm_device*      pwm_dev_red     = qpnp_led_red->rgb_cfg->pwm_cfg->pwm_dev;
	struct pwm_device*      pwm_dev_green   = qpnp_led_green->rgb_cfg->pwm_cfg->pwm_dev;
	struct pwm_device*      pwm_dev_blue    = qpnp_led_blue->rgb_cfg->pwm_cfg->pwm_dev;

	int     pattern_red_start       = parameter_pattern[PATTERN_INDEX_RED_START];
	int     pattern_red_length      = parameter_pattern[PATTERN_INDEX_RED_LENGTH];
	int     pattern_green_start     = parameter_pattern[PATTERN_INDEX_GREEN_START];
	int     pattern_green_length    = parameter_pattern[PATTERN_INDEX_GREEN_LENGTH];
	int     pattern_blue_start      = parameter_pattern[PATTERN_INDEX_BLUE_START];
	int     pattern_blue_length     = parameter_pattern[PATTERN_INDEX_BLUE_LENGTH];

	int     return_code     = 0;
	int*    table_ptr       = NULL;
	int     i               = 0;

	/* Apply scale factor to LUT(Look Up Table) entries to meet LG LED brightness guide */
	for( i=0; i<PATTERN_SIZE_LUT; i++ )
		parameter_scaled[i] = parameter_pattern[i] * qpnp_brightness_scale / BRIGHTNESS_BASE_RGB;

	/* If R/G/B share their LUT, then SKIP the individual color tuning */
	if (0 < pattern_red_length &&    // Whether red == green
		pattern_red_start==pattern_green_start && pattern_red_length==pattern_green_length )
		goto skip_color_tuning;
	if (0 < pattern_green_length &&  // Whether green == blue
		pattern_green_start==pattern_blue_start && pattern_green_length==pattern_blue_length )
		goto skip_color_tuning;
	if (0 < pattern_blue_length &&   // Whether blue == red
		pattern_blue_start==pattern_red_start && pattern_blue_length==pattern_red_length )
		goto skip_color_tuning;

	/* Apply R/G/B tuning factor to LUT(Look Up Table) entries for white balance */
	for ( i=0; i<PATTERN_SIZE_LUT; i++ ) {
		int lut_tuning;

		if (pattern_red_start<=i && i<pattern_red_start+pattern_red_length)
			lut_tuning = TUNING_PCT_RED;
		else if (pattern_green_start<=i && i<pattern_green_start+pattern_green_length)
			lut_tuning = TUNING_PCT_GREEN;
		else if (pattern_blue_start<=i && i<pattern_blue_start+pattern_blue_length)
			lut_tuning = TUNING_PCT_BLUE;
		else
			lut_tuning = 0;

		parameter_scaled[i] = parameter_scaled[i] * lut_tuning / BRIGHTNESS_BASE_PCT;
	}

skip_color_tuning:
	/* LUT config : Set R/G/B common parameters. */
	parameter_lut.lut_pause_lo      = parameter_pattern[PATTERN_INDEX_PAUSE_LO];
	parameter_lut.lut_pause_hi      = parameter_pattern[PATTERN_INDEX_PAUSE_HI];
	parameter_lut.ramp_step_ms      = parameter_pattern[PATTERN_INDEX_PAUSE_STEP];
	parameter_lut.flags             = parameter_pattern[PATTERN_INDEX_FLAGS];

	/* LUT config : for RED */
	if (pattern_red_length > 0) {
		parameter_lut.start_idx = pattern_red_start;
		parameter_lut.idx_len   = pattern_red_length;

		table_ptr   = &parameter_scaled[pattern_red_start];
		return_code = pwm_lut_config(pwm_dev_red, PWM_PERIOD_US, table_ptr, parameter_lut);
		if (return_code < 0)
			goto failed_to_play_pattern;

		qpnp_led_red->cdev.brightness = LED_FULL;
	}
	else
		qpnp_led_red->cdev.brightness = LED_OFF;

	/* LUT config : for GREEN */
	if (pattern_green_length > 0) {
		parameter_lut.start_idx = pattern_green_start;
		parameter_lut.idx_len   = pattern_green_length;

		table_ptr   = &parameter_scaled[pattern_green_start];
		return_code = pwm_lut_config(pwm_dev_green, PWM_PERIOD_US, table_ptr, parameter_lut);
		if (return_code < 0)
			goto failed_to_play_pattern;

		qpnp_led_green->cdev.brightness = LED_FULL;
	}
	else
		qpnp_led_green->cdev.brightness = LED_OFF;

	/* LUT config : for BLUE */
	if (pattern_blue_length > 0) {
		parameter_lut.start_idx = pattern_blue_start;
		parameter_lut.idx_len   = pattern_blue_length;

		table_ptr   = &parameter_scaled[pattern_blue_start];
		return_code = pwm_lut_config(pwm_dev_blue, PWM_PERIOD_US, table_ptr, parameter_lut);
		if (return_code < 0)
			goto failed_to_play_pattern;

		qpnp_led_blue->cdev.brightness = LED_FULL;
	}
	else
		qpnp_led_blue->cdev.brightness = LED_OFF;

	usleep_range(100,100);

	/* Control(Turn ON/OFF) LEDs at the same time */
	return_code = qpnp_rgb_set(qpnp_led_red);
	if (return_code < 0)
		goto failed_to_play_pattern;

	return_code = qpnp_rgb_set(qpnp_led_green);
	if (return_code < 0)
		goto failed_to_play_pattern;

	return_code = qpnp_rgb_set(qpnp_led_blue);
	if (return_code < 0)
		goto failed_to_play_pattern;

	return 0;

failed_to_play_pattern :
	pr_err("Failed to play pattern\n");
	return return_code;
}

static int qpnp_pattern_solid(struct qpnp_led_data *led)
{
	int                     parameter_solid[2] = {LPG_NEED_TO_SET, LPG_NEED_TO_SET};
	struct lut_params       parameter_lut;

	struct pwm_device*      led_pwm_dev     = led->rgb_cfg->pwm_cfg->pwm_dev;
	int                     led_id          = led->id;
	char*                   led_name        = NULL;

	int                     lut_start       = -1;
	int                     lut_tuning      = 0;
	switch (led_id)
	{
	case QPNP_ID_RGB_RED :
		led_name    = "RED";
		lut_start   = 0;
		lut_tuning  = TUNING_PCT_RED;
		break;
	case QPNP_ID_RGB_GREEN :
		led_name    = "GREEN";
		lut_start   = 2;
		lut_tuning  = TUNING_PCT_GREEN;
		break;
	case QPNP_ID_RGB_BLUE :
		led_name    = "BLUE";
		lut_start   = 4;
		lut_tuning  = TUNING_PCT_BLUE;
		break;
	default :
		pr_err("Invalid LED ID\n");
		return -EINVAL;
	}

	parameter_solid[0] = parameter_solid[1] = (led->cdev.brightness & 0xFF)
			* qpnp_brightness_scale / BRIGHTNESS_BASE_RGB * lut_tuning / BRIGHTNESS_BASE_PCT;

	parameter_lut.lut_pause_hi      = 0;
	parameter_lut.lut_pause_lo      = 0;
	parameter_lut.ramp_step_ms      = 0;
	parameter_lut.flags             = PATTERN_FLAG_SOLID;

	parameter_lut.start_idx         = lut_start;
	parameter_lut.idx_len           = 2;

	if (parameter_solid[0] > 0) {
		int return_code = pwm_lut_config(led_pwm_dev, PWM_PERIOD_US, parameter_solid, parameter_lut);
		if (return_code)
			printk("pwm_lut_config failed : %d\n", return_code);
	}

	usleep_range(100,100);
	printk("LED : %s, COLOR : %d\n", led_name, led->cdev.brightness);
	return qpnp_rgb_set(led);
}

static void qpnp_pattern_turnoff(void)
{
	int* turnoff_pattern = qpnp_pattern_scenario_parameter(PATTERN_SCENARIO_DEFAULT_OFF);

	qpnp_pattern_play(turnoff_pattern);
}

static ssize_t qpnp_pattern_select(const char* string_select, size_t string_size)
{
	enum qpnp_pattern_scenario select_scenario = PATTERN_SCENARIO_DEFAULT_OFF;
	int                        select_number   = 0;
	int*                       select_pattern  = NULL;

	if (sscanf(string_select, "%d", &select_number) != 1) {
		printk("[RGB LED] bad arguments\n");
		goto select_error;
	}

	select_scenario = select_number;
	select_pattern  = qpnp_pattern_scenario_parameter(select_scenario);

	if (select_pattern == NULL) {
		printk("Invalid led pattern value : %d, Turn off all LEDs\n", select_scenario);
		goto select_error;
	}

	printk("[RGB LED] Play pattern %d, (%s)\n",
		select_scenario, qpnp_pattern_scenario_name(select_scenario));

	qpnp_pattern_play(select_pattern);
	return string_size;

select_error :
	qpnp_pattern_turnoff();
	return -EINVAL;
}

static ssize_t qpnp_pattern_input(const char* string_input, size_t string_size)
{
	int     input_pattern[PATTERN_SIZE_ARRAY];

	if (sscanf(string_input, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,0x%02x",
		/* 0 .................................................. 23 */   // [LUT TABLE]
		&input_pattern[ 0], &input_pattern[ 1], &input_pattern[ 2],
		&input_pattern[ 3], &input_pattern[ 4], &input_pattern[ 5],
		&input_pattern[ 6], &input_pattern[ 7], &input_pattern[ 8],
		&input_pattern[ 9], &input_pattern[10], &input_pattern[11],
		&input_pattern[12], &input_pattern[13], &input_pattern[14],
		&input_pattern[15], &input_pattern[16], &input_pattern[17],
		&input_pattern[18], &input_pattern[19], &input_pattern[20],
		&input_pattern[21], &input_pattern[22], &input_pattern[23],
		/*         [START]            [LENGTH]                     */
		&input_pattern[24], &input_pattern[25],                         // [RED]
		&input_pattern[26], &input_pattern[27],                         // [GREEN]
		&input_pattern[28], &input_pattern[29],                         // [BLUE]
		/*      [PAUSE_LO]          [PAUSE_HI]        [PAUSE_STEP] */   // [R/G/B COMMON]
		&input_pattern[30], &input_pattern[31], &input_pattern[32],
		/*         [FLAGS]                                         */
		&input_pattern[33]) != PATTERN_SIZE_ARRAY) {
			printk("[RGB LED] bad arguments ");

			qpnp_pattern_turnoff();
			return -EINVAL;
	}

	qpnp_pattern_print(input_pattern);
	qpnp_pattern_play(input_pattern);
	return string_size;
}

static ssize_t qpnp_pattern_blink(const char* string_blink, size_t string_size)
{
	int     blink_rgb       = 0;
	int     blink_on        = 0;
	int     blink_off       = 0;

	int     blink_pattern[] = {
		LPG_NEED_TO_SET, 0,     // [0] : Blink color for RED
		LPG_NEED_TO_SET, 0,     // [2] : Blink color for GREEN
		LPG_NEED_TO_SET, 0,     // [4] : Blink color for BLUE
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

		0, 2,
		2, 2,
		4, 2,

		LPG_NEED_TO_SET, LPG_NEED_TO_SET, PATTERN_STEP_DEFAULT,
		PATTERN_FLAG_BLINK
	};

	if (sscanf(string_blink, "0x%06x,%d,%d", &blink_rgb, &blink_on, &blink_off) != 3) {
		printk("[RGB LED] led_pattern_blink() bad arguments ");

		qpnp_pattern_turnoff();
		return -EINVAL;
	}

	printk("[RGB LED] rgb:%06x, on:%d, off:%d\n", blink_rgb, blink_on, blink_off);

	blink_pattern[0]    = (0xFF & (blink_rgb >> 16));
	blink_pattern[2]    = (0xFF & (blink_rgb >> 8));
	blink_pattern[4]    = (0xFF & (blink_rgb));

	blink_pattern[PATTERN_INDEX_PAUSE_LO]   = blink_on;
	blink_pattern[PATTERN_INDEX_PAUSE_HI]   = blink_off;

	qpnp_pattern_play(blink_pattern);
	return string_size;
}

static ssize_t qpnp_pattern_onoff(const char* string_onoff, size_t string_size)
{
	int     onoff_rgb        = 0;
	int     onoff_pattern [] = {
		LPG_NEED_TO_SET, LPG_NEED_TO_SET,     // [0][1] : Solid color for RED
		LPG_NEED_TO_SET, LPG_NEED_TO_SET,     // [2][3] : Solid color for GREEN
		LPG_NEED_TO_SET, LPG_NEED_TO_SET,     // [4][5] : Solid color for BLUE
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

		0, 2,
		2, 2,
		4, 2,

		PATTERN_PAUSE_DISABLED, PATTERN_PAUSE_DISABLED, PATTERN_PAUSE_DISABLED,
		PATTERN_FLAG_ONESHOT
	};

	if (sscanf(string_onoff, "0x%06x", &onoff_rgb) != 1) {
		printk("[RGB LED] led_pattern_onoff() bad arguments ");

		qpnp_pattern_turnoff();
		return -EINVAL;
	}

	onoff_pattern[0] = onoff_pattern[1] = (0xFF & (onoff_rgb >> 16));
	onoff_pattern[2] = onoff_pattern[3] = (0xFF & (onoff_rgb >> 8));
	onoff_pattern[4] = onoff_pattern[5] = (0xFF & (onoff_rgb));

	qpnp_pattern_play(onoff_pattern);
	return string_size;
}

static ssize_t qpnp_pattern_scale(const char* string_scale, size_t string_size)
{
	if (sscanf(string_scale, "%d", &qpnp_brightness_scale) != 1) {
		printk("[RGB LED] qpnp_pattern_scale() bad arguments ");

		qpnp_pattern_turnoff();
		return -EINVAL;
	}

	return string_size;
}

