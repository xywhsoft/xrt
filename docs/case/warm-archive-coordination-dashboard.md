# 把本地控制台服务升级成一个冷层回温归档协同面板

> 这页要解决的不是“自动回温”和“滚动归档”各自怎么写，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、cold tier、自动回温和 archive 语义之后，又开始需要把“命中不足继续留 cold”“命中达标自动回温”“长期无命中继续 archive”放进同一条后台主线，还要把当前冷热状态、最近回温历史和最近 archive 历史稳定交给页面、JSON、日志和快照时，怎样把 `dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让 access signal、策略判定和层级迁移各写各的。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 热层和冷层怎么拆
- 冷对象怎样回温回热层
- 长期无命中的冷对象怎样继续 archive

但真实服务里，冷层对象接下来只会走 3 种方向之一：

- 继续留在 cold
- 重新回到 hot
- 正式迁进 archive

所以“自动回温”和“滚动归档”并不是两套彼此独立的能力，而是：

> 同一条 cold state 的后续分流策略。

这页要补出的，就是这条协同主线。


## 2. 为什么这次不能再各写各的

如果访问命中和 archive sweep 继续分散在不同逻辑里，很快就会出现几个典型问题：

- 冷层对象到底处于“等待回温”还是“等待归档”没有正式边界
- 同一对象的命中次数、窗口起点和归档资格分散在多处临时变量里
- 页面为了展示“为什么这个对象没回温却 archive 了”开始扫日志和 worker 临时状态
- 后面再补多层存储时，整个状态模型会立刻失真

所以这页最重要的边界只有一句话：

> access signal 和 archive sweep 都必须进入同一个 worker，由它统一决定 cold entry 下一步该去哪。


## 3. 这一页里每层负责什么

| 层 | 角色 | 解决的问题 |
|----|------|------------|
| `dict` | 当前热层对象 | 当前热点查询与回温后的实时命中 |
| `list` | 当前冷层窗口与命中状态 | 冷对象、累计命中数、窗口起点、最近命中来源 |
| `list` | 最近回温历史 | 页面时间线和回温事件导出 |
| `list` | 最近 archive 历史 | 页面时间线和归档事件导出 |
| `queue + thread` | 后台执行 access signal 与 archive sweep | 请求线程不阻塞在协同策略收口上 |
| `time` | 冷却时间、命中窗口、回温时间、归档时间 | 协同策略的正式时间边界 |
| `path + file` | 快照路径与落盘 | 让协同状态进入文件系统 |
| `xvalue + json` | 导出冷热、回温、archive 摘要 | 页面、脚本、快照共享同一份模型 |


## 4. 一个最小但完整的协同骨架

这段代码故意只保留 6 件事：

1. `dict` 保存当前热层对象
2. `list` 保存当前冷层窗口与命中状态
3. `list` 保存最近回温历史
4. `list` 保存最近 archive 历史
5. `queue + thread` 同时处理 access signal 和 archive sweep
6. `path + file + json` 把当前协同状态落成 `runtime/warm-archive-coordination.json`

这个骨架会实际做这些事：

- 初始化 coordination center
- 注册 3 个热层对象
- 先把 `beta` 和 `gamma` 手工转入冷层
- 对 `beta` 连续提交 2 次 access signal，让它自动回温
- 稍后再提交一次 archive sweep，让长期无命中的 `gamma` 进入 archive
- 主线程打印热层、冷层、回温历史、archive 历史和最终快照

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_JOB_ACCESS 1
#define DEMO_JOB_SWEEP 2

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
	xtime tArchivedAt;
	uint64 iKeyHash;
	int iHeat;
	int iWarmHits;
	char sKey[64];
	char sTitle[64];
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
	const char* sKey;
	bool bFound;
	int64 iSeq;
} DemoColdLookup;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xlist pWarmLog;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextWarmSeq;
	int64 iNextArchiveSeq;
	uint32 iWarmWindowMs;
	uint32 iWarmMinHits;
	uint32 iArchiveAfterMs;
	char sSnapshotPath[260];
} DemoCoordCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}
	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
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

static bool procCollectHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	xvalue vRow;
	(void)pKey;

	if ( (vArr == NULL) || (pItem == NULL) ) {
		return FALSE;
	}
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectCold(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArr == NULL) || (pEntry == NULL) ) {
		return FALSE;
	}
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pEntry->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "last_hit_source", 0u, pEntry->sLastHitSource, 0u, FALSE);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEntry->iWarmHits);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "first_warm_hit_at", 0u, pEntry->tFirstWarmHitAt);
	xvoTableSetInt(vRow, "last_warm_hit_at", 0u, pEntry->tLastWarmHitAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectEvent(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoStateEvent* pEvent = (DemoStateEvent*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArr == NULL) || (pEvent == NULL) ) {
		return FALSE;
	}
	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, pEvent->sReason, 0u, FALSE);
	xvoTableSetInt(vRow, "warm_hits", 0u, pEvent->iWarmHits);
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEvent->tArchivedAt);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoCoordCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHot = NULL;
	xvalue vCold = NULL;
	xvalue vWarm = NULL;
	xvalue vArchive = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0u;
	bool bOk = FALSE;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	vRoot = xvoCreateTable();
	vHot = xvoCreateArray();
	vCold = xvoCreateArray();
	vWarm = xvoCreateArray();
	vArchive = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHot, vHot);
	xrtListWalk(pCenter->pCold, procCollectCold, vCold);
	xrtListWalk(pCenter->pWarmLog, procCollectEvent, vWarm);
	xrtListWalk(pCenter->pArchive, procCollectEvent, vArchive);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "warm_count", 0u, xrtListCount(pCenter->pWarmLog));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "warm_window_ms", 0u, pCenter->iWarmWindowMs);
	xvoTableSetInt(vRoot, "warm_min_hits", 0u, pCenter->iWarmMinHits);
	xvoTableSetInt(vRoot, "archive_after_ms", 0u, pCenter->iArchiveAfterMs);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHot, TRUE);
	xvoUnref(vHot);
	vHot = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vCold, TRUE);
	xvoUnref(vCold);
	vCold = NULL;
	xvoTableSetValue(vRoot, "warm_events", 0u, vWarm, TRUE);
	xvoUnref(vWarm);
	vWarm = NULL;
	xvoTableSetValue(vRoot, "archive_events", 0u, vArchive, TRUE);
	xvoUnref(vArchive);
	vArchive = NULL;

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
	if ( sDir != NULL ) xrtFree(sDir);
	if ( sJson != NULL ) xrtFree(sJson);
	if ( vArchive != NULL ) xvoUnref(vArchive);
	if ( vWarm != NULL ) xvoUnref(vWarm);
	if ( vCold != NULL ) xvoUnref(vCold);
	if ( vHot != NULL ) xvoUnref(vHot);
	if ( vRoot != NULL ) xvoUnref(vRoot);
	return bOk;
}

static bool procAppendColdLocked(DemoCoordCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
{
	DemoColdEntry* pEntry = NULL;
	bool bNew = FALSE;

	pCenter->iNextColdSeq++;
	pEntry = (DemoColdEntry*)xrtListSet(pCenter->pCold, pCenter->iNextColdSeq, &bNew);
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
	return TRUE;
}

static void procAppendWarmLocked(DemoCoordCenter* pCenter, DemoColdEntry* pCold, const char* sReason)
{
	DemoStateEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextWarmSeq++;
	pEvent = (DemoStateEvent*)xrtListSet(pCenter->pWarmLog, pCenter->iNextWarmSeq, &bNew);
	if ( pEvent == NULL ) {
		return;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->iWarmHits = pCold->iWarmHits;
	pEvent->tWarmedAt = xrtNow();
}

static void procAppendArchiveLocked(DemoCoordCenter* pCenter, DemoColdEntry* pCold, const char* sReason)
{
	DemoStateEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextArchiveSeq++;
	pEvent = (DemoStateEvent*)xrtListSet(pCenter->pArchive, pCenter->iNextArchiveSeq, &bNew);
	if ( pEvent == NULL ) {
		return;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pCold->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pCold->sTitle);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pCold->iKeyHash;
	pEvent->iHeat = pCold->iHeat;
	pEvent->iWarmHits = pCold->iWarmHits;
	pEvent->tArchivedAt = xrtNow();
}

static bool procCoordCenterInit(DemoCoordCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iWarmWindowMs = 400u;
	pCenter->iWarmMinHits = 2u;
	pCenter->iArchiveAfterMs = 180u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pCold = xrtListCreate(sizeof(DemoColdEntry), XRT_OBJMODE_SHARED);
	pCenter->pWarmLog = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->pArchive = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->hMutex = xrtMutexCreate();
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	sSnapshotPath = xrtPathJoin(2, "runtime", "warm-archive-coordination.json");

	if ( (pCenter->pHot == NULL) || (pCenter->pCold == NULL) || (pCenter->pWarmLog == NULL) || (pCenter->pArchive == NULL) || (pCenter->hMutex == NULL) || (pCenter->hQueue == NULL) || (sSnapshotPath == NULL) ) {
		if ( sSnapshotPath != NULL ) xrtFree(sSnapshotPath);
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoCoordCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));

	if ( (pCenter == NULL) || (sKey == NULL) || (pItem == NULL) ) {
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

static bool procMoveHotToColdLocked(DemoCoordCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));

	if ( pItem == NULL ) {
		return FALSE;
	}
	(void)procAppendColdLocked(pCenter, pItem, sTierReason);
	xrtFree(pItem);
	return TRUE;
}

static bool procSubmitJob(DemoCoordCenter* pCenter, int iKind, const char* sKey, const char* sText)
{
	DemoJob* pJob = (DemoJob*)xrtMalloc(sizeof(DemoJob));

	if ( (pCenter == NULL) || (pJob == NULL) ) {
		return FALSE;
	}
	memset(pJob, 0, sizeof(*pJob));
	pJob->iKind = iKind;
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sText, sizeof(pJob->sText), sText);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}
	return TRUE;
}

static uint32 procCoordWorker(ptr pArg)
{
	DemoCoordCenter* pCenter = (DemoCoordCenter*)pArg;
	DemoJob* pJob = NULL;
	DemoColdLookup tLookup;
	DemoColdEntry* pCold = NULL;
	DemoHotItem* pHot = NULL;
	xqueue_result iRet;
	xtime tNow;
	int64 iSeq;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) break;
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) continue;

		xrtSleep(40u);
		xrtMutexLock(pCenter->hMutex);
		tNow = xrtNow();

		if ( pJob->iKind == DEMO_JOB_ACCESS ) {
			memset(&tLookup, 0, sizeof(tLookup));
			tLookup.sKey = pJob->sKey;
			xrtListWalk(pCenter->pCold, procFindColdWalk, &tLookup);
			if ( tLookup.bFound ) {
				pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, tLookup.iSeq);
				if ( pCold != NULL ) {
					if ( (pCold->tFirstWarmHitAt == 0) || ((tNow - pCold->tFirstWarmHitAt) > (xtime)pCenter->iWarmWindowMs) ) {
						pCold->iWarmHits = 1;
						pCold->tFirstWarmHitAt = tNow;
					} else {
						pCold->iWarmHits++;
					}
					pCold->tLastWarmHitAt = tNow;
					procCopyText(pCold->sLastHitSource, sizeof(pCold->sLastHitSource), pJob->sText);

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
								procAppendWarmLocked(pCenter, pCold, "policy-hit-threshold");
								(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
								pHot = NULL;
							}
						}
					}
				}
			}
		} else if ( pJob->iKind == DEMO_JOB_SWEEP ) {
			for ( iSeq = 1; iSeq <= pCenter->iNextColdSeq; iSeq++ ) {
				pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, iSeq);
				if ( pCold == NULL ) continue;
				if ( (tNow - pCold->tCooledAt) < (xtime)pCenter->iArchiveAfterMs ) continue;
				if ( (pCold->tLastWarmHitAt != 0) && ((tNow - pCold->tLastWarmHitAt) <= (xtime)pCenter->iWarmWindowMs) ) continue;
				if ( pCold->iWarmHits >= (int)pCenter->iWarmMinHits ) continue;
				procAppendArchiveLocked(pCenter, pCold, pJob->sText);
				(void)xrtListRemove(pCenter->pCold, iSeq);
			}
		}

		(void)procWriteSnapshotLocked(pCenter);
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
		printf("hot key=%s title=%s heat=%d hash=%llu\n", pItem->sKey, pItem->sTitle, pItem->iHeat, (unsigned long long)pItem->iKeyHash);
	}
	return FALSE;
}

static bool procDumpCold(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdEntry* pEntry = (DemoColdEntry*)pVal;
	(void)pArg;
	printf("cold seq=%lld key=%s hits=%d source=%s cooled_at=%lld\n", (long long)iKey, pEntry->sKey, pEntry->iWarmHits, pEntry->sLastHitSource, (long long)pEntry->tCooledAt);
	return FALSE;
}

static bool procDumpEvent(int64 iKey, ptr pVal, ptr pArg)
{
	DemoStateEvent* pEvent = (DemoStateEvent*)pVal;
	const char* sTag = (const char*)pArg;
	printf("%s seq=%lld key=%s reason=%s hits=%d\n", sTag, (long long)iKey, pEvent->sKey, pEvent->sReason, pEvent->iWarmHits);
	return FALSE;
}

static bool procFreeHot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoHotItem* pItem = *(DemoHotItem**)pVal;
	(void)pKey;
	(void)pArg;
	if ( pItem != NULL ) xrtFree(pItem);
	return FALSE;
}

static void procCoordCenterUnit(DemoCoordCenter* pCenter)
{
	if ( pCenter == NULL ) return;
	if ( pCenter->hQueue != NULL ) xrtMPSCQWaitClose(pCenter->hQueue);
	if ( pCenter->hWorker != NULL ) {
		xrtThreadWait(pCenter->hWorker);
		xrtThreadDestroy(pCenter->hWorker);
	}
	if ( pCenter->hQueue != NULL ) xrtMPSCQWaitDestroy(pCenter->hQueue);
	if ( (pCenter->hMutex != NULL) && (pCenter->pHot != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pHot, procFreeHot, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}
	if ( pCenter->hMutex != NULL ) xrtMutexDestroy(pCenter->hMutex);
	if ( pCenter->pArchive != NULL ) xrtListDestroy(pCenter->pArchive);
	if ( pCenter->pWarmLog != NULL ) xrtListDestroy(pCenter->pWarmLog);
	if ( pCenter->pCold != NULL ) xrtListDestroy(pCenter->pCold);
	if ( pCenter->pHot != NULL ) xrtDictDestroy(pCenter->pHot);
}

int main(void)
{
	DemoCoordCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) return 1;
	if ( !procCoordCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procCoordWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procCoordCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 4) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 2) ) goto cleanup;

	xrtMutexLock(tCenter.hMutex);
	(void)procMoveHotToColdLocked(&tCenter, "node:beta", "idle-window");
	(void)procMoveHotToColdLocked(&tCenter, "node:gamma", "cooldown-batch");
	(void)procWriteSnapshotLocked(&tCenter);
	xrtMutexUnlock(tCenter.hMutex);

	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "ui-click") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "api-refresh") ) goto cleanup;
	xrtSleep(220u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "cold-idle-archive") ) goto cleanup;

	xrtSleep(350u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);
	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);
	printf("== warm ==\n");
	xrtListWalk(tCenter.pWarmLog, procDumpEvent, "warm");
	printf("== archive ==\n");
	xrtListWalk(tCenter.pArchive, procDumpEvent, "archive");
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
	}

	iRet = 0;

cleanup:
	procCoordCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 5 个点

### 6.1 cold entry 现在有 3 条正式去向

同一条 cold state 现在可能：

- 继续留在 cold
- 被自动回温
- 被 archive sweep 继续迁进 archive

这就是为什么它必须成为正式策略分流点。


### 6.2 access signal 和 archive sweep 必须进入同一个 worker

这页最重要的边界就是：

- 请求线程只投递 `ACCESS` 和 `SWEEP`
- worker 统一判断 cold entry 下一步该去哪

这样回温和归档不会互相打架。


### 6.3 archive sweep 不能只看年龄

这页里的 archive 条件明确要求：

- 已经超过 archive 老化阈值
- 最近没有仍在窗口内的 warm hit
- 当前累计命中还没达到回温阈值

也就是说，archive 不是简单比年龄，warm-back 也不是简单比 hits。


### 6.4 “近期命中不足”时，正确行为是继续留 cold

这页故意保留一个教学重点：

- `beta` 连续 2 次命中，自动回温
- `gamma` 长期无命中，被 archive

而那些“刚命中过 1 次但还没达标”的对象，应该继续留在 cold。


### 6.5 `path + file` 让协同策略成为正式系统状态

`procWriteSnapshotLocked()` 会把：

- 当前热层
- 当前冷层策略状态
- 最近回温历史
- 最近 archive 历史

统一导成 `runtime/warm-archive-coordination.json`。  
这意味着协同策略不再只是 worker 的内部逻辑，而是一份可以被脚本和面板消费的正式状态。


## 7. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 多层存储
	把热层、冷层和长期层拆成更完整的多层结构
2. 自动回温 + 滚动归档联动
	把 sweep、回温和长期层继续做成更完整的层级策略
3. 冷热多级老化
	把“热 -> 温 -> 冷 -> archive”拆成更细的 4 层模型

如果你还没完全建立这里的层级拆分心智，建议一起复读：

- [把本地控制台服务升级成一个自动回温策略面板](auto-warm-policy-dashboard.md)
- [把本地控制台服务升级成一个冷热滚动归档面板](rolling-tier-archive-dashboard.md)
- [把本地控制台服务升级成一个冷层回温面板](warm-back-dashboard.md)
