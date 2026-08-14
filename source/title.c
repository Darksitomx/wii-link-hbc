#include "title.h"
#include "debug.h"
#include "i18n.h"
#include "sha1.h"
#include "util.h"

#include <gccore.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TMD_RECORDS_OFFSET 0x1E4
#define TMD_BODY_OFFSET 0x140
#define TICKET_SIZE_V0 0x2A4

static char g_error[192];
static const uint8_t common_keys[3][16] ATTRIBUTE_ALIGN(32) = {
    {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7},
    {0x63,0xb8,0x2b,0xb4,0xf4,0x61,0x4e,0x2e,0x13,0xf2,0xfe,0xfb,0xba,0x4c,0x9b,0x7e},
    {0x30,0xbf,0xc7,0x6e,0x7c,0x19,0xaf,0xbb,0x23,0x16,0x33,0x30,0xce,0xd7,0xc2,0x8d}
};

static int title_fail(const char *message) {
    message = TR(message);
    snprintf(g_error, sizeof(g_error), "%s", message);
    ERROR("Title: %s", message);
    return -1;
}

static int title_fail_code(const char *message, int code) {
    message = TR(message);
    snprintf(g_error, sizeof(g_error), "%s (%d)", message, code);
    ERROR("Title: %s", g_error);
    return -1;
}

const char *title_last_error(void) { return g_error; }

static int read_file_prefix(const char *path, uint8_t *buffer, size_t capacity, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return title_fail("No se pudo abrir un componente del titulo");
    size_t got = fread(buffer, 1, capacity, fp);
    if (ferror(fp)) { fclose(fp); return title_fail("Error leyendo componente del titulo"); }
    fclose(fp);
    *out_size = got;
    return 0;
}

static int parse_tmd(WiiTitle *title) {
    if (title->tmd_size < TMD_RECORDS_OFFSET) return title_fail("TMD demasiado pequeno");
    uint16_t count = read_be16(title->tmd + 0x1DE);
    size_t real_size = TMD_RECORDS_OFFSET + (size_t)count * 36;
    if (!count || count > MAX_CONTENTS || real_size > title->tmd_size || real_size > WIILINK_MAX_TMD_SIZE)
        return title_fail("Registros de contenido TMD no validos");
    title->tmd_size = real_size;
    title->content_count = count;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *record = title->tmd + TMD_RECORDS_OFFSET + i * 36;
        TitleContent *content = &title->contents[i];
        content->cid = read_be32(record);
        content->index = read_be16(record + 4);
        content->type = read_be16(record + 6);
        content->size = read_be64(record + 8);
        memcpy(content->hash, record + 16, 20);
        content->encrypted_path[0] = '\0';
        if (content->size > 0x7FFFFFFFu) return title_fail("Contenido demasiado grande para FAT/libogc");
    }
    return 0;
}

int title_load_tmd(WiiTitle *title, const char *path) {
    if (read_file_prefix(path, title->tmd, sizeof(title->tmd), &title->tmd_size) != 0) return -1;
    int rc = parse_tmd(title);
    if (!rc) INFO("TMD loaded: %u contents, %u bytes", title->content_count, (unsigned)title->tmd_size);
    return rc;
}

int title_merge_custom_tmd(WiiTitle *title, const char *path) {
    TitleContent records[MAX_CONTENTS];
    uint16_t old_count = title->content_count;
    memcpy(records, title->contents, sizeof(records));

    uint8_t custom[WIILINK_MAX_TMD_SIZE];
    size_t custom_size;
    if (read_file_prefix(path, custom, sizeof(custom), &custom_size) != 0) return -1;
    if (custom_size < TMD_RECORDS_OFFSET) return title_fail("TMD personalizado truncado");
    uint16_t custom_count = read_be16(custom + 0x1DE);
    size_t real_size = TMD_RECORDS_OFFSET + (size_t)custom_count * 36;
    if (custom_count != old_count || real_size > custom_size || real_size > sizeof(title->tmd))
        return title_fail("TMD personalizado incompatible");

    memcpy(title->tmd, custom, real_size);
    title->tmd_size = real_size;
    title->content_count = old_count;
    memcpy(title->contents, records, sizeof(records));
    for (size_t i = 0; i < old_count; ++i) {
        uint8_t *record = title->tmd + TMD_RECORDS_OFFSET + i * 36;
        write_be32(record, records[i].cid);
        write_be16(record + 4, records[i].index);
        write_be16(record + 6, records[i].type);
        write_be64(record + 8, records[i].size);
        memcpy(record + 16, records[i].hash, 20);
    }
    INFO("Merged WiiLink custom TMD");
    return 0;
}

int title_load_ticket(WiiTitle *title, const char *path) {
    if (read_file_prefix(path, title->ticket, sizeof(title->ticket), &title->ticket_size) != 0) return -1;
    if (title->ticket_size < TICKET_SIZE_V0) return title_fail("Ticket truncado");
    title->ticket_size = TICKET_SIZE_V0;
    if (title->ticket[0x1BC] != 0) return title_fail("Solo se admiten tickets v0");
    uint8_t key_index = title->ticket[0x1F1];
    if (key_index > 2) return title_fail("Indice de common key desconocido");

    uint8_t iv[32] ATTRIBUTE_ALIGN(32) = {0};
    uint8_t input[32] ATTRIBUTE_ALIGN(32) = {0};
    uint8_t output[32] ATTRIBUTE_ALIGN(32) = {0};
    memcpy(input, title->ticket + 0x1BF, 16);
    memcpy(iv, title->ticket + 0x1DC, 8);
    int rc = AES_Decrypt(common_keys[key_index], 16, iv, 16, input, output, 16);
    if (rc < 0) return title_fail_code("No se pudo descifrar title key", rc);
    memcpy(title->title_key, output, 16);
    INFO("Ticket loaded (common key %u)", key_index);
    return 0;
}

int title_load_cert(WiiTitle *title, const char *path) {
    if (read_file_prefix(path, title->cert, sizeof(title->cert), &title->cert_size) != 0) return -1;
    if (title->cert_size < 2000) return title_fail("Cadena de certificados truncada");
    INFO("Certificate chain loaded: %u bytes", (unsigned)title->cert_size);
    return 0;
}

static int crypt_content_file(const WiiTitle *title, size_t position, const char *input_path,
                              const char *output_path, bool encrypt) {
    if (position >= title->content_count) return title_fail("Posicion de contenido no valida");
    const TitleContent *content = &title->contents[position];
    FILE *input = fopen(input_path, "rb");
    if (!input) return title_fail("No se pudo abrir contenido para criptografia");
    char partial[520];
    snprintf(partial, sizeof(partial), "%s.part", output_path);
    unlink(partial);
    FILE *output = fopen(partial, "wb");
    if (!output) { fclose(input); return title_fail("No se pudo crear contenido cifrado/descifrado"); }

    uint8_t key[32] ATTRIBUTE_ALIGN(32) = {0};
    memcpy(key, title->title_key, 16);
    uint8_t *in_buffer = memalign(32, IO_BUFFER_SIZE);
    uint8_t *out_buffer = memalign(32, IO_BUFFER_SIZE);
    uint8_t *iv = memalign(32, 32);
    if (!in_buffer || !out_buffer || !iv) {
        free(in_buffer); free(out_buffer); free(iv); fclose(input); fclose(output);
        unlink(partial); return title_fail("Memoria insuficiente para AES");
    }
    memset(iv, 0, 32);
    write_be16(iv, content->index);

    uint64_t input_remaining = encrypt ? content->size : align_up_u64(content->size, 16);
    uint64_t output_remaining = encrypt ? align_up_u64(content->size, 16) : content->size;
    int result = 0;
    while (input_remaining) {
        size_t chunk = input_remaining > IO_BUFFER_SIZE ? IO_BUFFER_SIZE : (size_t)input_remaining;
        size_t crypt_chunk = (size_t)align_up_u64(chunk, 16);
        memset(in_buffer, 0, crypt_chunk);
        size_t got = fread(in_buffer, 1, chunk, input);
        if (got != chunk) {
            result = title_fail(encrypt ? "Contenido descifrado truncado" : "Contenido NUS cifrado truncado");
            break;
        }
        int rc = encrypt
            ? AES_Encrypt(key, 16, iv, 16, in_buffer, out_buffer, crypt_chunk)
            : AES_Decrypt(key, 16, iv, 16, in_buffer, out_buffer, crypt_chunk);
        if (rc < 0) { result = title_fail_code("Fallo del motor AES", rc); break; }
        size_t write_size = output_remaining > crypt_chunk ? crypt_chunk : (size_t)output_remaining;
        if (fwrite(out_buffer, 1, write_size, output) != write_size) {
            result = title_fail("Error escribiendo resultado AES"); break;
        }
        input_remaining -= chunk;
        output_remaining -= write_size;
    }
    fflush(output);
    free(in_buffer); free(out_buffer); free(iv);
    fclose(input); fclose(output);
    if (result == 0) {
        unlink(output_path);
        if (rename(partial, output_path) != 0) result = title_fail("No se pudo finalizar resultado AES");
    } else unlink(partial);
    return result;
}

int title_decrypt_content(const WiiTitle *title, size_t position, const char *encrypted_path,
                          const char *decrypted_path) {
    debug_set_step(TR("Descifrando contenido NUS"));
    return crypt_content_file(title, position, encrypted_path, decrypted_path, false);
}

int title_encrypt_content(const WiiTitle *title, size_t position, const char *decrypted_path,
                          const char *encrypted_path) {
    debug_set_step(TR("Cifrando contenido parcheado"));
    return crypt_content_file(title, position, decrypted_path, encrypted_path, true);
}

int title_verify_decrypted(const WiiTitle *title, size_t position, const char *decrypted_path) {
    if (position >= title->content_count) return title_fail("Posicion de contenido no valida");
    uint8_t digest[20];
    if (sha1_file(decrypted_path, title->contents[position].size, digest) != 0)
        return title_fail("No se pudo calcular SHA-1 original");
    if (memcmp(digest, title->contents[position].hash, 20) != 0)
        return title_fail("SHA-1 original no coincide; descarga o ticket incorrecto");
    TRACE("Original SHA-1 verified for content %u", (unsigned)position);
    return 0;
}

int title_update_content(WiiTitle *title, size_t position, const char *decrypted_path,
                         const char *encrypted_path) {
    if (position >= title->content_count) return title_fail("Posicion de contenido no valida");
    uint64_t size = file_size_path(decrypted_path);
    if (!size) return title_fail("Contenido parcheado vacio");
    uint8_t digest[20];
    if (sha1_file(decrypted_path, size, digest) != 0) return title_fail("No se pudo calcular SHA-1 parcheado");
    TitleContent *content = &title->contents[position];
    content->size = size;
    memcpy(content->hash, digest, 20);
    snprintf(content->encrypted_path, sizeof(content->encrypted_path), "%s", encrypted_path);
    uint8_t *record = title->tmd + TMD_RECORDS_OFFSET + position * 36;
    write_be64(record + 8, size);
    memcpy(record + 16, digest, 20);
    INFO("Updated content %u: %llu bytes", (unsigned)position, (unsigned long long)size);
    return 0;
}

int title_fakesign_tmd(WiiTitle *title) {
    if (title->tmd_size < TMD_RECORDS_OFFSET) return title_fail("No hay TMD para fakesign");
    memset(title->tmd + 4, 0, 256);
    uint8_t digest[20];
    for (uint32_t value = 0; value <= 0xFFFFu; ++value) {
        write_be16(title->tmd + 0x1E2, (uint16_t)value);
        sha1_calculate(title->tmd + TMD_BODY_OFFSET, title->tmd_size - TMD_BODY_OFFSET, digest);
        if (digest[0] == 0) {
            INFO("TMD fakesigned with minor version %lu", (unsigned long)value);
            return 0;
        }
    }
    return title_fail("No se pudo fakesign el TMD");
}

static int copy_file_to(FILE *out, const char *path, uint64_t length) {
    FILE *in = fopen(path, "rb");
    if (!in) return title_fail("No se pudo abrir contenido al crear WAD");
    int rc = file_copy_range(in, out, 0, length);
    fclose(in);
    if (rc != 0) return title_fail("No se pudo copiar contenido al WAD");
    return 0;
}

int title_build_wad(const WiiTitle *title, const char *output_path) {
    debug_set_step(TR("Empaquetando WAD"));
    if (!title->cert_size || !title->ticket_size || !title->tmd_size || !title->content_count)
        return title_fail("Faltan componentes para crear WAD");
    uint64_t content_region_size = 0;
    for (size_t i = 0; i < title->content_count; ++i) {
        if (!title->contents[i].encrypted_path[0]) return title_fail("Falta un contenido cifrado");
        uint64_t needed = align_up_u64(title->contents[i].size, 16);
        if (file_size_path(title->contents[i].encrypted_path) < needed)
            return title_fail("Archivo de contenido cifrado incompleto");
        content_region_size += (i + 1 == title->content_count)
            ? title->contents[i].size : align_up_u64(title->contents[i].size, 64);
    }
    if (content_region_size > 0xFFFFFFFFu) return title_fail("Region de contenido WAD demasiado grande");

    if (ensure_parent_dir(output_path) != 0) return title_fail("No se pudo crear directorio WAD");
    char partial[520];
    snprintf(partial, sizeof(partial), "%s.part", output_path);
    unlink(partial);
    FILE *out = fopen(partial, "wb");
    if (!out) return title_fail("No se pudo crear WAD");

    uint8_t header[64] = {0};
    write_be32(header, 0x20);
    header[4] = 'I'; header[5] = 's';
    write_be32(header + 8, (uint32_t)title->cert_size);
    write_be32(header + 12, 0);
    write_be32(header + 16, (uint32_t)title->ticket_size);
    write_be32(header + 20, (uint32_t)title->tmd_size);
    write_be32(header + 24, (uint32_t)content_region_size);
    write_be32(header + 28, 0);

    int rc = 0;
    if (fwrite(header, 1, sizeof(header), out) != sizeof(header) ||
        fwrite(title->cert, 1, title->cert_size, out) != title->cert_size || pad_file(out, 64) ||
        fwrite(title->ticket, 1, title->ticket_size, out) != title->ticket_size || pad_file(out, 64) ||
        fwrite(title->tmd, 1, title->tmd_size, out) != title->tmd_size || pad_file(out, 64)) {
        rc = title_fail("No se pudieron escribir metadatos WAD");
    }
    for (size_t i = 0; rc == 0 && i < title->content_count; ++i) {
        uint64_t encrypted_size = align_up_u64(title->contents[i].size, 16);
        if (copy_file_to(out, title->contents[i].encrypted_path, encrypted_size) != 0) rc = -1;
        if (rc == 0 && i + 1 < title->content_count && pad_file(out, 64) != 0)
            rc = title_fail("No se pudo alinear contenido WAD");
    }
    if (rc == 0 && pad_file(out, 64) != 0) rc = title_fail("No se pudo finalizar WAD");
    fflush(out);
    fclose(out);

    if (rc == 0) {
        unlink(output_path);
        if (rename(partial, output_path) != 0) rc = title_fail("No se pudo renombrar WAD final");
        else INFO("WAD built: %s (%llu bytes)", output_path,
                  (unsigned long long)file_size_path(output_path));
    }
    if (rc != 0) unlink(partial);
    return rc;
}
