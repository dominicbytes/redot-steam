[CmdletBinding()]
param(
    [string]$DockerPath = "docker",
    [string]$Image = "redotsteam-linux-runtime:26.2-arm64",
    [string]$RedotBinary = ".work/redot-linux-arm64/engine/redot.linuxbsd.editor.arm64",
    [switch]$BuildImage
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$redotHostPath = Join-Path $projectRoot $RedotBinary
$pluginBinary = Join-Path $projectRoot "bin/addons/godotsteam/linuxarm64/libgodotsteam.linux.template_debug.arm64.so"
$fixtureDirectory = Join-Path $projectRoot "tests/fixture/addons/godotsteam/linuxarm64"
$steamRuntime = Join-Path $fixtureDirectory "libsteam_api.so"
if (-not (Test-Path -LiteralPath $redotHostPath -PathType Leaf)) { throw "Linux arm64 Redot binary not found: $redotHostPath" }
if (-not (Test-Path -LiteralPath $pluginBinary -PathType Leaf)) { throw "Linux arm64 RedotSteam debug binary not found: $pluginBinary" }
if (-not (Test-Path -LiteralPath $steamRuntime -PathType Leaf)) { throw "Private Linux arm64 Steam runtime not provisioned in the fixture." }

Copy-Item -LiteralPath $pluginBinary -Destination (Join-Path $fixtureDirectory (Split-Path -Leaf $pluginBinary)) -Force
if ($BuildImage) {
    & $DockerPath build --platform linux/arm64 --file (Join-Path $PSScriptRoot "Dockerfile.linux-runtime") --tag $Image $projectRoot
    if ($LASTEXITCODE -ne 0) { throw "Failed to build Linux arm64 runtime test image." }
}

$mount = "${projectRoot}:/src"
$command = "chmod +x '$RedotBinary' && timeout 45s env LD_LIBRARY_PATH=/src/tests/fixture/addons/godotsteam/linuxarm64 '$RedotBinary' --headless --audio-driver Dummy --path tests/fixture --quit-after 20"
python (Join-Path $PSScriptRoot "run_fixture.py") --marker REDOTSTEAM_FIXTURE -- $DockerPath run --rm --platform linux/arm64 --volume $mount --workdir /src $Image bash -lc $command
if ($LASTEXITCODE -ne 0) { throw "Linux arm64 native fixture failed with exit code $LASTEXITCODE." }
