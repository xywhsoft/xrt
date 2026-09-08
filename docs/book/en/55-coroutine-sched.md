---
num: 55
slug: coroutine-sched
title: Coroutines (Part 2): The Scheduler and Timers
volume: 卷六 进程与并发
type: practice
lead: The scheduler takes over the driving — Post submission, Sleep timing, the RunUntil pump loop; one instance per thread as the standard model.
api: coroutine
---

## Orientation

The middle coroutine chapter upgrades the first volume's manual three beats (Create/Resume/read-final-state) to **scheduler custody**: `xrtCoSchedCreate` builds the scheduler, `xrtCoGo` (or `xrtCoSchedPost` to submit a function) starts a coroutine, `xrtCoSleep` is the in-coroutine timed yield, `xrtCoSchedRun` drives the pump (until all coroutines finish) with `PollFor/PollUntil` stepping on event waits; the **timer wheel** is one with the scheduler (a Sleep's yield is woken by the timing wheel); **one scheduler instance per thread** is the standard model — the coroutine's thread affinity is satisfied naturally by the scheduler. After this chapter, business code almost never touches the first volume's primitives.

## Introduction

The first volume's scheduler prototype (three tasks alternating) left a question: who calls Resume? If the answer is "the main loop rotating by hand", then "which coroutine runs first, who wakes the one sleeping seconds, how new coroutines jump the queue" all must be hand-written — that "driving policy" code is the scheduler. Its three core decisions: the **ready queue** (Post enqueues, Run dequeues and drives — FIFO or by policy); the **timer wheel** (a sleeping coroutine hangs on the timing structure, expiry returns it to the ready queue — the answer to "who wakes it"); the **pump loop** (RunUntil drives with a deadline — the answer to "when to stop": return when the queue is empty and no timers pend, or the deadline hits).

The standard shape assembled from these mechanisms is **one scheduler per thread + a pump loop**: the thread's main loop is just `while ( running ) SchedRunUntil(sched, deadline)` — submitted coroutines get driven, Sleeps wake on time, external events (IO completion callbacks) Post new coroutines. Volume 7's network engine's event loop is this shape industrialized: IO event → wake the corresponding connection's coroutine → the pump loop continues.

## Concepts

### The scheduler's core three

```diagram flow
- Instance: xrtCoSchedCreate() -> scheduler (CreateLimit can cap submissions - the backpressure valve)
- Start: xrtCoGo(scheduler, proc, data, params) - create and enqueue in one step; Post submits ordinary functions
- Time/suspend: xrtCoSleep(microseconds) timed yield; the xrtCoPark family suspends until Wake/cancel/deadline
- Pump: xrtCoSchedRun runs until all finish; Step/PollFor/PollUntil single-step and time-bounded event waits
```

**`xrtCoGo` is the standard entrance**: creation and enqueueing in one step (most scenes need no Create-then-submit); `xrtCoSchedPost` submits ordinary functions (signature `xcoschedpostproc` — a light event, executed on submission); Post carries the backpressure cap (submissions fail when the CreateLimit cap is full — Chapter 21's backpressure valve, scheduler edition).**Sleep's semantic points**: called inside a coroutine (undefined on an ordinary thread); yields to the timing wheel, woken by the pump at expiry — the thread keeps driving others.**The Park/Wake pair**: Park suspends the current coroutine until Woken (waking from external threads is safe), cancelled, or the deadline — the standard channel for external events (IO completion) to wake coroutines.

### The pump loop: Run and RunUntil

`xrtCoSchedRun` drives until **all coroutines finish** (the batch-task waiting shape); `xrtCoSchedStep` single-steps (drives at most one ready coroutine — the shape for embedding an existing event loop); `PollFor/PollUntil` **wait for events and step within a deadline** (waking as soon as an external Post arrives — the resident pump loop's standard beat: `while(running) PollFor(sched, 10ms)`). `xrtCoSchedAlive` reports the live-coroutine count (a monitoring metric); `xrtCoSchedClose` requests cancel-all and stops accepting (the scheduler's shutdown entrance — Chapter 53's cancellation tree's upper button). The pump loop's two host shapes: **resident thread** — the `while(running) PollFor(sched, 10ms)` loop, external events waking via Post (the network-engine shape); **embedded** — an outer event loop manually calling Step/PollFor (a GUI framework's idle time slices — embedding the scheduler into an existing loop).

### The timer family

Timing capability arrives in three families: `xrtCoSleep` (in-coroutine sleep), `xrtCoParkFor/ParkUntil` (suspend until a deadline or Wake — external event and timer, whichever first), and delayed execution of submissions (a Posted function executes on the next pump beat — a natural "next frame" semantics). All run on the same pump — **timed wakeups and coroutine resumption serialize on the same thread** (the single-threaded scheduler's zero-lock dividend).

### One per thread: the model and its reasons

The standard model is one scheduler instance per thread. Three layers of reasons: the coroutine's thread affinity (the first volume's hard law) is satisfied automatically by "coroutines run only in their owning thread's scheduler"; a scheduler within one thread is **zero-lock** (queues and the timing wheel are touched only by the owning thread — Chapter 52's "when not to use locks" largest exemption zone); multi-thread scaling = multiple independent schedulers + cross-thread Post (submission is thread-safe — external events Post from any thread into the right thread's scheduler). This and Chapter 58's executor/task-pool "work-stealing multithreading" are two models — each with its home turf (the comparison in the next section).

### Scheduler vs task pool: choosing between two models

| Dimension | Scheduler (coroutines) | Task pool (Chapter 58) |
| --- | --- | --- |
| Unit | coroutine (stackful, can yield) | task (a function, runs to completion) |
| Waiting | Sleep/yield - suspension nearly free | occupies a thread or pairs a Future callback |
| Thread model | one instance per thread (single-threaded, zero-lock) | multi-threaded shared pool (work stealing/queue) |
| Home turf | IO-dense, many suspended waits | CPU-dense, few long tasks |

The selection mnemonic: **wait a lot — scheduler (suspension is cheap); compute a lot — task pool (throughput first)**; mixed loads use both (a CPU task running in the pool Posts back to the scheduler-side coroutine on completion — Chapter 57's continuation across models).

## Examples

### Complete program: three tasks alternating and the pump

From the repository sample `examples/concurrency/coroutine_scheduler/main.c`:

```embed path="examples/concurrency/coroutine_scheduler/main.c" title="examples/concurrency/coroutine_scheduler/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine_scheduler/main.c -lws2_32 -liphlpapi
task 1
task 2
task 3
```

**What just happened.** (1) Three coroutines `xrtCoGo` into the scheduler — created and enqueued. (2) The pump (Run or PollFor stepping) drives: each driven to its suspension point, rotated, driven to completion — the output order of `task 1/2/3` is decided jointly by the queue and the yield points (the full scheduling-policy tour is in the tour sample). (3) After all finish, `xrtCoSchedRun` returns; `Alive` reaching zero is assertable; the three coroutines' final states are each checkable. (4) Against the first volume's "manual three beats": this sample has no explicit Create/Resume — **separating starting from driving** is the scheduler's whole abstraction.

### Complete program: timing and the pump loop

From `examples/concurrency/coroutine_event/main.c` — coroutine events and timed wakeups:

```embed path="examples/concurrency/coroutine_event/main.c" title="examples/concurrency/coroutine_event/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/coroutine_event/main.c -lws2_32 -liphlpapi
（事件按定时序到达并唤醒等待协程——输出体现时序）
```

**What just happened.** (1) A coroutine suspends with `xrtCoPark` (waiting for an external wake); on timer expiry or a Wake call the coroutine returns to the ready queue — the **timed/event wakeup path** walked end to end (Park's return value distinguishes natural wake/cancel/deadline). (2) During the wait the pump loop drives other coroutines (or returns empty) — "concurrent waiting" on a single thread is the combination of yield + wakeup. (3) Timed wakeups and coroutine resumption serialize on the same thread — data passed between coroutines needs no lock (empirical proof of the single-thread scheduling dividend; cross-thread, only Post and Wake are the two safe entrances). The coroutine_tour sample is left as full-interface reading: scheduler configuration, policy variants, statistics — one assertion group each.

## Contracts

- **Start/drive separation**: Go/Post only enqueue, only the pump drives — submission and Wake are thread-safe (legal cross-thread); driving belongs solely to the scheduler's thread.
- **Sleep/Park semantics**: called inside coroutines (undefined on ordinary threads); Sleep on the timing wheel, Park waiting for Wake/cancel/deadline — neither sleeps the thread.
- **Pump shapes**: Run to all-finished; PollFor/Until time-bounded event stepping (the resident loop beat); Step single-step embedding.
- **Zero-lock dividend**: within a single-threaded scheduler, queues/timing wheel/callbacks are lock-free — only cross-instance cooperation needs synchronization (via Post or Chapter 21's queues).
- **Model selection**: wait a lot (IO-dense) — scheduler; compute a lot (CPU-dense) — task pool; mixed loads run the two models in concert.

### From examples to engineering: three hosts of the scheduler

**Network engine** (Volume 7's home turf): a resident thread's `PollFor(10ms)` pump + IO completion callbacks `Wake`-ing connection coroutines — one coroutine per connection, straight-line read-write handling; the engine's event layer (from Chapter 62) shares the thread with the scheduler pump — event driving and coroutine resuming on one thread, zero locks.**Async business orchestration**: multi-step flows (check cache → query DB → write back) with each step Parking for an external event (DB callback Wakes) — straight-line code, asynchronous execution; against Future chains: coroutines have a stack and locals, state stays out of closures.**The Actor model**: one scheduler coroutine per Actor, messages submitted via Post (submission is "sending a letter") — single-coroutine handling guarantees lock-free Actors. The three hosts share one shape: **the pump is the heart, Post/Wake the arteries, coroutines the cells** — understand this circulation and you understand every use of the scheduler.

### The shutdown path: the scheduler's drain

The scheduler's shutdown is where Chapter 48's signals and Chapter 53's cancellation meet in the coroutine world. The full path: external trigger (signal or management command) → `xrtCoSchedClose` (requests cancel-all coroutines and stops accepting — Chapter 53's cancellation tree's big button) → the pump keeps running until `Alive` reaches zero (each coroutine senses cancellation at its Park/Sleep/yield point and drains via its cleanup stack) → `xrtCoSchedDestroy` tears down. Three disciplines: **after Close, the pump keeps running** (cancellation is cooperative — no pump, no one drains); **Alive at zero before Destroy** (same root as the first volume's "Destroy succeeds only when finished"); **time-bounded waiting** (the pump waits for the drain under a deadline — on timeout, abandon grace, record the undrained count, force-quit — Chapter 48's five-second convention, scheduler edition).

### An observation reminder: the scheduler's health metrics

The scheduler itself needs observation (Chapter 49's pipeline, sentry post in the concurrent world): **Alive count** (live coroutines — the yardstick of steady-state ceilings and leak drift), **Post queue depth** (backpressure level — CreateLimit overflow counts are overload signals), **pump cycle time** (one PollFor beat's actual duration — lengthening cycles mean some coroutine does heavy work before yielding — Pitfall 1's runtime fingerprint), **Sleep/Park wakeup latency** (timing-precision drift — evidence the pump rhythm is being dragged). Four metrics into structured logs (Chapter 37's fields), and the scheduler's behavior turns from a black box into curves — for concurrency's "feels wrong", read the curves before guessing at code.

## Pitfalls

### Pitfall 1: long blocking inside the pump loop

Symptom: every other coroutine on the scheduler collectively stalls — some coroutine made a blocking call inside the pump (synchronous IO, a thread sleep, a lock wait); all coroutines of the same scheduler wait.

Cause: the scheduler is cooperative — one coroutine not yielding keeps everyone else from running; a blocking call is not a yield.

```c bad
static ptr worker(ptr pData)
{
	BlockingRead(Fd, Buf, Size);   /* blocking IO - not a Yield! */
	return NULL;                    /* the pump is stuck: every other coroutine waits */
}
```

```c good
static ptr worker(ptr pData)
{
	/* move blocking operations out of the scheduler: submit to the task pool (Chapter 58), Post back on completion */
	xrtTaskSubmit(gPool, blockingTask, pData, NULL);   /* CPU/IO task into the pool */
	xrtCoPark();   /* suspend - the pool's completion callback Wakes */
	/* the pool's callback Posts a new coroutine back to this scheduler (Chapter 57's continuation shape) */
	return NULL;
}
```

### Pitfall 2: touching scheduler internals across threads

Symptom: occasional crashes or queue corruption — an external thread manipulates scheduler state directly (clearing the queue, walking coroutines).

Cause: cross-thread allows only Post (internally synchronized); every other operation assumes a single thread.

```c bad
/* thread B */
/* directly manipulating scheduler internals - racing the pump loop, undefined */
```

```c good
/* thread B */
xrtCoSchedPost(gSched, shutdownProc, NULL);   /* Post a "shutdown function" */
/* the shutdown coroutine performs cleanup inside the scheduler's thread - the right thread doing the right thing */
```

## Exercises

### Basic: rewriting three beats into a pump

Rewrite the first volume's generator exercise from the manual three beats (Create/Resume×N) into the scheduler edition (CoGo + Run) — same output, driving code gone.

### Advanced: a timing-wheel experiment

Three coroutines each `xrtCoSleep` 100/200/300ms then print — verify wake order and timing; then mix in a compute coroutine that doesn't Sleep and watch it interleave.

### Challenge: a mini event loop

Implement a single-threaded event loop: a scheduler pump (PollFor 10ms) + Chapter 21's MPSC queue for external events + dispatch to coroutines. Acceptance criteria: events submitted by external threads are processed within the next pump cycle; Sleep timing and event wakeups coexist; idle spinning stays under 50% (the PollFor deadline tuned with data to back it).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Core three | CoSchedCreate builds the instance (Limit sets backpressure) / CoGo-Post start and submit (thread-safe) / Run-Poll-Step the pump family |
| Sleep/Park | Sleep yields on time, Park waits for Wake (external threads may wake safely)/cancel/deadline - neither sleeps the thread |
| Pump family | Run to all-finished (waiting) / PollFor-Until time-bounded event stepping (resident) / Step single-step (embedded) |
| Zero-lock zone | inside a single-threaded scheduler, lock-free - cross-thread goes only through Post |
| Model comparison | coroutine scheduler (IO-dense, much suspension) vs task pool (Chapter 58; CPU-dense throughput) |
| Blocking discipline | no blocking calls inside the pump - move to the task pool, Post back on completion |
