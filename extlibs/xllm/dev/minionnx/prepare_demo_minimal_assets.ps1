param(
    [string]$PyDepsDir = "$PSScriptRoot\build\pydeps",
    [string]$CorpusPath = "$PSScriptRoot\..\onnxdemo\demo_corpus.txt",
    [string]$GeneratorScript = "$PSScriptRoot\..\onnxdemo\tools\generate_demo_assets.py",
    [string]$ModelDir = "$PSScriptRoot\..\onnxdemo\build\model"
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

$python = Get-Command python -ErrorAction Stop
New-Item -ItemType Directory -Force $PyDepsDir | Out-Null
New-Item -ItemType Directory -Force $ModelDir | Out-Null

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

Invoke-Step "Generate toy text embedding ONNX model" {
    python $GeneratorScript --corpus $CorpusPath --output-dir $ModelDir --embed-dim 16
}

$manualConfigPath = Join-Path $ModelDir "manual_required_ops.config"
Invoke-Step "Generate manual reduced operator config" {
    python "$PSScriptRoot\generate_reduced_ops_config.py" --model "$ModelDir\toy_text_embedder.onnx" --output $manualConfigPath
}

$ortOutputDir = Join-Path $ModelDir "ort"
New-Item -ItemType Directory -Force $ortOutputDir | Out-Null
$typedConfigPath = Join-Path $ortOutputDir "toy_text_embedder.required_operators_and_types.config"

try {
    Invoke-Step "Convert ONNX model to ORT format and emit typed config" {
        python -m onnxruntime.tools.convert_onnx_models_to_ort --output_dir $ortOutputDir --optimization_style Fixed --enable_type_reduction "$ModelDir\toy_text_embedder.onnx"
    }
} catch {
    Write-Warning "ORT conversion failed. The ONNX model and manual config are still available."
    Write-Warning $_
}

Write-Host ""
Write-Host "Model assets ready:"
Write-Host "  ONNX  : $ModelDir\toy_text_embedder.onnx"
Write-Host "  ORT   : $ortOutputDir\toy_text_embedder.ort"
Write-Host "  Vocab : $ModelDir\vocab.txt"
Write-Host "  Config: $manualConfigPath"
Write-Host "  Typed : $typedConfigPath"
