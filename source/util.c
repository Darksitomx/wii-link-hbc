#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

uint16_t read_be16(const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t read_be32(const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

uint64_t read_be64(const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;
    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
}

void write_be16(void *ptr, uint16_t value) {
    uint8_t *p = (uint8_t *)ptr;
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

void write_be32(void *ptr, uint32_t value) {
    uint8_t *p = (uint8_t *)ptr;
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

void write_be64(void *ptr, uint64_t value) {
    write_be32(ptr, (uint32_t)(value >> 32));
    write_be32((uint8_t *)ptr + 4, (uint32_t)value);
}

uint64_t align_up_u64(uint64_t value, uint32_t alignment) {
    return (value + alignment - 1u) & ~((uint64_t)alignment - 1u);
}

int mkdir_recursive(const char *path) {
    if (!path || !*path) return -1;
    char temp[512];
    if (strlen(path) >= sizeof(temp)) return -1;
    strcpy(temp, path);
    size_t len = strlen(temp);
    while (len > 1 && temp[len - 1] == '/') temp[--len] = '\0';

    for (char *p = temp + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(temp, 0777) != 0 && errno != EEXIST) return -1;
        *p = '/';
    }
    if (mkdir(temp, 0777) != 0 && errno != EEXIST) return -1;
    return 0;
}

int ensure_parent_dir(const char *path) {
    char temp[512];
    if (!path || strlen(path) >= sizeof(temp)) return -1;
    strcpy(temp, path);
    char *slash = strrchr(temp, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (!*temp) return 0;
    return mkdir_recursive(temp);
}

int write_zeroes(FILE *fp, uint64_t count) {
    static const uint8_t zeroes[512] = {0};
    while (count) {
        size_t chunk = count > sizeof(zeroes) ? sizeof(zeroes) : (size_t)count;
        if (fwrite(zeroes, 1, chunk, fp) != chunk) return -1;
        count -= chunk;
    }
    return 0;
}

int pad_file(FILE *fp, uint32_t alignment) {
    long pos = ftell(fp);
    if (pos < 0) return -1;
    uint64_t aligned = align_up_u64((uint64_t)pos, alignment);
    return write_zeroes(fp, aligned - (uint64_t)pos);
}

int file_copy_range(FILE *src, FILE *dst, uint64_t offset, uint64_t length) {
    uint8_t buffer[8192];
    if (fseek(src, (long)offset, SEEK_SET) != 0) return -1;
    while (length) {
        size_t chunk = length > sizeof(buffer) ? sizeof(buffer) : (size_t)length;
        size_t got = fread(buffer, 1, chunk, src);
        if (got != chunk || fwrite(buffer, 1, got, dst) != got) return -1;
        length -= got;
    }
    return 0;
}

uint64_t file_size_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_size;
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

void safe_filename(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    size_t out = 0;
    for (size_t i = 0; src && src[i] && out + 1 < dst_size; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            dst[out++] = '_';
        } else {
            dst[out++] = (char)c;
        }
    }
    while (out > 0 && (dst[out - 1] == ' ' || dst[out - 1] == '.')) --out;
    dst[out] = '\0';
}

void remove_tree_files(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    char child[512];
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            remove_tree_files(child);
            rmdir(child);
        } else {
            unlink(child);
        }
    }
    closedir(dir);
}
