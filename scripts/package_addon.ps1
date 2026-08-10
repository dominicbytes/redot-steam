[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [string]$BuildRoot = "bin/addons/godotsteam",
    [string]$OutputDirectory = "artifacts/packages"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot $BuildRoot
if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Build output not found: $source" }

$output = Join-Path $projectRoot $OutputDirectory
$packageRoot = Join-Path $projectRoot ".work/package"
$staging = Join-Path $packageRoot "addons/godotsteam"
if (Test-Path -LiteralPath $packageRoot) {
    $resolvedPackageRoot = (Resolve-Path -LiteralPath $packageRoot).Path
    $expectedRoot = (Join-Path $projectRoot ".work/package")
    if ($resolvedPackageRoot -ne $expectedRoot) { throw "Refusing to clear unexpected package staging path: $resolvedPackageRoot" }
    Remove-Item -LiteralPath $resolvedPackageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staging | Out-Null

Get-ChildItem -LiteralPath $source -Recurse -File | Where-Object {
    $_.Name -notin @("steam_api.dll", "steam_api64.dll", "libsteam_api.so", "libsteam_api.dylib") -and
    $_.Extension -notin @(".exp", ".lib", ".a")
} | ForEach-Object {
    $relative = $_.FullName.Substring($source.Length).TrimStart("/", "\")
    $destination = Join-Path $staging $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
}

$forbidden = Get-ChildItem -LiteralPath (Split-Path -Parent (Split-Path -Parent $staging)) -Recurse -File | Where-Object {
    $_.Name -in @("steam_api.h", "steam_api.dll", "steam_api64.dll", "steam_api.lib", "steam_api64.lib", "libsteam_api.so", "libsteam_api.dylib")
}
if ($forbidden) { throw "Restricted Steamworks files entered package staging" }

New-Item -ItemType Directory -Force -Path $output | Out-Null
$archive = Join-Path $output "redotsteam-$Version-addon.zip"
Compress-Archive -LiteralPath (Join-Path (Split-Path -Parent (Split-Path -Parent $staging)) "addons") -DestinationPath $archive -Force
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $archive
"$($hash.Hash)  $(Split-Path -Leaf $archive)" | Set-Content -LiteralPath (Join-Path $output "SHA256SUMS") -Encoding ascii
$hash
