# XRT Case Study: Upgrade the Local Console Service into a Retry Backoff Dashboard

> This case is not about "just try once more after failure". It targets a more realistic service problem: when one local console service needs to move failed tasks into a background retry lane, keep the next retry time as formal state, expose recent attempt history on the dashboard, and keep the page, JSON, logs, and snapshots aligned to the same model, how do you turn `dict + list + queue + thread + time + hash + xhttpd + template` into one formal mainline instead of letting failures, retries, backoff, and rendering each invent their own version of the state?

[中文](retry-backoff-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have the earlier local console service:

- it loads config
- it writes logs
- it runs background work through `queue + thread`
- it exposes both JSON and HTML

Now the requirements move one step further into failure handling:

- one task may fail on the first attempt
- failure must not turn into an immediate infinite loop
- the next retry time must be computable and observable
- the page must show both current pending retry slots and recent attempt history
- the latest retry state must also be written into a snapshot file

If you keep the logic at:

- "sleep and retry in place"
- or one HTTP handler that loops until success

four problems appear very quickly:

- the request thread gets pinned by the retry chain
- current task state and historical attempts collapse into one unclear structure
- the page starts touching worker-local temporary state just to show "when will this run again"
- logs, JSON, HTML, and snapshots all build slightly different fields for the same retry state

So this page is not really about "how to write a retry loop". It is about:

> how to split the current retry slot, the recent attempt history, and the background backoff queue into three stable lines once failed work enters a long-lived retry lane.


## 2. Why This Is Not a `task group`

### 2.1 The problem here is not one bounded batch

The earlier batch page solved:

- submit one whole batch
- give that batch one formal convergence point

That is:

- one bounded batch scope

This page is solving something different:

- after one task fails, when should it re-enter execution
- that failure chain may last for multiple rounds
- the page cares more about "which round is this lane in" and "when is the next retry"

So this is no longer:

- "when does one batch join?"

It is:

- "what state is this retry lane in right now?"


### 2.2 `queue + thread + time` is the more natural mainline

The core here is not a set of child futures. It is:

- one long-lived worker
- one re-entrant retry queue
- one current slot per task
- one recent timeline of attempts

That is why the more natural mainline is:

- `queue + thread + time`

not:

- `task group + join future`


### 2.3 `dict` and `list` deliberately separate current state from history

In this page:

- `dict`
	- stores the current retry slot for each task
- `list`
	- stores the recent attempt history

Once those two layers are separated, the page and JSON output no longer need to reconstruct one timeline out of worker-local temporary state.


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `dict` | stores the current `task_key -> retry slot` state | which round the task is in, when it retries next, whether it is still pending |
| `list` | stores recent attempt history | dashboard timeline and recent-event export |
| `queue + thread` | executes and requeues retry jobs in the background | keeps HTTP off the failure chain |
| `time` | computes the next retry instant | backoff and next execution window |
| `hash` | gives the task key one stable fingerprint | short identifiers in pages, logs, and snapshots |
| `xvalue + json` | exports the current retry summary | JSON and snapshots share one model |
| `xhttpd + template` | exposes the retry dashboard | one browser and script-facing entry |  

One-sentence version:

> `dict` owns "what state this retry lane is in now", `list` owns "what attempts happened recently", and the worker owns "when the next round re-enters the queue".


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the snapshot meaning into a retry dashboard:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-retry.json
```

Where:

- `config/console.json`
	- keeps the default maximum retry count and base backoff duration
- `web/dashboard.html`
	- renders the retry dashboard
- `runtime/console.log`
	- records submission, failure, backoff, and final success/failure
- `runtime/last-retry.json`
	- stores the latest exported retry summary


## 5. Compact Retry Slot, History Line, and Background Backoff Queue

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the three boundaries that matter most:

1. `dict` stores the current retry slot
2. `list` stores the recent attempt history
3. `queue + thread + time` owns the background backoff retry path

It deliberately stays practical:

- initialize the retry center
- submit two tasks
- let one task fail twice and succeed on the third attempt
- let the worker compute the next retry time and requeue the next round
- print the current retry slots and recent retry history from the main thread

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iSlotId;
	int32 iLastAttempt;
	int32 iFailCount;
	int32 iSuccessCount;
	xtime tNextRetryAt;
	uint64 iTaskHash;
	bool bPending;
	char sTaskKey[64];
	char sLastStatus[24];
} DemoRetrySlot;

typedef struct
{
	char sTaskKey[64];
	int32 iAttempt;
	int32 iStatus;
	int32 iBackoffMs;
	xtime tWhen;
} DemoRetryLog;

typedef struct
{
	char sTaskKey[64];
	int32 iAttempt;
	int32 iMaxRetry;
	int32 iSuccessAttempt;
	int32 iBaseBackoffMs;
	xtime tNextRunAt;
} DemoRetryJob;

typedef struct
{
	xdict pSlots;
	xlist pLogs;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextSlotId;
	int64 iNextLogSeq;
} DemoRetryCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static DemoRetrySlot* procFindRetrySlotLocked(DemoRetryCenter* pCenter, const char* sTaskKey)
{
	if ( (pCenter == NULL) || (sTaskKey == NULL) ) {
		return NULL;
	}

	return (DemoRetrySlot*)xrtDictGetPtr(pCenter->pSlots, (ptr)sTaskKey, (uint32)strlen(sTaskKey));
}

static DemoRetrySlot* procEnsureRetrySlotLocked(DemoRetryCenter* pCenter, const char* sTaskKey)
{
	DemoRetrySlot* pSlot = NULL;

	pSlot = procFindRetrySlotLocked(pCenter, sTaskKey);
	if ( pSlot != NULL ) {
		return pSlot;
	}

	pSlot = (DemoRetrySlot*)xrtMalloc(sizeof(DemoRetrySlot));
	if ( pSlot == NULL ) {
		return NULL;
	}

	memset(pSlot, 0, sizeof(*pSlot));
	pCenter->iNextSlotId++;
	pSlot->iSlotId = pCenter->iNextSlotId;
	pSlot->iTaskHash = xrtHash64((ptr)sTaskKey, strlen(sTaskKey));
	procCopyText(pSlot->sTaskKey, sizeof(pSlot->sTaskKey), sTaskKey);
	procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "queued");

	if ( !xrtDictSetPtr(pCenter->pSlots, (ptr)pSlot->sTaskKey, (uint32)strlen(pSlot->sTaskKey), pSlot, NULL) ) {
		xrtFree(pSlot);
		return NULL;
	}

	return pSlot;
}

static bool procAppendRetryLogLocked(DemoRetryCenter* pCenter, const char* sTaskKey, int32 iAttempt, int32 iStatus, int32 iBackoffMs)
{
	DemoRetryLog* pLog = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (sTaskKey == NULL) ) {
		return FALSE;
	}

	pCenter->iNextLogSeq++;
	iSeq = pCenter->iNextLogSeq;
	pLog = (DemoRetryLog*)xrtListSet(pCenter->pLogs, iSeq, &bNew);
	if ( pLog == NULL ) {
		return FALSE;
	}

	memset(pLog, 0, sizeof(*pLog));
	procCopyText(pLog->sTaskKey, sizeof(pLog->sTaskKey), sTaskKey);
	pLog->iAttempt = iAttempt;
	pLog->iStatus = iStatus;
	pLog->iBackoffMs = iBackoffMs;
	pLog->tWhen = xrtNow();

	if ( iSeq > 12 ) {
		(void)xrtListRemove(pCenter->pLogs, iSeq - 12);
	}

	return TRUE;
}

static bool procRetryCenterInit(DemoRetryCenter* pCenter)
{
	xqueue_config tCfg;

	memset(pCenter, 0, sizeof(*pCenter));

	pCenter->pSlots = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pCenter->pSlots == NULL ) {
		return FALSE;
	}

	pCenter->pLogs = xrtListCreate(sizeof(DemoRetryLog), XRT_OBJMODE_SHARED);
	if ( pCenter->pLogs == NULL ) {
		xrtDictDestroy(pCenter->pSlots);
		pCenter->pSlots = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pLogs);
		xrtDictDestroy(pCenter->pSlots);
		pCenter->pLogs = NULL;
		pCenter->pSlots = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pLogs);
		xrtDictDestroy(pCenter->pSlots);
		pCenter->hMutex = NULL;
		pCenter->pLogs = NULL;
		pCenter->pSlots = NULL;
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitRetry(DemoRetryCenter* pCenter, const char* sTaskKey, int32 iMaxRetry, int32 iSuccessAttempt, int32 iBaseBackoffMs)
{
	DemoRetryJob* pJob = NULL;
	DemoRetrySlot* pSlot = NULL;

	if ( (pCenter == NULL) || (sTaskKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoRetryJob*)xrtMalloc(sizeof(DemoRetryJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sTaskKey, sizeof(pJob->sTaskKey), sTaskKey);
	pJob->iAttempt = 1;
	pJob->iMaxRetry = iMaxRetry;
	pJob->iSuccessAttempt = iSuccessAttempt;
	pJob->iBaseBackoffMs = iBaseBackoffMs;
	pJob->tNextRunAt = xrtNow();

	xrtMutexLock(pCenter->hMutex);
	pSlot = procEnsureRetrySlotLocked(pCenter, sTaskKey);
	if ( pSlot != NULL ) {
		pSlot->bPending = TRUE;
		pSlot->tNextRetryAt = pJob->tNextRunAt;
		procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "queued");
	}
	xrtMutexUnlock(pCenter->hMutex);

	if ( pSlot == NULL ) {
		xrtFree(pJob);
		return FALSE;
	}

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procRetryWorker(ptr pArg)
{
	DemoRetryCenter* pCenter = (DemoRetryCenter*)pArg;
	DemoRetryJob* pJob = NULL;
	DemoRetryJob* pNextJob = NULL;
	DemoRetrySlot* pSlot = NULL;
	xqueue_result iRet;
	xtime tNow;
	int32 iBackoffMs;
	bool bSuccess;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		tNow = xrtNow();
		if ( pJob->tNextRunAt > tNow ) {
			xrtSleep((uint32)(pJob->tNextRunAt - tNow));
		}

		bSuccess = (pJob->iAttempt >= pJob->iSuccessAttempt);
		iBackoffMs = pJob->iBaseBackoffMs * pJob->iAttempt;

		xrtMutexLock(pCenter->hMutex);
		pSlot = procEnsureRetrySlotLocked(pCenter, pJob->sTaskKey);
		if ( pSlot != NULL ) {
			pSlot->iLastAttempt = pJob->iAttempt;
		}

		if ( bSuccess ) {
			if ( pSlot != NULL ) {
				pSlot->iSuccessCount++;
				pSlot->bPending = FALSE;
				pSlot->tNextRetryAt = 0;
				procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "success");
			}
			(void)procAppendRetryLogLocked(pCenter, pJob->sTaskKey, pJob->iAttempt, XRT_NET_OK, 0);
			xrtMutexUnlock(pCenter->hMutex);
			xrtFree(pJob);
			continue;
		}

		if ( pSlot != NULL ) {
			pSlot->iFailCount++;
		}

		if ( pJob->iAttempt < pJob->iMaxRetry ) {
			if ( pSlot != NULL ) {
				pSlot->bPending = TRUE;
				pSlot->tNextRetryAt = xrtNow() + iBackoffMs;
				procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "retrying");
			}
			(void)procAppendRetryLogLocked(pCenter, pJob->sTaskKey, pJob->iAttempt, XRT_NET_ERROR, iBackoffMs);
			xrtMutexUnlock(pCenter->hMutex);

			pNextJob = (DemoRetryJob*)xrtMalloc(sizeof(DemoRetryJob));
			if ( pNextJob != NULL ) {
				memcpy(pNextJob, pJob, sizeof(*pNextJob));
				pNextJob->iAttempt++;
				pNextJob->tNextRunAt = xrtNow() + iBackoffMs;

				if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pNextJob) != XQUEUE_OK ) {
					xrtFree(pNextJob);
				}
			}

			xrtFree(pJob);
			continue;
		}

		if ( pSlot != NULL ) {
			pSlot->bPending = FALSE;
			pSlot->tNextRetryAt = 0;
			procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "failed");
		}
		(void)procAppendRetryLogLocked(pCenter, pJob->sTaskKey, pJob->iAttempt, XRT_NET_ERROR, 0);
		xrtMutexUnlock(pCenter->hMutex);

		xrtFree(pJob);
	}

	return 0u;
}

static bool procDumpRetrySlot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoRetrySlot* pSlot = *(DemoRetrySlot**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pSlot != NULL ) {
		printf(
			"slot key=%s attempt=%d fail=%d success=%d pending=%d next_retry=%lld hash=%llu status=%s\n",
			pSlot->sTaskKey,
			(int)pSlot->iLastAttempt,
			(int)pSlot->iFailCount,
			(int)pSlot->iSuccessCount,
			(int)pSlot->bPending,
			(long long)pSlot->tNextRetryAt,
			(unsigned long long)pSlot->iTaskHash,
			pSlot->sLastStatus
		);
	}

	return FALSE;
}

static bool procDumpRetryLog(int64 iKey, ptr pVal, ptr pArg)
{
	DemoRetryLog* pLog = (DemoRetryLog*)pVal;
	(void)pArg;

	printf(
		"log seq=%lld key=%s attempt=%d status=%d backoff=%d when=%lld\n",
		(long long)iKey,
		pLog->sTaskKey,
		(int)pLog->iAttempt,
		(int)pLog->iStatus,
		(int)pLog->iBackoffMs,
		(long long)pLog->tWhen
	);
	return FALSE;
}

static bool procFreeRetrySlot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoRetrySlot* pSlot = *(DemoRetrySlot**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pSlot != NULL ) {
		xrtFree(pSlot);
	}
	return FALSE;
}

static void procRetryCenterUnit(DemoRetryCenter* pCenter)
{
	if ( pCenter == NULL ) {
		return;
	}

	if ( pCenter->hQueue != NULL ) {
		xrtMPSCQWaitClose(pCenter->hQueue);
	}
	if ( pCenter->hWorker != NULL ) {
		xrtThreadWait(pCenter->hWorker);
		xrtThreadDestroy(pCenter->hWorker);
		pCenter->hWorker = NULL;
	}
	if ( pCenter->hQueue != NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		pCenter->hQueue = NULL;
	}

	if ( (pCenter->hMutex != NULL) && (pCenter->pSlots != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pSlots, procFreeRetrySlot, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}

	if ( pCenter->hMutex != NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		pCenter->hMutex = NULL;
	}
	if ( pCenter->pLogs != NULL ) {
		xrtListDestroy(pCenter->pLogs);
		pCenter->pLogs = NULL;
	}
	if ( pCenter->pSlots != NULL ) {
		xrtDictDestroy(pCenter->pSlots);
		pCenter->pSlots = NULL;
	}
}

int main(void)
{
	DemoRetryCenter tCenter;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procRetryCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procRetryWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procRetryCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procSubmitRetry(&tCenter, "probe:alpha", 4, 3, 80) ) {
		goto cleanup;
	}
	if ( !procSubmitRetry(&tCenter, "probe:beta", 3, 2, 60) ) {
		goto cleanup;
	}

	xrtSleep(900u);

	xrtMutexLock(tCenter.hMutex);
	printf("== current retry slots ==\n");
	xrtDictWalk(tCenter.pSlots, procDumpRetrySlot, NULL);

	printf("== recent retry logs ==\n");
	xrtListWalk(tCenter.pLogs, procDumpRetryLog, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	iRet = 0;

cleanup:
	procRetryCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. Five Things That Matter Most

### 6.1 `dict` stores the current retry slot, not the history

`pSlots` stores:

- which round this `task_key` is currently in
- whether it is still pending
- when it retries next

It is not responsible for all past attempts.


### 6.2 `list` stores stable attempt history, not live retry objects

`pLogs` uses:

- `xrtListCreate(sizeof(DemoRetryLog), XRT_OBJMODE_SHARED)`

That means:

- history records are copied into the list by value

So the page and snapshots consume stable history, not worker-local temporary objects.


### 6.3 The retry lane is one long-lived worker, not one bounded batch

This worker is not:

- one set of tasks that converges once and disappears

It is:

- a worker that keeps receiving failed tasks
- computing the next backoff instant
- and pushing the next round back into the queue

That is why this mainline fits:

- `queue + thread + time`

more naturally than:

- `task group`


### 6.4 The next retry time is formal state, not one hidden sleep

Once `tNextRetryAt` is a real slot field, it becomes available to:

- the page
- the JSON output
- the snapshot

That is much closer to a real service than hiding the timing inside one worker-local `sleep`.


### 6.5 `hash` gives the task key one stable lightweight identifier

`iTaskHash` is useful here for:

- short identifiers on the page and in logs

It is not:

- a security primitive
- an authorization mechanism


## 7. How It Reconnects to HTTP, Templates, and Snapshots

### 7.1 `POST /api/retry`

It should only:

1. parse the task key
2. enqueue one retry job
3. return "accepted" immediately

The handler should not run the retry loop inline.


### 7.2 `GET /api/retry`

The most natural JSON output is two parts:

1. the current retry slots
2. the recent attempt history

These map directly onto:

- `dict`
- `list`


### 7.3 `GET /dashboard`

The page naturally splits into:

1. a table of current retry lanes
2. a timeline of recent attempts

That is also why those two structures should be split from the beginning.


### 7.4 `runtime/last-retry.json`

Whenever the worker updates the slot state, it should export:

- the current retry slot summary
- the recent attempt history

into one shared `xvalue`, then write that unified model into the snapshot file so the page, JSON, and logs all stay aligned.


## 8. Common Mistakes

### 8.1 Running `while retry` directly on the HTTP thread

This is the most common and worst regression point.

As soon as one task keeps failing, the request thread is pinned.


### 8.2 Using one structure to express both current state and history

That makes it much harder to display:

- the next retry time
- the recent timeline

in a stable way.


### 8.3 Keeping the next retry time only in a local variable

If `next_retry_at` is not one formal state field, the page and snapshot cannot reliably answer:

- when will this task run again?


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. the archive dashboard
2. hot-cold cache tiering
3. retry plus subprocess probing

If the state split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Cache Refresh Dashboard](cache-refresh-dashboard.en.md)
- [Upgrade the Local Console Service into a Batch Task Dashboard](batch-task-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
