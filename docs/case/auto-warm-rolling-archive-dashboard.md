# 把本地控制台服务升级成一个自动回温 + 滚动归档联动面板

> 这页要解决的不是“自动回温”和“滚动归档”各写一半再拼起来，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、cold tier、自动回温策略和 archive 语义之后，又开始需要把“冷却进入 cold”“命中达标自动回温”“cold window 超限继续滚动归档”“长期无命中再做 stale sweep”放进同一条后台主线，还要把当前冷热状态、最近回温历史和最近 archive 历史稳定交给页面、JSON、日志和快照时，怎样把 `dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让 cooldown、access signal、overflow compaction 和页面展示各写各的。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 冷层对象怎样按命中阈值自动回温
- cold window 怎样继续滚进 archive 层
- 冷对象怎样在同一条 worker 里统一收口

但真实服务再往前走一步，很快就会出现一个新的联动要求：

- 新一轮 cooldown 持续把对象送进 cold window
- 某些 cold entry 又在短窗口里连续被访问，应该自动回温
- 某些 cold entry 没有重新变热，却又开始挤占当前 cold window
- 页面和 JSON 还要同时回答“谁刚刚回温了”和“谁刚刚被滚进 archive 了”
- 最新冷热与联动策略状态还要继续落成快照文件

如果这时还把逻辑拆成：

- 一个地方只管 access hit
- 一个地方只管冷层裁剪
- 页面自己去拼最近回温和最近归档

很快就会出现几个典型问题：

- cold entry 当前命中次数、最近命中来源和归档资格没有统一状态源
- keep-cold 上限和自动回温窗口开始互相打架
- 页面为了显示“这个对象为什么没回温却 archive 了”开始扫日志或 worker 临时变量
- 后面继续往多层存储或多级老化扩展时，整个状态模型会立刻失真

所以这页真正要补出的，不是“把两篇案例并排放一起”，而是：

> 当同一条 cold state 既可能因为命中重新变热，又可能因为窗口压力继续滚进 archive 时，怎样把它们收口到同一条长期运行主线里。


## 2. 为什么这次不能把两条主线继续拆开

### 2.1 自动回温回答的是“谁重新变热了”

自动回温真正解决的是：

- 某个 cold entry 在正式时间窗口里重新出现持续热度

它回答的不是：

- 这个对象有没有被访问过一次

所以它一定要盯住：

- `warm_hits`
- `first_warm_hit_at`
- `last_warm_hit_at`


### 2.2 滚动归档回答的是“谁不该继续占着 cold window”

滚动归档真正解决的是：

- 当前 cold window 已经需要给更新的冷对象让位
- 或者某个冷对象已经长期没有重新变热，应该继续进入 archive 历史

它回答的也不是：

- 这个对象现在是不是还存在

它要回答的是：

- 它什么时候离开当前 cold window
- 是因为 overflow 还是因为 stale sweep


### 2.3 联动点其实就是 cold entry 本身

到了这一层，真正的分流点不再是：

- 某个孤立回调
- 某个页面局部逻辑

而是：

- 同一条 cold entry

因为同一条 cold entry 现在同时需要携带：

- 冷却进入 cold 的时间
- 当前 warm hits
- 最近命中来源
- 是否还在 warm window
- 是否已经可以被滚进 archive


### 2.4 请求线程只该提交 signal，不该直接做收口

请求线程更适合做：

- 提交 `ACCESS`
- 提交 `COOL`
- 触发一个周期性 `SWEEP`

它不适合直接承担：

- 热层移除和冷层插入
- warm threshold 判定
- cold overflow compaction
- archive history 记录
- 快照落盘

所以这页真正的核心边界只有一句话：

> `COOL`、`ACCESS` 和 `SWEEP` 都必须进入同一个 worker，由它决定 cold entry 下一步是留 cold、回 hot，还是滚进 archive。


## 3. 这条联动主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前热层对象 | 当前热点查询与回温后的实时命中 |
| `list` | 当前 cold window 与命中状态 | 冷对象、当前 warm hits、最近命中来源、冷却时间 |
| `list` | 最近自动回温历史 | 页面时间线和回温事件导出 |
| `list` | 最近 archive 历史 | 页面时间线和滚动归档事件导出 |
| `queue + thread` | 后台执行 `COOL / ACCESS / SWEEP` | 请求线程不阻塞在层级迁移和策略收口上 |
| `time` | 冷却、命中、回温、归档时间 | 联动策略的正式时间边界 |
| `hash` | 给对象 key 一个稳定指纹 | 页面、日志、快照里的轻量标识 |
| `path + file` | 组织快照路径并落盘 | 让联动状态进入真实文件系统 |
| `xvalue + json` | 导出冷热、回温、archive 摘要 | 页面、脚本、快照共享同一份模型 |
| `xhttpd + template` | 展示热层、cold window、warm 历史和 archive 历史 | 浏览器和脚本共享一个入口 |

一句话记住：

> `dict` 管“现在热着什么”，一条 `list` 管“当前 cold window 里还在等待分流的对象”，另外两条 `list` 分别管“最近回温”和“最近 archive”，worker 管“什么时候由联动策略正式迁移状态”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成联动面板语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/auto-warm-rolling-archive.json
```

其中：

- `config/console.json`
	- 保存 `keep_cold`、`warm_window_ms`、`warm_min_hits`、`archive_after_ms`
- `web/dashboard.html`
	- 同时展示当前热层、当前 cold window、最近 warm history 和最近 archive history
- `runtime/console.log`
	- 记录 cooldown 提交、warm threshold 命中和 archive compaction
- `runtime/auto-warm-rolling-archive.json`
	- 保存最近一次联动状态摘要


## 5. 先把 cooldown、命中和滚动归档这条后台主线立起来

下面这段代码故意只保留 6 件事：

1. `dict` 保存当前热层对象
2. `list` 保存当前 cold window 和 warm hits 状态
3. `list` 保存最近自动回温历史
4. `list` 保存最近 archive 历史
5. `queue + thread` 同时处理 `COOL`、`ACCESS` 和 `SWEEP`
6. `path + file + json` 把当前联动状态落成 `runtime/auto-warm-rolling-archive.json`

这个骨架会实际做这些事：

- 初始化 rolling warm center
- 注册 `alpha / beta / gamma / delta`
- 先把 `beta` 和 `gamma` 手工转入 cold
- 对 `beta` 连续提交 2 次 `ACCESS`，让它自动回温
- 再对 `delta` 和 `alpha` 提交 `COOL`，让 cold window 发生 overflow
- 最后提交一次 `SWEEP`，让长期无命中的 cold entry 继续进入 archive
- 主线程打印热层、cold window、回温历史、archive 历史和最终快照

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_JOB_COOL 1
#define DEMO_JOB_ACCESS 2
#define DEMO_JOB_SWEEP 3

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
	xtime tPrevCooledAt;
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
	uint32 iKeepCold;
	uint32 iKeepWarm;
	uint32 iKeepArchive;
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
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pItem->iKeyHash);
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
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEntry->iKeyHash);
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
	xvoTableSetInt(vRow, "heat", 0u, pEvent->iHeat);
	xvoTableSetInt(vRow, "prev_cooled_at", 0u, pEvent->tPrevCooledAt);
	xvoTableSetInt(vRow, "warmed_at", 0u, pEvent->tWarmedAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEvent->tArchivedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEvent->iKeyHash);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoRollingWarmCenter* pCenter)
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

	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "warm_count", 0u, xrtListCount(pCenter->pWarmLog));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "keep_cold", 0u, pCenter->iKeepCold);
	xvoTableSetInt(vRoot, "warm_window_ms", 0u, pCenter->iWarmWindowMs);
	xvoTableSetInt(vRoot, "warm_min_hits", 0u, pCenter->iWarmMinHits);
	xvoTableSetInt(vRoot, "archive_after_ms", 0u, pCenter->iArchiveAfterMs);
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
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

static bool procAppendColdLocked(DemoRollingWarmCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
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

static void procAppendWarmLocked(DemoRollingWarmCenter* pCenter, DemoColdEntry* pCold, const char* sReason)
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
	pEvent->tPrevCooledAt = pCold->tCooledAt;
	pEvent->tWarmedAt = xrtNow();

	if ( pCenter->iNextWarmSeq > (int64)pCenter->iKeepWarm ) {
		(void)xrtListRemove(pCenter->pWarmLog, pCenter->iNextWarmSeq - (int64)pCenter->iKeepWarm);
	}
}

static void procAppendArchiveLocked(DemoRollingWarmCenter* pCenter, DemoColdEntry* pCold, const char* sReason)
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
	pEvent->tPrevCooledAt = pCold->tCooledAt;
	pEvent->tArchivedAt = xrtNow();

	if ( pCenter->iNextArchiveSeq > (int64)pCenter->iKeepArchive ) {
		(void)xrtListRemove(pCenter->pArchive, pCenter->iNextArchiveSeq - (int64)pCenter->iKeepArchive);
	}
}

static int64 procFindArchiveCandidateSeqLocked(DemoRollingWarmCenter* pCenter, xtime tNow, bool bOverflowOnly)
{
	int64 iSeq;
	DemoColdEntry* pCold;

	for ( iSeq = 1; iSeq <= pCenter->iNextColdSeq; iSeq++ ) {
		pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, iSeq);
		if ( pCold == NULL ) {
			continue;
		}

		if ( (pCold->tLastWarmHitAt != 0) && ((tNow - pCold->tLastWarmHitAt) <= (xtime)pCenter->iWarmWindowMs) ) {
			continue;
		}

		if ( !bOverflowOnly ) {
			if ( (tNow - pCold->tCooledAt) < (xtime)pCenter->iArchiveAfterMs ) {
				continue;
			}
			if ( pCold->iWarmHits >= (int)pCenter->iWarmMinHits ) {
				continue;
			}
		}

		return iSeq;
	}

	return 0;
}

static void procCompactColdLocked(DemoRollingWarmCenter* pCenter, xtime tNow)
{
	int64 iSeq;
	DemoColdEntry* pCold;

	while ( xrtListCount(pCenter->pCold) > pCenter->iKeepCold ) {
		iSeq = procFindArchiveCandidateSeqLocked(pCenter, tNow, TRUE);
		if ( iSeq == 0 ) {
			break;
		}

		pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, iSeq);
		if ( pCold == NULL ) {
			(void)xrtListRemove(pCenter->pCold, iSeq);
			continue;
		}

		procAppendArchiveLocked(pCenter, pCold, "cold-window-overflow");
		(void)xrtListRemove(pCenter->pCold, iSeq);
	}

	for ( ;; ) {
		iSeq = procFindArchiveCandidateSeqLocked(pCenter, tNow, FALSE);
		if ( iSeq == 0 ) {
			break;
		}

		pCold = (DemoColdEntry*)xrtListGet(pCenter->pCold, iSeq);
		if ( pCold == NULL ) {
			(void)xrtListRemove(pCenter->pCold, iSeq);
			continue;
		}

		procAppendArchiveLocked(pCenter, pCold, "stale-cold-sweep");
		(void)xrtListRemove(pCenter->pCold, iSeq);
	}
}

static bool procCenterInit(DemoRollingWarmCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 2u;
	pCenter->iKeepWarm = 6u;
	pCenter->iKeepArchive = 6u;
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
	sSnapshotPath = xrtPathJoin(2, "runtime", "auto-warm-rolling-archive.json");

	if ( (pCenter->pHot == NULL) || (pCenter->pCold == NULL) || (pCenter->pWarmLog == NULL) || (pCenter->pArchive == NULL) || (pCenter->hMutex == NULL) || (pCenter->hQueue == NULL) || (sSnapshotPath == NULL) ) {
		if ( sSnapshotPath != NULL ) xrtFree(sSnapshotPath);
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoRollingWarmCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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

static bool procMoveHotToColdLocked(DemoRollingWarmCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoHotItem* pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));

	if ( pItem == NULL ) {
		return FALSE;
	}

	(void)procAppendColdLocked(pCenter, pItem, sTierReason);
	xrtFree(pItem);
	return TRUE;
}

static bool procSubmitJob(DemoRollingWarmCenter* pCenter, int iKind, const char* sKey, const char* sText)
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

static uint32 procCenterWorker(ptr pArg)
{
	DemoRollingWarmCenter* pCenter = (DemoRollingWarmCenter*)pArg;
	DemoJob* pJob = NULL;
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
		tNow = xrtNow();

		if ( pJob->iKind == DEMO_JOB_COOL ) {
			(void)procMoveHotToColdLocked(pCenter, pJob->sKey, pJob->sText);
			tNow = xrtNow();
			procCompactColdLocked(pCenter, tNow);
		} else if ( pJob->iKind == DEMO_JOB_ACCESS ) {
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
			tNow = xrtNow();
			procCompactColdLocked(pCenter, tNow);
		} else if ( pJob->iKind == DEMO_JOB_SWEEP ) {
			procCompactColdLocked(pCenter, tNow);
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

static void procCenterUnit(DemoRollingWarmCenter* pCenter)
{
	if ( pCenter == NULL ) {
		return;
	}
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
	DemoRollingWarmCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) return 1;
	if ( !procCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procCenterWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 4) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 2) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:delta", "Delta Backfill", 3) ) goto cleanup;

	xrtMutexLock(tCenter.hMutex);
	(void)procMoveHotToColdLocked(&tCenter, "node:beta", "idle-window");
	(void)procMoveHotToColdLocked(&tCenter, "node:gamma", "backlog-window");
	(void)procWriteSnapshotLocked(&tCenter);
	xrtMutexUnlock(tCenter.hMutex);

	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "ui-click") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "api-refresh") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_COOL, "node:delta", "scheduled-cooldown") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_COOL, "node:alpha", "window-pressure") ) goto cleanup;
	xrtSleep(220u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "periodic-idle-sweep") ) goto cleanup;

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
	procCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 6 个点

### 6.1 同一条 cold entry 现在有 3 条正式去向

同一条 cold state 现在可能：

- 继续留在 cold
- 因命中达标自动回温
- 因 overflow 或 stale sweep 继续滚进 archive

这就是为什么它必须成为正式分流点，而不是散落在 3 份临时逻辑里。


### 6.2 `COOL`、`ACCESS` 和 `SWEEP` 必须进入同一个 worker

这页最重要的边界就是：

- 请求线程只投递 signal
- worker 统一判断当前 cold entry 下一步该去哪

这样 warm-back 和 rolling archive 才不会互相打架。


### 6.3 `keep_cold` 不是“见到超限就随便删一条”

这页里的 overflow compaction 明确要求：

- 优先从最旧、且最近不在 warm window 内的 cold entry 里找候选
- 找到候选后，写成正式 archive 事件再移除

所以这不是：

- 直接 `Remove` 一条 list 记录

而是：

- 一次带原因的状态迁移


### 6.4 overflow archive 和 stale sweep 不是同一种原因

这页把 archive reason 明确拆成两类：

- `cold-window-overflow`
- `stale-cold-sweep`

它们都会进入 archive history，但业务语义完全不同。页面和日志不该把这两类事件混在一起。


### 6.5 自动回温一定要和 cold 移除一起收口

当 `warm_hits >= warm_min_hits` 时，worker 做的不是：

- 先把对象插进 hot，冷层之后再说

而是：

- 重建 hot item
- 写 warm event
- 正式把 cold entry 从当前 cold window 移除

这样快照里才不会短暂出现“同一个 key 同时在 hot 和 cold”。


### 6.6 页面、JSON、日志都应该读同一份导出模型

这一页里最稳的做法仍然是：

- worker 每轮收口后写一份统一 snapshot

这样：

- 页面可以直接看当前状态
- 脚本可以直接拿 JSON
- 日志只是帮助解释，不再是页面的唯一数据源


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先改模型，再决定是否复用这套骨架：

- 如果对象处理是一次性 bounded fan-out，而不是长期运行服务，优先考虑 `task group + future`
- 如果 archive 已经变成真正的慢 I/O 链路，应该继续拆出 `subprocess / file_async`
- 如果你要把状态实时推给浏览器，页面层应继续补 `xws + queue + coroutine`


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前热层、cold window、warm history、archive history
2. `POST /api/access`
	- 只提交 `ACCESS` signal，不直接改状态
3. `POST /api/cool`
	- 只提交 `COOL` signal，让 worker 决定何时进入 cold
4. `POST /api/sweep`
	- 手工触发一次 sweep，或让定时器做周期调用
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染冷热状态和两条历史线


## 9. 下一步阅读

如果你准备继续把层级状态做深，最顺的下一步是：

1. [多层存储](multi-tier-storage-dashboard.md)
	把 hot / warm / cold / archive 继续拆成真正的多层介质与恢复路径
2. [冷热多级老化](hot-cold-multi-level-aging-dashboard.md)
	把“热 -> warm -> cool -> cold -> archive”继续扩成更多正式层级
3. [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)
	对比“同一条 cold state 的统一分流”和“带 rolling window 的联动收口”到底差在哪
