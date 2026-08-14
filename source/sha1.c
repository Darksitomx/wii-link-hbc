/* Small SHA-1 implementation for streaming title contents from SD. */
#include "sha1.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static uint32_t rol(uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32u - bits));
}

static void transform(Sha1Context *ctx, const uint8_t block[64]) {
    uint32_t w[80];
    for (unsigned i = 0; i < 16; ++i) w[i] = read_be32(block + i * 4);
    for (unsigned i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2];
    uint32_t d = ctx->state[3], e = ctx->state[4];
    for (unsigned i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
        uint32_t temp = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = temp;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c;
    ctx->state[3] += d; ctx->state[4] += e;
}

void sha1_init(Sha1Context *ctx) {
    ctx->state[0] = 0x67452301u; ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu; ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u; ctx->length = 0; ctx->buffer_used = 0;
}

void sha1_update(Sha1Context *ctx, const void *input, size_t size) {
    const uint8_t *data = (const uint8_t *)input;
    ctx->length += (uint64_t)size * 8u;
    while (size) {
        size_t room = 64 - ctx->buffer_used;
        size_t take = size < room ? size : room;
        memcpy(ctx->buffer + ctx->buffer_used, data, take);
        ctx->buffer_used += take;
        data += take;
        size -= take;
        if (ctx->buffer_used == 64) {
            transform(ctx, ctx->buffer);
            ctx->buffer_used = 0;
        }
    }
}

void sha1_final(Sha1Context *ctx, uint8_t digest[SHA1_DIGEST_SIZE]) {
    uint64_t bit_length = ctx->length;
    uint8_t pad[128] = {0x80};
    size_t pad_len = ctx->buffer_used < 56 ? 56 - ctx->buffer_used : 120 - ctx->buffer_used;
    sha1_update(ctx, pad, pad_len);
    uint8_t length_be[8];
    write_be64(length_be, bit_length);
    sha1_update(ctx, length_be, 8);
    for (unsigned i = 0; i < 5; ++i) write_be32(digest + i * 4, ctx->state[i]);
    memset(ctx, 0, sizeof(*ctx));
}

void sha1_calculate(const void *data, size_t size, uint8_t digest[SHA1_DIGEST_SIZE]) {
    Sha1Context ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, size);
    sha1_final(&ctx, digest);
}

int sha1_file(const char *path, uint64_t limit, uint8_t digest[SHA1_DIGEST_SIZE]) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    uint8_t buffer[8192];
    Sha1Context ctx;
    sha1_init(&ctx);
    while (limit) {
        size_t chunk = limit > sizeof(buffer) ? sizeof(buffer) : (size_t)limit;
        size_t got = fread(buffer, 1, chunk, fp);
        if (!got) { fclose(fp); return -1; }
        sha1_update(&ctx, buffer, got);
        limit -= got;
    }
    fclose(fp);
    sha1_final(&ctx, digest);
    return 0;
}
