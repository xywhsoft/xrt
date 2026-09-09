param(
    [string]$RootDir = "build\release_bundle",
    [switch]$VerifyCompile
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
Set-Location $repoRoot

function Fail($Message) { throw "[verify-artifact] $Message" }

function Invoke-Checked($Name, [string[]]$Command, $Cwd) {
    Write-Host "[verify-artifact] $Name"
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

function Test-Checksums($Root, $ChecksumFile) {
    $rootFull = [System.IO.Path]::GetFullPath($Root)
    foreach ($line in Get-Content -LiteralPath $ChecksumFile) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^([0-9a-fA-F]{64})\s+\s*(.+)$') {
            Fail "invalid checksum row in $ChecksumFile`: $line"
        }
        $expected = $Matches[1].ToLowerInvariant()
        $rel = $Matches[2] -replace '/', '\'
        $path = Join-Path $rootFull $rel
        if (-not (Test-Path -LiteralPath $path)) {
            Fail "checksum target missing: $rel"
        }
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
        if ($actual -ne $expected) {
            Fail "checksum mismatch: $rel"
        }
    }
}

$root = (Resolve-Path -LiteralPath $RootDir).Path
$zipPath = Join-Path $root "xllm-windows.zip"
$metadataPath = Join-Path $root "release_metadata.json"
$rootSumsPath = Join-Path $root "SHA256SUMS.txt"

foreach ($path in @($zipPath, $metadataPath, $rootSumsPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        Fail "required artifact missing: $path"
    }
}

$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
$version = (Get-Content -LiteralPath (Join-Path $repoRoot "VERSION") -Raw).Trim()
if ($metadata.version -ne $version) {
    Fail "metadata version $($metadata.version) does not match repo VERSION $version"
}
if ($metadata.bundle_file -ne "xllm-windows.zip") {
    Fail "metadata bundle_file must be xllm-windows.zip"
}

Test-Checksums $root $rootSumsPath

$extractRoot = Join-Path $root "_verify_extract"
if (Test-Path -LiteralPath $extractRoot) {
    Remove-Item -LiteralPath $extractRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
Expand-Archive -LiteralPath $zipPath -DestinationPath $extractRoot -Force

foreach ($path in @("include\xllm.h", "include\xllm-session.h", "include\xllm-memory.h", "src", "lib\xrt.h", "lib\sqlite\sqlite3.c", "BUNDLE_SHA256SUMS.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $extractRoot $path))) {
        Fail "bundle content missing: $path"
    }
}
Test-Checksums $extractRoot (Join-Path $extractRoot "BUNDLE_SHA256SUMS.txt")

if ($VerifyCompile) {
    $coreSource = Join-Path $extractRoot "verify_core.c"
    @'
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "include/xllm.h"
#include <stdio.h>
int main(void) {
    xllm_runtime *pRuntime = NULL;
    printf("%s\n", xllm_version());
    if (xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime) {
        return 1;
    }
    xllm_runtime_destroy(pRuntime);
    return 0;
}
'@ | Set-Content -LiteralPath $coreSource -Encoding ASCII
    Invoke-Checked "compile core consumer" @("gcc", "-std=c11", "-Wall", "-Wextra", "-I.", "-Iinclude", "-Ilib", "verify_core.c", "-o", "verify_core.exe", "-lws2_32", "-liphlpapi", "-lshell32", "-lcrypt32") $extractRoot
    Invoke-Checked "run core consumer" @(".\verify_core.exe") $extractRoot

    $memorySource = Join-Path $extractRoot "verify_memory.c"
    @'
#define XRT_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#include "include/xllm-memory.h"
int main(void) {
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tOptions;
    if (xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime) {
        return 1;
    }
    xllm_memory_options_init(&tOptions);
    tOptions.sNamespace = "verify-memory";
    tOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    if (xllm_memory_create(pRuntime, &tOptions, &pMemory) != XRT_NET_OK || !pMemory) {
        xllm_runtime_destroy(pRuntime);
        return 2;
    }
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
'@ | Set-Content -LiteralPath $memorySource -Encoding ASCII
    Invoke-Checked "compile memory consumer" @("gcc", "-std=c11", "-Wall", "-Wextra", "-I.", "-Iinclude", "-Ilib", "-Ilib\sqlite", "-Ilib\onnxruntime", "verify_memory.c", "lib\sqlite\sqlite3.c", "-o", "verify_memory.exe", "-lws2_32", "-liphlpapi", "-lshell32", "-lcrypt32") $extractRoot
    Invoke-Checked "run memory consumer" @(".\verify_memory.exe") $extractRoot
}

Write-Host "[verify-artifact] ok $root"
