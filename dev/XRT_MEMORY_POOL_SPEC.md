# XRT Global Memory Pool and Memory Debug Specification

## 1. Document Control

- Document: `dev/XRT_MEMORY_POOL_SPEC.md`
- Status: Accepted
- Version: `0.1.11`
- Last updated: `2026-03-16`
- Scope: Global allocator takeover, mempool refactor, memory debug system, temp-memory redesign, and object lifetime diagnostics
- Source of truth: This file is the primary execution spec for the XRT memory-pool overhaul

### 1.1 Purpose

This document defines the next major memory-system refactor for XRT.

The goal is not to add one more allocator beside the current system. The goal
is to establish one production-grade mainline allocation system that:

- accelerates small object and structure allocation
- preserves or improves release-build performance
- provides opt-in debug visibility for leak, double-free, and lifetime errors
- integrates cleanly with the existing runtime ownership model
- does not regress the network, coroutine, container, or TLS hot paths

This spec also exists to prevent a common failure mode:

1. optimize allocator development experience first
2. push debug cost into production paths
3. accidentally make server throughput, latency, or allocator contention worse

That outcome is explicitly forbidden by this spec.

### 1.2 Update Rules

The following rules are mandatory:

1. Any material allocator design change must update this file before or with code.
2. Any completed task must update the milestone checklist in Section 17.
3. Any design decision affecting release cost, debug scope, allocator layering, or fallback policy must add an item to Section 20.
4. Every meaningful work session should append a short entry to Section 21.
5. No allocator work is considered complete until the validation gates in Section 18 pass.

### 1.3 Status Legend

- `todo`: not started
- `in_progress`: active work
- `blocked`: waiting on a dependency, decision, or bug fix
- `done`: implemented and accepted against milestone criteria
- `deferred`: intentionally pushed out of the current phase


## 2. Executive Summary

XRT currently has:

- process-global allocation entry points via `xrtMalloc/xrtCalloc/xrtRealloc/xrtFree`
- explicit low-level allocator types such as `xmemunit`, `xfsmempool`, and `xmempool`
- a prepared thread-local memory state slot in `xrtThreadData.tMem`
- a thread-local temporary-memory ring
- separate specialized allocator logic in the modern network stack

What XRT does not yet have is one mainline allocation system that:

- accelerates the common small-object allocation path globally
- keeps release builds lean
- provides opt-in debug metadata and leak visibility
- supports object lifetime diagnostics across XRT-managed roots

This phase adds that system.

The mainline design approved by this spec is:

1. small allocations in the `16..1024` byte range use a new global mempool system
2. bucket selection uses a lookup table, not the current `xmempool` binary-search tree
3. allocations above `1024` bytes fall back to the configured backing allocator
4. release builds store only minimal allocation headers
5. debug builds are enabled only by macros and add file/line/canary/registry metadata
6. explicit allocator types (`xmempool`, `xfsmempool`) gain compatible debug features
7. XRT root objects that require explicit destruction gain lifetime diagnostics
8. temporary memory is redesigned as a thread-local scratch arena, not a fixed pointer ring
9. `xmempool` uses its own lookup-table bucket mapping design instead of sharing
   the global allocator core


## 3. Existing State

### 3.1 Current Global Allocation Entry Points

Current public allocation entry points live in `lib/base.h`:

- `xrtMalloc`
- `xrtCalloc`
- `xrtRealloc`
- `xrtFree`

At present they are thin wrappers over the configured process-global allocator
hooks or the libc allocator fallback. They do not currently apply any global
pooling or global debug instrumentation.

### 3.2 Existing Explicit Pool Types

Current explicit allocator building blocks are:

- `xmemunit`
  - fixed-size unit allocator with 256 slots
- `xfsmempool`
  - fixed-size pool composed of multiple `xmemunit` units
- `xmempool`
  - multi-size pool using fixed-size bucket roots plus a large-allocation fallback

These allocators were originally designed as explicit allocator objects.
They are not currently shaped as a process-global general-purpose `malloc`
replacement.

### 3.3 Existing Thread Memory State

`xrtThreadData` already contains `tMem`, which is the correct long-term landing
zone for thread-local allocator state.

The runtime phase-2 work explicitly prepared this slot for future allocator hot
paths, but mainline allocation still does not route through it.

### 3.4 Existing Temporary Memory

`xrtTempMemory` currently uses a thread-local fixed ring of pointer slots.

That mechanism is easy to use, but it is not a real scratch allocator:

- it is bounded by a fixed slot count
- it still performs heap allocation per request
- it cannot efficiently serve bursty formatting/parsing workloads
- it provides weak debug visibility

### 3.5 Existing Network Memory Path

The modern network stack already uses `xnet_mem` with worker-local caches and a
purpose-built block model.

That allocator path is already optimized for network transport hot paths and is
not to be replaced by the new global allocator. The global allocator may become
its backing allocator for misses or large blocks, but must not flatten its local
cache model.


## 4. Core Policy

### 4.1 One Official Mainline Allocation System

Mainline must converge on one official global allocation path:

- `xrtMalloc`
- `xrtCalloc`
- `xrtRealloc`
- `xrtFree`

These must route into the new memory system.

The old "thin wrapper over libc hooks only" behavior will cease to be the
official mainline allocation model once this phase is complete.

### 4.2 Explicit vs Global Allocators

The following distinction is mandatory:

- the new global allocator is the official process-wide general allocation path
- `xmemunit`, `xfsmempool`, and `xmempool` remain explicit allocator objects
- explicit allocators may share debug conventions and selected helper logic
- explicit allocators are not themselves the entire global allocator
- `xmempool` is not required to share the same allocator core implementation as
  the global allocator because its concurrency and use-shape requirements differ

In particular, mainline must not simply instantiate one global `xmempool` and
use it as `malloc/free` under contention.

### 4.3 Release vs Debug Separation

Release and debug memory behavior must be clearly separated:

- release builds keep only minimal metadata required for correctness
- debug builds add richer diagnostics strictly through explicit macros/build flags
- release hot paths must not pay for debug registries, file/line recording, or
  heavy validation

### 4.4 Production-First Rule

This phase is successful only if:

- small-object allocation gets faster or at least not worse under target loads
- debug features do not materially contaminate release hot paths
- network/coroutine/container benchmarks do not regress beyond accepted limits


## 5. Product Goals

### 5.1 Performance Goals

The new memory system must:

- accelerate structure and node allocation across XRT
- reduce allocator contention for same-thread hot paths
- avoid unnecessary system allocator calls in the `16..1024` byte range
- maintain predictable free-path cost
- preserve acceptable `realloc` behavior for dynamic-buffer users

### 5.2 Debug Goals

The debug system must support:

- leak detection
- double-free detection
- invalid-free detection
- basic overwrite/stomp detection
- object-lifetime diagnostics for explicit-destroy roots
- clear reporting of unfreed roots and allocations
- machine-readable and human-readable memory reports

### 5.3 Reporting Goals

The debug system must be able to generate memory reports that include at least:

- event type
- pointer or object address
- allocation size where applicable
- allocator/object category
- source file
- source line
- thread id
- event time
- aggregate counts and byte totals

Reports must cover:

- `malloc`-path leaks
- pooled allocation leaks
- object leaks
- double-free
- invalid-free
- overwrite/canary corruption
- suspicious use-after-free cases where diagnosable

### 5.4 Usability Goals

The new memory system must preserve easy usage:

- existing `xrtMalloc/xrtFree` call shapes remain valid
- object creation APIs stay lightweight in release builds
- debug tracking can be enabled without invasive user code changes


## 6. Non-Goals

The following are explicitly out of scope for this phase:

- tracing GC
- generational GC
- lock-free cross-process shared-memory allocators
- replacing `xnet_mem` with the new global allocator
- making all allocations permanently retain file/line metadata in release builds
- adding backtrace capture to release builds


## 7. Approved High-Level Design

### 7.1 Allocation Range Policy

The approved mainline allocation policy is:

- `0..15` bytes
  - rounded up to the `16` byte class
- `16..1024` bytes
  - serviced by the new global mempool system
- `>1024` bytes
  - serviced by the configured backing allocator

The `1024` byte cutoff is chosen for phase-1 of this overhaul because:

- it captures a large share of structure and node allocations
- it avoids overreaching into variable-length buffer territory too early
- it reduces the chance of making `realloc`-heavy workloads worse

This cutoff may later become tunable, but the initial implementation target is
fixed by this spec.

### 7.2 Bucket Layout

The approved small-object bucket plan is:

- step size: `16` bytes
- number of classes: `64`
- class sizes:
  - `16, 32, 48, ..., 1024`

Bucket selection must use a lookup-table strategy, not the existing
`xmempool` binary-tree search.

The design target is:

- O(1) class selection
- no pointer chasing in the common classification path
- no size-tree walk in release hot paths

### 7.3 `xmempool` Bucket Mapping Policy

`xmempool` is explicitly approved to use a different internal implementation
from the global allocator.

The mainline `xmempool` design must:

- replace the current binary-tree bucket search
- own a per-pool size-to-bucket lookup table
- size that table according to a configurable fallback cutoff
- use the table to select bucket classes directly

The construction contract is:

- constructor parameter `0`
  - uses the official default bucket layout
  - `16` byte step up to `1024`
- constructor parameter `N > 0`
  - means `xmempool` uses `N` as its fallback cutoff
  - allocates a lookup table covering that range
  - any size above `N` falls back to the backing allocator

This means `xmempool` becomes user-configurable for explicit allocator
workloads, while the global allocator remains fixed to the official mainline
policy for this phase.

### 7.4 Allocator Layering

The new global allocator has three layers:

1. thread-local cache layer
2. process-global central pool layer
3. backing allocator layer

The intended path is:

- `size -> class LUT -> thread cache -> central pool -> backing allocator`

### 7.5 Backing Allocator

The new global allocator does not eliminate the backing allocator.

The backing allocator remains necessary for:

- allocations above `1024` bytes
- bootstrap and early-init paths
- internal spill/fallback cases
- optional debug-heavy paths

The backing allocator remains the configured `xCore.malloc/calloc/realloc/free`
hooks or libc fallback.

### 7.6 Medium Allocation Deferral Policy

The `1025..4096` byte range is explicitly deferred from pool optimization in
this phase.

Phase-1 rationale:

- this range is more likely to contain variable-size buffers than stable object nodes
- aggressive fixed-class pooling in this band risks high internal fragmentation
- dense class tables in this band increase metadata, cache pressure, and central-pool complexity
- poorly designed medium pooling can underperform mature system allocators

Therefore, this phase will:

- keep `>1024` bytes on the backing allocator path
- measure this range during baseline telemetry
- defer any medium-allocation pool work to a later dedicated phase

No phase-1 implementation may silently pool `1025..4096` byte allocations as
if they were just larger small objects.


## 8. Detailed Allocator Architecture

### 8.1 Small Allocation Path

For `16..1024` bytes:

- classify size using a static LUT
- fetch one block from the current thread-local free list for that class
- if local cache is empty, refill from the central pool
- if the central pool is empty for that class, allocate one span from the
  backing allocator and carve it into objects

### 8.2 Thread-Local Cache

The new allocator must route through `xrtThreadData.tMem`.

Each attached thread should have:

- a per-class free-list head
- a per-class free-count or cached span cursor
- per-thread scratch/temp allocator state
- optional debug counters in debug builds

This is a strict hot-path rule:

- the common owner-thread path must not repeatedly resolve TLS/FLS data
- nested allocator/container code should pass allocator context directly when
  practical

### 8.3 Central Pool

The central pool manages:

- class metadata
- span lists per class
- refill/drain coordination
- optional shared counters/statistics

The central pool must not become a global serialization bottleneck.

Acceptable approaches include:

- per-class locks with thread-local batching
- per-class central free lists with coarse refill chunks
- one central list per class plus refill quanta to reduce lock frequency

### 8.4 Span Layout

Each small-class span should be carved into same-size objects.

The exact span size can be implementation-defined, but the design must satisfy:

- low internal fragmentation for small classes
- bounded refill cost
- straightforward block-owner lookup during free

### 8.5 Minimal Allocation Header

Release allocations served by the global mempool must carry only minimal header
metadata required for correctness:

- magic or signature
- allocator flags
- class id or large-allocation marker
- requested size or recoverable size information

Release mode must not store:

- source file path
- source line number
- heavy registry links
- redzone descriptors beyond minimal correctness markers

### 8.6 `calloc`

`xrtCalloc` must:

- route through the small-object pool for small allocations
- zero the returned memory
- preserve the same failure/error semantics as current `xrtCalloc`

### 8.7 `realloc`

`xrtRealloc` must be treated as its own path.

Rules:

- `NULL + size` behaves like `malloc`
- `ptr + 0` follows existing XRT-compatible free behavior
- small-to-same-class growth may reuse the same block if safe
- cross-class growth/shrink may allocate/copy/free
- large allocations may defer to the backing allocator

The implementation must not simply degrade all `realloc` operations into
expensive unconditional allocate-copy-free sequences if a cheaper path is safe.


## 9. Debug System Design

### 9.1 Debug Enablement Policy

All heavy memory diagnostics must be opt-in.

Approved activation model:

- public macros
- compile-time flags
- debug-only wrapper entry points

Release mainline must remain unaffected unless debug is enabled.

### 9.2 Debug Macro Contract

The intended public pattern is:

- `xrtMalloc(size)` expands to `xrtMallocDbg(size, __FILE__, __LINE__)` in debug mode
- `xrtFree(ptr)` expands to `xrtFreeDbg(ptr, __FILE__, __LINE__)` in debug mode
- equivalent debug-routing applies to `calloc`, `realloc`, and selected object
  constructors

The exact macro names may be implementation-defined, but the behavioral
contract is fixed:

- debug captures source site at allocation/free call time
- release does not

### 9.3 Debug Metadata

Debug allocations may extend headers with:

- source file
- source line
- allocation sequence number
- allocating thread id
- freed/not-freed state
- front and tail canary
- registry links

### 9.4 Debug Registries

Debug mode must maintain:

- live-allocation registry
- freed-allocation state tracking or quarantine
- statistics by size class

At minimum the registry must allow:

- leak listing at shutdown
- double-free detection
- invalid-free detection
- stomp/corruption suspicion reporting

### 9.5 Debug Fill Policy

Debug mode should use poison patterns:

- fresh allocation fill
- freed block fill
- optional temp-arena reset fill

The exact byte values may be implementation-defined.

### 9.6 Quarantine

Debug mode may optionally delay real reuse of recently freed blocks to improve:

- use-after-free detection
- double-free detection
- stomp detection

This behavior is forbidden in release mode.

### 9.7 Debug Event Taxonomy

The debug allocator and object-lifetime system must define a clear event model.

At minimum, the following event classes are required:

- `ALLOC`
- `FREE`
- `REALLOC`
- `LEAK`
- `POOL_LEAK`
- `OBJECT_LEAK`
- `DOUBLE_FREE`
- `INVALID_FREE`
- `WRONG_ALLOCATOR_FREE`
- `BUFFER_OVERFLOW_SUSPECT`
- `BUFFER_UNDERFLOW_SUSPECT`
- `USE_AFTER_FREE_SUSPECT`
- `OBJECT_CREATE`
- `OBJECT_DESTROY`
- `OBJECT_DOUBLE_DESTROY`
- `TEMP_ALLOC`
- `TEMP_RESET`

Exact naming may differ in implementation, but all behaviors above must be
representable in reports and diagnostics.

### 9.8 Debug Report Outputs

Debug mode must support both:

- machine-readable report output, preferably JSON
- human-readable report output, preferably text or Markdown

The report system must be able to emit:

1. event-level reports
2. end-of-run live/leak reports
3. aggregated hotspot reports by file/line
4. allocator class/capacity reports
5. object lifetime reports

### 9.9 Aggregation Requirements

The debug reporting layer should aggregate at least:

- allocation count by file/line
- live allocation count by file/line
- leaked bytes by file/line
- object leak count by object type
- class-level current and peak bytes
- class-level current and peak block counts
- current and peak temp arena usage


## 10. `xmempool` and `xfsmempool` Refactor

### 10.1 Shared Bucket/Class Tables

`xmempool` and the new global allocator should align in policy where useful, but
they are not required to share one allocator core implementation.

They may still share:

- debug header conventions where practical
- helper utilities for class-table building
- compatible validation/reporting rules

### 10.2 `xmempool`

`xmempool` must be refactored away from binary-tree bucket lookup for the
default and configured class layouts.

The required `xmempool` model is:

- each pool owns a size-to-bucket mapping table
- the table length is derived from the pool fallback cutoff
- the pool maps `size -> bucket` directly with no tree search
- sizes above the fallback cutoff go to the backing allocator
- explicit-pool semantics remain separate from the global allocator

The default constructor policy is:

- `0` means use the official default layout:
  - `16` byte step
  - up to `1024`

The configurable constructor policy is:

- `N > 0` means:
  - build a mapping table for sizes `0..N`
  - define buckets according to the selected per-pool policy
  - fall back for `size > N`

This phase does not require arbitrary user-defined nonuniform class layouts, but
the new `xmempool` design must not depend on the old hard-coded tree.

`xmempool` remains an explicit allocator type and is not the entire global
allocator.

### 10.3 `xfsmempool`

`xfsmempool` remains a dedicated fixed-size allocator.

It must gain compatible debug instrumentation in debug mode:

- alloc/free state validation
- optional source tracking
- optional lifetime registry integration

Release `xfsmempool` must remain small and fast.


## 11. Object Lifetime Diagnostics

### 11.1 Scope

In debug mode, XRT should track explicit-destroy root objects.

Target categories include:

- arrays
- pointer arrays
- dynamic stacks
- tree roots
- dict roots
- list roots
- explicit mempool roots
- fixed-size mempool roots
- other roots that have explicit `Create/Destroy` or `Init/Unit` lifetimes

### 11.2 Design Rule

Release object layouts must not be bloated by always-on diagnostic fields.

The approved strategy is:

- keep object-lifetime diagnostics in a debug-only side registry
- register on `Create*` / `Init*`
- unregister on `Destroy*` / `Unit*`
- report leaks or mismatched lifetime operations at shutdown or explicit dump time

### 11.3 Recorded Metadata

Debug object-lifetime records should include:

- object address
- object type identifier
- create/init source file
- create/init source line
- creating thread id
- destroy/unit source file and line if available
- current lifecycle state

### 11.4 Supported Errors

The object debug system should detect:

- leaked root object not explicitly destroyed
- double-destroy / double-unit
- destroy of unknown object
- mismatched destroy routine where diagnosable
- wrong-thread destroy where diagnosable


## 12. Temporary Memory Redesign

### 12.1 Current Problem

The current temporary-memory ring is not a real allocator.

It allocates individual heap blocks and stores them in a small fixed pointer
array, which is convenient but weak for performance and diagnostics.

### 12.2 Approved Replacement

Temporary memory must be redesigned as a thread-local scratch system:

- one or more bump arenas for small temporary allocations
- optional spill blocks for larger temporary allocations
- reset-on-cleanup semantics

### 12.3 Temp Allocation Policy

Recommended temp policy:

- small temp requests come from the thread scratch arena
- large temp requests may spill to backing allocation
- `xrtFreeTempMemory()` resets scratch arenas and frees spill blocks

### 12.4 Temp Debug Policy

Debug mode should tag temporary allocations separately so that:

- temp leaks or misuse are distinguishable from normal heap leaks
- arena reset can poison/reset memory
- thread detach can validate temp cleanup state
- temp-related events can appear in the debug report stream

### 12.5 Compatibility Rule

`xrtTempMemory()` and `xrtFreeTempMemory()` must remain available to preserve
script-style convenience usage.

The implementation may change internally, but the convenience API contract
must remain.


## 13. Integration Rules

### 13.1 Runtime Integration

The new allocator must integrate with:

- `xrtThreadData.tMem`
- runtime attach/detach
- thread cleanup registration
- ownership/shared-mode semantics where allocator roots are explicit objects

### 13.2 Container Integration

Containers that currently call `xrtMalloc/xrtFree` should automatically benefit
from the new allocator without public API changes.

Object lifetime diagnostics for container roots must be debug-only and must not
break existing layouts in release mode.

### 13.3 Network Integration

`xnet_mem` remains the primary network hot-path allocator.

Rules:

- do not replace `xnet_mem` local worker caches with the global allocator
- do not force transport hot paths to route each block operation through the
  global mempool
- optional backing-allocation integration is allowed for cache misses or large
  blocks if benchmark-safe

### 13.4 Coroutine Integration

Coroutine scheduling, stack management, and event/future objects must be
re-tested against the new allocator.

This is especially important because coroutine-heavy code tends to amplify:

- small object churn
- cross-thread wake paths
- temp allocation bursts


## 14. Known Risks and Prohibited Failure Modes

### 14.1 Known Risks

- release regressions caused by over-instrumentation
- central-pool contention under high thread count
- `realloc` regressions for variable-size buffer workloads
- hidden interaction regressions in network and coroutine subsystems
- object-debug registry becoming a scalability bottleneck in debug mode

### 14.2 Prohibited Failure Modes

The following outcomes are explicitly unacceptable:

- replacing the current global allocator with one shared `xmempool` instance
- introducing mandatory debug bookkeeping into release mode
- making `xnet_mem` slower in the name of allocator unification
- making `realloc` significantly worse for common buffer workloads
- changing public object layouts in release mode only to support debug tracking
- leaking thread-local scratch arenas across thread detach


## 15. Validation and Benchmark Plan

### 15.1 Microbenchmarks

The following benchmark lanes are required:

- small fixed-size alloc/free throughput
- mixed small-class alloc/free throughput
- `calloc` throughput for pooled sizes
- `realloc` growth/shrink behavior
- multi-thread allocator contention
- temp allocation burst workloads

### 15.2 Module-Level Benchmarks

The following higher-level validations are required:

- coroutine benchmark suite
- xnet stage and native-core gate
- HTTP/WS/TLS stage harness
- container-heavy churn tests for dict/list/tree/array/dynstack

### 15.3 Debug Validation

Debug validation must include intentional tests for:

- leak
- double-free
- invalid-free
- overwrite/canary corruption
- leaked explicit root object
- double-destroy of explicit root object
- wrong-allocator free
- temp-arena misuse where diagnosable
- report generation and parsing smoke tests

### 15.4 Acceptance Rule

This phase can only be accepted if:

- release small-object performance improves or stays within defined tolerance
- debug mode successfully detects the targeted error classes
- major XRT module benchmarks do not regress beyond agreed thresholds
- runtime attach/detach and temp cleanup remain correct
- debug mode can emit both machine-readable and human-readable memory reports


## 16. Deliverables

The phase must produce:

- mainline global mempool implementation
- refactored `xmempool`
- refactored `xfsmempool` debug support
- debug allocator macros and registry
- object lifetime registry
- temp arena implementation
- debug memory report exporters
- tests
- benchmark results
- migration and debug usage documentation


## 17. Milestones and Tasks

### 17.1 Milestone Table

| Milestone | Status | Exit Target |
| --- | --- | --- |
| M0 Baseline audit and instrumentation | done | allocation distribution and hotspot evidence captured |
| M1 Global mempool core | done | `16..1024` global pool active behind internal API |
| M2 `xrtMalloc/xrtFree` takeover | done | public global allocator routed through mempool |
| M3 `calloc/realloc` completion | done | full public allocation surface supported |
| M4 `xmempool/xfsmempool` alignment | done | explicit pools share class logic and debug contracts |
| M5 Debug allocator system | done | leak/double-free/invalid-free/canary diagnostics functional |
| M6 Object lifetime diagnostics | done | explicit-destroy roots tracked in debug mode |
| M7 Temporary memory redesign | done | scratch arena replaces pointer ring |
| M8 Validation and release documentation | done | benches/tests/docs complete and accepted |

### 17.2 M0 Baseline Audit and Instrumentation

- [x] Add temporary allocator telemetry without changing semantics
- [x] Measure size distribution of `xrtMalloc` users
- [x] Identify hot modules that churn small allocations
- [x] Record baseline for coroutine, network, and container tests

### 17.3 M1 Global Mempool Core

- [x] Define class table for `16..1024`
- [x] Implement LUT-based bucket selection
- [x] Implement per-thread cache in `xrtThreadData.tMem`
- [x] Implement central pool/span refill path
- [x] Define minimal release allocation header

### 17.4 M2 `xrtMalloc/xrtFree` Takeover

- [x] Route pooled-size `xrtMalloc` through the new allocator
- [x] Route pooled-size `xrtFree` through the new allocator
- [x] Preserve large-allocation fallback path
- [x] Preserve error semantics and bootstrap safety

### 17.5 M3 `calloc/realloc` Completion

- [x] Implement pooled-size `xrtCalloc`
- [x] Implement safe `xrtRealloc` source detection
- [x] Handle same-class and cross-class `realloc` correctly
- [x] Verify zero-size and null-pointer compatibility behavior

### 17.6 M4 Explicit Pool Alignment

- [x] Replace default `xmempool` tree bucket lookup with class LUT
- [x] Add per-pool fallback-cutoff configuration for `xmempool`
- [x] Make `0` map to the official default `16..1024` layout
- [x] Build size-to-bucket LUTs sized to the configured cutoff
- [x] Add compatible debug headers/validation to `xfsmempool`
- [x] Preserve explicit allocator semantics and owner/shared behavior

### 17.7 M5 Debug Allocator System

- [x] Add debug macro routing for allocation APIs
- [x] Add live-allocation registry
- [x] Add double-free and invalid-free detection
- [x] Add wrong-allocator-free detection
- [x] Add canary or redzone validation
- [x] Add leak dump/report path
- [x] Add machine-readable report output
- [x] Add human-readable report output
- [x] Ensure release builds do not carry heavy debug cost

### 17.8 M6 Object Lifetime Diagnostics

- [x] Define debug object-type identifiers for explicit roots
- [x] Add debug register/unregister hooks for `Create/Destroy`
- [x] Add debug register/unregister hooks for `Init/Unit`
- [x] Report leaked or double-destroyed roots

### 17.9 M7 Temporary Memory Redesign

- [x] Replace pointer ring with thread-local scratch arena
- [x] Add spill-block path for oversized temp allocations
- [x] Route `xrtFreeTempMemory()` to arena reset + spill free
- [x] Add temp-allocation debug tags
- [x] Add temp usage and reset information to debug reports

### 17.10 M8 Validation and Documentation

- [x] Add allocator-specific tests
- [x] Add object-lifetime debug tests
- [x] Add temp-arena tests
- [x] Run coroutine/network/container benchmark comparison
- [x] Write debug usage guide and migration notes


## 18. Acceptance Criteria

This phase is accepted only if all of the following are true:

1. `xrtMalloc/xrtFree/xrtCalloc/xrtRealloc` use the new global allocator for
   pooled sizes.
2. Release builds keep only minimal allocation headers.
3. Debug builds can identify leaks, double-free, and invalid-free reliably.
4. Debug builds can report leaked explicit root objects.
5. `xmempool` default bucket selection no longer uses the old tree search for
   the standard class layout.
6. `xmempool` can be configured with a per-pool fallback cutoff and uses a LUT
   sized to that cutoff.
7. `xrtTempMemory()` uses the new scratch-arena model.
8. The coroutine, network, TLS, HTTP, and WS validation gates remain green.
9. Benchmarks show no unacceptable regression in mainline production paths.
10. Debug mode can emit usable machine-readable and human-readable memory reports.


## 19. Open Questions

1. What is the best initial span size per class for XRT's current workload mix?
2. Should `1024` remain fixed long-term or become a later tuning option?
3. If a later medium-allocation phase is opened, what class layout best serves
   `1025..4096` without harming buffer workloads?
4. Should debug registries use one global lock, sharded locks, or per-thread
   staging with merge?
5. Which explicit root objects should be covered in phase-1 of object lifetime
   diagnostics, and which can be added later?


## 20. Decision Log

1. `2026-03-16`: The initial official small-allocation range is fixed at
   `16..1024` bytes.
2. `2026-03-16`: Bucket selection must use a lookup table, not the current
   `xmempool` binary-tree search.
3. `2026-03-16`: Allocations above `1024` bytes fall back to the configured
   backing allocator in phase-1 of this overhaul.
4. `2026-03-16`: Release builds retain only minimal allocation headers.
5. `2026-03-16`: Heavy debug diagnostics are enabled only through macros/build
   flags and must not contaminate release hot paths.
6. `2026-03-16`: `xnet_mem` remains the primary network hot-path allocator and
   is not replaced by the global mempool.
7. `2026-03-16`: Temporary memory must be redesigned as a scratch arena rather
   than expanded as a pointer ring.
8. `2026-03-16`: Object lifetime diagnostics are implemented as a debug-only
   side registry rather than permanently bloating release object layouts.
9. `2026-03-16`: The `1025..4096` byte range is explicitly deferred from pool
   optimization in this phase and remains on the backing allocator path.
10. `2026-03-16`: Debug mode must produce both machine-readable and
    human-readable memory reports with event, leak, and aggregate views.
11. `2026-03-16`: `xmempool` and the global allocator do not have to share one
    allocator core; `xmempool` will use its own per-pool LUT mapping model
    because explicit pools and the process-global allocator have different
    concurrency and policy requirements.
12. `2026-03-16`: `xmempool` construction parameter `0` means the official
    default `16`-byte-step layout up to `1024`, while `N > 0` means a per-pool
    fallback cutoff of `N`.
13. `2026-03-16`: The first `xmempool` LUT migration keeps the existing MMU and
    big-allocation ownership semantics intact; only bucket selection and cutoff
    policy change in this phase.
14. `2026-03-16`: The phase-1 global allocator core is allowed to land as an
    internal skeleton before takeover. Class descriptors, lookup tables, thread
    cache state, and minimal release headers may exist ahead of the actual
    pooled `xrtMalloc/xrtFree` cutover.
15. `2026-03-16`: The phase-1 allocator stores the minimal release header on
    both pooled and fallback allocations so that `free/realloc` can use one
    metadata contract without a debug registry.
16. `2026-03-16`: Phase-1 debug mode uses header-based live tracking for the
    global allocator and a debug-only foreign-allocation registry for
    `xmempool`/`xfsmempool`, instead of modifying explicit-pool block layouts in
    release mode.
17. `2026-03-16`: Phase-1 quarantine is applied only to debug-mode backing
    allocations; pooled blocks return to their normal cache/central paths after
    validation to keep allocator behavior closer to release semantics.
18. `2026-03-16`: Phase-1 object lifetime diagnostics cover explicit-lifetime
    roots with stable `Create/Destroy` or `Init/Unit` APIs: `array`, `dict`,
    `list`, `avltree`, `dynstack`, `mempool`, and `fsmempool`. Roots without a
    stable destroy contract remain outside this phase.
19. `2026-03-16`: Temp arena blocks reuse the existing per-thread block chain
    before expanding, and oversize spill blocks come directly from the backing
    allocator instead of entering the pooled small-object path.
20. `2026-03-16`: Acceptance-grade benchmark waits use monotonic wall-clock
    deadlines instead of sleep-iteration counts so that timer granularity on
    Windows does not silently stretch benchmark timeouts and invalidate the
    pressure lane.
21. `2026-03-16`: The `xnet` stream recv path must re-arm receive work after a
    paused read drains buffered data; otherwise send-pressure validation can
    report false timeouts or partial receive completion even when the send queue
    has already drained.


## 21. Session Log

- `2026-03-16`: Created the initial global memory-pool and debug-system spec. Audited the current `xrtMalloc` wrappers, explicit pool allocators, thread memory state, temporary-memory ring, and `xnet_mem` hot-path allocator. Recorded the approved direction: `16..1024` LUT-based pooled allocation, `>1024` fallback to backing allocator, release-minimal headers, debug-only macro instrumentation, object lifetime registry, and scratch-arena temp memory.
- `2026-03-16`: Expanded the spec to lock in the phase-1 medium-allocation deferral policy (`>1024` stays on the backing allocator) and formalized the debug-reporting contract, including event taxonomy, report formats, aggregate requirements, wrong-allocator-free detection, and temp/object report coverage.
- `2026-03-16`: Updated the spec to split `xmempool` from the global allocator implementation strategy. `xmempool` will no longer depend on the old binary-tree bucket design and will instead own a size-to-bucket LUT sized by a configurable per-pool fallback cutoff; constructor parameter `0` maps to the official default `16..1024` layout.
- `2026-03-16`: Started M0 implementation. Added opt-in allocator telemetry state and public snapshot/reset APIs to the runtime, wired zero-semantic-change counters into `xrtMalloc/xrtCalloc/xrtRealloc/xrtFree/xrtTempMemory`, and added `dev/test_memtelemetry_core.c` plus `test/test_memtelemetry.h` as the first dedicated baseline harness. Verified the new harness passes and that the existing `dev/test_xnet2_stage.c` suite still runs cleanly after the instrumentation-only changes.
- `2026-03-16`: Completed M0 baseline capture and wrote the first telemetry report in `dev/bench/MEMORY_TELEMETRY_BASELINE_20260316.md`. Current evidence shows runtime/container churn is dominated by `<=32B` allocations while coroutine and xnet also exhibit concentrated pressure in a small set of pooled classes, with large fallback bytes still dominated by network/runtime setup paths.
- `2026-03-16`: Started M4 explicit-pool alignment. Replaced `xmempool` tree bucket selection with a size-to-bucket LUT, changed constructor semantics to `0 => default 16..1024` and `N > 0 => per-pool fallback cutoff`, added a focused `dev/test_mempool_core.c` harness, and updated in-tree tests/examples that still assumed the old `1/2` tree-plan selector.
- `2026-03-16`: Started M1 global mempool core scaffolding. Added the first official global class table, size-class LUT, minimal release header type, and thread-local cache skeleton to the runtime, then validated them with `dev/test_memglobal_core.c` while keeping `xrtMalloc/xrtFree` behavior unchanged.
- `2026-03-16`: Completed the first usable global allocator cutover. Added central per-class freelists, span allocation/refill, thread-local cache refill/drain, backing-allocation headers, and routed `xrtMalloc/xrtFree/xrtCalloc/xrtRealloc` through the new allocator. Verified focused `memglobal` takeover cases plus the runtime/coroutine/xnet baseline harness and the full `dev/test_xnet2_stage.c` suite.
- `2026-03-16`: Completed M4/M5. Added debug-routed `xrtMalloc/xrtCalloc/xrtRealloc/xrtFree` entry points, header-based live allocation tracking, front/tail canary validation, invalid/double-free detection, explicit-pool wrong-allocator detection via a foreign-allocation registry, bounded backing-allocation quarantine, and text/JSON report exporters. Added `dev/test_memdebug_core.c` plus `test/test_memdebug_core.h`, then verified both a dedicated `XRT_MEM_DEBUG` harness and the full `dev/test_xnet2_stage.c` suite under debug instrumentation.
- `2026-03-16`: Completed M6. Added a debug-only explicit-root object registry, wired `Create/Destroy` and `Init/Unit` hooks for `array`, `dict`, `list`, `avltree`, `dynstack`, `mempool`, and `fsmempool`, expanded the text/JSON memory reports with leaked-root sections and object lifecycle events, and upgraded `test/test_memdebug_core.h` to assert `OBJECT_LEAK` and `OBJECT_DOUBLE_DESTROY` coverage. Re-ran the focused `XRT_MEM_DEBUG` harness and the full debug `dev/test_xnet2_stage.c` suite successfully.
- `2026-03-16`: Completed M7 and advanced M8. Replaced the old temp-pointer ring with a thread-local scratch arena plus spill blocks, wired thread attach/detach cleanup into the new arena state, added temp allocation/reset counters to the debug reports, ensured existing arena blocks are reused across resets before expanding, and added `dev/test_temparena_core.c` plus `test/test_temparena_core.h` to verify reuse, spill handling, and temp debug report coverage in both release and debug builds.
- `2026-03-16`: Advanced M8 with the first focused validation report and usage docs. Added allocator and container churn benches, reran coroutine and xnet spot baselines on the current tree, wrote `dev/bench/MEMORY_POOL_BENCH_20260316.md`, and added `dev/XRT_MEMORY_DEBUG_GUIDE.md`. The benchmark phase remains in progress because the current `xnet` pressure lane timed out and is not yet accepted as a regression-grade result.
- `2026-03-16`: Completed M8. Fixed the `xnet` pressure-lane benchmark to use monotonic wall-clock deadlines instead of sleep-count loops, repaired the paused-read recv rearm path in `xnet_stream`, reran the official pressure lane successfully, and revalidated the full `dev/test_xnet2_stage.c` suite. With allocator, container, coroutine, xnet spot, and accepted pressure results in place, the benchmark/documentation phase is now closed.
