[CmdletBinding()]
param(
    [ValidateSet("alpha", "beta", "rc", "stable-prep")]
    [string]$Stage = "alpha",
    [string]$RedotBin = $env:REDOT_BIN,
    [string]$SdkPath = "godotsteam/sdk",
    [string]$DockerPath = "docker",
    [string]$Artifact,
    [string]$EvidenceRoot = "artifacts/gauntlet"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$upstreamCommit = "8e0b976d0ae02a23c35daecf32c6b7bfeba0da08"
$env:GIT_CONFIG_COUNT = "1"
$env:GIT_CONFIG_KEY_0 = "safe.directory"
$env:GIT_CONFIG_VALUE_0 = $projectRoot
if (-not $RedotBin) {
    $RedotBin = Join-Path (Split-Path -Parent (Split-Path -Parent $projectRoot)) ".tools/redot/26.2-stable-windows/redot.windows.editor.x86_64.exe"
}
if (-not [System.IO.Path]::IsPathRooted($SdkPath)) { $SdkPath = Join-Path $projectRoot $SdkPath }
if (-not [System.IO.Path]::IsPathRooted($EvidenceRoot)) { $EvidenceRoot = Join-Path $projectRoot $EvidenceRoot }
$stageRoot = Join-Path $EvidenceRoot $Stage
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

$results = [System.Collections.Generic.List[object]]::new()
function Invoke-Check {
    param([string]$Name, [scriptblock]$Command)
    $log = Join-Path $stageRoot "$Name.log"
    try {
        $output = & $Command 2>&1 | Out-String
        $output | Set-Content -LiteralPath $log -Encoding utf8
        if ($LASTEXITCODE -ne 0) { throw "$Name exited with $LASTEXITCODE" }
        $results.Add([ordered]@{ name = $Name; status = "PASS"; log = $log })
    } catch {
        $_ | Out-String | Add-Content -LiteralPath $log -Encoding utf8
        $results.Add([ordered]@{ name = $Name; status = "FAIL"; log = $log; reason = $_.Exception.Message })
    }
}

Push-Location $projectRoot
try {
    Invoke-Check "source-lock" { & (Join-Path $PSScriptRoot "verify_source_lock.ps1") -RedotBin $RedotBin -SdkPath $SdkPath }
    Invoke-Check "upstream-parity" { python scripts/generate_parity_inventory.py --check }
    Invoke-Check "source" { python scripts/verify_port.py }
    Invoke-Check "binaries" { python scripts/verify_binaries.py }
    Invoke-Check "mock-runtime" { & $RedotBin --headless --path tests/mock-fixture --quit-after 120 }
    Invoke-Check "windows-native-runtime" { & $RedotBin --headless --path tests/fixture --quit-after 120 }
    Invoke-Check "linux-x86_64-native-runtime" { & (Join-Path $PSScriptRoot "test_linux_x86_64.ps1") -DockerPath $DockerPath }
    Invoke-Check "linux-arm64-native-runtime" { & (Join-Path $PSScriptRoot "test_linux_arm64.ps1") -DockerPath $DockerPath }

    if (-not $Artifact) {
        $Artifact = Get-ChildItem -LiteralPath (Join-Path $projectRoot "artifacts/packages") -Filter "redotsteam-*-addon.zip" -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if ($Artifact) {
        Invoke-Check "package" { python scripts/verify_package.py $Artifact }
    } else {
        $results.Add([ordered]@{ name = "package"; status = "FAIL"; reason = "No addon ZIP was supplied or discovered" })
    }

    $blocked = $null
    if ($Stage -in @("beta", "rc", "stable-prep") -and -not $env:REDOTSTEAM_PRIVATE_APP_ID) {
        $blocked = "REDOTSTEAM_PRIVATE_APP_ID and the private full-parity environment are unavailable"
    }
    if ($Stage -in @("rc", "stable-prep") -and -not $env:REDOTSTEAM_REPRO_EVIDENCE) {
        $blocked = "Two-runner reproducibility evidence is unavailable"
    }
    if ($Stage -eq "stable-prep" -and -not $env:REDOTSTEAM_INDEPENDENT_EVIDENCE) {
        $blocked = "Independent clean-runner repeat evidence for the unchanged RC artifact is unavailable"
    }

    $failed = @($results | Where-Object status -eq "FAIL")
    $status = if ($failed.Count) { "FAIL" } elseif ($blocked) { "BLOCKED" } else { "PASS" }
    $gate = [ordered]@{
        schema = 1
        project = "RedotSteam"
        stage = $Stage
        status = $status
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        downstream_commit = (git rev-parse HEAD)
        upstream_commit = $upstreamCommit
        redot = (& $RedotBin --version | Select-Object -Last 1)
        sdk_in_package = $false
        artifact = $Artifact
        blocked_reason = $blocked
        checks = $results
    }
    $gatePath = Join-Path $stageRoot "gate-$Stage.json"
    $gate | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $gatePath -Encoding utf8
    Write-Output "REDOTSTEAM_GAUNTLET_STAGE=$Stage"
    Write-Output "REDOTSTEAM_GAUNTLET_STATUS=$status"
    Write-Output "REDOTSTEAM_GAUNTLET_EVIDENCE=$gatePath"
    if ($status -eq "FAIL") { exit 1 }
    if ($status -eq "BLOCKED") { exit 2 }
} finally {
    Pop-Location
}
