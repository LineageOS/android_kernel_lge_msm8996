#ifndef __FPC_LOG_H
#define __FPC_LOG_H

#include <linux/kernel.h>

/*! ERROR LOG LEVEL */
#define LOG_LEVEL_E 3
/*! NOTICE LOG LEVEL */
#define LOG_LEVEL_N 5
/*! INFORMATION LOG LEVEL */
#define LOG_LEVEL_I 6
/*! DEBUG LOG LEVEL */
#define LOG_LEVEL_D 7

#ifndef LOG_LEVEL
/*! LOG LEVEL DEFINATION */
#define LOG_LEVEL LOG_LEVEL_D
#endif

#ifndef MODULE_TAG
/*! MODULE TAG DEFINATION */
#define MODULE_TAG "[FINGERPRINT] "
#endif

#if (LOG_LEVEL >= LOG_LEVEL_E)
/*! print error message */
#define PERR(fmt, args...) \
	pr_err(MODULE_TAG \
	"[%s] " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PERR(fmt, args...)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_N)
/*! print notice message */
#define PNOTICE(fmt, args...) \
	pr_notice(MODULE_TAG \
	"[%s] " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PNOTICE(fmt, args...)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_I)
/*! print information message */
#define PINFO(fmt, args...) pr_info(MODULE_TAG \
	"[%s] " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PINFO(fmt, args...)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_D)
#define PDEBUG(fmt, args...) pr_devel(MODULE_TAG \
	"[%s] " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PDEBUG(fmt, args...)
#endif

#endif/*__FPC_LOG_H*/
/*@}*/
