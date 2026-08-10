#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path, PurePosixPath

EXPECTED_BINARIES = {
    "addons/godotsteam/win64/libgodotsteam.windows.template_debug.x86_64.dll",
    "addons/godotsteam/win64/libgodotsteam.windows.template_release.x86_64.dll",
    "addons/godotsteam/linux64/libgodotsteam.linux.template_debug.x86_64.so",
    "addons/godotsteam/linux64/libgodotsteam.linux.template_release.x86_64.so",
    "addons/godotsteam/linuxarm64/libgodotsteam.linux.template_debug.arm64.so",
    "addons/godotsteam/linuxarm64/libgodotsteam.linux.template_release.arm64.so",
}
REQUIRED = {
    "addons/godotsteam/README.md",
    "addons/godotsteam/THIRD_PARTY_NOTICES.md",
    "addons/godotsteam/tools/provision_steam_runtime.ps1",
    "addons/godotsteam/godotsteam.gdextension",
}
FORBIDDEN_SUFFIXES = {".a", ".exp", ".h", ".hpp", ".lib"}
FORBIDDEN_NAMES = {
    "steam_api.dll",
    "steam_api64.dll",
    "libsteam_api.so",
    "libsteam_api.dylib",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.artifact) as package:
        files = {info.filename.replace("\\", "/") for info in package.infolist() if not info.is_dir()}
    missing = (EXPECTED_BINARIES | REQUIRED) - files
    if missing:
        raise AssertionError(f"package is missing required files: {sorted(missing)}")
    for name in sorted(files):
        path = PurePosixPath(name)
        if path.name.lower() in FORBIDDEN_NAMES or path.suffix.lower() in FORBIDDEN_SUFFIXES:
            raise AssertionError(f"restricted/build-only file entered package: {name}")
        if "godotsteam/sdk" in name.lower() or "/public/steam/" in name.lower():
            raise AssertionError(f"SDK path entered package: {name}")
    native = {name for name in files if PurePosixPath(name).suffix.lower() in {".dll", ".so", ".dylib"}}
    if native != EXPECTED_BINARIES:
        raise AssertionError(f"unexpected native file set: {sorted(native)}")
    print("REDOTSTEAM_PACKAGE_CHECKS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"REDOTSTEAM_PACKAGE_CHECKS=FAIL: {error}")
        raise SystemExit(1)
