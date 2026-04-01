# Upgrade the Local Console Service into a Rolling Tier Archive Dashboard

> This case is not about casually deleting a few old cold-tier records after the cold window fills up. It targets the more realistic service problem: once one local console service already has a hot tier, a cold tier, and warm-back semantics, how do you keep long-idle cold objects moving into one archive tier, while still exporting the current hot state, the current cold window, recent archive history, and one stable snapshot to the page, JSON, logs, and scripts through one formal `dict + list + list + queue + thread + time + path + file + hash + xhttpd + template` chain?

[Back to Case Index](README.en.md)

---

## 1. Scenario

Assume the service already has these pieces:

- current hot objects live in the hot tier
- cooled objects move into the cold tier
- some cold-tier objects can still warm back into the hot tier
- the page and JSON can already see both hot and cold summaries

Then the service moves one step further and runs into one longer-lived problem:

- some cold-tier objects stay idle for too long
- keeping them inside the current cold window is no longer worth it
- but they still should not simply disappear
- the page and scripts still need to show what recently rolled out of the cold tier and became archived
- the latest hot/cold/archive state still needs one snapshot file

If this is reduced to:

- remove one item as soon as the cold window overflows
- write one log line on the side

the state model becomes blurry immediately.

So this page adds one formal answer:

> once one service already has hot and cold tiers, how should cold overflow continue into archive with one stable exported model?


## 2. Why This Is Not “Delete When the Cold Window Overflows”

### 2.1 Cold overflow means state migration, not record invalidation

When one object leaves the cold tier and enters the archive tier, what actually happens is:

- it is no longer part of the current cold window
- it is still one formal historical record
- the page and snapshots still need to say when it was rolled into archive


### 2.2 The current cold window and long-lived archive history are not the same data

The cold window answers:

- which cold objects still remain in the active window
- which objects can still warm back

Archive history answers:

- which objects have already rolled out of the active window
- what their last cold state looked like
- which stable records the page and scripts should still display


### 2.3 Deletion only answers “it is gone now”; archive answers “what happened before”

If one object is removed from the cold tier directly, the system can only answer:

- it is no longer in the cold window

But one real service still needs to answer:

- what key it had
- why it entered the cold tier
- when it rolled out of the active window
- how it should be displayed in archive history


## 3. What Each Layer Owns Here

| Layer | Role in this page | What it actually solves |
|------|--------------------|--------------------------|
| `dict` | current hot-tier objects | current hot lookups and fast hits |
| `list` | current cold window | cold objects that can still warm back |
| `list` | recent archive history | page timelines and stable archive export |
| `queue + thread` | background cooling and rolling archive | keeps request threads away from tier migration and snapshot writes |
| `time` | touch time, cool time, archive time | formal time boundaries across the three states |
| `hash` | stable key fingerprint | lightweight page, log, and snapshot identifiers |
| `path + file` | rolling snapshot path and persistence | moves hot/cold/archive state into the filesystem |
| `xvalue + json` | exported hot/cold/archive summaries | lets the page, scripts, and snapshots share one model |
| `xhttpd + template` | three-tier presentation | gives browsers and scripts one shared entry |

One sentence to remember:

> `dict` manages what is still hot, one `list` manages what still remains in the current cold window, another `list` manages what has formally rolled into archive, and the worker decides when cold overflow moves forward.


## 4. File and Output Contract

This page keeps the same local-console-service directory shape and gives it rolling-archive semantics:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/rolling-tier-archive.json
```

Where:

- `config/console.json`
	- stores the cold-window size, archive-retention count, and queue capacity
- `web/dashboard.html`
	- shows the current hot tier, current cold tier, and recent archive history together
- `runtime/console.log`
	- records cooling submission and rolling-archive completion
- `runtime/rolling-tier-archive.json`
	- stores the latest three-tier summary


## 5. One Minimal Rolling-Archive Skeleton

The following code intentionally focuses on the rolling-archive core and leaves the full `xhttpd` handlers and template rendering out of the block.

It keeps five things:

1. `dict` stores the current hot-tier objects
2. `list` stores the current cold window
3. `list` stores recent archive history
4. `queue + thread` pushes cooling and cold-overflow work into the background
5. `path + file + json` writes the current three-tier summary into `runtime/rolling-tier-archive.json`

This skeleton actually does the following:

- initializes one rolling center
- registers four hot-tier objects
- submits three cooling requests
- lets the worker move objects from the hot tier into the cold tier
- lets the third cooling step overflow the cold window and move the oldest cold record into archive
- prints the hot tier, cold tier, archive history, and final snapshot in the main thread

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iHotId;
	xtime tTouchedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoHotItem;

typedef struct
{
	xtime tTouchedAt;
	xtime tCooledAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
} DemoColdEntry;

typedef struct
{
	xtime tTouchedAt;
	xtime tCooledAt;
	xtime tArchivedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
	char sArchiveReason[48];
} DemoArchiveEntry;

typedef struct
{
	char sKey[64];
	char sTierReason[48];
} DemoTierJob;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextArchiveSeq;
	uint32 iKeepCold;
	uint32 iKeepArchive;
	char sSnapshotPath[260];
} DemoRollingCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static bool procCollectHotSnapshot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue vHotArr = (xvalue)pArg;
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	xvalue vRow;
	(void)pKey;

	if ( (vHotArr == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pItem->iKeyHash);
	(void)xvoArrayAppendValue(vHotArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectColdSnapshot(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vColdArr = (xvalue)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vColdArr == NULL) || (pEntry == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pEntry->sTierReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEntry->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pEntry->tTouchedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEntry->iKeyHash);
	(void)xvoArrayAppendValue(vColdArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectArchiveSnapshot(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArchiveArr = (xvalue)pArg;
	DemoArchiveEntry* pEntry = (DemoArchiveEntry*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArchiveArr == NULL) || (pEntry == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pEntry->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "archive_reason", 0u, pEntry->sArchiveReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEntry->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pEntry->tTouchedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEntry->tArchivedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEntry->iKeyHash);
	(void)xvoArrayAppendValue(vArchiveArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoRollingCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHotArr = NULL;
	xvalue vColdArr = NULL;
	xvalue vArchiveArr = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0u;
	bool bOk = FALSE;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	vRoot = xvoCreateTable();
	vHotArr = xvoCreateArray();
	vColdArr = xvoCreateArray();
	vArchiveArr = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHotSnapshot, vHotArr);
	xrtListWalk(pCenter->pCold, procCollectColdSnapshot, vColdArr);
	xrtListWalk(pCenter->pArchive, procCollectArchiveSnapshot, vArchiveArr);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHotArr, TRUE);
	xvoUnref(vHotArr);
	vHotArr = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vColdArr, TRUE);
	xvoUnref(vColdArr);
	vColdArr = NULL;
	xvoTableSetValue(vRoot, "archive_items", 0u, vArchiveArr, TRUE);
	xvoUnref(vArchiveArr);
	vArchiveArr = NULL;

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	sDir = xrtPathGetDir(pCenter->sSnapshotPath, 0u);
	if ( (sDir == NULL) || (xrtDirCreateAll(sDir) != TRUE) ) {
		goto cleanup;
	}

	if ( xrtFileWriteAll(pCenter->sSnapshotPath, sJson, iJsonSize, XRT_CP_UTF8) < 0 ) {
		goto cleanup;
	}

	bOk = TRUE;

cleanup:
	if ( sDir != NULL ) {
		xrtFree(sDir);
	}
	if ( sJson != NULL ) {
		xrtFree(sJson);
	}
	if ( vArchiveArr != NULL ) {
		xvoUnref(vArchiveArr);
	}
	if ( vColdArr != NULL ) {
		xvoUnref(vColdArr);
	}
	if ( vHotArr != NULL ) {
		xvoUnref(vHotArr);
	}
	if ( vRoot != NULL ) {
		xvoUnref(vRoot);
	}
	return bOk;
}

static bool procAppendArchiveLocked(DemoRollingCenter* pCenter, const DemoColdEntry* pCold, const char* sArchiveReason)
{
	DemoArchiveEntry* pEntry = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pCold == NULL) ) {
		return FALSE;
	}

	pCenter->iNextArchiveSeq++;
	iSeq = pCenter->iNextArchiveSeq;
	pEntry = (DemoArchiveEntry*)xrtListSet(pCenter->pArchive, iSeq, &bNew);
	if ( pEntry == NULL ) {
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pCold->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pCold->sTitle);
	procCopyText(pEntry->sTierReason, sizeof(pEntry->sTierReason), pCold->sTierReason);
	procCopyText(pEntry->sArchiveReason, sizeof(pEntry->sArchiveReason), sArchiveReason);
	pEntry->iKeyHash = pCold->iKeyHash;
	pEntry->iHeat = pCold->iHeat;
	pEntry->tTouchedAt = pCold->tTouchedAt;
	pEntry->tCooledAt = pCold->tCooledAt;
	pEntry->tArchivedAt = xrtNow();

	if ( iSeq > (int64)pCenter->iKeepArchive ) {
		(void)xrtListRemove(pCenter->pArchive, iSeq - (int64)pCenter->iKeepArchive);
	}

	return TRUE;
}

static bool procAppendColdLocked(DemoRollingCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
{
	DemoColdEntry* pEntry = NULL;
	DemoColdEntry* pOverflow = NULL;
	bool bNew = FALSE;
	int64 iSeq;
	int64 iOverflowSeq;

	if ( (pCenter == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	pCenter->iNextColdSeq++;
	iSeq = pCenter->iNextColdSeq;
	pEntry = (DemoColdEntry*)xrtListSet(pCenter->pCold, iSeq, &bNew);
	if ( pEntry == NULL ) {
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pItem->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pItem->sTitle);
	procCopyText(pEntry->sTierReason, sizeof(pEntry->sTierReason), sTierReason);
	pEntry->iKeyHash = pItem->iKeyHash;
	pEntry->iHeat = pItem->iHeat;
	pEntry->tTouchedAt = pItem->tTouchedAt;
	pEntry->tCooledAt = xrtNow();

	if ( iSeq > (int64)pCenter->iKeepCold ) {
		iOverflowSeq = iSeq - (int64)pCenter->iKeepCold;
		pOverflow = (DemoColdEntry*)xrtListGet(pCenter->pCold, iOverflowSeq);
		if ( pOverflow != NULL ) {
			(void)procAppendArchiveLocked(pCenter, pOverflow, "cold-window-roll");
			(void)xrtListRemove(pCenter->pCold, iOverflowSeq);
		}
	}

	return TRUE;
}

static bool procRollingCenterInit(DemoRollingCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 2u;
	pCenter->iKeepArchive = 8u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pCenter->pHot == NULL ) {
		return FALSE;
	}

	pCenter->pCold = xrtListCreate(sizeof(DemoColdEntry), XRT_OBJMODE_SHARED);
	if ( pCenter->pCold == NULL ) {
		xrtDictDestroy(pCenter->pHot);
		pCenter->pHot = NULL;
		return FALSE;
	}

	pCenter->pArchive = xrtListCreate(sizeof(DemoArchiveEntry), XRT_OBJMODE_SHARED);
	if ( pCenter->pArchive == NULL ) {
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pArchive);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pArchive = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pArchive);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hMutex = NULL;
		pCenter->pArchive = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	sSnapshotPath = xrtPathJoin(2, "runtime", "rolling-tier-archive.json");
	if ( sSnapshotPath == NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pArchive);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hQueue = NULL;
		pCenter->hMutex = NULL;
		pCenter->pArchive = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoRollingCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
{
	DemoHotItem* pItem = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pItem = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
	if ( pItem == NULL ) {
		return FALSE;
	}

	memset(pItem, 0, sizeof(*pItem));
	pCenter->iNextHotId++;
	pItem->iHotId = pCenter->iNextHotId;
	pItem->tTouchedAt = xrtNow();
	pItem->iKeyHash = xrtHash64((ptr)sKey, strlen(sKey));
	pItem->iHeat = iHeat;
	procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);
	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
		xrtFree(pItem);
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitCool(DemoRollingCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoTierJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoTierJob*)xrtMalloc(sizeof(DemoTierJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sTierReason, sizeof(pJob->sTierReason), sTierReason);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procRollingWorker(ptr pArg)
{
	DemoRollingCenter* pCenter = (DemoRollingCenter*)pArg;
	DemoTierJob* pJob = NULL;
	DemoHotItem* pItem = NULL;
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
		pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)pJob->sKey, (uint32)strlen(pJob->sKey));
		if ( pItem != NULL ) {
			(void)procAppendColdLocked(pCenter, pItem, pJob->sTierReason);
			(void)procWriteSnapshotLocked(pCenter);
		}
		xrtMutexUnlock(pCenter->hMutex);

		if ( pItem != NULL ) {
			xrtFree(pItem);
			pItem = NULL;
		}
		xrtFree(pJob);
	}

	return 0u;
}

static bool procDumpHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		printf(
			"hot key=%s title=%s heat=%d touched_at=%lld hash=%llu\n",
			pItem->sKey,
			pItem->sTitle,
			pItem->iHeat,
			(long long)pItem->tTouchedAt,
			(unsigned long long)pItem->iKeyHash
		);
	}
	return FALSE;
}

static bool procDumpCold(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	(void)pArg;

	printf(
		"cold seq=%lld key=%s title=%s heat=%d reason=%s cooled_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->iHeat,
		pEntry->sTierReason,
		(long long)pEntry->tCooledAt,
		(unsigned long long)pEntry->iKeyHash
	);
	return FALSE;
}

static bool procDumpArchive(int64 iKey, ptr pVal, ptr pArg)
{
	DemoArchiveEntry* pEntry = (DemoArchiveEntry*)pVal;
	(void)pArg;

	printf(
		"archive seq=%lld key=%s title=%s tier_reason=%s archive_reason=%s archived_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->sTierReason,
		pEntry->sArchiveReason,
		(long long)pEntry->tArchivedAt,
		(unsigned long long)pEntry->iKeyHash
	);
	return FALSE;
}

static bool procFreeHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		xrtFree(pItem);
	}
	return FALSE;
}

static void procRollingCenterUnit(DemoRollingCenter* pCenter)
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

	if ( (pCenter->hMutex != NULL) && (pCenter->pHot != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pHot, procFreeHot, NULL);
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
	if ( pCenter->pCold != NULL ) {
		xrtListDestroy(pCenter->pCold);
		pCenter->pCold = NULL;
	}
	if ( pCenter->pHot != NULL ) {
		xrtDictDestroy(pCenter->pHot);
		pCenter->pHot = NULL;
	}
}

int main(void)
{
	DemoRollingCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procRollingCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procRollingWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procRollingCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 8) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 5) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:delta", "Delta Board", 3) ) {
		goto cleanup;
	}

	if ( !procSubmitCool(&tCenter, "node:beta", "idle-window") ) {
		goto cleanup;
	}
	if ( !procSubmitCool(&tCenter, "node:gamma", "cooldown-batch") ) {
		goto cleanup;
	}
	if ( !procSubmitCool(&tCenter, "node:delta", "aging-roll") ) {
		goto cleanup;
	}

	xrtSleep(400u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);

	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);

	printf("== archive ==\n");
	xrtListWalk(tCenter.pArchive, procDumpArchive, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
		sSnapshot = NULL;
	}

	iRet = 0;

cleanup:
	procRollingCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. Six Critical Boundaries in This Code

### 6.1 The cold window and archive history must be split into two separate `list` instances

This page now keeps:

- `pHot`
- `pCold`
- `pArchive`

That means cold overflow is no longer “one less row in the cold list”. It becomes one formal `cold -> archive` migration.


### 6.2 Cold overflow must converge through one formal migration, not a silent trim

The most important boundary in `procAppendColdLocked()` is:

- append the new cold record first
- then check whether the cold window is already over capacity
- if it is, copy the oldest cold record into the archive list
- only then remove the oldest cold record from the active cold window

That is what lets the page and the snapshot say clearly:

- which rows still belong to the current cold window
- which rows have already become archive history


### 6.3 The request thread only submits cooling work; it does not own tier rolling

One important boundary in this page is:

- request-side logic only submits cooling work
- the worker actually performs `hot -> cold` and, when needed, `cold -> archive`

That keeps future archive growth away from the request thread.


### 6.4 `time + path + file` turn rolling archive into formal system state

`tCooledAt` and `tArchivedAt` make the time boundaries visible.  
`procWriteSnapshotLocked()` exports:

- the current hot tier
- the current cold tier
- recent archive history

into `runtime/rolling-tier-archive.json`.

That means rolling archive stops being one in-memory side effect and becomes one formal state layer shared by the page, scripts, and exports.


### 6.5 This page intentionally stops before automatic warm-back policy

This page teaches:

- how cold overflow moves into archive in a stable way

not yet:

- how many hits trigger automatic warm-back
- when archive records should return into hotter tiers

Because if the three-layer boundary is not stable yet, policy logic only makes the model harder to reason about.


### 6.6 `hash` is still only one stable fingerprint, not one security feature

`iKeyHash` is used here as:

- one lightweight identifier in the page
- one stable short fingerprint in logs and snapshots

It is not an auth token and should not be treated as a security boundary.


## 7. How It Reconnects to HTTP, Templates, and Scripts

### 7.1 `POST /api/cool`

It should only:

1. parse the object key
2. build one cooling request
3. push that request into the background queue
4. return “accepted” immediately

The handler should not trim the cold window or write archive state inline.


### 7.2 `GET /api/dashboard`

The most natural JSON output has three parts:

1. the current hot-tier summary
2. the current cold-tier summary
3. recent archive history


### 7.3 `GET /dashboard`

The page naturally splits into:

1. one hot-tier table
2. one current cold-window panel
3. one recent archive timeline

That is why the template should not reconstruct cold overflow from log files.


### 7.4 `runtime/rolling-tier-archive.json`

Ops scripts, CLI tools, and page exports should consume this snapshot instead of reading worker-local temporary state directly.


## 8. Common Mistakes

### 8.1 Removing one cold record directly after the window overflows

If cold overflow is handled as direct removal, the page and the snapshot can only say:

- the old cold record is gone

but cannot say:

- whether it formally became archive state


### 8.2 Using archive to replace the current cold window

The cold tier owns “what may still warm back soon”.  
Archive owns “what has already rolled out of the active window”.

If these two are collapsed into one structure, later warm-back and multi-tier storage work becomes unstable immediately.


### 8.3 Pretending blind linear reads and writes will scale forever once the cold window grows

This page keeps:

- one `list` for the active cold window
- fixed sequence numbers for overflow handling

because that boundary is easier to teach.

If the cold tier keeps growing later, the structure should be upgraded instead of pretending this implementation scales forever.


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. [automatic warm-back policy](auto-warm-policy-dashboard.en.md)
2. multi-tier storage with deeper hot, cold, and archive layers
3. [warm-back and archive coordination](warm-archive-coordination-dashboard.en.md)

If the tier split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
- [Upgrade the Local Console Service into a Warm-Back Dashboard](warm-back-dashboard.en.md)
- [Upgrade the Local Console Service into an Archive Dashboard](archive-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
