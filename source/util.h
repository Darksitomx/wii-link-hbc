#ifndef WIILINK_UTIL_H
#define WIILINK_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

uint16_t read_be16(const void *p);
uint32_t read_be32(const void *p);
uint64_t read_be64(const void *p);
void write_be16(void *p, uint16_t value);
void write_be32(void *p, uint32_t value);
void write_be64(void *p, uint64_t value);
uint64_t align_up_u64(uint64_t value, uint32_t alignment);
int mkdir_recursive(const char *path);
int ensure_parent_dir(const char *path);
int file_copy_range(FILE *src, FILE *dst, uint64_t offset, uint64_t length);
int write_zeroes(FILE *fp, uint64_t count);
int pad_file(FILE *fp, uint32_t alignment);
uint64_t file_size_path(const char *path);
bool file_exists(const char *path);
void safe_filename(char *dst, size_t dst_size, const char *src);
void remove_tree_files(const char *path);

#endif
