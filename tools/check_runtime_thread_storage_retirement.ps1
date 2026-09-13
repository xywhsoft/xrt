param([string]$Gcc = 'gcc', [Parameter(Mandatory=$true)][string]$Tcc)
$ErrorActionPreference = 'Stop'
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $directory = Join-Path $rootPath 'out/runtime_thread_storage_retirement'
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $files = @(@(Get-ChildItem src,include,single,config -Recurse -File) +
        @(Get-Item tests/single/test_single_runtime_retire.c,tests/fixtures/runtime_retire_dll.c,
            tests/fixtures/runtime_retire_host.c,$PSCommandPath)) | Sort-Object FullName -Unique
    $inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash} })
    # All runtime-owned native allocation goes through the registry. Dynamic
    # thread keys remain caller-owned and have their existing explicit destroy.
    $allocations = @(Get-ChildItem src -Recurse -File -Filter '*.c' | Select-String '\b(?:FlsAlloc|TlsAlloc)\s*\(')
    if ($allocations.Count -ne 2 -or @($allocations | Where-Object {
        $_.Path -notmatch 'src[\\/]core[\\/]core.c$|src[\\/]concurrency[\\/]thread_key.c$'
    }).Count) { throw 'Unregistered native thread storage allocation' }
    & $Gcc -std=c11 -Wall -Wextra -Werror tests/fixtures/runtime_retire_host.c -o "$directory/host.exe" *> "$directory/host-build.log"
    if ($LASTEXITCODE -ne 0) { throw 'Native unload host build failed' }
    $results = @()
    foreach ($compiler in @(@{Name='gcc';Path=$Gcc},@{Name='tcc';Path=$Tcc})) {
        $name = $compiler.Name; $compilerPath = (Get-Command $compiler.Path).Source
        [string[]]$flags = if ($name -eq 'gcc') { @('-std=c11','-Wall','-Wextra','-Werror') } else { @('-Wall') }
        & $compilerPath @flags -Iinclude tests/single/test_single_runtime_retire.c -o "$directory/$name-unit.exe" *> "$directory/$name-unit-build.log"
        if ($LASTEXITCODE -ne 0) { throw "$name retirement unit build failed" }
        & $compilerPath @flags -Iinclude -shared tests/fixtures/runtime_retire_dll.c -o "$directory/$name.dll" *> "$directory/$name-dll-build.log"
        if ($LASTEXITCODE -ne 0) { throw "$name unload DLL build failed" }
        $cases = @(1..5 | ForEach-Object { @{Name="$name-fault-$_";Exe="$directory/$name-unit.exe";Args=@($_)} })
        $cases += @{Name="$name-unload";Exe="$directory/host.exe";Args=@("$directory/$name.dll")}
        foreach ($case in $cases) {
            $log = Join-Path $directory $case.Name
            $process = Start-Process -FilePath $case.Exe -ArgumentList $case.Args -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput "$log.log" -RedirectStandardError "$log.err.log"
            if ($process.WaitForExit(45000)) { $code = $process.ExitCode }
            else { $process.Kill(); $process.WaitForExit(); $code = 'timeout-45s' }
            $process.Dispose()
            $marker = if ($case.Name.EndsWith('-unload')) { 'MEM_FREE confirmed' } else { 'runtime thread storage retirement passed' }
            $passed = $code -eq 0 -and [bool](Select-String -LiteralPath "$log.log" -SimpleMatch $marker -Quiet)
            $results += [pscustomobject]@{Name=$case.Name;Exit=$code;Passed=$passed;Log="$log.log";ErrorLog="$log.err.log"}
            Write-Output "$($case.Name) exit=$code passed=$passed"
        }
    }
    $unchanged = @($inputs | Where-Object { (Get-FileHash $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
    $artifacts = @(Get-ChildItem $directory -File | Where-Object Extension -in '.exe','.dll' | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash}
    })
    $report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Inputs=$inputs;
        InputsUnchanged=$unchanged;Artifacts=$artifacts;LoadCycles=192;ParkedThreadExits=768;ParkedFiberDeletes=576;
        Scope='Windows x64 GCC and TinyCC, explicit terminal thread storage retirement, not object graph/resource collection or POSIX unloading.'}
    [IO.File]::WriteAllText("$directory/results.json", ($report|ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($results | Where-Object { !$_.Passed }).Count) { throw 'Retirement checks failed; all cases collected' }
} finally { Pop-Location }
