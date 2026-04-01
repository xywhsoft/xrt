# 把本地控制台服务升级成一个冷层回温面板

> 这个案例要解决的不是“冷层对象被访问了，就直接塞回热层”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot/cold 两层状态，又开始需要把冷层对象在合适时机回温到热层，还要把最近回温事件稳定展示给页面、JSON、日志和快照时，怎样把 `dict + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让访问命中、回温判定、热层恢复和页面展示各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面的冷热分层面板：

- 当前热点对象在热层
- 已降温对象在冷层
- 冷层快照会稳定落盘
- 页面和 JSON 都能看到冷热两层

现在系统又往前走一步，出现了新的业务动作：

- 某个冷层对象又被访问到了
- 它不该继续一直躺在冷层
- 但也不应该由请求线程直接同步恢复
- 页面还要看到“最近哪些对象被回温了”
- 最新回温结果还要进入快照文件

如果这时只写成：

- 命中冷层后直接改几行字段
- 顺手塞回热层

很快就会出现 4 个问题：

- 请求线程被回温收口和快照写盘拖住
- 热层恢复和冷层移除没有正式事件记录
- 页面为了显示“最近回温”开始扫日志或看 worker 临时状态
- 冷层回温和冷热分层边界混在一起，后面谁也不敢改

所以这页真正要补出的，不是“怎么把冷对象重新插回 dict”，而是：

> 当系统已经有了 cold tier，怎样把 warm-back promotion 变成一条正式后台主线，并给它一份稳定可导出的状态模型。


## 2. 为什么这次不是“命中冷层后直接同步恢复”

### 2.1 回温是状态迁移，不是一次普通读取

一旦对象从冷层恢复到热层，真正发生的是：

- 它的层级变了
- 它的时间语义变了
- 页面和快照要记录一次正式事件

所以这已经不是：

- 单纯读取一个冷对象

而是：

- 一次正式的层级迁移


### 2.2 冷层命中并不意味着请求线程该执行全部恢复逻辑

请求线程更适合做：

- 判断这个 key 是否该回温
- 生成一个回温请求
- 立即返回已接收或返回当前冷态

它不适合直接承担：

- 冷层查找后的删除
- 热层对象重建
- 回温事件记录
- 快照落盘

这些动作一旦变重，请求线程马上退化。


### 2.3 回温历史应该成为正式值记录

页面和快照真正需要的不是：

- “这个对象当前又在热层里了”

还需要：

- 它是什么时候回温的
- 为什么回温
- 回温前它在冷层是什么状态

所以这页里，除了热层和冷层，还需要一条正式的：

- 回温历史线


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存当前热层对象 | 当前热点查询与恢复后的实时命中 |
| `list` | 保存当前冷层窗口 | 当前冷对象窗口与冷层导出 |
| `list` | 保存最近回温历史 | 页面时间线和回温事件导出 |
| `queue + thread` | 后台执行回温 | 请求线程不阻塞在 promotion 上 |
| `time` | 记录冷却时间与回温时间 | 回温事件的正式时间边界 |
| `hash` | 给对象 key 一个稳定指纹 | 页面、日志和快照里的轻量标识 |
| `path + file` | 组织回温快照路径并落盘 | 让回温结果进入真实文件系统 |
| `xvalue + json` | 导出当前冷热与回温摘要 | 页面、脚本、快照共享同一份模型 |
| `xhttpd + template` | 展示热层、冷层和回温历史 | 浏览器和脚本共享一个入口 |

一句话记住：

> `dict` 管“现在重新变热了什么”，一条 `list` 管“现在冷层里还有什么”，另一条 `list` 管“最近回温了什么”，worker 管“什么时候把冷层对象正式 promotion 回热层”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成回温面板语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/warm-back.json
```

其中：

- `config/console.json`
	- 保存冷层窗口大小、回温队列容量、是否允许自动回温等配置
- `web/dashboard.html`
	- 同时展示当前热层、当前冷层和最近回温历史
- `runtime/console.log`
	- 记录回温提交和回温完成
- `runtime/warm-back.json`
	- 保存最近一次冷热与回温摘要


## 5. 先把热层、冷层和回温通道立起来

下面这段代码故意先聚焦 warm-back promotion 核心，不把完整 `xhttpd` handler 和模板渲染全部塞进去。

它保留最关键的 5 件事：

1. `dict` 保存当前热层对象
2. `list` 保存当前冷层窗口
3. `list` 保存最近回温历史
4. `queue + thread` 把回温动作放到后台
5. `path + file + json` 把当前冷热与回温摘要落成 `runtime/warm-back.json`

这个骨架会实际做这些事：

- 初始化 warm-back center
- 注册几个热层对象
- 先把两个对象手工转入冷层，形成 cold window
- 提交一次回温请求
- worker 把对象从冷层 promotion 回热层
- 主线程打印热层、冷层、回温历史和最终快照

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
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
} DemoColdEntry;

typedef struct
{
	xtime tWarmedAt;
	xtime tPrevCooledAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sReason[48];
} DemoWarmEvent;

typedef struct
{
	char sKey[64];
	char sReason[48];
} DemoWarmJob;

typedef struct
{
	const char* sKey;
	bool bFound;
	int64 iSeq;
	DemoColdEntry tEntry;
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
	char sSnapshotPath[260];
} DemoWarmCenter;


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
	xvoTableSetInt(vRow, "heat", 0u, pEntry->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pEntry->tTouchedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
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
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	xvoTableSetInt(vRow, "prev_cooled_at", 0u, pEvent->tPrevCooledAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEvent->iKeyHash);
	(void)xvoArrayAppendValue(vWarmArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoWarmCenter* pCenter)
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

static bool procAppendColdLocked(DemoWarmCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
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

static bool procAppendWarmEventLocked(DemoWarmCenter* pCenter, const DemoColdEntry* pCold, const char* sReason)
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
	pEvent->tPrevCooledAt = pCold->tCooledAt;
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
		memcpy(&pLookup->tEntry, pEntry, sizeof(pLookup->tEntry));
		return TRUE;
	}

	return FALSE;
}

static bool procWarmCenterInit(DemoWarmCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 8u;
	pCenter->iKeepWarm = 8u;

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

	sSnapshotPath = xrtPathJoin(2, "runtime", "warm-back.json");
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

static bool procRegisterHot(DemoWarmCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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

static bool procMoveHotToColdLocked(DemoWarmCenter* pCenter, const char* sKey, const char* sTierReason)
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

static bool procSubmitWarm(DemoWarmCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoWarmJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoWarmJob*)xrtMalloc(sizeof(DemoWarmJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sReason, sizeof(pJob->sReason), sReason);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procWarmWorker(ptr pArg)
{
	DemoWarmCenter* pCenter = (DemoWarmCenter*)pArg;
	DemoWarmJob* pJob = NULL;
	DemoHotItem* pHot = NULL;
	DemoColdLookup tLookup;
	xqueue_result iRet;

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
			pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
			if ( pHot != NULL ) {
				memset(pHot, 0, sizeof(*pHot));
				pCenter->iNextHotId++;
				pHot->iHotId = pCenter->iNextHotId;
				pHot->tTouchedAt = xrtNow();
				pHot->iKeyHash = tLookup.tEntry.iKeyHash;
				pHot->iHeat = tLookup.tEntry.iHeat + 3;
				procCopyText(pHot->sKey, sizeof(pHot->sKey), tLookup.tEntry.sKey);
				procCopyText(pHot->sTitle, sizeof(pHot->sTitle), tLookup.tEntry.sTitle);

				if ( xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
					(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
					(void)procAppendWarmEventLocked(pCenter, &tLookup.tEntry, pJob->sReason);
					(void)procWriteSnapshotLocked(pCenter);
					pHot = NULL;
				}
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
		"cold seq=%lld key=%s title=%s heat=%d reason=%s cooled_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->iHeat,
		pEntry->sTierReason,
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
		"warm seq=%lld key=%s title=%s reason=%s warmed_at=%lld prev_cooled_at=%lld hash=%llu\n",
		(long long)iKey,
		pEvent->sKey,
		pEvent->sTitle,
		pEvent->sReason,
		(long long)pEvent->tWarmedAt,
		(long long)pEvent->tPrevCooledAt,
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

static void procWarmCenterUnit(DemoWarmCenter* pCenter)
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
	DemoWarmCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procWarmCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procWarmWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procWarmCenterUnit(&tCenter);
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

	if ( !procSubmitWarm(&tCenter, "node:beta", "user-hit") ) {
		goto cleanup;
	}

	xrtSleep(300u);

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
	procWarmCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 6 个点

### 6.1 回温历史必须成为正式事件线

这页里除了：

- `pHot`
- `pCold`

还多了一条：

- `pWarmLog`

这意味着“对象已经回温”不再只是热层里少一个冷对象、多一个热对象，而是：

- 一次正式 promotion 事件


### 6.2 请求线程只做判定和投递，不做 promotion 收口

这页最关键的边界之一就是：

- 请求线程只提交 warm job
- worker 真正执行冷层查找、热层恢复、历史记录和快照写盘

这样后面即使回温逻辑继续长大，请求线程也不会马上退化。


### 6.3 当前冷层窗口用 `list`，所以 lookup 是显式设计点

这页故意保留了一个教学重点：

- 当前冷层窗口依然是 `list`
- 所以 `procFindColdWalk()` 需要线性扫描

这不是 bug，而是边界说明：

- 如果你的 cold window 很小，这样足够清楚
- 如果冷层已经大到需要频繁按 key 查找，就该升级成 `dict` 或 `avltree`


### 6.4 回温不是重新造一份无历史对象

`procWarmWorker()` 不是随便 new 一个热对象，而是：

- 从冷层快照里恢复 key/title/hash/heat
- 记录上一次 `cooled_at`
- 再写入一条正式 warm event

这样页面和快照才能说清楚：

- 它从哪一条冷记录恢复回来的


### 6.5 `path + file` 让回温结果真正进入系统状态

`procWriteSnapshotLocked()` 会把：

- 当前热层
- 当前冷层
- 最近回温历史

统一导成 `runtime/warm-back.json`。  
这意味着 warm-back 不再只是内存里的瞬时动作，而是一份可以被脚本和面板消费的正式状态。


### 6.6 这页故意先不补“自动回温判定策略”

这页先讲的是：

- 一次明确的 promotion 通道

而不是：

- 命中多少次自动回温
- 多久访问一次自动回温

因为如果先不把 promotion 边界讲稳，自动策略只会把状态再写乱。


## 7. 接回 HTTP、模板和脚本时怎么落

### 7.1 `POST /api/warm`

只做：

1. 解析对象 key
2. 给出回温原因
3. 推入后台队列
4. 立即返回已接收

不要在 handler 里直接把对象从冷层塞回热层。


### 7.2 `GET /api/dashboard`

最自然的 JSON 输出就是 3 块：

1. 当前热层摘要
2. 当前冷层摘要
3. 最近回温历史


### 7.3 `GET /dashboard`

页面最自然的布局就是：

1. 热层表
2. 冷层窗口
3. 最近回温时间线

这样模板页不需要再从日志里反推回温事件。


### 7.4 `runtime/warm-back.json`

运维脚本、CLI 工具和页面导出都应该优先消费这份快照，而不是直接读 worker 临时状态。


## 8. 这页最容易写错的地方

### 8.1 命中冷层后直接同步 promotion

这会让请求线程重新吞下所有层级迁移和快照写盘成本。


### 8.2 只改冷热两层，不记录回温历史

这样页面、快照和日志最后都会不知道：

- 这个对象是什么时候被重新拉热的
- 为什么被拉热


### 8.3 在大冷层里继续无脑线性扫描

这页故意保留线性扫描，是为了教学边界。  
如果冷层窗口已经很大，就应该升级 cold lookup 结构，而不是假装 `list` 还能一直撑住。


## 9. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 冷层滚动归档
	让回温失败或长期未回温的数据继续转进长期归档，可以继续读 [把本地控制台服务升级成一个冷热滚动归档面板](rolling-tier-archive-dashboard.md)
2. 自动回温策略
	把“命中阈值”“时间阈值”补成正式策略，可以继续读 [把本地控制台服务升级成一个自动回温策略面板](auto-warm-policy-dashboard.md)
3. 多层存储
	把热层、冷层、长期层拆成更完整的 3 层

如果你还没完全建立这里的层级拆分心智，建议一起复读：

- [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
- [把本地控制台服务升级成一个缓存刷新面板](cache-refresh-dashboard.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
