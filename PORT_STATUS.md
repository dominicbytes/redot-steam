# GodotSteam / RedotSteam

Port status for Redot 26.2.

## Current state

- Native implementation and Alpha qualification: in progress
- Disposition: Alpha passed; Beta blocked on a private Steam app
- Authoritative upstream: https://codeberg.org/godotsteam/godotsteam
- Source branch: `gdextension`
- Upstream commit: `8e0b976d0ae02a23c35daecf32c6b7bfeba0da08`
- Selected source baseline: Codeberg `gdextension` commit `8e0b976d0ae02a23c35daecf32c6b7bfeba0da08` (GodotSteam 4.21)
- Redot target: `26.2.stable.official.4f5b14aba`; binary, extension API, and interface hashes locked
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
- Native evidence: Windows x86_64, Linux x86_64, and Linux arm64 debug/release builds pass; matching Redot 26.2 native fixtures load/unload and exit cleanly
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
- Alpha evidence: PASS against SDK-free archive SHA-256 `15144421AEF6BE9DDBFD75238547459F59DA0DDF421EC1E0F598C21F70D34CD5`
- Next steps: Beta private-app suite, then clean-runner RC reproducibility and GitHub publication

The tracked upstream README is preserved unchanged as `UPSTREAM_README.md`. Port-specific status lives here.

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
