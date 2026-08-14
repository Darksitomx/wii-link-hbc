#ifndef WIILINK_I18N_H
#define WIILINK_I18N_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    UI_LANGUAGE_SPANISH = 0,
    UI_LANGUAGE_ENGLISH = 1,
    UI_LANGUAGE_COUNT
} UiLanguage;

/* Loads usb:/apps/wiilink-patcher/language.cfg. Returns true if valid. */
bool i18n_init(void);
UiLanguage i18n_language(void);
void i18n_set_language(UiLanguage language, bool persist);
const char *i18n_language_code(UiLanguage language);
const char *i18n_language_name(UiLanguage language);
const char *i18n_tr(const char *spanish_source);
int i18n_printf(const char *spanish_format, ...) __attribute__((format(printf, 1, 2)));
int i18n_snprintf(char *buffer, size_t size, const char *spanish_format, ...)
    __attribute__((format(printf, 3, 4)));

/* Gettext-style source lookup. Spanish is the canonical/fallback catalog. */
#define TR(text) i18n_tr(text)

#endif
