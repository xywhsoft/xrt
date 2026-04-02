# 把本地控制台服务升级成一个恢复优先级面板

> 这页要解决的不是“warm 有就先拿，没有就读文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、warm tier、cool tier、cold file 和 archive 语义之后，又开始需要把“请求线程只提交恢复信号”“worker 按恢复优先级挑选 warm / cool / cold file”“冷文件恢复还要服从成本预算和超时窗口”“页面和 JSON 要稳定解释这次为什么走了这条恢复路径”放进同一条后台主线，还要把当前各层状态、最近恢复历史和最近 defer 历史稳定交给页面、JSON、日志和快照时，怎样把 `dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让 warm 命中、冷文件恢复和超时兜底各写各的。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- hot / warm / cool / cold file 这些层怎么拆
- 自动回温和滚动归档怎样协同
- 多层存储和多级老化怎样建立正式边界

但真实服务再往前走一步，很快就会出现一个新的问题：

- 同一个 key 可能仍有 warm 影子
- 也可能已经有 cool 影子
- 甚至还保留着一份 cold file snapshot
- 请求线程这次只说“我要恢复这个 key”，却不该直接把恢复路径写死
- 页面和 JSON 还要能回答“这次为什么走 warm，不是走 cool”“为什么这次没有立刻读冷文件，而是被 defer 了”

如果这时还把实现停在：

- `if ( warm ) ... else if ( cool ) ... else read file`

很快就会出现几个典型问题：

- 恢复顺序只剩分支顺序，没有正式策略
- warm / cool 的新鲜度窗口和 cold file 的成本预算没有统一边界
- 请求线程开始背上文件恢复和降级判断
- 页面为了展示“为什么恢复失败或延迟了”开始扫日志和 worker 临时变量

所以这页真正要补出的，不是“再多写一个 if”，而是：

> 当同一个 key 可能有多条恢复来源时，怎样把恢复优先级本身做成一条正式、可解释、可导出的后台主线。


## 2. 为什么这次不能再用“找到哪个就直接恢复”

### 2.1 `warm / cool / cold file` 不是同一成本

`warm` 更适合回答：

- 这份对象刚离开 hot
- 恢复成本要尽量接近热层
- 优先级应该最高

`cool` 更适合回答：

- 对象已经离开最活跃窗口
- 但还没必要立刻触发文件恢复
- 它应该排在 warm 之后、cold file 之前

`cold file` 更适合回答：

- 内存层已经没有合适恢复来源
- 这次真的需要付出文件恢复成本
- 但它仍然要服从 restore budget、超时窗口和 defer 策略


### 2.2 恢复优先级不是分支顺序，而是服务策略

这页真正稳定下来的，不是：

- 代码里 `if / else` 的先后顺序

而是：

- warm 新鲜度窗口
- cool 新鲜度窗口
- cold file 成本预算
- defer 是否已经成为正式状态


### 2.3 “defer” 不是失败日志，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 这次恢复为什么没走文件
- 是因为没有来源，还是因为来源太贵
- 这次请求是 miss，还是 defer

所以 defer 不该只留在日志里，而应该进入：

- 最近 defer history
- dashboard snapshot
- API 返回的正式字段


## 3. 这条恢复优先级主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 hot tier | 当前热点查询与恢复后的实时命中 |
| `dict` | 当前 warm tier | 最优先、最低成本的恢复来源 |
| `dict` | 当前 cool tier | 次优先、仍在内存里的恢复来源 |
| `list` | 当前 cold catalog | 正式落盘的恢复来源与文件路径 |
| `list` | 最近 restore history | 页面时间线和恢复事件导出 |
| `list` | 最近 defer history | 页面时间线和“这次为什么暂缓冷文件恢复”解释 |
| `queue + thread` | 后台执行 `RESTORE / SWEEP` | 请求线程不阻塞在恢复选型和文件恢复上 |
| `time` | 记录 warm / cool 新鲜度和 restore deadline | 恢复优先级的正式时间边界 |
| `path + file` | 组织 cold file 路径和 dashboard 快照路径 | 让冷文件恢复真正进入文件系统 |
| `hash` | 给对象 key 一个稳定指纹 | 生成冷文件名与页面轻量标识 |
| `xvalue + json` | 导出当前层级、restore 和 defer 摘要 | 页面、脚本、恢复逻辑共享同一份模型 |
| `xhttpd + template` | 展示恢复来源与 defer 原因 | 浏览器和脚本共享一个入口 |

一句话记住：

> `warm` 管“最便宜的恢复来源”，`cool` 管“第二便宜的恢复来源”，`cold file` 管“正式落盘后的恢复来源”，worker 管“这次恢复到底该走哪一层，还是先 defer”。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成恢复优先级语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/recovery-priority.json
runtime/cold/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `warm_fresh_ms`、`cool_fresh_ms`、`max_cold_file_cost`
- `web/dashboard.html`
	- 同时展示当前 hot / warm / cool / cold、最近 restore history、最近 defer history
- `runtime/console.log`
	- 记录 restore signal、实际恢复来源和 defer 原因
- `runtime/recovery-priority.json`
	- 保存最近一次恢复优先级摘要
- `runtime/cold/<hash>.json`
	- 保存单个对象的 cold snapshot 文件


## 5. 先把“恢复信号 -> worker 选来源 -> restore 或 defer”这条后台主线立起来

下面这段代码故意只保留 7 件事：

1. `dict` 保存当前 hot / warm / cool 三层
2. `list` 保存当前 cold catalog
3. `list` 保存最近 restore history
4. `list` 保存最近 defer history
5. `queue + thread` 只处理 `RESTORE / SWEEP`
6. `time` 只负责 warm / cool 新鲜度窗口
7. `path + file` 只负责 cold file 路径，不让请求线程自己拼文件

这个骨架会实际做这些事：

- 初始化 recovery center 和恢复策略
- 让 `beta` 同时拥有 warm / cool / cold 三条候选来源，但优先走 warm
- 让 `gamma` 的 warm 影子已经过期，只能走 cool
- 让 `delta` 没有内存来源，只能走 cold file
- 让 `epsilon` 也只有 cold file，但文件成本超预算，这次进入 defer
- 主线程打印最终恢复路径与 defer 原因

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


## 6. 恢复优先级里真正要稳定下来的边界

### 6.1 请求线程只提交恢复意图，不直接决定恢复来源

这页真正稳定下来的，不是“某个 handler 里怎么写 if / else”，而是：

- 请求线程只提交 `RESTORE`
- worker 才真正决定 warm / cool / cold file / defer


### 6.2 `warm` 和 `cool` 的区别不是名字，而是窗口

如果没有把：

- `warm_fresh_ms`
- `cool_fresh_ms`

写成正式策略字段，那么恢复顺序就会退化成：

- 代码先看到谁就恢复谁


### 6.3 `cold file` 一旦进入优先级模型，就必须服从预算

这一页里 `cold file` 不再只是：

- 最后兜底

它还要明确回答：

- 这次文件恢复贵不贵
- 当前是不是应该 defer


### 6.4 `defer` 一旦成立，就该进入页面和快照

如果 defer 只是日志里一句话，系统就无法稳定回答：

- 这次到底是 miss 还是预算延后
- 页面为什么没有立刻恢复这个 key


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先改模型，再决定是否复用这套骨架：

- 如果你真正需要的是一致性快照回放，而不是恢复优先级，应该先补跨介质编排
- 如果 cold file 恢复已经是批量慢 I/O，应该继续拆 `file_async`
- 如果恢复路径里还要拉外部工具、解压或校验，应该继续补 `subprocess`


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 hot / warm / cool / cold / restore / defer 摘要
2. `POST /api/restore`
	- 只提交恢复信号，让 worker 决定来源
3. `POST /api/sweep`
	- 手工触发或定时触发恢复窗口清理
4. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染层级状态、恢复历史和 defer 原因


## 9. 下一步阅读

如果你准备继续把恢复路径做得更复杂，最顺的下一步是：

1. [把本地控制台服务升级成一个冷热多级老化面板](hot-cold-multi-level-aging-dashboard.md)
	对比“多级老化窗口”和“恢复优先级策略”到底差在哪
2. [把本地控制台服务升级成一个多层存储面板](multi-tier-storage-dashboard.md)
	对比“跨介质恢复来源”和“恢复优先级调度”的边界区别
3. [把本地控制台服务升级成一个跨介质编排面板](cross-media-orchestration-dashboard.md)
	把文件恢复、包文件和外部工具链继续放进统一调度
