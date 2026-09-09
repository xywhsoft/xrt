param(
    [string]$RootDir = "build\release_bundle",
    [string]$OutputDir = "build\downstream_smoke"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
Set-Location $repoRoot

function Invoke-Checked($Name, [string[]]$Command, $Cwd) {
    Write-Host "[downstream-smoke] $Name"
    Push-Location $Cwd
    try {
        & $Command[0] $Command[1..($Command.Count - 1)]
        if ($LASTEXITCODE -ne 0) {
            throw "$Name failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
}

$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $RootDir))
if (-not (Test-Path -LiteralPath (Join-Path $releaseRoot "xllm-windows.zip"))) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File "tools\release\build_release_bundle.ps1" -OutputDir $releaseRoot -VerifyCompile
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$outRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
if (Test-Path -LiteralPath $outRoot) {
    Remove-Item -LiteralPath $outRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null
Expand-Archive -LiteralPath (Join-Path $releaseRoot "xllm-windows.zip") -DestinationPath $outRoot -Force

$source = Join-Path $outRoot "downstream_smoke.c"
@'
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "include/xllm-memory.h"
#include <string.h>
int main(void) {
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tOptions;
    xllm_memory_ingest_options tIngest;
    xllm_memory_search_options tSearch;
    xllm_memory_search_result tResult;
    xllm_error tError;
    int rc;

    xllm_error_init(&tError);
    xllm_memory_options_init(&tOptions);
    xllm_memory_ingest_options_init(&tIngest);
    xllm_memory_search_options_init(&tSearch);
    memset(&tResult, 0, sizeof(tResult));

    if (xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime) {
        return 1;
    }
    tOptions.sNamespace = "downstream-smoke";
    tOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    rc = xllm_memory_create(pRuntime, &tOptions, &pMemory);
    if (rc != XRT_NET_OK || !pMemory) {
        xllm_runtime_destroy(pRuntime);
        return 2;
    }
    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sRecordId = "downstream";
    tIngest.sText = "downstream release smoke validates memory ingest and search from bundled files";
    if (xllm_memory_ingest_text(pMemory, &tIngest, &tError) != XRT_NET_OK) {
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 3;
    }
    tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearch.sQuery = "release smoke memory search";
    tSearch.uMaxHits = 2u;
    if (xllm_memory_search(pMemory, &tSearch, &tResult, &tError) != XRT_NET_OK || tResult.iHitCount == 0u) {
        xllm_memory_search_result_reset(&tResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }
    xllm_memory_search_result_reset(&tResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
'@ | Set-Content -LiteralPath $source -Encoding ASCII

Invoke-Checked "compile downstream memory app" @("gcc", "-std=c11", "-Wall", "-Wextra", "-I.", "-Iinclude", "-Ilib", "-Ilib\sqlite", "-Ilib\onnxruntime", "downstream_smoke.c", "lib\sqlite\sqlite3.c", "-o", "downstream_smoke.exe", "-lws2_32", "-liphlpapi", "-lshell32", "-lcrypt32") $outRoot
Invoke-Checked "run downstream memory app" @(".\downstream_smoke.exe") $outRoot

$report = [ordered]@{
    success = $true
    release_root = $releaseRoot
    output_dir = $outRoot
    generated_at = (Get-Date).ToString("s")
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outRoot "downstream_integration_report.json") -Encoding ASCII
Set-Content -LiteralPath (Join-Path $outRoot "downstream_integration_report.txt") -Value "success: True" -Encoding ASCII
Write-Host "[downstream-smoke] ok $outRoot"
