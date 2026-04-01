# 把本地控制台服务升级成一个自动回温策略面板

> 这个案例要解决的不是“访问两次就回温”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、cold tier 和 warm-back 主线之后，又开始需要把“命中阈值”“时间窗口”“自动回温事件”和当前冷热状态稳定交给页面、JSON、日志和快照时，怎样把 `dict + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让命中统计、回温判定和页面展示各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面的冷热分层和冷层回温主线：

- 热点对象在热层
- 已降温对象在冷层
- 某些冷对象可以被重新拉回热层
- 页面和 JSON 已经能看到冷热两层与最近回温历史

现在系统又往前走一步，开始出现一个更真实的业务要求：

- 某个冷层对象被偶尔访问一次，不应该马上回温
- 某个冷层对象在一个短时间窗口里连续被访问多次，就应该自动回温
- 页面要看到“当前冷层对象已经累计了几次回温命中”
- 回温一旦发生，还要留下正式事件记录
- 最新冷热与策略状态还要继续落成快照文件

如果这时只写成：

- 访问一次冷层就立即回温
- 或者请求线程里自己记一个临时计数器

很快就会出现 4 个问题：

- 请求线程开始承担命中统计、策略判断和回温收口
- 冷层对象当前累计了多少次命中没有正式状态来源
- 页面为了显示“距自动回温还差几次”开始扫 worker 临时状态
- 回温策略和显式 warm job 边界混在一起，后面谁也不敢改

所以这页真正要补出的，不是“怎么写一个 if hit_count >= 2”，而是：

> 当系统已经有了 cold tier，怎样把自动回温策略变成一条正式后台主线，并给它一份稳定可导出的状态模型。


## 2. 为什么这次不是“命中一次就直接回温”

### 2.1 自动回温解决的是“持续热度”，不是“一次读取”

如果一个冷对象只是偶尔被访问一次，它可能只是：

- 一次探测
- 一次偶发查看
- 一次并不值得重新拉回热层的读取

自动回温真正要回答的是：

- 这个冷对象是不是重新变热了

所以这里判断的不是：

- 是否发生过一次命中

而是：

- 在一个正式时间窗口里，是否出现了足够的连续命中


### 2.2 请求线程不该直接承担策略判定和 promotion 收口

请求线程更适合做：

- 识别一个冷层命中
- 生成一个 access signal
- 立即返回已接收或当前冷态

它不适合直接承担：

- 命中计数更新
- 时间窗口重置
- 自动回温判定
- 热层恢复
- 事件记录和快照写盘

这些动作一旦变重，请求线程马上退化。


### 2.3 自动回温需要两份正式状态

这页真正需要的，不只是：

- 最近回温了什么

还需要：

- 当前冷层对象已经累计了多少次命中
- 这轮策略窗口从什么时候开始

所以这页里除了热层、冷层和回温历史，还必须把冷层里的：

- 策略命中状态

也写成正式字段，而不是临时变量。


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存当前热层对象 | 当前热点查询与自动回温后的实时命中 |
| `list` | 保存当前冷层窗口与策略状态 | 当前冷对象、累计命中数、窗口起点 |
| `list` | 保存最近自动回温历史 | 页面时间线和自动回温事件导出 |
| `queue + thread` | 后台执行 access signal 与策略判定 | 请求线程不阻塞在策略收口上 |
| `time` | 记录冷却时间、命中窗口和回温时间 | 自动回温策略的正式时间边界 |
| `hash` | 给对象 key 一个稳定指纹 | 页面、日志和快照里的轻量标识 |
| `path + file` | 组织自动回温快照路径并落盘 | 让策略状态进入真实文件系统 |
| `xvalue + json` | 导出当前冷热与策略摘要 | 页面、脚本、快照共享同一份模型 |
| `xhttpd + template` | 展示冷热状态、当前命中和回温历史 | 浏览器和脚本共享一个入口 |

一句话记住：

> `dict` 管“现在已经重新变热了什么”，一条 `list` 管“当前冷层对象与它们的自动回温命中状态”，另一条 `list` 管“最近自动回温了什么”，worker 管“什么时候由策略正式 promotion 回热层”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成自动回温策略面板语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/auto-warm-policy.json
```

其中：

- `config/console.json`
	- 保存最小回温命中次数、命中窗口和队列容量
- `web/dashboard.html`
	- 同时展示当前热层、当前冷层和最近自动回温历史
- `runtime/console.log`
	- 记录访问命中、策略触发和自动回温完成
- `runtime/auto-warm-policy.json`
	- 保存最近一次冷热与策略摘要


## 5. 先把冷层命中、策略窗口和自动回温通道立起来

下面这段代码故意先聚焦 auto-warm policy 核心，不把完整 `xhttpd` handler 和模板渲染全部塞进去。

它保留最关键的 5 件事：

1. `dict` 保存当前热层对象
2. `list` 保存当前冷层窗口和命中状态
3. `list` 保存最近自动回温历史
4. `queue + thread` 把 access signal 和策略判定放到后台
5. `path + file + json` 把当前冷热与策略摘要落成 `runtime/auto-warm-policy.json`

这个骨架会实际做这些事：

- 初始化 auto-warm center
- 注册几个热层对象
- 先把两个对象手工转入冷层
- 连续提交 3 次 access signal
- worker 用“2 次命中 / 400ms 窗口”策略判断是否自动回温
- 主线程打印热层、冷层、最近自动回温历史和最终快照

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iHotId;
	xtime tTouchedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoHotItem;

typedef struct
{
	xtime tTouchedAt;
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
	xtime tWarmedAt;
	xtime tPrevCooledAt;
	xtime tFirstWarmHitAt;
	xtime tLastWarmHitAt;
	uint64 iKeyHash;
	int iHeat;
	int iWarmHits;
	char sKey[64];
	char sTitle[64];
	char sReason[48];
} DemoWarmEvent;

typedef struct
{
	char sKey[64];
	char sSource[48];
} DemoAccessJob;

typedef struct
{
	const char* sKey;
	bool bFound;
	int64 iSeq;
} DemoColdLookup;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xlist pWarmLog;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextWarmSeq;
	uint32 iKeepCold;
	uint32 iKeepWarm;
	uint32 iWarmWindowMs;
	uint32 iWarmMinHits;
	char sSnapshotPath[260];
} DemoAutoWarmCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static bool procCollectHotSnapshot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue vHotArr = (xvalue)pArg;
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	xvalue vRow;
	(void)pKey;

	if ( (vHotArr == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pItem->iKeyHash);
	(void)xvoArrayAppendValue(vHotArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectColdSnapshot(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vColdArr = (xvalue)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vColdArr == NULL) || (pEntry == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pEntry->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "last_hit_source", 0u, pEntry->sLastHitSource, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEntry->iHeat);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEntry->iWarmHits);
	xvoTableSetInt(vRow, "touched_at", 0u, pEntry->tTouchedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "first_warm_hit_at", 0u, pEntry->tFirstWarmHitAt);
	xvoTableSetInt(vRow, "last_warm_hit_at", 0u, pEntry->tLastWarmHitAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEntry->iKeyHash);
	(void)xvoArrayAppendValue(vColdArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectWarmSnapshot(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vWarmArr = (xvalue)pArg;
	DemoWarmEvent* pEvent = (DemoWarmEvent*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vWarmArr == NULL) || (pEvent == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, pEvent->sReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEvent->iHeat);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEvent->iWarmHits);
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	xvoTableSetInt(vRow, "prev_cooled_at", 0u, pEvent->tPrevCooledAt);
	xvoTableSetInt(vRow, "first_warm_hit_at", 0u, pEvent->tFirstWarmHitAt);
	xvoTableSetInt(vRow, "last_warm_hit_at", 0u, pEvent->tLastWarmHitAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEvent->iKeyHash);
	(void)xvoArrayAppendValue(vWarmArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoAutoWarmCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHotArr = NULL;
	xvalue vColdArr = NULL;
	xvalue vWarmArr = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0u;
	bool bOk = FALSE;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	vRoot = xvoCreateTable();
	vHotArr = xvoCreateArray();
	vColdArr = xvoCreateArray();
	vWarmArr = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHotSnapshot, vHotArr);
	xrtListWalk(pCenter->pCold, procCollectColdSnapshot, vColdArr);
	xrtListWalk(pCenter->pWarmLog, procCollectWarmSnapshot, vWarmArr);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "warm_event_count", 0u, xrtListCount(pCenter->pWarmLog));
	xvoTableSetInt(vRoot, "warm_window_ms", 0u, pCenter->iWarmWindowMs);
	xvoTableSetInt(vRoot, "warm_min_hits", 0u, pCenter->iWarmMinHits);
	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHotArr, TRUE);
	xvoUnref(vHotArr);
	vHotArr = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vColdArr, TRUE);
	xvoUnref(vColdArr);
	vColdArr = NULL;
	xvoTableSetValue(vRoot, "warm_events", 0u, vWarmArr, TRUE);
	xvoUnref(vWarmArr);
	vWarmArr = NULL;

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	sDir = xrtPathGetDir(pCenter->sSnapshotPath, 0u);
	if ( (sDir == NULL) || (xrtDirCreateAll(sDir) != TRUE) ) {
		goto cleanup;
	}

	if ( xrtFileWriteAll(pCenter->sSnapshotPath, sJson, iJsonSize, XRT_CP_UTF8) < 0 ) {
		goto cleanup;
	}

	bOk = TRUE;

cleanup:
	if ( sDir != NULL ) {
		xrtFree(sDir);
	}
	if ( sJson != NULL ) {
		xrtFree(sJson);
	}
	if ( vWarmArr != NULL ) {
		xvoUnref(vWarmArr);
	}
	if ( vColdArr != NULL ) {
		xvoUnref(vColdArr);
	}
	if ( vHotArr != NULL ) {
		xvoUnref(vHotArr);
	}
	if ( vRoot != NULL ) {
		xvoUnref(vRoot);
	}
	return bOk;
}

static bool procAppendColdLocked(DemoAutoWarmCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
{
	DemoColdEntry* pEntry = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	pCenter->iNextColdSeq++;
	iSeq = pCenter->iNextColdSeq;
	pEntry = (DemoColdEntry*)xrtListSet(pCenter->pCold, iSeq, &bNew);
	if ( pEntry == NULL ) {
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pItem->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pItem->sTitle);
	procCopyText(pEntry->sTierReason, sizeof(pEntry->sTierReason), sTierReason);
	pEntry->iKeyHash = pItem->iKeyHash;
	pEntry->iHeat = pItem->iHeat;
	pEntry->tTouchedAt = pItem->tTouchedAt;
	pEntry->tCooledAt = xrtNow();

	if ( iSeq > (int64)pCenter->iKeepCold ) {
		(void)xrtListRemove(pCenter->pCold, iSeq - (int64)pCenter->iKeepCold);
	}

	return TRUE;
}

static bool procAppendWarmEventLocked(DemoAutoWarmCenter* pCenter, const DemoColdEntry* pCold, const char* sReason)
{
	DemoWarmEvent* pEvent = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pCold == NULL) ) {
		return FALSE;
	}

	pCenter->iNextWarmSeq++;
	iSeq = pCenter->iNextWarmSeq;
	pEvent = (DemoWarmEvent*)xrtListSet(pCenter->pWarmLog, iSeq, &bNew);
	if ( pEvent == NULL ) {
		return FALSE;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->iWarmHits = pCold->iWarmHits;
	pEvent->tPrevCooledAt = pCold->tCooledAt;
	pEvent->tFirstWarmHitAt = pCold->tFirstWarmHitAt;
	pEvent->tLastWarmHitAt = pCold->tLastWarmHitAt;
	pEvent->tWarmedAt = xrtNow();

	if ( iSeq > (int64)pCenter->iKeepWarm ) {
		(void)xrtListRemove(pCenter->pWarmLog, iSeq - (int64)pCenter->iKeepWarm);
	}

	return TRUE;
}

static bool procFindColdWalk(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdLookup* pLookup = (DemoColdLookup*)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;

	if ( (pLookup == NULL) || (pEntry == NULL) || (pLookup->sKey == NULL) ) {
		return FALSE;
	}

	if ( strcmp(pEntry->sKey, pLookup->sKey) == 0 ) {
		pLookup->bFound = TRUE;
		pLookup->iSeq = iKey;
		return TRUE;
	}

	return FALSE;
}

static bool procAutoWarmCenterInit(DemoAutoWarmCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 8u;
	pCenter->iKeepWarm = 8u;
	pCenter->iWarmWindowMs = 400u;
	pCenter->iWarmMinHits = 2u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pCenter->pHot == NULL ) {
		return FALSE;
	}

	pCenter->pCold = xrtListCreate(sizeof(DemoColdEntry), XRT_OBJMODE_SHARED);
	if ( pCenter->pCold == NULL ) {
		xrtDictDestroy(pCenter->pHot);
		pCenter->pHot = NULL;
		return FALSE;
	}

	pCenter->pWarmLog = xrtListCreate(sizeof(DemoWarmEvent), XRT_OBJMODE_SHARED);
	if ( pCenter->pWarmLog == NULL ) {
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pWarmLog);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pWarmLog = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pWarmLog);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hMutex = NULL;
		pCenter->pWarmLog = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	sSnapshotPath = xrtPathJoin(2, "runtime", "auto-warm-policy.json");
	if ( sSnapshotPath == NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pWarmLog);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hQueue = NULL;
		pCenter->hMutex = NULL;
		pCenter->pWarmLog = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoAutoWarmCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
{
	DemoHotItem* pItem = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pItem = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
	if ( pItem == NULL ) {
		return FALSE;
	}

	memset(pItem, 0, sizeof(*pItem));
	pCenter->iNextHotId++;
	pItem->iHotId = pCenter->iNextHotId;
	pItem->tTouchedAt = xrtNow();
	pItem->iKeyHash = xrtHash64((ptr)sKey, strlen(sKey));
	pItem->iHeat = iHeat;
	procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);
	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
		xrtFree(pItem);
		return FALSE;
	}

	return TRUE;
}

static bool procMoveHotToColdLocked(DemoAutoWarmCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoHotItem* pItem = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));
	if ( pItem == NULL ) {
		return FALSE;
	}

	(void)procAppendColdLocked(pCenter, pItem, sTierReason);
	xrtFree(pItem);
	return TRUE;
}

static bool procSubmitAccess(DemoAutoWarmCenter* pCenter, const char* sKey, const char* sSource)
{
	DemoAccessJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoAccessJob*)xrtMalloc(sizeof(DemoAccessJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sSource, sizeof(pJob->sSource), sSource);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procAutoWarmWorker(ptr pArg)
{
	DemoAutoWarmCenter* pCenter = (DemoAutoWarmCenter*)pArg;
	DemoAccessJob* pJob = NULL;
	DemoColdLookup tLookup;
	DemoColdEntry* pCold = NULL;
	DemoHotItem* pHot = NULL;
	xqueue_result iRet;
	xtime tNow;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		xrtSleep(40u);

		xrtMutexLock(pCenter->hMutex);
		memset(&tLookup, 0, sizeof(tLookup));
		tLookup.sKey = pJob->sKey;
		xrtListWalk(pCenter->pCold, procFindColdWalk, &tLookup);

		if ( tLookup.bFound ) {
			pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, tLookup.iSeq);
			if ( pCold != NULL ) {
				tNow = xrtNow();
				if ( (pCold->tFirstWarmHitAt == 0) || ((tNow - pCold->tFirstWarmHitAt) > (xtime)pCenter->iWarmWindowMs) ) {
					pCold->iWarmHits = 1;
					pCold->tFirstWarmHitAt = tNow;
				} else {
					pCold->iWarmHits++;
				}
				pCold->tLastWarmHitAt = tNow;
				procCopyText(pCold->sLastHitSource, sizeof(pCold->sLastHitSource), pJob->sSource);

				if ( pCold->iWarmHits >= (int)pCenter->iWarmMinHits ) {
					pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
					if ( pHot != NULL ) {
						memset(pHot, 0, sizeof(*pHot));
						pCenter->iNextHotId++;
						pHot->iHotId = pCenter->iNextHotId;
						pHot->tTouchedAt = tNow;
						pHot->iKeyHash = pCold->iKeyHash;
						pHot->iHeat = pCold->iHeat + pCold->iWarmHits + 1;
						procCopyText(pHot->sKey, sizeof(pHot->sKey), pCold->sKey);
						procCopyText(pHot->sTitle, sizeof(pHot->sTitle), pCold->sTitle);

						if ( xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
							(void)procAppendWarmEventLocked(pCenter, pCold, "policy-hit-threshold");
							(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
							pHot = NULL;
						}
					}
				}
				(void)procWriteSnapshotLocked(pCenter);
			}
		}
		xrtMutexUnlock(pCenter->hMutex);

		if ( pHot != NULL ) {
			xrtFree(pHot);
			pHot = NULL;
		}
		xrtFree(pJob);
	}

	return 0u;
}

static bool procDumpHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		printf(
			"hot key=%s title=%s heat=%d touched_at=%lld hash=%llu\n",
			pItem->sKey,
			pItem->sTitle,
			pItem->iHeat,
			(long long)pItem->tTouchedAt,
			(unsigned long long)pItem->iKeyHash
		);
	}
	return FALSE;
}

static bool procDumpCold(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	(void)pArg;

	printf(
		"cold seq=%lld key=%s title=%s hits=%d first_hit=%lld last_hit=%lld source=%s cooled_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->iWarmHits,
		(long long)pEntry->tFirstWarmHitAt,
		(long long)pEntry->tLastWarmHitAt,
		pEntry->sLastHitSource,
		(long long)pEntry->tCooledAt,
		(unsigned long long)pEntry->iKeyHash
	);
	return FALSE;
}

static bool procDumpWarm(int64 iKey, ptr pVal, ptr pArg)
{
	DemoWarmEvent* pEvent = (DemoWarmEvent*)pVal;
	(void)pArg;

	printf(
		"warm seq=%lld key=%s title=%s reason=%s hits=%d warmed_at=%lld first_hit=%lld last_hit=%lld hash=%llu\n",
		(long long)iKey,
		pEvent->sKey,
		pEvent->sTitle,
		pEvent->sReason,
		pEvent->iWarmHits,
		(long long)pEvent->tWarmedAt,
		(long long)pEvent->tFirstWarmHitAt,
		(long long)pEvent->tLastWarmHitAt,
		(unsigned long long)pEvent->iKeyHash
	);
	return FALSE;
}

static bool procFreeHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		xrtFree(pItem);
	}
	return FALSE;
}

static void procAutoWarmCenterUnit(DemoAutoWarmCenter* pCenter)
{
	if ( pCenter == NULL ) {
		return;
	}

	if ( pCenter->hQueue != NULL ) {
		xrtMPSCQWaitClose(pCenter->hQueue);
	}
	if ( pCenter->hWorker != NULL ) {
		xrtThreadWait(pCenter->hWorker);
		xrtThreadDestroy(pCenter->hWorker);
		pCenter->hWorker = NULL;
	}
	if ( pCenter->hQueue != NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		pCenter->hQueue = NULL;
	}

	if ( (pCenter->hMutex != NULL) && (pCenter->pHot != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pHot, procFreeHot, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}

	if ( pCenter->hMutex != NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		pCenter->hMutex = NULL;
	}
	if ( pCenter->pWarmLog != NULL ) {
		xrtListDestroy(pCenter->pWarmLog);
		pCenter->pWarmLog = NULL;
	}
	if ( pCenter->pCold != NULL ) {
		xrtListDestroy(pCenter->pCold);
		pCenter->pCold = NULL;
	}
	if ( pCenter->pHot != NULL ) {
		xrtDictDestroy(pCenter->pHot);
		pCenter->pHot = NULL;
	}
}

int main(void)
{
	DemoAutoWarmCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procAutoWarmCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procAutoWarmWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procAutoWarmCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 4) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 2) ) {
		goto cleanup;
	}

	xrtMutexLock(tCenter.hMutex);
	(void)procMoveHotToColdLocked(&tCenter, "node:beta", "idle-window");
	(void)procMoveHotToColdLocked(&tCenter, "node:gamma", "cooldown-batch");
	(void)procWriteSnapshotLocked(&tCenter);
	xrtMutexUnlock(tCenter.hMutex);

	if ( !procSubmitAccess(&tCenter, "node:beta", "ui-click") ) {
		goto cleanup;
	}
	if ( !procSubmitAccess(&tCenter, "node:gamma", "api-peek") ) {
		goto cleanup;
	}
	if ( !procSubmitAccess(&tCenter, "node:beta", "ui-click") ) {
		goto cleanup;
	}

	xrtSleep(350u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);

	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);

	printf("== warm ==\n");
	xrtListWalk(tCenter.pWarmLog, procDumpWarm, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
		sSnapshot = NULL;
	}

	iRet = 0;

cleanup:
	procAutoWarmCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 6 个点

### 6.1 自动回温策略状态必须留在 cold entry 上

这页里冷层记录除了：

- `tCooledAt`

还多了：

- `iWarmHits`
- `tFirstWarmHitAt`
- `tLastWarmHitAt`
- `sLastHitSource`

这意味着“距自动回温还差几次”已经是正式状态，而不是 worker 的临时变量。


### 6.2 请求线程只提交 access signal，不直接做回温判定

这页最关键的边界之一就是：

- 请求线程只提交 cold hit
- worker 真正执行命中统计、窗口重置、阈值判断和自动回温

这样后面即使策略继续长大，请求线程也不会马上退化。


### 6.3 自动回温不是“有命中就 promotion”

这页故意把策略写成：

- `2` 次命中
- `400ms` 窗口

也就是：

- 一次偶发命中不会回温
- 同一窗口内连续命中才会 promotion

这正是自动回温和显式 warm job 的根本区别。


### 6.4 时间窗口过期后要正式重置，不要继续累加旧命中

`procAutoWarmWorker()` 里最重要的策略边界是：

- 如果当前时间已经超过 `tFirstWarmHitAt + warm_window`
- 就把本轮命中重置为 `1`

否则旧命中会无限累加，最后任何冷对象都迟早被错误回温。


### 6.5 回温事件仍然要留下正式历史线

就算是策略自动触发，页面和快照仍然需要知道：

- 这次是因为什么回温的
- 回温前累计了几次命中
- 这轮策略窗口从什么时候开始

所以 `pWarmLog` 仍然是正式事件线，不是可有可无的附带记录。


### 6.6 `path + file` 让策略状态真正进入系统状态

`procWriteSnapshotLocked()` 会把：

- 当前热层
- 当前冷层策略状态
- 最近自动回温历史

统一导成 `runtime/auto-warm-policy.json`。  
这意味着自动回温不再只是内存里的策略副作用，而是一份可以被脚本和面板消费的正式状态。


## 7. 接回 HTTP、模板和脚本时怎么落

### 7.1 `POST /api/access`

只做：

1. 解析对象 key
2. 判断当前是不是 cold hit
3. 生成 access signal
4. 推入后台队列

不要在 handler 里直接做自动回温判定和热层恢复。


### 7.2 `GET /api/dashboard`

最自然的 JSON 输出就是 3 块：

1. 当前热层摘要
2. 当前冷层与策略状态
3. 最近自动回温历史


### 7.3 `GET /dashboard`

页面最自然的布局就是：

1. 热层表
2. 冷层窗口和当前累计命中
3. 最近自动回温时间线

这样模板页不需要再从日志里反推“这次回温是不是策略自动触发的”。


### 7.4 `runtime/auto-warm-policy.json`

运维脚本、CLI 工具和页面导出都应该优先消费这份快照，而不是直接读 worker 临时状态。


## 8. 这页最容易写错的地方

### 8.1 命中一次就直接自动回温

这会让冷层几乎失去存在意义，任何偶发访问都会把对象重新拉热。


### 8.2 把累计命中放在请求线程临时变量里

这样页面和快照最后都不知道：

- 当前到底已经累计了几次
- 这一轮窗口是从什么时候开始的


### 8.3 窗口过期后不重置命中

如果旧命中一直累加，策略就不再是“连续热度”，而变成“总会迟早回温”，这会直接把策略写错。


## 9. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 多层存储
	把热层、冷层和长期层拆成更完整的 3 层
2. 冷层回温 + 归档协同
	把“命中不足继续留 cold”“长期无命中继续 archive”补成完整策略页，可以继续读 [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)
3. 自动回温 + 滚动归档联动
	把策略命中和 cold overflow 放进同一条长期运行主线

如果你还没完全建立这里的层级拆分心智，建议一起复读：

- [把本地控制台服务升级成一个冷层回温面板](warm-back-dashboard.md)
- [把本地控制台服务升级成一个冷热滚动归档面板](rolling-tier-archive-dashboard.md)
- [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
