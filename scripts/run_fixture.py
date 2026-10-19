#!/usr/bin/env python3
"""Run a fixture with a deadline and require its completion report."""
from __future__ import annotations

import argparse
import json
import os
import re
import signal
import subprocess


def validate_output(output: str, marker: str) -> None:
    if re.search(r"SCRIPT ERROR|^\s*ERROR:|failed to load", output, re.I | re.M):
        raise AssertionError("engine reported a script/load/runtime error")
    reports = re.findall(r"^" + re.escape(marker) + r"=(.*)$", output, re.M)
    if len(reports) != 1:
        raise AssertionError("expected exactly one fixture completion marker")
    if marker in ("REDOTSTEAM_FIXTURE", "REDOTSTEAM_INIT_FAILURE"):
        report = json.loads(reports[0])
        if not isinstance(report, dict):
            raise AssertionError("fixture report must be an object")
        if type(report.get("failure_count")) is not int or report["failure_count"] != 0 or report.get("failures") != []:
            raise AssertionError("native fixture did not report zero failures")
    elif reports[0].strip() != "PASS":
        raise AssertionError("fixture did not complete successfully")


def run(command: list[str], marker: str, timeout: float) -> int:
    options = {"start_new_session": True} if os.name != "nt" else {}
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", **options) as process:
        try:
            output, _ = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            if os.name == "nt":
                subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True, timeout=10)
            else:
                os.killpg(process.pid, signal.SIGKILL)
            process.kill()
            process.communicate(timeout=10)
            raise AssertionError("fixture exceeded wall-clock timeout")
        print(output, end="")
        if process.returncode:
            raise AssertionError(f"fixture exited with {process.returncode}")
        validate_output(output, marker)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--marker", required=True, choices=["REDOTSTEAM_FIXTURE", "REDOTSTEAM_MOCK_FIXTURE", "REDOTSTEAM_INIT_FAILURE"])
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command or args.timeout <= 0:
        parser.error("a command and positive timeout are required")
    return run(command, args.marker, args.timeout)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError, OSError) as error:
        print(f"REDOTSTEAM_RUNTIME_CHECK=FAIL: {type(error).__name__}: {error}")
        raise SystemExit(1)
