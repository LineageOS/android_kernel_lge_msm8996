#ifndef __LGE_ACCESSARY_CONNECTER_H
#define __LGE_ACCESSARY_CINNECTER_H


#define GPIO_LOW 		0
#define GPIO_HIGH 		1

#define GPIO_IN 		0
#define GPIO_OUT 		1

#define GPIO_ST_2MA 	0
#define GPIO_ST_10MA 	4

#define START_BYTE 		0xF0
#define STATUS_BYTE 	0x88 		/* TEST Qury */

#ifndef BIT
#define BIT(x) 			(1 << (x))
#endif

enum friends_type_t {
	FRIENDS_TYPE_UNKNOWN,
	FRIENDS_TYPE_ECARD,
	FRIENDS_TYPE_POWER_CONSUMER,	/* UM */
	FRIENDS_TYPE_POWER_PROVIDER,	/* CM / BM */
	FRIENDS_TYPE_INVALID,
};

enum connect_friends_type {
	UNKNOWN_DEVICE 	= 0xFF,
	FRIENDS1_ID 		= 0x71,
	FRIENDS2_ID 		= 0x72,
	FRIENDS3_ID 		= 0x73,
	FRIENDS4_ID 		= 0x74,
	FRIENDS5_ID 		= 0x75,			/* E-card */
};

enum friends_command {
	start_into_charging 	= 0x80,		/* friends -> device charging */
	start_from_charging 	= 0x40,		/* Device -> friends charging */
	check_temp 				= 0x20,		/* friends temp is 4 steps */
	check_scc 				= 0x10,		/* friends soc is 4 steps */
};
enum friends_temp_status {				/* Temperature uint Celsius */
	LOW_TEMP = 0,						/* Under 0 */
	NORMAL_TEMP,						/* 0 ~ 45 */
	HIGH_TEMP,							/* 45 ~ 55 */
	WARN_TEMP,							/* Over 55 or under -10 */
};

enum firends_soc_status {				/* Soc uint % */
	SHUTDOWN = 0,						/* Under 0 */
	LOW_LEVEL,							/* 1 ~ 25 */
	NORMAL_LEVEL,						/* 26 ~ 75 */
	HIGH_LEVEL,							/* 76 ~ 100 */
};

typedef unsigned byte;

#endif
