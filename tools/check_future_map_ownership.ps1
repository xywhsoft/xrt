param([Parameter(Mandatory=$true)][string]$Directory,
    [string]$Gcc='gcc', [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Use a new evidence directory' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $records=@(Get-ChildItem src,include,single,config,tests -Recurse -File)+@(Get-Item tools/build.py,$PSCommandPath,tools/check_future_waiter_ownership.ps1)
    $inputs=@($records|Sort-Object FullName -Unique|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    $lanes=@()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        $name=$compiler.Name
        & python tools/build.py --compiler $compiler.Path --suite future_map_ownership_tests --no-examples --rebuild *> "$outputPath/$name.log"
        $code=$LASTEXITCODE
        $tests=@(Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[test\]').Count
        $complete=[bool](Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[pass\]' -Quiet)
        $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Tests=$tests;Expected=2;Complete=$complete}
        Write-Output "$name mapped group tests=$tests/2 exit=$code"
    }
    $previousRelative=[IO.Path]::GetRelativePath((Get-Location).Path, "$outputPath/previous")
    & tools/check_future_waiter_ownership.ps1 -Gcc $Gcc -Tcc $Tcc -Directory $previousRelative
    $previous=Get-Content -LiteralPath "$outputPath/previous/results.json" -Raw | ConvertFrom-Json
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;
        PreviousGraphProcesses=12;Existing=$previous.Existing;LifecycleCasesPerNewProcess=1000;LaunchProbesPerNewProcess=9600;
        Scope='Single activation for mapped Any/All/Race, exact context and result ownership, source-preserving preparation rollback, accepted cancellation and synchronous mapper completion. No language frame safepoint or automatic code pin claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$previous.InputsUnchanged -or @($lanes|Where-Object {$_.Exit -ne 0 -or $_.Tests -ne 2 -or !$_.Complete}).Count) { throw 'Mapped Future batch failed' }
    Write-Output 'Mapped Future batch passed: four new processes, previous 12/12 and original 26/26 plus Atomic 2'
} finally { Pop-Location }
