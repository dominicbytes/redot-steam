#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADDON = ROOT / "bin/addons/godotsteam"
EXPECTED = {
    "win64/libgodotsteam.windows.template_debug.x86_64.dll": ("pe", 0x8664),
    "win64/libgodotsteam.windows.template_release.x86_64.dll": ("pe", 0x8664),
    "linux64/libgodotsteam.linux.template_debug.x86_64.so": ("elf", 62),
    "linux64/libgodotsteam.linux.template_release.x86_64.so": ("elf", 62),
    "linuxarm64/libgodotsteam.linux.template_debug.arm64.so": ("elf", 183),
    "linuxarm64/libgodotsteam.linux.template_release.arm64.so": ("elf", 183),
}
TARGETS = {"windows-x86_64": "win64/", "linux-x86_64": "linux64/", "linux-arm64": "linuxarm64/"}


def expected_for(targets: list[str] | None = None) -> dict:
    prefixes = tuple(TARGETS[target] for target in (targets or TARGETS))
    return {name: spec for name, spec in EXPECTED.items() if name.startswith(prefixes)}


def span(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise AssertionError("truncated image structure")
    return data[offset:offset + size]


def unpack(data: bytes, fmt: str, offset: int) -> tuple:
    return struct.unpack(fmt, span(data, offset, struct.calcsize(fmt)))


def cstring(data: bytes, offset: int) -> bytes:
    if not 0 <= offset < len(data):
        raise AssertionError("string offset outside table")
    end = data.find(b"\0", offset)
    if end < 0:
        raise AssertionError("unterminated image string")
    return data[offset:end]


def verify_pe(data: bytes) -> int:
    if span(data, 0, 2) != b"MZ":
        raise AssertionError("expected PE/DOS signature")
    pe, = unpack(data, "<I", 0x3C)
    if pe < 64 or span(data, pe, 4) != b"PE\0\0":
        raise AssertionError("expected PE signature")
    machine, count, _, _, _, opt_size, flags = unpack(data, "<HHIIIHH", pe + 4)
    opt = pe + 24
    if not count or opt_size < 240 or not flags & 0x2000 or unpack(data, "<H", opt)[0] != 0x20B:
        raise AssertionError("expected PE32+ DLL with sections")
    span(data, opt, opt_size)
    if unpack(data, "<I", opt + 108)[0] < 2:
        raise AssertionError("missing PE data directories")
    sections = []
    for index in range(count):
        row = opt + opt_size + index * 40
        _, vsize, rva, raw_size, raw, _, _, _, _, characteristics = unpack(data, "<8sIIIIIIHHI", row)
        span(data, raw, raw_size)
        sections.append((rva, raw_size, raw, characteristics))

    def mapped(rva: int, size: int = 1, executable: bool = False) -> int:
        for start, length, raw, characteristics in sections:
            if start <= rva and rva + size <= start + length and (not executable or characteristics & 0x20000000):
                return raw + rva - start
        raise AssertionError("PE address outside file-backed sections")

    def name_at(rva: int) -> bytes:
        for start, length, raw, _ in sections:
            if start <= rva < start + length:
                return cstring(span(data, raw, length), rva - start)
        raise AssertionError("PE string outside sections")

    export_rva, export_size, import_rva, import_size = unpack(data, "<IIII", opt + 112)
    if export_size < 40 or import_size < 20:
        raise AssertionError("missing PE export/import directory")
    export = mapped(export_rva, export_size)
    _, _, _, _, _, ordinal_base, function_count, name_count, functions, names, ordinals = unpack(data, "<IIHHIIIIIII", export)
    function_table = mapped(functions, function_count * 4)
    name_table = mapped(names, name_count * 4)
    ordinal_table = mapped(ordinals, name_count * 2)
    found = False
    for index in range(name_count):
        name_rva, = unpack(data, "<I", name_table + index * 4)
        if name_at(name_rva) == b"godotsteam_init":
            ordinal, = unpack(data, "<H", ordinal_table + index * 2)
            if ordinal >= function_count:
                raise AssertionError("invalid PE export ordinal")
            address, = unpack(data, "<I", function_table + ordinal * 4)
            if export_rva <= address < export_rva + export_size:
                raise AssertionError("entry point is a forwarded export")
            mapped(address, executable=True)
            found = True
    if not found:
        raise AssertionError("missing godotsteam_init export")
    imports = mapped(import_rva, import_size)
    dependencies = []
    for offset in range(0, import_size - 19, 20):
        descriptor = unpack(data, "<IIIII", imports + offset)
        if not any(descriptor):
            break
        dependencies.append(name_at(descriptor[3]).lower())
        mapped(descriptor[4], 8)
    else:
        raise AssertionError("unterminated PE imports")
    if b"steam_api64.dll" not in dependencies:
        raise AssertionError("missing Steam PE import")
    return machine


def verify_elf(data: bytes) -> int:
    if span(data, 0, 7) != b"\x7fELF\x02\x01\x01":
        raise AssertionError("expected 64-bit little-endian ELF")
    kind, machine, version, _, phoff, shoff, _, ehsize, phsize, phnum, shsize, shnum, _ = unpack(data, "<HHIQQQIHHHHHH", 16)
    if kind != 3 or version != 1 or ehsize != 64 or phsize != 56 or not phnum or shsize != 64 or not shnum:
        raise AssertionError("expected shared ELF with program and section tables")
    loads, dynamics = [], []
    for index in range(phnum):
        ptype, flags, offset, address, _, filesz, memsz, _ = unpack(data, "<IIQQQQQQ", phoff + index * phsize)
        span(data, offset, filesz)
        if filesz > memsz:
            raise AssertionError("invalid ELF segment size")
        if ptype == 1:
            loads.append((address, filesz, offset, flags))
        elif ptype == 2:
            dynamics.append(span(data, offset, filesz))

    def mapped(address: int, size: int, executable: bool = False) -> bytes:
        for start, length, offset, flags in loads:
            if start <= address and address + size <= start + length and (not executable or flags & 1):
                return span(data, offset + address - start, size)
        raise AssertionError("ELF address outside file-backed load segments")

    if len(dynamics) != 1:
        raise AssertionError("expected one ELF dynamic segment")
    tags: dict[int, list[int]] = {}
    for offset in range(0, len(dynamics[0]), 16):
        tag, value = unpack(dynamics[0], "<qQ", offset)
        if tag == 0:
            break
        tags.setdefault(tag, []).append(value)
    else:
        raise AssertionError("unterminated ELF dynamic segment")
    if not all(tag in tags for tag in (1, 5, 6, 10, 11)):
        raise AssertionError("missing ELF dependency/string tables")
    if tags[11] != [24]:
        raise AssertionError("invalid ELF dynamic symbol entry size")
    strings = mapped(tags[5][0], tags[10][0])
    if b"libsteam_api.so" not in [cstring(strings, offset) for offset in tags[1]]:
        raise AssertionError("missing Steam ELF dependency")
    runpaths = [cstring(strings, offset).split(b":") for tag in (15, 29) for offset in tags.get(tag, [])]
    if not any(origin in parts for parts in runpaths for origin in (b"$ORIGIN", b"${ORIGIN}")):
        raise AssertionError("missing origin ELF runpath")
    sections = [unpack(data, "<IIQQQQIIQQ", shoff + index * shsize) for index in range(shnum)]
    for section in sections:
        if section[1] != 11 or section[3] != tags[6][0]:
            continue
        _, _, _, _, offset, size, link, _, _, entsize = section
        if entsize != 24 or size % 24 or link >= shnum:
            raise AssertionError("invalid ELF dynamic symbols")
        symbols = span(data, offset, size)
        if mapped(section[3], size) != symbols:
            raise AssertionError("ELF dynamic symbols do not match load mapping")
        strtab = sections[link]
        if strtab[1] != 3 or strtab[3] != tags[5][0] or strtab[5] != tags[10][0]:
            raise AssertionError("invalid ELF symbol string table")
        names = span(data, strtab[4], strtab[5])
        for row in range(0, size, 24):
            name, info, visibility, index, address, symbol_size = unpack(symbols, "<IBBHQQ", row)
            if cstring(names, name) == b"godotsteam_init" and info >> 4 in (1, 2) and info & 15 == 2 and visibility & 3 in (0, 3) and 0 < index < shnum:
                mapped(address, max(symbol_size, 1), executable=True)
                return machine
    raise AssertionError("missing defined godotsteam_init dynamic export")


def verify_binary(data: bytes, kind: str, expected_machine: int) -> None:
    actual = verify_pe(data) if kind == "pe" else verify_elf(data)
    if actual != expected_machine:
        raise AssertionError("wrong binary architecture")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--addon", type=Path, default=ADDON)
    parser.add_argument("--targets", nargs="+", choices=TARGETS)
    args = parser.parse_args()
    for relative, (kind, expected_machine) in expected_for(args.targets).items():
        path = args.addon / relative
        if not path.is_file():
            raise AssertionError(f"missing required binary: {relative}")
        data = path.read_bytes()
        verify_binary(data, kind, expected_machine)
        print(f"{relative} {hashlib.sha256(data).hexdigest().upper()}")
    print("REDOTSTEAM_BINARY_CHECKS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, struct.error) as error:
        print(f"REDOTSTEAM_BINARY_CHECKS=FAIL: {type(error).__name__}")
        raise SystemExit(1)
