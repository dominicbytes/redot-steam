#!/usr/bin/env python3
"""Compile production user/UGC methods with offline API doubles."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
from run_tests import method

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = (ROOT / "godotsteam/godotsteam.cpp").read_text(encoding="utf-8")
    names = ("initiateGameConnection", "beginAuthSession", "getDownloadedItems",
             "getQueryUGCChildren", "getQueryUGCContentDescriptors", "getUserContentDescriptorPreferences",
             "setItemsDisabledLocally", "setSubscriptionsLoadOrder", "getSupportedGameVersionData",
             "getGlobalStatIntHistory", "getGlobalStatFloatHistory", "getImageSize",
             "gamepad_text_input_dismissed", "leaderboard_scores_downloaded",
             "sendQueryUGCRequest", "releaseQueryUGCRequest", "ugc_query_completed")
    bodies = []
    for name in names:
        start = source.index(f"Steam::{name}(")
        signature = source[source.rfind("\n", 0, start) + 1:start] + f"Steam::{name}("
        bodies.append(method(source, signature))
    harness = (ROOT / "tests/native/ugc_user.cpp").read_text(encoding="utf-8")
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as directory:
        scratch = Path(directory)
        (scratch / "test.cpp").write_text(harness.replace("// PRODUCTION_METHODS", "\n".join(bodies)), encoding="utf-8")
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
