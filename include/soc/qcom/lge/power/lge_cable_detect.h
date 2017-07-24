/*
 *  Copyright (C) 2014, Daeho Choi <daeho.choi@lge.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __LGE_CABLE_DETECT_H__
#define __LGE_CABLE_DETECT_H__

#include <linux/list.h>
typedef enum {
	CABLE_ADC_NO_INIT = 0,
	CABLE_ADC_MHL_1K,
	CABLE_ADC_U_28P7K,
	CABLE_ADC_28P7K,
	CABLE_ADC_56K,
	CABLE_ADC_100K,
	CABLE_ADC_130K,
	CABLE_ADC_180K,
	CABLE_ADC_200K,
	CABLE_ADC_220K,
	CABLE_ADC_270K,
	CABLE_ADC_330K,
	CABLE_ADC_620K,
	CABLE_ADC_910K,
	CABLE_ADC_NONE,
	CABLE_ADC_MAX,
} cable_adc_type;

typedef enum {
	LT_CABLE_56K = 6,
	LT_CABLE_130K,
	USB_CABLE_400MA,
	USB_CABLE_DTC_500MA,
	ABNORMAL_USB_CABLE_400MA,
	LT_CABLE_910K,
	NONE_INIT_CABLE
} cable_boot_type;

typedef enum {
	NORMAL_CABLE,
	FACTORY_CABLE,
	CABLE_TYPE_MAX,
} factory_cable_type;




#endif
