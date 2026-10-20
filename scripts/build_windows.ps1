[CmdletBinding()]
param(
    [ValidateSet("template_debug", "template_release")]
    [string]$Target = "template_debug",
    [string]$SdkPath = "godotsteam/sdk",
    # Both supported engines use the pinned 26.2 binding ABI.
    [string]$RedotApiDir = "redot-api/26.2",
    [string]$TargetPath = "bin",
    [switch]$IncludeSteamRuntime,
    [int]$Jobs = 4
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$python = if ($env:REDOTSTEAM_PYTHON) { $env:REDOTSTEAM_PYTHON } else { "python" }
$localPython = Join-Path $projectRoot ".work/python"
$localCompiler = Join-Path $projectRoot ".work/toolchain/winlibs/mingw64/bin"

if (Test-Path -LiteralPath $localPython) { $env:PYTHONPATH = $localPython }
if (Test-Path -LiteralPath $localCompiler) { $env:PATH = "$localCompiler;$env:PATH" }

Push-Location $projectRoot
try {
    $runtime = if ($IncludeSteamRuntime) { "yes" } else { "no" }
    $sconsArgs = @(
        "-m", "SCons",
        "platform=windows", "arch=x86_64", "target=$Target",
        "redot_api_dir=$RedotApiDir", "steam_sdk_path=$SdkPath",
        "target_path=$TargetPath", "target_name=godotsteam",
        "include_steam_runtime=$runtime", "-j$Jobs"
    )
    & $python @sconsArgs
    if ($LASTEXITCODE -ne 0) { throw "RedotSteam build failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}
