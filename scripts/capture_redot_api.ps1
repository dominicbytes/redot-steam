[CmdletBinding()]
param(
    [string]$RedotBin = $env:REDOT_BIN,
    # Dumps are inspection inputs; do not overwrite the pinned binding ABI by default.
    [string]$OutputDirectory = ".work/redot-api-capture/output"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $RedotBin -or -not (Test-Path -LiteralPath $RedotBin -PathType Leaf)) {
    throw "Redot editor not found. Set REDOT_BIN or pass -RedotBin."
}
$RedotBin = (Resolve-Path -LiteralPath $RedotBin).Path
$captureRoot = Join-Path $projectRoot ".work/redot-api-capture"
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

foreach ($dump in @("extension-api", "gdextension-interface")) {
    $process = Start-Process -FilePath $RedotBin -WorkingDirectory $captureRoot -WindowStyle Hidden -PassThru -ArgumentList @(
        "--headless", "--audio-driver", "Dummy", "--quit-after", "2", "--dump-$dump"
    )
    if (-not $process.WaitForExit(60000)) {
        $process.Kill()
        throw "Redot $dump dump timed out."
    }
    if ($process.ExitCode -ne 0) { throw "Redot $dump dump failed with exit code $($process.ExitCode)" }
}

$destination = if ([System.IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $projectRoot $OutputDirectory }
$api = Get-Content -LiteralPath (Join-Path $captureRoot "extension_api.json") -Raw | ConvertFrom-Json
$pinned = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "redot-api/26.2"))
if ([System.IO.Path]::GetFullPath($destination) -eq $pinned -and
    ($api.redot_header.version_major -ne 26 -or $api.redot_header.version_minor -ne 2)) {
    throw "Only a Redot 26.2 dump may replace the pinned 26.2 binding ABI. Inspect 26.3 dumps separately."
}
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath (Join-Path $captureRoot "extension_api.json") -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $captureRoot "gdextension_interface.h") -Destination $destination -Force
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $destination "extension_api.json"), (Join-Path $destination "gdextension_interface.h")
