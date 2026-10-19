#!/usr/bin/env python3
"""Exercise production wrapper argument/lifetime contracts without Steam."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

from run_tests import method

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = (ROOT / "godotsteam/godotsteam.cpp").read_text(encoding="utf-8")
    signatures = (
        "void Steam::convert_config_options(",
        "void Steam::downloadClanActivityCounts(",
        "uint64_t Steam::getChatMemberByIndex(",
        "Dictionary Steam::getFriendGamePlayed(",
        "Array Steam::getFriendsGroupMembersList(",
        "bool Steam::setHTTPRequestRawPostBody(",
        "void Steam::triggerSimpleHapticEvent(",
        "int32 Steam::exchangeItems(",
        "int32 Steam::generateItems(",
        "Array Steam::getResultItems(",
        "void Steam::startPurchase(",
        "uint32_t Steam::createListenSocketIP(",
        "uint32_t Steam::createListenSocketP2P(",
        "uint32_t Steam::connectP2P(",
        "uint32_t Steam::connectByIPAddress(",
        "uint32_t Steam::connectToHostedDedicatedServer(",
        "uint32_t Steam::createListenSocketP2PFakeIP(",
        "void Steam::html_browser_restarted(",
        "void Steam::html_file_open_dialog(",
        "void Steam::fileLoadDialogResponse(",
    )
    methods = [method(source, signature) for signature in signatures]
    declarations = "\n".join(body[:body.index("{")].replace("Steam::", "") + ";" for body in methods)
    harness = (ROOT / "tests/native/core_wrappers.cpp").read_text(encoding="utf-8")
    harness = harness.replace("// PRODUCTION_DECLARATIONS", declarations).replace("// PRODUCTION_METHODS", "\n".join(methods))
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise SystemExit("Run from a C++ compiler environment or set CXX.")
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        (scratch / "test.cpp").write_text(harness, encoding="utf-8")
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
