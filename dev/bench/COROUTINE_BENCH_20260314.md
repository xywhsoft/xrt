# XRT Coroutine Baseline (2026-03-14)

This report records the first repeatable coroutine benchmark matrix after the
Linux x64 backend fix, the dedicated coroutine stage-script packaging, and the
switch to optimized (`-O2`) benchmark runners.

Companion scripts:

- `dev/bench/run_coroutine_bench_windows.ps1`
- `dev/bench/run_coroutine_bench_linux.sh`
- `dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md`

## Matrix

Windows and Debian 13 both use:

- context switch: `100000`
- create/destroy: `50000`
- timer churn: `10000`
- scheduler post wake: `10000`

## Results

| Test | Windows x64 (`asm-x64-win64`) | Debian 13 x64 (`asm-x64-sysv`) | Read |
|---|---:|---:|---|
| Context switches/s | 26199059.5 | 70330986.5 | Linux x64 is faster in this run |
| ns per switch | 38.169 | 14.218 | Linux x64 currently has the lower switch cost |
| Create/destroy ops/s | 370411.8 | 230309.8 | Windows currently leads on pure create/destroy |
| Create/destroy ns/op | 2699.698 | 4341.977 | Windows currently has the lower lifecycle cost |
| Create/resume/destroy ops/s | 245945.5 | 156686.7 | Windows currently leads on full one-shot lifecycle |
| Create/resume/destroy ns/op | 4065.942 | 6382.161 | Windows currently has the lower full lifecycle cost |
| Timer waits/s | 7910766.6 | 9560330.0 | Both are faster under optimized builds; Linux is slightly faster here |
| ns per timer wait | 126.410 | 104.599 | Both are in the same range |
| Scheduler post wakeups/s | 1640124.0 | 18519.8 | Windows is dramatically faster in the current post path |
| ns per scheduler wake | 609.710 | 53996.262 | Linux cross-thread post wake remains the clearest remaining coroutine hot path |

## Notes

- `bench_context_switch` measures main-thread resume/yield switching on the active
  coroutine backend.
- `bench_create_destroy` reports both `create/destroy` and
  `create/resume/destroy`.
- `bench_timer_churn` is intentionally tied to the coroutine scheduler's
  monotonic clock base.
- `bench_sched_post` measures cross-thread `xrtCoSchedPost()` wake cost for a
  scheduler-owned sleeping coroutine.
- The runner scripts now compile the benchmark binaries with `-O2`, so these
  numbers supersede the earlier unoptimized script outputs.
- A low-risk scheduler post-node reuse optimization has already landed and
  improved `bench_sched_post` modestly on both platforms, but the Linux
  cross-thread wake path is still the clearest remaining coroutine hot path.
