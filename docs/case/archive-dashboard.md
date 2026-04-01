# 把本地控制台服务升级成一个归档面板

> 这个案例要解决的不是“任务做完以后删掉就行了”，而是更贴近真实服务的问题：当一个本地控制台服务既要维护当前活跃对象，又要把结束对象正式归档，还要把最近归档历史稳定展示给页面、JSON、日志和快照时，怎样把 `dict + list + queue + thread + path + file + xhttpd + template` 串成一条正式主线，而不是让删除、日志、页面和归档文件各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面那个本地控制台服务：

- 它能读配置
- 能写日志
- 能用 `queue + thread` 跑后台任务
- 能同时输出 JSON 和 HTML

现在服务又往前走一步，开始有“归档”需求：

- 某个活跃对象结束后，不应该直接消失
- 页面要同时看到“当前活跃对象”和“最近归档历史”
- 归档动作不能阻塞 HTTP handler
- 归档记录需要带原因、时间和稳定标识
- 最近一轮归档摘要还要能落成快照文件

如果这时只写成：

- 从当前字典里 `Remove`
- 再顺手 `printf` 一行日志

很快就会出现 4 个问题：

- 活跃态和归档态混在一起，没有正式边界
- 页面为了显示历史开始扫日志文件或 worker 临时状态
- 后续要补 JSON / 快照时，又要再拼一套归档字段
- “删除”和“归档”被写成同一件事，后面谁也不敢改

这页真正要补出的不是“怎么删对象”，而是：

> 当系统里开始有正式归档语义时，怎样把当前活跃态、归档态和后台归档通道拆成三条稳定主线。


## 2. 为什么这次不是“从活跃表里删掉就完了”

### 2.1 删除只回答“现在不在了”，归档回答“之前发生了什么”

从当前活跃表里移除，只能说明：

- 这个对象现在不再活跃

但归档真正还需要保留：

- 它原来是什么
- 它为什么结束
- 它何时结束
- 它在页面和快照里该怎么继续展示

所以“删除”和“归档”不是一回事。


### 2.2 `dict` 负责当前态，不负责历史态

这页里：

- `dict`
	- 保存当前活跃对象

它最适合回答：

- 这个 key 现在是不是活跃
- 活跃对象现在的最新状态是什么

但它不适合直接承担：

- 最近第 1 条归档
- 最近第 2 条归档
- 最近第 3 条归档

所以归档历史应该拆出去。


### 2.3 归档历史更适合成为稳定值记录，而不是活对象引用

一旦对象已经结束，页面和快照真正需要的是：

- 一个稳定快照

而不是：

- 指向活跃对象的指针

这正是：

- `list`

很适合承担的地方。


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存当前活跃对象 | 当前态查询和管理 |
| `list` | 保存最近归档历史 | 页面时间线和最近归档导出 |
| `queue + thread` | 后台执行归档动作 | HTTP 不阻塞在归档收口上 |
| `path + file` | 归档快照和落盘路径 | 让归档结果进入真实文件系统 |
| `hash` | 给对象 key 一个稳定指纹 | 页面、日志、快照里的轻量标识 |
| `xvalue + json` | 导出最新归档摘要 | JSON 和快照共享同一份模型 |
| `xhttpd + template` | 展示活跃态和归档态 | 浏览器和脚本共享一个入口 |

一句话记住：

> `dict` 管“现在还活着什么”，`list` 管“最近归档了什么”，worker 管“什么时候把活跃态正式转成归档态”。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页只把快照语义改成归档面板：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-archive.json
```

其中：

- `config/console.json`
	- 保存归档队列容量、最近保留条数等配置
- `web/dashboard.html`
	- 用于渲染归档面板
- `runtime/console.log`
	- 记录归档提交和归档完成
- `runtime/last-archive.json`
	- 保存最近导出的归档摘要


## 5. 先把活跃态、归档态和后台归档队列立起来

下面这段代码故意先聚焦归档核心，不把完整 `xhttpd` handler 和模板渲染全部塞进去。

它保留最关键的三件事：

1. `dict` 保存当前活跃对象
2. `list` 保存最近归档历史
3. `queue + thread` 把归档动作放到后台

这个骨架会实际做这些事：

- 初始化 archive center
- 注册几个活跃对象
- 投递两个归档请求
- worker 把活跃对象转成归档记录
- 主线程分别打印当前活跃对象和最近归档历史

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iActiveId;
	xtime tStartedAt;
	uint64 iKeyHash;
	char sKey[64];
	char sTitle[64];
} DemoActiveItem;

typedef struct
{
	char sKey[64];
	char sTitle[64];
	char sReason[48];
	uint64 iKeyHash;
	xtime tArchivedAt;
} DemoArchiveEntry;

typedef struct
{
	char sKey[64];
	char sReason[48];
} DemoArchiveJob;

typedef struct
{
	xdict pActive;
	xlist pArchive;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextActiveId;
	int64 iNextArchiveSeq;
} DemoArchiveCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static DemoActiveItem* procFindActiveLocked(DemoArchiveCenter* pCenter, const char* sKey)
{
	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return NULL;
	}

	return (DemoActiveItem*)xrtDictGetPtr(pCenter->pActive, (ptr)sKey, (uint32)strlen(sKey));
}

static bool procAppendArchiveLocked(DemoArchiveCenter* pCenter, DemoActiveItem* pItem, const char* sReason)
{
	DemoArchiveEntry* pEntry = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (pItem == NULL) ) {
		return FALSE;
	}

	pCenter->iNextArchiveSeq++;
	iSeq = pCenter->iNextArchiveSeq;
	pEntry = (DemoArchiveEntry*)xrtListSet(pCenter->pArchive, iSeq, &bNew);
	if ( pEntry == NULL ) {
		return FALSE;
	}

	memset(pEntry, 0, sizeof(*pEntry));
	procCopyText(pEntry->sKey, sizeof(pEntry->sKey), pItem->sKey);
	procCopyText(pEntry->sTitle, sizeof(pEntry->sTitle), pItem->sTitle);
	procCopyText(pEntry->sReason, sizeof(pEntry->sReason), sReason);
	pEntry->iKeyHash = pItem->iKeyHash;
	pEntry->tArchivedAt = xrtNow();

	if ( iSeq > 10 ) {
		(void)xrtListRemove(pCenter->pArchive, iSeq - 10);
	}

	return TRUE;
}

static bool procArchiveCenterInit(DemoArchiveCenter* pCenter)
{
	xqueue_config tCfg;

	memset(pCenter, 0, sizeof(*pCenter));

	pCenter->pActive = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pCenter->pActive == NULL ) {
		return FALSE;
	}

	pCenter->pArchive = xrtListCreate(sizeof(DemoArchiveEntry), XRT_OBJMODE_SHARED);
	if ( pCenter->pArchive == NULL ) {
		xrtDictDestroy(pCenter->pActive);
		pCenter->pActive = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pArchive);
		xrtDictDestroy(pCenter->pActive);
		pCenter->pArchive = NULL;
		pCenter->pActive = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pArchive);
		xrtDictDestroy(pCenter->pActive);
		pCenter->hMutex = NULL;
		pCenter->pArchive = NULL;
		pCenter->pActive = NULL;
		return FALSE;
	}

	return TRUE;
}

static bool procRegisterActive(DemoArchiveCenter* pCenter, const char* sKey, const char* sTitle)
{
	DemoActiveItem* pItem = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pItem = (DemoActiveItem*)xrtMalloc(sizeof(DemoActiveItem));
	if ( pItem == NULL ) {
		return FALSE;
	}

	memset(pItem, 0, sizeof(*pItem));
	pCenter->iNextActiveId++;
	pItem->iActiveId = pCenter->iNextActiveId;
	pItem->tStartedAt = xrtNow();
	pItem->iKeyHash = xrtHash64((ptr)sKey, strlen(sKey));
	procCopyText(pItem->sKey, sizeof(pItem->sKey), sKey);
	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);

	if ( !xrtDictSetPtr(pCenter->pActive, (ptr)pItem->sKey, (uint32)strlen(pItem->sKey), pItem, NULL) ) {
		xrtFree(pItem);
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitArchive(DemoArchiveCenter* pCenter, const char* sKey, const char* sReason)
{
	DemoArchiveJob* pJob = NULL;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoArchiveJob*)xrtMalloc(sizeof(DemoArchiveJob));
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

static uint32 procArchiveWorker(ptr pArg)
{
	DemoArchiveCenter* pCenter = (DemoArchiveCenter*)pArg;
	DemoArchiveJob* pJob = NULL;
	DemoActiveItem* pItem = NULL;
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
		pItem = (DemoActiveItem*)xrtDictRemovePtr(pCenter->pActive, (ptr)pJob->sKey, (uint32)strlen(pJob->sKey));
		if ( pItem != NULL ) {
			(void)procAppendArchiveLocked(pCenter, pItem, pJob->sReason);
		}
		xrtMutexUnlock(pCenter->hMutex);

		if ( pItem != NULL ) {
			xrtFree(pItem);
		}
		xrtFree(pJob);
	}

	return 0u;
}

static bool procDumpActive(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoActiveItem* pItem = *(DemoActiveItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		printf(
			"active key=%s title=%s started_at=%lld hash=%llu\n",
			pItem->sKey,
			pItem->sTitle,
			(long long)pItem->tStartedAt,
			(unsigned long long)pItem->iKeyHash
		);
	}

	return FALSE;
}

static bool procDumpArchive(int64 iKey, ptr pVal, ptr pArg)
{
	DemoArchiveEntry* pEntry = (DemoArchiveEntry*)pVal;
	(void)pArg;

	printf(
		"archive seq=%lld key=%s title=%s reason=%s archived_at=%lld hash=%llu\n",
		(long long)iKey,
		pEntry->sKey,
		pEntry->sTitle,
		pEntry->sReason,
		(long long)pEntry->tArchivedAt,
		(unsigned long long)pEntry->iKeyHash
	);
	return FALSE;
}

static bool procFreeActive(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoActiveItem* pItem = *(DemoActiveItem**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pItem != NULL ) {
		xrtFree(pItem);
	}
	return FALSE;
}

static void procArchiveCenterUnit(DemoArchiveCenter* pCenter)
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

	if ( (pCenter->hMutex != NULL) && (pCenter->pActive != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pActive, procFreeActive, NULL);
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
	if ( pCenter->pActive != NULL ) {
		xrtDictDestroy(pCenter->pActive);
		pCenter->pActive = NULL;
	}
}

int main(void)
{
	DemoArchiveCenter tCenter;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procArchiveCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procArchiveWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procArchiveCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procRegisterActive(&tCenter, "job:alpha", "Alpha Import") ) {
		goto cleanup;
	}
	if ( !procRegisterActive(&tCenter, "job:beta", "Beta Refresh") ) {
		goto cleanup;
	}
	if ( !procRegisterActive(&tCenter, "job:gamma", "Gamma Export") ) {
		goto cleanup;
	}

	if ( !procSubmitArchive(&tCenter, "job:beta", "completed") ) {
		goto cleanup;
	}
	if ( !procSubmitArchive(&tCenter, "job:gamma", "expired") ) {
		goto cleanup;
	}

	xrtSleep(300u);

	xrtMutexLock(tCenter.hMutex);
	printf("== active ==\n");
	xrtDictWalk(tCenter.pActive, procDumpActive, NULL);

	printf("== archive ==\n");
	xrtListWalk(tCenter.pArchive, procDumpArchive, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	iRet = 0;

cleanup:
	procArchiveCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 5 个点

### 6.1 `dict` 保存的是当前活跃态，不是归档态

`pActive` 里保存的是：

- 当前还活跃的对象

一旦对象正式进入归档，最稳的动作就是：

- 从 `dict` 里移除
- 转成稳定归档记录


### 6.2 `list` 保存的是稳定归档记录，不是活对象指针

`pArchive` 用的是：

- `xrtListCreate(sizeof(DemoArchiveEntry), XRT_OBJMODE_SHARED)`

也就是：

- 归档记录直接按值落进 `list`

这样页面、JSON 和快照读到的是稳定归档态，而不是活对象引用。


### 6.3 归档动作走后台 worker，不走 HTTP 线程

这页最重要的边界之一就是：

- HTTP 只发归档请求
- worker 真正把活跃态转成归档态

这样后面不管归档收口里再加文件落盘、快照写出还是通知逻辑，HTTP 线程都不会直接被拖住。


### 6.4 归档不等于日志

日志当然仍然重要，但这页里真正可供页面和快照消费的是：

- `DemoArchiveEntry`

也就是说，日志只是观察路径之一，不再承担归档状态本身。


### 6.5 `hash` 让归档对象有一个稳定标识

`iKeyHash` 在这里承担的是：

- 页面、日志、快照中的轻量标识

它不是安全能力，只是一个稳定指纹。


## 7. 接回 HTTP、模板和快照时怎么落

### 7.1 `POST /api/archive`

只做：

1. 解析对象 key
2. 生成一个归档请求
3. 推入后台队列
4. 立即返回已接收

不要在 handler 里直接做删除和归档收口。


### 7.2 `GET /api/archive`

更适合直接导出两块：

1. 当前活跃对象
2. 最近归档历史

它们正好对应：

- `dict`
- `list`


### 7.3 `GET /dashboard`

页面最自然的布局就是：

1. 当前活跃对象表
2. 最近归档时间线

这样模板页就不需要再去扫日志文件硬推归档历史。


### 7.4 `runtime/last-archive.json`

worker 完成一次归档后，把：

- 当前活跃对象摘要
- 最近归档历史

统一导成一份 `xvalue`，再落成快照文件，这样页面、JSON 和日志才能沿同一份状态工作。


## 8. 这页最容易写错的地方

### 8.1 把归档直接写成删除

这样短期看似简单，长期就会让：

- 页面历史
- 快照导出
- 审计记录

都找不到一个正式状态来源。


### 8.2 用日志代替归档状态

日志只适合做观察，不能替代：

- 页面直接消费的归档记录
- JSON / 快照直接导出的归档记录


### 8.3 把归档动作放回请求线程

这会让归档收口重新变成同步慢路径，一旦后面归档逻辑变重，HTTP 面板马上退化。


## 9. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 冷热分层面板
	把当前活跃层和冷数据快照层正式拆开
2. 归档 + 重试联动
	把最终失败任务转进归档面板
3. 归档文件滚动
	让最近归档历史和长期归档文件分层

如果你还没完全建立这里的状态拆分心智，建议一起复读：

- [把本地控制台服务升级成一个重试退避面板](retry-backoff-dashboard.md)
- [把本地控制台服务升级成一个缓存刷新面板](cache-refresh-dashboard.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
