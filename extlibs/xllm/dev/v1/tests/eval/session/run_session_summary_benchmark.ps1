param(
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..\..")).Path
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build\session_summary_benchmark"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $repoRoot $OutputDir
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "build") | Out-Null

$exePath = Join-Path $repoRoot "build\session_summary_benchmark.exe"

& gcc -std=c11 -Wall -Wextra -I. -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION `
    tests\eval\session\session_summary_benchmark.c `
    -o $exePath `
    -lws2_32 -liphlpapi -lshell32

if ($LASTEXITCODE -ne 0) {
    throw "session summary benchmark build failed with exit code $LASTEXITCODE"
}

& $exePath $OutputDir
if ($LASTEXITCODE -ne 0) {
    throw "session summary benchmark failed with exit code $LASTEXITCODE"
}
