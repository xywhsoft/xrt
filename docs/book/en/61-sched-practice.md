---
num: 61
slug: sched-practice
title: The Scheduler in Practice: Coroutines, Channels, and Tasks in Joint Action
volume: 卷六 进程与并发 · 卷六收官
type: composition
lead: Assembling all of Volume 6's tools into one service skeleton — event loop, connection coroutines, compute offload, and structured shutdown.
api: coroutine, channel, future, task, cancel, task_net
---

## Orientation

Volume 6's closing chapter (composition type) assembles the whole volume's tools into a **runnable service skeleton**: the scheduler pump as event loop (Chapter 56), one coroutine per connection for straight-line handling (Chapters 55/56), a Channel as the internal message bus (Chapter 57), compute/CPU stretches offloaded to the task pool (Chapter 59), Futures/continuations bridging the two worlds (Chapter 58), and task groups for structured lifecycle and graceful shutdown (Chapter 60 + Chapter 54's cancellation tree + Chapter 49's signal trigger). The composition chapter's thesis: **concurrency tools' value lies not in individual use but in assembly into a complete program that "runs, is observable, and can stop"** — this chapter's skeleton is Volume 7's network engine in rehearsal.

## Introduction

A mini HTTP service's complete requirements list: accept connections (IO event-driven); straight-line logic per connection (read request → handle → write response); some requests need heavy computation (must not block the event loop); internal component cooperation (rate limiter, statistics aggregator); graceful shutdown (signal trigger → stop accepting → drain in-flight → timed force-quit). Each requirement maps to a Volume 6 tool — but each tool alone doesn't cover the whole: only a scheduler without a pool and compute jams the pump; only a pool without groups and shutdown has no join; only groups without the cancellation tree and shutdown doesn't propagate.

Assembly is the only path. This chapter walks the complete skeleton along the data flow — each link annotated with which chapter's what it uses, and why so chosen. Strip the protocol details from this skeleton and you have Volume 7's network engine (from Chapter 64), where you will see the same skeleton's industrial form loaded with TCP/TLS/HTTP.

## Concepts

### The skeleton panorama

```diagram flow
- Event loop: the scheduler's PollFor pump (Chapter 56) - the thread's main loop
- Accept: the accept coroutine (hung on listen events) -> new connection -> connection coroutine via xrtCoGo (Chapters 55/56)
- Connection coroutine: read->handle->write straight-line; heavy stretches Post to the task pool (Chapter 59) + Park awaiting the result (Chapter 56)
- Pool task: CPU computation finishes -> Future published (Chapter 58) -> Wake wakes the connection coroutine
- Internal bus: statistics/rate-limit messages into a Channel (Chapter 57) -> the aggregator coroutine consumes
- Lifecycle: the service group (Chapter 60) holds all connection subgroups; signal (Chapter 49) -> root token (Chapter 54) -> whole-tree cancellation
- Shutdown: group Cancel -> each coroutine senses at its yield point -> cleanup stack drains -> group Wait bounded -> Destroy
```

### Assembly's three key decisions

**Decision one: the division line between pump thread and pool**. The event-loop thread (the scheduler) runs all **waiting-type** logic (connections, protocols, coordination); the task pool runs all **computing-type** stretches (heavy computation, blocking IO). The boundary criterion: does this code **yield or hold** — yielding (waiting for data, waiting for events) belongs to the scheduler, holding (computing, blocking calls) to the pool. What if mixed: compute jams the pump (Pitfall 1, Chapter 56), or waiting occupies the pool (pool concurrency wasted).**Decision two: message versus sharing**. Connection coroutines share **zero state** among themselves (independent stacks each); cross-coroutine data flows (statistics reporting) go through a Channel — "tight inside, clear outside" (Chapter 57's CSP division), landed. The statistics aggregator is the only stateful coroutine (it owns the counters); every update travels by message.**Decision three: shutdown's two channels**. Signal → root token → scheduler Close (Chapter 56) + service group Cancel (Chapter 60), a dual channel — the scheduler stops the pump, the group drains; the two cooperate rather than duplicate (Close manages "no new work gets driven", Cancel manages "all in-flight drains").

### Walking the data flow

A request with heavy computation, end to end: (1) the accept coroutine receives a new connection (a listen event Wakes it) → `xrtCoGo` a connection coroutine; (2) the connection coroutine reads the request (Parking while the IO event is pending); (3) the handling stretch finds heavy computation → `xrtTaskSubmit` into the pool (with the subgroup's Future) → the connection coroutine Parks awaiting the result; (4) the pool thread finishes → Promise Resolves → `xrtCoWake` the connection coroutine; (5) the connection coroutine resumes, assembles the response, writes back (a write event); (6) a statistics message TrySends into the Channel (dropped when full — statistics' droppable degradation semantics) → the aggregator consumes. The whole path: **the pump thread blocks zero, the pool threads wait zero, messages lock zero** — three "zeros" are the assembly's verifiable correctness metrics.

### Walking the shutdown

The shutdown sequence (Chapter 49's signals, complete second half): (1) the signal callback sets the stop flag (light work — Chapter 49's discipline); (2) the main loop sees the flag: listener closed (stop accepting) → root token Request (Chapter 54) → service group Cancel + scheduler Close; (3) each connection coroutine senses cancellation at its Park/Yield point → cleanup stack executes in reverse (Chapter 55) → final state Cancelled; (4) the pool's in-flight tasks exit at checkpoints (Chapter 59 Pitfall 1's good form); (5) group WaitFor (5-second bound — Chapter 49's convention) → all final states, graceful exit; on timeout, record the undrained count, Destroy, and go. (6) The final statistics report (the group's stats — Chapter 60's observation confluence) into the logs.

### The assembly checklist

When writing your own concurrency skeleton, pass five checks: **zero blocking in the pump** (no blocking call on the scheduler thread — scan every coroutine's code); **every wait cancellable** (every Park/Wait/Recv carries a token — no death-waits at shutdown); **three-way final states** (success/failure/cancelled counted and reported separately — Chapter 60 Pitfall 2); **lifecycles aligned** (each resource class's scope boundary clear: connection subgroup = connection, service group = service, pool = engine — destruction order reverse of dependencies); **observation throughout** (queue depth, group Active, cancel counts, pump cycle — Chapter 50's pipeline, every sentry open in concurrency). All five green, and the skeleton graduates from "runs" to "deployable".

## Examples

### Complete program: the skeleton's core loop (event loop + coroutines + pool)

Assembly's minimal runnable core — pump, connection coroutines, compute offload, message bus, each in place:

```c
/* service_core.c - Volume 6 skeleton core loop: scheduler + coroutines + pool + channel */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

static xcosched* gSched;
static xtaskpool* gPool;
static xchannel* gStats;
static xatomic32 gDone;

static void aggregate(ptr pData)          /* the statistics aggregator coroutine body */
{
	ptr Item;
	while ( xrtChannelRecv(gStats, &Item) == XWAIT_OK ) {
		(void)xrtAtomic32FetchAdd((xatomic32*)pData, 1, XMEMORY_RELAXED);
	}
}

static int computeHeavy(int Input)        /* mocked heavy computation (runs on the pool) */
{
	int Sum = 0;
	for ( int i = 0; i < Input * 100; ++i ) {
		Sum += i % 7;
	}
	return Sum;
}

static ptr workerTask(ptr pData)          /* pool task: compute then publish */
{
	int Input = (int)(uintptr_t)pData;
	int Result = computeHeavy(Input);
	return (ptr)(uintptr_t)Result;
}

static ptr connection(ptr pData)          /* connection coroutine: straight-line logic */
{
	int Input = (int)(uintptr_t)pData;
	xfuture* Job = xrtTaskSubmit(gPool, workerTask,
		(ptr)(uintptr_t)Input, NULL);      /* offload pool (Chapter 59) */
	if ( Job == NULL ) {
		return NULL;
	}
	if ( xrtFutureWait(Job) != XWAIT_OK ) {  /* no blocking in the pump - direct wait here for demo simplicity */
		xrtFutureDestroy(Job);
		return NULL;
	}
	int Value = (int)(uintptr_t)xrtFutureValue(Job);  /* borrowed read (Chapter 58) */
	xrtFutureDestroy(Job);
	(void)xrtChannelTrySend(gStats, (ptr)(uintptr_t)Value);  /* bus reporting (Chapter 57) */
	return (ptr)(uintptr_t)Value;
}

int main(void)
{
	xatomic32 Count;
	xcoro* Agg;
	xtaskpoolconfig PoolCfg = { 2, 32, 0 };

	xrtAtomic32Init(&Count, 0);
	xrtAtomic32Init(&gDone, 0);
	gSched = xrtCoSchedCreate();               /* event loop (Chapter 56) */
	gPool = xrtTaskPoolCreate(&PoolCfg);       /* compute offload (Chapter 59) */
	gStats = xrtChannelCreate(64);             /* message bus (Chapter 57) */
	if ( (gSched == NULL) || (gPool == NULL) || (gStats == NULL) ) {
		return 1;
	}
	Agg = xrtCoGo(gSched, aggregate, &Count);  /* the aggregator coroutine */
	for ( int i = 1; i <= 3; ++i ) {
		(void)xrtCoGo(gSched, connection, (ptr)(uintptr_t)i);
	}
	(void)xrtCoSchedRun(gSched);              /* pump runs until all coroutines finish */
	xrtChannelClose(gStats);                   /* drain (Chapter 57's three-party rule) */
	(void)xrtCoSchedRun(gSched);               /* the aggregator consumes the backlog */
	xrtCoSchedDestroy(gSched);
	(void)xrtTaskPoolClose(gPool);             /* pool shutdown protocol (Chapter 59) */
	xrtTaskPoolWait(gPool);
	xrtTaskPoolDestroy(gPool);
	printf("aggregated=%d\n", (int)xrtAtomic32Load(&Count, XMEMORY_ACQUIRE));
	return 0;
}
```

```term
$ gcc -O2 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single service_core.c -lws2_32 -liphlpapi
$ ./a.exe
aggregated=3
```

**Where this code stands.** The volume's tools share one frame: `xrtCoSchedCreate`'s pump as the main line, three connection coroutines `xrtCoGo` running concurrent straight-line logic, heavy computation `xrtTaskSubmit`-ed to the offload pool, the channel built by `xrtChannelCreate` as the statistics bus, the aggregator coroutine consuming.**The three-phase teardown** in reverse dependency order: pump Run to empty → channel Close and drain → Run again (the aggregator consumes the backlog) → pool Close+Wait — each layer's shutdown protocol (Chapters 56/57/59) chained in the right order. `aggregated=3` testifies that all three connections' messages reached the bus and were all consumed. Note that the connection coroutine's `xrtFutureWait` in a real skeleton should be Park awaiting Wake (no blocking in the pump — this sample waits directly for brevity; Chapter 56 Pitfall 1's annotation applies here).

### Complete program: structured shutdown

From `examples/concurrency/task_group_scope/main.c` (Chapter 60) + Chapter 54's cancel sample in combined use — the skeleton's shutdown link:

```embed path="examples/concurrency/task_group_scope/main.c" title="examples/concurrency/task_group_scope/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/task_group_scope/main.c -lws2_32 -lipharp 2>/dev/null || gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/task_group_scope/main.c -lws2_32 -liphlpapi
parent completed = 1, cancelled = 1
```

**Where this code stands.** The shutdown link's empirical skeleton: parent group Cancel (the service group's big button) → child groups and leaves, whole tree requested → leaves each confirm final states (one completed, one cancelled — producer autonomy) → the parent's Wait takes the mixed statistics. Substitute it into the skeleton's shutdown sequence steps (3)~(5), add the signal trigger (Chapter 49) and the bounded wait (WaitFor) — the complete graceful-shutdown loop.**The two samples together cover the skeleton's every link**: sample one assembles the runtime (pump/coroutines/pool/bus), sample two the shutdown time (groups/cancellation tree/final-state statistics) — one runs, one stops, skeleton complete.

### Three new link-design checks (Volume 4's four, concurrency edition)

Chapter 36 set four link checks for data pipelines, Chapter 50 the observability edition — the concurrency skeleton adds three.**Check one: single data ownership**. Each concurrent entity (coroutine/task) owns its data; cross-entity only messages or Futures pass — the statistics aggregator is the only coroutine owning the counters (every update through the channel) — if two coroutines can write the same variable, the assembly is already wrong.**Check two: error paths structured too**. Was the skeleton's behavior after task failure written — the connection subgroup hangs the failed Future into the group too (visible in stats); failure responses ride the same channel (the consumer handles by final-state class); "success runs, failure never considered" is the seed of shutdown incidents in concurrency.**Check three: decomposable nesting**. Every layer of the skeleton (pump/bus/pool/group) testable alone — Chapter 50's pipeline thinking applied at the assembly layer: each layer's inputs and outputs explicit (messages/tasks/final states), layers organized as Chapter 36's station-function shapes — only testable layers dare compose.

### From skeleton to network engine: a Volume 7 preview

The mapping from this skeleton to Volume 7's network engine deserves stating in the closing chapter.**Scheduler pump → event port** (Chapter 64's five backends: IOCP/epoll/kqueue/io_uring/select — the pump's PollFor corresponds to the port's waiting interface);**connection coroutine → the engine Stream's event callbacks** (Chapter 66's gold-standard Accept/Read/Close callback tables — Volume 7 expresses these as callbacks rather than coroutines directly, but the scheduler can wrap callbacks as coroutines);**task-pool offload → the network engine's Worker pool** (isomorphic — compute never jams the IO threads);**message bus → the engine's internal statistics/management channels**;**service group → the engine's shutdown sequence** (Chapter 66's "Close callbacks may still be finishing on Workers; Engine destruction waits" — task-group Wait, engine edition). The mapping is no equivalence — Volume 7 adds protocol layers (TCP state machine, TLS, HTTP) and production details (backpressure watermarks, reference-counted buffers) above the skeleton — but **the skeleton's five-item checklist applies verbatim to the network engine**. Read Volume 7 with this chapter's skeleton diagram, asking each chapter "where on the skeleton does this layer mount".

### Network task groups (task_net): the Volume 6 to Volume 7 bridge

A ready-made bridge stands between the skeleton and the network engine: **an optional bridge from the task system onto the network Engine** (`XRT_FEATURE_TASK_NET` — one-directional; the network never leaks back into the Future/Task core). `xrtTaskNet` submits a task onto a designated affinity Worker: the `xtasknetproc` signature **borrows** an `xnetworker` ahead of the ordinary task arguments — direct access to the Worker buffer pool and Engine context, with cancellation, task values, structured errors, the temporary arena, and data ownership contracts all identical to ordinary tasks; the `After/Until` variants hand "delayed / deadline" timing to the Engine's timer (Chapter 9's deadline mathematics, engine edition). The group form `xrtTaskGroupNetUntil` reserves a task-group activity slot **before** submitting — **when the group is closed or at its cap the task never starts**; group cancellation propagates to the Future and waits for the timer's real cancellation to complete. Two hard rules: **network tasks run on the event-loop thread — no blocking or long computation** (heavy work goes to `xtaskpool`, exactly the skeleton's "pump never blocks" discipline modularized); when the Engine stops, immediate tasks still execute during the drain phase while delayed tasks fail with the structured error `XERR_CLOSED` — before the terminal state publishes, the timer, the cancel listener, and data destruction all complete (a consumer never sees a context still in Worker use).

From the repository example `examples/network/task/main.c` — immediate/delayed/deadline/group, four submission kinds in one tour:

```embed path="examples/network/task/main.c" title="examples/network/task/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/task/main.c -lws2_32 -liphlpapi
worker=0
value=42
worker=0
after: value=42
worker=0
until: value=42
worker=0
group-until: done
```

**What just happened.** (1) `xrtTaskNet` submits an immediate task — `xrtFutureWaitFor` waits with Chapter 9's deadline semantics, and the value is read through the `xrtFutureValue` borrow (Chapter 58's discipline). (2) `After`/`Until` demonstrate timer submission — the two differ only in taking "relative microseconds" versus "an absolute deadline", and the output proves the task indeed ran after expiry. (3) `GroupNetUntil` atomically admits a delayed task into a task group — group cancellation and cap protection are guaranteed by the reserved slot. (4) All four segments print `worker=0` — affinity submission lands on the same Worker, precisely the shape of "small work on the IO thread".

### The volume's retrospective: Volume 6's asset list

Volume 6 closes; the inventory.**Primitives layer**: the thread four-step lifecycle, the four-piece set (mutex/condition/quota/read-write lock), deadlock's four conditions and lock-order prevention (Chapter 53).**Cooperation layer**: the cancellation token trio and propagation tree (53), coroutine four states, three final states and the cleanup stack (54), the scheduler's core three and the pump family's three shapes (55), Channel capacity/close-three-party/Select/cancellation integration (56), Future four states/continuations/combinators/wait family (57), executor twin shapes/shutdown protocol/batch (58).**Structured layer**: the task group's core five/final-state tally/parent-child scopes/Done Future (59).**Mental models**: the cancellation main line, the three-layer assembly line, shared-versus-message, the waiting cost ladder, three granularities, structured concurrency's lineage. Plus this chapter's assembly skill — this list is the foundation of "can read any modern concurrency library, can design your own concurrency architecture". From Volume 7 on, these tools all enter combat.

## Contracts

- **Three-zero metrics**: pump thread blocks zero, pool threads wait zero, message paths lock zero — the assembly's verifiable correctness metrics.
- **Division criterion**: yielding belongs to the scheduler, holding to the pool — the boundary judged by "does it yield".
- **Shutdown's dual channel**: scheduler Close (stop driving) + group Cancel (drain the in-flight) — cooperating, not redundant; ordering and bounded waiting paired.
- **Teardown in reverse**: pump → bus → pool (reverse dependency); each layer its own shutdown protocol (Chapters 56/57/59).
- **Lifecycle alignment**: connection subgroup = connection, service group = service — scope boundary is resource boundary.
- **Five observation items**: queue depth, group Active, cancel counts, pump cycle, three-way final states — the deployment threshold.

### Evolution path: the skeleton's three-level iteration

The skeleton isn't designed in one shot; three levels each carry their returns.**Level one (runs)**: pump + coroutines + pool, the minimal three-piece loop — this chapter's sample one is this level; verify the three-zero metrics, walk the data flow.**Level two (stops)**: add signals, the cancellation tree, the service group, bounded waiting — the shutdown link Chapter 60's sample two supplies; verify the checklist's last three items (every wait cancellable/three-way final states/lifecycle alignment).**Level three (observable)**: wire Chapter 50's pipeline — queue depth, group Active, pump cycle into structured logs; add fault injection (Chapter 6) to verify failure paths. Most projects stop at one-and-a-half — "runs but can't stop gracefully" is the most common technical debt of concurrent systems; level two is the deployment threshold, level three the operations threshold. Roughly a week each — the skeleton's investment pays back in full at the first 3 a.m. alert.

### Performance view: the skeleton's three tuning points

The skeleton's performance bottlenecks sit at three predictable spots.**Pump cycle** (the PollFor deadline): too long delays event response, too short spins away CPU — start at 10ms, tune by event-latency needs (interactive ~1ms, batch ~50ms); the spin rate is the tuning feedback (the challenge exercise's acceptance item).**Pool depth** (Chapter 59's economics): CPU-type = core count, IO-type = concurrency budget — decided by the heavy-compute share; after measuring task-duration distributions, apply Chapter 59's formulas.**Channel capacity** (Chapter 57's three numbers): the statistics bus reports via TrySend, drop-when-full (capacity 64's peak shaving) — the droppable-message degradation semantics written into the design; request channels pick small values by backpressure-conduction needs. The three share one principle: **run with defaults first, wire observation, tune by data** — Chapter 137's concurrency installment of performance analysis will return with tools.

### One last reminder: the skeleton is a living document

The skeleton code (with the five-item checklist and assembly-decision comments) should be maintained as the team's **living document**: newcomers read the skeleton first (faster than ten book chapters — theory comes back later); new services start by copying the skeleton (the assembly decisions already proven); architecture changes modify the skeleton first (three-zero metrics green before touching business code). Skeleton-rot signals: any checklist item chronically red, assembly-decision comments drifting from the code, no one able to state some layer's shutdown protocol — any signal marks the refactoring window.**A concurrent system's maintainability lies not in code volume but in structural clarity** — the skeleton is that structure's physical carrier. Take it into Volume 7.

## Pitfalls

### Pitfall 1: wrong assembly order — coroutines before the pump

Symptom: coroutine submission fails or crashes — the depended-on scheduler/pool/channel isn't built yet (or already destroyed) while someone uses it.

Cause: the skeleton's components have a dependency order (pump before coroutines, bus before producers) — initialize reverse of dependencies, destroy forward of dependencies.

```c bad
(void)xrtCoGo(gSched, worker, NULL);    /* coroutine submitted first */
gSched = xrtCoSchedCreate();             /* pump built after - the submission target is invalid */
```

```c good
gSched = xrtCoSchedCreate();             /* pump first (the dependency base) */
gPool = xrtTaskPoolCreate(&PoolCfg);     /* then the offload */
gStats = xrtChannelCreate(64);           /* then the bus */
(void)xrtCoGo(gSched, worker, NULL);     /* work submitted last - dependencies ready */
```

### Pitfall 2: shutdown misses a layer — pool closed, bus undrained

Symptom: after shutdown the statistics come up short — messages still hang in the channel with no consumer; or pool tasks still write the closed channel (an error storm).

Cause: the three-layer shutdown (pump/bus/pool) walked only two — each layer has its own protocol; a missed layer is missed draining.

```c bad
xrtCoSchedClose(gSched);
(void)xrtCoSchedRun(gSched);   /* pump drained */
xrtTaskPoolDestroy(gPool);      /* pool destroyed outright - the bus's statistics messages stranded forever */
```

```c good
xrtCoSchedClose(gSched);
(void)xrtCoSchedRun(gSched);   /* pump drained (producers stopped) */
xrtChannelClose(gStats);
(void)xrtCoSchedRun(gSched);   /* bus drained (consumers finished) */
(void)xrtTaskPoolClose(gPool);
xrtTaskPoolWait(gPool);        /* pool drained - all three layers collected */
xrtTaskPoolDestroy(gPool);
```

### Debug walkthrough: the skeleton's three frequent failures

Skeleton-level failures (not single-tool bugs) come in three frequent shapes with localization paths.**Failure one: periodic response-latency spikes** — some connection's heavy computation ran directly on the pump (Chapter 56 Pitfall 1, skeleton edition): localize via the pump-cycle curve (spikes aligned with some request class) → scan that class's coroutine code for blocking calls → move to the pool.**Failure two: intermittent shutdown hangs** — some layer missed draining or some wait uncancellable: localize via the group Active curve (items not reaching zero after shutdown) → check that task class's waiting shape (was RecvCancel tokened; do pool tasks have checkpoints) → add the cancellable wait.**Failure three: statistics don't reconcile** — messages lost (TrySend drop-on-full is design, but dropped where not designed) or the consumer never drained (a shutdown layer missed): localize by comparing the three-level statistics (sent/received/aggregated) for the direction of the gap → follow the direction to the loss point. All three failures localize through observation (Chapter 50's pipeline) — **without observation, a skeleton's failure localization returns to the age of guessing**.

### The echo of Chapter 36: two pipelines compared

This chapter and Chapter 36 are the book's two composition pipelines; a comparison closes them.**The data pipeline** (Chapter 36): text in → value-tree processing → text out — synchronous, single-threaded, station functions chained.**The service pipeline** (this chapter): events in → coroutine handling → message flow → final-state convergence — asynchronous, multi-threaded, skeleton components assembled. The shared skeleton thinking: stations/components each with a single responsibility, explicit in/out contracts, individually testable; the difference is **the time dimension** — the data pipeline's stations execute in order, the service pipeline's components run concurrently, so the latter adds cancellation, convergence, and observation — three dimensions the data pipeline doesn't need. The two pipelines confluence in Volume 7 — the network engine is both a data pipeline (protocol parsing) and a service pipeline (connection management) — read Volume 7 with both chapters' checklists, and every chapter locates onto the pipeline.

## Exercises

### Basic: reproduce the three-piece core

Reproduce the skeleton's core loop (sample one halved: pump + two coroutines + channel) — output the statistics testimony; then deliberately Destroy the pool early and observe the error (understand dependency order).

### Advanced: cancellable connections

Give the connection coroutines cancellation tokens (Chapter 54): on cancellation the cleanup stack drains, final state Cancelled; the main program Cancels all connections then WaitFor joins bounded — verifying the "every wait cancellable" check.

### Challenge: the complete skeleton

Assemble the full service skeleton: signal-triggered shutdown + a rate-limiter coroutine (Channel receiving requests, token-bucket limiting) + three concurrent connections (each with compute offload) + three-level service-group statistics. Acceptance criteria: the five-item checklist passed item by item (output the checklist); the full shutdown under 5 seconds; the three statistics levels reconcile; Chapter 6 zero leaks. Save this skeleton — Volume 7's network chapters will load protocols onto it.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Assembly panorama | pump event loop + connection coroutines straight-line + compute offload pool + message bus + group lifecycle |
| Three-zero metrics | pump zero blocking / pool zero waiting / messages zero locking - the assembly correctness ruler |
| Division criterion | yielding to the scheduler, holding to the pool - "does it yield" decides in one vote |
| Shutdown dual channel | scheduler Close stops driving + group Cancel drains the in-flight - together a complete shutdown |
| Teardown in reverse | pump -> bus -> pool - reverse dependency, each layer its own protocol |
| Checklist | pump zero blocking / waits cancellable / three-way final states / lifecycle alignment / observation throughout |
| Skeleton's destiny | strip protocols, load TCP/TLS/HTTP = Volume 7's network engine |
| Network tasks | the task_net one-way bridge: affinity Worker + timer; no blocking heavy work on the event loop; group slot reservation |
