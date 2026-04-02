# XRT Case Study: Upgrade the Local Console Service into a Hot-Cold Multi-Level Aging Dashboard

> This page is not about adding one more mid-tier name. It targets the more realistic service problem: once one local console service already has a hot tier, multiple in-memory aging windows, formal cold files, and archive semantics, how do you turn `dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` into one formal mainline so that `hot -> warm -> cool -> cold file -> archive` remains stable for the page, JSON, logs, snapshots, and restore paths?

[中文](hot-cold-multi-level-aging-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

The previous page already introduced multi-tier storage:

- hot objects stay on the current fast path
- warm objects are still in memory
- cold objects restore from files
- old cold state can eventually become archive-only history

But some services still need a finer aging ladder:

- some objects just left the hot tier and should stay very close to fast restore
- some objects already left that first warm window, but still should not write to disk immediately
- only the colder tail should flush into snapshot files
- restore should first try the cheaper in-memory tiers before touching files

So the real problem is no longer "warm versus cold". It is:

> how to make `hot -> warm -> cool -> cold file -> archive` one stable aging and restore ladder instead of one vague pile of mid-tier state.


## 2. Why Multi-Tier Storage Is Still Not Enough

Multi-tier storage already solves:

- crossing memory and filesystem boundaries
- keeping warm state different from cold-file state

But it does not fully solve:

- whether one recently cooled object should still be closer to the hot tier than another colder in-memory object
- which restore path should be tried first among multiple in-memory stages
- how one sweep decides `warm -> cool -> cold file -> archive` without collapsing all mid-tier state back together

The key split here is:

- `warm`
	- closest in-memory stop after leaving the hot tier
- `cool`
	- colder in-memory stop before formal file flush
- `cold file`
	- formal on-disk restore source


## 3. What Each Layer Owns

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot tier | current live hits |
| `dict` | current warm tier | fastest in-memory restore after cooling |
| `dict` | current cool tier | cheaper in-memory holding area before disk |
| `list` | current cold catalog | formal file-backed restore sources |
| `list` | recent restore history | page timeline and restore event export |
| `list` | recent archive history | page timeline and archive event export |
| `queue + thread` | background `TO_WARM / ACCESS / SWEEP` convergence | keeps request threads out of aging transitions |
| `time` | warm/cool/cold/archive timing boundaries | makes aging windows explicit |
| `path + file` | cold file paths and snapshot output | moves the coldest tier into the filesystem |
| `xvalue + json` | current tier summary | lets the page, scripts, and snapshots share one model |
| `xhttpd + template` | dashboard output | one browser and script-facing entry |


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the outputs into multi-level aging semantics:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/hot-cold-multi-level-aging.json
runtime/cold/<key>.json
```

Where:

- `config/console.json`
	- keeps `warm_age_ms`, `cool_age_ms`, and `archive_after_ms`
- `web/dashboard.html`
	- shows current hot, warm, cool, cold, restore-history, and archive-history state
- `runtime/console.log`
	- records aging transitions, restores, and cold-file flushes
- `runtime/hot-cold-multi-level-aging.json`
	- stores the latest multi-level aging summary
- `runtime/cold/<key>.json`
	- stores one cold snapshot file per object


## 5. Compact Core: Restore Order Must Match Aging Order

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the boundaries that matter most:

1. one object ages from `hot` into `warm`, then `cool`, then `cold file`
2. restore tries `warm` first, then `cool`, and only then `cold file`
3. archive starts only after the cold-file state stops being part of the active restore ladder
4. request threads still only submit signals while the worker owns transitions

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoHotItem;

typedef struct
{
	xtime tWarmedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoWarmItem;

typedef struct
{
	xtime tCooledAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoCoolItem;

typedef struct
{
	xtime tFlushedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sColdPath[260];
} DemoColdRecord;

typedef struct
{
	xdict pHot;
	xdict pWarm;
	xdict pCool;
	xlist pCold;
	xlist pRestore;
	xlist pArchive;
	xmpscqwait hQueue;
	xthread hWorker;
	char sSnapshotPath[260];
} DemoAgingCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildColdPath(char* sDst, size_t iCap, const char* sKey)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/cold/%s.json", (sKey != NULL) ? sKey : "unknown");
}

static const char* procChooseRestoreRoute(bool bHasWarm, bool bHasCool, bool bHasColdFile)
{
	if ( bHasWarm ) {
		return "restore_from_warm";
	}

	if ( bHasCool ) {
		return "restore_from_cool";
	}

	if ( bHasColdFile ) {
		return "restore_from_cold_file";
	}

	return "miss";
}

static const char* procChooseNextTier(bool bExpiredWarmWindow, bool bExpiredCoolWindow)
{
	if ( !bExpiredWarmWindow ) {
		return "stay_warm";
	}

	if ( !bExpiredCoolWindow ) {
		return "move_to_cool";
	}

	return "flush_to_cold_file";
}

int main(void)
{
	DemoAgingCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoCoolItem tGammaCool;
	DemoColdRecord tDeltaCold;
	const char* sRoute;
	const char* sNext;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(&tGammaCool, 0, sizeof(tGammaCool));
	memset(&tDeltaCold, 0, sizeof(tDeltaCold));

	procCopyText(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "runtime/hot-cold-multi-level-aging.json");

	tBetaWarm.iKeyHash = 1002u;
	tBetaWarm.iHeat = 6;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "Beta Warm");

	tGammaCool.iKeyHash = 1003u;
	tGammaCool.iHeat = 4;
	procCopyText(tGammaCool.sKey, sizeof(tGammaCool.sKey), "gamma");
	procCopyText(tGammaCool.sTitle, sizeof(tGammaCool.sTitle), "Gamma Cool");

	tDeltaCold.iKeyHash = 1004u;
	tDeltaCold.iHeat = 1;
	procCopyText(tDeltaCold.sKey, sizeof(tDeltaCold.sKey), "delta");
	procBuildColdPath(tDeltaCold.sColdPath, sizeof(tDeltaCold.sColdPath), tDeltaCold.sKey);

	sRoute = procChooseRestoreRoute(TRUE, FALSE, FALSE);
	printf("beta -> %s\n", sRoute);

	sRoute = procChooseRestoreRoute(FALSE, TRUE, FALSE);
	printf("gamma -> %s\n", sRoute);

	sRoute = procChooseRestoreRoute(FALSE, FALSE, TRUE);
	printf("delta -> %s (%s)\n", sRoute, tDeltaCold.sColdPath);

	sNext = procChooseNextTier(TRUE, FALSE);
	printf("warm aging decision -> %s\n", sNext);

	sNext = procChooseNextTier(TRUE, TRUE);
	printf("cool aging decision -> %s\n", sNext);
	printf("snapshot: %s\n", tCenter.sSnapshotPath);
	return 0;
}
```


## 6. What to Read Next

After this page, the next natural upgrades are:

- [Upgrade the Local Console Service into a Recovery Priority Dashboard](recovery-priority-dashboard.en.md)
- [Upgrade the Local Console Service into a Multi-Tier Storage Dashboard](multi-tier-storage-dashboard.en.md)
- [Upgrade the Local Console Service into a Warm-Archive Coordination Dashboard](warm-archive-coordination-dashboard.en.md)
- [Upgrade the Local Console Service into an Auto-Warm + Rolling-Archive Integration Dashboard](auto-warm-rolling-archive-dashboard.en.md)
