param([string]$Gcc='gcc', [string]$Tcc='E:/software/tcc/tcc.exe',
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Use a fresh evidence directory' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $files=@(Get-ChildItem src,include,single,config -Recurse -File)
    $files+=@(Get-Item tests/concurrency/test_future_ownership.c,tests/single/test_single_future_ownership.c,tools/build.py,$PSCommandPath)
    $inputs=@($files|Sort-Object FullName -Unique|ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash} })
    $lanes=@()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        $name=$compiler.Name
        & python tools/build.py --compiler $compiler.Path --suite future_ownership_tests --no-examples --rebuild *> "$outputPath/$name.log"
        $code=$LASTEXITCODE
        $tests=@(Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[test\]').Count
        $complete=[bool](Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[pass\]' -Quiet)
        $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Tests=$tests;Passed=($code -eq 0 -and $tests -eq 2 -and $complete)}
        Write-Output "$name new layouts=$tests exit=$code"
    }
    & python tools/build.py --compiler $Gcc --suite core,cancel,future,future_continue,future_combine --no-single --no-examples --rebuild *> "$outputPath/existing.log"
    $existingCode=$LASTEXITCODE
    $existingCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\]').Count
    $existingComplete=[bool](Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[pass\]' -Quiet)
    $unchanged=@($inputs|Where-Object {(Get-FileHash $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;
        Existing=[pscustomobject]@{Exit=$existingCode;Tests=$existingCount;Complete=$existingComplete};
        Scope='Physical Future/Promise, cancellation parents, error causes, traced result/context and forwarding. Unknown waiters/owned payload fail closed. Quiescence and code residency remain caller guarantees; no automatic language-module lifetime claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($lanes|Where-Object {!$_.Passed}).Count -or $existingCode -ne 0 -or !$existingComplete -or $existingCount -ne 19) { throw 'Future ownership batch failed; inspect logs' }
    Write-Output 'Future ownership passed: four new compiler/layout processes and original 19/19 regression processes'
} finally { Pop-Location }
