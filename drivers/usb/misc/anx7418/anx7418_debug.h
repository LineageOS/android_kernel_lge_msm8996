#ifndef __ANX7418_DEBUG_H__
#define __ANX7418_DEBUG_H__

#ifdef CONFIG_DEBUG_FS
extern void anx_dbg_event(const char*, int);
extern void anx_dbg_print(const char*, int, const char*);
#else
static inline void anx_dbg_event(const char *name, int status)
{  }
static inline void anx_dbg_print(const char *name, int status, const char *extra)
{  }
#endif
#endif /* __ANX7418_DEBUG_H__ */
