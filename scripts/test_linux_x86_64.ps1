[CmdletBinding()]
param(
    [string]$DockerPath = "docker",
    [string]$Image = "redotsteam-linux-runtime:26.2",
    [string]$RedotBinary = ".work/redot-linux/engine/redot.linuxbsd.editor.x86_64",
    [switch]$BuildImage
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "evidence_text.ps1")
$privatePaths = @($projectRoot, $RedotBinary, $DockerPath)
$redotHostPath = Join-Path $projectRoot $RedotBinary
$pluginBinary = Join-Path $projectRoot "bin/addons/godotsteam/linux64/libgodotsteam.linux.template_debug.x86_64.so"
$fixtureDirectory = Join-Path $projectRoot "tests/fixture/addons/godotsteam/linux64"
$steamRuntime = Join-Path $fixtureDirectory "libsteam_api.so"
if (-not (Test-Path -LiteralPath $redotHostPath -PathType Leaf)) { throw "Linux Redot binary not found." }
if (-not (Test-Path -LiteralPath $pluginBinary -PathType Leaf)) { throw "Linux x86_64 RedotSteam debug binary not found." }
if (-not (Test-Path -LiteralPath $steamRuntime -PathType Leaf)) { throw "Private Linux Steam runtime not provisioned in the fixture." }

Copy-Item -LiteralPath $pluginBinary -Destination (Join-Path $fixtureDirectory (Split-Path -Leaf $pluginBinary)) -Force

if ($BuildImage) {
    & $DockerPath build --file (Join-Path $PSScriptRoot "Dockerfile.linux-runtime") --tag $Image $projectRoot 2>&1 | ConvertTo-EvidenceText -PrivatePaths $privatePaths
    if ($LASTEXITCODE -ne 0) { throw "Failed to build Linux runtime test image." }
}

$mount = "${projectRoot}:/src"
$command = "chmod +x '$RedotBinary' && timeout 45s env LD_LIBRARY_PATH=/src/tests/fixture/addons/godotsteam/linux64 '$RedotBinary' --headless --audio-driver Dummy --path tests/fixture --quit-after 20"
python (Join-Path $PSScriptRoot "run_fixture.py") --marker REDOTSTEAM_FIXTURE -- $DockerPath run --rm --platform linux/amd64 --volume $mount --workdir /src $Image bash -lc $command 2>&1 | ConvertTo-EvidenceText -PrivatePaths $privatePaths
if ($LASTEXITCODE -ne 0) { throw "Linux x86_64 native fixture failed with exit code $LASTEXITCODE." }
