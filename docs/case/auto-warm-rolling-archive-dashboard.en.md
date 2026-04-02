# XRT Case Study: Upgrade the Local Console Service into an Auto-Warm + Rolling-Archive Integration Dashboard

> This page is not about placing automatic warm-back and rolling archive side by side. It targets the more realistic service problem: once one local console service already has a hot tier, a cold window, automatic warm-back rules, and archive semantics, how do you converge `COOL`, `ACCESS`, and `SWEEP` into one formal `dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` mainline so that the current hot state, current cold window, recent warm-back history, and recent archive history all stay aligned for the page, JSON, logs, and snapshots?

[中文](auto-warm-rolling-archive-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

The earlier pages already covered these lines one by one:

- cold entries can warm back automatically after enough hits
- the cold window can keep rolling older entries into archive
- page and snapshot output can expose the current tier state

But once these requirements appear in the same long-lived service, one cold entry now has to answer all of these questions at once:

- did it just cool down into the current cold window
- did it collect enough recent hits to become hot again
- did it stay cold for too long without recovering
- did it overflow the current cold window and need archive compaction

So the real problem is no longer "one page for warm-back" plus "one page for rolling archive". It is:

> how to let one cold-state model choose between staying cold, warming back, or rolling into archive, all inside one worker-owned convergence path.


## 2. Why These Two Lines Cannot Keep Independent State

If automatic warm-back and rolling archive stay in separate logic paths, the model degrades quickly:

- `warm_hits`, `first_warm_hit_at`, and archive eligibility stop sharing one authoritative state source
- the page starts reading logs or worker-local scratch state just to explain why one object warmed back or got archived
- keep-cold limits and warm-threshold windows start fighting each other instead of converging cleanly
- later upgrades into deeper storage tiers or aging ladders become much harder than they should be

The most important boundary in this page is one sentence:

> request threads only submit `COOL`, `ACCESS`, or periodic `SWEEP` signals; one worker decides whether the cold entry stays cold, warms back, or rolls into archive.


## 3. What Each Layer Owns

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot-tier objects | live lookups after warm-back |
| `list` | current cold window | cold entries, hit counters, recent hit source, cooling time |
| `list` | recent warm-back history | page timeline and warm-back event export |
| `list` | recent archive history | page timeline and archive event export |
| `queue + thread` | background `COOL / ACCESS / SWEEP` convergence | keeps request threads out of state migration |
| `time` | cool-down, hit-window, warm-back, and archive timestamps | formal policy boundaries |
| `path + file` | snapshot paths and persistence | moves the integrated state into the filesystem |
| `xvalue + json` | current hot/cold/warm/archive summary | lets the page, scripts, and snapshot share one model |
| `xhttpd + template` | dashboard output | one browser and script-facing entry |


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the snapshot meaning into one integrated warm/archive dashboard:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/auto-warm-rolling-archive.json
```

Where:

- `config/console.json`
	- keeps `keep_cold`, `warm_window_ms`, `warm_min_hits`, and `archive_after_ms`
- `web/dashboard.html`
	- shows the current hot tier, current cold window, recent warm-back history, and recent archive history
- `runtime/console.log`
	- records cool-down submissions, access signals, warm-back decisions, and archive compaction
- `runtime/auto-warm-rolling-archive.json`
	- stores the latest integrated state snapshot


## 5. Compact Core: One Worker Decides Stay-Cold, Warm-Back, or Archive

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the boundaries that matter most:

1. request threads only submit signals
2. one cold-entry state model carries hit counters and cold timing
3. one worker-owned convergence path decides the next destination
4. warm-back and archive histories are exported as separate stable lines

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_JOB_COOL 1
#define DEMO_JOB_ACCESS 2
#define DEMO_JOB_SWEEP 3

typedef struct
{
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
	char sTierReason[48];
	char sLastHitSource[48];
} DemoColdEntry;

typedef struct
{
	xtime tAt;
	uint64 iKeyHash;
	int iWarmHits;
	char sKey[64];
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
	xdict pHot;
	xlist pCold;
	xlist pWarmLog;
	xlist pArchive;
	xmpscqwait hQueue;
	xthread hWorker;
	uint32 iKeepCold;
	uint32 iWarmWindowMs;
	uint32 iWarmMinHits;
	uint32 iArchiveAfterMs;
	char sSnapshotPath[260];
} DemoRollingWarmCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procApplyCool(DemoColdEntry* pEntry, const char* sReason, xtime tNow)
{
	if ( pEntry == NULL ) {
		return;
	}

	pEntry->tCooledAt = tNow;
	pEntry->tFirstWarmHitAt = 0;
	pEntry->tLastWarmHitAt = 0;
	pEntry->iWarmHits = 0;
	procCopyText(pEntry->sTierReason, sizeof(pEntry->sTierReason), sReason);
	procCopyText(pEntry->sLastHitSource, sizeof(pEntry->sLastHitSource), "cool");
}

static void procApplyAccess(DemoColdEntry* pEntry, const char* sSource, xtime tNow)
{
	if ( pEntry == NULL ) {
		return;
	}

	if ( pEntry->iWarmHits == 0 ) {
		pEntry->tFirstWarmHitAt = tNow;
	}

	pEntry->iWarmHits++;
	pEntry->tLastWarmHitAt = tNow;
	procCopyText(pEntry->sLastHitSource, sizeof(pEntry->sLastHitSource), sSource);
}

static bool procShouldWarmBack(const DemoColdEntry* pEntry, const DemoRollingWarmCenter* pCenter, xtime tNow)
{
	if ( (pEntry == NULL) || (pCenter == NULL) ) {
		return FALSE;
	}

	if ( pEntry->iWarmHits < (int)pCenter->iWarmMinHits ) {
		return FALSE;
	}

	if ( tNow < pEntry->tFirstWarmHitAt ) {
		return FALSE;
	}

	return ((uint32)(tNow - pEntry->tFirstWarmHitAt) <= pCenter->iWarmWindowMs);
}

static bool procShouldArchive(const DemoColdEntry* pEntry, const DemoRollingWarmCenter* pCenter, xtime tNow)
{
	if ( (pEntry == NULL) || (pCenter == NULL) ) {
		return FALSE;
	}

	if ( tNow < pEntry->tCooledAt ) {
		return FALSE;
	}

	return ((uint32)(tNow - pEntry->tCooledAt) >= pCenter->iArchiveAfterMs);
}

static const char* procChooseNextAction(const DemoColdEntry* pEntry, const DemoRollingWarmCenter* pCenter, xtime tNow)
{
	if ( procShouldWarmBack(pEntry, pCenter, tNow) ) {
		return "warm_back";
	}

	if ( procShouldArchive(pEntry, pCenter, tNow) ) {
		return "archive";
	}

	return "keep_cold";
}

int main(void)
{
	DemoRollingWarmCenter tCenter;
	DemoColdEntry tBeta;
	DemoJob tJobs[5];
	xtime tBase = 1000;
	const char* sAction;
	size_t i;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBeta, 0, sizeof(tBeta));
	memset(tJobs, 0, sizeof(tJobs));

	tCenter.iKeepCold = 8u;
	tCenter.iWarmWindowMs = 600u;
	tCenter.iWarmMinHits = 2u;
	tCenter.iArchiveAfterMs = 3000u;
	procCopyText(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "runtime/auto-warm-rolling-archive.json");

	tBeta.iKeyHash = 1002u;
	tBeta.iHeat = 7;
	procCopyText(tBeta.sKey, sizeof(tBeta.sKey), "beta");
	procCopyText(tBeta.sTitle, sizeof(tBeta.sTitle), "Beta Worker");

	tJobs[0].iKind = DEMO_JOB_COOL;
	procCopyText(tJobs[0].sKey, sizeof(tJobs[0].sKey), "beta");
	procCopyText(tJobs[0].sText, sizeof(tJobs[0].sText), "manual_cooldown");

	tJobs[1].iKind = DEMO_JOB_ACCESS;
	procCopyText(tJobs[1].sKey, sizeof(tJobs[1].sKey), "beta");
	procCopyText(tJobs[1].sText, sizeof(tJobs[1].sText), "page_hit");

	tJobs[2].iKind = DEMO_JOB_ACCESS;
	procCopyText(tJobs[2].sKey, sizeof(tJobs[2].sKey), "beta");
	procCopyText(tJobs[2].sText, sizeof(tJobs[2].sText), "json_poll");

	tJobs[3].iKind = DEMO_JOB_SWEEP;
	procCopyText(tJobs[3].sKey, sizeof(tJobs[3].sKey), "beta");
	procCopyText(tJobs[3].sText, sizeof(tJobs[3].sText), "periodic_sweep");

	for ( i = 0u; i < 4u; ++i ) {
		if ( tJobs[i].iKind == DEMO_JOB_COOL ) {
			procApplyCool(&tBeta, tJobs[i].sText, tBase);
		} else if ( tJobs[i].iKind == DEMO_JOB_ACCESS ) {
			procApplyAccess(&tBeta, tJobs[i].sText, tBase + (xtime)((i + 1u) * 100u));
		}
	}

	sAction = procChooseNextAction(&tBeta, &tCenter, tBase + 500);
	printf("%s -> %s (%d hits)\n", tBeta.sKey, sAction, tBeta.iWarmHits);

	sAction = procChooseNextAction(&tBeta, &tCenter, tBase + 5000);
	printf("%s -> %s after longer sweep\n", tBeta.sKey, sAction);
	printf("snapshot: %s\n", tCenter.sSnapshotPath);
	return 0;
}
```


## 6. What to Read Next

After this page, the next natural upgrades are:

- [Upgrade the Local Console Service into a Multi-Tier Storage Dashboard](multi-tier-storage-dashboard.en.md)
- [Upgrade the Local Console Service into a Hot-Cold Multi-Level Aging Dashboard](hot-cold-multi-level-aging-dashboard.en.md)
- [Upgrade the Local Console Service into a Warm-Archive Coordination Dashboard](warm-archive-coordination-dashboard.en.md)
