param(
    [string]$OutputDir = "build\release_bundle",
    [switch]$VerifyCompile
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
Set-Location $repoRoot

function Invoke-Checked($Name, [string[]]$Command) {
    Write-Host "[release-bundle] $Name"
    & $Command[0] $Command[1..($Command.Count - 1)]
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Assert-BuildPath($Path) {
    $full = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
    $buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build"))
    if (-not $full.StartsWith($buildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "output path must stay under build: $full"
    }
    return $full
}

function Write-Checksums($Root, $OutFile) {
    $rootFull = [System.IO.Path]::GetFullPath($Root)
    $rows = Get-ChildItem -LiteralPath $rootFull -Recurse -File |
        Where-Object { $_.FullName -ne [System.IO.Path]::GetFullPath($OutFile) } |
        Sort-Object FullName |
        ForEach-Object {
            $rel = $_.FullName.Substring($rootFull.Length).TrimStart('\') -replace '\\','/'
            $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
            "$hash  $rel"
        }
    Set-Content -LiteralPath $OutFile -Value $rows -Encoding ASCII
}

$outRoot = Assert-BuildPath $OutputDir
$packageRoot = Join-Path $outRoot "package"
$zipPath = Join-Path $outRoot "xllm-windows.zip"
$metadataPath = Join-Path $outRoot "release_metadata.json"
$rootSumsPath = Join-Path $outRoot "SHA256SUMS.txt"

if (Test-Path -LiteralPath $outRoot) {
    Remove-Item -LiteralPath $outRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

Invoke-Checked "verify version" @("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\verify_release_version.ps1", "-RootDir", $repoRoot)

$includeRoot = Join-Path $packageRoot "include"
New-Item -ItemType Directory -Force -Path $includeRoot | Out-Null
Copy-Item -LiteralPath "xllm.h","xllm-session.h","xllm-memory.h","xllm-memory-bridge.h" -Destination $includeRoot -Force
Copy-Item -LiteralPath "xllm.c","xllm-session.c","xllm-memory.c","xllm-memory-bridge.h","VERSION","README.md","README.en.md","RELEASE_NOTES.md" -Destination $packageRoot -Force
Copy-Item -LiteralPath "src" -Destination $packageRoot -Recurse -Force
Copy-Item -LiteralPath "docs" -Destination $packageRoot -Recurse -Force
Copy-Item -LiteralPath "lib" -Destination $packageRoot -Recurse -Force

$nativeOrt = "build\onnxruntime_nupkg\runtimes\win-x64\native"
if (Test-Path -LiteralPath $nativeOrt) {
    $bundleOrt = Join-Path $packageRoot "lib\onnxruntime"
    New-Item -ItemType Directory -Force -Path $bundleOrt | Out-Null
    Copy-Item -LiteralPath (Join-Path $nativeOrt "onnxruntime.dll") -Destination $bundleOrt -Force
    Copy-Item -LiteralPath (Join-Path $nativeOrt "onnxruntime_providers_shared.dll") -Destination $bundleOrt -Force
}

Write-Checksums $packageRoot (Join-Path $packageRoot "BUNDLE_SHA256SUMS.txt")

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $zipPath -Force

$version = (Get-Content -LiteralPath "VERSION" -Raw).Trim()
$metadata = [ordered]@{
    name = "xllm"
    version = $version
    platform = "windows"
    bundle_file = "xllm-windows.zip"
    generated_at = (Get-Date).ToUniversalTime().ToString("o")
    checksum_file = "SHA256SUMS.txt"
    bundle_checksum_file = "BUNDLE_SHA256SUMS.txt"
    layout = @{
        include = "include"
        src = "src"
        lib = "lib"
    }
}
$metadata | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metadataPath -Encoding ASCII
Write-Checksums $outRoot $rootSumsPath

if ($VerifyCompile) {
    Invoke-Checked "verify artifact" @("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\release\verify_release_artifact.ps1", "-RootDir", $outRoot, "-VerifyCompile")
}

Write-Host "[release-bundle] ok $outRoot"
