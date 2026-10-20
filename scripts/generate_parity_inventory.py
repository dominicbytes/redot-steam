#!/usr/bin/env python3
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "parity" / "upstream-api.json"
UPSTREAM_COMMIT = "8e0b976d0ae02a23c35daecf32c6b7bfeba0da08"
CLASS_SOURCES = {
    "Steam": "godotsteam/godotsteam.cpp",
    "SteamMultiplayerPeer": "godotsteam/godotsteam_multiplayer_peer.cpp",
    "SteamPacketPeer": "godotsteam/steam_packet_peer.cpp",
}


def sha256(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def git(*args: str, binary: bool = False) -> str | bytes:
    output = subprocess.check_output(
        ["git", "-C", str(ROOT), *args],
        text=not binary,
    )
    return output if binary else output.strip()


def upstream_bytes(path: str) -> bytes:
    return git("show", f"{UPSTREAM_COMMIT}:{path}", binary=True)


def extract_cpp(text: str) -> dict[str, list[str]]:
    return {
        "methods": sorted(set(re.findall(r'ClassDB::bind_method\(D_METHOD\("([^"]+)"', text))),
        "signals": sorted(set(re.findall(r'ADD_SIGNAL\(MethodInfo\("([^"]+)"', text))),
        "properties": sorted(set(re.findall(r'ADD_PROPERTY\(PropertyInfo\([^\n]*?"([^"]+)"', text))),
        "constants": sorted(set(re.findall(r'BIND_(?:ENUM_)?CONSTANT\(([^)]+)\)', text))),
    }


def extract_declarations(text: str, class_name: str) -> list[str]:
    # Compare tokenized public sections, including types, defaults and enum values.
    # Comment and whitespace changes do not alter the declaration contract.
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
    match = re.search(r"class\s+" + re.escape(class_name) + r"\s*:[^{]+\{", text)
    if not match:
        raise AssertionError(f"missing class declaration: {class_name}")
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[^\s]', text[match.end():])
    depth, public, sections, current = 1, False, [], []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if depth == 1 and token in ("public", "protected", "private") and tokens[index + 1] == ":":
            if current:
                sections.append(" ".join(current))
                current = []
            public = token == "public"
            index += 2
            continue
        if token == "}":
            depth -= 1
            if depth == 0:
                if current:
                    sections.append(" ".join(current))
                return sections
        if public:
            current.append(token)
        if token == "{":
            depth += 1
        index += 1
    raise AssertionError(f"unterminated class declaration: {class_name}")


def extract_docs(text: str) -> dict[str, list[str]]:
    root = ET.fromstring(text)
    section = lambda name, child: sorted(node.attrib["name"] for node in root.findall(f"./{name}/{child}"))
    return {
        "methods": section("methods", "method"),
        "signals": section("signals", "signal"),
        "constants": section("constants", "constant"),
        "properties": section("members", "member"),
    }


def summarize(bindings: dict[str, list[str]], documented: dict[str, list[str]]) -> dict:
    return {
        "bindings": bindings,
        "documented": documented,
        "counts": {key: len(value) for key, value in bindings.items()},
        "documented_counts": {key: len(value) for key, value in documented.items()},
        "bound_not_documented": {
            key: sorted(set(bindings[key]) - set(documented[key])) for key in bindings
        },
        "documented_not_bound": {
            key: sorted(set(documented[key]) - set(bindings[key])) for key in bindings
        },
    }


def compare(upstream: dict, downstream: dict) -> dict:
    comparison = {}
    exact = True
    for surface in ("bindings", "documented"):
        comparison[surface] = {}
        for kind in ("methods", "signals", "properties", "constants"):
            expected = set(upstream[surface][kind])
            actual = set(downstream[surface][kind])
            missing = sorted(expected - actual)
            added = sorted(actual - expected)
            comparison[surface][kind] = {"missing": missing, "added": added}
            exact = exact and not missing and not added
    comparison["exact"] = exact
    return comparison


def parse_manifest(text: str) -> dict[str, list[dict[str, str]]]:
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_string(text)
    libraries = [
        {"feature": key.strip(), "path": value.strip('"')}
        for key, value in sorted(config["libraries"].items())
    ]
    dependencies = []
    for key, value in sorted(config["dependencies"].items()):
        match = re.search(r'"([^"]+)"', value)
        if match:
            dependencies.append({"feature": key.strip(), "path": match.group(1)})
    return {"libraries": libraries, "dependencies": dependencies}


def build_inventory() -> dict:
    classes = {}
    tracked_files = ["godotsteam.gdextension"]
    all_exact = True
    for name, source in CLASS_SOURCES.items():
        documentation = f"godotsteam/doc_classes/{name}.xml"
        header = source.removesuffix(".cpp") + ".h"
        tracked_files.extend((source, header, documentation))
        upstream = summarize(
            extract_cpp(upstream_bytes(source).decode("utf-8")),
            extract_docs(upstream_bytes(documentation).decode("utf-8")),
        )
        downstream = summarize(
            extract_cpp((ROOT / source).read_text(encoding="utf-8")),
            extract_docs((ROOT / documentation).read_text(encoding="utf-8")),
        )
        upstream["declarations"] = extract_declarations(upstream_bytes(header).decode("utf-8"), name)
        downstream["declarations"] = extract_declarations((ROOT / header).read_text(encoding="utf-8"), name)
        parity = compare(upstream, downstream)
        parity["declarations_exact"] = upstream["declarations"] == downstream["declarations"]
        parity["exact"] = parity["exact"] and parity["declarations_exact"]
        all_exact = all_exact and parity["exact"]
        classes[name] = {
            "source": source,
            "documentation": documentation,
            "upstream": upstream,
            "downstream": downstream,
            "parity": parity,
        }

    upstream_manifest_text = upstream_bytes("godotsteam.gdextension").decode("utf-8")
    downstream_manifest_text = (ROOT / "godotsteam.gdextension").read_text(encoding="utf-8")
    upstream_manifest = parse_manifest(upstream_manifest_text)
    downstream_manifest = parse_manifest(downstream_manifest_text)
    manifest_exact = upstream_manifest == downstream_manifest
    all_exact = all_exact and manifest_exact

    return {
        "schema": 3,
        "oracle": {
            "repository": "https://codeberg.org/godotsteam/godotsteam",
            "commit": UPSTREAM_COMMIT,
            "godotsteam_version": "4.21",
            "steamworks_sdk": "1.65",
        },
        "classes": classes,
        "platform_manifest": {
            "upstream": upstream_manifest,
            "downstream": downstream_manifest,
            "exact": manifest_exact,
        },
        "editor": {
            "plugin": "plugin/plugin.cfg",
            "dock_scene": "plugin/editor/steamworks_panel.tscn",
            "updater": "plugin/editor/updates/updates.gd",
            "expected_downstream_differences": [
                "RedotSteam branding",
                "opt-in release notification",
                "no automatic SDK runtime mutation",
            ],
        },
        "parity_exact": all_exact,
        "source_hashes": {
            path: {
                "upstream": sha256(upstream_bytes(path)),
                "downstream": sha256((ROOT / path).read_bytes()),
            }
            for path in sorted(set(tracked_files))
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    content = json.dumps(build_inventory(), indent=2, sort_keys=True) + "\n"
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != content:
            print(f"Parity inventory is stale: {OUTPUT.relative_to(ROOT)}", file=sys.stderr)
            return 1
        print(f"Parity inventory verified: {OUTPUT.relative_to(ROOT)}")
        return 0
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(content, encoding="utf-8")
    print(f"Wrote {OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
