param(
    [string]$RootDir
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RootDir)) {
    $RootDir = (Resolve-Path -LiteralPath (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..\..")).Path
} else {
    $RootDir = (Resolve-Path -LiteralPath $RootDir).Path
}

function Fail($Message) {
    throw "[verify-version] $Message"
}

$versionPath = Join-Path $RootDir "VERSION"
$headerPath = Join-Path $RootDir "xllm.h"
$basePath = Join-Path $RootDir "src\xllm_base\xllm_base.c"
$notesPath = Join-Path $RootDir "RELEASE_NOTES.md"

foreach ($path in @($versionPath, $headerPath, $basePath, $notesPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        Fail "required file missing: $path"
    }
}

$version = (Get-Content -LiteralPath $versionPath -Raw).Trim()
if ($version -notmatch '^(\d+)\.(\d+)\.(\d+)(?:-[0-9A-Za-z.-]+)?$') {
    Fail "VERSION is not a supported semver string: $version"
}
$major = [int]$Matches[1]
$minor = [int]$Matches[2]
$patch = [int]$Matches[3]

$header = Get-Content -LiteralPath $headerPath -Raw
if ($header -notmatch '#define\s+XLLM_VERSION_MAJOR\s+(\d+)') { Fail "XLLM_VERSION_MAJOR missing" }
$headerMajor = [int]$Matches[1]
if ($header -notmatch '#define\s+XLLM_VERSION_MINOR\s+(\d+)') { Fail "XLLM_VERSION_MINOR missing" }
$headerMinor = [int]$Matches[1]
if ($header -notmatch '#define\s+XLLM_VERSION_PATCH\s+(\d+)') { Fail "XLLM_VERSION_PATCH missing" }
$headerPatch = [int]$Matches[1]

if ($major -ne $headerMajor -or $minor -ne $headerMinor -or $patch -ne $headerPatch) {
    Fail "VERSION ($version) does not match xllm.h macros ($headerMajor.$headerMinor.$headerPatch)"
}

$base = Get-Content -LiteralPath $basePath -Raw
$escapedVersion = [regex]::Escape($version)
if ($base -notmatch "return\s+`"$escapedVersion`"\s*;") {
    Fail "xllm_version() return value does not match VERSION ($version)"
}

$notes = Get-Content -LiteralPath $notesPath -Raw
if ($notes -notmatch "(?m)^##\s+$escapedVersion(?:\s|-|$)") {
    Fail "RELEASE_NOTES.md does not contain an entry for $version"
}

Write-Host "[verify-version] ok $version"
