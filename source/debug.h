#ifndef WIILINK_DEBUG_H
#define WIILINK_DEBUG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LOG_TRACE,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void debug_install_crash_handler(void);
void debug_init(void);
void debug_close(void);
void debug_set_screen(bool enabled);
bool debug_screen_enabled(void);
void debug_log(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void debug_set_step(const char *step);
const char *debug_last_step(void);
const char *debug_log_path(void);
void debug_flush(void);

#define TRACE(...) debug_log(LOG_TRACE, __VA_ARGS__)
#define INFO(...)  debug_log(LOG_INFO, __VA_ARGS__)
#define WARN(...)  debug_log(LOG_WARN, __VA_ARGS__)
#define ERROR(...) debug_log(LOG_ERROR, __VA_ARGS__)

#endif
