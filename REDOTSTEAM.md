# RedotSteam

RedotSteam is the Redot 26.2 / 26.3 port of GodotSteam 4.21. It preserves the upstream `Steam` singleton, native classes, method names, signals, constants, documentation, editor dock, and `addons/godotsteam/` installation path.

## Engine compatibility

Use `redot_api_dir=redot-api/26.2` for both engines. The Windows wrapper defaults to this snapshot and accepts `-RedotApiDir` for explicit input selection. Do not replace it with a raw 26.3 API dump: 26.3 adds the `Struct` Variant, which the pinned godot-cpp Variant implementation does not support, and official 26.3 returns null for its extension conversion functions. The APIs used by this extension and editor plugin remain compatible.

The extension manifest uses `compatibility_minimum = "4.5"`, matching the pinned bindings' 4.5 lineage. It does not use Redot's product version numbering. No maximum is set; later Redot versions still need separate verification.

Windows x86-64 debug and release DLLs are tested on official 26.2 and 26.3 and an unofficial 26.3 build with an engine shutdown fix. Live App ID 480 initialization, callback pumping and shutdown pass; a separate missing-App-ID initialization fails cleanly, native and mock fixtures run, peer objects construct and release, and the editor plugin parses and enables after import and restart. Official 26.3 headless editor shutdown may crash or report RID leaks even without the addon. Initial discovery of a CORE-level extension also needs an editor restart; see `PORT_STATUS.md` for limitations. Native behavior without a running Steam client has not been verified. Mock failure and native missing-App-ID failure were verified instead. Linux runtime verification remains at 26.2; macOS and Android are unverified.

To inspect another engine's API, set `REDOT_BIN` and run `scripts/capture_redot_api.ps1 -OutputDirectory <scratch>`. Dumps go to scratch by default, and a 26.3 dump is rejected if directed at the pinned 26.2 snapshot. `scripts/verify_source_lock.ps1 -RedotBin <REDOT_BIN> -SdkPath <SDK>` recognizes both official Windows engine identities while checking the unchanged binding inputs.

## Binary package installation

Extract the addon ZIP into your project so it contains `addons/godotsteam/`. Supply Steamworks SDK 1.65 separately and run the shipped helper from the project root:

```powershell
./addons/godotsteam/tools/provision_steam_runtime.ps1 -SdkPath <SDK> -AddonPath addons/godotsteam -Target windows-x86_64
```

Choose `linux-x86_64` or `linux-arm64` for the corresponding Linux runtime. Enable RedotSteam in Project Settings > Plugins and restart the editor after import. The package does not require a compiler, SCons, or a godot-cpp checkout.

## Source checkout dependencies

- Redot 26.2 LTS or Redot 26.3 stable (Windows x86-64 verified).
- Steamworks SDK 1.65 supplied by the plugin user.
- `godot-cpp` commit `6388e26dd8a42071f65f764a3ef3d9523dda3d6e`.
- SCons 4.x and a supported C++ compiler.

Valve SDK headers, import libraries, and redistributable runtime libraries are deliberately excluded from this repository and from release archives. Place the SDK at `godotsteam/sdk/` or pass `steam_sdk_path=<path>` when building. For binary-package setup, use the shipped helper shown above. The following build and verification commands require a source checkout.

## Windows build

```powershell
./scripts/build_windows.ps1 -Target template_debug -SdkPath <SDK> -TargetPath bin
```

Add `-IncludeSteamRuntime` only for local runtime verification. Public packages must leave it disabled.

## Linux build

```bash
STEAMWORKS_SDK_PATH=<SDK> ARCH=x86_64 TARGET=template_debug ./scripts/build_linux.sh
STEAMWORKS_SDK_PATH=<SDK> ARCH=arm64 TARGET=template_release ./scripts/build_linux.sh
```

Windows release operators can produce the same Linux targets with a verified portable Zig installation:

```powershell
$env:REDOTSTEAM_PYTHON = "<PYTHON>"
.\scripts\build_linux_cross.ps1 -Architecture x86_64 -Target template_release -ZigPath <ZIG> -SdkPath <SDK>
.\scripts\build_linux_cross.ps1 -Architecture arm64 -Target template_release -ZigPath <ZIG> -SdkPath <SDK>
```

The cross-build defaults to glibc 2.27. This toolchain consumes SDK inputs for compilation and linking only; it does not add Steamworks redistributables unless `-IncludeSteamRuntime` is explicitly used for a private fixture.

For release-grade Linux arm64 artifacts, use the pinned Docker GNU cross-toolchain:

```bash
docker build -f scripts/Dockerfile.linux-cross-arm64 -t redotsteam-linux-cross-arm64:26.2 .
docker run --rm -v "$PWD:/src" -w /src redotsteam-linux-cross-arm64:26.2 bash -lc \
  'STEAMWORKS_SDK_PATH=godotsteam/sdk TARGET=template_release JOBS=8 bash scripts/build_linux_arm64_cross.sh'
```

The GNU path is the release default. Zig remains a portability fallback until its unload/exit behavior passes the same fixture.

## Verification

```powershell
python scripts/generate_parity_inventory.py --check
python scripts/verify_port.py
./scripts/verify_source_lock.ps1 -SdkPath <SDK>
```

The authoritative upstream source is [GodotSteam on Codeberg](https://codeberg.org/godotsteam/godotsteam). The original upstream README is preserved as `UPSTREAM_README.md`, and its MIT license is retained.
