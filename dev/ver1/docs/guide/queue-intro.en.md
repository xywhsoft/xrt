# XRT Guide: Queue Intro

> Goal: explain why queues are needed, how they differ from shared state plus locks, how to choose between `SPSC / MPSC / MPMC / wait wrapper`, and how to reason about `Close / Drain / Reset` correctly.

[中文](queue-intro.md) | [Back to Guides](README.en.md)

---

## 0. What to Read First

If you have not built the overall XRT multitasking mental model yet, read these first:

- [Multitasking Overview](multitask-overview.en.md)
- [Thread Intro](thread-intro.en.md)

This page focuses on one thing:

- why threads need an explicit handoff protocol
- what problem queues really solve
- how the current XRT queue mainline should be turned into code


## 1. Why Do We Need Queues?

Many programs grow like this:

1. the main thread does everything
2. later the program is too slow, so a worker thread is added
3. later two threads start sharing the same data
4. finally the code is full of shared fields, locks, flags, and confusion about who is allowed to mutate what

The problem is usually not "there are not enough threads".

It is:

> there is no clear handoff boundary.


### 1.1 The Problem with a Single Main Thread

If there is only one main thread, producing work and consuming work can only happen in sequence:

- prepare data first
- process data next
- while processing is still running, the next batch must wait

There is barely a handoff concept here because everything happens in one execution flow.


### 1.2 The Problem with Shared State in Multiple Threads

As soon as the program becomes multi-threaded, the first reaction is often:

- several threads update one array
- several threads push into one list
- locks are used to protect it just enough to keep it running

That may work in the short term, but several problems quickly appear:

- who owns the data is unclear
- who frees it is unclear
- what happens when it is full is unclear
- when the consumer should exit after producers stop is unclear


### 1.3 The 3 Things Queues Really Solve

The value of a queue is not "the program looks more advanced".

It is that queues make these 3 things explicit:

1. who produces and who consumes
2. how data is handed off in FIFO order
3. how both sides should close down when the queue is full, closed, or drained

One-sentence version:

> Threads solve who executes. Queues solve how execution units hand work off to each other.


## 2. A Queue Is Not an Execution Unit

This must be clear first.

A queue will not:

- open threads for you
- do computation for you
- execute tasks for you

A queue only:

- stores pointers that are waiting to be handed off
- returns them to consumers in order
- gives clear results for states such as full, empty, closed, and drained

So the normal division of labor is:

- threads execute
- queues hand work off
- `future` carries results
- coroutines orchestrate the flow

If a queue is treated as the execution model itself, the design will become confused later.


## 3. How to Choose from the Current XRT Queue Family

The current formal XRT queue mainline has 4 entry points:

| Type | Producers | Consumers | Best fit | Cost you pay |
|------|-----------|-----------|----------|--------------|
| `xspscq` | 1 | 1 | single-producer/single-consumer hot path | strictly 1-to-1 only |
| `xmpscq` | many | 1 | many workers pushing into one consumer | the consumer side is still single |
| `xmpmcq` | many | many | general worker pool | highest cost, should not be the default |
| `xmpscqwait` | many | 1 | `MPSC` with blocking consumer waits | pop path is serialized; not a "blocking MPMC" |

For a first pass, remember this rule:

1. if `SPSC` is enough, do not start with `MPSC`
2. if `MPSC` is enough, do not start with `MPMC`
3. only choose `xmpscqwait` when the consumer truly needs blocking waits


## 4. 6 Rules to Remember Before Using a Queue

### 4.1 This Is a Bounded Queue, Not an Unbounded Queue

That means:

- capacity is fixed
- a full queue returns `XQUEUE_FULL`
- the caller must decide whether to retry, yield, drop, or slow down

This is part of backpressure.


### 4.2 Actual Capacity Is Rounded Up to a Power of Two

For example, if you request:

- `3`

the actual capacity becomes:

- `4`

So do not confuse requested capacity with the final capacity.


### 4.3 This Is a Pointer Queue, Not an Object-Lifetime Manager

The queue stores:

- `ptr`

not object copies.

That means:

- before `Push` succeeds, the producer still owns the payload
- after `Push` succeeds, responsibility usually moves to the consumer
- a `Drain` callback helps you consume leftovers, but does not free objects automatically


### 4.4 `Close` Does Not Mean "Clear Everything Immediately"

`Close` means:

- no new `Push` is allowed
- elements already inside may still be popped

So the usual shutdown order is:

1. stop producing new work
2. `Close`
3. consume or `Drain` the remaining elements
4. then destroy the queue


### 4.5 `Reset` Is Only for a Closed and Fully Drained Queue

That means:

- if the queue is still active, do not `Reset`
- if the queue still contains elements, do not `Reset`

If `Reset` is treated as a brute-force restart, in-flight elements are easily lost.


### 4.6 `ApproxCount` Is an Observation Value Only

Especially under `MPSC / MPMC`, it is suitable for:

- monitoring
- logging
- rough load estimation

It is not suitable for:

- strict synchronization conditions
- conclusions like "it is 0, so nobody will push anything anymore"


## 5. Demo 1: Learn `SPSC` First and Understand the Smallest Handoff

Start with the simplest model:

- one producer
- one consumer
- hand off 5 integers in FIFO order

```c
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	xspscq hQueue;
	volatile int32 iSum;
} DemoSPSCConsumerCtx;

static uint32 procConsumer(ptr pArg)
{
	DemoSPSCConsumerCtx* pCtx = (DemoSPSCConsumerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtSPSCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			pCtx->iSum += (int32)(uintptr_t)pItem;
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

int main(void)
{
	xqueue_config tCfg;
	xspscq hQueue;
	xthread hThread;
	DemoSPSCConsumerCtx tCtx;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 8u;

	hQueue = xrtSPSCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = hQueue;

	hThread = xrtThreadCreate(procConsumer, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtSPSCQDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	for ( i = 1; i <= 5; i++ ) {
		for ( ;; ) {
			xqueue_result iRet = xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)i);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}

			xrtSPSCQClose(hQueue);
			xrtThreadWait(hThread);
			xrtThreadDestroy(hThread);
			xrtSPSCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
	}

	xrtSPSCQClose(hQueue);
	xrtThreadWait(hThread);
	printf("sum = %d\n", (int)tCtx.iSum);

	xrtThreadDestroy(hThread);
	xrtSPSCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

The most important part of this demo is not adding numbers from 1 to 5. It is understanding:

- `SPSC` allows exactly one producer and one consumer
- `FULL` and `EMPTY` are normal states, not exceptions
- after `Close`, the consumer receives `XQUEUE_CLOSED` only after all queued items are drained

Here integers are cast to `ptr` only to keep the demo minimal. In real projects, it is safer to pass:

- heap object pointers
- pointers to structs with a stable lifetime


## 6. Demo 2: Learn `MPSC` and Understand "Many Producers into One Consumer"

Many real project queues look like this:

- many workers produce jobs
- one save thread, log thread, or main thread collects them

That is the typical `MPSC` shape.

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iProducerId;
	int32 iValue;
} DemoJob;

typedef struct
{
	xmpscq hQueue;
	int32 iProducerId;
	int32 iCount;
} DemoMPSCProducerCtx;

static void procDrainJob(ptr pItem, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pItem);
}

static uint32 procProducer(ptr pArg)
{
	DemoMPSCProducerCtx* pCtx = (DemoMPSCProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iCount; i++ ) {
		DemoJob* pJob = (DemoJob*)xrtMalloc(sizeof(DemoJob));
		if ( pJob == NULL ) {
			return 1;
		}

		pJob->iProducerId = pCtx->iProducerId;
		pJob->iValue = i + 1;

		for ( ;; ) {
			xqueue_result iRet = xrtMPSCQTryPush(pCtx->hQueue, pJob);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}

			xrtFree(pJob);
			return 2;
		}
	}

	return 0;
}

int main(void)
{
	xqueue_config tCfg;
	xmpscq hQueue;
	xthread arrThreads[3];
	DemoMPSCProducerCtx arrCtx[3];
	int32 iReceived = 0;
	int32 iStarted = 0;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 16u;

	hQueue = xrtMPSCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(arrThreads, 0, sizeof(arrThreads));
	memset(arrCtx, 0, sizeof(arrCtx));

	for ( i = 0; i < 3; i++ ) {
		arrCtx[i].hQueue = hQueue;
		arrCtx[i].iProducerId = i + 1;
		arrCtx[i].iCount = 4;
		arrThreads[i] = xrtThreadCreate(procProducer, &arrCtx[i], 0);
		if ( arrThreads[i] == NULL ) {
			xrtMPSCQClose(hQueue);
			for ( i = 0; i < iStarted; i++ ) {
				xrtThreadWait(arrThreads[i]);
				xrtThreadDestroy(arrThreads[i]);
			}
			xrtMPSCQDrain(hQueue, procDrainJob, NULL);
			xrtMPSCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
		iStarted++;
	}

	for ( i = 0; i < 3; i++ ) {
		xrtThreadWait(arrThreads[i]);
		xrtThreadDestroy(arrThreads[i]);
	}

	xrtMPSCQClose(hQueue);

	for ( ;; ) {
		ptr pItem = NULL;
		xqueue_result iRet = xrtMPSCQTryPop(hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			DemoJob* pJob = (DemoJob*)pItem;
			printf("producer=%d value=%d\n", (int)pJob->iProducerId, (int)pJob->iValue);
			xrtFree(pJob);
			iReceived++;
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
	}

	printf("received=%d\n", (int)iReceived);
	xrtMPSCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

There are 2 boundaries to watch closely here.

### 6.1 The Queue Turns Shared Writes into Message Handoff

Producers no longer mutate the consumer's internal state directly. They only:

- construct `DemoJob`
- push `DemoJob*` into the queue

This makes module boundaries much clearer.


### 6.2 Ownership Moves Only After `Push` Succeeds

This rule must be remembered exactly:

- before `Push` succeeds, `pJob` still belongs to the producer
- after `Push` succeeds, `pJob` enters the consumer's responsibility domain
- if `Push` fails and you give up, the producer still has to `xrtFree()` it

This is the core ownership rule of a pointer queue.


## 7. Demo 3: Use `xmpscqwait` When the Consumer Should Block

The earlier `TryPop()` model is busy polling:

- if the queue is empty, `Yield`
- try again later

That is common on hot paths, but some programs want this instead:

- sleep when there is no work
- wake when work arrives

That is when `xmpscqwait` fits.

```c
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmpscqwait hQueue;
} DemoWaitConsumerCtx;

static uint32 procWaitConsumer(ptr pArg)
{
	DemoWaitConsumerCtx* pCtx = (DemoWaitConsumerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, 500u);
		if ( iRet == XQUEUE_OK ) {
			printf("pop value = %d\n", (int)(uintptr_t)pItem);
			continue;
		}
		if ( iRet == XQUEUE_TIMEOUT ) {
			printf("consumer idle...\n");
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

int main(void)
{
	xqueue_config tCfg;
	xmpscqwait hQueue;
	xthread hThread;
	DemoWaitConsumerCtx tCtx;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 8u;

	hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = hQueue;

	hThread = xrtThreadCreate(procWaitConsumer, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtMPSCQWaitDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	xrtSleep(200);

	for ( i = 1; i <= 3; i++ ) {
		if ( xrtMPSCQWaitTryPush(hQueue, (ptr)(uintptr_t)(100 + i)) != XQUEUE_OK ) {
			xrtMPSCQWaitClose(hQueue);
			xrtThreadWait(hThread);
			xrtThreadDestroy(hThread);
			xrtMPSCQWaitDestroy(hQueue);
			xrtUnit();
			return 1;
		}
	}

	xrtSleep(200);
	xrtMPSCQWaitClose(hQueue);

	xrtThreadWait(hThread);
	xrtThreadDestroy(hThread);
	xrtMPSCQWaitDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

The key conclusion here is not the API name. It is:

- `xmpscqwait` is still `MPSC`
- it only adds blocking `Pop`
- there is no "blocking MPMC" hidden behind it

So if your model is:

- many producers
- one dedicated consumer thread
- the consumer should sleep while idle

then `xmpscqwait` fits very well.

If you need:

- many consumers blocking at the same time

do not mistake it for `MPMC wait`.


## 8. Demo 4: Learn `MPMC` Last and Understand Worker Pools

`MPMC` is the most general model, but also the one you should be least eager to choose by default.

Only move to it when you truly need:

- many producers pushing work
- many consumers taking work

```c
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmpmcq hQueue;
	int32 iBaseValue;
	int32 iCount;
} DemoMPMCProducerCtx;

typedef struct
{
	xmpmcq hQueue;
	volatile int32 iHandled;
} DemoMPMCConsumerCtx;

static uint32 procMPMCProducer(ptr pArg)
{
	DemoMPMCProducerCtx* pCtx = (DemoMPMCProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iCount; i++ ) {
		for ( ;; ) {
			xqueue_result iRet = xrtMPMCQTryPush(
				pCtx->hQueue,
				(ptr)(uintptr_t)(pCtx->iBaseValue + i + 1)
			);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			return 1;
		}
	}

	return 0;
}

static uint32 procMPMCConsumer(ptr pArg)
{
	DemoMPMCConsumerCtx* pCtx = (DemoMPMCConsumerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPMCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			pCtx->iHandled++;
			printf("consumer got value=%d\n", (int)(uintptr_t)pItem);
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

int main(void)
{
	xqueue_config tCfg;
	xmpmcq hQueue;
	xthread arrProducer[2];
	xthread arrConsumer[2];
	DemoMPMCProducerCtx arrProdCtx[2];
	DemoMPMCConsumerCtx arrConsCtx[2];
	int32 iConsumerStarted = 0;
	int32 iProducerStarted = 0;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 16u;

	hQueue = xrtMPMCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrConsumer, 0, sizeof(arrConsumer));
	memset(arrProdCtx, 0, sizeof(arrProdCtx));
	memset(arrConsCtx, 0, sizeof(arrConsCtx));

	for ( i = 0; i < 2; i++ ) {
		arrConsCtx[i].hQueue = hQueue;
		arrConsumer[i] = xrtThreadCreate(procMPMCConsumer, &arrConsCtx[i], 0);
		if ( arrConsumer[i] == NULL ) {
			xrtMPMCQClose(hQueue);
			for ( i = 0; i < iConsumerStarted; i++ ) {
				xrtThreadWait(arrConsumer[i]);
				xrtThreadDestroy(arrConsumer[i]);
			}
			xrtMPMCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
		iConsumerStarted++;
	}

	for ( i = 0; i < 2; i++ ) {
		arrProdCtx[i].hQueue = hQueue;
		arrProdCtx[i].iBaseValue = i * 100;
		arrProdCtx[i].iCount = 4;
		arrProducer[i] = xrtThreadCreate(procMPMCProducer, &arrProdCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			xrtMPMCQClose(hQueue);
			for ( i = 0; i < iProducerStarted; i++ ) {
				xrtThreadWait(arrProducer[i]);
				xrtThreadDestroy(arrProducer[i]);
			}
			for ( i = 0; i < iConsumerStarted; i++ ) {
				xrtThreadWait(arrConsumer[i]);
				xrtThreadDestroy(arrConsumer[i]);
			}
			xrtMPMCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
		iProducerStarted++;
	}

	for ( i = 0; i < 2; i++ ) {
		xrtThreadWait(arrProducer[i]);
		xrtThreadDestroy(arrProducer[i]);
	}

	xrtMPMCQClose(hQueue);

	for ( i = 0; i < 2; i++ ) {
		xrtThreadWait(arrConsumer[i]);
		printf("consumer[%d] handled=%d\n", (int)i, (int)arrConsCtx[i].iHandled);
		xrtThreadDestroy(arrConsumer[i]);
	}

	xrtMPMCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

What this demo is really trying to show:

- `MPMC` fits worker pools
- on shutdown, you do not need to wait for the queue to become empty before calling `Close`
- after `Close`, consumers may still drain remaining tasks until they receive `XQUEUE_CLOSED`

If the consumer side is always only one thread, do not choose `MPMC` just because it looks more general.


## 9. How to Think about `Close / Drain / Reset`

This is one of the easiest lifecycle areas to write incorrectly.

### 9.1 `Close`

`Close` means:

- reject new `Push`
- still allow queued elements to be consumed

Typical moments to call it:

- production is finished
- the program is shutting down
- a worker should stop receiving new jobs


### 9.2 `Drain`

`Drain` means:

- pull leftover elements out one by one
- hand them to your callback

It is commonly used for:

- cleaning heap objects that were not fully consumed
- centralized shutdown cleanup

If payloads are heap objects, the callback often looks like this:

```c
static void procDrain(ptr pItem, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pItem);
}
```


### 9.3 `Reset`

The preconditions are strict:

- the queue is already `Close`d
- the queue has already been emptied either by normal `Pop` or by `Drain`

It fits:

- reusing one embedded queue object
- tests and staged reuse

It does not fit:

- a producer/consumer queue that is still running


## 10. Batch APIs, Approximate Counts, and Performance Choices

`MPSC / MPMC` also provide:

- `PushBatch`
- `PopBatch`

These fit:

- processing tasks in batches
- reducing per-item call overhead

Remember 2 things:

### 10.1 Batch APIs Allow Partial Success

For example, you may try to push 8 items and only 5 succeed. That is normal.

So the correct mental model is:

- the return value tells you how many items succeeded
- the remaining items are still the caller's responsibility


### 10.2 Choose the Right Model Before Optimizing Batch Size

The usual optimization order should be:

1. choose the right model from `SPSC / MPSC / MPMC`
2. decide whether a wait wrapper is needed
3. only then tune batch behavior and capacity

Do not begin with "the strongest MPMC plus Batch" by default.


## 11. Windows / Linux Cross-Platform Notes

The queue API already smooths over platform differences, but writing style still matters.

### 11.1 Never Assume Scheduling Order Is Identical

So do not depend on:

- one specific thread always getting execution first
- `Yield` always immediately giving control to another specific thread

Depend on:

- queue return codes
- `Close` / `CLOSED`
- explicit wait and exit protocols


### 11.2 "Integer to Pointer" in the Demos Is Demo-Only

In real cross-platform code, the steadier pattern is always:

- pass struct pointers
- make ownership explicit

That keeps pointer width and business data from being mixed together.


### 11.3 Do Not Push Addresses of Stack Locals into an Async Queue

For example:

```c
DemoJob tJob;
```

If `&tJob` is pushed into the queue and the producer returns, that address becomes invalid.

This kind of bug is hard to diagnose on both Windows and Linux.


## 12. Common Mistakes

### 12.1 Mistake 1: Treat the Queue as a Thread Replacement

A queue does not execute tasks. It only hands them off.


### 12.2 Mistake 2: Start with `MPMC` by Default

The wider the concurrency model, the higher the complexity and cost.


### 12.3 Mistake 3: See `ApproxCount == 0` and Declare the System Idle

It is only an observation value, not a strict synchronization result.


### 12.4 Mistake 4: Call `Destroy` Right After `Close`

If consumers are still running or leftovers are still waiting to be processed, shutdown becomes chaotic.


### 12.5 Mistake 5: Forget to Handle the Payload After `Push` Fails

A pointer queue will not release it for you.


## 13. What to Read Next

Recommended order:

1. [Queue API](../api/api-queue.en.md)
2. [Case: Multi-Producer Worker with Queue + Future](../case/queue-worker-future.md) (Chinese for now)
3. [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)
4. [Wait-Source Intro: Speak One Waiting Language for Futures and Network Events](wait-source-intro.en.md)
5. [xnet-v2 Stream Wait-Source Intro](xnet-stream-wait-source-intro.en.md)
6. [Case: Thread / Coroutine / Future Coordination](../case/thread-coroutine-future.en.md)

---

**One-sentence summary:** queues are not there to make concurrency look sophisticated; they are there to make handoff, backpressure, and shutdown explicit. In the current XRT mainline, choose the right model from `SPSC / MPSC / MPMC / xmpscqwait` first, and only then talk about batch operations and performance tuning.
