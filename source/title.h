#ifndef WIILINK_TITLE_H
#define WIILINK_TITLE_H

#include "config.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t cid;
    uint16_t index;
    uint16_t type;
    uint64_t size;
    uint8_t hash[20];
    char encrypted_path[320];
} TitleContent;

typedef struct {
    uint8_t tmd[WIILINK_MAX_TMD_SIZE];
    size_t tmd_size;
    uint8_t ticket[MAX_TICKET_SIZE];
    size_t ticket_size;
    uint8_t cert[4096];
    size_t cert_size;
    uint8_t title_key[16];
    TitleContent contents[MAX_CONTENTS];
    uint16_t content_count;
} WiiTitle;

int title_load_tmd(WiiTitle *title, const char *path);
int title_merge_custom_tmd(WiiTitle *title, const char *path);
int title_load_ticket(WiiTitle *title, const char *path);
int title_load_cert(WiiTitle *title, const char *path);
int title_decrypt_content(const WiiTitle *title, size_t position, const char *encrypted_path,
                          const char *decrypted_path);
int title_encrypt_content(const WiiTitle *title, size_t position, const char *decrypted_path,
                          const char *encrypted_path);
int title_verify_decrypted(const WiiTitle *title, size_t position, const char *decrypted_path);
int title_update_content(WiiTitle *title, size_t position, const char *decrypted_path,
                         const char *encrypted_path);
int title_fakesign_tmd(WiiTitle *title);
int title_build_wad(const WiiTitle *title, const char *output_path);
const char *title_last_error(void);

#endif
