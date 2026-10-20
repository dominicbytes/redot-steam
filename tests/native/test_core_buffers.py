#!/usr/bin/env python3
"""Compile production core wrappers with offline Steam and container doubles."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_tests import method

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = (ROOT / "godotsteam/godotsteam.cpp").read_text(encoding="utf-8")
    signatures = (
        "PackedInt64Array Steam::sendMessages(",
        "Dictionary Steam::getConfigValue(",
        "int Steam::estimatePingTimeBetweenTwoLocations(",
        "int Steam::estimatePingTimeFromLocalHost(",
        "String Steam::convertPingLocationToString(",
        "Dictionary Steam::getDetailedConnectionStatus(",
        "Dictionary Steam::getCertificateRequest(",
        "Dictionary Steam::setCertificate(",
        "String Steam::getListenSocketAddress(",
        "Dictionary Steam::getFakeIP(",
        "void Steam::fake_ip_result(",
        "Array Steam::getAvailableBeaconLocations(",
        "Dictionary Steam::createMouseCursor(",
        "Array Steam::getInput(",
        "bool Steam::fileWrite(",
        "void Steam::fileWriteAsync(",
        "bool Steam::fileWriteStreamWriteChunk(",
        "PackedByteArray Steam::ugcRead(",
    )
    bodies = "\n".join(method(source, signature) for signature in signatures)
    harness = (ROOT / "tests/native/core_buffers.cpp").read_text(encoding="utf-8-sig")
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as directory:
        scratch = Path(directory)
        (scratch / "test.cpp").write_text(harness.replace("// PRODUCTION_METHODS", bodies), encoding="utf-8")
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
