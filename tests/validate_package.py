#!/usr/bin/env python3
"""Ensure release ZIP contains only the HBC application folder and three files."""
from __future__ import annotations

import pathlib
import re
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CONFIG = (ROOT / "source" / "config.h").read_text(encoding="utf-8")
VERSION = re.search(r'#define APP_VERSION "([^"]+)"', CONFIG).group(1)
ARCHIVE = ROOT / f"wiilink-patcher-wii-{VERSION}.zip"
EXPECTED = {
    "wiilink-patcher-wii/boot.dol",
    "wiilink-patcher-wii/icon.png",
    "wiilink-patcher-wii/meta.xml",
}


def main() -> int:
    if not ARCHIVE.exists():
        print(f"Package validation FAILED: {ARCHIVE.name} does not exist")
        return 1
    with zipfile.ZipFile(ARCHIVE) as archive:
        files = {name for name in archive.namelist() if not name.endswith("/")}
        if files != EXPECTED:
            print("Package validation FAILED")
            print(f"Expected: {sorted(EXPECTED)}")
            print(f"Received: {sorted(files)}")
            return 1
        if archive.testzip() is not None:
            print("Package validation FAILED: corrupt ZIP entry")
            return 1
    print(f"Package validation PASS: {ARCHIVE.name} contains only boot.dol, icon.png, and meta.xml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
