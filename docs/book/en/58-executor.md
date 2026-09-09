---
num: 58
slug: executor
title: Executors and Tasks
volume: 卷六 进程与并发
type: practice
lead: The abstraction of execution resources — submit-and-forget and Future-task twin shapes, batch interfaces, and the Close+Wait shutdown protocol.
api: executor, future
---

## Orientation

The executor is the **abstraction of execution resources**: tasks are submitted to it rather than opening threads directly — pooled reuse and work stealing are implemented at this layer; the business side sees only **submission and completion**. Twin task shapes: **submit-and-forget** (`xrtExecutorSubmit` — runs and done, no return value; the executor sample's 1000-task counting is this shape) and **Future tasks** (`xrtTaskSubmit` returning a Future — Chapter 57's currency); a **batch interface** (SubmitBatch — amortizing synchronization cost); and a **shutdown protocol** (`xrtExecutorClose` stops intake + `Wait` drains — isomorphic to Chapter 21's queue close-and-shutdown). Chapter 47's file-async task pool (xtaskpool) is the executor family's specialized form — this chapter teaches the general one.

## Introduction

The "open a thread per task" model goes bankrupt in three places.**Volume**: ten thousand concurrent tasks = ten thousand threads = memory explosion (1MB stack each) — pooled reuse runs ten thousand tasks on a few dozen threads.**Overhead**: thread creation is a millisecond-scale system call — for frequent short tasks the overhead exceeds the task itself; pool submission is microseconds (a lock-free queue enqueue).**Management**: a thousand bare threads each minding its own stopping, waiting, and error collection; the executor gives this batch of threads one uniform Close+Wait shutdown protocol and one statistical language.

So the executor's value formula: **thread reuse (volume) + lightweight submission (overhead) + uniform shutdown (management)**. The business-side change is just replacing "open a thread" with "submit a task" — you already did so in Chapter 47 (the file-async task pool); this chapter makes the executor's full interface surface and shutdown protocol clear.

## Concepts

### Twin task shapes

```diagram flow
- Forget task: xrtExecutorSubmit(executor, function, data) - no return value
- Future task: xrtTaskSubmit(task pool, function, data, cancel token) -> Future
- Batch: SubmitBatch / TaskSubmit batch variants - enqueue a group at once
- Choice: need a result/need cancellation -> Future shape; pure side effect (counting, notification, cleanup) -> forget
```

**Forget versus Future is not a rank distinction** — it is a fork of needs: pure side-effect tasks (log flushing, cache refresh, statistics reporting) waste a Future (nobody waits); result-bearing ones (computation, queries) must have a Future (Chapter 57's delivery model). The cancellation-token parameter rides naturally in the Future shape (Chapter 53's tree threading the task pool).

### The minimal submit-complete loop

`xrtExecutorCreate(配置)` (config) builds the instance (thread count and queue depth configurable — depth is the backpressure valve; a full pool fails submission rather than piling infinitely); the task function's signature is kin to the coroutine proc (`ptr(ptr)` or a void shape per the entrance); `xrtExecutorWait` waits for all to complete (`WaitFor/Until` timed variants). The executor sample's loop: submit 1000 forget tasks (each atomically incrementing a counter) → Wait → `completed: 1000` — the simplest form of **submit, execute, join**.

### The shutdown protocol: Close + Wait

| Step | Semantics |
| --- | --- |
| `xrtExecutorClose` | stops intake (further submissions fail); already-enqueued tasks **keep executing** |
| `Wait` / `WaitFor` | waits for the queue to drain and in-flight tasks to finish - the backlog is not lost |
| Destroy | after everything finishes, tear down the resources |

Fully isomorphic to Chapter 21's queue Close section (Chapter 47's shutdown protocol) — **closing is "stop intake", not "void"**: backlog tasks run to completion, Future tasks' results remain collectable. The executor_tour sample verifies: `close+wait closed=1 queued=0` — Close then Wait to drain, the queue at zero: machine testimony.

### Task pool vs scheduler: the endgame comparison

Chapter 55's table upgrades at the volume's end into its **endgame edition** (both chapters now learned): the task pool runs **functions** (stackless — run to completion, no mid-flight yield); the scheduler runs **coroutines** (stackful — Park/Sleep yield and resume).**The combined shape** is engineering's standard answer: blocking/CPU work met inside a coroutine is Posted to the task pool; the completion callback delivers a Future/continuation the result back to the scheduler-side coroutine (Chapter 57's continuation across models) — **coroutines are the waiting skeleton, the task pool the computing muscle**. Volume 7's network engine is exactly this combination in full.

## Examples

### Complete program: the thousand-task loop

From the repository sample `examples/concurrency/executor/main.c`:

```embed path="examples/concurrency/executor/main.c" title="examples/concurrency/executor/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/executor/main.c -lws2_32 -liphlpapi
completed: 1000
```

**What just happened.** (1) `xrtExecutorCreate(NULL)` — NULL takes the default configuration (thread count and queue depth per platform; pass a config to customize — depth is backpressure). (2) 1000 forget tasks submitted — each atomically increments a counter (Chapter 9's atomics — the counter is the tasks' only sharing; the join statistics complete lock-free). (3) `Wait` for all — `completed: 1000` proves all 1000 tasks executed on pool threads, none lost. (4) Note **no thread was opened for any task** — the pool reused fixed threads; submission cost is a microsecond-scale enqueue. This 62-line loop is the executor's minimal complete usage.

### Complete program: batch and shutdown

From `examples/concurrency/executor_tour/main.c` — batch interface and the Close+Wait protocol:

```embed path="examples/concurrency/executor_tour/main.c" title="examples/concurrency/executor_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/executor_tour/main.c -lws2_32 -liphlpapi
executor: batch=3 stats(submitted>=3 completed=3) ok
executor: close+wait closed=1 queued=0 ok
```

**What just happened.** (1) `SubmitBatch` enqueues three tasks at once — batching amortizes the synchronous cost (the executor edition of Chapter 21's queue batch interface); the statistics (submitted/completed) reconcile per batch. (2) After Close, further submissions fail (intake-stop verified) while the enqueued keep running; Wait drains — `queued=0` testifies the queue reached zero. (3) The protocol's two steps are non-interchangeable: Close first (else Wait may never finish between old tasks and new submissions), Wait after (Destroy only once the backlog executes). (4) Future-shaped tasks (`xrtTaskSubmit` with a cancellation token) unfold in the future/executor sample family — Chapter 57 taught them; this chapter composes.

## Contracts

- **Twin shapes**: forget (side effects) / Future (result + cancellation) — a fork of needs, not a rank.
- **Backpressure built in**: the queue-depth config is the cap — a full pool fails submission rather than piling (Chapter 21's discipline).
- **Shutdown protocol**: Close stops intake (backlog continues) → Wait drains → Destroy — the order is iron; isomorphic to queue closing.
- **Batch interface**: a group of tasks enqueued at once — amortized synchronization; statistics reconcile per batch.
- **Task discipline**: task functions don't loop forever (unless honoring a cancellation token); long tasks accept the token and set checkpoints (Chapter 53).
- **Model combination**: the scheduler manages the waiting skeleton, the pool the computing muscle — cross-model via Future/Post.

### From examples to engineering: three hosts of the executor

**Parallel computation** (the Future shape's home turf): batch processing, image processing, data analysis — data fragment shards submitted as Future tasks, joined via `future_combine` or Wait one by one; pool depth = CPU core count as the starting point (the compute boundary), excess submissions queue (backpressure).**The async-IO foundation** (Chapter 47's shape): file async, DNS resolution, external-process waiting — asynchronous-blocking IO wrapped as tasks into the pool, the caller takes a Future without waiting; depth by IO concurrency needs (IO-dense may exceed core count — waiting costs no CPU).**The scheduler's compute annex** (the combined shape): a coroutine meeting a CPU or blocking stretch → Post to the pool → the pool's completed Future/continuation → back to the scheduler-side coroutine — Volume 7's network engine's standard division. The three hosts' depth configs differ: CPU boundary by core count, IO boundary by concurrency budget, mixed by measurement — **depth is a performance parameter** (an object of Chapter 136's benchmarking methods).

### The executor family panorama

XRT's "execution resources" come in more than one implementation; one panorama prevents confusion.**xexecutor** (this chapter): the general executor — forget + batch + Close/Wait protocol; the task model is "a function that runs to completion".**xtaskpool** (Chapter 47): the task pool — `xrtTaskSubmit` returns a Future, cancellation token a first-class parameter; the dedicated foundation of file async; its relation to the executor is "same family, different emphasis" — taskpool's Future/cancellation integration is deeper, executor's batch and statistics fuller.**xcosched** (Chapter 55): the coroutine scheduler — not a task pool (runs coroutines, not functions), but its pump loop fills the same-position "execution resource" role. The three share one base vocabulary (queues, backpressure, shutdown); selection follows the task shape: functions → executor/taskpool, coroutines → sched, mixed → combination.

### An operations view: the pool's capacity economics

Pool depth is not a guess — it has economics.**Too small**: insufficient parallelism (CPU cores idle) or insufficient IO concurrency (throughput capped by the pool) — symptoms: queue depth chronically full, long task waits.**Too large**: thread memory (stacks), context switching (CPU-type excess does no good — there are only so many cores), downstream pressure (IO-type excess knocks over database connections) — symptoms: memory climbing, switching high, downstream erroring.**Measure and tune**: three datasets read together — queue-depth curves (chronically full → add or expand downstream), task-duration distributions (Chapter 6's statistics or Chapter 41's timing), system metrics (CPU/memory/switching). Starting points: CPU-type = core count, IO-type = concurrency budget (80% of downstream capacity), mixed starts CPU-type and grows by measurement.**Depth is not bigger-is-better** — it is "the gatekeeper of downstream capacity".

## Pitfalls

### Pitfall 1: infinite loops inside tasks never shut down

Symptom: Close+Wait never returns — one task loops forever and draining recedes past the horizon; the shutdown timeout force-kills, leaving undrained state.

Cause: the task ignores cancellation — the executor's shutdown presumes tasks end; a checkpoint-free infinite loop jams the protocol.

```c bad
static void taskForever(ptr pData)
{
	while ( true ) {
		Poll();   /* infinite loop - never checks the cancellation token */
	}
}
```

```c good
static void taskCancellable(ptr pData, xcancel* pCancel)   /* the Future shape carries a token */
{
	while ( !xrtCancelRequested(pCancel) ) {   /* checkpoint (Chapter 53) */
		Poll();
	}
	/* cancellation hit - the task returns, draining advances */
}
```

### Pitfall 2: leaking resources created inside forget tasks

Symptom: a forget task creates Futures/resources nobody releases — one leak per task; the statistics' live-object staircase climbs.

Cause: the forget shape's "nobody waits" is not "nobody is responsible" — owning products created inside a task must finish inside the task.

```c bad
static void taskLeak(ptr pData)
{
	xvalue* pTmp = xrtValueInt(42);   /* created and dropped - nobody frees */
	Report(pTmp);
	/* Release missing - the forget task's leak window */
}
```

```c good
static void taskClean(ptr pData)
{
	xvalue* pTmp = xrtValueInt(42);
	Report(pTmp);
	xrtValueRelease(pTmp);   /* created in the task, finished in the task */
}
```

## Exercises

### Basic: twin-shape comparison

The same computation (squaring) run ten times via forget+atomic result and via Future+Value — one concluding sentence each on code volume, join method, and result reading.

### Advanced: a batch-submission benchmark

1000 tasks: one-by-one submission versus ten batches of 100 — compare total submission time and throughput (Chapter 6's statistics or timing); quantify the batching dividend in your conclusion.

### Challenge: a mixed-workload executor

A configured executor: CPU tasks (Future shape, cancellable) + mocked IO tasks (forget, time-limited) mixed; the full shutdown protocol walked (Close → timed Wait → unfinished statistics). Acceptance criteria: workload ratio configurable; the cancellation tree threads the Future tasks (in-flight tasks drain within 2 seconds of shutdown); unfinished statistics reconcile with the logs.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Twin shapes | ExecutorSubmit forget / TaskSubmit→Future (result + cancellation) |
| Backpressure | queue depth is the cap - a full pool fails submission, no piling |
| Shutdown protocol | Close stops intake (backlog flows) → Wait drains → Destroy - the order is iron |
| Batch | SubmitBatch amortizes synchronization; statistics reconcile per batch |
| Task discipline | no infinite loops (or with token checkpoints); products of the task finish in the task |
| Combined model | the scheduler manages waiting, the pool computing - Future/Post across models |
