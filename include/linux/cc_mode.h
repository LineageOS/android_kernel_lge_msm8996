#ifndef _CC_MODE_H
#define _CC_MODE_H

#define FLAG_CC_MODE 0x01
#define FLAG_CC_AUDIT_LOGGING 0x02
#define FLAG_FORCE_CERT_VALIDATION 0x04
#define FLAG_FORCE_LIMIT_TLS_CIPHER_SUITES 0x08
#define FLAG_FORCE_LIMIT_TLS_ECC_ALG 0x10
#define FLAG_FORCE_USE_RANDOM_DEV 0x20
#define FLAG_SDP_SERVICE 0x40   // SDP [lge-disa-certification@lge.com]

#ifdef CONFIG_CRYPTO_CCMODE
extern int cc_mode;
extern int cc_mode_flag;
extern int get_cc_mode_state(void);
#else
#define cc_mode 0
#define cc_mode_flag 0
static inline int get_cc_mode_state(void)
{
	return 0;
}
#endif
#endif
