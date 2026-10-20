# RedotSteam

RedotSteam is a Redot 26.2 / 26.3 port of [GodotSteam 4.21](https://codeberg.org/godotsteam/godotsteam). It keeps the upstream `Steam` singleton, native classes, methods, signals, constants, documentation, editor dock, and `addons/godotsteam/` installation path compatible with Redot.

The supported Windows runtimes are `Redot 26.2.stable.official.4f5b14aba` and `Redot 26.3.stable.official.85e520394`. Both use the pinned 26.2 binding ABI. Windows x86-64 and Linux x86-64/arm64 are required platforms; macOS and Android remain tracked for later completion. Older Redot releases are not a compatibility target.

## Status

- Exact public API parity with the pinned GodotSteam 4.21 source is verified by the checked-in parity inventory.
- Historical Alpha evidence records Windows x86-64 and Linux x86-64/arm64 debug/release builds and Redot 26.2 fixture checks. It does not qualify the current candidate.
- Windows x86-64 debug/release binaries built against the 26.2 ABI also pass live App ID 480 init/callback/shutdown, missing-App-ID failure, native and mock fixture runs, and editor-plugin parsing on 26.3. Official 26.3 has headless editor shutdown defects reproducible without this addon; see [PORT_STATUS.md](PORT_STATUS.md). Linux runtime support on 26.3 has not been verified.
- The first editor session after installing the addon needs a restart before tool scripts can use the Steam classes by name; this avoids a Redot editor documentation crash at shutdown. Current qualification remains incomplete. Beta, RC and stable-prep are BLOCKED until their suites and evidence checks are implemented; Beta also requires private Steam app access. See [PORT_STATUS.md](PORT_STATUS.md).

## Required user-supplied dependency

RedotSteam does not redistribute Valve's Steamworks SDK. Supply Steamworks SDK 1.65 when building, either at `godotsteam/sdk/` or through `steam_sdk_path=<path>`. SDK headers, import libraries, and runtime libraries are ignored by Git and excluded from public packages.

The public [Steamworks SDK documentation](https://partner.steamgames.com/doc/sdk) describes SDK access and integration. You are responsible for complying with Valve's terms when obtaining and using it.

## Clone and verify

```bash
git clone --recurse-submodules https://github.com/dominicbytes/redot-steam.git
cd redot-steam
python scripts/generate_parity_inventory.py --check
python scripts/verify_port.py
```

See [REDOTSTEAM.md](REDOTSTEAM.md) for build commands and SDK provisioning.

## Upstream and licensing

The authoritative upstream is [GodotSteam on Codeberg](https://codeberg.org/godotsteam/godotsteam), pinned in [UPSTREAM_LOCK.md](UPSTREAM_LOCK.md). Its original README is preserved as [UPSTREAM_README.md](UPSTREAM_README.md). RedotSteam retains the upstream MIT license and contributor attribution; see [license.md](license.md) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Notes

I vibe coded this in GPT Sol 5.6. Use at your own risk. PR's welcome to fix errors.

## About Dominic Bytes

Greetings! I am Dominic Bytes, the synth walker. I hail from the distant future. Where brains occupy robot bodies, time travel is a trip to the corner store, and the neon glow of our attire is powered by the light of our souls. Join me on a 1.21 gigawatt powered journey of chill vibes with gaming, anime, movies, and more!

- [Website](https://dominicbytes.carrd.co/)
- [X](https://x.com/DominicBytes)
- [Twitch](https://www.twitch.tv/dominicbytes)
- [YouTube](http://www.youtube.com/@DominicBytes)
- [Kick](https://kick.com/dominicbytes)
