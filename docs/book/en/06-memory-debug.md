---
num: 6
slug: memory-debug
title: Memory Debugging and Statistics: Fault Injection and Leak Detection
volume: 卷一 起步与核心
type: practice
lead: Make "the N-th allocation fails" an injectable test condition; make leaks and allocation behavior readable numbers.
api: memory_debug, memory_stats
---

## Orientation

Chapter 5 choked all library memory down to five entry points; this chapter cashes in two more dividends of that chokepoint: **fault injection** — `xrtMemDebugFailAfter` makes the (N+1)-th allocation fail exactly, to verify that failure paths roll back cleanly; and **statistics** — `xrtMemStatsGet` answers "how much was allocated, how high was the pool hit rate, how big was the peak" with a set of counters. The two modules divide the labor clearly: the debug module records per-allocation detail (heavy, for locating problems), the stats module only counts (nearly zero cost, safe to keep on in production). Every `_oom` test variant later in the book builds on this chapter's injection entry — one of the methodological cornerstones of XRT's engineering quality.

## Introduction

Ask a cruel question: what does your program do when the 1000th allocation fails? The honest answer for most code is "no idea" — the failure path has never been systematically exercised, because in normal runs the 1000th allocation always succeeds. When production finally runs out of memory, those never-executed rollback paths run for the first time — and they are precisely the easiest to get wrong: missed frees, out-of-order frees, continuing with half-initialized structures.

The testing world's old trick is swapping malloc for a fallible macro, but that covers only your own code. Because XRT is choked library-wide (Chapter 5), one line of `xrtMemDebugFailAfter(N)` makes **the library and your business code fail together** at the (N+1)-th allocation — a JSON parsed halfway, a TLS handshake at its midpoint: their failure rollbacks can be verified one by one for the first time. This is the common foundation of the hundreds of OOM test variants in the repository.

The counterpart of fault injection is **production-side observation**: a resident service's memory creeps up, and you need to answer "what is growing" before chasing individual cases. XRT's pair of modules maps exactly onto these two directions — this chapter's last section composes them into a complete investigation funnel.

## Concepts

### Two modules, two kinds of observation

| Module | What it records | Cost | When to enable |
| --- | --- | --- | --- |
| `memory_stats` | Counters: call counts, bytes, peaks, pool hits | Nearly zero | Always on in production, or per window |
| `memory_debug` | Per-allocation/free detail with call positions | Noticeable | When locating leaks or injecting faults |

The division-of-labor principle: **numbers for trends, details for individual cases**. Suspect a slow memory creep — enable stats first and watch the live-byte curve; a leak confirmed — enable the debug heap and use the event stream to lock onto the birthplace. The two switches are independent; you can enable either alone — the daily recommendation is stats only, saving the debug heap for the day something goes wrong.

### Fault injection: the precise semantics of FailAfter

```diagram state
not injected -> injected: FailAfter(N) (the first N succeed)
injected -> triggered: the (N+1)-th allocation returns NULL
triggered -> not injected: FailClear (resets together with the trigger flag)
injected -> not injected: FailClear (also clears when not triggered)
```

Two semantic details dictate how tests are written. First, `FailAfter(1)` means "1st succeeds, 2nd fails" — the parameter N counts the **first N successes**; failure happens on the (N+1)-th, not the N-th. Second, `FailClear` resets the injection **and** the "triggered" flag together, and `xrtMemDebugFailTriggered` queries whether it fired — the assertion "the failure really happened at the expected occurrence" rests on it.

### The event stream: replay in time order

`xrtMemDebugVisit` visits debug events (every allocation, free, injection) in time order via callback, and `xrtMemDebugEventName` converts the event enum into a stable name. Combined with the At-family position information from Chapter 5, the information chain for locating one leak is complete: **the event stream tells you "which allocation was never freed"; the position information tells you "which line it was born on"**.

Three handier tools in the same family: `xrtMemDebugVisitLive` visits only allocations **still live** (pairing checks skip the noise of already-freed ones); `xrtMemDebugSnapshot` fetches a "currently live count and bytes" snapshot in one call, ideal for probes at business milestones ("after this batch of requests, live bytes should return to baseline"); `xrtMemDebugReport` formats the detail into a file or callback, ideal for archivable leak reports. The selection mnemonic: **snapshots for comparison, Live for hunting leaks, Report for archives**.

### Reading the stats: what the numbers mean

Among the `xmemstats` counters, the most diagnostic value lies in the relationship of three numbers: business-side allocation count, pool hits, direct-to-backend count, versus **actual system call count**. Small blocks hitting the size-class cache generate no system calls — "100 business allocations, only 3 system calls" means pooling is working. Use `xrtMemStatsReset` to mark the start of a window, run a stretch of load, and fetch with one `xrtMemStatsGet`. Peak plus current value also answers capacity-planning questions: who contributes the process's resident-memory ceiling, and whether it scales with load — such questions get the systematic treatment in Volume 12's performance-analysis chapter.

## Examples

### Complete program: the FailAfter full loop

From the repository example `examples/memory/fail_inject/main.c` — the complete closed loop of inject, trigger, query, clear, and reset:

```embed path="examples/memory/fail_inject/main.c" title="examples/memory/fail_inject/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/fail_inject/main.c -lws2_32 -liphlpapi
enabled=1
fail-after-2: 1st=ok 2nd=NULL triggered=1
event[0]: alloc
cleared: 3rd=ok triggered-still=0
reset=1
```

**What just happened.** (1) `xrtMemDebugEnable` turns on the debug heap — both injection and the event stream depend on it. (2) After `FailAfter(2)`: the 1st allocation succeeds, the 2nd returns `NULL`, and `FailTriggered` confirms the firing — three assertions nail "the failure happened at the expected occurrence". (3) `xrtMemDebugVisit` replays the event stream, printing the first allocation event's stable name `alloc`. (4) After `FailClear`, the 3rd allocation is back to normal and the trigger flag is cleared with it. (5) Disabling the debug heap resets everything. This 50-line program is the skeleton of all OOM tests: replace the middle part with your business calls, and you can verify their failure rollbacks.

Upgrading the skeleton into a complete test takes two steps. First, turn "single injection" into "exhaustive scan": increment N from 0 up to the total allocation count, running the business function fully once per N — a crash, assertion failure, or non-zero live memory at any N catches a failure-path defect. Second, add a "recovery assertion": after the scan ends, disable injection and the business should behave completely normally again. The repository's `_oom` test variants (JSON, TLS, networking — every module has them) are all this shape; you will meet them in force in Volume 12's testing chapters.

### Complete program: reading the statistics

From `examples/memory/stats/main.c` — measuring the pooling effect of an allocation workload through a stats window:

```embed path="examples/memory/stats/main.c" title="examples/memory/stats/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/stats/main.c -lws2_32 -liphlpapi
before=0 after=1
malloc_calls=2 pooled=1 direct=1 backing=3
```

**What just happened.** (1) `xrtMemStatsEnable` turns on collection; `before=0 after=1` confirms the switch took. (2) The business does two `xrtMalloc` calls: the 48-byte small block hits the size-class cache (`pooled=1`, zero system calls); the 4096-byte large block goes direct to the backend (`direct=1`). (3) `backing=3` is the number of actual hits on the system allocator — one direct plus the pool's internal pre-warming allocations for its size classes; "2 business calls, 3 system calls" shows a freshly warmed pool — in steady state this ratio keeps shrinking. Read the relationship of these three numbers first, then byte volumes and peaks.

Applying these readings to a real service is **window measurement**: take a snapshot at two business milestones — say "before processing a batch of requests" and "after all are processed and reclaimed" — and compare whether live bytes return to baseline. If they do, the batch leaks nothing; if not, the delta is the suspect volume, and you switch on the debug heap and use Live visits to pick out individual cases. Stats answer "is there a problem and how big"; debug answers "exactly which allocation" — a two-layer funnel that avoids jumping straight into the high-cost detail mode.

## Contracts

- **Cost discipline**: enable the debug heap only when locating problems; performance-sensitive paths and production normals use the stats module; any performance numbers taken while the debug heap is on do not count.
- **Injection semantics**: `FailAfter(N)` = first N succeed, (N+1)-th fails; `FailClear` resets together with the trigger flag; `FailTriggered` is the assertion basis.
- **The assertion trio**: not triggered before injection, failure actually happens, trigger flag flips true — the standard skeleton of an OOM test; after scanning all N, the business returns to normal.
- **Stats window**: `Reset` marks the start, run the load, `Get` fetches the readings; do not touch the switches during the window; judge per-batch reclamation by comparing "before/after snapshots".
- **Events are read-only**: callbacks of `Visit` / `VisitLive` must not allocate (they would recurse into the debug heap) — only record and match.

## Pitfalls

### Pitfall 1: the off-by-one misreading of FailAfter

Symptoms: an OOM test "occasionally" misses the target path — injection set, yet the allocation you meant to intercept isn't, or it intercepts one early.

Cause: reading `FailAfter(N)` as "the N-th fails". The actual semantics are **first N succeed, (N+1)-th fails** — to intercept the very first allocation, use N=0.

```c bad
xrtMemDebugFailAfter(1);   /* wanted the FIRST allocation to fail */
p = xrtMalloc(64);         /* reality: this one succeeded */
p = xrtMalloc(64);         /* the failure lands here — one beat late */
```

```c good
xrtMemDebugFailAfter(0);   /* first 0 succeed */
p = xrtMalloc(64);         /* the first allocation fails: exactly what was wanted */
```

### Pitfall 2: running benchmarks with the debug heap on

Symptoms: benchmark numbers degrade heavily; re-measuring after "optimizing" stays bad; no change point found against historical data.

Cause: the debug heap records per-allocation detail with positions, at several times the cost of normal allocation — numbers taken while it is on are not the program's real performance.

```c bad
xrtMemDebugEnable(true);
run_benchmark();          /* the numbers include the debug heap's own overhead */
xrtMemDebugEnable(false);
```

```c good
xrtMemStatsEnable();      /* to observe, use stats: nearly zero cost */
xrtMemStatsReset();
run_benchmark();
xrtMemStatsGet(&Stats);   /* counters delivered, performance numbers trustworthy */
```

## Exercises

### Basic: inject and recover

Following the fail_inject example: after `FailAfter(1)`, make two consecutive allocations and assert the first succeeds, the second fails, and the trigger flag is true; after `FailClear`, a third succeeds and the flag is back to zero. Print all four assertions as `ok` to complete.

### Advanced: an OOM checkup for Chapter 5's code

Wrap the main flow of Chapter 5's MemDup example in an injection loop: increment the injection point from N=0 upward, asserting after each run that the program exits with the expected failure code **and live bytes return to zero** (verified with your counting allocator or stats). Hint: this is exactly how the repository's `_oom` variants are written; if under some injection the program does not exit as a failure (say it keeps running), that itself is a failure-path defect — write it down; it is this exercise's most valuable output.

### Challenge: a leak locator

Write a `leak_check()` helper: enable the debug heap → run a function under test (with one deliberate missed free hidden) → `Visit` the event stream to count unpaired allocations → print each leak's size and position (`__FILE__:__LINE__`). Acceptance: for the "missed one Free" test code it reports exactly one leak line; for clean code it reports zero leaks.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Division of labor | stats counts always-on for trends; debug details on demand for cases — a two-layer funnel |
| Injection | `FailAfter(N)`: first N succeed, (N+1)-th fails; `FailClear` fully resets |
| Assertions | not triggered before injection → failure happens → `FailTriggered` flips true |
| Event stream | `Visit` replays in time order; `EventName` stable names; allocating inside callbacks forbidden |
| Snapshot/Live/Report | snapshots for comparison, Live for hunting leaks, Report for archives |
| Stats window | `Enable` → `Reset` → load → `Get`; watch the pooled/direct/backing three-number relationship |
| OOM test skeleton | injection + business call + rollback assertion + live-zero verification; scan all N exhaustively |
