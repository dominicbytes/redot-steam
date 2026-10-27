#!/usr/bin/env python3
"""Compile production methods with offline doubles; no SDK or client is loaded.

Run from a compiler environment (cl or c++ on PATH). A full extension build
remains necessary to check ABI integration.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def source_text(path, source_ref):
    if source_ref:
        return subprocess.run(["git", "show", f"{source_ref}:{path}"], cwd=ROOT,
                              check=True, capture_output=True, text=True).stdout
    return (ROOT / path).read_text(encoding="utf-8")


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref", help="Test production methods from a git revision")
    parser.add_argument("--case", default="all", choices=['all', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C7', 'C8', 'C9'])
    args = parser.parse_args()
    source = source_text("godotsteam/godotsteam_multiplayer_peer.cpp", args.source_ref)
    bodies = "\n".join(method(source, signature) for signature in (
        "Error SteamMultiplayerPeer::create_host(",
        "void SteamMultiplayerPeer::_remove_peer(",
        "void SteamMultiplayerPeer::_purge_connection_packets(",
        "void SteamMultiplayerPeer::_poll()",
        "void SteamMultiplayerPeer::_upgrade_peer(",
        "void SteamMultiplayerPeer::network_connection_status_changed(",
        "void SteamMultiplayerPeer::_close()",
        "Error SteamMultiplayerPeer::_get_packet(",
        "int SteamMultiplayerPeer::_get_available_packet_count()",
        "SteamMultiplayerPeer::~SteamMultiplayerPeer()",
        "void SteamMultiplayerPeer::_disconnect_peer(",
        "Error SteamMultiplayerPeer::add_peer(",
        "void SteamMultiplayerPeer::_add_pending_peer(",
        "Error SteamMultiplayerPeer::create_client(",
        "Error SteamMultiplayerPeer::_put_packet(",
    ))
    settings = source_text("godotsteam/godotsteam_project_settings.cpp", args.source_ref)
    bodies += "\n" + method(settings, "int SteamProjectSettings::get_max_queued_packets()")
    harness = (ROOT / "tests/native/multiplayer_peer.cpp").read_text(encoding="utf-8")
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        (scratch / "test.cpp").write_text(harness.replace("// PRODUCTION_METHODS", bodies), encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/W4", f"/pathmap:{scratch}=scratch", "test.cpp", "/Fe:test.exe"]
            executable = "test.exe"
        else:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", f"-ffile-prefix-map={scratch.as_posix()}=scratch", "test.cpp", "-o", "test"]
            executable = "test"
        subprocess.run(command, cwd=scratch, check=True, timeout=60)
        subprocess.run([str(scratch / executable), args.case], cwd=scratch, check=True, timeout=10)


if __name__ == "__main__":
    main()
