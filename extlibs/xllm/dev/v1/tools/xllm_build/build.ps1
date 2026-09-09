param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..\..")).Path
Set-Location $repoRoot

function Show-Usage {
    Write-Host "xllm build/test entry"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  build.bat singlehead"
    Write-Host "  build.bat smoke [-Filter <case-or-group>] [-OutputDir <dir>] [-ListCases]"
    Write-Host "  build.bat smoke-list"
    Write-Host "  build.bat memory-eval [-OutputDir <dir>] [-BaselineReport <json>]"
    Write-Host "  build.bat memory-bench [-OutputDir <dir>]"
    Write-Host "  build.bat context-packer-eval [-OutputDir <dir>]"
    Write-Host "  build.bat session-summary-bench [-OutputDir <dir>]"
    Write-Host "  build.bat static-analysis [static analysis options]"
    Write-Host "  build.bat probe [-CaseFilter <case>] [-OutputDir <dir>]"
    Write-Host "  build.bat release-bundle [release bundle options]"
    Write-Host "  build.bat release-gate [release gate options] [-RunDownstream]"
    Write-Host "  build.bat release-gate-options-test [test options]"
    Write-Host "  build.bat downstream-smoke [downstream integration smoke options]"
    Write-Host "  build.bat verify-version"
    Write-Host "  build.bat verify-artifact [verify artifact options]"
    Write-Host ""
    Write-Host "Aliases:"
    Write-Host "  build     -> singlehead"
    Write-Host "  test      -> smoke"
}

function Invoke-Checked {
    param(
        [string]$Name,
        [string[]]$Command
    )

    & $Command[0] $Command[1..($Command.Count - 1)]
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

if (-not $Arguments -or $Arguments.Count -eq 0) {
    Show-Usage
    exit 0
}

$command = $Arguments[0].ToLowerInvariant()
$rest = @()
if ($Arguments.Count -gt 1) {
    $rest = @($Arguments[1..($Arguments.Count - 1)])
}

switch ($command) {
    "build" {
        Invoke-Checked -Name "singlehead" -Command (@("cmd.exe", "/c", "tools\single_head\build_single_head.bat") + $rest)
    }
    "singlehead" {
        Invoke-Checked -Name "singlehead" -Command (@("cmd.exe", "/c", "tools\single_head\build_single_head.bat") + $rest)
    }
    "smoke" {
        Invoke-Checked -Name "smoke matrix" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\smoke\run_smoke_matrix.ps1") + $rest)
    }
    "test" {
        Invoke-Checked -Name "smoke matrix" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\smoke\run_smoke_matrix.ps1") + $rest)
    }
    "smoke-list" {
        Invoke-Checked -Name "smoke matrix list" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\smoke\run_smoke_matrix.ps1", "-ListCases") + $rest)
    }
    "memory-eval" {
        Invoke-Checked -Name "memory retrieval eval" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\memory\run_memory_eval.ps1") + $rest)
    }
    "memory-bench" {
        Invoke-Checked -Name "memory benchmark" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\memory\run_memory_benchmark.ps1") + $rest)
    }
    "memory-benchmark" {
        Invoke-Checked -Name "memory benchmark" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\memory\run_memory_benchmark.ps1") + $rest)
    }
    "context-packer-eval" {
        Invoke-Checked -Name "context packer eval" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\context\run_context_packer_eval.ps1") + $rest)
    }
    "context-eval" {
        Invoke-Checked -Name "context packer eval" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\context\run_context_packer_eval.ps1") + $rest)
    }
    "session-summary-bench" {
        Invoke-Checked -Name "session summary benchmark" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\session\run_session_summary_benchmark.ps1") + $rest)
    }
    "session-summary-benchmark" {
        Invoke-Checked -Name "session summary benchmark" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\eval\session\run_session_summary_benchmark.ps1") + $rest)
    }
    "static-analysis" {
        Invoke-Checked -Name "static analysis" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\static_analysis\run_static_analysis.ps1") + $rest)
    }
    "probe" {
        Invoke-Checked -Name "real provider probe matrix" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests\probe\run_real_provider_probe_matrix.ps1") + $rest)
    }
    "release-bundle" {
        Invoke-Checked -Name "release bundle" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\build_release_bundle.ps1") + $rest)
    }
    "release-gate" {
        Invoke-Checked -Name "release gate" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\run_release_gate.ps1") + $rest)
    }
    "release-gate-options-test" {
        Invoke-Checked -Name "release gate option tests" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\test_release_gate_options.ps1") + $rest)
    }
    "downstream-smoke" {
        Invoke-Checked -Name "downstream integration smoke" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\run_downstream_integration_smoke.ps1") + $rest)
    }
    "downstream-integration-smoke" {
        Invoke-Checked -Name "downstream integration smoke" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\run_downstream_integration_smoke.ps1") + $rest)
    }
    "verify-version" {
        Invoke-Checked -Name "verify release version" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\verify_release_version.ps1", "-RootDir", $repoRoot) + $rest)
    }
    "verify-artifact" {
        Invoke-Checked -Name "verify release artifact" -Command (@("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\verify_release_artifact.ps1") + $rest)
    }
    default {
        Show-Usage
        throw "unknown build command: $command"
    }
}
