# 把本地控制台服务升级成一个缓存刷新面板

> 这个案例要解决的不是“做一张哈希表把值存进去”，而是更贴近真实服务的问题：当一个本地控制台服务既要给脚本和页面提供缓存读取入口，又要把慢刷新放到后台线程，还要把最近刷新历史稳定展示出来时，怎样把 `dict + list + hash + queue + thread + xhttpd + template` 串成一条正式主线，而不是让缓存命中、刷新、日志和页面各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面那个本地控制台服务：

- 能读配置
- 能写日志
- 能用 `queue + thread` 做后台工作
- 能同时输出 JSON 和 HTML

现在服务要再承担一个很常见的角色：

- `GET /api/cache?key=...` 读取某个缓存项
- 缓存命中要尽量快
- 缓存过期或强制刷新不能阻塞 HTTP handler
- 页面需要同时看到“当前缓存内容”和“最近刷新历史”
- 最新缓存摘要还要能落到快照文件

如果这时只写一张“字典 + 一个 if 过期了就同步刷新”的代码，很快就会出现 4 类问题：

- 请求线程被慢刷新卡住
- 当前缓存对象和最近刷新记录混在一张结构里，边界越来越乱
- 页面层开始直接扫活跃缓存对象去拼时间线
- 日志、JSON、HTML、快照各自维护一套字段

所以这页真正要补出的不是“怎么做缓存”，而是：

> 当缓存既有读路径、又有后台刷新、又有页面展示时，怎样把当前状态和刷新历史拆成两套稳定数据，再用一条后台主线把它们连起来。


## 2. 为什么这次不是“只上一个 `dict`”

### 2.1 `dict` 负责当前值，不负责时间线

`dict` 很适合做：

- `key -> current item`
- 快速命中
- 用 key 直接找到当前缓存对象

但它不擅长直接表达：

- 最近第 1 次刷新
- 最近第 2 次刷新
- 最近第 3 次刷新

因为这已经不是“按文本 key 查当前值”的问题，而是：

- 按顺序保留一段最近历史

所以这页会故意拆成两层：

- `dict`
	- 保存当前缓存对象
- `list`
	- 保存最近刷新历史


### 2.2 同步刷新会直接破坏缓存面板的意义

如果 `GET /api/cache` 在命中过期时直接同步刷新：

- 浏览器会卡住
- 脚本接口也会卡住
- 一旦刷新路径里有文件、网络或子进程，整个请求线程就被占住了

这正是：

- `queue + thread`

要接进来的原因。

更稳的主线是：

- HTTP 只决定“要不要发刷新请求”
- 后台线程真正执行刷新
- 页面和 JSON 只消费刷新后的稳定摘要


### 2.3 `hash` 在这里负责“内容指纹”，不是安全校验

缓存面板里常见的一个需求是：

- 页面上想知道值有没有变
- 日志里想快速标识这次刷新是不是新内容

这时最自然的工具就是：

- `xrtHash64()`

它在这里承担的是：

- 稳定内容指纹

而不是：

- 安全口令
- 签名校验


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存 `key -> current item` | 当前缓存值的稳定命中 |
| `list` | 保存最近刷新历史 | 页面时间线和最近事件导出 |
| `hash` | 为值生成稳定指纹 | 判断内容是否变化 |
| `queue + thread` | 后台执行慢刷新 | HTTP 不阻塞 |
| `logging` | 记录命中、投递、刷新完成 | 让缓存行为可追踪 |
| `xvalue + json` | 导出最新缓存摘要 | JSON 和快照共享模型 |
| `xhttpd + template` | 暴露读取与管理面板 | 浏览器和脚本使用同一服务入口 |

一句话记住：

> `dict` 管“现在是什么”，`list` 管“最近发生了什么”，后台线程管“慢刷新什么时候做”。 


## 4. 文件和输出约定

延续本地控制台服务的目录约定，这页只把快照含义改成缓存面板：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-cache.json
```

其中：

- `config/console.json`
	- 保存默认 TTL、刷新队列容量等配置
- `web/dashboard.html`
	- 用于渲染缓存面板
- `runtime/console.log`
	- 记录刷新投递、完成和失败
- `runtime/last-cache.json`
	- 保存最近导出的缓存摘要


## 5. 先把缓存核心、刷新线程和最近历史立起来

下面这段代码故意先聚焦缓存核心，不把完整 `xhttpd` handler 和模板渲染全部塞进来。

它保留最关键的三件事：

1. `dict` 保存当前缓存对象
2. `list` 保存最近刷新历史
3. `queue + thread` 把慢刷新从主线程挪到后台

这个骨架会实际做这些事：

- 初始化缓存存储
- 提交几次刷新请求
- 后台线程生成缓存值和 hash
- 主线程读取当前缓存对象
- 按“当前缓存对象 + 最近刷新历史”两条线打印状态

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iEntryId;
	xtime tExpireAt;
	uint64 iValueHash;
	int32 iHitCount;
	bool bRefreshing;
	char sKey[64];
	char sValue[96];
} DemoCacheItem;

typedef struct
{
	char sKey[64];
	int32 iDelayMs;
} DemoRefreshJob;

typedef struct
{
	char sKey[64];
	uint64 iValueHash;
	int32 iStatus;
	xtime tUpdatedAt;
} DemoRefreshLog;

typedef struct
{
	xdict pByKey;
	xlist pRefreshLog;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextEntryId;
	int64 iNextLogSeq;
	int64 iTTL;
} DemoCacheStore;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static DemoCacheItem* procFindCacheItemLocked(DemoCacheStore* pStore, const char* sKey)
{
	if ( (pStore == NULL) || (sKey == NULL) ) {
		return NULL;
	}

	return (DemoCacheItem*)xrtDictGetPtr(pStore->pByKey, (ptr)sKey, (uint32)strlen(sKey));
}

static bool procAppendRefreshLogLocked(DemoCacheStore* pStore, const char* sKey, uint64 iHash, int32 iStatus)
{
	DemoRefreshLog* pLog = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pStore == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pStore->iNextLogSeq++;
	iSeq = pStore->iNextLogSeq;
	pLog = (DemoRefreshLog*)xrtListSet(pStore->pRefreshLog, iSeq, &bNew);
	if ( pLog == NULL ) {
		return FALSE;
	}

	memset(pLog, 0, sizeof(*pLog));
	procCopyText(pLog->sKey, sizeof(pLog->sKey), sKey);
	pLog->iValueHash = iHash;
	pLog->iStatus = iStatus;
	pLog->tUpdatedAt = xrtNow();

	if ( iSeq > 8 ) {
		(void)xrtListRemove(pStore->pRefreshLog, iSeq - 8);
	}

	return TRUE;
}

static bool procUpsertCacheItemLocked(DemoCacheStore* pStore, const char* sKey, const char* sValue)
{
	DemoCacheItem* pItem = NULL;
	uint64 iHash;

	if ( (pStore == NULL) || (sKey == NULL) || (sValue == NULL) ) {
		return FALSE;
	}

	pItem = procFindCacheItemLocked(pStore, sKey);
	if ( pItem == NULL ) {
		pItem = (DemoCacheItem*)xrtMalloc(sizeof(DemoCacheItem));
		if ( pItem == NULL ) {
			return FALSE;
		}

		memset(pItem, 0, sizeof(*pItem));
		pStore->iNextEntryId++;
		pItem->iEntryId = pStore->iNextEntryId;
		procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);

		if ( !xrtDictSetPtr(pStore->pByKey, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
			xrtFree(pItem);
			return FALSE;
		}
	}

	iHash = xrtHash64((ptr)sValue, strlen(sValue));
	procCopyText(pItem->sValue, sizeof(pItem->sValue), sValue);
	pItem->iValueHash = iHash;
	pItem->tExpireAt = xrtNow() + pStore->iTTL;
	pItem->bRefreshing = FALSE;

	return procAppendRefreshLogLocked(pStore, sKey, iHash, XRT_NET_OK);
}

static uint32 procRefreshWorker(ptr pArg)
{
	DemoCacheStore* pStore = (DemoCacheStore*)pArg;
	DemoRefreshJob* pJob = NULL;
	xqueue_result iRet;
	char sValue[96];

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pStore->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		xrtSleep((pJob->iDelayMs > 0) ? (uint32)pJob->iDelayMs : 50u);
		snprintf(
			sValue,
			sizeof(sValue),
			"value<%s>#%lld",
			pJob->sKey,
			(long long)xrtNow()
		);

		xrtMutexLock(pStore->hMutex);
		(void)procUpsertCacheItemLocked(pStore, pJob->sKey, sValue);
		xrtMutexUnlock(pStore->hMutex);

		xrtFree(pJob);
	}

	return 0u;
}

static bool procCacheStoreInit(DemoCacheStore* pStore)
{
	xqueue_config tCfg;

	memset(pStore, 0, sizeof(*pStore));
	pStore->iTTL = 3000;

	pStore->pByKey = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pStore->pByKey == NULL ) {
		return FALSE;
	}

	pStore->pRefreshLog = xrtListCreate(sizeof(DemoRefreshLog), XRT_OBJMODE_SHARED);
	if ( pStore->pRefreshLog == NULL ) {
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	pStore->hMutex = xrtMutexCreate();
	if ( pStore->hMutex == NULL ) {
		xrtListDestroy(pStore->pRefreshLog);
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pStore->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pStore->hQueue == NULL ) {
		xrtMutexDestroy(pStore->hMutex);
		xrtListDestroy(pStore->pRefreshLog);
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	pStore->hWorker = xrtThreadCreate((ptr)procRefreshWorker, pStore, 0u);
	if ( pStore->hWorker == NULL ) {
		xrtMPSCQWaitDestroy(pStore->hQueue);
		xrtMutexDestroy(pStore->hMutex);
		xrtListDestroy(pStore->pRefreshLog);
		xrtDictDestroy(pStore->pByKey);
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitRefresh(DemoCacheStore* pStore, const char* sKey, int32 iDelayMs)
{
	DemoRefreshJob* pJob = NULL;

	if ( (pStore == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoRefreshJob*)xrtMalloc(sizeof(DemoRefreshJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sKey, sizeof(pJob->sKey), sKey);
	pJob->iDelayMs = iDelayMs;

	if ( xrtMPSCQWaitTryPush(pStore->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static bool procDumpCacheItem(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoCacheItem* pItem = *(DemoCacheItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		printf(
			"cache key=%s value=%s hits=%d hash=%llu expire_at=%lld\n",
			pItem->sKey,
			pItem->sValue,
			(int)pItem->iHitCount,
			(unsigned long long)pItem->iValueHash,
			(long long)pItem->tExpireAt
		);
	}

	return FALSE;
}

static bool procDumpRefreshLog(int64 iKey, ptr pVal, ptr pArg)
{
	DemoRefreshLog* pLog = (DemoRefreshLog*)pVal;
	(void)pArg;

	printf(
		"log seq=%lld key=%s status=%d hash=%llu updated_at=%lld\n",
		(long long)iKey,
		pLog->sKey,
		(int)pLog->iStatus,
		(unsigned long long)pLog->iValueHash,
		(long long)pLog->tUpdatedAt
	);
	return FALSE;
}

static bool procFreeCacheItem(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoCacheItem* pItem = *(DemoCacheItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		xrtFree(pItem);
	}
	return FALSE;
}

static void procCacheStoreUnit(DemoCacheStore* pStore)
{
	if ( pStore == NULL ) {
		return;
	}

	if ( pStore->hQueue != NULL ) {
		xrtMPSCQWaitClose(pStore->hQueue);
	}
	if ( pStore->hWorker != NULL ) {
		xrtThreadWait(pStore->hWorker);
		xrtThreadDestroy(pStore->hWorker);
	}
	if ( pStore->hQueue != NULL ) {
		xrtMPSCQWaitDestroy(pStore->hQueue);
	}

	if ( (pStore->hMutex != NULL) && (pStore->pByKey != NULL) ) {
		xrtMutexLock(pStore->hMutex);
		xrtDictWalk(pStore->pByKey, procFreeCacheItem, NULL);
		xrtMutexUnlock(pStore->hMutex);
	}

	if ( pStore->hMutex != NULL ) {
		xrtMutexDestroy(pStore->hMutex);
	}
	if ( pStore->pRefreshLog != NULL ) {
		xrtListDestroy(pStore->pRefreshLog);
	}
	if ( pStore->pByKey != NULL ) {
		xrtDictDestroy(pStore->pByKey);
	}
}

int main(void)
{
	DemoCacheStore tStore;
	DemoCacheItem* pItem = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procCacheStoreInit(&tStore) ) {
		xrtUnit();
		return 1;
	}

	if ( !procSubmitRefresh(&tStore, "user:1001", 80) ) {
		goto cleanup;
	}
	if ( !procSubmitRefresh(&tStore, "user:1002", 120) ) {
		goto cleanup;
	}
	if ( !procSubmitRefresh(&tStore, "user:1001", 60) ) {
		goto cleanup;
	}

	xrtSleep(500u);

	xrtMutexLock(tStore.hMutex);
	pItem = procFindCacheItemLocked(&tStore, "user:1001");
	if ( pItem != NULL ) {
		pItem->iHitCount++;
		printf("read key=user:1001 value=%s hash=%llu\n", pItem->sValue, (unsigned long long)pItem->iValueHash);
	}

	printf("== current cache ==\n");
	xrtDictWalk(tStore.pByKey, procDumpCacheItem, NULL);

	printf("== recent refresh log ==\n");
	xrtListWalk(tStore.pRefreshLog, procDumpRefreshLog, NULL);
	xrtMutexUnlock(tStore.hMutex);

	iRet = 0;

cleanup:
	procCacheStoreUnit(&tStore);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 5 个点

### 6.1 `dict` 和 `list` 不是同一层

这页最重要的拆分是：

- `dict`
	- 当前缓存对象
- `list`
	- 最近刷新历史

也就是说，页面里“当前值”和“最近发生了什么”不是同一份数据结构硬挤出来的。


### 6.2 后台线程刷新，主线程只读稳定状态

这条线里：

- `procSubmitRefresh()`
	- 只是投递刷新请求
- `procRefreshWorker()`
	- 真正生成新值并写回缓存

所以主线程和 HTTP handler 更适合扮演：

- 读缓存
- 发刷新请求

而不是自己执行慢刷新。


### 6.3 `hash` 让“值有没有变”变成稳定字段

一旦你有了：

- `iValueHash`

页面、日志、快照就都可以统一表达：

- 这次刷新是不是新内容

而不用每一层都自己重新比较整段文本。


### 6.4 `list` 这里存的是值，不是对象所有权

`pRefreshLog` 用的是：

- `xrtListCreate(sizeof(DemoRefreshLog), XRT_OBJMODE_SHARED)`

也就是：

- 直接把历史记录结构体放进 `list`

这样最近历史天然是稳定快照，不依赖外部对象继续存活。


### 6.5 真正导出给页面和 JSON 的应该是“快照摘要”

这段骨架最后打印的是：

- 当前缓存项
- 最近刷新历史

真实服务里，更稳的下一步是：

1. worker 刷新完成后生成 `xvalue` 摘要
2. `GET /api/cache`
	- 读取当前缓存摘要
3. `GET /dashboard`
	- 用模板渲染这份摘要
4. `runtime/last-cache.json`
	- 落这份摘要的最新快照


## 7. 接回 HTTP、模板和快照时怎么落

### 7.1 `GET /api/cache?key=...`

最稳的做法是：

- 只读取当前缓存对象
- 如果检测到过期
	- 投递后台刷新
	- 当前请求仍返回旧值或返回“refreshing”

不要在 handler 里：

- 直接等待刷新完成
- 直接 `Sleep`
- 直接重算最近历史


### 7.2 `POST /api/cache/refresh`

只做：

1. 解析 key
2. 投递 `DemoRefreshJob`
3. 立即返回已接收

不要在 HTTP 线程里直接写缓存对象。


### 7.3 `GET /dashboard`

页面最适合展示两块：

1. 当前缓存表
2. 最近刷新历史

它们正好对应：

- `dict`
- `list`

也正因为这两层已经拆开，模板页就不需要临时从当前缓存对象里“硬推”一条时间线。


### 7.4 `runtime/last-cache.json`

worker 完成刷新后，把：

- 当前缓存摘要
- 最近刷新历史

导成一份统一 `xvalue`，再写进快照文件。这样日志、JSON API 和页面能沿同一份模型对齐。


## 8. 这页最容易写错的地方

### 8.1 把最近历史也塞回 `dict`

这样会导致：

- 当前对象和历史对象混在一起
- 页面很难稳定展示最近 N 次事件
- 旧历史和当前值的生命周期互相影响


### 8.2 过期后在请求线程里同步刷新

这是缓存面板最常见的退化点。

一旦刷新路径慢下来，整个服务的“缓存命中”入口就不再是快路径了。


### 8.3 把 `hash` 当成安全能力

这里的 `xrtHash64()` 只是：

- 稳定内容指纹

它不能替代：

- 签名
- 加密
- 认证


### 8.4 页面直接读 worker 临时状态

页面和 JSON 应只读：

- 已经复制好的当前缓存对象
- 已经落进 `list` 的最近历史

不要让模板层去碰：

- 正在处理的 `DemoRefreshJob`
- worker 的临时字符串缓冲区


## 9. 这页之后最自然的下一步

如果你继续往业务层推进，最自然的下一步是：

1. 多租户缓存索引
	- `tenant + key` 的二级索引怎么组织
2. 刷新失败重试
	- 最近历史里怎样保留失败与退避信息
3. 与小项目主线合并
	- 真正接成一个带 JSON / HTML / snapshot 的完整缓存面板

如果你还没完全建立这里的容器心智，建议一起复读：

- [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](../guide/dict-intro.md)
- [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](../guide/list-intro.md)
- [把配置、日志、任务、网络和模板串成一个本地控制台服务](local-console-service.md)
