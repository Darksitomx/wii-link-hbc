#ifndef WIILINK_BSPATCH_H
#define WIILINK_BSPATCH_H

#include <stdint.h>

typedef void (*PatchProgress)(uint64_t done, uint64_t total, void *user);

int bspatch_file(const char *old_path, const char *patch_path, const char *new_path,
                 PatchProgress progress, void *user);
const char *bspatch_last_error(void);

#endif
