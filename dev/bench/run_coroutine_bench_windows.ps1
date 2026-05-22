$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$cflags = @("-O2", "-I", ".")
$winlibs = @("-lws2_32", "-liphlpapi")

gcc dev/bench/coroutine/bench_context_switch.c xrt.c @cflags -o dev/bench/coroutine/bench_context_switch_win.exe @winlibs
gcc dev/bench/coroutine/bench_create_destroy.c xrt.c @cflags -o dev/bench/coroutine/bench_create_destroy_win.exe @winlibs
gcc dev/bench/coroutine/bench_timer_churn.c xrt.c @cflags -o dev/bench/coroutine/bench_timer_churn_win.exe @winlibs
gcc dev/bench/coroutine/bench_sched_post.c xrt.c @cflags -o dev/bench/coroutine/bench_sched_post_win.exe @winlibs

& "$root/dev/bench/coroutine/bench_context_switch_win.exe" 100000
& "$root/dev/bench/coroutine/bench_create_destroy_win.exe" 50000
& "$root/dev/bench/coroutine/bench_timer_churn_win.exe" 10000
& "$root/dev/bench/coroutine/bench_sched_post_win.exe" 10000
