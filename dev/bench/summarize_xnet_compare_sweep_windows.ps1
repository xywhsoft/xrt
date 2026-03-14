param(
	[string]$InputDir = "",
	[string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $InputDir) {
	$InputDir = Join-Path $root "dev/bench/sweeps/windows"
}

if (-not (Test-Path -Path $InputDir -PathType Container)) {
	throw "input directory not found: $InputDir"
}

$files = Get-ChildItem -Path $InputDir -Filter "*.log" | Sort-Object LastWriteTime, Name
if (-not $files) {
	throw "no sweep logs found under: $InputDir"
}

$metricOrder = @(
	"v1_tcp_msgps",
	"v2_tcp_total_msgps",
	"v2_tcp_steady_msgps",
	"v1_tls_msgps",
	"v2_tls_total_msgps",
	"v2_tls_steady_msgps",
	"v1_udp_send_pps",
	"v2_udp_send_pps",
	"v2_pressure_drain_ms"
)

$metricLabels = @{
	"v1_tcp_msgps" = "xnet-v1 TCP msg/s"
	"v2_tcp_total_msgps" = "xnet-v2 TCP total msg/s"
	"v2_tcp_steady_msgps" = "xnet-v2 TCP steady msg/s"
	"v1_tls_msgps" = "xnet-v1 TLS msg/s"
	"v2_tls_total_msgps" = "xnet-v2 TLS total msg/s"
	"v2_tls_steady_msgps" = "xnet-v2 TLS steady msg/s"
	"v1_udp_send_pps" = "xnet-v1 UDP send pps"
	"v2_udp_send_pps" = "xnet-v2 UDP send pps"
	"v2_pressure_drain_ms" = "xnet-v2 queue drain ms"
}

$metrics = @{}
foreach ($name in $metricOrder) {
	$metrics[$name] = [System.Collections.Generic.List[double]]::new()
}

$incomplete = @{}
$matrix = @{
	platform = ""
	tcp = ""
	tls = ""
	udp = ""
	pressure = ""
}
$usedFiles = [System.Collections.Generic.List[string]]::new()
$skippedFiles = [System.Collections.Generic.List[string]]::new()

function Add-Metric {
	param(
		[hashtable]$MetricMap,
		[string]$Name,
		[double]$Value
	)

	if (-not $MetricMap.ContainsKey($Name)) {
		$MetricMap[$Name] = [System.Collections.Generic.List[double]]::new()
	}
	$MetricMap[$Name].Add($Value)
}

foreach ($file in $files) {
	$section = ""
	$hasMatrix = $false
	$sawMetric = $false
	$fileMetrics = @{}
	foreach ($metricName in $metricOrder) {
		$fileMetrics[$metricName] = [System.Collections.Generic.List[double]]::new()
	}
	$fileIncomplete = @{}
	$lines = Get-Content -Path $file.FullName
	foreach ($line in $lines) {
		switch -Regex ($line) {
			'^xnet compare matrix \((.+)\)$' {
				$hasMatrix = $true
				if (-not $matrix.platform) {
					$matrix.platform = $Matches[1]
				}
				continue
			}
			'^tcp:\s+(.+)$' {
				if (-not $matrix.tcp) {
					$matrix.tcp = $Matches[1]
				}
				continue
			}
			'^tls:\s+(.+)$' {
				if (-not $matrix.tls) {
					$matrix.tls = $Matches[1]
				}
				continue
			}
			'^udp:\s+(.+)$' {
				if (-not $matrix.udp) {
					$matrix.udp = $Matches[1]
				}
				continue
			}
			'^pressure:\s+(.+)$' {
				if (-not $matrix.pressure) {
					$matrix.pressure = $Matches[1]
				}
				continue
			}
			'^xnet-v1 bench_echo_tcp$' {
				$section = "v1_tcp"
				continue
			}
			'^xnet2 bench_echo_tcp$' {
				$section = "v2_tcp"
				continue
			}
			'^xnet-v1 bench_echo_tls$' {
				$section = "v1_tls"
				continue
			}
			'^xnet2 bench_echo_tls$' {
				$section = "v2_tls"
				continue
			}
			'^xnet-v1 bench_udp_burst$' {
				$section = "v1_udp"
				continue
			}
			'^xnet2 bench_udp_burst$' {
				$section = "v2_udp"
				continue
			}
			'^xnet2 bench_send_queue_pressure$' {
				$section = "v2_pressure"
				continue
			}
			'^benchmark_incomplete:\s+(.+)$' {
				$name = $Matches[1].Trim()
				if ($fileIncomplete.ContainsKey($name)) {
					$fileIncomplete[$name] += 1
				} else {
					$fileIncomplete[$name] = 1
				}
				continue
			}
		}

		switch ($section) {
			"v1_tcp" {
				if ($line -match '^messages_per_sec:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v1_tcp_msgps" -Value ([double]$Matches[1])
					$sawMetric = $true
				}
				continue
			}
			"v2_tcp" {
				if ($line -match '^messages_per_sec_total:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v2_tcp_total_msgps" -Value ([double]$Matches[1])
					$sawMetric = $true
				} elseif ($line -match '^messages_per_sec_steady:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v2_tcp_steady_msgps" -Value ([double]$Matches[1])
					$sawMetric = $true
				}
				continue
			}
			"v1_tls" {
				if ($line -match '^messages_per_sec:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v1_tls_msgps" -Value ([double]$Matches[1])
					$sawMetric = $true
				}
				continue
			}
			"v2_tls" {
				if ($line -match '^messages_per_sec_total:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v2_tls_total_msgps" -Value ([double]$Matches[1])
					$sawMetric = $true
				} elseif ($line -match '^messages_per_sec_steady:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v2_tls_steady_msgps" -Value ([double]$Matches[1])
					$sawMetric = $true
				}
				continue
			}
			"v1_udp" {
				if ($line -match '^send_packets_per_sec:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v1_udp_send_pps" -Value ([double]$Matches[1])
					$sawMetric = $true
				}
				continue
			}
			"v2_udp" {
				if ($line -match '^send_packets_per_sec:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v2_udp_send_pps" -Value ([double]$Matches[1])
					$sawMetric = $true
				}
				continue
			}
			"v2_pressure" {
				if ($line -match '^drain_elapsed_ns:\s+([0-9.]+)$') {
					Add-Metric -MetricMap $fileMetrics -Name "v2_pressure_drain_ms" -Value (([double]$Matches[1]) / 1000000.0)
					$sawMetric = $true
				}
				continue
			}
		}
	}

	if (($hasMatrix -and $sawMetric) -or (
		$fileMetrics["v2_tcp_total_msgps"].Count -gt 0 -and
		$fileMetrics["v2_tls_total_msgps"].Count -gt 0 -and
		$fileMetrics["v2_udp_send_pps"].Count -gt 0 -and
		$fileMetrics["v2_pressure_drain_ms"].Count -gt 0
	)) {
		foreach ($metricName in $metricOrder) {
			foreach ($value in $fileMetrics[$metricName]) {
				Add-Metric -MetricMap $metrics -Name $metricName -Value $value
			}
		}
		foreach ($name in $fileIncomplete.Keys) {
			if ($incomplete.ContainsKey($name)) {
				$incomplete[$name] += $fileIncomplete[$name]
			} else {
				$incomplete[$name] = $fileIncomplete[$name]
			}
		}
		$usedFiles.Add($file.Name)
	} else {
		$skippedFiles.Add($file.Name)
	}
}

function Get-Stats {
	param([System.Collections.Generic.List[double]]$Values)

	if (-not $Values -or $Values.Count -eq 0) {
		return $null
	}

	$sum = 0.0
	$sumsq = 0.0
	$min = $Values[0]
	$max = $Values[0]
	foreach ($value in $Values) {
		$sum += $value
		$sumsq += ($value * $value)
		if ($value -lt $min) {
			$min = $value
		}
		if ($value -gt $max) {
			$max = $value
		}
	}
	$mean = $sum / $Values.Count
	$variance = ($sumsq / $Values.Count) - ($mean * $mean)
	if ($variance -lt 0.0) {
		$variance = 0.0
	}
	$stddev = [Math]::Sqrt($variance)
	$cv = 0.0
	if ($mean -ne 0.0) {
		$cv = ($stddev / $mean) * 100.0
	}

	return @{
		count = $Values.Count
		mean = $mean
		min = $min
		max = $max
		cv = $cv
	}
}

function Format-Number {
	param([double]$Value)
	return ('{0:N1}' -f $Value)
}

$report = [System.Collections.Generic.List[string]]::new()
$report.Add("# XNET Compare Sweep Summary")
$report.Add("")
$report.Add("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')")
$report.Add("")
$report.Add("- Platform: $($matrix.platform)")
$report.Add(("- Source dir: {0}" -f $InputDir))
$report.Add("- Logs found: $($files.Count)")
$report.Add("- Logs used: $($usedFiles.Count)")
$report.Add("- Logs skipped: $($skippedFiles.Count)")
$report.Add(("- Matrix TCP: {0}" -f $matrix.tcp))
$report.Add(("- Matrix TLS: {0}" -f $matrix.tls))
$report.Add(("- Matrix UDP: {0}" -f $matrix.udp))
$report.Add(("- Matrix pressure: {0}" -f $matrix.pressure))
$report.Add("")
$report.Add("| Metric | Samples | Avg | Min | Max | CV % |")
$report.Add("|---|---:|---:|---:|---:|---:|")
foreach ($metricName in $metricOrder) {
	$stats = Get-Stats -Values $metrics[$metricName]
	if ($null -eq $stats) {
		continue
	}
	$report.Add("| $($metricLabels[$metricName]) | $($stats.count) | $(Format-Number $stats.mean) | $(Format-Number $stats.min) | $(Format-Number $stats.max) | $(Format-Number $stats.cv) |")
}

if ($incomplete.Count -gt 0) {
	$report.Add("")
	$report.Add("| Incomplete benchmark | Count |")
	$report.Add("|---|---:|")
	foreach ($name in ($incomplete.Keys | Sort-Object)) {
		$report.Add("| $name | $($incomplete[$name]) |")
	}
}

if ($skippedFiles.Count -gt 0) {
	$report.Add("")
	$report.Add("Skipped logs:")
	foreach ($name in $skippedFiles) {
		$report.Add(("- {0}" -f $name))
	}
}

$output = ($report -join [Environment]::NewLine)
if ($OutFile) {
	$output | Set-Content -Path $OutFile -NoNewline
}

$output
