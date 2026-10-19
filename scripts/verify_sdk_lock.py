#!/usr/bin/env python3
"""Verify the admitted SDK input trees without distributing their contents."""
import argparse
import json
from pathlib import Path

from create_build_manifest import tree_digest

ROOT = Path(__file__).resolve().parents[1]


def verify_sdk(sdk: Path) -> None:
    lock = json.loads((ROOT / "scripts/sdk-input-lock.json").read_text(encoding="utf-8"))
    for relative, expected in lock["trees"].items():
        if tree_digest(sdk / relative) != expected:
            raise AssertionError(f"SDK input hash mismatch: {relative}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-path", type=Path, required=True)
    args = parser.parse_args()
    try:
        verify_sdk(args.sdk_path)
        print("REDOTSTEAM_SDK_LOCK=PASS")
    except (AssertionError, OSError):
        print("REDOTSTEAM_SDK_LOCK=FAIL")
        raise SystemExit(1)
