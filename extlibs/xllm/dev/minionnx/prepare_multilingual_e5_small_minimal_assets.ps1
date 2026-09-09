param(
    [string]$ModelPath = "$PSScriptRoot\..\embedbench\build\real_onnx\model_cache\WiseIntelligence__multilingual-e5-small-Optimum-ONNX-Quantized-AVX2\model.onnx",
    [string]$OutputDir = "$PSScriptRoot\build\multilingual-e5-small",
    [string]$PyDepsDir = "$PSScriptRoot\build\pydeps"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ModelPath)) {
    throw "Could not find multilingual-e5-small ONNX model at $ModelPath"
}

& "$PSScriptRoot\prepare_model_minimal_assets.ps1" -ModelPath $ModelPath -OutputDir $OutputDir -PyDepsDir $PyDepsDir
if (-not $?) {
    throw "prepare_model_minimal_assets.ps1 failed."
}
