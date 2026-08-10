[CmdletBinding()]
param(
    [string]$RedotBin = $env:REDOT_BIN,
    [string]$OutputDirectory = "redot-api/26.2"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not $RedotBin) {
    $RedotBin = Join-Path (Split-Path -Parent (Split-Path -Parent $projectRoot)) ".tools/redot/26.2-stable-windows/redot.windows.editor.x86_64.console.exe"
}
if (-not (Test-Path -LiteralPath $RedotBin -PathType Leaf)) {
    throw "Redot editor not found. Set REDOT_BIN or pass -RedotBin."
}

$captureRoot = Join-Path $projectRoot ".work/redot-api-capture"
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
$captureProject = Join-Path $captureRoot "project.godot"
Set-Content -LiteralPath $captureProject -Encoding UTF8 -Value @(
    "[application]",
    'config/name="Redot API Capture"',
    "",
    "[rendering]",
    'renderer/rendering_method="gl_compatibility"'
)

Push-Location $captureRoot
try {
    & $RedotBin --headless --editor --path . --dump-gdextension-interface
    if ($LASTEXITCODE -ne 0) { throw "Redot interface dump failed with exit code $LASTEXITCODE" }
    & $RedotBin --headless --editor --path . --dump-extension-api
    if ($LASTEXITCODE -ne 0) { throw "Redot API dump failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}

$destination = Join-Path $projectRoot $OutputDirectory
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath (Join-Path $captureRoot "extension_api.json") -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $captureRoot "gdextension_interface.h") -Destination $destination -Force
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $destination "extension_api.json"), (Join-Path $destination "gdextension_interface.h")
