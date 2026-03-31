# 线程、协程与 Future 协同模型

> 这个案例不再只讲“线程负责什么、协程负责什么”的概念分工，而是把 `thread + future + wait-source + task group + coroutine` 串成一条完整的结构化并发链路。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个“批量汇总报告”逻辑，它有 4 个约束：

1. 有几段独立工作适合放到线程里并行执行
2. 外层希望保留顺序代码式的编排体验
3. 整批工作要有统一 timeout
4. 这批子任务结束后，要有一个明确的结构化收口点

如果没有统一主线，程序很容易变成：

- 线程句柄自己管
- 结果靠共享状态和锁拼出来
- timeout 靠外层临时打补丁
- 最后到底“哪一批任务算结束了”说不清

这个案例要解决的，正是这 4 个问题同时出现时该怎么写。


## 2. 为什么这次不用别的方案

这一页故意不是“所有多任务能力都上一遍”，而是只选当前最合适的一组。

### 2.1 为什么不是“直接开几个线程然后逐个 Wait”

因为那样你虽然能把任务跑起来，但仍然会缺：

- 统一结果对象
- 统一 timeout 入口
- 这批子任务的正式 scope


### 2.2 为什么不是 `queue`

因为这次的问题不是：

- 持续生产 / 持续消费
- 模块间消息交接

而是：

- 一批有界子任务
- 一次 fan-out
- 一次统一 join

所以这页重点不在 `queue`。


### 2.3 为什么不是“直接 `WhenAll` 一个 future 数组”

因为这次你不只是需要一个组合结果，还需要：

- 一个正式 scope
- `Close`
- `Cancel`
- `Destroy`
- 后续可扩展成 child scope

这正是 `task group` 比“future 数组”更合适的地方。


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `thread task` | 执行独立工作 | 真正的并行执行单元 |
| `future` | 承接每个子任务结果 | 统一结果模型 |
| `task group` | 管住这批 child | `Close / Join / Cancel / Destroy` |
| `join future` | 承接整个 scope 的完成点 | close-aware final barrier |
| `wait-source` | 把 join 变成统一等待对象 | 协程里统一 timeout 等待 |
| `coroutine` | 编排整条流程 | 顺序代码式 orchestration |

可以先记一句话：

> 线程负责干活，future 负责结果，task group 负责这批任务的 scope，wait-source 负责统一等待，协程负责把整条流程写清楚。


## 4. 代码目标

下面这段完整代码会做 5 件事：

1. 主线程初始化 runtime 和协程调度器
2. 协程创建一个 `task group`
3. group 内发起 3 个线程任务
4. 把 group 的 `join future` 包成 `wait-source`，在协程里统一等待
5. 成功就汇总结果，超时就统一取消并收口


## 5. 完整代码

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iInput;
	int32 iOutput;
} DemoPartJob;

typedef struct
{
	DemoPartJob arrJob[3];
	int32 iTotal;
	bool bTimeout;
} DemoReportCtx;

static int32 procPartTask(ptr pArg, xfuture_result* pOut)
{
	DemoPartJob* pJob = (DemoPartJob*)pArg;

	if ( (pJob == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(100);
	pJob->iOutput = pJob->iInput * pJob->iInput;
	pOut->pValue = pJob;
	return XRT_NET_OK;
}

static void procReportCo(ptr pArg)
{
	DemoReportCtx* pCtx = (DemoReportCtx*)pArg;
	xtaskgroup* pGroup;
	xfuture* arrFuture[3];
	xfuture* pJoin;
	xwaitsrc tSrc;
	int32 iSum;
	int i;

	memset(arrFuture, 0, sizeof(arrFuture));
	pJoin = NULL;
	iSum = 0;

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return;
	}

	for ( i = 0; i < 3; i++ ) {
		arrFuture[i] = xTaskGroupRunThread(pGroup, procPartTask, &pCtx->arrJob[i], 0);
		if ( arrFuture[i] == NULL ) {
			xTaskGroupCancel(pGroup);
			xTaskGroupDestroy(pGroup);
			goto cleanup;
		}
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		xTaskGroupDestroy(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);

	tSrc = xWaitSourceFromFuture(pJoin);
	if ( !xWaitSourceWaitCoTimeout(&tSrc, 3000) ) {
		pCtx->bTimeout = TRUE;
		xTaskGroupCancel(pGroup);
		xFutureRelease(pJoin);
		xTaskGroupDestroy(pGroup);
		goto cleanup;
	}

	for ( i = 0; i < 3; i++ ) {
		DemoPartJob* pJob = (DemoPartJob*)xFutureValue(arrFuture[i]);
		if ( pJob != NULL ) {
			iSum += pJob->iOutput;
		}
	}

	pCtx->iTotal = iSum;

	xFutureRelease(pJoin);
	xTaskGroupDestroy(pGroup);

cleanup:
	for ( i = 0; i < 3; i++ ) {
		if ( arrFuture[i] != NULL ) {
			xFutureRelease(arrFuture[i]);
		}
	}
}

int main(void)
{
	xcosched* pSched;
	DemoReportCtx tCtx;

	xrtInit();
	memset(&tCtx, 0, sizeof(tCtx));

	tCtx.arrJob[0].iInput = 10;
	tCtx.arrJob[1].iInput = 20;
	tCtx.arrJob[2].iInput = 30;

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		xrtUnit();
		return 1;
	}

	if ( xrtCoSchedSpawn(pSched, procReportCo, &tCtx, 0) == NULL ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 1;
	}

	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);

	if ( tCtx.bTimeout ) {
		printf("report timeout\n");
	} else {
		printf("total = %d\n", (int)tCtx.iTotal);
	}

	xrtUnit();
	return 0;
}
```


## 6. 这段代码为什么是“结构化”的

这段代码最重要的不是“算出三个平方再求和”，而是它把收口边界写清楚了。

### 6.1 fan-out 点是清晰的

这批子任务都是从同一个 `pGroup` 发出去的：

- `xTaskGroupRunThread(...)`

所以它们天然属于同一个 scope。


### 6.2 stop-growing 点是清晰的

当这批任务都发完后，立刻：

- `xTaskGroupClose(pGroup)`

这等价于明确告诉系统：

> 这批子任务不会再继续长了。


### 6.3 final barrier 也是清晰的

代码不是自己数计数器、自己拼状态，而是：

- `pJoin = xTaskGroupJoinFuture(pGroup)`

这让“这整个 scope 什么时候算结束”变成了一个正式对象。


### 6.4 wait 入口仍然统一

协程没有直接硬编码成：

- 只会等某一个 `future`

而是把 `join future` 再转成：

- `xwaitsrc`

然后统一使用：

- `xWaitSourceWaitCoTimeout(...)`

这意味着以后如果这条链路的等待对象换成别的 `wait-source`，编排层心智模型不需要变。


## 7. 运行过程怎么理解

可以把上面的代码按下面顺序读：

1. `main()` 负责 runtime 和协程调度器
2. `procReportCo()` 是整条流程的 orchestrator
3. 3 个 `procPartTask()` 在线程里并行执行
4. 每个线程任务各自产生一个 `future`
5. `task group` 再把“整批 child 的结束点”变成一个 `join future`
6. 协程通过 `wait-source` 等待这个 join
7. join 完成后，再统一读取 child future 里的结果

也就是说，这条链不是：

- 线程直接回调协程

而是：

- 线程 -> child future -> task group join future -> wait-source -> 协程继续执行


## 8. 这段代码为什么比“线程 + 共享状态”更稳

如果你不用这条链，最常见的写法通常会变成：

- 主线程开 3 个线程
- 每个线程把结果写回一个共享数组
- 再加锁
- 再加一个计数器
- 再手工判断是不是全部完成
- 再自己拼 timeout 和取消

这类代码短期能跑，但很快会变得难维护，因为：

- 结果合同不统一
- timeout 不统一
- 结束点不统一
- 这批任务到底是谁在“拥有”也不清楚

而现在这条链里：

- 子任务结果统一放进 `future`
- 这批任务统一归 `task group`
- timeout 统一走 `wait-source`
- 编排统一交给协程

结构边界会清楚很多。


## 9. 这次为什么没用 `CreateChild`

很多人学了 `task group` 后，容易一上来就想把 child scope 也写进第一个案例。

这页故意没这么做，因为这里的核心问题只是：

- 一批有界子任务怎么统一收口

一个 group 就够了。

如果后面这条“生成报告”的流程又要拆成：

- `download` 子 scope
- `parse` 子 scope
- `render` 子 scope

那时再上：

- `xTaskGroupCreateChild()`

会更自然。


## 10. 可以怎样继续扩展

这个案例后面最自然的升级方向有 4 个：

### 10.1 把线程任务换成 engine task

把：

- `xTaskGroupRunThread()`

换成：

- `xTaskGroupRunEngine()`

就能把执行层迁到 network engine worker。


### 10.2 把 `join future` 后面接 continuation

如果汇总步骤不想直接写在协程里，也可以在 join 之后继续挂：

- `Then*`
- `Catch*`
- `Finally*`


### 10.3 把 timeout 失败路径升级成统一错误对象

现在示例里只是写：

- `bTimeout = TRUE`

真实项目里可以把它升级成：

- 统一错误码
- 统一错误消息
- 统一上层响应对象


### 10.4 再往下接网络等待

如果后面某一步不再是线程任务，而是：

- stream readable
- dgram recv
- listener accept

那仍然可以沿着同一条：

- `wait-source`

主线继续写下去。


## 11. 常见错误

### 11.1 错误一：忘了 `Close`，却直接等 `JoinFuture`

从当前合同看，group 如果一直保持 open：

- `JoinFuture` 会认为后面还可能继续长 child

这时你会误以为 join 卡死了。


### 11.2 错误二：超时了只返回，不主动收口 group

这会让 pending child 继续跑下去，scope 边界就被打散了。

更稳的做法是：

- `Cancel`
- `Destroy`

至少要把收口动作写清楚。


### 11.3 错误三：把 `task group` 当成普通 future 列表

这样你就会忽略：

- `Close`
- `JoinFuture`
- `Cancel`
- `Destroy`

而这些才是结构化并发最重要的部分。


### 11.4 错误四：协程既负责编排，又亲自承担重 CPU 工作

这会把“编排层”和“执行层”重新混回去。

更稳的分工仍然是：

- 协程写流程
- 线程或 engine 真正干活


## 12. 建议继续阅读

- [Task Group 入门：从统一等待走到结构化收口](../guide/task-group-intro.md)
- [Wait-Source 入门：把 Future 和网络等待说成同一种语言](../guide/wait-source-intro.md)
- [协程、Future 与 Task 入门](../guide/coroutine-future-task-intro.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [Coroutine API](../api/api-coroutine.md)

---

**一句话总结：** 这条案例主线的关键不在“开了几个线程”，而在“线程结果统一进 future，这批 future 统一归 task group，这个 group 再通过 join future 和 wait-source 被协程正式收口”；这就是 XRT 当前结构化并发最值得掌握的写法之一。
