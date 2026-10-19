"""SDK-free regressions for release verification false-pass probes."""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import create_build_manifest as manifest
import generate_parity_inventory as parity
import run_fixture
import verify_binaries as binaries
import verify_package as packages
import verify_sdk_lock as sdk_lock


def pe_image():
    data = bytearray(0x800)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, 0x8664, 1, 0, 0, 0, 240, 0x2000)
    struct.pack_into("<H", data, 0x98, 0x20B)
    struct.pack_into("<I", data, 0x98 + 108, 16)
    struct.pack_into("<IIII", data, 0x98 + 112, 0x1000, 0x100, 0x1100, 40)
    struct.pack_into("<8sIIIIIIHHI", data, 0x188, b".text", 0x600, 0x1000, 0x600, 0x200, 0, 0, 0, 0, 0x60000020)
    struct.pack_into("<IIHHIIIIIII", data, 0x200, 0, 0, 0, 0, 0, 1, 1, 1, 0x1040, 0x1044, 0x1048)
    struct.pack_into("<IIH", data, 0x240, 0x1300, 0x1050, 0)
    data[0x250:0x25F] = b"godotsteam_init\0"
    struct.pack_into("<IIIII", data, 0x300, 0, 0, 0, 0x1140, 0x1180)
    data[0x340:0x350] = b"steam_api64.dll\0\0"
    data[0x500] = 0xC3
    return bytes(data)


def elf_image():
    data = bytearray(0x800)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHIQQQIHHHHHH", data, 16, 3, 62, 1, 0, 64, 0x600, 0, 64, 56, 2, 64, 3, 0)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, len(data), len(data), 0x1000)
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 6, 0x200, 0x200, 0, 112, 112, 8)
    for index, pair in enumerate(((1, 1), (5, 0x300), (6, 0x400), (10, 80), (11, 24), (29, 17), (0, 0))):
        struct.pack_into("<qQ", data, 0x200 + index * 16, *pair)
    strings = b"\0libsteam_api.so\0$ORIGIN\0godotsteam_init\0"
    data[0x300:0x300 + len(strings)] = strings
    struct.pack_into("<IBBHQQ", data, 0x400 + 24, 25, 0x12, 0, 1, 0x500, 1)
    data[0x500] = 0xC3
    struct.pack_into("<IIQQQQIIQQ", data, 0x640, 0, 3, 2, 0x300, 0x300, 80, 0, 0, 1, 0)
    struct.pack_into("<IIQQQQIIQQ", data, 0x680, 0, 11, 2, 0x400, 0x400, 48, 1, 0, 8, 24)
    return bytes(data)


class PayloadTests(unittest.TestCase):
    def test_structured_images(self):
        binaries.verify_binary(pe_image(), "pe", 0x8664)
        binaries.verify_binary(elf_image(), "elf", 62)

    def test_header_only_decoys_and_empty_images(self):
        pe = bytearray(128)
        pe[:2] = b"MZ"
        struct.pack_into("<I", pe, 0x3C, 64)
        pe[64:68] = b"PE\0\0"
        struct.pack_into("<H", pe, 68, 0x8664)
        elf = bytearray(128)
        elf[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<H", elf, 18, 62)
        elf[30:54] = b"libsteam_api.so $ORIGIN\0\0"
        for data, kind, machine in ((bytes(pe), "pe", 0x8664), (bytes(elf), "elf", 62), (b"", "pe", 0x8664), (b"", "elf", 183)):
            with self.subTest(kind=kind, size=len(data)), self.assertRaises(AssertionError):
                binaries.verify_binary(data, kind, machine)

    def test_export_dependency_architecture_and_runpath(self):
        for image, kind, machine, original, replacement in (
            (pe_image(), "pe", 0x8664, b"godotsteam_init", b"unrelated_init"),
            (pe_image(), "pe", 0x8664, b"steam_api64.dll", b"other_api6.dll"),
            (elf_image(), "elf", 62, b"godotsteam_init", b"unrelated_init"),
            (elf_image(), "elf", 62, b"libsteam_api.so", b"libother_api.so"),
            (elf_image(), "elf", 62, b"$ORIGIN", b"garbage"),
        ):
            with self.subTest(original=original, kind=kind), self.assertRaises(AssertionError):
                binaries.verify_binary(image.replace(original, replacement), kind, machine)
        with self.assertRaises(AssertionError):
            binaries.verify_binary(elf_image(), "elf", 183)

    def test_loader_tables_cannot_be_replaced_by_string_decoys(self):
        dependency = bytearray(elf_image())
        struct.pack_into("<Q", dependency, 0x208, 25)
        export = bytearray(elf_image())
        struct.pack_into("<H", export, 0x41E, 0)
        for data in (dependency, export):
            self.assertIn(b"libsteam_api.so", data)
            self.assertIn(b"godotsteam_init", data)
            with self.assertRaises(AssertionError):
                binaries.verify_binary(bytes(data), "elf", 62)

    def test_package_reads_payload_and_requires_complete_addon(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path(directory) / "addon.zip"
            def write(overrides=None, omit=()):
                content = {name: b"payload" for name in packages.REQUIRED}
                content.update({"addons/godotsteam/" + name: pe_image() for name in binaries.expected_for(["windows-x86_64"])})
                content.update(overrides or {})
                with zipfile.ZipFile(artifact, "w") as package:
                    for name, data in content.items():
                        if name not in omit:
                            package.writestr(name, data)
            write()
            packages.verify_package(artifact, ["windows-x86_64"])
            with self.assertRaises(AssertionError):
                packages.verify_package(artifact)  # Partial packages never qualify all targets.
            for name in packages.REQUIRED:
                write(omit=(name,))
                with self.subTest(missing=name), self.assertRaises(AssertionError):
                    packages.verify_package(artifact, ["windows-x86_64"])
            write({next(iter(packages.EXPECTED_BINARIES)): b""})
            with self.assertRaises(AssertionError):
                packages.verify_package(artifact, ["windows-x86_64"])
            name = "addons/godotsteam/" + next(iter(binaries.expected_for(["windows-x86_64"])))
            write({name: b"MZ"})
            with self.assertRaises(AssertionError):
                packages.verify_package(artifact, ["windows-x86_64"])


class RuntimeTests(unittest.TestCase):
    def test_completion_is_required(self):
        for output in ("", "SCRIPT ERROR: parse failure\nREDOTSTEAM_MOCK_FIXTURE=PASS\n", "ERROR: Failed to load script\nREDOTSTEAM_MOCK_FIXTURE=PASS\n", "REDOTSTEAM_MOCK_FIXTURE=PASS\n" * 2):
            with self.assertRaises(AssertionError):
                run_fixture.validate_output(output, "REDOTSTEAM_MOCK_FIXTURE")
        run_fixture.validate_output("REDOTSTEAM_MOCK_FIXTURE=PASS\n", "REDOTSTEAM_MOCK_FIXTURE")

    def test_native_zero_failures(self):
        for payload in ([], {}, {"failure_count": 1, "failures": ["bad"]}, {"failure_count": False, "failures": []}, {"failure_count": 0}):
            with self.assertRaises(AssertionError):
                run_fixture.validate_output("REDOTSTEAM_FIXTURE=" + json.dumps(payload), "REDOTSTEAM_FIXTURE")
        for marker in ("REDOTSTEAM_FIXTURE", "REDOTSTEAM_INIT_FAILURE"):
            run_fixture.validate_output(marker + '={"failure_count":0,"failures":[]}', marker)

    def test_zero_exit_without_report_and_wall_clock_timeout(self):
        for code, timeout in (("pass", 10), ("import time; time.sleep(30)", 0.1)):
            with self.assertRaises(AssertionError), contextlib.redirect_stdout(io.StringIO()):
                run_fixture.run([sys.executable, "-c", code], "REDOTSTEAM_MOCK_FIXTURE", timeout)


class InventoryAndManifestTests(unittest.TestCase):
    def test_header_parameter_mutation_changes_inventory(self):
        original = Path.read_text
        def read(path, *args, **kwargs):
            text = original(path, *args, **kwargs)
            if path == parity.ROOT / "godotsteam/godotsteam.h":
                old = "String getAppInstallDir(uint32_t app_id);"
                self.assertIn(old, text)
                return text.replace(old, "String getAppInstallDir(uint64_t app_id);")
            return text
        with mock.patch.object(Path, "read_text", read):
            inventory = parity.build_inventory()
            with mock.patch.object(sys, "argv", ["inventory", "--check"]), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(parity.main(), 1)
        self.assertFalse(inventory["parity_exact"])
        self.assertFalse(inventory["classes"]["Steam"]["parity"]["declarations_exact"])

    def test_declarations_cover_return_defaults_and_enums(self):
        header = "class Example : public Object { public: enum X { A = 2 }; int call(int arg = 3); private: int ignored; };"
        baseline = parity.extract_declarations(header, "Example")
        for old, new in (("int call", "void call"), ("arg = 3", "arg = 4"), ("A = 2", "A = 7")):
            self.assertNotEqual(baseline, parity.extract_declarations(header.replace(old, new), "Example"))

    def test_input_trees_must_exist_and_contain_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for path in (root / "missing", root):
                with self.assertRaises(AssertionError):
                    manifest.tree_digest(path)
            (root / "empty").touch()
            with self.assertRaises(AssertionError):
                manifest.tree_digest(root)
            (root / "empty").write_bytes(b"sdk")
            self.assertEqual(len(manifest.tree_digest(root)), 64)
            with self.assertRaises(AssertionError):
                sdk_lock.verify_sdk(root)

    def test_manifest_rejects_missing_sdk_and_reports_observations_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifact = root / "addon.zip"
            content = {name: b"payload" for name in packages.REQUIRED}
            content.update({"addons/godotsteam/" + name: pe_image() for name in binaries.expected_for(["windows-x86_64"])})
            with zipfile.ZipFile(artifact, "w") as package:
                for name, data in content.items():
                    package.writestr(name, data)
            output = root / "manifest.json"
            sdk = root / "sdk"
            argv = ["manifest", str(artifact), "--sdk-path", str(sdk), "--output", str(output), "--targets", "windows-x86_64"]
            with mock.patch.object(sys, "argv", argv), self.assertRaises(AssertionError):
                manifest.main()
            self.assertFalse(output.exists())
            for relative in ("public/steam/header", "redistributable_bin/runtime"):
                path = sdk / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"input")
            with mock.patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(manifest.main(), 0)
            observed = json.loads(output.read_text())
            self.assertFalse(observed["build_provenance_verified"])
            self.assertNotIn("redot", observed)
            self.assertNotIn("used_for_build", observed["steamworks_sdk_observed_at_manifest_creation"])

    def test_sdk_lock_detects_changed_admitted_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tree = root / "sdk/public/steam"
            tree.mkdir(parents=True)
            header = tree / "header"
            header.write_bytes(b"admitted")
            (root / "scripts").mkdir()
            (root / "scripts/sdk-input-lock.json").write_text(json.dumps({"trees": {"public/steam": manifest.tree_digest(tree)}}))
            with mock.patch.object(sdk_lock, "ROOT", root):
                sdk_lock.verify_sdk(root / "sdk")
                header.write_bytes(b"changed")
                with self.assertRaises(AssertionError):
                    sdk_lock.verify_sdk(root / "sdk")


SHELLS = [shell for shell in ("powershell", "pwsh") if shutil.which(shell)]


@unittest.skipUnless(SHELLS, "PowerShell is unavailable")
class PowerShellTests(unittest.TestCase):
    def test_later_stages_ignore_bogus_environment_evidence(self):
        for shell in SHELLS:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "scripts").mkdir()
                script = root / "scripts/run_gauntlet.ps1"
                shutil.copyfile(ROOT / "scripts/run_gauntlet.ps1", script)
                env = dict(os.environ, REDOTSTEAM_PRIVATE_APP_ID="anything", REDOTSTEAM_REPRO_EVIDENCE="anything", REDOTSTEAM_INDEPENDENT_EVIDENCE="anything")
                for stage in ("beta", "rc", "stable-prep"):
                    result = subprocess.run([shell, "-NoProfile", "-File", str(script), "-Stage", stage, "-EvidenceRoot", "evidence"], env=env, capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    gate = json.loads((root / f"evidence/{stage}/gate-{stage}.json").read_text(encoding="utf-8-sig"))
                    self.assertEqual(gate["status"], "BLOCKED")

    def test_package_normalizes_relative_parent_and_rooted_paths(self):
        for shell in SHELLS:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "repo/scripts").mkdir(parents=True)
                (root / "build/addon").mkdir(parents=True)
                (root / "build/addon/marker.txt").write_text("payload")
                (root / "build/addon/steam_api64.dll").write_bytes(b"private")
                script = root / "repo/scripts/package_addon.ps1"
                shutil.copyfile(ROOT / "scripts/package_addon.ps1", script)
                for build in ("../build/addon", str(root / "build/addon")):
                    result = subprocess.run([shell, "-NoProfile", "-File", str(script), "-Version", "test", "-BuildRoot", build, "-OutputDirectory", str(root / "packages")], capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    with zipfile.ZipFile(root / "packages/redotsteam-test-addon.zip") as package:
                        self.assertEqual([info.filename for info in package.infolist() if not info.is_dir()], ["addons/godotsteam/marker.txt"])


if __name__ == "__main__":
    unittest.main()
