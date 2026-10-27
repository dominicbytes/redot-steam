# GodotSteam / RedotSteam

Port status for Redot 26.2 and 26.3.

## Behavior changes

- Audit item HARD-MP-015: a forced local disconnect emits no peer_disconnected signal; retiring a connection discards its queued packets. Remote and graceful disconnects still report the peer.

## Current state

- Alpha qualification: incomplete for the current candidate
- Disposition: historical Alpha evidence recorded; current candidate is not release-qualified
- Beta, RC and stable-prep: BLOCKED until their suites and evidence checks are implemented; Beta also requires private Steam app access
- Authoritative upstream: https://codeberg.org/godotsteam/godotsteam
- Source branch: `gdextension`
- Upstream commit: `8e0b976d0ae02a23c35daecf32c6b7bfeba0da08`
- Selected source baseline: Codeberg `gdextension` commit `8e0b976d0ae02a23c35daecf32c6b7bfeba0da08` (GodotSteam 4.21)
- Binding ABI baseline: `26.2.stable.official.4f5b14aba`; extension API and interface hashes remain locked
- Additional Windows runtime: `26.3.stable.official.85e520394` (x86_64 debug/release compatibility smoke verified)
- Intended public downstream: https://github.com/dominicbytes/redot-steam
- Stable release scope: full practical GodotSteam parity on Redot
- Required platforms: Windows and Linux
- Tracked best-effort platforms: macOS and Android
- Required architectures: Windows x86_64; Linux x86_64; Linux arm64
- Tracked best-effort architectures: Windows x86_32; Linux x86_32
- Stable artifacts: source plus validated binaries for every required architecture
- Tracked artifacts: binaries publish only after equivalent validation
- Initial Redot support: Redot 26.2 LTS
- Update policy: certify each later Redot version through a tested RedotSteam compatibility release
- Active maintenance: current Redot LTS plus latest stable
- Historical maintenance: certified binaries remain downloadable; fixes are best effort
- Steamworks SDK policy: one exact supported version/hash per release; upgrades rerun all gates
- Initial SDK target: Steamworks SDK 1.65; the mirror is used only as an ignored local code/build oracle and excluded from packages
- Historical Redot 26.2 native evidence: Windows x86_64, Linux x86_64, and Linux arm64 debug/release builds pass; matching Redot 26.2 native fixtures load/unload and exit cleanly
- Steam Web API boundary: optional separately named public HTTP integration only; it does not replace native Steam client APIs
- Public Steam test profile: App ID 480
- Full-parity Steam test profile: private app; configuration and credentials remain outside Git
- Release tag scheme: `v<GodotSteam>-redot<Redot>.<port revision>`; conventional prerelease suffixes for previews
- Release cadence: event-driven with no fixed calendar schedule
- Downloadable package: one universal `redotsteam-<tag>-addon.zip` rooted at `addons/godotsteam/`
- CI trust boundary: public-source checks on GitHub-hosted ephemeral runners; restricted builds/private tests through a separate approval-gated private pipeline on clean disposable runners
- Release-output boundary: only sanitized artifacts, checksums/manifests, and scrubbed evidence cross from the private pipeline
- Artifact verification: every addon ZIP requires SHA-256, a machine-readable build/input manifest, and a GitHub artifact attestation
- Platform code signing: deferred until certificate ownership and private-key management are separately approved
- Release qualification: exactly four stages—Alpha -> Beta -> RC -> Stable—with a complete GodotSteam-vs-RedotSteam Gauntlet at each stage
- Promotion safety: a failed gate never publishes or advances; immutable build-input changes reset the candidate to Alpha
- Stable promotion: automatically publish the exact unchanged RC addon ZIP after the final complete required-target Gauntlet passes on independent clean disposable runners with valid verification assets and zero unresolved P0-P2 defects
- API parity: exact across the three native classes, extension manifest, and documentation inventory
- Historical Alpha evidence: recorded PASS against SDK-free archive SHA-256 `15144421AEF6BE9DDBFD75238547459F59DA0DDF421EC1E0F598C21F70D34CD5`; does not qualify the current candidate
- Next steps: complete current Alpha qualification and implement private-app, reproducibility and independent promotion suites with evidence validation

The tracked upstream README is preserved unchanged as `UPSTREAM_README.md`. Port-specific status lives here.

## Redot 26.3 compatibility

The same Windows x86_64 debug/release DLLs use the checked-in 26.2 binding ABI on both engines. Fresh 26.2 dumps match the snapshots byte for byte. The 26.3 interface adds `GDEXTENSION_VARIANT_TYPE_STRUCT` (39); the existing type IDs and interface functions are unchanged. All classes in `build_profile.json`, multiplayer/packet peer virtuals, ProjectSettings and the editor APIs used by `plugin/` retain their existing signatures and hashes; `OS.get_real_path()` is an unused addition. Raw 26.3 bindings are not adopted because the pinned godot-cpp Variant lacks Struct support and official 26.3 provides neither Struct converter.

Windows verification covers native class/singleton registration, App ID 480 initialization/callback pumping/shutdown, missing-App-ID initialization failure, peer construction/release, mock fixtures, editor-plugin parsing/enablement after import and restart, and clean game-process exit on official 26.2, official 26.3 and an unofficial 26.3 build with an engine shutdown fix. Official 26.3 editor shutdown crashes/RID leaks also occur in no-addon controls. They do not occur during the fixture game runs. On 26.2 and 26.3 the editor crashed at shutdown after first discovering any extension that exposes classes, because a queued documentation update ran after the documentation database was freed. When the editor discovers the addon while running, the classes are registered without exposure for that session and become fully available after a restart. `tests/editor_import/run.py` covers cold import, restart and game startup.

Native behavior without a running Steam client has not been verified. Mock failure and native missing-App-ID failure were verified instead. These checks do not certify live multiplayer, private-app behavior, Linux runtime behavior on 26.3, macOS or Android, or full release qualification. Historical 26.2 Alpha evidence above is separate from this compatibility smoke.

## Multiplayer peer admission divergence

Remote peer IDs must be positive signed 32-bit values, differ from the local ID,
and be unused. On clients, only the Steam identity selected by `create_client()`
may claim authority ID 1; hosts never admit a remote ID 1. Rejected connections
are closed without `peer_connected`. Receive delivery and disconnect callbacks
must match the connection that owns the admitted ID, including later packets
in the same receive batch. Valid non-authority mesh connections remain supported.

Offline native regressions: `python tests/native/run_tests.py` from a C++ compiler
environment. These compile production method bodies with test doubles; they do
not initialize Steam or verify real-client symmetric connection negotiation.

## Receive queue divergence

`steam/multiplayer_peer/max_queued_packets` defaults to 256 retained incoming
messages per `SteamMultiplayerPeer`, shared across all its connections. Polling
requests at most the remaining queue capacity (and at most 255 messages per
call), leaving excess messages with Steam's own flow control. A full queue
pauses reception, including handshakes, until the application drains packets.
Values below 1 are clamped to 1 and values above the signed 32-bit maximum are
clamped to that maximum. Lowering the limit preserves existing queued messages
and pauses reception until they drain below the new limit.

The last packet borrowed by `get_packet()` is retained separately until the next
read or close. Closing or destroying the peer releases it and every queued
message exactly once. Offline regressions cover slow consumers, resumed
reception, minimum capacity, close, and destruction without initializing Steam.

## Channel bounds divergence

`SteamPacketPeer::send()` accepts every channel in `0 <= channel < max_channels`,
including channel 3 with the default four lanes. Negative channels and channels
at or above the configured lane count now report an error and return
`ERR_INVALID_PARAMETER` before allocating or sending a message. Invalid channels
are no longer silently redirected to lane 0.

Offline native regressions: `python tests/native/test_lane_bounds.py` from a C++
compiler environment. These compile the production send method with transport
doubles and verify all valid lanes plus lower and upper invalid boundaries for
one, four, and eight lanes. They do not initialize Steam or verify live transport.
