#include "debug.h"
#include "config.h"
#include "util.h"

#include <gccore.h>
#include <tuxedo/ppc/context.h>
#include <tuxedo/ppc/exception.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE *g_log;
static bool g_screen;
static volatile int g_in_crash;
static char g_step[160] = "startup";

static const char *level_name(LogLevel level) {
    static const char *const names[] = {"TRACE", "INFO", "WARN", "ERROR"};
    return (level >= LOG_TRACE && level <= LOG_ERROR) ? names[level] : "LOG";
}

static bool valid_stack_address(uint32_t address) {
    return (address >= 0x80000100u && address < 0x81800000u) ||
           (address >= 0x90000100u && address < 0x94000000u);
}

static const char *exception_name(unsigned exid) {
    switch (exid) {
        case PPC_EXCPT_MCHK: return "Machine Check";
        case PPC_EXCPT_DSI: return "DSI";
        case PPC_EXCPT_ISI: return "ISI";
        case PPC_EXCPT_ALIGN: return "Alignment";
        case PPC_EXCPT_UNDEF: return "Program";
        case PPC_EXCPT_FPU: return "Floating Point";
        case PPC_EXCPT_TRACE: return "Trace";
        case PPC_EXCPT_PM: return "Performance Monitor";
        case PPC_EXCPT_BKPT: return "Breakpoint";
        default: return "Unknown";
    }
}

static void print_crash(FILE *fp, unsigned exid, PPCContext *ctx) {
    if (!fp || !ctx) return;
    fprintf(fp, "WiiLink Patcher Wii %s (%s)\n", APP_VERSION, APP_BUILD);
    fprintf(fp, "Exception: %s (%u)\nLast step: %s\n", exception_name(exid), exid, g_step);
    fprintf(fp, "PC=%08lx LR=%08lx CTR=%08lx CR=%08lx XER=%08lx MSR=%08lx\n",
            (unsigned long)ctx->pc, (unsigned long)ctx->lr,
            (unsigned long)ctx->ctr, (unsigned long)ctx->cr,
            (unsigned long)ctx->xer, (unsigned long)ctx->msr);
    for (unsigned i = 0; i < 32; i += 4) {
        fprintf(fp, "r%-2u=%08lx r%-2u=%08lx r%-2u=%08lx r%-2u=%08lx\n",
                i, (unsigned long)ctx->gpr[i], i + 1, (unsigned long)ctx->gpr[i + 1],
                i + 2, (unsigned long)ctx->gpr[i + 2], i + 3, (unsigned long)ctx->gpr[i + 3]);
    }
    fprintf(fp, "Stack trace (use the .map file or powerpc-eabi-addr2line):\n");
    fprintf(fp, "  #00 %08lx (PC)\n  #01 %08lx (LR)\n",
            (unsigned long)ctx->pc, (unsigned long)ctx->lr);

    uint32_t sp = ctx->gpr[1];
    for (unsigned frame = 2; frame < 18 && valid_stack_address(sp); ++frame) {
        const uint32_t *record = (const uint32_t *)(uintptr_t)sp;
        uint32_t next = record[0];
        uint32_t lr = record[1];
        if (lr) fprintf(fp, "  #%02u %08lx\n", frame, (unsigned long)lr);
        if (next <= sp || !valid_stack_address(next) || (next & 3u)) break;
        sp = next;
    }
}

static void crash_panic(unsigned exid, PPCContext *ctx) {
    if (g_in_crash++) {
        for (;;) { }
    }

    FILE *fp = fopen(CRASH_LOG_PATH, "wb");
    if (fp) {
        print_crash(fp, exid, ctx);
        fflush(fp);
        fclose(fp);
    }
    if (g_log) {
        fprintf(g_log, "\n--- FATAL EXCEPTION ---\n");
        print_crash(g_log, exid, ctx);
        fflush(g_log);
    }

    printf("\x1b[2J\x1b[1;1H\x1b[41;37m WiiLink Patcher Wii - CRASH \x1b[0m\n\n");
    printf("Excepcion %s (%u)\n", exception_name(exid), exid);
    printf("PC %08lx   LR %08lx\n", (unsigned long)ctx->pc, (unsigned long)ctx->lr);
    printf("Ultimo paso: %.58s\n\n", g_step);
    printf("Crashlog: %s\n", CRASH_LOG_PATH);
    printf("Debug log: %s\n\n", DEBUG_LOG_PATH);
    printf("Fotografia esta pantalla y conserva los logs.\n");
    printf("Usa boot.elf.map para resolver las direcciones.\n");
    printf("Mantén POWER para apagar la consola.\n\nStack:\n");

    uint32_t sp = ctx->gpr[1];
    printf("%08lx %08lx ", (unsigned long)ctx->pc, (unsigned long)ctx->lr);
    for (unsigned frame = 0; frame < 10 && valid_stack_address(sp); ++frame) {
        const uint32_t *record = (const uint32_t *)(uintptr_t)sp;
        uint32_t next = record[0];
        printf("%08lx%s", (unsigned long)record[1], (frame % 4 == 3) ? "\n" : " ");
        if (next <= sp || !valid_stack_address(next) || (next & 3u)) break;
        sp = next;
    }
    VIDEO_Flush();
    for (;;) VIDEO_WaitVSync();
}

void debug_install_crash_handler(void) {
    PPCExcptCurPanicFn = crash_panic;
}

void debug_init(void) {
    mkdir_recursive(LOG_DIR);
    if (file_size_path(DEBUG_LOG_PATH) > 512u * 1024u) {
        char old_path[256];
        snprintf(old_path, sizeof(old_path), "%s.old", DEBUG_LOG_PATH);
        unlink(old_path);
        rename(DEBUG_LOG_PATH, old_path);
    }
    g_log = fopen(DEBUG_LOG_PATH, "ab");
    time_t now = time(NULL);
    if (g_log) {
        fprintf(g_log, "\n=== %s %s | session %ld ===\n", APP_NAME, APP_VERSION, (long)now);
        fflush(g_log);
    }
    INFO("Build %s; IOS %d rev %d", APP_BUILD, IOS_GetVersion(), IOS_GetRevision());
}

void debug_close(void) {
    if (g_log) {
        INFO("Session closed normally");
        fclose(g_log);
        g_log = NULL;
    }
}

void debug_set_screen(bool enabled) { g_screen = enabled; }
bool debug_screen_enabled(void) { return g_screen; }

void debug_log(LogLevel level, const char *fmt, ...) {
    char message[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    time_t now = time(NULL);
    if (g_log) {
        fprintf(g_log, "[%ld] %-5s %s\n", (long)now, level_name(level), message);
        fflush(g_log);
    }
    if (g_screen || level >= LOG_WARN) {
        const char *color = level == LOG_ERROR ? "\x1b[31m" :
                            level == LOG_WARN ? "\x1b[33m" :
                            level == LOG_TRACE ? "\x1b[36m" : "\x1b[37m";
        printf("%s[%s]\x1b[0m %s\n", color, level_name(level), message);
    }
}

void debug_set_step(const char *step) {
    if (!step) return;
    snprintf(g_step, sizeof(g_step), "%s", step);
    TRACE("STEP: %s", g_step);
}

const char *debug_last_step(void) { return g_step; }
const char *debug_log_path(void) { return DEBUG_LOG_PATH; }
void debug_flush(void) { if (g_log) fflush(g_log); }
