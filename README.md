# WiiLink Patcher Wii

Cliente nativo para Wii (`boot.dol`) basado en un fork de
[OpenShopChannel/libreshop-client](https://github.com/OpenShopChannel/libreshop-client).
Porta a C/libogc el flujo relevante de
[WiiLink24/WiiLink-Patcher-GUI](https://github.com/WiiLink24/WiiLink-Patcher-GUI):

1. descarga TMD, ticket y contenidos desde Nintendo Update Servers (NUS);
2. verifica SHA-1 y descifra el contenido con el motor AES de IOS;
3. descarga y aplica los parches `BSDIFF40` oficiales de WiiLink;
4. actualiza y fakesigna el TMD;
5. cifra los contenidos y genera el WAD en `usb:/WAD`;
6. descarga las aplicaciones auxiliares desde Open Shop Channel.

> **Seguridad:** la aplicación no instala ni modifica la NAND. Genera WAD y descarga
> `yawmME`; el usuario decide qué instalar. Comprueba siempre que la región del WAD
> sea la de la consola y conserva una copia de NAND/BootMii.

## Cambios en 0.2.0

- Selector persistente de idioma para la interfaz: Español y English.
- En el primer arranque se solicita el idioma; después puede cambiarse desde el
  menú principal.
- Catálogo extensible en `source/i18n.c`: el español es la fuente/fallback y las
  traducciones nuevas se añaden sin modificar la lógica de la interfaz.
- Los estados, errores HTTP/NUS/BSDIFF/AES/WAD y la pantalla de crash también se
  muestran en el idioma seleccionado.
- La preferencia se guarda en `usb:/apps/wiilink-patcher/language.cfg`.

## Cambios en 0.1.2

- Corrige el falso error `BSPATCH: el tamaño del parche no coincide` en USB.
  `libfat` puede mantener desactualizado el tamaño del directorio FAT mientras el
  archivo sigue abierto; ahora BSPATCH valida `new_pos` y `ftell()`, fuerza
  `fflush()`/`fclose()` y solo después renombra el resultado.

## Funciones

- Instalación express para Forecast, News, Nintendo Channel, Everybody Votes y
  Check Mii Out/Mii Contest, con selección de región.
- Selección personalizada de los canales WiiConnect24, regionales y extras presentes
  en `WiiLink-Patcher-GUI` v1.5.3.
- Wii Room en nueve idiomas; Photo Prints/Digicam; Food Channel estándar, Domino's
  y Just Eat; Kirby TV; Wii Speak; Today and Tomorrow; Internet Channel; System
  Channel Restorer.
- Descarga de `yawmME`, `sntp`, `Mail-Patcher`, AnyGlobe Changer, WSR Patcher y
  Account Linker cuando correspondan.
- Wii Remote, Classic Controller y mando de GameCube.
- HTTP con `Content-Length` o `chunked`, reintentos y escritura atómica `.part`.
- Procesamiento por streaming: los contenidos, AES y BSDIFF pasan por SD y no se
  cargan completos en RAM.

## Logs y depuración

- Log persistente: `usb:/apps/wiilink-patcher/logs/debug.log`.
- El log rota a `debug.log.old` al superar 512 KiB.
- Crashlog: `usb:/apps/wiilink-patcher/logs/crash.log`.
- El crash handler muestra excepción, PC, LR, último paso y stack trace en pantalla,
  y guarda los 32 registros GPR y hasta 16 direcciones de retorno.
- Pulsa **1** en Wii Remote o **X** en Classic/GameCube para alternar el debug en
  pantalla en cualquier menú.
- El ZIP incluye `debug-symbols/wiilink-patcher-wii.elf` y su `.map`. Para
  resolver una dirección:

```sh
powerpc-eabi-addr2line -e debug-symbols/wiilink-patcher-wii.elf -f -C 0xDIRECCION
```

La build de desarrollo arranca con el debug en pantalla activo:

```sh
make debug
```

## Compilación

Dependencias de devkitPro:

```sh
sudo dkp-pacman -S wii-dev ppc-bzip2
source /etc/profile.d/devkit-env.sh
make clean
make -j2
make package
```

Salidas:

- `wiilink-patcher-wii.dol`
- `wiilink-patcher-wii.elf`
- `wiilink-patcher-wii.elf.map`
- `wiilink-patcher-wii-0.2.0.zip`

Para instalar manualmente, copia el contenido del ZIP a la raíz de una unidad USB FAT32.
Homebrew Channel cargará `usb:/apps/wiilink-patcher/boot.dol`.

## Actualizar el catálogo

`source/catalog_generated.c` se genera desde el `patches.json` de
WiiLink-Patcher-GUI:

```sh
python3 tools/generate_catalog.py
```

El generador conserva `category_id`, `item_id`, región, idioma, versión NUS,
parches, TMD/ticket especiales, dependencias y aplicaciones auxiliares.

## Pruebas realizadas

- Compilación limpia con devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0.
- Arranque del DOL verificado en Dolphin 2503; el montaje `usb:/` requiere la prueba final en hardware real con almacenamiento USB.
- SHA-1 contrastado con el vector `SHA1("abc")`.
- Catálogo de interfaz validado: 210 entradas Español → English, sin claves
  duplicadas y con especificadores `printf` compatibles.
- `bspatch` por streaming contrastado con `bsdiff4` sobre datos aleatorios de
  500 KiB y sobre el parche real `forecast/Forecast_1.bsdiff`.
- Catálogo validado contra los endpoints HTTP de NUS, WiiLink Patcher y OSC.

No se puede sustituir una prueba en hardware real: antes de publicar una release,
prueba al menos una Wii y una vWii, conserva los logs y verifica los WAD con una
herramienta independiente.

## Licencias y atribución

- Base LibreShop: GPL-3.0, conservada en `LICENSE`.
- Datos/flujo de WiiLink-Patcher-GUI: MPL-2.0, conservada en
  `LICENSE.WIILINK-GUI`.
- BSDIFF40 es el formato creado por Colin Percival; este port usa la interfaz bzip2
  de devkitPro.
- Los parches y servicios descargados pertenecen a sus respectivos proyectos.
