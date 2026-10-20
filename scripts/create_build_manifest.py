#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import zipfile
import struct

from verify_binaries import TARGETS
from verify_package import verify_package
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM_COMMIT = "8e0b976d0ae02a23c35daecf32c6b7bfeba0da08"
FORBIDDEN = {"steam_api.h", "steam_api.dll", "steam_api64.dll", "steam_api.lib", "steam_api64.lib", "libsteam_api.so", "libsteam_api.dylib"}

SDK_TEXT_EXTENSIONS = (".h", ".json")


def digest_bytes(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest().upper()


def digest_file(path: Path) -> str:
    return digest_bytes(path.read_bytes())


def git(*args: str) -> str:
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def tree_digest(root: Path, text_extensions=()) -> str:
    digest = hashlib.sha256()
    files = sorted((item for item in root.rglob("*") if item.is_file()), key=lambda item: item.relative_to(root).as_posix())
    if not root.is_dir() or not files or not any(path.stat().st_size for path in files):
        raise AssertionError("required input tree is missing or empty")
    for path in files:
        digest.update(path.relative_to(root).as_posix().encode("utf-8"))
        digest.update(b"\0")
        content = path.read_bytes()
        if path.suffix.lower() in text_extensions:
            content = content.replace(b"\r\n", b"\n")
        digest.update(content)
        digest.update(b"\0")
    return digest.hexdigest().upper()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--sdk-path", type=Path, default=ROOT / "godotsteam/sdk")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--targets", nargs="+", choices=TARGETS)
    args = parser.parse_args()
    artifact = args.artifact.resolve()
    output = args.output.resolve() if args.output else artifact.with_suffix(".build-manifest.json")

    verify_package(artifact, args.targets)
    binaries = []
    with zipfile.ZipFile(artifact) as package:
        for info in package.infolist():
            if info.is_dir():
                continue
            name = Path(info.filename).name
            if name in FORBIDDEN:
                raise SystemExit(f"Forbidden Steamworks SDK file in package: {info.filename}")
            if Path(info.filename).suffix.lower() in {".dll", ".so", ".dylib"}:
                binaries.append({"path": info.filename, "sha256": digest_bytes(package.read(info))})

    status = subprocess.check_output(
        ["git", "-C", str(ROOT), "status", "--porcelain", "--ignore-submodules=dirty"],
        text=True,
    )
    sdk_headers = args.sdk_path.resolve() / "public/steam"
    sdk_redist = args.sdk_path.resolve() / "redistributable_bin"
    manifest = {
        "schema": 2,
        "purpose": "Artifact and input observations at manifest creation; not build provenance",
        "build_provenance_verified": False,
        "targets": args.targets or list(TARGETS),
        "project": "RedotSteam",
        "artifact": {"path": artifact.name, "sha256": digest_file(artifact)},
        "source_observed_at_manifest_creation": {
            "downstream_repository": "https://github.com/dominicbytes/redot-steam",
            "downstream_commit": git("rev-parse", "HEAD"),
            "upstream_repository": "https://codeberg.org/godotsteam/godotsteam",
            "upstream_commit": UPSTREAM_COMMIT,
            "working_tree_dirty": bool(status.strip()),
        },
        "binding_abi_observed_at_manifest_creation": {
            "extension_api_sha256": digest_file(ROOT / "redot-api/26.2/extension_api.json"),
            "gdextension_interface_sha256": digest_file(ROOT / "redot-api/26.2/gdextension_interface.h"),
        },
        "godot_cpp_commit_observed": subprocess.check_output(["git", "-C", str(ROOT / "godot-cpp"), "rev-parse", "HEAD"], text=True).strip(),
        "steamworks_sdk_observed_at_manifest_creation": {
            "included_in_package": False,
            "provenance": "USER_SUPPLIED_INPUT_NOT_BUILD_ATTESTATION",
            "public_headers_tree_sha256": tree_digest(sdk_headers),
            "redistributables_tree_sha256": tree_digest(sdk_redist),
            "admission_text_normalization": "CRLF_TO_LF",
            "admission_text_extensions": SDK_TEXT_EXTENSIONS,
            "public_headers_admission_tree_sha256": tree_digest(sdk_headers, SDK_TEXT_EXTENSIONS),
            "redistributables_admission_tree_sha256": tree_digest(sdk_redist, SDK_TEXT_EXTENSIONS),
        },
        "binaries": sorted(binaries, key=lambda item: item["path"]),
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("REDOTSTEAM_MANIFEST=OBSERVATIONS_ONLY")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, struct.error, zipfile.BadZipFile) as error:
        print(f"REDOTSTEAM_MANIFEST=FAIL: {type(error).__name__}")
        raise SystemExit(1)
