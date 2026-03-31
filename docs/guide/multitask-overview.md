# XRT 多任务总论：线程、队列、协程与 Future 怎么选

> 目标：先讲清楚为什么需要多任务，再讲清楚 XRT 当前主线里的几种多任务方式分别解决什么问题、优缺点是什么，以及应该按什么顺序学习。

[返回教学文档](README.md) | [返回文档中心](../README.md)

---

## 1. 先回答最关键的问题：为什么需要多任务

很多人第一次写程序时，都是一条主线程一路往下跑：

1. 读配置
2. 读文件
3. 连网络
4. 等返回
5. 解析结果
6. 再继续下一步

在最小程序里，这没有问题。

问题出在一旦程序开始同时做几件事，单主线程会立刻出现下面这些瓶颈：

- 一件慢事会拖住所有事
- I/O 等待和 CPU 计算会互相抢时间
- 一个模块的阻塞会把另一个模块的响应时间一起拉长
- 程序越来越像“谁先卡住，整个流程就跟着卡住”

最典型的 3 类场景是：

### 1.1 等待外部资源

例如：

- 等文件读完
- 等网络返回
- 等子进程退出

这类场景的核心问题不是“CPU 不够”，而是“主线程被等待占住了”。


### 1.2 需要并行做独立工作

例如：

- 一边接收请求，一边做压缩或加密
- 一边处理当前任务，一边预读下一批数据
- 多个 worker 同时处理队列里的任务

这类场景的核心问题不是“代码写不下去”，而是“真正需要多个执行单元”。


### 1.3 需要把很多等待步骤写清楚

例如：

- 先发请求，再等响应，再根据响应继续发下一步
- 同时等多个 future，哪个先回来就先处理哪个
- 一个任务失败后，要走统一的清理和收尾流程

这类场景的核心问题不是“有没有线程”，而是“流程编排会不会变成回调地狱或状态机泥团”。


## 2. 单主线程到底卡在哪里

下面这个示例故意很简单，但它把单主线程的本质问题暴露得很清楚：

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xrtInit();

	printf("step-1 read config\n");
	xrtSleep(500);

	printf("step-2 call remote api\n");
	xrtSleep(2000);

	printf("step-3 parse and save result\n");
	xrtSleep(700);

	xrtUnit();
	return 0;
}
```

这段代码的问题不是逻辑错了，而是：

- 远端 API 一慢，后面的解析和保存就只能等
- 如果这时还要响应另一个请求，主线程根本腾不出来
- 如果中间再插一个 CPU 密集计算，卡顿会更明显

所以“多任务”本质上不是为了炫技，而是为了把下面这几件事拆开：

- 谁负责执行
- 谁负责等待
- 谁负责交接结果
- 谁负责组织整个流程


## 3. XRT 当前主线里的多任务工具箱

XRT 当前不是只给你一个“线程 API”，而是给了几层分工明确的工具：

### 3.1 `thread`

这是最传统、也最容易理解的一层。

它解决的是：

- 真正的并行执行
- 独立阻塞任务
- 需要脱离当前线程上下文的工作

它的优点：

- 直观
- 真并行
- 适合 CPU 密集或独立阻塞任务

它的代价：

- 线程切换有成本
- 生命周期管理比单线程复杂
- 线程越多，调试越难


### 3.2 `queue`

队列不是执行单元，它解决的是“交接”。

它最适合：

- 生产者 / 消费者
- 线程间消息投递
- worker 模型
- 背压和关闭流程

它的优点：

- 把“谁生产、谁消费”分开
- 很适合做模块边界
- 能把共享状态改成消息传递

它的代价：

- 队列只负责交接，不负责执行
- payload 生命周期需要你自己管
- 选错并发模型会平白增加复杂度


### 3.3 `coroutine`

协程解决的是“单线程内怎样把大量等待流程写得像顺序代码”。

它最适合：

- 单线程里的流程编排
- sleep / deadline / event wait
- `future / wait-source` 组合等待

它的优点：

- 代码顺序性强
- 很适合异步流程编排
- 不必为每个等待都开线程

它的边界：

- 协程不是抢占式并行
- 协程不适合自己承担重 CPU 工作
- 协程也不该替代所有线程


### 3.4 `future / task / promise`

这是 XRT 当前主线里最容易被低估的一层。

它解决的不是“多开一个执行单元”，而是：

- 怎样统一表示异步结果
- 怎样把线程、协程、engine worker 的结果说成同一种语言
- 怎样把等待、超时、取消、组合等待统一起来

当前推荐理解是：

- `task`：发起执行
- `future`：承接结果
- `promise`：写入结果


### 3.5 `wait-source`

`wait-source` 解决的是“等什么”。

它把这些东西统一成一种等待对象：

- future
- stream wait
- dgram recv
- listener accept

这层非常重要，因为它让“网络等待”和“普通异步结果等待”开始说同一种话。


### 3.6 `task group`

`task group` 解决的是“这批任务属于同一个逻辑 scope，怎么统一收口”。

它最适合：

- 同一批子任务统一 join
- 关闭后不再接收新任务
- 父任务取消时，子任务一起传播取消

它不是简单 future 数组，而是结构化并发的入口。


## 4. 先记住一张选型表

| 方式 | 你真正得到的能力 | 最适合 | 主要代价 |
|------|------------------|--------|----------|
| `thread` | 独立执行单元 | CPU 密集、独立阻塞任务 | 线程成本、生命周期复杂 |
| `queue` | 线程间交接与背压 | 生产者 / 消费者、worker | 不负责执行，所有权要自己管 |
| `coroutine` | 单线程流程编排 | 大量等待链、事件驱动流程 | 不能替代并行执行 |
| `future/task` | 统一异步结果与执行抽象 | 结果组合、超时、取消、延续 | 需要先建立统一心智模型 |
| `wait-source` | 统一等待接口 | future 与网络等待收口 | 更偏基础设施，不是第一眼最直观 |
| `task group` | 一批任务的 scope 管理 | 批任务 join / cancel / close | 适合中大型流程，不是最小 DEMO 首选 |


## 5. 推荐的学习顺序

如果你是第一次系统学习 XRT 多任务，推荐按这个顺序理解：

1. 线程为什么不能乱开
2. 队列为什么只是交接，不是执行
3. 协程为什么适合编排，不适合重活
4. `future / task` 为什么能把结果统一起来
5. `wait-source` 为什么能把网络和普通异步等待打通
6. `task group` 为什么是 scope，不只是容器

也可以记成一句话：

> 线程负责执行，队列负责交接，协程负责编排，future 负责结果，wait-source 负责统一等待，task group 负责成批收口。


## 6. 从最小 DEMO 开始

下面按“最小能跑 -> 更接近真实项目”的顺序走一遍。

### 6.1 DEMO 1：先学线程，理解“独立执行”

这个 DEMO 只做一件事：把一段慢工作交给 worker 线程。

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iJobId;
} DemoThreadCtx;

static uint32 procWorker(ptr pArg)
{
	DemoThreadCtx* pCtx = (DemoThreadCtx*)pArg;

	printf("worker %d start\n", (int)pCtx->iJobId);
	xrtSleep(500);
	printf("worker %d done\n", (int)pCtx->iJobId);
	return 0;
}

int main(void)
{
	xthread pThread;
	DemoThreadCtx tCtx;

	xrtInit();

	tCtx.iJobId = 1;
	pThread = xrtThreadCreate(procWorker, &tCtx, 0);
	if ( pThread == NULL ) {
		printf("create thread failed: %s\n", (char*)xrtGetError());
		xrtUnit();
		return 1;
	}

	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);

	xrtUnit();
	return 0;
}
```

这一步你应该先理解：

- `xrtThreadCreate()` 创建的是独立执行单元
- 由 XRT 创建的线程会自动附加到 runtime
- `xrtThreadDestroy()` 不是 kill，它只是销毁线程管理对象

适合这种方式的工作：

- CPU 密集计算
- 独立阻塞操作
- 必须脱离当前线程的宿主限制

不适合这种方式的工作：

- 只是想“等一下结果”
- 大量短小碎片任务
- 主要瓶颈是流程编排，而不是执行本身


### 6.2 DEMO 2：再学队列，理解“交接”和“背压”

如果程序已经不止一个线程，下一件最容易乱掉的事，就是线程之间怎么交接数据。

下面这个 DEMO 用 `xmpscqwait` 演示：

- 一个生产者线程投递数据
- 主线程阻塞式消费
- 队列本身不拥有 payload，需要调用方负责释放

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmpscqwait hQueue;
	int32 iStart;
} ProducerCtx;

static uint32 procProducer(ptr pArg)
{
	ProducerCtx* pCtx = (ProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < 3; i++ ) {
		int32* pValue = (int32*)xrtMalloc(sizeof(int32));
		if ( pValue == NULL ) {
			return 1;
		}

		*pValue = pCtx->iStart + i;

		while ( xrtMPSCQWaitTryPush(pCtx->hQueue, pValue) == XQUEUE_FULL ) {
			xrtSleep(1);
		}
	}

	return 0;
}

int main(void)
{
	xqueue_config tCfg;
	xmpscqwait hQueue;
	xthread pThread;
	ProducerCtx tCtx;
	ptr pItem;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 8;

	hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	tCtx.hQueue = hQueue;
	tCtx.iStart = 100;

	pThread = xrtThreadCreate(procProducer, &tCtx, 0);
	if ( pThread == NULL ) {
		xrtMPSCQWaitDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	for ( i = 0; i < 3; i++ ) {
		if ( xrtMPSCQWaitPopTimeout(hQueue, &pItem, 1000) == XQUEUE_OK ) {
			int32* pValue = (int32*)pItem;
			printf("pop value = %d\n", (int)*pValue);
			xrtFree(pValue);
		}
	}

	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	xrtMPSCQWaitClose(hQueue);
	xrtMPSCQWaitDestroy(hQueue);

	xrtUnit();
	return 0;
}
```

这一步要重点理解：

- 队列负责的是交接，不是执行
- `MPSC` 很适合“多个生产者 -> 一个消费者”
- `wait wrapper` 解决的是“消费者怎么阻塞等待”
- 队列是指针队列，payload 生命周期要自己管


### 6.3 DEMO 3：再学 `task + future`，理解“统一结果”

如果你继续只用“线程 + 共享变量 + 锁”，程序很快会遇到另一个问题：

- 线程能跑了
- 结果也算出来了
- 但每个模块都在用自己的返回协议

这时就该上 `task + future`。

```c
#include "xrt.h"
#include <stdio.h>

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

int main(void)
{
	int32 iValue;
	int32* pResult;
	xfuture* pFuture;

	xrtInit();

	iValue = 12;
	pFuture = xTaskRunThread(procSquare, &iValue, 0);
	if ( pFuture == NULL ) {
		xrtUnit();
		return 1;
	}

	pResult = (int32*)xFutureWaitValueTimeout(pFuture, 3000);
	if ( pResult != NULL ) {
		printf("square = %d\n", (int)*pResult);
		xrtFree(pResult);
	}

	xFutureRelease(pFuture);
	xrtUnit();
	return 0;
}
```

这一步要理解的重点是：

- `xTaskRunThread()` 把“线程执行”统一包装成 `future`
- 调用方开始等的是 `future`，而不是某个自定义共享变量
- 后面不管任务来自线程、协程还是 engine，结果模型都能统一


### 6.4 DEMO 4：最后学协程，理解“编排”

到了这一步，才适合把协程接进来。

原因很简单：

- 线程解决执行
- `future` 解决结果
- 协程最擅长的是把等待链写成顺序代码

下面这个 DEMO 演示：

- 协程作为编排层
- 两个计算任务在线程里执行
- 协程等待两个 `future`，最后在单线程里汇总结果

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iA;
	int32 iB;
} DemoCoCtx;

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

static void procMainCo(ptr pArg)
{
	DemoCoCtx* pCtx = (DemoCoCtx*)pArg;
	xfuture* pFutureA;
	xfuture* pFutureB;
	int32* pA;
	int32* pB;

	pFutureA = xTaskRunThread(procSquare, &pCtx->iA, 0);
	pFutureB = xTaskRunThread(procSquare, &pCtx->iB, 0);

	pA = (int32*)xFutureWaitCoValueTimeout(pFutureA, 3000);
	pB = (int32*)xFutureWaitCoValueTimeout(pFutureB, 3000);

	if ( (pA != NULL) && (pB != NULL) ) {
		printf("sum = %d\n", (int)(*pA + *pB));
	}

	if ( pA ) xrtFree(pA);
	if ( pB ) xrtFree(pB);
	xFutureRelease(pFutureA);
	xFutureRelease(pFutureB);
}

int main(void)
{
	xcosched* pSched;
	DemoCoCtx tCtx;

	xrtInit();

	tCtx.iA = 10;
	tCtx.iB = 20;

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		xrtUnit();
		return 1;
	}

	xrtCoSchedSpawn(pSched, procMainCo, &tCtx, 0);
	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);

	xrtUnit();
	return 0;
}
```

这个 DEMO 最想让你看懂的是：

- 协程没有自己去做重活
- 线程没有负责写流程
- `future` 成了线程和协程之间的结果桥

这正是 XRT 当前主线最推荐的分工方式。


## 7. 那 `wait-source` 和 `task group` 什么时候上

到了上面的第 4 个 DEMO，你已经能写出很多程序了。

接下来两层通常出现在更完整的项目里。

### 7.1 `wait-source`

当你开始同时等待这些对象时：

- future
- stream readable / writable
- dgram recv
- listener accept

就不应该再给每种对象单独发明一套等待协议，而应优先收口到 `xwaitsrc`。

它适合：

- 网络和非网络等待统一
- 协程里统一等待入口
- 组合等待和超时管理


### 7.2 `task group`

当你开始管理“一批属于同一个 scope 的子任务”时，就该切到 `task group`。

它适合：

- 一批子任务统一 join
- 动态增减 future
- close 后禁止继续加任务
- 父任务取消时把子任务一起收口

一句话理解：

> `task group` 不是“数组版 future”，而是结构化并发的作用域。


## 8. 常见误区

### 8.1 误区一：只要程序慢，就多开线程

错误。

如果瓶颈是：

- 等 I/O
- 等 future
- 等网络事件

那很多时候你真正需要的是：

- 协程
- `future`
- `wait-source`

而不是更多线程。


### 8.2 误区二：有了协程，就不需要线程

错误。

协程适合编排，不适合替代真正的并行执行。

CPU 密集任务、独立阻塞任务，依然应该优先交给线程或 engine worker。


### 8.3 误区三：队列能解决所有并发问题

错误。

队列只能解决交接。

它不能替代：

- 执行单元
- 结果模型
- 结构化等待


### 8.4 误区四：线程函数直接改共享状态就够了

短期能跑，长期会越来越乱。

更稳的做法通常是：

- 队列交接消息
- `future` 承接结果
- 协程统一等待和编排


## 9. 推荐记忆法

如果你现在只想先记住一条线，就记下面这句：

> 线程负责执行，队列负责交接，协程负责编排，future 负责结果，wait-source 负责统一等待，task group 负责成批收口。

只要这条线不乱，程序结构通常就不会太差。


## 10. 下一步应该读什么

建议按这个顺序继续：

1. [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)
2. [XRT 运行时与线程附加入门](runtime-thread-attach.md)
3. [Queue 队列模块](../api/api-queue.md)
4. [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
5. [Coroutine API](../api/api-coroutine.md)
6. [Future / Task / Promise](../api/api-future-task-promise.md)
7. [线程、协程与 Future 协同模型](../case/thread-coroutine-future.md)

---

**一句话总结：** 多任务不是“多开几个线程”这么简单；在 XRT 当前主线里，真正稳定的思路是先分清执行、交接、编排、结果和等待这几层，再决定该用线程、队列、协程还是 future。
