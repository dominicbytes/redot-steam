#!/usr/bin/env python3
"""Compile the production send method with offline transport doubles."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = (ROOT / "godotsteam/steam_packet_peer.cpp").read_text(encoding="utf-8")
    start = source.index("Error SteamPacketPeer::send(")
    end = source.index("\nError SteamPacketPeer::ping(", start)
    harness = (ROOT / "tests/native/lane_bounds.cpp").read_text(encoding="utf-8")
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as directory:
        scratch = Path(directory)
        (scratch / "test.cpp").write_text(harness.replace("// PRODUCTION_SEND", source[start:end]), encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/W4", "test.cpp", "/Fe:test.exe"]
            executable = "test.exe"
        else:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "test.cpp", "-o", "test"]
            executable = "test"
        subprocess.run(command, cwd=scratch, check=True, timeout=60)
        subprocess.run([str(scratch / executable)], cwd=scratch, check=True, timeout=10)


if __name__ == "__main__":
    main()
