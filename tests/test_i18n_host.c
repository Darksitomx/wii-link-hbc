#include "i18n.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    i18n_set_language(UI_LANGUAGE_SPANISH, false);
    if (strcmp(TR("Menu principal"), "Menu principal")) return 1;

    i18n_set_language(UI_LANGUAGE_ENGLISH, false);
    if (strcmp(TR("Menu principal"), "Main menu")) return 2;
    if (strcmp(TR("Texto futuro sin traduccion"), "Texto futuro sin traduccion")) return 3;

    char buffer[128];
    i18n_snprintf(buffer, sizeof(buffer), "Contenido %u/%u (CID %08lX)", 2u, 7u, 0x1234ul);
    if (strcmp(buffer, "Content 2/7 (CID 00001234)")) {
        fprintf(stderr, "Unexpected translation: %s\n", buffer);
        return 4;
    }
    return 0;
}
