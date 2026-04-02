# XRT Case Study: Upgrade the Local Console Service into a Heavy Recovery-Chain Dashboard

> This page is not about "add one more subprocess after cross-media orchestration". It targets the more realistic service problem: once one local console service already has warm shadows, package bundles, external hydrate tools, and async publication, how do you turn `dict + dict + list + list + list + queue + thread + task group + future + subprocess + file_async + path + file + hash + xhttpd + template` into one formal mainline so that one recovery request can grow manifest inspect, verify, hydrate, and publish stages, and the dashboard, JSON, logs, and snapshots can still explain which stage is running, why the chain deferred, and when the whole recovery scope actually finished?

[中文](heavy-recovery-chain-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

The earlier pages already established these lines:

- recovery priority can choose warm, cool, cold file, or defer
- cross-media orchestration can choose local file, package bundle, subprocess, and publish

But a real service quickly grows one more requirement:

- some keys do not stop after "choose one recovery source"
- they must inspect one package manifest first
- then verify the bundle
- then run one hydrate tool
- then publish the staging output into one formal snapshot
- and the page still needs to answer whether the chain is blocked in verify, hydrate, publish, or defer

So the real problem is no longer "one more tool-chain example". It is:

> how to turn one multi-stage recovery request into one formal scope with child stages, one close-aware join point, and one explainable chain status.


## 2. Why `future + subprocess` Is No Longer Enough

One `future` is still useful, because it can tell you whether:

- verify finished
- hydrate failed
- publish reached a terminal state

But one `future` does not tell you:

- whether those stages belong to the same recovery request
- when the request stops accepting new child stages
- who owns the final close-aware barrier for the whole chain

This is the new split that matters here:

- `queue + thread`
	- sends hydrate intent into the background
- `task group`
	- owns the scope for inspect / verify / hydrate / publish children
- `future`
	- still represents each child result plus the final join future

In one line:

> `subprocess` answers how one stage runs, `future` answers whether one stage finished, and `task group` answers when the whole heavy recovery chain is actually done.


## 3. What Each Layer Owns

| Layer | Role | What it actually solves |
|------|------|--------------------------|
| `dict` | current hot tier | live lookups after recovery completes |
| `dict` | current warm shadow | the cheapest short-path recovery source |
| `list` | current package catalog | the input source for heavy recovery |
| `list` | recent stage history | page and JSON output for "where the chain is now" |
| `list` | recent defer history | page and JSON output for "why the chain stopped here" |
| `list` | recent publish history | page and JSON output for "which chain actually published" |
| `queue + thread` | background `HYDRATE / SWEEP` convergence | keeps request threads out of long recovery chains |
| `task group` | owns all children of one recovery request | `Close / Join / Cancel / Destroy` |
| `future` | one child result and the final join barrier | unified completion and timeout edges |
| `subprocess` | verify / hydrate tool stages | formal external work instead of ad-hoc shell glue |
| `file_async` | publishes staging output into the final snapshot | a formal publish future |
| `path + file` | package / stage / snapshot paths | moves the chain into the filesystem |
| `hash` | stable key fingerprint | lightweight IDs for bundle and snapshot paths |
| `xvalue + json` | current stage, defer, and publish summaries | one shared state model |
| `xhttpd + template` | dashboard output | one browser and script-facing entry |


## 4. File Layout and Outputs

This case keeps the local-console-service layout and changes the outputs into heavy-recovery-chain semantics:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/heavy-recovery-chain.json
runtime/packages/<hash>.pkg
runtime/stage/<hash>.json
runtime/published/<hash>.json
```

Where:

- `config/console.json`
	- keeps `max_bundle_bytes`, `max_stage_count`, and `join_timeout_ms`
- `web/dashboard.html`
	- shows the current stage, recent stage history, recent defer history, and recent publish history
- `runtime/console.log`
	- records hydrate signals, stage starts, join completion, and defer reasons
- `runtime/heavy-recovery-chain.json`
	- stores the latest heavy-recovery summary
- `runtime/packages/<hash>.pkg`
	- stores the input bundle
- `runtime/stage/<hash>.json`
	- stores one staging output
- `runtime/published/<hash>.json`
	- stores the final published snapshot


## 5. Compact Core: One Scope, Multiple Child Stages, One Join

The reviewed Chinese page contains the full walkthrough. The English page keeps the formal mainline compact and source-aligned.

This skeleton preserves the boundaries that matter most:

1. request threads only submit hydrate intent
2. the worker creates one recovery scope
3. manifest inspect runs as a local child task
4. verify and hydrate run as subprocess futures
5. publish runs as one async-file future
6. the whole chain converges through one join future

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_HYDRATE 1
#define DEMO_SIGNAL_SWEEP 2
#define DEMO_MAX_CHILD 8

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
	xtime tPackedAt;
	uint64 iKeyHash;
	uint32 iBundleBytes;
	uint32 iStageCount;
	bool bNeedVerify;
	bool bNeedHydrate;
	char sKey[64];
	char sPackagePath[260];
} DemoPackageRecord;

typedef struct
{
	xtime tStartedAt;
	xtime tFinishedAt;
	uint64 iKeyHash;
	char sKey[64];
	char sStage[32];
	char sResult[48];
} DemoChainEvent;

typedef struct
{
	xtime tDeferredAt;
	uint64 iKeyHash;
	char sKey[64];
	char sReason[64];
} DemoDeferredEvent;

typedef struct
{
	uint32 iMaxBundleBytes;
	uint32 iMaxStageCount;
	uint32 iJoinTimeoutMs;
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
	xlist pPackage;
	xlist pStageHistory;
	xlist pDeferred;
	xlist pPublished;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoRecoveryPolicy tPolicy;
	char sSnapshotPath[260];
} DemoHeavyRecoveryCenter;

typedef struct
{
	char sStage[32];
	int iDelayMs;
	int iStatus;
} DemoStageTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildPath(char* sDst, size_t iCap, const char* sDir, const char* sKey, const char* sExt)
{
	const char* sSafeKey = (sKey != NULL) ? sKey : "";

	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(
		sDst,
		iCap,
		"runtime/%s/%016llx%s",
		(sDir != NULL) ? sDir : "stage",
		(unsigned long long)xrtHash64((ptr)sSafeKey, strlen(sSafeKey)),
		(sExt != NULL) ? sExt : ".json");
}

static int32 procLocalStageTask(ptr pArg, xfuture_result* pOut)
{
	DemoStageTask* pTask = (DemoStageTask*)pArg;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pTask->iDelayMs);
	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static xfuture* procSpawnEchoStage(xprocess** ppProcess, const char* sPayload)
{
	xprocessconfig tConfig;
	str arrArgs[4];

	if ( ppProcess == NULL ) {
		return NULL;
	}

	*ppProcess = NULL;
	xrtProcessConfigInit(&tConfig);
	tConfig.iTargetKind = XPROC_TARGET_EXEC;
	tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
	tConfig.Stdout.bCapture = TRUE;
	tConfig.Stderr.iMode = XPROC_STDIO_PIPE;
	tConfig.Stderr.bCapture = TRUE;

	#if defined(_WIN32) || defined(_WIN64)
		arrArgs[0] = (str)"/c";
		arrArgs[1] = (str)"echo";
		arrArgs[2] = (str)((sPayload != NULL) ? sPayload : "heavy-stage");
		tConfig.sProgram = (str)"cmd";
		tConfig.arrArgs = arrArgs;
		tConfig.iArgCount = 3u;
	#else
		arrArgs[0] = (str)"-lc";
		arrArgs[1] = (str)"printf 'heavy-stage\\n'";
		tConfig.sProgram = (str)"sh";
		tConfig.arrArgs = arrArgs;
		tConfig.iArgCount = 2u;
	#endif

	*ppProcess = xrtProcessSpawn(&tConfig);
	if ( *ppProcess == NULL ) {
		return NULL;
	}

	return xrtProcessWaitFuture(*ppProcess);
}

static const char* procChooseHeavyPlan(const DemoWarmItem* pWarm, const DemoPackageRecord* pPkg, const DemoRecoveryPolicy* pPolicy, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1500) ) {
		return "warm_shadow";
	}
	if ( pPkg == NULL ) {
		return "defer_no_source";
	}
	if ( pPkg->iBundleBytes > pPolicy->iMaxBundleBytes ) {
		return "defer_bundle_too_large";
	}
	if ( pPkg->iStageCount > pPolicy->iMaxStageCount ) {
		return "defer_stage_count_too_high";
	}
	return "heavy_chain";
}

static bool procRunHeavyChain(DemoHeavyRecoveryCenter* pCenter, const DemoPackageRecord* pPkg)
{
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xprocess* arrProcess[2];
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	DemoStageTask tInspect;
	char sStagePath[260];
	char sPublishPath[260];
	const char* sStageJson = "{\n\t\"state\": \"ready-for-publish\"\n}\n";
	int iChildCount = 0;
	int iProcessCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrProcess, 0, sizeof(arrProcess));
	memset(&tInspect, 0, sizeof(tInspect));

	if ( (pCenter == NULL) || (pPkg == NULL) ) {
		return false;
	}

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return false;
	}

	procCopyText(tInspect.sStage, sizeof(tInspect.sStage), "inspect_manifest");
	tInspect.iDelayMs = 60;
	tInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalStageTask, &tInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	if ( pPkg->bNeedVerify ) {
		arrChild[iChildCount] = procSpawnEchoStage(&arrProcess[iProcessCount], "verify_bundle");
		if ( (arrChild[iChildCount] == NULL) || (arrProcess[iProcessCount] == NULL) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		if ( !xTaskGroupAddFuture(pGroup, arrChild[iChildCount]) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		iChildCount++;
		iProcessCount++;
	}

	if ( pPkg->bNeedHydrate ) {
		arrChild[iChildCount] = procSpawnEchoStage(&arrProcess[iProcessCount], "hydrate_bundle");
		if ( (arrChild[iChildCount] == NULL) || (arrProcess[iProcessCount] == NULL) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		if ( !xTaskGroupAddFuture(pGroup, arrChild[iChildCount]) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		iChildCount++;
		iProcessCount++;
	}

	procBuildPath(sStagePath, sizeof(sStagePath), "stage", pPkg->sKey, ".json");
	procBuildPath(sPublishPath, sizeof(sPublishPath), "published", pPkg->sKey, ".json");

	pPublish = xrtFileWriteAllAsync(sPublishPath, (str)sStageJson, strlen(sStageJson), XRT_CP_UTF8);
	if ( pPublish == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	if ( !xTaskGroupAddFuture(pGroup, pPublish) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	arrChild[iChildCount] = pPublish;
	iChildCount++;

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);

	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iJoinTimeoutMs) ) {
		printf("%s -> heavy_chain_timeout\n", pPkg->sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	printf("%s -> heavy_chain_joined\n", pPkg->sKey);
	printf("stage-path: %s\n", sStagePath);
	printf("publish-path: %s\n", sPublishPath);

	for ( i = 0; i < iChildCount; i++ ) {
		printf("child[%d] status = %d\n", i, (int)xFutureStatus(arrChild[i]));
	}

	bFinished = true;

cleanup:
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iChildCount; i++ ) {
		if ( arrChild[i] != NULL ) {
			xFutureRelease(arrChild[i]);
		}
	}
	for ( i = 0; i < iProcessCount; i++ ) {
		if ( arrProcess[i] != NULL ) {
			xrtProcessDestroy(arrProcess[i]);
		}
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}

	return bFinished;
}

int main(void)
{
	DemoHeavyRecoveryCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoPackageRecord tThetaPkg;
	DemoPackageRecord tOmegaPkg;
	DemoDeferredEvent tDeferred;
	const char* sPlan;
	xtime tNow = xrtNow();

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(&tThetaPkg, 0, sizeof(tThetaPkg));
	memset(&tOmegaPkg, 0, sizeof(tOmegaPkg));
	memset(&tDeferred, 0, sizeof(tDeferred));

	tCenter.tPolicy.iMaxBundleBytes = 256u;
	tCenter.tPolicy.iMaxStageCount = 4u;
	tCenter.tPolicy.iJoinTimeoutMs = 5000u;
	procBuildPath(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), ".", "heavy-recovery-chain", ".json");

	tBetaWarm.tTouchedAt = tNow - 300;
	tBetaWarm.iKeyHash = 2001u;
	tBetaWarm.iHeat = 51;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	tThetaPkg.tPackedAt = tNow - 2600;
	tThetaPkg.iKeyHash = 2002u;
	tThetaPkg.iBundleBytes = 96u;
	tThetaPkg.iStageCount = 4u;
	tThetaPkg.bNeedVerify = true;
	tThetaPkg.bNeedHydrate = true;
	procCopyText(tThetaPkg.sKey, sizeof(tThetaPkg.sKey), "theta");
	procBuildPath(tThetaPkg.sPackagePath, sizeof(tThetaPkg.sPackagePath), "packages", tThetaPkg.sKey, ".pkg");

	tOmegaPkg.tPackedAt = tNow - 4200;
	tOmegaPkg.iKeyHash = 2003u;
	tOmegaPkg.iBundleBytes = 88u;
	tOmegaPkg.iStageCount = 6u;
	tOmegaPkg.bNeedVerify = true;
	tOmegaPkg.bNeedHydrate = true;
	procCopyText(tOmegaPkg.sKey, sizeof(tOmegaPkg.sKey), "omega");
	procBuildPath(tOmegaPkg.sPackagePath, sizeof(tOmegaPkg.sPackagePath), "packages", tOmegaPkg.sKey, ".pkg");

	sPlan = procChooseHeavyPlan(&tBetaWarm, NULL, &tCenter.tPolicy, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	sPlan = procChooseHeavyPlan(NULL, &tThetaPkg, &tCenter.tPolicy, tNow);
	if ( strcmp(sPlan, "heavy_chain") == 0 ) {
		(void)procRunHeavyChain(&tCenter, &tThetaPkg);
	}

	sPlan = procChooseHeavyPlan(NULL, &tOmegaPkg, &tCenter.tPolicy, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		tDeferred.tDeferredAt = tNow;
		tDeferred.iKeyHash = tOmegaPkg.iKeyHash;
		procCopyText(tDeferred.sKey, sizeof(tDeferred.sKey), tOmegaPkg.sKey);
		procCopyText(tDeferred.sReason, sizeof(tDeferred.sReason), "stage_count_too_high");
		printf("%s -> %s (%s)\n", tDeferred.sKey, sPlan, tDeferred.sReason);
	}

	printf("snapshot: %s\n", tCenter.sSnapshotPath);
	return 0;
}
```


## 6. Boundaries That Must Stay Stable

The mainline this page needs to stabilize is:

- request threads submit hydrate intent instead of owning stage lifecycles
- `task group` owns the scope of one heavy recovery chain, not the details of one stage
- subprocess futures are child stages, not the final completion point
- async-file publication is one explicit publish stage, not a replacement for recovery selection or scope control

If any of those lines collapse, the implementation quickly degrades back into scattered stage futures and handler-local glue.


## 7. What to Read Next

After this page, the next natural comparisons are:

- [Upgrade the Local Console Service into a Cross-Media Orchestration Dashboard](cross-media-orchestration-dashboard.en.md)
- [Upgrade the Local Console Service into a Batch Task Dashboard](batch-task-dashboard.en.md)
- the next heavier business pages around cross-machine recovery, recovery quotas, and distributed orchestration
