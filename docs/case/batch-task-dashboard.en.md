# XRT Case Study: Upgrade the Local Console Service into a Batch Task Dashboard

> This case is not about opening a few more threads. It targets a more realistic service problem: when one local console service needs to accept a whole batch, run multiple child tasks in parallel, and publish the final batch summary into JSON, HTML, logs, and a snapshot file, how do you give that chain one formal scope and one real completion boundary instead of gluing loose `future` objects and counters together?

[中文](batch-task-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have the previous local console service:

- it loads config
- it writes logs
- it runs background work through `queue + thread`
- it exposes both JSON and HTML

Now the requirements move one step further:

- `POST /api/batches` submits one whole batch
- one batch contains multiple independent items
- those items may run in parallel
- the batch must have one formal completion point
- after the batch finishes, its summary must be published into the page, JSON, and snapshot file

If you keep the old "push one job into the queue and let the worker do it" model, three problems appear quickly:

- it becomes unclear whether those items really belong to one logical scope
- timeout and cancellation degrade into hand-written pending counters
- the page, log, and snapshot all want to know when the whole batch is done, but nothing formal marks that boundary

So this page is not just another slow-task example. It is about:

> how one background job grows into a structured batch mainline once that job starts spawning multiple child tasks of its own.


## 2. Why `queue` Alone Is Not Enough

The key boundary here is easy to miss.

### 2.1 `queue` solves handoff, not batch scope

`queue` is still the right tool for:

- moving HTTP requests into the background quickly
- decoupling producers and consumers
- handling service shutdown with `Close / Drain`

But `queue` does not answer:

- whether twenty child tasks belong to one batch
- when that batch stops accepting new child tasks
- who waits for the whole batch
- how cancellation should propagate across that batch


### 2.2 `future` solves one result, not final batch convergence

`future` still matters because every item needs:

- one completion state
- one value or one failure

But a loose array of `xfuture*` still leaves you to invent:

- ownership of the batch
- one close-aware final barrier
- one place to propagate cancellation

That missing layer is exactly what `task group` provides.


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `config + path + file` | config, template, log, snapshot paths | puts the dashboard into the real filesystem |
| `logging` | batch started, timed out, finished | leaves stable operational events |
| `queue + thread` | receive the batch request from HTTP | keeps HTTP from blocking |
| `task group` | own all child tasks of one batch | `Close / Join / Cancel / Destroy` |
| `future` | carry item results and the group join point | one unified completion model |
| `xvalue + json` | store the latest batch summary | page, API, and snapshot share one model |
| `xhttpd + template` | trigger batches and display status | one browser and script-facing surface |

The mental model should be:

1. HTTP only accepts the batch
2. the worker only starts one batch scope
3. `task group` owns the child tasks inside that batch
4. JSON, HTML, logs, and snapshots consume only the final copied summary


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the snapshot to a batch-oriented one:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-batch.json
```

Where:

- `config/console.json` keeps the bind address, concurrency limit, and default batch size
- `web/dashboard.html` is still read and precompiled at startup
- `runtime/console.log` records batch lifecycle events
- `runtime/last-batch.json` stores the latest completed batch summary


## 5. Compact Worker-Side Batch Scope

The reviewed Chinese page contains the fully expanded explanation. The English page keeps the mainline compact and source-aligned.

This skeleton focuses on the most important question:

- once the worker receives one batch request, how should it use `task group` to own the whole batch formally?

It stays deliberately small:

- `main()` simulates one submitted batch
- `procRunBatch()` acts like the batch coordinator inside the worker
- each item runs through `xTaskGroupRunThread()`
- `xTaskGroupJoinFuture()` becomes the close-aware completion point for the whole batch

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iItemId;
	int32 iDelayMs;
	bool bShouldFail;
	bool bDone;
	int32 iStatus;
} DemoBatchItem;

typedef struct
{
	int32 iBatchId;
	int32 iItemCount;
	DemoBatchItem arrItem[8];
} DemoBatchRequest;

typedef struct
{
	int32 iLastBatchId;
	int32 iDoneCount;
	int32 iFailCount;
	bool bFinished;
} DemoBatchStats;

static int32 procBatchItemTask(ptr pArg, xfuture_result* pOut)
{
	DemoBatchItem* pItem = (DemoBatchItem*)pArg;

	if ( (pItem == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pItem->iDelayMs);
	pItem->bDone = TRUE;
	pItem->iStatus = pItem->bShouldFail ? XRT_NET_ERROR : XRT_NET_OK;

	pOut->pValue = pItem;
	pOut->iStatus = pItem->iStatus;
	return pItem->iStatus;
}

static bool procRunBatch(DemoBatchRequest* pReq, DemoBatchStats* pStats, int64 iTimeoutMs)
{
	xtaskgroup* pGroup = NULL;
	xfuture* arrFuture[8];
	xfuture* pJoin = NULL;
	int i;

	memset(arrFuture, 0, sizeof(arrFuture));
	memset(pStats, 0, sizeof(*pStats));

	if ( (pReq == NULL) || (pStats == NULL) ) {
		return FALSE;
	}
	if ( (pReq->iItemCount <= 0) || (pReq->iItemCount > 8) ) {
		return FALSE;
	}

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return FALSE;
	}

	for ( i = 0; i < pReq->iItemCount; i++ ) {
		arrFuture[i] = xTaskGroupRunThread(pGroup, procBatchItemTask, &pReq->arrItem[i], 0u);
		if ( arrFuture[i] == NULL ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);

	if ( !xFutureWaitTimeout(pJoin, iTimeoutMs) ) {
		printf("batch-%d timeout\n", (int)pReq->iBatchId);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pStats->iLastBatchId = pReq->iBatchId;
	pStats->bFinished = TRUE;

	for ( i = 0; i < pReq->iItemCount; i++ ) {
		DemoBatchItem* pItem = (DemoBatchItem*)xFutureValue(arrFuture[i]);

		if ( pItem == NULL ) {
			pStats->iFailCount++;
			continue;
		}

		if ( pItem->iStatus == XRT_NET_OK ) {
			pStats->iDoneCount++;
		} else {
			pStats->iFailCount++;
		}
	}

cleanup:
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < pReq->iItemCount; i++ ) {
		if ( arrFuture[i] != NULL ) {
			xFutureRelease(arrFuture[i]);
		}
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}

	return pStats->bFinished;
}

int main(void)
{
	DemoBatchRequest tReq;
	DemoBatchStats tStats;

	xrtInit();
	memset(&tReq, 0, sizeof(tReq));
	memset(&tStats, 0, sizeof(tStats));

	tReq.iBatchId = 101;
	tReq.iItemCount = 3;

	tReq.arrItem[0].iItemId = 1;
	tReq.arrItem[0].iDelayMs = 100;
	tReq.arrItem[0].bShouldFail = FALSE;

	tReq.arrItem[1].iItemId = 2;
	tReq.arrItem[1].iDelayMs = 150;
	tReq.arrItem[1].bShouldFail = TRUE;

	tReq.arrItem[2].iItemId = 3;
	tReq.arrItem[2].iDelayMs = 120;
	tReq.arrItem[2].bShouldFail = FALSE;

	if ( procRunBatch(&tReq, &tStats, 3000) ) {
		printf(
			"batch-%d finished: done=%d fail=%d\n",
			(int)tStats.iLastBatchId,
			(int)tStats.iDoneCount,
			(int)tStats.iFailCount
		);
	} else {
		printf("batch failed\n");
	}

	xrtUnit();
	return 0;
}
```


## 6. Five Things That Matter Most

### 6.1 The worker creates the `task group`

The stable placement is not:

- building a loose future array in the HTTP handler

It is:

- HTTP enqueues one batch request
- the worker receives that request
- the worker creates one `task group` for that batch

That keeps the batch scope attached to the background coordinator, not the request thread.


### 6.2 `JoinFuture` first, `Close` as the stop-growing declaration

The formal mainline here is:

1. spawn all child tasks
2. call `xTaskGroupJoinFuture(pGroup)`
3. call `xTaskGroupClose(pGroup)`

`Close` does not mean "finish immediately". It means:

> this batch scope will not accept any more child tasks from now on.


### 6.3 The timeout belongs to the whole batch

The key boundary is:

```c
if ( !xFutureWaitTimeout(pJoin, iTimeoutMs) ) {
	xTaskGroupCancel(pGroup);
	...
}
```

That does not ask whether one specific child finished. It asks whether:

- the batch, as one formal scope, converged before the deadline


### 6.4 `queue` still matters, but it does not own final convergence

In the real service, you should still keep:

- `POST /api/batches` -> `xmpscqwait`

because the HTTP handler still must not block on execution.

But after the worker pops the request, the real batch lifecycle becomes:

- create the `task group`
- spawn child tasks
- `Close`
- `Join`
- publish the copied summary


### 6.5 The dashboard should only read the copied summary

The page, JSON API, and snapshot do not want:

- live `xfuture*`
- one running group object
- one worker-side temporary array

They want a stable summary:

- batch id
- item count
- success count
- failure count
- completion time
- one latest summary line

So once the `task group` converges, the service should copy the summary immediately and export only that copied summary downstream.


## 7. How It Reconnects to HTTP, HTML, and Snapshots

With the worker-side coordinator in place, the service mainline becomes straightforward.

### 7.1 `POST /api/batches`

It should only:

1. parse the request body
2. build one `BatchRequest`
3. push it into `xmpscqwait`
4. return "accepted" immediately

It should not:

- spawn child tasks directly
- wait for the batch directly
- assemble the page summary inline


### 7.2 The worker thread

Once the worker receives a `BatchRequest`, it should:

1. log "batch started"
2. call `procRunBatch()`
3. build one `xvalue` summary object
4. update the in-memory latest dashboard model
5. write `runtime/last-batch.json`
6. log "batch finished"


### 7.3 `GET /api/batches/latest`

It should only export the latest copied summary as JSON.

The key point is not to recalculate the batch. It is to read:

- one stable summary that the worker already published after convergence


### 7.4 `GET /dashboard`

Keep the same boundary as the previous page:

- the template reads the latest copied summary model
- it does not inspect live child futures
- it does not perform batch statistics during rendering


## 8. Common Mistakes

### 8.1 Treating `queue` as the batch waiting model

`queue` only knows:

- one request arrived
- one request was popped

It does not know:

- how many child tasks grew inside that batch
- when the batch actually converged


### 8.2 Keeping only child futures and skipping the group join future

If you only keep:

- `arrFuture[i]`

the code quickly falls back to:

- manual counters
- manual "is the whole batch done yet?" checks

which is exactly the non-structured state this page is trying to replace.


### 8.3 Reading live batch internals from the page layer

The page and JSON outputs should only read:

- the copied summary

They should not walk:

- live child tasks
- a running group object
- worker-only temporary arrays


## 9. What To Read Next

If you want to push this further, the next natural directions are:

1. cache-oriented variants
2. multi-tenant batch indexes

If your `task group` mental model is still not stable yet, read these together:

- [Task Group Intro: From Unified Waiting to Structured Convergence](../guide/task-group-intro.en.md)
- [A Local Console Service with Config, Logging, Tasks, Networking, and Templates](local-console-service.en.md)
- [Upgrade the Local Console Service into a Subprocess Probe Dashboard](subprocess-probe-dashboard.en.md)
