//#define DEBUG
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/thermal.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/virtual_temp_sensor.h>

#define VTS_NAME	"vts"

struct composite_sensor {
	const char			*name;
	int				vts_index;
	int				weight;
	enum qpnp_vadc_channels		channel;
	struct list_head		list;
};

typedef struct virtual_temp_sensor {
	const char			*name;
	int				index;
	struct device			*dev;
	struct thermal_zone_device	*tz_vts;
	struct qpnp_vadc_chip		*vadc_dev;
	struct composite_sensor		*sensors;
	u32				scaling_factor;
	int				constant;
} VTS;

LIST_HEAD(composite_sensors_head);
LIST_HEAD(value_sensors_head);

static int vts_manual_set = 0;
module_param_named(
	debug_mask, vts_manual_set, int, S_IRUGO | S_IWUSR | S_IWGRP
);

int vts_register_value_sensor(struct value_sensor *vs) {
	int ret = 0;
	struct composite_sensor *sensor;
	struct value_sensor *vsp;
	if (!vs) {
		pr_err("vs is not valid\n");
		ret = -ENODEV;
		goto err;
	}
	list_for_each_entry(sensor, &composite_sensors_head, list) {
		if (!strncmp(sensor->name, vs->name, 200)) {
			pr_err("Fail to register. vts has same name of %s\n", vs->name);
			ret = -EEXIST;
			goto err;
		}
	}
	list_for_each_entry(vsp, &value_sensors_head, list) {
		if (!strncmp(vsp->name, vs->name, 200)) {
			pr_err("Fail to register. vts has same name of %s\n", vs->name);
			ret = -EEXIST;
			goto err;
		}
	}
	INIT_LIST_HEAD(&vs->list);
	list_add_tail(&vs->list, &value_sensors_head);
	pr_info("Register value sensor [%s] : weight = %d\n", vs->name, vs->weight);
err:
	return ret;
}
EXPORT_SYMBOL(vts_register_value_sensor);

void vts_unregister_value_sensor(struct value_sensor *vs) {
	/* Each dev should free value_sensor itself. VTS will not! */
	if (!vs)
		return;
	list_del(&vs->list);
}
EXPORT_SYMBOL(vts_unregister_value_sensor);

inline static int vts_get_value_sensor_temp(struct value_sensor *vs) {
	if (!vs)
		return 0;
	return vs->get_temp(vs->devdata);
}

static int vts_tz_get_temp(struct thermal_zone_device *thermal,
				unsigned long *temp)
{
	VTS *vts = thermal->devdata;
	struct composite_sensor *sensor;
	struct value_sensor *vs;
	long val = 0;

	if (!vts->vadc_dev)
		goto value_sensor;
	list_for_each_entry(sensor, &composite_sensors_head, list) {
		struct qpnp_vadc_result results;
		int ret;
		if (sensor->vts_index != vts->index)
			continue;
		ret = qpnp_vadc_read(vts->vadc_dev, sensor->channel, &results);
		if (ret) {
			pr_err("Fail to get adc(%d)\n", sensor->channel);
			return ret;
		}
		val += sensor->weight * results.physical;
	//	pr_err("%s : weight = %d, val = %ld\n", sensor->name, sensor->weight, val);
	}
value_sensor:
	list_for_each_entry(vs, &value_sensors_head, list) {
		if (vs->vts_index != vts->index)
			continue;
		val += vs->weight * vts_get_value_sensor_temp(vs);
	//	pr_err("%s : weight = %d, index = %d, val = %ld\n", vs->name, vs->weight, vs->vts_index, val);
	}
	val += vts->constant;
	val *= vts->scaling_factor;
	val /= 1000L;
	*temp = (unsigned long)val;

	if (vts_manual_set) {
		pr_err("vts force set to %d from %lu\n", vts_manual_set, *temp);
		*temp = (unsigned long)vts_manual_set;
	}

	return 0;
}

static struct thermal_zone_device_ops vts_thermal_zone_ops = {
	.get_temp = vts_tz_get_temp,
};

static int vts_probe(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *child;
	static int index = 0;
	VTS *vts = kmalloc(sizeof(VTS), GFP_KERNEL);
	int ret = 0;
	int count = 0;
	struct composite_sensor *_sensor, *temp;

	/* Alloc chipset data on memory */
	if (!vts) {
		pr_err("Fail to get *vts.\n");
		return -ENOMEM;
	}

	/* Get vts name from device tree.
	   Set lge, name property if you want to change the name */
	if (of_property_read_string(of_node, "lge,name", &vts->name))
		vts->name = "vts";
	vts->dev = &pdev->dev;
	vts->index = index;

	/* Get adc device */
	vts->vadc_dev = qpnp_get_vadc(vts->dev, vts->name);
	if (IS_ERR(vts->vadc_dev))
		pr_info("%s: All of adc sensors are disabled.\n", vts->name);

	/* Register thermal zone */
	vts->tz_vts = thermal_zone_device_register(
				vts->name, 0, 0, vts,
				&vts_thermal_zone_ops, NULL, 0, 0);
	if (IS_ERR(vts->tz_vts)) {
		ret = PTR_ERR(vts->tz_vts);
		if (ret != -EPROBE_DEFER)
			pr_err("%s: Fail to get thermal_zone_device.\n", vts->name);
		goto fail;
	}

	/* Get infos from device tree */
	if (of_property_read_u32(of_node, "lge,index", &vts->index))
		vts->index = index;

	if (of_property_read_u32(of_node, "lge,scaling-factor", &vts->scaling_factor))
		vts->scaling_factor = 1;

	if (of_property_read_u32(of_node, "lge,constant-milli", &vts->constant))
		vts->constant = 0;

	for_each_child_of_node(of_node, child) {
		struct composite_sensor *sensor = kmalloc(
				sizeof(struct composite_sensor), GFP_KERNEL);
		if (!sensor) {
			ret = PTR_ERR(sensor);
			pr_err("%s: Fail to malloc sensor.\n", vts->name);
			goto fail;
		}
		sensor->vts_index = vts->index;
		if (of_property_read_string(child, "label", &sensor->name)) {
			kfree(sensor);
			continue;
		}
		if (of_property_read_u32(child, "channel", &sensor->channel)) {
			kfree(sensor);
			continue;
		}
		if (of_property_read_u32(child, "weight-milli", &sensor->weight)) {
			kfree(sensor);
			continue;
		}
		if (of_property_read_bool(child,"weight-negative")) {
			sensor->weight *= -1;
		}

		pr_info("%s: %s is registered. chan=%d, weight=%d\n",
			vts->name, sensor->name, sensor->channel, sensor->weight);
		INIT_LIST_HEAD(&sensor->list);
		list_add_tail(&sensor->list, &composite_sensors_head);
		count++;
	}
	pr_info("%s: Add %d adc(s).\n", vts->name, count);

	platform_set_drvdata(pdev, vts);
	pr_info("%s: probe done.\n", vts->name);
	index++;
	return 0;
fail:
	pr_info("Fail to register vts\n");
	list_for_each_entry_safe(_sensor, temp, &composite_sensors_head, list) {
		if (_sensor->vts_index != vts->index)
			continue;
		list_del(&_sensor->list);
		kfree(_sensor);
	}
	vts->vadc_dev = NULL;
	thermal_zone_device_unregister(vts->tz_vts);
	platform_set_drvdata(pdev, NULL);
	kfree(vts);
	return ret;
}

static int vts_remove(struct platform_device *pdev)
{
	VTS *vts = platform_get_drvdata(pdev);
	struct composite_sensor *sensor, *temp;
	thermal_zone_device_unregister(vts->tz_vts);
	list_for_each_entry_safe(sensor, temp, &composite_sensors_head, list) {
		if (sensor->vts_index != vts->index)
			continue;
		list_del(&sensor->list);
		kfree(sensor);
	}
	platform_set_drvdata(pdev, NULL);
	kfree(vts);
	return 0;
}

static const struct of_device_id vts_match[] = {
	{ .compatible = "lge,vts", },
	{}
};

static struct platform_driver vts_driver = {
	.probe = vts_probe,
	.remove = vts_remove,
	.driver = {
		.name = "vts",
		.owner = THIS_MODULE,
		.of_match_table = vts_match,
	},
};

static int __init vts_init_driver(void)
{
	return platform_driver_register(&vts_driver);
}
late_initcall(vts_init_driver);

static void __exit vts_exit_driver(void)
{
	return platform_driver_unregister(&vts_driver);
}
module_exit(vts_exit_driver);

MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yongkwan Kim <yongk.kim@lge.com>");
