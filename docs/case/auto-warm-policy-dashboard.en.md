# Upgrade the Local Console Service into an Auto-Warm Policy Dashboard

> This page is not about “warm back after two hits” as one isolated trick. It covers the more realistic service problem: once one local console service already has a hot tier, a cold tier, and one warm-back path, how do you turn hit thresholds, warm windows, automatic warm-back events, and the current hot/cold state into one stable `dict + list + list + queue + thread + time + path + file + hash + xhttpd + template` mainline that the page, JSON, logs, and snapshots can all consume?

[Back to Case Index](README.en.md)

---

## 1. Scenario

Assume the service already has:

- one hot tier for currently active objects
- one cold tier for cooled objects
- one warm-back path that can move some cold objects back into the hot tier
- one dashboard and one JSON endpoint that can already see hot/cold state

Then the next real requirement appears:

- one occasional cold-tier hit should not warm an object back immediately
- several hits inside one short window should trigger automatic warm-back
- the page should see how many policy hits one cold object has already accumulated
- every policy-driven warm-back should still produce one formal event record
- the latest hot/cold/policy state should still be written into one snapshot file

So this page is really about:

> once the system already has a cold tier, how do you turn automatic warm-back into one formal background policy with one stable exported state model?


## 2. Why This Is Not “Warm Back After One Hit”

One cold-tier hit may only be:

- one probe
- one occasional read
- one touch that still does not justify promotion

Automatic warm-back is trying to answer:

- whether the object has become hot again inside one formal time window

That is why the request thread should only:

- detect one cold hit
- enqueue one access signal
- return quickly

It should not own:

- hit counting
- window resets
- promotion decisions
- snapshot writes


## 3. What Each Layer Owns Here

| Layer | Role | What it solves |
|------|------|----------------|
| `dict` | current hot-tier objects | hot lookups and real-time hits after promotion |
| `list` | current cold window and hit state | current cold objects, accumulated hits, and warm-window state |
| `list` | recent automatic warm-back history | page timelines and exported warm-back events |
| `queue + thread` | background access-signal execution | keeps request threads away from policy convergence |
| `time` | cool time, warm window, warm-back time | formal time boundaries for the policy |
| `path + file` | snapshot path and persistence | moves policy state into the filesystem |
| `xvalue + json` | hot/cold/policy summary | lets the page, scripts, and snapshots share one model |


## 4. File and Output Contract

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/auto-warm-policy.json
```

- `config/console.json`
	- stores the minimum hit count, warm window, and queue capacity
- `web/dashboard.html`
	- shows hot tier, cold tier, and recent automatic warm-back history
- `runtime/console.log`
	- records cold hits, policy triggers, and warm-back completion
- `runtime/auto-warm-policy.json`
	- stores the latest hot/cold/policy summary


## 5. One Minimal Automatic Warm-Back Skeleton

This skeleton intentionally keeps the page focused on five things:

1. `dict` stores the current hot tier
2. `list` stores the current cold window and policy-hit state
3. `list` stores recent automatic warm-back history
4. `queue + thread` run policy work in the background
5. `path + file + json` write one shared snapshot into `runtime/auto-warm-policy.json`

It actually does the following:

- initializes one auto-warm center
- registers three hot-tier objects
- manually moves two objects into the cold tier
- submits three access signals
- uses a “2 hits / 400ms window” rule
- prints the final snapshot

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
	xtime tCooledAt;
	xtime tFirstWarmHitAt;
	xtime tLastWarmHitAt;
	uint64 iKeyHash;
	int iHeat;
	int iWarmHits;
	char sKey[64];
	char sTitle[64];
	char sLastHitSource[48];
} DemoColdEntry;

typedef struct
{
	xtime tWarmedAt;
	xtime tFirstWarmHitAt;
	xtime tLastWarmHitAt;
	uint64 iKeyHash;
	int iHeat;
	int iWarmHits;
	char sKey[64];
	char sTitle[64];
	char sReason[48];
} DemoWarmEvent;

typedef struct
{
	char sKey[64];
	char sSource[48];
} DemoAccessJob;

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
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextWarmSeq;
	uint32 iWarmWindowMs;
	uint32 iWarmMinHits;
	char sSnapshotPath[260];
} DemoAutoWarmCenter;

static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) return;
	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static bool procFindColdWalk(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdLookup* pLookup = (DemoColdLookup*)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;

	if ( (pLookup == NULL) || (pEntry == NULL) || (pLookup->sKey == NULL) ) return FALSE;
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

	if ( (vArr == NULL) || (pItem == NULL) ) return FALSE;
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
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

	if ( (vArr == NULL) || (pEntry == NULL) ) return FALSE;
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "last_hit_source", 0u, pEntry->sLastHitSource, 0u, FALSE);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEntry->iWarmHits);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "first_warm_hit_at", 0u, pEntry->tFirstWarmHitAt);
	xvoTableSetInt(vRow, "last_warm_hit_at", 0u, pEntry->tLastWarmHitAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectWarm(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoWarmEvent* pEvent = (DemoWarmEvent*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArr == NULL) || (pEvent == NULL) ) return FALSE;
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, pEvent->sReason, 0u, FALSE);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEvent->iWarmHits);
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoAutoWarmCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHot = NULL;
	xvalue vCold = NULL;
	xvalue vWarm = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0u;
	bool bOk = FALSE;

	vRoot = xvoCreateTable();
	vHot = xvoCreateArray();
	vCold = xvoCreateArray();
	vWarm = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHot, vHot);
	xrtListWalk(pCenter->pCold, procCollectCold, vCold);
	xrtListWalk(pCenter->pWarmLog, procCollectWarm, vWarm);

	xvoTableSetValue(vRoot, "hot_items", 0u, vHot, TRUE);
	xvoUnref(vHot);
	vHot = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vCold, TRUE);
	xvoUnref(vCold);
	vCold = NULL;
	xvoTableSetValue(vRoot, "warm_events", 0u, vWarm, TRUE);
	xvoUnref(vWarm);
	vWarm = NULL;
	xvoTableSetInt(vRoot, "warm_window_ms", 0u, pCenter->iWarmWindowMs);
	xvoTableSetInt(vRoot, "warm_min_hits", 0u, pCenter->iWarmMinHits);

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) goto cleanup;
	sDir = xrtPathGetDir(pCenter->sSnapshotPath, 0u);
	if ( (sDir == NULL) || (xrtDirCreateAll(sDir) != TRUE) ) goto cleanup;
	if ( xrtFileWriteAll(pCenter->sSnapshotPath, sJson, iJsonSize, XRT_CP_UTF8) < 0 ) goto cleanup;
	bOk = TRUE;

cleanup:
	if ( sDir != NULL ) xrtFree(sDir);
	if ( sJson != NULL ) xrtFree(sJson);
	if ( vWarm != NULL ) xvoUnref(vWarm);
	if ( vCold != NULL ) xvoUnref(vCold);
	if ( vHot != NULL ) xvoUnref(vHot);
	if ( vRoot != NULL ) xvoUnref(vRoot);
	return bOk;
}

static bool procAppendWarmEventLocked(DemoAutoWarmCenter* pCenter, const DemoColdEntry* pCold)
{
	DemoWarmEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextWarmSeq++;
	pEvent = (DemoWarmEvent*)xrtListSet(pCenter->pWarmLog, pCenter->iNextWarmSeq, &bNew);
	if ( pEvent == NULL ) return FALSE;

	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), "policy-hit-threshold");
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->iWarmHits = pCold->iWarmHits;
	pEvent->tFirstWarmHitAt = pCold->tFirstWarmHitAt;
	pEvent->tLastWarmHitAt = pCold->tLastWarmHitAt;
	pEvent->tWarmedAt = xrtNow();
	return TRUE;
}

static bool procInit(DemoAutoWarmCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iWarmWindowMs = 400u;
	pCenter->iWarmMinHits = 2u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pCold = xrtListCreate(sizeof(DemoColdEntry), XRT_OBJMODE_SHARED);
	pCenter->pWarmLog = xrtListCreate(sizeof(DemoWarmEvent), XRT_OBJMODE_SHARED);
	pCenter->hMutex = xrtMutexCreate();
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	sSnapshotPath = xrtPathJoin(2, "runtime", "auto-warm-policy.json");

	if ( (pCenter->pHot == NULL) || (pCenter->pCold == NULL) || (pCenter->pWarmLog == NULL) || (pCenter->hMutex == NULL) || (pCenter->hQueue == NULL) || (sSnapshotPath == NULL) ) {
		if ( sSnapshotPath != NULL ) xrtFree(sSnapshotPath);
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoAutoWarmCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));

	if ( (pCenter == NULL) || (sKey == NULL) || (pItem == NULL) ) return FALSE;
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

static bool procMoveHotToColdLocked(DemoAutoWarmCenter* pCenter, const char* sKey)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));
	DemoColdEntry* pEntry = NULL;
	bool bNew = FALSE;

	if ( pItem == NULL ) return FALSE;
	pCenter->iNextColdSeq++;
	pEntry = (DemoColdEntry*)xrtListSet(pCenter->pCold, pCenter->iNextColdSeq, &bNew);
	if ( pEntry == NULL ) {
		xrtFree(pItem);
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pItem->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pItem->sTitle);
	pEntry->iKeyHash = pItem->iKeyHash;
	pEntry->iHeat = pItem->iHeat;
	pEntry->tCooledAt = xrtNow();
	xrtFree(pItem);
	return TRUE;
}

static bool procSubmitAccess(DemoAutoWarmCenter* pCenter, const char* sKey, const char* sSource)
{
	DemoAccessJob* pJob = (DemoAccessJob*)xrtMalloc(sizeof(DemoAccessJob));

	if ( (pCenter == NULL) || (sKey == NULL) || (pJob == NULL) ) return FALSE;
	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sSource, sizeof(pJob->sSource), sSource);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}
	return TRUE;
}

static uint32 procWorker(ptr pArg)
{
	DemoAutoWarmCenter* pCenter = (DemoAutoWarmCenter*)pArg;
	DemoAccessJob* pJob = NULL;
	DemoColdLookup tLookup;
	DemoColdEntry* pCold = NULL;
	DemoHotItem* pHot = NULL;
	xqueue_result iRet;
	xtime tNow;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) break;
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) continue;

		xrtMutexLock(pCenter->hMutex);
		memset(&tLookup, 0, sizeof(tLookup));
		tLookup.sKey = pJob->sKey;
		xrtListWalk(pCenter->pCold, procFindColdWalk, &tLookup);

		if ( tLookup.bFound ) {
			pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, tLookup.iSeq);
			if ( pCold != NULL ) {
				tNow = xrtNow();
				if ( (pCold->tFirstWarmHitAt == 0) || ((tNow - pCold->tFirstWarmHitAt) > (xtime)pCenter->iWarmWindowMs) ) {
					pCold->iWarmHits = 1;
					pCold->tFirstWarmHitAt = tNow;
				} else {
					pCold->iWarmHits++;
				}
				pCold->tLastWarmHitAt = tNow;
				procCopyText(pCold->sLastHitSource, sizeof(pCold->sLastHitSource), pJob->sSource);

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
							(void)procAppendWarmEventLocked(pCenter, pCold);
							(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
							pHot = NULL;
						}
					}
				}
				(void)procWriteSnapshotLocked(pCenter);
			}
		}

		xrtMutexUnlock(pCenter->hMutex);
		if ( pHot != NULL ) xrtFree(pHot);
		xrtFree(pJob);
	}
	return 0u;
}

static bool procFreeHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;
	if ( pItem != NULL ) xrtFree(pItem);
	return FALSE;
}

static void procUnit(DemoAutoWarmCenter* pCenter)
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
	if ( pCenter->pWarmLog != NULL ) xrtListDestroy(pCenter->pWarmLog);
	if ( pCenter->pCold != NULL ) xrtListDestroy(pCenter->pCold);
	if ( pCenter->pHot != NULL ) xrtDictDestroy(pCenter->pHot);
}

int main(void)
{
	DemoAutoWarmCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;

	if ( xrtInit() == NULL ) return 1;
	if ( !procInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 4) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 2) ) goto cleanup;

	xrtMutexLock(tCenter.hMutex);
	(void)procMoveHotToColdLocked(&tCenter, "node:beta");
	(void)procMoveHotToColdLocked(&tCenter, "node:gamma");
	(void)procWriteSnapshotLocked(&tCenter);
	xrtMutexUnlock(tCenter.hMutex);

	(void)procSubmitAccess(&tCenter, "node:beta", "ui-click");
	(void)procSubmitAccess(&tCenter, "node:gamma", "api-peek");
	(void)procSubmitAccess(&tCenter, "node:beta", "ui-click");

	xrtSleep(350u);
	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
	}

cleanup:
	procUnit(&tCenter);
	xrtUnit();
	return 0;
}
```


## 6. Six Critical Boundaries in This Code

### 6.1 Policy-hit state must stay on the cold entry

Each cold entry now carries:

- `iWarmHits`
- `tFirstWarmHitAt`
- `tLastWarmHitAt`
- `sLastHitSource`

So “how many more hits are still needed before automatic warm-back” is one formal state surface, not one worker-local temporary variable.


### 6.2 The request thread only submits access signals

The request path should only:

- detect the cold hit
- enqueue one access signal

The worker should own:

- hit counting
- window resets
- threshold checks
- actual promotion


### 6.3 Automatic warm-back is not “promote whenever one hit appears”

This page intentionally uses:

- `2` hits
- one `400ms` window

That means:

- one occasional hit does not warm an object back
- only consecutive hits inside the active window trigger promotion


### 6.4 The hit counter must reset after the window expires

If the current time is already beyond `tFirstWarmHitAt + warm_window`, the current round must reset to `1`.

Otherwise old hits keep accumulating forever and every cold object eventually warms back by accident.


### 6.5 Policy-driven warm-back still needs one formal event history

Even when the promotion is automatic, the page and snapshot still need to know:

- why the warm-back happened
- how many hits happened before the promotion
- when the current policy window started


### 6.6 `path + file` turn policy state into real system state

`procWriteSnapshotLocked()` exports:

- the current hot tier
- the current cold-tier policy state
- recent automatic warm-back history

into `runtime/auto-warm-policy.json`.

That means automatic warm-back stops being one in-memory side effect and becomes one formal state surface that scripts, dashboards, and exports can consume directly.


## 7. How It Reconnects to HTTP, Templates, and Scripts

### 7.1 `POST /api/access`

It should only:

1. parse the object key
2. detect one cold-tier hit
3. generate one access signal
4. enqueue it into the background queue

The handler should not perform policy checks or hot restoration inline.


### 7.2 `GET /api/dashboard`

The most natural JSON surface has three parts:

1. the current hot-tier summary
2. the current cold tier and policy-hit state
3. recent automatic warm-back history


### 7.3 `GET /dashboard`

The page naturally splits into:

1. one hot-tier table
2. one cold-window panel with current hit counts
3. one recent automatic warm-back timeline


### 7.4 `runtime/auto-warm-policy.json`

Ops scripts, CLI tools, and page exports should prefer this snapshot instead of reading worker-local temporary state directly.


## 8. Common Mistakes

### 8.1 Warming back immediately after one cold-tier hit

That makes the cold tier lose most of its purpose, because every occasional hit pulls an object back into the hot tier.


### 8.2 Keeping accumulated hit counts only in request-thread temporary variables

If accumulated hits never become formal state, the page and snapshot cannot answer:

- how many hits are currently accumulated
- when the current warm window started


### 8.3 Never resetting the hit count after the warm window expires

If old hits keep accumulating forever, the policy stops meaning “sustained heat” and degrades into “everything eventually warms back”.


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. multi-tier storage with explicit hot, cold, and long-lived layers
2. [warm-back and archive coordination](warm-archive-coordination-dashboard.en.md)
3. automatic warm-back plus rolling archive integration

If the tier split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Warm-Back Dashboard](warm-back-dashboard.en.md)
- [Upgrade the Local Console Service into a Rolling Tier Archive Dashboard](rolling-tier-archive-dashboard.en.md)
- [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
