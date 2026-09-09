---
num: 22
slug: pool
title: Memory Pools and the Variable-Size Pool
volume: 卷三 容器与数据结构 · 卷三收官
type: practice
lead: Slot-based allocation for same-size objects, LIFO reuse, mark-and-reclaim and paged growth; the variable-size pool and pooling statistics.
api: pool, memory_stats
---

## Orientation

The pool is Chapter 5's allocator gone "wholesale": same-size objects are allocated by **slot** — pages are added automatically when full, frees go straight onto a LIFO free list, and the next allocation gets the **most recently freed** slot (cache still warm). Scenes with "thousands of the same thing" — connection objects, frame structs, message headers — are where the pool compresses allocation cost from "syscall grade" to "pop one pointer off the free list", while eliminating the fragmentation that same-size allocation never had anyway — slots never scatter because their sizes never differ. This chapter covers the fixed pool `xpool`'s complete lifecycle (paging, LIFO reuse, two mark-reclaim directions) and the variable-size pool (for mixed sizes), and verifies the pooling payoff with Chapter 6's stats — what shape the `pooled`-to-`backing` ratio curve takes after pooling is something you'll feel after one run.

## Introduction

A gateway service maintains a hundred thousand connections, each connection object 48 bytes. One-by-one `xrtMalloc/xrtFree`: every disconnect-reconnect passes through the system allocator, fragmentation accumulates slowly, and Chapter 6's stats show `backing` counts rising linearly with traffic. Switch to a fixed pool: connection objects come out of slots and go back into slots — allocation and free are both pointer operations; the syscall happens only at the instant of "adding a page" (a page is about 64KB and holds 256 slots of 48 bytes before the next one). However heavy the traffic, the `backing` curve is a staircase: one step per 256 allocations. That is the pool's entire value — **raising the granularity of allocation from object level to page level**.

When should you not use a pool? When object sizes vary wildly (use the variable-size pool or the plain heap), when object counts are tiny (pool bookkeeping outweighs the benefit), when lifetimes are uniformly short (Chapter 7's arena fits better — the pool answers "frequent take-and-put-back", the arena answers "reclaim all together"; they answer different questions, often confused, never interchangeable). This "when not to" matters as much as the "when to" above: correct pool use begins with a correct "not using".

## Concepts

### The fixed-pool model

```diagram flow
- Initialization: build the pool by object size + alignment; per-page capacity is computed automatically (~64KB/page)
- Allocation: Alloc/Calloc take a slot from the free list; if empty, add a page
- Free: Free goes straight onto the LIFO free list - the next allocation gets the most recently freed slot
- Cleanup: Trim returns empty pages / Reset clears everything / Unit destroys
```

LIFO reuse is not an implementation detail, it is a **cache-friendly design decision**: the most recently freed slot is very likely still in some cache level; reusing it beats taking a long-untouched slot — the example will prove `pReused == pReleased` as an assertion. `Owns` asks whether a pointer belongs to this pool — the security gate against foreign pointers sneaking into Free (Freeing a heap pointer to a pool is undefined behavior; Owns is that checkpoint).

### Two mark-reclaim directions

Freeing one by one is wasteful in batch-processing scenes — returning a thousand objects individually loses to marking them and processing once. The pool offers both directions: **Sweep keeps the marked and reclaims the unmarked** (the batch processor marks "still wanted", everything else is swept away — cache eviction, end-of-request cleanup); **FreeMarked frees the marked and keeps the rest** (those whose reference count hit zero are marked and put back — semi-automatic reclamation paired with Chapter 3's reference counting). The two directions cover "keep the few, delete the many" and "delete the few, keep the many"; choose whichever side has fewer marks.

### The variable-size pool and pooling statistics

The variable-size pool (demonstrated by the `memory_pool` sample) handles mixed-size allocation: internally bucketed by size, small allocations go through buckets, large ones pass straight through. Chapter 6's `xmemstats` columns `pooled` (pool hits) and `backing` (syscalls) are the observability face of pooling — in a well-pooled service, business allocation counts far exceed backing, and the curve is a staircase (the page-add moments). Capacity planning and memory budgeting also land here: `live/peak` tell you the pool's actual water level.

### Pages and return

`SetRetain` sets how many empty pages survive a `Reset` (pre-warming for bursty traffic); `Trim` proactively returns empty pages (memory-tight periods); the heap-creation forms `Create`/`CreateLayout` can pin the slots per page explicitly (aligning with your business batch size). Page lifetimes belong to the pool — you never touch pages directly, only go in and out through the slot interface.

## Examples

### Complete program: the fixed pool's full lifecycle

From the repository example `examples/memory/pool/main.c` — paged growth, LIFO reuse, mark-reclaim, and statistics verification in one net:

```embed path="examples/memory/pool/main.c" title="examples/memory/pool/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/pool/main.c -lws2_32 -liphlpapi
page_capacity=256 pages=2 empty=2 live=0 peak=300 visited=300
```

**What just happened.** (1) Allocating 300 objects: 256 slots per page, the first two pages hold 512 — `pages=2`, `peak=300`. (2) The proof of free-and-reuse: free some slot then allocate immediately, and you get **the same address back** — the LIFO free list turns "most recently freed, first reused" into assertable behavior. (3) After mark-reclaim, `live=0` and `empty=2`: both pages fully idle yet still retained (the `SetRetain` pre-warm semantics); `visited=300` comes from `Visit`'s slot-order traversal — walking live objects for statistics or as GC mark roots is this interface. (4) Every number in the last output line reconciles from Chapter 6's statistics side — the pool's observability and the allocator chokepoint are one system.

### Complete program: the variable-size pool and water-level statistics

From `examples/memory/memory_pool/main.c`, pooling for mixed sizes:

```embed path="examples/memory/memory_pool/main.c" title="examples/memory/memory_pool/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/memory_pool/main.c -lws2_32 -liphlpapi
visited=3 small=0 large=0 live=0 peak_bytes=4215
```

**What just happened.** (1) Mixed-size allocation: small objects hit size buckets (the `small` count), large ones pass through (the `large` count) — the variable-size pool pays off most on the real workload of "mostly small + a few large". (2) `peak_bytes` gives the measured basis for the memory budget: the pool's peak water level is the input to capacity planning. (3) After freeing everything, `live=0` — the pool returns to its initial state and is reusable, and `Unit` returns it wholesale. Compared with the fixed pool: the variable-size pool trades a bit of slot-location speed for the management convenience of "not one pool per size" — choosing between them is just asking whether your object sizes are uniform.

### The pool's relation to Chapter 5's allocator

The pool is not the allocator's replacement; it is the wholesale layer above it. The most flexible combination is exactly Chapter 5's allocator replacement: implement a custom allocator doing size-based routing to the matching pool, and the whole program's allocation behavior converges at one point — this is where the kernel `memory_stats` pooled count comes from (the 48-byte size-class cache hit in Chapter 6's example is the kernel's built-in small-object pool at work). Business-side pools are more direct: a connection pool, a frame pool, a message pool, each minding its own same-size objects. The two routes don't conflict: the kernel pool backstops general small objects, business pools carry domain objects.

### Object construction versus slot custody

The pool manages only slot take-and-put-back, never object construction or destruction — the slot content you get is undefined (except the Calloc variant, zero-initialized). The standard pairing: `Alloc` takes a slot, business initialization constructs the object; destruct the object, `Free` returns the slot. Batch scenes needing destructor callbacks use `Visit` to walk live objects and finish them one by one, then Reset/Trim for bulk reclamation — traversal-finish, bulk-reclaim, page management: these three steps are the complete closed loop of pooled-object lifecycle management, and Chapter 66's connection table will use its full form.

## Contracts

- **Same-size slots**: the fixed pool slots by the size + alignment given at `Init`; different sizes get their own pools, or switch to the variable-size pool.
- **LIFO reuse**: freed slots go straight onto the free list; the next allocation preferentially reuses the most recently freed.
- **The checkpoint**: `Owns` decides pointer ownership; foreign pointers may not enter the pool's Free.
- **Mark direction**: keep-few-delete-many uses Sweep; delete-few-keep-many uses FreeMarked — choose by whichever side has fewer marks.
- **Page management**: `SetRetain` pre-warms empty pages, `Trim` returns them, `Reset` clears all; pages belong to the pool, business touches only slots.
- **Statistics reconciliation**: verify pooling with Chapter 6's `pooled/backing`; plan capacity with the `live/peak` water level.

### From examples to engineering: the pool's three hosts

Where the pool handle lives decides its role. **Service-resident pool**: hung on the service object, pre-warmed at startup per capacity planning (the pool version of the `Reserve` semantics is pre-allocating the first page), doing only slot take-and-put at runtime — for a hundred-thousand-connection gateway, the connection pool is this form. **Request-scoped short-lived pool**: created within a batch, Reset-with-Retain at batch end, Unit when truly done — division of labor with Chapter 7's arena (the pool manages object take-and-put, the arena manages temporary fragments). **Process-wide wholesale layer**: Chapter 5's allocator replacement wired to pools — business code is unaware, allocation behavior pooled wholesale. The three hosts weight configuration differently: resident pools emphasize capacity planning and water-level monitoring, short-lived pools the Retain/Trim rhythm, the wholesale layer size-distribution statistics.

## Pitfalls

### Pitfall 1: Freeing a heap pointer to the pool

Symptom: the free list is polluted by a foreign pointer, and later allocations return wild pointers — the crash lands in completely unrelated code, the hardest kind of accident to chase.

Cause: the pool's Free merely "hangs the slot back onto the free list"; it does not validate pointer provenance (validation costs, and the hot path won't carry it); the mixed-in heap pointer gets handed out as a slot next time.

```c bad
ptr pObject = xrtMalloc(64);        /* heap allocation */
/* ... after some tangled code ... */
xrtPoolFree(&tPool, pObject);        /* heap pointer into the pool - the free list is polluted */
```

```c good
ptr pObject = xrtMalloc(64);
/* unsure of provenance? ask the pool first */
if ( xrtPoolOwns(&tPool, pObject) ) {
	xrtPoolFree(&tPool, pObject);
} else {
	xrtFree(pObject);                /* return it to the right owner */
}
```

### Pitfall 2: using a slot pointer obtained before Reset

Symptom: after batch processing, accessing an old object pointer reads new data or the content of a returned page — "ghost data".

Cause: `Reset`/`Sweep`/`Trim` all carry **bulk-reclaim** semantics, invalidating all (or all unmarked) slots at once — every slot pointer previously handed out expires.

```c bad
connection* pConn = xrtPoolAlloc(&tPool);   /* take a slot */
ProcessRequest(pConn);
xrtPoolReset(&tPool);                        /* bulk reclaim: pConn has expired */
Reply(pConn);                                /* ghost access */
```

```c good
connection* pConn = xrtPoolAlloc(&tPool);
ProcessRequest(pConn);
Reply(pConn);                 /* finish using it before reclaiming - pointer validity ends at the reclaim point */
xrtPoolFree(&tPool, pConn);   /* or hand it to mark-reclaim at the batch boundary */
```

## Exercises

### Basic: proving LIFO reuse

Allocate 10 slots, free the 3rd and allocate immediately — assert the new address equals the freed one; then free 3 consecutive slots and observe the reuse order (the later-freed come back first); finally use `Visit` to count live objects — it should be 10-3-1+1 (walk through your own operation sequence before verifying).

### Advanced: a request batch processor (mark-reclaim in practice)

Implement a batch of requests with "allocate-process + Sweep mark-reclaim": during processing, surviving requests are Marked one by one; at batch end, Sweep reclaims the unmarked in one stroke; output the reclaimed count and the `live` water level. Hint: the Sweep direction fits the shape "most requests complete, a few hang across batches".

### Challenge: reconciling before and after pooling (proving the pool's value with numbers)

Simulate a hundred thousand "take connection — return connection" cycles: once with bare `xrtMalloc/xrtFree`, once with a fixed pool, recording `backing` counts and elapsed time with Chapter 6's stats in both runs. Acceptance criteria: the pooled run's backing count is a staircase (~one step per 256 allocations) and its elapsed time is clearly below the bare-heap run; output the two comparison tables with the exercise.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Fixed pool | same-size slots + paged growth (~64KB/page) + LIFO free-list reuse — slots never fragment |
| Reuse proof | free then allocate immediately gets the same address — the cache is still warm |
| The checkpoint | `Owns` decides ownership; foreign pointers are banned from Free — one slips in, the free list is polluted |
| Mark-reclaim | Sweep keeps the marked, deletes the rest / FreeMarked deletes the marked, keeps the rest |
| Page management | `SetRetain` pre-warms, `Trim` returns empty pages, `Reset` clears all |
| Variable-size pool | size buckets + large-block pass-through; pooling for mixed-size loads |
| Reconciliation tools | Chapter 6's `pooled/backing` staircase curve + `live/peak` water level |
| Division with arena | the pool manages "frequent take-and-put"; the arena manages "reclaim together"; the two often serve side by side |
| Three hosts | service-resident pool / request-scoped pool / allocator wholesale layer — different configuration emphases |
| Construction pairing | Alloc takes the slot → business constructs; destruct → return the slot; batches finish via Visit |
