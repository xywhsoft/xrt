$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path

$required = @(
    "verify_release_version.ps1",
    "build_release_bundle.ps1",
    "verify_release_artifact.ps1",
    "run_downstream_integration_smoke.ps1",
    "run_release_gate.ps1"
)

foreach ($name in $required) {
    $path = Join-Path $scriptRoot $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "[release-gate-options-test] missing release script: $name"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "RELEASE_NOTES.md"))) {
    throw "[release-gate-options-test] RELEASE_NOTES.md is required"
}

Write-Host "[release-gate-options-test] ok"
