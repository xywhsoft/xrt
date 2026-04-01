# 把本地控制台服务升级成一个冷热分层面板

> 这个案例要解决的不是“活跃对象结束后怎么归档”，而是另一个更常见、也更容易被写乱的问题：当一个本地控制台服务既要快速回答“现在最热的对象有哪些”，又要稳定保存“已经降温但仍然有效的数据快照”，还要把冷层状态落成文件给页面、JSON 和运维脚本复用时，怎样把 `dict + list + queue + thread + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是让热层、冷层、文件快照和页面各自维护一套半同步状态。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面那个本地控制台服务，也已经补出了归档面板：

- 当前活跃对象会放在内存里
- 最近历史会展示给页面
- 后台 worker 会负责慢操作
- 快照文件会落到 `runtime/`

但系统继续往前走，很快又会遇到另一个问题：

- 有些对象并没有“结束”
- 它们只是“不再值得留在热层”
- 页面仍然要能看到它们
- JSON 和运维脚本仍然要能拿到它们
- 只是查询优先级、更新频率和存储位置已经应该和热层分开

这时如果继续把所有对象都塞在一张热表里，就会出现 4 个典型后果：

- 热层越来越大，页面和接口都在为冷数据付费
- “当前活跃”和“最近可回看”没有正式边界
- 文件快照开始直接扫内存热表，冷层没有独立模型
- 后面一旦要补“回温”“冷层滚动”“多层存储”，所有逻辑都得推倒重来

所以这页真正要补出的不是“怎么把对象写进文件”，而是：

> 当系统已经需要正式区分 hot tier 和 cold tier 时，怎样把热层、冷层、降温通道和冷层快照拆成 4 条稳定主线。


## 2. 为什么归档面板还不够

### 2.1 归档意味着结束，冷层意味着还有效

归档面板里，一个对象进入 archive 往往意味着：

- 它已经结束
- 它只剩历史意义

但冷热分层不是这个语义。

冷层里的对象更像：

- 现在不该继续占热层位置
- 但仍然是有效记录
- 后面甚至可能被重新拉回热层

所以“归档”和“降温”不能继续写成同一个状态。


### 2.2 热层关注实时命中，冷层关注稳定快照

热层最适合回答：

- 这个 key 现在是不是热点
- 当前最新状态是什么
- 页面头部应该展示什么

冷层更适合回答：

- 最近有哪些对象已经降温
- 这些对象最后一次热态是什么
- 当前冷层快照应该落成什么文件

它们关注的问题不是一回事，所以也不该继续共用一个数据结构。


### 2.3 冷层不是日志，更不是临时复制

一旦页面、JSON 和文件快照都开始消费冷层，冷层就应该成为：

- 一份正式的稳定值记录

而不是：

- 从日志里反查
- 从 worker 临时内存里抄一份
- 从热层对象指针里“顺便看一下”

这正是 `list + path + file` 应该正式接入主线的地方。


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存当前热层对象 | 当前热点查询与快速命中 |
| `list` | 保存最近冷层记录 | 最近降温时间线与冷层导出 |
| `queue + thread` | 后台执行降温 | 请求线程不阻塞在层级迁移和落盘上 |
| `time` | 记录最近热触达与降温时间 | 热层和冷层的时间边界 |
| `hash` | 给对象 key 一个稳定指纹 | 页面、日志、快照里的轻量标识 |
| `path + file` | 组织冷层快照路径并落盘 | 让冷层真正进入文件系统 |
| `xvalue + json` | 导出当前冷热摘要 | 页面、脚本、快照共享同一份模型 |
| `xhttpd + template` | 展示热层与冷层 | 浏览器和脚本共享一个入口 |

一句话记住：

> `dict` 管“现在最热的对象”，`list` 管“最近已经降温的对象”，worker 管“什么时候把热层对象正式迁到冷层并落成快照”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页只把输出改成冷热分层语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/hot-cold.json
```

其中：

- `config/console.json`
	- 保存热层容量、冷层保留条数、降温阈值等配置
- `web/dashboard.html`
	- 同时展示当前热层对象和最近冷层对象
- `runtime/console.log`
	- 记录降温请求和快照写出
- `runtime/hot-cold.json`
	- 保存最近一次冷热分层摘要


## 5. 先把热层、冷层和降温通道立起来

下面这段代码故意先聚焦冷热分层核心，不把完整 `xhttpd` handler 和模板渲染全部塞进去。

它保留最关键的 4 件事：

1. `dict` 保存当前热层对象
2. `list` 保存最近冷层记录
3. `queue + thread` 把降温动作放到后台
4. `path + file + json` 把冷层摘要落成 `runtime/hot-cold.json`

这个骨架会实际做这些事：

- 初始化 hot-cold center
- 注册几个热层对象
- 投递两个降温请求
- worker 把对象从热层迁到冷层
- worker 每次迁移后重写冷层快照文件
- 主线程打印当前热层、最近冷层和最终快照内容

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
	char sKey[64];
	char sTierReason[48];
} DemoTierJob;

typedef struct
{
	xdict pHot;
	xlist pCold;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextHotId;
	int64 iNextColdSeq;
	uint32 iKeepCold;
	char sSnapshotPath[260];
} DemoHotColdCenter;


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

static bool procWriteSnapshotLocked(DemoHotColdCenter* pCenter)
{
	xvalue vRoot = NULL;
	xvalue vHotArr = NULL;
	xvalue vColdArr = NULL;
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

	xrtDictWalk(pCenter->pHot, procCollectHotSnapshot, vHotArr);
	xrtListWalk(pCenter->pCold, procCollectColdSnapshot, vColdArr);

	xvoTableSetInt(vRoot, "hot_count", 0u, xrtDictCount(pCenter->pHot));
	xvoTableSetInt(vRoot, "cold_count", 0u, xrtListCount(pCenter->pCold));
	xvoTableSetInt(vRoot, "generated_at", 0u, xrtNow());
	xvoTableSetText(vRoot, "snapshot_path", 0u, pCenter->sSnapshotPath, 0u, FALSE);
	xvoTableSetValue(vRoot, "hot_items", 0u, vHotArr, TRUE);
	xvoUnref(vHotArr);
	vHotArr = NULL;
	xvoTableSetValue(vRoot, "cold_items", 0u, vColdArr, TRUE);
	xvoUnref(vColdArr);
	vColdArr = NULL;

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

static bool procAppendColdLocked(DemoHotColdCenter* pCenter, DemoHotItem* pItem, const char* sTierReason)
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

static bool procHotColdCenterInit(DemoHotColdCenter* pCenter)
{
	xqueue_config tCfg;
	str sSnapshotPath = NULL;

	if ( pCenter == NULL ) {
		return FALSE;
	}

	memset(pCenter, 0, sizeof(*pCenter));
	pCenter->iKeepCold = 8u;

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

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hMutex = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	sSnapshotPath = xrtPathJoin(2, "runtime", "hot-cold.json");
	if ( sSnapshotPath == NULL ) {
		xrtMPSCQWaitDestroy(pCenter->hQueue);
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pCold);
		xrtDictDestroy(pCenter->pHot);
		pCenter->hQueue = NULL;
		pCenter->hMutex = NULL;
		pCenter->pCold = NULL;
		pCenter->pHot = NULL;
		return FALSE;
	}

	procCopyText(pCenter->sSnapshotPath, sizeof(pCenter->sSnapshotPath), sSnapshotPath);
	xrtFree(sSnapshotPath);
	return TRUE;
}

static bool procRegisterHot(DemoHotColdCenter* pCenter, const char* sKey, const char* sTitle, int iHeat)
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
	xrtMutexLock(pCenter->hMutex);
	pCenter->iNextHotId++;
	pItem->iHotId = pCenter->iNextHotId;
	pItem->tTouchedAt = xrtNow();
	pItem->iKeyHash = xrtHash64((ptr)sKey, strlen(sKey));
	pItem->iHeat = iHeat;
	procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);
	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);

	if ( !xrtDictSetPtr(pCenter->pHot, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
		xrtMutexUnlock(pCenter->hMutex);
		xrtFree(pItem);
		return FALSE;
	}
	xrtMutexUnlock(pCenter->hMutex);
	return TRUE;
}

static bool procSubmitCool(DemoHotColdCenter* pCenter, const char* sKey, const char* sTierReason)
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

static uint32 procTierWorker(ptr pArg)
{
	DemoHotColdCenter* pCenter = (DemoHotColdCenter*)pArg;
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

static void procHotColdCenterUnit(DemoHotColdCenter* pCenter)
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
	DemoHotColdCenter tCenter;
	str sSnapshot = NULL;
	size_t iSnapshotSize = 0u;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procHotColdCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procTierWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procHotColdCenterUnit(&tCenter);
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

	if ( !procSubmitCool(&tCenter, "node:beta", "idle-window") ) {
		goto cleanup;
	}
	if ( !procSubmitCool(&tCenter, "node:gamma", "cooldown-batch") ) {
		goto cleanup;
	}

	xrtSleep(300u);

	xrtMutexLock(tCenter.hMutex);
	printf("== hot ==\n");
	xrtDictWalk(tCenter.pHot, procDumpHot, NULL);

	printf("== cold ==\n");
	xrtListWalk(tCenter.pCold, procDumpCold, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	sSnapshot = xrtFileReadAll(tCenter.sSnapshotPath, XRT_CP_AUTO, &iSnapshotSize);
	if ( sSnapshot != NULL ) {
		printf("== snapshot ==\n%.*s\n", (int)iSnapshotSize, sSnapshot);
		xrtFree(sSnapshot);
		sSnapshot = NULL;
	}

	iRet = 0;

cleanup:
	procHotColdCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 6 个点

### 6.1 热层和冷层不是“当前态 + 历史态”的复制关系

这页里：

- `pHot`
	- 保存当前热层对象指针
- `pCold`
	- 保存已经降温后的稳定值记录

也就是说，冷层不是再指回热对象，而是已经变成：

- 可直接导出
- 可直接展示
- 可直接落盘

的一份正式模型。


### 6.2 冷层不是 archive

`procAppendColdLocked()` 做的不是“对象结束”，而是：

- 记录它最后一次热态
- 记录为什么降温
- 把它从热层迁到冷层

后面如果要加“回温”，就应该从冷层再升回热层，而不是去 archive 里找。


### 6.3 worker 负责分层，handler 只负责投递

这一页最重要的边界之一就是：

- HTTP handler 只提交 cool request
- worker 真正执行热层到冷层的迁移

这样后面不管你往迁移里再补：

- 文件落盘
- 冷层滚动
- 回温检查
- 额外通知

都不会重新把请求线程拖进慢路径。


### 6.4 `path + file` 让冷层成为正式落地状态

`procWriteSnapshotLocked()` 做了 3 步：

1. 用 `xvalue + json` 组织冷热摘要
2. 用 `xrtPathGetDir()` + `xrtDirCreateAll()` 确保目录存在
3. 用 `xrtFileWriteAll()` 落成 `runtime/hot-cold.json`

只有做到这一步，冷层才真正从“内存里的概念”变成“系统里的正式层”。


### 6.5 `hash` 只是稳定指纹，不是安全能力

`iKeyHash` 在这里承担的是：

- 页面里更短的对象指纹
- 日志和快照里的稳定轻量标识

它不是认证能力，也不应该被误当成安全 token。


### 6.6 这页故意还没做“回温”

这页的主线先收口在：

- hot -> cold

而不是：

- hot -> cold -> warm back -> hot

因为如果先不把热层、冷层和快照边界讲稳，后面的“回温”只会把状态写得更乱。


## 7. 接回 HTTP、模板和脚本时怎么落

### 7.1 `POST /api/cool`

只做：

1. 解析对象 key
2. 给出降温原因
3. 推入后台队列
4. 立即返回已接收

不要在 handler 里直接修改热层和冷层。


### 7.2 `GET /api/dashboard`

最自然的 JSON 输出就是两块：

1. 当前热层摘要
2. 最近冷层摘要

这正好对应：

- `dict`
- `list`


### 7.3 `GET /dashboard`

页面最自然的布局就是：

1. 热层对象卡片或表格
2. 最近冷层时间线
3. 当前冷层快照路径和生成时间

这样模板页不需要再去扫日志和零散文件拼状态。


### 7.4 `runtime/hot-cold.json`

运维脚本、CLI 工具和面板导出都应该优先消费这份快照，而不是直接读 worker 临时内存。

这样冷热分层才算真正成为：

- 一个正式状态层


## 8. 这页最容易写错的地方

### 8.1 把冷层继续写成 archive

这样你后面就很难再补：

- 回温
- 冷层滚动
- 多层存储

因为语义一开始就写错了。


### 8.2 让页面直接扫热层拼冷数据

一旦冷层还没有独立模型，页面就会慢慢演化成：

- 从热层补字段
- 从日志补字段
- 从文件补字段

最后谁也说不清哪份才是真状态。


### 8.3 在请求线程里直接写快照文件

冷热分层一旦开始真的落盘，文件、模板、JSON 和日志就都可能跟着进来。  
如果这些动作还留在请求线程里，面板很快就会退化。


## 9. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 冷层回温
	当某个冷层对象重新被访问时，再把它拉回热层，可以继续读 [把本地控制台服务升级成一个冷层回温面板](warm-back-dashboard.md)
2. 冷层滚动归档
	让冷层超过窗口后再正式归档进长期文件，可以继续读 [把本地控制台服务升级成一个冷热滚动归档面板](rolling-tier-archive-dashboard.md)
3. [多层存储](multi-tier-storage-dashboard.md)
	把热层、温层、冷文件和长期层拆成更清晰的正式多层

如果你还没完全建立这里的层级拆分心智，建议一起复读：

- [把本地控制台服务升级成一个缓存刷新面板](cache-refresh-dashboard.md)
- [把本地控制台服务升级成一个归档面板](archive-dashboard.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
