#ifndef WIILINK_UI_H
#define WIILINK_UI_H

#include <stdint.h>

void ui_run(void);
void ui_progress(const char *status, uint64_t done, uint64_t total, void *user);
void ui_show_fatal(const char *title, const char *message);

#endif
