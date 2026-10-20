#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
import struct

from verify_binaries import EXPECTED, TARGETS, expected_for, verify_binary
from pathlib import Path, PurePosixPath

EXPECTED_BINARIES = {"addons/godotsteam/" + name for name in EXPECTED}
REQUIRED = {
    "addons/godotsteam/LICENSE.txt",
    "addons/godotsteam/plugin.cfg",
    "addons/godotsteam/godotsteam_plugin.gd",
    "addons/godotsteam/editor/steamworks_panel.gd",
    "addons/godotsteam/editor/steamworks_panel.tscn",
    "addons/godotsteam/editor/updates/updates.gd",
    "addons/godotsteam/editor/ui/steam.svg",
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


def verify_package(artifact: Path, targets: list[str] | None = None) -> None:
    expected = {"addons/godotsteam/" + name: spec for name, spec in expected_for(targets).items()}
    with zipfile.ZipFile(artifact) as package:
        entries = [info for info in package.infolist() if not info.is_dir()]
        files = {info.filename for info in entries}
        if len(entries) != len(files):
            raise AssertionError("duplicate archive members")
        for info in entries:
            path = PurePosixPath(info.filename)
            if "\\" in info.filename or path.is_absolute() or ".." in path.parts or ":" in info.filename:
                raise AssertionError("unsafe archive member")
            if not info.file_size or not package.read(info):
                raise AssertionError("empty package member")
        for name, (kind, machine) in expected.items():
            if name in files:
                verify_binary(package.read(name), kind, machine)
    missing = (set(expected) | REQUIRED) - files
    if missing:
        raise AssertionError(f"package is missing required files: {sorted(missing)}")
    for name in sorted(files):
        path = PurePosixPath(name)
        if path.name.lower() in FORBIDDEN_NAMES or path.suffix.lower() in FORBIDDEN_SUFFIXES:
            raise AssertionError(f"restricted/build-only file entered package: {name}")
        if "godotsteam/sdk" in name.lower() or "/public/steam/" in name.lower():
            raise AssertionError(f"SDK path entered package: {name}")
    native = {name for name in files if PurePosixPath(name).suffix.lower() in {".dll", ".so", ".dylib"}}
    if native != set(expected):
        raise AssertionError(f"unexpected native file set: {sorted(native)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--targets", nargs="+", choices=TARGETS)
    args = parser.parse_args()
    verify_package(args.artifact, args.targets)
    print("REDOTSTEAM_PACKAGE_CHECKS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, struct.error, zipfile.BadZipFile) as error:
        print(f"REDOTSTEAM_PACKAGE_CHECKS=FAIL: {type(error).__name__}")
        raise SystemExit(1)
