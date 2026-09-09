---
num: 9
slug: atomic
title: Atomics, Spinlocks, and Waiting
volume: 卷一 起步与核心
type: concept
lead: Lock-free reads and writes for single variables, the shortest critical sections with spinlocks, and the mathematics of absolute deadlines — the common foundation of the whole book's concurrency; Volume 1 closes here.
api: atomic, spin, wait
---

## Orientation

Volume 1 closes with this chapter. The previous eight chapters built the foundation of the single-threaded world: types and views (Chapter 3), the error model (Chapter 4), memory management (Chapters 5–6), temporary arenas (Chapter 7), versions and trimming (Chapter 8). This chapter adds the last piece: **what keeps things sane when multiple execution flows touch the same data at the lowest level**. It climbs three steps: **atomic operations** (`xatomic32/64/ptr` — interleaving-free load, store, and read-modify-write on a single variable); **spinlocks** (`xspinlock` — protecting critical sections a few instructions long); **waiting primitives** (`xdeadline` and `xwaitresult` — the mathematics of timeouts and a unified vocabulary for wait outcomes).

This is a classic foundation chapter. Chapter 8 mentioned that the atomic-operations header is one of the first consumers of compile-time feature dispatch — the same header walks an internal implementation on platforms without atomic instructions and an inline path where they exist, thanks to the feature closure right here. Looking forward, Chapter 52's thread synchronization (synchronous coordination), Chapter 53's cancellation system, and the config center's "atomically swap the global pointer" (Chapters 138–139) all stand on this chapter's primitives; this chapter teaches only the **primitive layer** — single-machine, single-variable, tangible small pieces, leaving systematic concurrency design to Volume 6. After reading it you should be able to answer three questions: when you **don't need a lock** (a single variable suffices); when a lock **shouldn't be heavy either** (the orders-of-magnitude account of spin versus mutex); and why "wait for something until timeout" must use an **absolute deadline** rather than a relative timeout reset every round.

## Introduction

Three real incidents lead the way. **First: the counter loses updates.** A statistics service accumulates request counts with `count++`; eight threads each add one million times, the total settles around 7.2 million and differs on every run. `count++` is a read–modify–write of three steps; two threads' three steps can interleave — both read 41, each adds one, both write back 42 — one update vanishes without a sound. A mutex fixes it, but paying the sleep-wakeup price for an integer increment is a sledgehammer on a nut. **Second: the flag lies.** A producer thread fills a structure, then sets a flag variable to announce "data ready"; the consumer occasionally sees the flag true yet reads half-new data. Writes without ordering constraints carry no cross-variable promises — visibility of the flag does not imply visibility of the data. This is not probabilistic mysticism; it is the memory model. **Third: the retry loop's total time runs away.** Code that says "wait 100 ms per retry, give up after ten seconds" actually ran twenty-eight seconds — every round waited a fresh 100 ms, the waiting budget was reset a dozen times. What the system needs is not "how long each round waits" but "**until when**".

The three problems map to three tools: atomic read-modify-write makes `count++` one step; memory ordering makes publish-acquire a dependable pairing; deadline mathematics turns the timeout from a relative value into an absolute point in time. Together they are fewer than thirty functions, yet they are the common foundation of everything in this book from the queues (Chapter 21) to cancellation tokens (Chapter 53).

## Concepts

### Three widths and the read-modify-write family

The atomic module offers three widths: `xatomic32`, `xatomic64`, `xatomicptr` — 32/64-bit integers and one pointer width. Every width shares the same family shape: runtime `Init` (static objects use macros like `XRT_ATOMIC32_INIT(0u)` so the starting state is identical on all platforms), `Load`, `Store`, `Exchange`, `FetchAdd/FetchSub/FetchAnd/FetchOr/FetchXor`, and `CompareExchange`. The "Fetch" prefix uniformly means **return the old value**: `xrtAtomic64FetchAdd(&Counter, 1u, ...)` returns the value before the add while the counter is already updated — want the new value? Add one to the old; do not go back for another Load (one extra atomic op and one extra timing assumption).

A useful rule of thumb: **the shape of the sharing decides the tool**. Just a counter, a flag, a "who does this currently point to" pointer — a single variable; atomics suffice, lock-free and cheap. "Two fields must change together" — an invariant atomics cannot express (two atomic ops can still be interrupted between them); that is lock territory (Chapter 52). Passing pointer ownership between threads is queue territory (Chapter 21). The classic symptoms of using the wrong layer: wrapping a mutex around a queue (the queue is already thread-safe), taking a lock for a counter (atomics are an order of magnitude cheaper).

### Compare-and-exchange: the expected-value rewrite contract

`xrtAtomic64CompareExchange(&Counter, &iExpected, 10u, 成功序, 失败序)` (success order, failure order) deserves its own section more than anything else in the family: when the current value equals `*iExpected`, swap in `10u` and return true; **when it does not, return false and rewrite the actually observed current value into `*iExpected`**. That rewrite is a deliberate contract — the standard CAS loop is "try with an expected value; on failure, correct with the rewritten actual value and retry", no extra Load needed and no extra window between reading the expectation and trying again. The failure order must therefore carry acquire semantics (the rewritten value must be readable), must not be stronger than the success order, and must not contain RELEASE — parameter validation enforces these; an illegal combination sets `XERR_ARGUMENT` and leaves the object untouched.

The classic use of CAS is "modify one field of a struct without locking the whole struct": read the old value, compute the new one locally, CAS it back; failure means someone got there first — redo with the rewritten value. Every step of the loop is atomic; the whole read-compute-write is not — exactly its applicability boundary: **with sparse conflicts it almost always succeeds on the first try; with dense conflicts the loop itself becomes the hotspot**. The lock-free queue of Chapter 21 is this pattern, highly engineered.

### Memory ordering: five levels and one default rule

Every atomic operation takes an `xmemoryorder` parameter: `XMEMORY_RELAXED` (only this operation is atomic; no cross-variable ordering — enough for pure counting); `XMEMORY_ACQUIRE` (the reading side — later accesses may not reorder before it; used to "acquire" published data); `XMEMORY_RELEASE` (the writing side — earlier accesses may not reorder after it; used to "publish" completed writes); `XMEMORY_ACQ_REL` (both, for read-modify-write — the CAS default posture); `XMEMORY_SEQ_CST` (total sequential consistency — the most intuitive and the most expensive). The validity matrix is simple: Load accepts only RELAXED/ACQUIRE/SEQ_CST, Store only RELAXED/RELEASE/SEQ_CST; violations set `XERR_ARGUMENT`.

The engineering rule is a single line: **until proven otherwise, use SEQ_CST**. The trap of memory ordering is that being wrong still mostly runs — reorderings and visibility only surface on particular CPUs and compiler optimization levels; a green test does not mean correct. The right workflow: write it correctly with SEQ_CST first; after profiling (Chapter 135's method) identifies a hot counter or publish point, degrade — a deliberate degradation — **with evidence** to RELAXED or an ACQUIRE/RELEASE pairing, and write in a comment why it is safe. The platform promise is one-directional: platforms that only offer full fences implement with stronger ordering — the public contract is never weakened; you may rely on the documented floor.

Publish-acquire is the most-used pairing: producer-side `Store(标志, 1, RELEASE)` (flag) (data fully ready before the announcement), consumer-side `Load(标志, ACQUIRE)` (flag) (data read after seeing the flag is necessarily the announced version). If either side uses RELAXED, the promise evaporates — the star of pitfall 1 below.

### Fences, Pause, and the lock-free query

Three helpers complete the family: `xrtAtomicThreadFence(序)` (order) is a standalone fence (attached to no variable — the fallback for mixed shapes like "several plain writes plus one atomic flag"); `xrtAtomicSignalFence` constrains only compiler reordering, not the CPU (the signal-handler-versus-thread boundary); `xrtAtomicPause()` is the inter-spin hint instruction (PAUSE on x86, YIELD on ARM) — telling the pipeline "this loop is waiting on someone else; don't pour your speculative-execution resources into it". `xrtAtomicIsLockFree(sizeof(uint64))` answers "is this width genuinely lock-free on your platform" — asking it once at selection time settles most orders-of-magnitude questions.

### Spinlock: the few-instruction critical section

`xspinlock` is built on atomic CAS and is the **lightest lock in the library**: if it cannot be acquired, it busy-waits (spins) instead of sleeping. Its applicability is an orders-of-magnitude account: for a critical section a few instructions long, spinning costs one failed CAS plus a few Pause instructions; a mutex potentially costs a syscall and a thread switch — usually an order of magnitude more. Conversely, once the critical section contains IO, allocation, or more than a few dozen instructions, spinning goes from cheap to CPU-burning — the spinning thread not only does no work, it steals cores from the lock holder. So the criterion is **critical-section length**, not "spinlock sounds fancy".

The three lifecycle shapes are isomorphic to Chapter 52's four-piece set: stack `xrtSpinInit/xrtSpinUnit`, static `XRT_SPIN_INIT`, heap `xrtSpinCreate/xrtSpinDestroy`; `xrtSpinTryLock` is the non-blocking variant (returns false while held). One hard contract: **destroying a still-held lock fails and sets `XERR_STATE`** — not cleanup advice, an enforced leak check.

```diagram flow
- Single-variable sharing (counter / flag / pointer) -> atomics: lock-free, single-step interleave-safe
- Multi-variable invariants (fields must change together) -> locks: spin (a few instructions) or mutex (longer)
- Passing pointer ownership between threads -> queues (Chapter 21): already lock-free inside, no outer lock
- Waiting with a budget -> xdeadline absolute cutoff + xwaitresult unified outcome (systematized in Chapters 52-53)
```

### Waiting primitives: deadline mathematics and the five-state result

The wait module has three functions and one enum, yet it is the unified exit for the whole library's timeout semantics. `xdeadline` is an absolute point in time as `uint64` (monotonic-clock microseconds, Chapter 41): `xrtDeadlineAfter(相对微秒)` (relative microseconds) builds the cutoff from now (overflow returns `XRT_DEADLINE_NEVER`, i.e. never times out); `xrtDeadlineExpired` says whether it has arrived; `xrtDeadlineRemaining` returns the microseconds left. It solves the third incident: **constructed once, passed everywhere, never reset** — a retry loop waits with `Remaining` each round, and the total budget is exactly that first number.

`xwaitresult` splits "the outcome of waiting" into five mutually exclusive values: `XWAIT_ERROR` (genuine failure), `XWAIT_OK` (success), `XWAIT_TIMEOUT` (time is up), `XWAIT_CANCELLED` (cancelled — Chapter 53's tokens end here), `XWAIT_CLOSED` (the awaited object closed). The intent: **separate normal control flow from errors** — timeout and cancellation are expected branches the caller handles, not "exceptions" stuffed into the error chain. From Chapter 52's `xrtThreadWait` to the network stack's connection waits, this enum is what comes back — learn the semantics here; the systematized waiting and cancellation unfold in Volume 6.

## Examples

### Complete program 1: atomic counting and unambiguous CAS

```embed path="examples/core/atomic/main.c" title="examples/core/atomic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/atomic/main.c -lws2_32 -liphlpapi
counter=10
```

**What just happened.** (1) `XRT_ATOMIC64_INIT(0u)` macro initialization — static and stack objects alike use it; the starting state is identical across platforms. (2) `FetchAdd(1, RELAXED)` shows the weakest order sufficing for pure counting — publishing no other data means needing no ordering promise. (3) The CAS expectation is 1 (matching the actual value after the FetchAdd); success swaps in 10 — **on success `*iExpected` is untouched**. (4) The ACQ_REL success / ACQUIRE failure pairing is the standard CAS posture: the failure order must allow reading the rewritten value and must not be stronger than the success order. Change `iExpected` to 2 and rerun: the CAS fails, the rewrite puts 1 back — that value is the starting point of the retry.

### Complete program 2: the RMW family tour

```embed path="examples/core/atomic_tour/main.c" title="examples/core/atomic_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/atomic_tour/main.c -lws2_32 -liphlpapi
atomic: 32-bit exchange=0F and/or/xor = E/C/5
atomic: cas ok=1 fail-rewrites=A
atomic: 64-bit init/exchange/sub/and/or/xor = 7
atomic: ptr exchange/cas = ok
atomic: lockfree(4/8)=1 fence+pause ok
```

**What just happened.** (1) All three widths walk through Exchange and the bitwise family — "Fetch returns the old value" is direct evidence in every assertion (`FetchAnd` returns 0x3C while the new value 0x0C is its AND with the mask). (2) The second output line is dedicated to the CAS failure rewrite: expectation 0x99 mismatches, the actual value 0x0A is written back. (3) The pointer width exchanges and CAS-es static buffer addresses — the "who does this point to" publish scenario (lock-free stack tops, instance swaps) is its real job. (4) The closing `IsLockFree + ThreadFence + SignalFence + Pause` lights up all the helpers. This program doubles as a migration self-check: run it first on a new platform; five correct output lines mean the atomic layer behaves identically.

### Complete program 3: two spinlock lifecycles

```embed path="examples/concurrency/spin/main.c" title="examples/concurrency/spin/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/spin/main.c -lws2_32 -liphlpapi
counter=1
heap try=0/1 destroy ok
```

**What just happened.** (1) The stack form walks `Init → Lock → 临界区（一条自增）→ Unlock → Unit` (critical section: one increment) — the critical section is exactly one instruction long, the legal range for spinning. (2) The heap form completes `Create/TryLock/Destroy`: `TryLock` must fail while held, must succeed once free — both states verified. (3) The lock protects a demonstrative single variable — in real code the answer for a bare counter is atomics, not a lock; the example locks the demonstration of the lifecycle itself. Chapter 53 will time spinlocks inside the three-way "spin vs mutex vs lock-free queue" comparison.

### Complete program 4: deadline remaining and expiry

```embed path="examples/concurrency/deadline/main.c" title="examples/concurrency/deadline/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/deadline/main.c -lws2_32 -liphlpapi
remaining: 50000 us
expired: yes
```

**What just happened.** (1) `xrtDeadlineAfter(50000)` builds an absolute cutoff 50 ms out; `Remaining` immediately reads back about 50000 microseconds (monotonic-clock microsecond scale). (2) `xrtSleepUntil` (Chapter 41's time module) sleeps to the cutoff, after which `Expired` returns true — the three functions act out the closed loop of construct-wait-decide. (3) Note that `SleepUntil` eats the same deadline mathematics: timeout parameters across the library's waiting APIs convert through here — constructed once, passed everywhere.

## Contract

- **Atomicity boundary**: an atomic operation guarantees a single variable, single step, no interleaving; two atomic operations together carry **no** whole-transaction atomicity — cross-variable invariants belong to locks (Chapter 52), ownership transfer to queues (Chapter 21).
- **Fetch semantics**: always return the pre-operation old value; derive the new value yourself — no reflexive "go back and Load".
- **Compare-and-exchange (CAS) rewrite**: success leaves `*pExpected` untouched; failure writes the actually observed value into `*pExpected` — the retry loop corrects from it, no re-read needed.
- **Memory-order validity**: Load ∈ {RELAXED, ACQUIRE, SEQ_CST}; Store ∈ {RELAXED, RELEASE, SEQ_CST}; a CAS failure order contains no RELEASE and is not stronger than the success order. Illegal combinations set `XERR_ARGUMENT`; the object is unchanged.
- **Ordering floor promise**: implementations may only be stronger than the documented ordering, never weaker — write against the documentation, not against a specific platform.
- **Default rule**: SEQ_CST until proven relaxable; downgrades require profiling evidence (Chapter 135) and a written justification.
- **Spinlock lifecycle**: stack Init/Unit, static `XRT_SPIN_INIT`, heap Create/Destroy; **destroying a still-held lock fails with `XERR_STATE`**; TryLock is non-blocking and returns false while held.
- **deadline is a pure value**: no ownership, passed by value; `After` overflow returns `XRT_DEADLINE_NEVER`; `NEVER` never expires and `Remaining` returns `UINT64_MAX`.
- **xwaitresult, five exclusive states**: ERROR/OK/TIMEOUT/CANCELLED/CLOSED; timeout and cancellation are control flow, not errors — they do not enter the error chain.
- **Thread rules**: atomic objects have no partially-updated state (any observation sees either old or new); fences and Pause are portable across all platforms.

## Pitfalls

### Pitfall 1: publishing data without memory ordering (the flag that lied)

Symptom: after seeing the "data ready" flag, the consumer still reads stale data; the problem appears probabilistically and changes frequency with CPU or compiler flags.

Cause: the data write and the flag write both used RELAXED (or are plain writes) — there is no ordering promise between them; the flag can become visible before the data. A publish-acquire pairing missing one side is a pairing that does not exist.

```c bad
Data = Compute();                                   /* plain write */
xrtAtomic32Store(&Ready, 1u, XMEMORY_RELAXED);      /* weak publish: data may not be visible yet */
```

```c good
Data = Compute();                                   /* write data */
xrtAtomic32Store(&Ready, 1u, XMEMORY_RELEASE);      /* publish: all earlier writes are ready */
/* consumer pairing: after xrtAtomic32Load(&Ready, XMEMORY_ACQUIRE), Data is the published version */
```

### Pitfall 2: long work inside a spinlock

Symptom: under workload the CPUs saturate and throughput falls; profiling shows the hotspot inside `xrtSpinLock`'s busy-wait loop.

Cause: the critical section contains IO, allocation, or long computation — the spinning thread does no work yet steals cores, the lock holder is slowed, waits lengthen, a vicious cycle. Spinning presupposes "the critical section is a few instructions".

```c bad
xrtSpinLock(&Lock);
SaveToDisk(&Record);        /* IO in the critical section: milliseconds — spinners burn a whole core */
xrtSpinUnlock(&Lock);
```

```c good
xrtSpinLock(&Lock);
iCount++;                   /* a few instructions: the legal range for spinning */
xrtSpinUnlock(&Lock);
/* long work goes to a mutex (Chapter 52) — sleeping waits don't burn CPU; or compute first, then enter a short critical section to commit */
```

### Pitfall 3: a relative timeout inside the retry loop

Symptom: a loop labeled "retry up to ten seconds" actually runs twenty-odd; every round resets the waiting budget and the total diverges with the round count.

Cause: "how long each round waits" was mistaken for "how long in total". A relative timeout constrains one round; the loop multiplies it by N; what the system can truly promise is the **absolute cutoff**.

```c bad
for ( i = 0; i < 100; i++ ) {
	if ( TryOnce() ) { break; }
	Sleep(100000);        /* budget reset every round: 100 rounds can exceed 10 seconds */
}
```

```c good
xdeadline D = xrtDeadlineAfter(UINT64_C(10000000));   /* construct once, outside the loop */
while ( !TryOnce() ) {
	if ( xrtDeadlineExpired(D) ) { return XWAIT_TIMEOUT; }
	WaitOnce(xrtDeadlineRemaining(D));                 /* remaining shrinks, never resets */
}
return XWAIT_OK;
```

## Exercises

### Basic: four-thread lock-free counting

With `xrtAtomic64FetchAdd` (RELAXED), let four threads each add one million times; join and Load-verify the total is exactly four million. Acceptance: the total is constant (identical every run); replace atomics with plain `++` and observe the lost updates.

### Advanced: mutating a field through a CAS loop

A struct holds `Done` and `Value`; use `xrtAtomic32CompareExchange` to implement "set Done to 1 and write Value only if Done is 0" — on failure, correct and retry using the expected-value rewrite. Acceptance: under multi-thread contention exactly one thread succeeds; no locks anywhere.

### Challenge: spin vs mutex, a three-way timing

The same counting task in three implementations: atomic, spinlock, and mutex (Chapter 52's API); time each and compare. Then lengthen the critical section (add some computation) and re-measure; observe where the spinlock's advantage reverses. Acceptance: a quantified crossover point of "spin wins within a few instructions, mutex wins beyond"; the conclusion cross-checks with Chapter 52's division-of-labor section.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Position | Volume 1 finale: atomic primitives + spinlock + waiting mathematics — the common foundation of the book's concurrency |
| Three widths | `xatomic32/64/ptr`; Init macros (static) and functions (runtime) |
| Fetch semantics | returns the old value; derive the new one — don't go back and Load |
| CAS rewrite | success leaves the expectation; failure rewrites the actual value — the retry's starting point |
| Memory order | RELAXED/ACQUIRE/RELEASE/ACQ_REL/SEQ_CST; default to SEQ_CST, downgrade with evidence |
| Publish-acquire | RELEASE on the writer, ACQUIRE on the reader; either side RELAXED voids the promise |
| Helpers | ThreadFence/SignalFence/Pause/IsLockFree |
| Spinlock | for few-instruction critical sections only; destroying a held lock → `XERR_STATE`; TryLock non-blocking |
| deadline | absolute cutoff (monotonic microseconds); After once outside the loop, Remaining inside |
| xwaitresult | five states ERROR/OK/TIMEOUT/CANCELLED/CLOSED; timeout and cancellation are control flow |
| Division | single variable → atomics; invariants → locks; passing pointers → queues (Chapter 21) |
