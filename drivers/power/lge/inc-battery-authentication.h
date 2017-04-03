#ifndef _INC_BATTERY_AUTHENTICATION_H_
#define _INC_BATTERY_AUTHENTICATION_H_

enum battery_authentication {
	BATTERY_AUTHENTICATION_DS2704_N        = 17,
	BATTERY_AUTHENTICATION_DS2704_L        = 32,
	BATTERY_AUTHENTICATION_DS2704_C        = 48,
	BATTERY_AUTHENTICATION_ISL6296_N       = 73,
	BATTERY_AUTHENTICATION_ISL6296_L       = 94,
	BATTERY_AUTHENTICATION_ISL6296_C       = 105,
	BATTERY_AUTHENTICATION_RA4301_VC0      = 130,
	BATTERY_AUTHENTICATION_RA4301_VC1      = 147,
	BATTERY_AUTHENTICATION_RA4301_VC2      = 162,
	BATTERY_AUTHENTICATION_SW3800_VC0      = 187,
	BATTERY_AUTHENTICATION_SW3800_VC1      = 204,
	BATTERY_AUTHENTICATION_SW3800_VC2      = 219,

	BATTERY_AUTHENTICATION_FORCED          = 255,
	BATTERY_AUTHENTICATION_ABSENT          = 200,
	BATTERY_AUTHENTICATION_UNKNOWN         = 0,
};

enum battery_vendor {
	BATTERY_VENDOR_LGC,
	BATTERY_VENDOR_TOCAD,
	BATTERY_VENDOR_TECHNOHILL,
};

inline static enum battery_authentication type_from_name(
	char *batauth_name) {

	if (!strcmp(batauth_name, "DS2704_N"))
		return BATTERY_AUTHENTICATION_DS2704_N;
	if (!strcmp(batauth_name, "DS2704_L"))
		return BATTERY_AUTHENTICATION_DS2704_L;
	if (!strcmp(batauth_name, "DS2704_C"))
		return BATTERY_AUTHENTICATION_DS2704_C;
	if (!strcmp(batauth_name, "ISL6296_N"))
		return BATTERY_AUTHENTICATION_ISL6296_N;
	if (!strcmp(batauth_name, "ISL6296_L"))
		return BATTERY_AUTHENTICATION_ISL6296_L;
	if (!strcmp(batauth_name, "ISL6296_C"))
		return BATTERY_AUTHENTICATION_ISL6296_C;
	if (!strcmp(batauth_name, "RA4301_VC0"))
		return BATTERY_AUTHENTICATION_RA4301_VC0;
	if (!strcmp(batauth_name, "RA4301_VC1"))
		return BATTERY_AUTHENTICATION_RA4301_VC1;
	if (!strcmp(batauth_name, "RA4301_VC2"))
		return BATTERY_AUTHENTICATION_RA4301_VC2;
	if (!strcmp(batauth_name, "SW3800_VC0"))
		return BATTERY_AUTHENTICATION_SW3800_VC0;
	if (!strcmp(batauth_name, "SW3800_VC1"))
		return BATTERY_AUTHENTICATION_SW3800_VC1;
	if (!strcmp(batauth_name, "SW3800_VC2"))
		return BATTERY_AUTHENTICATION_SW3800_VC2;
	if (!strcmp(batauth_name, "FORCED_VALID"))
		return BATTERY_AUTHENTICATION_FORCED;
	if (!strcmp(batauth_name, "MISSED"))
		return BATTERY_AUTHENTICATION_ABSENT;

	return BATTERY_AUTHENTICATION_UNKNOWN;
}

inline static const char* name_from_type(
	enum battery_authentication batauth_type) {

	switch (batauth_type) {
		case BATTERY_AUTHENTICATION_DS2704_N :
			return "DS2704_N";
		case BATTERY_AUTHENTICATION_DS2704_L :
			return "DS2704_L";
		case BATTERY_AUTHENTICATION_DS2704_C :
			return "DS2704_C";
		case BATTERY_AUTHENTICATION_ISL6296_N :
			return "ISL6296_N";
		case BATTERY_AUTHENTICATION_ISL6296_L :
			return "ISL6296_L";
		case BATTERY_AUTHENTICATION_ISL6296_C :
			return "ISL6296_C";
		case BATTERY_AUTHENTICATION_RA4301_VC0 :
			return "RA4301_VC0";
		case BATTERY_AUTHENTICATION_RA4301_VC1 :
			return "RA4301_VC1";
		case BATTERY_AUTHENTICATION_RA4301_VC2 :
			return "RA4301_VC2";
		case BATTERY_AUTHENTICATION_SW3800_VC0 :
			return "SW3800_VC0";
		case BATTERY_AUTHENTICATION_SW3800_VC1 :
			return "SW3800_VC1";
		case BATTERY_AUTHENTICATION_SW3800_VC2 :
			return "SW3800_VC2";
		case BATTERY_AUTHENTICATION_FORCED :
			return "FORCED_TO_BE_VALID";
		case BATTERY_AUTHENTICATION_ABSENT :
			return "MISSED";

		case BATTERY_AUTHENTICATION_UNKNOWN :
		default :
			return "UNKNOWN";
	}
}

void battery_authentication_force(void);
bool battery_authentication_present(void);
bool battery_authentication_valid(void);
const char* battery_authentication_name(void);
enum battery_authentication battery_authentication_type(void);

#endif
