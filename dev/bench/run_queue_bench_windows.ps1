$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$cflags = @("-O2", "-I", ".")
$winlibs = @("-lws2_32", "-liphlpapi")
$itemsPerProducer = if ($env:XQUEUE_BENCH_ITEMS_PER_PRODUCER) { [uint32]$env:XQUEUE_BENCH_ITEMS_PER_PRODUCER } else { [uint32]500000 }
$capacity = if ($env:XQUEUE_BENCH_CAPACITY) { [uint32]$env:XQUEUE_BENCH_CAPACITY } else { [uint32]4096 }
$mpscProducers = if ($env:XQUEUE_BENCH_MPSC_PRODUCERS) { [uint32]$env:XQUEUE_BENCH_MPSC_PRODUCERS } else { [uint32]4 }
$mpmcProducers = if ($env:XQUEUE_BENCH_MPMC_PRODUCERS) { [uint32]$env:XQUEUE_BENCH_MPMC_PRODUCERS } else { [uint32]4 }
$mpmcConsumers = if ($env:XQUEUE_BENCH_MPMC_CONSUMERS) { [uint32]$env:XQUEUE_BENCH_MPMC_CONSUMERS } else { [uint32]4 }
$batchSize = if ($env:XQUEUE_BENCH_BATCH_SIZE) { [uint32]$env:XQUEUE_BENCH_BATCH_SIZE } else { [uint32]32 }
$cpuPin = if ($env:XBENCH_PIN_CPU) { $env:XBENCH_PIN_CPU } else { "disabled" }
$policyName = if ($env:XBENCH_PIN_CPU) { "diagnostic-process-pin" } else { "baseline-unpinned" }
$benchStamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$benchExe = Join-Path $root ("dev/bench/queue/bench_queue_pointer_win_{0}_{1}.exe" -f $PID, $benchStamp)

Write-Output "queue bench matrix (windows)"
Write-Output "items_per_producer: $itemsPerProducer"
Write-Output "capacity: $capacity"
Write-Output "mpsc_producers: $mpscProducers"
Write-Output "mpmc_producers: $mpmcProducers"
Write-Output "mpmc_consumers: $mpmcConsumers"
Write-Output "batch_size: $batchSize"
Write-Output "policy: $policyName"
Write-Output "cpu_pin: $cpuPin"

try {
	gcc dev/bench/queue/bench_queue_pointer.c xrt.c @cflags -o $benchExe @winlibs

	& $benchExe $itemsPerProducer $capacity $mpscProducers $mpmcProducers $mpmcConsumers $batchSize
} finally {
	if (Test-Path $benchExe) {
		Remove-Item $benchExe -Force -ErrorAction SilentlyContinue
	}
}
