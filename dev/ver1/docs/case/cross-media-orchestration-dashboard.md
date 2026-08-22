# 把本地控制台服务升级成一个跨介质编排面板

> 这页要解决的不是“文件恢复失败了就再试一个包文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 warm / cool / cold file / package bundle / 外部工具链这些恢复来源之后，又开始需要把“请求线程只提交 hydrate signal”“worker 决定走内存、走本地文件、走包文件，还是拉起外部工具”“重链路恢复完成后还要统一发布回本地快照和 dashboard”放进同一条后台主线，还要把当前恢复来源、最近编排历史和最近 defer 历史稳定交给页面、JSON、日志和快照时，怎样把 `dict + dict + list + list + queue + thread + future + subprocess + file_async + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让本地文件恢复、包文件回放和外部工具链各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经把本地控制台服务的基础链路搭起来，现在恢复来源开始变多：

- 有些 key 还留着 warm 或 cool 影子
- 有些 key 只剩本地 cold file
- 有些 key 只有一个 package bundle，需要先解包或校验
- 有些 key 的恢复已经不是“读一个本地文件”，而是“调起一个外部 hydrate 工具链”
- 页面和 JSON 还要能稳定解释“这次为什么直接走本地文件”“为什么切到了包文件”“为什么拉起了 subprocess”“为什么这次先 defer”

如果这时还把实现停在：

- `if ( local ) restore_local(); else restore_bundle();`

很快就会出现几个典型问题：

- 本地文件、本地包文件和外部工具链之间没有正式优先级
- 请求线程开始背上解包、校验和发布逻辑
- 重链路恢复完成后的快照发布没有统一收口
- 页面为了说明“这次恢复到底走了哪条链路”开始扫日志和 worker 临时变量

所以这页真正要补出的，不是“再多一个 fallback 分支”，而是：

> 当恢复路径已经跨内存、本地文件、包文件和外部工具链时，怎样把恢复链路本身做成一条正式、可解释、可导出的跨介质编排主线。


## 2. 为什么这次不能再把“恢复来源”和“恢复动作”混在一起

### 2.1 local file 回答的是“最低额外代价的持久化恢复”

本地文件更适合回答：

- 当前没有 warm / cool
- 但本机仍有正式 snapshot
- 不需要外部工具链就能恢复


### 2.2 package bundle 回答的是“恢复来源还在本地，但已经不是单个快照文件”

package bundle 更适合回答：

- 当前 cold file 可能已经被归并
- 需要从包文件里重新hydrate
- 但还没必要立刻切到更重的外部链路


### 2.3 subprocess 回答的是“恢复已经变成工具链任务”

subprocess 真正解决的是：

- 这次恢复不是一个普通文件读取
- 可能还要解包、校验、转换或重写
- 它应该进入正式 future / worker 编排，而不是卡在请求线程里


### 2.4 file_async 回答的是“重链路恢复后如何统一发布”

这页里 `file_async` 不是可有可无的点缀，而是用来回答：

- 恢复完成后怎样把 staging 结果统一发布到正式 snapshot
- 为什么不该让外部工具链直接覆盖 dashboard 快照


## 3. 这条跨介质编排主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 hot tier | 当前热点查询与恢复后的实时命中 |
| `dict` | 当前 warm tier | 最便宜的本机恢复来源 |
| `list` | 当前 local file catalog | 本地 snapshot 恢复来源 |
| `list` | 当前 package catalog | 包文件恢复来源与编排入口 |
| `list` | 最近 orchestration history | 页面时间线和“这次走了哪条链路”导出 |
| `list` | 最近 defer history | 页面时间线和“这次为什么先不跑重链路”解释 |
| `queue + thread` | 后台执行 `HYDRATE / SWEEP` | 请求线程不阻塞在跨介质恢复上 |
| `future` | 跟踪工具链阶段结果 | 让编排任务有正式完成点 |
| `subprocess` | 拉起解包、校验、转换等外部链路 | 把重恢复链路收进正式 runtime |
| `file_async` | 发布 staging 文件与结果快照 | 让重链路输出统一落盘 |
| `path + file` | 组织 cold / package / stage 路径 | 让恢复来源和中间结果进入文件系统 |
| `hash` | 给对象 key 一个稳定指纹 | 生成包文件名和页面轻量标识 |
| `xvalue + json` | 导出当前来源、编排历史和 defer 摘要 | 页面、脚本、恢复逻辑共享同一份模型 |
| `xhttpd + template` | 展示恢复来源与编排结果 | 浏览器和脚本共享一个入口 |

一句话记住：

> `warm` 管“最便宜的本机恢复”，`local file` 管“单文件恢复”，`package bundle` 管“包级恢复来源”，`subprocess + future + file_async` 管“真正的跨介质重链路编排与发布”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成跨介质编排语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/cross-media-orchestration.json
runtime/cold/<hash>.json
runtime/packages/<hash>.pkg
runtime/stage/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `max_local_file_cost`、`max_package_bytes`、`max_toolchain_cost`
- `web/dashboard.html`
	- 同时展示当前恢复来源、最近 orchestration history、最近 defer history
- `runtime/console.log`
	- 记录 hydrate signal、编排决策、subprocess 启动和发布完成
- `runtime/cross-media-orchestration.json`
	- 保存最近一次跨介质编排摘要
- `runtime/cold/<hash>.json`
	- 保存单个本地 cold snapshot
- `runtime/packages/<hash>.pkg`
	- 保存单个 package bundle
- `runtime/stage/<hash>.json`
	- 保存重链路恢复后的 staging 文件


## 5. 先把“hydrate signal -> worker 选链路 -> publish”这条后台主线立起来

下面这段代码故意只保留 8 件事：

1. `dict` 保存当前 hot / warm
2. `list` 保存 local file catalog
3. `list` 保存 package catalog
4. `list` 保存最近 orchestration history
5. `list` 保存最近 defer history
6. `queue + thread` 只处理 `HYDRATE / SWEEP`
7. `future` 只用一个占位句柄表示“重链路正在跑”
8. `subprocess + file_async` 只体现在 worker 选出来的正式 plan 里

这个骨架会实际做这些事：

- 初始化 orchestration center 和跨介质策略
- 让 `beta` 仍有 warm 影子，直接走 warm
- 让 `gamma` 只有 local file，走本地文件恢复
- 让 `delta` 只有 package bundle，直接走包级 hydrate
- 让 `epsilon` 需要外部校验，走 subprocess hydrate
- 让 `zeta` 也只能走工具链，但当前成本超预算，这次进入 defer
- 主线程打印最终编排路径与 defer 原因

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


## 6. 跨介质编排里真正要稳定下来的边界

### 6.1 请求线程只提交 hydrate 意图，不直接拉起工具链

这页真正稳定下来的，不是“哪个 handler 里拼了哪个命令”，而是：

- 请求线程只提交 `HYDRATE`
- worker 才真正决定 local / package / subprocess


### 6.2 package bundle 不是本地文件的别名

如果没有把 package bundle 当成独立恢复来源，系统很快就会退化成：

- 只要不是单文件，就直接走外部工具

这会让恢复链路过早变重。


### 6.3 subprocess 一旦成立，就必须有正式完成点

这页里 `subprocess` 不再只是：

- 调一下外部工具

它还必须回答：

- 什么时候开始
- 什么时候结束
- 谁来接住完成点

所以它应该和：

- `future`
- worker 历史

一起成为正式状态。


### 6.4 file_async 的职责是发布，不是替代恢复决策

`file_async` 在这里回答的是：

- 重链路输出怎样统一发布到 staging / snapshot

它不负责回答：

- 这次恢复到底走哪条链路


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先改模型，再决定是否复用这套骨架：

- 如果恢复已经跨机器、跨服务，不该继续只用本地 `queue + thread`
- 如果你真正要做的是一致性回放或分布式补偿，应该继续往更重的编排模型升级
- 如果外部工具链已经变成长流程，应继续补更细的 `future / wait-source` 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前恢复来源、orchestration history、defer history 摘要
2. `POST /api/hydrate`
	- 只提交 hydrate signal，让 worker 决定 local / package / subprocess
3. `POST /api/sweep`
	- 手工触发或定时触发恢复来源清理
4. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染来源状态、编排历史和 defer 原因


## 9. 下一步阅读

如果你想对照相邻案例，建议一起阅读：

1. [用 Subprocess + File Async 写一个工具链流水线](subprocess-file-async-pipeline.md)
	对照“单条工具链主线”和“业务恢复编排”怎么接起来。
2. [把本地控制台服务升级成一个子进程探测面板](subprocess-probe-dashboard.md)
	对照“把外部 CLI 接进后台 worker”这一层是怎么落地的。
3. [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
	对照“冷热状态拆分”和“多来源恢复编排”分别解决的是什么问题。
