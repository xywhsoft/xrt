---
num: 134
slug: embed
title: Embedding into a Host Program
volume: 卷十二 工程实践
type: practice
lead: Allocator takeover with first-allocation locking, mapping the error slot into the host, host-side module trimming, and always-on production stats — installing XRT into your program without it stealing the show.
api: core, memory_stats, error
---

## Orientation

"Embedding" is a first-class viewpoint in XRT's design — every chapter's APIs live under the assumption of "being part of a host program". This chapter gathers the integration points scattered across chapters into one manual: **memory** (`xrtSetAllocator` takes over as soon as main enters — afterwards every library allocation goes to the host; first-allocation locking keeps it stable); **errors** (mapping the thread error slot into the host's exception system — every "return value announcing failure" is a candidate source of host exceptions; error objects can travel across layers); **trimming** (the host selects only the root modules it needs — Chapter 130's trim semantics applied host-side; the minimal integration never silently grows); **observability** (`memory_stats`' near-zero-cost always-on counters — the host's health panel reads them directly); **form selection** (single-header/static/dynamic — Chapter 131's three consumer kinds as the host's decision). Embedding's highest goal: **the library is the host's component, not a parallel kingdom** — memory under your governance, errors translated by you, size decided by you.

## Introduction

The most common failure shape of host integration is "the library wants to be the master": malloc on its own initiative (the host's memory budget slips out of control), errors only printed (the host's error system can't catch them), the full payload dragged in (embedded can't hold it), global state deciding for itself (multi-instance/multi-version coexistence crashes). Every XRT integration point is a direct answer to these failures — and each is pinned by tests (allocator replacement has `allocator_tour`'s dual-track verification, error mapping has `_threads`' slot-isolation verification, trimming has the size-profile assertions, stats has the overhead baseline).

First build the **timeline view of takeover**: main's first act swaps the allocator (before locking) → initialize host facilities → enable stats as needed → during business runtime the error slot travels with the thread → at teardown, a stats snapshot + Drain of all library objects. Wrong order (swapping after the first allocation) is exactly the rejection scenario of Chapter 5 — this chapter strings them into a timeline.

## Concepts

### Memory takeover: the allocator and the lock

```diagram flow
- Timing: as main enters (before any XRT allocation) -> xrtSetAllocator(&host allocator)
- After takeover: every library allocation (pools/buffer internals included) goes to the host - budget, stats, arena under host governance
- Locking: after the first allocation SetAllocator is rejected - multi-TU/multi-module scenarios can never swap again
- Shape: the at-family four entrances (Alloc/Calloc/Realloc/Dup) + callback context
```

Common host allocator shapes: **arena** (request-scoped bump allocation — the whole block frees at request end, Chapter 7's host application), **budget counting** (a quota allocator — the library's memory ceiling belongs to host policy), **proxy accounting** (forwarding to the system allocator + bookkeeping — the host panel reads the library's consumption directly). **The lock's meaning**, stressed again: two library modules (or two places in host code) both try to swap the allocator — first come first served, the later one rejected — integration stability over flexibility.

### Error mapping: from the slot into the host's system

XRT's failure report is "return value + thread error slot" (Chapter 4) — the host-side mapping asks three questions: **where to intercept** (check return values at every XRT call site — a macro/wrapper function intercepts uniformly); **how to translate** (`xrtErrorKind` → host exception type/error code; `xrtErrorMessage` → human message; the cause chain → log expansion); **how to carry** (across threads, first `TakeError` — Chapter 4's carrying semantics honored inside the host's task queue). **Slot isolation**: host threads each hold their own slot — the isolation verified by `_threads` tests is exactly the host's concurrency-safety guarantee.

### Trimming and size: the host's right to choose

The host declares root modules (Chapter 130) — a "containers + strings only" host artifact contains no network symbol whatsoever (asserted by forbid_symbols). **Evolution safety**: no selection = Core — XRT adding modules in the future never silently enlarges the host artifact (the contract states this rule's purpose explicitly). **Multi-TU consistency**: all the host's TUs use a consistent module set (at modular link time) — the build system defines it in one place (CMake's target_compile_definitions declared once).

### Observability: stats always on in production

`memory_stats`'s (introduced in Chapter 6) host-side role is **the health panel's data source**: `xrtMemStatsEnable(true)` once, periodic `Snapshot` reads — `malloc_calls` (business allocation count) / `pooled` (pool hits) / `direct` (pass-through) / `backing` (calls reaching the system) — four readings forming memory health's minimal metric set. **Near-zero overhead** is the premise of always-on (counting only, no records — contrast memory_debug's full-record high cost); leak detection (live never returning to zero) goes into long-running services' alarm thresholds.

### The host's three-form decision

Single header (scripts/prototypes — one include) / static library (real projects — link-frozen) / dynamic library (plugin hosts — independent upgrades) — Chapter 131's consumer choice restated in host context as an integration decision: **the host build system's hookup cost** and **the distribution form** decide; behavioral consistency (asserted by verify) makes switching imperceptible.

## Examples

### First complete program: the always-on stats health panel

The program below is from `examples/memory/stats` — an observability sample in production form:

```embed path="examples/memory/stats/main.c" title="examples/memory/stats/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/stats/main.c -lws2_32 -liphlpapi
before=0 after=1
malloc_calls=2 pooled=1 direct=1 backing=3
```

**What just happened.** (1) `xrtMemStatsEnable(true)` + `Reset` — query the switch (before/after) then zero it: the observability discipline of **confirm first, then act**. (2) After two business allocations (a 48B small block + a 4096B large block), the snapshot reads: `pooled=1` (small block hit the pool — zero system calls), `direct=1` (large block over the threshold, passed through), `backing=3` (3 actual on the system side — 1 pass-through + 2 pool-internal warm-ups) — **the reconciliation of the business view with the system view** is exactly the raw material of capacity planning. (3) The host panel periodically running "snapshot + read" is the entirety of library memory health monitoring — always-on costs nearly nothing (counter increments).

### Second complete program: full verification of allocator takeover

The second program is from `examples/core/allocator_tour` — host memory takeover acceptance:

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**What just happened.** (1) After the swap, **all four at-family entrances verified** (Alloc/Calloc/Dup/Realloc) — the complete face a host allocator must implement (not just Alloc — Realloc's semantics are often forgotten). (2) The second line's locking verification: a second SetAllocator rejected — **the host integration's timing contract** (must precede the first allocation) turned from error into tested fact. (3) These two samples together are host integration's "Hello World+": takeover (this one) + observability (the previous) — memory sovereignty and visibility arriving together.

### The host checklist: the acceptance order of integration

Condense this chapter into a **pre-launch checklist** — tick items in dependency order: (1) allocator taken over at main's first line (all four at-family entrances implemented, locking semantics understood); (2) module trim declared (root-module list into the build system, size profile's forbidden symbols verified); (3) error-mapping layer in place (Kind translation table, cause chain into logs, the cross-thread TakeError path); (4) observability always on (stats enable + periodic snapshots + four-reading alarm thresholds); (5) diagnostic form decided (production EXCLUDE applied, debug builds switchable); (6) artifact form chosen (single-header/static/dynamic — hookup with the host build system verified); (7) teardown path rehearsed (all library objects Drained, final stats snapshot, error slots cleared). All seven ticked, and XRT merges into the host as a "component" — memory visible, errors translatable, size controllable, behavior verifiable — embedding's complete state is not "it compiles" but "sovereignty is clear".

## Contracts

- **Takeover timing**: SetAllocator before any XRT allocation (main's-first-line level); locked after the first allocation; all four at-family entrances implemented.
- **Memory sovereignty**: after takeover, every library allocation (pool internals included) goes to the host — budget/stats/arena belong to the host.
- **Error mapping**: check return values at call sites → translate Kind → display Message → cause chain into logs; across threads, TakeError first.
- **Slot isolation**: host threads each hold their own error slot — concurrency safety locked by _threads tests.
- **Trim sovereignty**: the host declares root modules; no selection = Core without growth; multi-TU sets consistent (one definition in the build system).
- **Observability always on**: stats counts only, near-zero cost; the four readings (calls/pooled/direct/backing) are health's minimal set.
- **Form consistency**: single-header/static/dynamic behavior asserted by verify — host switching at zero cost.
- **Diagnostics removable**: the MEMORY_DEBUG exclusion semantics (Chapter 130) — full production functionality minus diagnostics.

## Pitfalls

### Pitfall 1: swapping the allocator after initializing the library

Symptom: SetAllocator returns failure — the library's internal structures (pools/first caches) were already built with the default allocator.

Cause: the locking contract. The host's main does the swap first — any "let me initialize something first" ordering can cross the line.

```c bad
int main(void) {
	init_something_using_xrt();   /* already allocated inside */
	xrtSetAllocator(&MyAlloc);    /* rejected: the first allocation happened */
}
```

```c good
int main(void) {
	xrtSetAllocator(&MyAlloc);    /* the first act */
	init_something_using_xrt();   /* everything goes through the host allocator */
}
```

### Pitfall 2: host wrappers swallowing error detail

Symptom: the host side sees only "failed" — category/domain/cause chain all lost — retry strategy can only guess.

Cause: a lazy mapping layer (passing only bool). Kind is the machine-readable decision basis (Chapter 4) — TIMEOUT retries, AGAIN waits, ARGUMENT reports a bug — swallowing it degrades structured errors to a boolean.

```c bad
bool my_json_parse(str s, xvalue** out) {
	if ( !xrtJsonParse(...) ) { return false; }   /* all detail lost */
}
```

```c good
bool my_json_parse(str s, xvalue** out, my_error* e) {
	if ( !xrtJsonParse(...) ) {
		fill_host_error(e, xrtTakeError());  /* Kind/domain/chain fully translated */
		return false;
	}
}
```

### Pitfall 3: confusing stats with debug (both on)

Symptom: production runs with memory_debug enabled (every allocation recorded in detail) — performance drops, memory doubles.

Cause: the two differ in role: stats counts (always on), debug records details (briefly on while troubleshooting). The production default combo is stats on + debug excluded (`XRT_EXCLUDE_MEMORY_DEBUG` — Chapter 130's combination semantics).

```c bad
#define XRT_MODULE_ALL   /* includes memory_debug - full detail recording in production */
```

```c good
#define XRT_MODULE_ALL
#define XRT_EXCLUDE_MEMORY_DEBUG   /* production: keep stats, drop detail */
/* debugging builds add XRT_MODULE_MEMORY_DEBUG explicitly */
```

## Exercises

### Basic: request-scoped arena integration

A host arena (bump allocation + whole-block free at request end) as XRT's allocator: run container + string business within one request. Acceptance criteria: zero system allocations inside the library (backing=0 or only the arena's big block); one free at request end; stats reconciles.

### Advanced: the error-mapping layer

Write host mapping functions (Kind → host error codes, Message + cause chain → log structures) + wrap three common calls. Acceptance criteria: six Kinds each map correctly; a cross-thread task carries its error (the TakeError path) losslessly.

### Challenge: multi-tenant embedding

Two "tenants" (independent business modules) in one process, each with its own trim, sharing one XRT instance: a unified allocator (budgets accounted per tenant — context distinguishes), errors isolated per thread, stats read per tenant. Acceptance criteria: tenant A's OOM doesn't affect B; errors never cross slots; the payload is the union of both tenants' closures.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Highest goal | the library is the host's component — sovereignty over memory/errors/size rests with the host |
| Takeover timing | SetAllocator before any XRT allocation; all four at-family entrances implemented |
| Locking | no swap after the first allocation — integration stability first |
| Error mapping | return-value points → Kind translation → chain into logs; TakeError across threads |
| Slot isolation | threads each hold their slot — the host concurrency guarantee locked by _threads tests |
| Trim sovereignty | declare root modules; no selection = Core; evolution never silently grows |
| stats always on | near-zero-cost four readings: calls/pooled/direct/backing |
| Diagnostics removable | ALL+EXCLUDE_MEMORY_DEBUG — full production functionality minus detail |
| Form decision | single-header/static/dynamic — behaviorally consistent, zero-cost switching |
