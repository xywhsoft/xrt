# 把本地控制台服务升级成一个多层存储面板

> 这页要解决的不是“热层满了就顺手写个文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、warm tier、cold snapshot 和 archive 语义之后，又开始需要把“热对象先降到内存 warm”“warm 再落成磁盘 cold”“访问时按 warm / cold 两条恢复路径回热层”“长时间没恢复的 cold 再继续 archive”放进同一条后台主线，还要把当前各层状态、最近恢复历史和最近 archive 历史稳定交给页面、JSON、日志和快照时，怎样把 `dict + dict + list + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让热层对象、文件快照和恢复逻辑各写各的。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- hot / cold 两层怎么拆
- cold 对象怎样自动回温
- cold window 怎样继续滚动 archive

但真实服务再往前走一步，很快就会出现一个新的问题：

- 某些对象刚从 hot 降下来，还没必要立刻写盘
- 某些对象已经离开实时内存，但还需要保留一份可恢复的 cold snapshot
- 某些对象被重新访问时，应该先走 warm restore，再决定是否需要 cold file restore
- 某些 cold snapshot 很久没有再被访问，已经该继续进入 archive 历史
- 页面和 JSON 还要同时回答“谁还在 hot / warm / cold”“谁刚刚恢复了”“谁又被 archive 了”

如果这时还把实现停在：

- 一个 `dict` 存活对象
- 一个 `list` 存冷对象
- 需要恢复时直接自己拼文件路径读 JSON

很快就会出现几个典型问题：

- “warm 只是暂存内存”还是“cold 已经正式落盘”没有边界
- 页面为了显示各层状态开始同时扫内存结构和磁盘目录
- warm restore 和 cold restore 各有一套半独立逻辑，后面难以统一
- 后面继续扩“冷热多级老化”时，整个状态模型会直接变形

所以这页真正要补出的，不是“多加两个层名字”，而是：

> 当对象已经开始跨内存和文件系统迁移时，怎样把 hot / warm / cold / archive 做成一条正式可恢复的多层主线。


## 2. 为什么这次不能再把“内存”和“文件”混成一层

### 2.1 warm tier 解决的是“先离开热层，但还没必要立刻写盘”

warm tier 更适合回答：

- 这个对象已经不该继续占着 hot tier
- 但它短时间内仍可能被重新访问
- 现在把它先留在内存里，比立即走磁盘恢复更划算


### 2.2 cold tier 解决的是“正式落盘后的恢复来源”

cold tier 真正解决的是：

- 对象已经离开内存主路径
- 但系统仍保留一份正式可恢复的文件快照
- 以后恢复时，应该能从统一的 `path + file + json` 边界重新回到 hot


### 2.3 archive tier 解决的是“长期历史，不再是当前恢复主线”

archive 层更适合回答：

- 哪些 cold snapshot 已经不是当前主路径上的恢复来源
- 它们什么时候离开了 cold
- 以后页面和日志只需要看摘要，而不是再拿它们做实时恢复


### 2.4 真正的联动点是“状态迁移顺序”

到了这一层，系统真正要稳定回答的是：

- 什么时候 hot -> warm
- 什么时候 warm -> cold file
- 什么时候 warm / cold -> hot restore
- 什么时候 cold -> archive


## 3. 这条多层主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 hot tier | 当前热点查询与实时命中 |
| `dict` | 当前 warm tier | 已离开 hot，但仍保留在内存里的近期对象 |
| `list` | 当前 cold catalog | 已正式落盘的 cold snapshot 与恢复来源 |
| `list` | 最近 restore history | 页面时间线和恢复事件导出 |
| `list` | 最近 archive history | 页面时间线和 archive 事件导出 |
| `queue + thread` | 后台执行 `TO_WARM / FLUSH_COLD / ACCESS / SWEEP` | 请求线程不阻塞在迁移、恢复和落盘上 |
| `time` | 记录降层、写盘、恢复、归档时间 | 多层迁移的正式时间边界 |
| `path + file` | 组织 cold 文件路径和 dashboard 快照路径 | 让 cold 层和快照真正进入文件系统 |
| `hash` | 给对象 key 一个稳定指纹 | 生成 cold file 名称与页面轻量标识 |
| `xvalue + json` | 导出 cold file 内容和 dashboard 摘要 | 页面、脚本、恢复逻辑共享同一份模型 |
| `xhttpd + template` | 展示 hot / warm / cold / history | 浏览器和脚本共享一个入口 |

一句话记住：

> 一个 `dict` 管“现在热着什么”，另一个 `dict` 管“刚离开热层但还在内存里的什么”，一条 `list` 管“正式落盘的 cold snapshot catalog”，另外两条 `list` 分别管“最近恢复”和“最近 archive”，worker 管“对象什么时候跨层迁移”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成多层存储语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/multi-tier-storage.json
runtime/cold/<hash>.json
```

其中：

- `config/console.json`
	- 保存多层阈值、sweep 周期和 cold 保留策略
- `web/dashboard.html`
	- 同时展示当前 hot、warm、cold、restore history 和 archive history
- `runtime/console.log`
	- 记录跨层迁移、cold file 落盘和 restore
- `runtime/multi-tier-storage.json`
	- 保存最近一次多层摘要
- `runtime/cold/<hash>.json`
	- 保存单个对象的 cold snapshot 文件


## 5. 先把 hot / warm / cold / archive 这条后台主线立起来

下面这段代码故意只保留 7 件事：

1. `dict` 保存当前 hot tier
2. `dict` 保存当前 warm tier
3. `list` 保存当前 cold catalog
4. `list` 保存最近 restore history
5. `list` 保存最近 archive history
6. `queue + thread` 同时处理 `TO_WARM / FLUSH_COLD / ACCESS / SWEEP`
7. `path + file + json` 把 cold snapshot 和 dashboard snapshot 都落到文件系统

这个骨架会实际做这些事：

- 初始化 multi-tier center
- 注册 `alpha / beta / gamma / delta`
- 把 `beta / gamma / delta` 从 hot 先降到 warm
- 再把 `gamma / delta` 从 warm 正式落成 cold file
- 访问 `beta` 时走 warm restore
- 访问 `gamma` 时走 cold file restore
- 稍后再 sweep，让长期未恢复的 `delta` 进入 archive history
- 主线程打印当前各层、restore history、archive history 和最终快照

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_JOB_TO_WARM 1
#define DEMO_JOB_FLUSH_COLD 2
#define DEMO_JOB_ACCESS 3
#define DEMO_JOB_SWEEP 4

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
	xdict pHot;
	xdict pWarm;
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
	uint32 iArchiveAfterMs;
	char sSnapshotPath[260];
} DemoTierCenter;


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
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
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
	xvoTableSetText(vRow, "key", 0u, pItem->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pItem->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pItem->sTierReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pItem->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pItem->tTouchedAt);
	xvoTableSetInt(vRow, "warmed_at", 0u, pItem->tWarmedAt);
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
	xvoTableSetText(vRow, "key", 0u, pRecord->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pRecord->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pRecord->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "cold_path", 0u, pRecord->sColdPath, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pRecord->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pRecord->tTouchedAt);
	xvoTableSetInt(vRow, "warmed_at", 0u, pRecord->tWarmedAt);
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
	xvoTableSetText(vRow, "key", 0u, pEvent->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEvent->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "reason", 0u, pEvent->sReason, 0u, FALSE);
	xvoTableSetText(vRow, "from_tier", 0u, pEvent->sFromTier, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEvent->iHeat);
	xvoTableSetInt(vRow, "prev_stored_at", 0u, pEvent->tPrevStoredAt);
	xvoTableSetInt(vRow, "restored_at", 0u, pEvent->tRestoredAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEvent->tArchivedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEvent->iKeyHash);
	(void)xvoArrayAppendValue(vArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteColdFile(const DemoWarmItem* pItem, const char* sColdPath)
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

static bool procWriteDashboardSnapshotLocked(DemoTierCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHot = NULL;
	xvalue vWarm = NULL;
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
	vCold = xvoCreateArray();
	vRestore = xvoCreateArray();
	vArchive = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHot, vHot);
	xrtDictWalk(pCenter->pWarm, procCollectWarm, vWarm);
	xrtListWalk(pCenter->pCold, procCollectCold, vCold);
	xrtListWalk(pCenter->pRestore, procCollectEvent, vRestore);
	xrtListWalk(pCenter->pArchive, procCollectEvent, vArchive);

	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "warm_count", 0u, xrtDictCount(pCenter->pWarm));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "restore_count", 0u, xrtListCount(pCenter->pRestore));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "archive_after_ms", 0u, pCenter->iArchiveAfterMs);
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHot, TRUE);
	xvoUnref(vHot);
	vHot = NULL;
	xvoTableSetValue(vRoot, "warm_items", 0u, vWarm, TRUE);
	xvoUnref(vWarm);
	vWarm = NULL;
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
	if ( vWarm != NULL ) xvoUnref(vWarm);
	if ( vHot != NULL ) xvoUnref(vHot);
	if ( vRoot != NULL ) xvoUnref(vRoot);
	return bOk;
}

static void procAppendRestoreLocked(DemoTierCenter* pCenter, const char* sKey, const char* sTitle, uint64 iKeyHash, int iHeat, xtime tPrevStoredAt, const char* sFromTier, const char* sReason)
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

static void procAppendArchiveLocked(DemoTierCenter* pCenter, const DemoColdRecord* pRecord, const char* sReason)
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

static bool procCenterInit(DemoTierCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepRestore = 8u;
	pCenter->iKeepArchive = 8u;
	pCenter->iArchiveAfterMs = 180u;

	pCenter->pHot = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pWarm = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pCenter->pCold = xrtListCreate(sizeof(DemoColdRecord), XRT_OBJMODE_SHARED);
	pCenter->pRestore = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->pArchive = xrtListCreate(sizeof(DemoStateEvent), XRT_OBJMODE_SHARED);
	pCenter->hMutex = xrtMutexCreate();
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	sSnapshotPath = xrtPathJoin(2, "runtime", "multi-tier-storage.json");

	if ( (pCenter->pHot == NULL) || (pCenter->pWarm == NULL) || (pCenter->pCold == NULL) || (pCenter->pRestore == NULL) || (pCenter->pArchive == NULL) || (pCenter->hMutex == NULL) || (pCenter->hQueue == NULL) || (sSnapshotPath == NULL) ) {
		if ( sSnapshotPath != NULL ) xrtFree(sSnapshotPath);
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoTierCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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

static bool procMoveHotToWarmLocked(DemoTierCenter* pCenter, const char* sKey, const char* sTierReason)
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
	procCopyText(pWarm->sTierReason, sizeof(pWarm->sTierReason), sTierReason);

	if ( !xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL) ) {
		xrtFree(pWarm);
		(void)xrtDictSetPtr(pCenter->pHot, (ptr)pHot->sKey, (uint32)strlen(pHot->sKey), pHot, NULL);
		return FALSE;
	}

	xrtFree(pHot);
	return TRUE;
}

static bool procFlushWarmToColdLocked(DemoTierCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoWarmItem* pWarm = NULL;
	DemoColdRecord* pCold = NULL;
	bool bNew = FALSE;
	str sColdPath = NULL;

	pWarm = (DemoWarmItem*)xrtDictRemovePtr(pCenter->pWarm, (ptr)sKey, (uint32)strlen(sKey));
	if ( pWarm == NULL ) {
		return FALSE;
	}

	sColdPath = procBuildColdPath(pWarm->iKeyHash);
	if ( (sColdPath == NULL) || !procWriteColdFile(pWarm, sColdPath) ) {
		if ( sColdPath != NULL ) xrtFree(sColdPath);
		(void)xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL);
		return FALSE;
	}

	pCenter->iNextColdSeq++;
	pCold = (DemoColdRecord*)xrtListSet(pCenter->pCold, pCenter->iNextColdSeq, &bNew);
	if ( pCold == NULL ) {
		(void)xrtFileDelete(sColdPath);
		xrtFree(sColdPath);
		(void)xrtDictSetPtr(pCenter->pWarm, (ptr)pWarm->sKey, (uint32)strlen(pWarm->sKey), pWarm, NULL);
		return FALSE;
	}

	memset(pCold, 0, sizeof(*pCold));
	pCold->tTouchedAt = pWarm->tTouchedAt;
	pCold->tWarmedAt = pWarm->tWarmedAt;
	pCold->tFlushedAt = xrtNow();
	pCold->iKeyHash = pWarm->iKeyHash;
	pCold->iHeat = pWarm->iHeat;
	procCopyText(pCold->sKey, sizeof(pCold->sKey), pWarm->sKey);
	procCopyText(pCold->sTitle, sizeof(pCold->sTitle), pWarm->sTitle);
	procCopyText(pCold->sTierReason, sizeof(pCold->sTierReason), sReason);
	procCopyText(pCold->sColdPath, sizeof(pCold->sColdPath), sColdPath);

	xrtFree(sColdPath);
	xrtFree(pWarm);
	return TRUE;
}

static bool procRestoreWarmToHotLocked(DemoTierCenter* pCenter, const char* sKey, const char* sReason)
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

static bool procRestoreColdToHotLocked(DemoTierCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoColdLookup tLookup;
	DemoColdRecord* pCold = NULL;
	DemoHotItem* pHot = NULL;
	str sJson = NULL;
	xvalue vRoot = NULL;
	size_t iJsonSize = 0u;
	const char* sTitle = NULL;
	const char* sKeyText = NULL;
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

static bool procSubmitJob(DemoTierCenter* pCenter, int iKind, const char* sKey, const char* sText)
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

static void procSweepColdLocked(DemoTierCenter* pCenter, const char* sReason)
{
	int64 iSeq;
	DemoColdRecord* pCold = NULL;
	xtime tNow = xrtNow();

	for ( iSeq = 1; iSeq <= pCenter->iNextColdSeq; iSeq++ ) {
		pCold = (DemoColdRecord*)xrtListGet(pCenter->pCold, iSeq);
		if ( pCold == NULL ) {
			continue;
		}
		if ( (tNow - pCold->tFlushedAt) < (xtime)pCenter->iArchiveAfterMs ) {
			continue;
		}

		procAppendArchiveLocked(pCenter, pCold, sReason);
		if ( pCold->sColdPath[0] != '\0' ) {
			(void)xrtFileDelete(pCold->sColdPath);
		}
		(void)xrtListRemove(pCenter->pCold, iSeq);
	}
}

static uint32 procTierWorker(ptr pArg)
{
	DemoTierCenter* pCenter = (DemoTierCenter*)pArg;
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
		} else if ( pJob->iKind == DEMO_JOB_FLUSH_COLD ) {
			(void)procFlushWarmToColdLocked(pCenter, pJob->sKey, pJob->sText);
		} else if ( pJob->iKind == DEMO_JOB_ACCESS ) {
			if ( !procRestoreWarmToHotLocked(pCenter, pJob->sKey, pJob->sText) ) {
				(void)procRestoreColdToHotLocked(pCenter, pJob->sKey, pJob->sText);
			}
		} else if ( pJob->iKind == DEMO_JOB_SWEEP ) {
			procSweepColdLocked(pCenter, pJob->sText);
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

static void procCenterUnit(DemoTierCenter* pCenter)
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
		if ( pCenter->pWarm != NULL ) {
			xrtDictWalk(pCenter->pWarm, procFreeHeapItem, NULL);
		}
		xrtMutexUnlock(pCenter->hMutex);
	}
	if ( pCenter->hMutex != NULL ) xrtMutexDestroy(pCenter->hMutex);
	if ( pCenter->pArchive != NULL ) xrtListDestroy(pCenter->pArchive);
	if ( pCenter->pRestore != NULL ) xrtListDestroy(pCenter->pRestore);
	if ( pCenter->pCold != NULL ) xrtListDestroy(pCenter->pCold);
	if ( pCenter->pWarm != NULL ) xrtDictDestroy(pCenter->pWarm);
	if ( pCenter->pHot != NULL ) xrtDictDestroy(pCenter->pHot);
}

int main(void)
{
	DemoTierCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) return 1;
	if ( !procCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procTierWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 5) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 4) ) goto cleanup;
	if ( !procRegisterHot(&tCenter, "node:delta", "Delta Backfill", 2) ) goto cleanup;

	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:beta", "evict-to-warm") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:gamma", "evict-to-warm") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_TO_WARM, "node:delta", "evict-to-warm") ) goto cleanup;
	xrtSleep(160u);

	if ( !procSubmitJob(&tCenter, DEMO_JOB_FLUSH_COLD, "node:gamma", "persist-cold-snapshot") ) goto cleanup;
	if ( !procSubmitJob(&tCenter, DEMO_JOB_FLUSH_COLD, "node:delta", "persist-cold-snapshot") ) goto cleanup;
	xrtSleep(180u);

	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:beta", "ui-open") ) goto cleanup;
	xrtSleep(120u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_ACCESS, "node:gamma", "cold-file-restore") ) goto cleanup;
	xrtSleep(220u);
	if ( !procSubmitJob(&tCenter, DEMO_JOB_SWEEP, NULL, "archive-cold-snapshot") ) goto cleanup;

	xrtSleep(320u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);
	printf("== warm ==\n");
	xrtDictWalk(tCenter.pWarm, procDumpWarm, NULL);
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

### 6.1 warm 和 cold 不是一回事

这页里最重要的边界之一就是：

- warm 仍然是内存态
- cold 已经是正式文件态

所以恢复路径也必须区分成：

- warm restore
- cold file restore


### 6.2 cold restore 不能跳过正式解析边界

这页故意把 cold restore 写成：

- `xrtFileReadAll()`
- `xrtParseJSON()`
- `xvoTableGetText() / xvoTableGetInt()`

因为真正的多层存储问题，不只是“找到了路径”，而是：

- 你能不能稳定地从 cold file 恢复出正式对象


### 6.3 archive 层不该继续当实时恢复来源

这页里一旦 `SWEEP` 触发：

- cold record 会进入 archive history
- 对应 cold file 会被删除

这样 archive 层就明确变成：

- 历史摘要层


### 6.4 页面、脚本和 worker 应该共享同一份多层摘要

`procWriteDashboardSnapshotLocked()` 会统一导出：

- hot
- warm
- cold
- restore history
- archive history


### 6.5 真正的恢复顺序应该是 warm 优先，cold 兜底

这页在 `ACCESS` 里故意先尝试：

1. `procRestoreWarmToHotLocked()`
2. 失败后再走 `procRestoreColdToHotLocked()`

因为这正是多层存储里最常见的真实顺序：

- 先走便宜的内存层
- 再走昂贵的磁盘层


### 6.6 多层存储的难点不是“多几个 struct”，而是迁移边界

这一页真正要稳定下来的，不是字段数量，而是：

- hot -> warm
- warm -> cold file
- warm / cold -> hot restore
- cold -> archive


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先改模型，再决定是否复用这套骨架：

- 如果 cold 层已经变成真正的大批量慢 I/O，应该继续拆 `file_async`
- 如果 archive 层已经需要外部工具或压缩管线，应该继续补 `subprocess`
- 如果你真正要做的是更多中间层，而不是 warm / cold / archive 这 3 类职责，就该进入“冷热多级老化”那条下一页主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 hot / warm / cold / restore / archive 摘要
2. `POST /api/to-warm`
	- 只提交 `TO_WARM` signal
3. `POST /api/flush-cold`
	- 只提交 `FLUSH_COLD` signal
4. `POST /api/access`
	- 只提交恢复请求，让 worker 决定走 warm restore 还是 cold restore
5. `POST /api/sweep`
	- 手工触发或定时触发 archive sweep
6. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染所有层级和历史线


## 9. 下一步阅读

如果你准备继续把层级状态做得更细，最顺的下一步是：

1. [冷热多级老化](hot-cold-multi-level-aging-dashboard.md)
	把“热 -> warm -> cool -> cold -> archive”继续拆成更多正式层级和老化窗口
2. [把本地控制台服务升级成一个恢复优先级面板](recovery-priority-dashboard.md)
	对比“多层介质恢复来源”跟“正式恢复优先级策略”到底差在哪
3. [把本地控制台服务升级成一个自动回温 + 滚动归档联动面板](auto-warm-rolling-archive-dashboard.md)
	对比“多层介质与恢复路径”跟“单条 cold state 的联动收口”到底差在哪
4. [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)
	对比“统一 cold 分流”和“真正跨介质多层存储”的边界区别
