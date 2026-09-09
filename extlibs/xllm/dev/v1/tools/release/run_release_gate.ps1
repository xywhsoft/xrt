param(
    [string]$OutputDir = "build\release_bundle",
    [switch]$RunDownstream,
    [switch]$SkipStaticAnalysis,
    [switch]$SkipSmoke
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
Set-Location $repoRoot

function Invoke-Checked($Name, [string[]]$Command) {
    Write-Host "[release-gate] $Name"
    & $Command[0] $Command[1..($Command.Count - 1)]
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

Invoke-Checked "verify version" @("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\verify_release_version.ps1", "-RootDir", $repoRoot)
Invoke-Checked "singlehead" @("cmd.exe", "/c", "build.bat", "singlehead")

if (-not $SkipStaticAnalysis) {
    Invoke-Checked "static analysis" @("cmd.exe", "/c", "build.bat", "static-analysis")
}

if (-not $SkipSmoke) {
    Invoke-Checked "core/session smoke" @("cmd.exe", "/c", "build.bat", "smoke", "-OutputDir", "build\release_gate_smoke_core_session", "-Filter", "core,session")
}

Invoke-Checked "release bundle" @("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\build_release_bundle.ps1", "-OutputDir", $OutputDir, "-VerifyCompile")

if ($RunDownstream) {
    Invoke-Checked "downstream smoke" @("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\run_downstream_integration_smoke.ps1", "-RootDir", $OutputDir, "-OutputDir", "build\release_gate_downstream")
}

Write-Host "[release-gate] ok"
