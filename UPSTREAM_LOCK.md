# Upstream lock: GodotSteam / RedotSteam

| Field | Value |
|---|---|
| Project | GodotSteam / RedotSteam |
| Authoritative host | Codeberg |
| Repository URL | https://codeberg.org/godotsteam/godotsteam |
| Selected branch | gdextension |
| Upstream commit | 8e0b976d0ae02a23c35daecf32c6b7bfeba0da08 |
| Selected baseline | Codeberg `gdextension` commit `8e0b976d0ae02a23c35daecf32c6b7bfeba0da08`, GodotSteam 4.21 |
| Rejected baseline shortcut | v4.5.2-gde is GodotSteam 4.5.2 with `compatibility_minimum = 4.1`; it is not evidence of Godot Engine 4.5.2 compatibility |
| Modern candidate commits | v4.19.1-gde `64e022aca284c30a97846fa10470741267c37c94`; v4.20.1-gde `c693f6345ea19ede29b70d43d2166414be68eb0a`; v4.21-gde `807b7c97b9fa35e33406e38977dd6681725a7b4b` |
| Modern candidate godot-cpp pin | `6388e26dd8a42071f65f764a3ef3d9523dda3d6e` on declared branch `4.4` |
| License | MIT source; Steamworks SDK restricted/proprietary |
| Redot binding ABI baseline | `26.2.stable.official.4f5b14aba`; full commit `4f5b14abade2239104847d03d8f9056e4467cfcd`; Windows x86_64 binary SHA-256 `10DFBEFC273536F0C65903D6429E531C30133820BF0FA842F2D6E6FB81DB1D42` |
| Additional Windows runtime | `26.3.stable.official.85e520394`; commit `85e5203945af15755dd2f66cb6fdf7545fd88c0b`; x86_64 debug/release compatibility smoke using the unchanged 26.2 ABI; Linux 26.3 unverified |
| Redot extension API | `redot-api/26.2/extension_api.json` SHA-256 `177E7796166929B2193C9CCE2FD32F59601A0147D0D1E7FE904B94E8F69F6577` |
| Redot extension interface | `redot-api/26.2/gdextension_interface.h` SHA-256 `4CD695E86B92E2BF4E60BBE19CE137FAF41205DA1CF94F29E069AFEC0F7BF320` |
| Intended public downstream | https://github.com/dominicbytes/redot-steam |
| Stable release objective | Full practical GodotSteam GDExtension parity on Redot; no deliberate API/editor/documentation omissions |
| Release-blocking platforms | Windows, Linux |
| Tracked best-effort platforms | macOS, Android |
| Required architectures | Windows x86_64; Linux x86_64; Linux arm64 |
| Tracked best-effort architectures | Windows x86_32; Linux x86_32 |
| Public binary policy | Stable releases include source plus validated binaries for every required architecture; tracked binaries require equivalent validation |
| Initial Redot target | Redot 26.2 LTS |
| Redot update policy | Add later Redot versions only through explicitly tested RedotSteam compatibility releases |
| Active maintenance window | Current Redot LTS plus latest stable; one line when they coincide |
| Historical Redot support | Certified packages remain downloadable; fixes are best effort and non-blocking |
| Steamworks SDK policy | Pin one exact supported version/hash per release; upgrades rerun the full compatibility and rights matrix |
| Initial Steamworks SDK | 1.65(a), `rlabrecque/SteamworksSDK` mirror, ignored local compile/link oracle only; no SDK file is packaged |
| Documentation oracle | Valve SDK docs and Steam Web API docs; source/mock evidence only, never native proof |
| Wrapper research | SteamworksPy, Steamworks.NET, steamworks-rs, and steamworks4j are pattern references only; Open Steamworks is rejected as an archived reverse-engineered build input |
| Web API boundary | Optional separately named HTTP adapter for genuinely public endpoints only; not a substitute for Steam client initialization, callbacks, overlay, lobbies, networking, voice, or input |
| Steam test-app policy | App ID 480 for public smoke; private app for full parity; private configuration and credentials stay outside Git |
| Downstream tag scheme | `v<GodotSteam>-redot<Redot>.<port revision>`; previews append `-alpha.N`, `-beta.N`, or `-rc.N` |
| Release cadence | Event-driven; no fixed calendar; trigger on meaningful GodotSteam, Redot, Steamworks SDK, security, or critical port-fix changes |
| Downloadable package layout | One `redotsteam-<tag>-addon.zip` rooted at `addons/godotsteam/`; required debug/release binaries included; tracked binaries validation-gated |
| Restricted build runner | Separate private release-control pipeline; approved exact revision only; clean disposable runner; no public PR execution with restricted inputs |
| Public CI boundary | GitHub-hosted ephemeral runners; public-source checks only; no Steamworks SDK or private app credentials |
| Release-output boundary | Transfer only addon ZIP, checksums/manifests, and scrubbed logs/evidence from private build environment |
| Artifact verification | Every addon ZIP requires `SHA256SUMS`, `build-manifest.json`, and a GitHub artifact attestation |
| Manifest identity | Downstream tag/SHA; upstream Codeberg SHA; Redot version/build/API hashes; Steamworks SDK version/hash; toolchain/runner image; targets/configurations; evidence references; artifact hashes |
| Platform code signing | Deferred for initial releases; requires a separate certificate ownership, key custody, rotation, revocation, and renewal policy |
| Release stages | Exactly four: Alpha -> Beta -> RC -> Stable; immutable candidate admission is a prerequisite, not a fifth stage |
| Preview qualification | The release process runs a complete upstream-comparison Gauntlet at Alpha, Beta, and RC before advancing; Stable runs its own final Gauntlet |
| Gauntlet oracle | Selected immutable Codeberg GodotSteam commit in its locked Godot runtime compared scenario-for-scenario with RedotSteam on Redot 26.2 |
| Stable promotion | Exact unchanged RC addon ZIP; final complete required-target Gauntlet on independent clean disposable runners; verification assets/notices pass; zero unresolved P0-P2 |

Complete SDK tree hashes and final binary/package hashes are emitted into each candidate's private build manifest and gauntlet evidence; public artifacts retain identities but exclude SDK files.
