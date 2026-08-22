# XRT Case Study: Queue + Future Multi-Producer Worker

> A stable worker mainline for "continuous task handoff, one serial worker, and one result handle per job": `xmpscqwait + promise/future + thread`.

[中文](queue-worker-future.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building a background computation worker with these constraints:

1. multiple producer threads keep submitting jobs
2. only one worker thread executes the real business logic
3. callers do not want to share one mutable result array
4. shutdown must clearly separate "stop accepting new jobs" from "finish queued jobs"

Without one clear mainline, this usually degrades into:

- producers directly mutating shared arrays
- the worker filling results back under locks
- no clear ownership of job objects and result objects
- no clear shutdown order for producers, the worker, and the queue

This case solves the exact problem of continuous handoff plus one formal result handle per job.

---

## 2. Why This Mainline

### Why not "shared array + mutex"?

Because the real problem here is not "several threads edit the same state".

The real questions are:

- how producers hand jobs to the worker
- how the worker returns one result to one caller
- how full, closed, and drained states are handled cleanly

A shared array can be made to run, but the boundaries stay blurry.

### Why not `task group`?

`task group` is a better fit for:

- one bounded batch of child tasks
- one fan-out
- one structured join

This case is different:

- producers keep submitting over time
- one long-lived worker keeps consuming
- every job completes through its own `future`

So the core issue is not "how one bounded batch converges", but:

- how jobs keep flowing into a worker
- how results come back one by one

### Why `xmpscqwait`?

Because the concurrency shape is very specific:

- multi-producer
- single-consumer
- the consumer should block when no job is available

That is exactly where `xmpscqwait` fits best.

---

## 3. What Each Layer Does

| Layer | Role in this case | What it really gives you |
|------|-------------------|--------------------------|
| producer thread | create jobs and result handles | submission point and caller context |
| `xmpscqwait` | hand jobs to the worker | FIFO handoff, backpressure, `Close` |
| worker thread | block-pop and execute | the real serial processing point |
| `xpromise` | complete one job from the worker side | `Resolve / Reject` |
| `xfuture` | stay with the caller side | one formal waitable result per job |

One-sentence version:

> `queue` owns handoff, the worker owns execution, and `promise/future` formally returns one result per submitted job.

---

## 4. Code Skeleton

The reviewed Chinese page contains the full version. The skeleton below keeps the same ownership and shutdown boundaries:

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iProducerId;
	int32 iJobId;
	int32 iInput;
	xpromise* pPromise;
} DemoQueueJob;

typedef struct
{
	int32 iProducerId;
	int32 iJobId;
	int32 iInput;
	int32 iOutput;
} DemoQueueResult;

typedef struct
{
	xmpscqwait hQueue;
} DemoWorkerCtx;

static void procRejectJob(DemoQueueJob* pJob, int32 iStatus, str sError)
{
	if ( pJob == NULL ) {
		return;
	}

	if ( pJob->pPromise != NULL ) {
		(void)xPromiseReject(pJob->pPromise, iStatus, sError);
		xPromiseDestroy(pJob->pPromise);
	}

	xrtFree(pJob);
}

static uint32 procQueueWorker(ptr pArg)
{
	DemoWorkerCtx* pCtx = (DemoWorkerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, 500u);
		if ( iRet == XQUEUE_OK ) {
			DemoQueueJob* pJob = (DemoQueueJob*)pItem;
			DemoQueueResult* pResult = (DemoQueueResult*)xrtMalloc(sizeof(DemoQueueResult));

			if ( pResult == NULL ) {
				(void)xPromiseReject(pJob->pPromise, XRT_NET_ERROR, (str)"alloc result failed");
			} else {
				memset(pResult, 0, sizeof(DemoQueueResult));
				pResult->iProducerId = pJob->iProducerId;
				pResult->iJobId = pJob->iJobId;
				pResult->iInput = pJob->iInput;
				pResult->iOutput = pJob->iInput * pJob->iInput;

				if ( !xPromiseResolve(pJob->pPromise, pResult) ) {
					xrtFree(pResult);
				}
			}

			xPromiseDestroy(pJob->pPromise);
			xrtFree(pJob);
			pItem = NULL;
			continue;
		}

		if ( iRet == XQUEUE_TIMEOUT ) {
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

static bool procSubmitJob(xmpscqwait hQueue, int32 iProducerId, int32 iJobId, int32 iInput, xfuture** ppFuture)
{
	xfuture* pFuture;
	xpromise* pPromise;
	DemoQueueJob* pJob;

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		return FALSE;
	}

	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return FALSE;
	}

	pJob = (DemoQueueJob*)xrtMalloc(sizeof(DemoQueueJob));
	if ( pJob == NULL ) {
		(void)xPromiseReject(pPromise, XRT_NET_ERROR, (str)"alloc job failed");
		xPromiseDestroy(pPromise);
		*ppFuture = pFuture;
		return FALSE;
	}

	memset(pJob, 0, sizeof(DemoQueueJob));
	pJob->iProducerId = iProducerId;
	pJob->iJobId = iJobId;
	pJob->iInput = iInput;
	pJob->pPromise = pPromise;

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQWaitTryPush(hQueue, pJob);
		if ( iRet == XQUEUE_OK ) {
			*ppFuture = pFuture;
			return TRUE;
		}
		if ( iRet == XQUEUE_FULL ) {
			xrtThreadYield();
			continue;
		}

		procRejectJob(pJob, XRT_NET_ERROR, (str)"queue closed before submit");
		*ppFuture = pFuture;
		return FALSE;
	}
}

static void procPrintFutureResult(xfuture* pFuture)
{
	DemoQueueResult* pResult;
	const char* sError;

	if ( pFuture == NULL ) {
		printf("future missing\n");
		return;
	}

	if ( !xFutureWaitTimeout(pFuture, 3000) ) {
		printf("future wait timeout\n");
		return;
	}

	if ( xFutureState(pFuture) != XFUTURE_RESOLVED ) {
		sError = xFutureError(pFuture);
		printf("job failed: status=%d error=%s\n", (int)xFutureStatus(pFuture), (sError != NULL) ? sError : "(null)");
		return;
	}

	pResult = (DemoQueueResult*)xFutureValue(pFuture);
	if ( pResult != NULL ) {
		printf("producer=%d job=%d input=%d output=%d\n", (int)pResult->iProducerId, (int)pResult->iJobId, (int)pResult->iInput, (int)pResult->iOutput);
		xrtFree(pResult);
	}
}
```

---

## 5. The 3 Most Important Boundaries

### 5.1 Handoff boundary

The producer does not call the business function directly. It:

1. creates a `DemoQueueJob`
2. creates `future + promise`
3. pushes `DemoQueueJob*` into `xmpscqwait`

That means:

- the producer owns submission
- the worker owns execution

### 5.2 Result boundary

The worker does not mutate a producer-owned shared array. It:

- calls `xPromiseResolve(...)` on success
- calls `xPromiseReject(...)` on failure

So every job gets one formal result handle:

- `xfuture`

### 5.3 Shutdown boundary

The stable shutdown order is:

1. wait until producers stop submitting
2. call `xrtMPSCQWaitClose(hQueue)` to stop new jobs
3. let the worker finish queued jobs
4. wait for the worker thread to exit
5. then release futures, the queue, and thread objects

That is exactly how "stop accepting jobs" and "finish queued jobs" stay separate.

---

## 6. How Ownership Works

### 6.1 Job object and promise

Before a successful push:

- `DemoQueueJob*`
- `xpromise*`

still belong to the producer side.

After a successful push:

- `DemoQueueJob*`
- `xpromise*`

formally move to the worker side.

That is why the rule is:

- if `Push` fails, the producer rejects and cleans up
- if `Push` succeeds, the worker resolves/rejects and destroys

### 6.2 Future

The `future` does not move together with the job object.

It stays with the submitter side, and the caller later finishes with:

- `xFutureRelease(...)`

That separation is one of the most important values of the `promise/future` design.

### 6.3 Result object

The sample uses a heap-allocated `DemoQueueResult`.

The worker only passes that pointer into the future. The final reader owns:

- `xFutureValue(...)`
- `xrtFree(...)`

If the worker resolved a stack address instead, it would become invalid immediately after the worker function moved on.

---

## 7. Boundary vs `task group`

This page is meant to contrast with [Thread, Coroutine, and Future Coordination](thread-coroutine-future.en.md):

- that page is about how one bounded batch converges structurally
- this page is about how jobs keep entering a worker and return results one by one

The practical rule is:

- bounded fan-out / structured scope => think `task group` first
- continuous handoff / command worker => think `queue + future` first

---

## 8. Common Mistakes

### Mistake 1: turning the result path back into "shared state + lock"

Then the formal `future` boundary becomes pointless.

### Mistake 2: destroying the queue immediately after `Close`

`Close` only means:

- no new jobs are accepted

It does not mean:

- the queue is already drained

### Mistake 3: forgetting to reject the promise when `Push` fails

Then the caller sees a forever-pending future and assumes the worker is stuck.

### Mistake 4: resolving a stack-allocated result object

That pointer becomes invalid as soon as execution leaves that stack frame.

### Mistake 5: treating `xmpscqwait` as a blocking MPMC queue

It is still:

- multi-producer
- single-consumer

with an additional blocking pop path.

---

## 9. Suggested Reading

- [Guide: When to Use Queue Instead of Shared State](../guide/queue-intro.en.md)
- [Guide: Coroutine, Future, and Task Intro](../guide/coroutine-future-task-intro.en.md)
- [Guide: Task Group and Structured Convergence](../guide/task-group-intro.en.md)
- [Queue API](../api/api-queue.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Case: Thread, Coroutine, and Future Coordination](thread-coroutine-future.en.md)

---

**One-sentence summary:** the key is not "how many threads you started"; the key is that producers hand jobs to the worker through a queue, the worker completes one promise per job, and each caller receives one formal future result back.
