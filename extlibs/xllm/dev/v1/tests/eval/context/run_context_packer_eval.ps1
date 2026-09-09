param(
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..\..")).Path
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build\context_packer_eval"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $repoRoot $OutputDir
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "build") | Out-Null

$exePath = Join-Path $repoRoot "build\context_packer_eval.exe"

& gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -Ilib\onnxruntime -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION `
    tests\eval\context\context_packer_eval.c `
    lib\sqlite\sqlite3.c `
    -o $exePath `
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if ($LASTEXITCODE -ne 0) {
    throw "context packer eval build failed with exit code $LASTEXITCODE"
}

& $exePath $OutputDir
if ($LASTEXITCODE -ne 0) {
    throw "context packer eval failed with exit code $LASTEXITCODE"
}
