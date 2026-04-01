# XRT Case Study: Upgrade the Local Console Service into a Warm-Back Dashboard

> This case is not about "if one cold object gets hit, just stuff it back into the hot tier". It targets a more realistic service problem: once one local console service already has a hot-cold split, and now also needs to promote cold objects back into the hot tier at the right time, expose recent warm-back events on the page and in JSON, and keep logs and snapshots aligned to the same model, how do you turn `dict + list + queue + thread + time + path + file + hash + xhttpd + template` into one formal mainline instead of letting cold hits, promotion rules, hot restoration, and rendering each keep their own partial state?

[中文](warm-back-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have the earlier hot-cold tier dashboard:

- current hot objects stay in the hot tier
- cooled objects stay in the cold tier
- the cold snapshot is written to disk
- both the page and JSON can show the hot and cold tiers

Now the system moves one step further, and a new business action appears:

- one cold object is hit again
- it should not stay in the cold tier forever
- but the request thread should not restore it inline
- the page should show which objects were warmed back recently
- the latest warm-back result should also enter the snapshot file

If the implementation stays at:

- "hit the cold tier and directly tweak a few fields"
- "then push it back into the hot dict"

four problems appear almost immediately:

- the request thread gets dragged by promotion convergence and snapshot writes
- hot restoration and cold removal have no formal event record
- the page starts scanning logs or worker-local temporary state just to show "recently warmed"
- the warm-back path and the hot-cold split collapse into one unclear boundary

So this page is not really about "how to reinsert one cold object into a dict". It is about:

> once the system already has a cold tier, how to turn warm-back promotion into one formal background mainline with one stable exportable state model.


## 2. Why This Is Not "Hit Cold, Restore Inline"

### 2.1 Warm-back is a tier migration, not one normal read

Once one object moves from the cold tier back into the hot tier, what really changes is:

- its tier changes
- its time semantics change
- the page and the snapshot should record one formal event

So this is no longer:

- one ordinary cold read

It is:

- one formal tier migration


### 2.2 A cold hit does not mean the request thread should execute the full recovery path

The request thread is better suited for:

- deciding whether this key should be warmed back
- generating one warm-back request
- returning "accepted" quickly, or returning the current cold state

It should not directly own:

- cold lookup followed by removal
- hot object reconstruction
- warm-back event recording
- snapshot file writes

As soon as those actions grow heavier, the request thread degrades quickly.


### 2.3 Warm-back history should become formal value state

What the page and the snapshot really need is not only:

- "this object is hot again now"

They also need:

- when it was warmed back
- why it was warmed back
- what cold state it had before promotion

So this page needs not only the hot and cold tiers, but also one formal:

- warm-back history line


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `dict` | stores current hot objects | live lookup after promotion |
| `list` | stores the current cold window | current cold objects and cold export |
| `list` | stores recent warm-back history | page timeline and event export |
| `queue + thread` | performs warm-back in the background | keeps request threads out of promotion work |
| `time` | records cooling time and warm-back time | formal event timing boundaries |
| `hash` | gives each object key one stable fingerprint | short identifiers in pages, logs, and snapshots |
| `path + file` | builds and writes warm-back snapshot paths | turns warm-back into real filesystem state |
| `xvalue + json` | exports the current hot, cold, and warm summary | page, scripts, and snapshots share one model |
| `xhttpd + template` | shows hot objects, cold objects, and warm-back history | browsers and scripts share one entry |

One-sentence version:

> `dict` owns "what became hot again now", one `list` owns "what is still cold now", another `list` owns "what was warmed back recently", and the worker owns "when one cold object is formally promoted back into the hot tier".


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the output meaning into a warm-back dashboard:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/warm-back.json
```

Where:

- `config/console.json`
	- stores the cold-window size, warm-back queue capacity, and whether automatic warm-back is enabled
- `web/dashboard.html`
	- renders the current hot tier, the current cold tier, and recent warm-back history
- `runtime/console.log`
	- records warm-back submission and warm-back completion
- `runtime/warm-back.json`
	- stores the latest hot-cold-warm summary


## 5. Compact Core: Hot Tier, Cold Window, Warm-Back History, and Background Queue

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the five boundaries that matter most:

1. `dict` stores current hot objects
2. `list` stores the current cold window
3. `list` stores recent warm-back history
4. `queue + thread` moves promotion into the background
5. `path + file + json` writes the hot-cold-warm summary into `runtime/warm-back.json`

It deliberately stays practical:

- initialize the warm-back center
- register several hot objects
- manually move two objects into the cold window
- submit one warm-back request
- let the worker promote the object back into the hot tier
- print the hot tier, cold tier, warm history, and final snapshot

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
	xtime tWarmedAt;
	xtime tPrevCooledAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sReason[48];
} DemoWarmEvent;

typedef struct
{
	char sKey[64];
	char sReason[48];
} DemoWarmJob;

typedef struct
{
	const char* sKey;
	bool bFound;
	int64 iSeq;
	DemoColdEntry tEntry;
} DemoColdLookup;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xlist pWarmLog;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextWarmSeq;
	uint32 iKeepCold;
	uint32 iKeepWarm;
	char sSnapshotPath[260];
} DemoWarmCenter;


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

static bool procCollectWarmSnapshot(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vWarmArr = (xvalue)pArg;
	DemoWarmEvent* pEvent = (DemoWarmEvent*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vWarmArr == NULL) || (pEvent == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, pEvent->sReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEvent->iHeat);
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	xvoTableSetInt(vRow, "prev_cooled_at", 0u, pEvent->tPrevCooledAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEvent->iKeyHash);
	(void)xvoArrayAppendValue(vWarmArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoWarmCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHotArr = NULL;
	xvalue vColdArr = NULL;
	xvalue vWarmArr = NULL;
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
	vWarmArr = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHotSnapshot, vHotArr);
	xrtListWalk(pCenter->pCold, procCollectColdSnapshot, vColdArr);
	xrtListWalk(pCenter->pWarmLog, procCollectWarmSnapshot, vWarmArr);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "warm_event_count", 0u, xrtListCount(pCenter->pWarmLog));
	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHotArr, TRUE);
	xvoUnref(vHotArr);
	vHotArr = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vColdArr, TRUE);
	xvoUnref(vColdArr);
	vColdArr = NULL;
	xvoTableSetValue(vRoot, "warm_events", 0u, vWarmArr, TRUE);
	xvoUnref(vWarmArr);
	vWarmArr = NULL;

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
	if ( vWarmArr != NULL ) {
		xvoUnref(vWarmArr);
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

static bool procAppendColdLocked(DemoWarmCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
{
	DemoColdEntry* pEntry = NULL;
	bool bNew = FALSE;
	int64 iSeq;

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
		(void)xrtListRemove(pCenter->pCold, iSeq - (int64)pCenter->iKeepCold);
	}

	return TRUE;
}

static bool procAppendWarmEventLocked(DemoWarmCenter* pCenter, const DemoColdEntry* pCold, const char* sReason)
{
	DemoWarmEvent* pEvent = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pCold == NULL) ) {
		return FALSE;
	}

	pCenter->iNextWarmSeq++;
	iSeq = pCenter->iNextWarmSeq;
	pEvent = (DemoWarmEvent*)xrtListSet(pCenter->pWarmLog, iSeq, &bNew);
	if ( pEvent == NULL ) {
		return FALSE;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->tPrevCooledAt = pCold->tCooledAt;
	pEvent->tWarmedAt = xrtNow();

	if ( iSeq > (int64)pCenter->iKeepWarm ) {
		(void)xrtListRemove(pCenter->pWarmLog, iSeq - (int64)pCenter->iKeepWarm);
	}

	return TRUE;
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
		memcpy(&pLookup->tEntry, pEntry, sizeof(pLookup->tEntry));
		return TRUE;
	}

	return FALSE;
}

static bool procWarmCenterInit(DemoWarmCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 8u;
	pCenter->iKeepWarm = 8u;

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

	pCenter->pWarmLog = xrtListCreate(sizeof(DemoWarmEvent), XRT_OBJMODE_SHARED);
	if ( pCenter->pWarmLog == NULL ) {
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pWarmLog);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pWarmLog = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pWarmLog);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hMutex = NULL;
		pCenter->pWarmLog = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	sSnapshotPath = xrtPathJoin(2, "runtime", "warm-back.json");
	if ( sSnapshotPath == NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pWarmLog);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hQueue = NULL;
		pCenter->hMutex = NULL;
		pCenter->pWarmLog = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoWarmCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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

static bool procMoveHotToColdLocked(DemoWarmCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoHotItem* pItem = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));
	if ( pItem == NULL ) {
		return FALSE;
	}

	(void)procAppendColdLocked(pCenter, pItem, sTierReason);
	xrtFree(pItem);
	return TRUE;
}

static bool procSubmitWarm(DemoWarmCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoWarmJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoWarmJob*)xrtMalloc(sizeof(DemoWarmJob));
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

static uint32 procWarmWorker(ptr pArg)
{
	DemoWarmCenter* pCenter = (DemoWarmCenter*)pArg;
	DemoWarmJob* pJob = NULL;
	DemoHotItem* pHot = NULL;
	DemoColdLookup tLookup;
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
		memset(&tLookup, 0, sizeof(tLookup));
		tLookup.sKey = pJob->sKey;
		xrtListWalk(pCenter->pCold, procFindColdWalk, &tLookup);

		if ( tLookup.bFound ) {
			pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
			if ( pHot != NULL ) {
				memset(pHot, 0, sizeof(*pHot));
				pCenter->iNextHotId++;
				pHot->iHotId = pCenter->iNextHotId;
				pHot->tTouchedAt = xrtNow();
				pHot->iKeyHash = tLookup.tEntry.iKeyHash;
				pHot->iHeat = tLookup.tEntry.iHeat + 3;
				procCopyText(pHot->sKey, sizeof(pHot->sKey), tLookup.tEntry.sKey);
				procCopyText(pHot->sTitle, sizeof(pHot->sTitle), tLookup.tEntry.sTitle);

				if ( xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
					(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
					(void)procAppendWarmEventLocked(pCenter, &tLookup.tEntry, pJob->sReason);
					(void)procWriteSnapshotLocked(pCenter);
					pHot = NULL;
				}
			}
		}
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

static bool procDumpWarm(int64 iKey, ptr pVal, ptr pArg)
{
	DemoWarmEvent* pEvent = (DemoWarmEvent*)pVal;
	(void)pArg;

	printf(
		"warm seq=%lld key=%s title=%s reason=%s warmed_at=%lld prev_cooled_at=%lld hash=%llu\n",
		(long long)iKey,
		pEvent->sKey,
		pEvent->sTitle,
		pEvent->sReason,
		(long long)pEvent->tWarmedAt,
		(long long)pEvent->tPrevCooledAt,
		(unsigned long long)pEvent->iKeyHash
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

static void procWarmCenterUnit(DemoWarmCenter* pCenter)
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
	if ( pCenter->pWarmLog != NULL ) {
		xrtListDestroy(pCenter->pWarmLog);
		pCenter->pWarmLog = NULL;
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
	DemoWarmCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procWarmCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procWarmWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procWarmCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 4) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 2) ) {
		goto cleanup;
	}

	xrtMutexLock(tCenter.hMutex);
	(void)procMoveHotToColdLocked(&tCenter, "node:beta", "idle-window");
	(void)procMoveHotToColdLocked(&tCenter, "node:gamma", "cooldown-batch");
	(void)procWriteSnapshotLocked(&tCenter);
	xrtMutexUnlock(tCenter.hMutex);

	if ( !procSubmitWarm(&tCenter, "node:beta", "user-hit") ) {
		goto cleanup;
	}

	xrtSleep(300u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);

	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);

	printf("== warm ==\n");
	xrtListWalk(tCenter.pWarmLog, procDumpWarm, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
		sSnapshot = NULL;
	}

	iRet = 0;

cleanup:
	procWarmCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. Six Things That Matter Most

### 6.1 Warm-back history must become one formal event line

This page keeps not only:

- `pHot`
- `pCold`

but also:

- `pWarmLog`

That means "the object was warmed back" is no longer only:

- one fewer cold object
- one more hot object

It is:

- one formal promotion event


### 6.2 The request thread only decides and enqueues, it does not converge the promotion

One of the most important boundaries here is:

- the request thread only submits one warm job
- the worker actually performs cold lookup, hot restoration, history recording, and snapshot writes

That way, even if the warm-back logic keeps growing later, the request thread does not degrade immediately.


### 6.3 The current cold window still uses `list`, so lookup is one deliberate design point

This page deliberately keeps one teaching boundary explicit:

- the current cold window is still one `list`
- so `procFindColdWalk()` performs one linear scan

That is not a bug. It is a boundary statement:

- if the cold window is small, this stays clear enough
- if the cold tier grows and key-based lookup becomes frequent, the structure should be upgraded to `dict` or `avltree`


### 6.4 Warm-back is not recreating a new hot object without lineage

`procWarmWorker()` does not create one arbitrary hot object out of thin air. It:

- restores key, title, hash, and heat from the cold snapshot
- records the previous `cooled_at`
- writes one formal warm-back event

That is what lets the page and the snapshot say clearly:

- which cold record this promotion came from


### 6.5 `path + file` turns warm-back into real system state

`procWriteSnapshotLocked()` exports:

- the current hot tier
- the current cold tier
- recent warm-back history

into `runtime/warm-back.json`.

That means warm-back stops being one transient in-memory action and becomes one formal state layer that scripts, panels, and exports can consume.


### 6.6 This page intentionally stops before automatic warm-back policy

This page first teaches:

- one explicit promotion path

not yet:

- warm back after N hits
- warm back after one time threshold

Because if the promotion boundary itself is still not stable, automatic policy only makes the state model harder to reason about.


## 7. How It Reconnects to HTTP, Templates, and Scripts

### 7.1 `POST /api/warm`

It should only:

1. parse the object key
2. choose one warm-back reason
3. push one job into the background queue
4. return "accepted" immediately

The handler should not restore the object inline.


### 7.2 `GET /api/dashboard`

The most natural JSON surface has three parts:

1. the current hot-tier summary
2. the current cold-tier summary
3. recent warm-back history


### 7.3 `GET /dashboard`

The page naturally splits into:

1. one hot-tier table
2. one cold-window panel
3. one recent warm-back timeline

That is why the template should not reconstruct warm-back events from logs.


### 7.4 `runtime/warm-back.json`

Ops scripts, CLI tools, and page exports should prefer this snapshot instead of reading worker-local temporary state directly.


## 8. Common Mistakes

### 8.1 Promoting inline as soon as the cold tier is hit

That puts tier migration and snapshot writes back onto the request thread.


### 8.2 Updating hot and cold state but never recording warm-back history

If there is no formal warm-back history, the page, snapshot, and logs cannot answer:

- when the object was warmed back
- why it was warmed back


### 8.3 Continuing with blind linear scans after the cold tier becomes large

This page keeps the linear scan on purpose because it teaches the boundary clearly.  
If the cold window grows large, the cold lookup structure should be upgraded instead of pretending one `list` will keep scaling forever.


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. rolling cold-tier archive
2. [automatic warm-back policy](auto-warm-policy-dashboard.md) (Chinese for now)
3. multi-tier storage with hot, cold, and long-term layers

If the tier split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
- [Upgrade the Local Console Service into a Cache Refresh Dashboard](cache-refresh-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
