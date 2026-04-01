# XRT Case Study: Upgrade the Local Console Service into a Cache Refresh Dashboard

> This case is not about creating one hash table and pushing values into it. It targets a more realistic service problem: when one local console service must expose cache reads to scripts and browsers, move slow refresh work into the background, and show a stable refresh history on the dashboard, how do you turn `dict + list + hash + queue + thread + xhttpd + template` into one formal mainline instead of letting cache hits, refreshes, logs, and pages each build their own version of the state?

[中文](cache-refresh-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have the previous local console service:

- it loads config
- it writes logs
- it runs background work through `queue + thread`
- it exposes both JSON and HTML

Now the service needs one more very common role:

- `GET /api/cache?key=...` reads one cache item
- cache hits should stay fast
- expiration or forced refresh must not block the HTTP handler
- the dashboard must show both current cache contents and recent refresh history
- the latest cache summary must also be written into a snapshot file

If you only build "one dict and one synchronous refresh when expired", four problems appear quickly:

- the request thread gets blocked by slow refresh work
- current cache objects and refresh history collapse into one unclear structure
- the page layer starts walking live cache objects just to build a timeline
- logs, JSON, HTML, and snapshots all invent different fields for the same thing

So this page is not really about "how to build a cache". It is about:

> how to split current state and refresh history into two stable data lines, then connect them with one background refresh mainline.


## 2. Why This Is Not Just One `dict`

### 2.1 `dict` owns current values, not the timeline

`dict` is the natural tool for:

- `key -> current item`
- fast lookups
- finding the current cached object directly by key

But it does not naturally express:

- the most recent refresh
- the second most recent refresh
- the third most recent refresh

because that is no longer just "look up the current value by text key". It is:

- keep one ordered recent history

So this page deliberately splits the model:

- `dict`
	- current cache items
- `list`
	- recent refresh history


### 2.2 Synchronous refresh breaks the point of a cache dashboard

If `GET /api/cache` refreshes inline once an item expires:

- the browser blocks
- scripts block as well
- if refresh logic later grows into file, network, or subprocess work, the request thread gets pinned immediately

That is exactly why:

- `queue + thread`

must enter the mainline.

The stable model is:

- HTTP only decides whether to enqueue a refresh
- the background thread performs the refresh
- the page and JSON outputs consume the copied summary after refresh


### 2.3 `hash` is a content fingerprint here, not a security feature

One common dashboard requirement is:

- the page wants to know whether the value actually changed
- the log wants one short identifier for the refreshed content

The natural tool for that is:

- `xrtHash64()`

Its role here is:

- stable content fingerprint

not:

- authentication
- signatures
- secure comparison


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `dict` | stores `key -> current item` | stable lookup for the current cache value |
| `list` | stores recent refresh history | dashboard timeline and recent-event export |
| `hash` | fingerprints the cached value | tells whether content changed |
| `queue + thread` | performs slow refresh work in the background | keeps HTTP from blocking |
| `logging` | records hits, enqueue events, refresh completion | makes cache behavior traceable |
| `xvalue + json` | exports the latest cache summary | JSON and snapshot share one model |
| `xhttpd + template` | exposes the dashboard surface | one browser and script-facing entry |

One-sentence version:

> `dict` owns "what the cache is now", `list` owns "what happened recently", and the background thread owns "when slow refresh work runs".


## 4. File Layout and Outputs

This case keeps the local-console-service layout and only changes the snapshot meaning:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-cache.json
```

Where:

- `config/console.json`
	- keeps default TTL and refresh queue capacity
- `web/dashboard.html`
	- renders the cache dashboard
- `runtime/console.log`
	- records refresh enqueue, completion, and failure
- `runtime/last-cache.json`
	- stores the latest exported cache summary


## 5. Compact Cache Core with Refresh Thread and Recent History

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton keeps the three boundaries that matter most:

1. `dict` stores current cache objects
2. `list` stores recent refresh history
3. `queue + thread` moves slow refresh work into the background

It deliberately stays practical:

- initialize the cache store
- submit several refresh requests
- let the worker build values and hashes
- read one current cache object from the main thread
- print the current cache line and recent refresh history separately

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iEntryId;
	xtime tExpireAt;
	uint64 iValueHash;
	int32 iHitCount;
	bool bRefreshing;
	char sKey[64];
	char sValue[96];
} DemoCacheItem;

typedef struct
{
	char sKey[64];
	int32 iDelayMs;
} DemoRefreshJob;

typedef struct
{
	char sKey[64];
	uint64 iValueHash;
	int32 iStatus;
	xtime tUpdatedAt;
} DemoRefreshLog;

typedef struct
{
	xdict pByKey;
	xlist pRefreshLog;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextEntryId;
	int64 iNextLogSeq;
	int64 iTTL;
} DemoCacheStore;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static DemoCacheItem* procFindCacheItemLocked(DemoCacheStore* pStore, const char* sKey)
{
	if ( (pStore == NULL) || (sKey == NULL) ) {
		return NULL;
	}

	return (DemoCacheItem*)xrtDictGetPtr(pStore->pByKey, (ptr)sKey, (uint32)strlen(sKey));
}

static bool procAppendRefreshLogLocked(DemoCacheStore* pStore, const char* sKey, uint64 iHash, int32 iStatus)
{
	DemoRefreshLog* pLog = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pStore == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pStore->iNextLogSeq++;
	iSeq = pStore->iNextLogSeq;
	pLog = (DemoRefreshLog*)xrtListSet(pStore->pRefreshLog, iSeq, &bNew);
	if ( pLog == NULL ) {
		return FALSE;
	}

	memset(pLog, 0, sizeof(*pLog));
	procCopyText(pLog->sKey, sizeof(pLog->sKey), sKey);
	pLog->iValueHash = iHash;
	pLog->iStatus = iStatus;
	pLog->tUpdatedAt = xrtNow();

	if ( iSeq > 8 ) {
		(void)xrtListRemove(pStore->pRefreshLog, iSeq - 8);
	}

	return TRUE;
}

static bool procUpsertCacheItemLocked(DemoCacheStore* pStore, const char* sKey, const char* sValue)
{
	DemoCacheItem* pItem = NULL;
	uint64 iHash;

	if ( (pStore == NULL) || (sKey == NULL) || (sValue == NULL) ) {
		return FALSE;
	}

	pItem = procFindCacheItemLocked(pStore, sKey);
	if ( pItem == NULL ) {
		pItem = (DemoCacheItem*)xrtMalloc(sizeof(DemoCacheItem));
		if ( pItem == NULL ) {
			return FALSE;
		}

		memset(pItem, 0, sizeof(*pItem));
		pStore->iNextEntryId++;
		pItem->iEntryId = pStore->iNextEntryId;
		procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);

		if ( !xrtDictSetPtr(pStore->pByKey, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
			xrtFree(pItem);
			return FALSE;
		}
	}

	iHash = xrtHash64((ptr)sValue, strlen(sValue));
	procCopyText(pItem->sValue, sizeof(pItem->sValue), sValue);
	pItem->iValueHash = iHash;
	pItem->tExpireAt = xrtNow() + pStore->iTTL;
	pItem->bRefreshing = FALSE;

	return procAppendRefreshLogLocked(pStore, sKey, iHash, XRT_NET_OK);
}

static uint32 procRefreshWorker(ptr pArg)
{
	DemoCacheStore* pStore = (DemoCacheStore*)pArg;
	DemoRefreshJob* pJob = NULL;
	xqueue_result iRet;
	char sValue[96];

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pStore->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		xrtSleep((pJob->iDelayMs > 0) ? (uint32)pJob->iDelayMs : 50u);
		snprintf(
			sValue,
			sizeof(sValue),
			"value<%s>#%lld",
			pJob->sKey,
			(long long)xrtNow()
		);

		xrtMutexLock(pStore->hMutex);
		(void)procUpsertCacheItemLocked(pStore, pJob->sKey, sValue);
		xrtMutexUnlock(pStore->hMutex);

		xrtFree(pJob);
	}

	return 0u;
}

static bool procCacheStoreInit(DemoCacheStore* pStore)
{
	xqueue_config tCfg;

	memset(pStore, 0, sizeof(*pStore));
	pStore->iTTL = 3000;

	pStore->pByKey = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pStore->pByKey == NULL ) {
		return FALSE;
	}

	pStore->pRefreshLog = xrtListCreate(sizeof(DemoRefreshLog), XRT_OBJMODE_SHARED);
	if ( pStore->pRefreshLog == NULL ) {
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	pStore->hMutex = xrtMutexCreate();
	if ( pStore->hMutex == NULL ) {
		xrtListDestroy(pStore->pRefreshLog);
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pStore->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pStore->hQueue == NULL ) {
		xrtMutexDestroy(pStore->hMutex);
		xrtListDestroy(pStore->pRefreshLog);
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	pStore->hWorker = xrtThreadCreate((ptr)procRefreshWorker, pStore, 0u);
	if ( pStore->hWorker == NULL ) {
		xrtMPSCQWaitDestroy(pStore->hQueue);
		xrtMutexDestroy(pStore->hMutex);
		xrtListDestroy(pStore->pRefreshLog);
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitRefresh(DemoCacheStore* pStore, const char* sKey, int32 iDelayMs)
{
	DemoRefreshJob* pJob = NULL;

	if ( (pStore == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoRefreshJob*)xrtMalloc(sizeof(DemoRefreshJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	pJob->iDelayMs = iDelayMs;

	if ( xrtMPSCQWaitTryPush(pStore->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static bool procDumpCacheItem(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoCacheItem* pItem = *(DemoCacheItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		printf(
			"cache key=%s value=%s hits=%d hash=%llu expire_at=%lld\n",
			pItem->sKey,
			pItem->sValue,
			(int)pItem->iHitCount,
			(unsigned long long)pItem->iValueHash,
			(long long)pItem->tExpireAt
		);
	}

	return FALSE;
}

static bool procDumpRefreshLog(int64 iKey, ptr pVal, ptr pArg)
{
	DemoRefreshLog* pLog = (DemoRefreshLog*)pVal;
	(void)pArg;

	printf(
		"log seq=%lld key=%s status=%d hash=%llu updated_at=%lld\n",
		(long long)iKey,
		pLog->sKey,
		(int)pLog->iStatus,
		(unsigned long long)pLog->iValueHash,
		(long long)pLog->tUpdatedAt
	);
	return FALSE;
}

static bool procFreeCacheItem(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoCacheItem* pItem = *(DemoCacheItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		xrtFree(pItem);
	}
	return FALSE;
}

static void procCacheStoreUnit(DemoCacheStore* pStore)
{
	if ( pStore == NULL ) {
		return;
	}

	if ( pStore->hQueue != NULL ) {
		xrtMPSCQWaitClose(pStore->hQueue);
	}
	if ( pStore->hWorker != NULL ) {
		xrtThreadWait(pStore->hWorker);
		xrtThreadDestroy(pStore->hWorker);
	}
	if ( pStore->hQueue != NULL ) {
		xrtMPSCQWaitDestroy(pStore->hQueue);
	}

	if ( (pStore->hMutex != NULL) && (pStore->pByKey != NULL) ) {
		xrtMutexLock(pStore->hMutex);
		xrtDictWalk(pStore->pByKey, procFreeCacheItem, NULL);
		xrtMutexUnlock(pStore->hMutex);
	}

	if ( pStore->hMutex != NULL ) {
		xrtMutexDestroy(pStore->hMutex);
	}
	if ( pStore->pRefreshLog != NULL ) {
		xrtListDestroy(pStore->pRefreshLog);
	}
	if ( pStore->pByKey != NULL ) {
		xrtDictDestroy(pStore->pByKey);
	}
}

int main(void)
{
	DemoCacheStore tStore;
	DemoCacheItem* pItem = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procCacheStoreInit(&tStore) ) {
		xrtUnit();
		return 1;
	}

	if ( !procSubmitRefresh(&tStore, "user:1001", 80) ) {
		goto cleanup;
	}
	if ( !procSubmitRefresh(&tStore, "user:1002", 120) ) {
		goto cleanup;
	}
	if ( !procSubmitRefresh(&tStore, "user:1001", 60) ) {
		goto cleanup;
	}

	xrtSleep(500u);

	xrtMutexLock(tStore.hMutex);
	pItem = procFindCacheItemLocked(&tStore, "user:1001");
	if ( pItem != NULL ) {
		pItem->iHitCount++;
		printf("read key=user:1001 value=%s hash=%llu\n", pItem->sValue, (unsigned long long)pItem->iValueHash);
	}

	printf("== current cache ==\n");
	xrtDictWalk(tStore.pByKey, procDumpCacheItem, NULL);

	printf("== recent refresh log ==\n");
	xrtListWalk(tStore.pRefreshLog, procDumpRefreshLog, NULL);
	xrtMutexUnlock(tStore.hMutex);

	iRet = 0;

cleanup:
	procCacheStoreUnit(&tStore);
	xrtUnit();
	return iRet;
}
```


## 6. Five Things That Matter Most

### 6.1 `dict` and `list` are not the same layer

This page intentionally keeps:

- `dict`
	- current cache objects
- `list`
	- recent refresh history

That means "the current value" and "what happened recently" are not squeezed out of one unclear structure.


### 6.2 The background thread refreshes, the main thread reads stable state

In this mainline:

- `procSubmitRefresh()`
	- only enqueues refresh work
- `procRefreshWorker()`
	- actually generates the new value and writes it back

So the main thread and the HTTP handlers stay in the safer role:

- read cache state
- enqueue refresh work

rather than executing the slow refresh path directly.


### 6.3 `hash` turns "did the value change?" into one stable field

Once you keep:

- `iValueHash`

the page, log, and snapshot all gain one shared way to say:

- whether this refresh produced new content

without each layer comparing the full value text again.


### 6.4 The `list` stores value snapshots, not live object ownership

`pRefreshLog` uses:

- `xrtListCreate(sizeof(DemoRefreshLog), XRT_OBJMODE_SHARED)`

So the history line holds concrete value snapshots, not pointers to external objects that still need to stay alive.


### 6.5 The page and JSON should export one copied cache summary

The skeleton prints:

- current cache items
- recent refresh history

The next stable production step is:

1. the worker builds one `xvalue` summary after refresh
2. `GET /api/cache`
	- reads the current cache summary
3. `GET /dashboard`
	- renders the same summary through the template
4. `runtime/last-cache.json`
	- writes the latest snapshot of that summary


## 7. How It Reconnects to HTTP, Templates, and Snapshots

### 7.1 `GET /api/cache?key=...`

The stable approach is:

- only read the current cache object
- if it is expired
	- enqueue a refresh
	- return the old value or a "refreshing" state immediately

The handler should not:

- wait for the refresh directly
- sleep directly
- rebuild refresh history inline


### 7.2 `POST /api/cache/refresh`

It should only:

1. parse the key
2. enqueue one `DemoRefreshJob`
3. return "accepted" immediately

The HTTP thread should not write the cache object directly.


### 7.3 `GET /dashboard`

The page naturally splits into two parts:

1. current cache table
2. recent refresh history

Those map directly onto:

- `dict`
- `list`

That is exactly why the page does not need to synthesize a timeline out of live cache objects during rendering.


### 7.4 `runtime/last-cache.json`

Once the worker completes a refresh, it should export:

- the current cache summary
- the recent refresh history

into one shared `xvalue`, then write that unified model into the snapshot file so logs, JSON, and the page all stay aligned.


## 8. Common Mistakes

### 8.1 Trying to store recent history back inside the `dict`

That mixes:

- current objects
- historical events

into one structure and makes the dashboard harder to reason about.


### 8.2 Refreshing inline on the request thread after expiration

This is the fastest path back to a blocked cache surface.


### 8.3 Treating `hash` as a security primitive

`xrtHash64()` is only:

- one stable content fingerprint

It is not:

- encryption
- signing
- authentication


### 8.4 Letting the page read worker-side temporary state

The page and JSON outputs should only read:

- copied current cache objects
- copied refresh history entries

They should not inspect:

- a live `DemoRefreshJob`
- worker-local temporary buffers


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. multi-tenant cache indexes
2. refresh retry and backoff
3. merging this into the full local service mainline

If the container split is still not fully stable yet, read these together:

- [Dict Intro: When to Use a Keyed Dictionary Instead of Arrays, Lists, or Hand-Rolled Trees](../guide/dict-intro.md) (Chinese for now)
- [List Intro: When to Use an Integer-Keyed Sparse Mapping Instead of Arrays or Dict](../guide/list-intro.md) (Chinese for now)
- [A Local Console Service with Config, Logging, Tasks, Networking, and Templates](local-console-service.en.md)
