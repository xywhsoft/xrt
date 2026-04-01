# Upgrade the Local Console Service into a Warm-Archive Coordination Dashboard

> This page is not about teaching automatic warm-back and rolling archive as two separate features. It targets the more realistic service problem: once one local console service already has a hot tier, a cold tier, automatic warm-back rules, and archive semantics, how do you put "stay cold when hits are still insufficient", "warm back automatically when the threshold is reached", and "archive when long-lived cold entries still do not recover" into one background mainline, while still exporting the current hot/cold state, recent warm-back history, and recent archive history to the page, JSON, logs, and snapshot files through one formal `dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` chain?

[Back to Case Index](README.en.md)

---

## 1. Scenario

The previous pages already covered these pieces one by one:

- how to split hot and cold tiers
- how one cold object returns to the hot tier
- how one long-idle cold object continues into archive

But in one real service, one cold entry only has three next destinations:

- stay in the cold tier
- return to the hot tier
- move into archive

So automatic warm-back and rolling archive are not two isolated capabilities. They are:

> two branches of the same cold-state convergence path.

This page adds that shared coordination path.


## 2. Why This Cannot Stay Split Across Different Logic Paths

If access hits and archive sweeps stay in different parts of the service, the model degrades quickly:

- the service no longer has one formal boundary for "waiting for warm-back" versus "ready for archive"
- hit counts, warm windows, and archive eligibility leak into temporary state in multiple places
- the page starts scanning logs or worker-local state just to explain why one object did not warm back but still got archived
- once you add deeper multi-tier storage later, the state model falls apart immediately

So the most important boundary in this page is one sentence:

> access signals and archive sweeps must converge inside the same worker, and that worker decides where the cold entry goes next.


## 3. What Each Layer Owns Here

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot-tier objects | real-time hot lookups and post-warm-back hits |
| `list` | current cold window and hit state | cold objects, accumulated hits, window start, and recent hit source |
| `list` | recent warm-back history | page timelines and warm-back event export |
| `list` | recent archive history | page timelines and archive event export |
| `queue + thread` | background access-signal and archive-sweep execution | keeps request threads away from coordination convergence |
| `time` | cool-down time, warm window, warm-back time, archive time | formal time boundaries for the coordination policy |
| `path + file` | snapshot path and persistence | moves coordination state into the filesystem |
| `xvalue + json` | hot/cold/warm/archive summaries | lets the page, scripts, and snapshots share the same model |


## 4. One Minimal but Complete Coordination Skeleton

This code intentionally keeps only six things:

1. `dict` stores the current hot-tier objects
2. `list` stores the current cold window and hit state
3. `list` stores recent warm-back history
4. `list` stores recent archive history
5. `queue + thread` handles access signals and archive sweeps together
6. `path + file + json` writes the current coordinated state into `runtime/warm-archive-coordination.json`

This skeleton actually does the following:

- initializes one coordination center
- registers three hot-tier objects
- manually moves `beta` and `gamma` into the cold tier first
- submits two access signals for `beta` so it warms back automatically
- submits one archive sweep later so long-idle `gamma` moves into archive
- prints the hot tier, cold tier, warm-back history, archive history, and final snapshot in the main thread

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_JOB_ACCESS 1
#define DEMO_JOB_SWEEP 2

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
	xtime tFirstWarmHitAt;
	xtime tLastWarmHitAt;
	uint64 iKeyHash;
	int iHeat;
	int iWarmHits;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
	char sLastHitSource[48];
} DemoColdEntry;

typedef struct
{
	xtime tWarmedAt;
	xtime tArchivedAt;
	uint64 iKeyHash;
	int iHeat;
	int iWarmHits;
	char sKey[64];
	char sTitle[64];
	char sReason[48];
} DemoStateEvent;

typedef struct
{
	int iKind;
	char sKey[64];
	char sText[48];
} DemoJob;

typedef struct
{
	const char* sKey;
	bool bFound;
	int64 iSeq;
} DemoColdLookup;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xlist pWarmLog;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextWarmSeq;
	int64 iNextArchiveSeq;
	uint32 iWarmWindowMs;
	uint32 iWarmMinHits;
	uint32 iArchiveAfterMs;
	char sSnapshotPath[260];
} DemoCoordCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}
	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static bool procFindColdWalk(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdLookup* pLookup = (DemoColdLookup*)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;

	if ( (pLookup == NULL) || (pEntry == NULL) || (pLookup->sKey == NULL) ) {
		return FALSE;
	}
	if ( strcmp(pEntry->sKey, pLookup->sKey) == 0 ) {
		pLookup->bFound = TRUE;
		pLookup->iSeq = iKey;
		return TRUE;
	}
	return FALSE;
}

static bool procCollectHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	xvalue vRow;
	(void)pKey;

	if ( (vArr == NULL) || (pItem == NULL) ) {
		return FALSE;
	}
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectCold(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArr == NULL) || (pEntry == NULL) ) {
		return FALSE;
	}
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pEntry->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "last_hit_source", 0u, pEntry->sLastHitSource, 0u, FALSE);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEntry->iWarmHits);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "first_warm_hit_at", 0u, pEntry->tFirstWarmHitAt);
	xvoTableSetInt(vRow, "last_warm_hit_at", 0u, pEntry->tLastWarmHitAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectEvent(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoStateEvent* pEvent = (DemoStateEvent*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArr == NULL) || (pEvent == NULL) ) {
		return FALSE;
	}
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, pEvent->sReason, 0u, FALSE);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEvent->iWarmHits);
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEvent->tArchivedAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoCoordCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHot = NULL;
	xvalue vCold = NULL;
	xvalue vWarm = NULL;
	xvalue vArchive = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0u;
	bool bOk = FALSE;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	vRoot = xvoCreateTable();
	vHot = xvoCreateArray();
	vCold = xvoCreateArray();
	vWarm = xvoCreateArray();
	vArchive = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHot, vHot);
	xrtListWalk(pCenter->pCold, procCollectCold, vCold);
	xrtListWalk(pCenter->pWarmLog, procCollectEvent, vWarm);
	xrtListWalk(pCenter->pArchive, procCollectEvent, vArchive);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "warm_count", 0u, xrtListCount(pCenter->pWarmLog));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "warm_window_ms", 0u, pCenter->iWarmWindowMs);
	xvoTableSetInt(vRoot, "warm_min_hits", 0u, pCenter->iWarmMinHits);
	xvoTableSetInt(vRoot, "archive_after_ms", 0u, pCenter->iArchiveAfterMs);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHot, TRUE);
	xvoUnref(vHot);
	vHot = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vCold, TRUE);
	xvoUnref(vCold);
	vCold = NULL;
	xvoTableSetValue(vRoot, "warm_events", 0u, vWarm, TRUE);
	xvoUnref(vWarm);
	vWarm = NULL;
	xvoTableSetValue(vRoot, "archive_events", 0u, vArchive, TRUE);
	xvoUnref(vArchive);
	vArchive = NULL;

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
	if ( sDir != NULL ) xrtFree(sDir);
	if ( sJson != NULL ) xrtFree(sJson);
	if ( vArchive != NULL ) xvoUnref(vArchive);
	if ( vWarm != NULL ) xvoUnref(vWarm);
	if ( vCold != NULL ) xvoUnref(vCold);
	if ( vHot != NULL ) xvoUnref(vHot);
	if ( vRoot != NULL ) xvoUnref(vRoot);
	return bOk;
}

static bool procAppendColdLocked(DemoCoordCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
{
	DemoColdEntry* pEntry = NULL;
	bool bNew = FALSE;

	pCenter->iNextColdSeq++;
	pEntry = (DemoColdEntry*)xrtListSet(pCenter->pCold, pCenter->iNextColdSeq, &bNew);
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
	return TRUE;
}

static void procAppendWarmLocked(DemoCoordCenter* pCenter, DemoColdEntry* pCold, const char* sReason)
{
	DemoStateEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextWarmSeq++;
	pEvent = (DemoStateEvent*)xrtListSet(pCenter->pWarmLog, pCenter->iNextWarmSeq, &bNew);
	if ( pEvent == NULL ) {
		return;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->iWarmHits = pCold->iWarmHits;
	pEvent->tWarmedAt = xrtNow();
}

static void procAppendArchiveLocked(DemoCoordCenter* pCenter, DemoColdEntry* pCold, const char* sReason)
{
	DemoStateEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextArchiveSeq++;
	pEvent = (DemoStateEvent*)xrtListSet(pCenter->pArchive, pCenter->iNextArchiveSeq, &bNew);
	if ( pEvent == NULL ) {
		return;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->iWarmHits = pCold->iWarmHits;
	pEvent->tArchivedAt = xrtNow();
}

static bool procCoordCenterInit(DemoCoordCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iWarmWindowMs = 400u;
	pCenter->iWarmMinHits = 2u;
	pCenter->iArchiveAfterMs = 180u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pCold = xrtListCreate(sizeof(DemoColdEntry), XRT_OBJMODE_SHARED);
	pCenter->pWarmLog = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->pArchive = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->hMutex = xrtMutexCreate();
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	sSnapshotPath = xrtPathJoin(2, "runtime", "warm-archive-coordination.json");

	if ( (pCenter->pHot == NULL) || (pCenter->pCold == NULL) || (pCenter->pWarmLog == NULL) || (pCenter->pArchive == NULL) || (pCenter->hMutex == NULL) || (pCenter->hQueue == NULL) || (sSnapshotPath == NULL) ) {
		if ( sSnapshotPath != NULL ) xrtFree(sSnapshotPath);
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoCoordCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));

	if ( (pCenter == NULL) || (sKey == NULL) || (pItem == NULL) ) {
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

static bool procMoveHotToColdLocked(DemoCoordCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));

	if ( pItem == NULL ) {
		return FALSE;
	}
	(void)procAppendColdLocked(pCenter, pItem, sTierReason);
	xrtFree(pItem);
	return TRUE;
}

static bool procSubmitJob(DemoCoordCenter* pCenter, int iKind, const char* sKey, const char* sText)
{
	DemoJob* pJob = (DemoJob*)xrtMalloc(sizeof(DemoJob));

	if ( (pCenter == NULL) || (pJob == NULL) ) {
		return FALSE;
	}
	memset(pJob, 0, sizeof(*pJob));
	pJob->iKind = iKind;
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sText, sizeof(pJob->sText), sText);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}
	return TRUE;
}

static uint32 procCoordWorker(ptr pArg)
{
	DemoCoordCenter* pCenter = (DemoCoordCenter*)pArg;
	DemoJob* pJob = NULL;
	DemoColdLookup tLookup;
	DemoColdEntry* pCold = NULL;
	DemoHotItem* pHot = NULL;
	xqueue_result iRet;
	xtime tNow;
	int64 iSeq;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) break;
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) continue;

		xrtSleep(40u);
		xrtMutexLock(pCenter->hMutex);
		tNow = xrtNow();

		if ( pJob->iKind == DEMO_JOB_ACCESS ) {
			memset(&tLookup, 0, sizeof(tLookup));
			tLookup.sKey = pJob->sKey;
			xrtListWalk(pCenter->pCold, procFindColdWalk, &tLookup);
			if ( tLookup.bFound ) {
				pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, tLookup.iSeq);
				if ( pCold != NULL ) {
					if ( (pCold->tFirstWarmHitAt == 0) || ((tNow - pCold->tFirstWarmHitAt) > (xtime)pCenter->iWarmWindowMs) ) {
						pCold->iWarmHits = 1;
						pCold->tFirstWarmHitAt = tNow;
					} else {
						pCold->iWarmHits++;
					}
					pCold->tLastWarmHitAt = tNow;
					procCopyText(pCold->sLastHitSource, sizeof(pCold->sLastHitSource), pJob->sText);

					if ( pCold->iWarmHits >= (int)pCenter->iWarmMinHits ) {
						pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
						if ( pHot != NULL ) {
							memset(pHot, 0, sizeof(*pHot));
							pCenter->iNextHotId++;
							pHot->iHotId = pCenter->iNextHotId;
							pHot->tTouchedAt = tNow;
							pHot->iKeyHash = pCold->iKeyHash;
							pHot->iHeat = pCold->iHeat + pCold->iWarmHits + 1;
							procCopyText(pHot->sKey, sizeof(pHot->sKey), pCold->sKey);
							procCopyText(pHot->sTitle, sizeof(pHot->sTitle), pCold->sTitle);
							if ( xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
								procAppendWarmLocked(pCenter, pCold, "policy-hit-threshold");
								(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
								pHot = NULL;
							}
						}
					}
				}
			}
		} else if ( pJob->iKind == DEMO_JOB_SWEEP ) {
			for ( iSeq = 1; iSeq <= pCenter->iNextColdSeq; iSeq++ ) {
				pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, iSeq);
				if ( pCold == NULL ) continue;
				if ( (tNow - pCold->tCooledAt) < (xtime)pCenter->iArchiveAfterMs ) continue;
				if ( (pCold->tLastWarmHitAt != 0) && ((tNow - pCold->tLastWarmHitAt) <= (xtime)pCenter->iWarmWindowMs) ) continue;
				if ( pCold->iWarmHits >= (int)pCenter->iWarmMinHits ) continue;
				procAppendArchiveLocked(pCenter, pCold, pJob->sText);
				(void)xrtListRemove(pCenter->pCold, iSeq);
			}
		}

		(void)procWriteSnapshotLocked(pCenter);
		xrtMutexUnlock(pCenter->hMutex);

		if ( pHot != NULL ) {
			xrtFree(pHot);
			pHot = NULL;
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
		printf("hot key=%s title=%s heat=%d hash=%llu\n", pItem->sKey, pItem->sTitle, pItem->iHeat, (unsigned long long)pItem->iKeyHash);
	}
	return FALSE;
}

static bool procDumpCold(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	(void)pArg;
	printf("cold seq=%lld key=%s hits=%d source=%s cooled_at=%lld\n", (long long)iKey, pEntry->sKey, pEntry->iWarmHits, pEntry->sLastHitSource, (long long)pEntry->tCooledAt);
	return FALSE;
}

static bool procDumpEvent(int64 iKey, ptr pVal, ptr pArg)
{
	DemoStateEvent* pEvent = (DemoStateEvent*)pVal;
	const char* sTag = (const char*)pArg;
	printf("%s seq=%lld key=%s reason=%s hits=%d\n", sTag, (long long)iKey, pEvent->sKey, pEvent->sReason, pEvent->iWarmHits);
	return FALSE;
}

static bool procFreeHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;
	if ( pItem != NULL ) xrtFree(pItem);
	return FALSE;
}

static void procCoordCenterUnit(DemoCoordCenter* pCenter)
{
	if ( pCenter == NULL ) return;
	if ( pCenter->hQueue != NULL ) xrtMPSCQWaitClose(pCenter->hQueue);
	if ( pCenter->hWorker != NULL ) {
		xrtThreadWait(pCenter->hWorker);
		xrtThreadDestroy(pCenter->hWorker);
	}
	if ( pCenter->hQueue != NULL ) xrtMPSCQWaitDestroy(pCenter->hQueue);
	if ( (pCenter->hMutex != NULL) && (pCenter->pHot != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pHot, procFreeHot, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}
	if ( pCenter->hMutex != NULL ) xrtMutexDestroy(pCenter->hMutex);
	if ( pCenter->pArchive != NULL ) xrtListDestroy(pCenter->pArchive);
	if ( pCenter->pWarmLog != NULL ) xrtListDestroy(pCenter->pWarmLog);
	if ( pCenter->pCold != NULL ) xrtListDestroy(pCenter->pCold);
	if ( pCenter->pHot != NULL ) xrtDictDestroy(pCenter->pHot);
}

int main(void)
{
	DemoCoordCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) return 1;
	if ( !procCoordCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procCoordWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procCoordCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 4) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 2) ) goto cleanup;

	xrtMutexLock(tCenter.hMutex);
	(void)procMoveHotToColdLocked(&tCenter, "node:beta", "idle-window");
	(void)procMoveHotToColdLocked(&tCenter, "node:gamma", "cooldown-batch");
	(void)procWriteSnapshotLocked(&tCenter);
	xrtMutexUnlock(tCenter.hMutex);

	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "ui-click") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "api-refresh") ) goto cleanup;
	xrtSleep(220u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "cold-idle-archive") ) goto cleanup;

	xrtSleep(350u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);
	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);
	printf("== warm ==\n");
	xrtListWalk(tCenter.pWarmLog, procDumpEvent, "warm");
	printf("== archive ==\n");
	xrtListWalk(tCenter.pArchive, procDumpEvent, "archive");
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
	}

	iRet = 0;

cleanup:
	procCoordCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. Five Critical Boundaries in This Code

### 6.1 One cold entry now has three formal destinations

The same cold state can now:

- stay in the cold tier
- warm back automatically
- move into archive after one sweep

That is why the cold entry itself becomes the real coordination pivot.


### 6.2 Access signals and archive sweeps must converge in the same worker

The most important boundary in this page is:

- request threads only submit `ACCESS` and `SWEEP`
- the worker decides where each cold entry goes next

That is what keeps warm-back and archive decisions from racing against each other.


### 6.3 Archive sweeps are not based on age alone

The archive condition here explicitly requires all of the following:

- the cold entry is already older than the archive-aging threshold
- there is no recent warm hit still inside the active warm window
- the current hit count still does not satisfy the warm-back threshold

So archive is not just "older than N milliseconds", and warm-back is not just "more than N hits". The worker must read both parts together.


### 6.4 If recent hits still do not reach the threshold, the right answer is to stay cold

This page intentionally keeps one teaching boundary visible:

- `beta` receives two close hits and warms back
- `gamma` stays idle long enough and gets archived

Any cold entry that only has one recent hit and still does not qualify should remain in the cold tier.


### 6.5 `path + file` turns coordination into formal system state

`procWriteSnapshotLocked()` exports:

- the current hot tier
- the current cold-tier policy state
- recent warm-back history
- recent archive history

into `runtime/warm-archive-coordination.json`.

That means the coordination rule stops being one private worker-side behavior and becomes one formal state surface that scripts, panels, and exports can consume directly.


## 7. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. multi-tier storage with more explicit long-lived layers
2. automatic warm-back plus rolling archive integration
3. multi-stage aging such as `hot -> warm -> cold -> archive`

If the tier split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Warm-Back Dashboard](warm-back-dashboard.en.md)
- [Upgrade the Local Console Service into a Rolling Tier Archive Dashboard](rolling-tier-archive-dashboard.en.md)
- [Upgrade the Local Console Service into an Auto-Warm Policy Dashboard](auto-warm-policy-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
