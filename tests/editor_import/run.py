#!/usr/bin/env python3
"""Exercise fresh addon discovery, restart, game and autoload startup.

Requires a built addon with its private Steam runtime; see README.md here.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def report_passed(output, name):
    """Require one structured fixture report with zero failures."""
    lines = [line for line in output.splitlines() if line.startswith(name + "=")]
    if len(lines) != 1:
        return False
    try:
        report = json.loads(lines[0][len(name) + 1:])
    except json.JSONDecodeError:
        return False
    return isinstance(report, dict) and report.get("failure_count") == 0 and report.get("failures") == []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--addon", required=True, type=Path)
    parser.add_argument("--scratch", required=True, type=Path, help="New directory outside the checkout")
    parser.add_argument("--library-target", choices=("template_debug", "template_release"), default="template_debug")
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()
    engine = args.engine.resolve(strict=True)
    addon = args.addon.resolve(strict=True)
    scratch = args.scratch.resolve()
    if scratch.is_relative_to(ROOT) or scratch.exists():
        parser.error("--scratch must be a new directory outside the checkout")
    if not (addon / "godotsteam.gdextension").is_file():
        parser.error("--addon must contain the built addon manifest")
    scratch.mkdir(parents=True)
    env = os.environ.copy()
    profile = scratch / "profile"
    profile.mkdir()
    env["APPDATA"] = str(profile)
    rows = []

    def run(project, phase, extra, marker=None):
        command = [str(engine), "--headless", "--audio-driver", "Dummy", "--path", str(project), "--quit-after", "120", *extra]
        try:
            process = subprocess.Popen(command, cwd=project, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, text=True, encoding="utf-8", errors="replace")
            output = process.communicate(timeout=args.timeout)[0]
            code = process.returncode
        except subprocess.TimeoutExpired:
            if os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, timeout=10)
            else:
                process.kill()
            output = process.communicate(timeout=10)[0] + "\nEngine timed out"
            code = "TIMEOUT"
        diagnostics = [line for line in output.splitlines() if "ERROR:" in line or "WARNING:" in line]
        if callable(marker):
            marked = marker(output)
        else:
            marked = marker is None or marker in output
        passed = code == 0 and not diagnostics and marked
        row = {"project": project.name, "phase": phase, "exit": code, "diagnostics": len(diagnostics), "passed": passed}
        rows.append(row)
        print(json.dumps(row), flush=True)
        # Keep retained diagnostics portable, including native stack frames.
        output = re.sub(r"\x1b\[[0-9;]*m", "", output)
        for path, label in ((scratch, "<scratch>"), (ROOT, "<repo clone>"), (engine, "<engine>"), (addon, "<built addon>")):
            output = output.replace(str(path), label).replace(path.as_posix(), label)
        output = re.sub(r"[A-Za-z]:[\\/][^\r\n\"<>]*", "<local path>", output)
        output = re.sub(r"\b[0-9]{17}\b", "<steam id>", output)
        (scratch / (project.name + "-" + phase + ".log")).write_text(output, encoding="utf-8")

    control = scratch / "control"
    control.mkdir()
    (control / "project.godot").write_text('config_version=5\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    run(control, "cold-import", ["--editor", "--import"])
    run(control, "restart", ["--editor"])
    partial = scratch / "plugin-only"
    shutil.copytree(ROOT / "tests/fixture", partial, ignore=shutil.ignore_patterns(".godot", "addons"))
    shutil.copytree(addon, partial / "addons/godotsteam", ignore=shutil.ignore_patterns(".godot", "*.gdextension", "win64", "win32", "linux64", "linux32", "linuxarm64", "osx", "androidarm64"))
    run(partial, "cold-import", ["--editor", "--import"])
    run(partial, "restart", ["--editor"])
    for enabled in (False, True):
        project = scratch / ("enabled" if enabled else "disabled")
        shutil.copytree(ROOT / "tests/fixture", project, ignore=shutil.ignore_patterns(".godot", "addons"))
        shutil.copytree(addon, project / "addons/godotsteam", ignore=shutil.ignore_patterns(".godot"))
        manifest = project / "addons/godotsteam/godotsteam.gdextension"
        manifest.write_text(manifest.read_text(encoding="utf-8").replace(".template_debug.", "." + args.library_target + "."), encoding="utf-8")
        config = project / "project.godot"
        text = config.read_text(encoding="utf-8")
        if not enabled:
            config.write_text(text.replace('enabled=PackedStringArray("godotsteam")', 'enabled=PackedStringArray()'), encoding="utf-8")
        run(project, "cold-import", ["--editor", "--import"], "RedotSteam (GodotSteam API v4.21)" if enabled else None)
        run(project, "restart", ["--editor"], "RedotSteam (GodotSteam API v4.21)" if enabled else None)
        run(project, "game", [], '"failure_count":0')
        run(project, "init-failure", ["--script", "res://init_failure.gd"], lambda output: report_passed(output, "REDOTSTEAM_INIT_FAILURE"))
        if enabled:
            for name in ("startup.gd", "startup.gd.uid", "startup.tscn"):
                shutil.copy2(Path(__file__).parent / name, project / name)
            startup = text.replace('run/main_scene="res://main.tscn"', 'run/main_scene="res://startup.tscn"')
            startup += '\n[autoload]\nSteamStartupCheck="*res://startup.gd"\n'
            for auto_init in (False, True):
                configured = startup.replace("initialize_on_startup=false", "initialize_on_startup=" + str(auto_init).lower())
                if auto_init:
                    configured = configured.replace("[steam]", "[steam]\ninitialization/app_data/app_id=480")
                config.write_text(configured, encoding="utf-8")
                run(project, "autoload-" + str(auto_init).lower(), [], "REDOTSTEAM_AUTOLOAD=PASS")
    (scratch / "results.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
    passed = all(row["passed"] for row in rows)
    print("REDOTSTEAM_EDITOR_IMPORT=" + ("PASS" if passed else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
