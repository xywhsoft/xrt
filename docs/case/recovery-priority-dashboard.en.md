# XRT Case Study: Upgrade the Local Console Service into a Recovery Priority Dashboard

> This page is not about writing `if ( warm ) ... else if ( cool ) ... else read file` in one request handler. It targets the more realistic service problem: once one local console service already has hot, warm, cool, cold-file, and archive semantics, how do you turn `dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` into one formal mainline so that request threads only submit restore intent, the worker chooses `warm / cool / cold file / defer`, and the page, JSON, logs, and snapshots can all explain why that restore path was chosen?

[中文](recovery-priority-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

The earlier pages already established these boundaries:

- hot, warm, cool, and cold-file tiers can coexist
- multi-tier storage can keep recovery sources across memory and filesystem
- multi-level aging can move objects through multiple cooling windows

But a real service quickly hits one more requirement:

- the same key may still have a warm shadow
- or a cooler in-memory shadow
- or only one cold-file snapshot
- the request thread only knows "restore this key", but should not hard-code the restore path
- the page and JSON still need to explain why the service chose warm, cool, cold file, or defer

So the real problem is no longer "which branch happens to run first". It is:

> how to turn recovery priority itself into one formal, explainable, exportable background mainline.


## 2. Why This Cannot Stay as "Find the First Match and Restore It"

If recovery keeps degrading into plain branch order, several problems appear quickly:

- `warm`, `cool`, and `cold file` stop being different cost classes and turn into one vague pile of fallbacks
- freshness windows for warm and cool state no longer share one formal policy boundary
- cold-file recovery cost and deadlines cannot be expressed cleanly
- request threads start carrying file restore and downgrade decisions that belong in the worker
- the page must scan logs or worker-local scratch state just to explain one deferred restore

The key boundary in this page is one sentence:

> request threads submit restore intent; the worker owns recovery priority, restore selection, and defer decisions.


## 3. What Each Layer Owns

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot tier | live lookups after successful restore |
| `dict` | current warm tier | cheapest and highest-priority recovery source |
| `dict` | current cool tier | second-priority in-memory recovery source |
| `list` | current cold catalog | formal file-backed recovery source |
| `list` | recent restore history | page timeline and restore-event export |
| `list` | recent defer history | page timeline and "why not cold file yet" explanations |
| `queue + thread` | background `RESTORE / SWEEP` convergence | keeps request threads out of restore selection |
| `time` | warm/cool freshness and restore deadlines | formal priority windows |
| `path + file` | cold-file paths and dashboard snapshots | moves file-backed recovery into the filesystem |
| `hash` | stable key fingerprint | lightweight IDs for pages, logs, and snapshots |
| `xvalue + json` | current tier, restore, and defer summaries | one shared model for the page, scripts, and snapshots |
| `xhttpd + template` | dashboard output | one browser and script-facing entry |


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the outputs into recovery-priority semantics:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/recovery-priority.json
runtime/cold/<hash>.json
```

Where:

- `config/console.json`
	- keeps `warm_fresh_ms`, `cool_fresh_ms`, and `max_cold_file_cost`
- `web/dashboard.html`
	- shows current hot, warm, cool, cold, restore history, and defer history
- `runtime/console.log`
	- records restore signals, selected restore sources, and defer reasons
- `runtime/recovery-priority.json`
	- stores the latest recovery-priority snapshot
- `runtime/cold/<hash>.json`
	- stores one cold snapshot file per object


## 5. Compact Core: Restore Signal, Priority Selection, Restore or Defer

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the boundaries that matter most:

1. request threads submit restore intent instead of hard-coding the path
2. warm and cool windows are real policy fields, not branch order
3. cold-file recovery must still respect cost budget
4. defer is formal state, not just one log line

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_RESTORE 1
#define DEMO_SIGNAL_SWEEP 2

typedef struct
{
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoHotItem;

typedef struct
{
	xtime tTouchedAt;
	xtime tWarmedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoWarmItem;

typedef struct
{
	xtime tTouchedAt;
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
	uint32 iFileCost;
	int iHeat;
	char sKey[64];
	char sColdPath[260];
} DemoColdRecord;

typedef struct
{
	xtime tRestoredAt;
	uint64 iKeyHash;
	char sKey[64];
	char sFromTier[16];
	char sReason[48];
} DemoRestoreEvent;

typedef struct
{
	xtime tDeferredAt;
	uint64 iKeyHash;
	char sKey[64];
	char sReason[48];
} DemoDeferredEvent;

typedef struct
{
	uint32 iWarmFreshMs;
	uint32 iCoolFreshMs;
	uint32 iMaxColdFileCost;
} DemoRecoveryPolicy;

typedef struct
{
	int iKind;
	char sKey[64];
	char sClient[48];
} DemoSignal;

typedef struct
{
	xdict pHot;
	xdict pWarm;
	xdict pCool;
	xlist pCold;
	xlist pRestore;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoRecoveryPolicy tPolicy;
	char sSnapshotPath[260];
} DemoRecoveryCenter;


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

static bool procWarmUsable(const DemoWarmItem* pWarm, const DemoRecoveryPolicy* pPolicy, xtime tNow)
{
	if ( (pWarm == NULL) || (pPolicy == NULL) ) {
		return FALSE;
	}

	if ( tNow < pWarm->tTouchedAt ) {
		return FALSE;
	}

	return ((uint32)(tNow - pWarm->tTouchedAt) <= pPolicy->iWarmFreshMs);
}

static bool procCoolUsable(const DemoCoolItem* pCool, const DemoRecoveryPolicy* pPolicy, xtime tNow)
{
	if ( (pCool == NULL) || (pPolicy == NULL) ) {
		return FALSE;
	}

	if ( tNow < pCool->tTouchedAt ) {
		return FALSE;
	}

	return ((uint32)(tNow - pCool->tTouchedAt) <= pPolicy->iCoolFreshMs);
}

static bool procColdUsable(const DemoColdRecord* pCold, const DemoRecoveryPolicy* pPolicy)
{
	if ( (pCold == NULL) || (pPolicy == NULL) ) {
		return FALSE;
	}

	return (pCold->iFileCost <= pPolicy->iMaxColdFileCost);
}

static const char* procChooseRestoreSource(
	const DemoWarmItem* pWarm,
	const DemoCoolItem* pCool,
	const DemoColdRecord* pCold,
	const DemoRecoveryPolicy* pPolicy,
	xtime tNow
)
{
	if ( procWarmUsable(pWarm, pPolicy, tNow) ) {
		return "restore_from_warm";
	}

	if ( procCoolUsable(pCool, pPolicy, tNow) ) {
		return "restore_from_cool";
	}

	if ( procColdUsable(pCold, pPolicy) ) {
		return "restore_from_cold_file";
	}

	if ( pCold != NULL ) {
		return "defer_cold_file";
	}

	return "miss";
}

static void procRecordRestore(DemoRestoreEvent* pEvent, const char* sKey, const char* sFromTier, const char* sReason, xtime tNow)
{
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->tRestoredAt = tNow;
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sFromTier, sizeof(pEvent->sFromTier), sFromTier);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
}

static void procRecordDeferred(DemoDeferredEvent* pEvent, const char* sKey, const char* sReason, xtime tNow)
{
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->tDeferredAt = tNow;
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
}

int main(void)
{
	DemoRecoveryCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoWarmItem tGammaWarm;
	DemoCoolItem tGammaCool;
	DemoColdRecord tDeltaCold;
	DemoColdRecord tEpsilonCold;
	DemoRestoreEvent tRestore;
	DemoDeferredEvent tDeferred;
	const char* sRoute;
	xtime tNow = 5000;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(&tGammaWarm, 0, sizeof(tGammaWarm));
	memset(&tGammaCool, 0, sizeof(tGammaCool));
	memset(&tDeltaCold, 0, sizeof(tDeltaCold));
	memset(&tEpsilonCold, 0, sizeof(tEpsilonCold));
	memset(&tRestore, 0, sizeof(tRestore));
	memset(&tDeferred, 0, sizeof(tDeferred));

	tCenter.tPolicy.iWarmFreshMs = 300u;
	tCenter.tPolicy.iCoolFreshMs = 1000u;
	tCenter.tPolicy.iMaxColdFileCost = 8u;
	procCopyText(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "runtime/recovery-priority.json");

	tBetaWarm.tTouchedAt = tNow - 100;
	tBetaWarm.iKeyHash = 1002u;
	tBetaWarm.iHeat = 8;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "Beta Warm");

	tGammaWarm.tTouchedAt = tNow - 800;
	tGammaWarm.iKeyHash = 1003u;
	tGammaWarm.iHeat = 4;
	procCopyText(tGammaWarm.sKey, sizeof(tGammaWarm.sKey), "gamma");
	procCopyText(tGammaWarm.sTitle, sizeof(tGammaWarm.sTitle), "Gamma Warm Shadow");

	tGammaCool.tTouchedAt = tNow - 200;
	tGammaCool.iKeyHash = 1003u;
	tGammaCool.iHeat = 3;
	procCopyText(tGammaCool.sKey, sizeof(tGammaCool.sKey), "gamma");
	procCopyText(tGammaCool.sTitle, sizeof(tGammaCool.sTitle), "Gamma Cool");

	tDeltaCold.tFlushedAt = tNow - 1500;
	tDeltaCold.iKeyHash = 1004u;
	tDeltaCold.iFileCost = 4u;
	tDeltaCold.iHeat = 1;
	procCopyText(tDeltaCold.sKey, sizeof(tDeltaCold.sKey), "delta");
	procBuildColdPath(tDeltaCold.sColdPath, sizeof(tDeltaCold.sColdPath), tDeltaCold.sKey);

	tEpsilonCold.tFlushedAt = tNow - 1500;
	tEpsilonCold.iKeyHash = 1005u;
	tEpsilonCold.iFileCost = 12u;
	tEpsilonCold.iHeat = 1;
	procCopyText(tEpsilonCold.sKey, sizeof(tEpsilonCold.sKey), "epsilon");
	procBuildColdPath(tEpsilonCold.sColdPath, sizeof(tEpsilonCold.sColdPath), tEpsilonCold.sKey);

	sRoute = procChooseRestoreSource(&tBetaWarm, NULL, NULL, &tCenter.tPolicy, tNow);
	procRecordRestore(&tRestore, "beta", "warm", sRoute, tNow);
	printf("%s -> %s\n", tRestore.sKey, tRestore.sReason);

	sRoute = procChooseRestoreSource(&tGammaWarm, &tGammaCool, NULL, &tCenter.tPolicy, tNow);
	procRecordRestore(&tRestore, "gamma", "cool", sRoute, tNow);
	printf("%s -> %s\n", tRestore.sKey, tRestore.sReason);

	sRoute = procChooseRestoreSource(NULL, NULL, &tDeltaCold, &tCenter.tPolicy, tNow);
	procRecordRestore(&tRestore, "delta", "cold_file", sRoute, tNow);
	printf("%s -> %s (%s)\n", tRestore.sKey, tRestore.sReason, tDeltaCold.sColdPath);

	sRoute = procChooseRestoreSource(NULL, NULL, &tEpsilonCold, &tCenter.tPolicy, tNow);
	if ( strcmp(sRoute, "defer_cold_file") == 0 ) {
		procRecordDeferred(&tDeferred, "epsilon", "cold_file_cost_too_high", tNow);
		printf("%s -> %s (%s)\n", tDeferred.sKey, sRoute, tDeferred.sReason);
	} else {
		printf("unexpected route: %s\n", sRoute);
	}

	printf("snapshot: %s\n", tCenter.sSnapshotPath);
	return 0;
}
```


## 6. What to Read Next

After this page, the next natural comparisons are:

- [Upgrade the Local Console Service into a Hot-Cold Multi-Level Aging Dashboard](hot-cold-multi-level-aging-dashboard.en.md)
- [Upgrade the Local Console Service into a Multi-Tier Storage Dashboard](multi-tier-storage-dashboard.en.md)
- [Upgrade the Local Console Service into a Cross-Media Orchestration Dashboard](cross-media-orchestration-dashboard.en.md)
