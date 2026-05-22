param(
	[string]$InputDir = "",
	[string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $InputDir) {
	$InputDir = Join-Path $root "dev/bench/sweeps/queue/windows"
}

if (-not (Test-Path -Path $InputDir -PathType Container)) {
	throw "input directory not found: $InputDir"
}

$files = Get-ChildItem -Path $InputDir -Filter "*.log" | Sort-Object LastWriteTime, Name
if (-not $files) {
	throw "no sweep logs found under: $InputDir"
}

$familyOrder = @("capacity", "mpsc_producers", "mpmc_pairs")
$familyTitles = @{
	"capacity" = "Capacity Sweep"
	"mpsc_producers" = "MPSC Producer Sweep"
	"mpmc_pairs" = "MPMC Pair Sweep"
}
$rows = [System.Collections.Generic.List[object]]::new()
$skippedFiles = [System.Collections.Generic.List[string]]::new()
$platform = ""

foreach ($file in $files) {
	$data = @{}
	foreach ($line in (Get-Content -Path $file.FullName)) {
		switch -Regex ($line) {
			'^queue sweep matrix \((.+)\)$' {
				$data.platform = $Matches[1]
				continue
			}
			'^(family|label):\s+(.+)$' {
				$data[$Matches[1]] = $Matches[2]
				continue
			}
			'^(policy|cpu_pin):\s+(.+)$' {
				$data[$Matches[1]] = $Matches[2]
				continue
			}
			'^(items_per_producer|capacity|mpsc_producers|mpmc_producers|mpmc_consumers):\s+([0-9]+)$' {
				$data[$Matches[1]] = [uint32]$Matches[2]
				continue
			}
			'^(spsc_items_per_sec|mpsc_items_per_sec|mpmc_items_per_sec):\s+([0-9.]+)$' {
				$data[$Matches[1]] = [double]$Matches[2]
				continue
			}
		}
	}

	if (
		$data.ContainsKey('platform') -and
		$data.ContainsKey('family') -and
		$data.ContainsKey('label') -and
		$data.ContainsKey('items_per_producer') -and
		$data.ContainsKey('capacity') -and
		$data.ContainsKey('mpsc_producers') -and
		$data.ContainsKey('mpmc_producers') -and
		$data.ContainsKey('mpmc_consumers') -and
		$data.ContainsKey('spsc_items_per_sec') -and
		$data.ContainsKey('mpsc_items_per_sec') -and
		$data.ContainsKey('mpmc_items_per_sec')
	) {
		if (-not $platform) {
			$platform = $data.platform
		}
		$rows.Add([pscustomobject]@{
			Family = [string]$data.family
			Label = [string]$data.label
			Policy = if ($data.ContainsKey('policy')) { [string]$data.policy } else { "baseline-unpinned" }
			CpuPin = if ($data.ContainsKey('cpu_pin')) { [string]$data.cpu_pin } else { "disabled" }
			ItemsPerProducer = [uint32]$data.items_per_producer
			Capacity = [uint32]$data.capacity
			MpscProducers = [uint32]$data.mpsc_producers
			MpmcProducers = [uint32]$data.mpmc_producers
			MpmcConsumers = [uint32]$data.mpmc_consumers
			SpscItemsPerSec = [double]$data.spsc_items_per_sec
			MpscItemsPerSec = [double]$data.mpsc_items_per_sec
			MpmcItemsPerSec = [double]$data.mpmc_items_per_sec
			LastWriteUtc = $file.LastWriteTimeUtc
			FileName = $file.Name
		})
	} else {
		$skippedFiles.Add($file.Name)
	}
}

$rawRowCount = $rows.Count
$rows = @(
	$rows |
		Group-Object {
			'{0}|{1}|{2}|{3}|{4}|{5}|{6}|{7}|{8}' -f $_.Family, $_.Label, $_.Policy, $_.CpuPin, $_.ItemsPerProducer, $_.Capacity, $_.MpscProducers, $_.MpmcProducers, $_.MpmcConsumers
		} |
		ForEach-Object {
			$_.Group | Sort-Object LastWriteUtc, FileName | Select-Object -Last 1
		}
)
$dedupedCount = $rawRowCount - $rows.Count

function Format-Rate {
	param([double]$Value)
	return ('{0:N1}' -f $Value)
}

$report = [System.Collections.Generic.List[string]]::new()
$report.Add("# Queue Bench Sweep Summary")
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

foreach ($family in $familyOrder) {
	$familyRows = @($rows | Where-Object { $_.Family -eq $family })
	if (-not $familyRows) {
		continue
	}

	switch ($family) {
		"capacity" {
			$familyRows = @($familyRows | Sort-Object Capacity, Label)
		}
		"mpsc_producers" {
			$familyRows = @($familyRows | Sort-Object MpscProducers, Label)
		}
		"mpmc_pairs" {
			$familyRows = @($familyRows | Sort-Object MpmcProducers, MpmcConsumers, Label)
		}
	}

	$report.Add("## $($familyTitles[$family])")
	$report.Add("")
	$report.Add("| Label | Policy | CPU pin | Items/producer | Capacity | MPSC P | MPMC P | MPMC C | SPSC items/s | MPSC items/s | MPMC items/s |")
	$report.Add("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|")
	foreach ($row in $familyRows) {
		$report.Add("| $($row.Label) | $($row.Policy) | $($row.CpuPin) | $($row.ItemsPerProducer) | $($row.Capacity) | $($row.MpscProducers) | $($row.MpmcProducers) | $($row.MpmcConsumers) | $(Format-Rate $row.SpscItemsPerSec) | $(Format-Rate $row.MpscItemsPerSec) | $(Format-Rate $row.MpmcItemsPerSec) |")
	}
	$report.Add("")
}

if ($skippedFiles.Count -gt 0) {
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
