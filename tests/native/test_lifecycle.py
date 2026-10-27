#!/usr/bin/env python3
"""Compile Steam lifecycle methods with offline doubles; no SDK or client is loaded."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


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
    source = (ROOT / "godotsteam/godotsteam.cpp").read_text(encoding="utf-8")
    bodies = "\n".join(method(source, s) for s in ("void Steam::steamShutdown()", "Steam::~Steam()"))
    harness = (ROOT / "tests/native/lifecycle.cpp").read_text(encoding="utf-8")
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        (scratch / "test.cpp").write_text(harness.replace("// PRODUCTION_LIFECYCLE", bodies), encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/W4", f"/pathmap:{scratch}=scratch", "test.cpp", "/Fe:test.exe"]
            executable = "test.exe"
        else:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", f"-ffile-prefix-map={scratch.as_posix()}=scratch", "test.cpp", "-o", "test"]
            executable = "test"
        subprocess.run(command, cwd=scratch, check=True, timeout=60)
        subprocess.run([str(scratch / executable)], cwd=scratch, check=True, timeout=10)


if __name__ == "__main__":
    main()
