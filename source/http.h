#ifndef WIILINK_HTTP_H
#define WIILINK_HTTP_H

#include <stddef.h>
#include <stdint.h>

typedef void (*HttpProgress)(uint64_t done, uint64_t total, void *user);

int http_download(const char *url, const char *destination, HttpProgress progress, void *user);
int http_get_memory(const char *url, void *buffer, size_t capacity, size_t *out_size);
const char *http_last_error(void);

#endif
