---
num: 55
slug: coroutine
title: Coroutines (Part 1): Creation, Switching, and the Cleanup Stack
volume: 卷六 进程与并发
type: practice
lead: The primitive layer of stackful coroutines — Resume drives to the suspension point, four states and three final states, cooperative cancellation with a cleanup stack, and an independent error slot.
api: coroutine
---

## Orientation

The first of three coroutine chapters covers the **primitive layer**: `xrtCoCreate` creates (stackful — switching back continues from the yield point), `xrtCoResume` drives to the next suspension point, `xrtCoYield` yields; four states (READY/RUNNING/SUSPENDED/DONE) plus three final states (RETURNED/CANCELLED/ERROR) depict the complete lifecycle; the **cleanup stack** (CleanupPush/Pop and Defer) guarantees reverse-order finishing on every exit path; coroutines carry their own **independent error slot** and a built-in arena (Chapter 7). The backend auto-selects Windows Fibers / hand-written POSIX context switching (four architectures supported). Most business code uses the middle volume's scheduler rather than this chapter's primitives — but the primitives are the foundation for understanding everything above.

## Introduction

Back to Chapter 53's "consumer waits" problem: the thread sleeps on a Condition — one wait occupies one thread (the 1MB-stack system cost); a thousand concurrent waits mean a thousand threads, memory and scheduling both exploding. Coroutines change the model: **when waiting, don't sleep in place — hand over execution and suspend the whole "task"** — the cost is merely tens of KB of coroutine stack; a thousand suspended coroutines take only tens of MB. On wakeup, the context restores and continues from the suspension point — logically like "sleeping", physically "yielding".

This model's engineering dividend shows in IO-dense scenes: in a network service, thousands of connections each "waiting for data" — the thread model means either thread explosion or select-polling complexity; the coroutine model gives each connection one coroutine, yielding while waiting for data, resuming when it arrives — **write synchronous-style code, get asynchronous scalability**. Volume 7's network engine uses it exactly so.

## Concepts

### Lifecycle: four states, three final states

```diagram flow
- Create: xrtCoCreate(proc, data, config) -> READY (not started)
- Drive: xrtCoResume -> runs to a Yield (SUSPENDED) or to the end (DONE)
- Yield: xrtCoYield, the cooperation point - the next Resume continues from the yield point
- Final states: RETURNED (normal return) / CANCELLED (cancelled) / ERROR (the proc failed)
- Finishing: Destroy succeeds only for unstarted or finished coroutines - an active one is Cancelled first
```

**Resume drives to the next suspension point** is the key sentence for understanding coroutines: one Resume pushes the coroutine to the next Yield or to its end — not "executed to completion" but "executed to". The gold-standard chapter (ch4's error model) demonstrated the three-beat form: after creation, three Resumes (SUSPENDED after the second, DONE+RETURNED after the third). The coroutine proc signature is `ptr(ptr)` — the return value read as a borrow via `xrtCoResult`.

### Yield and cooperative cancellation

`xrtCoYield()` returns `xwaitresult` — a normal yield returns OK; **on a cancellation request it returns CANCELLED**: the coroutine senses cancellation at the yield point, cleans up, and returns (final state CANCELLED). `xrtCoCancel` only marks (the same cooperative semantics as Chapter 54's token) — actual termination happens at the coroutine's own next yield point; `xrtCoCurrent` answers "am I inside a coroutine" (calling Yield from ordinary code is undefined — check first, then yield). The cancellation-token family (Chapter 54) enters the coroutine system through the scheduler — unfolded in the middle volume.

### The cleanup stack: a guarantee for every exit path

Resources inside a coroutine (open handles, borrowed buffers) are entrusted to the **cleanup stack**: `xrtCoCleanupPush/Pop` for manual management (on-stack nodes, zero overhead) or `xrtCoDefer` for automatic management; whether the coroutine **returns normally, is cancelled, or errors** — the cleanup stack executes in **reverse order** (the same guarantee as Go's defer). This complements Chapter 7's arena's "reclaim the whole stretch": the arena manages memory, the cleanup stack manages actions (non-memory finishing like closing handles, sending notifications).

### Independent error slot and built-in arena

A coroutine **switches in its own error slot** (Chapter 4's concurrent-edition full flowering): coroutine A's errors never pollute the hosting thread or other coroutines — migrating coroutines keeps their error context; inside the coroutine, `xrtGetError`/`xrtTakeError` operate on its own slot as usual. Built-in arena: coroutine creation attaches an independent temp arena, reclaimed automatically at the end — Chapter 7's "request-scoped temporary memory" in coroutine form (the coroutine is the "request").

### Thread affinity: a coroutine belongs to its creating thread

A coroutine **is permanently owned by the thread that created it** — Resuming it from another thread is undefined. Cross-thread cooperation goes through the scheduler's Post (middle volume) or message passing; this discipline is bound to the stackful implementation's context (TLS/stack guards) — not advice but hard law.

## Examples

### Complete program: a two-segment coroutine with yield

From the repository sample `examples/concurrency/coroutine/main.c` — 59 lines through create/yield/final states:

```embed path="examples/concurrency/coroutine/main.c" title="examples/concurrency/coroutine/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine/main.c -lws2_32 -liphlpapi
after yield: 21
result: 42
```

**What just happened.** (1) The coroutine proc has two segments: the first half `*pValue += 1` (20→21), a Yield, the second half `*pValue *= 2`. (2) First Resume: runs to the Yield and suspends (SUSPENDED) — at this point `after yield: 21` prints (21 is the first half's product — **the yield point is the observation point**, where the caller sees the intermediate state). (3) Second Resume: continues from the yield point and runs to return (DONE + RETURNED) — `result: 42` is the return value (21×2) read as a borrow via `xrtCoResult`. (4) The rhythm of three interactions (Resume/Resume/read result) is the entire feel of the coroutine primitives — the scheduler (middle volume) automates these three beats.

### Complete program: a scheduler prototype

From `examples/concurrency/coroutine_scheduler/main.c` — multiple coroutines alternating:

```embed path="examples/concurrency/coroutine_scheduler/main.c" title="examples/concurrency/coroutine_scheduler/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine_scheduler/main.c -lws2_32 -liphlpapi
task 1
task 2
task 3
```

**What just happened.** (1) Three coroutines are submitted into the scheduler — `task 1/2/3` printing in turn demonstrates **alternating execution**: not one task run to completion before the next, but each run to its suspension point and rotated (the exact rhythm depends on scheduling policy — the middle volume unfolds it). (2) This is "cooperative multitasking" in miniature: one thread, zero locks, three "concurrent" tasks — coroutine concurrency needs no multiple cores, only alternation points. (3) The scheduler wraps this chapter's manual Create/Resume rhythm into an automatic Post/Run rhythm — business code writes the "proc", the scheduler does the "driving". The coroutine_tour and coroutine_event samples extend the reading to the full interface and coroutine events.

## Contracts

- **Drive semantics**: Resume drives to the next suspension point (Yield) or a final state; not to completion.
- **Three-way final states**: RETURNED/CANCELLED/ERROR; Result/Error are borrows — read immediately after the final state, used up before Destroy.
- **Thread affinity**: permanently owned by the creating thread; cross-thread goes through scheduler Post or messages — a hard discipline.
- **Cooperative cancellation**: Cancel marks, Yield senses, cleanup then the CANCELLED final state; re-entering itself returns XERR_STATE.
- **Cleanup stack**: reverse-order execution covers every exit path; arena manages memory, cleanup stack manages actions — complementary, not interchangeable.
- **Independent error slot**: a coroutine's errors never pollute the hosting thread; the built-in arena reclaims with the coroutine.
- **Destroy timing**: succeeds only for unstarted or finished coroutines — an active one is first Cancelled to a final state.

### The coroutine-versus-thread comparison table

Coroutines and threads are the two poles of the concurrency toolbox; one comparison table sets the boundaries.**Cost**: a thread is a 1MB stack plus a kernel scheduling object; a coroutine tens of KB (configurable) plus user-mode switching (nanoseconds).**Preemption**: a thread can be preempted (the scheduler decides); a coroutine can only yield cooperatively (the code decides — so "torn apart mid-execution" doesn't exist at coroutine level).**Parallelism**: threads are truly parallel (multiple cores at once); coroutines alternate within one thread (parallelism comes from the "many threads × many coroutines each" combination — the middle volume's scheduler model).**Fit**: CPU-dense parallelism — threads (or task pools); IO-dense high concurrency — coroutines (wait much, run little); the two usually combine — one scheduler per thread, hundreds of coroutines per scheduler.**Mental model**: threads share memory and need synchronization (Chapter 53); coroutines alternate within one thread — sharing between coroutines is naturally exclusive in many scenes (the yield point is the boundary), but synchronization still applies between scheduler instances across threads.

### From examples to engineering: three hosts of coroutines

**Network connection handling** (Volume 7's home turf): one coroutine per connection — straight-line logic of "read until EOF" (read, parse, respond loop), yielding while waiting, resuming when data arrives; a thousand connections = a thousand coroutines + one or two threads — the first reason coroutines exist.**Async flow orchestration**: multi-step asynchronous flows (check cache, on miss query the database, write back, respond) written as a straight-line coroutine — each step awaits and yields, resumes and continues; compared with Future chains (Chapter 58), the coroutine version keeps locals and a stack — state need not live explicitly in closures.**Generators and lazy sequences**: chunked processing of a large file — each Yield produces one chunk, the consumer sets the pace (backpressure's natural form). The three hosts' shared criterion: **"wait a lot, want the logic straight-line" — the coroutine's feel-advantage lives in the words 'straight line'**.

### Stackful versus stackless: an engineering view

The two schools of coroutine implementation deserve an engineering comparison.**Stackful** (XRT's choice): each coroutine has its own stack — yields work at any call depth (Yield inside framework code works too), locals are naturally preserved, debuggers see the real stack; the price is per-coroutine stack memory (tens of KB up) and the context-switch stack swap.**Stackless** (C++20 coroutines / state-machine compilation): the compiler slices the function into a state machine — zero stack memory, switching nearly free; the price is that only the coroutine function body itself can yield (functions down the call chain cannot), locals live in the state machine (lifetime restricted), and debugging sees the dismantled machine. XRT chose stackful for this reason: C has no compiler magic available, and offloading hand-written state-machine complexity onto every user is unacceptable — stackful's "yield at any depth + real-stack debugging" is the pragmatic answer for C engineering. This view also explains why the coroutine stack size is configurable (the same consideration as Chapter 52's thread stacks).

## Pitfalls

### Pitfall 1: cross-thread Resume

Symptom: occasional crashes or garbled state — the coroutine is being driven on another thread; reproduction depends on thread timing.

Cause: a coroutine is permanently owned by its creating thread (the context implementation binds TLS and stack guards) — driving it across threads is undefined behavior.

```c bad
/* thread A creates */          /* thread B drives */
xcoro* Co = xrtCoCreate(proc, Data, &Config);
xrtCoResume(Co);            /* B drives A's coroutine - undefined */
```

```c good
/* cross-thread cooperation goes through messages: A's coroutine is driven on thread A */
/* thread B wants to trigger -> send a message to A's queue/channel (Chapters 21/57) */
/* or use the scheduler's Post to keep the "driving duty" on the right thread (middle volume) */
```

### Pitfall 2: the cleanup stack's reverse order broken

Symptom: resource-release order scrambled — a handle closed and then used; order-dependent finishing fails intermittently.

Cause: manual CleanupPop popping out of order, or mixing Defer with manual Pop so the execution order drifts from reverse-of-push.

```c bad
xrtCoCleanupPush(&Node1, closeHandle, H1);   /* pushed first: should execute last */
xrtCoCleanupPush(&Node2, releaseBuffer, B);  /* pushed second: should execute first */
xrtCoCleanupPop(&Node1);   /* manually popped Node1 first - reverse order broken */
```

```c good
xrtCoCleanupPush(&Node1, closeHandle, H1);
xrtCoCleanupPush(&Node2, releaseBuffer, B);
xrtCoCleanupPop(&Node2);   /* last in, first out - reverse order preserved */
xrtCoCleanupPop(&Node1);
/* or use Defer throughout: let the coroutine machinery guarantee the order (recommended - one less slip dimension) */
```

## Exercises

### Basic: a generator

Implement an integer-sequence generator with Resume/Yield: each Resume produces one value (shared variable or Result), DONE after the tenth — the coroutine-primitive edition of the gold-standard chapter's exercise.

### Advanced: producer-consumer (single-thread edition)

Two coroutines plus one buffer variable: the producer yields leaving data, the consumer yields taking it away — lock-free cooperative alternation; feel that "concurrency needs no multiple cores".

### Challenge: a cancellable pipeline coroutine

A three-stage coroutine pipeline (read/compute/write) with each stage's resources on the cleanup stack; after the cancellation token hits, all three stages finish cleanly (final state CANCELLED, cleanup stack fully executed, arena reclaimed). Acceptance criteria: cancellation at any moment leaves zero resource leaks (Chapter 6's statistics); final states and cleanup execution order auditable in structured logs.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three-beat rhythm | Create (READY) → Resume×N (to a Yield or DONE) → read Result at the final state |
| Four states, three final states | READY/RUNNING/SUSPENDED/DONE; RETURNED/CANCELLED/ERROR |
| Yield semantics | continues from the yield point; a CANCELLED return means cancellation hit - clean up, then return |
| Cleanup stack | Push/Pop manual or Defer automatic; **reverse-order execution covers every exit path** |
| Independent facilities | error slot isolated (migration never pollutes) + built-in arena (reclaimed with the coroutine) |
| Thread affinity | permanently owned by the creating thread - cross-thread via scheduler Post or messages (hard law) |
| Destroy threshold | succeeds only unstarted or finished - the active are first Cancelled to a final state |
