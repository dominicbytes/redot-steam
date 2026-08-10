[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("x86_64", "arm64")]
    [string]$Architecture,
    [ValidateSet("template_debug", "template_release")]
    [string]$Target = "template_debug",
    [Parameter(Mandatory = $true)]
    [string]$ZigPath,
    [string]$SdkPath = "godotsteam/sdk",
    [string]$TargetPath = "bin",
    [string]$PythonPath = $env:REDOTSTEAM_PYTHON,
    [string]$GlibcVersion = "2.27",
    [int]$Jobs = 4,
    [switch]$IncludeSteamRuntime
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $PythonPath) { throw "Set REDOTSTEAM_PYTHON or pass -PythonPath." }
if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) { throw "Python not found: $PythonPath" }
if (-not (Test-Path -LiteralPath $ZigPath -PathType Leaf)) { throw "Zig not found: $ZigPath" }

$runtime = if ($IncludeSteamRuntime) { "yes" } else { "no" }
$arguments = @(
    "-m", "SCons",
    "platform=linux",
    "arch=$Architecture",
    "target=$Target",
    "redot_api_dir=redot-api/26.2",
    "steam_sdk_path=$SdkPath",
    "target_path=$TargetPath",
    "target_name=godotsteam",
    "include_steam_runtime=$runtime",
    "linux_cross_zig=$ZigPath",
    "linux_cross_glibc=$GlibcVersion",
    "use_hot_reload=no",
    "-j$Jobs"
)

Push-Location $projectRoot
try {
    & $PythonPath @arguments
    if ($LASTEXITCODE -ne 0) { throw "Linux $Architecture $Target build failed with exit code $LASTEXITCODE." }
}
finally {
    Pop-Location
}
