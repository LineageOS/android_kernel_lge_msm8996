#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

/* Customized Locale table */
//CONFIG_BCM4359
struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"CK", "XZ", 11},	/* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XZ", 11},	/* Universal if Country code is Cuba (13.4.27)*/
	{"FO", "XZ", 11},	/* Universal if Country code is Faroe Island (13.4.27)*/
	{"IM", "XZ", 11},	/* Universal if Country code is Isle of Man (13.4.27)*/
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"JE", "XZ", 11},	/* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XZ", 11},	/* Universal if Country code is North Korea (13.4.27)*/
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"NF", "XZ", 11},	/* Universal if Country code is Norfolk Island (13.4.27)*/
	{"NU", "XZ", 11},	/* Universal if Country code is Niue (13.4.27)*/
	{"PM", "XZ", 11},	/* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"PN", "XZ", 11},	/* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SS", "XZ", 11},	/* Universal if Country code is South_Sudan (13.4.27)*/
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"AD", "AD", 0},
	{"AE", "AE", 6},
	{"AF", "AF", 0},
	{"AG", "AG", 2},
	{"AI", "AW", 2}, /*updated 2015.04.01*/
	{"AL", "AL", 2},
	{"AM", "AM", 1}, /*change 0 -> 1*/
	{"AN", "GD", 2}, /*updated 2015.04.01*/
	{"AO", "AO", 0},
	{"AR", "AU", 6},
	{"AS", "AU", 6},
	{"AT", "AT", 4},
	{"AU", "AU", 6},
	{"AW", "AW", 2},
	{"AZ", "AZ", 2},
	{"BA", "BA", 2},
	{"BB", "BB", 0},
	{"BD", "AU", 6},
	{"BE", "BE", 4},
	{"BF", "BF", 0},
	{"BG", "BG", 4},
	{"BH", "BH", 4},
	{"BI", "BI", 0},
	{"BJ", "BJ", 0},
	{"BM", "AU", 6},
	{"BN", "BN", 4},
	{"BO", "NG", 0}, /*updated 2015.04.01*/
	{"BR", "BR", 15},
	{"BS", "BS", 2},
	{"BT", "BJ", 0}, /*updated 2015.04.01*/
	{"BW", "BJ", 0}, /*updated 2015.04.01*/
	{"BY", "BY", 3},
	{"BZ", "BZ", 0},
	{"CA", "US", 988},
	{"CD", "CD", 0},
	{"CF", "CF", 0},
	{"CG", "CG", 0},
	{"CH", "CH", 4},
	{"CI", "CI", 0},
	{"CL", "CL", 0},
	{"CM", "CM", 0},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"CV", "CV", 0},
	{"CX", "CX", 0},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DJ", "DJ", 0},
	{"DK", "DK", 4},
	{"DM", "DM", 0},
	{"DO", "DO", 0},
	{"DZ", "DZ", 1},
	{"EC", "EC", 21},
	{"EE", "EE", 4},
	{"EG", "EG", 13}, /*updated 2015.04.01*/
	{"ER", "ER", 0},
	{"ES", "ES", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FJ", "FJ", 0},
	{"FK", "FK", 0},
	{"FM", "FM", 0},
	{"FR", "FR", 5},
	{"GA", "GA", 0},
	{"GB", "GB", 6},
	{"GD", "GD", 2},
	{"GE", "GE", 0},
	{"GF", "GF", 2},
	{"GH", "GH", 0},
	{"GI", "GI", 0}, /*updated 2015.04.01*/
	{"GL", "GR", 4}, /*updated 2015.04.01*/
	{"GM", "GM", 0},
	{"GN", "GN", 0},
	{"GP", "GP", 2},
	{"GQ", "GQ", 0},
	{"GR", "GR", 4},
	{"GT", "GT", 1},
	{"GU", "GU", 12},
	{"GW", "GW", 0},
	{"GY", "GY", 0},
	{"HK", "HK", 2},
	{"HN", "HN", 0},
	{"HR", "HR", 4},
	{"HT", "HT", 0},
	{"HU", "HU", 4},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 7},
	{"IN", "IN", 3},
	{"IQ", "IQ", 0},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"JM", "JM", 0},
	{"JO", "JO", 3},
	{"JP", "JP", 58},
	{"KE", "SA", 0},
	{"KG", "KG", 0},
	{"KH", "KH", 2},
	{"KI", "KI", 0},
	{"KM", "KM", 0},
	{"KN", "KN", 0},
	{"KR", "KR", 962}, /* updated 2016.11.22 */
	{"KW", "KW", 5},
	{"KY", "KY", 3},
	{"KZ", "KZ", 0},
	{"LA", "LA", 2},
	{"LB", "LB", 5},
	{"LC", "LC", 0},
	{"LI", "LI", 4},
	{"LK", "LK", 1},
	{"LR", "LR", 0},
	{"LS", "LS", 2},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"LY", "LI", 4},/*updated 2015.04.01*/
	{"MA", "MA", 2},
	{"MC", "MC", 1},
	{"MD", "MD", 2},
	{"ME", "ME", 2},
	{"MF", "MF", 0},
	{"MG", "MG", 0},
	{"MK", "MK", 2},
	{"ML", "ML", 0},
	{"MM", "MM", 0},
	{"MN", "MN", 1},
	{"MO", "SG", 0}, /*updated 2015.04.01*/
	{"MP", "MP", 0},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MS", "MS", 0},
	{"MT", "MT", 4},
	{"MU", "MU", 2},
	{"MV", "MV", 3},
	{"MW", "MW", 1},
	{"MX", "MX", 20},
	{"MY", "MY", 19}, /*change 3 -> 19*/
	{"MZ", "MZ", 0},
	{"NA", "NA", 0},
	{"NC", "NC", 0},
	{"NE", "NE", 0},
	{"NG", "NG", 0},
	{"NI", "NI", 2},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NP", "NP", 3}, /*updated 2015.04.01*/
	{"NR", "NR", 0},
	{"NZ", "NZ", 4}, /*updated 2015.04.01*/
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PE", "PE", 20},
	{"PF", "PF", 0},
	{"PG", "AU", 6},
	{"PH", "PH", 5},
	{"PK", "PK", 0},
	{"PL", "PL", 4},
	{"PR", "US", 988}, /*changed US/118 -> US/988*/
	{"PT", "PT", 4},
	{"PW", "PW", 0},
	{"PY", "PY", 2},
	{"QA", "QA", 0},
	{"RE", "RE", 2},
	{"RKS", "KG", 0},
	{"RO", "RO", 4},
	{"RS", "RS", 2},
	{"RU", "RU", 13},
	{"RW", "RW", 0},
	{"SA", "SA", 0},
	{"SB", "SB", 0},
	{"SC", "SC", 0},
	{"SE", "SE", 4},
	{"SG", "SG", 0},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"SL", "SL", 0},
	{"SM", "SM", 0},
	{"SN", "SN", 2},
	{"SO", "SO", 0},
	{"SR", "SR", 0},
	{"ST", "ST", 0},
	{"SV", "SV", 25},
	{"SZ", "SZ", 0},
	{"TC", "TC", 0},
	{"TD", "TD", 0},
	{"TF", "TF", 0},
	{"TG", "TG", 0},
	{"TH", "TH", 5},
	{"TJ", "TJ", 0},
	{"TM", "TM", 0},
	{"TN", "TN", 1}, /*updated 2015.04.01*/
	{"TO", "TO", 0},
	{"TR", "TR", 7},
	{"TT", "TT", 3},
	{"TV", "TV", 0},
	{"TW", "TW", 1},
	{"TZ", "TZ", 0},
	{"UA", "UA", 8},
	{"UG", "UG", 2},
	{"UM", "US", 988},
	{"US", "US", 988},
	{"UY", "UY", 1},
	{"UZ", "MA", 2},
	{"VA", "VA", 2},
	{"VC", "VC", 0},
	{"VE", "VE", 3},
	{"VG", "VG", 2},
	{"VI", "US", 988},
	{"VN", "VN", 4},
	{"VU", "VU", 0},
	{"WS", "SA", 0},/*updated 2015.04.01*/
	{"YE", "YE", 0},
	{"YT", "YT", 2},
	{"ZA", "ZA", 6},
	{"ZM", "LA", 2},
	{"ZW", "ZW", 0},
	{"DC", "XZ", 999},
};
EXPORT_SYMBOL(bcm_wifi_translate_custom_table);
