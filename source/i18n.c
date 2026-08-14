#include "i18n.h"
#include "config.h"
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *source;
    const char *english;
} TranslationEntry;

static UiLanguage g_language = UI_LANGUAGE_SPANISH;

/*
 * Spanish is the stable source language. To add another translation, add a
 * catalog column/table and extend UiLanguage; callers never need to change.
 */
static const TranslationEntry english_catalog[] = {
    /* Startup and global UI. */
    {"Inicializando red (DHCP)...\n", "Initializing network (DHCP)...\n"},
    {"Red lista: %s\n", "Network ready: %s\n"},
    {"Montando USB...\n", "Mounting USB...\n"},
    {"\nERROR: no se pudo inicializar FAT.\n", "\nERROR: FAT could not be initialized.\n"},
    {"Conecta una unidad USB FAT32 y reinicia la aplicacion.\n", "Connect a FAT32 USB drive and restart the application.\n"},
    {"\nERROR: usb:/ no esta montado o no permite escritura.\n", "\nERROR: usb:/ is not mounted or is not writable.\n"},
    {"Usa una unidad USB FAT32 conectada al puerto 0.\n", "Use a FAT32 USB drive connected to port 0.\n"},
    {"AES no disponible", "AES unavailable"},
    {"IOS no permitio abrir /dev/aes. Prueba otro IOS/HBC actualizado.", "IOS could not open /dev/aes. Try another IOS or an updated HBC."},
    {"Red no disponible", "Network unavailable"},
    {"No se pudo obtener configuracion DHCP. Revisa la conexion de Wii y vuelve a abrir.", "DHCP configuration failed. Check the Wii network connection and reopen the app."},
    {"Salida normal", "Normal exit"},
    {"\x1b[2J\x1b[1;1HSaliendo...\n", "\x1b[2J\x1b[1;1HExiting...\n"},
    {"A: aceptar   B: volver   HOME: salir", "A: select   B: back   HOME: exit"},
    {"1/X: debug en pantalla [%s]", "1/X: on-screen debug [%s]"},
    {"Los WAD se guardan en usb:/WAD. El programa NO escribe en NAND.\n", "WAD files are saved to usb:/WAD. This app does NOT write to NAND.\n"},
    {"Haz una copia de NAND/BootMii antes de instalar WAD.\n\n", "Make a NAND/BootMii backup before installing WAD files.\n\n"},
    {"Continuar", "Continue"},
    {"Cancelar", "Cancel"},
    {"\nIzquierda/Derecha: elegir   A: aceptar   B: cancelar", "\nLeft/Right: choose   A: select   B: cancel"},
    {"\nPulsa B/A/HOME para volver...", "\nPress B/A/HOME to go back..."},
    {"Error desconocido", "Unknown error"},
    {"Ultimo paso: %s\n", "Last step: %s\n"},
    {"Procesando", "Processing"},
    {"Procesando canal", "Processing channel"},
    {"Canal: %s\n", "Channel: %s\n"},
    {"No apagues la consola ni retires la unidad USB.\n\n", "Do not power off the console or remove the USB drive.\n\n"},
    {"Fallo al preparar canal", "Failed to prepare channel"},
    {"Completado:", "Completed:"},
    {"Salida: %s\n", "Output: %s\n"},
    {"Norteamerica (NTSC-U)", "North America (NTSC-U)"},
    {"Europa (PAL)", "Europe (PAL)"},
    {"Japon (NTSC-J)", "Japan (NTSC-J)"},
    {"Ningun canal regional", "No regional channel"},
    {"Express - region", "Express - region"},
    {"Express - idioma", "Express - channel language"},
    {"Express - plataforma", "Express - platform"},
    {"Express - canal regional opcional", "Express - optional regional channel"},
    {"Confirmar instalacion express", "Confirm express setup"},
    {"Se prepararan Forecast, News, Nintendo, Everybody Votes y Check Mii Out.\nTambien se descargaran yawmME, sntp y Mail-Patcher.", "Forecast, News, Nintendo, Everybody Votes and Check Mii Out will be prepared.\nyawmME, sntp and Mail-Patcher will also be downloaded."},
    {"Preparando aplicaciones auxiliares", "Preparing support applications"},
    {"Fallo en aplicaciones auxiliares", "Support application failure"},
    {"Fallo en Address Settings", "Address Settings failure"},
    {"Instalacion express lista", "Express setup complete"},
    {"Los WAD estan en usb:/WAD.\n", "WAD files are in usb:/WAD.\n"},
    {"Abre yawmME desde Homebrew Channel e instala solo los WAD preparados.\n", "Open yawmME from the Homebrew Channel and install only the prepared WAD files.\n"},
    {"Ejecuta sntp y Mail-Patcher cuando corresponda a la guia de WiiLink.\n", "Run sntp and Mail-Patcher when requested by the WiiLink guide.\n"},
    {"No instales WAD de una region equivocada.\n", "Do not install WAD files for the wrong region.\n"},
    {"Region de la consola", "Console region"},
    {"Confirmar canal", "Confirm channel"},
    {"Preparando yawmME y utilidades", "Preparing yawmME and utilities"},
    {"Canal preparado", "Channel prepared"},
    {"Instala el WAD con yawmME desde Homebrew Channel.\n", "Install the WAD with yawmME from the Homebrew Channel.\n"},
    {"Descargar yawmME + sntp + Mail-Patcher", "Download yawmME + sntp + Mail-Patcher"},
    {"Descargar WiiLink Address Settings (SPD)", "Download WiiLink Address Settings (SPD)"},
    {"Descargar System Channel Restorer", "Download System Channel Restorer"},
    {"Volver", "Back"},
    {"Utilidades", "Utilities"},
    {"Descargando utilidades", "Downloading utilities"},
    {"Fallo de descarga", "Download failure"},
    {"Descarga completa", "Download complete"},
    {"Los archivos estan listos en la unidad USB.\n", "The files are ready on the USB drive.\n"},
    {"Ultimas lineas de debug.log", "Latest debug.log lines"},
    {"No hay log disponible.\n", "No log is available.\n"},
    {"Probar red y servidores", "Test network and servers"},
    {"Ver debug.log", "View debug.log"},
    {"Diagnostico", "Diagnostics"},
    {"Diagnostico de red", "Network diagnostics"},
    {"Diagnostico fallido", "Diagnostics failed"},
    {"WiiLink, NUS y OSC responden correctamente.", "WiiLink, NUS and OSC are responding correctly."},
    {"Instalacion express", "Express setup"},
    {"Canales WiiConnect24 (personalizado)", "WiiConnect24 channels (custom)"},
    {"Canales regionales (personalizado)", "Regional channels (custom)"},
    {"Utilidades y aplicaciones auxiliares", "Utilities and support applications"},
    {"Diagnostico y logs", "Diagnostics and logs"},
    {"Salir", "Exit"},
    {"Menu principal", "Main menu"},
    {"A: aceptar   B/HOME: salir   D-Pad: mover", "A: select   B/HOME: exit   D-Pad: move"},
    {"Canales WiiConnect24", "WiiConnect24 channels"},
    {"Canales regionales", "Regional channels"},
    {"Idioma de la interfaz", "Interface language"},
    {"Idioma guardado", "Language saved"},
    {"La interfaz se actualizo.\n", "The interface has been updated.\n"},

    /* Patcher statuses and errors. */
    {"error desconocido", "unknown error"},
    {"No se pudo crear directorio de app OSC", "Could not create the OSC app directory"},
    {"Descargando %s: %s", "Downloading %s: %s"},
    {"Descargando %s: icon.png", "Downloading %s: icon.png"},
    {"Descargando WiiLink Address Settings", "Downloading WiiLink Address Settings"},
    {"Descargando certificado CP", "Downloading CP certificate"},
    {"Descargando certificados CA/XS", "Downloading CA/XS certificates"},
    {"No se pudieron preparar certificados", "Could not prepare certificates"},
    {"No se pudo crear cert.bin", "Could not create cert.bin"},
    {"Cadena de certificados invalida", "Invalid certificate chain"},
    {"Descargando TMD", "Downloading TMD"},
    {"TMD invalido", "Invalid TMD"},
    {"Descargando ticket", "Downloading ticket"},
    {"Ticket invalido", "Invalid ticket"},
    {"Certificado invalido", "Invalid certificate"},
    {"Contenido %u/%u (CID %08lX)", "Content %u/%u (CID %08lX)"},
    {"Descifrando contenido", "Decrypting content"},
    {"No se pudo descifrar contenido", "Could not decrypt content"},
    {"Verificacion original fallo", "Original content verification failed"},
    {"Descargando parche %s", "Downloading patch %s"},
    {"Aplicando parche BSDIFF", "Applying BSDIFF patch"},
    {"BSDIFF fallo", "BSDIFF failed"},
    {"No se pudo actualizar TMD", "Could not update TMD"},
    {"Cifrando contenido parcheado", "Encrypting patched content"},
    {"No se pudo cifrar contenido", "Could not encrypt content"},
    {"Descargando TMD WiiLink", "Downloading WiiLink TMD"},
    {"TMD WiiLink incompatible", "Incompatible WiiLink TMD"},
    {"Fakesign TMD fallo", "TMD fakesigning failed"},
    {"Empaquetando WAD", "Packing WAD"},
    {"No se pudo crear WAD", "Could not create WAD"},
    {"Canal nulo", "Null channel"},
    {"No se pudo preparar un canal dependiente", "Could not prepare a dependent channel"},
    {"Completado", "Completed"},
    {"Diagnostico de red WiiLink", "WiiLink network diagnostics"},
    {"Probando servidor WiiLink", "Testing WiiLink server"},
    {"Servidor WiiLink inaccesible", "WiiLink server is unreachable"},
    {"Respuesta inesperada del servidor WiiLink", "Unexpected response from the WiiLink server"},
    {"Probando Nintendo Update Servers", "Testing Nintendo Update Servers"},
    {"Nintendo Update Servers inaccesible", "Nintendo Update Servers are unreachable"},
    {"Respuesta TMD de Nintendo truncada", "Truncated Nintendo TMD response"},
    {"Probando Open Shop Channel", "Testing Open Shop Channel"},
    {"Open Shop Channel inaccesible", "Open Shop Channel is unreachable"},
    {"Diagnostico correcto", "Diagnostics successful"},

    /* Title/WAD errors. */
    {"No se pudo abrir un componente del titulo", "Could not open a title component"},
    {"Error leyendo componente del titulo", "Error reading a title component"},
    {"TMD demasiado pequeno", "TMD is too small"},
    {"Registros de contenido TMD no validos", "Invalid TMD content records"},
    {"Contenido demasiado grande para FAT/libogc", "Content is too large for FAT/libogc"},
    {"TMD personalizado truncado", "Truncated custom TMD"},
    {"TMD personalizado incompatible", "Incompatible custom TMD"},
    {"Ticket truncado", "Truncated ticket"},
    {"Solo se admiten tickets v0", "Only v0 tickets are supported"},
    {"Indice de common key desconocido", "Unknown common key index"},
    {"No se pudo descifrar title key", "Could not decrypt title key"},
    {"Cadena de certificados truncada", "Truncated certificate chain"},
    {"Posicion de contenido no valida", "Invalid content position"},
    {"No se pudo abrir contenido para criptografia", "Could not open content for cryptography"},
    {"No se pudo crear contenido cifrado/descifrado", "Could not create encrypted/decrypted content"},
    {"Memoria insuficiente para AES", "Not enough memory for AES"},
    {"Contenido descifrado truncado", "Truncated decrypted content"},
    {"Contenido NUS cifrado truncado", "Truncated encrypted NUS content"},
    {"Fallo del motor AES", "AES engine failure"},
    {"Error escribiendo resultado AES", "Error writing AES output"},
    {"No se pudo finalizar resultado AES", "Could not finalize AES output"},
    {"Descifrando contenido NUS", "Decrypting NUS content"},
    {"No se pudo calcular SHA-1 original", "Could not calculate original SHA-1"},
    {"SHA-1 original no coincide; descarga o ticket incorrecto", "Original SHA-1 mismatch; incorrect download or ticket"},
    {"Contenido parcheado vacio", "Patched content is empty"},
    {"No se pudo calcular SHA-1 parcheado", "Could not calculate patched SHA-1"},
    {"No hay TMD para fakesign", "No TMD is available for fakesigning"},
    {"No se pudo fakesign el TMD", "Could not fakesign the TMD"},
    {"No se pudo abrir contenido al crear WAD", "Could not open content while creating WAD"},
    {"No se pudo copiar contenido al WAD", "Could not copy content into the WAD"},
    {"Faltan componentes para crear WAD", "Title components are missing for WAD creation"},
    {"Falta un contenido cifrado", "An encrypted content file is missing"},
    {"Archivo de contenido cifrado incompleto", "Encrypted content file is incomplete"},
    {"Region de contenido WAD demasiado grande", "WAD content region is too large"},
    {"No se pudo crear directorio WAD", "Could not create WAD directory"},
    {"No se pudieron escribir metadatos WAD", "Could not write WAD metadata"},
    {"No se pudo alinear contenido WAD", "Could not align WAD content"},
    {"No se pudo finalizar WAD", "Could not finalize WAD"},
    {"No se pudo renombrar WAD final", "Could not rename final WAD"},

    /* BSPATCH errors. */
    {"No se pudo abrir el parche", "Could not open the patch"},
    {"Cabecera del parche truncada", "Truncated patch header"},
    {"Formato de parche no reconocido", "Unrecognized patch format"},
    {"Tamanos BSDIFF no validos", "Invalid BSDIFF sizes"},
    {"No se pudieron abrir archivos temporales", "Could not open temporary files"},
    {"No se pudo posicionar el parche", "Could not seek within the patch"},
    {"No se pudieron abrir flujos bzip2", "Could not open bzip2 streams"},
    {"Flujo de control BSDIFF truncado", "Truncated BSDIFF control stream"},
    {"Datos de control BSDIFF no validos", "Invalid BSDIFF control data"},
    {"Flujo diff BSDIFF truncado", "Truncated BSDIFF diff stream"},
    {"No se pudo leer el contenido original", "Could not read original content"},
    {"No se pudo escribir contenido parcheado", "Could not write patched content"},
    {"Flujo extra BSDIFF truncado", "Truncated BSDIFF extra stream"},
    {"No se pudo escribir datos extra", "Could not write extra data"},
    {"No se pudo vaciar el contenido parcheado", "Could not flush patched content"},
    {"Tamano BSDIFF incorrecto: esperado %lld, escrito %ld", "Incorrect BSDIFF size: expected %lld, wrote %ld"},
    {"No se pudo cerrar el contenido parcheado", "Could not close patched content"},
    {"No se pudo finalizar contenido parcheado", "Could not finalize patched content"},

    /* HTTP errors. */
    {"Solo se admiten URL http://", "Only http:// URLs are supported"},
    {"Host HTTP no valido", "Invalid HTTP host"},
    {"Ruta HTTP demasiado larga", "HTTP path is too long"},
    {"Fallo enviando solicitud", "Failed to send request"},
    {"No se pudo resolver DNS", "DNS resolution failed"},
    {"No se pudo crear socket", "Could not create socket"},
    {"No se pudo conectar al servidor", "Could not connect to server"},
    {"Respuesta chunked truncada", "Truncated chunked response"},
    {"Cuerpo HTTP chunked incompleto", "Incomplete chunked HTTP body"},
    {"Separador chunked no valido", "Invalid chunk separator"},
    {"Descarga HTTP incompleta", "Incomplete HTTP download"},
    {"Error recibiendo HTTP", "HTTP receive error"},
    {"Cabecera HTTP incompleta", "Incomplete HTTP header"},
    {"Cabecera HTTP demasiado grande", "HTTP header is too large"},
    {"Estado HTTP no valido", "Invalid HTTP status"},
    {"Servidor devolvio estado HTTP", "Server returned HTTP status"},
    {"Error escribiendo en SD/USB", "Error writing to USB storage"},
    {"Respuesta HTTP excede el buffer", "HTTP response exceeds the buffer"},
    {"No se pudo crear el directorio de destino", "Could not create destination directory"},
    {"No se pudo abrir el archivo de destino", "Could not open destination file"},
    {"No se pudo finalizar el archivo descargado", "Could not finalize downloaded file"},
    {"Reintento HTTP %d/%d: %s", "HTTP retry %d/%d: %s"},

    /* Crash screen. */
    {"Excepcion %s (%u)\n", "Exception %s (%u)\n"},
    {"Ultimo paso: %.58s\n\n", "Last step: %.58s\n\n"},
    {"Fotografia esta pantalla y conserva los logs.\n", "Take a photo of this screen and keep the logs.\n"},
    {"Usa boot.elf.map para resolver las direcciones.\n", "Use the ELF/map symbols to resolve addresses.\n"},
    {"Manten POWER para apagar la consola.\n\nStack:\n", "Hold POWER to turn off the console.\n\nStack:\n"},
};

const char *i18n_language_code(UiLanguage language) {
    return language == UI_LANGUAGE_ENGLISH ? "en" : "es";
}

const char *i18n_language_name(UiLanguage language) {
    return language == UI_LANGUAGE_ENGLISH ? "English" : "Espanol";
}

UiLanguage i18n_language(void) { return g_language; }

const char *i18n_tr(const char *source) {
    if (!source || g_language == UI_LANGUAGE_SPANISH) return source;
    if (g_language == UI_LANGUAGE_ENGLISH) {
        for (size_t i = 0; i < sizeof(english_catalog) / sizeof(english_catalog[0]); ++i) {
            if (!strcmp(source, english_catalog[i].source)) return english_catalog[i].english;
        }
    }
    return source;
}

int i18n_printf(const char *spanish_format, ...) {
    va_list args;
    va_start(args, spanish_format);
    int result = vprintf(i18n_tr(spanish_format), args);
    va_end(args);
    return result;
}

int i18n_snprintf(char *buffer, size_t size, const char *spanish_format, ...) {
    va_list args;
    va_start(args, spanish_format);
    int result = vsnprintf(buffer, size, i18n_tr(spanish_format), args);
    va_end(args);
    return result;
}

static void save_language(void) {
    char partial[256];
    snprintf(partial, sizeof(partial), "%s.part", LANGUAGE_CONFIG_PATH);
    FILE *fp = fopen(partial, "wb");
    if (!fp) {
        WARN("Could not save interface language");
        return;
    }
    fprintf(fp, "language=%s\n", i18n_language_code(g_language));
    if (fclose(fp) != 0) {
        unlink(partial);
        WARN("Could not close interface language file");
        return;
    }
    unlink(LANGUAGE_CONFIG_PATH);
    if (rename(partial, LANGUAGE_CONFIG_PATH) != 0) {
        unlink(partial);
        WARN("Could not finalize interface language file");
    }
}

void i18n_set_language(UiLanguage language, bool persist) {
    if (language < 0 || language >= UI_LANGUAGE_COUNT) language = UI_LANGUAGE_SPANISH;
    g_language = language;
    if (persist) save_language();
}

bool i18n_init(void) {
    FILE *fp = fopen(LANGUAGE_CONFIG_PATH, "rb");
    if (!fp) return false;
    char line[64] = {0};
    bool valid = false;
    if (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "language=en")) {
            g_language = UI_LANGUAGE_ENGLISH;
            valid = true;
        } else if (strstr(line, "language=es")) {
            g_language = UI_LANGUAGE_SPANISH;
            valid = true;
        }
    }
    fclose(fp);
    return valid;
}
