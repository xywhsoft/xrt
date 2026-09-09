param(
    [string]$OrtRepo = "",
    [string]$ModelDir = "$PSScriptRoot\..\onnxdemo\build\model\ort",
    [string]$ConfigPath = "",
    [switch]$PrepareDemoAssets,
    [switch]$PrintOnly
)

$ErrorActionPreference = "Stop"

if ($PrepareDemoAssets) {
    & "$PSScriptRoot\prepare_demo_minimal_assets.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "prepare_demo_minimal_assets.ps1 failed with exit code $LASTEXITCODE"
    }
}

if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $typedConfig = Join-Path $ModelDir "toy_text_embedder.required_operators_and_types.config"
    $manualConfig = Join-Path (Split-Path $ModelDir -Parent) "manual_required_ops.config"
    if (Test-Path $typedConfig) {
        $ConfigPath = $typedConfig
    } elseif (Test-Path $manualConfig) {
        $ConfigPath = $manualConfig
    } else {
        throw "Could not find a reduced operator config. Run prepare_demo_minimal_assets.ps1 first."
    }
}

if ([string]::IsNullOrWhiteSpace($OrtRepo)) {
    if ($PrintOnly) {
        $OrtRepo = "<ORT_REPO>"
    } else {
        throw "Provide -OrtRepo pointing at an ONNX Runtime source checkout."
    }
}

$buildScript = Join-Path $OrtRepo "build.bat"
if (-not (Test-Path $buildScript) -and -not $PrintOnly) {
    throw "Could not find build.bat under $OrtRepo"
}

$cmake = Get-Command cmake -ErrorAction SilentlyContinue

$commandParts = @(
    "`"$buildScript`""
    "--update"
    "--build"
    "--config MinSizeRel"
    "--build_shared_lib"
    "--skip_tests"
    "--parallel"
    "--targets onnxruntime"
    "--minimal_build"
    "--disable_ml_ops"
    "--disable_exceptions"
    "--disable_rtti"
    "--include_ops_by_config `"$ConfigPath`""
    "--enable_reduced_operator_type_support"
)

$command = $commandParts -join " "

Write-Host "Suggested minimal runtime build command:"
Write-Host $command

if ($PrintOnly) {
    return
}

if (-not $cmake) {
    Write-Warning "cmake is not available on this machine. The command above was not executed."
    return
}

cmd /c $command
if ($LASTEXITCODE -ne 0) {
    throw "ONNX Runtime minimal build failed with exit code $LASTEXITCODE"
}
