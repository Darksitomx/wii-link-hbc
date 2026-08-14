#!/usr/bin/env python3
"""Generate the compact Wii-side catalog from WiiLink-Patcher-GUI patches.json."""
from __future__ import annotations

import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "data" / "patches.json"
OUTPUT = ROOT / "source" / "catalog_generated.c"


def cstr(value) -> str:
    if value is None:
        value = ""
    return json.dumps(str(value), ensure_ascii=False)


def ident(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", value)


def main() -> None:
    src = DEFAULT_INPUT
    data = json.loads(src.read_text(encoding="utf-8"))
    lines = [
        "/* Generated from WiiLink-Patcher-GUI/data/patches.json. Do not edit manually. */",
        '#include "catalog.h"',
        "",
    ]
    channel_names: list[str] = []
    category_rows: list[str] = []

    for category in data:
        cat_id = category["category_id"]
        channel_symbols = []
        for channel in category["channels"]:
            sym = f"cat_{cat_id}_item_{channel['item_id']}"
            channel_symbols.append(sym)
            patches = channel.get("patches") or []
            if patches:
                lines.append(f"static const PatchDef {sym}_patches[] = {{")
                for patch in patches:
                    lines.append(
                        f"    {{ {cstr(patch['patch_name'])}, {int(patch['content_id'])} }},"
                    )
                lines.append("};")
            apps = channel.get("additional_apps") or []
            if apps:
                lines.append(f"static const char *const {sym}_apps[] = {{")
                for app in apps:
                    lines.append(f"    {cstr(app)},")
                lines.append("};")
            lines.append("")

        first_index = len(channel_names)
        for sym in channel_symbols:
            channel_names.append(sym)
        category_rows.append(
            "    { %d, %s, %s, %s, %d, %d },"
            % (
                cat_id,
                cstr(category["name"]),
                cstr(category["type"]),
                cstr(category["network"]),
                first_index,
                len(channel_symbols),
            )
        )

    lines.append("const ChannelDef g_channels[] = {")
    for category in data:
        cat_id = category["category_id"]
        for channel in category["channels"]:
            sym = f"cat_{cat_id}_item_{channel['item_id']}"
            patches = channel.get("patches") or []
            apps = channel.get("additional_apps") or []
            files = channel.get("additional_files") or []
            additional = channel.get("additional_channels") or []
            add_cat = additional[0]["category"] if additional else 0
            add_item = additional[0]["channel"] if additional else 0
            flags = []
            if "ticket" in files:
                flags.append("CHANNEL_CUSTOM_TICKET")
            if "tmd" in files:
                flags.append("CHANNEL_CUSTOM_TMD")
            flag_expr = " | ".join(flags) if flags else "0"
            lines.append("    {")
            lines.append(
                f"        {cat_id}, {int(channel['item_id'])}, {cstr(channel['name'])},"
            )
            lines.append(
                f"        {cstr(channel.get('language'))}, {cstr(channel.get('region'))},"
            )
            lines.append(
                f"        {cstr(channel.get('title_id'))}, {int(channel.get('latest_version') or 0)}, {cstr(channel.get('patch_folder'))},"
            )
            lines.append(
                f"        {sym + '_patches' if patches else 'NULL'}, {len(patches)},"
            )
            lines.append(
                f"        {sym + '_apps' if apps else 'NULL'}, {len(apps)}, {add_cat}, {add_item}, {flag_expr}"
            )
            lines.append("    },")
    lines.append("};")
    lines.append(
        "const size_t g_channel_count = sizeof(g_channels) / sizeof(g_channels[0]);"
    )
    lines.append("")
    lines.append("const CategoryDef g_categories[] = {")
    lines.extend(category_rows)
    lines.append("};")
    lines.append(
        "const size_t g_category_count = sizeof(g_categories) / sizeof(g_categories[0]);"
    )
    lines.append("")
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUTPUT} ({len(channel_names)} channels, {len(data)} categories)")


if __name__ == "__main__":
    main()
