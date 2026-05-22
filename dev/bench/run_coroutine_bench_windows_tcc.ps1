$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$cflags = @("-m64", "-I", ".", "-O2")
$winlibs = @("-lws2_32", "-liphlpapi", "-lShell32")

tcc dev/bench/coroutine/bench_context_switch.c xrt.c @cflags -o dev/bench/coroutine/bench_context_switch_tcc_win.exe @winlibs
tcc dev/bench/coroutine/bench_create_destroy.c xrt.c @cflags -o dev/bench/coroutine/bench_create_destroy_tcc_win.exe @winlibs
tcc dev/bench/coroutine/bench_timer_churn.c xrt.c @cflags -o dev/bench/coroutine/bench_timer_churn_tcc_win.exe @winlibs
tcc dev/bench/coroutine/bench_sched_post.c xrt.c @cflags -o dev/bench/coroutine/bench_sched_post_tcc_win.exe @winlibs

& "$root/dev/bench/coroutine/bench_context_switch_tcc_win.exe" 100000
& "$root/dev/bench/coroutine/bench_create_destroy_tcc_win.exe" 50000
& "$root/dev/bench/coroutine/bench_timer_churn_tcc_win.exe" 10000
& "$root/dev/bench/coroutine/bench_sched_post_tcc_win.exe" 10000
