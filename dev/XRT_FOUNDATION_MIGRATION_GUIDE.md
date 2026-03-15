# XRT Foundation Migration Guide

## Purpose

This guide explains how to migrate onto the finalized XRT foundation mainline.

The governing policy is simple:

- mainline keeps one official implementation per foundational capability
- superseded or staged implementations live under `dev/` or are removed


## Mainline Set

The official foundation set is:

- `lib/thread.h`
- `lib/coroutine.h`
- `lib/nettls.h`
- `lib/xnet_base.h`
- `lib/xnet_mem.h`
- `lib/xnet_port.h`
- `lib/xnet_port_iocp.h`
- `lib/xnet_port_uring.h`
- `lib/xnet_engine.h`
- `lib/xnet_stream.h`
- `lib/xnet_dgram.h`
- `lib/xnet_sync.h`
- `lib/xnet_tls.h`
- `lib/xcodec.h`
- `lib/xcodec_http1.h`
- `lib/xcodec_ws.h`
- `lib/xhttp.h`
- `lib/xhttpd.h`
- `lib/xws.h`


## What Left Mainline

The following no longer define production guidance:

- archived network v1 code under `dev/net-v1/`
- transitional compatibility declarations that pointed at removed behavior
- accept-poll thread paths for HTTP server and WebSocket server
- dangerous thread-control APIs that did not fit the finalized runtime model


## Thread and Runtime Migration

Use:

- thread creation, join, timeout wait, mutex, condition, semaphore, and event
  semantics from the current mainline runtime

Do not use:

- forced thread kill, suspend, or resume semantics from historical code or old
  examples

Behavior changes:

- timed waits are monotonic in finalized paths
- timeout waits are completion-oriented instead of polling-oriented


## Coroutine Migration

Use:

- coroutine scheduler, join, event, and wait primitives from `lib/coroutine.h`

Behavior changes:

- join/event/future waits no longer fail just because more than one waiter
  exists
- wait/cancel/detach behavior follows the generalized wait-queue model
- production coroutine backends are required by default; compatibility backends
  now require explicit build-time opt-in


## Network and TLS Migration

Use:

- `xnetengine`, `xnetlistener`, `xnetstream`, `xdgramsock`, `xnetfuture`
- builtin TLS through `xnet_tls` and the transport-owned `nettls` bridge

Do not use:

- archived v1 socket, loop, TCP, UDP, HTTP, or WebSocket APIs from the old
  network stack

Behavior changes:

- sync facades are built on the async transport core
- HTTP/WS server acceptance is listener-driven instead of accept-poll-thread
  driven
- builtin TLS no longer depends on legacy network ABI types
- TLS 1.2 session resumption is exposed through `xtlsresume`,
  `xrtTlsExportResume()`, `xrtTlsResumeDestroy()`, and `xrtTlsWasResumed()`


## HTTP and WebSocket Migration

Use:

- `xhttp` for HTTP/1.1 client work
- `xhttpd` for HTTP/1.1 server work
- `xws` for WebSocket client/server work

Behavior changes:

- `xhttp` and `xhttpd` are mainline HTTP/1.1 modules, not historical `*2`
  transitional names
- WebSocket fragmented message reassembly is supported by `xws`
- app-layer servers no longer own separate accept polling threads


## Validation Entry Points

Useful current entry points:

- `dev/test_coroutine_core.c`
- `dev/test_tls_boundary_core.c`
- `dev/test_xnet2_stage.c`
- benchmark and compare scripts under `dev/bench/`


## Archive Policy

If you need to inspect superseded code for reference, use:

- `dev/net-v1/`
- `dev/archive/runtime/`
- `dev/archive/coroutine/`
- `dev/archive/xnet/`
- `dev/archive/app/`

Archived code is reference-only. It is not mainline production guidance.
