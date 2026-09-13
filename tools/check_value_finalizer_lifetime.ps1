param([string]$Gcc='gcc', [string]$Tcc='E:/software/tcc/tcc.exe',
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Keep prior evidence; choose a fresh directory' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $files=@(Get-ChildItem src,include,single,config -Recurse -File)
    $files+=@(Get-Item tests/value/test_value_finalizer_lifetime.c,tests/single/test_single_value_finalizer_lifetime.c,
        tests/test.h,tools/build.py,tools/amalgamate.py,$PSCommandPath)
    $inputs=@($files | Sort-Object FullName -Unique | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $lanes=@()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        $name=$compiler.Name
        & python tools/build.py --compiler $compiler.Path --suite value_finalizer_lifetime_tests --no-examples --rebuild *> "$outputPath/$name.log"
        $code=$LASTEXITCODE
        $tests=@(Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[test\]').Count
        $markers=@(Select-String -LiteralPath "$outputPath/$name.log" -SimpleMatch 'Value finalizer lifetime: 501 lifetimes, 100 four-thread releases, 2400 binding refusals, 4800 graph probes, 1600 actual failures').Count
        $complete=[bool](Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[pass\]' -Quiet)
        $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Tests=$tests;Markers=$markers;Complete=$complete}
        Write-Output "$name exit=$code layouts=$tests markers=$markers"
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $artifacts=@(Get-ChildItem out/gcc/native/value_finalizer_lifetime_tests,out/tcc/native/value_finalizer_lifetime_tests -File -Filter '*.exe' -ErrorAction SilentlyContinue | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;Artifacts=$artifacts;
        Scope='Atomic last-backing finalization; actual receiver shells held by finalizer-backed cursors; owned context released after fields; exact graph, refusal/OOM, concurrency and allocator assertions. No production graph safepoint, module cycle collection or automatic language code-pin claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($lanes | Where-Object {$_.Exit -ne 0 -or $_.Tests -ne 2 -or $_.Markers -ne 2 -or !$_.Complete}).Count) { throw 'Finalizer batch failed; inspect both compiler logs' }
} finally { Pop-Location }
