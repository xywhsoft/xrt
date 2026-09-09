param(
    [string]$RepoId = "WiseIntelligence/multilingual-e5-small-Optimum-ONNX-Quantized-AVX2",
    [string]$ModelCacheDir = "$PSScriptRoot\..\embedbench\build\real_onnx\model_cache",
    [string]$OutputDir = "$PSScriptRoot\build\multilingual-e5-small",
    [string]$PyDepsDir = "$PSScriptRoot\build\pydeps",
    [string]$OnnxRuntimeVersion = "1.24.1",
    [string]$OnnxRuntimeOutputDir = "$PSScriptRoot\..\..\lib\onnxruntime",
    [switch]$SkipMinimalAssets,
    [switch]$PrintOnly
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [string]$Description,
        [scriptblock]$Script
    )

    Write-Host "==> $Description"
    $global:LASTEXITCODE = 0
    & $Script
    if (-not $?) {
        throw "$Description failed."
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

function Get-SlugifiedRepoId {
    param([string]$InputRepoId)

    return $InputRepoId.Replace("/", "__")
}

function Test-UsablePython {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pythonCommand) {
        return $false
    }

    cmd /c "python --version >nul 2>nul"
    return ($LASTEXITCODE -eq 0)
}

function Invoke-DownloadFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Uri,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $parentDir = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parentDir)) {
        New-Item -ItemType Directory -Force $parentDir | Out-Null
    }

    Write-Host "    -> $Uri"
    Invoke-WebRequest -Uri $Uri -OutFile $Path
}

$python = if (Test-UsablePython) { Get-Command python -ErrorAction SilentlyContinue } else { $null }
$resolvedCacheRoot = [System.IO.Path]::GetFullPath($ModelCacheDir)
$slug = Get-SlugifiedRepoId -InputRepoId $RepoId
$resolvedRepoDir = Join-Path $resolvedCacheRoot $slug
$modelOnnxPath = Join-Path $resolvedRepoDir "model.onnx"
$resolvedOrtDir = [System.IO.Path]::GetFullPath($OnnxRuntimeOutputDir)
$ortDllPath = Join-Path $resolvedOrtDir "onnxruntime.dll"

Write-Host "repo id        : $RepoId"
Write-Host "cache root     : $resolvedCacheRoot"
Write-Host "repo dir       : $resolvedRepoDir"
Write-Host "model onnx     : $modelOnnxPath"
Write-Host "ort version    : $OnnxRuntimeVersion"
Write-Host "ort output dir : $resolvedOrtDir"
Write-Host "output dir     : $OutputDir"
Write-Host "python deps    : $PyDepsDir"
Write-Host "skip minimal   : $($SkipMinimalAssets.IsPresent)"

if ($PrintOnly) {
    Write-Host ""
    Write-Host "This script will:"
    Write-Host "  1. install huggingface_hub into a local target dir if needed"
    Write-Host "  2. download the model assets into:"
    Write-Host "     $resolvedRepoDir"
    Write-Host "  3. download the ONNX Runtime CPU DLL into:"
    Write-Host "     $ortDllPath"
    if (-not $SkipMinimalAssets) {
        Write-Host "  4. generate minimal-build assets by calling:"
        Write-Host "     prepare_multilingual_e5_small_minimal_assets.ps1"
    }
    exit 0
}

New-Item -ItemType Directory -Force $PyDepsDir | Out-Null
New-Item -ItemType Directory -Force $resolvedCacheRoot | Out-Null
New-Item -ItemType Directory -Force $resolvedRepoDir | Out-Null
New-Item -ItemType Directory -Force $resolvedOrtDir | Out-Null

if (-not (Test-Path $ortDllPath)) {
    $runtimePackagePath = Join-Path $PyDepsDir "Microsoft.ML.OnnxRuntime.$OnnxRuntimeVersion.nupkg"
    $runtimeExtractDir = Join-Path $PyDepsDir "Microsoft.ML.OnnxRuntime.$OnnxRuntimeVersion"

    Invoke-Step "Download ONNX Runtime CPU package" {
        Invoke-DownloadFile `
            -Uri "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/$OnnxRuntimeVersion" `
            -Path $runtimePackagePath
    }

    if (Test-Path $runtimeExtractDir) {
        Remove-Item -Recurse -Force $runtimeExtractDir
    }
    Expand-Archive -Path $runtimePackagePath -DestinationPath $runtimeExtractDir
    Copy-Item -Force (Join-Path $runtimeExtractDir "runtimes\win-x64\native\onnxruntime.dll") $ortDllPath
}

if ($python) {
    $hfHubMarker = Join-Path $PyDepsDir "huggingface_hub"
    if (-not (Test-Path $hfHubMarker)) {
        Invoke-Step "Install local Python download dependency" {
            python -m pip install --index-url https://pypi.org/simple --target $PyDepsDir huggingface_hub>=0.31.0
        }
    }

    if ($env:PYTHONPATH) {
        $env:PYTHONPATH = "$PyDepsDir;$env:PYTHONPATH"
    } else {
        $env:PYTHONPATH = $PyDepsDir
    }

    Invoke-Step "Download multilingual-e5-small repo snapshot" {
        python -c @"
from huggingface_hub import snapshot_download
snapshot_download(
    repo_id=r'''$RepoId''',
    local_dir=r'''$resolvedRepoDir''',
    local_dir_use_symlinks=False
)
"@
    }
} else {
    $repoBaseUrl = "https://huggingface.co/$RepoId/resolve/main"
    $requiredFiles = @(
        "model.onnx",
        "tokenizer.json",
        "tokenizer_config.json",
        "sentencepiece.bpe.model"
    )

    Invoke-Step "Download multilingual-e5-small required files (Python unavailable)" {
        foreach ($leaf in $requiredFiles) {
            $outPath = Join-Path $resolvedRepoDir $leaf
            if (-not (Test-Path $outPath)) {
                Invoke-DownloadFile -Uri "$repoBaseUrl/$leaf" -Path $outPath
            }
        }
    }
}

if (-not (Test-Path $modelOnnxPath)) {
    throw "Downloaded repo does not contain model.onnx at $modelOnnxPath"
}

if (-not $SkipMinimalAssets -and $python) {
    & "$PSScriptRoot\prepare_multilingual_e5_small_minimal_assets.ps1" -ModelPath $modelOnnxPath -OutputDir $OutputDir -PyDepsDir $PyDepsDir
    if (-not $?) {
        throw "prepare_multilingual_e5_small_minimal_assets.ps1 failed."
    }
} elseif (-not $SkipMinimalAssets) {
    Write-Host "Skipping minimal ORT asset generation because Python is unavailable."
}

Write-Host ""
Write-Host "multilingual-e5-small assets are ready."
Write-Host "repo dir   : $resolvedRepoDir"
Write-Host "model onnx : $modelOnnxPath"
Write-Host "runtime dll: $ortDllPath"
