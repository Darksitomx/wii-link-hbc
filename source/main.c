#include "config.h"
#include "debug.h"
#include "i18n.h"
#include "ui.h"
#include "util.h"

#include <fat.h>
#include <gccore.h>
#include <network.h>
#include <ogc/conf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

static void *g_framebuffer;
static GXRModeObj *g_video_mode;

static void init_video(void) {
    VIDEO_Init();
    g_video_mode = VIDEO_GetPreferredMode(NULL);
    g_framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(g_video_mode));
    console_init(g_framebuffer, 20, 20, g_video_mode->fbWidth, g_video_mode->xfbHeight,
                 g_video_mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(g_video_mode);
    VIDEO_SetNextFramebuffer(g_framebuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (g_video_mode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    printf("\x1b[2J\x1b[1;1H");
}

static int init_network(void) {
    char ip[16] = {0}, mask[16] = {0}, gateway[16] = {0};
    printf("%s", TR("Inicializando red (DHCP)...\n"));
    s32 rc = if_config(ip, mask, gateway, true, 20);
    if (rc < 0) {
        ERROR("if_config failed: %ld", (long)rc);
        return -1;
    }
    INFO("Network ready: ip=%s gateway=%s mask=%s", ip, gateway, mask);
    i18n_printf("Red lista: %s\n", ip);
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    init_video();
    WPAD_Init();
    PAD_Init();
    debug_install_crash_handler();

    i18n_set_language(CONF_GetLanguage() == CONF_LANG_ENGLISH
                          ? UI_LANGUAGE_ENGLISH : UI_LANGUAGE_SPANISH, false);
    printf("%s %s\nBuild %s\n\n", APP_NAME, APP_VERSION, APP_BUILD);
    printf("%s", TR("Montando USB...\n"));
    if (!fatInitDefault()) {
        printf("%s", TR("\nERROR: no se pudo inicializar FAT.\n"));
        printf("%s", TR("Conecta una unidad USB FAT32 y reinicia la aplicacion.\n"));
        for (;;) {
            VIDEO_WaitVSync(); WPAD_ScanPads(); PAD_ScanPads();
            if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) & PAD_BUTTON_START)) break;
        }
        return 1;
    }
    if (mkdir_recursive(APP_DIR) != 0 || mkdir_recursive(LOG_DIR) != 0 ||
        mkdir_recursive(WAD_DIR) != 0 || mkdir_recursive(WORK_DIR) != 0) {
        printf("%s", TR("\nERROR: usb:/ no esta montado o no permite escritura.\n"));
        printf("%s", TR("Usa una unidad USB FAT32 conectada al puerto 0.\n"));
        for (;;) {
            VIDEO_WaitVSync(); WPAD_ScanPads(); PAD_ScanPads();
            if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) & PAD_BUTTON_START)) break;
        }
        return 1;
    }
    bool language_configured = i18n_init();
    debug_init();
#ifdef DEBUG_SCREEN_DEFAULT
    debug_set_screen(true);
#endif
    if (!language_configured) ui_select_language(true);

    if (AES_Init() < 0) {
        ui_show_fatal(TR("AES no disponible"), TR("IOS no permitio abrir /dev/aes. Prueba otro IOS/HBC actualizado."));
        debug_close();
        return 2;
    }
    if (init_network() != 0) {
        ui_show_fatal(TR("Red no disponible"), TR("No se pudo obtener configuracion DHCP. Revisa la conexion de Wii y vuelve a abrir."));
        AES_Close();
        debug_close();
        return 3;
    }

    INFO("Startup completed");
    ui_run();

    debug_set_step(TR("Salida normal"));
    INFO("Exiting to Homebrew Channel");
    AES_Close();
    debug_close();
    printf("%s", TR("\x1b[2J\x1b[1;1HSaliendo...\n"));
    return 0;
}
