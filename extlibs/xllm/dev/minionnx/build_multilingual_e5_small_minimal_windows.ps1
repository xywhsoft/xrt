param(
    [string]$OrtRepo = "",
    [string]$AssetsDir = "$PSScriptRoot\build\multilingual-e5-small",
    [switch]$PrepareAssets,
    [switch]$PrintOnly
)

$ErrorActionPreference = "Stop"

if ($PrepareAssets) {
    & "$PSScriptRoot\prepare_multilingual_e5_small_minimal_assets.ps1" -OutputDir $AssetsDir
    if (-not $?) {
        throw "prepare_multilingual_e5_small_minimal_assets.ps1 failed."
    }
}

$modelDir = Join-Path $AssetsDir "ort"
$typedConfigPath = Join-Path $modelDir "model.required_operators_and_types.config"

if (-not (Test-Path $typedConfigPath)) {
    throw "Could not find typed config at $typedConfigPath. Run with -PrepareAssets first."
}

if ($PrintOnly) {
    & "$PSScriptRoot\build_minimal_windows.ps1" -OrtRepo $OrtRepo -ModelDir $modelDir -ConfigPath $typedConfigPath -PrintOnly
} else {
    & "$PSScriptRoot\build_minimal_windows.ps1" -OrtRepo $OrtRepo -ModelDir $modelDir -ConfigPath $typedConfigPath
}
if (-not $?) {
    throw "build_minimal_windows.ps1 failed."
}
