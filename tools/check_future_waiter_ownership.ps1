param([string]$Gcc='gcc', [string]$Tcc='E:/software/tcc/tcc.exe',
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Use a fresh evidence directory' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $files=@(Get-ChildItem src,include,single,config,tests -Recurse -File)
    $files+=@(Get-Item tools/build.py,$PSCommandPath)
    $inputs=@($files|Sort-Object FullName -Unique|ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash} })
    $lanes=@()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        foreach ($suite in @('future_waiter_ownership_tests','future_ownership_tests','task_result_ownership_tests')) {
            $name="$($compiler.Name)-$suite"
            & python tools/build.py --compiler $compiler.Path --suite $suite --no-examples --rebuild *> "$outputPath/$name.log"
            $code=$LASTEXITCODE
            $tests=@(Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[test\]').Count
            $complete=[bool](Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[pass\]' -Quiet)
            $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Tests=$tests;Expected=2;Passed=($code -eq 0 -and $tests -eq 2 -and $complete)}
            Write-Output "$name tests=$tests/2 exit=$code"
        }
    }
    & python tools/build.py --compiler $Gcc --suite core,cancel,future,future_continue,future_combine,task,task_pool,atomic --no-single --no-examples --rebuild *> "$outputPath/existing.log"
    $existingCode=$LASTEXITCODE
    $existingCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\]').Count
    $atomicCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\].*[/\\]test_atomic(_threads)?\.exe$').Count
    $originalCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\]' | Where-Object {$_.Line -notmatch '[/\\]test_atomic(_threads)?\.exe$'}).Count
    $existingComplete=[bool](Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[pass\]' -Quiet)
    $unchanged=@($inputs|Where-Object {(Get-FileHash $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;
        Existing=[pscustomobject]@{Exit=$existingCode;Tests=$originalCount;Expected=26;AtomicSupportTests=$atomicCount;TotalProcesses=$existingCount;Complete=$existingComplete};
        WatchTransfersPerNewLane=300;AggregateGraphsPerNewLane=500;PendingOomProbesPerNewLane=2000;
        Scope='Exact traced Watch releases and shared Any/All/Race nodes, duplicates, pending operation roots, cancellation registrations, settled values and rollback. Old unknown waiters remain opaque. No concurrent safepoint, automatic module pin or complete language frame/aggregate adapter claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or $lanes.Count -ne 6 -or @($lanes|Where-Object {!$_.Passed}).Count -or $existingCode -ne 0 -or !$existingComplete -or $originalCount -ne 26 -or $atomicCount -ne 2 -or $existingCount -ne 28) { throw 'Future waiter ownership batch failed; inspect logs' }
    Write-Output 'Future waiter ownership passed: four new layout/compiler processes, eight prior graph processes, original 26/26 and two Atomic support checks'
} finally { Pop-Location }
