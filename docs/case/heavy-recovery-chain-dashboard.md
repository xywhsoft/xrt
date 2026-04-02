# 把本地控制台服务升级成一个重链路恢复面板

> 这页要解决的不是“跨介质编排里又多加一个 subprocess”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 warm shadow、package bundle、外部 hydrate 工具和异步发布之后，又开始需要把“同一次恢复请求拆成 manifest inspect / verify / hydrate / publish 多个阶段”“这些阶段有的来自线程任务，有的来自 subprocess future，有的来自 file_async future”“整个恢复请求什么时候才算真正结束”放进同一个正式 scope，还要把当前阶段、最近链路历史和最近 defer 原因稳定交给页面、JSON、日志和快照时，怎样把 `dict + dict + list + list + list + queue + thread + task group + future + subprocess + file_async + path + file + hash + xhttpd + template` 串成一条正式主线，而不是继续把每个阶段散成几段 if/else 和几个互相看不见的 future。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 恢复优先级怎样决定 warm / cool / cold file / defer
- 跨介质编排怎样把 local file、package bundle、subprocess 和 publish 放进同一条主线

但真实服务再往前走一步，很快又会碰到一个新的问题：

- 有些 key 不是“挑一条恢复来源然后马上结束”
- 而是要先 inspect package manifest
- 再 verify bundle
- 再跑 hydrate tool
- 最后还要把 staging 结果 publish 成正式 snapshot
- 页面和 JSON 还要能回答“现在卡在 verify 还是 hydrate”“整条链路什么时候算真正完成”“为什么这次在 stage-2 就 defer 了”

如果这时还把实现停在：

- `pFuture = xrtProcessWaitFuture(pProcess);`
- `if ( done ) publish();`

很快就会出现几个典型问题：

- 一个 `future` 只能回答“某个阶段结束了没有”，不能回答“整条恢复链结束了没有”
- `subprocess` 结束和正式 publish 完成不是同一个完成点
- 请求线程开始关心阶段顺序、阶段超时和失败收口
- 页面为了说明“这条恢复链到底做到哪一步了”开始扫日志和临时变量

所以这页真正要补出的，不是“再多一个工具链例子”，而是：

> 当一次恢复请求会长出多个阶段子任务时，怎样把它做成一条有正式 scope、有最终 join 点、能解释阶段状态的重链路恢复主线。


## 2. 为什么这次不能只靠 `future + subprocess`

### 2.1 单个 `future` 只能回答单个阶段

`future` 很重要，因为它能回答：

- verify 阶段有没有结束
- hydrate 阶段有没有报错
- publish 阶段有没有进入终态

但它不回答：

- 这几个阶段是不是同一次恢复请求
- 什么时候已经不再允许继续给这次恢复加 child
- 谁来给整条恢复链提供 close-aware final barrier


### 2.2 subprocess 完成不等于整条恢复链结束

到了这一步，系统真正要稳定回答的是：

- 工具链是不是已经成功跑完
- staging 文件是不是已经准备好
- publish future 是不是也已经完成
- 当前这条恢复链是不是可以正式写成“done”

这说明：

- `xrtProcessWaitFuture()` 只是阶段 future
- 它不能自动变成整条恢复链的正式完成点


### 2.3 这次真正新增的是“恢复 scope”

更稳的理解方式是：

- `queue + thread`
	- 负责把恢复请求交给后台
- `task group`
	- 负责把 inspect / verify / hydrate / publish 这些 child 收进同一个 scope
- `future`
	- 负责每个阶段的完成态和最终 join

一句话记住：

> 这次你真正要新增的，不是再多写几个 `future*` 变量，而是给一整条恢复链加一个正式的 `task group scope`。


## 3. 这条重链路恢复主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 hot tier | 恢复完成后的实时命中 |
| `dict` | 当前 warm shadow | 最便宜的短链路恢复来源 |
| `list` | 当前 package catalog | 重链路恢复的输入来源 |
| `list` | 最近 stage history | 页面和 JSON 展示“这次做到哪一步了” |
| `list` | 最近 defer history | 页面和 JSON 展示“为什么没继续往下跑” |
| `list` | 最近 publish history | 页面和 JSON 展示“哪次链路真正发布完成了” |
| `queue + thread` | 后台消费 `HYDRATE / SWEEP` | 请求线程不阻塞在重链路上 |
| `task group` | 管住一次恢复请求长出来的所有 child | `Close / Join / Cancel / Destroy` |
| `future` | 表达每个阶段与整条链路的完成态 | 统一结果与等待边界 |
| `subprocess` | verify / hydrate 这类外部工具阶段 | 正式外部链路，不是伪函数调用 |
| `file_async` | 把 staging 结果发布成正式 snapshot | 正式 publish future |
| `path + file` | 组织 package / stage / snapshot 路径 | 让重链路真正落到文件系统 |
| `hash` | 给 key 一个稳定指纹 | 生成 bundle、stage、snapshot 文件名 |
| `xvalue + json` | 导出当前阶段、链路历史和 defer 摘要 | 页面、脚本、运维共享同一份状态模型 |
| `xhttpd + template` | 展示恢复链当前状态和触发入口 | 浏览器与脚本共用一个入口 |

一句话记住：

> `subprocess` 回答“阶段怎么执行”，`future` 回答“阶段有没有结束”，`task group` 回答“这一整条恢复链什么时候才算真正收口”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成重链路恢复语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/heavy-recovery-chain.json
runtime/packages/<hash>.pkg
runtime/stage/<hash>.json
runtime/published/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `max_bundle_bytes`、`max_stage_count`、`join_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前阶段、最近 stage history、最近 defer history、最近 publish history
- `runtime/console.log`
	- 记录 hydrate signal、stage 启动、join 完成和 defer 原因
- `runtime/heavy-recovery-chain.json`
	- 保存最近一次重链路恢复摘要
- `runtime/packages/<hash>.pkg`
	- 保存输入 bundle
- `runtime/stage/<hash>.json`
	- 保存 hydrate 过程中的 staging 输出
- `runtime/published/<hash>.json`
	- 保存发布完成后的正式 snapshot


## 5. 先把“hydrate signal -> worker 创建 scope -> 多阶段 child 统一 join”这条后台主线立起来

下面这段代码故意只保留 9 件事：

1. `dict` 保存当前 hot / warm shadow
2. `list` 保存 package catalog
3. `list` 保存 stage / defer / publish history
4. `queue + thread` 只承接 `HYDRATE / SWEEP`
5. `task group` 只负责一次恢复请求的 scope
6. manifest inspect 用线程 child 表示
7. verify / hydrate 用 subprocess future 表示
8. publish 用 `file_async future` 表示
9. 整条恢复链只靠一个 join future 收口

这个骨架会展示这些事：

- `beta` 仍有 warm shadow，不进入重链路
- `theta` 只有 package bundle，需要 inspect -> verify -> hydrate -> publish
- `omega` 也有 package bundle，但 stage 数超预算，这次直接 defer
- 主线程最后只关心整条恢复链的 join 结果，不自己管理每个阶段的收口

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


## 6. 重链路恢复里真正要稳定下来的边界

### 6.1 请求线程只提交恢复意图，不管理阶段生命周期

这页真正稳定下来的，不是“哪个 handler 拉起了哪个工具”，而是：

- 请求线程只提交 `HYDRATE`
- worker 负责创建这次恢复的 scope
- `task group` 负责把多阶段 child 收口


### 6.2 `task group` 管的是整条恢复链，不是某个阶段

这页里 `task group` 不回答：

- verify 阶段具体做了什么

它回答的是：

- 这次恢复还能不能继续加 child
- 什么时候这次恢复已经 close
- 整条恢复链什么时候 join 完成


### 6.3 subprocess future 只是阶段 child，不是最终完成点

这页里真正容易写错的地方是：

- 看到 `xrtProcessWaitFuture()` 完成，就把整条恢复链标成 done

但更稳的做法应该是：

- 把 verify / hydrate 当成 child future
- 继续把 publish future 也纳入同一个 group
- 只有 join future 完成，才把整条恢复链写成正式 done


### 6.4 `file_async` 的职责是 publish，不是替代阶段编排

`file_async` 在这里真正回答的是：

- staging 结果怎样异步发布到正式 snapshot

它不负责回答：

- 这次到底要不要进 verify
- 这次要不要继续 hydrate
- 这条恢复链什么时候算真正完成


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果恢复链已经跨机器、跨服务，不该继续只用本地 `queue + thread`
- 如果阶段之间需要真正的流式背压，而不是几个明确阶段 future，应继续补 `wait-source` 或更正式的 IO 编排
- 如果恢复请求之间还要做优先级抢占和全局配额，应该继续补调度器层，而不是只靠一个局部 `task group`


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前链路摘要、stage history、defer history、publish history
2. `POST /api/hydrate`
	- 只提交恢复意图，让 worker 创建或拒绝这次重链路 scope
3. `GET /api/recoveries/:key`
	- 查询某个 key 当前卡在哪个阶段、最近一次 join 结果是什么
4. `POST /api/sweep`
	- 手工或定时触发超时 scope 清理
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染当前阶段、defer 原因和 publish 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个跨介质编排面板](cross-media-orchestration-dashboard.md)
	对比“单次跨介质恢复”和“多阶段重链路恢复”到底差在哪
2. [把本地控制台服务升级成一个批处理任务面板](batch-task-dashboard.md)
	回看 `task group` 在“批次 child 收口”和“恢复链 child 收口”里的共通边界
3. [把本地控制台服务升级成一个恢复配额面板](recovery-quota-dashboard.md)
	把单条重链路恢复继续升级成“多条链路排队、抢 slot、join 后提升”的正式 admission 主线
