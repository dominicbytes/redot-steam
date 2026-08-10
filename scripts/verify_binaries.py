#!/usr/bin/env python3
from __future__ import annotations

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


def machine(data: bytes, kind: str) -> int:
    if kind == "elf":
        if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
            raise AssertionError("expected 64-bit little-endian ELF")
        return struct.unpack_from("<H", data, 18)[0]
    if data[:2] != b"MZ":
        raise AssertionError("expected PE/DOS signature")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise AssertionError("expected PE signature")
    return struct.unpack_from("<H", data, pe_offset + 4)[0]


def main() -> int:
    for relative, (kind, expected_machine) in EXPECTED.items():
        path = ADDON / relative
        if not path.is_file():
            raise AssertionError(f"missing required binary: {relative}")
        data = path.read_bytes()
        actual = machine(data, kind)
        if actual != expected_machine:
            raise AssertionError(f"wrong machine for {relative}: {actual}")
        if kind == "elf":
            if b"libsteam_api.so" not in data or b"$ORIGIN" not in data:
                raise AssertionError(f"missing Steam dependency or origin runpath: {relative}")
        print(f"{relative} {hashlib.sha256(data).hexdigest().upper()}")
    print("REDOTSTEAM_BINARY_CHECKS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"REDOTSTEAM_BINARY_CHECKS=FAIL: {error}")
        raise SystemExit(1)
