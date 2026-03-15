# XRT Foundation Production Review 2026-03-15

## Purpose

This document records the production-readiness review for the finalized XRT
foundation mainline.

It is the acceptance-oriented companion to
`dev/XRT_FOUNDATION_FINALIZATION_SPEC.md`.


## Current Scope

Reviewed areas:

- runtime/thread foundation
- coroutine runtime
- native xnet transport core
- builtin TLS integration
- HTTP client/server
- WebSocket client/server


## Local Validation Status

The following gates have been re-run successfully in the current Windows
workspace:

- `sh build_test_modern.sh`
- `gcc -fsyntax-only xrt.c -I .`
- `gcc dev/test_coroutine_core.c xrt.c -I . -o dev/test_coroutine_core_mainline.exe -lws2_32 -liphlpapi`
- `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_mainline.exe -lws2_32 -liphlpapi`
- `gcc dev/test_xnet_windows_native_core.c xrt.c -I . -o dev/test_xnet_windows_native_core.exe -lws2_32 -liphlpapi`
- `gcc dev/bench/xnet2/bench_tls_resume.c xrt.c -I . -o dev/bench/xnet2/bench_tls_resume_doc.exe -lws2_32 -liphlpapi`

Observed status:

- coroutine modern flow: pass
- xnet stage suite: pass
- Windows native TCP/TLS repeated connect/echo/close gate: pass
- TLS resume benchmark lane: pass


## Static Boundary Review

Confirmed in current mainline:

- no dead thread-kill/suspend/resume API remains exported
- public coroutine backend selection no longer exposes archive fallback
- HTTP server and WebSocket server no longer use accept polling threads
- public mainline comments now describe the finalized architecture rather than
  historical staged rewrite steps

Clarifications:

- `lib/thread.h` and `lib/xnet_sync.h` still contain `CLOCK_REALTIME` as a
  fallback for platforms that do not expose `CLOCK_MONOTONIC`; on mainstream
  POSIX targets they initialize condition variables with `CLOCK_MONOTONIC`
  semantics
- `lib/xnet_port_uring.h` may still block on the ring file descriptor, but it
  no longer carries the historical socket-watch transport path


## Debian Validation Status

The final Debian validation pass has now been rerun successfully on the host at
`/opt/xrt`.

Successful gates:

- `sh build_test_xnet_native_core.sh`
- `sh build_test_xnet2_stage.sh`
- `sh build_test_coroutine_stage.sh`
- `sh build_test_modern.sh`

Remote log files:

- `/opt/xrt/dev/linux_final_xnet_native_core.log`
- `/opt/xrt/dev/linux_final_xnet2_stage.log`
- `/opt/xrt/dev/linux_final_coroutine_stage.log`
- `/opt/xrt/dev/linux_final_modern.log`

Observed status:

- Debian native TCP/TLS repeated connect/echo/close gate: pass
- Debian xnet stage suite: pass
- Debian coroutine/sync/listener modern stage: pass
- Debian full modern flow: pass


## Production Verdict

Current verdict: `accepted`

Meaning:

- the mainline architecture is now aligned with the finalized production design
- the Windows validation and local correctness gates are green
- the Debian native transport and modern-flow validation gates are green
- the finalized foundation mainline is accepted for production use under the
  current benchmark and validation set
