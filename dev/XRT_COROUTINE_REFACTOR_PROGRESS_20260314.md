# XRT Coroutine Progress Addendum (2026-03-14)

This addendum records the coroutine/xnet integration work completed after the main
`dev/XRT_COROUTINE_REFACTOR_SPEC.md` draft, without rewriting the whole spec body.

## Scope

Complete the real listener accept wait-source bridge so sync-thread waits, future
waits, and coroutine waits no longer depend on deferred polling-only listener helpers.

## Code Paths Changed

- `lib/xnet_stream.h`
  - listener now owns worker affinity, async-hold tracking, accept waiter state,
    accept op ids, and port-driven accept watch registration/cancellation
- `lib/xnet_sync.h`
  - added `XNET_WAITSRC_LISTENER`
  - added listener future / sync / coroutine / wait-source helpers
  - added listener pending-cancel cleanup on top of the existing future bridge
- `test/test_xnet2_sync.h`
  - added listener future regressions
  - added listener wait-source regressions
  - added listener coroutine regressions
- `dev/test_xnet2_listener_accept_core.c`
  - added a focused harness that validates future / wait-source / coroutine
    listener-accept behavior independently of unrelated older coroutine failures

## Validation

- `gcc -fsyntax-only dev/test_xnet2_listener_accept_core.c -I .`
- `gcc dev/test_xnet2_listener_accept_core.c xrt.c -I . -o dev/test_xnet2_listener_accept_core_dbg2.exe -lws2_32 -liphlpapi`
- focused harness run:
  - future path PASS
  - wait-source path PASS
  - coroutine path PASS
- `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`
- `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_coroutine_listener.exe -lws2_32 -liphlpapi`
- `xnet2` stage harness PASS after listener integration

## Decision

`listener accept` is allowed to enter the unified wait-source / future / coroutine
surface only after it owns the same lifecycle primitives as stream and dgram waits:

- owner-worker affinity
- async-hold protection
- explicit waiter registration/cancellation
- `xnet_port`-driven submit/harvest completion

That condition is now satisfied, so listener accept is no longer treated as a
deferred polling-only surface.

## Effect On Roadmap

- `M6 xnet integration` is no longer blocked by listener accept
- remaining coroutine-side priority is now:
  - hardware/backend validation (`M4`)
  - tests/docs/benchmarks (`M7`)
  - only if still needed later: broader raw socket/port readiness wait-source work

## Suggested Handoff

Resume the remaining network-library work on top of the now-complete coroutine
bridge for:

- future waits
- stream waits
- datagram waits
- listener accept waits

## Linux x64 Backend Follow-up

After the listener bridge landed, native Debian validation exposed a separate
Linux-only coroutine backend fault: the listener coroutine path crashed inside
`__xrt_co_swap()` before the accept logic itself became relevant.

### Root Cause

`lib/coroutine.h` used a free-register inline-asm form for the SysV x64 backend:

- `__xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)` took both pointers as
  generic `"r"` inputs
- the swap body then saved/restored callee-saved registers such as `rbx/r12-r15`
- GCC was therefore free to place `pFrom` / `pTo` in registers that the swap body
  itself saved or overwrote during the context transition

That made the first context switch unstable on Linux x64 and manifested as an
ASan crash in `__xrt_co_swap()`.

### Fix

The SysV x64 backend now pins the two context pointers to the standard caller-saved
argument registers:

- `pFrom -> rdi`
- `pTo   -> rsi`

The swap body addresses both contexts through fixed `rdi/rsi`, eliminating the
register-allocation aliasing that corrupted the transition.

### Validation

Windows:

- `dev/test_xnet2_listener_accept_core.c + xrt.c`
  - focused listener harness still PASS
- `dev/test_xnet2_stage.c + xrt.c`
  - stage harness still PASS
- `dev/test_coroutine_core.c + xrt.c`
  - pure coroutine regression harness PASS

Debian 13:

- `dev/test_coroutine_min.c + xrt.c`
  - fixed the minimal scheduler crash, exit code `0`
- `dev/test_xnet2_listener_accept_core.c + xrt.c`
  - future / wait-source / coroutine listener paths PASS
- `dev/test_xnet2_sync_core.c + xrt.c`
  - sync/wait-source coroutine bridge PASS
- `dev/test_coroutine_core.c + xrt.c`
  - existing coroutine regression suite PASS on `asm-x64-sysv`
- `dev/test_xnet2_stage.c + xrt.c`
  - full Linux stage harness PASS

### Roadmap Effect

- Linux x64 production backend is no longer a blocker for coroutine/xnet work
- `listener accept` wait-source remains complete
- remaining coroutine backlog is now mostly:
  - non-x86 production-backend device validation
  - tests/docs/benchmark packaging and default-flow integration

## Benchmark Scaffolding Follow-up

To push `M7` forward, the session also added the first standalone coroutine
benchmark entry points under `dev/bench/coroutine/`:

- `bench_context_switch.c`
- `bench_create_destroy.c`
- `bench_timer_churn.c`
- `bench_sched_post.c`
- `README.md`

### Benchmark Validation

Windows x64 (`asm-x64-win64`) smoke:

- `bench_context_switch.exe 10000`
- `bench_create_destroy.exe 20000`
- `bench_timer_churn_fix.exe 5000`
- `bench_sched_post_fix.exe 5000`

Debian 13 x64 (`asm-x64-sysv`) smoke:

- `bench_context_switch_linux 10000`
- `bench_create_destroy_linux 20000`
- `bench_timer_churn_linux 5000`
- `bench_sched_post_linux 5000`

### Notes

- `bench_timer_churn.c` was explicitly corrected to use the same monotonic
  millisecond clock base as the coroutine scheduler, avoiding mixed-clock
  deadline bugs.
- `bench_sched_post.c` was tightened to avoid 10ms polling noise in the poster
  handshake, so its output now reflects scheduler-post wake cost instead of test
  harness sleep granularity.

## Stage Script Follow-up

Coroutine validation has now also been packaged into repeatable test scripts:

- `build_test_coroutine.sh`
  - builds/runs the pure coroutine regression harness
- `build_test_coroutine_stage.sh`
  - builds/runs the minimal coroutine smoke, pure coroutine suite, `xnet_sync`
    bridge suite, and focused listener-accept suite in sequence

### Script Validation

Debian 13:

- `./build_test_coroutine.sh`
  - PASS
- `./build_test_coroutine_stage.sh`
  - PASS

This means the coroutine work is no longer dependent on remembering ad hoc build
commands from the session log; there is now a stable stage path for future Linux
bring-up and regression reruns.

## Cross-Platform Modern Flow Follow-up

The coroutine test entry points have now been tightened into a default modern flow
instead of remaining Linux-only shell helpers:

- `build_test.sh`
  - now defaults to `build_test_modern.sh`
- `build_test_modern.sh`
  - runs the coroutine stage first, then `xnet2` stage
- `build_test_coroutine.sh`
- `build_test_coroutine_stage.sh`
- `build_test_xnet2_stage.sh`
- `build_test_legacy.sh`
  - retained as the explicit legacy `test.c` lane

The shell scripts now select platform-specific link flags automatically:

- Windows Git Bash
  - `-lws2_32 -liphlpapi`
- Linux
  - `-pthread`

### Modern Flow Validation

Windows local:

- `bash -lc 'cd /d/Git/xrt && ./build_test_modern.sh'`
  - PASS

Debian 13:

- `./build_test_modern.sh`
  - PASS

## Scripted Baseline Follow-up

The coroutine benchmark scaffolding has now been promoted into scripted,
repeatable baselines:

- `dev/bench/run_coroutine_bench_windows.ps1`
- `dev/bench/run_coroutine_bench_linux.sh`
- `dev/bench/COROUTINE_BENCH_20260314.md`

### Baseline Validation

Windows x64 (`asm-x64-win64`):

- `run_coroutine_bench_windows.ps1`
  - context switch: `98.435 ns/switch`
  - create/destroy: `3062.234 ns/op`
  - create/resume/destroy: `4686.002 ns/op`
  - timer churn: `278.420 ns/wait`
  - sched post wake: `760.260 ns/wakeup`

Debian 13 x64 (`asm-x64-sysv`):

- `run_coroutine_bench_linux.sh`
  - context switch: `64.845 ns/switch`
  - create/destroy: `4255.721 ns/op`
  - create/resume/destroy: `6139.112 ns/op`
  - timer churn: `270.023 ns/wait`
  - sched post wake: `55343.819 ns/wakeup`

### Roadmap Effect

- `M7` is no longer blocked on “how to rerun” or “how to capture a baseline”
- the clearest remaining coroutine hot path is now the Linux cross-thread
  `xrtCoSchedPost()` wake path
- remaining coroutine-side work is concentrated in:
  - public API / user-facing documentation
  - broader benchmark sweeps
  - non-x86 device validation

## Optimized Runner Follow-up

The scripted coroutine benchmark runners now build with `-O2`, and the baseline
report has been refreshed to use those optimized runs instead of the earlier
unoptimized script output.

- `dev/bench/run_coroutine_bench_windows.ps1`
- `dev/bench/run_coroutine_bench_linux.sh`
- `dev/bench/COROUTINE_BENCH_20260314.md`

Updated optimized results:

- Windows x64 (`asm-x64-win64`)
  - context switch: `38.169 ns/switch`
  - create/destroy: `2699.698 ns/op`
  - create/resume/destroy: `4065.942 ns/op`
  - timer churn: `126.410 ns/wait`
  - sched post wake: `609.710 ns/wakeup`
- Debian 13 x64 (`asm-x64-sysv`)
  - context switch: `14.218 ns/switch`
  - create/destroy: `4341.977 ns/op`
  - create/resume/destroy: `6382.161 ns/op`
  - timer churn: `104.599 ns/wait`
  - sched post wake: `53996.262 ns/wakeup`

Roadmap impact:

- the coroutine benchmark path is now release-like by default
- Linux cross-thread `xrtCoSchedPost()` remains the clearest remaining runtime
  hot path
- future coroutine optimization work should compare against this optimized
  baseline rather than the original unoptimized script-run numbers
