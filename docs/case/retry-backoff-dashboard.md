# 把本地控制台服务升级成一个重试退避面板

> 这个案例要解决的不是“失败了就再试一次”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务需要把失败任务放进后台重试、保留下一次重试时间、把最近尝试历史稳定展示出来，并让页面、JSON、日志和快照都对齐到同一份状态时，怎样把 `dict + list + queue + thread + time + hash + xhttpd + template` 串成一条正式主线，而不是让失败、重试、退避和页面展示各写各的。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了前面那个本地控制台服务：

- 它能读配置
- 能写日志
- 能用 `queue + thread` 跑后台工作
- 能同时输出 JSON 和 HTML

现在服务再往前走一步，开始接“失败后重试”的任务：

- 某个任务第一次执行可能失败
- 失败后不能立刻无限重跑
- 下一次重试时间必须可计算、可观察
- 页面要看到“当前待重试任务”和“最近尝试历史”
- 最新重试状态还要能落成一个快照文件

如果这时只写成：

- 失败了就 `sleep` 一下重跑
- 或者 HTTP handler 里直接 while retry

很快就会出现 4 个问题：

- 请求线程被失败重试链拖住
- 当前任务状态和历史尝试混在同一份结构里
- 页面层为了显示“下一次什么时候重试”开始碰 worker 临时状态
- 日志、JSON、HTML 和快照各自再拼一套字段

这页真正要补出的，不是“如何 for 循环重试”，而是：

> 当失败任务要进入长生命周期的重试通道时，怎样把当前重试槽位、最近尝试历史和后台退避队列拆成三条稳定主线。


## 2. 为什么这次不是 `task group`

### 2.1 这次的问题不是 bounded batch

前面的批处理页解决的是：

- 一次提交一批任务
- 一批任务最终要有一个正式收口点

那是：

- bounded batch scope

而这页的问题是：

- 某个任务失败后，什么时候再次进入执行队列
- 失败链可能持续多轮
- 页面更关心“当前处于哪一轮、下一次在什么时候”

这已经不是：

- “这一批什么时候 join”

而是：

- “这条 retry lane 现在处于什么状态”


### 2.2 这里更适合 `queue + thread + time`

这页的核心不是一组 child future，而是：

- 一个长生命周期 worker
- 一个可重入的 retry queue
- 每个任务一条当前状态槽位
- 一条最近尝试历史时间线

所以这次更自然的主线是：

- `queue + thread + time`

而不是：

- `task group + join future`


### 2.3 `dict` 和 `list` 在这里分别负责“当前态”和“历史态”

这页里：

- `dict`
	- 保存当前每个任务的 retry slot
- `list`
	- 保存最近尝试历史

这两个层次一旦拆开，页面和 JSON 就不需要再从 worker 临时状态里硬挤出一条时间线。


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `dict` | 保存 `task_key -> retry slot` 当前状态 | 当前第几轮、下次何时重试、是否仍待处理 |
| `list` | 保存最近尝试历史 | 页面时间线和最近事件导出 |
| `queue + thread` | 后台执行和重入重试任务 | HTTP 不阻塞在失败链上 |
| `time` | 计算下一次重试时刻 | 退避、下一次执行窗口 |
| `hash` | 给任务 key 一个稳定指纹 | 页面、日志和快照中的轻量标识 |
| `xvalue + json` | 导出当前 retry summary | JSON 和快照共享同一份模型 |
| `xhttpd + template` | 展示和触发重试面板 | 浏览器和脚本共享入口 |

一句话记住：

> `dict` 管“现在这条重试链是什么状态”，`list` 管“最近发生了哪些尝试”，worker 管“下一轮什么时候再进队列”。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页只把快照语义改成 retry 面板：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-retry.json
```

其中：

- `config/console.json`
	- 保存默认最大重试次数和基础退避时间
- `web/dashboard.html`
	- 用于渲染 retry 面板
- `runtime/console.log`
	- 记录提交、失败、退避和最终成功
- `runtime/last-retry.json`
	- 保存最近导出的 retry 摘要


## 5. 先把 retry slot、历史线和后台退避队列立起来

下面这段代码故意先聚焦 retry 核心，不把完整 `xhttpd` handler 和模板渲染全部塞进去。

它保留最关键的三件事：

1. `dict` 保存当前 retry slot
2. `list` 保存最近尝试历史
3. `queue + thread + time` 负责后台退避重试

这个骨架会实际做这些事：

- 初始化 retry center
- 提交两个任务
- 某个任务前两次失败、第三次成功
- worker 计算下一次退避时间并重入队列
- 主线程打印当前 retry slot 和最近尝试历史

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int64 iSlotId;
	int32 iLastAttempt;
	int32 iFailCount;
	int32 iSuccessCount;
	xtime tNextRetryAt;
	uint64 iTaskHash;
	bool bPending;
	char sTaskKey[64];
	char sLastStatus[24];
} DemoRetrySlot;

typedef struct
{
	char sTaskKey[64];
	int32 iAttempt;
	int32 iStatus;
	int32 iBackoffMs;
	xtime tWhen;
} DemoRetryLog;

typedef struct
{
	char sTaskKey[64];
	int32 iAttempt;
	int32 iMaxRetry;
	int32 iSuccessAttempt;
	int32 iBaseBackoffMs;
	xtime tNextRunAt;
} DemoRetryJob;

typedef struct
{
	xdict pSlots;
	xlist pLogs;
	xmutex hMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	int64 iNextSlotId;
	int64 iNextLogSeq;
} DemoRetryCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static DemoRetrySlot* procFindRetrySlotLocked(DemoRetryCenter* pCenter, const char* sTaskKey)
{
	if ( (pCenter == NULL) || (sTaskKey == NULL) ) {
		return NULL;
	}

	return (DemoRetrySlot*)xrtDictGetPtr(pCenter->pSlots, (ptr)sTaskKey, (uint32)strlen(sTaskKey));
}

static DemoRetrySlot* procEnsureRetrySlotLocked(DemoRetryCenter* pCenter, const char* sTaskKey)
{
	DemoRetrySlot* pSlot = NULL;

	pSlot = procFindRetrySlotLocked(pCenter, sTaskKey);
	if ( pSlot != NULL ) {
		return pSlot;
	}

	pSlot = (DemoRetrySlot*)xrtMalloc(sizeof(DemoRetrySlot));
	if ( pSlot == NULL ) {
		return NULL;
	}

	memset(pSlot, 0, sizeof(*pSlot));
	pCenter->iNextSlotId++;
	pSlot->iSlotId = pCenter->iNextSlotId;
	pSlot->iTaskHash = xrtHash64((ptr)sTaskKey, strlen(sTaskKey));
	procCopyText(pSlot->sTaskKey, sizeof(pSlot->sTaskKey), sTaskKey);
	procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "queued");

	if ( !xrtDictSetPtr(pCenter->pSlots, (ptr)pSlot->sTaskKey, (uint32)strlen(pSlot->sTaskKey), pSlot, NULL) ) {
		xrtFree(pSlot);
		return NULL;
	}

	return pSlot;
}

static bool procAppendRetryLogLocked(DemoRetryCenter* pCenter, const char* sTaskKey, int32 iAttempt, int32 iStatus, int32 iBackoffMs)
{
	DemoRetryLog* pLog = NULL;
	bool bNew = FALSE;
	int64 iSeq;

	if ( (pCenter == NULL) || (sTaskKey == NULL) ) {
		return FALSE;
	}

	pCenter->iNextLogSeq++;
	iSeq = pCenter->iNextLogSeq;
	pLog = (DemoRetryLog*)xrtListSet(pCenter->pLogs, iSeq, &bNew);
	if ( pLog == NULL ) {
		return FALSE;
	}

	memset(pLog, 0, sizeof(*pLog));
	procCopyText(pLog->sTaskKey, sizeof(pLog->sTaskKey), sTaskKey);
	pLog->iAttempt = iAttempt;
	pLog->iStatus = iStatus;
	pLog->iBackoffMs = iBackoffMs;
	pLog->tWhen = xrtNow();

	if ( iSeq > 12 ) {
		(void)xrtListRemove(pCenter->pLogs, iSeq - 12);
	}

	return TRUE;
}

static bool procRetryCenterInit(DemoRetryCenter* pCenter)
{
	xqueue_config tCfg;

	memset(pCenter, 0, sizeof(*pCenter));

	pCenter->pSlots = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( pCenter->pSlots == NULL ) {
		return FALSE;
	}

	pCenter->pLogs = xrtListCreate(sizeof(DemoRetryLog), XRT_OBJMODE_SHARED);
	if ( pCenter->pLogs == NULL ) {
		xrtDictDestroy(pCenter->pSlots);
		pCenter->pSlots = NULL;
		return FALSE;
	}

	pCenter->hMutex = xrtMutexCreate();
	if ( pCenter->hMutex == NULL ) {
		xrtListDestroy(pCenter->pLogs);
		xrtDictDestroy(pCenter->pSlots);
		pCenter->pLogs = NULL;
		pCenter->pSlots = NULL;
		return FALSE;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64u;
	pCenter->hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pCenter->hQueue == NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		xrtListDestroy(pCenter->pLogs);
		xrtDictDestroy(pCenter->pSlots);
		pCenter->hMutex = NULL;
		pCenter->pLogs = NULL;
		pCenter->pSlots = NULL;
		return FALSE;
	}

	return TRUE;
}

static bool procSubmitRetry(DemoRetryCenter* pCenter, const char* sTaskKey, int32 iMaxRetry, int32 iSuccessAttempt, int32 iBaseBackoffMs)
{
	DemoRetryJob* pJob = NULL;
	DemoRetrySlot* pSlot = NULL;

	if ( (pCenter == NULL) || (sTaskKey == NULL) ) {
		return FALSE;
	}

	pJob = (DemoRetryJob*)xrtMalloc(sizeof(DemoRetryJob));
	if ( pJob == NULL ) {
		return FALSE;
	}

	memset(pJob, 0, sizeof(*pJob));
	procCopyText(pJob->sTaskKey, sizeof(pJob->sTaskKey), sTaskKey);
	pJob->iAttempt = 1;
	pJob->iMaxRetry = iMaxRetry;
	pJob->iSuccessAttempt = iSuccessAttempt;
	pJob->iBaseBackoffMs = iBaseBackoffMs;
	pJob->tNextRunAt = xrtNow();

	xrtMutexLock(pCenter->hMutex);
	pSlot = procEnsureRetrySlotLocked(pCenter, sTaskKey);
	if ( pSlot != NULL ) {
		pSlot->bPending = TRUE;
		pSlot->tNextRetryAt = pJob->tNextRunAt;
		procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "queued");
	}
	xrtMutexUnlock(pCenter->hMutex);

	if ( pSlot == NULL ) {
		xrtFree(pJob);
		return FALSE;
	}

	if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pJob) != XQUEUE_OK ) {
		xrtFree(pJob);
		return FALSE;
	}

	return TRUE;
}

static uint32 procRetryWorker(ptr pArg)
{
	DemoRetryCenter* pCenter = (DemoRetryCenter*)pArg;
	DemoRetryJob* pJob = NULL;
	DemoRetryJob* pNextJob = NULL;
	DemoRetrySlot* pSlot = NULL;
	xqueue_result iRet;
	xtime tNow;
	int32 iBackoffMs;
	bool bSuccess;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pCenter->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		tNow = xrtNow();
		if ( pJob->tNextRunAt > tNow ) {
			xrtSleep((uint32)(pJob->tNextRunAt - tNow));
		}

		bSuccess = (pJob->iAttempt >= pJob->iSuccessAttempt);
		iBackoffMs = pJob->iBaseBackoffMs * pJob->iAttempt;

		xrtMutexLock(pCenter->hMutex);
		pSlot = procEnsureRetrySlotLocked(pCenter, pJob->sTaskKey);
		if ( pSlot != NULL ) {
			pSlot->iLastAttempt = pJob->iAttempt;
		}

		if ( bSuccess ) {
			if ( pSlot != NULL ) {
				pSlot->iSuccessCount++;
				pSlot->bPending = FALSE;
				pSlot->tNextRetryAt = 0;
				procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "success");
			}
			(void)procAppendRetryLogLocked(pCenter, pJob->sTaskKey, pJob->iAttempt, XRT_NET_OK, 0);
			xrtMutexUnlock(pCenter->hMutex);
			xrtFree(pJob);
			continue;
		}

		if ( pSlot != NULL ) {
			pSlot->iFailCount++;
		}

		if ( pJob->iAttempt < pJob->iMaxRetry ) {
			if ( pSlot != NULL ) {
				pSlot->bPending = TRUE;
				pSlot->tNextRetryAt = xrtNow() + iBackoffMs;
				procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "retrying");
			}
			(void)procAppendRetryLogLocked(pCenter, pJob->sTaskKey, pJob->iAttempt, XRT_NET_ERROR, iBackoffMs);
			xrtMutexUnlock(pCenter->hMutex);

			pNextJob = (DemoRetryJob*)xrtMalloc(sizeof(DemoRetryJob));
			if ( pNextJob != NULL ) {
				memcpy(pNextJob, pJob, sizeof(*pNextJob));
				pNextJob->iAttempt++;
				pNextJob->tNextRunAt = xrtNow() + iBackoffMs;

				if ( xrtMPSCQWaitTryPush(pCenter->hQueue, pNextJob) != XQUEUE_OK ) {
					xrtFree(pNextJob);
				}
			}

			xrtFree(pJob);
			continue;
		}

		if ( pSlot != NULL ) {
			pSlot->bPending = FALSE;
			pSlot->tNextRetryAt = 0;
			procCopyText(pSlot->sLastStatus, sizeof(pSlot->sLastStatus), "failed");
		}
		(void)procAppendRetryLogLocked(pCenter, pJob->sTaskKey, pJob->iAttempt, XRT_NET_ERROR, 0);
		xrtMutexUnlock(pCenter->hMutex);

		xrtFree(pJob);
	}

	return 0u;
}

static bool procDumpRetrySlot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoRetrySlot* pSlot = *(DemoRetrySlot**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pSlot != NULL ) {
		printf(
			"slot key=%s attempt=%d fail=%d success=%d pending=%d next_retry=%lld hash=%llu status=%s\n",
			pSlot->sTaskKey,
			(int)pSlot->iLastAttempt,
			(int)pSlot->iFailCount,
			(int)pSlot->iSuccessCount,
			(int)pSlot->bPending,
			(long long)pSlot->tNextRetryAt,
			(unsigned long long)pSlot->iTaskHash,
			pSlot->sLastStatus
		);
	}

	return FALSE;
}

static bool procDumpRetryLog(int64 iKey, ptr pVal, ptr pArg)
{
	DemoRetryLog* pLog = (DemoRetryLog*)pVal;
	(void)pArg;

	printf(
		"log seq=%lld key=%s attempt=%d status=%d backoff=%d when=%lld\n",
		(long long)iKey,
		pLog->sTaskKey,
		(int)pLog->iAttempt,
		(int)pLog->iStatus,
		(int)pLog->iBackoffMs,
		(long long)pLog->tWhen
	);
	return FALSE;
}

static bool procFreeRetrySlot(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoRetrySlot* pSlot = *(DemoRetrySlot**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pSlot != NULL ) {
		xrtFree(pSlot);
	}
	return FALSE;
}

static void procRetryCenterUnit(DemoRetryCenter* pCenter)
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

	if ( (pCenter->hMutex != NULL) && (pCenter->pSlots != NULL) ) {
		xrtMutexLock(pCenter->hMutex);
		xrtDictWalk(pCenter->pSlots, procFreeRetrySlot, NULL);
		xrtMutexUnlock(pCenter->hMutex);
	}

	if ( pCenter->hMutex != NULL ) {
		xrtMutexDestroy(pCenter->hMutex);
		pCenter->hMutex = NULL;
	}
	if ( pCenter->pLogs != NULL ) {
		xrtListDestroy(pCenter->pLogs);
		pCenter->pLogs = NULL;
	}
	if ( pCenter->pSlots != NULL ) {
		xrtDictDestroy(pCenter->pSlots);
		pCenter->pSlots = NULL;
	}
}

int main(void)
{
	DemoRetryCenter tCenter;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procRetryCenterInit(&tCenter) ) {
		xrtUnit();
		return 1;
	}

	tCenter.hWorker = xrtThreadCreate((ptr)procRetryWorker, &tCenter, 0u);
	if ( tCenter.hWorker == NULL ) {
		procRetryCenterUnit(&tCenter);
		xrtUnit();
		return 1;
	}

	if ( !procSubmitRetry(&tCenter, "probe:alpha", 4, 3, 80) ) {
		goto cleanup;
	}
	if ( !procSubmitRetry(&tCenter, "probe:beta", 3, 2, 60) ) {
		goto cleanup;
	}

	xrtSleep(900u);

	xrtMutexLock(tCenter.hMutex);
	printf("== current retry slots ==\n");
	xrtDictWalk(tCenter.pSlots, procDumpRetrySlot, NULL);

	printf("== recent retry logs ==\n");
	xrtListWalk(tCenter.pLogs, procDumpRetryLog, NULL);
	xrtMutexUnlock(tCenter.hMutex);

	iRet = 0;

cleanup:
	procRetryCenterUnit(&tCenter);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 5 个点

### 6.1 `dict` 保存的是“当前重试槽位”，不是历史

`pSlots` 里真正保存的是：

- 某个 `task_key` 当前处于第几轮
- 是否仍然 pending
- 下一次何时重试

它不负责保留所有过去尝试。


### 6.2 `list` 保存的是“最近尝试历史”，不是活对象

`pLogs` 用的是：

- `xrtListCreate(sizeof(DemoRetryLog), XRT_OBJMODE_SHARED)`

也就是：

- 历史记录结构体直接按值落进 `list`

这样页面和快照读到的是稳定历史，而不是 worker 临时对象。


### 6.3 retry lane 是长生命周期 worker，不是 bounded batch

这里的 worker 不是：

- 一次 join 之后彻底结束的一批任务

而是：

- 持续接收失败任务
- 计算下一次退避时间
- 把下一轮继续推回队列

所以这条主线更适合：

- `queue + thread + time`

而不是再回到 `task group`。


### 6.4 退避时间是正式状态，不是临时 sleep 数字

`tNextRetryAt` 一旦成为 slot 字段，就意味着：

- 页面能显示下一次时间
- JSON 能导出下一次时间
- 快照能保留下一次时间

这比在 worker 里偷偷 `sleep` 更接近真实系统。


### 6.5 `hash` 让任务 key 有一个稳定指纹

`iTaskHash` 在这里承担的是：

- 页面和日志里的轻量标识

而不是：

- 安全验证
- 权限判定


## 7. 接回 HTTP、模板和快照时怎么落

### 7.1 `POST /api/retry`

只做：

1. 解析任务 key
2. 投递一个 retry job
3. 立即返回已接收

不要在 handler 里直接跑重试循环。


### 7.2 `GET /api/retry`

更适合直接导出两块：

1. 当前 retry slot
2. 最近尝试历史

它们正好对应：

- `dict`
- `list`


### 7.3 `GET /dashboard`

页面最自然的布局就是：

1. 当前待重试任务表
2. 最近尝试时间线

这也是为什么这两层数据结构应该从一开始就拆开。


### 7.4 `runtime/last-retry.json`

每次 worker 更新 slot 后，把：

- 当前 retry slot 摘要
- 最近尝试历史

统一导成一份 `xvalue`，再落成快照文件，这样页面、JSON 和日志才能沿同一份状态工作。


## 8. 这页最容易写错的地方

### 8.1 失败后在 HTTP 线程里直接 while retry

这是最常见、也最糟糕的退化点。

一旦某个任务连续失败，整个请求线程就会被拖住。


### 8.2 用一张结构同时表达“当前态”和“历史态”

这样最后你既很难稳定展示下一次重试时间，也很难稳定导出最近历史。


### 8.3 把退避时间只放在局部变量里

如果 `next_retry_at` 不是正式状态字段，页面和快照就没法稳定表达“这条任务什么时候会再试一次”。


## 9. 这页之后最自然的下一步

如果继续往业务层推进，最自然的下一步有 3 个：

1. 归档面板
	把最终失败或成功的任务转进历史归档
2. 冷热分层缓存
	把“当前活跃数据”和“冷数据快照”拆成两层
3. 重试 + 子进程探测
	把这条 retry lane 挂回子进程探测面板

如果你还没完全建立这里的状态拆分心智，建议一起复读：

- [把本地控制台服务升级成一个缓存刷新面板](cache-refresh-dashboard.md)
- [把本地控制台服务升级成一个批处理任务面板](batch-task-dashboard.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
