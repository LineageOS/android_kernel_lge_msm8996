
#define DEBUG
#define pr_fmt(fmt) "CABLE-DETECTION: %s: " fmt, __func__
#define pr_detection(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/platform_device.h>

#include "inc-limit-voter.h"

/********************************************************
 * macro                                                *
 *********************************************************/

#define OF_PROP_READ_S32(dnode, buf, prop, rc) \
do {										   \
	if (rc)									   \
		break;								   \
										       \
	rc = of_property_read_s32(dnode, "lge," prop, &buf); \
										       \
	if (rc)									   \
		pr_detection("Error reading " #prop " property rc = %d\n", rc); \
	else									   \
		pr_detection("%s : %d\n", prop, buf);	   \
} while (0)

#define OF_PROP_READ_STR(dnode, buf, prop, rc) \
do {										   \
	if (rc)									   \
		break;								   \
										       \
	rc = of_property_read_string(dnode, "lge," prop, &buf); \
								               \
	if (rc)									   \
		pr_detection("Error reading " #prop " property rc = %d\n", rc); \
	else									   \
		pr_detection("%s : %s\n", prop, buf);	   \
} while (0)

// will be changed.. print buf
#define OF_PROP_READ_ARR(dnode, buf, count, prop, rc) \
    do { \
        if (rc) \
            break; \
                   \
        rc = of_property_read_u32_array((dnode), (prop), (buf), (count)); \
                                                   \
        if (rc)                                    \
            pr_detection("Error reading " #prop " property rc = %d\n", rc); \
        else {                                     \
            pr_detection("%s : %d %d\n", prop, buf[0], buf[1]);  \
        }                                          \
    } while (0)

#define OF_PROP_READ_GPIO(dnode, buf, prop) \
    do { \
        (buf) = of_get_named_gpio(dnode, prop, 0); \
        \
        if ((buf) < 0) \
            pr_detection("Error reading " #prop " property rc = %d\n", (buf)); \
        else \
            pr_detection("%s : %d\n", prop, (buf)); \
    } while (0)
            

/* for only this module */
#define set_chg_current(type) \
    do { \
        iusb_current = current_info[(type)].iusb_current; \
        ibat_current = current_info[(type)].ibat_current; \
    } while (0)
    
#define CABLE_DETECTION_COMPATIBLE  "lge,cable-detection"
#define CABLE_DETECTION_DRIVER      "lge-cable-detection"
#define CABLE_DETECTION_NAME        "cable-detection"

//#define SBU_SEL_GPIO      78
//#define OUT_HIGH          1
//#define OUT_LOW           0

//#define USB_ID_CH         0xe

/********************************************************
 * enum                                                 *
 *********************************************************/

typedef enum {
   CABLE_BOOT_FACTORY_56K = 6,
   CABLE_BOOT_FACTORY_130K,
   CABLE_BOOT_USB_NORMAL_400MA,
   CABLE_BOOT_USB_DTC_500MA,
   CABLE_BOOT_USB_ABNORMAL_400MA,
   CABLE_BOOT_FACTORY_910K,
   CABLE_BOOT_NONE
} cable_boot_type;

typedef enum {
   CABLE_ADC_NO_INIT = 0,
   CABLE_ADC_56K,
   CABLE_ADC_130K,
   CABLE_ADC_910K,
   CABLE_ADC_NONE,
   CABLE_ADC_MAX,
} cable_adc_type;

typedef enum {
    CABLE_TYPE_FACTORY = 0,
    CABLE_TYPE_SDP,
    CABLE_TYPE_CDP,
    CABLE_TYPE_DCP,
    CABLE_TYPE_QC20,
    CABLE_TYPE_QC30,
    CABLE_TYPE_TYPEC,
    CABLE_TYPE_TYPEC_PD,
    CABLE_TYPE_MAX,
} cable_type;

/********************************************************
 * structure                                            *
 *********************************************************/

struct cable_info_table {
   cable_adc_type  type;
   int			   threshhold_low;
   int			   threshhold_high;
};

struct cable_current {
    int iusb_current;
    int ibat_current;
};

struct cable_detection {
	/* cable information */
    struct cable_info_table cable_info[CABLE_ADC_MAX];
    struct cable_current cable_current[CABLE_TYPE_MAX];
    
    /* voter */
    struct limit_voter detect_iusb_voter;
    struct limit_voter detect_ibat_voter;

	/* cable info */
    cable_adc_type  adc_cable_type;

    /* vadc for usb_id */
    struct qpnp_vadc_chip*  vadc_usb_id;
	/* usb id channel */    
    int usb_id_channel;

    /* mutex lock */
    struct mutex cable_detection_lock;

    /* gpios for SBU_EN, SBU_SEL */
    int sbu_en;
    int sbu_sel;
};

/********************************************************
 * variables                                            *
 *********************************************************/

static struct cable_detection *cable_detection_me;
static int boot_cable_type;

/********************************************************
 *   functions                                          *
 *********************************************************/

bool is_factory_mode(void){
    bool ret = false;

    switch(boot_cable_type){
        case CABLE_BOOT_FACTORY_56K   :
        case CABLE_BOOT_FACTORY_130K  :
        case CABLE_BOOT_FACTORY_910K  :
            ret = true;
            break;
        default :
            ret = false;
            break;
    }

    return ret;
}

bool is_factory_cable(void){
    bool ret = false;

    switch(cable_detection_me->adc_cable_type){
        case CABLE_ADC_56K  :
        case CABLE_ADC_130K :
        case CABLE_ADC_910K :
            ret = true;
            break;
        default :
            ret = false;
            break;
    }

    return ret;
}

static int cable_detection_get_usb_id_adc(void)
{
	int usb_id_adc, i;
	struct qpnp_vadc_result results;

    if (!cable_detection_me) {
        pr_err("cable_detection is not probed yet \n");
        return -99; //// will be changed
    }

    mutex_lock(&cable_detection_me->cable_detection_lock);

    /* SBU_EN low, SBU_SEL high */
    //gpiod_set_value(gpio_to_desc(cable_detection_me->sbu_en), false);
    //gpiod_set_value(gpio_to_desc(cable_detection_me->sbu_sel), true);
    
    qpnp_vadc_read(cable_detection_me->vadc_usb_id, 
                   cable_detection_me->usb_id_channel, 
                   &results);
    // will be added exception handler
    
    /* SBU_EN high, SBU_SEL low */
    //gpiod_set_value(gpio_to_desc(cable_detection_me->sbu_sel), false);
    //gpiod_set_value(gpio_to_desc(cable_detection_me->sbu_en), true);

    mutex_unlock(&cable_detection_me->cable_detection_lock);

    usb_id_adc = (int)results.physical;
    for(i = 0; i < CABLE_ADC_MAX; i++) {
        if (usb_id_adc >= cable_detection_me->cable_info[i].threshhold_low &&
            usb_id_adc <= cable_detection_me->cable_info[i].threshhold_high) {
            cable_detection_me->adc_cable_type = 
                cable_detection_me->cable_info[i].type;
            break;
        }
    }

    pr_detection("cable id adc %d cable id %d\n", usb_id_adc, i);

    /* set as open cable, if not in range */
    if (CABLE_ADC_MAX == i) {
        cable_detection_me->adc_cable_type = 
            cable_detection_me->cable_info[CABLE_ADC_NONE].type;

        pr_detection("cable id adc is not in range\n");
    }

    return usb_id_adc;
}

static int cable_detection_get_cable_type(char* name){
    int ret = 0;
	union power_supply_propval val = {0, };
    struct power_supply *psy = power_supply_get_by_name(name);
    
    if (psy && psy->get_property) {
		psy->get_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
		ret = val.intval;

        pr_detection("%s : cable type : %d\n", name, val.intval);
	} else {
        if (!psy) {
            pr_err("%s psy is not ready yet!\n", name);
            return -100; // will be changed
        } else {
            pr_err("%s psy does not have get prop func\n", name);
            return -200; // will be changed
        }
	}
    
    return ret;
}

int cable_detection_decision_chg_current(
    int usb_type, int usb_c_type, struct cable_current *chg_current) {
    int ret = 0, iusb_current, ibat_current;
    struct cable_current *current_info = cable_detection_me->cable_current;

    /* init iusb, ibat as 0 */
    set_chg_current(0);

    /* decision iusb, ibat */
    if (is_factory_cable()) {
        set_chg_current(CABLE_TYPE_FACTORY);
    } else {
        switch (usb_type) {
            case POWER_SUPPLY_TYPE_USB :
                set_chg_current(CABLE_TYPE_SDP);
                break;
            case POWER_SUPPLY_TYPE_USB_CDP :
                set_chg_current(CABLE_TYPE_CDP);
                break;
            case POWER_SUPPLY_TYPE_USB_DCP :
                set_chg_current(CABLE_TYPE_DCP);
                break;
            case POWER_SUPPLY_TYPE_USB_HVDCP:
                set_chg_current(CABLE_TYPE_HVDCP);
                break;
            case POWER_SUPPLY_TYPE_USB_HVDCP_3 :
                set_chg_current(CABLE_TYPE_HVDCP3);
                break;
            default :
                set_chg_current(CABLE_TYPE_SDP);       
                break;
        }

        switch (usb_c_type) {
            case POWER_SUPPLY_TYPE_CTYPE :
                set_chg_current(CABLE_TYPE_TYPEC);
                break;
            case POWER_SUPPLY_TYPE_CTYPE_PD :
                set_chg_current(CABLE_TYPE_TYPEC_PD);
                break;
            default :
                /* none. for non-ctype cable */
                break;
        }
    }
    
    chg_current->iusb_current = iusb_current;
    chg_current->ibat_current = ibat_current;

    return ret;
}

int cable_detection_charger_detect(void){
    int ret = 0;
    int usb_type, usb_c_type;
    struct cable_current chg_current;

    if (!cable_detection_me) {
        pr_err("cable_detection is not probed yet \n");
        return -99; //// will be changed
    }

    /* get type from usb, usb_pd */
	usb_type = cable_detection_get_cable_type("usb");
    if (usb_type < 0) {
        pr_err("usb psy is not ready\n");
        return usb_type; //ret;
    }
    
	usb_c_type = cable_detection_get_cable_type("usb_pd");
    if (usb_c_type < 0) {
        pr_err("usb_pd psy is not ready\n");
        return usb_type; //ret;
    }

    /* read usb id adc */
    ret = cable_detection_get_usb_id_adc();
    if (ret == -99) {
        pr_err("vadc is not ready\n");
        return ret;
    }
    
    cable_detection_decision_chg_current(usb_type, usb_c_type, &chg_current);

    pr_detection("usb type %d ctype %d iusb %d ibat %d\n",
        usb_type, usb_c_type, chg_current.iusb_current, chg_current.ibat_current);
        
    /* vote if battery-veneer is exist */
    if (power_supply_get_by_name("battery-veneer")) {
        limit_voter_set(&cable_detection_me->detect_iusb_voter, chg_current.iusb_current);
        limit_voter_set(&cable_detection_me->detect_ibat_voter, chg_current.ibat_current);
    }
    
    return ret;
}


/********************************************************
 * init functions                                       *
 *********************************************************/

static int cable_detection_probe_gpios(struct device_node* dev_node,
    struct cable_detection* detection_me) {
	int ret = 0;

    OF_PROP_READ_GPIO(dev_node, detection_me->sbu_en,  "lge,gpio-sbu-en");
    OF_PROP_READ_GPIO(dev_node, detection_me->sbu_sel, "lge,gpio-sbu-sel");

    /* SBU_EN(MSM gpio90) initialize */
	ret = gpio_request_one(detection_me->sbu_en, GPIOF_OUT_INIT_HIGH, "gpio-sbu-en");
	if (ret < 0) {
		pr_err("Fail to request gpio_sbu_en\n");
		return ret;
	}

    /* SBU_SEL(PM gpio3) initialize */
    //ret = gpio_request_one(detection_me->sbu_sel, GPIOF_OUT_INIT_LOW, "gpio-sbu-sel");
	//if (ret < 0) {
	//	pr_err("Fail to request gpio_sbu_sel\n");
	//	return ret;
	//}
    
	return ret;
}

static int cable_detection_probe_cable_info(struct device_node* dev_node,
	struct cable_detection* detection_me) {
    int i, rc = 0;
    u32 cable_value[2];
    const char *propname_cable_type[CABLE_TYPE_MAX] = {
        "lge,factory-cable-current",
        "lge,sdp-cable-current",
        "lge,cdp-cable-current",
        "lge,dcp-cable-current",
        "lge,qc20-cable-current",
        "lge,qc30-cable-current",
        "lge,typec-cable-current",
        "lge,typec-pd-cable-current"
    };
    const char *propname_usb_id[CABLE_ADC_MAX] = {
        "lge,no-init-cable",
        "lge,cable-56k",
        "lge,cable-130k",
        "lge,cable-910k",
        "lge,cable-none"
    };
    
    OF_PROP_READ_S32(dev_node, detection_me->usb_id_channel, "usb-id-chan", rc);

    for (i = 0; i < CABLE_TYPE_MAX; i++) {
        OF_PROP_READ_ARR(dev_node, cable_value, sizeof(cable_value) / sizeof(cable_value[0]),
            propname_cable_type[i], rc);

        detection_me->cable_current[i].iusb_current = cable_value[0];
        detection_me->cable_current[i].ibat_current = cable_value[1];
    }

    for (i = 0 ; i < CABLE_ADC_MAX; i++) {
        OF_PROP_READ_ARR(dev_node, cable_value, sizeof(cable_value) / sizeof(cable_value[0]),
            propname_usb_id[i], rc);

        detection_me->cable_info[i].type            = i;//cable_adc_type[i];
        detection_me->cable_info[i].threshhold_low  = cable_value[0];
        detection_me->cable_info[i].threshhold_high = cable_value[1];
    }

    return rc;
}

static int cable_detection_probe_voter(struct device_node* dev_node,
	struct cable_detection* detection_me) {
	int rc = 0;

	const char* voter_name;
	int voter_type;

    /* setting iusb voter */
	OF_PROP_READ_STR(dev_node, voter_name, "iusb-voter-name", rc);
	OF_PROP_READ_S32(dev_node, voter_type, "iusb-voter-type", rc);
	if (rc)
		return rc;

	rc = limit_voter_register(&detection_me->detect_iusb_voter, voter_name,
			voter_type, NULL, NULL, NULL);
	if (rc)
		return rc;

    /* setting ibat voter */
    OF_PROP_READ_STR(dev_node, voter_name, "ibat-voter-name", rc);
	OF_PROP_READ_S32(dev_node, voter_type, "ibat-voter-type", rc);
	if (rc)
		return rc;

	rc = limit_voter_register(&detection_me->detect_ibat_voter, voter_name,
			voter_type, NULL, NULL, NULL);
	if (rc)
		return rc;

	return rc;
}


static int cable_detection_probe(struct platform_device *pdev) {
	struct cable_detection* detection_me;
	struct device_node *dev_node = pdev->dev.of_node;
	int ret;

	pr_detection("Start\n");

	detection_me = kzalloc(sizeof(struct cable_detection), GFP_KERNEL);
	if (!detection_me) {
		pr_err("Failed to alloc memory\n");
		goto error;
	}

	detection_me->vadc_usb_id = qpnp_get_vadc(&pdev->dev, "usb_id");
	if (IS_ERR(detection_me->vadc_usb_id)) {
		ret = PTR_ERR(detection_me->vadc_usb_id);
		if (ret != -EPROBE_DEFER) {
			pr_err("usb_id property fail to get\n");
		} else {
			pr_err("not yet initializeing vadc\n");
			goto err_hw_init;
		}
	}

    mutex_init(&detection_me->cable_detection_lock);

	ret = cable_detection_probe_cable_info(dev_node, detection_me);
	if (ret < 0) {
		pr_err("Fail to parse parameters\n");
		goto err_hw_init;
	}

    ret = cable_detection_probe_gpios(dev_node, detection_me);
	if (ret < 0) {
		pr_err("Fail to request gpio at probe\n");
		goto err_hw_init;
	}

	ret = cable_detection_probe_voter(dev_node, detection_me);
	if (ret < 0) {
		pr_err("Fail to preset structure\n");
		goto err_hw_init;
	}

	platform_set_drvdata(pdev, detection_me);

    cable_detection_me = detection_me;

	pr_detection("Complete probing\n");
	return 0;

err_hw_init:
	kfree(detection_me);
    if (detection_me->sbu_en)
		gpio_free(detection_me->sbu_en);
	if (detection_me->sbu_sel)
		gpio_free(detection_me->sbu_sel);
error:
	pr_err("Failed to probe\n");

	return ret;
}

static int cable_detection_remove(struct platform_device *pdev) {
	struct cable_detection *cable_detection = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	kfree(cable_detection);
	return 0;
}

static const struct of_device_id cable_detection_match [] = {
	{ .compatible = CABLE_DETECTION_COMPATIBLE },
	{ },
};

static const struct platform_device_id cable_detection_id [] = {
	{ CABLE_DETECTION_DRIVER, 0 },
	{ },
};

static struct platform_driver cable_detection_driver = {
	.driver = {
		.name = CABLE_DETECTION_DRIVER,
		.owner = THIS_MODULE,
		.of_match_table = cable_detection_match,
	},
	.probe    = cable_detection_probe,
	.remove   = cable_detection_remove,
	.id_table = cable_detection_id,
};

static int __init cable_detection_init(void) {
	return platform_driver_register(&cable_detection_driver);
}

static void __exit cable_detection_exit(void) {
	platform_driver_unregister(&cable_detection_driver);
}

static int __init boot_cable_init(char *boot_cable){
    if (!strcmp(boot_cable, "LT_56K"))
        boot_cable_type = CABLE_BOOT_FACTORY_56K;
    else if (!strcmp(boot_cable, "LT_130K"))
        boot_cable_type = CABLE_BOOT_FACTORY_130K;
    else if (!strcmp(boot_cable, "400MA"))
        boot_cable_type = CABLE_BOOT_USB_NORMAL_400MA;
    else if (!strcmp(boot_cable, "DTC_500MA"))
        boot_cable_type = CABLE_BOOT_USB_DTC_500MA;
    else if (!strcmp(boot_cable, "Abnormal_400MA"))
        boot_cable_type = CABLE_BOOT_USB_ABNORMAL_400MA;
    else if (!strcmp(boot_cable, "LT_910K"))
        boot_cable_type = CABLE_BOOT_FACTORY_910K;
    else if (!strcmp(boot_cable, "NO_INIT"))
        boot_cable_type = CABLE_BOOT_NONE;
    else
        boot_cable_type = CABLE_BOOT_NONE;

    pr_detection("Boot cable : %s %d\n", boot_cable, boot_cable_type);

    return 1;
}

__setup("bootcable.type=", boot_cable_init);

module_init(cable_detection_init);
module_exit(cable_detection_exit);

MODULE_DESCRIPTION(CABLE_DETECTION_DRIVER);
MODULE_LICENSE("GPL v2");



#if 0

typedef enum {
    CABLE_TYPE_BOOT,
    CABLE_TYPE_ADC,
} cable_type_;


int get_ohm(cable_type_ type){
    switch(type){
        case CABLE_TYPE_BOOT :
            if(boot_cable_type == CABLE_BOOT_FACTORY_56K)       return 56;
            else if(boot_cable_type == CABLE_BOOT_FACTORY_130K) return 130;
            else if(boot_cable_type == CABLE_BOOT_FACTORY_910K) return 910;
            else                                      return 0;
            break;
        case CABLE_TYPE_ADC :
            if(boot_cable_type == CABLE_ADC_56K)       return 56;
            else if(boot_cable_type == CABLE_ADC_130K) return 130;
            else if(boot_cable_type == CABLE_ADC_910K) return 910;
            else                                       return 0;
            break;
        default :
    }
}

int get_ohm_boot_cable(void){
    return get_ohm(CABLE_TYPE_BOOT);
}

int get_ohm_adc_cable(void){
    return get_ohm(CABLE_TYPE_ADC);
}

#endif
