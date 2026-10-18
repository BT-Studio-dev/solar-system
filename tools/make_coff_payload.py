#!/usr/bin/env python3
"""Embed a binary blob into a COFF object file for the Windows linker.

The payload becomes a read-only input section with three external symbols:

    sss_payload        address of the first payload byte
    sss_payload_size   address of a 32-bit word holding the byte count
    sss_payload_crc    address of a 32-bit word holding the CRC-32

Declare them in C as objects ("extern const unsigned int sss_payload_size;")
so a plain load reads the stored word instead of the address.

Usage:
    tools/make_coff_payload.py --input build/payload.bin --output build/obj/payload.o
"""
from __future__ import annotations

import argparse
import struct
import zlib

IMAGE_FILE_MACHINE_I386 = 0x014C
IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040
IMAGE_SCN_MEM_READ = 0x40000000
IMAGE_SCN_ALIGN_4BYTES = 0x00300000
IMAGE_SYM_CLASS_EXTERNAL = 2


def build_object(payload: bytes, section: str = ".rdata") -> bytes:
    blob = payload + struct.pack("<II", len(payload), zlib.crc32(payload) & 0xFFFFFFFF)
    data_off = 20 + 40  # one section header, already 4-byte aligned
    symptr = (data_off + len(blob) + 3) & ~3

    names = [
        ("_sss_payload", 0),
        ("_sss_payload_size", len(payload)),
        ("_sss_payload_crc", len(payload) + 4),
    ]
    # COFF string table: 4-byte size (including itself) then NUL-terminated names.
    strtab = bytearray(b"\0\0\0\0")
    entries = []
    for name, value in names:
        raw = name.encode("ascii")
        if len(raw) <= 8:
            entries.append((raw.ljust(8, b"\0"), value))
        else:
            offset = len(strtab)
            strtab += raw + b"\0"
            entries.append((b"\0\0\0\0" + struct.pack("<I", offset), value))
    struct.pack_into("<I", strtab, 0, len(strtab))

    out = bytearray()
    out += struct.pack("<HHIIIHH", IMAGE_FILE_MACHINE_I386, 1, 0, symptr, len(entries), 0, 0)
    out += section.encode("ascii").ljust(8, b"\0")
    out += struct.pack(
        "<IIIIIIHHI",
        0,  # VirtualSize
        0,  # VirtualAddress
        len(blob),
        data_off,
        0,  # PointerToRelocations
        0,  # PointerToLinenumbers
        0,  # NumberOfRelocations
        0,  # NumberOfLinenumbers
        IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_ALIGN_4BYTES,
    )
    assert len(out) == data_off
    out += blob
    out += b"\0" * (symptr - len(out))
    for name, value in entries:
        out += name
        out += struct.pack("<IhHBB", value, 1, 0, IMAGE_SYM_CLASS_EXTERNAL, 0)
    out += strtab
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--section", default=".rdata")
    args = ap.parse_args()
    with open(args.input, "rb") as f:
        payload = f.read()
    with open(args.output, "wb") as f:
        f.write(build_object(payload, args.section))
    print(f"embedded {len(payload)} payload bytes in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
