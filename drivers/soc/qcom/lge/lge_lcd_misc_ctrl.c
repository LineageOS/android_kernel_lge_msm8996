/* arch/arm/mach-msm/lge/lge_kcal_ctrl.c
 *
 * Interface to calibrate display color temperature.
 *
 * Copyright (C) 2012 LGE
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/lge/board_lge.h>

#ifdef CONFIG_LGE_LCD_TUNING
static struct lcd_platform_data *lcd_pdata;
extern int find_lcd_cmd(void);
extern void put_lcd_cmd(void);
//extern int put_lut_table(int num,uint32_t value);
//extern uint32_t igc_Table_LUT[256];
extern int cmd_num;
char read_cmd[128];
int reg_num;
uint32_t read_array[256];
extern int get_backlight_map(int *bl_map);
extern int get_backlight_map_size(int *bl_size);
extern int set_backlight_map(int bl_size, int *bl_map);

static ssize_t lcd_backlight_ctrl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, bl_size, ret;
	int *bl_map;

	ret = get_backlight_map_size(&bl_size);
	if (ret) {
		pr_err("lcd_backlight_ctrl_show not supported\n");
		return sprintf(buf, "Not support\n");
	}

	bl_map = kzalloc(sizeof(int) * bl_size, GFP_KERNEL);
	if (!bl_map) {
		pr_err("Error to get bl_map memory\n");
		return sprintf(buf, "Error to get bl_map\n");
	}

	ret = get_backlight_map(bl_map);
	if (ret) {
		pr_err("get_backlight_map fail\n");
		kfree(bl_map);
		return sprintf(buf, "Error to get bl_map\n");
	}

	/* bl size have to print in out string */
	sprintf(buf, "bl size : %d\n", bl_size);
	for(i=0;i<bl_size;i++) {
		if (i%10 == 0)
			sprintf(buf, "%s%d\n", buf, bl_map[i]);
		else
			sprintf(buf, "%s%d ", buf, bl_map[i]);
	}
	kfree(bl_map);
	return sprintf(buf, "%s\n", buf);
}

static ssize_t lcd_backlight_ctrl_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, i, bl_size, value, index = 0;
	int *bl_map;

	pr_err("buf : %s\n", buf);
	sscanf(buf, "%d", &bl_size);
	if (bl_size <= 0) {
		pr_err("bl_size error\n");
		return count;
	}

	bl_map = kzalloc(sizeof(int) * bl_size, GFP_KERNEL);
	if (!bl_map) {
		pr_err("Error to get bl_map memory\n");
		return count;
	}
	pr_info("bl_size : %d\n", bl_size);

	for(index=0,i=0;index<count;index++) {
		if (buf[index]==' ') {
			sscanf(&buf[index+1], "%d", &value);
			bl_map[i++]=value;
		}
	}

	if (i!=bl_size) {
		pr_err("bl map parsing error\n");
		kfree(bl_map);
		return count;
	}

	for(i=0;i<bl_size;i++) {
		pr_err("%3d ", (int)bl_map[i]);
	}

	ret = set_backlight_map(bl_size, bl_map);
	if (ret) {
		pr_err("set_backlight_map error\n");
		kfree(bl_map);
		return count;
	}

	kfree(bl_map);

	return count;
}

static ssize_t lcd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int tun_lcd_t[128];
	if (!count)
		return -EINVAL;
	memset(tun_lcd_t,0,128*sizeof(int));
	sscanf(buf, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
	 &tun_lcd_t[1], &tun_lcd_t[2], &tun_lcd_t[3], &tun_lcd_t[4], &tun_lcd_t[5], &tun_lcd_t[6], &tun_lcd_t[7], &tun_lcd_t[8],
	 &tun_lcd_t[9], &tun_lcd_t[10], &tun_lcd_t[11], &tun_lcd_t[12], &tun_lcd_t[13], &tun_lcd_t[14], &tun_lcd_t[15], &tun_lcd_t[16],
	 &tun_lcd_t[17], &tun_lcd_t[18], &tun_lcd_t[19], &tun_lcd_t[20], &tun_lcd_t[21], &tun_lcd_t[22], &tun_lcd_t[23], &tun_lcd_t[24],
	 &tun_lcd_t[25], &tun_lcd_t[26], &tun_lcd_t[27], &tun_lcd_t[28], &tun_lcd_t[29], &tun_lcd_t[30], &tun_lcd_t[31], &tun_lcd_t[32],
	 &tun_lcd_t[33], &tun_lcd_t[34], &tun_lcd_t[35], &tun_lcd_t[36], &tun_lcd_t[37], &tun_lcd_t[38], &tun_lcd_t[39], &tun_lcd_t[40],
	 &tun_lcd_t[41], &tun_lcd_t[42], &tun_lcd_t[43], &tun_lcd_t[44], &tun_lcd_t[45], &tun_lcd_t[46], &tun_lcd_t[47], &tun_lcd_t[48],
	 &tun_lcd_t[49], &tun_lcd_t[50], &tun_lcd_t[51], &tun_lcd_t[52], &tun_lcd_t[53], &tun_lcd_t[54], &tun_lcd_t[55], &tun_lcd_t[56],
	 &tun_lcd_t[57], &tun_lcd_t[58], &tun_lcd_t[59], &tun_lcd_t[60], &tun_lcd_t[61], &tun_lcd_t[62], &tun_lcd_t[63], &tun_lcd_t[64],
	 &tun_lcd_t[65], &tun_lcd_t[66], &tun_lcd_t[67], &tun_lcd_t[68], &tun_lcd_t[69], &tun_lcd_t[70], &tun_lcd_t[71], &tun_lcd_t[72],
	 &tun_lcd_t[73], &tun_lcd_t[74], &tun_lcd_t[75], &tun_lcd_t[76], &tun_lcd_t[77], &tun_lcd_t[78], &tun_lcd_t[79], &tun_lcd_t[80],
	 &tun_lcd_t[81], &tun_lcd_t[82], &tun_lcd_t[83], &tun_lcd_t[84], &tun_lcd_t[85], &tun_lcd_t[86], &tun_lcd_t[87], &tun_lcd_t[88],
	 &tun_lcd_t[89], &tun_lcd_t[90], &tun_lcd_t[91], &tun_lcd_t[92], &tun_lcd_t[93], &tun_lcd_t[94], &tun_lcd_t[95], &tun_lcd_t[96],
	 &tun_lcd_t[97], &tun_lcd_t[98], &tun_lcd_t[99], &tun_lcd_t[100], &tun_lcd_t[101], &tun_lcd_t[102], &tun_lcd_t[103],
	 &tun_lcd_t[104], &tun_lcd_t[105], &tun_lcd_t[106], &tun_lcd_t[107], &tun_lcd_t[108], &tun_lcd_t[109], &tun_lcd_t[110],
	 &tun_lcd_t[111], &tun_lcd_t[112], &tun_lcd_t[113], &tun_lcd_t[114], &tun_lcd_t[115], &tun_lcd_t[116], &tun_lcd_t[117],
	 &tun_lcd_t[118], &tun_lcd_t[119], &tun_lcd_t[120], &tun_lcd_t[121], &tun_lcd_t[122], &tun_lcd_t[123], &tun_lcd_t[124],
	 &tun_lcd_t[125], &tun_lcd_t[126], &tun_lcd_t[127]
   );

	lcd_pdata->set_values(tun_lcd_t);
	put_lcd_cmd();
	return count;
}

static ssize_t lcd_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	unsigned int tun_lcd_t[128];
	int i;
	pr_info("%s:cmd_num=%d\n",__func__,cmd_num);
	lcd_pdata->get_values(tun_lcd_t);
	if(cmd_num)
	{
		sprintf(buf, "%x", reg_num);
		for(i=1;i<cmd_num;i++)
		{
			sprintf(buf, "%s %x",buf,tun_lcd_t[i]);
			pr_info("%s: tun_lcd_t[%d]=%x",__func__,i,tun_lcd_t[i]);
		}
	}
	return sprintf(buf,"%s \n",buf);
}
static ssize_t lcd_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if(!count)
		return reg_num= 0;

	sscanf(buf, "%d", &reg_num);
	pr_info("reg_num=%x\n",reg_num);
   return count;
}

static ssize_t lcd_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i,cmd_num;
	memset(read_cmd,0,128*sizeof(char));
	cmd_num=find_lcd_cmd();
	pr_info("%s:cmd_num=%d",__func__,cmd_num);
	for(i=0;i<cmd_num;i++)
	{
		sprintf(buf, "%s %x",buf,read_cmd[i]);
		pr_info("%s: read_cmd[%d]=%x",__func__,i,read_cmd[i]);
	}
	return sprintf(buf,"%s \n",buf);
}
/*
static ssize_t lut_store(struct device *dev, struct device_attribute *attr,
		          const char *buf, size_t count)
{
	int array_num, ret;
   uint32_t change_value;
	if(!count)
		return array_num= 0;

	sscanf(buf, "%d %x", &array_num,&change_value);
	pr_info("array_num=%d change_num=%x\n",array_num,change_value);
   ret = put_lut_table(array_num,change_value);
	return count;
}

static ssize_t lut_show(struct device *dev,
		            struct device_attribute *attr, char *buf)
{
	int i;
	memcpy(read_array,igc_Table_LUT,256*sizeof(uint32_t));
	for(i=0;i<256;i++)
	{
		sprintf(buf, "%s %x",buf,read_array[i]);
		if(i != 0 && i%10==0)
			sprintf(buf, "%s \n",buf);
	}
	return sprintf(buf,"%s \n",buf);
}
*/
static DEVICE_ATTR(lcd, 0644, lcd_show, lcd_store);
static DEVICE_ATTR(lcd_ctrl, 0644, lcd_ctrl_show, lcd_ctrl_store);
static DEVICE_ATTR(lcd_backlight_ctrl, 0644, lcd_backlight_ctrl_show, lcd_backlight_ctrl_store);

//static DEVICE_ATTR(lut, 0644, lut_show, lut_store);

static int lcd_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0;

	lcd_pdata = pdev->dev.platform_data;

	if(!lcd_pdata->set_values || !lcd_pdata->get_values){
		return -1;
	}

	rc = device_create_file(&pdev->dev, &dev_attr_lcd);
	if(rc !=0)
		return -1;
	rc = device_create_file(&pdev->dev, &dev_attr_lcd_ctrl);
	if(rc !=0)
		return -1;
	rc = device_create_file(&pdev->dev, &dev_attr_lcd_backlight_ctrl);
	if (rc !=0)
		return -1;

/*
	rc = device_create_file(&pdev->dev, &dev_attr_lut);
	if(rc !=0)
		return -1;
*/
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcd_ctrl_probe,
	.driver = {
		.name   = "lcd_ctrl",
	},
};

int __init lcd_ctrl_init(void)
{
	return platform_driver_register(&this_driver);
}

device_initcall(lcd_ctrl_init);
#endif

MODULE_DESCRIPTION("LGE MISC driver");
MODULE_LICENSE("GPL v2");

