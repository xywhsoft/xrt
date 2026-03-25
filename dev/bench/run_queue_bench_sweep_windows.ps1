param(
	[string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

if (-not $OutDir) {
	$OutDir = Join-Path $root "dev/bench/sweeps/queue/windows"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$cflags = @("-O2", "-I", ".")
$winlibs = @("-lws2_32", "-liphlpapi")
$itemsPerProducer = if ($env:XQUEUE_SWEEP_ITEMS_PER_PRODUCER) { [uint32]$env:XQUEUE_SWEEP_ITEMS_PER_PRODUCER } else { [uint32]250000 }
$fixedCapacity = if ($env:XQUEUE_SWEEP_FIXED_CAPACITY) { [uint32]$env:XQUEUE_SWEEP_FIXED_CAPACITY } else { [uint32]4096 }
$defaultMpscProducers = if ($env:XQUEUE_SWEEP_DEFAULT_MPSC_PRODUCERS) { [uint32]$env:XQUEUE_SWEEP_DEFAULT_MPSC_PRODUCERS } else { [uint32]4 }
$defaultMpmcProducers = if ($env:XQUEUE_SWEEP_DEFAULT_MPMC_PRODUCERS) { [uint32]$env:XQUEUE_SWEEP_DEFAULT_MPMC_PRODUCERS } else { [uint32]4 }
$defaultMpmcConsumers = if ($env:XQUEUE_SWEEP_DEFAULT_MPMC_CONSUMERS) { [uint32]$env:XQUEUE_SWEEP_DEFAULT_MPMC_CONSUMERS } else { [uint32]4 }
$batchSize = if ($env:XQUEUE_SWEEP_BATCH_SIZE) { [uint32]$env:XQUEUE_SWEEP_BATCH_SIZE } else { [uint32]32 }
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

function Get-PairList {
	param(
		[string]$EnvValue,
		[string[]]$Default
	)

	$tokens = Split-SweepTokens $EnvValue
	if (-not $tokens -or $tokens.Count -eq 0) {
		$tokens = $Default
	}

	$list = [System.Collections.Generic.List[object]]::new()
	foreach ($token in $tokens) {
		if ($token -notmatch '^(\d+)[xX](\d+)$') {
			throw "invalid MPMC pair token: $token"
		}
		$list.Add([pscustomobject]@{
			Label = ("mpmc-{0}x{1}" -f $Matches[1], $Matches[2])
			Producers = [uint32]$Matches[1]
			Consumers = [uint32]$Matches[2]
		})
	}
	return $list.ToArray()
}

$capacityList = Get-UIntList -EnvValue $env:XQUEUE_SWEEP_CAPACITY_LIST -Default @([uint32]256, [uint32]1024, [uint32]4096, [uint32]16384)
$mpscProducerList = Get-UIntList -EnvValue $env:XQUEUE_SWEEP_MPSC_PRODUCER_LIST -Default @([uint32]1, [uint32]2, [uint32]4, [uint32]8)
$mpmcPairList = Get-PairList -EnvValue $env:XQUEUE_SWEEP_MPMC_PAIR_LIST -Default @("1x1", "2x2", "4x4", "8x8")

Write-Output "queue bench sweep (windows)"
Write-Output "out_dir: $OutDir"
Write-Output "items_per_producer: $itemsPerProducer"
Write-Output "fixed_capacity: $fixedCapacity"
Write-Output "batch_size: $batchSize"
Write-Output "policy: $policyName"
Write-Output "cpu_pin: $cpuPin"
Write-Output "capacity_list: $($capacityList -join ', ')"
Write-Output "mpsc_producer_list: $($mpscProducerList -join ', ')"
Write-Output "mpmc_pair_list: $(($mpmcPairList | ForEach-Object { '{0}x{1}' -f $_.Producers, $_.Consumers }) -join ', ')"

function Invoke-QueueSweepRun {
	param(
		[string]$Family,
		[string]$Label,
		[uint32]$Capacity,
		[uint32]$MpscProducers,
		[uint32]$MpmcProducers,
		[uint32]$MpmcConsumers
	)

	$stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
	$logPath = Join-Path $OutDir ("queue_{0}_{1}_{2}.log" -f $Family, $Label, $stamp)
	$benchOutput = & $benchExe $itemsPerProducer $Capacity $MpscProducers $MpmcProducers $MpmcConsumers $batchSize 2>&1
	$payload = @(
		"queue sweep matrix (windows)"
		"family: $Family"
		"label: $Label"
		"items_per_producer: $itemsPerProducer"
		"capacity: $Capacity"
		"mpsc_producers: $MpscProducers"
		"mpmc_producers: $MpmcProducers"
		"mpmc_consumers: $MpmcConsumers"
		"batch_size: $batchSize"
		"policy: $policyName"
		"cpu_pin: $cpuPin"
	) + $benchOutput

	$payload | Tee-Object -FilePath $logPath
}

try {
	gcc dev/bench/queue/bench_queue_pointer.c xrt.c @cflags -o $benchExe @winlibs

	foreach ($capacity in $capacityList) {
		Invoke-QueueSweepRun -Family "capacity" -Label ("cap-{0}" -f $capacity) -Capacity $capacity -MpscProducers $defaultMpscProducers -MpmcProducers $defaultMpmcProducers -MpmcConsumers $defaultMpmcConsumers
	}

	foreach ($mpscProducers in $mpscProducerList) {
		Invoke-QueueSweepRun -Family "mpsc_producers" -Label ("mpsc-p{0}" -f $mpscProducers) -Capacity $fixedCapacity -MpscProducers $mpscProducers -MpmcProducers $defaultMpmcProducers -MpmcConsumers $defaultMpmcConsumers
	}

	foreach ($pair in $mpmcPairList) {
		Invoke-QueueSweepRun -Family "mpmc_pairs" -Label $pair.Label -Capacity $fixedCapacity -MpscProducers $defaultMpscProducers -MpmcProducers $pair.Producers -MpmcConsumers $pair.Consumers
	}
} finally {
	if (Test-Path $benchExe) {
		Remove-Item $benchExe -Force -ErrorAction SilentlyContinue
	}
}
