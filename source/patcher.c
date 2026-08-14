#include "patcher.h"
#include "bspatch.h"
#include "config.h"
#include "debug.h"
#include "http.h"
#include "i18n.h"
#include "title.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static PatcherProgress g_progress;
static void *g_progress_user;
static char g_error[256];
static char g_output[520];
static const char *g_status = "";

static int patcher_fail(const char *message) {
    message = TR(message);
    snprintf(g_error, sizeof(g_error), "%s", message);
    ERROR("Patcher: %s", message);
    return -1;
}

static int patcher_fail_detail(const char *message, const char *detail) {
    message = TR(message);
    detail = TR(detail ? detail : "error desconocido");
    snprintf(g_error, sizeof(g_error), "%s: %s", message, detail);
    ERROR("Patcher: %s", g_error);
    return -1;
}

const char *patcher_last_error(void) { return g_error; }
const char *patcher_last_output(void) { return g_output; }

void patcher_set_progress_callback(PatcherProgress callback, void *user) {
    g_progress = callback;
    g_progress_user = user;
}

static void emit(const char *status, uint64_t done, uint64_t total) {
    status = TR(status);
    g_status = status;
    if (g_progress) g_progress(status, done, total, g_progress_user);
}

static void transfer_progress(uint64_t done, uint64_t total, void *user) {
    (void)user;
    emit(g_status, done, total);
}

static void lowercase(char *destination, size_t size, const char *source) {
    size_t i = 0;
    for (; source && source[i] && i + 1 < size; ++i)
        destination[i] = (char)tolower((unsigned char)source[i]);
    destination[i] = '\0';
}

static int download_component(const char *url, const char *path, const char *status) {
    debug_set_step(TR(status));
    emit(status, 0, 0);
    if (http_download(url, path, transfer_progress, NULL) != 0)
        return patcher_fail_detail(status, http_last_error());
    return 0;
}

int patcher_download_osc_app(const char *app_name) {
    const char *remote_name = !strcmp(app_name, "agc") ? "AnyGlobe_Changer" : app_name;
    char app_path[384], url[1200], destination[520], status[160];
    snprintf(app_path, sizeof(app_path), STORAGE_ROOT "/apps/%s", remote_name);
    if (mkdir_recursive(app_path) != 0) return patcher_fail("No se pudo crear directorio de app OSC");

    static const char *const files[] = {"boot.dol", "meta.xml"};
    for (size_t i = 0; i < 2; ++i) {
        snprintf(url, sizeof(url), "%s/unzipped_apps/%s/apps/%s/%s",
                 OSC_BASE, remote_name, remote_name, files[i]);
        snprintf(destination, sizeof(destination), "%s/%s", app_path, files[i]);
        i18n_snprintf(status, sizeof(status), "Descargando %s: %s", remote_name, files[i]);
        if (download_component(url, destination, status) != 0) return -1;
    }
    snprintf(url, sizeof(url), "%s/api/v3/contents/%s/icon.png", OSC_BASE, remote_name);
    snprintf(destination, sizeof(destination), "%s/icon.png", app_path);
    i18n_snprintf(status, sizeof(status), "Descargando %s: icon.png", remote_name);
    if (download_component(url, destination, status) != 0) return -1;
    INFO("OSC app ready: %s", remote_name);
    return 0;
}

int patcher_prepare_support_apps(void) {
    debug_set_step(TR("Preparando aplicaciones auxiliares"));
    static const char *const apps[] = {"yawmME", "sntp", "Mail-Patcher"};
    for (size_t i = 0; i < sizeof(apps) / sizeof(apps[0]); ++i)
        if (patcher_download_osc_app(apps[i]) != 0) return -1;
    return 0;
}

int patcher_download_spd(void) {
    char destination[520];
    snprintf(destination, sizeof(destination), "%s/INSTALL ME - WiiLink Address Settings.wad", WAD_DIR);
    if (download_component(PATCHER_BASE "/spd/WiiLink_SPD.wad", destination,
                           "Descargando WiiLink Address Settings") != 0) return -1;
    snprintf(g_output, sizeof(g_output), "%s", destination);
    return 0;
}

static int prepare_certificate_chain(char *out_path, size_t out_size) {
    snprintf(out_path, out_size, "%s/cache/cert.bin", APP_DIR);
    if (file_size_path(out_path) == 2560) return 0;
    char tmd_path[420], ticket_path[420];
    snprintf(tmd_path, sizeof(tmd_path), "%s/system-menu-tmd.bin", WORK_DIR);
    snprintf(ticket_path, sizeof(ticket_path), "%s/system-menu-cetk.bin", WORK_DIR);
    if (download_component(NUS_BASE "/0000000100000002/tmd.513", tmd_path,
                           "Descargando certificado CP") != 0) return -1;
    if (download_component(NUS_BASE "/0000000100000002/cetk", ticket_path,
                           "Descargando certificados CA/XS") != 0) return -1;

    FILE *tmd = fopen(tmd_path, "rb");
    FILE *ticket = fopen(ticket_path, "rb");
    if (!tmd || !ticket || ensure_parent_dir(out_path) != 0) {
        if (tmd) fclose(tmd);
        if (ticket) fclose(ticket);
        return patcher_fail("No se pudieron preparar certificados");
    }
    FILE *cert = fopen(out_path, "wb");
    if (!cert) { fclose(tmd); fclose(ticket); return patcher_fail("No se pudo crear cert.bin"); }
    int rc = 0;
    /* WAD order used by libWiiPy: CA (1024), CP (768), XS (768). */
    if (file_copy_range(ticket, cert, 0x2A4 + 768, 1024) ||
        file_copy_range(tmd, cert, 0x328, 768) ||
        file_copy_range(ticket, cert, 0x2A4, 768)) rc = -1;
    fclose(tmd); fclose(ticket); fflush(cert); fclose(cert);
    if (rc || file_size_path(out_path) != 2560) {
        unlink(out_path); return patcher_fail("Cadena de certificados invalida");
    }
    return 0;
}

static int find_patch(const ChannelDef *channel, size_t position, const PatchDef **out) {
    for (size_t i = 0; i < channel->patch_count; ++i) {
        if (channel->patches[i].content_position == position) {
            *out = &channel->patches[i];
            return 1;
        }
    }
    *out = NULL;
    return 0;
}

static int patch_title_only(const ChannelDef *channel) {
    WiiTitle title;
    memset(&title, 0, sizeof(title));
    mkdir_recursive(WORK_DIR);
    mkdir_recursive(WAD_DIR);

    char url[1200], path[520], folder[96];
    lowercase(folder, sizeof(folder), channel->patch_folder);

    snprintf(url, sizeof(url), "%s/%s/tmd.%u", NUS_BASE, channel->title_id,
             channel->latest_version);
    snprintf(path, sizeof(path), "%s/title.tmd", WORK_DIR);
    if (download_component(url, path, "Descargando TMD") != 0) return -1;
    if (title_load_tmd(&title, path) != 0) return patcher_fail_detail("TMD invalido", title_last_error());

    if (channel->flags & CHANNEL_CUSTOM_TICKET) {
        snprintf(url, sizeof(url), "%s/%s/%s.tik", PATCHER_BASE, folder, channel->title_id);
    } else {
        snprintf(url, sizeof(url), "%s/%s/cetk", NUS_BASE, channel->title_id);
    }
    snprintf(path, sizeof(path), "%s/title.tik", WORK_DIR);
    if (download_component(url, path, "Descargando ticket") != 0) return -1;
    if (title_load_ticket(&title, path) != 0) return patcher_fail_detail("Ticket invalido", title_last_error());

    char cert_path[520];
    if (prepare_certificate_chain(cert_path, sizeof(cert_path)) != 0) return -1;
    if (title_load_cert(&title, cert_path) != 0) return patcher_fail_detail("Certificado invalido", title_last_error());

    for (size_t i = 0; i < title.content_count; ++i) {
        TitleContent *content = &title.contents[i];
        snprintf(url, sizeof(url), "%s/%s/%08lX", NUS_BASE, channel->title_id,
                 (unsigned long)content->cid);
        snprintf(path, sizeof(path), "%s/content-%02u-%08lX.app", WORK_DIR,
                 (unsigned)i, (unsigned long)content->cid);
        char status[128];
        i18n_snprintf(status, sizeof(status), "Contenido %u/%u (CID %08lX)",
                      (unsigned)(i + 1), title.content_count, (unsigned long)content->cid);
        if (download_component(url, path, status) != 0) return -1;
        snprintf(content->encrypted_path, sizeof(content->encrypted_path), "%.319s", path);
    }

    for (size_t i = 0; i < title.content_count; ++i) {
        const PatchDef *patch = NULL;
        if (!find_patch(channel, i, &patch)) continue;
        char original[520], patched[520], patch_file[520], encrypted[520];
        snprintf(original, sizeof(original), "%s/content-%02u.dec", WORK_DIR, (unsigned)i);
        snprintf(patched, sizeof(patched), "%s/content-%02u.patched", WORK_DIR, (unsigned)i);
        snprintf(patch_file, sizeof(patch_file), "%s/content-%02u.bsdiff", WORK_DIR, (unsigned)i);
        snprintf(encrypted, sizeof(encrypted), "%s/content-%02u-patched.app", WORK_DIR, (unsigned)i);

        emit("Descifrando contenido", 0, title.contents[i].size);
        if (title_decrypt_content(&title, i, title.contents[i].encrypted_path, original) != 0)
            return patcher_fail_detail("No se pudo descifrar contenido", title_last_error());
        if (title_verify_decrypted(&title, i, original) != 0)
            return patcher_fail_detail("Verificacion original fallo", title_last_error());

        snprintf(url, sizeof(url), "%s/bsdiff/%s/%s.bsdiff", PATCHER_BASE, folder, patch->name);
        char patch_status[160];
        i18n_snprintf(patch_status, sizeof(patch_status), "Descargando parche %s", patch->name);
        if (download_component(url, patch_file, patch_status) != 0) return -1;

        debug_set_step(TR("Aplicando parche BSDIFF"));
        g_status = TR("Aplicando parche BSDIFF");
        if (bspatch_file(original, patch_file, patched, transfer_progress, NULL) != 0)
            return patcher_fail_detail("BSDIFF fallo", bspatch_last_error());
        if (title_update_content(&title, i, patched, encrypted) != 0)
            return patcher_fail_detail("No se pudo actualizar TMD", title_last_error());
        emit("Cifrando contenido parcheado", 0, title.contents[i].size);
        if (title_encrypt_content(&title, i, patched, encrypted) != 0)
            return patcher_fail_detail("No se pudo cifrar contenido", title_last_error());
        unlink(original); unlink(patched); unlink(patch_file);
    }

    if (channel->flags & CHANNEL_CUSTOM_TMD) {
        snprintf(url, sizeof(url), "%s/%s/%s.tmd", PATCHER_BASE, folder, channel->patch_folder);
        snprintf(path, sizeof(path), "%s/custom.tmd", WORK_DIR);
        if (download_component(url, path, "Descargando TMD WiiLink") != 0) return -1;
        if (title_merge_custom_tmd(&title, path) != 0)
            return patcher_fail_detail("TMD WiiLink incompatible", title_last_error());
    }
    if (channel->patch_count || (channel->flags & CHANNEL_CUSTOM_TMD)) {
        if (title_fakesign_tmd(&title) != 0)
            return patcher_fail_detail("Fakesign TMD fallo", title_last_error());
    }

    const CategoryDef *category = catalog_category_for_channel(channel);
    char base_name[300], final_name[380];
    safe_filename(base_name, sizeof(base_name), channel->name);
    if (channel->patch_count && category && strcmp(category->network, "WiiLink"))
        snprintf(final_name, sizeof(final_name), "%s (%s).wad", base_name, category->network);
    else if (channel->patch_count)
        snprintf(final_name, sizeof(final_name), "%s (WiiLink).wad", base_name);
    else
        snprintf(final_name, sizeof(final_name), "%s.wad", base_name);
    snprintf(g_output, sizeof(g_output), "%s/%s", WAD_DIR, final_name);
    emit("Empaquetando WAD", 0, 0);
    if (title_build_wad(&title, g_output) != 0)
        return patcher_fail_detail("No se pudo crear WAD", title_last_error());
    return 0;
}

int patcher_patch_channel(const ChannelDef *channel, int include_dependencies) {
    if (!channel) return patcher_fail("Canal nulo");
    g_error[0] = '\0';
    g_output[0] = '\0';
    INFO("Selected channel %u_%u: %s", channel->category_id, channel->item_id, channel->name);
    debug_set_step(channel->name);
    remove_tree_files(WORK_DIR);
    mkdir_recursive(WORK_DIR);

    char primary_output[sizeof(g_output)];
    primary_output[0] = '\0';
    if (channel->title_id && channel->title_id[0]) {
        if (patch_title_only(channel) != 0) return -1;
        snprintf(primary_output, sizeof(primary_output), "%s", g_output);
    }
    for (size_t i = 0; i < channel->additional_app_count; ++i) {
        if (patcher_download_osc_app(channel->additional_apps[i]) != 0) return -1;
    }
    if (include_dependencies && channel->additional_category) {
        const ChannelDef *dependency = catalog_channel(channel->additional_category, channel->additional_item);
        if (!dependency || patcher_patch_channel(dependency, 0) != 0)
            return patcher_fail("No se pudo preparar un canal dependiente");
    }
    if (primary_output[0]) snprintf(g_output, sizeof(g_output), "%s", primary_output);
    emit("Completado", 1, 1);
    remove_tree_files(WORK_DIR);
    return 0;
}

int patcher_connection_test(void) {
    uint8_t buffer[4096];
    size_t size = 0;
    debug_set_step(TR("Diagnostico de red WiiLink"));
    emit("Probando servidor WiiLink", 0, 0);
    if (http_get_memory(PATCHER_BASE "/connectiontest.txt", buffer, sizeof(buffer), &size) != 0)
        return patcher_fail_detail("Servidor WiiLink inaccesible", http_last_error());
    static const char expected[] = "If the patcher can read this, the connection test succeeds.\n";
    if (size != sizeof(expected) - 1 || memcmp(buffer, expected, size) != 0)
        return patcher_fail("Respuesta inesperada del servidor WiiLink");
    emit("Probando Nintendo Update Servers", 0, 0);
    if (http_get_memory(NUS_BASE "/000100014841564a/tmd", buffer, sizeof(buffer), &size) != 0)
        return patcher_fail_detail("Nintendo Update Servers inaccesible", http_last_error());
    if (size < 0x1E4)
        return patcher_fail("Respuesta TMD de Nintendo truncada");
    emit("Probando Open Shop Channel", 0, 0);
    if (http_get_memory(OSC_BASE "/api/v4/information", buffer, sizeof(buffer), &size) != 0)
        return patcher_fail_detail("Open Shop Channel inaccesible", http_last_error());
    INFO("All connection tests passed");
    emit("Diagnostico correcto", 1, 1);
    return 0;
}
