# XRT Case Study: Upgrade the Local Console Service into a Cross-Media Orchestration Dashboard

> This page is not about "if local restore fails, try one package file next". It targets the more realistic service problem: once one local console service already has warm, local-file, package-bundle, and external tool-chain recovery sources, how do you turn `dict + dict + list + list + queue + thread + future + subprocess + file_async + path + file + hash + xhttpd + template` into one formal mainline so that request threads only submit hydrate intent, the worker chooses memory, local file, package bundle, or subprocess, and the page, JSON, logs, and snapshots can all explain which cross-media path ran and why?

[中文](cross-media-orchestration-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume the local console service skeleton is already in place, and recovery sources are now starting to multiply:

- some keys still have a warm or cool shadow
- some keys only have one local cold snapshot
- some keys only exist inside one package bundle and need unpack or verification
- some recoveries are no longer "read one file", but "run one external hydrate tool chain"
- the page and JSON still need to explain why the service used local file, switched to package bundle, launched subprocess, or deferred the job

So the real problem is no longer "one more fallback branch". It is:

> how to turn the recovery chain itself into one formal, explainable, exportable cross-media orchestration path.


## 2. Why Recovery Sources and Recovery Actions Cannot Stay Blurred Together

If local file, package bundle, and subprocess recovery keep collapsing into one vague fallback chain, several problems appear quickly:

- local file, package bundle, and external tool chains no longer have explicit priority boundaries
- request threads start carrying unpack, verify, and publish logic
- heavy recovery completion no longer has one formal convergence point
- the page must scan logs or worker-local scratch state just to explain which path was used

The key split in this page is:

- `local file`
	- answers "recover from the cheapest persistent source on this machine"
- `package bundle`
	- answers "the source is still local, but it is no longer one plain snapshot file"
- `subprocess`
	- answers "recovery has become a tool-chain task"
- `file_async`
	- answers "how to publish heavy recovery output back into staging and snapshots"


## 3. What Each Layer Owns

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot tier | live lookups after successful recovery |
| `dict` | current warm tier | cheapest in-memory recovery source |
| `list` | current local-file catalog | local snapshot recovery source |
| `list` | current package catalog | bundle-based recovery source and orchestration entry |
| `list` | recent orchestration history | page timeline and "which path ran" export |
| `list` | recent defer history | page timeline and "why heavy recovery was skipped for now" export |
| `queue + thread` | background `HYDRATE / SWEEP` convergence | keeps request threads out of cross-media recovery |
| `future` | tracks heavy tool-chain completion | gives orchestration one formal completion point |
| `subprocess` | runs unpack / verify / transform tool chains | moves heavy recovery into the runtime mainline |
| `file_async` | publishes staging files and final snapshots | keeps heavy output publication explicit |
| `path + file` | builds cold / package / stage paths | moves recovery sources and intermediate results into the filesystem |
| `hash` | stable key fingerprint | lightweight IDs for bundles, pages, and logs |
| `xvalue + json` | current source, orchestration, and defer summaries | lets the page, scripts, and recovery logic share one model |
| `xhttpd + template` | dashboard output | one browser and script-facing entry |


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the outputs into cross-media orchestration semantics:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/cross-media-orchestration.json
runtime/cold/<hash>.json
runtime/packages/<hash>.pkg
runtime/stage/<hash>.json
```

Where:

- `config/console.json`
	- keeps `max_local_file_cost`, `max_package_bytes`, and `max_toolchain_cost`
- `web/dashboard.html`
	- shows current recovery sources, recent orchestration history, and recent defer history
- `runtime/console.log`
	- records hydrate signals, orchestration decisions, subprocess launch, and publish completion
- `runtime/cross-media-orchestration.json`
	- stores the latest orchestration summary
- `runtime/cold/<hash>.json`
	- stores one local cold snapshot
- `runtime/packages/<hash>.pkg`
	- stores one package bundle
- `runtime/stage/<hash>.json`
	- stores one staging file produced by a heavier recovery path


## 5. Compact Core: Hydrate Signal, Plan Selection, Publish

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the boundaries that matter most:

1. request threads submit hydrate intent instead of launching tools directly
2. the worker chooses warm, local file, package bundle, subprocess, or defer
3. heavy recovery still needs one formal completion point
4. publication is explicit and separate from recovery selection

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_HYDRATE 1
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
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoWarmItem;

typedef struct
{
	xtime tReadyAt;
	uint64 iKeyHash;
	uint32 iFileCost;
	char sKey[64];
	char sLocalPath[260];
} DemoLocalRecord;

typedef struct
{
	xtime tPackedAt;
	uint64 iKeyHash;
	uint32 iBundleBytes;
	bool bNeedVerify;
	char sKey[64];
	char sPackagePath[260];
} DemoPackageRecord;

typedef struct
{
	xtime tStartedAt;
	xtime tFinishedAt;
	uint64 iKeyHash;
	char sKey[64];
	char sPlan[32];
	char sReason[64];
} DemoStageEvent;

typedef struct
{
	xtime tDeferredAt;
	uint64 iKeyHash;
	char sKey[64];
	char sReason[64];
} DemoDeferredEvent;

typedef struct
{
	uint32 iMaxLocalFileCost;
	uint32 iMaxPackageBytes;
	uint32 iMaxToolchainCost;
} DemoOrchestrationPolicy;

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
	xlist pLocal;
	xlist pPackage;
	xlist pHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	ptr hHydrateFuture;
	DemoOrchestrationPolicy tPolicy;
	char sSnapshotPath[260];
} DemoOrchestrationCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildPath(char* sDst, size_t iCap, const char* sDir, const char* sKey, const char* sExt)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/%s/%s%s", (sDir != NULL) ? sDir : "stage", (sKey != NULL) ? sKey : "unknown", (sExt != NULL) ? sExt : ".dat");
}

static bool procWarmUsable(const DemoWarmItem* pWarm, xtime tNow)
{
	if ( pWarm == NULL ) {
		return FALSE;
	}

	return (tNow >= pWarm->tTouchedAt);
}

static bool procLocalUsable(const DemoLocalRecord* pLocal, const DemoOrchestrationPolicy* pPolicy)
{
	if ( (pLocal == NULL) || (pPolicy == NULL) ) {
		return FALSE;
	}

	return (pLocal->iFileCost <= pPolicy->iMaxLocalFileCost);
}

static bool procPackageUsable(const DemoPackageRecord* pPkg, const DemoOrchestrationPolicy* pPolicy)
{
	if ( (pPkg == NULL) || (pPolicy == NULL) ) {
		return FALSE;
	}

	return (pPkg->iBundleBytes <= pPolicy->iMaxPackageBytes);
}

static const char* procChoosePlan(
	const DemoWarmItem* pWarm,
	const DemoLocalRecord* pLocal,
	const DemoPackageRecord* pPkg,
	const DemoOrchestrationPolicy* pPolicy,
	xtime tNow
)
{
	if ( procWarmUsable(pWarm, tNow) ) {
		return "restore_from_warm";
	}

	if ( procLocalUsable(pLocal, pPolicy) ) {
		return "restore_from_local_file";
	}

	if ( procPackageUsable(pPkg, pPolicy) && (pPkg->bNeedVerify == FALSE) ) {
		return "hydrate_from_package";
	}

	if ( procPackageUsable(pPkg, pPolicy) && (pPkg->bNeedVerify == TRUE) && (pPolicy->iMaxToolchainCost >= 10u) ) {
		return "hydrate_via_subprocess";
	}

	if ( pPkg != NULL ) {
		return "defer_cross_media";
	}

	return "miss";
}

static void procRecordStage(DemoStageEvent* pEvent, const char* sKey, const char* sPlan, const char* sReason, xtime tNow)
{
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->tStartedAt = tNow;
	pEvent->tFinishedAt = tNow + 1;
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sPlan, sizeof(pEvent->sPlan), sPlan);
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
	DemoOrchestrationCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoLocalRecord tGammaLocal;
	DemoPackageRecord tDeltaPkg;
	DemoPackageRecord tEpsilonPkg;
	DemoPackageRecord tZetaPkg;
	DemoStageEvent tStage;
	DemoDeferredEvent tDeferred;
	const char* sPlan;
	xtime tNow = 9000;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(&tGammaLocal, 0, sizeof(tGammaLocal));
	memset(&tDeltaPkg, 0, sizeof(tDeltaPkg));
	memset(&tEpsilonPkg, 0, sizeof(tEpsilonPkg));
	memset(&tZetaPkg, 0, sizeof(tZetaPkg));
	memset(&tStage, 0, sizeof(tStage));
	memset(&tDeferred, 0, sizeof(tDeferred));

	tCenter.tPolicy.iMaxLocalFileCost = 4u;
	tCenter.tPolicy.iMaxPackageBytes = 64u;
	tCenter.tPolicy.iMaxToolchainCost = 10u;
	procCopyText(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "runtime/cross-media-orchestration.json");

	tBetaWarm.tTouchedAt = tNow - 50;
	tBetaWarm.iKeyHash = 1002u;
	tBetaWarm.iHeat = 8;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "Beta Warm");

	tGammaLocal.tReadyAt = tNow - 1000;
	tGammaLocal.iKeyHash = 1003u;
	tGammaLocal.iFileCost = 3u;
	procCopyText(tGammaLocal.sKey, sizeof(tGammaLocal.sKey), "gamma");
	procBuildPath(tGammaLocal.sLocalPath, sizeof(tGammaLocal.sLocalPath), "cold", tGammaLocal.sKey, ".json");

	tDeltaPkg.tPackedAt = tNow - 2000;
	tDeltaPkg.iKeyHash = 1004u;
	tDeltaPkg.iBundleBytes = 32u;
	tDeltaPkg.bNeedVerify = FALSE;
	procCopyText(tDeltaPkg.sKey, sizeof(tDeltaPkg.sKey), "delta");
	procBuildPath(tDeltaPkg.sPackagePath, sizeof(tDeltaPkg.sPackagePath), "packages", tDeltaPkg.sKey, ".pkg");

	tEpsilonPkg.tPackedAt = tNow - 2000;
	tEpsilonPkg.iKeyHash = 1005u;
	tEpsilonPkg.iBundleBytes = 48u;
	tEpsilonPkg.bNeedVerify = TRUE;
	procCopyText(tEpsilonPkg.sKey, sizeof(tEpsilonPkg.sKey), "epsilon");
	procBuildPath(tEpsilonPkg.sPackagePath, sizeof(tEpsilonPkg.sPackagePath), "packages", tEpsilonPkg.sKey, ".pkg");

	tZetaPkg.tPackedAt = tNow - 2000;
	tZetaPkg.iKeyHash = 1006u;
	tZetaPkg.iBundleBytes = 48u;
	tZetaPkg.bNeedVerify = TRUE;
	procCopyText(tZetaPkg.sKey, sizeof(tZetaPkg.sKey), "zeta");
	procBuildPath(tZetaPkg.sPackagePath, sizeof(tZetaPkg.sPackagePath), "packages", tZetaPkg.sKey, ".pkg");

	sPlan = procChoosePlan(&tBetaWarm, NULL, NULL, &tCenter.tPolicy, tNow);
	procRecordStage(&tStage, "beta", sPlan, "warm_shadow_available", tNow);
	printf("%s -> %s\n", tStage.sKey, tStage.sPlan);

	sPlan = procChoosePlan(NULL, &tGammaLocal, NULL, &tCenter.tPolicy, tNow);
	procRecordStage(&tStage, "gamma", sPlan, "local_snapshot_ready", tNow);
	printf("%s -> %s (%s)\n", tStage.sKey, tStage.sPlan, tGammaLocal.sLocalPath);

	sPlan = procChoosePlan(NULL, NULL, &tDeltaPkg, &tCenter.tPolicy, tNow);
	procRecordStage(&tStage, "delta", sPlan, "package_bundle_ready", tNow);
	printf("%s -> %s (%s)\n", tStage.sKey, tStage.sPlan, tDeltaPkg.sPackagePath);

	sPlan = procChoosePlan(NULL, NULL, &tEpsilonPkg, &tCenter.tPolicy, tNow);
	procRecordStage(&tStage, "epsilon", sPlan, "verify_then_publish", tNow);
	printf("%s -> %s (%s)\n", tStage.sKey, tStage.sPlan, tEpsilonPkg.sPackagePath);

	tCenter.tPolicy.iMaxToolchainCost = 4u;
	sPlan = procChoosePlan(NULL, NULL, &tZetaPkg, &tCenter.tPolicy, tNow);
	if ( strcmp(sPlan, "defer_cross_media") == 0 ) {
		procRecordDeferred(&tDeferred, "zeta", "toolchain_cost_too_high", tNow);
		printf("%s -> %s (%s)\n", tDeferred.sKey, sPlan, tDeferred.sReason);
	}

	printf("snapshot: %s\n", tCenter.sSnapshotPath);
	return 0;
}
```


## 6. What to Read Next

The closest neighboring references for this page are:

- [Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)
- [Upgrade the Local Console Service into a Subprocess Probe Dashboard](subprocess-probe-dashboard.en.md)
- [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
