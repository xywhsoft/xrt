# 用 Queue + Future 写一个多生产者 Worker

> 这个案例聚焦“持续提交任务、单 worker 串行处理、每个任务各自回结果”这条主线，把 `xmpscqwait + promise/future + thread` 串成一个完整可落地的 worker 模型。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个“后台计算 worker”，它有 4 个约束：

1. 有多个生产者线程会持续提交任务
2. 真正执行业务逻辑的只有一个 worker 线程
3. 调用方不想直接共享结果数组，而是希望每个任务拿到自己的结果句柄
4. shutdown 时要能明确区分“停止接单”和“把已入队任务做完”

如果没有一条清晰主线，代码很容易变成：

- 生产者直接改共享数组
- worker 再拿锁回填结果
- 谁负责释放任务对象和结果对象说不清
- 程序退出时，不知道应该先停生产者、先停 worker，还是先销毁队列

这个案例要解决的，正是这类“持续 handoff + 每任务单独结果”的问题。


## 2. 为什么这次不用别的方案

这一页故意只选最适合这个场景的一组能力。

### 2.1 为什么不是“共享数组 + 锁”

因为这次的问题不是“多个线程一起改同一个状态”。

真正的问题是：

- 生产者怎样把任务交给 worker
- worker 怎样把结果回给提交者
- 队列满了、关闭了、排空了时怎样收口

直接共享数组虽然也能写，但边界会越来越糊。


### 2.2 为什么不是 `task group`

`task group` 更适合：

- 一批有界 child task
- 一次 fan-out
- 一次结构化 join

而这个案例更像：

- 多个生产者持续提交
- 一个长期存活的 worker 线程持续消费
- 每个任务完成时，各自通过一个 `future` 回结果

也就是说，这里的核心不是“一批任务怎样统一收口”，而是“任务怎样持续交接给 worker，再把结果逐个回给调用方”。


### 2.3 为什么选 `xmpscqwait`

因为这次的并发形状非常明确：

- 多生产者
- 单消费者
- 消费端没任务时应该阻塞等待

这正是 `xmpscqwait` 最适合的场景。

如果你这里只有一个生产者，用 `xspscq` 更轻。
如果你有多个消费者一起抢任务，才应该考虑 `xmpmcq`。


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| producer thread | 创建任务和结果句柄 | 提交点和调用方上下文 |
| `xmpscqwait` | 把任务交给 worker | FIFO handoff、背压、`Close` |
| worker thread | 阻塞式取任务并执行 | 真正的串行处理点 |
| `xpromise` | worker 完成任务时写结果 | `Resolve / Reject` |
| `xfuture` | 调用方持有每个任务的结果 | 每任务单独等待和错误读取 |

可以先记一句话：

> `queue` 负责交接，worker 负责执行，`promise/future` 负责把每个任务的结果正式还给调用方。


## 4. 代码目标

下面这段完整代码会做 6 件事：

1. 创建一个 `xmpscqwait`
2. 启动一个长期存活的 worker 线程
3. 启动两个 producer 线程，各自提交 3 个任务
4. 每个任务都创建一个 `future`
5. worker 消费任务后，通过 `promise` 完成对应 `future`
6. 主线程关闭队列、等待 worker 退出，并逐个读取 future 结果


## 5. 完整代码

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_PRODUCER_COUNT	2
#define DEMO_JOB_PER_PRODUCER	3

typedef struct
{
	int32 iProducerId;
	int32 iJobId;
	int32 iInput;
	xpromise* pPromise;
} DemoQueueJob;

typedef struct
{
	int32 iProducerId;
	int32 iJobId;
	int32 iInput;
	int32 iOutput;
} DemoQueueResult;

typedef struct
{
	xmpscqwait hQueue;
} DemoWorkerCtx;

typedef struct
{
	xmpscqwait hQueue;
	int32 iProducerId;
	int32 iBaseValue;
	int32 iJobCount;
	xfuture** arrFuture;
} DemoProducerCtx;

static void procRejectJob(DemoQueueJob* pJob, int32 iStatus, str sError)
{
	if ( pJob == NULL ) {
		return;
	}

	if ( pJob->pPromise != NULL ) {
		(void)xPromiseReject(pJob->pPromise, iStatus, sError);
		xPromiseDestroy(pJob->pPromise);
	}

	xrtFree(pJob);
}

static void procCleanupResidualJobs(xmpscqwait hQueue)
{
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQWaitTryPop(hQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			return;
		}

		procRejectJob(
			(DemoQueueJob*)pItem,
			XRT_NET_ERROR,
			(str)"worker stopped before job completed"
		);
		pItem = NULL;
	}
}

static uint32 procQueueWorker(ptr pArg)
{
	DemoWorkerCtx* pCtx = (DemoWorkerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, 500u);
		if ( iRet == XQUEUE_OK ) {
			DemoQueueJob* pJob = (DemoQueueJob*)pItem;
			DemoQueueResult* pResult = (DemoQueueResult*)xrtMalloc(sizeof(DemoQueueResult));

			if ( pResult == NULL ) {
				(void)xPromiseReject(pJob->pPromise, XRT_NET_ERROR, (str)"alloc result failed");
			} else {
				memset(pResult, 0, sizeof(DemoQueueResult));
				xrtSleep(50);

				pResult->iProducerId = pJob->iProducerId;
				pResult->iJobId = pJob->iJobId;
				pResult->iInput = pJob->iInput;
				pResult->iOutput = pJob->iInput * pJob->iInput;

				if ( !xPromiseResolve(pJob->pPromise, pResult) ) {
					xrtFree(pResult);
				}
			}

			xPromiseDestroy(pJob->pPromise);
			xrtFree(pJob);
			pItem = NULL;
			continue;
		}
		if ( iRet == XQUEUE_TIMEOUT ) {
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

static uint32 procProducer(ptr pArg)
{
	DemoProducerCtx* pCtx = (DemoProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iJobCount; i++ ) {
		xfuture* pFuture = xFutureCreate();
		xpromise* pPromise;
		DemoQueueJob* pJob;

		if ( pFuture == NULL ) {
			return 1;
		}

		pPromise = xPromiseCreate(pFuture);
		if ( pPromise == NULL ) {
			xFutureRelease(pFuture);
			return 2;
		}

		pCtx->arrFuture[i] = pFuture;

		pJob = (DemoQueueJob*)xrtMalloc(sizeof(DemoQueueJob));
		if ( pJob == NULL ) {
			(void)xPromiseReject(pPromise, XRT_NET_ERROR, (str)"alloc job failed");
			xPromiseDestroy(pPromise);
			continue;
		}

		memset(pJob, 0, sizeof(DemoQueueJob));
		pJob->iProducerId = pCtx->iProducerId;
		pJob->iJobId = i + 1;
		pJob->iInput = pCtx->iBaseValue + i + 1;
		pJob->pPromise = pPromise;

		for ( ;; ) {
			xqueue_result iRet = xrtMPSCQWaitTryPush(pCtx->hQueue, pJob);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}

			procRejectJob(pJob, XRT_NET_ERROR, (str)"queue closed before submit");
			break;
		}
	}

	return 0;
}

static void procPrintFutureResult(xfuture* pFuture)
{
	const char* sError;
	DemoQueueResult* pResult;

	if ( pFuture == NULL ) {
		printf("future missing\n");
		return;
	}

	if ( !xFutureWaitTimeout(pFuture, 3000) ) {
		printf("future wait timeout\n");
		return;
	}

	if ( xFutureState(pFuture) != XFUTURE_RESOLVED ) {
		sError = xFutureError(pFuture);
		printf(
			"job failed: state=%d status=%d error=%s\n",
			(int)xFutureState(pFuture),
			(int)xFutureStatus(pFuture),
			(sError != NULL) ? sError : "(null)"
		);
		return;
	}

	pResult = (DemoQueueResult*)xFutureValue(pFuture);
	if ( pResult == NULL ) {
		printf("job resolved with null result\n");
		return;
	}

	printf(
		"producer=%d job=%d input=%d output=%d\n",
		(int)pResult->iProducerId,
		(int)pResult->iJobId,
		(int)pResult->iInput,
		(int)pResult->iOutput
	);
	xrtFree(pResult);
}

static void procReleaseFutureTable(
	xfuture* arrFuture[][DEMO_JOB_PER_PRODUCER],
	int32 iProducerCount
)
{
	int32 i;
	int32 j;

	for ( i = 0; i < iProducerCount; i++ ) {
		for ( j = 0; j < DEMO_JOB_PER_PRODUCER; j++ ) {
			if ( arrFuture[i][j] != NULL ) {
				xFutureRelease(arrFuture[i][j]);
				arrFuture[i][j] = NULL;
			}
		}
	}
}

int main(void)
{
	xqueue_config tCfg;
	xmpscqwait hQueue;
	xthread hWorker;
	xthread arrProducerThread[DEMO_PRODUCER_COUNT];
	DemoWorkerCtx tWorkerCtx;
	DemoProducerCtx arrProducerCtx[DEMO_PRODUCER_COUNT];
	xfuture* arrFuture[DEMO_PRODUCER_COUNT][DEMO_JOB_PER_PRODUCER];
	int32 iProducerStarted;
	int32 i;
	int32 j;

	xrtInit();
	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tWorkerCtx, 0, sizeof(tWorkerCtx));
	memset(arrProducerThread, 0, sizeof(arrProducerThread));
	memset(arrProducerCtx, 0, sizeof(arrProducerCtx));
	memset(arrFuture, 0, sizeof(arrFuture));

	tCfg.iCapacity = 8u;
	hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	tWorkerCtx.hQueue = hQueue;
	hWorker = xrtThreadCreate(procQueueWorker, &tWorkerCtx, 0);
	if ( hWorker == NULL ) {
		xrtMPSCQWaitDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	iProducerStarted = 0;

	for ( i = 0; i < DEMO_PRODUCER_COUNT; i++ ) {
		arrProducerCtx[i].hQueue = hQueue;
		arrProducerCtx[i].iProducerId = i + 1;
		arrProducerCtx[i].iBaseValue = i * 100;
		arrProducerCtx[i].iJobCount = DEMO_JOB_PER_PRODUCER;
		arrProducerCtx[i].arrFuture = arrFuture[i];

		arrProducerThread[i] = xrtThreadCreate(procProducer, &arrProducerCtx[i], 0);
		if ( arrProducerThread[i] == NULL ) {
			xrtMPSCQWaitClose(hQueue);
			for ( j = 0; j < iProducerStarted; j++ ) {
				xrtThreadWait(arrProducerThread[j]);
				xrtThreadDestroy(arrProducerThread[j]);
			}
			xrtThreadWait(hWorker);
			procCleanupResidualJobs(hQueue);
			procReleaseFutureTable(arrFuture, DEMO_PRODUCER_COUNT);
			xrtThreadDestroy(hWorker);
			xrtMPSCQWaitDestroy(hQueue);
			xrtUnit();
			return 1;
		}

		iProducerStarted++;
	}

	for ( i = 0; i < DEMO_PRODUCER_COUNT; i++ ) {
		xrtThreadWait(arrProducerThread[i]);
		xrtThreadDestroy(arrProducerThread[i]);
	}

	xrtMPSCQWaitClose(hQueue);
	xrtThreadWait(hWorker);
	procCleanupResidualJobs(hQueue);

	for ( i = 0; i < DEMO_PRODUCER_COUNT; i++ ) {
		for ( j = 0; j < DEMO_JOB_PER_PRODUCER; j++ ) {
			procPrintFutureResult(arrFuture[i][j]);
			if ( arrFuture[i][j] != NULL ) {
				xFutureRelease(arrFuture[i][j]);
				arrFuture[i][j] = NULL;
			}
		}
	}

	xrtThreadDestroy(hWorker);
	xrtMPSCQWaitDestroy(hQueue);
	xrtUnit();
	return 0;
}
```


## 6. 这段代码最重要的 3 个边界

这页真正想讲透的，不是“算平方”本身，而是下面 3 个边界。

### 6.1 交接边界

生产者并不直接调用业务函数，而是：

1. 创建 `DemoQueueJob`
2. 创建 `future + promise`
3. 把 `DemoQueueJob*` 推进 `xmpscqwait`

也就是说：

- 生产者只负责提交
- worker 才负责执行

这会让模块职责非常清楚。


### 6.2 结果边界

worker 并不去回写生产者线程里的共享数组，而是：

- 成功就 `xPromiseResolve(...)`
- 失败就 `xPromiseReject(...)`

这样每个任务都会得到一个正式结果对象：

- `xfuture`

主线程后面只需要拿着这些 future 逐个等待和读取即可。


### 6.3 shutdown 边界

这段代码把退出顺序写得很明确：

1. 先等所有 producer 线程结束
2. 再 `xrtMPSCQWaitClose(hQueue)`，宣布不再接新任务
3. worker 把已入队任务继续做完
4. 主线程等待 worker 退出
5. 最后释放 future、队列和线程对象

这就是“停止接单”和“处理完存量任务”分开的写法。


## 7. 所有权怎么理解

这部分一定要讲细，因为指针队列最容易在这里写错。

### 7.1 任务对象和 promise

`Push` 成功前：

- `DemoQueueJob*`
- `xpromise*`

都还是 producer 自己的。

`Push` 成功后：

- `DemoQueueJob*`
- `xpromise*`

才正式交给 worker。

所以代码里才会有这条规则：

- `Push` 失败时，由 producer 调 `procRejectJob(...)`
- `Push` 成功时，由 worker 负责 `Resolve/Reject + Destroy + Free`


### 7.2 future

`future` 不跟着任务对象一起转移给 worker。

它始终留在提交方这一侧，由调用方持有并在最终读取后：

- `xFutureRelease(...)`

这正是 `promise/future` 分离设计最重要的价值之一。


### 7.3 结果对象

示例里 `DemoQueueResult` 是堆对象。

worker `Resolve` 时只是把指针交进 future，真正读取并释放它的，是最终拿到结果的调用方：

- `xFutureValue(...)`
- `xrtFree(...)`

如果把结果对象放在 worker 的栈上，函数返回后地址就失效了。


## 8. 这条模型和 `task group` 的边界

这页和 [线程、协程与 Future 协同模型](thread-coroutine-future.md) 刻意形成对照：

- 那一页解决的是“一批有界任务怎样结构化收口”
- 这一页解决的是“任务怎样持续进入 worker，再把结果逐个回给提交方”

可以这样记：

- bounded fan-out / structured scope => 优先想 `task group`
- 持续 handoff / worker command queue => 优先想 `queue + future`

如果这两类问题混在一起看，后面就很容易把队列和结构化并发写乱。


## 9. 常见错误

### 9.1 错误一：把 `future` 塞进 worker 线程共享数组里回填

这样还是会回到“共享状态 + 锁”的老路。


### 9.2 错误二：`Close` 之后立刻 `Destroy` 队列

`Close` 的意思只是：

- 不再接收新任务

不是“立刻清空并退出”。


### 9.3 错误三：`Push` 失败后忘了处理 `promise`

这会让 `future` 永远 pending，调用方后面会误以为 worker 卡死了。


### 9.4 错误四：把栈上临时结果地址 `Resolve` 给 future

worker 返回后，这个地址马上失效。


### 9.5 错误五：把 `xmpscqwait` 当成“阻塞版 MPMC”

它仍然是：

- 多生产者
- 单消费者

这里只是额外给了阻塞式 `Pop`。


## 10. 建议继续阅读

- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
- [协程、Future 与 Task 入门](../guide/coroutine-future-task-intro.md)
- [Task Group 入门：从统一等待走到结构化收口](../guide/task-group-intro.md)
- [Queue API](../api/api-queue.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [线程、协程与 Future 协同模型](thread-coroutine-future.md)

---

**一句话总结：** 这条案例主线的关键不在“开了几个线程”，而在“producer 用 queue 把任务交给 worker，worker 用 promise 完成 future，把每个任务的结果单独还给调用方”；这正是 XRT 里“持续 handoff + 每任务结果”最稳的一条写法。
