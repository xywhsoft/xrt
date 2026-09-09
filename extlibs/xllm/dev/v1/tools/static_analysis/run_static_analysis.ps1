param(
    [switch]$SkipCompile,
    [switch]$SkipSmokeList,
    [switch]$SkipDiffCheck,
    [switch]$SkipBoundaryCheck,
    [switch]$SkipReleaseOptionTests
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
Set-Location $repoRoot

function Invoke-ProcessChecked {
    param(
        [string]$Name,
        [string[]]$Command
    )

    Write-Host ("[static] {0}" -f $Name)
    & $Command[0] $Command[1..($Command.Count - 1)]
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Invoke-GccSyntaxOnly {
    param(
        [string]$Name,
        [string]$Source
    )

    $args = @(
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-fsyntax-only",
        "-I.",
        "-Ilib",
        "-Ilib\sqlite",
        "-Ilib\onnxruntime",
        "-DXRT_IMPLEMENTATION",
        "-DXLLM_IMPLEMENTATION",
        $Source,
        "lib\sqlite\sqlite3.c",
        "-lws2_32",
        "-liphlpapi",
        "-lshell32",
        "-lcrypt32"
    )
    Invoke-ProcessChecked -Name ("gcc syntax-only {0}" -f $Name) -Command (@("gcc") + $args)
}

function Test-MemoryInternalBoundary {
    $rootMemoryPath = Join-Path $repoRoot "src\xllm_memory\xllm_memory.c"
    $matches = Select-String -LiteralPath $rootMemoryPath -Pattern "^\s*XLLM_API\b" -ErrorAction Stop
    if ($matches) {
        $details = ($matches | ForEach-Object { "{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim() }) -join [Environment]::NewLine
        throw "memory internal boundary violation: public XLLM_API entrypoints must live in focused xllm_memory_* modules, not src\xllm_memory\xllm_memory.c`n$details"
    }
}

if (-not $SkipDiffCheck) {
    Invoke-ProcessChecked -Name "git diff --check" -Command @("git", "diff", "--check")
}

if (-not $SkipBoundaryCheck) {
    Write-Host "[static] memory internal boundary"
    Test-MemoryInternalBoundary
}

if (-not $SkipSmokeList) {
    Invoke-ProcessChecked -Name "smoke case enumeration" -Command @(
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        "tests\smoke\run_smoke_matrix.ps1",
        "-ListCases"
    )
}

if (-not $SkipReleaseOptionTests) {
    Invoke-ProcessChecked -Name "release gate option tests" -Command @(
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        "tools\release\test_release_gate_options.ps1"
    )
}

if (-not $SkipCompile) {
    Invoke-GccSyntaxOnly -Name "memory_benchmark" -Source "tests\eval\memory\memory_benchmark.c"
    Invoke-GccSyntaxOnly -Name "context_packer_eval" -Source "tests\eval\context\context_packer_eval.c"
    Invoke-GccSyntaxOnly -Name "session_summary_benchmark" -Source "tests\eval\session\session_summary_benchmark.c"
    Invoke-GccSyntaxOnly -Name "resource_cleanup" -Source "examples\smoke_resource_cleanup.c"
    Invoke-GccSyntaxOnly -Name "log_event_taxonomy" -Source "examples\smoke_log_event_taxonomy.c"
    Invoke-GccSyntaxOnly -Name "session_state_options" -Source "examples\smoke_session_state_options.c"
    Invoke-GccSyntaxOnly -Name "session_tool_compact_regression" -Source "examples\smoke_session_tool_compact_regression.c"
    Invoke-GccSyntaxOnly -Name "memory_task_type" -Source "examples\smoke_memory_task_type.c"
    Invoke-GccSyntaxOnly -Name "memory_fact_preference_type" -Source "examples\smoke_memory_fact_preference_type.c"
    Invoke-GccSyntaxOnly -Name "memory_compact_conversation" -Source "examples\smoke_memory_compact_conversation.c"
    Invoke-GccSyntaxOnly -Name "memory_concurrent_ingest_search" -Source "examples\smoke_memory_concurrent_ingest_search.c"
    Invoke-GccSyntaxOnly -Name "memory_search_apply_turn" -Source "examples\smoke_memory_search_apply_turn.c"
    Invoke-GccSyntaxOnly -Name "task_memory_example" -Source "examples\task_memory\task_memory.c"
    Invoke-GccSyntaxOnly -Name "memory_ingest_progress" -Source "examples\smoke_memory_ingest_progress.c"
    Invoke-GccSyntaxOnly -Name "memory_workspace_status" -Source "examples\smoke_memory_workspace_status.c"
    Invoke-GccSyntaxOnly -Name "memory_crash_consistency" -Source "examples\smoke_memory_crash_consistency.c"
    Invoke-GccSyntaxOnly -Name "memory_health_check" -Source "examples\smoke_memory_health_check.c"
    Invoke-GccSyntaxOnly -Name "memory_retrieval_debug" -Source "examples\smoke_memory_retrieval_debug.c"
    Invoke-GccSyntaxOnly -Name "memory_workspace_long_run" -Source "examples\smoke_memory_workspace_long_run.c"
}

Write-Host "[static] ok"
