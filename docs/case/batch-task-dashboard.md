# 把本地控制台服务升级成一个批处理任务面板

> 这个案例要解决的不是“怎么再开几个线程”，而是更贴近真实服务的问题：当一个本地控制台服务要一次接收一批目标、并行跑完一批子任务、最后再把整批结果统一发布到 JSON、HTML 和快照文件时，怎样让这条链路有正式的 scope 和收口点，而不是继续靠散落的 `future` 和计数器硬拼。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有上一页那个本地控制台服务：

- 它能读配置
- 能写日志
- 能用 `queue + thread` 跑后台任务
- 能同时提供 JSON 和 HTML

现在需求再往前走一步：

- `POST /api/batches` 一次提交一整批任务
- 一批任务里包含多个独立 item
- 每个 item 可以并行执行
- 整批任务什么时候真正结束，必须有一个正式完成点
- 批次完成后，要把汇总结果写回页面、JSON 和快照文件

如果这时还只沿用“队列里塞一个任务，worker 做完就回去”的模型，代码会很快出现 3 个问题：

- 一批 item 到底算不算同一个逻辑 scope，说不清
- 外层只能手工记 pending 计数，timeout 和取消会越来越乱
- 页面、日志、快照都想知道“整批什么时候结束”，但没有一个正式完成边界

所以这页要补出的，不是另一个慢任务示例，而是：

> 当后台工作从“单个任务”升级成“一个批次里再长出多条子任务”时，怎样把它变成正式的结构化批处理主线。


## 2. 为什么这次不能只靠 `queue`

先把最容易混淆的边界讲清楚。

### 2.1 `queue` 解决的是交接，不是批次 scope

`queue` 很适合做这些事：

- HTTP handler 把请求快速交给后台线程
- 生产者和消费者解耦
- 用 `Close / Drain` 做服务 shutdown

但 `queue` 不回答下面这些问题：

- 这一批 20 个子任务是不是同一批工作
- 什么时候不再允许往这批工作里继续加 child
- 谁来统一等待“整批完成”
- timeout 或 shutdown 时，这一批 child 怎么一起取消


### 2.2 `future` 解决的是单个结果，不是整批收口

`future` 当然仍然重要，因为：

- 每个 item 都要有自己的完成态
- 每个 item 都可能成功、失败或超时

但如果你只拿到一组 `xfuture*`，你仍然需要自己做：

- 这批 future 的归属管理
- close-aware final barrier
- 父任务取消时对子任务的传播

这正是 `task group` 要补上的那层。


### 2.3 这次真正新增的是“批次 scope”

更稳的理解方式是：

- `queue`：把“启动一个批次”的请求交给后台
- `task group`：把“这一批 child”组织成一个正式 scope
- `future`：承接每个 item 和整个 group 的完成结果

一句话记住：

> `queue` 负责把批次送到后台，`task group` 负责把批次内部的 child 正式收口。


## 3. 这条主线里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `config + path + file` | 面板配置、模板路径、快照路径 | 服务落到真实文件系统 |
| `logging` | 记录批次开始、超时、完成 | 给运维和排错留下稳定事件 |
| `queue + thread` | 接住 HTTP 提交的批次请求 | HTTP 不阻塞，后台统一调度 |
| `task group` | 管住一个批次内部的所有 child | `Close / Join / Cancel / Destroy` |
| `future` | 承接每个 item 和 group join | 统一结果与等待边界 |
| `xvalue + json` | 承接最新批次摘要 | 页面、API、快照共享同一份模型 |
| `xhttpd + template` | 展示批次状态与触发入口 | 给脚本和浏览器一个稳定出口 |

这页真正要你建立的心智模型是：

1. HTTP 只负责“接单”
2. worker 只负责“启动一个批次”
3. `task group` 负责“管住一批 child”
4. snapshot / JSON / HTML 只消费“批次已经收口后的摘要”


## 4. 这次的目录和输出约定

继续沿用本地控制台服务的基础目录，只是把快照文件改成更贴近批处理：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-batch.json
```

其中：

- `config/console.json` 保留服务监听地址、并发上限、默认批大小
- `web/dashboard.html` 继续作为启动时预编译的模板
- `runtime/console.log` 记录批次开始、超时、完成
- `runtime/last-batch.json` 保存最近一个已收口批次的摘要


## 5. 先把 worker 内部的 batch scope 立起来

下面这段代码先不把 HTTP 和模板全塞进来，而是先把最关键的“worker 收到一个批次请求后，怎样用 `task group` 管住整个批次”讲清楚。

它故意保持为一个可编译、可运行的最小骨架：

- `main()` 模拟提交一个批次
- `procRunBatch()` 扮演后台 worker 里的“批次协调器”
- 每个 item 用 `xTaskGroupRunThread()` 作为一个 child task
- `xTaskGroupJoinFuture()` 给出整批 close-aware 完成点

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iItemId;
	int32 iDelayMs;
	bool bShouldFail;
	bool bDone;
	int32 iStatus;
} DemoBatchItem;

typedef struct
{
	int32 iBatchId;
	int32 iItemCount;
	DemoBatchItem arrItem[8];
} DemoBatchRequest;

typedef struct
{
	int32 iLastBatchId;
	int32 iDoneCount;
	int32 iFailCount;
	bool bFinished;
} DemoBatchStats;

static int32 procBatchItemTask(ptr pArg, xfuture_result* pOut)
{
	DemoBatchItem* pItem = (DemoBatchItem*)pArg;

	if ( (pItem == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pItem->iDelayMs);
	pItem->bDone = TRUE;
	pItem->iStatus = pItem->bShouldFail ? XRT_NET_ERROR : XRT_NET_OK;

	pOut->pValue = pItem;
	pOut->iStatus = pItem->iStatus;
	return pItem->iStatus;
}

static bool procRunBatch(DemoBatchRequest* pReq, DemoBatchStats* pStats, int64 iTimeoutMs)
{
	xtaskgroup* pGroup = NULL;
	xfuture* arrFuture[8];
	xfuture* pJoin = NULL;
	int i;

	memset(arrFuture, 0, sizeof(arrFuture));
	memset(pStats, 0, sizeof(*pStats));

	if ( (pReq == NULL) || (pStats == NULL) ) {
		return FALSE;
	}
	if ( (pReq->iItemCount <= 0) || (pReq->iItemCount > 8) ) {
		return FALSE;
	}

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return FALSE;
	}

	for ( i = 0; i < pReq->iItemCount; i++ ) {
		arrFuture[i] = xTaskGroupRunThread(pGroup, procBatchItemTask, &pReq->arrItem[i], 0u);
		if ( arrFuture[i] == NULL ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);

	if ( !xFutureWaitTimeout(pJoin, iTimeoutMs) ) {
		printf("batch-%d timeout\n", (int)pReq->iBatchId);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pStats->iLastBatchId = pReq->iBatchId;
	pStats->bFinished = TRUE;

	for ( i = 0; i < pReq->iItemCount; i++ ) {
		DemoBatchItem* pItem = (DemoBatchItem*)xFutureValue(arrFuture[i]);

		if ( pItem == NULL ) {
			pStats->iFailCount++;
			continue;
		}

		if ( pItem->iStatus == XRT_NET_OK ) {
			pStats->iDoneCount++;
		} else {
			pStats->iFailCount++;
		}
	}

cleanup:
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < pReq->iItemCount; i++ ) {
		if ( arrFuture[i] != NULL ) {
			xFutureRelease(arrFuture[i]);
		}
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}

	return pStats->bFinished;
}

int main(void)
{
	DemoBatchRequest tReq;
	DemoBatchStats tStats;

	xrtInit();
	memset(&tReq, 0, sizeof(tReq));
	memset(&tStats, 0, sizeof(tStats));

	tReq.iBatchId = 101;
	tReq.iItemCount = 3;

	tReq.arrItem[0].iItemId = 1;
	tReq.arrItem[0].iDelayMs = 100;
	tReq.arrItem[0].bShouldFail = FALSE;

	tReq.arrItem[1].iItemId = 2;
	tReq.arrItem[1].iDelayMs = 150;
	tReq.arrItem[1].bShouldFail = TRUE;

	tReq.arrItem[2].iItemId = 3;
	tReq.arrItem[2].iDelayMs = 120;
	tReq.arrItem[2].bShouldFail = FALSE;

	if ( procRunBatch(&tReq, &tStats, 3000) ) {
		printf(
			"batch-%d finished: done=%d fail=%d\n",
			(int)tStats.iLastBatchId,
			(int)tStats.iDoneCount,
			(int)tStats.iFailCount
		);
	} else {
		printf("batch failed\n");
	}

	xrtUnit();
	return 0;
}
```


## 6. 这段代码里最关键的 5 个点

### 6.1 `task group` 是在 worker 内部创建的

最稳的落点不是：

- HTTP handler 自己拼一组 `future`

而是：

- HTTP 只投递一个批次请求
- worker 真正收到批次后，再创建 `task group`

这样批次 scope 就和后台协调器绑定，而不是和请求线程绑定。


### 6.2 `JoinFuture` 先拿，`Close` 再宣布 stop-growing

这段骨架刻意保持了当前正式主线的写法：

1. 批次 child 全部发出去
2. `pJoin = xTaskGroupJoinFuture(pGroup)`
3. `xTaskGroupClose(pGroup)`

这里的 `Close` 不是“立刻结束”，而是：

> 从这一刻开始，这个批次不再接收新的 child。


### 6.3 timeout 等的是整批，而不是某个 child

这页最关键的等待边界是：

```c
if ( !xFutureWaitTimeout(pJoin, iTimeoutMs) ) {
	xTaskGroupCancel(pGroup);
	...
}
```

它表达的不是：

- 第 3 个 item 有没有结束

而是：

- 这个批次作为一个整体，是否在 deadline 前正式收口


### 6.4 `queue` 仍然重要，但它不再承担整批收口

真实服务里，你仍然应该保留：

- `POST /api/batches` -> `xmpscqwait`

因为 HTTP handler 仍然不能直接阻塞在批次执行上。

但从 `queue` 里取出请求之后，真正该发生的是：

- 创建 `task group`
- 发 child
- `Close`
- `Join`
- 发布摘要

也就是说：

> `queue` 负责把批次送到后台，`task group` 负责把后台内部的批次收口。


### 6.5 最终发布的永远是“摘要”，不是活的 child

页面、JSON 和快照真正需要的不是：

- 一堆还活着的 `xfuture*`

它们需要的是：

- 批次编号
- item 总数
- 成功数
- 失败数
- 最后完成时间
- 最近一条摘要文本

所以 `task group` 收口之后，应该立刻把这些信息拷贝成稳定摘要，再交给：

- `xvalue + json`
- `template`
- 日志


## 7. 接回 HTTP、页面和快照时怎么落

有了上面的批次协调器，真实服务里的主线就很清楚了。

### 7.1 `POST /api/batches`

只做 4 件事：

1. 解析请求体
2. 生成一个 `BatchRequest`
3. 推入 `xmpscqwait`
4. 立即返回“已接收”

不要在 handler 里：

- 直接 `xTaskGroupRunThread()`
- 直接 `WaitTimeout()`
- 直接拼页面摘要


### 7.2 worker 线程

worker 从队列里拿到一个 `BatchRequest` 后：

1. 记录“批次开始”日志
2. 调 `procRunBatch()`
3. 生成 `xvalue` 摘要对象
4. 同步更新内存里的 latest dashboard model
5. 写 `runtime/last-batch.json`
6. 记录“批次完成”日志


### 7.3 `GET /api/batches/latest`

直接把最新摘要模型导出成 JSON。

重点不是“再算一次”，而是：

- 只读 worker 已经收口后的稳定摘要


### 7.4 `GET /dashboard`

继续沿用上一页的思路：

- 模板只消费最新摘要模型
- 不直接碰活的 child task
- 不在页面渲染时临时做批次统计


## 8. 一个更接近真实服务的扩展方向

上面这份骨架已经把批次 scope 立住了，但真实服务通常还会继续加 3 层：

### 8.1 并发上限

不要无限制把一个批次里的所有 child 都直接跑出去。

更稳的做法是：

- 配置里放一个 `batch_parallelism`
- worker 按窗口分批 `RunThread`
- 或者拆成父批次 + 子 scope


### 8.2 取消接口

如果面板要提供：

- `POST /api/batches/{id}/cancel`

那就不能只保留最终摘要，还要在运行中维护：

- 当前活跃批次表
- `batch_id -> xtaskgroup*` 的可控引用

但这里也要保持边界：

- HTTP 只是发取消请求
- 真正的 `xTaskGroupCancel()` 仍由后台线程执行


### 8.3 历史批次索引

如果面板不只显示 latest，而是要显示最近 100 批：

- latest summary 继续保留一个快照对象
- history list 则可以用 `list / avltree / mempool`

这就会自然接到前面那条容器与内存池业务主线。


## 9. 这页最容易写错的地方

### 9.1 把 `queue` 当成整批等待模型

这是最常见误区。

`queue` 只知道：

- 有请求来了
- 有请求被取走了

它不知道：

- 一个批次内部长了多少 child
- 什么时候真的 close-aware 完成


### 9.2 只拿 child future，不拿 group join future

如果你只保留：

- `arrFuture[i]`

最后代码会退化成：

- 自己数一遍有没有都结束
- 自己判断什么时候算整批完成

这恰好又退回了没有结构化收口的状态。


### 9.3 `Close` 太早或太晚

太早会导致：

- 合法 child 还没发完，group 已经拒绝新增

太晚会导致：

- `JoinFuture` 一直保持 open-aware 等待
- 你以为批次已经结束，其实 scope 还没 stop-growing


### 9.4 页面直接读活的批次对象

页面和 JSON 输出应只读取：

- 已复制的稳定摘要

而不是直接扫：

- 活跃 child future
- 运行中的 group
- worker 内部的临时数组


## 10. 这页之后最自然的下一步

如果你读到这里，接下来最适合继续补的是两类案例：

1. 缓存型业务变体
	把“批次完成后的稳定摘要”再挂到缓存和过期策略上
2. 多租户批次索引
	把“单个 latest batch”升级成“tenant + batch_id”的多索引组织

如果你还没完全建立 `task group` 的结构化收口心智，建议先回去复读：

- [Task Group 入门：从统一等待走到结构化收口](../guide/task-group-intro.md)
- [把配置、日志、任务、网络和模板串成一个本地控制台服务](local-console-service.md)
- [把本地控制台服务升级成一个子进程探测面板](subprocess-probe-dashboard.md)
