# 把本地控制台服务升级成一个恢复配额面板

> 这页要解决的不是“重链路恢复再加一个计数器”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了多阶段重链路恢复之后，又开始需要把“同一时刻最多允许几条重恢复链运行”“新的 hydrate 请求是立即进 active scope、进入 pending queue，还是直接 defer”“上一条链路 join 完成之后谁来提升下一条等待请求”放进同一条后台主线，还要把当前 active 数、waiting 队列、最近 defer 原因和最近 publish 结果稳定交给页面、JSON、日志和快照时，怎样把 `dict + list + list + list + queue + thread + task group + future + subprocess + file_async + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是继续让每个 handler 自己看一个全局变量然后临时决定跑不跑。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 跨介质编排怎样决定 local / package / subprocess / publish
- 重链路恢复怎样把 inspect / verify / hydrate / publish 收进一个 `task group` scope

但真实服务再往前走一步，很快又会出现一个新的问题：

- 这次不是只有一条重恢复链
- 而是会同时来 5 个、10 个、50 个 hydrate 请求
- 每条链路都可能长出 inspect / verify / hydrate / publish 多个 child
- 本机 CPU、磁盘和外部工具链容量却不是无限的
- 页面和 JSON 还要能回答“为什么这次没有立刻开始”“它是在排队，还是被正式 defer 了”“哪一条链路占着当前 quota”

如果这时还把实现停在：

- `if ( iRunning < 2 ) start_chain(); else return busy;`

很快就会出现几个典型问题：

- 请求线程开始背上 quota 判断和排队逻辑
- “暂时排队”和“正式 defer”混成一个模糊状态
- join 完成后没有正式的提升逻辑去接下一条等待请求
- 页面为了说明“现在谁在跑、谁在等、谁被拒绝了”开始扫日志和临时变量

所以这页真正要补出的，不是“再多一个并发上限判断”，而是：

> 当重恢复链已经成为长期运行主线时，怎样把 quota、waiting queue、active scope 和 join 后提升逻辑做成一条正式、可解释、可导出的后台主线。


## 2. 为什么这次不能只靠“局部 task group”

### 2.1 `task group` 解决的是单条链路收口，不是全局配额

上一页里 `task group` 已经很好地解决了：

- inspect / verify / hydrate / publish 属于同一条恢复链
- 这条恢复链什么时候 close
- 这条恢复链什么时候 join 完成

但它不回答：

- 这一刻全局最多能有多少条重链路同时运行
- 新来的请求是进入 active 还是 pending
- 哪条 join 完成之后要提升下一条等待请求


### 2.2 “排队等待”不是失败，也不是 defer

到了这一层，系统真正要稳定回答的是：

- 这次请求当前没开始，是因为 quota 满了，还是因为 bundle 太大
- 它现在属于 waiting，还是已经是正式 defer
- 什么时候 waiting 会被提升成 active

所以更稳的理解方式是：

- `waiting`
	- 还有资格运行，只是暂时没有 slot
- `defer`
	- 当前策略明确拒绝继续往下跑


### 2.3 这次真正新增的是“全局 admission 层”

更稳的分工方式是：

- `queue + thread`
	- 仍然承接 `HYDRATE / SWEEP`
- `task group`
	- 仍然只管理一条恢复链内部的 child
- `admission / quota`
	- 决定这条恢复链是进入 active、进入 waiting，还是直接 defer

一句话记住：

> 上一页补的是“单条链路怎么收口”，这一页补的是“多条链路怎么排队、抢 slot 和统一提升”。


## 3. 这条恢复配额主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active registry | 当前有哪些重链路正在占用 quota |
| `list` | waiting queue | 没拿到 slot 但仍有资格运行的请求 |
| `list` | recent quota history | 页面和 JSON 展示“谁拿到 slot、谁被提升、谁 join 完成” |
| `list` | recent defer history | 页面和 JSON 展示“为什么这次没资格进 waiting / active” |
| `queue + thread` | 后台消费 `HYDRATE / SWEEP` | 请求线程不阻塞在 admission 与链路调度上 |
| `task group` | 管住单条 active 重恢复链 | `Close / Join / Cancel / Destroy` |
| `future` | 表达每个阶段与整条链路的完成态 | 统一结果与 join 边界 |
| `subprocess` | verify / hydrate 这类外部阶段 | 正式重链路阶段，不是 handler 里的临时命令 |
| `file_async` | 发布 staging 结果 | 正式 publish child |
| `time` | 记录 queued / started / finished 时间 | quota 时间线和等待时长 |
| `path + file` | 组织 package / stage / snapshot 路径 | 让链路真正进入文件系统 |
| `hash` | 给 key 一个稳定指纹 | 生成 bundle、stage、snapshot 路径 |
| `xvalue + json` | 导出 active / waiting / defer / publish 摘要 | 页面、脚本、日志共享同一份模型 |
| `xhttpd + template` | 展示当前 quota 状态 | 浏览器和脚本共享一个入口 |

一句话记住：

> `task group` 管一条链，`quota` 管很多条链谁先跑、谁等着、谁被正式拒绝。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成恢复配额语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/recovery-quota.json
runtime/packages/<hash>.pkg
runtime/stage/<hash>.json
runtime/published/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `max_active_recoveries`、`max_bundle_bytes`、`max_stage_count`、`join_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 active、waiting、最近 quota history、最近 defer history
- `runtime/console.log`
	- 记录 hydrate signal、slot 获取、等待提升、join 完成和 defer 原因
- `runtime/recovery-quota.json`
	- 保存最近一次 quota 摘要
- `runtime/packages/<hash>.pkg`
	- 保存输入 bundle
- `runtime/stage/<hash>.json`
	- 保存重链路 stage 结果
- `runtime/published/<hash>.json`
	- 保存发布完成后的正式 snapshot


## 5. 先把“hydrate signal -> quota gate -> active scope -> join 后提升 waiting”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. `dict` 表示当前 active registry
2. `list` 表示 waiting queue
3. `list` 表示 quota / defer / publish history
4. `queue + thread` 仍然只承接 `HYDRATE / SWEEP`
5. `task group` 仍然只管理单条恢复链
6. inspect 用线程 child 表示
7. verify / hydrate 用 subprocess future 表示
8. publish 用 `file_async future` 表示
9. quota 只决定 `run_now / queue_wait / defer`
10. 某条 join 完成之后，worker 再提升下一条 waiting 请求

这个骨架会展示这些事：

- `beta` 仍有 warm shadow，不进入重链路
- `theta` 拿到第一个 slot，直接进入 active scope
- `sigma` 也有资格运行，但当前 quota 满了，先进入 waiting
- `omega` stage 数超预算，这次直接 defer
- `theta` join 完成之后，worker 把 `sigma` 从 waiting 提升成新的 active

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
	xtime tAt;
	uint64 iKeyHash;
	char sKey[64];
	char sState[32];
	char sReason[64];
} DemoQuotaEvent;

typedef struct
{
	uint32 iMaxActiveRecoveries;
	uint32 iMaxBundleBytes;
	uint32 iMaxStageCount;
	uint32 iJoinTimeoutMs;
} DemoQuotaPolicy;

typedef struct
{
	xdict pActive;
	xlist pWaiting;
	xlist pQuotaHistory;
	xlist pDeferred;
	xlist pPublished;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoQuotaPolicy tPolicy;
	uint32 iActiveChains;
	uint32 iWaitingCount;
	char sSnapshotPath[260];
} DemoQuotaCenter;

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
		arrArgs[2] = (str)((sPayload != NULL) ? sPayload : "quota-stage");
		tConfig.sProgram = (str)"cmd";
		tConfig.arrArgs = arrArgs;
		tConfig.iArgCount = 3u;
	#else
		arrArgs[0] = (str)"-lc";
		arrArgs[1] = (str)"printf 'quota-stage\\n'";
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

static const char* procChooseQuotaPlan(const DemoWarmItem* pWarm, const DemoPackageRecord* pPkg, const DemoQuotaCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "warm_shadow";
	}
	if ( pPkg == NULL ) {
		return "defer_no_source";
	}
	if ( pPkg->iBundleBytes > pCenter->tPolicy.iMaxBundleBytes ) {
		return "defer_bundle_too_large";
	}
	if ( pPkg->iStageCount > pCenter->tPolicy.iMaxStageCount ) {
		return "defer_stage_count_too_high";
	}
	if ( pCenter->iActiveChains >= pCenter->tPolicy.iMaxActiveRecoveries ) {
		return "queue_wait";
	}
	return "run_now";
}

static bool procRunQuotaChain(DemoQuotaCenter* pCenter, const DemoPackageRecord* pPkg)
{
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xprocess* arrProcess[2];
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	DemoStageTask tInspect;
	char sStagePath[260];
	char sPublishPath[260];
	const char* sStageJson = "{\n\t\"state\": \"quota-published\"\n}\n";
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
	tInspect.iDelayMs = 40;
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
		printf("%s -> quota_chain_timeout\n", pPkg->sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	printf("%s -> quota_chain_joined\n", pPkg->sKey);
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

static void procRecordEvent(DemoQuotaEvent* pEvent, const char* sKey, const char* sState, const char* sReason, xtime tNow, uint64 iKeyHash)
{
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->tAt = tNow;
	pEvent->iKeyHash = iKeyHash;
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sState, sizeof(pEvent->sState), sState);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
}

int main(void)
{
	DemoQuotaCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoPackageRecord tThetaPkg;
	DemoPackageRecord tSigmaPkg;
	DemoPackageRecord tOmegaPkg;
	DemoQuotaEvent tEvent;
	const char* sPlan;
	xtime tNow = xrtNow();

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(&tThetaPkg, 0, sizeof(tThetaPkg));
	memset(&tSigmaPkg, 0, sizeof(tSigmaPkg));
	memset(&tOmegaPkg, 0, sizeof(tOmegaPkg));
	memset(&tEvent, 0, sizeof(tEvent));

	tCenter.tPolicy.iMaxActiveRecoveries = 1u;
	tCenter.tPolicy.iMaxBundleBytes = 256u;
	tCenter.tPolicy.iMaxStageCount = 4u;
	tCenter.tPolicy.iJoinTimeoutMs = 5000u;
	procBuildPath(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), ".", "recovery-quota", ".json");

	tBetaWarm.tTouchedAt = tNow - 200;
	tBetaWarm.iKeyHash = 3001u;
	tBetaWarm.iHeat = 17;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	tThetaPkg.tPackedAt = tNow - 3200;
	tThetaPkg.iKeyHash = 3002u;
	tThetaPkg.iBundleBytes = 96u;
	tThetaPkg.iStageCount = 4u;
	tThetaPkg.bNeedVerify = true;
	tThetaPkg.bNeedHydrate = true;
	procCopyText(tThetaPkg.sKey, sizeof(tThetaPkg.sKey), "theta");
	procBuildPath(tThetaPkg.sPackagePath, sizeof(tThetaPkg.sPackagePath), "packages", tThetaPkg.sKey, ".pkg");

	tSigmaPkg.tPackedAt = tNow - 2400;
	tSigmaPkg.iKeyHash = 3003u;
	tSigmaPkg.iBundleBytes = 72u;
	tSigmaPkg.iStageCount = 3u;
	tSigmaPkg.bNeedVerify = true;
	tSigmaPkg.bNeedHydrate = true;
	procCopyText(tSigmaPkg.sKey, sizeof(tSigmaPkg.sKey), "sigma");
	procBuildPath(tSigmaPkg.sPackagePath, sizeof(tSigmaPkg.sPackagePath), "packages", tSigmaPkg.sKey, ".pkg");

	tOmegaPkg.tPackedAt = tNow - 2000;
	tOmegaPkg.iKeyHash = 3004u;
	tOmegaPkg.iBundleBytes = 80u;
	tOmegaPkg.iStageCount = 6u;
	tOmegaPkg.bNeedVerify = true;
	tOmegaPkg.bNeedHydrate = true;
	procCopyText(tOmegaPkg.sKey, sizeof(tOmegaPkg.sKey), "omega");
	procBuildPath(tOmegaPkg.sPackagePath, sizeof(tOmegaPkg.sPackagePath), "packages", tOmegaPkg.sKey, ".pkg");

	sPlan = procChooseQuotaPlan(&tBetaWarm, NULL, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	sPlan = procChooseQuotaPlan(NULL, &tThetaPkg, &tCenter, tNow);
	if ( strcmp(sPlan, "run_now") == 0 ) {
		tCenter.iActiveChains++;
		procRecordEvent(&tEvent, tThetaPkg.sKey, "active_start", "slot_acquired", tNow, tThetaPkg.iKeyHash);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	sPlan = procChooseQuotaPlan(NULL, &tSigmaPkg, &tCenter, tNow);
	if ( strcmp(sPlan, "queue_wait") == 0 ) {
		tCenter.iWaitingCount++;
		procRecordEvent(&tEvent, tSigmaPkg.sKey, "waiting", "quota_full", tNow, tSigmaPkg.iKeyHash);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	sPlan = procChooseQuotaPlan(NULL, &tOmegaPkg, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		procRecordEvent(&tEvent, tOmegaPkg.sKey, "defer", "stage_count_too_high", tNow, tOmegaPkg.iKeyHash);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	if ( procRunQuotaChain(&tCenter, &tThetaPkg) ) {
		tCenter.iActiveChains--;
		procRecordEvent(&tEvent, tThetaPkg.sKey, "joined", "slot_released", tNow + 1, tThetaPkg.iKeyHash);
		printf("%s -> %s (%s)\n", tEvent.sKey, tEvent.sState, tEvent.sReason);
	}

	if ( (tCenter.iWaitingCount > 0u) && (tCenter.iActiveChains < tCenter.tPolicy.iMaxActiveRecoveries) ) {
		tCenter.iWaitingCount--;
		tCenter.iActiveChains++;
		procRecordEvent(&tEvent, tSigmaPkg.sKey, "promoted", "quota_slot_released", tNow + 2, tSigmaPkg.iKeyHash);
		printf("%s -> %s (%s)\n", tEvent.sKey, tEvent.sState, tEvent.sReason);

		if ( procRunQuotaChain(&tCenter, &tSigmaPkg) ) {
			tCenter.iActiveChains--;
			procRecordEvent(&tEvent, tSigmaPkg.sKey, "joined", "publish_completed", tNow + 3, tSigmaPkg.iKeyHash);
			printf("%s -> %s (%s)\n", tEvent.sKey, tEvent.sState, tEvent.sReason);
		}
	}

	printf("active=%u waiting=%u snapshot=%s\n",
		(unsigned)tCenter.iActiveChains,
		(unsigned)tCenter.iWaitingCount,
		tCenter.sSnapshotPath);
	return 0;
}
```


## 6. 恢复配额里真正要稳定下来的边界

### 6.1 请求线程只提交 hydrate，不直接抢 quota

这页真正稳定下来的，不是“哪个 handler 判断了一下当前并发数”，而是：

- 请求线程只提交恢复意图
- worker 才决定 `run_now / queue_wait / defer`


### 6.2 `waiting` 和 `defer` 必须是两种正式状态

这页里最容易写乱的地方是：

- 把“当前没 slot”也写成 defer

但更稳的做法应该是：

- `queue_wait`
	- 仍然有资格运行，只是暂时等 slot
- `defer_*`
	- 当前策略明确拒绝继续往下跑


### 6.3 quota 层不替代 `task group`

`quota` 在这里真正回答的是：

- 哪条链可以开始
- 哪条链先排队
- 哪条链 join 完成后提升谁

它不回答：

- inspect / verify / hydrate / publish 这些 child 怎样收口

那一层仍然应该交给单条恢复链自己的 `task group`。


### 6.4 join 完成之后必须有正式提升逻辑

如果没有把“slot 释放 -> promote waiting”做成正式逻辑，系统很快就会退化成：

- 请求进来时碰巧能跑就跑
- 之前 waiting 的请求一直没有正式提升点

这会让 quota 变成一个半成品状态机。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果 quota 已经跨机器共享，不该继续只用单机内存状态
- 如果请求之间要支持抢占、优先级和租约续期，应继续补更正式的调度层
- 如果 active / waiting 状态需要跨进程持久化，应继续补数据库或分布式协调层


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 active 数、waiting 数、最近 quota history、最近 defer history
2. `POST /api/hydrate`
	- 只提交恢复意图，让 worker 决定 `run_now / queue_wait / defer`
3. `GET /api/queues/recovery`
	- 查询当前 waiting 队列和最近一次提升记录
4. `POST /api/sweep`
	- 手工或定时触发超时 active scope 清理
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 active / waiting / defer / publish 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个重链路恢复面板](heavy-recovery-chain-dashboard.md)
	对比“单条重链路 scope”和“多条链路 quota admission”到底差在哪
2. [把本地控制台服务升级成一个批处理任务面板](batch-task-dashboard.md)
	回看 `task group` 在“批次 scope”和“恢复链 scope”里的共通边界
3. [把本地控制台服务升级成一个跨机器恢复面板](cross-machine-recovery-dashboard.md)
	把单机 quota 主线继续升级成“远端 peer 选择、remote fetch、publish 回本地”的正式恢复模型
