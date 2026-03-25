param(
	[string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

if (-not $OutDir) {
	$OutDir = Join-Path $root "dev/bench/sweeps/queue_batch/windows"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$cflags = @("-O2", "-I", ".")
$winlibs = @("-lws2_32", "-liphlpapi")
$itemsPerProducer = if ($env:XQUEUE_BATCH_SWEEP_ITEMS_PER_PRODUCER) { [uint32]$env:XQUEUE_BATCH_SWEEP_ITEMS_PER_PRODUCER } else { [uint32]500000 }
$capacity = if ($env:XQUEUE_BATCH_SWEEP_CAPACITY) { [uint32]$env:XQUEUE_BATCH_SWEEP_CAPACITY } else { [uint32]4096 }
$mpscProducers = if ($env:XQUEUE_BATCH_SWEEP_MPSC_PRODUCERS) { [uint32]$env:XQUEUE_BATCH_SWEEP_MPSC_PRODUCERS } else { [uint32]4 }
$mpmcProducers = if ($env:XQUEUE_BATCH_SWEEP_MPMC_PRODUCERS) { [uint32]$env:XQUEUE_BATCH_SWEEP_MPMC_PRODUCERS } else { [uint32]4 }
$mpmcConsumers = if ($env:XQUEUE_BATCH_SWEEP_MPMC_CONSUMERS) { [uint32]$env:XQUEUE_BATCH_SWEEP_MPMC_CONSUMERS } else { [uint32]4 }
$cpuPin = if ($env:XBENCH_PIN_CPU) { $env:XBENCH_PIN_CPU } else { "disabled" }
$policyName = if ($env:XBENCH_PIN_CPU) { "diagnostic-process-pin" } else { "baseline-unpinned" }
$benchStamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$benchExe = Join-Path $root ("dev/bench/queue/bench_queue_pointer_win_{0}_{1}.exe" -f $PID, $benchStamp)

function Split-SweepTokens {
	param([string]$Value)

	if (-not $Value) {
		return @()
	}

	return @($Value -split "[,\s]+" | Where-Object { $_ })
}

function Get-UIntList {
	param(
		[string]$EnvValue,
		[uint32[]]$Default
	)

	$tokens = Split-SweepTokens $EnvValue
	if (-not $tokens -or $tokens.Count -eq 0) {
		return $Default
	}

	$list = [System.Collections.Generic.List[uint32]]::new()
	foreach ($token in $tokens) {
		$list.Add([uint32]$token)
	}
	return $list.ToArray()
}

$batchSizeList = Get-UIntList -EnvValue $env:XQUEUE_BATCH_SWEEP_BATCH_SIZE_LIST -Default @([uint32]1, [uint32]4, [uint32]8, [uint32]16, [uint32]32, [uint32]64)

Write-Output "queue batch sweep (windows)"
Write-Output "out_dir: $OutDir"
Write-Output "items_per_producer: $itemsPerProducer"
Write-Output "capacity: $capacity"
Write-Output "mpsc_producers: $mpscProducers"
Write-Output "mpmc_producers: $mpmcProducers"
Write-Output "mpmc_consumers: $mpmcConsumers"
Write-Output "batch_size_list: $($batchSizeList -join ', ')"
Write-Output "policy: $policyName"
Write-Output "cpu_pin: $cpuPin"

try {
	gcc dev/bench/queue/bench_queue_pointer.c xrt.c @cflags -o $benchExe @winlibs

	foreach ($batchSize in $batchSizeList) {
		$stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
		$label = "batch-$batchSize"
		$logPath = Join-Path $OutDir ("queue_batch_{0}_{1}.log" -f $label, $stamp)
		$benchOutput = & $benchExe $itemsPerProducer $capacity $mpscProducers $mpmcProducers $mpmcConsumers $batchSize 2>&1
		$payload = @(
			"queue batch sweep matrix (windows)"
			"label: $label"
			"items_per_producer: $itemsPerProducer"
			"capacity: $capacity"
			"mpsc_producers: $mpscProducers"
			"mpmc_producers: $mpmcProducers"
			"mpmc_consumers: $mpmcConsumers"
			"batch_size: $batchSize"
			"policy: $policyName"
			"cpu_pin: $cpuPin"
		) + $benchOutput

		$payload | Tee-Object -FilePath $logPath
	}
} finally {
	if (Test-Path $benchExe) {
		Remove-Item $benchExe -Force -ErrorAction SilentlyContinue
	}
}
