# 把本地控制台服务升级成一个冷热滚动归档面板

> 这个案例要解决的不是“冷层满了以后顺手删几条旧记录”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 hot tier、cold tier 和 warm-back 语义之后，又开始需要把长期没回温、已经超过 cold window 的对象继续滚进 archive 层，还要把当前热层、当前冷层、最近 archive 历史和快照稳定交给页面、JSON、日志和脚本时，怎样把 `dict + list + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让冷层裁剪、归档落盘和页面展示各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面的几条业务主线：

- 当前热点对象在热层
- 已降温对象会进入冷层
- 某些冷层对象还能回温回热层
- 页面和 JSON 已经能看到冷热两层

现在系统又往前走一步，开始出现一个更长生命周期的问题：

- 有些冷层对象很久都没有再被访问
- 它们继续留在 cold window 里已经不划算
- 但它们也不应该直接消失
- 页面和脚本还需要看到“最近滚出冷层并正式归档了什么”
- 最新冷热与 archive 状态还要继续落成快照文件

如果这时只写成：

- 冷层超过窗口就 `Remove`
- 顺手写一行日志

很快就会出现 4 个问题：

- cold window 裁剪和 archive 语义混在一起，没有正式边界
- 页面为了显示“最近归档”开始扫日志或看 worker 临时状态
- 冷层对象是“被删了”还是“被正式归档了”说不清楚
- 后面再补自动回温策略或 3 层存储时，状态模型会立刻失真

所以这页真正要补出的，不是“怎么把冷层删掉几条”，而是：

> 当系统已经有了 hot/cold 两层后，怎样把 cold overflow 继续滚进 archive 层，并给它一份稳定可导出的状态模型。


## 2. 为什么这次不是“冷层超窗就直接删除”

### 2.1 冷层超窗意味着状态继续迁移，不意味着记录作废

当一个对象从 cold tier 继续滚进 archive tier，真正发生的是：

- 它不再属于当前冷层窗口
- 但它依然是正式历史记录
- 页面和快照还要能说明它是什么时候被滚动归档的

所以这里不是：

- 简单删掉一条冷层记录

而是：

- 一次正式的冷层到 archive 层迁移


### 2.2 当前冷层窗口和长期归档历史不是同一种数据

cold window 更适合回答：

- 当前还有哪些冷层对象仍在窗口里
- 哪些对象还可能被回温

archive history 更适合回答：

- 哪些对象已经正式滚出当前窗口
- 它们最后一次冷态是什么
- 页面和脚本最近应该展示哪些稳定历史

这两层关注的问题不是一回事，所以不该继续塞在同一条 `list` 里。


### 2.3 删除只回答“现在没有了”，archive 回答“曾经发生过什么”

如果对象只是从 cold layer 里直接删掉，系统最后只能回答：

- 它现在不在 cold window 里了

但真正还需要回答的是：

- 它是什么 key
- 为什么最初进入冷层
- 它何时被滚出当前窗口
- 它在 archive 层里该如何稳定展示

所以这页里必须再补出一条正式的：

- archive 历史线


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存当前热层对象 | 当前热点查询与快速命中 |
| `list` | 保存当前冷层窗口 | 当前仍可回温的冷对象 |
| `list` | 保存最近 archive 历史 | 页面时间线与稳定归档导出 |
| `queue + thread` | 后台执行降温与滚动归档 | 请求线程不阻塞在层级迁移和落盘上 |
| `time` | 记录触达、降温、滚动归档时间 | 三层状态的正式时间边界 |
| `hash` | 给对象 key 一个稳定指纹 | 页面、日志、快照里的轻量标识 |
| `path + file` | 组织滚动快照路径并落盘 | 让冷热与 archive 状态进入文件系统 |
| `xvalue + json` | 导出当前热层、冷层和 archive 摘要 | 页面、脚本、快照共享同一份模型 |
| `xhttpd + template` | 展示三层状态 | 浏览器和脚本共享一个入口 |

一句话记住：

> `dict` 管“现在还热着什么”，一条 `list` 管“当前冷层窗口里还有什么”，另一条 `list` 管“最近正式滚进 archive 了什么”，worker 管“什么时候把 cold overflow 继续迁进 archive 层”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成滚动归档面板语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/rolling-tier-archive.json
```

其中：

- `config/console.json`
	- 保存 cold window 大小、archive 历史保留条数和队列容量
- `web/dashboard.html`
	- 同时展示当前热层、当前冷层和最近 archive 历史
- `runtime/console.log`
	- 记录降温提交和滚动归档完成
- `runtime/rolling-tier-archive.json`
	- 保存最近一次三层摘要


## 5. 先把热层、冷层和滚动归档通道立起来

下面这段代码故意先聚焦 rolling archive 核心，不把完整 `xhttpd` handler 和模板渲染全部塞进去。

它保留最关键的 5 件事：

1. `dict` 保存当前热层对象
2. `list` 保存当前冷层窗口
3. `list` 保存最近 archive 历史
4. `queue + thread` 把降温和 cold overflow 滚动放到后台
5. `path + file + json` 把当前三层摘要落成 `runtime/rolling-tier-archive.json`

这个骨架会实际做这些事：

- 初始化 rolling center
- 注册 4 个热层对象
- 连续提交 3 次降温请求
- worker 把对象从热层迁入冷层
- 当第三次降温让 cold window 超过上限时，把最旧冷层记录继续滚进 archive
- 主线程打印热层、冷层、archive 历史和最终快照

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
	xtime tTouchedAt;
	xtime tCooledAt;
	xtime tArchivedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
	char sTierReason[48];
	char sArchiveReason[48];
} DemoArchiveEntry;

typedef struct
{
	char sKey[64];
	char sTierReason[48];
} DemoTierJob;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	int64 iNextArchiveSeq;
	uint32 iKeepCold;
	uint32 iKeepArchive;
	char sSnapshotPath[260];
} DemoRollingCenter;


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

static bool procCollectArchiveSnapshot(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue vArchiveArr = (xvalue)pArg;
	DemoArchiveEntry* pEntry = (DemoArchiveEntry*)pVal;
	xvalue vRow;
	(void)iKey;

	if ( (vArchiveArr == NULL) || (pEntry == NULL) ) {
		return FALSE;
	}

	vRow = xvoCreateTable();
	xvoTableSetText(vRow, "key", 0u, pEntry->sKey, 0u, FALSE);
	xvoTableSetText(vRow, "title", 0u, pEntry->sTitle, 0u, FALSE);
	xvoTableSetText(vRow, "tier_reason", 0u, pEntry->sTierReason, 0u, FALSE);
	xvoTableSetText(vRow, "archive_reason", 0u, pEntry->sArchiveReason, 0u, FALSE);
	xvoTableSetInt(vRow, "heat", 0u, pEntry->iHeat);
	xvoTableSetInt(vRow, "touched_at", 0u, pEntry->tTouchedAt);
	xvoTableSetInt(vRow, "cooled_at", 0u, pEntry->tCooledAt);
	xvoTableSetInt(vRow, "archived_at", 0u, pEntry->tArchivedAt);
	xvoTableSetInt(vRow, "key_hash", 0u, (int64)pEntry->iKeyHash);
	(void)xvoArrayAppendValue(vArchiveArr, vRow, TRUE);
	xvoUnref(vRow);
	return FALSE;
}

static bool procWriteSnapshotLocked(DemoRollingCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHotArr = NULL;
	xvalue vColdArr = NULL;
	xvalue vArchiveArr = NULL;
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
	vArchiveArr = xvoCreateArray();

	xrtDictWalk(pCenter->pHot, procCollectHotSnapshot, vHotArr);
	xrtListWalk(pCenter->pCold, procCollectColdSnapshot, vColdArr);
	xrtListWalk(pCenter->pArchive, procCollectArchiveSnapshot, vArchiveArr);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "archive_count", 0u, xrtListCount(pCenter->pArchive));
	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHotArr, TRUE);
	xvoUnref(vHotArr);
	vHotArr = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vColdArr, TRUE);
	xvoUnref(vColdArr);
	vColdArr = NULL;
	xvoTableSetValue(vRoot, "archive_items", 0u, vArchiveArr, TRUE);
	xvoUnref(vArchiveArr);
	vArchiveArr = NULL;

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
	if ( vArchiveArr != NULL ) {
		xvoUnref(vArchiveArr);
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

static bool procAppendArchiveLocked(DemoRollingCenter* pCenter, const DemoColdEntry* pCold, const char* sArchiveReason)
{
	DemoArchiveEntry* pEntry = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pCold == NULL) ) {
		return FALSE;
	}

	pCenter->iNextArchiveSeq++;
	iSeq = pCenter->iNextArchiveSeq;
	pEntry = (DemoArchiveEntry*)xrtListSet(pCenter->pArchive, iSeq, &bNew);
	if ( pEntry == NULL ) {
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pCold->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pCold->sTitle);
	procCopyText(pEntry->sTierReason, sizeof(pEntry->sTierReason), pCold->sTierReason);
	procCopyText(pEntry->sArchiveReason, sizeof(pEntry->sArchiveReason), sArchiveReason);
	pEntry->iKeyHash = pCold->iKeyHash;
	pEntry->iHeat = pCold->iHeat;
	pEntry->tTouchedAt = pCold->tTouchedAt;
	pEntry->tCooledAt = pCold->tCooledAt;
	pEntry->tArchivedAt = xrtNow();

	if ( iSeq > (int64)pCenter->iKeepArchive ) {
		(void)xrtListRemove(pCenter->pArchive, iSeq - (int64)pCenter->iKeepArchive);
	}

	return TRUE;
}

static bool procAppendColdLocked(DemoRollingCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
{
	DemoColdEntry* pEntry = NULL;
	DemoColdEntry* pOverflow = NULL;
	bool bNew = FALSE;
	int64 iSeq;
	int64 iOverflowSeq;

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
		iOverflowSeq = iSeq - (int64)pCenter->iKeepCold;
		pOverflow = (DemoColdEntry*)xrtListGet(pCenter->pCold, iOverflowSeq);
		if ( pOverflow != NULL ) {
			(void)procAppendArchiveLocked(pCenter, pOverflow, "cold-window-roll");
			(void)xrtListRemove(pCenter->pCold, iOverflowSeq);
		}
	}

	return TRUE;
}

static bool procRollingCenterInit(DemoRollingCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 2u;
	pCenter->iKeepArchive = 8u;

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

	pCenter->pArchive = xrtListCreate(sizeof(DemoArchiveEntry), XRT_OBJMODE_SHARED);
	if ( pCenter->pArchive == NULL ) {
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pArchive);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pArchive = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pArchive);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hMutex = NULL;
		pCenter->pArchive = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	sSnapshotPath = xrtPathJoin(2, "runtime", "rolling-tier-archive.json");
	if ( sSnapshotPath == NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pArchive);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hQueue = NULL;
		pCenter->hMutex = NULL;
		pCenter->pArchive = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoRollingCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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

static bool procSubmitCool(DemoRollingCenter* pCenter, const char* sKey, const char* sTierReason)
{
	DemoTierJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoTierJob*)xrtMalloc(sizeof(DemoTierJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	procCopyText(pJob->sTierReason, sizeof(pJob->sTierReason), sTierReason);

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procRollingWorker(ptr pArg)
{
	DemoRollingCenter* pCenter = (DemoRollingCenter*)pArg;
	DemoTierJob* pJob = NULL;
	DemoHotItem* pItem = NULL;
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
		pItem = (DemoHotItem*)xrtDictRemovePtr(pCenter->pHot, (ptr)pJob->sKey, (uint32)strlen(pJob->sKey));
		if ( pItem != NULL ) {
			(void)procAppendColdLocked(pCenter, pItem, pJob->sTierReason);
			(void)procWriteSnapshotLocked(pCenter);
		}
		xrtMutexUnlock(pCenter->hMutex);

		if ( pItem != NULL ) {
			xrtFree(pItem);
			pItem = NULL;
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

static bool procDumpArchive(int64 iKey, ptr pVal, ptr pArg)
{
	DemoArchiveEntry* pEntry = (DemoArchiveEntry*)pVal;
	(void)pArg;

	printf(
		"archive seq=%lld key=%s title=%s tier_reason=%s archive_reason=%s archived_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->sTierReason,
		pEntry->sArchiveReason,
		(long long)pEntry->tArchivedAt,
		(unsigned long long)pEntry->iKeyHash
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

static void procRollingCenterUnit(DemoRollingCenter* pCenter)
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
	if ( pCenter->pArchive != NULL ) {
		xrtListDestroy(pCenter->pArchive);
		pCenter->pArchive = NULL;
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
	DemoRollingCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procRollingCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procRollingWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procRollingCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterHot(&tCenter, "node:alpha", "Alpha Feed", 10) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:beta", "Beta Digest", 8) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:gamma", "Gamma Report", 5) ) {
		goto cleanup;
	}
	if ( !procRegisterHot(&tCenter, "node:delta", "Delta Board", 3) ) {
		goto cleanup;
	}

	if ( !procSubmitCool(&tCenter, "node:beta", "idle-window") ) {
		goto cleanup;
	}
	if ( !procSubmitCool(&tCenter, "node:gamma", "cooldown-batch") ) {
		goto cleanup;
	}
	if ( !procSubmitCool(&tCenter, "node:delta", "aging-roll") ) {
		goto cleanup;
	}

	xrtSleep(400u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);

	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);

	printf("== archive ==\n");
	xrtListWalk(tCenter.pArchive, procDumpArchive, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
		sSnapshot = NULL;
	}

	iRet = 0;

cleanup:
	procRollingCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 6 个点

### 6.1 cold window 和 archive history 必须拆成两条 `list`

这页里除了：

- `pHot`
- `pCold`

还新增了一条：

- `pArchive`

这意味着“冷层超窗”不再只是 cold 里少一条记录，而是：

- 一次正式 cold -> archive 迁移


### 6.2 cold overflow 的收口必须走正式迁移，不是静默裁剪

`procAppendColdLocked()` 里最关键的边界是：

- 先把对象转成冷层记录
- 再判断是否超过 `keepCold`
- 如果超过，就先复制成 archive 记录
- 最后才从 cold 里移除最旧条目

这样页面和快照才能说清楚：

- 哪一条是当前冷层
- 哪一条已经正式滚进 archive


### 6.3 请求线程只负责投递 cool request，不负责层级滚动

这页最重要的边界之一就是：

- 请求线程只投递降温请求
- worker 真正执行 hot -> cold，以及必要时 cold -> archive

这样后面即使滚动逻辑继续长大，请求线程也不会马上退化。


### 6.4 `time + path + file` 让滚动归档成为正式系统状态

`tCooledAt` 和 `tArchivedAt` 让三层状态的时间边界正式可见；  
`procWriteSnapshotLocked()` 会把：

- 当前热层
- 当前冷层
- 最近 archive 历史

统一落成 `runtime/rolling-tier-archive.json`。  
这意味着滚动归档不再只是内存中的副作用，而是一份可以给页面、脚本和导出复用的正式状态。


### 6.5 这页故意先不补自动回温策略

这页先讲的是：

- cold overflow 如何稳定迁进 archive

而不是：

- 命中多少次自动回温
- 归档后什么情况下自动回拉

因为如果先不把 3 层边界讲稳，自动策略只会继续把状态写乱。


### 6.6 `hash` 仍然只是稳定指纹，不是安全能力

`iKeyHash` 在这里承担的是：

- 页面里的轻量标识
- 日志和快照里的稳定短指纹

它不是认证 token，也不该被当成安全字段。


## 7. 接回 HTTP、模板和脚本时怎么落

### 7.1 `POST /api/cool`

只做：

1. 解析对象 key
2. 生成降温请求
3. 推入后台队列
4. 立即返回已接收

不要在 handler 里直接裁剪 cold window 或写 archive。


### 7.2 `GET /api/dashboard`

最自然的 JSON 输出就是 3 块：

1. 当前热层摘要
2. 当前冷层摘要
3. 最近 archive 历史


### 7.3 `GET /dashboard`

页面最自然的布局就是：

1. 热层表
2. 当前冷层窗口
3. 最近 archive 时间线

这样模板页不需要再从日志里反推哪些对象已经滚出 cold window。


### 7.4 `runtime/rolling-tier-archive.json`

运维脚本、CLI 工具和页面导出都应该优先消费这份快照，而不是直接读 worker 临时状态。


## 8. 这页最容易写错的地方

### 8.1 冷层超窗后直接 `Remove`

这会让 archive 状态彻底失踪，页面和快照最后只能知道：

- 这条冷层记录不见了

却不知道：

- 它到底是不是已经正式归档


### 8.2 用 archive 去代替当前 cold window

cold 负责“当前仍可能回温的窗口”，archive 负责“已经滚出当前窗口的稳定历史”。  
把两者混在一起，后面 warm-back 和多层存储都会写乱。


### 8.3 在 cold window 很大时继续无脑线性读写

这页为了教学清晰，仍然保留：

- 当前冷层窗口用 `list`
- 通过固定序号做 overflow 判断

如果 cold tier 后面继续长大，就该升级结构，而不是假装当前实现还能一直撑住。


## 9. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 自动回温策略
	把“命中阈值”“时间阈值”补成正式策略，可以继续读 [把本地控制台服务升级成一个自动回温策略面板](auto-warm-policy-dashboard.md)
2. 多层存储
	把热层、冷层、archive 层继续拆成更完整的多层结构
3. 冷层回温 + 归档协同
	把“回温失败继续留 cold”“长期无命中继续 archive”补成完整策略页，可以继续读 [把本地控制台服务升级成一个冷层回温归档协同面板](warm-archive-coordination-dashboard.md)

如果你还没完全建立这里的层级拆分心智，建议一起复读：

- [把本地控制台服务升级成一个冷热分层面板](hot-cold-tier-dashboard.md)
- [把本地控制台服务升级成一个冷层回温面板](warm-back-dashboard.md)
- [把本地控制台服务升级成一个归档面板](archive-dashboard.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
