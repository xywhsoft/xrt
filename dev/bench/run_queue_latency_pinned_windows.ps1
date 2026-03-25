param(
	[string]$OutDir = "",
	[uint32]$Cpu = 0,
	[switch]$SkipSummary
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

if (-not $OutDir) {
	$OutDir = Join-Path $root ("dev/bench/sweeps/queue_latency/windows_pinned_cpu{0}" -f $Cpu)
}

$summaryOut = Join-Path $OutDir ("SUMMARY_{0}.md" -f (Get-Date -Format "yyyyMMdd"))
$prevCpuPin = $env:XBENCH_PIN_CPU
$hadCpuPin = Test-Path Env:XBENCH_PIN_CPU

Write-Output "queue latency pinned sweep wrapper (windows)"
Write-Output "out_dir: $OutDir"
Write-Output "cpu_pin: $Cpu"

try {
	$env:XBENCH_PIN_CPU = "$Cpu"
	& (Join-Path $root "dev/bench/run_queue_latency_sweep_windows.ps1") -OutDir $OutDir

	if (-not $SkipSummary) {
		& (Join-Path $root "dev/bench/summarize_queue_latency_sweep_windows.ps1") -InputDir $OutDir -OutFile $summaryOut
	}
} finally {
	if ($hadCpuPin) {
		$env:XBENCH_PIN_CPU = $prevCpuPin
	} else {
		Remove-Item Env:XBENCH_PIN_CPU -ErrorAction SilentlyContinue
	}
}
