# XRT Task Group 入门：从统一等待走到结构化收口

> 目标：讲清楚为什么在 `future` 和 `wait-source` 之后还需要 `task group`，以及它如何用 `Close / ReapCompleted / Join / CreateChild` 把“一批属于同一逻辑 scope 的子任务”正式收口。

[返回教学文档](README.md) | [返回文档中心](../README.md)

---

## 0. 建议先读什么

如果你还没有建立 XRT 多任务整体心智模型，建议先读：

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
- [Wait-Source 入门：把 Future 和网络等待说成同一种语言](wait-source-intro.md)

这篇文章只聚焦最后一层：

- 为什么“统一等待”之后还需要“结构化收口”
- 为什么 `task group` 不是 future 数组
- 为什么 `Close / Join / CreateChild` 会改变你组织异步代码的方式


## 1. 为什么 `future` 和 `wait-source` 还不够

先回顾前两层分别解决了什么问题：

- `future` 解决“结果怎么统一表达”
- `wait-source` 解决“等待入口怎么统一”

它们已经能让你把：

- 线程任务
- 协程任务
- 网络等待

说成一套比较统一的异步语言。

但程序一旦开始真正管理“一批子任务”，新的问题会立刻出现：

1. 这几个 `future` 到底算不算同一批工作
2. 什么时候还允许继续往这批工作里加任务
3. 什么时候应该明确宣布“这批任务不会再长了”
4. 谁负责统一 `join`
5. 父任务取消时，子任务怎么一起收口
6. 子 scope 再长出自己的 child 时，外层怎么知道什么时候才算真的结束

如果没有正式的 scope 对象，代码很快会退化成：

- 到处散着 `xfuture*`
- 外层自己拼数组
- 一会儿自己 `WhenAll`
- 一会儿手工记录计数
- 一会儿手工做“关闭后不再接收新任务”的标记

这正是 `task group` 存在的原因。


## 2. `task group` 真正回答的是哪个问题

`task group` 解决的不是：

- 再提供一个 future 组合器

它真正回答的是：

> 这一批子任务属于同一个逻辑作用域时，怎样把它们当成“一个 scope”去关闭、等待、取消、回收和向父级传播。

所以更稳的理解方式是：

- `future`：单个结果对象
- `wait-source`：统一等待对象
- `task group`：一批子任务的作用域对象

这三层不是竞争关系，而是递进关系。


## 3. `future`、`wait-source`、`task group` 应该怎么分工

| 对象 | 它回答的问题 | 你真正得到的能力 |
|------|--------------|------------------|
| `future` | 结果是什么 | 状态、值、错误、取消、continuation |
| `wait-source` | 现在等谁 | 统一等待入口、统一 timeout / deadline、统一协程等待 |
| `task group` | 这一批任务怎么收口 | scope、close、reap、join、cancel、父子传播 |

可以记成一句话：

> `future` 负责结果，`wait-source` 负责统一等待，`task group` 负责结构化收口。


## 4. 先记住 7 条规则

这一页里最重要的不是函数名，而是合同。

### 4.1 open 状态下，group 还会继续长

只要 group 还没 `Close`，你就仍然可以：

- `xTaskGroupAddFuture()`
- `xTaskGroupRunThread()`
- `xTaskGroupRunCo()`
- `xTaskGroupRunEngine()`
- `xTaskGroupRunDelayed()`

这说明它不是静态数组，而是动态 scope。


### 4.2 `Close` 的意思是“不再接收新 child”

`xTaskGroupClose()` 的核心语义是：

- 关闭入口
- 拒绝后续新增 child

它**不等于**：

- 立刻销毁 group
- 立刻把当前 pending child 全部取消

这一点非常重要。


### 4.3 `Wait*` 更像“观察当前成员都完成了”

从当前 `api-future-task-promise.md` 和 `test_future_core_impl.h` 的测试合同看，更稳的理解方式是：

- `xTaskGroupWait / WaitTimeout / WaitUntil`
	- 适合等待“当前已跟踪 child”都进入终态
	- 不要求 group 先 `Close`
	- 也不意味着 group 自己已经完成最终收口

也就是说，`Wait*` 更像 barrier，而不是最终 `join`。


### 4.4 `JoinFuture / Join*` 是 close-aware final barrier

当前测试合同已经明确了：

- group open 阶段，`xTaskGroupJoinFuture()` 不会提前完成
- 后续新增 child 仍会被纳入 join
- 只有 `Close` 且没有 pending child 之后，join 才完成

所以：

- `Wait*` 更适合“看这一轮有没有都跑完”
- `Join*` 更适合“这整个 scope 什么时候算真的结束”


### 4.5 `ReapCompleted` 是给长生命周期 open group 用的

如果 group 还保持 open，但已经有一批 child 完成了，你可以用：

- `xTaskGroupReapCompleted()`

把已经完成的 child 从 group 里回收掉，避免统计和内存上的“已完成成员堆积”。


### 4.6 `Destroy` 不是普通 free

当前主线里，应当把：

- `xTaskGroupDestroy()`

理解成：

- `Close`
- 取消仍然 pending 的 child
- 销毁 group 自身

但要注意：

> group 销毁，不等于调用方手里持有的 `xfuture*` 会自动不用管。

如果你自己还持有 child future，后面仍然要 `xFutureRelease()`。


### 4.7 nested scope 优先走 `CreateChild`

如果你已经明确在做父子 scope，优先用：

- `xTaskGroupCreateChild()`

而不是手工拼：

- `BindParent`
- `AddFuture`
- 自己维护 parent join

这是当前主线里更正式、也更不容易写乱的入口。


## 5. 先认识一组核心 API

最常用的一组入口如下：

```c
XXAPI xtaskgroup* xTaskGroupCreate(void);
XXAPI void xTaskGroupDestroy(xtaskgroup* pGroup);
XXAPI void xTaskGroupClose(xtaskgroup* pGroup);

XXAPI bool xTaskGroupAddFuture(xtaskgroup* pGroup, xfuture* pFuture);
XXAPI int xTaskGroupCount(xtaskgroup* pGroup);
XXAPI int xTaskGroupReapCompleted(xtaskgroup* pGroup);

XXAPI xfuture* xTaskGroupJoinFuture(xtaskgroup* pGroup);
XXAPI bool xTaskGroupJoin(xtaskgroup* pGroup);
XXAPI bool xTaskGroupJoinTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);

XXAPI bool xTaskGroupWait(xtaskgroup* pGroup);
XXAPI bool xTaskGroupWaitTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);

XXAPI void xTaskGroupCancel(xtaskgroup* pGroup);
XXAPI xtaskgroup* xTaskGroupCreateChild(xtaskgroup* pParent);

XXAPI xfuture* xTaskGroupRunThread(xtaskgroup* pGroup, xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize);
XXAPI xfuture* xTaskGroupRunCo(xtaskgroup* pGroup, xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize);
XXAPI xfuture* xTaskGroupRunEngine(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg);
```

初学阶段先优先掌握：

- `Create / Close / Destroy`
- `RunThread / AddFuture`
- `Wait* / Join*`
- `ReapCompleted`
- `CreateChild`


## 6. DEMO 1：最小 scope，先学 `Close + Join`

第一步不要一上来就写 nested scope。

先学最基础的“这批任务已经属于同一个 scope，最后怎么统一收口”。

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iInput;
	int32 iOutput;
} DemoTaskGroupJob;

static int32 procSquareTask(ptr pArg, xfuture_result* pOut)
{
	DemoTaskGroupJob* pJob = (DemoTaskGroupJob*)pArg;

	if ( (pJob == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(100);
	pJob->iOutput = pJob->iInput * pJob->iInput;
	pOut->pValue = pJob;
	return XRT_NET_OK;
}

int main(void)
{
	xtaskgroup* pGroup;
	xfuture* pA;
	xfuture* pB;
	DemoTaskGroupJob tJobA;
	DemoTaskGroupJob tJobB;

	xrtInit();

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		xrtUnit();
		return 1;
	}

	tJobA.iInput = 10;
	tJobA.iOutput = 0;
	tJobB.iInput = 20;
	tJobB.iOutput = 0;

	pA = xTaskGroupRunThread(pGroup, procSquareTask, &tJobA, 0);
	pB = xTaskGroupRunThread(pGroup, procSquareTask, &tJobB, 0);
	if ( (pA == NULL) || (pB == NULL) ) {
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	xTaskGroupClose(pGroup);

	if ( !xTaskGroupJoinTimeout(pGroup, 3000) ) {
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	printf("job-a = %d\n", (int)((DemoTaskGroupJob*)xFutureValue(pA))->iOutput);
	printf("job-b = %d\n", (int)((DemoTaskGroupJob*)xFutureValue(pB))->iOutput);

	xFutureRelease(pA);
	xFutureRelease(pB);
	xTaskGroupDestroy(pGroup);
	xrtUnit();
	return 0;
}
```

这个 DEMO 最想让你建立的心智模型是：

1. `RunThread` 只是往 group 里发 child task
2. `Close` 是宣布“不再接新任务”
3. `JoinTimeout` 才是“把这一批任务正式收口”

如果你只记得一个动作顺序，就先记：

> 创建 group -> 往里加 child -> `Close` -> `Join`


## 7. DEMO 2：为什么 `JoinFuture` 不是普通 `WhenAll`

这一段非常关键，因为它决定你会不会把 `task group` 错看成 future 数组。

下面这个 DEMO 刻意不用 `RunThread`，而是直接用 `future + promise`，这样更容易看出：

- group open 时，join 不会提前完成
- 后续新增 future 会继续纳入 join
- 真正完成点在 `Close` 之后

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xtaskgroup* pGroup;
	xfuture* pA;
	xfuture* pB;
	xfuture* pJoin;
	xpromise* pPromiseA;
	xpromise* pPromiseB;
	int iValueA;
	int iValueB;

	xrtInit();

	pGroup = xTaskGroupCreate();
	pA = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	iValueA = 1301;

	if ( (pGroup == NULL) || (pA == NULL) || (pPromiseA == NULL) ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pA ) xFutureRelease(pA);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	xTaskGroupAddFuture(pGroup, pA);
	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	(void)xPromiseResolve(pPromiseA, &iValueA);

	if ( !xFutureWaitTimeout(pJoin, 20) ) {
		printf("join is still pending because group is still open\n");
	}

	pB = xFutureCreate();
	pPromiseB = xPromiseCreate(pB);
	iValueB = 1302;

	if ( (pB == NULL) || (pPromiseB == NULL) || !xTaskGroupAddFuture(pGroup, pB) ) {
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pB ) xFutureRelease(pB);
		xFutureRelease(pJoin);
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	(void)xPromiseResolve(pPromiseB, &iValueB);

	xTaskGroupClose(pGroup);

	if ( xFutureWaitTimeout(pJoin, 1000) ) {
		printf("join completes only after close\n");
	}

	(void)xTaskGroupJoinTimeout(pGroup, 1000);

	xFutureRelease(pJoin);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);
	xTaskGroupDestroy(pGroup);
	xrtUnit();
	return 0;
}
```

这一步最该看懂的是：

- `JoinFuture` 观察的是整个 scope
- scope 没 `Close`，它就认为“后面还可能长出新 child”
- 所以它不会像静态 `WhenAll` 那样，在当前集合完成后就立刻终态

这就是“close-aware dynamic join”的实际意义。


## 8. DEMO 3：为什么长生命周期 open group 需要 `ReapCompleted`

如果你的 group 不是“开完一批任务马上关”，而是像一个长期工作的协调器：

- 一段时间内持续接收新 child
- 一部分 child 已经完成
- 另一部分 child 还在跑

这时就不应该让所有已完成成员一直堆在 group 里。

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xtaskgroup* pGroup;
	xfuture* pDone;
	xfuture* pPending;
	xpromise* pDonePromise;
	xpromise* pPendingPromise;
	int iValue;
	int iReaped;

	xrtInit();

	pGroup = xTaskGroupCreate();
	pDone = xFutureCreate();
	pPending = xFutureCreate();
	pDonePromise = xPromiseCreate(pDone);
	pPendingPromise = xPromiseCreate(pPending);
	iValue = 1201;

	if ( (pGroup == NULL) || (pDone == NULL) || (pPending == NULL) || (pDonePromise == NULL) || (pPendingPromise == NULL) ) {
		if ( pDonePromise ) xPromiseDestroy(pDonePromise);
		if ( pPendingPromise ) xPromiseDestroy(pPendingPromise);
		if ( pDone ) xFutureRelease(pDone);
		if ( pPending ) xFutureRelease(pPending);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	xTaskGroupAddFuture(pGroup, pDone);
	xTaskGroupAddFuture(pGroup, pPending);

	(void)xPromiseResolve(pDonePromise, &iValue);

	iReaped = xTaskGroupReapCompleted(pGroup);
	printf("reaped = %d\n", iReaped);
	printf("count after reap = %d\n", xTaskGroupCount(pGroup));

	(void)xPromiseResolve(pPendingPromise, &iValue);

	xTaskGroupClose(pGroup);
	(void)xTaskGroupJoinTimeout(pGroup, 1000);

	xPromiseDestroy(pDonePromise);
	xPromiseDestroy(pPendingPromise);
	xFutureRelease(pDone);
	xFutureRelease(pPending);
	xTaskGroupDestroy(pGroup);
	xrtUnit();
	return 0;
}
```

这个 DEMO 要重点理解两点：

### 8.1 `ReapCompleted` 不是 `Join`

它做的是：

- 把已经完成的 child 从 group 当前统计里回收掉

它不做的是：

- 宣布 scope 关闭
- 等整个 scope 最终结束


### 8.2 它适合长期 open 的协调器

如果 group 还打算继续接新 child，那么：

- `Close + Join` 还太早

这时更稳的动作顺序通常是：

1. 持续接 child
2. 定期 `ReapCompleted`
3. 到明确结束点再 `Close + Join`


## 9. DEMO 4：nested scope，为什么要优先 `CreateChild`

真正让 `task group` 从“管理一批任务”升级成“结构化并发入口”的，是 child scope。

下面这个 DEMO 演示的重点是：

- parent 和 child 都是 scope
- child 的任务会被 parent 收进最终 join
- parent 关闭时，child 也会跟着被收口

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xtaskgroup* pParent;
	xtaskgroup* pChild;
	xfuture* pLeaf;
	xfuture* pParentJoin;
	xpromise* pLeafPromise;

	xrtInit();

	pParent = xTaskGroupCreate();
	pChild = xTaskGroupCreateChild(pParent);
	pLeaf = xFutureCreate();
	pLeafPromise = xPromiseCreate(pLeaf);

	if ( (pParent == NULL) || (pChild == NULL) || (pLeaf == NULL) || (pLeafPromise == NULL) ) {
		if ( pLeafPromise ) xPromiseDestroy(pLeafPromise);
		if ( pLeaf ) xFutureRelease(pLeaf);
		if ( pChild ) xTaskGroupDestroy(pChild);
		if ( pParent ) xTaskGroupDestroy(pParent);
		xrtUnit();
		return 1;
	}

	xTaskGroupAddFuture(pChild, pLeaf);
	pParentJoin = xTaskGroupJoinFuture(pParent);
	if ( pParentJoin == NULL ) {
		xPromiseDestroy(pLeafPromise);
		xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		xrtUnit();
		return 1;
	}

	xTaskGroupClose(pParent);

	if ( xFutureWaitTimeout(pParentJoin, 1000) ) {
		printf("parent scope joined\n");
	}

	if ( xFutureState(pLeaf) == XFUTURE_CANCELLED ) {
		printf("leaf child was cancelled by parent scope close\n");
	}

	xFutureRelease(pParentJoin);
	xPromiseDestroy(pLeafPromise);
	xFutureRelease(pLeaf);
	xTaskGroupDestroy(pChild);
	xTaskGroupDestroy(pParent);
	xrtUnit();
	return 0;
}
```

从当前 API 文档和测试合同看，这里最值得记住的是：

- child 会自动绑定到 parent scope
- child join 会自动纳入 parent
- parent `Close / Cancel / Destroy` 会向 child 传播收口语义

这就是为什么当前主线更推荐：

- `CreateChild`

而不是自己手工拼 parent-child 关系。


## 10. `Wait`、`Join`、`Cancel`、`Destroy` 到底怎么选

这 4 组动作最容易混。

| 动作 | 更适合的场景 | 不该误解成什么 |
|------|--------------|----------------|
| `Wait*` | 我只想等当前成员都进终态 | 最终 scope join |
| `JoinFuture / Join*` | 我已经准备把整个 scope 正式收口 | 普通 `WhenAll` |
| `Cancel` | 我想主动向当前 pending child 发取消请求 | 自动销毁 group |
| `Destroy` | 我已经不再需要这个 scope 对象 | 普通 free，不带收口语义 |

如果只记一条经验法则：

- “这一轮先看看是不是都跑完了”用 `Wait*`
- “这整个 scope 到此为止”用 `Close + Join*`


## 11. 什么时候还会用到 `BindParent`

大多数正式 nested scope 场景，优先用：

- `xTaskGroupCreateChild()`

但如果你已经有一个外部 parent future，希望它的取消能向这组 child 传播，这时才考虑：

- `xTaskGroupBindParent()`

例如：

```c
xtaskgroup* pGroup = xTaskGroupCreate();

xTaskGroupBindParent(pGroup, pParentFuture);
xTaskGroupAddFuture(pGroup, pChildFuture);
```

从当前测试合同看：

- 一个 group 可以绑定多个 parent future
- 其中任一个 parent cancel，都可能向当前 child scope 传播取消

所以这更适合：

- 和现有上游 future 树对接

而不是作为初学入口。


## 12. Windows / Linux 跨平台注意点

### 12.1 group 不替你托管调用方手里的 future 引用

例如：

- `xTaskGroupRunThread()` 会返回一个 `xfuture*`
- 你手里如果持有它，后面仍然要 `xFutureRelease()`

不要误以为：

- group 销毁了
- 你自己的引用就自动不需要管了


### 12.2 `Close` 不等于 `Cancel`

这是最容易写错的一点。

- `Close`
	- 不再接收新 child
- `Cancel`
	- 主动向当前 pending child 发取消请求

如果你把两者混成一件事，跨平台调试时会非常痛苦，因为你会看见：

- group 已经关闭
- 但 child 还在按合同自己跑到终态


### 12.3 长生命周期 group 要定期 `ReapCompleted`

Windows 和 Linux 在调度时机上会有差异，但这个结构性建议不变：

- 如果 group 长时间保持 open
- 又持续有 child 完成

就不要让它一直只加不回收。


## 13. 常见错误

### 13.1 错误一：把 `task group` 当成 future 数组

这样你就会忽略：

- `Close`
- `JoinFuture`
- `CreateChild`
- 取消传播

而这些恰恰才是它最重要的部分。


### 13.2 错误二：忘了 `Close`，却在等 `JoinFuture`

如果 group 一直保持 open，那么从当前合同看：

- `JoinFuture` 会一直认为“后面还可能继续长 child”

于是你会误以为程序卡死了。


### 13.3 错误三：以为 `Close` 会自动把当前 child 全部取消

不对。

如果你真正想取消：

- 用 `xTaskGroupCancel()`
- 或者最终 `Destroy()`


### 13.4 错误四：长生命周期 group 不做 `ReapCompleted`

这样已完成成员会一直挂在 group 统计里，后面你自己也会越来越难看清：

- 当前还剩多少 pending
- 当前 scope 到底是什么状态


### 13.5 错误五：明明是 nested scope，却手工拼 parent-child

如果当前场景天然就是父子作用域，优先用：

- `xTaskGroupCreateChild()`

这会比手工维护 parent bind、join add、cancel 传播更稳。


## 14. 下一步应该读什么

建议按这个顺序继续：

1. [Future / Task / Promise](../api/api-future-task-promise.md)
2. [线程、协程与 Future 协同模型](../case/thread-coroutine-future.md)
3. [Wait-Source 入门：把 Future 和网络等待说成同一种语言](wait-source-intro.md)
4. [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)

---

**一句话总结：** `task group` 不是“future 容器”，而是“一批子任务的结构化作用域”；`future` 负责结果，`wait-source` 负责统一等待，而 `task group` 负责宣布何时停止长出新任务、何时正式 join、何时把父子 scope 一起收口。
