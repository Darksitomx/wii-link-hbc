# WiiLink Patcher Wii

[Español](README-es.md)

> **Unofficial project:** this community client was created by **Darksito** and
> **Mila Soraki**. It is not affiliated with or endorsed by WiiLink.

Native Wii client (`boot.dol`) based on a fork of
[OpenShopChannel/libreshop-client](https://github.com/OpenShopChannel/libreshop-client).
It ports the relevant workflow from
[WiiLink24/WiiLink-Patcher-GUI](https://github.com/WiiLink24/WiiLink-Patcher-GUI)
to C/libogc:

1. downloads the TMD, ticket, and contents from Nintendo Update Servers (NUS);
2. verifies SHA-1 and decrypts content using the IOS AES engine;
3. downloads and applies the official WiiLink `BSDIFF40` patches;
4. updates and fakesigns the TMD;
5. encrypts the contents and generates the WAD in `usb:/WAD`;
6. downloads support applications from Open Shop Channel.

> **Safety:** the application does not install to or modify NAND. It generates WAD
> files and downloads `yawmME`; the user decides what to install. Always verify that
> the WAD region matches the console and keep a NAND/BootMii backup.

## Changes in 0.2.1

- The release ZIP is intentionally minimal. It contains only the
  `wiilink-patcher-wii` folder with `boot.dol`, `icon.png`, and `meta.xml`.
- Homebrew metadata is now in English and clearly identifies the application as
  an unofficial project created by Darksito and Mila Soraki.
- Runtime data now uses `usb:/apps/wiilink-patcher-wii` to match the packaged
  application folder.

## Changes in 0.2.0

- Persistent interface language selector: Español and English.
- The language is requested on first launch and can later be changed from the main
  menu.
- Extensible catalog in `source/i18n.c`: Spanish is the source/fallback language,
  and new translations can be added without changing the interface logic.
- Status messages, HTTP/NUS/BSDIFF/AES/WAD errors, and the crash screen are shown
  in the selected language.
- The preference is saved to `usb:/apps/wiilink-patcher-wii/language.cfg`.

## Changes in 0.1.2

- Fixes the false `BSPATCH: patch size does not match` error on USB.
  `libfat` may keep the FAT directory size stale while a file is open; BSPATCH now
  validates `new_pos` and `ftell()`, forces `fflush()`/`fclose()`, and only then
  renames the result.

## Features

- Express setup for Forecast, News, Nintendo Channel, Everybody Votes, and Check
  Mii Out/Mii Contest, with region selection.
- Custom selection of WiiConnect24, regional, and extra channels available in
  `WiiLink-Patcher-GUI` v1.5.3.
- Wii Room in nine languages; Photo Prints/Digicam; standard, Domino's, and Just
  Eat Food Channel variants; Kirby TV; Wii Speak; Today and Tomorrow; Internet
  Channel; and System Channel Restorer.
- Downloads `yawmME`, `sntp`, `Mail-Patcher`, AnyGlobe Changer, WSR Patcher, and
  Account Linker when required.
- Wii Remote, Classic Controller, and GameCube Controller support.
- HTTP support for `Content-Length` and `chunked` responses, retries, and atomic
  `.part` writes.
- Streaming processing: contents, AES, and BSDIFF pass through USB and are never
  loaded completely into RAM.

## Logs and debugging

- Persistent log: `usb:/apps/wiilink-patcher-wii/logs/debug.log`.
- The log rotates to `debug.log.old` after 512 KiB.
- Crash log: `usb:/apps/wiilink-patcher-wii/logs/crash.log`.
- The crash handler displays the exception, PC, LR, last step, and stack trace, and
  saves all 32 GPR registers plus up to 16 return addresses.
- Press **1** on a Wii Remote or **X** on a Classic/GameCube Controller to toggle
  on-screen debugging from any menu.
- Local builds produce `wiilink-patcher-wii.elf` and its `.map` for resolving
  crash addresses. They are intentionally excluded from the minimal release ZIP:

```sh
powerpc-eabi-addr2line -e wiilink-patcher-wii.elf -f -C 0xADDRESS
```

The development build starts with on-screen debugging enabled:

```sh
make debug
```

## Building

Required devkitPro packages:

```sh
sudo dkp-pacman -S wii-dev ppc-bzip2
source /etc/profile.d/devkit-env.sh
make clean
make -j2
make package
```

Outputs:

- `wiilink-patcher-wii.dol`
- `wiilink-patcher-wii.elf`
- `wiilink-patcher-wii.elf.map`
- `wiilink-patcher-wii-0.2.1.zip`

The release ZIP contains exactly:

```text
wiilink-patcher-wii/
├── boot.dol
├── icon.png
└── meta.xml
```

For a manual installation, copy the `wiilink-patcher-wii` folder into
`usb:/apps/`. The Homebrew Channel will load
`usb:/apps/wiilink-patcher-wii/boot.dol`.

## Updating the catalog

`source/catalog_generated.c` is generated from WiiLink-Patcher-GUI's
`patches.json`:

```sh
python3 tools/generate_catalog.py
```

The generator preserves `category_id`, `item_id`, region, language, NUS version,
patches, special TMD/ticket files, dependencies, and support applications.

## Validation performed

- Clean build with devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0.
- DOL startup verified in Dolphin 2503; the `usb:/` mount still requires final
  validation on real hardware with USB storage.
- SHA-1 checked against the `SHA1("abc")` test vector.
- Interface catalog validated: 210 Spanish → English entries, no duplicate keys,
  and compatible `printf` format specifiers.
- Streaming `bspatch` checked against `bsdiff4` using 500 KiB of randomized data
  and the real `forecast/Forecast_1.bsdiff` patch.
- Catalog checked against the NUS, WiiLink Patcher, and OSC HTTP endpoints.

Testing on real hardware cannot be replaced: before publishing a release, test at
least one Wii and one vWii, preserve the logs, and verify generated WAD files with
an independent tool.

## Licenses and attribution

- LibreShop base: GPL-3.0, preserved in `LICENSE`.
- WiiLink-Patcher-GUI data/workflow: MPL-2.0, preserved in
  `LICENSE.WIILINK-GUI`.
- BSDIFF40 is the format created by Colin Percival; this port uses devkitPro's
  bzip2 interface.
- Downloaded patches and services belong to their respective projects.
