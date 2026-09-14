---
num: 137
slug: perf
title: Performance Tuning and Memory Debugging in Practice
volume: 卷十二 工程实践 · 卷十二收官
type: practice
lead: The tuning ladder from counters to time, the two levers of pooling and batching, the practical flow of the memory-debug trio, and backend choice for coroutine switching — methodology landing, closing the volume.
api: memory_stats, memory_debug, core
---

## Orientation

Volume 12 closes: Chapter 136 gave the methodology; this chapter is the **field manual** — applying the method to XRT itself. Four blocks: **the tuning ladder** (stats counters → debug detail → time benchmarks — the fixed order of allocations first, time later; each rung answers a different question); **the two levers** (pooling — hot-path object reuse (the pool/pool_page trade); batching — amortizing system calls and atomic coordination (the benchmark evidence chain of Chapters 21/68)); **memory debugging in practice** (the Reset/Snapshot/VisitLive trio locating leaks; the quarantine catching double-free/UAF; stats always on for alarms — three tool layers each in place); **backend choice** (querying and benchmarking coroutine-switch backends — the selection evidence when context switches run a million times a second). The book's performance threads (Chapter 62's skeleton economics, Chapter 99's diagnostic fields, Chapter 111's ownership matrix's performance face) gather here.

## Introduction

The common-sense rate of tuning: **most "performance problems" are not algorithm problems** — they are over-allocation (building containers per request), over-frequent system calls (sending item by item), or the measurement itself being wrong (Chapter 136's noise). Hence practice's first action is always **look at counters** (stats: malloc_calls/backing — how many allocations, how many reached the system), not time — counters are deterministic (no noise, no environment dependency) and point straight at the cause; time only tells you "slow", never "why". XRT freezes this ladder into tools: stats (always-on counters) → memory_debug (event detail) → bench (time statistics) — **each rung answers one class of question**: how many / which one / how long.

The two levers' quantification comes from the benchmarks cited in Chapter 136: **batching** — MPSC single-item about 8M items/s, batch 32 about 98M (**12×**: coordination cost amortized); **pooling** — small blocks hitting the pool at zero system calls (stats' pooled/backing readings show it directly). The levers' shared essence: **amortize fixed costs** — pooling amortizes allocation/free, batching amortizes synchronous coordination/calls. Practical order: first ask "can it batch?" (business shape permitting, the biggest win), then "should it pool?" (effective when the object lifecycle is stable).

## Concepts

### The tuning ladder: three observation rungs

```diagram flow
- Rung 1 stats (always-on counters): malloc_calls / pooled / direct / backing / live
  - answers "how many": allocation scale, pool hit rate, live trend (alarm thresholds)
- Rung 2 memory_debug (briefly on while troubleshooting): every event + site + quarantine
  - answers "which one": leak points (file:line), double-free/UAF scenes
- Rung 3 bench (profiled time): median + gates + A/B
  - answers "how long": before/after optimization, selection comparisons, regression monitoring
```

**Ladder discipline**: skipping rungs (going straight to timing) is the most common detour — allocation anomalies are visible at rung one. **Tool cost rises per rung** (counters near zero / detail high / benchmarks minutes) — cost dictates the always-on policy: rung one always, rung two during troubleshooting, rung three on change.

### Lever one: pooling

The two-form trade (`pool` vs `pool_page`): **multi-page pool** — capacity grows automatically (pages added when full), LIFO free list (reusing the most recently freed slot — the cache is still warm); **single-page pool_page** — one contiguous block + pure pointer arithmetic (no page chain, no cross-page addressing), capacity stops at the top — **the first choice for embedded/hot-path objects (AST nodes, message frames)**. Pooling's precondition: **equal-sized objects + stable lifetimes** (the build-use-free rhythm repeats); when unmet (mixed sizes/messy lifetimes), pooling adds overhead instead — Chapter 22's selection judgment revisited in practice. **Verified by stats**: after pooling, the pooled share rises and backing falls — the lever working, read directly.

### Lever two: batching

Batching amortizes two cost classes: **system calls** (network sends item by item → the Vec family in one — Chapter 68's batch rung of the five send gears) and **atomic coordination** (queues item by item → batch reservation — the queue benchmark's 12× evidence). **Design implication**: when the caller can naturally group (many messages per frame / many tasks per request), **prefer batch interfaces** — this is API selection, not a performance trick; when grouping is impossible, fall back to single items (interface semantics equivalent — Chapter 21's "learn one usage, three queues share it", batch edition). **Sweep fixes the batch size**: batch returns saturate (the queue sweep flattens past 32) — blindly enlarging only adds latency, not throughput — Chapter 136's sweep experiment as selection output.

### The memory-debugging practice flows

**Leak-localization flow** (Chapter 6's tools in battle formation): `xrtMemDebugReset` before the suspect segment → run it → `Snapshot` → a `LiveCount` that won't reach zero means a leak → `VisitLive` prints the site (file:line) → fix → rerun to zero. **Corruption-localization flow**: the quarantine's delayed reuse makes double-free/UAF **necessarily visible** (instead of passing silently) — crash point + quarantine counts (the JSON report) cross-locate. **Long-running alarms**: periodic sampling of stats' live/peak — a monotonic rise = a leak trend (no need to wait for OOM). The three flows share one principle: **the evidence for memory errors waits inside the tools** — no guessing required.

### Backend choice: the coroutine-switch sample

Coroutine performance's critical path is **context switching** (cost per switch × a million per second). `xrtCoBackend()` queries the current backend (assembly/POSIX family — decided by platform and build); the benchmark family of `dev/bench/coroutine` quantifies: `bench_context_switch` (pure switch throughput), `bench_create_destroy` (coroutine build/destroy cost — the basis for pooling), `bench_sched_post` (scheduler posting), `bench_timer_churn` (timer churn). **The selection flow**: backend query → context_switch benchmark → contrast with thread switching (Chapter 55's conclusion verified locally) — **numbers support the design choice** (the evidence base for coroutine-ize vs thread-ize).

## Examples

### First complete program: the pooling lever, measured

The program below is from `examples/memory/pool_page` — single-page pool micro-observation:

```embed path="examples/memory/pool_page/main.c" title="examples/memory/pool_page/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/pool_page/main.c -lws2_32 -liphlpapi
stride=32 alignment=32 live=0 capacity=256
```

**What just happened.** (1) `stride=32` (slot stride) and `alignment=32` — **equal-sized slots**, pooling's precondition expressed directly on the API face; capacity=256 stops at the top (the single-page form's boundary semantics). (2) Against the `pool` sample (multi-page: `page_capacity=256 pages=2 ... peak=300` — automatic page addition beyond capacity): **the two-form trade** read side by side — freedom to grow versus pure pointer arithmetic's speed. (3) The selection basis for hot-path objects (AST nodes/message frames): Chapter 22's theory + this example's boundary + stats' pooled reading — three faces composing the practical judgment.

### Second complete program: multi-page pool reuse semantics

The second program is from `examples/memory/pool` — LIFO reuse and page growth, measured:

```embed path="examples/memory/pool/main.c" title="examples/memory/pool/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/pool/main.c -lws2_32 -liphlpapi
page_capacity=256 pages=2 empty=2 live=0 peak=300 visited=300
```

**What just happened.** (1) `peak=300 > 256×1` — capacity overflow triggered page addition (pages=2); after clearing, `empty=2 live=0` — pages present but empty (**the next round reuses without allocating** — peak memory traded for later zero allocations). (2) The LIFO free list's cache meaning (header comment): freed slots enter at the head, the next Alloc gets **the most recently freed** slot — cache still warm (the `pReused == pReleased` verification point). (3) The pooling lever's complete loop: this example's reuse semantics + the stats sample's pooled reading + a queue-style benchmark — **from semantics to observation to numbers**, the lever's three-way evidence.

## Contracts

- **Ladder discipline**: stats (how many) → debug (which one) → bench (how long) — counters before time; cost dictates the always-on policy.
- **Batching first**: when natural grouping exists, prefer batch interfaces — an API-selection-grade lever; the saturation point comes from sweep.
- **Pooling preconditions**: equal-sized objects + a stable lifecycle; when unmet, don't pool. Two forms: single-page (pure arithmetic / capped) / multi-page (auto growth / LIFO reuse).
- **Leak trio**: Reset→Snapshot→VisitLive (site to file:line) — the evidence waits in the tools.
- **Corruption visible**: the quarantine makes double-free/UAF necessarily visible — no relying on luck.
- **Long-running alarms**: periodic live/peak sampling — a monotonic rise is a trend alarm.
- **Backend queryable**: `xrtCoBackend` + the coroutine benchmark family — numeric selection of switch cost.
- **Observation reuse**: Chapter 99's Info fields (Wire/Bytes/timestamps) are the network-face health check — complementary to the memory ladder.

## Pitfalls

### Pitfall 1: timing without looking at counters

Symptom: several optimization rounds, time unchanged — eventually finding the bottleneck is one system allocation per request (visible at a glance in stats).

Cause: skipping rungs. Time says only "slow"; counters say "why" (too many allocations / pool misses / frequent system calls) — the ladder's first rung is always cheap and always on.

```c bad
/* suspect slow -> change code directly -> run the benchmark */
tweak_something();
bench();   /* time unchanged: never knew why it was slow - changed nothing */
```

```c good
/* counters first: a stats snapshot - backing abnormally high */
snapshot_stats();
/* locate the allocation hot spot -> pool/batch -> then time */
```

### Pitfall 2: pooling mixed-size objects

Symptom: the pool's internal fragmentation eats more memory, hit rate low — after pooling, stats actually worsen.

Cause: pooling's precondition is **equal sizes**. Mixed sizes (a different buffer per request) through a pool = waste per size class + management overhead — the practical face of Chapter 22's "when unmet, don't pool".

```c bad
/* message buffers 32B..4KB, mixed sizes, all through one pool */
/* internal fragmentation + low hits: memory up, performance down */
pool_init(&OnePoolForEverything, 4096);
```

```c good
/* equal-sized frames/nodes through the pool; mixed sizes through the default heap */
pool_init(&FramePool, sizeof(frame));   /* equal size: legitimate */
/* mixed sizes: just verify backing is reasonable via stats */
```

### Pitfall 3: blindly enlarging batches

Symptom: batch raised from 32 to 512 — throughput flat, latency up (waiting to fill batches).

Cause: batch returns saturate (past amortization it's pure waiting cost); the sweep curve fixes the inflection — beyond it, more batch only adds latency.

```c bad
#define BATCH 512   /* "more is faster" - throughput flat, latency up */
```

```c good
/* sweep finds the saturation point: flattens past, say, 32 */
#define BATCH 32    /* the inflection is the choice; latency-sensitive scenes go smaller */
```

## Exercises

### Basic: two observation rungs linked

On business code with allocations: first a stats snapshot (counters), then the debug trio (detail) — what does each rung answer? Acceptance criteria: the counter rung can judge "abnormal or not"; the detail rung can name any single event's site.

### Advanced: before/after pooling

An AST-node-like workload (equal-sized 32B build-use-free looped 100k times): default heap vs pool_page — stats' backing comparison + a time benchmark A/B. Acceptance criteria: backing trends to zero after pooling; the time improvement consistent with the allocation share (explainable).

### Challenge: an end-to-end tuning report

Pick a combined scenario (say JSON parsing + container assembly): walk the whole ladder (counter localization → pooling/batching optimizations → benchmark A/B) — write a four-section report (Environment/Findings/Changes/Results). Acceptance criteria: every change backed by numbers; the report structured like a dev/bench report (archivable).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Ladder | stats how many → debug which one → bench how long — counters before time |
| Lever one: pooling | equal-sized + stable lifetimes; single-page (fast/fixed capacity) / multi-page (growth/LIFO reuse) |
| Lever two: batching | amortizes system calls + atomic coordination; sweep fixes the saturation point |
| Quantified evidence | batching 12× (queue benchmark); pooling backing → zero (stats) |
| Leak trio | Reset→Snapshot→VisitLive (site to line) |
| Corruption visible | the quarantine — double-free/UAF always shows |
| Long-running alarms | periodic live/peak sampling — trends without waiting for OOM |
| Backend selection | CoBackend query + switch benchmark — numbers for the coroutine decision |
| Network-face check | Chapter 99's Info fields — Wire/Bytes/timestamps |
| Closing position | methodology (135) landing (136) — the volume's engineering practice joined |
