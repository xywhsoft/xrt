param(
    [switch]$FailOnMissing
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root "xwork.h"
$apiDir = Join-Path $root "docs\api"

if (-not (Test-Path $headerPath)) {
    throw "Missing header: $headerPath"
}

if (-not (Test-Path $apiDir)) {
    throw "Missing API docs directory: $apiDir"
}

$header = Get-Content -Raw -Path $headerPath
$matches = [regex]::Matches($header, "XWORK_API\s+[^;]+?\b(xwork_[A-Za-z0-9_]+)\s*\(")
$functions = @($matches | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)

$functionSet = New-Object "System.Collections.Generic.HashSet[string]"
foreach ($name in $functions) {
    [void]$functionSet.Add($name)
}

$covered = New-Object "System.Collections.Generic.HashSet[string]"
$apiFiles = Get-ChildItem -Path $apiDir -Filter "*.md" -File | Where-Object {
    $_.Name -notin @("README.md", "README.en.md", "API_PAGE_TEMPLATE.md", "API_PAGE_TEMPLATE.en.md")
}

foreach ($file in $apiFiles) {
    $lines = Get-Content -Path $file.FullName
    foreach ($line in $lines) {
        if ($line -match "^###\s+`?(xwork_[A-Za-z0-9_]+)`?\s*$") {
            if ($functionSet.Contains($Matches[1])) {
                [void]$covered.Add($Matches[1])
            }
        }
    }
}

$missing = @($functions | Where-Object { -not $covered.Contains($_) })

Write-Host "xwork API reference coverage"
Write-Host ("Header functions : {0}" -f $functions.Count)
Write-Host ("Covered headings : {0}" -f $covered.Count)
Write-Host ("Missing headings : {0}" -f $missing.Count)

if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "Missing functions:"
    foreach ($name in $missing) {
        Write-Host ("- {0}" -f $name)
    }
}

if ($FailOnMissing -and $missing.Count -gt 0) {
    exit 1
}
