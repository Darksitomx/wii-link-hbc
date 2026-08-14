#ifndef WIILINK_PATCHER_H
#define WIILINK_PATCHER_H

#include "catalog.h"

#include <stddef.h>
#include <stdint.h>

typedef void (*PatcherProgress)(const char *status, uint64_t done, uint64_t total, void *user);

void patcher_set_progress_callback(PatcherProgress callback, void *user);
int patcher_prepare_support_apps(void);
int patcher_download_osc_app(const char *app_name);
int patcher_download_spd(void);
int patcher_patch_channel(const ChannelDef *channel, int include_dependencies);
int patcher_connection_test(void);
const char *patcher_last_error(void);
const char *patcher_last_output(void);

#endif
