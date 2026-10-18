#!/usr/bin/env python3
"""Inject Win32 resources into a PE image after linking.

The cross toolchain used to build this project on Linux has no rc.exe, so the
application icon, the version resource and the application manifest (DPI
awareness, requested execution level, Windows 7 compatibility) are compiled
here into a fresh .rsrc section and merged into the linked image.

Also stamps the subsystem version so the loader never advertises a newer OS
requirement than Windows 7.

Usage:
  python tools/pe_patch.py --input in.exe --output out.exe --icon icon.ico \
      --manifest app.manifest --version 1.0.0 --product "Solar System Sandbox" ...
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field

RT_ICON = 3
RT_GROUP_ICON = 14
RT_VERSION = 16
RT_MANIFEST = 24

LANG_EN_US = 0x0409
CODEPAGE_UNICODE = 1200


# --------------------------------------------------------------------- input

@dataclass
class IconImage:
    width: int
    height: int
    colors: int
    planes: int
    bpp: int
    data: bytes


def parse_ico(path: str) -> list[IconImage]:
    with open(path, "rb") as f:
        blob = f.read()
    reserved, kind, count = struct.unpack_from("<HHH", blob, 0)
    if reserved != 0 or kind != 1:
        raise SystemExit(f"{path}: not an .ico file")
    images: list[IconImage] = []
    for i in range(count):
        off = 6 + i * 16
        w, h, colors, _res, planes, bpp, size, data_off = struct.unpack_from("<BBBBHHII", blob, off)
        images.append(IconImage(w or 256, h or 256, colors, planes, bpp,
                                blob[data_off:data_off + size]))
    return images


# ------------------------------------------------------------------ builder

class Dir:
    """A resource directory: named/id entries with either subdirectories or data."""

    def __init__(self) -> None:
        self.entries: dict[int, "Dir | Leaf"] = {}

    def sub(self, key: int) -> "Dir":
        node = self.entries.get(key)
        if not isinstance(node, Dir):
            node = Dir()
            self.entries[key] = node
        return node

    def leaf(self, key: int, data: bytes, codepage: int = CODEPAGE_UNICODE,
             lang: int = LANG_EN_US) -> None:
        """Store data at the standard Type -> Name -> Language level."""
        self.sub(key).leaf_raw(lang, Leaf(data, codepage))

    def leaf_raw(self, key: int, leaf: "Leaf") -> None:
        self.entries[key] = leaf


@dataclass
class Leaf:
    data: bytes
    codepage: int = CODEPAGE_UNICODE


def align4(n: int) -> int:
    return (n + 3) & ~3


def build_rsrc(root: Dir, base_rva: int = 0) -> bytes:
    """Serialize a directory tree into a .rsrc section image.

    Directory offsets are relative to the section, but each data entry stores
    an absolute RVA, hence base_rva.
    """
    # Assign offsets: all directories first, then the string table (unused),
    # then data entries.
    dir_offsets: dict[int, int] = {}
    leaves: list[tuple[int, Leaf]] = []  # (offset of data entry, leaf)

    def collect(node: Dir) -> None:
        for key, child in node.entries.items():
            if isinstance(child, Dir):
                collect(child)
            else:
                leaves.append((id(child), child))

    collect(root)

    # Layout: recursive directories, each 16 + 8*n bytes
    data_entries_size = 16 * len(leaves)
    dir_cursor = 0

    # First pass: positions of all directories, depth first.
    dir_offsets.clear()

    def layout_dirs(node: Dir, offset: int) -> int:
        dir_offsets[id(node)] = offset
        offset += 16 + 8 * len(node.entries)
        for key in sorted(node.entries):
            child = node.entries[key]
            if isinstance(child, Dir):
                offset = layout_dirs(child, offset)
        return offset

    dirs_end = layout_dirs(root, 0)
    dirs_end = align4(dirs_end)

    # Second pass: data entry positions and payload positions.
    payload = bytearray()
    payload_cursor = dirs_end + data_entries_size
    data_offsets: dict[int, tuple[int, int]] = {}  # leaf id -> (entry offset, data offset)
    entry_offset = dirs_end
    for leaf_id, leaf in leaves:
        d = align4(payload_cursor)
        payload_cursor = d + len(leaf.data)
        data_offsets[leaf_id] = (entry_offset, d)
        entry_offset += 16

    total = align4(payload_cursor)
    out = bytearray(total)

    def write_dir(node: Dir) -> None:
        base = dir_offsets[id(node)]
        keys = sorted(node.entries)
        named = 0
        struct.pack_into("<IIHHHH", out, base, 0, 0, 0, 0, named, len(keys))
        for i, key in enumerate(keys):
            child = node.entries[key]
            ent = base + 16 + i * 8
            if isinstance(child, Dir):
                struct.pack_into("<II", out, ent, key, 0x80000000 | dir_offsets[id(child)])
            else:
                e_off, d_off = data_offsets[id(child)]
                struct.pack_into("<II", out, ent, key, e_off)
        for key in keys:
            child = node.entries[key]
            if isinstance(child, Dir):
                write_dir(child)

    write_dir(root)

    for leaf_id, leaf in leaves:
        e_off, d_off = data_offsets[leaf_id]
        struct.pack_into("<IIII", out, e_off, base_rva + d_off, len(leaf.data), leaf.codepage, 0)
        out[d_off:d_off + len(leaf.data)] = leaf.data
    return bytes(out)


def version_resource(info: dict) -> bytes:
    """A VS_VERSIONINFO structure with StringFileInfo + VarFileInfo."""
    def pad(b: bytes) -> bytes:
        while len(b) % 4:
            b += b"\x00"
        return b

    def wstr(s: str) -> bytes:
        return s.encode("utf-16-le") + b"\x00\x00"

    # Written explicitly for clarity: wLength, wValueLength, wType, szKey, padding, Value
    def node(key: str, value: bytes, wtype: int, value_len: int | None = None,
             text_value: bool = True) -> bytes:
        if value_len is None:
            value_len = len(value) // 2 if text_value else len(value)
        key_b = wstr(key)
        head = struct.pack("<HHH", 0, value_len, wtype) + key_b
        head = pad(head)
        raw = head + value
        raw = pad(raw)
        raw = struct.pack("<H", len(raw)) + raw[2:]
        return raw

    def string_node(k: str, v: str) -> bytes:
        val = wstr(v)
        key_b = wstr(k)
        head = struct.pack("<HHH", 0, len(v) + 1, 1) + key_b
        head = pad(head)
        raw = head + val
        raw = pad(raw)
        return struct.pack("<H", len(raw)) + raw[2:]

    ver = info["version"]
    parts = [int(x) for x in ver.split(".")] + [0, 0, 0, 0]
    ms, ls = (parts[0] << 16) | parts[1], (parts[2] << 16) | parts[3]
    fixed = struct.pack("<IIIIIIIIIIIIII", 0xFEEF04BD, 0x00010000, ms, ls, ms, ls, 0x3F, 0,
                        0x00040004, 0x00000001, 0, 0, 0x00000000, 0)
    strings = [
        ("CompanyName", info["company"]),
        ("FileDescription", info["description"]),
        ("FileVersion", ver),
        ("InternalName", info.get("internal", "SolarSandbox")),
        ("LegalCopyright", info.get("copyright", "")),
        ("OriginalFilename", info.get("original", "SolarSandbox.exe")),
        ("ProductName", info["product"]),
        ("ProductVersion", ver),
    ]
    string_block = b"".join(string_node(k, v) for k, v in strings)
    string_table = node("040904B0", string_block, 1, value_len=0, text_value=False)
    string_file_info = node("StringFileInfo", string_table, 1, value_len=0, text_value=False)
    translation = struct.pack("<HH", LANG_EN_US, CODEPAGE_UNICODE)
    var = node("Translation", translation, 0, value_len=len(translation), text_value=False)
    var_file_info = node("VarFileInfo", var, 1, value_len=0, text_value=False)
    fixed_node = node("VS_VERSION_INFO", fixed, 0, value_len=len(fixed), text_value=False)
    # splice children into the fixed node's value position
    inner = fixed_node
    key_len = 2 * (len("VS_VERSION_INFO") + 1)
    head_len = 6 + key_len
    head_len = (head_len + 3) & ~3
    prefix = inner[:head_len] + fixed
    prefix = prefix + b"\x00" * ((4 - len(prefix) % 4) % 4) if len(prefix) % 4 else prefix
    body = prefix + string_file_info + var_file_info
    body = struct.pack("<H", len(body)) + body[2:]
    return body


def group_icon_resource(images: list[IconImage]) -> bytes:
    icon_id = 1
    entries = []
    for img in images:
        entries.append(struct.pack("<BBBBHHIH", img.width % 256, img.height % 256, img.colors, 0,
                                   img.planes, img.bpp, len(img.data), icon_id))
        icon_id += 1
    header = struct.pack("<HHH", 0, 1, len(images))
    return header + b"".join(entries)


# --------------------------------------------------------------- PE surgery

def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def next_section_rva(blob: bytes) -> int:
    pe_off = struct.unpack_from("<I", blob, 0x3C)[0]
    coff = pe_off + 4
    nsections = struct.unpack_from("<H", blob, coff + 2)[0]
    opt_size = struct.unpack_from("<H", blob, coff + 16)[0]
    opt = coff + 20
    sect_align = struct.unpack_from("<I", blob, opt + 32)[0] or 4096
    section_table = opt + opt_size
    end = 0
    for i in range(nsections):
        ent = section_table + i * 40
        vsize, va, raw_size = struct.unpack_from("<III", blob, ent + 8)
        end = max(end, va + max(vsize, raw_size))
    return align(end, sect_align)


def patch_pe(blob: bytes, rsrc: bytes, subsystem_version: tuple[int, int]) -> bytes:
    pe_off = struct.unpack_from("<I", blob, 0x3C)[0]
    if blob[pe_off:pe_off + 4] != b"PE\x00\x00":
        raise SystemExit("not a PE image")
    coff = pe_off + 4
    nsections = struct.unpack_from("<H", blob, coff + 2)[0]
    opt_size = struct.unpack_from("<H", blob, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", blob, opt)[0]
    is64 = magic == 0x20B
    # Data directory 2 = resources
    dd_off = opt + (112 if is64 else 96)
    file_align = struct.unpack_from("<I", blob, opt + 36)[0] or 512
    sect_align = struct.unpack_from("<I", blob, opt + 32)[0] or 4096
    size_of_image = struct.unpack_from("<I", blob, opt + 56)[0]
    headers_size = struct.unpack_from("<I", blob, opt + 60)[0]
    check_sum_off = opt + 64
    subsystem_ver_off = opt + 48

    section_table = opt + opt_size
    last_end = 0
    last_va_end = 0
    for i in range(nsections):
        ent = section_table + i * 40
        vsize, va, raw_size, raw_ptr = struct.unpack_from("<IIII", blob, ent + 8)
        last_end = max(last_end, raw_ptr + raw_size)
        last_va_end = max(last_va_end, va + max(vsize, raw_size))
        chars = struct.unpack_from("<I", blob, ent + 36)[0]

    new_va = align(last_va_end, sect_align)
    raw_ptr = align(last_end, file_align)
    raw_size = align(len(rsrc), file_align)

    # New section header
    name = b".rsrc\x00\x00\x00"
    header = name + struct.pack("<IIIIIIHHI", len(rsrc), new_va, raw_size, raw_ptr, 0, 0, 0, 0,
                                0x40000040)

    out = bytearray(blob[:headers_size])
    if len(out) < headers_size:
        out.extend(b"\x00" * (headers_size - len(out)))
    struct.pack_into("<H", out, coff + 2, nsections + 1)
    # account for the new section's raw data in SizeOfInitializedData
    init_data = struct.unpack_from("<I", out, opt + 8)[0]
    struct.pack_into("<I", out, opt + 8, init_data + raw_size)
    # Write section header
    hdr_off = section_table + nsections * 40
    if hdr_off + 40 > headers_size:
        raise SystemExit("no room for a new section header (rebuild with a larger file alignment)")
    out[hdr_off:hdr_off + 40] = header
    # Sizes
    struct.pack_into("<I", out, opt + 56, align(new_va + len(rsrc), sect_align))
    struct.pack_into("<I", out, opt + 60, headers_size)
    # Resource data directory
    struct.pack_into("<II", out, dd_off + 16, new_va, len(rsrc))
    # Subsystem version (Windows 7 = 6.1)
    struct.pack_into("<HH", out, subsystem_ver_off, subsystem_version[0], subsystem_version[1])
    # Minimum OS version
    struct.pack_into("<HH", out, opt + 40, 6, 1)
    # Clear checksum so it can be recomputed (0 means "not checked" for exe files)
    struct.pack_into("<I", out, check_sum_off, 0)

    body = bytearray(blob[headers_size:])
    if len(body) < raw_ptr - headers_size:
        body.extend(b"\x00" * (raw_ptr - headers_size - len(body)))
    body = body[:raw_ptr - headers_size]
    body.extend(rsrc)
    body.extend(b"\x00" * (raw_size - len(rsrc)))
    out.extend(body)
    return bytes(out)


def pe_checksum(blob: bytes, check_sum_off: int) -> int:
    total = 0
    data = bytearray(blob)
    data[check_sum_off:check_sum_off + 4] = b"\x00\x00\x00\x00"
    if len(data) % 2:
        data.append(0)
    for i in range(0, len(data), 2):
        total += data[i] | (data[i + 1] << 8)
        total = (total & 0xFFFF) + (total >> 16)
    total = (total & 0xFFFF) + (total >> 16)
    return (total + len(blob)) & 0xFFFFFFFF


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--icon")
    ap.add_argument("--icon-id", type=int, default=1,
                    help="resource ID of the icon group (RT_GROUP_ICON name)")
    ap.add_argument("--manifest")
    ap.add_argument("--version", default="1.0.0")
    ap.add_argument("--product", default="Solar System Sandbox")
    ap.add_argument("--company", default="Solar Sandbox Project")
    ap.add_argument("--description", default="Solar System Sandbox")
    ap.add_argument("--internal", default="SolarSandbox")
    ap.add_argument("--original", default="SolarSandbox.exe")
    ap.add_argument("--copyright", default="Copyright (c) 2026 Solar Sandbox Project")
    ap.add_argument("--subsystem-version", default="6.1")
    args = ap.parse_args()

    with open(args.input, "rb") as f:
        blob = f.read()

    root = Dir()
    if args.icon:
        images = parse_ico(args.icon)
        icon_dir = root.sub(RT_ICON)
        for i, img in enumerate(images, start=1):
            icon_dir.leaf(i, img.data)
        root.sub(RT_GROUP_ICON).leaf(args.icon_id, group_icon_resource(images))
    if args.manifest:
        with open(args.manifest, "rb") as f:
            manifest = f.read()
        root.sub(RT_MANIFEST).leaf(1, manifest, codepage=65001)
    root.sub(RT_VERSION).leaf(1, version_resource({
        "version": args.version,
        "product": args.product,
        "company": args.company,
        "description": args.description,
        "internal": args.internal,
        "original": args.original,
        "copyright": args.copyright,
    }))

    base_rva = next_section_rva(blob)
    rsrc = build_rsrc(root, base_rva)
    major, minor = (int(x) for x in args.subsystem_version.split("."))
    patched = patch_pe(blob, rsrc, (major, minor))
    pe_off = struct.unpack_from("<I", patched, 0x3C)[0]
    opt = pe_off + 24
    sum_off = opt + 64
    checksum = pe_checksum(patched, sum_off)
    patched = bytearray(patched)
    struct.pack_into("<I", patched, sum_off, checksum)

    with open(args.output, "wb") as f:
        f.write(patched)
    print(f"patched {args.output}: +{len(rsrc)} bytes resources, checksum 0x{checksum:08X}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
