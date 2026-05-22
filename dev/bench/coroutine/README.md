# XRT Coroutine Benchmarks

This directory contains the first coroutine-focused benchmark scaffolding for the
refactored runtime. These are lightweight developer tools intended for smoke runs
and trend tracking, not yet curated as formal release baselines.

## Benchmarks

- `bench_context_switch.c`
  - measures main<->coroutine context-switch throughput on the current backend
- `bench_create_destroy.c`
  - measures `create/destroy` and `create/resume/destroy` lifecycle cost
- `bench_timer_churn.c`
  - measures scheduler timer insert/remove + immediate wake churn on the same
    monotonic clock base used by the coroutine runtime
- `bench_sched_post.c`
  - measures cross-thread `xrtCoSchedPost()` wake throughput for a scheduler-owned
    sleeping coroutine

## Example Builds

Windows:

```powershell
gcc dev/bench/coroutine/bench_context_switch.c xrt.c -I . -o dev/bench/coroutine/bench_context_switch.exe -lws2_32 -liphlpapi
gcc dev/bench/coroutine/bench_create_destroy.c xrt.c -I . -o dev/bench/coroutine/bench_create_destroy.exe -lws2_32 -liphlpapi
gcc dev/bench/coroutine/bench_timer_churn.c xrt.c -I . -o dev/bench/coroutine/bench_timer_churn.exe -lws2_32 -liphlpapi
gcc dev/bench/coroutine/bench_sched_post.c xrt.c -I . -o dev/bench/coroutine/bench_sched_post.exe -lws2_32 -liphlpapi
```

Linux:

```bash
gcc dev/bench/coroutine/bench_context_switch.c xrt.c -I . -pthread -o dev/bench/coroutine/bench_context_switch_linux
gcc dev/bench/coroutine/bench_create_destroy.c xrt.c -I . -pthread -o dev/bench/coroutine/bench_create_destroy_linux
gcc dev/bench/coroutine/bench_timer_churn.c xrt.c -I . -pthread -o dev/bench/coroutine/bench_timer_churn_linux
gcc dev/bench/coroutine/bench_sched_post.c xrt.c -I . -pthread -o dev/bench/coroutine/bench_sched_post_linux
```

## Example Smoke Runs

Windows:

```powershell
.\dev\bench\coroutine\bench_context_switch.exe 10000
.\dev\bench\coroutine\bench_create_destroy.exe 20000
.\dev\bench\coroutine\bench_timer_churn.exe 5000
.\dev\bench\coroutine\bench_sched_post.exe 5000
```

Linux:

```bash
./dev/bench/coroutine/bench_context_switch_linux 10000
./dev/bench/coroutine/bench_create_destroy_linux 20000
./dev/bench/coroutine/bench_timer_churn_linux 5000
./dev/bench/coroutine/bench_sched_post_linux 5000
```

## Notes

- `bench_timer_churn.c` intentionally uses the same monotonic millisecond base as
  the coroutine scheduler instead of `xrtTimer()` to avoid cross-clock drift.
- `bench_sched_post.c` is designed as a scheduler-post wake benchmark, not a
  generic coroutine messaging benchmark.
- Before treating any result as a baseline, rerun with larger iteration counts and
  pin down CPU/power-management noise on the target machine.

## Repeatable Baseline Scripts

- `dev/bench/run_coroutine_bench_windows.ps1`
  - builds and runs the curated Windows baseline matrix
- `dev/bench/run_coroutine_bench_linux.sh`
  - builds and runs the curated Linux baseline matrix
- `dev/bench/COROUTINE_BENCH_20260314.md`
  - records the first cross-platform baseline captured from those scripts
