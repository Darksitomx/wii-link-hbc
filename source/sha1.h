#ifndef WIILINK_SHA1_H
#define WIILINK_SHA1_H

#include <stddef.h>
#include <stdint.h>

#define SHA1_DIGEST_SIZE 20

typedef struct {
    uint32_t state[5];
    uint64_t length;
    uint8_t buffer[64];
    size_t buffer_used;
} Sha1Context;

void sha1_init(Sha1Context *ctx);
void sha1_update(Sha1Context *ctx, const void *data, size_t size);
void sha1_final(Sha1Context *ctx, uint8_t digest[SHA1_DIGEST_SIZE]);
void sha1_calculate(const void *data, size_t size, uint8_t digest[SHA1_DIGEST_SIZE]);
int sha1_file(const char *path, uint64_t limit, uint8_t digest[SHA1_DIGEST_SIZE]);

#endif
