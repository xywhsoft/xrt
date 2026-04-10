# XRT Guide: Multitasking Overview

> Goal: explain why multitasking is needed before talking about APIs, then clarify what each option in the current XRT mainline actually solves, what the trade-offs are, and in which order they should be learned.

[中文](multitask-overview.md) | [Back to Guides](README.en.md)

---

## 1. Start with the Most Important Question: Why Do We Need Multitasking?

When people write their first small program, they often keep everything on one main thread:

1. read config
2. read files
3. connect to the network
4. wait for the response
5. parse the result
6. continue to the next step

That is fine for the smallest programs.

The problem starts when the program needs to do several things at the same time. A single main thread quickly hits these bottlenecks:

- one slow operation delays everything else
- I/O waiting and CPU work compete for the same time slice
- blocking in one module increases latency in another module
- the whole flow becomes "whatever blocks first blocks the whole program"

The 3 most common scenarios are:

### 1.1 Waiting for External Resources

For example:

- waiting for a file read
- waiting for a network response
- waiting for a subprocess to exit

The core problem here is not "the CPU is too weak". It is "the main thread is occupied by waiting".


### 1.2 Running Independent Work in Parallel

For example:

- receiving requests while compressing or encrypting
- processing the current task while prefetching the next batch
- having multiple workers consume tasks from a queue

The core problem here is not "the code is hard to write". It is "you really need more than one execution unit".


### 1.3 Keeping Long Wait Chains Readable

For example:

- send a request, wait for a response, then issue the next step
- wait for multiple futures and handle whichever completes first
- use one unified cleanup path after a task fails

The core problem here is not "whether threads exist". It is "whether orchestration turns into callback soup or a tangled state machine".


## 2. Where Does a Single Main Thread Actually Get Stuck?

The example below is intentionally small, but it makes the core problem obvious:

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xrtInit();

	printf("step-1 read config\n");
	xrtSleep(500);

	printf("step-2 call remote api\n");
	xrtSleep(2000);

	printf("step-3 parse and save result\n");
	xrtSleep(700);

	xrtUnit();
	return 0;
}
```

The logic is not wrong. The problem is:

- if the remote API is slow, parsing and saving must wait
- if another request arrives now, the main thread has no room left
- if a CPU-heavy step is inserted in the middle, the stall becomes even worse

So multitasking is not for show. It is for separating these responsibilities:

- who executes
- who waits
- who hands results off
- who organizes the whole flow


## 3. The Current XRT Multitasking Toolbox

XRT does not give you just one "thread API". It gives you several layers with different responsibilities.

### 3.1 `thread`

This is the most traditional and easiest layer to understand.

It solves:

- real parallel execution
- independent blocking work
- work that must leave the current thread context

Advantages:

- straightforward
- truly parallel
- good for CPU-heavy or independent blocking jobs

Costs:

- thread switching has overhead
- lifecycle management is more complex than single-thread code
- debugging gets harder as thread count grows


### 3.2 `queue`

A queue is not an execution unit. It solves handoff.

It fits:

- producer / consumer flows
- cross-thread message delivery
- worker models
- backpressure and close flows

Advantages:

- separates producers from consumers
- makes a strong module boundary
- lets you replace shared mutable state with message passing

Costs:

- a queue only hands work off, it does not execute it
- you still manage payload lifetime yourself
- the wrong queue model adds complexity for no gain


### 3.3 `coroutine`

Coroutines solve this problem:

"How do I write lots of waiting steps in one thread as if they were normal sequential code?"

They fit:

- single-thread orchestration
- sleep / deadline / event wait
- combined waiting over `future / wait-source`

Advantages:

- strong sequential readability
- good for async orchestration
- no need to spawn a thread for every wait

Boundaries:

- coroutines are not preemptive parallelism
- coroutines should not carry heavy CPU work themselves
- coroutines should not replace all threads


### 3.4 `future / task / promise`

This is one of the most underestimated layers in the current XRT mainline.

It does not solve "open one more execution unit". It solves:

- how to represent async results in one consistent way
- how to speak the same language for thread, coroutine, and engine-worker results
- how to unify waiting, timeout, cancellation, and composition

The recommended mental model is:

- `task`: start execution
- `future`: carry the result
- `promise`: write the result


### 3.5 `wait-source`

`wait-source` solves "what exactly are we waiting on?"

It turns these into one waiting object model:

- future
- stream wait
- dgram recv
- listener accept

This layer matters because it lets network waiting and ordinary async-result waiting use the same language.


### 3.6 `task group`

`task group` solves "these tasks belong to one logical scope; how do we close and collect them together?"

It fits:

- joining one batch of child tasks
- refusing new tasks after close
- propagating cancellation from parent work to child work

It is not just a future array. It is the entry point to structured concurrency.


## 4. One Selection Table to Remember First

| Model | What you really gain | Best fit | Main cost |
|------|----------------------|----------|-----------|
| `thread` | independent execution unit | CPU-heavy work, independent blocking tasks | thread overhead, harder lifecycle |
| `queue` | handoff and backpressure across threads | producer / consumer, worker systems | no execution, ownership is yours |
| `coroutine` | single-thread orchestration | long wait chains, event-driven flows | does not replace parallel execution |
| `future/task` | unified async result and execution abstraction | result composition, timeout, cancellation, continuation | needs one shared mental model first |
| `wait-source` | unified waiting interface | converging future and network waits | more infrastructure-like, less obvious at first glance |
| `task group` | scope management for a batch of tasks | batch join / cancel / close | better for medium and larger flows than the smallest demo |


## 5. Recommended Learning Order

If this is your first systematic pass through XRT multitasking, learn in this order:

1. why threads should not be created blindly
2. why queues are handoff, not execution
3. why coroutines fit orchestration rather than heavy work
4. why `future / task` unifies results
5. why `wait-source` unifies network and ordinary async waiting
6. why `task group` is a scope, not only a container

Or remember it in one sentence:

> Threads execute, queues hand work off, coroutines orchestrate, futures carry results, wait-sources unify waiting, and task groups close batches structurally.


## 6. Start from the Smallest Demos

The path below goes from the smallest runnable example toward something closer to a real project.

### 6.1 Demo 1: Start with Threads and Learn Independent Execution

This demo does one thing only: move one slow job into a worker thread.

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iJobId;
} DemoThreadCtx;

static uint32 procWorker(ptr pArg)
{
	DemoThreadCtx* pCtx = (DemoThreadCtx*)pArg;

	printf("worker %d start\n", (int)pCtx->iJobId);
	xrtSleep(500);
	printf("worker %d done\n", (int)pCtx->iJobId);
	return 0;
}

int main(void)
{
	xthread pThread;
	DemoThreadCtx tCtx;

	xrtInit();

	tCtx.iJobId = 1;
	pThread = xrtThreadCreate(procWorker, &tCtx, 0);
	if ( pThread == NULL ) {
		printf("create thread failed: %s\n", (char*)xrtGetError());
		xrtUnit();
		return 1;
	}

	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);

	xrtUnit();
	return 0;
}
```

What you should understand first:

- `xrtThreadCreate()` creates an independent execution unit
- threads created by XRT attach to the runtime automatically
- `xrtThreadDestroy()` does not kill the thread; it frees the thread management object

Good fits:

- CPU-heavy work
- independent blocking operations
- host constraints that require work to leave the current thread

Poor fits:

- "I only want to wait for a result"
- large numbers of tiny short-lived jobs
- problems where orchestration is the main bottleneck


### 6.2 Demo 2: Learn Queues as Handoff and Backpressure

As soon as there is more than one thread, the next thing that gets messy is data handoff.

This demo uses `xmpscqwait` to show:

- one producer thread pushes data
- the main thread consumes it in a blocking way
- the queue itself does not own the payload, so the caller frees it

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmpscqwait hQueue;
	int32 iStart;
} ProducerCtx;

static uint32 procProducer(ptr pArg)
{
	ProducerCtx* pCtx = (ProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < 3; i++ ) {
		int32* pValue = (int32*)xrtMalloc(sizeof(int32));
		if ( pValue == NULL ) {
			return 1;
		}

		*pValue = pCtx->iStart + i;

		while ( xrtMPSCQWaitTryPush(pCtx->hQueue, pValue) == XQUEUE_FULL ) {
			xrtSleep(1);
		}
	}

	return 0;
}

int main(void)
{
	xqueue_config tCfg;
	xmpscqwait hQueue;
	xthread pThread;
	ProducerCtx tCtx;
	ptr pItem;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 8;

	hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	tCtx.hQueue = hQueue;
	tCtx.iStart = 100;

	pThread = xrtThreadCreate(procProducer, &tCtx, 0);
	if ( pThread == NULL ) {
		xrtMPSCQWaitDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	for ( i = 0; i < 3; i++ ) {
		if ( xrtMPSCQWaitPopTimeout(hQueue, &pItem, 1000) == XQUEUE_OK ) {
			int32* pValue = (int32*)pItem;
			printf("pop value = %d\n", (int)*pValue);
			xrtFree(pValue);
		}
	}

	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	xrtMPSCQWaitClose(hQueue);
	xrtMPSCQWaitDestroy(hQueue);

	xrtUnit();
	return 0;
}
```

Key points here:

- a queue is about handoff, not execution
- `MPSC` fits "multiple producers -> one consumer"
- the wait wrapper solves "how does the consumer block while waiting?"
- this is a pointer queue, so payload lifetime remains your responsibility


### 6.3 Demo 3: Learn `task + future` as Unified Results

If you keep using only threads, shared variables, and locks, another problem appears very quickly:

- the thread runs
- the result exists
- but every module invents its own result protocol

That is when `task + future` should enter.

```c
#include "xrt.h"
#include <stdio.h>

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

int main(void)
{
	int32 iValue;
	int32* pResult;
	xfuture* pFuture;

	xrtInit();

	iValue = 12;
	pFuture = xTaskRunThread(procSquare, &iValue, 0);
	if ( pFuture == NULL ) {
		xrtUnit();
		return 1;
	}

	pResult = (int32*)xFutureWaitValueTimeout(pFuture, 3000);
	if ( pResult != NULL ) {
		printf("square = %d\n", (int)*pResult);
		xrtFree(pResult);
	}

	xFutureRelease(pFuture);
	xrtUnit();
	return 0;
}
```

The core shift:

- `xTaskRunThread()` wraps thread execution into a `future`
- the caller now waits on a `future`, not on a custom shared-variable protocol
- later, whether the task came from a thread, coroutine, or engine, the result model can stay the same


### 6.4 Demo 4: Add Coroutines Last and Learn Orchestration

This is the moment to bring coroutines in.

The reason is simple:

- threads solve execution
- `future` solves results
- coroutines are best at turning wait chains into readable sequential code

This demo shows:

- a coroutine used as the orchestration layer
- two compute tasks executed in threads
- the coroutine waiting on both futures and aggregating results on one thread

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iA;
	int32 iB;
} DemoCoCtx;

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

static void procMainCo(ptr pArg)
{
	DemoCoCtx* pCtx = (DemoCoCtx*)pArg;
	xfuture* pFutureA;
	xfuture* pFutureB;
	int32* pA;
	int32* pB;

	pFutureA = xTaskRunThread(procSquare, &pCtx->iA, 0);
	pFutureB = xTaskRunThread(procSquare, &pCtx->iB, 0);

	pA = (int32*)xFutureWaitCoValueTimeout(pFutureA, 3000);
	pB = (int32*)xFutureWaitCoValueTimeout(pFutureB, 3000);

	if ( (pA != NULL) && (pB != NULL) ) {
		printf("sum = %d\n", (int)(*pA + *pB));
	}

	if ( pA ) xrtFree(pA);
	if ( pB ) xrtFree(pB);
	xFutureRelease(pFutureA);
	xFutureRelease(pFutureB);
}

int main(void)
{
	xcosched* pSched;
	DemoCoCtx tCtx;

	xrtInit();

	tCtx.iA = 10;
	tCtx.iB = 20;

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		xrtUnit();
		return 1;
	}

	xrtCoSchedSpawn(pSched, procMainCo, &tCtx, 0);
	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);

	xrtUnit();
	return 0;
}
```

The real point of this demo is:

- the coroutine does not do heavy work itself
- the threads do not own the whole flow
- `future` becomes the result bridge between thread execution and coroutine orchestration

That is the recommended division of labor in the current XRT mainline.


## 7. When Should `wait-source` and `task group` Enter?

By the time you reach the fourth demo, you can already build many useful programs.

The next two layers usually appear in fuller projects.

### 7.1 `wait-source`

When you begin waiting on several kinds of objects together:

- future
- stream readable / writable
- dgram recv
- listener accept

you should stop inventing a separate waiting protocol for each one and converge on `xwaitsrc`.

It fits:

- unified network and non-network waiting
- one waiting entry inside coroutines
- combined waiting and timeout management


### 7.2 `task group`

When you start managing "a batch of child tasks that belong to one scope", it is time for `task group`.

It fits:

- joining one batch of child tasks together
- dynamically adding and removing futures
- refusing new tasks after close
- closing child work together when the parent scope is canceled

One-sentence version:

> `task group` is not "future array plus more functions". It is the scope of structured concurrency.


## 8. Common Mistakes

### 8.1 Mistake 1: If the Program Is Slow, Just Create More Threads

Wrong.

If the real bottleneck is:

- waiting on I/O
- waiting on futures
- waiting on network events

then what you often need is:

- coroutines
- `future`
- `wait-source`

not more threads.


### 8.2 Mistake 2: If Coroutines Exist, Threads Are No Longer Needed

Wrong.

Coroutines are for orchestration. They do not replace real parallel execution.

CPU-heavy work and independent blocking work should still go to threads or engine workers first.


### 8.3 Mistake 3: Queues Solve All Concurrency Problems

Wrong.

Queues only solve handoff.

They do not replace:

- execution units
- result models
- structured waiting


### 8.4 Mistake 4: Let Thread Functions Write Shared State Directly and Call It Done

That may run in the short term, but it gets messy fast.

The steadier pattern is usually:

- queues for message handoff
- `future` for results
- coroutines for unified waiting and orchestration


## 9. Recommended Memory Hook

If you want one sentence to keep in mind, use this:

> Threads execute, queues hand work off, coroutines orchestrate, futures carry results, wait-sources unify waiting, and task groups close batches structurally.

If this line stays clear, the program structure is usually still in good shape.


## 10. What to Read Next

Read next in this order:

1. [Thread Intro: When to Create Threads, and When Not To](thread-intro.en.md)
2. [Queue Intro: When Message Handoff Is Better Than Shared State](queue-intro.en.md)
3. [Runtime and Thread Attach](runtime-thread-attach.en.md)
4. [Queue API](../api/api-queue.en.md)
5. [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)
6. [Wait-Source Intro: Speak One Waiting Language for Futures and Network Events](wait-source-intro.en.md)
7. [Task Group Intro: From Unified Waiting to Structured Closure](task-group-intro.en.md)
8. [Coroutine API](../api/api-coroutine.en.md)
9. [Future / Task / Promise](../api/api-future-task-promise.en.md)
10. [Case: Thread / Coroutine / Future Coordination](../case/thread-coroutine-future.en.md)

---

**One-sentence summary:** multitasking is not just "open more threads"; in the current XRT mainline, the stable way to think is to separate execution, handoff, orchestration, results, and waiting first, then decide whether the job belongs to threads, queues, coroutines, or futures.
