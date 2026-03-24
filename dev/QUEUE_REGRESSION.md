# Queue Regression Notes

## Purpose

This note records the lightweight regression entry points added for the
`xqueue` rollout. The goal is to keep queue behavior, trim behavior, and DLL
size guardrails easy to rerun without widening XRT's default maintenance cost.


## Primary Scripts

- `build_GCC_QUEUE_REGRESSION_x64.bat`
  - Main GCC regression entry for the queue work.
  - Builds and runs:
    - `queue_core`
    - `thread_core`
    - `xnet2_engine`
    - `test_trim.c`
    - `singlehead/test_singlehead.c`
    - `singlehead/test_singlehead_trim.c`
  - Also runs two negative compile probes to ensure `XRT_NO_QUEUE` actually
    hides `xqueue_config` in both normal-header and single-header builds.

- `build_GCC_QUEUE_REGRESSION_linux.sh`
  - Linux GCC regression entry for the same queue-focused surface.
  - Builds and runs:
    - `queue_core`
    - `thread_core`
    - `xnet2_engine`
    - `test_trim.c`
    - `singlehead/test_singlehead.c`
    - `singlehead/test_singlehead_trim.c`
  - Also runs the same normal-header and single-header negative no-queue
    compile probes.

- `build_GCC_SIZE_GUARD_x64.bat`
  - Builds a full DLL and a trim DLL with:
    - full: default `xrt.dll`
    - trim: `-DXRT_NO_QUEUE -DXRT_NO_NETWORK`
  - Fails if the trim DLL is not smaller than the full DLL.
  - Fails if the trim DLL exceeds the configured size thresholds.

- `singlehead/build_test.bat`
  - Local single-header convenience entry.
  - Runs both:
    - `test_singlehead.exe`
    - `test_singlehead_trim.exe`

- `build_GCC_DLL_x64_local.bat`
  - Builds a local GCC DLL to `release/x64/xrt_gcc_local.dll`.
  - Use this when you want a fresh DLL without touching the tracked
    `release/x64/xrt.dll` artifact.

- `build_TCC_DLL_x64_local.bat`
  - Builds a local TCC DLL to `release/x64/xrt_tcc_local.dll`.
  - Mirrors the GCC local-build intent for quick local validation.
  - Cleans the temporary local `.def` companion file after a successful build.

- `build_GCC_DLL_x64_release.bat`
  - Explicitly updates the tracked `release/x64/xrt.dll` with GCC.
  - Use this when the repository artifact itself should move forward.

- `build_TCC_DLL_x64_release.bat`
  - Explicitly updates the tracked `release/x64/xrt.dll` with TCC.
  - Keeps the tracked-artifact lane separate from local validation.

- `build_GCC_DLL_x64.bat` / `build_TCC_DLL_x64.bat`
  - Compatibility wrappers.
  - They still update the tracked DLL, but now print a reminder that
    `*_local.bat` is the safer default for day-to-day development.


## Guarded Behaviors

### Queue API

`queue_core` now locks down these contracts for `SPSC`, `MPSC`, and `MPMC`:

- zero-capacity and oversized-capacity creation must fail
- `NULL` queue inputs must return `XQUEUE_ERROR` or a zero/false result
- `NULL` drain callbacks must not consume queued items
- closing an empty queue must immediately make it drained
- drained closed queues must reset cleanly
- basic threaded producer/consumer paths must remain intact
- LP64 builds must not widen 32-bit queue/value atomics into adjacent-field
  corruption
- shared `__xrtAtomicLoadU32` / `__xrtAtomicStoreU32` /
  `__xrtAtomicCompareExchangeU32` helpers must stay on `uint32` fields;
  long-based atomics remain for `volatile long` state only

### XNet Integration

`xnet2_engine` now guards:

- bounded worker command queues returning `XRT_NET_AGAIN` on pressure
- delayed posts also returning `XRT_NET_AGAIN` when the command queue is full
- failed delayed posts not executing later by accident
- worker command-node freelist warm/reuse/return behavior

### Trim Paths

`test_trim.c` and `singlehead/test_singlehead_trim.c` guard:

- `XRT_NO_QUEUE`
- `XRT_NO_NETWORK`
- thread/runtime basics that should still work in trimmed builds

The negative compile probes exist to catch a more subtle regression:
`XRT_NO_QUEUE` must remove queue types from the public surface, not merely stop
using them at runtime.

The current Linux regression lane also matters for a second reason:
the first Debian 13 queue benchmark run exposed a `uint32* -> long*` atomic
width bug that only manifested on 64-bit Linux. The direct atomic-width guards
now keep that class of regression from hiding behind later benchmark crashes.
Those guards cover both the shared `uint32` helper layer in `xrt.h` and the
`xvalue` CAS wrapper that sits on top of it.


## Size Guard Thresholds

`build_GCC_SIZE_GUARD_x64.bat` currently uses:

- `XRT_SIZE_GUARD_MAX_TRIM_PERCENT=80`
- `XRT_SIZE_GUARD_MIN_DELTA_BYTES=131072`
- `XRT_SIZE_GUARD_KEEP_ARTIFACTS=0`

These can be overridden by environment variables before running the script.

Example:

```bat
set XRT_SIZE_GUARD_MAX_TRIM_PERCENT=75
set XRT_SIZE_GUARD_MIN_DELTA_BYTES=200000
set XRT_SIZE_GUARD_KEEP_ARTIFACTS=1
cmd /c build_GCC_SIZE_GUARD_x64.bat
```

By default the script keeps `xrt_size_report.txt` and removes the temporary
full/trim DLL artifacts after a successful run. Set
`XRT_SIZE_GUARD_KEEP_ARTIFACTS=1` when you want to inspect those DLLs directly.


## Current Observed Baseline

Observed on March 24, 2026 from `build_GCC_SIZE_GUARD_x64.bat`:

- full DLL: `864768` bytes
- trim DLL: `611328` bytes
- delta: `253440` bytes
- trim percent: `70%`

These numbers are a working baseline, not a frozen ABI target.


## Update Rule

If queue-related code intentionally changes binary size or trim behavior:

1. Run `build_GCC_QUEUE_REGRESSION_x64.bat`.
2. Record the new size result.
3. Only change guard thresholds when there is a measured reason.
4. Keep trim builds meaningfully smaller than full builds.

For Linux queue/runtime regressions, run `build_GCC_QUEUE_REGRESSION_linux.sh`
in the same source tree or in a synchronized remote worktree.


## Out Of Scope

These scripts do not replace:

- full repository regression runs
- performance benchmarking
- future `wait wrapper` validation
- warning-clean builds across the whole tree
