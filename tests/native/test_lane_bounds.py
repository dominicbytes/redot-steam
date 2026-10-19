#!/usr/bin/env python3
"""Compile production packet and settings methods with offline doubles."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import argparse
from run_tests import method, source_text

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref", help="Test production methods from a git revision")
    parser.add_argument("--case", default="all", choices=['all', 'C2', 'C7'])
    args = parser.parse_args()
    source = source_text("godotsteam/steam_packet_peer.cpp", args.source_ref)
    bodies = "\n".join(method(source, signature) for signature in (
        "SteamPacketPeer::SteamPacketPeer()", "void SteamPacketPeer::set_connection_handle(",
        "Error SteamPacketPeer::send(", "Error SteamPacketPeer::ping(",
    ))
    settings = source_text("godotsteam/godotsteam_project_settings.cpp", args.source_ref)
    bodies += "\n" + "\n".join(method(settings, signature) for signature in (
        "void SteamProjectSettings::register_settings()", "int SteamProjectSettings::get_max_channels()",
    ))
    harness = (ROOT / "tests/native/lane_bounds.cpp").read_text(encoding="utf-8")
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as directory:
        scratch = Path(directory)
        (scratch / "test.cpp").write_text(harness.replace("// PRODUCTION_SEND", bodies), encoding="utf-8")
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
