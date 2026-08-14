/* File-streaming bspatch based on Colin Percival's BSDIFF40 format. */
#include "bspatch.h"
#include "debug.h"
#include "util.h"

#include <bzlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char g_error[192];

static int patch_fail(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message);
    ERROR("bspatch: %s", message);
    return -1;
}

const char *bspatch_last_error(void) { return g_error; }

static int64_t offtin(const uint8_t buf[8]) {
    int64_t value = buf[7] & 0x7F;
    for (int i = 6; i >= 0; --i) value = value * 256 + buf[i];
    return (buf[7] & 0x80) ? -value : value;
}

static int bz_read_exact(BZFILE *bz, int *bzerror, void *output, int wanted) {
    uint8_t *out = (uint8_t *)output;
    int done = 0;
    while (done < wanted) {
        int got = BZ2_bzRead(bzerror, bz, out + done, wanted - done);
        if (*bzerror != BZ_OK && *bzerror != BZ_STREAM_END) return -1;
        done += got;
        if (*bzerror == BZ_STREAM_END && done < wanted) return -1;
    }
    return 0;
}

int bspatch_file(const char *old_path, const char *patch_path, const char *new_path,
                 PatchProgress progress, void *user) {
    int rc = -1;
    FILE *old_file = NULL, *new_file = NULL;
    FILE *ctrl_file = NULL, *diff_file = NULL, *extra_file = NULL;
    BZFILE *ctrl_bz = NULL, *diff_bz = NULL, *extra_bz = NULL;
    int ctrl_error = BZ_OK, diff_error = BZ_OK, extra_error = BZ_OK;
    uint8_t header[32];
    char partial[520];
    snprintf(partial, sizeof(partial), "%s.part", new_path);
    unlink(partial);

    FILE *header_file = fopen(patch_path, "rb");
    if (!header_file) return patch_fail("No se pudo abrir el parche");
    if (fread(header, 1, sizeof(header), header_file) != sizeof(header)) {
        fclose(header_file); return patch_fail("Cabecera del parche truncada");
    }
    fclose(header_file);
    if (memcmp(header, "BSDIFF40", 8) != 0) return patch_fail("Formato de parche no reconocido");
    int64_t ctrl_len = offtin(header + 8);
    int64_t diff_len = offtin(header + 16);
    int64_t new_size = offtin(header + 24);
    uint64_t patch_size = file_size_path(patch_path);
    if (ctrl_len < 0 || diff_len < 0 || new_size < 0 ||
        32u + (uint64_t)ctrl_len + (uint64_t)diff_len > patch_size)
        return patch_fail("Tamanos BSDIFF no validos");

    int64_t old_size = (int64_t)file_size_path(old_path);
    old_file = fopen(old_path, "rb");
    new_file = fopen(partial, "wb");
    ctrl_file = fopen(patch_path, "rb");
    diff_file = fopen(patch_path, "rb");
    extra_file = fopen(patch_path, "rb");
    if (!old_file || !new_file || !ctrl_file || !diff_file || !extra_file) {
        patch_fail("No se pudieron abrir archivos temporales"); goto cleanup;
    }
    if (fseek(ctrl_file, 32, SEEK_SET) ||
        fseek(diff_file, (long)(32 + ctrl_len), SEEK_SET) ||
        fseek(extra_file, (long)(32 + ctrl_len + diff_len), SEEK_SET)) {
        patch_fail("No se pudo posicionar el parche"); goto cleanup;
    }
    ctrl_bz = BZ2_bzReadOpen(&ctrl_error, ctrl_file, 0, 0, NULL, 0);
    diff_bz = BZ2_bzReadOpen(&diff_error, diff_file, 0, 0, NULL, 0);
    extra_bz = BZ2_bzReadOpen(&extra_error, extra_file, 0, 0, NULL, 0);
    if (!ctrl_bz || !diff_bz || !extra_bz || ctrl_error != BZ_OK ||
        diff_error != BZ_OK || extra_error != BZ_OK) {
        patch_fail("No se pudieron abrir flujos bzip2"); goto cleanup;
    }

    int64_t old_pos = 0, new_pos = 0;
    uint8_t ctrl_raw[24], diff_buffer[8192], old_buffer[8192], extra_buffer[8192];
    if (progress) progress(0, (uint64_t)new_size, user);
    while (new_pos < new_size) {
        if (bz_read_exact(ctrl_bz, &ctrl_error, ctrl_raw, sizeof(ctrl_raw)) != 0) {
            patch_fail("Flujo de control BSDIFF truncado"); goto cleanup;
        }
        int64_t ctrl[3] = {offtin(ctrl_raw), offtin(ctrl_raw + 8), offtin(ctrl_raw + 16)};
        if (ctrl[0] < 0 || ctrl[1] < 0 || new_pos + ctrl[0] > new_size ||
            new_pos + ctrl[0] + ctrl[1] > new_size) {
            patch_fail("Datos de control BSDIFF no validos"); goto cleanup;
        }

        int64_t remaining = ctrl[0];
        while (remaining > 0) {
            int chunk = remaining > (int64_t)sizeof(diff_buffer) ? (int)sizeof(diff_buffer) : (int)remaining;
            if (bz_read_exact(diff_bz, &diff_error, diff_buffer, chunk) != 0) {
                patch_fail("Flujo diff BSDIFF truncado"); goto cleanup;
            }
            memset(old_buffer, 0, (size_t)chunk);
            int64_t valid_start = old_pos < 0 ? 0 : old_pos;
            int64_t valid_end = old_pos + chunk > old_size ? old_size : old_pos + chunk;
            if (valid_start < valid_end) {
                size_t destination_offset = (size_t)(valid_start - old_pos);
                size_t valid_length = (size_t)(valid_end - valid_start);
                if (fseek(old_file, (long)valid_start, SEEK_SET) != 0 ||
                    fread(old_buffer + destination_offset, 1, valid_length, old_file) != valid_length) {
                    patch_fail("No se pudo leer el contenido original"); goto cleanup;
                }
            }
            for (int i = 0; i < chunk; ++i) diff_buffer[i] = (uint8_t)(diff_buffer[i] + old_buffer[i]);
            if (fwrite(diff_buffer, 1, (size_t)chunk, new_file) != (size_t)chunk) {
                patch_fail("No se pudo escribir contenido parcheado"); goto cleanup;
            }
            old_pos += chunk; new_pos += chunk; remaining -= chunk;
            if (progress) progress((uint64_t)new_pos, (uint64_t)new_size, user);
        }

        remaining = ctrl[1];
        while (remaining > 0) {
            int chunk = remaining > (int64_t)sizeof(extra_buffer) ? (int)sizeof(extra_buffer) : (int)remaining;
            if (bz_read_exact(extra_bz, &extra_error, extra_buffer, chunk) != 0) {
                patch_fail("Flujo extra BSDIFF truncado"); goto cleanup;
            }
            if (fwrite(extra_buffer, 1, (size_t)chunk, new_file) != (size_t)chunk) {
                patch_fail("No se pudo escribir datos extra"); goto cleanup;
            }
            new_pos += chunk; remaining -= chunk;
            if (progress) progress((uint64_t)new_pos, (uint64_t)new_size, user);
        }
        old_pos += ctrl[2];
    }
    fflush(new_file);
    if ((int64_t)file_size_path(partial) != new_size) {
        patch_fail("El tamano parcheado no coincide"); goto cleanup;
    }
    rc = 0;

cleanup:
    if (ctrl_bz) BZ2_bzReadClose(&ctrl_error, ctrl_bz);
    if (diff_bz) BZ2_bzReadClose(&diff_error, diff_bz);
    if (extra_bz) BZ2_bzReadClose(&extra_error, extra_bz);
    if (old_file) fclose(old_file);
    if (new_file) fclose(new_file);
    if (ctrl_file) fclose(ctrl_file);
    if (diff_file) fclose(diff_file);
    if (extra_file) fclose(extra_file);
    if (rc == 0) {
        unlink(new_path);
        if (rename(partial, new_path) != 0) {
            unlink(partial); return patch_fail("No se pudo finalizar contenido parcheado");
        }
        INFO("Applied BSDIFF: %s -> %s", old_path, new_path);
    } else {
        unlink(partial);
    }
    return rc;
}
