param(
    [switch]$WithVsCMake,
    [switch]$PrintOnly
)

$ErrorActionPreference = "Stop"

$winget = Get-Command winget -ErrorAction Stop

$vsArgs = @(
    "--wait"
    "--passive"
    "--norestart"
    "--add Microsoft.VisualStudio.Workload.VCTools"
    "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    "--add Microsoft.VisualStudio.Component.Windows11SDK.26100"
)

if ($WithVsCMake) {
    $vsArgs += "--add Microsoft.VisualStudio.Component.VC.CMake.Project"
}

$vsOverride = $vsArgs -join " "
$vsCommand = "winget install --exact --id Microsoft.VisualStudio.2022.BuildTools --accept-source-agreements --accept-package-agreements --override `"$vsOverride`""

if ($WithVsCMake) {
    $cmakeCommand = $null
} else {
    $cmakeCommand = "winget install --exact --id Kitware.CMake --accept-source-agreements --accept-package-agreements"
}

Write-Host "Recommended minimal MSVC environment install commands:"
Write-Host $vsCommand
if ($cmakeCommand) {
    Write-Host $cmakeCommand
}

if ($PrintOnly) {
    return
}

Invoke-Expression $vsCommand
if (-not $?) {
    throw "Visual Studio Build Tools installation failed."
}

if ($cmakeCommand) {
    Invoke-Expression $cmakeCommand
    if (-not $?) {
        throw "CMake installation failed."
    }
}

Write-Host ""
Write-Host "Installation commands completed. Open a new terminal before checking cl/cmake."
