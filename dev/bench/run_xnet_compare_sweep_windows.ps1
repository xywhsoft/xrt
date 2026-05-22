$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$runs = if ($env:XNET_COMPARE_SWEEP_RUNS) { [int]$env:XNET_COMPARE_SWEEP_RUNS } else { 3 }
$outDir = Join-Path $root "dev/bench/sweeps/windows"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "xnet compare sweep (windows)"
Write-Host "runs: $runs"
Write-Host "out:  $outDir"

for ($i = 1; $i -le $runs; $i++) {
	$stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
	$log = Join-Path $outDir ("xnet_compare_run_{0}_{1}.log" -f $i, $stamp)
	Write-Host ""
	Write-Host ("=== run {0}/{1} ===" -f $i, $runs)
	& "$root/dev/bench/run_xnet_compare_windows.ps1" 2>&1 |
		Tee-Object -FilePath $log
}
