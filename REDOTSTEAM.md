# RedotSteam

RedotSteam is the Redot 26.2 port of GodotSteam 4.21. It preserves the upstream `Steam` singleton, native classes, method names, signals, constants, documentation, editor dock, and `addons/godotsteam/` installation path.

## Required user-supplied dependencies

- Redot 26.2 LTS.
- Steamworks SDK 1.65 supplied by the plugin user.
- `godot-cpp` commit `6388e26dd8a42071f65f764a3ef3d9523dda3d6e`.
- SCons 4.x and a supported C++ compiler.

Valve SDK headers, import libraries, and redistributable runtime libraries are deliberately excluded from this repository and from release archives. Place the SDK at `godotsteam/sdk/` or pass `steam_sdk_path=<path>` when building. After installing a RedotSteam package, use `scripts/provision_steam_runtime.ps1` to copy the appropriate runtime from your own SDK into `addons/godotsteam/`.

## Windows build

```powershell
./scripts/build_windows.ps1 -Target template_debug -SdkPath "C:/path/to/sdk" -TargetPath bin
```

Add `-IncludeSteamRuntime` only for local runtime verification. Public packages must leave it disabled.

## Linux build

```bash
STEAMWORKS_SDK_PATH=/path/to/sdk ARCH=x86_64 TARGET=template_debug ./scripts/build_linux.sh
STEAMWORKS_SDK_PATH=/path/to/sdk ARCH=arm64 TARGET=template_release ./scripts/build_linux.sh
```

Windows release operators can produce the same Linux targets with a verified portable Zig installation:

```powershell
$env:REDOTSTEAM_PYTHON = "C:\path\to\python.exe"
.\scripts\build_linux_cross.ps1 -Architecture x86_64 -Target template_release -ZigPath C:\path\to\zig.exe -SdkPath C:\private\steamworks-sdk
.\scripts\build_linux_cross.ps1 -Architecture arm64 -Target template_release -ZigPath C:\path\to\zig.exe -SdkPath C:\private\steamworks-sdk
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
./scripts/verify_source_lock.ps1 -SdkPath "C:/path/to/sdk"
```

The authoritative upstream source is [GodotSteam on Codeberg](https://codeberg.org/godotsteam/godotsteam). The original upstream README is preserved as `UPSTREAM_README.md`, and its MIT license is retained.
