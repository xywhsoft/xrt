param(
    [string]$OutputDir,
    [string]$CaseFilter
)

$ErrorActionPreference = "Stop"

function Get-RequiredEnv {
    param([string]$Name)
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Missing required environment variable: $Name"
    }
    return $value
}

function Get-OptionalEnv {
    param([string]$Name, [string]$DefaultValue = "")
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $DefaultValue
    }
    return $value
}

function Get-BoolEnv {
    param([string]$Name, [bool]$DefaultValue = $false)
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $DefaultValue
    }
    switch -Regex ($value.ToLowerInvariant()) {
        "^(1|true|yes|on)$" { return $true }
        "^(0|false|no|off)$" { return $false }
        default { return $DefaultValue }
    }
}

function Get-CaseFilterSet {
    param([string]$RawInput = "")

    $raw = $RawInput
    if ([string]::IsNullOrWhiteSpace($raw)) {
        $raw = [Environment]::GetEnvironmentVariable("XLLM_REAL_CASE_FILTER")
    }
    $set = @{}

    if ([string]::IsNullOrWhiteSpace($raw)) {
        return $null
    }

    foreach ($item in ($raw -split "[,; ]+")) {
        if (-not [string]::IsNullOrWhiteSpace($item)) {
            $set[$item.Trim().ToLowerInvariant()] = $true
        }
    }

    if ($set.Count -eq 0) {
        return $null
    }

    return $set
}

function Test-CaseRequested {
    param(
        [hashtable]$FilterSet,
        [string]$Name
    )

    if ($FilterSet -eq $null) {
        return $false
    }
    if ([string]::IsNullOrWhiteSpace($Name)) {
        return $false
    }
    return $FilterSet.ContainsKey($Name.Trim().ToLowerInvariant())
}

function Add-KnownCaseName {
    param(
        [hashtable]$KnownSet,
        [string]$Name
    )

    if ($KnownSet -eq $null -or [string]::IsNullOrWhiteSpace($Name)) {
        return
    }
    $KnownSet[$Name.Trim().ToLowerInvariant()] = $true
}

function Invoke-ProbeCase {
    param(
        [string]$Name,
        [hashtable]$Overrides,
        [string]$ExePath,
        [string]$LogDir
    )

    $backup = @{}
    $logPath = Join-Path $LogDir ($Name + ".log")
    $summaryPath = Join-Path $LogDir ($Name + ".summary.json")
    $effectiveOverrides = @{}
    foreach ($key in $Overrides.Keys) {
        $effectiveOverrides[$key] = $Overrides[$key]
    }
    $effectiveOverrides["XLLM_REAL_SUMMARY_PATH"] = $summaryPath
    $effectiveOverrides["XLLM_REAL_CASE_NAME"] = $Name

    foreach ($key in $effectiveOverrides.Keys) {
        $backup[$key] = [Environment]::GetEnvironmentVariable($key)
        [Environment]::SetEnvironmentVariable($key, $effectiveOverrides[$key])
    }

    try {
        $startedAt = Get-Date
        $quotedExePath = '"' + $ExePath.Replace('"', '""') + '"'
        $quotedLogPath = '"' + $logPath.Replace('"', '""') + '"'
        if (Test-Path $logPath) {
            Remove-Item $logPath -Force
        }
        if (Test-Path $summaryPath) {
            Remove-Item $summaryPath -Force
        }
        Write-Host "==> case: $Name"
        cmd /c ($quotedExePath + " > " + $quotedLogPath + " 2>&1")
        $exitCode = $LASTEXITCODE
        $summary = $null
        if (Test-Path $summaryPath) {
            $summaryJson = Get-Content -Raw $summaryPath
            if (-not [string]::IsNullOrWhiteSpace($summaryJson)) {
                $summary = $summaryJson | ConvertFrom-Json
            }
        } else {
            $summaryLine = Get-Content $logPath | Where-Object { $_ -like "probe_summary_json:*" } | Select-Object -Last 1
            if (-not [string]::IsNullOrWhiteSpace($summaryLine)) {
                $summaryJson = $summaryLine.Substring("probe_summary_json:".Length).Trim()
                if (-not [string]::IsNullOrWhiteSpace($summaryJson)) {
                    $summary = $summaryJson | ConvertFrom-Json
                }
            }
        }
        Write-Host "log: $logPath"
        if ($exitCode -ne 0 -and (Get-BoolEnv "XLLM_REAL_VERBOSE_FAILURE" $true)) {
            Get-Content $logPath
        }
        $durationMs = [int][Math]::Round(((Get-Date) - $startedAt).TotalMilliseconds)
        if ($summary -ne $null -and $summary.PSObject.Properties.Name -contains "duration_ms" -and $summary.duration_ms -ne $null) {
            $durationMs = [int]$summary.duration_ms
        }
        return [PSCustomObject]@{
            Name = $Name
            ExitCode = $exitCode
            LogPath = $logPath
            SummaryPath = $summaryPath
            DurationMs = $durationMs
            Summary = $summary
        }
    }
    finally {
        foreach ($key in $effectiveOverrides.Keys) {
            [Environment]::SetEnvironmentVariable($key, $backup[$key])
        }
    }
}

function New-SkippedCaseResult {
    param(
        [string]$Name,
        [string]$Reason
    )

    return [PSCustomObject]@{
        Name = $Name
        Reason = $Reason
    }
}

function Invoke-ProxyPreflight {
    $kind = Get-OptionalEnv "XLLM_REAL_PROXY_KIND"
    $proxyHost = Get-OptionalEnv "XLLM_REAL_PROXY_HOST"
    $port = Get-OptionalEnv "XLLM_REAL_PROXY_PORT"
    $testUrl = Get-OptionalEnv "XLLM_REAL_PROXY_TEST_URL" "https://example.com"

    if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($proxyHost) -or [string]::IsNullOrWhiteSpace($port)) {
        return $null
    }

    $kind = $kind.Trim().ToLowerInvariant()
    if ($kind -eq "none" -or $kind -eq "unspecified") {
        return [PSCustomObject]@{
            enabled = $false
            kind = $kind
            host = $proxyHost
            port = $port
            test_url = $testUrl
            ok = $true
            skipped = $true
            message = "proxy explicitly disabled"
        }
    }

    $curlPath = (Get-Command curl.exe -ErrorAction SilentlyContinue).Source
    if ([string]::IsNullOrWhiteSpace($curlPath)) {
        return [PSCustomObject]@{
            enabled = $true
            kind = $kind
            host = $proxyHost
            port = $port
            test_url = $testUrl
            ok = $false
            skipped = $false
            exit_code = -1
            message = "curl.exe not found"
        }
    }

    $args = @("--silent", "--show-error", "--head", "--max-time", "20", $testUrl)
    switch ($kind) {
        "http_connect" { $args = @("-x", ("http://{0}:{1}" -f $proxyHost, $port)) + $args }
        "http-connect" { $args = @("-x", ("http://{0}:{1}" -f $proxyHost, $port)) + $args }
        "http" { $args = @("-x", ("http://{0}:{1}" -f $proxyHost, $port)) + $args }
        "socks5" { $args = @("--socks5-hostname", ("{0}:{1}" -f $proxyHost, $port)) + $args }
        default {
            return [PSCustomObject]@{
                enabled = $true
                kind = $kind
                host = $proxyHost
                port = $port
                test_url = $testUrl
                ok = $false
                skipped = $false
                exit_code = -1
                message = ("unsupported proxy kind for preflight: {0}" -f $kind)
            }
        }
    }

    $user = Get-OptionalEnv "XLLM_REAL_PROXY_USER"
    $pass = Get-OptionalEnv "XLLM_REAL_PROXY_PASS"
    if (-not [string]::IsNullOrWhiteSpace($user)) {
        $args = @("--proxy-user", ("{0}:{1}" -f $user, $pass)) + $args
    }

    $output = & $curlPath @args 2>&1
    $exitCode = $LASTEXITCODE
    $outputText = (($output | ForEach-Object { "$_" }) -join [Environment]::NewLine).Trim()

    return [PSCustomObject]@{
        enabled = $true
        kind = $kind
        host = $proxyHost
        port = $port
        test_url = $testUrl
        ok = ($exitCode -eq 0)
        skipped = $false
        exit_code = $exitCode
        message = $outputText
    }
}

function Get-ObjectField {
    param(
        $Object,
        [string]$Name,
        $DefaultValue = $null
    )

    if ($Object -eq $null -or [string]::IsNullOrWhiteSpace($Name)) {
        return $DefaultValue
    }
    if ($Object.PSObject.Properties.Name -contains $Name) {
        $value = $Object.$Name
        if ($null -ne $value) {
            return $value
        }
    }
    return $DefaultValue
}

function New-StableCaseSummary {
    param($Result)

    $summary = $Result.Summary
    return [PSCustomObject]@{
        name = $Result.Name
        outcome = $(if ($Result.ExitCode -eq 0) { "ok" } else { "failed" })
        exit_code = $Result.ExitCode
        success = [bool](Get-ObjectField -Object $summary -Name "success" -DefaultValue ($Result.ExitCode -eq 0))
        status = Get-ObjectField -Object $summary -Name "status" -DefaultValue ""
        error_code = Get-ObjectField -Object $summary -Name "error_code" -DefaultValue ""
        http_status = Get-ObjectField -Object $summary -Name "http_status" -DefaultValue 0
        finish_reason = Get-ObjectField -Object $summary -Name "finish_reason" -DefaultValue ""
        response_format = Get-ObjectField -Object $summary -Name "response_format" -DefaultValue ""
        stream_mode = Get-ObjectField -Object $summary -Name "stream_mode" -DefaultValue 0
        multimodal = [bool](Get-ObjectField -Object $summary -Name "multimodal" -DefaultValue $false)
        tool_enabled = [bool](Get-ObjectField -Object $summary -Name "tool_enabled" -DefaultValue $false)
        tool_call_count = Get-ObjectField -Object $summary -Name "tool_call_count" -DefaultValue 0
        text_part_count = Get-ObjectField -Object $summary -Name "text_part_count" -DefaultValue 0
        artifact_part_count = Get-ObjectField -Object $summary -Name "artifact_part_count" -DefaultValue 0
        json_part_count = Get-ObjectField -Object $summary -Name "json_part_count" -DefaultValue 0
    }
}

function Write-StableProbeSummary {
    param(
        [string]$Path,
        [string]$JsonPath,
        [bool]$Success,
        [string]$Phase = "matrix",
        [string]$Adapter = "",
        [string]$Model = "",
        [string]$CaseFilter = "",
        [int]$ExecutedCount = 0,
        [int]$FailedCount = 0,
        [int]$SkippedCount = 0,
        [object[]]$Results = @(),
        [object[]]$SkippedCases = @(),
        [string[]]$MissingNames = @()
    )

    $stableResults = @(
        $Results |
            ForEach-Object { New-StableCaseSummary -Result $_ } |
            Sort-Object -Property name
    )
    $stableSkipped = @(
        $SkippedCases |
            ForEach-Object {
                [PSCustomObject]@{
                    name = $_.Name
                    reason = $_.Reason
                }
            } |
            Sort-Object -Property name
    )
    $normalizedCaseFilter = ""
    if (-not [string]::IsNullOrWhiteSpace($CaseFilter)) {
        $normalizedCaseFilter = (($CaseFilter -split "[,; ]+") |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object { $_.Trim().ToLowerInvariant() } |
            Sort-Object) -join ","
    }

    $payload = [PSCustomObject]@{
        schema_version = 1
        success = $Success
        phase = $Phase
        adapter = $Adapter
        model = $Model
        case_filter = $normalizedCaseFilter
        executed_case_count = $ExecutedCount
        failed_count = $FailedCount
        skipped_count = $SkippedCount
        missing_env = @($MissingNames | Sort-Object)
        results = $stableResults
        skipped_results = $stableSkipped
    }

    $lines = @()
    $lines += "xllm real provider stable summary v1"
    $lines += ("success={0}" -f $Success.ToString().ToLowerInvariant())
    $lines += ("phase={0}" -f $Phase)
    $lines += ("adapter={0}" -f $Adapter)
    $lines += ("model={0}" -f $Model)
    $lines += ("case_filter={0}" -f $normalizedCaseFilter)
    $lines += ("executed_case_count={0}" -f $ExecutedCount)
    $lines += ("failed_count={0}" -f $FailedCount)
    $lines += ("skipped_count={0}" -f $SkippedCount)
    if ($MissingNames.Count -gt 0) {
        $lines += "missing_env:"
        foreach ($name in ($MissingNames | Sort-Object)) {
            $lines += ("- {0}" -f $name)
        }
    }
    if ($stableResults.Count -gt 0) {
        $lines += "results:"
        foreach ($result in $stableResults) {
            $lines += ("- {0}: outcome={1} status={2} error={3} finish={4} stream={5} multimodal={6} tool={7} artifacts={8}" -f
                $result.name,
                $result.outcome,
                $result.status,
                $result.error_code,
                $result.finish_reason,
                $result.stream_mode,
                $result.multimodal.ToString().ToLowerInvariant(),
                $result.tool_enabled.ToString().ToLowerInvariant(),
                $result.artifact_part_count)
        }
    }
    if ($stableSkipped.Count -gt 0) {
        $lines += "skipped:"
        foreach ($skipped in $stableSkipped) {
            $lines += ("- {0}: {1}" -f $skipped.name, $skipped.reason)
        }
    }

    $lines | Set-Content -Path $Path -Encoding UTF8
    $payload | ConvertTo-Json -Depth 8 | Set-Content -Path $JsonPath -Encoding UTF8
}

function Write-PhaseFailureReport {
    param(
        [string]$Phase,
        [string]$Message,
        [string]$ReportPath,
        [string]$ReportJsonPath,
        [string]$StableReportPath = "",
        [string]$StableReportJsonPath = "",
        [string]$OutputDir,
        [string]$Adapter = "",
        [string]$BaseUrl = "",
        [string]$Model = "",
        [string]$CaseFilter = "",
        [int]$DurationMs = 0,
        [string[]]$MissingNames = @(),
        $ProxyPreflight = $null
    )

    $lines = @()
    $payload = [PSCustomObject]@{
        success = $false
        phase = $Phase
        error_code = "invalid_request"
        message = $Message
        adapter = $Adapter
        base_url = $BaseUrl
        model = $Model
    case_filter = $CaseFilter
    output_dir = $OutputDir
    case_count = 0
    executed_case_count = 0
    failed_count = 0
    skipped_count = 0
    total_duration_ms = $DurationMs
    generated_at = (Get-Date).ToString("o")
    proxy_preflight = $ProxyPreflight
    missing_env = $MissingNames
    results = @()
    skipped_results = @()
    }

    $lines += "xllm real provider probe matrix"
    $lines += ("phase={0}" -f $Phase)
    $lines += "status=failed"
    $lines += ("message={0}" -f $Message)
    $lines += ("adapter={0}" -f $Adapter)
    $lines += ("base_url={0}" -f $BaseUrl)
    $lines += ("model={0}" -f $Model)
    $lines += ("case_filter={0}" -f $CaseFilter)
    $lines += ("output_dir={0}" -f $OutputDir)
    $lines += ("duration_ms={0}" -f $DurationMs)
    if ($MissingNames.Count -gt 0) {
        $lines += ""
        $lines += "missing:"
        foreach ($name in $MissingNames) {
            $lines += ("- {0}" -f $name)
        }
    }
    if ($ProxyPreflight -ne $null) {
        $lines += ""
        $lines += "proxy_preflight_detail:"
        $lines += ("  kind: {0}" -f $ProxyPreflight.kind)
        $lines += ("  host: {0}" -f $ProxyPreflight.host)
        $lines += ("  port: {0}" -f $ProxyPreflight.port)
        $lines += ("  test_url: {0}" -f $ProxyPreflight.test_url)
        $lines += ("  ok: {0}" -f $ProxyPreflight.ok)
        $lines += ("  exit_code: {0}" -f $ProxyPreflight.exit_code)
        $lines += ("  message: {0}" -f $ProxyPreflight.message)
    }

    $lines | Set-Content -Path $ReportPath -Encoding UTF8
    $payload | ConvertTo-Json -Depth 6 | Set-Content -Path $ReportJsonPath -Encoding UTF8
    if (-not [string]::IsNullOrWhiteSpace($StableReportPath) -and -not [string]::IsNullOrWhiteSpace($StableReportJsonPath)) {
        Write-StableProbeSummary `
            -Path $StableReportPath `
            -JsonPath $StableReportJsonPath `
            -Success $false `
            -Phase $Phase `
            -Adapter $Adapter `
            -Model $Model `
            -CaseFilter $CaseFilter `
            -ExecutedCount 0 `
            -FailedCount 0 `
            -SkippedCount 0 `
            -MissingNames $MissingNames
    }
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
Set-Location $repoRoot
$buildScript = Join-Path $repoRoot "tests\smoke\scripts\build_real_provider_probe_example.bat"
$exePath = Join-Path $repoRoot "build\real_provider_probe.exe"
$scriptStartedAt = Get-Date
$outputDirRaw = $OutputDir
if ([string]::IsNullOrWhiteSpace($outputDirRaw)) {
    $outputDirRaw = Get-OptionalEnv "XLLM_REAL_OUTPUT_DIR"
}
$outputDir = if ([string]::IsNullOrWhiteSpace($outputDirRaw)) {
    Join-Path $repoRoot "build"
} elseif ([System.IO.Path]::IsPathRooted($outputDirRaw.Trim())) {
    $outputDirRaw.Trim()
} else {
    Join-Path $repoRoot $outputDirRaw.Trim()
}
$reportPath = Join-Path $outputDir "real_provider_probe_report.txt"
$reportJsonPath = Join-Path $outputDir "real_provider_probe_report.json"
$stableReportPath = Join-Path $outputDir "real_provider_probe_stable_summary.txt"
$stableReportJsonPath = Join-Path $outputDir "real_provider_probe_stable_summary.json"
$logDir = Join-Path $outputDir "real_provider_probe_logs"
$caseFilterRaw = $CaseFilter
if ([string]::IsNullOrWhiteSpace($caseFilterRaw)) {
    $caseFilterRaw = Get-OptionalEnv "XLLM_REAL_CASE_FILTER"
}
$caseFilter = Get-CaseFilterSet -RawInput $caseFilterRaw
$adapterName = Get-OptionalEnv "XLLM_REAL_ADAPTER"
$baseUrl = Get-OptionalEnv "XLLM_REAL_BASE_URL"
$modelName = Get-OptionalEnv "XLLM_REAL_MODEL"
$proxyPreflight = Invoke-ProxyPreflight
$proxyPreflightOnly = Get-BoolEnv "XLLM_REAL_PROXY_PREFLIGHT_ONLY" $false

if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$missingRequired = @()
foreach ($name in @("XLLM_REAL_ADAPTER", "XLLM_REAL_BASE_URL", "XLLM_REAL_MODEL")) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        $missingRequired += $name
    }
}

if ($missingRequired.Count -gt 0) {
    if ($proxyPreflightOnly) {
        if ($proxyPreflight -eq $null) {
            Write-PhaseFailureReport `
                -Phase "proxy_preflight" `
                -Message "Proxy preflight only mode requires XLLM_REAL_PROXY_KIND, XLLM_REAL_PROXY_HOST, and XLLM_REAL_PROXY_PORT." `
                -ReportPath $reportPath `
                -ReportJsonPath $reportJsonPath `
                -StableReportPath $stableReportPath `
                -StableReportJsonPath $stableReportJsonPath `
                -OutputDir $outputDir `
                -Adapter $adapterName `
                -BaseUrl $baseUrl `
                -Model $modelName `
                -CaseFilter $caseFilterRaw `
                -DurationMs ([int][Math]::Round(((Get-Date) - $scriptStartedAt).TotalMilliseconds)) `
                -MissingNames @("XLLM_REAL_PROXY_KIND", "XLLM_REAL_PROXY_HOST", "XLLM_REAL_PROXY_PORT")
            [Console]::Error.WriteLine("Proxy preflight only mode requires XLLM_REAL_PROXY_KIND, XLLM_REAL_PROXY_HOST, and XLLM_REAL_PROXY_PORT")
            Write-Host "report: $reportPath"
            Write-Host "report json: $reportJsonPath"
            exit 1
        }

        $proxyOnlyLines = @()
        $proxyOnlyLines += "xllm real provider proxy preflight"
        $proxyOnlyLines += "phase=proxy_preflight"
        $proxyOnlyLines += ("status={0}" -f $(if ($proxyPreflight.ok) { "ok" } else { "failed" }))
        $proxyOnlyLines += ("output_dir={0}" -f $outputDir)
        $proxyOnlyLines += ("duration_ms={0}" -f ([int][Math]::Round(((Get-Date) - $scriptStartedAt).TotalMilliseconds)))
        $proxyOnlyLines += ""
        $proxyOnlyLines += "proxy_preflight_detail:"
        $proxyOnlyLines += ("  kind: {0}" -f $proxyPreflight.kind)
        $proxyOnlyLines += ("  host: {0}" -f $proxyPreflight.host)
        $proxyOnlyLines += ("  port: {0}" -f $proxyPreflight.port)
        $proxyOnlyLines += ("  test_url: {0}" -f $proxyPreflight.test_url)
        $proxyOnlyLines += ("  ok: {0}" -f $proxyPreflight.ok)
        $proxyOnlyLines += ("  exit_code: {0}" -f $proxyPreflight.exit_code)
        $proxyOnlyLines += ("  message: {0}" -f $proxyPreflight.message)

        $proxyOnlyPayload = [PSCustomObject]@{
            success = $proxyPreflight.ok
            phase = "proxy_preflight"
            error_code = $(if ($proxyPreflight.ok) { "" } else { "network" })
            message = $(if ($proxyPreflight.ok) { "Proxy preflight succeeded." } else { "Proxy preflight failed." })
            adapter = $adapterName
            base_url = $baseUrl
            model = $modelName
            case_filter = $caseFilterRaw
            output_dir = $outputDir
            case_count = 0
            executed_case_count = 0
            failed_count = $(if ($proxyPreflight.ok) { 0 } else { 1 })
            skipped_count = 0
            total_duration_ms = [int][Math]::Round(((Get-Date) - $scriptStartedAt).TotalMilliseconds)
            generated_at = (Get-Date).ToString("o")
            proxy_preflight = $proxyPreflight
            results = @()
            skipped_results = @()
        }

        $proxyOnlyLines | Set-Content -Path $reportPath -Encoding UTF8
        $proxyOnlyPayload | ConvertTo-Json -Depth 8 | Set-Content -Path $reportJsonPath -Encoding UTF8
        Write-StableProbeSummary `
            -Path $stableReportPath `
            -JsonPath $stableReportJsonPath `
            -Success $proxyPreflight.ok `
            -Phase "proxy_preflight" `
            -Adapter $adapterName `
            -Model $modelName `
            -CaseFilter $caseFilterRaw `
            -ExecutedCount 0 `
            -FailedCount $(if ($proxyPreflight.ok) { 0 } else { 1 }) `
            -SkippedCount 0
        Write-Host "report: $reportPath"
        Write-Host "report json: $reportJsonPath"
        Write-Host "stable summary: $stableReportPath"
        Write-Host "stable summary json: $stableReportJsonPath"
        exit $(if ($proxyPreflight.ok) { 0 } else { 1 })
    }

    Write-PhaseFailureReport `
        -Phase "setup" `
        -Message "Missing required environment variables." `
        -ReportPath $reportPath `
        -ReportJsonPath $reportJsonPath `
        -StableReportPath $stableReportPath `
        -StableReportJsonPath $stableReportJsonPath `
        -OutputDir $outputDir `
        -Adapter $adapterName `
        -BaseUrl $baseUrl `
        -Model $modelName `
        -CaseFilter $caseFilterRaw `
        -DurationMs ([int][Math]::Round(((Get-Date) - $scriptStartedAt).TotalMilliseconds)) `
        -MissingNames $missingRequired `
        -ProxyPreflight $proxyPreflight
    [Console]::Error.WriteLine("Missing required environment variables: {0}" -f ($missingRequired -join ", "))
    Write-Host "report: $reportPath"
    Write-Host "report json: $reportJsonPath"
    Write-Host "stable summary: $stableReportPath"
    Write-Host "stable summary json: $stableReportJsonPath"
    exit 1
}

cmd /c $buildScript
if ($LASTEXITCODE -ne 0) {
    Write-PhaseFailureReport `
        -Phase "build" `
        -Message "Failed to build real_provider_probe.exe." `
        -ReportPath $reportPath `
        -ReportJsonPath $reportJsonPath `
        -StableReportPath $stableReportPath `
        -StableReportJsonPath $stableReportJsonPath `
        -OutputDir $outputDir `
        -Adapter $adapterName `
        -BaseUrl $baseUrl `
        -Model $modelName `
        -CaseFilter $caseFilterRaw `
        -DurationMs ([int][Math]::Round(((Get-Date) - $scriptStartedAt).TotalMilliseconds)) `
        -ProxyPreflight $proxyPreflight
    [Console]::Error.WriteLine("Failed to build real_provider_probe.exe")
    Write-Host "report: $reportPath"
    Write-Host "report json: $reportJsonPath"
    Write-Host "stable summary: $stableReportPath"
    Write-Host "stable summary json: $stableReportJsonPath"
    exit 1
}

if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}

$cases = @()
$skippedCases = @()
$knownCaseNames = @{}

Add-KnownCaseName -KnownSet $knownCaseNames -Name "text"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "stream"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "json"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "json_schema"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "thinking"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "tool"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "provider_tool"
Add-KnownCaseName -KnownSet $knownCaseNames -Name "multimodal"

$cases += [PSCustomObject]@{
    Name = "text"
    Enabled = $true
    Overrides = @{
        XLLM_REAL_STREAM = "0"
        XLLM_REAL_RESPONSE_FORMAT = "text"
        XLLM_REAL_ENABLE_TOOL = "0"
        XLLM_REAL_PROMPT = "Reply with exactly: pong"
        XLLM_REAL_EXPECT_TEXT_CONTAINS = "pong"
    }
}

if (Get-BoolEnv "XLLM_REAL_RUN_STREAM_CASES" $true) {
    $cases += [PSCustomObject]@{
        Name = "stream"
        Enabled = $true
        Overrides = @{
            XLLM_REAL_STREAM = "prefer"
            XLLM_REAL_RESPONSE_FORMAT = "text"
            XLLM_REAL_ENABLE_TOOL = "0"
            XLLM_REAL_PROMPT = "Reply with exactly: pong"
            XLLM_REAL_EXPECT_TEXT_CONTAINS = "pong"
        }
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "stream") {
    $skippedCases += New-SkippedCaseResult -Name "stream" -Reason "stream case is disabled by XLLM_REAL_RUN_STREAM_CASES"
}

if (Get-BoolEnv "XLLM_REAL_RUN_JSON_CASES" $true) {
    $cases += [PSCustomObject]@{
        Name = "json"
        Enabled = $true
        Overrides = @{
            XLLM_REAL_STREAM = "0"
            XLLM_REAL_RESPONSE_FORMAT = "json"
            XLLM_REAL_BEST_EFFORT_JSON = "0"
            XLLM_REAL_ENABLE_TOOL = "0"
            XLLM_REAL_PROMPT = "Return a JSON object exactly like {""value"":""pong""}."
            XLLM_REAL_EXPECT_JSON_CONTAINS = '"value":"pong"'
        }
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "json") {
    $skippedCases += New-SkippedCaseResult -Name "json" -Reason "json case is disabled by XLLM_REAL_RUN_JSON_CASES"
}

if (Get-BoolEnv "XLLM_REAL_RUN_JSON_SCHEMA_CASES" $false) {
    $cases += [PSCustomObject]@{
        Name = "json_schema"
        Enabled = $true
        Overrides = @{
            XLLM_REAL_STREAM = "0"
            XLLM_REAL_RESPONSE_FORMAT = "json_schema"
            XLLM_REAL_JSON_SCHEMA = '{"type":"object","properties":{"value":{"type":"string"}},"required":["value"]}'
            XLLM_REAL_ENABLE_TOOL = "0"
            XLLM_REAL_PROMPT = "Return a JSON object with a string field named value set to pong."
            XLLM_REAL_EXPECT_JSON_CONTAINS = '"value":"pong"'
        }
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "json_schema") {
    $skippedCases += New-SkippedCaseResult -Name "json_schema" -Reason "json_schema case is disabled by XLLM_REAL_RUN_JSON_SCHEMA_CASES"
}

if (Get-BoolEnv "XLLM_REAL_RUN_THINKING_CASES" $false) {
    $cases += [PSCustomObject]@{
        Name = "thinking"
        Enabled = $true
        Overrides = @{
            XLLM_REAL_STREAM = "prefer"
            XLLM_REAL_REASONING = "medium"
            XLLM_REAL_EXPOSE_THINKING = "1"
            XLLM_REAL_ENABLE_TOOL = "0"
            XLLM_REAL_PROMPT = "Answer briefly with pong."
            XLLM_REAL_EXPECT_TEXT_CONTAINS = "pong"
        }
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "thinking") {
    $skippedCases += New-SkippedCaseResult -Name "thinking" -Reason "thinking case is disabled by XLLM_REAL_RUN_THINKING_CASES"
}

if (Get-BoolEnv "XLLM_REAL_RUN_TOOL_CASES" $false) {
    $cases += [PSCustomObject]@{
        Name = "tool"
        Enabled = $true
        Overrides = @{
            XLLM_REAL_STREAM = "0"
            XLLM_REAL_ENABLE_TOOL = "1"
            XLLM_REAL_TOOL_REQUIRED = "1"
            XLLM_REAL_TOOL_RESULT_TEXT = "tool-pong"
            XLLM_REAL_PROMPT = "Use the provided tool once and then answer with its result."
            XLLM_REAL_EXPECT_TEXT_CONTAINS = "tool-pong"
        }
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "tool") {
    $skippedCases += New-SkippedCaseResult -Name "tool" -Reason "tool case is disabled by XLLM_REAL_RUN_TOOL_CASES"
}

if (Get-BoolEnv "XLLM_REAL_RUN_PROVIDER_TOOL_CASES" $false) {
    $supportsProviderToolCase = (
        $adapterName -eq "openai_compat" -or
        $adapterName -eq "anthropic_native" -or
        $adapterName -eq "ollama_native"
    )

    if ($supportsProviderToolCase) {
        $cases += [PSCustomObject]@{
            Name = "provider_tool"
            Enabled = $true
            Overrides = @{
                XLLM_REAL_STREAM = "0"
                XLLM_REAL_ENABLE_TOOL = "1"
                XLLM_REAL_TOOL_KIND = "provider"
                XLLM_REAL_TOOL_REQUIRED = "0"
                XLLM_REAL_PROMPT = "Use the provided built-in provider tool if available, then answer in one short sentence."
            }
        }
    } else {
        $skipReason = "provider_tool case is only supported for openai_compat, anthropic_native, and ollama_native"
        Write-Host ("skip provider_tool case for adapter {0}: {1}" -f $adapterName, $skipReason)
        $skippedCases += New-SkippedCaseResult -Name "provider_tool" -Reason $skipReason
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "provider_tool") {
    $skippedCases += New-SkippedCaseResult -Name "provider_tool" -Reason "provider_tool case is disabled by XLLM_REAL_RUN_PROVIDER_TOOL_CASES"
}

if (Get-BoolEnv "XLLM_REAL_RUN_MULTIMODAL_CASES" $false) {
    $hasMultimodalInput =
        -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable("XLLM_REAL_IMAGE_URL")) -or
        -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable("XLLM_REAL_IMAGE_PATH")) -or
        -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable("XLLM_REAL_IMAGE_FILE_ID")) -or
        -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable("XLLM_REAL_FILE_URL")) -or
        -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable("XLLM_REAL_FILE_PATH")) -or
        -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable("XLLM_REAL_FILE_FILE_ID"))

    if ($hasMultimodalInput) {
        $cases += [PSCustomObject]@{
            Name = "multimodal"
            Enabled = $true
            Overrides = @{
                XLLM_REAL_STREAM = "0"
                XLLM_REAL_ENABLE_TOOL = "0"
            }
        }
    } else {
        $skipReason = "multimodal case requires at least one image or file input environment variable"
        Write-Host ("skip multimodal case: {0}" -f $skipReason)
        $skippedCases += New-SkippedCaseResult -Name "multimodal" -Reason $skipReason
    }
} elseif (Test-CaseRequested -FilterSet $caseFilter -Name "multimodal") {
    $skippedCases += New-SkippedCaseResult -Name "multimodal" -Reason "multimodal case is disabled by XLLM_REAL_RUN_MULTIMODAL_CASES"
}

if ($caseFilter -ne $null) {
    foreach ($requestedCaseName in $caseFilter.Keys) {
        if (-not $knownCaseNames.ContainsKey($requestedCaseName)) {
            $skippedCases += New-SkippedCaseResult -Name $requestedCaseName -Reason "unknown case name in XLLM_REAL_CASE_FILTER"
        }
    }
}

$results = @()
foreach ($case in $cases) {
    if (-not $case.Enabled) {
        continue
    }
    if ($caseFilter -ne $null -and -not $caseFilter.ContainsKey($case.Name.ToLowerInvariant())) {
        continue
    }
    $results += Invoke-ProbeCase -Name $case.Name -Overrides $case.Overrides -ExePath $exePath -LogDir $logDir
}

$failedResults = @($results | Where-Object { $_.ExitCode -ne 0 })
$failedCount = $failedResults.Count
$totalDurationMs = ($results | Measure-Object -Property DurationMs -Sum).Sum
if ($null -eq $totalDurationMs) {
    $totalDurationMs = 0
}
$displayTotalDurationMs = [int]$totalDurationMs

$lines = @()
$lines += "xllm real provider probe matrix"
$lines += ("adapter={0}" -f $adapterName)
$lines += ("base_url={0}" -f $baseUrl)
$lines += ("model={0}" -f $modelName)
$lines += ("case_filter={0}" -f $caseFilterRaw)
$lines += ("output_dir={0}" -f $outputDir)
$lines += ("proxy_preflight={0}" -f $(if ($proxyPreflight -eq $null) { "not_configured" } elseif ($proxyPreflight.ok) { "ok" } else { "failed" }))
$lines += ("executed_case_count={0}" -f $results.Count)
$lines += ("skipped_count={0}" -f $skippedCases.Count)
$lines += ("failed_count={0}" -f $failedCount)
$lines += ("total_duration_ms={0}" -f $displayTotalDurationMs)
$lines += ""
if ($proxyPreflight -ne $null) {
    $lines += "proxy_preflight_detail:"
    $lines += ("  kind: {0}" -f $proxyPreflight.kind)
    $lines += ("  host: {0}" -f $proxyPreflight.host)
    $lines += ("  port: {0}" -f $proxyPreflight.port)
    $lines += ("  test_url: {0}" -f $proxyPreflight.test_url)
    $lines += ("  ok: {0}" -f $proxyPreflight.ok)
    $lines += ("  exit_code: {0}" -f $proxyPreflight.exit_code)
    $lines += ("  message: {0}" -f $proxyPreflight.message)
    $lines += ""
}
$lines += ""
$lines += "results:"
foreach ($result in $results) {
    $statusLabel = if ($result.ExitCode -eq 0) { "ok" } else { "failed" }
    $lines += ("- {0}: {1}" -f $result.Name, $statusLabel)
    $lines += ("  duration_ms: {0}" -f $result.DurationMs)
    $lines += ("  log: {0}" -f $result.LogPath)
    $lines += ("  summary: {0}" -f $result.SummaryPath)
    if ($result.Summary -ne $null) {
        $lines += ("  adapter: {0}" -f $result.Summary.adapter)
        $lines += ("  model: {0}" -f $result.Summary.model)
        $lines += ("  status: {0}" -f $result.Summary.status)
        $lines += ("  finish_reason: {0}" -f $result.Summary.finish_reason)
        $lines += ("  error_code: {0}" -f $result.Summary.error_code)
        $lines += ("  error_message: {0}" -f $result.Summary.error_message)
    }
}
if ($skippedCases.Count -gt 0) {
    $lines += ""
    $lines += "skipped:"
    foreach ($skipped in $skippedCases) {
        $lines += ("- {0}: {1}" -f $skipped.Name, $skipped.Reason)
    }
}
$reportObject = [PSCustomObject]@{
    success = ($failedCount -eq 0)
    adapter = $adapterName
    base_url = $baseUrl
    model = $modelName
    case_filter = $caseFilterRaw
    output_dir = $outputDir
    case_count = $results.Count
    executed_case_count = $results.Count
    failed_count = $failedCount
    skipped_count = $skippedCases.Count
    total_duration_ms = $displayTotalDurationMs
    generated_at = (Get-Date).ToString("o")
    proxy_preflight = $proxyPreflight
    results = @(
        $results | ForEach-Object {
            [PSCustomObject]@{
                name = $_.Name
                exit_code = $_.ExitCode
                duration_ms = $_.DurationMs
                log_path = $_.LogPath
                summary_path = $_.SummaryPath
                status = if ($_.Summary -ne $null) { $_.Summary.status } else { $null }
                error_code = if ($_.Summary -ne $null) { $_.Summary.error_code } else { $null }
                error_message = if ($_.Summary -ne $null) { $_.Summary.error_message } else { $null }
                summary = $_.Summary
            }
        }
    )
    skipped_results = @(
        $skippedCases | ForEach-Object {
            [PSCustomObject]@{
                name = $_.Name
                reason = $_.Reason
            }
        }
    )
}

$lines | Set-Content -Path $reportPath -Encoding UTF8
$reportObject | ConvertTo-Json -Depth 10 | Set-Content -Path $reportJsonPath -Encoding UTF8
Write-StableProbeSummary `
    -Path $stableReportPath `
    -JsonPath $stableReportJsonPath `
    -Success ($failedCount -eq 0) `
    -Phase "matrix" `
    -Adapter $adapterName `
    -Model $modelName `
    -CaseFilter $caseFilterRaw `
    -ExecutedCount $results.Count `
    -FailedCount $failedCount `
    -SkippedCount $skippedCases.Count `
    -Results $results `
    -SkippedCases $skippedCases
Write-Host ""
Write-Host "matrix complete"
Write-Host ("cases: {0}, skipped: {1}, failed: {2}, total_duration_ms: {3}" -f $results.Count, $skippedCases.Count, $failedCount, $displayTotalDurationMs)
if ($proxyPreflight -ne $null) {
    Write-Host ("proxy preflight: {0} kind={1} host={2}:{3}" -f $(if ($proxyPreflight.ok) { "ok" } else { "failed" }), $proxyPreflight.kind, $proxyPreflight.host, $proxyPreflight.port)
}
if ($failedResults.Count -gt 0) {
    Write-Host "failed cases:"
    foreach ($failed in $failedResults) {
        $summaryStatus = if ($failed.Summary -ne $null -and -not [string]::IsNullOrWhiteSpace($failed.Summary.status)) {
            $failed.Summary.status
        } else {
            ""
        }
        $summaryError = if ($failed.Summary -ne $null -and -not [string]::IsNullOrWhiteSpace($failed.Summary.error_code)) {
            $failed.Summary.error_code
        } else {
            ""
        }
        $summaryMessage = if ($failed.Summary -ne $null -and -not [string]::IsNullOrWhiteSpace($failed.Summary.error_message)) {
            $failed.Summary.error_message
        } else {
            ""
        }
        Write-Host ("- {0}: exit={1} duration_ms={2} status={3} error={4} message={5}" -f $failed.Name, $failed.ExitCode, $failed.DurationMs, $summaryStatus, $summaryError, $summaryMessage)
    }
}
Write-Host "report: $reportPath"
Write-Host "report json: $reportJsonPath"
Write-Host "stable summary: $stableReportPath"
Write-Host "stable summary json: $stableReportJsonPath"

if ($failedCount -gt 0) {
    exit 1
}
