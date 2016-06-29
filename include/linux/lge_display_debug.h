#ifndef _LGE_DISPLAY_DEBUG_
#define _LGE_DISPLAY_DEBUG_

#include <linux/kernel.h>
#include <linux/string.h>

enum {
	LEVEL_ERR		= 0,
	LEVEL_WARN,
	LEVEL_INFOR,
	LEVEL_DEBUG,
	LEVEL_MAX,
};

/* Tag string for each funtion */
#define AOD		"[Disp][AOD]"
#define BL		"[Disp][Backlight]"
#define HLB		"[Disp][HL_Backlight]"
#define HDMI	"[Disp][HDMI]"
#define NONE	"[Disp]"

#if defined(CONFIG_LGE_DISPLAY_DYNAMIC_LOG)
extern uint32_t display_debug_level;
#define DISP_ERR(tag, fmt, args...)			\
	do {							\
		if (display_debug_level >= LEVEL_ERR)	\
			pr_err(tag fmt, ##args);	\
	} while (0)

#define DISP_WARN(tag, fmt, args...)			\
	do {							\
		if (display_debug_level >= LEVEL_WARN) \
			pr_err(tag fmt, ##args);	\
	} while (0)

#define DISP_INFO(tag, fmt, args...)			\
	do {							\
		if (display_debug_level >= LEVEL_INFOR)	\
			pr_err(tag fmt, ##args);	\
	} while (0)

#define DISP_DEBUG(tag, fmt, args...)			\
	do {							\
		if (display_debug_level >= LEVEL_DEBUG) \
			pr_err(tag fmt, ##args);	\
	} while (0)
#else
#define DISP_ERR(tag, fmt, args...)			\
	do {							\
		if (false)	\
			pr_err(tag fmt, ##args);	\
	} while (0)

#define DISP_WARN(tag, fmt, args...)			\
	do {							\
		if (false) \
			pr_err(tag fmt, ##args);	\
	} while (0)

#define DISP_INFO(tag, fmt, args...)			\
	do {							\
		if (false)	\
			pr_err(tag fmt, ##args);	\
	} while (0)

#define DISP_DEBUG(tag, fmt, args...)			\
	do {							\
		if (false) \
			pr_err(tag fmt, ##args);	\
	} while (0)
#endif
#endif
