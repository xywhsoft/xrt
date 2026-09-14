---
num: 48
slug: file-async
title: Asynchronous File IO
volume: 卷五 系统服务
type: practice
lead: Native asynchronous file reads/writes on the submit-complete model — callbacks fire on the system thread pool; waiting is optional, synchronous or via Futures.
api: file_async, file
---

## Orientation

The file-async module upgrades file IO from the blocking model to the **submit-complete model**: `xrtAsyncFileReadAt/WriteAt` submit an operation and immediately return a **Future** — the IO executes on a bound **task-pool thread** (the concurrent edition of Chapter 22's pools), and the calling thread takes the Future and moves on; `xrtFutureWait` awaits completion, `xrtFutureValue` retrieves the result (a read's result is `xfiledata` — data view + size + offset + EOF flag). The whole-file convenience layer `xrtFileReadAllAsync/WriteAllAsync` finishes in one step. This chapter covers the model's semantics, the Future disciplines (the iron laws of the async boundary), and the synchronous/asynchronous trade-off.

## Introduction

A log-archival service must move large files: synchronous Read a chunk, process a chunk, write a chunk — single-threaded serial execution, with all disk wait time charged to the processing thread. Want to move three files concurrently? Under the synchronous model that takes three threads each blocking (threads are expensive — Chapter 55's ledger). The asynchronous model changes the frame: **after submitting the "read this chunk" request, the thread is free** — the system completes the IO in the background and notifies you on completion; one thread can keep dozens of IOs in flight. The disk's parallelism potential (NCQ/SSD multi-channel) and the thread's liberation happen together.

The price is a mental-model switch: from "return means done" to "return means accepted" — completion timing, callback thread, and failure paths all change shape. This chapter's task is to make the model's disciplines clear; it is also the file-edition rehearsal of Chapter 63's network engine (event-driven) — the two chapters share the same submit-complete thinking.

## Concepts

### The submit-complete model

```diagram flow
- Bind: xrtAsyncFileOpen(path, flags, task pool) -> async handle (hung on the bounded pool you created)
- Submit: xrtAsyncFileReadAt/WriteAt(handle, offset, ...) -> Future; acceptance is the return
- Execute: a pool thread completes the IO; the bounded queue (e.g. 32) provides backpressure - a full pool fails submission, no endless piling
- Complete: FutureWait to wait / FutureValue to take xfiledata; Close is also asynchronous and returns a Future
```

The key recognition: **holding a Future is not completion** — between acceptance and completion the calling thread is free but the data isn't ready; every use of the result must pass Wait first (or Volume 6's continuation composition). The task pool is **one you created** (`xrtTaskPoolCreate({线程数, 队列深度, 标志})` — thread count, queue depth, flags) — the bounded queue's backpressure engages at "full pool, submission fails", not at "piled to OOM" (the file edition of Chapter 21's backpressure valve).

### Future disciplines: three laws of the async boundary

**First law: the value is borrowed** — after completion, `FutureValue`'s `xfiledata` is a **borrowed** view, valid until the next operation; to hold it long, copy it yourself (Chapter 3's borrowing discipline). **Second law: the buffer freeze window** — between submission and completion, the operation's buffer is untouchable (the IO thread is reading/writing it); to reuse early, use a buffer pool, one buffer per IO. **Third law: failure is also completion** — a Future's failure state and success state go through the same wait/value interfaces; a short read marks EOF with the `End` flag (a boolean bit of `xfiledata`), and failures carry error detail — wait code written for the success branch only will hang or misjudge when the disk fills.

### Two waiting postures (the choice is an architecture decision)

| Posture | Entrance | Fits |
| --- | --- | --- |
| Blocking wait | `xrtFutureWait` + `FutureValue` | simple scenes - chained dependencies, wait-for-all after batch submission |
| Future composition | Futures into Volume 6 combinators (continuation/concurrent joins) | concurrency systems - orchestrating many IO results |

Blocking wait is no regression: the batch pattern **"submit many + wait together"** still beats pure synchronous (the IOs ran in parallel on the pool); only completion handling is serial — a pragmatic choice for file batch processing. Future composition is the formal handshake between asynchrony and the concurrency system — Chapter 59 unfolds continuations, Chapter 60's task groups unfold concurrent joins. The whole-read/whole-write convenience layer (the async_whole sample): `xrtFileReadAllAsync/WriteAllAsync` does the whole file in one step — with a hard-cap variant (`ReadAllLimitAsync` — over the limit fails; resource limits' sentry in the async layer).

### The trade-off against the synchronous layer (not all IO should be async)

Cold water first: not all file IO deserves asynchrony. **Small config reads/writes** — synchronous three lines (Chapter 45) is simpler; async's complexity is paid for nothing. **Startup asset loading** — serial semantics is clearer; async gains are millisecond-level. **Large-file transfers, concurrent batch processing, IO-dense services** — async's home turf. The mnemonic: **many IOs, each large, parallelizable → async; scattered small reads/writes → synchronous**. Mixing is common too (synchronous at startup, async at runtime).

## Examples

### Complete program: the async write-read loop

From the repository sample `examples/file/async/main.c`:

```embed path="examples/file/async/main.c" title="examples/file/async/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/file/async/main.c -lws2_32 -liphlpapi
hello async file
```

**What just happened.** (1) `xtaskpoolconfig Config = { 2, 32, 0 }` — a **bounded task pool** of two threads and queue depth 32: IO executes on pool threads, and a full pool fails submission (backpressure's landing point at the file layer). (2) `xrtAsyncFileOpen` (pool, path, options) binds the file to the pool — every subsequent read/write queues through it. (3) `xrtAsyncFileWriteAt` and `xrtAsyncFileReadAt` each return a Future — the sample's `waitValue` helper (Wait + State check + Value fetch) is the blocking-wait posture in full; the read's result carries the data view and size via `xfiledata`. (4) **Read-write ordering is guaranteed through the Future chain** — wait for the write to complete before submitting the read; chained dependencies are established explicitly by waiting (no waiting means concurrent, order unguaranteed). (5) Close is also asynchronous and returns a Future — closing must also be waited on before the handle's resources are truly returned; printing `hello async file` verifies the write-read loop.

### Complete program: whole-file async

From `examples/file/async_whole/main.c` — the whole-read/whole-write convenience layer:

```embed path="examples/file/async_whole/main.c" title="examples/file/async_whole/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/file/async_whole/main.c -lws2_32 -liphlpapi
hello async whole file
```

**What just happened.** (1) The convenience layer packs "submit chunks - collect completions" into one call — whole-file async reads/writes without exposing chunking detail. (2) It solves the scene "the whole content is what I want" (config loading, asset reading) — chunking and assembly done by the library. (3) Its division with Chapter 45's synchronous whole-read: small files, synchronous three lines; large files or concurrency needed, come here — "convenience" doesn't change the model, only trims boilerplate.

## Contracts

- **Acceptance is the return**: a submission function's return means acceptance only; completion is delivered via callback or the wait object.
- **Callback-thread iron law**: callbacks run on system IO threads — light work, threaded handoff, complete failure branches.
- **Batch waiting**: many submissions + one unified wait is a legal shape — the parallel-IO gain is kept, completion handling serial.
- **Future entrance**: asynchronous operations can wrap into Futures and enter the concurrency system (Chapter 59) — the standard channel for composition and continuations.
- **Trade-off mnemonic**: many, large, parallelizable → async; scattered small reads/writes → synchronous; mixing chosen per phase.

### From examples to engineering: three hosts of async files

**Large-file transfer** (the pipeline's home turf): when read-chunk-A's Future completes, submit write-A and read-B — double-buffered rotation, zero idle waiting throughout; the advanced exercise is exactly this pipeline's skeleton. **Batch archiving** (concurrency's home turf): N files each submit a whole-read → when the Futures are all in, compress one by one → write back asynchronously — pool depth naturally caps concurrency (backpressure built in); Chapter 29's compression + Chapter 22's pools converge in the async layer. **Startup preloading** (the convenience layer's home turf): `ReadAllAsync` concurrently preloads several asset files while the main flow keeps initializing; Wait when needed — overlapping startup time. The three hosts share one point: **concurrency is governed by pool depth** — async is not unbounded concurrency but "controlled parallelism".

### The junction with Chapter 60's task system

This chapter's Futures and task pools are the file-side entrance of Chapter 60's task system: `xrtTaskSubmit` submits any function into the same pool — file IO and computation share the pool, one backpressure; the Future combinators (Chapter 59) express "merge when both read A and read B complete" as declarative orchestration; task groups (Chapter 60) manage "the lifecycle and cancellation of ten archive tasks" as one. What this chapter teaches is the primitive layer — pool, handle, Future; Volume 6 assembles them into complete concurrent workflows. Preview with this lens: every pit here has a corresponding wrapper-layer solution in Volume 6.

### A mental anchor: accepted ≠ completed

The chapter's most important sentence deserves its own section: **acceptance is not completion**. The synchronous intuition (call returns = result available) fails in the async world — a Future is "a promise of a result", not the result. Three concrete reminders: reading data immediately after getting a Future is the direct cause of Pitfall 2; Close returning a Future means "closing also waits" — handle resources are truly returned only after Close's Future completes; "wait for all" after batch submission means all, not any. Tape this sentence to your monitor; it prevents eighty percent of async pitfalls.

## Pitfalls

### Pitfall 1: heavy work inside a Future callback (the throughput killer)

Symptom: async throughput drops instead of rising; the task pool's queue backs up; every other async operation on the same pool collectively delays.

Cause: the callback occupies a task-pool thread — heavy computation eats the pool's throughput, and queued IOs all wait.

```c bad
static void onDone(ptr pValue)
{
	ProcessEntireFile(pValue);   /* heavy computation in a Future callback - hogging a scheduler thread */
}
```

```c good
static void onDone(ptr pValue)
{
	xrtTaskSubmit(gPool, DoHeavyWork, pValue, NULL);   /* light work: forward into the task pool */
}
/* heavy work runs as a pool task - the Future callback only forwards (the task system's usage, Chapter 60) */
```

### Pitfall 2: touching the buffer right after submission (freeze-window discipline)

Symptom: occasional dirty writes or half-new-half-old reads — the calling thread is touching the buffer while a pool thread's IO touches it too.

Cause: the buffer, in its freeze window (submission to completion), is touched concurrently from both sides — the second law skipped.

```c bad
xrtAsyncFileWriteAt(File, Offset, Buffer, Size, NULL);
memset(Buffer, 0, Size);   /* clearing the buffer right after submission - a pool thread is still reading it */
```

```c good
xrtAsyncFileWriteAt(File, Offset, Buffer, Size, NULL);
/* the buffer is frozen from submission until Wait completes - touch it only afterward */
/* to reuse early: switch to a buffer pool (Chapter 22), one buffer per IO */
```

## Exercises

### Basic: two operations concurrently (async's dividend, lesson one)

Submit one asynchronous write per file for two files, then wait together — compare total time against "synchronously writing two files" (the difference is obvious on SSD); print both timings side by side.

### Advanced: a pipelined mover

Implement `copy_async(源, 目标)` (source, target): read-chunk-A's completion triggers write-A and read-B — pipeline alternation, zero idle waiting throughout. Hint: rotate a fixed number of double buffers (Chapter 22's pool).

### Challenge: a concurrent log archiver

Ten log files concurrently gzipped and archived (Chapter 29's streaming compression): async read + compress on a worker thread + async write, the whole chain concurrent. Acceptance criteria: total time clearly below the serial version (report the comparison numbers); peak memory bounded (concurrency capped by the buffer pool); Chapter 6's statistics verify zero leaks.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Model | submit-complete: acceptance returns a Future; after Wait, take xfiledata via Value |
| Task pool | async handles bind to a bounded pool you create; queue depth = backpressure valve, full pool fails submission |
| Future's three laws | the value is borrowed / the buffer freeze window / failure is also completion (End bit marks EOF) |
| Waiting postures | Wait+Value (chained dependencies and batch waits) / Future composition (Volume 6 orchestration) |
| Convenience layer | ReadAllAsync/WriteAllAsync whole-file async in one step; the Limit variant brings a cap |
| Trade-off | many, large, parallelizable → async; scattered small reads/writes → synchronous |
