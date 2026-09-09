param(
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..\..")).Path
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build\memory_benchmark"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $repoRoot $OutputDir
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "build") | Out-Null

$dbPath = Join-Path $OutputDir "memory_benchmark.db"
Remove-Item -Force -ErrorAction SilentlyContinue -LiteralPath $dbPath
Remove-Item -Force -ErrorAction SilentlyContinue -LiteralPath "$dbPath-wal"
Remove-Item -Force -ErrorAction SilentlyContinue -LiteralPath "$dbPath-shm"

$exePath = Join-Path $repoRoot "build\memory_benchmark.exe"

& gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -Ilib\onnxruntime -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION `
    tests\eval\memory\memory_benchmark.c `
    lib\sqlite\sqlite3.c `
    -o $exePath `
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if ($LASTEXITCODE -ne 0) {
    throw "memory benchmark build failed with exit code $LASTEXITCODE"
}

& $exePath $OutputDir
if ($LASTEXITCODE -ne 0) {
    throw "memory benchmark failed with exit code $LASTEXITCODE"
}
