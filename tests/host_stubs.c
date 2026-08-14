#include "debug.h"
#include <stdarg.h>
#include <stdio.h>
static int failed;
void debug_install_crash_handler(void) {}
void debug_init(void) {}
void debug_close(void) {}
void debug_set_screen(_Bool e) {(void)e;}
_Bool debug_screen_enabled(void) {return 0;}
void debug_log(LogLevel level, const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); fputc('\n',stderr); va_end(ap);
    if(level==LOG_ERROR) failed=1;
}
void debug_set_step(const char *s) {(void)s;}
const char *debug_last_step(void) {return "";}
const char *debug_log_path(void) {return "";}
void debug_flush(void) {}
