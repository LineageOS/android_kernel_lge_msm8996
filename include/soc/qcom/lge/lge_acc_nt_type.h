#ifndef __LGE_ACC_NT_TYPE_H
#define __LGE_ACC_NT_TYPE_H

typedef enum {
        NT_TYPE_ERROR_MIN,
        NT_TYPE_NA_VZW,
        NT_TYPE_KR_LGU,
        NT_TYPE_CM,
        NT_TYPE_EU_GLOBAL,
        NT_TYPE_NA_SPR,
        NT_TYPE_KR_SKT,
        NT_TYPE_HM,
        NT_TYPE_JP_KDDI,
        NT_TYPE_NA_ATT,
        NT_TYPE_KR_KT,
        NT_TYPE_EU_AUSTRALIA,
        NT_TYPE_NA_TMUS,
        NT_TYPE_RESERVED1,
        NT_TYPE_RESERVED2,
        NT_TYPE_ERROR_MAX,
        NT_TYPE_MAX,
        NT_TYPE_ERROR_UNKNOWN = NT_TYPE_MAX,
} nt_type_t;

extern nt_type_t get_acc_nt_type(void);

#endif
