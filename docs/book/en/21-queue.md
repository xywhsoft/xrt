---
num: 21
slug: queue
title: Lock-Free Queues: SPSC / MPSC / MPMC
volume: 卷三 容器与数据结构
type: practice
lead: Three topologies, three queues — batch interfaces, the close-and-drain shutdown protocol, and the "topology first" selection discipline.
api: queue
---

## Orientation

The queue is Volume 3's only **thread-safe** container — it was born for cross-thread data transfer, and it is therefore the bridge between Volume 3 and Volume 6 (concurrency): this chapter teaches the container's usage and discipline, and Volume 6 wires it into the complete system of threads, coroutines, and Channels — master the single weapon here, and formations come later. XRT provides three topologies: **SPSC** (single producer, single consumer — the fastest), **MPSC** (multiple producers, single consumer — the standard shape of event aggregation), and **MPMC** (any number of producers to any number of consumers — the most general and the most expensive). The three share one usage language: **binding external slots** (zero internal allocation), **batch send/receive** (PushBatch/PopBatch), and the **close-and-drain shutdown protocol** (after Close, producers stop and consumers drain). This chapter nails these three things and gives the "topology first" selection discipline — generality has a price; if you can pin down the topology, don't pay for the doubt.

## Introduction

A logging thread must receive log events from all worker threads — that is MPSC: several producers, one consumer. A render thread and a decode thread pass frames one-to-one — that is SPSC. A task dispatcher lets any thread submit and any thread claim — that is MPMC. If all three scenes were solved with "one mutex wrapped around an ordinary queue", it would run — but on a path doing millions of handoffs per second, lock contention grinds multicore parallelism flat — eight of ten cores waiting on a lock is a routine casualty; and the difference between the locked and lock-free designs will show up as concrete numbers from your own machine in this chapter's comparison exercise.

Lock-free queues replace locks with atomic operations: producer and consumer each advance their own pointer and publish it atomically for the other side — SPSC needs only two atomic head/tail pointers and does single-digit-millions per second at the median; MPMC must publish-synchronize every slot, the most expensive per item, but the batch interface amortizes the cost down (repository benchmark: 134 million items per second at batch 32). **The more determined the topology, the less synchronization** — that is "topology first": if log aggregation is definitely MPSC, use MPSC; don't reach for MPMC "just in case".

## Concepts

### Three topologies, one table

| Queue | Topology | Per-item cost | Typical scenario |
| --- | --- | --- | --- |
| SPSC | one-to-one | smallest | two-thread pipeline (decode → render) |
| MPSC | many-to-one | small | event aggregation (all threads → logging/monitoring thread) |
| MPMC | any | largest | task dispatch, work stealing |

### Three shared idioms

```diagram flow
- Binding slots: InitBuffer binds a caller array - the queue itself does zero internal allocation, capacity is the array length
- Batch send/receive: PushBatch/PopBatch move a whole group at once, amortizing synchronization cost
- Close and drain: Close announces once "no more producers"; consumers drain the remainder and terminate naturally
```

**Binding slots** means the queue's storage comes from the caller (a stack array, static storage, a pool — anything works) — the queue provides only synchronization and index logic, and this is one of the shapes the `_noalloc` contract tests cover. The **batch interface's** return value is "how many actually completed" (the `Count` of `xqueuebatchresult`) — the queue may be neither full nor empty and still complete your request partially; the return tells you how far you got. **Close and drain** is the synchronous heart of a concurrent program's termination protocol: Close must be called only after "confirming all producers are done"; already-enqueued data is not lost, and consumers keep draining.

### FIFO and starvation

All three queues are FIFO: first in, first out, order preserved across batches. When MPMC's multiple consumers PopBatch concurrently, the queue internally guarantees **each message is claimed exactly once** — no duplicate delivery, and no consumer coordination needed. "Fairness" is deliberately not promised: a high-frequency consumer may keep claiming more — if you need weighted fairness, do it at the business layer (quota counters); don't expect it from the queue.

### The relation to Volume 6

This chapter teaches only "passing pointers between threads on one machine" — cross-process transport belongs to Volume 7's network and serialization territory. Volume 6 places the queue into the complete concurrency system: cooperation with the coroutine scheduler (blocking waits become awaitable Pops), the Channel as a "queue with close semantics" wrapper, and when to use a Future instead of a queue. For now remember just this: **queues transfer `ptr`** — the message object's lifecycle belongs to the sender for allocation and to the receiver for processing; past the queue boundary, the sender must not free it again.

## Examples

### Complete program: MPMC batch send/receive and shutdown

From the repository example `examples/containers/queue_mpmc/main.c`:

```embed path="examples/containers/queue_mpmc/main.c" title="examples/containers/queue_mpmc/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/queue_mpmc/main.c -lws2_32 -liphlpapi
10
20
30
40
```

**What just happened.** (1) `xqueueslot Storage[8]` is the caller-provided slot array, bound by `InitBuffer(&Queue, Storage, 8)` — capacity 8 is decided by the array length, and the queue itself does zero allocation. (2) Two `PushBatch` calls of 2 items each: the return value `.Count` must be checked — "I asked for 2, how many actually went in" is a real question under load, and the program advances by the actual count. (3) `Close` is called after all producers finish — in this single-threaded example it follows immediately; in a multithreaded program it belongs to the shutdown protocol (wait for producer threads to join, then Close). (4) `PopBatch` claims 4 at once and the output is strictly FIFO: 10, 20, 30, 40 — order preserved across batches. With multiple consumers concurrently executing the same PopBatch, the interior guarantees exactly-once per item. (5) The messages are `ptr`: the `&pValues[i]` in the queue points into a stack array — the receiver borrows (borrowing) without freeing, the same ownership discipline as Chapter 5's connection handling.

### Complete program: SPSC minimal form

From `examples/containers/queue_spsc/main.c`, the fastest path of the one-to-one topology:

```embed path="examples/containers/queue_spsc/main.c" title="examples/containers/queue_spsc/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/queue_spsc/main.c -lws2_32 -liphlpapi
10
20
30
```

**What just happened.** The interface shape is fully isomorphic with MPMC — bound slots, batch send/receive, close and drain, all three present — the difference is entirely internal: SPSC has only one atomic head and one atomic tail pointer; the producer writes the head, the consumer writes the tail, and they never contend. For the same message volume its per-item cost is the smallest — that is "dedicated queue when the topology is determined" cashed in. Read the two samples' interface calls side by side: **learn one usage, and it drives all three queues** — that is the queue family's API design doing its job.

### Capacity meets backpressure for the first time

Queue capacity (the slot-array length) is not "bigger is better" — it is the first valve of **backpressure**: when the queue is full, PushBatch partially succeeds, and the producer sensing it cannot push should slow down or buffer — exactly a miniature of Chapter 68's network backpressure. Both extremes of capacity planning have costs: too small, and burst traffic fills it instantly, with producers spinning and retrying; too large, and resident memory climbs, and a consumer failure leaves a bigger backlog and a longer recovery. A practical starting point: capacity at 2 to 4 times what the consumer processes in one scheduling cycle, then calibrate with Chapter 6's stats and live queue-water-level measurements. The queue's Count is a public field — the producer's nearly-full check costs nothing.

### The discipline boundary of lock-free

Lock-free means no internal mutexes; it does not mean "concurrent anything goes" — two disciplines still hold. First, the two ends of SPSC are each exclusive: one producer thread and one consumer thread; multiple threads Pushing the same SPSC is undefined behavior — that is MPSC's scene. Second, the publication ordering of data inside slots is guaranteed by the queue, but your own message objects must be fully written before enqueue and only written again after dequeue — a write race across the queue boundary is not the queue's responsibility. Keep these two disciplines firmly in mind, and the speed of lock-free queues truly belongs to you.

## Contracts

- **Bound storage**: the slot array comes from the caller; capacity = slot-array length; the queue does zero internal allocation.
- **Batch checking**: Push/Pop return a `Count` of what actually completed; partial success is possible both when full and when empty.
- **Shutdown protocol**: `Close` is called once after all producers end; already-enqueued data can still be drained; Push after Close fails.
- **Exactly once**: with multiple consumers popping concurrently, each message is claimed exactly once — no external deduplication needed.
- **Message ownership**: queues transfer `ptr`; objects are created by the sender and processed by the receiver, ownership discipline as in Chapter 5; write before enqueue, write again only after dequeue.
- **Topology first**: if you can pin down one-to-one or many-to-one, choose SPSC/MPSC; MPMC charges the highest unit price for generality.

### Selection quick-triage: three questions decide the queue

In practice, ask yourself three questions in order. First: **how many pipelines?** Two threads one-to-one — SPSC, done. Second: **who aggregates?** Many sources converging on one processor (logging, monitoring, event loop) — MPSC. Third: **is it really any-to-any?** Many scenes written as MPMC, looked at closely, are actually "multiple producers + a fixed pool of consumers" — when consumer count is fixed and tasks are equivalent, MPSC into a dispatcher and SPSC from the dispatcher to each consumer, two levels of dedicated queues, is often faster and clearer than one level of MPMC. Whatever still stands after the three answers is a genuine MPMC scene: dynamic work stealing, centerless task graphs. Write these three Q&As into your design document's selection section, and reviews have something to check against.

## Pitfalls

### Pitfall 1: ignoring the batch return value (counts that don't add up under stress)

Symptom: occasional lost messages or messages stranded — counts that don't reconcile under stress testing, while everything is normal at low workload.

Cause: `PushBatch` requests 32, the queue has 5 slots free, so 5 go in; skipping the `Count` check treats all 32 as accepted, and every subsequent message is misaligned.

```c bad
xrtMPMCQueuePushBatch(&Queue, pMessages, 32u);   /* return value discarded */
/* the queue may have taken only the first N - the remaining 32-N silently strand in the send buffer */
```

```c good
xqueuebatchresult Sent = xrtMPMCQueuePushBatch(&Queue, pMessages, 32u);
if ( Sent.Count < 32u ) {
	RetryLater(pMessages + Sent.Count, 32u - Sent.Count);   /* resume from the breakpoint */
}
```

### Pitfall 2: closing before the producers have stopped (the shutdown protocol broken)

Symptom: occasional crashes or "the last batch of messages lost" during shutdown; reproduction depends on thread timing and gets harder to catch the more you stress — exactly why the shutdown protocol must run in a fixed order.

Cause: `Close` presupposes **all producers finished** — closing while some thread is still Pushing leaves the behavior of later Pushes undefined (or handled as failure, which your code ignored).

```c bad
/* main thread: not waiting for the workers */
xrtMPMCQueueClose(&Queue);
/* some worker is still Pushing at this moment - the shutdown protocol is broken */
```

```c good
for ( i = 0; i < WorkerCount; ++i ) {
	xrtThreadWait(Workers[i], NULL);   /* first wait for all producers to finish (Chapter 56's thread API) */
}
xrtMPMCQueueClose(&Queue);            /* then close */
DrainAndShutdown(&Queue);             /* consumers drain, then terminate naturally */
```

## Exercises

### Basic: FIFO and batch checking

Reproduce the MPMC example: capacity 4, three Push batches totaling 6 items (the middle batch partially succeeds), assert the actual progress by return values, then drain once and check the output order — first work out on paper which batch partially succeeds and how far it advances, then compare with the program.

### Advanced: a two-thread pipeline (shutdown protocol in practice)

Connect two threads with an SPSC queue: the producer sends one million incrementing sequence-number pointers (pointing into slots of a static array), the consumer verifies strict increase and finishes; close with the shutdown protocol. Hint: map sequence numbers modulo into a fixed slot array to avoid a million allocations.

### Challenge: a log aggregator (the full MPSC exercise)

Implement a minimal logging thread with an MPSC queue: 4 worker threads each produce a hundred thousand `{线程号, 序号}` events (sent in batches of 64), and the logging thread aggregates counts and verifies "within each thread the sequence numbers are continuous". Acceptance criteria: total counts with zero loss and zero duplication; per-thread sequences continuous without holes; clean shutdown (the logging thread exits naturally rather than being killed).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three topologies | SPSC one-to-one fastest / MPSC many-to-one / MPMC any, most general — one fully isomorphic three-part interface |
| Bound storage | `InitBuffer(队列, 槽位数组, 容量)`; zero internal allocation, capacity is the array length |
| Batch discipline | the returned `Count` must be checked; partial success resumes from the breakpoint |
| Shutdown protocol | all producers done (join) → `Close` → consumers drain → natural termination — four steps in fixed order |
| Exactly once | concurrent Pops by multiple consumers: no duplicates, no losses, no external dedup |
| Message ownership | transfers `ptr`; the sender creates, the receiver processes |
| Topology first | if the topology is determined, don't use MPMC — generality is priced per item |
| Three selection questions | one-to-one? who aggregates? truly any-to-any? — answer, then decide |
| Capacity starting point | 2–4× the consumer's per-cycle throughput; nearly-full checks read the public Count |
