# XRT Case Study: Upgrade the Local Console Service into an Archive Dashboard

> This case is not about "just remove the item after it is done". It targets a more realistic service problem: when one local console service must keep current active objects, turn finished objects into formal archive records, and expose recent archive history consistently to the page, JSON, logs, and snapshots, how do you turn `dict + list + queue + thread + path + file + xhttpd + template` into one formal mainline instead of letting deletion, logs, pages, and archive files each maintain their own version of the state?

[中文](archive-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have the earlier local console service:

- it loads config
- it writes logs
- it runs background work through `queue + thread`
- it exposes both JSON and HTML

Now the service moves one step further and gains a formal archive requirement:

- after one active object finishes, it should not simply disappear
- the page must show both current active objects and recent archive history
- archiving must not block the HTTP handler
- archive records need reason, time, and a stable identifier
- the latest archive summary should also be written into a snapshot file

If you keep the logic at:

- remove it from the current dict
- print one log line

four problems appear very quickly:

- live state and archive state collapse into one unclear boundary
- the page starts scanning log files or worker-local state just to show history
- once you add JSON or snapshots, one more archive field set gets invented again
- "delete" and "archive" become the same thing, so later changes become dangerous

So this page is not really about "how to delete one object". It is about:

> how to split current live state, archive state, and the background archive path into three stable lines once the system starts carrying real archive semantics.


## 2. Why This Is Not "Just Remove It from the Live Table"

### 2.1 Deletion only answers "it is no longer here", archive answers "what happened before"

Removing one object from the live table can only say:

- this object is no longer active

But a real archive still needs to preserve:

- what it used to be
- why it ended
- when it ended
- how it should still be shown on the page and in snapshots

So "delete" and "archive" are not the same thing.


### 2.2 `dict` owns current state, not historical state

In this page:

- `dict`
	- stores current active objects

It is best at answering:

- whether this key is active right now
- what the latest live state is

But it is not naturally suited to:

- the most recent archive entry
- the second most recent archive entry
- the third most recent archive entry

That is why archive history should be split out.


### 2.3 Archive history should become stable value records, not live object references

Once one object has ended, what the page and snapshot really need is:

- one stable snapshot

not:

- a pointer back into one old live object

That is exactly where:

- `list`

fits naturally.


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `dict` | stores current active objects | current-state lookup and management |
| `list` | stores recent archive history | page timeline and recent archive export |
| `queue + thread` | executes archive work in the background | keeps HTTP out of archive convergence |
| `path + file` | archive snapshot paths and file output | moves archive state into the real filesystem |
| `hash` | gives each object key one stable fingerprint | short identifiers for pages, logs, and snapshots |
| `xvalue + json` | exports the latest archive summary | JSON and snapshots share one model |
| `xhttpd + template` | shows live and archive state | one shared browser and script-facing entry |

One-sentence version:

> `dict` owns "what is still alive now", `list` owns "what was archived recently", and the worker owns "when one live object is formally turned into one archive record".


## 4. File Layout and Outputs

This case keeps the local-console-service layout and only changes the snapshot meaning into an archive dashboard:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-archive.json
```

Where:

- `config/console.json`
	- keeps archive queue capacity and recent retention limits
- `web/dashboard.html`
	- renders the archive dashboard
- `runtime/console.log`
	- records archive submission and archive completion
- `runtime/last-archive.json`
	- stores the latest exported archive summary


## 5. Compact Core: Live State, Archive State, Background Archive Queue

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the three boundaries that matter most:

1. `dict` stores current active objects
2. `list` stores recent archive history
3. `queue + thread` moves archive work into the background

It deliberately stays practical:

- initialize the archive center
- register several active objects
- submit two archive requests
- let the worker turn active objects into archive records
- print the current active objects and recent archive history from the main thread

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iActiveId;
	xtime tStartedAt;
	uint64 iKeyHash;
	char sKey[64];
	char sTitle[64];
} DemoActiveItem;

typedef struct
{
	char sKey[64];
	char sTitle[64];
	char sReason[48];
	uint64 iKeyHash;
	xtime tArchivedAt;
} DemoArchiveEntry;

typedef struct
{
	char sKey[64];
	char sReason[48];
} DemoArchiveJob;

typedef struct
{
	xdict pActive;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextActiveId;
	int64 iNextArchiveSeq;
} DemoArchiveCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static DemoActiveItem* procFindActiveLocked(DemoArchiveCenter* pCenter, const char* sKey)
{
	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return NULL;
	}

	return (DemoActiveItem*)xrtDictGetPtr(pCenter->pActive, (ptr)sKey, (uint32)strlen(sKey));
}

static bool procAppendArchiveLocked(DemoArchiveCenter* pCenter, DemoActiveItem* pItem, const char* sReason)
{
	DemoArchiveEntry* pEntry = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	pCenter->iNextArchiveSeq++;
	iSeq = pCenter->iNextArchiveSeq;
	pEntry = (DemoArchiveEntry*)xrtListSet(pCenter->pArchive, iSeq, &bNew);
	if ( pEntry == NULL ) {
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pItem->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pItem->sTitle);
	procCopyText(pEntry->sReason, sizeof(pEntry->sReason), sReason);
	pEntry->iKeyHash = pItem->iKeyHash;
	pEntry->tArchivedAt = xrtNow();

	if ( iSeq > 10 ) {
		(void)xrtListRemove(pCenter->pArchive, iSeq - 10);
	}

	return TRUE;
}

static bool procArchiveCenterInit(DemoArchiveCenter* pCenter)
{
	xqueue_config tCfg;

	memset(pCenter, 0, sizeof(*pCenter));

	pCenter->pActive = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pCenter->pActive == NULL ) {
		return FALSE;
	}

	pCenter->pArchive = xrtListCreate(sizeof(DemoArchiveEntry), XRT_OBJMODE_SHARED);
	if ( pCenter->pArchive == NULL ) {
		xrtDictDestroy(pCenter->pActive);
		pCenter->pActive = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pArchive);
		xrtDictDestroy(pCenter->pActive);
		pCenter->pArchive = NULL;
		pCenter->pActive = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pArchive);
		xrtDictDestroy(pCenter->pActive);
		pCenter->hMutex = NULL;
		pCenter->pArchive = NULL;
		pCenter->pActive = NULL;
		return FALSE;
	}

	return TRUE;
}

static bool procRegisterActive(DemoArchiveCenter* pCenter, const char* sKey, const char* sTitle)
{
	DemoActiveItem* pItem = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pItem = (DemoActiveItem*)xrtMalloc(sizeof(DemoActiveItem));
	if ( pItem == NULL ) {
		return FALSE;
	}

	memset(pItem, 0, sizeof(*pItem));
	pCenter->iNextActiveId++;
	pItem->iActiveId = pCenter->iNextActiveId;
	pItem->tStartedAt = xrtNow();
	pItem->iKeyHash = xrtHash64((ptr)sKey, strlen(sKey));
	procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);
	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);

	if ( !xrtDictSetPtr(pCenter->pActive, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
		xrtFree(pItem);
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitArchive(DemoArchiveCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoArchiveJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoArchiveJob*)xrtMalloc(sizeof(DemoArchiveJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sReason, sizeof(pJob->sReason), sReason);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procArchiveWorker(ptr pArg)
{
	DemoArchiveCenter* pCenter = (DemoArchiveCenter*)pArg;
	DemoArchiveJob* pJob = NULL;
	DemoActiveItem* pItem = NULL;
	xqueue_result iRet;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		xrtSleep(40u);

		xrtMutexLock(pCenter->hMutex);
		pItem = (DemoActiveItem*)xrtDictRemovePtr(pCenter->pActive, (ptr)pJob->sKey, (uint32)strlen(pJob->sKey));
		if ( pItem != NULL ) {
			(void)procAppendArchiveLocked(pCenter, pItem, pJob->sReason);
		}
		xrtMutexUnlock(pCenter->hMutex);

		if ( pItem != NULL ) {
			xrtFree(pItem);
		}
		xrtFree(pJob);
	}

	return 0u;
}

static bool procDumpActive(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoActiveItem* pItem = *(DemoActiveItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		printf(
			"active key=%s title=%s started_at=%lld hash=%llu\n",
			pItem->sKey,
			pItem->sTitle,
			(long long)pItem->tStartedAt,
			(unsigned long long)pItem->iKeyHash
		);
	}

	return FALSE;
}

static bool procDumpArchive(int64 iKey, ptr pVal, ptr pArg)
{
	DemoArchiveEntry* pEntry = (DemoArchiveEntry*)pVal;
	(void)pArg;

	printf(
		"archive seq=%lld key=%s title=%s reason=%s archived_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->sReason,
		(long long)pEntry->tArchivedAt,
		(unsigned long long)pEntry->iKeyHash
	);
	return FALSE;
}

static bool procFreeActive(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoActiveItem* pItem = *(DemoActiveItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		xrtFree(pItem);
	}
	return FALSE;
}

static void procArchiveCenterUnit(DemoArchiveCenter* pCenter)
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

	if ( (pCenter->hMutex != NULL) && (pCenter->pActive != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pActive, procFreeActive, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}

	if ( pCenter->hMutex != NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		pCenter->hMutex = NULL;
	}
	if ( pCenter->pArchive != NULL ) {
		xrtListDestroy(pCenter->pArchive);
		pCenter->pArchive = NULL;
	}
	if ( pCenter->pActive != NULL ) {
		xrtDictDestroy(pCenter->pActive);
		pCenter->pActive = NULL;
	}
}

int main(void)
{
	DemoArchiveCenter tCenter;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procArchiveCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procArchiveWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procArchiveCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterActive(&tCenter, "job:alpha", "Alpha Import") ) {
		goto cleanup;
	}
	if ( !procRegisterActive(&tCenter, "job:beta", "Beta Refresh") ) {
		goto cleanup;
	}
	if ( !procRegisterActive(&tCenter, "job:gamma", "Gamma Export") ) {
		goto cleanup;
	}

	if ( !procSubmitArchive(&tCenter, "job:beta", "completed") ) {
		goto cleanup;
	}
	if ( !procSubmitArchive(&tCenter, "job:gamma", "expired") ) {
		goto cleanup;
	}

	xrtSleep(300u);

	xrtMutexLock(tCenter.hMutex);
	printf("== active ==\n");
	xrtDictWalk(tCenter.pActive, procDumpActive, NULL);

	printf("== archive ==\n");
	xrtListWalk(tCenter.pArchive, procDumpArchive, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	iRet = 0;

cleanup:
	procArchiveCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. Five Things That Matter Most

### 6.1 `dict` stores the current live state, not the archive state

`pActive` stores:

- objects that are still active right now

Once one object formally enters the archive, the most stable action is:

- remove it from `dict`
- convert it into one stable archive record


### 6.2 `list` stores stable archive records, not live object pointers

`pArchive` uses:

- `xrtListCreate(sizeof(DemoArchiveEntry), XRT_OBJMODE_SHARED)`

That means:

- archive records are copied into the list by value

So the page, JSON, and snapshots consume stable archive state, not pointers back into live objects.


### 6.3 Archive work belongs to the background worker, not the HTTP thread

One of the most important boundaries in this page is:

- HTTP only submits archive requests
- the worker actually turns live state into archive state

So later additions such as file output, snapshots, or notifications do not drag the HTTP thread into the slow path.


### 6.4 Archive is not the same as logs

Logs still matter, but the real archive state that the page and snapshots consume here is:

- `DemoArchiveEntry`

In other words, logs are only one observation path, not the archive state itself.


### 6.5 `hash` gives archive objects one stable identifier

`iKeyHash` is used here as:

- one lightweight identifier in pages, logs, and snapshots

It is not one security feature. It is only one stable fingerprint.


## 7. How It Reconnects to HTTP, Templates, and Snapshots

### 7.1 `POST /api/archive`

It should only:

1. parse the object key
2. build one archive request
3. enqueue it into the background queue
4. return "accepted" immediately

The handler should not perform deletion and archive convergence inline.


### 7.2 `GET /api/archive`

The most natural JSON output is two parts:

1. current active objects
2. recent archive history

These map directly onto:

- `dict`
- `list`


### 7.3 `GET /dashboard`

The page naturally splits into:

1. one table of current active objects
2. one recent archive timeline

That is why the template does not need to scan log files just to reconstruct archive history.


### 7.4 `runtime/last-archive.json`

After the worker completes one archive step, it should export:

- the current active-object summary
- the recent archive history

into one shared `xvalue`, then write that model into a snapshot file so the page, JSON, and logs all stay aligned.


## 8. Common Mistakes

### 8.1 Treating archive as direct deletion

That may look simpler in the short term, but in the longer term it leaves:

- page history
- snapshot export
- audit records

without any formal source of truth.


### 8.2 Using logs as the archive state

Logs are useful for observation, but they cannot replace:

- archive records consumed directly by the page
- archive records exported directly by JSON and snapshots


### 8.3 Moving archive work back into the request thread

That turns archive convergence back into one synchronous slow path. As soon as archive logic grows heavier, the dashboard degrades immediately.


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. the hot-cold tier dashboard
2. archive plus retry integration
3. rolling archive files

If the state split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Retry Backoff Dashboard](retry-backoff-dashboard.en.md)
- [Upgrade the Local Console Service into a Cache Refresh Dashboard](cache-refresh-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
