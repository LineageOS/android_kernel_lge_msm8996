#define pr_fmt(fmt) "LGE_BATT_DETECT: %s: " fmt, __func__

#include <soc/qcom/lge/lge_batt_detection.h>

char *return_lge_battery_name(){
	char *lge_batt_profile_name = NULL;
	
#if defined(CONFIG_MACH_MSM8996_ELSA) || defined(CONFIG_MACH_MSM8996_ANNA) // V20, ANNA
#if defined(CONFIG_MACH_MSM8996_ELSA_DCM_JP) || defined(CONFIG_MACH_MSM8996_ELSA_KDDI_JP) // Jap V20
	lge_batt_profile_name = "LGE_BLT28_Tocad_3000mAh"; // The 3000mAh batt from Japanese V20
#else // If it isn't a japanese V20, get the standard 3200mAh battery profile
	lge_batt_profile_name = "LGE_BL44E1F_LGC_3200mAh"; // For V20 and whatever ANNA is.
#endif 
#elif defined(CONFIG_MACH_MSM8996_LUCYE) // G6
	lge_batt_profile_name = "LGE_BLT32_LGC_3300mAh"; // Standard G6 battery
#else // G5
	lge_batt_profile_name = "lge_bl42d1f_2800mah_averaged_masterslave_nov30th2015"; // Standard G5 Battery
#endif

	pr_debug("Battery name: %s", lge_batt_profile_name);
	return lge_batt_profile_name;
}