[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SdkPath,
    [Parameter(Mandatory = $true)]
    [string]$AddonPath,
    [ValidateSet("windows-x86_64", "linux-x86_64", "linux-arm64", "macos", "android-arm64")]
    [string]$Target = "windows-x86_64"
)

$ErrorActionPreference = "Stop"
$map = @{
    "windows-x86_64" = @("redistributable_bin/win64/steam_api64.dll", "win64/steam_api64.dll")
    "linux-x86_64"   = @("redistributable_bin/linux64/libsteam_api.so", "linux64/libsteam_api.so")
    "linux-arm64"    = @("redistributable_bin/linuxarm64/libsteam_api.so", "linuxarm64/libsteam_api.so")
    "macos"          = @("redistributable_bin/osx/libsteam_api.dylib", "osx/libsteam_api.dylib")
    "android-arm64"  = @("redistributable_bin/androidarm64/libsteam_api.so", "androidarm64/libsteam_api.so")
}
$sourceRelative, $destinationRelative = $map[$Target]
$source = Join-Path $SdkPath $sourceRelative
$destination = Join-Path $AddonPath $destinationRelative
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Steamworks runtime not found: $source"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
Copy-Item -LiteralPath $source -Destination $destination -Force
Get-FileHash -Algorithm SHA256 -LiteralPath $destination
