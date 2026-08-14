#!/usr/bin/env python3
"""Validate the bundled WiiLink GUI catalog against the public HTTP endpoints."""
from __future__ import annotations

import concurrent.futures
import json
import pathlib
import struct
import sys
import requests

ROOT = pathlib.Path(__file__).resolve().parents[1]
CATALOG = ROOT / "data" / "patches.json"
NUS = "http://nus.cdn.shop.wii.com/ccs/download"
PATCHER = "http://patcher.wiilink24.com"
OSC = "http://hbb1.oscwii.org"


def check_url(url: str) -> tuple[str, int, int]:
    response = requests.head(url, timeout=20)
    return url, response.status_code, int(response.headers.get("content-length", 0))


def main() -> int:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    urls: set[str] = {
        f"{PATCHER}/connectiontest.txt",
        f"{NUS}/0000000100000002/tmd.513",
        f"{NUS}/0000000100000002/cetk",
    }
    apps = {"yawmME", "sntp", "Mail-Patcher"}
    tmd_urls: list[str] = []
    for category in catalog:
        for channel in category["channels"]:
            apps.update("AnyGlobe_Changer" if x == "agc" else x for x in (channel.get("additional_apps") or []))
            title_id = channel.get("title_id")
            if not title_id:
                continue
            folder = channel.get("patch_folder") or ""
            low_folder = folder.lower()
            tmd_url = f"{NUS}/{title_id}/tmd.{channel['latest_version']}"
            urls.add(tmd_url)
            tmd_urls.append(tmd_url)
            files = channel.get("additional_files") or []
            urls.add(f"{PATCHER}/{low_folder}/{title_id}.tik" if "ticket" in files else f"{NUS}/{title_id}/cetk")
            if "tmd" in files:
                urls.add(f"{PATCHER}/{low_folder}/{folder}.tmd")
            for patch in channel.get("patches") or []:
                urls.add(f"{PATCHER}/bsdiff/{low_folder}/{patch['patch_name']}.bsdiff")
    for app in apps:
        urls.add(f"{OSC}/unzipped_apps/{app}/apps/{app}/boot.dol")
        urls.add(f"{OSC}/unzipped_apps/{app}/apps/{app}/meta.xml")
        urls.add(f"{OSC}/api/v3/contents/{app}/icon.png")

    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
        for url, status, length in pool.map(check_url, sorted(urls)):
            if status != 200 or (length <= 0 and not url.endswith("connectiontest.txt")):
                failures.append(f"{status} len={length}: {url}")

    # Parse one copy of each TMD to catch an HTML/error response masquerading as 200.
    for url in sorted(set(tmd_urls)):
        data = requests.get(url, timeout=20).content
        if len(data) < 0x1E4:
            failures.append(f"Truncated TMD: {url}")
            continue
        count = struct.unpack(">H", data[0x1DE:0x1E0])[0]
        if count == 0 or 0x1E4 + count * 36 > len(data):
            failures.append(f"Invalid TMD records: {url}")

    if failures:
        print("Catalog validation FAILED:")
        print("\n".join(failures))
        return 1
    print(f"Catalog validation PASS: {len(urls)} unique assets, {len(set(tmd_urls))} TMDs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
