#pragma once

#define TP_DEBUG(fmt, ...) tp_debug_f(__FILE__,__LINE__,__func__, fmt, ## __VA_ARGS__)
#define TP_DEBUG_DUMP(p,s, fmt, ...) tp_debug_dump_f(__FILE__,__LINE__,__func__, p, s, fmt, ## __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

void tp_debug_f(const char *file, const int line, const char *func, const char *fmt,...);
void tp_debug_dump_f(const char *file, const int line, const char *func, const void *ptr, int size, const char *fmt,...);

#ifdef __cplusplus
}
#endif
