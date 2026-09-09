---
num: 52
slug: thread-sync
title: Threads and Synchronization Primitives
volume: 卷六 进程与并发
type: practice
lead: Thread creation and joining, the mutex/condition/semaphore/read-write-lock four-piece set, and fallible once initialization — the foundation of concurrency.
api: thread, sync, atomic
---

## Orientation

Volume 6 starts construction at the lowest layer: **threads** (`xrtThreadCreate/Wait` — the create, run, join three-step lifecycle) and the **four-piece set of synchronization primitives** (Mutex, Condition, Semaphore, RWLock), plus the **fallible once initialization** previewed in Chapter 5 (the gold-standard chapter). This chapter's stance is the "primitives layer" — they are the concurrency system's raw materials; most business code should never touch primitives directly (that is Channel/Future/task-group territory), but understanding the primitives is the precondition for understanding the wrappers above and the final landing point for diagnosing concurrency problems.

## Introduction

Three plain questions lead the way. Question one: two threads increment the same counter ten thousand times each — why isn't the result twenty thousand — `count++` is read-modify-write, three steps whose interleaving across two threads is arbitrary; a Mutex turns the stretch into "one person at a time". Question two: how does a consumer thread wait for a producer's data — spin polling burns CPU, sleep polling adds latency; Condition is the "sleep until someone wakes you" mechanism. Question three: configuration initialization is slow and may fail, and many threads need it — every thread trying the initialization is waste plus a race; once guarantees "exactly one thread executes, failure is retryable, the others wait for the outcome".

The three questions map to three answer sets and draw the primitives' division of labor: **Mutex governs mutual exclusion** (shared data, one at a time), **Condition governs waiting** (sleep until the condition holds), **Semaphore governs quota** (at most N at once), **RWLock governs the read-write asymmetry** (shared reads, exclusive writes). The price of choosing the wrong primitive is deadlock or performance collapse — the pitfalls section holds both kinds of examples.

## Concepts

### The thread lifecycle

```diagram flow
- Create: xrtThreadCreate(proc, data, stack size) -> handle (stack size 0 uses the default)
- Run: the proc signature xthreadproc - the return value is the thread's result (int)
- Join: xrtThreadWait(handle) -> retrieve the result and release - no Wait means a leak
- Auxiliaries: Current/Yield/CurrentId - introspection and yielding (Yield was used in Chapter 64's TCP sample)
```

`Wait`'s discipline is this chapter's first iron law: **every created thread is Joined exactly once** — not Waiting is a resource leak, Waiting twice is undefined. The thread's return value (the source of the `exit: 42` output) is retrieved via Wait — the most primitive result passing between threads.

### The four-piece primitive set

| Primitive | Semantics | Standard scene |
| --- | --- | --- |
| Mutex | one at a time; Lock/Unlock; TryLock non-blocking probe | any modification of a shared data structure |
| Condition | Wait sleeps until Signal/Broadcast; **pairs with a Mutex** | producer-consumer's "data has arrived" |
| Semaphore | counting quota; Wait decrements, Post replenishes | connection-pool caps, concurrency control |
| RWLock | shared reads, exclusive writes; many readers one writer | read-heavy, write-light configuration |

The family shape is uniform: two lifecycles — on-stack Init/Unit or on-heap Create/Destroy — Try non-blocking variants, and wait-class support for deadlines (the Chapter 41 xtime junction — `WaitFor/WaitUntil` with timeout).**Condition's pairing discipline**: a Condition must work with a Mutex — hold the lock before Wait, Wait atomically "releases + sleeps" inside, re-acquires on waking (the mechanism's core; forgetting the pairing is undefined behavior).

### once: fallible one-time initialization

Chapter 5's gold standard covered once's fallible semantics (`bool (*xonceproc)(ptr)` — returning false is retryable, `XERR_STATE` guards recursion). This chapter adds its place on the concurrency map: **once is "the Mutex replacement for initialization paths"** — cheaper than a Mutex (lock-free fast path) and semantically sharper ("exactly once" rather than "mutual exclusion"). Config loading, global resource construction, and lazy singletons are all its territory.

### The four deadlock conditions and prevention

Deadlock requires four conditions simultaneously: mutual exclusion, hold-and-wait, no preemption, circular wait. Engineering prevention breaks the easiest of the last two: **lock ordering** — the whole program agrees on acquisition order (say "A before B"), and circular wait structurally vanishes; **TryLock escalation** — take the second lock with Try, and on failure drop what you hold and retry (breaking hold-and-wait). The last resort at the four-piece level is **timeout** — Wait with a deadline; a timeout diagnoses (the runtime form of deadlock detection).

## Examples

### Complete program: thread creation and joining

From the repository sample `examples/concurrency/thread/main.c`:

```embed path="examples/concurrency/thread/main.c" title="examples/concurrency/thread/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/thread/main.c -lws2_32 -liphlpapi
worker alpha: 13300
exit: 42
```

**What just happened.** (1) `xrtThreadCreate` takes three things: the proc (the `xthreadproc` signature — `int32(ptr)` return), the data (`ptr` passed through — a name string here), and the stack size (0 for the platform default; give it explicitly for deep recursion). (2) The worker thread prints its name and thread ID (`13300` — varies per run); (3) the main thread joins with `xrtThreadWait` — `xwaitresult` reports the wait outcome; the thread's return value (42 here) travels by thread-local convention or a Future (Chapter 57), and `exit: 42` is the sample's own fetch from an atomic — **Wait does the joining, not the value retrieval**; (4) after Wait the handle is released — no Destroy needed (or allowed).

### Complete program: a mutex-protected critical section

From `examples/concurrency/sync/main.c`:

```embed path="examples/concurrency/sync/main.c" title="examples/concurrency/sync/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/concurrency/sync/main.c -lws2_32 -liphlpapi
protected section
```

**What just happened.** (1) Mutex Init on the stack (zero allocation — the same container-style choice as the heap Create/Destroy variant). (2) Between Lock and Unlock is the critical section — two threads' Locks on the same Mutex exclude each other, and the critical code runs as if unopposed (`protected section` prints exactly once, untorn). (3) The sync_tour sample is the four-piece set's full-interface tour: Mutex's Try (probing for the lock), Condition's Wait/WaitFor (timed waiting paired with Signal), Semaphore's quota semantics (after `post-many=3`, three Waits all pass) — four ok lines for the four pieces. The condition and rwlock samples unfold complete use cases for conditional waiting and the read-write lock respectively.

## Contracts

- **Thread balancing**: exactly one Wait per thread (returning xwaitresult); no Wait leaks, double Wait is undefined; return values travel via atomics or Futures — Wait only joins.
- **Minimal critical sections**: between Lock and Unlock, only code that must be exclusive — IO, logging, allocation stay outside where possible.
- **Condition pairing**: must work with a Mutex; hold the lock before Wait; choose Broadcast explicitly where multiple consumers apply.
- **Lock-order discipline**: multi-lock programs agree program-wide on acquisition order; or a TryLock escalation strategy — pick one and write it into the module documentation.
- **once semantics**: exactly once + retryable failure + `XERR_STATE` recursion guard (Chapter 5's contract in force verbatim).
- **Timeout diagnosis**: wait-class primitives take deadlines — a timeout is deadlock's runtime signal, not something to retry silently.

### Positioning the primitives layer: when not to use the four-piece set

Teaching primitives demands teaching "when not to use them" — three signs that business code is touching primitives directly.**Sign one: you're using a Mutex to protect "task handoff"** — two threads passing data by hand-assembling lock + condition variable is exactly Channel territory (Chapter 56): queue + wakeup + backpressure in one step.**Sign two: you're assembling an "asynchronous result" from thread + Wait** — creating a thread just to compute a value and Wait for it is Future/task-pool territory (Chapters 57/58): submit, get a Future, pooled thread reuse.**Sign three: you're using a Semaphore for "concurrency control"** — rate limiting runs on a semaphore, but a task pool's queue depth is natively the concurrency cap (with backpressure besides). The shared criterion behind the signs: **primitives solve "how to synchronize"; the upper layers solve "how to cooperate"** — writing cooperation logic while holding primitives means you are reinventing the upper layer. Legitimate direct uses of primitives: protecting pure data structures (caches, counters), performance-critical paths (where lock overhead is visible), boundary glue with external thread libraries.

### The division with Chapter 21's queues and Chapter 9's atomics

The three-layer toolbox for concurrent data access is easily confused; one comparison settles it.**Atomic operations** (Chapter 9): single-variable reads and writes — counters, flags, pointers; lock-free but single-variable only.**Mutex/the four-piece set** (this chapter): maintaining multi-variable invariants — constraints like "two fields must change together" are beyond atomics; locks take them.**Lock-free queues** (Chapter 21): SPSC/MPSC/MPMC pointer passing — lock-free inside the queue, you add no lock. Selection is decided by "the shape of the sharing": single variable → atomics; invariants → locks; passing pointers → queues. The classic layer-mixing errors are wrapping a Mutex around a queue (the queue is already thread-safe) or locking a counter (atomics are cheaper) — layer awareness is isomorphic with Chapter 23's container selection.

### A testing view: deterministic testing for concurrency

The race-condition quality of concurrency code - "appears once in ten thousand runs" - turns testing into voodoo. Three engineering remedies.**Stress to open the window**: race windows open under pressure — tests run full thread counts + injected yields (Yield to force interleaving) + long durations (CI nightly jobs).**Reduce dimension for assertions**: intermediate states at arbitrary times are unassertable; assertions land only after join points (global checks once all Waits complete) — "eventually consistent" is far more testable than "consistent at every step".**Tool reinforcement**: TSan-class sanitizers on a dedicated CI lane (slow, but catches real races); Chapter 6's fault injection fills in OOM scenarios on lock paths. The realistic expectation of the three together: **concurrency tests prove "no problem found", not "no problem"** — localization still rests on Chapter 49's observation pipeline (microsecond-timestamped logs re-sorting the race scene).

## Pitfalls

### Pitfall 1: forgetting Wait, or double Wait

Symptom: the former — thread resource leaks (visible in Chapter 6's statistics); the latter — occasional crashes or garbled return values.

Cause: a thread handle is "a promise that must be joined" — creating it incurs one Wait; Wait is the promise redeemed, and a second Wait is a double redemption.

```c bad
xthread* Worker = xrtThreadCreate(work, NULL, &Config);
/* ... the main thread exits without Wait - Worker leaks; worse: the thread is still running at process exit */
```

```c good
xthread* Worker = xrtThreadCreate(work, NULL, 0);
if ( xrtThreadWait(Worker) != XWAIT_OK ) {   /* exactly once */
	return false;
}
/* the thread's return value travels via atomic/Future/queue - Wait only joins */
```

### Pitfall 2: inverted lock order causing deadlock

Symptom: occasional mutual hangs — thread 1 holds A waiting for B, thread 2 holds B waiting for A; absent under low concurrency, occasional in stress tests, guaranteed in production.

Cause: two code sites take the same lock pair in opposite orders — the four conditions of circular wait assemble.

```c bad
/* thread 1's code */          /* thread 2's code */
xrtMutexLock(&A);            xrtMutexLock(&B);
xrtMutexLock(&B);            xrtMutexLock(&A);   /* opposite order - a deadlock window */
Transfer(A, B);              Transfer(B, A);
xrtMutexUnlock(&B);          xrtMutexUnlock(&A);
xrtMutexUnlock(&A);          xrtMutexUnlock(&B);
```

```c good
/* program-wide convention: when both A and B are needed, always A before B (written into module docs and review checklists) */
/* both thread 1 and thread 2 become: */
xrtMutexLock(&A);            /* A first - the convention */
xrtMutexLock(&B);            /* then B */
Transfer(A, B);
xrtMutexUnlock(&B);
xrtMutexUnlock(&A);
```

## Exercises

### Basic: proving the counter race

Two threads each incrementing a lock-free counter one million times versus a Mutex-protected version — compare results (the lock-free version differs every run and lands below two million); then add an atomic-operations version (Chapter 9) for a three-way comparison.

### Advanced: producer-consumer

Condition + Mutex implementing production and consumption over a bounded queue: the producer waits when full, the consumer waits when empty, each waking the other — the standard dual-condition-variable exercise (with Chapter 21's queue as the foundation).

### Challenge: a read-write-lock benchmark

Protect the same read-heavy data (90% reads/10% writes) with Mutex and with RWLock; run a hundred thousand operations across four concurrent threads — compare throughput. Acceptance criteria: the RWLock version's throughput clearly exceeds the Mutex version (the shared-read dividend); writer starvation absent (or eliminated with a writer-preference policy); the numbers written into a comparison table.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Thread three steps | Create (proc + data + config) → run → Wait (retrieve + release, exactly once) |
| Four-piece set | Mutex mutual exclusion / Condition paired waiting / Semaphore quota / RWLock shared reads exclusive writes |
| Lifecycle | Init/Unit on-stack zero allocation or Create/Destroy on-heap - consistent with container conventions |
| Timeout family | WaitFor/WaitUntil with deadlines - a timeout is a deadlock signal, not a retry signal |
| once | exactly once + retryable failure + recursion guard - the Mutex replacement for initialization paths |
| Deadlock prevention | a lock-order convention or TryLock escalation - pick one, write it into the docs |
| Critical-section discipline | minimize - IO/logging/allocation out of the section |
