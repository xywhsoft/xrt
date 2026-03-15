# XRT Foundation Finalization Specification

## 1. Document Control

- Document: `dev/XRT_FOUNDATION_FINALIZATION_SPEC.md`
- Status: Accepted
- Version: `0.1.23`
- Last updated: `2026-03-15`
- Scope: Finalization of thread, coroutine, network, TLS, HTTP, and WebSocket foundations
- Source of truth: This file is the primary execution spec for the finalization phase

### 1.1 Purpose

This document defines the finalization phase for the modern XRT foundation.

The goal of this phase is not to add one more parallel implementation. The goal
is to converge the runtime, coroutine, network, TLS, HTTP, and WebSocket layers
into one production-grade mainline implementation per capability.

This spec exists to prevent three kinds of drift:

1. architectural drift caused by keeping staged and final designs alive together
2. quality drift caused by accepting "works in benchmarks" instead of
   production-grade semantics
3. repository drift caused by leaving obsolete implementations in mainline

### 1.2 Update Rules

The following rules are mandatory:

1. Any material architecture change must update this file before or with code.
2. Any completed task must update the milestone checklist in Section 15.
3. Any decision that changes scope, compatibility, performance strategy, or
   removal policy must add an entry to Section 18.
4. Every work session must append a short entry to Section 19.
5. Code is not considered "finalized" until its milestone exit criteria and
   validation gates are met.

### 1.3 Status Legend

- `todo`: not started
- `in_progress`: active work
- `blocked`: waiting on a dependency, decision, or bug fix
- `done`: implemented and accepted
- `deferred`: intentionally pushed out of the current phase


## 2. Executive Summary

XRT is intended to be a C infrastructure library for the Internet and AI era.
That means the foundation cannot merely be feature-complete. It must also be:

- fast enough for large-scale server workloads
- semantically correct under production timing, cancellation, and teardown
- explicit in ownership and concurrency rules
- internally consistent across runtime, coroutine, network, and protocol layers
- free of obsolete mainline implementations that dilute quality

The current codebase has already completed the finalization pass for the
foundation rewrite.

All milestone hard gates are now closed on both the Windows local workspace and
the Debian Linux validation host, including native transport, coroutine/sync
modern flow, and app-layer server/client integration.

This phase has now finished the rewrite properly.


## 3. Core Policy

### 3.1 One Official Mainline Implementation

Mainline must contain exactly one official implementation per foundational
capability:

- one official thread/runtime implementation
- one official coroutine runtime
- one official Windows network backend
- one official Linux network backend
- one official in-tree TLS implementation
- one official HTTP client
- one official HTTP server
- one official WebSocket implementation

Alternative, staged, transitional, compatibility, experimental, and superseded
implementations do not belong in mainline after the finalization milestone.

Those artifacts must be moved to `dev/` or removed entirely.

### 3.2 Mainline Quality Bar

Mainline code must satisfy all of the following:

- production-grade semantics
- production-grade teardown behavior
- no structurally misleading names
- no exported dead APIs
- no hidden hard caps that invalidate scalability goals
- no architecture comments that describe an old design

### 3.3 Archival Policy

Historical implementations may be retained only under `dev/` if all of the
following are true:

- they are no longer compiled by mainline
- they are no longer declared in public mainline headers
- they are clearly marked as archived
- they are not referenced as current production guidance


## 4. Product Goals

### 4.1 Primary Product Position

XRT must serve both of the following without splitting into separate stacks:

- high-performance server development
- ergonomic client development

### 4.2 Production Goals

The finalized foundation must provide:

- one coherent runtime ownership model across threads, coroutines, and network I/O
- native asynchronous transport cores on Windows and Linux
- sync facades implemented on top of the async core, not as parallel stacks
- predictable teardown, cancellation, timeout, and resource lifecycle behavior
- measurable performance with repeatable benchmark methodology
- enough clarity that new protocol modules can be built without rediscovering core invariants


## 5. Non-Goals

The following are not goals of this phase:

- preserving obsolete public APIs that conflict with the new architecture
- keeping staged backends in mainline "just in case"
- supporting every historical platform in mainline before production backends are correct
- hiding incompatibilities that should be surfaced as deliberate breaking changes
- optimizing benchmark optics at the expense of architecture correctness


## 6. Current Confirmed Problems

This section records the currently confirmed issues that motivate this phase.

### 6.1 Thread and Runtime Layer

- public thread APIs still expose dangerous or missing operations
- POSIX timed waits still use wall-clock time instead of monotonic time
- timeout and completion semantics are not fully uniform across platforms
- inspection paths still risk mutating target thread state

### 6.2 Coroutine Layer

- join, event, and future waits still have single-waiter restrictions
- runtime contracts are stronger than before, but not yet fully generalized
- some validation and benchmark closure work remains outside the mainline default flow

### 6.3 Network Core Layer

- the Windows backend is still staged rather than true IOCP transport
- the Linux backend is still staged rather than true io_uring transport
- staged watch models still leave artificial caps and harvest behavior in the data path
- sync convenience paths still depend on interim wait strategies

### 6.4 App-Layer Network Modules

- HTTP server and WebSocket server still keep accept polling threads
- app-layer servers are not yet fully driven by native listener completion
- comments and API descriptions still reflect transitional behavior in some files

### 6.5 Repository and Mainline Hygiene

- old, dangerous, or misleading APIs still leak into public headers
- some old compatibility assumptions remain in comments and naming
- there is not yet a single explicit finalization spec governing cross-module closure


## 7. Final Target Architecture

The final architecture is:

1. `thread/runtime` defines ownership, timed waits, cancellation, and completion semantics.
2. `coroutine` builds on that runtime model and provides stackful scheduling plus wait queues.
3. `xnet_port` provides true native backend execution on each supported production platform.
4. `xnet_engine` owns workers, command queues, timers, and dispatch.
5. `xnet_stream` and `xnet_dgram` implement transport semantics with no staged watch fallback in mainline.
6. `xnet_tls` binds the builtin TLS engine onto stream transport without depending on legacy network types.
7. `xhttp`, `xhttpd`, and `xws` build only on the new transport/runtime model.
8. sync APIs are facades over the same async core.


## 8. Mainline Boundary Rules

### 8.1 What Must Remain in Mainline

- finalized runtime and thread APIs
- finalized coroutine runtime and synchronization primitives
- finalized native Windows and Linux transport backends
- finalized TLS integration
- finalized HTTP, HTTP server, and WebSocket implementations
- tests, benchmarks, and docs that validate the current mainline behavior

### 8.2 What Must Leave Mainline

- archived network v1 implementations
- staged network transport backends once native backends are accepted
- compatibility declarations for removed APIs
- old comments that describe no-longer-supported behavior
- test entrypoints that validate obsolete stacks as if they were current

### 8.3 Archive Locations

Archived code must live under:

- `dev/net-v1/`
- `dev/log/`
- future archival folders under `dev/` with explicit status notes


## 9. Foundation-Wide Invariants

The following invariants are mandatory across all modules:

1. Ownership is explicit.
2. Timeout semantics use monotonic time.
3. Cancellation must leave resources in a defined state.
4. Completion and teardown must be idempotent.
5. No public API may advertise behavior that mainline does not implement.
6. App-layer modules may not carry shadow transport implementations.
7. Benchmarks may not hide known pathological behavior behind synthetic shortcuts.


## 10. Required Design Changes

### 10.1 Thread and Runtime Finalization

Required changes:

- remove exported dead or dangerous APIs from public mainline
- unify timed wait semantics around monotonic time
- replace polling-based timeout wait paths with proper completion-based waits
- make thread state inspection side-effect free
- make timeout, completion, and destroy semantics consistent across Windows and POSIX

Target outcome:

- thread wait semantics are production-safe
- no undefined public thread symbols remain
- no mainline API depends on dangerous suspend/kill mechanics

### 10.2 Coroutine Finalization

Required changes:

- replace single-waiter join/event/future handling with a proper wait queue model
- unify wait node lifecycle across join, event, future, stream, listener, and datagram wait
- ensure cancellation and timeout detach cleanly from all wait queues
- finish default-flow testing and benchmark closure for the finalized runtime

Target outcome:

- coroutine synchronization is general-purpose rather than single-consumer
- coroutine integration with runtime and xnet is consistent and auditable

### 10.3 Windows Backend Finalization

Required changes:

- replace staged readiness and synthetic completion with true IOCP transport
- use native overlapped accept/connect/send/recv paths
- remove dependence on `select()`, `FD_SETSIZE`, and synthetic data completions
- support batch harvest on real completion queues

Target outcome:

- Windows transport core is genuinely IOCP-based
- scalability is no longer bounded by staged watch-set behavior

### 10.4 Linux Backend Finalization

Required changes:

- replace staged wake-and-poll transport with true io_uring transport
- move accept, connect, recv, send, recvmsg, and sendmsg onto real ring operations
- eliminate fixed watch snapshot caps from the hot path
- support native batch submit and harvest semantics

Target outcome:

- Linux transport core is genuinely io_uring-based
- scalability no longer depends on staged watch polling

### 10.5 App-Layer Server Finalization

Required changes:

- remove dedicated accept polling threads from HTTP and WebSocket servers
- drive accept entirely from `xnetlistener` native completion
- remove accept poll configuration from public app-layer server configs
- ensure connection lifecycle, shutdown, and error handling stay inside the shared xnet model

Target outcome:

- app-layer servers become true protocol layers over xnet rather than hybrid shells

### 10.6 TLS Finalization

Required changes:

- keep builtin TLS as the only in-tree provider
- ensure TLS record I/O is chain/span-native and transport-owned
- avoid legacy network type coupling entirely
- add dedicated TLS validation and benchmark gates

Target outcome:

- TLS remains self-contained but is fully aligned with the new transport core

### 10.7 Documentation and API Surface Finalization

Required changes:

- refresh header top comments to match the actual architecture
- remove transitional naming that implies versioned production APIs when none exist
- ensure public headers describe exactly one current production path

Target outcome:

- headers, tests, docs, and specs all describe the same system


## 11. Benchmark and Validation Policy

### 11.1 Required Benchmark Families

The finalized stack must maintain benchmark coverage for:

- thread creation and join
- timed wait and wake latency
- coroutine context switch
- coroutine scheduler post/wake
- TCP echo
- TLS echo
- UDP burst
- send queue pressure
- HTTP request throughput
- HTTP keep-alive reuse
- WebSocket echo and fragmentation
- TLS handshake throughput and resume

### 11.2 Required Validation Families

The finalized stack must pass:

- unit and stage tests
- long-duration soak tests
- repeated benchmark sweeps
- parser fuzzing for HTTP, WS, and TLS record boundaries
- sanitizer runs on supported toolchains where available

### 11.3 Benchmark Integrity Rules

Benchmarks must not:

- use staged shortcuts that bypass the real production path
- mix setup time into steady-state throughput without explicitly reporting both
- hide partial completion or unread bytes behind optimistic completion criteria


## 12. Production Acceptance Gates

Mainline is considered production-grade only when all of the following are true:

1. There is no staged transport backend left in mainline.
2. There is no exported dead or undefined public API.
3. All timed waits use monotonic time semantics.
4. Coroutine synchronization supports multiple waiters where the API contract implies it.
5. HTTP server and WebSocket server have no dedicated accept polling threads.
6. TLS is fully integrated with the new transport path and has dedicated validation.
7. Benchmark methodology is repeatable and documented.
8. Current comments and docs match actual behavior.


## 13. Deliverables

Required deliverables for this phase:

- updated mainline code
- archived superseded code under `dev/`
- updated public headers
- updated validation entrypoints
- updated benchmark scripts and reports
- updated migration notes
- updated module specs and summary docs


## 14. Milestones

### M0. Spec Freeze and Boundary Freeze

Status: `done`

Purpose:

- freeze the finalization scope
- define what remains in mainline and what must be archived or removed

Exit criteria:

- this spec is accepted as the current source of truth
- all impacted modules are listed and assigned to milestones

### M1. Thread and Runtime Closure

Status: `done`

Scope:

- delete dead or dangerous public APIs
- unify monotonic timed waits
- replace polling timeout waits with proper completion waits
- align thread lifecycle semantics across platforms

Exit criteria:

- no missing thread symbols remain in public API
- no wall-clock timed wait remains in the finalized runtime path
- timeout and completion semantics are documented and tested

### M2. Coroutine Synchronization Closure

Status: `done`

Scope:

- implement generalized wait queues
- remove single-waiter limitations where semantics require fan-in or fan-out
- complete cancellation and timeout detach auditing

Exit criteria:

- join, event, and future waits no longer fail merely because a second waiter exists
- cancellation and timeout paths pass dedicated stress tests

### M3. Windows Native Backend

Status: `done`

Scope:

- convert the Windows backend to native IOCP transport
- remove staged readiness/select behavior from mainline

Exit criteria:

- no `select()`-based staged transport remains in the Windows mainline backend
- no `FD_SETSIZE`-bounded watch path remains in the mainline backend
- Windows repeated sweeps pass native transport validation gates

### M4. Linux Native Backend

Status: `done`

Scope:

- convert the Linux backend to native io_uring transport
- remove staged poll-based watch transport from mainline

Exit criteria:

- no staged poll transport remains in the Linux mainline backend
- no fixed watch snapshot cap remains in the Linux hot path
- Linux repeated sweeps pass native transport validation gates

### M5. App-Layer Server Closure

Status: `done`

Scope:

- remove HTTP and WS accept polling threads
- wire server accept entirely through native listener completion

Exit criteria:

- no app-layer accept polling thread remains in mainline
- app-layer configs no longer expose accept poll controls

### M6. TLS Hot Path and Validation Closure

Status: `done`

Scope:

- finish chain/span-native TLS hot path alignment
- add dedicated TLS validation and benchmark gates

Exit criteria:

- TLS does not depend on legacy network types
- TLS throughput, handshake, and resume benchmarks are documented

### M7. Mainline Purge, Docs, and Release Closure

Status: `done`

Scope:

- remove superseded mainline artifacts
- refresh header docs and migration notes
- lock final benchmark and validation baselines

Exit criteria:

- mainline contains only the final production implementation
- old/staged code is either archived under `dev/` or removed
- docs and headers reflect the actual finalized architecture


## 15. Task Checklist

### M0

- [x] M0-T01 Create and freeze the finalization spec
- [x] M0-T02 Enumerate all staged, obsolete, and archived mainline artifacts
- [x] M0-T03 Mark archive destinations under `dev/`

### M1

- [x] M1-T01 Remove dead thread APIs from public headers
- [x] M1-T02 Replace polling-based thread timeout waits
- [x] M1-T03 Convert timed waits to monotonic time
- [x] M1-T04 Make thread state inspection side-effect free
- [x] M1-T05 Add thread validation and benchmark coverage

### M2

- [x] M2-T01 Introduce generalized coroutine wait queue nodes
- [x] M2-T02 Migrate join wait to generalized wait queues
- [x] M2-T03 Migrate event wait to generalized wait queues
- [x] M2-T04 Migrate xnet future wait to generalized wait queues
- [x] M2-T05 Add cancellation and timeout stress tests

### M3

- [x] M3-T01 Design native IOCP op model
- [x] M3-T02 Implement native accept/connect completion flow
- [x] M3-T03 Implement native recv/send completion flow
- [x] M3-T04 Implement native datagram completion flow
- [x] M3-T05 Remove staged readiness code from mainline Windows backend
- [x] M3-T06 Add Windows native backend benchmark and soak gates

### M4

- [x] M4-T01 Design native io_uring op model
- [x] M4-T02 Implement ring-driven accept/connect flow
- [x] M4-T03 Implement ring-driven recv/send flow
- [x] M4-T04 Implement ring-driven datagram flow
- [x] M4-T05 Remove staged poll/watch code from mainline Linux backend
- [x] M4-T06 Add Linux native backend benchmark and soak gates

### M5

- [x] M5-T01 Remove HTTP server accept polling thread
- [x] M5-T02 Remove WebSocket server accept polling thread
- [x] M5-T03 Delete accept-poll configuration from public server configs
- [x] M5-T04 Add listener-driven server acceptance regression coverage

### M6

- [x] M6-T01 Audit TLS record hot path against chain/span transport ownership
- [x] M6-T02 Remove remaining transitional TLS coupling
- [x] M6-T03 Add handshake throughput benchmark
- [x] M6-T04 Add resume/ticket benchmark
- [x] M6-T05 Add TLS parser and boundary fuzz coverage

### M7

- [x] M7-T01 Archive or delete staged mainline artifacts
- [x] M7-T02 Refresh header top comments and module descriptions
- [x] M7-T03 Refresh benchmark reports and methodology docs
- [x] M7-T04 Refresh migration guidance and release notes
- [x] M7-T05 Final production-readiness review


## 16. Exit Metrics

### 16.1 Correctness Metrics

- zero undefined public symbols in supported build matrices
- zero staged backend code paths in mainline production transport
- zero wall-clock timed waits in finalized runtime and sync paths
- zero app-layer accept polling threads in mainline

### 16.2 Performance Metrics

The exact numeric targets may evolve by host class, but the following are mandatory:

- native backend performance must beat or match the current staged baseline in repeated sweeps
- variance must improve where staged transport currently injects jitter
- no benchmark lane may rely on a hard-coded artificial watch cap

### 16.3 Repository Metrics

- no archived implementation remains declared as current production API
- all archived code is under `dev/` or deleted
- public headers describe one current architecture only


## 17. Risks

- native backend rewrites can temporarily destabilize current benchmark baselines
- removing compatibility APIs may require coordinated migration notes
- multi-waiter coroutine primitives can expose hidden lifecycle bugs
- app-layer server simplification may uncover assumptions currently masked by polling threads
- TLS hot path tightening may surface latent buffer ownership bugs


## 18. Decision Log

### D-001

Date: `2026-03-15`

Decision:

Mainline must keep exactly one official production implementation per foundational capability. Obsolete, staged, superseded, and experimental implementations do not remain in mainline once the finalization phase closes.

Rationale:

XRT is targeting excellence rather than historical compatibility clutter. The repository must make the intended production path obvious.

### D-002

Date: `2026-03-15`

Decision:

Coroutine join pinning now tracks outstanding join references rather than a single waiter bit. Waking a waiter clears only the wait state; it does not implicitly destroy a DEAD coroutine once the last waiter resumes.

Rationale:

Multi-waiter join requires the target coroutine to remain valid until every waiter has returned from `xrtCoJoin()` and the caller has had a chance to inspect or close the handle.

### D-003

Date: `2026-03-15`

Decision:

Windows native backend closure is being staged by capability rather than by a single all-or-nothing switch. Native IOCP `accept`, `recv`, `send`, and datagram completion paths can land and become the mainline transport path before `ConnectEx` and benchmark/soak gates are fully closed.

Rationale:

This keeps the mainline moving toward the final architecture without pretending the milestone is complete before native client connect and formal validation gates exist.

### D-004

Date: `2026-03-15`

Decision:

Development benchmark helpers that exercise xnet stream servers must follow the same listener-driven accept model as mainline app servers. Dedicated accept polling threads are not acceptable in benchmark harnesses that are used to characterize production-path behavior.

Rationale:

Keeping a polling accept thread in benchmark-only wrappers would reintroduce timing noise and architectural drift into the very measurements used to validate the finalized stack.

### D-005

Date: `2026-03-15`

Decision:

The builtin TLS engine's internal byte buffers now use forward-only consume semantics with deferred compaction rather than eager `memmove` on every consume. Compaction is only allowed when append growth needs contiguous tail space.

Rationale:

This removes an obvious hot-path penalty from TLS send/recv/handshake processing without forcing a full external API change before the remaining chain/span-native TLS transport work is complete.

### D-006

Date: `2026-03-15`

Decision:

Builtin TLS handshake progression now has a socketless drive entrypoint for transport-owned integration. `xnet` no longer calls a socket-reading TLS handshake helper; it feeds cipher bytes into TLS and advances the state machine through an explicit drive function that refuses direct socket I/O.

Rationale:

This removes the last transport-direction ambiguity between TLS and `xnet`, keeps builtin TLS self-contained, and makes ownership clear: transport owns sockets, TLS owns protocol state.

### D-007

Date: `2026-03-15`

Decision:

The Windows IOCP backend must evict sockets from its bound-socket cache when a close op is submitted. Closed socket handles may be reused by Winsock, so a bind cache that only grows is not a safe memoization strategy.

Rationale:

Without close-time eviction, reused socket handles can be misclassified as already IOCP-bound, causing intermittent loss of recv/send completions and hard-to-reproduce TLS/session stalls under repeated open/close cycles.

### D-008

Date: `2026-03-15`

Decision:

`M6-T04` may only be closed with a real session-resume or ticket-resume path. Cold reconnect measurements are not acceptable substitutes for resume benchmarking, and the milestone remains open until builtin TLS exposes resumable session state.

Rationale:

The finalization phase is explicitly rejecting misleading benchmark methodology. Reporting reconnect throughput as "resume" would hide a real feature gap instead of measuring the intended protocol capability.

### D-009

Date: `2026-03-15`

Decision:

Mainline header comments and benchmark reports must describe the currently
implemented production behavior, not historical "v2/skeleton" staging language
or aspirational backend states.

Rationale:

The finalization phase is explicitly converging on one production path. Leaving
stale naming or speculative comments in public-facing headers undermines that
goal and makes handoff, review, and release decisions less trustworthy.

### D-010

Date: `2026-03-15`

Decision:

When the Windows mainline transport path is using IOCP-native stream ops,
client connect must use `ConnectEx` completion instead of blocking
`connect()+timeout` followed by a synthetic open event.

Rationale:

Leaving blocking connect in the Windows "native" path would keep one of the
most important stream lifecycle transitions outside the completion model and
would undercut the milestone's architectural closure.

### D-011

Date: `2026-03-15`

Decision:

Coroutine compatibility backends are no longer part of the default mainline
selection path. Mainline now requires a production coroutine backend by default,
and compatibility backends only activate through explicit opt-in build flags.

Rationale:

This keeps the supported production targets on one official coroutine
implementation while still allowing archive-style fallback builds to be
requested deliberately instead of silently defining mainline behavior.

### D-012

Date: `2026-03-15`

Decision:

The public coroutine backend introspection surface now names fallback builds as
`archive` rather than `compat`. Production builds still report the production
backend tier/style, while explicit fallback builds report archive tier/style and
archive backend names such as `fiber-archive` or `ucontext-archive`.

Rationale:

The finalization phase is converging on one official mainline implementation.
If fallback code remains available for explicit archive-only builds, the public
surface should describe it honestly instead of implying it is a normal
compatibility tier of the current mainline.

### D-013

Date: `2026-03-15`

Decision:

The Linux mainline backend now prefers native io_uring submission and
completion harvesting whenever the ring is available. The old staged watch path
remains only as an interim fallback while the Linux native closure milestone is
still in progress.

Rationale:

This keeps the production path moving toward true ring-driven transport without
pretending the Linux backend is already fully closed. It also gives the worker,
stream, and datagram layers one native-path contract to integrate against while
the remaining fallback code is being removed.

### D-014

Date: `2026-03-15`

Decision:

Coroutine fallback selection now uses archive naming internally as well as
publicly. The mainline opt-in macro is `XRT_CO_ENABLE_ARCHIVE_BACKEND`, and the
internal fallback tier/style identifiers now use `ARCHIVE` rather than
`COMPAT`.

Rationale:

Finalization is about making the production path obvious. Keeping `compat`
terminology inside the mainline selection logic after the public surface has
already moved to `archive` would preserve exactly the kind of architectural
ambiguity this phase is trying to remove.

### D-015

Date: `2026-03-15`

Decision:

Datagram send paths must converge on the same native backend port model as
stream send paths whenever the backend supports native send completions. Owner
thread helper tasks may still be used to marshal user data onto the owner
worker, but the actual transport send should not bypass the port layer on the
production path.

Rationale:

Allowing datagram receive to use the native backend while datagram send still
goes directly through ad-hoc socket calls would leave `xnet_dgram` split across
two transport models. The final architecture needs one async core, not a mixed
send/recv ownership story.

### D-016

Date: `2026-03-15`

Decision:

The TLS resume benchmark lane is considered valid only when it measures a real
mainline resumable-session API and proves resumed state on both peers. A warm
full handshake may seed the resume object, but its latency must be reported
separately and may not be folded into the resumed-handshake latency table.

Rationale:

This keeps the finalization phase honest about protocol capability and prevents
reconnect numbers from being mislabeled as resumed TLS performance. The goal is
to benchmark the actual mainline resume path, not a lookalike approximation.

### D-017

Date: `2026-03-15`

Decision:

Coroutine archive fallback backends have been removed from the mainline build
path. `lib/coroutine.h` now requires a production backend, and public coroutine
backend introspection in mainline builds exposes only the production tier/style
constants.

Rationale:

The finalization policy is one official mainline implementation per capability.
Leaving fiber/ucontext archive fallback code compiled inside the public
coroutine header would keep a second implementation alive in mainline and blur
the production boundary.

### D-018

Date: `2026-03-15`

Decision:

The Linux mainline backend no longer keeps a staged socket watch transport
fallback. `lib/xnet_port_uring.h` now requires native ring initialization at
port init time, and transport socket ops fail fast if the backend cannot submit
them natively.

Rationale:

Keeping a second socket-poll transport path alive inside the Linux backend
would violate the "one official mainline implementation" rule even if the
native ring path existed alongside it. Failing fast is preferable to silently
degrading to a superseded transport model.

### D-019

Date: `2026-03-15`

Decision:

Listener accept wait timeout/cancellation no longer cancels the underlying
native listener accept operation. Timeout now unregisters only the waiter slot;
the outstanding native accept remains armed and may satisfy the next waiter
registered on the same listener.

Rationale:

Canceling the waiter and the underlying native accept together allowed a stale
accept completion to consume the next incoming connection, which in turn left a
retry waiter blocked forever. The production listener model needs one native
accept lifecycle and many waiter lifecycles, not a forced 1:1 coupling.


## 19. Session Log

### 2026-03-15

- Created the foundation finalization spec to govern closure of thread, coroutine, network, TLS, HTTP, and WebSocket foundations.
- Froze the "one official mainline implementation" policy.
- Defined milestone structure for backend native completion work, runtime semantic closure, app-layer server convergence, TLS hot-path validation, and mainline archival cleanup.
- Added `dev/XRT_FOUNDATION_BOUNDARY_INVENTORY.md` to enumerate what stays in mainline, what must be removed or rewritten, and which archive destinations are official for this phase.
- Completed M1 thread/runtime closure:
  removed dead thread APIs from `xrt.h`, replaced POSIX polling timeout waits with completion-based waits, moved POSIX semaphore/condition/future timed waits onto monotonic semantics, made `xrtThreadGetState()` side-effect free, and added focused thread core validation plus baseline benchmark scaffolding under `dev/bench/thread/`.
- Completed M2 coroutine synchronization closure:
  generalized coroutine wait queues for join and event waits, lifted the single-waiter restriction from `xrtNetFutureWaitCo*`, added multi-join / multi-event / multi-future regression coverage, and revalidated coroutine core plus full `xnet` stage harness after fixing join-pin lifetime semantics.
- Advanced M3 Windows native backend closure:
  converted the mainline IOCP backend from staged readiness transport to native overlapped `AcceptEx` / `WSARecv` / `WSASend` / `WSARecvFrom` / `WSASendTo` submission, added bound-socket association tracking, rewired stream and datagram paths onto native completion-driven flow, and restored full stage-harness green status on Windows after fixing HTTPD and WebSocket close/error semantics.
- Validation for the current M3 step:
  `gcc -fsyntax-only xrt.c -I .`, `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`, `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_final.exe -lws2_32 -liphlpapi`, and full execution of `dev/test_xnet2_stage_final.exe` all passed on Windows.
- Completed M5 app-layer server closure:
  removed mainline HTTPD and WebSocket accept polling threads, switched both modules to listener-owned accept wait rearming on the xnet worker, deleted accept-poll config exposure, and added explicit reaccept regressions that prove one server instance can accept a second independent connection after the first closes.
- Tightened benchmark-path integrity for the same closure:
  migrated `dev/bench/xnet2/bench_stream_server.h` from a polling accept thread to the same listener-driven accept pattern so TCP/TLS stream benchmarks no longer hide mainline behavior behind a dev-only accept loop.
- Validation for the current M5 step:
  `gcc -fsyntax-only xrt.c -I .`, `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m5h.exe -lws2_32 -liphlpapi`, full execution of `dev/test_xnet2_stage_m5h.exe`, `gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c -I . -o dev/bench/xnet2/bench_echo_tcp_m5.exe -lws2_32 -liphlpapi`, `gcc dev/bench/xnet2/bench_echo_tls.c xrt.c -I . -o dev/bench/xnet2/bench_echo_tls_m5.exe -lws2_32 -liphlpapi`, and small-parameter runs of `bench_echo_tcp_m5.exe 64 32` and `bench_echo_tls_m5.exe 64 32` all passed on Windows.
- Started M6 TLS hot-path audit:
  confirmed that mainline TLS no longer depends on legacy v1 network types, but also confirmed that the current TLS record path still relies on internal linear send/recv buffers with `memmove`-based consumption, so chain/span-native transport ownership is not yet fully closed and M6 remains active.
- Advanced M6 TLS hot-path closure:
  replaced eager `memmove`-on-consume behavior in `lib/nettls.h` with forward-only internal buffer consumption plus deferred compaction on append pressure, keeping the public TLS/xnet adapter ABI stable while removing a clear transport-adjacent hot-path penalty.
- Validation for the current M6 step:
  `gcc -fsyntax-only xrt.c -I .`, `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m6a.exe -lws2_32 -liphlpapi`, full execution of `dev/test_xnet2_stage_m6a.exe` captured in `dev/_tmp_stage_m6a.log` with no `FAIL` lines, and `gcc dev/bench/xnet2/bench_echo_tls.c xrt.c -I . -o dev/bench/xnet2/bench_echo_tls_m6a.exe -lws2_32 -liphlpapi` plus `bench_echo_tls_m6a.exe 64 32` all passed on Windows.
- Advanced M6 TLS/xnet transport boundary closure:
  added a socketless TLS drive entrypoint (`xrtTlsDrive`) so `xnet` now advances builtin TLS only through transport-fed cipher bytes, rewired the async TLS handshake path in `xnet_stream` to use that drive API, and added outstanding-I/O hold plus deferred-destroy handling so late stream completions can no longer target freed streams during repeated TLS teardown.
- Fixed a Windows native backend regression discovered by the new TLS repeat validation:
  the IOCP backend's bound-socket cache now removes sockets on close submission, preventing reused Winsock handles from being misidentified as already IOCP-bound and eliminating intermittent repeated-TLS handshake stalls.
- Validation for the current M6 closure step:
  `gcc -fsyntax-only xrt.c -I .`, `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`, `gcc dev/_tmp_tls_repeat_diag.c xrt.c -I . -o dev/_tmp_tls_repeat_diag_fix2.exe -lws2_32 -liphlpapi`, execution of `dev/_tmp_tls_repeat_diag_fix2.exe` with 4/4 sequential TLS rounds passing, `gcc dev/_tmp_tls_once_then_repeat_diag.c xrt.c -I . -o dev/_tmp_tls_once_then_repeat_diag_fix.exe -lws2_32 -liphlpapi`, execution of `dev/_tmp_tls_once_then_repeat_diag_fix.exe`, `gcc dev/_tmp_test_tls_ws_mem.c xrt.c -I . -o dev/_tmp_test_tls_ws_mem_fix.exe -lws2_32 -liphlpapi`, execution of `dev/_tmp_test_tls_ws_mem_fix.exe`, `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m6fix.exe -lws2_32 -liphlpapi`, and full execution of `dev/test_xnet2_stage_m6fix.exe` all passed on Windows.
- Added the first dedicated TLS handshake throughput benchmark:
  `dev/bench/xnet2/bench_tls_handshake.c` now measures repeated real TLS handshakes over the finalized `xnet` transport path, reports active handshake throughput plus loop throughput, and records p50/p95/p99 handshake latency.
- Validation for the current M6 benchmark step:
  `gcc -fsyntax-only dev/bench/xnet2/bench_tls_handshake.c -I .`, `gcc dev/bench/xnet2/bench_tls_handshake.c xrt.c -I . -o dev/bench/xnet2/bench_tls_handshake_smoke.exe -lws2_32 -liphlpapi`, and execution of `dev/bench/xnet2/bench_tls_handshake_smoke.exe 16` passed on Windows with `handshakes_per_sec_active=3.422`, `handshake_avg_us=292260.331`, and zero client/server errors.
- Added dedicated TLS boundary stress coverage:
  `test/test_tls_boundary.h` and `dev/test_tls_boundary_core.c` now drive builtin TLS entirely through `xrtTlsDrive` / `xrtTlsFeed` without sockets, repeatedly fragmenting handshake records, application data, and close-notify records across deterministic pseudo-random chunk boundaries.
- Validation for the current M6 boundary step:
  `gcc -fsyntax-only dev/test_tls_boundary_core.c -I .`, `gcc dev/test_tls_boundary_core.c xrt.c -I . -o dev/test_tls_boundary_core.exe -lws2_32 -liphlpapi`, and execution of `dev/test_tls_boundary_core.exe` passed on Windows with the TLS boundary handshake/data/close stress rounds all reporting `PASS`.
- M6-T04 remains open by design:
  current builtin TLS has no mainline resumable session/ticket API yet, so resume benchmarking is blocked on protocol capability rather than missing harness work.
- Advanced M7 docs and release closure:
  refreshed the top-of-file module descriptions across `nettls`, `xcodec*`,
  `xnet_*`, and related headers so mainline comments now describe the current
  production path instead of old `v2`, `skeleton`, or phase-staging language.
- Added current benchmark methodology and TLS-specific validation reports:
  `dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md` now defines the benchmark
  reporting policy for coroutine, transport, and TLS work, while
  `dev/bench/TLS_BENCH_20260315.md` records the dedicated TLS handshake and
  boundary-stress baselines.
- Added current migration and release guidance:
  `dev/XRT_FOUNDATION_MIGRATION_GUIDE.md` and
  `dev/XRT_FOUNDATION_RELEASE_NOTES_20260315.md` now capture the official
  mainline module set, what left mainline, and how production users should read
  the finalized foundation surface.
- Validation for the current M7 documentation closure step:
  `gcc -fsyntax-only xrt.c -I .`, `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`,
  `gcc dev/bench/xnet2/bench_tls_handshake.c xrt.c -I . -o dev/bench/xnet2/bench_tls_handshake_doc.exe -lws2_32 -liphlpapi`,
  execution of `dev/bench/xnet2/bench_tls_handshake_doc.exe 32` with
  `handshakes_per_sec_active=3.410`, `handshake_avg_us=293230.834`,
  `handshake_p95_us=307862.500`, `gcc dev/test_tls_boundary_core.c xrt.c -I . -o dev/test_tls_boundary_core_doc.exe -lws2_32 -liphlpapi`,
  execution of `dev/test_tls_boundary_core_doc.exe` reporting TLS boundary
  stress `PASS`, `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_doc.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_doc.exe` all passed on Windows.
- Completed M3 Windows native backend closure:
  the Windows stream connect path now submits real `ConnectEx` completions
  through the IOCP backend, `xnet_stream` updates stream addresses on connect
  completion instead of relying on a prior blocking connect, and the new
  dedicated soak gate `dev/test_xnet_windows_native_core.c` repeatedly validates
  native TCP and TLS connect/accept/open/echo/close cycles.
- Validation for the current M3 native-connect closure step:
  `gcc -fsyntax-only xrt.c -I .`, `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m3connect.exe -lws2_32 -liphlpapi`,
  full execution of `dev/test_xnet2_stage_m3connect.exe`, `gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c -I . -o dev/bench/xnet2/bench_echo_tcp_m3connect.exe -lws2_32 -liphlpapi`,
  execution of `dev/bench/xnet2/bench_echo_tcp_m3connect.exe 64 32`,
  `gcc dev/bench/xnet2/bench_echo_tls.c xrt.c -I . -o dev/bench/xnet2/bench_echo_tls_m3connect.exe -lws2_32 -liphlpapi`,
  execution of `dev/bench/xnet2/bench_echo_tls_m3connect.exe 16 32`,
  `gcc dev/bench/xnet2/bench_tls_handshake.c xrt.c -I . -o dev/bench/xnet2/bench_tls_handshake_m3connect.exe -lws2_32 -liphlpapi`,
  execution of `dev/bench/xnet2/bench_tls_handshake_m3connect.exe 16`,
  `gcc -fsyntax-only dev/test_xnet_windows_native_core.c -I .`,
  `gcc dev/test_xnet_windows_native_core.c xrt.c -I . -o dev/test_xnet_windows_native_core.exe -lws2_32 -liphlpapi`,
  and execution of `dev/test_xnet_windows_native_core.exe 16 4` all passed on
  Windows.
- Advanced M7-T01 mainline boundary cleanup:
  coroutine compatibility backends no longer participate in the default
  mainline selection path; `lib/coroutine.h` now disables archive fallback
  fallback by default and requires a production backend unless the build
  explicitly opts into archive fallback mode.
- Validation for the current M7 boundary-cleanup step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc dev/test_coroutine_core.c xrt.c -I . -o dev/test_coroutine_core_prodonly.exe -lws2_32 -liphlpapi`,
  execution of `dev/test_coroutine_core_prodonly.exe`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_prodonly.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_prodonly.exe` all passed on
  Windows with the production coroutine backend selected.
- Tightened the remaining public coroutine archive semantics:
  public tier/style constants now use `ARCHIVE` naming instead of `COMPAT`,
  `xrtCoGetBackendTier()` / `xrtCoGetBackendStyle()` report archive identifiers
  for explicit fallback builds, and archive backend names now use
  `fiber-archive` / `ucontext-archive`.
- Validation for the current archive-naming cleanup step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc dev/test_coroutine_core.c xrt.c -I . -o dev/test_coroutine_core_archivename.exe -lws2_32 -liphlpapi`,
  and execution of `dev/test_coroutine_core_archivename.exe` all passed on
  Windows with the production backend still reporting tier/style `2`.
- Advanced M4 Linux native backend closure:
  `lib/xnet_port_uring.h` now carries a real native io_uring op model with raw
  ring setup, SQ/CQ helpers, native accept/connect/recv/send/recvfrom/sendto
  submissions, completion-to-event translation, and active-I/O tracking.
  `xnet_stream` and `xnet_dgram` now treat a ready uring port as native
  transport, and `dev/test_xnet_linux_native_core.c` now serves as the Linux
  native backend gate for repeated plain/TLS loopback connect/echo/close
  rounds, while still degrading to a skip on non-Linux hosts.
- Validation for the current M4 native-op step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`,
  `gcc -fsyntax-only dev/test_coroutine_core.c -I .`,
  `gcc -fsyntax-only dev/test_xnet_linux_native_core.c -I .`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m4slice.exe -lws2_32 -liphlpapi`,
  full execution of `dev/test_xnet2_stage_m4slice.exe`,
  `gcc dev/test_coroutine_core.c xrt.c -I . -o dev/test_coroutine_core_m4slice.exe -lws2_32 -liphlpapi`,
  execution of `dev/test_coroutine_core_m4slice.exe`,
  `gcc dev/test_xnet_linux_native_core.c xrt.c -I . -o dev/test_xnet_linux_native_core_gate.exe -lws2_32 -liphlpapi`,
  and `cmd /c D:\Git\xrt\dev\test_xnet_linux_native_core_gate.exe` reporting
  `skipped (non-Linux)` all passed in the current Windows workspace.
- Advanced M7-T01 archive cleanup:
  `lib/coroutine.h` now uses archive naming in its mainline selection macros as
  well (`XRT_CO_ENABLE_ARCHIVE_BACKEND`, `__XRT_CO_BACKEND_TIER_ARCHIVE`,
  `__XRT_CO_BACKEND_STYLE_ARCHIVE`), and an explicit archive note now sits next
  to the remaining top-of-file fallback commentary so the mainline selection
  path no longer advertises `compat` semantics.
- Validation for the current archive-macro cleanup step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc -fsyntax-only dev/test_coroutine_core.c -I .`,
  `gcc dev/test_coroutine_core.c xrt.c -I . -o dev/test_coroutine_core_archiveclean.exe -lws2_32 -liphlpapi`,
  execution of `dev/test_coroutine_core_archiveclean.exe`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_archiveclean.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_archiveclean.exe` all passed on
  Windows.
- Advanced native datagram send convergence:
  `lib/xnet_dgram.h` now routes copied datagram sends through native port
  `SENDTO` submission when the active backend exposes native transport I/O,
  keeping owner-worker marshaling but removing the old split where native
  receive used the port while send still bypassed it through direct socket
  calls.
- Validation for the current native datagram-send step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_dgramnative.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_dgramnative.exe` all passed on
  Windows with the datagram stage suite remaining green.
- Closed M6-T04 TLS resume benchmarking:
  builtin TLS now exposes a real resumable-session mainline API
  (`xtlsresume`, `xrtTlsExportResume()`, `xrtTlsResumeDestroy()`,
  `xrtTlsWasResumed()`), the resumed TLS 1.2 server path no longer emits a
  duplicate server `Finished`, and `dev/bench/xnet2/bench_tls_resume.c` now
  measures warm-seeded real resumed reconnects while requiring resumed state on
  both client and server.
- Validation for the current TLS resume closure step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc -fsyntax-only dev/bench/xnet2/bench_tls_resume.c -I .`,
  `gcc dev/bench/xnet2/bench_tls_resume.c xrt.c -I . -o dev/bench/xnet2/bench_tls_resume_doc.exe -lws2_32 -liphlpapi`,
  execution of `dev/bench/xnet2/bench_tls_resume_doc.exe 32` with
  `resumed_handshakes_per_sec_active=29.014`, `resume_avg_us=34466.544`,
  `resume_p95_us=38730.300`, `resumed_client_ok_count=32`,
  `resumed_server_ok_count=32`, and `resumed_payload_ok_count=32`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m6resume.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_m6resume.exe` all passed on
  Windows.
- Advanced M7-T01 coroutine mainline cleanup:
  removed archive fallback backend selection and implementation code from
  `lib/coroutine.h`, deleted public archive tier/style exposure from `xrt.h`,
  updated coroutine introspection tests to require the production tier/style,
  and refreshed `dev/archive/coroutine/README.md` plus the boundary inventory
  so archive fallback is now documented only under `dev/`.
- Validation for the current coroutine mainline-cleanup step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc dev/test_coroutine_core.c xrt.c -I . -o dev/test_coroutine_core_mainline.exe -lws2_32 -liphlpapi`,
  execution of `dev/test_coroutine_core_mainline.exe`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_mainline.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_mainline.exe` all passed on
  Windows with backend introspection still reporting `asm-x64-win64`, tier `2`,
  and style `2`.
- Advanced M4 Linux backend boundary cleanup:
  removed the staged socket watch transport path from `lib/xnet_port_uring.h`,
  made Linux port init fail if a native io_uring ring cannot be created,
  deleted watch registration/removal state from the backend context, and made
  transport socket submits fail fast instead of silently registering staged
  readiness watches.
- Validation for the current Linux backend cleanup step:
  `gcc -fsyntax-only xrt.c -I .`,
  `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_m4cleanup.exe -lws2_32 -liphlpapi`,
  and full execution of `dev/test_xnet2_stage_m4cleanup.exe` all passed in the
  current Windows workspace. Linux native soak/benchmark validation remains the
  open gate for `M4-T06`.
- Refreshed the local production boundary and review materials:
  cleaned the remaining public "staged mainline" wording from
  `lib/xnet_port_uring.h` and the stream validation surface, removed the dead
  `__xwsSleepMs()` helper from `lib/xws.h`, updated
  `dev/XRT_FOUNDATION_BOUNDARY_INVENTORY.md` so historical wording is now
  confined to archived or historical material, and added
  `dev/XRT_FOUNDATION_PRODUCTION_REVIEW_20260315.md` as the acceptance-oriented
  review companion for this spec.
- Validation for the current M7-T01 cleanup step:
  `sh build_test_modern.sh` completed with all suites passing in the current
  Windows workspace, including coroutine, sync, stream, datagram, TLS, HTTP,
  HTTPD, WebSocket, and Windows native backend core gates.
- Closed the remaining Linux native validation gates on the Debian host:
  synchronized the current tree to `/opt/xrt`, revalidated the native
  `io_uring` transport gate, fixed Linux listener retry semantics after
  coroutine accept-timeout cancellation, and reran the full modern flow until
  `build_test_xnet_native_core.sh`, `build_test_xnet2_stage.sh`,
  `build_test_coroutine_stage.sh`, and `build_test_modern.sh` all completed
  successfully on Debian.
- Final production acceptance is now closed:
  `M4-T06` and `M7-T05` are complete, the Linux host pass is no longer a
  blocker, and the finalized foundation mainline is accepted for production use
  under the current benchmark/report set.
