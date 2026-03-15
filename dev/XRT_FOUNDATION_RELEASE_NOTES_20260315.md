# XRT Foundation Release Notes (Draft, 2026-03-15)

## Summary

This draft records the current foundation-finalization state for:

- thread/runtime
- coroutine runtime
- xnet transport
- builtin TLS
- HTTP client/server
- WebSocket

The governing rule for this release line is:

- mainline keeps one official implementation per capability
- superseded code belongs in `dev/` or is removed from the build path


## Major Foundation Changes

### Runtime and Threading

- dead or dangerous public thread-control APIs were removed from mainline
- finalized timed-wait paths use monotonic time semantics
- timeout waits were moved away from polling-style behavior

### Coroutine Runtime

- join/event/future waits were moved onto generalized wait queues
- multi-waiter scenarios are no longer rejected by construction
- validation and benchmark entry points were formalized under `dev/`
- compatibility coroutine backends are no longer part of the default mainline
  selection path; production backends are required unless a build opts in
  explicitly

### Network Core

- the modern xnet stack is the only production network stack in mainline
- HTTP server and WebSocket server no longer keep dedicated accept polling
  threads
- sync network helpers continue to build on the async transport/runtime core

### Builtin TLS

- builtin TLS no longer depends on legacy v1 network ABI types
- transport-owned socketless drive/feed integration is now part of the mainline
  TLS boundary
- dedicated handshake and boundary validation coverage was added
- mainline TLS now exposes session-resume export/reuse APIs and a dedicated
  resumed-handshake benchmark lane

### HTTP and WebSocket

- public module names are now `xhttp`, `xhttpd`, and `xws`
- transitional `*2` public naming no longer defines the mainline API
- WebSocket fragmented message reassembly is part of current mainline behavior


## Validation Highlights

Current validation entry points include:

- `dev/test_coroutine_core.c`
- `dev/test_tls_boundary_core.c`
- `dev/test_xnet2_stage.c`
- benchmark reports under `dev/bench/`

Key benchmark/report documents:

- `dev/bench/COROUTINE_BENCH_20260314.md`
- `dev/bench/XNET_COMPARE_20260314.md`
- `dev/bench/XNET_COMPARE_SWEEP_20260314.md`
- `dev/bench/TLS_BENCH_20260315.md`


## Current TLS Resume Status

- builtin TLS mainline now supports exported TLS 1.2 session-resume state for
  application and benchmark use
- the dedicated TLS report now includes a real resumed-handshake lane based on
  `xtlsresume`, not cold reconnect substitution
- if a future session-ticket path is added, it will be reported separately from
  the current session-id resume baseline


## Release Guidance

- use the modern mainline foundation modules for all new production work
- treat `dev/net-v1/` and other archive folders as reference-only
- rely on the finalized validation entry points and benchmark documents instead
  of historical examples or archived test paths
