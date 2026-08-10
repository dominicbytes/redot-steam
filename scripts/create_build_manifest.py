#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM_COMMIT = "8e0b976d0ae02a23c35daecf32c6b7bfeba0da08"
FORBIDDEN = {"steam_api.h", "steam_api.dll", "steam_api64.dll", "steam_api.lib", "steam_api64.lib", "libsteam_api.so", "libsteam_api.dylib"}


def digest_bytes(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest().upper()


def digest_file(path: Path) -> str:
    return digest_bytes(path.read_bytes())


def git(*args: str) -> str:
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def tree_digest(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        digest.update(path.relative_to(root).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest().upper()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--sdk-path", type=Path, default=ROOT / "godotsteam/sdk")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    artifact = args.artifact.resolve()
    output = args.output.resolve() if args.output else artifact.with_suffix(".build-manifest.json")

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
        "schema": 1,
        "project": "RedotSteam",
        "artifact": {"path": artifact.name, "sha256": digest_file(artifact)},
        "source": {
            "downstream_repository": "https://github.com/dominicbytes/redot-steam",
            "downstream_commit": git("rev-parse", "HEAD"),
            "upstream_repository": "https://codeberg.org/godotsteam/godotsteam",
            "upstream_commit": UPSTREAM_COMMIT,
            "working_tree_dirty": bool(status.strip()),
        },
        "redot": {
            "version": "26.2.stable.official.4f5b14aba",
            "binary_sha256": "10DFBEFC273536F0C65903D6429E531C30133820BF0FA842F2D6E6FB81DB1D42",
            "extension_api_sha256": digest_file(ROOT / "redot-api/26.2/extension_api.json"),
            "gdextension_interface_sha256": digest_file(ROOT / "redot-api/26.2/gdextension_interface.h"),
        },
        "godot_cpp_commit": "6388e26dd8a42071f65f764a3ef3d9523dda3d6e",
        "steamworks_sdk": {
            "version": "1.65",
            "used_for_build": True,
            "included_in_package": False,
            "provenance": "USER_SUPPLIED",
            "public_headers_tree_sha256": tree_digest(sdk_headers),
            "redistributables_tree_sha256": tree_digest(sdk_redist),
        },
        "binaries": sorted(binaries, key=lambda item: item["path"]),
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
