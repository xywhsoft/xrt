# XRT Case Study: Upgrade the Local Console Service into a Multi-Tier Storage Dashboard

> This page is not about writing one cold snapshot file after the hot tier fills up. It targets the more realistic service problem: once one local console service already has hot objects, a short-lived warm tier, on-disk cold snapshots, and archive semantics, how do you turn `dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` into one formal mainline so that hot state, warm state, cold-file state, restore history, and archive history all stay aligned for the page, JSON, logs, and snapshots?

[中文](multi-tier-storage-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

The earlier pages already split parts of the problem:

- hot objects can cool down into a colder lane
- cold state can warm back into the hot tier
- older cold state can continue into archive

But one real service usually needs an additional middle boundary:

- some objects left the hot tier, but they are still too recent to write to disk immediately
- some objects already left the in-memory path and should now restore from formal cold snapshot files
- some cold snapshots have become historical enough that only archive summaries should remain on the page

So the real problem is not "one more tier name". It is:

> how to split hot, warm, cold-file, and archive responsibilities so that restore and demotion paths stay stable once objects really start crossing both memory and filesystem boundaries.


## 2. Why Memory and Filesystem Cannot Stay Blurred Together

If warm state and cold-file state keep sharing one vague "cold tier", several problems appear quickly:

- the service loses the boundary between "still in memory" and "must restore from disk"
- warm restore and cold-file restore grow into two half-independent paths with no shared model
- page rendering starts mixing in-memory scans and directory scans just to explain current state
- later upgrades into multi-level aging become much harder than they should be

The key split in this page is:

- `warm`
	- answers "left the hot tier, but still worth keeping in memory briefly"
- `cold file`
	- answers "no longer in the in-memory path, restore from a formal snapshot file"
- `archive`
	- answers "no longer on the current restore mainline"


## 3. What Each Layer Owns

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot tier | active lookups and current live hits |
| `dict` | current warm tier | recently cooled objects still kept in memory |
| `list` | current cold catalog | formal on-disk cold snapshots and restore sources |
| `list` | recent restore history | page timeline and restore event export |
| `list` | recent archive history | page timeline and archive event export |
| `queue + thread` | background `TO_WARM / FLUSH_COLD / ACCESS / SWEEP` | keeps request threads out of migration and file persistence |
| `time` | demotion, flush, restore, and archive timestamps | formal cross-tier timing boundaries |
| `path + file` | cold file paths and dashboard snapshot paths | moves the cold tier into the filesystem |
| `xvalue + json` | dashboard summary and cold snapshot content | lets the page, scripts, and restore logic share one model |
| `xhttpd + template` | dashboard output | one browser and script-facing entry |


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the outputs into multi-tier storage semantics:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/multi-tier-storage.json
runtime/cold/<key>.json
```

Where:

- `config/console.json`
	- keeps demotion thresholds, sweep interval, and cold retention policy
- `web/dashboard.html`
	- shows current hot, warm, cold, restore-history, and archive-history state
- `runtime/console.log`
	- records demotion, cold flush, restore, and archive actions
- `runtime/multi-tier-storage.json`
	- stores the latest multi-tier summary
- `runtime/cold/<key>.json`
	- stores one formal cold snapshot file per object


## 5. Compact Core: Warm Restore First, Cold File Restore Second

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the boundaries that matter most:

1. hot and warm are still in-memory tiers with different timing semantics
2. the cold catalog points at formal on-disk restore sources
3. restore should first try warm state, then cold-file state
4. archive only starts after cold-file state stops being part of the active restore mainline

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
	char sTierReason[48];
} DemoWarmItem;

typedef struct
{
	xtime tFlushedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sColdPath[260];
} DemoColdRecord;

typedef struct
{
	xtime tAt;
	uint64 iKeyHash;
	char sKey[64];
	char sFromTier[16];
	char sReason[48];
} DemoStateEvent;

typedef struct
{
	xdict pHot;
	xdict pWarm;
	xlist pCold;
	xlist pRestore;
	xlist pArchive;
	xmpscqwait hQueue;
	xthread hWorker;
	char sSnapshotPath[260];
} DemoTierCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildColdPath(char* sDst, size_t iCap, const char* sKey)
{
	char sFile[96];

	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sFile, sizeof(sFile), "%s.json", (sKey != NULL) ? sKey : "unknown");
	snprintf(sDst, iCap, "runtime/cold/%s", sFile);
}

static const char* procChooseRestoreRoute(bool bHasWarm, bool bHasColdFile)
{
	if ( bHasWarm ) {
		return "restore_from_warm";
	}

	if ( bHasColdFile ) {
		return "restore_from_cold_file";
	}

	return "miss";
}

static void procRecordEvent(DemoStateEvent* pEvent, const char* sKey, const char* sFromTier, const char* sReason, xtime tNow)
{
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->tAt = tNow;
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sFromTier, sizeof(pEvent->sFromTier), sFromTier);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
}

int main(void)
{
	DemoTierCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoColdRecord tGammaCold;
	DemoStateEvent tRestore;
	const char* sRoute;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(&tGammaCold, 0, sizeof(tGammaCold));
	memset(&tRestore, 0, sizeof(tRestore));

	procCopyText(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "runtime/multi-tier-storage.json");

	tBetaWarm.iKeyHash = 1002u;
	tBetaWarm.iHeat = 5;
	tBetaWarm.tWarmedAt = 1200;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "Beta Warm");
	procCopyText(tBetaWarm.sTierReason, sizeof(tBetaWarm.sTierReason), "recently_demoted");

	tGammaCold.iKeyHash = 1003u;
	tGammaCold.iHeat = 2;
	tGammaCold.tFlushedAt = 1800;
	procCopyText(tGammaCold.sKey, sizeof(tGammaCold.sKey), "gamma");
	procCopyText(tGammaCold.sTitle, sizeof(tGammaCold.sTitle), "Gamma Cold");
	procBuildColdPath(tGammaCold.sColdPath, sizeof(tGammaCold.sColdPath), tGammaCold.sKey);

	sRoute = procChooseRestoreRoute(TRUE, FALSE);
	procRecordEvent(&tRestore, "beta", "warm", sRoute, 2000);
	printf("%s -> %s\n", tRestore.sKey, tRestore.sReason);

	sRoute = procChooseRestoreRoute(FALSE, TRUE);
	procRecordEvent(&tRestore, "gamma", "cold_file", sRoute, 2200);
	printf("%s -> %s (%s)\n", tRestore.sKey, tRestore.sReason, tGammaCold.sColdPath);

	printf("snapshot: %s\n", tCenter.sSnapshotPath);
	return 0;
}
```


## 6. What to Read Next

After this page, the next natural upgrades are:

- [Upgrade the Local Console Service into a Hot-Cold Multi-Level Aging Dashboard](hot-cold-multi-level-aging-dashboard.en.md)
- [Upgrade the Local Console Service into a Recovery Priority Dashboard](recovery-priority-dashboard.en.md)
- [Upgrade the Local Console Service into a Warm-Archive Coordination Dashboard](warm-archive-coordination-dashboard.en.md)
- [Upgrade the Local Console Service into an Auto-Warm + Rolling-Archive Integration Dashboard](auto-warm-rolling-archive-dashboard.en.md)
