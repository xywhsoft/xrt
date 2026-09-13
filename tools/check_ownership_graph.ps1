param([string]$Gcc='gcc', [Parameter(Mandatory=$true)][string]$Tcc,
    [string]$Directory='out/ownership_graph_v149')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Keep previous graph evidence; choose a fresh directory' }
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    $files=@(Get-ChildItem src,include,single,config,extlibs/xruntime/src,extlibs/xruntime/include,
        extlibs/xruntime/single,extlibs/xruntime/config -Recurse -File)
    $files+=@(Get-Item tests/value/test_ownership_graph.c,tests/single/test_single_ownership.c,
        extlibs/xruntime/tests/test_runtime_ownership.c,extlibs/xruntime/tests/single/test_single_runtime_ownership.c,
        tools/build.py,tools/amalgamate.py,$PSCommandPath)
    $inputs=@($files|Sort-Object FullName -Unique|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash}
    })
    $lanes=@()
    foreach($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        foreach($suite in @('ownership_graph_tests','runtime_ownership_tests')) {
            $name=$compiler.Name+'-'+$suite
            $arguments=@('tools/build.py','--compiler',$compiler.Path,'--suite',$suite,'--no-examples','--rebuild')
            if($suite -eq 'runtime_ownership_tests') { $arguments+=@('--manifest','extlibs/xruntime/config/modules.json') }
            & python @arguments *> "$outputPath/$name.log"
            $code=$LASTEXITCODE
            $testCount=@(Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[test\]').Count
            $complete=[bool](Select-String -LiteralPath "$outputPath/$name.log" -Pattern '^\[pass\]' -Quiet)
            $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Tests=$testCount;Passed=($code -eq 0 -and $testCount -eq 2 -and $complete);Log="$outputPath/$name.log"}
            Write-Output "$name exit=$code layouts=$testCount"
        }
    }
    & python tools/build.py --compiler $Gcc --suite core,value_graph,value_collection,runtime_value_object,runtime_value_callable,runtime_object_graph `
        --manifest extlibs/xruntime/config/modules.json --no-single --no-examples --rebuild *> "$outputPath/existing.log"
    $existingCode=$LASTEXITCODE
    $existingCount=@(Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[test\]').Count
    $existingComplete=[bool](Select-String -LiteralPath "$outputPath/existing.log" -Pattern '^\[pass\]' -Quiet)
    $unchanged=@($inputs|Where-Object {(Get-FileHash $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $artifacts=@(Get-ChildItem out/gcc/native/ownership_graph_tests,out/tcc/native/ownership_graph_tests,
        out/gcc/native/runtime_ownership_tests,out/tcc/native/runtime_ownership_tests -File -Filter '*.exe'|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash}
    })
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Lanes=$lanes;Inputs=$inputs;InputsUnchanged=$unchanged;
        Artifacts=$artifacts;Existing=[pscustomobject]@{Exit=$existingCode;Tests=$existingCount;Complete=$existingComplete};
        Scope='Complete physical graph inspection and native object cycle collection with Value/COW aliases. No language module automatic lifetime admission, concurrent safepoint or generated-code drop lease claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or @($lanes|Where-Object {!$_.Passed}).Count -or $existingCode -ne 0 -or !$existingComplete -or $existingCount -ne 24) {
        throw 'Graph batch failed; inspect all lane logs'
    }
    Write-Output 'Graph batch passed: 8 new compiler/layout test processes; original XRT regression denominator 24/24'
} finally { Pop-Location }
