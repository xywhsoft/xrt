param(
    [Parameter(Mandatory = $true)]
    [string]$ModelPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,
    [string]$PyDepsDir = "$PSScriptRoot\build\pydeps"
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [string]$Description,
        [scriptblock]$Script
    )

    Write-Host "==> $Description"
    & $Script
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

$resolvedModelPath = (Resolve-Path $ModelPath).Path
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$python = Get-Command python -ErrorAction Stop

New-Item -ItemType Directory -Force $PyDepsDir | Out-Null
New-Item -ItemType Directory -Force $resolvedOutputDir | Out-Null

$onnxMarker = Join-Path $PyDepsDir "onnx"
$ortMarker = Join-Path $PyDepsDir "onnxruntime"
if (-not (Test-Path $onnxMarker) -or -not (Test-Path $ortMarker)) {
    Invoke-Step "Install local Python build dependencies" {
        python -m pip install --index-url https://pypi.org/simple --target $PyDepsDir onnx==1.18.0 onnxruntime==1.24.4
    }
}

if ($env:PYTHONPATH) {
    $env:PYTHONPATH = "$PyDepsDir;$env:PYTHONPATH"
} else {
    $env:PYTHONPATH = $PyDepsDir
}

$copiedModelPath = Join-Path $resolvedOutputDir ([System.IO.Path]::GetFileName($resolvedModelPath))
Copy-Item -Force $resolvedModelPath $copiedModelPath

$manualConfigPath = Join-Path $resolvedOutputDir "$([System.IO.Path]::GetFileNameWithoutExtension($resolvedModelPath)).required_ops.config"
Invoke-Step "Generate manual reduced operator config" {
    python "$PSScriptRoot\generate_reduced_ops_config.py" --model $copiedModelPath --output $manualConfigPath
}

$ortOutputDir = Join-Path $resolvedOutputDir "ort"
New-Item -ItemType Directory -Force $ortOutputDir | Out-Null
Invoke-Step "Convert ONNX model to ORT format and emit typed config" {
    python -m onnxruntime.tools.convert_onnx_models_to_ort --output_dir $ortOutputDir --optimization_style Fixed --enable_type_reduction $copiedModelPath
}

$typedConfigPath = Join-Path $ortOutputDir "$([System.IO.Path]::GetFileNameWithoutExtension($resolvedModelPath)).required_operators_and_types.config"

Write-Host ""
Write-Host "Model-specific minimal-build assets ready:"
Write-Host "  ONNX  : $copiedModelPath"
Write-Host "  ORT   : $ortOutputDir\$([System.IO.Path]::GetFileNameWithoutExtension($resolvedModelPath)).ort"
Write-Host "  Config: $manualConfigPath"
Write-Host "  Typed : $typedConfigPath"
