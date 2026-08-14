#!/usr/bin/env python3
"""Static checks for the native interface translation catalog."""
from __future__ import annotations

import ast
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "source" / "i18n.c").read_text(encoding="utf-8")
PAIR_RE = re.compile(r'\{"((?:\\.|[^"\\])*)",\s*"((?:\\.|[^"\\])*)"\}')
FORMAT_RE = re.compile(r'%(?!%)(?:[-+ #0]*)(?:\d+|\*)?(?:\.\d+|\.\*)?(?:hh|h|ll|l|j|z|t|L)?[diuoxXfFeEgGaAcspn]')


def decode(value: str) -> str:
    return ast.literal_eval('"' + value + '"')


def main() -> int:
    pairs = [(decode(source), decode(english)) for source, english in PAIR_RE.findall(SOURCE)]
    errors: list[str] = []
    counts = collections.Counter(source for source, _ in pairs)
    duplicates = [source for source, count in counts.items() if count > 1]
    if duplicates:
        errors.append(f"duplicate source keys: {duplicates}")
    if len(pairs) < 200:
        errors.append(f"catalog unexpectedly small: {len(pairs)} entries")

    for source, english in pairs:
        if FORMAT_RE.findall(source) != FORMAT_RE.findall(english):
            errors.append(
                f"format mismatch: {source!r} -> {english!r}: "
                f"{FORMAT_RE.findall(source)} != {FORMAT_RE.findall(english)}"
            )

    catalog = dict(pairs)
    required = {
        "Menu principal": "Main menu",
        "Instalacion express": "Express setup",
        "Idioma de la interfaz": "Interface language",
        "Procesando canal": "Processing channel",
        "Aplicando parche BSDIFF": "Applying BSDIFF patch",
        "No se pudo crear WAD": "Could not create WAD",
        "No se pudo conectar al servidor": "Could not connect to server",
        "Tamano BSDIFF incorrecto: esperado %lld, escrito %ld":
            "Incorrect BSDIFF size: expected %lld, wrote %ld",
    }
    for source, expected in required.items():
        if catalog.get(source) != expected:
            errors.append(f"missing/incorrect required translation: {source!r}")

    all_code = "\n".join(path.read_text(encoding="utf-8") for path in (ROOT / "source").glob("*.c"))
    for stale in ("No apagues la consola ni retires la SD", "Los archivos estan listos en la SD"):
        if stale in all_code:
            errors.append(f"stale SD interface text remains: {stale!r}")

    if errors:
        print("Translation validation FAILED:")
        print("\n".join(f" - {error}" for error in errors))
        return 1
    print(f"Translation validation PASS: {len(pairs)} Spanish -> English entries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
