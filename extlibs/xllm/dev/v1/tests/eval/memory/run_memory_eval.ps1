param(
    [string]$OutputDir,
    [string]$BaselineReport
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..\..")).Path
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build\memory_eval"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $repoRoot $OutputDir
}

if (-not [string]::IsNullOrWhiteSpace($BaselineReport) -and -not [System.IO.Path]::IsPathRooted($BaselineReport)) {
    $BaselineReport = Join-Path $repoRoot $BaselineReport
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "build") | Out-Null

$exePath = Join-Path $repoRoot "build\memory_eval.exe"
$reportPath = Join-Path $OutputDir "memory_eval_report.json"

& gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -Ilib\onnxruntime -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION `
    tests\eval\memory\memory_eval.c `
    lib\sqlite\sqlite3.c `
    -o $exePath `
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if ($LASTEXITCODE -ne 0) {
    throw "memory eval build failed with exit code $LASTEXITCODE"
}

& $exePath $OutputDir
if ($LASTEXITCODE -ne 0) {
    throw "memory eval failed with exit code $LASTEXITCODE"
}

if (-not [string]::IsNullOrWhiteSpace($BaselineReport)) {
    if (-not (Test-Path -LiteralPath $BaselineReport)) {
        throw "baseline report not found: $BaselineReport"
    }
    if (-not (Test-Path -LiteralPath $reportPath)) {
        throw "current report not found: $reportPath"
    }

    $baseline = Get-Content -Raw -LiteralPath $BaselineReport | ConvertFrom-Json
    $current = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
    $metricNames = @(
        "recall_at_1",
        "recall_at_5",
        "mrr",
        "precision_at_1",
        "precision_at_5",
        "avg_latency_ms",
        "avg_context_chars"
    )
    $metricComparisons = @()

    foreach ($metricName in $metricNames) {
        $baselineValue = [double]$baseline.metrics.$metricName
        $currentValue = [double]$current.metrics.$metricName
        $metricComparisons += [pscustomobject]@{
            name = $metricName
            baseline = $baselineValue
            current = $currentValue
            delta = $currentValue - $baselineValue
        }
    }

    $baselineRanks = @{}
    foreach ($query in $baseline.queries) {
        $baselineRanks[$query.id] = $query
    }

    $queryComparisons = @()
    foreach ($query in $current.queries) {
        $baselineQuery = $baselineRanks[$query.id]
        $baselineRank = if ($baselineQuery) { [int]$baselineQuery.rank } else { $null }
        $currentRank = [int]$query.rank
        $queryComparisons += [pscustomobject]@{
            id = $query.id
            group = $query.group
            baseline_rank = $baselineRank
            current_rank = $currentRank
            rank_delta = if ($null -ne $baselineRank) { $currentRank - $baselineRank } else { $null }
            baseline_top_record_id = if ($baselineQuery) { $baselineQuery.top_record_id } else { $null }
            current_top_record_id = $query.top_record_id
        }
    }

    $comparison = [pscustomobject]@{
        dataset = $current.dataset
        baseline_report = $BaselineReport
        current_report = $reportPath
        baseline_query_count = [int]$baseline.query_count
        current_query_count = [int]$current.query_count
        metrics = $metricComparisons
        queries = $queryComparisons
    }

    $compareJsonPath = Join-Path $OutputDir "memory_eval_compare.json"
    $compareTxtPath = Join-Path $OutputDir "memory_eval_compare.txt"
    $comparison | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath $compareJsonPath

    $lines = @()
    $lines += "dataset: $($comparison.dataset)"
    $lines += "baseline_report: $BaselineReport"
    $lines += "current_report: $reportPath"
    $lines += "baseline_query_count: $($comparison.baseline_query_count)"
    $lines += "current_query_count: $($comparison.current_query_count)"
    $lines += ""
    $lines += "metrics:"
    foreach ($metric in $metricComparisons) {
        $lines += ("  {0}: baseline={1:N6} current={2:N6} delta={3:N6}" -f $metric.name, $metric.baseline, $metric.current, $metric.delta)
    }
    $lines += ""
    $lines += "queries:"
    foreach ($query in $queryComparisons) {
        $lines += ("  {0} [{1}]: baseline_rank={2} current_rank={3} rank_delta={4} baseline_top={5} current_top={6}" -f `
            $query.id, `
            $query.group, `
            $(if ($null -ne $query.baseline_rank) { $query.baseline_rank } else { "missing" }), `
            $query.current_rank, `
            $(if ($null -ne $query.rank_delta) { $query.rank_delta } else { "missing" }), `
            $(if ($null -ne $query.baseline_top_record_id) { $query.baseline_top_record_id } else { "missing" }), `
            $query.current_top_record_id)
    }
    $lines | Set-Content -Encoding UTF8 -LiteralPath $compareTxtPath

    Write-Host "memory eval compare json: $compareJsonPath"
    Write-Host "memory eval compare txt:  $compareTxtPath"
}
