# 把本地控制台服务升级成一个冷热多级老化面板

> 这页要解决的不是“多加一个 warm 层名字”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、多层中间层、cold file 和 archive 语义之后，又开始需要把“热对象先老化到 warm”“warm 再老化到 cool”“cool 再落成 cold file”“访问时按 warm / cool / cold 三条恢复路径回热层”“长时间没有恢复的 cold 再继续 archive”放进同一条后台主线，还要把当前各层状态、最近恢复历史和最近 archive 历史稳定交给页面、JSON、日志和快照时，怎样把 `dict + dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让热层对象、老化窗口和文件恢复各写各的。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- hot / cold 两层怎么拆
- hot / warm / cold / archive 这种多层介质怎么建立
- cold state 怎样统一做回温和归档分流

但真实服务再往前走一步，很快就会出现一个新的问题：

- 有些对象离开 hot 后，不该立刻写盘
- 有些对象已经离开 warm，却还值得保留一个更便宜的中间冷却层
- 有些对象最终才会变成 cold file
- 访问恢复时，不该所有对象都直接走最贵的文件恢复
- 页面和 JSON 还要同时回答“谁在 warm”“谁在 cool”“谁只剩 cold file”“谁已经被 archive”

如果这时还把模型停在：

- hot
- warm
- cold

很快就会出现几个典型问题：

- warm 层会被迫同时承担“刚离开热层”和“接近落盘前的慢层”两种语义
- 恢复路径只有“内存”和“文件”两档，代价差距过大
- sweep 很难回答对象到底该继续留内存、落盘，还是 archive
- 后面继续扩容量策略和老化窗口时，整个状态模型会再次失真

所以这页真正要补出的，不是“多写一个 struct”，而是：

> 当对象需要经过多段老化窗口时，怎样把 `hot -> warm -> cool -> cold file -> archive` 做成一条正式、可恢复、可导出的多级老化主线。


## 2. 为什么这次不能只停在“多层存储”

### 2.1 多层存储解决的是“跨介质恢复”

上一页更强调的是：

- 哪些对象还在内存
- 哪些对象已经落盘
- 如何从 cold file 恢复回 hot

它解决的是：

- 介质边界


### 2.2 多级老化解决的是“中间层要不要继续细分”

这一页真正解决的是：

- 从 hot 离开的对象，不是马上只有一个 warm 中间层
- 不同老化阶段的对象，应该有不同的停留成本和恢复代价

所以这页更关注：

- 老化窗口
- 中间层职责
- 恢复优先级


### 2.3 `warm` 和 `cool` 回答的问题不是一回事

`warm` 更适合回答：

- 对象刚离开 hot
- 短时间内很可能被重新访问
- 恢复成本应该尽量接近 hot

`cool` 更适合回答：

- 对象已经离开最活跃窗口
- 仍然先保留在内存
- 但比 warm 更接近 cold file


### 2.4 真正的难点是“老化顺序”和“恢复顺序”

到了这一层，系统真正需要稳定回答的是：

- 什么时候 hot -> warm
- 什么时候 warm -> cool
- 什么时候 cool -> cold file
- 什么时候 cold -> archive
- 访问恢复时，为什么先看 warm，再看 cool，最后才看 cold file


## 3. 这条多级老化主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 hot tier | 当前热点查询与实时命中 |
| `dict` | 当前 warm tier | 刚离开 hot、最接近快速恢复的对象 |
| `dict` | 当前 cool tier | 已离开 warm、但还不想立刻写盘的对象 |
| `list` | 当前 cold catalog | 已正式落盘的 cold snapshot 与恢复来源 |
| `list` | 最近 restore history | 页面时间线和恢复事件导出 |
| `list` | 最近 archive history | 页面时间线和 archive 事件导出 |
| `queue + thread` | 后台执行 `TO_WARM / ACCESS / SWEEP` | 请求线程不阻塞在多级老化和恢复上 |
| `time` | 记录 warm / cool / cold / archive 时间边界 | 多级老化的正式窗口 |
| `path + file` | 组织 cold 文件和 dashboard 快照路径 | 让最冷层真正进入文件系统 |
| `hash` | 给对象 key 一个稳定指纹 | 生成 cold file 名称与轻量标识 |
| `xvalue + json` | 导出 cold file 内容和 dashboard 摘要 | 页面、脚本、恢复逻辑共享同一份模型 |
| `xhttpd + template` | 展示 hot / warm / cool / cold / history | 浏览器和脚本共享一个入口 |

一句话记住：

> `hot` 管“现在最热的对象”，`warm` 管“刚离开热层但还值得极速恢复的对象”，`cool` 管“已经继续降温但仍先留内存的对象”，`cold` 管“正式落盘的恢复来源”，worker 管“对象什么时候继续老化或按优先级恢复”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成多级老化语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/hot-cold-multi-level-aging.json
runtime/cold/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `warm_age_ms`、`cool_age_ms`、`archive_after_ms`
- `web/dashboard.html`
	- 同时展示当前 hot、warm、cool、cold、restore history 和 archive history
- `runtime/console.log`
	- 记录老化迁移、cold file 落盘和恢复
- `runtime/hot-cold-multi-level-aging.json`
	- 保存最近一次多级老化摘要
- `runtime/cold/<hash>.json`
	- 保存单个对象的 cold snapshot 文件


## 5. 先把 hot / warm / cool / cold / archive 这条后台主线立起来

下面这段代码故意只保留 7 件事：

1. `dict` 保存当前 hot tier
2. `dict` 保存当前 warm tier
3. `dict` 保存当前 cool tier
4. `list` 保存当前 cold catalog
5. `list` 保存最近 restore history
6. `list` 保存最近 archive history
7. `queue + thread` 只处理 `TO_WARM / ACCESS / SWEEP`，让 worker 统一决定继续老化还是恢复

这个骨架会实际做这些事：

- 初始化 aging center
- 注册 `alpha / beta / gamma / delta / epsilon`
- 把 `beta / gamma / delta / epsilon` 从 hot 送进 warm
- 访问 `beta`，先走 warm restore
- 做一次 sweep，让 `gamma / delta / epsilon` 从 warm 继续老化到 cool
- 访问 `gamma`，走 cool restore
- 再做一次 sweep，让 `delta / epsilon` 从 cool 落成 cold file
- 访问 `delta`，走 cold file restore
- 最后再 sweep 一次，让长期未恢复的 `epsilon` 进入 archive

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_JOB_TO_WARM 1
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
	xtime tWarmedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
} DemoWarmItem;

typedef struct
{
	xtime tTouchedAt;
	xtime tWarmedAt;
	xtime tCooledAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
} DemoCoolItem;

typedef struct
{
	xtime tTouchedAt;
	xtime tWarmedAt;
	xtime tCooledAt;
	xtime tFlushedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
	char sColdPath[260];
} DemoColdRecord;

typedef struct
{
	xtime tRestoredAt;
	xtime tArchivedAt;
	xtime tPrevStoredAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sReason[48];
	char sFromTier[16];
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
	xtime tNow;
	uint32 iThresholdMs;
	int iCount;
	char arrKey[32][64];
} DemoAgingBatch;

typedef struct
{
	xdict pHot;
	xdict pWarm;
	xdict pCool;
	xlist pCold;
	xlist pRestore;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextRestoreSeq;
	int64 iNextArchiveSeq;
	uint32 iKeepRestore;
	uint32 iKeepArchive;
	uint32 iWarmAgeMs;
	uint32 iCoolAgeMs;
	uint32 iArchiveAfterMs;
	char sSnapshotPath[260];
} DemoAgingCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}
	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static str procBuildColdPath(uint64 iKeyHash)
{
	char sFile[64];

	snprintf(sFile, sizeof(sFile), "%llu.json", (unsigned long long)iKeyHash);
	return xrtPathJoin(3, "runtime", "cold", sFile);
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
	xvoTableSetText(vRow, "key", 0u, (ptr)pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, (ptr)pItem->sTitle, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pItem->iKeyHash);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectWarm(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoWarmItem* pItem = *(DemoWarmItem**)pVal;
	xvalue vRow;
	(void)pKey;

	if ( (vArr == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, (ptr)pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, (ptr)pItem->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, (ptr)pItem->sTierReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRow, "warmed_at", 0u, pItem->tWarmedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pItem->iKeyHash);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectCool(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoCoolItem* pItem = *(DemoCoolItem**)pVal;
	xvalue vRow;
	(void)pKey;

	if ( (vArr == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, (ptr)pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, (ptr)pItem->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, (ptr)pItem->sTierReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRow, "warmed_at", 0u, pItem->tWarmedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pItem->tCooledAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pItem->iKeyHash);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procCollectCold(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArr = (xvalue)pArg;
	DemoColdRecord* pRecord = (DemoColdRecord*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArr == NULL) || (pRecord == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, (ptr)pRecord->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, (ptr)pRecord->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, (ptr)pRecord->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "cold_path", 0u, (ptr)pRecord->sColdPath, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pRecord->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pRecord->tTouchedAt);
	xvoTableSetInt(vRow, "warmed_at", 0u, pRecord->tWarmedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pRecord->tCooledAt);
	xvoTableSetInt(vRow, "flushed_at", 0u, pRecord->tFlushedAt);
	xvoTableSetInt(vRow, "file_exists", 0u, xrtFileExists(pRecord->sColdPath) ? 1 : 0);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pRecord->iKeyHash);
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
	xvoTableSetText(vRow, "key", 0u, (ptr)pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, (ptr)pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, (ptr)pEvent->sReason, 0u, FALSE);
	xvoTableSetText(vRow, "from_tier", 0u, (ptr)pEvent->sFromTier, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEvent->iHeat);
	xvoTableSetInt(vRow, "prev_stored_at", 0u, pEvent->tPrevStoredAt);
	xvoTableSetInt(vRow, "restored_at", 0u, pEvent->tRestoredAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEvent->tArchivedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEvent->iKeyHash);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteColdFile(const DemoCoolItem* pItem, const char* sColdPath)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0u;
	bool bOk = FALSE;

	if ( (pItem == NULL) || (sColdPath == NULL) || (*sColdPath == '\0') ) {
		return FALSE;
	}

	vRoot = xvoCreateTable();
	xvoTableSetText(vRoot, "key", 0u, (ptr)pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRoot, "title", 0u, (ptr)pItem->sTitle, 0u, FALSE);
	xvoTableSetText(vRoot, "tier_reason", 0u, (ptr)pItem->sTierReason, 0u, FALSE);
	xvoTableSetInt(vRoot, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRoot, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRoot, "warmed_at", 0u, pItem->tWarmedAt);
	xvoTableSetInt(vRoot, "cooled_at", 0u, pItem->tCooledAt);
	xvoTableSetInt(vRoot, "key_hash", 0u, (int64)pItem->iKeyHash);

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	sDir = xrtPathGetDir((str)sColdPath, 0u);
	if ( (sDir == NULL) || (xrtDirCreateAll(sDir) != TRUE) ) {
		goto cleanup;
	}

	if ( xrtFileWriteAll((str)sColdPath, sJson, iJsonSize, XRT_CP_UTF8) < 0 ) {
		goto cleanup;
	}

	bOk = TRUE;

cleanup:
	if ( sDir != NULL ) xrtFree(sDir);
	if ( sJson != NULL ) xrtFree(sJson);
	if ( vRoot != NULL ) xvoUnref(vRoot);
	return bOk;
}

static bool procWriteDashboardSnapshotLocked(DemoAgingCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHot = NULL;
	xvalue vWarm = NULL;
	xvalue vCool = NULL;
	xvalue vCold = NULL;
	xvalue vRestore = NULL;
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
	vWarm = xvoCreateArray();
	vCool = xvoCreateArray();
	vCold = xvoCreateArray();
	vRestore = xvoCreateArray();
	vArchive = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHot, vHot);
	xrtDictWalk(pCenter->pWarm, procCollectWarm, vWarm);
	xrtDictWalk(pCenter->pCool, procCollectCool, vCool);
	xrtListWalk(pCenter->pCold, procCollectCold, vCold);
	xrtListWalk(pCenter->pRestore, procCollectEvent, vRestore);
	xrtListWalk(pCenter->pArchive, procCollectEvent, vArchive);

	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "warm_count", 0u, xrtDictCount(pCenter->pWarm));
	xvoTableSetInt(vRoot, "cool_count", 0u, xrtDictCount(pCenter->pCool));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "restore_count", 0u, xrtListCount(pCenter->pRestore));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "warm_age_ms", 0u, pCenter->iWarmAgeMs);
	xvoTableSetInt(vRoot, "cool_age_ms", 0u, pCenter->iCoolAgeMs);
	xvoTableSetInt(vRoot, "archive_after_ms", 0u, pCenter->iArchiveAfterMs);
	xvoTableSetText(vRoot, "snapshot_path", 0u, (ptr)pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHot, TRUE);
	xvoUnref(vHot);
	vHot = NULL;
	xvoTableSetValue(vRoot, "warm_items", 0u, vWarm, TRUE);
	xvoUnref(vWarm);
	vWarm = NULL;
	xvoTableSetValue(vRoot, "cool_items", 0u, vCool, TRUE);
	xvoUnref(vCool);
	vCool = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vCold, TRUE);
	xvoUnref(vCold);
	vCold = NULL;
	xvoTableSetValue(vRoot, "restore_events", 0u, vRestore, TRUE);
	xvoUnref(vRestore);
	vRestore = NULL;
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
	if ( vRestore != NULL ) xvoUnref(vRestore);
	if ( vCold != NULL ) xvoUnref(vCold);
	if ( vCool != NULL ) xvoUnref(vCool);
	if ( vWarm != NULL ) xvoUnref(vWarm);
	if ( vHot != NULL ) xvoUnref(vHot);
	if ( vRoot != NULL ) xvoUnref(vRoot);
	return bOk;
}

static bool procCollectWarmAgedKeys(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoAgingBatch* pBatch = (DemoAgingBatch*)pArg;
	DemoWarmItem* pItem = *(DemoWarmItem**)pVal;
	(void)pKey;

	if ( (pBatch == NULL) || (pItem == NULL) ) {
		return FALSE;
	}
	if ( pBatch->iCount >= 32 ) {
		return TRUE;
	}
	if ( (pBatch->tNow - pItem->tWarmedAt) >= (xtime)pBatch->iThresholdMs ) {
		procCopyText(pBatch->arrKey[pBatch->iCount], sizeof(pBatch->arrKey[pBatch->iCount]), pItem->sKey);
		pBatch->iCount++;
	}
	return FALSE;
}

static bool procCollectCoolAgedKeys(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoAgingBatch* pBatch = (DemoAgingBatch*)pArg;
	DemoCoolItem* pItem = *(DemoCoolItem**)pVal;
	(void)pKey;

	if ( (pBatch == NULL) || (pItem == NULL) ) {
		return FALSE;
	}
	if ( pBatch->iCount >= 32 ) {
		return TRUE;
	}
	if ( (pBatch->tNow - pItem->tCooledAt) >= (xtime)pBatch->iThresholdMs ) {
		procCopyText(pBatch->arrKey[pBatch->iCount], sizeof(pBatch->arrKey[pBatch->iCount]), pItem->sKey);
		pBatch->iCount++;
	}
	return FALSE;
}

static void procAppendRestoreLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sTitle, uint64 iKeyHash, int iHeat, xtime tPrevStoredAt, const char* sFromTier, const char* sReason)
{
	DemoStateEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextRestoreSeq++;
	pEvent = (DemoStateEvent*)xrtListSet(pCenter->pRestore, pCenter->iNextRestoreSeq, &bNew);
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), sTitle);
	procCopyText(pEvent->sFromTier, sizeof(pEvent->sFromTier), sFromTier);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = iKeyHash;
	pEvent->iHeat = iHeat;
	pEvent->tPrevStoredAt = tPrevStoredAt;
	pEvent->tRestoredAt = xrtNow();

	if ( pCenter->iNextRestoreSeq > (int64)pCenter->iKeepRestore ) {
		(void)xrtListRemove(pCenter->pRestore, pCenter->iNextRestoreSeq - (int64)pCenter->iKeepRestore);
	}
}

static void procAppendArchiveLocked(DemoAgingCenter* pCenter, const DemoColdRecord* pRecord, const char* sReason)
{
	DemoStateEvent* pEvent = NULL;
	bool bNew = FALSE;

	pCenter->iNextArchiveSeq++;
	pEvent = (DemoStateEvent*)xrtListSet(pCenter->pArchive, pCenter->iNextArchiveSeq, &bNew);
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), pRecord->sKey);
	procCopyText(pEvent->sTitle, sizeof(pEvent->sTitle), pRecord->sTitle);
	procCopyText(pEvent->sFromTier, sizeof(pEvent->sFromTier), "cold");
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
	pEvent->iKeyHash = pRecord->iKeyHash;
	pEvent->iHeat = pRecord->iHeat;
	pEvent->tPrevStoredAt = pRecord->tFlushedAt;
	pEvent->tArchivedAt = xrtNow();

	if ( pCenter->iNextArchiveSeq > (int64)pCenter->iKeepArchive ) {
		(void)xrtListRemove(pCenter->pArchive, pCenter->iNextArchiveSeq - (int64)pCenter->iKeepArchive);
	}
}

static bool procFindColdWalk(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdLookup* pLookup = (DemoColdLookup*)pArg;
	DemoColdRecord* pRecord = (DemoColdRecord*)pVal;

	if ( (pLookup == NULL) || (pRecord == NULL) || (pLookup->sKey == NULL) ) {
		return FALSE;
	}
	if ( strcmp(pRecord->sKey, pLookup->sKey) == 0 ) {
		pLookup->bFound = TRUE;
		pLookup->iSeq = iKey;
		return TRUE;
	}
	return FALSE;
}

static bool procCenterInit(DemoAgingCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepRestore = 8u;
	pCenter->iKeepArchive = 8u;
	pCenter->iWarmAgeMs = 140u;
	pCenter->iCoolAgeMs = 160u;
	pCenter->iArchiveAfterMs = 180u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pWarm = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pCool = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pCold = xrtListCreate(sizeof(DemoColdRecord), XRT_OBJMODE_SHARED);
	pCenter->pRestore = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->pArchive = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->hMutex = xrtMutexCreate();
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	sSnapshotPath = xrtPathJoin(2, "runtime", "hot-cold-multi-level-aging.json");

	if ( (pCenter->pHot == NULL) || (pCenter->pWarm == NULL) || (pCenter->pCool == NULL) || (pCenter->pCold == NULL) || (pCenter->pRestore == NULL) || (pCenter->pArchive == NULL) || (pCenter->hMutex == NULL) || (pCenter->hQueue == NULL) || (sSnapshotPath == NULL) ) {
		if ( sSnapshotPath != NULL ) xrtFree(sSnapshotPath);
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoAgingCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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

static bool procMoveHotToWarmLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoHotItem* pHot = NULL;
	DemoWarmItem* pWarm = NULL;

	pHot = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)sKey, (uint32)strlen(sKey));
	if ( pHot == NULL ) {
		return FALSE;
	}

	pWarm = (DemoWarmItem*)xrtMalloc(sizeof(DemoWarmItem));
	if ( pWarm == NULL ) {
		(void)xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL);
		return FALSE;
	}

	memset(pWarm, 0, sizeof(*pWarm));
	pWarm->tTouchedAt = pHot->tTouchedAt;
	pWarm->tWarmedAt = xrtNow();
	pWarm->iKeyHash = pHot->iKeyHash;
	pWarm->iHeat = pHot->iHeat;
	procCopyText(pWarm->sKey, sizeof(pWarm->sKey), pHot->sKey);
	procCopyText(pWarm->sTitle, sizeof(pWarm->sTitle), pHot->sTitle);
	procCopyText(pWarm->sTierReason, sizeof(pWarm->sTierReason), sReason);

	if ( !xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL) ) {
		xrtFree(pWarm);
		(void)xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL);
		return FALSE;
	}

	xrtFree(pHot);
	return TRUE;
}

static bool procMoveWarmToCoolLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoWarmItem* pWarm = NULL;
	DemoCoolItem* pCool = NULL;

	pWarm = (DemoWarmItem*)xrtDictRemovePtr(pCenter->pWarm, (ptr)sKey, (uint32)strlen(sKey));
	if ( pWarm == NULL ) {
		return FALSE;
	}

	pCool = (DemoCoolItem*)xrtMalloc(sizeof(DemoCoolItem));
	if ( pCool == NULL ) {
		(void)xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL);
		return FALSE;
	}

	memset(pCool, 0, sizeof(*pCool));
	pCool->tTouchedAt = pWarm->tTouchedAt;
	pCool->tWarmedAt = pWarm->tWarmedAt;
	pCool->tCooledAt = xrtNow();
	pCool->iKeyHash = pWarm->iKeyHash;
	pCool->iHeat = pWarm->iHeat;
	procCopyText(pCool->sKey, sizeof(pCool->sKey), pWarm->sKey);
	procCopyText(pCool->sTitle, sizeof(pCool->sTitle), pWarm->sTitle);
	procCopyText(pCool->sTierReason, sizeof(pCool->sTierReason), sReason);

	if ( !xrtDictSetPtr(pCenter->pCool, (ptr)pCool->sKey, (uint32)strlen(pCool->sKey), pCool, NULL) ) {
		xrtFree(pCool);
		(void)xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL);
		return FALSE;
	}

	xrtFree(pWarm);
	return TRUE;
}

static bool procFlushCoolToColdLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoCoolItem* pCool = NULL;
	DemoColdRecord* pCold = NULL;
	bool bNew = FALSE;
	str sColdPath = NULL;

	pCool = (DemoCoolItem*)xrtDictRemovePtr(pCenter->pCool, (ptr)sKey, (uint32)strlen(sKey));
	if ( pCool == NULL ) {
		return FALSE;
	}

	sColdPath = procBuildColdPath(pCool->iKeyHash);
	if ( (sColdPath == NULL) || !procWriteColdFile(pCool, sColdPath) ) {
		if ( sColdPath != NULL ) xrtFree(sColdPath);
		(void)xrtDictSetPtr(pCenter->pCool, (ptr)pCool->sKey, (uint32)strlen(pCool->sKey), pCool, NULL);
		return FALSE;
	}

	pCenter->iNextColdSeq++;
	pCold = (DemoColdRecord*)xrtListSet(pCenter->pCold, pCenter->iNextColdSeq, &bNew);
	if ( pCold == NULL ) {
		(void)xrtFileDelete(sColdPath);
		xrtFree(sColdPath);
		(void)xrtDictSetPtr(pCenter->pCool, (ptr)pCool->sKey, (uint32)strlen(pCool->sKey), pCool, NULL);
		return FALSE;
	}

	memset(pCold, 0, sizeof(*pCold));
	pCold->tTouchedAt = pCool->tTouchedAt;
	pCold->tWarmedAt = pCool->tWarmedAt;
	pCold->tCooledAt = pCool->tCooledAt;
	pCold->tFlushedAt = xrtNow();
	pCold->iKeyHash = pCool->iKeyHash;
	pCold->iHeat = pCool->iHeat;
	procCopyText(pCold->sKey, sizeof(pCold->sKey), pCool->sKey);
	procCopyText(pCold->sTitle, sizeof(pCold->sTitle), pCool->sTitle);
	procCopyText(pCold->sTierReason, sizeof(pCold->sTierReason), sReason);
	procCopyText(pCold->sColdPath, sizeof(pCold->sColdPath), sColdPath);

	xrtFree(sColdPath);
	xrtFree(pCool);
	return TRUE;
}

static bool procRestoreWarmToHotLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoWarmItem* pWarm = NULL;
	DemoHotItem* pHot = NULL;

	pWarm = (DemoWarmItem*)xrtDictRemovePtr(pCenter->pWarm, (ptr)sKey, (uint32)strlen(sKey));
	if ( pWarm == NULL ) {
		return FALSE;
	}

	pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
	if ( pHot == NULL ) {
		(void)xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL);
		return FALSE;
	}

	memset(pHot, 0, sizeof(*pHot));
	pCenter->iNextHotId++;
	pHot->iHotId = pCenter->iNextHotId;
	pHot->tTouchedAt = xrtNow();
	pHot->iKeyHash = pWarm->iKeyHash;
	pHot->iHeat = pWarm->iHeat + 1;
	procCopyText(pHot->sKey, sizeof(pHot->sKey), pWarm->sKey);
	procCopyText(pHot->sTitle, sizeof(pHot->sTitle), pWarm->sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
		xrtFree(pHot);
		(void)xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL);
		return FALSE;
	}

	procAppendRestoreLocked(pCenter, pWarm->sKey, pWarm->sTitle, pWarm->iKeyHash, pHot->iHeat, pWarm->tWarmedAt, "warm", sReason);
	xrtFree(pWarm);
	return TRUE;
}

static bool procRestoreCoolToHotLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoCoolItem* pCool = NULL;
	DemoHotItem* pHot = NULL;

	pCool = (DemoCoolItem*)xrtDictRemovePtr(pCenter->pCool, (ptr)sKey, (uint32)strlen(sKey));
	if ( pCool == NULL ) {
		return FALSE;
	}

	pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
	if ( pHot == NULL ) {
		(void)xrtDictSetPtr(pCenter->pCool, (ptr)pCool->sKey, (uint32)strlen(pCool->sKey), pCool, NULL);
		return FALSE;
	}

	memset(pHot, 0, sizeof(*pHot));
	pCenter->iNextHotId++;
	pHot->iHotId = pCenter->iNextHotId;
	pHot->tTouchedAt = xrtNow();
	pHot->iKeyHash = pCool->iKeyHash;
	pHot->iHeat = pCool->iHeat + 1;
	procCopyText(pHot->sKey, sizeof(pHot->sKey), pCool->sKey);
	procCopyText(pHot->sTitle, sizeof(pHot->sTitle), pCool->sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
		xrtFree(pHot);
		(void)xrtDictSetPtr(pCenter->pCool, (ptr)pCool->sKey, (uint32)strlen(pCool->sKey), pCool, NULL);
		return FALSE;
	}

	procAppendRestoreLocked(pCenter, pCool->sKey, pCool->sTitle, pCool->iKeyHash, pHot->iHeat, pCool->tCooledAt, "cool", sReason);
	xrtFree(pCool);
	return TRUE;
}

static bool procRestoreColdToHotLocked(DemoAgingCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoColdLookup tLookup;
	DemoColdRecord* pCold = NULL;
	DemoHotItem* pHot = NULL;
	str sJson = NULL;
	xvalue vRoot = NULL;
	size_t iJsonSize = 0u;
	const char* sKeyText = NULL;
	const char* sTitle = NULL;
	int64 iHeat = 0;

	memset(&tLookup, 0, sizeof(tLookup));
	tLookup.sKey = sKey;
	xrtListWalk(pCenter->pCold, procFindColdWalk, &tLookup);
	if ( !tLookup.bFound ) {
		return FALSE;
	}

	pCold = (DemoColdRecord*)xrtListGet(pCenter->pCold, tLookup.iSeq);
	if ( pCold == NULL ) {
		return FALSE;
	}

	sJson = xrtFileReadAll(pCold->sColdPath, XRT_CP_AUTO, &iJsonSize);
	if ( sJson == NULL ) {
		return FALSE;
	}

	vRoot = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);
	sJson = NULL;
	if ( xvoIsNull(vRoot) ) {
		if ( vRoot != NULL ) xvoUnref(vRoot);
		return FALSE;
	}

	sKeyText = xvoTableGetText(vRoot, "key", 0u);
	sTitle = xvoTableGetText(vRoot, "title", 0u);
	iHeat = xvoTableGetInt(vRoot, "heat", 0u);

	pHot = (DemoHotItem*)xrtMalloc(sizeof(DemoHotItem));
	if ( pHot == NULL ) {
		xvoUnref(vRoot);
		return FALSE;
	}

	memset(pHot, 0, sizeof(*pHot));
	pCenter->iNextHotId++;
	pHot->iHotId = pCenter->iNextHotId;
	pHot->tTouchedAt = xrtNow();
	pHot->iKeyHash = pCold->iKeyHash;
	pHot->iHeat = (int)iHeat + 1;
	procCopyText(pHot->sKey, sizeof(pHot->sKey), (sKeyText != NULL) ? sKeyText : pCold->sKey);
	procCopyText(pHot->sTitle, sizeof(pHot->sTitle), (sTitle != NULL) ? sTitle : pCold->sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL) ) {
		xrtFree(pHot);
		xvoUnref(vRoot);
		return FALSE;
	}

	procAppendRestoreLocked(pCenter, pCold->sKey, pCold->sTitle, pCold->iKeyHash, pHot->iHeat, pCold->tFlushedAt, "cold", sReason);
	(void)xrtFileDelete(pCold->sColdPath);
	(void)xrtListRemove(pCenter->pCold, tLookup.iSeq);
	xvoUnref(vRoot);
	return TRUE;
}

static bool procSubmitJob(DemoAgingCenter* pCenter, int iKind, const char* sKey, const char* sText)
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

static void procSweepAgingLocked(DemoAgingCenter* pCenter)
{
	DemoAgingBatch tBatch;
	DemoColdRecord* pCold = NULL;
	int i;
	int64 iSeq;
	xtime tNow;

	memset(&tBatch, 0, sizeof(tBatch));
	tBatch.tNow = xrtNow();
	tBatch.iThresholdMs = pCenter->iWarmAgeMs;
	xrtDictWalk(pCenter->pWarm, procCollectWarmAgedKeys, &tBatch);
	for ( i = 0; i < tBatch.iCount; i++ ) {
		(void)procMoveWarmToCoolLocked(pCenter, tBatch.arrKey[i], "aged-from-warm");
	}

	memset(&tBatch, 0, sizeof(tBatch));
	tBatch.tNow = xrtNow();
	tBatch.iThresholdMs = pCenter->iCoolAgeMs;
	xrtDictWalk(pCenter->pCool, procCollectCoolAgedKeys, &tBatch);
	for ( i = 0; i < tBatch.iCount; i++ ) {
		(void)procFlushCoolToColdLocked(pCenter, tBatch.arrKey[i], "aged-to-cold-file");
	}

	tNow = xrtNow();
	for ( iSeq = 1; iSeq <= pCenter->iNextColdSeq; iSeq++ ) {
		pCold = (DemoColdRecord*)xrtListGet(pCenter->pCold, iSeq);
		if ( pCold == NULL ) {
			continue;
		}
		if ( (tNow - pCold->tFlushedAt) < (xtime)pCenter->iArchiveAfterMs ) {
			continue;
		}

		procAppendArchiveLocked(pCenter, pCold, "aged-to-archive");
		if ( pCold->sColdPath[0] != '\0' ) {
			(void)xrtFileDelete(pCold->sColdPath);
		}
		(void)xrtListRemove(pCenter->pCold, iSeq);
	}
}

static uint32 procAgingWorker(ptr pArg)
{
	DemoAgingCenter* pCenter = (DemoAgingCenter*)pArg;
	DemoJob* pJob = NULL;
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

		if ( pJob->iKind == DEMO_JOB_TO_WARM ) {
			(void)procMoveHotToWarmLocked(pCenter, pJob->sKey, pJob->sText);
		} else if ( pJob->iKind == DEMO_JOB_ACCESS ) {
			if ( !procRestoreWarmToHotLocked(pCenter, pJob->sKey, pJob->sText) ) {
				if ( !procRestoreCoolToHotLocked(pCenter, pJob->sKey, pJob->sText) ) {
					(void)procRestoreColdToHotLocked(pCenter, pJob->sKey, pJob->sText);
				}
			}
		} else if ( pJob->iKind == DEMO_JOB_SWEEP ) {
			procSweepAgingLocked(pCenter);
		}

		(void)procWriteDashboardSnapshotLocked(pCenter);
		xrtMutexUnlock(pCenter->hMutex);
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

static bool procDumpWarm(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoWarmItem* pItem = *(DemoWarmItem**)pVal;
	(void)pKey;
	(void)pArg;
	if ( pItem != NULL ) {
		printf("warm key=%s title=%s reason=%s warmed_at=%lld\n", pItem->sKey, pItem->sTitle, pItem->sTierReason, (long long)pItem->tWarmedAt);
	}
	return FALSE;
}

static bool procDumpCool(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoCoolItem* pItem = *(DemoCoolItem**)pVal;
	(void)pKey;
	(void)pArg;
	if ( pItem != NULL ) {
		printf("cool key=%s title=%s reason=%s cooled_at=%lld\n", pItem->sKey, pItem->sTitle, pItem->sTierReason, (long long)pItem->tCooledAt);
	}
	return FALSE;
}

static bool procDumpCold(int64 iKey, ptr pVal, ptr pArg)
{
	DemoColdRecord* pRecord = (DemoColdRecord*)pVal;
	(void)pArg;
	printf("cold seq=%lld key=%s path=%s flushed_at=%lld\n", (long long)iKey, pRecord->sKey, pRecord->sColdPath, (long long)pRecord->tFlushedAt);
	return FALSE;
}

static bool procDumpEvent(int64 iKey, ptr pVal, ptr pArg)
{
	DemoStateEvent* pEvent = (DemoStateEvent*)pVal;
	const char* sTag = (const char*)pArg;
	printf("%s seq=%lld key=%s from=%s reason=%s\n", sTag, (long long)iKey, pEvent->sKey, pEvent->sFromTier, pEvent->sReason);
	return FALSE;
}

static bool procFreeHeapItem(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	ptr pItem = *(ptr*)pVal;
	(void)pKey;
	(void)pArg;
	if ( pItem != NULL ) xrtFree(pItem);
	return FALSE;
}

static void procCenterUnit(DemoAgingCenter* pCenter)
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
		xrtDictWalk(pCenter->pHot, procFreeHeapItem, NULL);
		if ( pCenter->pWarm != NULL ) xrtDictWalk(pCenter->pWarm, procFreeHeapItem, NULL);
		if ( pCenter->pCool != NULL ) xrtDictWalk(pCenter->pCool, procFreeHeapItem, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}
	if ( pCenter->hMutex != NULL ) xrtMutexDestroy(pCenter->hMutex);
	if ( pCenter->pArchive != NULL ) xrtListDestroy(pCenter->pArchive);
	if ( pCenter->pRestore != NULL ) xrtListDestroy(pCenter->pRestore);
	if ( pCenter->pCold != NULL ) xrtListDestroy(pCenter->pCold);
	if ( pCenter->pCool != NULL ) xrtDictDestroy(pCenter->pCool);
	if ( pCenter->pWarm != NULL ) xrtDictDestroy(pCenter->pWarm);
	if ( pCenter->pHot != NULL ) xrtDictDestroy(pCenter->pHot);
}

int main(void)
{
	DemoAgingCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) return 1;
	if ( !procCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procAgingWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 6) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 5) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:delta", "Delta Backfill", 4) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:epsilon", "Epsilon Audit", 3) ) goto cleanup;

	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:beta", "aged-from-hot") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:gamma", "aged-from-hot") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:delta", "aged-from-hot") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:epsilon", "aged-from-hot") ) goto cleanup;
	xrtSleep(120u);

	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "warm-hit") ) goto cleanup;
	xrtSleep(180u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "aging-sweep-1") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:gamma", "cool-hit") ) goto cleanup;
	xrtSleep(220u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "aging-sweep-2") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:delta", "cold-file-hit") ) goto cleanup;
	xrtSleep(220u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "aging-sweep-3") ) goto cleanup;

	xrtSleep(320u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);
	printf("== warm ==\n");
	xrtDictWalk(tCenter.pWarm, procDumpWarm, NULL);
	printf("== cool ==\n");
	xrtDictWalk(tCenter.pCool, procDumpCool, NULL);
	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);
	printf("== restore ==\n");
	xrtListWalk(tCenter.pRestore, procDumpEvent, "restore");
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

### 6.1 这页真正新增的是 `cool` 这一层

它不是：

- warm 的别名
- cold 的别名

它真正解决的是：

- 给“已经不够热，但还没必要立刻写盘”的对象一层正式中间态


### 6.2 恢复顺序必须严格按代价递增

这页里的 `ACCESS` 会按顺序尝试：

1. `warm`
2. `cool`
3. `cold file`

因为多级老化真正要稳定下来的，不只是“有几层”，而是：

- 恢复时先走便宜层，再走贵层


### 6.3 `SWEEP` 不是简单删数据，而是继续推进正式老化

这页里的 `SWEEP` 实际承担了 3 种动作：

- `warm -> cool`
- `cool -> cold file`
- `cold -> archive`

这就是为什么它必须放在同一个 worker 里统一执行。


### 6.4 cold file 仍然是正式恢复来源，不是日志副本

这页故意把 cold restore 写成：

- `xrtFileReadAll()`
- `xrtParseJSON()`
- `xvoTableGetText() / xvoTableGetInt()`

因为真正的 cold 层，不是“顺手写个文件”，而是一份正式恢复来源。


### 6.5 archive 一旦成立，就不再继续当实时恢复来源

这页里一旦 `cold` 继续老化到 `archive`：

- cold file 会被删除
- archive 只保留摘要事件

这样 archive 层就明确只是：

- 历史层


### 6.6 多级老化的核心不是字段多，而是窗口多

这一页真正稳定下来的，是这些窗口：

- warm age
- cool age
- archive age

如果窗口没有写稳，多一层名字也没有意义。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先改模型，再决定是否复用这套骨架：

- 如果你真正需要的是 bounded fan-out 任务收口，而不是长期老化服务，优先考虑 `task group`
- 如果 cold 层已经是大批量慢 I/O，应该继续拆 `file_async`
- 如果 archive 层已经需要压缩、打包或外部工具，应该继续补 `subprocess`


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 hot / warm / cool / cold / restore / archive 摘要
2. `POST /api/to-warm`
	- 只提交 `TO_WARM` signal
3. `POST /api/access`
	- 只提交恢复请求，让 worker 决定走 warm / cool / cold 哪一层
4. `POST /api/sweep`
	- 手工触发或定时触发多级老化 sweep
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染全部层级和历史线


## 9. 下一步阅读

如果你准备继续把这条主线和其他策略做对照，最顺的下一步是：

1. [把本地控制台服务升级成一个多层存储面板](multi-tier-storage-dashboard.md)
	对比“跨介质恢复路径”和“多级老化窗口”到底差在哪
2. [把本地控制台服务升级成一个自动回温 + 滚动归档联动面板](auto-warm-rolling-archive-dashboard.md)
	对比“单条 cold state 的策略联动”和“多级老化窗口”的边界区别
3. [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)
	对比“统一 cold 分流”与“更多中间层”的设计取舍
