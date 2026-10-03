param([string]$Gcc='gcc', [string]$Tcc='E:/software/tcc/tcc.exe',
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve previous discovery evidence; use a fresh directory' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    & python tools/amalgamate.py --check *> "$outPath/generated-before.log"
    if ($LASTEXITCODE) { throw 'Generated XRT must be current before freezing inputs' }
    $files=@(Get-ChildItem src,include,single,config -Recurse -File)
    $files+=@(Get-ChildItem tests/value,tests/single -File | Where-Object Name -match 'discovery|lifetime|construction|publication|ownership|cursor_lifecycle')
    $files+=@(Get-Item tests/test.h,tools/build.py,tools/amalgamate.py,$PSCommandPath,tools/check_value_object_discovery.sh)
    $files+=@(Get-Item examples/value/discovery/main.c)
    $inputs=@($files | Sort-Object FullName -Unique | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $suite='value_object_discovery_tests,value_lifetime_copy_tests,value_finalizer_construction_tests,value_finalizer_publication_tests,value_finalizer_lifetime_tests,value_iterator_ownership_tests,ownership_adapter_tests'
    $lanes=@()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        $name=$compiler.Name
        & python tools/build.py --compiler $compiler.Path --suite $suite --rebuild *> "$outPath/$name.log"
        $code=$LASTEXITCODE
        $tests=@(Select-String -LiteralPath "$outPath/$name.log" -Pattern '^\[test\]').Count
        $markers=@(Select-String -LiteralPath "$outPath/$name.log" -SimpleMatch 'Object discovery: 200 copied backings, 200 detached/abort graphs, 800 concurrent lifetimes, full clone OOM prefix;').Count
        $complete=[bool](Select-String -LiteralPath "$outPath/$name.log" -Pattern '^\[pass\]' -Quiet)
        $lanes+=[pscustomobject]@{Name=$name;Exit=$code;Programs=$tests;Tests=16;Examples=1;DiscoveryMarkers=$markers;Complete=$complete}
        Write-Output "$name exit=$code programs=$tests discovery-layouts=$markers"
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $success=$unchanged -and @($lanes | Where-Object {$_.Exit -ne 0 -or $_.Programs -ne 17 -or $_.DiscoveryMarkers -ne 2 -or !$_.Complete}).Count -eq 0
    $report=[pscustomobject]@{Complete=$success;InputsUnchanged=$unchanged;Inputs=$inputs;Lanes=$lanes;
        Scope='Native backing-discovery prerequisite only: borrowed exact-policy anchors, independent COW/deep backings, real external roots and phased class/native-field cycles, intact once-only finalizers, complete factory/clone allocation prefixes, four native mutators versus freeze, exact memory ledgers. Existing construction/copy/publication/finalizer/cursor/adapter populations retained. No xlang host discovery integration, automatic module-cycle reclamation or unmapping claim.'}
    [IO.File]::WriteAllText("$outPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$success) { throw 'Backing discovery batch failed; inspect both fixed-input compiler logs' }
} finally { Pop-Location }
