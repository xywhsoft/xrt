# XRT Foundation Boundary Inventory

## 1. Purpose

This document records the mainline/archive boundary for the foundation
finalization phase defined in
`dev/XRT_FOUNDATION_FINALIZATION_SPEC.md`.

It has two jobs:

1. identify the artifacts that must remain in mainline
2. identify the artifacts that must be removed, rewritten, or archived

This file is intentionally concrete. It is meant to be used while editing code,
not as a high-level design essay.


## 2. Mainline Production Set

The following modules remain in mainline and are part of the final production
path:

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

These modules now define the accepted production mainline. New work should
extend or optimize this set in place instead of introducing parallel
foundational implementations.


## 3. Mainline Problems to Eliminate

### 3.1 Coroutine Archive Remnants

The coroutine archive remnants have now been removed from the mainline build
path:

- `lib/coroutine.h` now requires a production backend and no longer compiles
  fiber/ucontext fallback code
- public coroutine backend introspection now reports only the production tier
  and production style in mainline builds
- historical fallback code is documented only under
  [dev/archive/coroutine](/D:/Git/xrt/dev/archive/coroutine/README.md)

Reason:

- the finalization policy is "one official mainline implementation"
- fallback coroutine backends should not silently define the production path

Action:

- keep fallback/backend history out of the public mainline build
- do not reintroduce archive backend selection macros or archive tier/style
  constants into public headers

### 3.2 Linux Backend Closure

The Linux mainline backend has now closed its transport finalization gates:

- native ring scaffolding and operation submission now live in [lib/xnet_port_uring.h](/D:/Git/xrt/lib/xnet_port_uring.h)
- Linux mainline now requires a native io_uring ring at init time instead of
  silently falling back to staged socket watch polling
- Debian validation has passed the native core gate, the xnet stage suite, the
  coroutine stage suite, and the full modern flow

Action:

- keep the Linux backend on the native ring path only; do not reintroduce a
  staged socket watch transport fallback into mainline
- treat any future Linux transport work as in-place optimization of the native
  io_uring path rather than as a new fallback layer

### 3.3 TLS Resume Mainline Coverage

The builtin TLS mainline now exposes a resumable-session API and dedicated
resume benchmark path:

- `xtlsresume`, `xrtTlsExportResume()`, `xrtTlsResumeDestroy()`,
  and `xrtTlsWasResumed()` are part of the public mainline
- `dev/bench/xnet2/bench_tls_resume.c` measures real TLS 1.2 resumed reconnects
- `dev/test_xnet2_stage.c` validates warm-handshake export plus resumed
  client/server state

Action:

- keep session-resume coverage in the mainline benchmark/report set
- if a future session-ticket path is added, document it as a separate baseline
  instead of silently replacing the current session-id resume lane

### 3.4 Documentation Boundary

Public mainline headers are now expected to describe only the finalized
production architecture. Historical "staged", "phase", or compatibility-era
language belongs only in archived material and historical specs under `dev/`.

Action:

- keep public mainline comments aligned with the finalized architecture only
- keep historical wording confined to archived code and historical progress/spec
  documents


## 4. Archived or Archive-Only Areas

The following locations are not part of the production mainline and must remain
archive-only:

- `dev/net-v1/`
- `dev/log/`
- historical merge snapshots under `dev/linux-debug-merge/`

These locations may be used for reference or recovery, but they must not be
reintroduced into public mainline API or build paths.


## 5. Official Archive Destinations

The official archive destinations for this phase are:

- `dev/net-v1/` for superseded network v1 code
- `dev/archive/runtime/` for removed runtime/thread compatibility artifacts
- `dev/archive/coroutine/` for removed coroutine transitional code
- `dev/archive/xnet/` for removed staged transport backend code
- `dev/archive/app/` for removed app-layer transitional infrastructure

If one of these directories is needed, its contents must be accompanied by a
short `README.md` explaining why the archived code exists and why it is no
longer mainline.


## 6. Non-Source Artifacts

The current worktree also contains many untracked benchmark/test executables and
temporary artifacts under `dev/`, `release/`, and benchmark subdirectories.

These are not part of the production source baseline and should be cleaned or
ignored separately. They are not treated as mainline implementation artifacts,
but they should not be allowed to accumulate indefinitely.


## 7. M0 Outcome

M0 is considered complete when:

- the official mainline production set is identified
- the specific non-final mainline structures are enumerated
- archive destinations are defined
- the finalization spec checklist is updated accordingly
