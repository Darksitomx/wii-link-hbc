#include "ui.h"
#include "catalog.h"
#include "config.h"
#include "debug.h"
#include "i18n.h"
#include "patcher.h"

#include <gccore.h>
#include <ogc/conf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#define SCREEN_WIDTH 78
#define PAGE_ROWS 18

static int g_last_percent = -1;

static void clear_screen(void) { printf("\x1b[2J\x1b[1;1H"); }

static void print_header(const char *title) {
    clear_screen();
    printf("\x1b[44;37m %-48.48s %26s \x1b[0m\n", TR(title), APP_NAME " " APP_VERSION);
    printf("------------------------------------------------------------------------------\n");
}

static void wait_vsync_input(void) {
    VIDEO_WaitVSync();
    WPAD_ScanPads();
    PAD_ScanPads();
}

enum { KEY_NONE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_HOME, KEY_DEBUG };

static int read_key(void) {
    for (;;) {
        wait_vsync_input();
        u32 w = WPAD_ButtonsDown(0);
        u16 g = PAD_ButtonsDown(0);
        if ((w & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) || (g & PAD_BUTTON_UP)) return KEY_UP;
        if ((w & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) || (g & PAD_BUTTON_DOWN)) return KEY_DOWN;
        if ((w & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)) || (g & PAD_BUTTON_LEFT)) return KEY_LEFT;
        if ((w & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)) || (g & PAD_BUTTON_RIGHT)) return KEY_RIGHT;
        if ((w & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) || (g & PAD_BUTTON_A)) return KEY_A;
        if ((w & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)) || (g & PAD_BUTTON_B)) return KEY_B;
        if ((w & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)) || (g & PAD_BUTTON_START)) return KEY_HOME;
        if ((w & (WPAD_BUTTON_1 | WPAD_CLASSIC_BUTTON_X)) || (g & PAD_BUTTON_X)) return KEY_DEBUG;
    }
}

static int menu(const char *title, const char *const *items, size_t count, int initial, const char *help) {
    if (!count) return -1;
    int selected = initial >= 0 && initial < (int)count ? initial : 0;
    int offset = 0;
    for (;;) {
        if (selected < offset) offset = selected;
        if (selected >= offset + PAGE_ROWS) offset = selected - PAGE_ROWS + 1;
        print_header(title);
        size_t shown = count - (size_t)offset > PAGE_ROWS ? PAGE_ROWS : count - (size_t)offset;
        for (size_t row = 0; row < shown; ++row) {
            size_t index = (size_t)offset + row;
            printf(index == (size_t)selected ? "\x1b[47;30m > %-72.72s \x1b[0m\n" : "   %-72.72s\n", TR(items[index]));
        }
        for (size_t row = shown; row < PAGE_ROWS; ++row) printf("\n");
        printf("------------------------------------------------------------------------------\n");
        printf("%s\n", TR(help ? help : "A: aceptar   B: volver   HOME: salir"));
        i18n_printf("1/X: debug en pantalla [%s]", debug_screen_enabled() ? "ON" : "OFF");

        int key = read_key();
        if (key == KEY_UP) selected = selected > 0 ? selected - 1 : (int)count - 1;
        else if (key == KEY_DOWN) selected = selected + 1 < (int)count ? selected + 1 : 0;
        else if (key == KEY_LEFT) { selected -= PAGE_ROWS; if (selected < 0) selected = 0; }
        else if (key == KEY_RIGHT) { selected += PAGE_ROWS; if (selected >= (int)count) selected = (int)count - 1; }
        else if (key == KEY_A) return selected;
        else if (key == KEY_B) return -1;
        else if (key == KEY_HOME) return -2;
        else if (key == KEY_DEBUG) {
            debug_set_screen(!debug_screen_enabled());
            INFO("On-screen debug %s", debug_screen_enabled() ? "enabled" : "disabled");
        }
    }
}

static bool confirm(const char *title, const char *message) {
    int selected = 0;
    for (;;) {
        print_header(title);
        printf("\n%s\n\n", TR(message));
        printf("%s", TR("Los WAD se guardan en usb:/WAD. El programa NO escribe en NAND.\n"));
        printf("%s", TR("Haz una copia de NAND/BootMii antes de instalar WAD.\n\n"));
        if (selected == 0)
            printf("\x1b[47;30m > %-10s \x1b[0m     %s\n", TR("Continuar"), TR("Cancelar"));
        else
            printf("   %-10s     \x1b[47;30m > %s \x1b[0m\n", TR("Continuar"), TR("Cancelar"));
        printf("%s", TR("\nIzquierda/Derecha: elegir   A: aceptar   B: cancelar"));
        int key = read_key();
        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN)
            selected = !selected;
        else if (key == KEY_A) return selected == 0;
        else if (key == KEY_B || key == KEY_HOME) return false;
        else if (key == KEY_DEBUG) debug_set_screen(!debug_screen_enabled());
    }
}

static void wait_back(void) {
    printf("%s", TR("\nPulsa B/A/HOME para volver..."));
    for (;;) {
        int key = read_key();
        if (key == KEY_A || key == KEY_B || key == KEY_HOME) return;
    }
}

void ui_show_fatal(const char *title, const char *message) {
    print_header(title);
    printf("\x1b[31mERROR\x1b[0m\n\n%s\n\n", TR(message ? message : "Error desconocido"));
    i18n_printf("Ultimo paso: %s\n", debug_last_step());
    printf("Log: %s\n", debug_log_path());
    wait_back();
}

void ui_progress(const char *status, uint64_t done, uint64_t total, void *user) {
    (void)user;
    int percent = total ? (int)((done * 100u) / total) : -1;
    if (percent == g_last_percent && total) return;
    g_last_percent = percent;
    int bars = percent < 0 ? 0 : percent * 36 / 100;
    char bar[37];
    for (int i = 0; i < 36; ++i) bar[i] = i < bars ? '#' : '-';
    bar[36] = '\0';
    printf("\x1b[s\x1b[25;1H\x1b[2K%-56.56s\n\x1b[2K[%s] ", TR(status ? status : "Procesando"), bar);
    if (percent >= 0) printf("%3d%%", percent); else printf("%llu bytes", (unsigned long long)done);
    printf("\x1b[u");
    VIDEO_Flush();
}

static int detected_region_index(void) {
    int region = CONF_GetRegion();
    if (region == CONF_REGION_EU) return 1;
    if (region == CONF_REGION_JP) return 2;
    return 0;
}

static const ChannelDef *language_variant(uint16_t category_id, int language) {
    /* Language menu order: English, Spanish, French, German, Italian, Dutch,
       Portuguese, Russian, Japanese. */
    static const uint8_t wii_room_items[] = {2, 3, 4, 5, 6, 7, 8, 9, 1};
    if (category_id == 7) return catalog_channel(7, wii_room_items[language]);
    if (category_id == 8) return catalog_channel(8, language == 8 ? 1 : 2);
    if (category_id == 9) return catalog_channel(9, language == 8 ? 1 : 2);
    return NULL;
}

static int run_channel(const ChannelDef *channel) {
    print_header("Procesando canal");
    i18n_printf("Canal: %s\n", channel->name);
    printf("%s", TR("No apagues la consola ni retires la unidad USB.\n\n"));
    for (int i = 0; i < 17; ++i) printf("\n");
    g_last_percent = -2;
    int rc = patcher_patch_channel(channel, 1);
    printf("\x1b[24;1H\x1b[J");
    if (rc != 0) {
        ui_show_fatal("Fallo al preparar canal", patcher_last_error());
        return -1;
    }
    printf("\x1b[32m%s\x1b[0m %s\n", TR("Completado:"), channel->name);
    if (patcher_last_output()[0]) i18n_printf("Salida: %s\n", patcher_last_output());
    return 0;
}

static void express_setup(void) {
    static const char *const regions[] = {"Norteamerica (NTSC-U)", "Europa (PAL)", "Japon (NTSC-J)"};
    static const char *const languages[] = {
        "English", "Espanol", "Francais", "Deutsch", "Italiano",
        "Nederlands", "Portugues (Brasil)", "Russian", "Japanese"
    };
    static const char *const platforms[] = {"Wii", "vWii (Wii U)"};
    static const char *const regional_options[] = {
        "Ningun canal regional", "Wii Room / Wii no Ma", "Photo Prints / Digicam", "Food Channel (Standard)"
    };
    int region = menu("Express - region", regions, 3, detected_region_index(), NULL);
    if (region < 0) return;
    int language = menu("Express - idioma", languages, 9, 0, NULL);
    if (language < 0) return;
    int platform = menu("Express - plataforma", platforms, 2, 0, NULL);
    if (platform < 0) return;
    int regional = menu("Express - canal regional opcional", regional_options, 4, 0, NULL);
    if (regional < 0) return;
    if (!confirm("Confirmar instalacion express",
        "Se prepararan Forecast, News, Nintendo, Everybody Votes y Check Mii Out.\n"
        "Tambien se descargaran yawmME, sntp y Mail-Patcher.")) return;

    print_header("Preparando aplicaciones auxiliares");
    for (int i = 0; i < 20; ++i) printf("\n");
    if (patcher_prepare_support_apps() != 0) {
        ui_show_fatal("Fallo en aplicaciones auxiliares", patcher_last_error()); return;
    }
    if (platform == 1) {
        const ChannelDef *eula = catalog_channel(14, (uint16_t)(region + 1));
        if (run_channel(eula) != 0) return;
    }
    static const uint16_t base_categories[] = {1, 2, 3, 4, 6};
    for (size_t i = 0; i < sizeof(base_categories) / sizeof(base_categories[0]); ++i) {
        const ChannelDef *channel = catalog_channel(base_categories[i], (uint16_t)(region + 1));
        if (!channel || run_channel(channel) != 0) return;
    }
    if (regional) {
        const ChannelDef *channel = language_variant((uint16_t)(6 + regional), language);
        if (!channel || run_channel(channel) != 0) return;
        if (region != 2 && patcher_download_spd() != 0) {
            ui_show_fatal("Fallo en Address Settings", patcher_last_error()); return;
        }
    }
    print_header("Instalacion express lista");
    printf("%s", TR("Los WAD estan en usb:/WAD.\n"));
    printf("%s", TR("Abre yawmME desde Homebrew Channel e instala solo los WAD preparados.\n"));
    printf("%s", TR("Ejecuta sntp y Mail-Patcher cuando corresponda a la guia de WiiLink.\n"));
    printf("%s", TR("No instales WAD de una region equivocada.\n"));
    wait_back();
}

static bool type_matches(const CategoryDef *category, const char *type) {
    return category && !strcmp(category->type, type);
}

static void custom_setup(const char *type, const char *title) {
    const CategoryDef *categories[32];
    const char *labels[32];
    size_t count = 0;
    for (size_t i = 0; i < g_category_count && count < 32; ++i) {
        if (type_matches(&g_categories[i], type)) {
            categories[count] = &g_categories[i];
            labels[count] = g_categories[i].name;
            ++count;
        }
    }
    int cat_choice = menu(title, labels, count, 0, NULL);
    if (cat_choice < 0) return;
    const CategoryDef *category = categories[cat_choice];
    const char *variants[MAX_MENU_ITEMS];
    for (size_t i = 0; i < category->channel_count; ++i)
        variants[i] = g_channels[category->first_channel + i].name;
    int variant = menu(category->name, variants, category->channel_count, 0, NULL);
    if (variant < 0) return;
    const ChannelDef *channel = &g_channels[category->first_channel + variant];
    int regional_console_region = -1;
    if (!strcmp(type, "regional")) {
        static const char *const regions[] = {
            "Norteamerica (NTSC-U)", "Europa (PAL)", "Japon (NTSC-J)"
        };
        regional_console_region = menu("Region de la consola", regions, 3, detected_region_index(), NULL);
        if (regional_console_region < 0) return;
    }
    if (!confirm("Confirmar canal", channel->name)) return;

    print_header("Preparando yawmME y utilidades");
    for (int i = 0; i < 20; ++i) printf("\n");
    if (patcher_prepare_support_apps() != 0) {
        ui_show_fatal("Fallo en aplicaciones auxiliares", patcher_last_error()); return;
    }
    if (regional_console_region >= 0 && regional_console_region != 2) {
        if (patcher_download_spd() != 0) {
            ui_show_fatal("Fallo en Address Settings", patcher_last_error()); return;
        }
    }
    /* Domino's uses the regional Internet Channel, matching the GUI patcher's special case. */
    if (channel->category_id == 15) {
        const ChannelDef *internet = catalog_channel(16, (uint16_t)(regional_console_region + 1));
        if (!internet || run_channel(internet) != 0) return;
    }
    if (run_channel(channel) != 0) return;
    print_header("Canal preparado");
    printf("%s\n\n", channel->name);
    if (patcher_last_output()[0]) i18n_printf("Salida: %s\n", patcher_last_output());
    printf("%s", TR("Instala el WAD con yawmME desde Homebrew Channel.\n"));
    wait_back();
}

static void utilities_menu(void) {
    static const char *const items[] = {
        "Descargar yawmME + sntp + Mail-Patcher",
        "Descargar WiiLink Address Settings (SPD)",
        "Descargar System Channel Restorer",
        "Volver"
    };
    int choice = menu("Utilidades", items, 4, 0, NULL);
    if (choice < 0 || choice == 3) return;
    print_header("Descargando utilidades");
    for (int i = 0; i < 20; ++i) printf("\n");
    int rc = choice == 0 ? patcher_prepare_support_apps() :
             choice == 1 ? patcher_download_spd() : patcher_download_osc_app("system-channel-restorer");
    if (rc != 0) ui_show_fatal("Fallo de descarga", patcher_last_error());
    else { print_header("Descarga completa"); printf("%s", TR("Los archivos estan listos en la unidad USB.\n")); wait_back(); }
}

static void view_log(void) {
    print_header("Ultimas lineas de debug.log");
    FILE *fp = fopen(DEBUG_LOG_PATH, "rb");
    if (!fp) { printf("%s", TR("No hay log disponible.\n")); wait_back(); return; }
    char lines[20][79];
    int next = 0, used = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        snprintf(lines[next], sizeof(lines[next]), "%.77s", line);
        size_t n = strlen(lines[next]);
        while (n && (lines[next][n - 1] == '\n' || lines[next][n - 1] == '\r')) lines[next][--n] = '\0';
        next = (next + 1) % 20;
        if (used < 20) ++used;
    }
    fclose(fp);
    int start = used == 20 ? next : 0;
    for (int i = 0; i < used; ++i) printf("%s\n", lines[(start + i) % 20]);
    wait_back();
}

static void diagnostics(void) {
    static const char *const items[] = {"Probar red y servidores", "Ver debug.log", "Volver"};
    int choice = menu("Diagnostico", items, 3, 0, NULL);
    if (choice < 0 || choice == 2) return;
    if (choice == 1) { view_log(); return; }
    print_header("Diagnostico de red");
    printf("IOS: %d rev %d\n", IOS_GetVersion(), IOS_GetRevision());
    printf("Log: %s\n\n", DEBUG_LOG_PATH);
    for (int i = 0; i < 18; ++i) printf("\n");
    g_last_percent = -2;
    if (patcher_connection_test() != 0) ui_show_fatal("Diagnostico fallido", patcher_last_error());
    else { printf("\x1b[24;1H\x1b[J\x1b[32m%s\x1b[0m\n", TR("WiiLink, NUS y OSC responden correctamente.")); wait_back(); }
}

void ui_select_language(bool required) {
    static const char *const languages[] = {"Espanol", "English"};
    for (;;) {
        int choice = menu("Idioma / Language", languages, 2, (int)i18n_language(),
                          "A: seleccionar / select   B: volver / back");
        if (choice >= 0) {
            i18n_set_language((UiLanguage)choice, true);
            print_header("Idioma guardado");
            printf("%s", TR("La interfaz se actualizo.\n"));
            wait_back();
            return;
        }
        if (!required) return;
    }
}

void ui_run(void) {
    patcher_set_progress_callback(ui_progress, NULL);
    static const char *const items[] = {
        "Instalacion express",
        "Canales WiiConnect24 (personalizado)",
        "Canales regionales (personalizado)",
        "Extras",
        "Utilidades y aplicaciones auxiliares",
        "Diagnostico y logs",
        "Idioma de la interfaz",
        "Salir"
    };
    for (;;) {
        int choice = menu("Menu principal", items, sizeof(items) / sizeof(items[0]), 0,
                          "A: aceptar   B/HOME: salir   D-Pad: mover");
        if (choice < 0 || choice == 7) return;
        if (choice == 0) express_setup();
        else if (choice == 1) custom_setup("wc24", "Canales WiiConnect24");
        else if (choice == 2) custom_setup("regional", "Canales regionales");
        else if (choice == 3) custom_setup("extra", "Extras");
        else if (choice == 4) utilities_menu();
        else if (choice == 5) diagnostics();
        else if (choice == 6) ui_select_language(false);
    }
}
