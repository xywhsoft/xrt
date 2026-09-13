param([string]$Gcc='gcc', [string]$Tcc='E:/software/tcc/tcc.exe',
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Use a fresh evidence directory' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $files=@(Get-ChildItem src,include,single,config,tests,extlibs/xruntime/src,extlibs/xruntime/include,extlibs/xruntime/single,extlibs/xruntime/config,extlibs/xruntime/tests -Recurse -File)
    $files+=@(Get-Item tools/build.py,$PSCommandPath)
    $inputs=@($files|Sort-Object FullName -Unique|ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash} })
    $lanes=@()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        foreach ($suite in @('task_result_ownership_tests','future_ownership_tests','runtime_value_future')) {
            $name="$($compiler.Name)-$suite"
            $extra = if ($suite -eq 'runtime_value_future') { @('--manifest','extlibs/xruntime/config/modules.json') } else { @() }
            & python tools/build.py @extra --compiler $compiler.Path --suite $suite --no-examples --rebuild *> "$outputPath/$name.log"
            $code=$LASTEXITCODE
            $tests=@(Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[test\]').Count
            $expected=2
            $complete=[bool](Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[pass\]' -Quiet)
            $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Tests=$tests;Expected=$expected;Passed=($code -eq 0 -and $tests -eq $expected -and $complete)}
            Write-Output "$name tests=$tests/$expected exit=$code"
        }
        # This existing single-header fixture is not in the module's
        # single_tests manifest. Compile and execute it explicitly; do not
        # count the modular suite's two processes as three layouts.
        $name="$($compiler.Name)-runtime_value_future_single"; $binary="$outputPath/$name.exe"
        [string[]]$crt=@()
        if ($compiler.Name -eq 'tcc') { $crt=@('C:/Windows/System32/ucrtbase.dll') }
        & $compiler.Path -std=c11 -DXRT_MODULE_ALL -DXRUNTIME_MODULE_ALL extlibs/xruntime/tests/single/test_single_runtime_value_future.c @crt -lws2_32 -liphlpapi -lshell32 -lole32 -ladvapi32 -o $binary *> "$outputPath/$name.build.log"
        $buildExit=$LASTEXITCODE; $runExit=$null
        if ($buildExit -eq 0) { & $binary *> "$outputPath/$name.run.log"; $runExit=$LASTEXITCODE }
        $lanes+=[pscustomobject]@{Name=$name;Exit=$runExit;Build=$buildExit;Tests=1;Expected=1;Passed=($buildExit -eq 0 -and $runExit -eq 0)}
        Write-Output "$name build=$buildExit run=$runExit"
    }
    # Existing task-pool barrier tests use Atomic explicitly. Include that
    # test dependency and report its two checks separately from the fixed 26.
    & python tools/build.py --compiler $Gcc --suite core,cancel,future,future_continue,future_combine,task,task_pool,atomic --no-single --no-examples --rebuild *> "$outputPath/existing.log"
    $existingCode=$LASTEXITCODE
    $existingCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\]').Count
    $atomicCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\].*[/\\]test_atomic(_threads)?\.exe$').Count
    $originalCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\]' | Where-Object {$_.Line -notmatch '[/\\]test_atomic(_threads)?\.exe$'}).Count
    $existingComplete=[bool](Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[pass\]' -Quiet)
    $unchanged=@($inputs|Where-Object {(Get-FileHash $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;
        Existing=[pscustomobject]@{Exit=$existingCode;Tests=$originalCount;Expected=26;AtomicSupportTests=$atomicCount;TotalProcesses=$existingCount;Complete=$existingComplete};
        Scope='ABI-preserving atomic task result Trace; joined-worker graphs, native aliases, accepted/failed/cancelled/borrowed results and submission rollback. Existing Future graph and ValueFuture wrappers independently tested. No pending frame/Watch ownership, graph safepoint or automatic module residency claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or $lanes.Count -ne 8 -or @($lanes|Where-Object {!$_.Passed}).Count -or $existingCode -ne 0 -or !$existingComplete -or $originalCount -ne 26 -or $atomicCount -ne 2 -or $existingCount -ne 28) { throw 'Task result ownership batch failed; inspect logs' }
    Write-Output 'Task result ownership batch passed: 14 compiler/layout processes, existing 26/26 and two Atomic test-dependency checks'
} finally { Pop-Location }
