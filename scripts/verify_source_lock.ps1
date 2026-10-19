[CmdletBinding()]
param(
    [string]$RedotBin = $env:REDOT_BIN,
    [string]$SdkPath = "godotsteam/sdk"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$upstreamCommit = "8e0b976d0ae02a23c35daecf32c6b7bfeba0da08"
if (-not $RedotBin -or -not (Test-Path -LiteralPath $RedotBin -PathType Leaf)) {
    throw "Set REDOT_BIN or pass -RedotBin for the engine being verified."
}
$engineHashes = @{
    "26.2.stable.official.4f5b14aba" = @(
        "10DFBEFC273536F0C65903D6429E531C30133820BF0FA842F2D6E6FB81DB1D42",
        "5633D02A28A73514084DF6A60FFE01FABDBBB9AC5E28FDFD590ED47277F51989"
    )
    "26.3.stable.official.85e520394" = @(
        "F89D39A9AFCAFE8BBBF8519435B2150F35BE9BB104559FCA9E3F7EA247DAF9B6",
        "40072F7BCA52BB5D663157241116B5C089A3BC7675BA6779D8A92E687643B2E0"
    )
}
$sdkRoot = if ([System.IO.Path]::IsPathRooted($SdkPath)) { $SdkPath } else { Join-Path $projectRoot $SdkPath }
$utilsHeader = Join-Path $sdkRoot "public/steam/isteamutils.h"
$godotCppRoot = Join-Path $projectRoot "godot-cpp"

$checks = [ordered]@{
    redot_binary = "<REDOT_BIN>"
    binding_abi = "redot-api/26.2"
    redot_version = (& $RedotBin --version | Select-Object -Last 1)
    redot_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $RedotBin).Hash
    extension_api_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot "redot-api/26.2/extension_api.json")).Hash
    gdextension_interface_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot "redot-api/26.2/gdextension_interface.h")).Hash
    downstream_commit = (git -C $projectRoot rev-parse HEAD)
    upstream_commit = $upstreamCommit
    godot_cpp_commit = (git -c "safe.directory=$godotCppRoot" -C $godotCppRoot rev-parse HEAD)
    steamworks_header_present = Test-Path -LiteralPath (Join-Path $sdkRoot "public/steam/steam_api.h")
    steamworks_165_features = (Select-String -LiteralPath $utilsHeader -Pattern "IsRunningOnSteamHardware" -Quiet -ErrorAction SilentlyContinue) -and (Select-String -LiteralPath $utilsHeader -Pattern "IsRunningUnderProton" -Quiet -ErrorAction SilentlyContinue)
}
$checks | ConvertTo-Json -Depth 3

if (-not $checks.steamworks_header_present) { throw "Steamworks SDK headers are missing" }
if (-not $checks.steamworks_165_features) { throw "Steamworks SDK does not expose the expected 1.65 ISteamUtils surface" }
if ($checks.godot_cpp_commit -ne "6388e26dd8a42071f65f764a3ef3d9523dda3d6e") { throw "godot-cpp pin mismatch" }
if (-not $engineHashes.ContainsKey($checks.redot_version) -or $checks.redot_sha256 -notin $engineHashes[$checks.redot_version]) { throw "Redot binary version/hash mismatch" }
if ($checks.extension_api_sha256 -ne "177E7796166929B2193C9CCE2FD32F59601A0147D0D1E7FE904B94E8F69F6577") { throw "Redot API hash mismatch" }
