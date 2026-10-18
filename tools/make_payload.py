#!/usr/bin/env python3
"""Pack a directory tree into the installer payload blob.

The blob is linked into the setup executable as a read-only section (see
tools/make_coff_payload.py) and unpacked by src/installer/payload.c.

Layout (little endian):

    char     magic[8]     "SSSPAY1\\0"
    uint32   format       1
    uint32   count        number of entries
    uint32   total        sum of all entry payload sizes
    repeat count times:
        uint16 name_len   bytes of UTF-8 name (no terminator)
        uint16 flags      reserved, 0
        uint32 size       bytes of file data
        uint32 crc        CRC-32 of the file data
        char   name[]     "sub/dir/file.ext", '/' separators
        char   data[size]

A "setup.ini" entry is always written first; the installer reads the product
name, version and executable name from it.

Usage:
    tools/make_payload.py --dir build/payload --out build/payload.bin \
        --version 1.0.0 --name "Solar System Sandbox"
"""
from __future__ import annotations

import argparse
import os
import struct
import zlib

MAGIC = b"SSSPAY1\0"
FORMAT_VERSION = 1


def gather(root: str) -> list[tuple[str, bytes]]:
    files: list[tuple[str, bytes]] = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for filename in sorted(filenames):
            full = os.path.join(dirpath, filename)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            with open(full, "rb") as f:
                files.append((rel, f.read()))
    files.sort(key=lambda item: item[0])
    return files


def build(entries: list[tuple[str, bytes]]) -> bytes:
    total = sum(len(data) for _, data in entries)
    out = bytearray(MAGIC)
    out += struct.pack("<III", FORMAT_VERSION, len(entries), total)
    for name, data in entries:
        raw = name.encode("utf-8")
        if len(raw) > 0xFFFF:
            raise ValueError(f"payload name too long: {name}")
        out += struct.pack("<HHII", len(raw), 0, len(data), zlib.crc32(data) & 0xFFFFFFFF)
        out += raw
        out += data
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dir", required=True, help="directory to pack")
    ap.add_argument("--out", required=True, help="payload blob to write")
    ap.add_argument("--version", default="1.0.0")
    ap.add_argument("--name", default="Solar System Sandbox")
    ap.add_argument("--publisher", default="Solar Sandbox Project")
    ap.add_argument("--exe", default="SolarSandbox.exe")
    args = ap.parse_args()

    entries = gather(args.dir)
    if not entries:
        raise SystemExit(f"no files found in {args.dir}")
    meta = (
        f"Name={args.name}\n"
        f"Version={args.version}\n"
        f"Publisher={args.publisher}\n"
        f"Executable={args.exe}\n"
    )
    entries.insert(0, ("setup.ini", meta.encode("utf-8")))

    blob = build(entries)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(blob)

    print(f"payload: {len(entries)} files, {len(blob)} bytes -> {args.out}")
    for name, data in entries:
        print(f"  {len(data):>8}  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
