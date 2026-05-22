param(
	[string]$InputDir = "",
	[string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $InputDir) {
	$InputDir = Join-Path $root "dev/bench/sweeps/queue_batch/windows"
}

if (-not (Test-Path -Path $InputDir -PathType Container)) {
	throw "input directory not found: $InputDir"
}

$files = Get-ChildItem -Path $InputDir -Filter "*.log" | Sort-Object LastWriteTime, Name
if (-not $files) {
	throw "no sweep logs found under: $InputDir"
}

$rows = [System.Collections.Generic.List[object]]::new()
$skippedFiles = [System.Collections.Generic.List[string]]::new()
$platform = ""

foreach ($file in $files) {
	$data = @{}
	foreach ($line in (Get-Content -Path $file.FullName)) {
		switch -Regex ($line) {
			'^queue batch sweep matrix \((.+)\)$' {
				$data.platform = $Matches[1]
				continue
			}
			'^label:\s+(.+)$' {
				$data.label = $Matches[1]
				continue
			}
			'^(policy|cpu_pin):\s+(.+)$' {
				$data[$Matches[1]] = $Matches[2]
				continue
			}
			'^(items_per_producer|capacity|mpsc_producers|mpmc_producers|mpmc_consumers|batch_size):\s+([0-9]+)$' {
				$data[$Matches[1]] = [uint32]$Matches[2]
				continue
			}
			'^(mpsc_items_per_sec|mpsc_batch_items_per_sec|mpmc_items_per_sec|mpmc_batch_items_per_sec):\s+([0-9.]+)$' {
				$data[$Matches[1]] = [double]$Matches[2]
				continue
			}
		}
	}

	if (
		$data.ContainsKey('platform') -and
		$data.ContainsKey('label') -and
		$data.ContainsKey('items_per_producer') -and
		$data.ContainsKey('capacity') -and
		$data.ContainsKey('mpsc_producers') -and
		$data.ContainsKey('mpmc_producers') -and
		$data.ContainsKey('mpmc_consumers') -and
		$data.ContainsKey('batch_size') -and
		$data.ContainsKey('mpsc_items_per_sec') -and
		$data.ContainsKey('mpsc_batch_items_per_sec') -and
		$data.ContainsKey('mpmc_items_per_sec') -and
		$data.ContainsKey('mpmc_batch_items_per_sec')
	) {
		if (-not $platform) {
			$platform = $data.platform
		}
		$rows.Add([pscustomobject]@{
			Label = [string]$data.label
			Policy = if ($data.ContainsKey('policy')) { [string]$data.policy } else { "baseline-unpinned" }
			CpuPin = if ($data.ContainsKey('cpu_pin')) { [string]$data.cpu_pin } else { "disabled" }
			ItemsPerProducer = [uint32]$data.items_per_producer
			Capacity = [uint32]$data.capacity
			MpscProducers = [uint32]$data.mpsc_producers
			MpmcProducers = [uint32]$data.mpmc_producers
			MpmcConsumers = [uint32]$data.mpmc_consumers
			BatchSize = [uint32]$data.batch_size
			MpscSingle = [double]$data.mpsc_items_per_sec
			MpscBatch = [double]$data.mpsc_batch_items_per_sec
			MpmcSingle = [double]$data.mpmc_items_per_sec
			MpmcBatch = [double]$data.mpmc_batch_items_per_sec
			LastWriteUtc = $file.LastWriteTimeUtc
			FileName = $file.Name
		})
	} else {
		$skippedFiles.Add($file.Name)
	}
}

function Format-Rate {
	param([double]$Value)
	return ('{0:N1}' -f $Value)
}

function Format-Ratio {
	param(
		[double]$Numerator,
		[double]$Denominator
	)

	if ($Denominator -eq 0.0) {
		return "0.00x"
	}

	return ('{0:N2}x' -f ($Numerator / $Denominator))
}

$rawRowCount = $rows.Count
$rows = @(
	$rows |
		Group-Object {
			'{0}|{1}|{2}|{3}|{4}|{5}|{6}|{7}|{8}' -f $_.Label, $_.Policy, $_.CpuPin, $_.BatchSize, $_.ItemsPerProducer, $_.Capacity, $_.MpscProducers, $_.MpmcProducers, $_.MpmcConsumers
		} |
		ForEach-Object {
			$_.Group | Sort-Object LastWriteUtc, FileName | Select-Object -Last 1
		}
)
$dedupedCount = $rawRowCount - $rows.Count
$rows = @($rows | Sort-Object BatchSize, Label)

$report = [System.Collections.Generic.List[string]]::new()
$report.Add("# Queue Batch Sweep Summary")
$report.Add("")
$report.Add("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')")
$report.Add("")
$report.Add("- Platform: $platform")
$report.Add("- Source dir: $InputDir")
$report.Add("- Logs found: $($files.Count)")
$report.Add("- Logs used: $($rows.Count)")
$report.Add("- Duplicate samples pruned: $dedupedCount")
$report.Add("- Logs skipped: $($skippedFiles.Count)")
$report.Add("- Policies seen: $((@($rows | ForEach-Object { '{0} (cpu {1})' -f $_.Policy, $_.CpuPin } | Sort-Object -Unique) -join ', '))")
$report.Add("")
$report.Add("| Label | Policy | CPU pin | Batch size | Items/producer | Capacity | MPSC P | MPMC P | MPMC C | MPSC single | MPSC batch | MPSC batch/single | MPMC single | MPMC batch | MPMC batch/single |")
$report.Add("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
foreach ($row in $rows) {
	$report.Add("| $($row.Label) | $($row.Policy) | $($row.CpuPin) | $($row.BatchSize) | $($row.ItemsPerProducer) | $($row.Capacity) | $($row.MpscProducers) | $($row.MpmcProducers) | $($row.MpmcConsumers) | $(Format-Rate $row.MpscSingle) | $(Format-Rate $row.MpscBatch) | $(Format-Ratio $row.MpscBatch $row.MpscSingle) | $(Format-Rate $row.MpmcSingle) | $(Format-Rate $row.MpmcBatch) | $(Format-Ratio $row.MpmcBatch $row.MpmcSingle) |")
}

if ($skippedFiles.Count -gt 0) {
	$report.Add("")
	$report.Add("Skipped logs:")
	foreach ($name in $skippedFiles) {
		$report.Add("- $name")
	}
}

$output = ($report -join [Environment]::NewLine)
if ($OutFile) {
	$output | Set-Content -Path $OutFile -NoNewline
}

$output
