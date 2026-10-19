#!/usr/bin/env python3
from __future__ import annotations

import configparser
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    # Redot 26.3 uses this same ABI; its raw Struct bindings are unsupported by the pin.
    api = json.loads((ROOT / "redot-api/26.2/extension_api.json").read_text(encoding="utf-8"))
    require(api["redot_header"]["version_major"] == 26, "Redot binding ABI major version mismatch")
    require(api["redot_header"]["version_minor"] == 2, "Pinned 26.2 binding ABI mismatch (also used on 26.3)")
    require(api["header"]["version_major"] == 4 and api["header"]["version_minor"] == 5, "Godot API lineage mismatch")

    inventory = json.loads((ROOT / "docs/gamedev/parity/upstream-api.json").read_text(encoding="utf-8"))
    require(inventory["classes"]["Steam"]["upstream"]["counts"]["methods"] >= 700, "Steam method inventory is unexpectedly incomplete")
    require(inventory["classes"]["Steam"]["upstream"]["counts"]["signals"] >= 180, "Steam signal inventory is unexpectedly incomplete")
    require(set(inventory["classes"]) == {"Steam", "SteamMultiplayerPeer", "SteamPacketPeer"}, "Native class inventory mismatch")
    # These additions are deliberate fixes; every other upstream difference fails the check.
    additions = {
        "methods": ["fileLoadDialogResponse"],
        "signals": [],
        "constants": [],
        "properties": [],
    }
    for name, entry in inventory["classes"].items():
        for surface in ("bindings", "documented"):
            for kind, difference in entry["parity"][surface].items():
                expected = additions[kind] if name == "Steam" else []
                require(difference == {"missing": [], "added": expected},
                        f"Unexpected upstream difference: {name}.{surface}.{kind}")
    require(inventory["platform_manifest"]["exact"], "Platform manifest differs from upstream")

    plugin = configparser.ConfigParser()
    plugin.read(ROOT / "plugin/plugin.cfg", encoding="utf-8")
    require(plugin["plugin"]["name"].strip('"') == "RedotSteam", "Editor plugin branding mismatch")

    sconstruct = (ROOT / "SConstruct").read_text(encoding="utf-8")
    require('BoolVariable("include_steam_runtime"' in sconstruct, "Runtime exclusion option missing")
    require("if env[\"include_steam_runtime\"]" in sconstruct, "Steam runtime copy is not gated")

    tracked_sdk = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "godotsteam/sdk"], text=True).splitlines()
    require(tracked_sdk == ["godotsteam/sdk/Include Steam SDK here"], f"Restricted SDK files are tracked: {tracked_sdk}")

    print("REDOTSTEAM_SOURCE_CHECKS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"REDOTSTEAM_SOURCE_CHECKS=FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
