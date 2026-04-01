# XRT Case Study: Upgrade the Local Console Service into a Hot-Cold Tier Dashboard

> This case is not about "what to do after one active object finishes". It targets another very common service problem that becomes messy quickly if left implicit: when one local console service must answer "what is hot right now", keep a stable set of cooled-but-still-valid objects, and write that cold-tier state into files for dashboards, JSON, and scripts, how do you turn `dict + list + queue + thread + time + path + file + hash + xhttpd + template` into one formal mainline instead of letting hot objects, cold snapshots, file outputs, and page rendering each maintain a half-synchronized copy of the state?

[中文](hot-cold-tier-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have the earlier local console service, and you also already rebuilt the archive dashboard:

- current active objects live in memory
- recent history can be shown on the page
- slow work is moved into a background worker
- snapshot files are written into `runtime/`

But once the system grows a little further, another requirement appears very quickly:

- some objects are not "finished"
- they are simply no longer worth occupying the hot tier
- the page still needs to show them
- JSON and ops scripts still need to read them
- only their lookup priority, update frequency, and storage location should now differ from the hot tier

If you keep forcing everything into one hot table, four consequences appear almost immediately:

- the hot tier keeps growing and the page/API pay the cost for cold data
- "currently hot" and "recently reviewable" have no formal boundary
- file snapshots start walking the live hot table directly, so the cold tier has no independent model
- once you need warm-back promotion, rolling cold windows, or multi-tier storage, the whole model has to be rewritten

So this page is not really about "how to write one object to a file". It is about:

> how to split the hot tier, the cold tier, the cooling path, and the cold snapshot into four stable lines once the system truly needs hot-cold separation.


## 2. Why the Archive Dashboard Is Not Enough

### 2.1 Archive means finished, cold tier means still valid

In the archive dashboard, an object entering the archive usually means:

- it is finished
- it only keeps historical meaning

That is not the meaning of hot-cold tiering.

Objects in the cold tier are closer to:

- no longer worth staying in the hot tier
- still valid records
- potentially promoted back into the hot tier later

So "archive" and "cool down" must not be treated as the same state.


### 2.2 The hot tier optimizes live hits, the cold tier optimizes stable snapshots

The hot tier is best suited to answer:

- whether this key is currently hot
- what the latest live state is
- what should appear in the page header or live summary

The cold tier is better suited to answer:

- which objects cooled down recently
- what their last hot-state snapshot was
- what the current cold-tier snapshot file should contain

These are not the same questions, so they should not continue sharing one vague structure.


### 2.3 The cold tier is not a log and not a temporary copy

Once the page, JSON, and snapshot files all start consuming the cold tier, it should become:

- one formal stable value model

not:

- something reconstructed from logs
- something copied out of worker-local memory
- something still pointing at live hot objects

That is exactly where `list + path + file` should formally enter the mainline.


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `dict` | stores current hot objects | fast current-state lookup |
| `list` | stores recent cold-tier records | recent cooling timeline and cold export |
| `queue + thread` | performs cooling in the background | keeps request threads out of tier migration and file I/O |
| `time` | records last hot touch and cooling time | makes the hot/cold boundary explicit in time |
| `hash` | fingerprints each object key | short stable identifiers for pages, logs, and snapshots |
| `path + file` | builds and writes cold snapshot paths | turns the cold tier into real filesystem state |
| `xvalue + json` | exports the hot-cold summary | lets page, scripts, and snapshots share one model |
| `xhttpd + template` | shows both tiers | gives browsers and scripts one shared surface |

One-sentence version:

> `dict` owns "what is hot right now", `list` owns "what cooled recently", and the worker owns "when one hot object is formally moved into the cold tier and persisted".


## 4. File Layout and Outputs

This case keeps the local-console-service layout and only changes the output meaning into hot-cold tiering:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/hot-cold.json
```

Where:

- `config/console.json`
	- keeps hot-tier capacity, cold retention count, and cooling thresholds
- `web/dashboard.html`
	- renders both hot items and recent cold items
- `runtime/console.log`
	- records cooling requests and snapshot writes
- `runtime/hot-cold.json`
	- stores the latest hot-cold summary


## 5. Compact Core: Hot Tier, Cold Tier, Cooling Queue

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the four boundaries that matter most:

1. `dict` stores current hot objects
2. `list` stores recent cold records
3. `queue + thread` moves cooling work into the background
4. `path + file + json` writes the cold summary into `runtime/hot-cold.json`

It deliberately stays practical:

- initialize the hot-cold center
- register several hot objects
- submit two cooling requests
- let the worker move objects from hot to cold
- rewrite the snapshot after each cooling step
- print the current hot tier, recent cold tier, and final snapshot content

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
	char sKey[64];
	char sTierReason[48];
} DemoTierJob;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	uint32 iKeepCold;
	char sSnapshotPath[260];
} DemoHotColdCenter;


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

static bool procWriteSnapshotLocked(DemoHotColdCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHotArr = NULL;
	xvalue vColdArr = NULL;
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

	xrtDictWalk(pCenter->pHot, procCollectHotSnapshot, vHotArr);
	xrtListWalk(pCenter->pCold, procCollectColdSnapshot, vColdArr);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHotArr, TRUE);
	xvoUnref(vHotArr);
	vHotArr = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vColdArr, TRUE);
	xvoUnref(vColdArr);
	vColdArr = NULL;

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

static bool procAppendColdLocked(DemoHotColdCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
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

static bool procHotColdCenterInit(DemoHotColdCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 8u;

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

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hMutex = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	sSnapshotPath = xrtPathJoin(2, "runtime", "hot-cold.json");
	if ( sSnapshotPath == NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hQueue = NULL;
		pCenter->hMutex = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoHotColdCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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
	xrtMutexLock(pCenter->hMutex);
	pCenter->iNextHotId++;
	pItem->iHotId = pCenter->iNextHotId;
	pItem->tTouchedAt = xrtNow();
	pItem->iKeyHash = xrtHash64((ptr)sKey, strlen(sKey));
	pItem->iHeat = iHeat;
	procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);
	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
		xrtMutexUnlock(pCenter->hMutex);
		xrtFree(pItem);
		return FALSE;
	}
	xrtMutexUnlock(pCenter->hMutex);
	return TRUE;
}

static bool procSubmitCool(DemoHotColdCenter* pCenter, const char* sKey, const char* sTierReason)
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

static uint32 procTierWorker(ptr pArg)
{
	DemoHotColdCenter* pCenter = (DemoHotColdCenter*)pArg;
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

static void procHotColdCenterUnit(DemoHotColdCenter* pCenter)
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
	DemoHotColdCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procHotColdCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procTierWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procHotColdCenterUnit(&tCenter);
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

	if ( !procSubmitCool(&tCenter, "node:beta", "idle-window") ) {
		goto cleanup;
	}
	if ( !procSubmitCool(&tCenter, "node:gamma", "cooldown-batch") ) {
		goto cleanup;
	}

	xrtSleep(300u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);

	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
		sSnapshot = NULL;
	}

	iRet = 0;

cleanup:
	procHotColdCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. Six Things That Matter Most

### 6.1 The hot tier and the cold tier are not "current plus history" in one structure

This case keeps:

- `pHot`
	- pointers to current hot objects
- `pCold`
	- stable value records after cooling

That means the cold tier is no longer pointing back into live hot objects. It is already:

- directly exportable
- directly renderable
- directly persistable

as a formal model.


### 6.2 The cold tier is not the archive

`procAppendColdLocked()` does not mean "the object has finished". It means:

- keep its last hot-state snapshot
- record why it cooled down
- move it from the hot tier into the cold tier

If you later add warm-back promotion, it should happen from the cold tier back into the hot tier, not through the archive.


### 6.3 The worker owns tier migration, handlers only enqueue

One of the most important boundaries in this page is:

- HTTP handlers only submit cool requests
- the worker actually performs the hot-to-cold migration

So later additions such as:

- file output
- rolling cold windows
- warm-back checks
- extra notifications

do not drag request threads back into the slow path.


### 6.4 `path + file` turns the cold tier into persisted state

`procWriteSnapshotLocked()` performs three steps:

1. build one hot-cold summary with `xvalue + json`
2. ensure the directory exists with `xrtPathGetDir()` + `xrtDirCreateAll()`
3. write `runtime/hot-cold.json` with `xrtFileWriteAll()`

Only after that step does the cold tier stop being "just an in-memory idea" and become one real system layer.


### 6.5 `hash` is only one stable fingerprint

`iKeyHash` exists here for:

- shorter object identifiers on the page
- stable lightweight identifiers in logs and snapshots

It is not:

- authentication
- a secret
- a security token


### 6.6 This page intentionally stops before warm-back

The mainline here ends at:

- hot -> cold

not:

- hot -> cold -> warm back -> hot

because if the hot/cold boundaries are not stable yet, warm-back logic only makes the model harder to reason about.


## 7. How It Reconnects to HTTP, Templates, and Scripts

### 7.1 `POST /api/cool`

It should only:

1. parse the object key
2. choose a cooling reason
3. enqueue the background request
4. return "accepted" immediately

The handler should not mutate the hot tier or the cold tier directly.


### 7.2 `GET /api/dashboard`

The natural JSON surface is two parts:

1. current hot-tier summary
2. recent cold-tier summary

These map directly onto:

- `dict`
- `list`


### 7.3 `GET /dashboard`

The page naturally splits into:

1. hot object cards or a hot table
2. a recent cold-tier timeline
3. current snapshot path and generation time

That is why the template does not need to synthesize cold data out of logs or scattered files.


### 7.4 `runtime/hot-cold.json`

Ops scripts, CLI tools, and panel exports should prefer this snapshot instead of reading worker-local temporary memory.

That is the point where hot-cold tiering becomes:

- one formal state layer


## 8. Common Mistakes

### 8.1 Treating the cold tier as the archive again

That makes later features such as:

- warm-back promotion
- rolling cold windows
- multi-tier storage

much harder, because the state meaning is wrong from the beginning.


### 8.2 Letting the page walk the hot tier to reconstruct cold data

If the cold tier has no independent model, the page gradually turns into:

- fields copied from the hot tier
- fields reconstructed from logs
- fields reconstructed from files

At that point nobody can clearly say which state is authoritative.


### 8.3 Writing snapshot files directly on the request thread

Once hot-cold tiering really starts persisting data, files, templates, JSON, and logs all tend to enter the same path.  
If those actions still happen inside request threads, the dashboard degrades very quickly.


## 9. What To Read Next

If you keep moving up the business layer, the next natural directions are:

1. warm-back promotion from the cold tier
2. rolling cold-tier archiving
3. multi-tier storage with hot, cold, and long-term layers

If the tier split is still not fully stable yet, read these together:

- [Upgrade the Local Console Service into a Warm-Back Dashboard](warm-back-dashboard.en.md)
- [Upgrade the Local Console Service into a Cache Refresh Dashboard](cache-refresh-dashboard.en.md)
- [Upgrade the Local Console Service into an Archive Dashboard](archive-dashboard.en.md)
- [Queue Intro: When to Hand Off Messages Instead of Sharing State](../guide/queue-intro.en.md)
