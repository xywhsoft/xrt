param(
    [string]$Destination = "$PSScriptRoot\build\onnxruntime-src",
    [string]$Tag = "v1.24.4"
)

$ErrorActionPreference = "Stop"

$resolvedDestination = [System.IO.Path]::GetFullPath($Destination)
$parentDir = Split-Path $resolvedDestination -Parent
New-Item -ItemType Directory -Force $parentDir | Out-Null

if (-not (Test-Path $resolvedDestination)) {
    git clone --depth 1 --recursive --shallow-submodules --branch $Tag https://github.com/microsoft/onnxruntime.git $resolvedDestination
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed with exit code $LASTEXITCODE"
    }
} else {
    Push-Location $resolvedDestination
    try {
        git fetch --tags origin
        if ($LASTEXITCODE -ne 0) {
            throw "git fetch failed with exit code $LASTEXITCODE"
        }
        git checkout $Tag
        if ($LASTEXITCODE -ne 0) {
            throw "git checkout $Tag failed with exit code $LASTEXITCODE"
        }
        git submodule update --init --recursive --depth 1
        if ($LASTEXITCODE -ne 0) {
            throw "git submodule update failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
}

Write-Host "ONNX Runtime source is ready at $resolvedDestination"
