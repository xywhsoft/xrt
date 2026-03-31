# XRT 线程入门：什么时候该开线程，什么时候不该

> 目标：把“线程”和“一条主线程顺序执行”的区别讲清楚，再用最小 DEMO、停止协议、互斥体和条件变量 4 个层次，带你真正上手 XRT 的线程主线。

[返回教学文档](README.md) | [返回文档中心](../README.md)

---

## 0. 建议先读什么

如果你还没有建立整体心智模型，建议先读：

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
- [XRT 运行时与线程附加入门](runtime-thread-attach.md)

本文只聚焦一件事：

- 线程到底该在什么场景下用
- 在 XRT 里怎样把线程写得稳一点


## 1. 线程和主线程到底有什么区别

先别急着记 API，先把本质分清楚。

### 1.1 一条主线程的工作方式

一条主线程顺序执行时，逻辑通常像这样：

1. 做 A
2. A 没做完，就不能做 B
3. B 没做完，就不能做 C

它的优点是：

- 简单
- 调试直观
- 生命周期清楚

它的问题是：

- 只要有一段阻塞，后面的工作全都要排队
- 一段 CPU 密集计算会拖慢整个程序
- 稍微复杂一点的“同时进行”需求就会开始难写


### 1.2 线程带来的东西

线程带来的不是“异步感”，而是：

> 一个独立的执行单元。

这意味着：

- 主线程可以继续做自己的事
- worker 线程可以并行处理别的工作
- 两边可以通过同步原语或消息交接来协作

你真正得到的是：

- 独立调度
- 独立栈
- 独立线程级运行时状态

你没有自动得到的是：

- 清晰的结果模型
- 清晰的共享数据边界
- 自动正确的生命周期管理

这也是为什么线程很好用，但也最容易被滥用。


## 2. 什么时候应该优先用线程

下面这些场景，线程通常是合理的第一选择：

### 2.1 CPU 密集计算

例如：

- 压缩
- 加密
- 大量文本处理
- 批量解析

这类工作如果塞在主线程，很容易把整个程序拖慢。


### 2.2 独立阻塞操作

例如：

- 某个库只有阻塞接口
- 某个外部调用很慢，暂时又没法改成事件驱动
- 需要把一个长命令或长流程放到后台跑


### 2.3 明确的 worker 模型

例如：

- 后台日志处理线程
- 后台压缩线程
- 专门消费任务队列的 worker

这类场景里，线程的角色非常明确：

- 不是为了“等待”
- 是为了“执行”


## 3. 什么时候不该先上线程

下面这些场景，很多时候不应该第一反应就是开线程：

### 3.1 只是想等待多个异步步骤

如果你的主要问题是：

- 先等这个，再等那个
- 要组织一条等待链
- 要做超时、取消、组合等待

那你更该先考虑的是：

- 协程
- `future / task`
- `wait-source`


### 3.2 只是要在线程之间交接数据

如果问题本质是：

- 一个地方产出数据
- 另一个地方消费数据

那真正的主角通常是：

- `queue`

线程只是执行者，不是交接协议本身。


### 3.3 只是想“让主线程看起来不阻塞”

这类需求最容易演变成：

- 到处乱开线程
- 线程结束后把结果写到一堆共享字段里
- 后面再靠轮询和锁硬拼回来

短期能跑，长期会越来越难维护。


## 4. XRT 线程主线的 4 条基本规则

### 4.1 主线程用 `xrtInit()` 进入运行时

主线程通常不需要手工附加，`xrtInit()` 已经会处理：

- 全局运行时初始化
- 当前线程附加


### 4.2 `xrtThreadCreate()` 创建的线程会自动附加

这意味着在线程函数里，你可以直接使用很多运行时绑定能力，例如：

- `xrtGetError()`
- `xrtTempMemory()`
- 默认随机数


### 4.3 推荐顺序始终是 `Wait -> Destroy`

也就是：

1. `xrtThreadWait()`
2. `xrtThreadDestroy()`

`Destroy` 不是“结束线程”，而是“释放线程管理对象”。


### 4.4 `Stop` 是协作式停止，不是强杀

也就是：

- 外部发 `xrtThreadStop()`
- 线程内部主动检查 `xrtThreadShouldStop()`
- 线程自己走到安全退出点

这是推荐主线。

`xrtThreadKill()` 只是危险兜底，不应作为常规设计。


## 5. DEMO 1：第一个线程

先从最简单的开始：

- 主线程创建 worker
- worker 做一点慢工作
- 主线程等待它结束

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

	printf("worker %d start, tid=%llu\n",
		(int)pCtx->iJobId,
		(unsigned long long)xrtThreadGetCurrentId());

	xrtSleep(300);

	printf("worker %d done\n", (int)pCtx->iJobId);
	return 0;
}

int main(void)
{
	xthread hThread;
	DemoThreadCtx tCtx;

	xrtInit();

	tCtx.iJobId = 1;
	hThread = xrtThreadCreate(procWorker, &tCtx, 0);
	if ( hThread == NULL ) {
		printf("create thread failed: %s\n", (char*)xrtGetError());
		xrtUnit();
		return 1;
	}

	xrtThreadWait(hThread);
	xrtThreadDestroy(hThread);

	xrtUnit();
	return 0;
}
```

这一步重点不是功能，而是把线程生命周期记住：

- 创建
- 运行
- 等待
- 销毁


## 6. DEMO 2：正确停止线程

第二步一定要学停止协议。

因为现实里的线程通常不是“跑一下就结束”，而是：

- 循环工作
- 定期检查是否该退出

下面这个例子是 XRT 推荐思路：

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xthread* phThread;
	volatile int32 iCounter;
} DemoStopCtx;

static uint32 procLoop(ptr pArg)
{
	DemoStopCtx* pCtx = (DemoStopCtx*)pArg;
	int32 i;

	for ( i = 0; i < 1000; i++ ) {
		xthread hSelf = pCtx->phThread ? *pCtx->phThread : NULL;

		if ( hSelf && xrtThreadShouldStop(hSelf) ) {
			printf("worker stop requested\n");
			return 1;
		}

		pCtx->iCounter++;
		xrtSleep(10);
	}

	return 0;
}

int main(void)
{
	xthread hThread = NULL;
	DemoStopCtx tCtx;

	xrtInit();

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.phThread = &hThread;

	hThread = xrtThreadCreate(procLoop, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtUnit();
		return 1;
	}

	xrtSleep(80);
	xrtThreadStop(hThread);

	xrtThreadWait(hThread);
	printf("counter = %d, exit = %u\n",
		(int)tCtx.iCounter,
		(unsigned)xrtThreadGetExitCode(hThread));

	xrtThreadDestroy(hThread);
	xrtUnit();
	return 0;
}
```

这一步重点要理解：

- `Stop` 只是发信号
- 真正的退出点在线程内部
- 这样线程可以在安全位置收尾，而不是被外部突然打断


## 7. DEMO 3：共享数据要靠互斥体保护

线程一多，最容易出问题的就是共享状态。

下面这个 DEMO 只演示一件事：

- 多个线程都要改同一个计数器
- 不加锁就可能乱
- 加 `xmutex` 后边界会清楚很多

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmutex hLock;
	volatile int32 iCounter;
	int32 iLoopCount;
} DemoMutexCtx;

static uint32 procAdd(ptr pArg)
{
	DemoMutexCtx* pCtx = (DemoMutexCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iLoopCount; i++ ) {
		xrtMutexLock(pCtx->hLock);
		pCtx->iCounter++;
		xrtMutexUnlock(pCtx->hLock);
	}

	return 0;
}

int main(void)
{
	DemoMutexCtx tCtx;
	xthread arrThreads[4];
	int32 i;

	xrtInit();

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hLock = xrtMutexCreate();
	tCtx.iLoopCount = 10000;

	if ( tCtx.hLock == NULL ) {
		xrtUnit();
		return 1;
	}

	for ( i = 0; i < 4; i++ ) {
		arrThreads[i] = xrtThreadCreate(procAdd, &tCtx, 0);
	}

	for ( i = 0; i < 4; i++ ) {
		if ( arrThreads[i] ) {
			xrtThreadWait(arrThreads[i]);
			xrtThreadDestroy(arrThreads[i]);
		}
	}

	printf("counter = %d\n", (int)tCtx.iCounter);

	xrtMutexDestroy(tCtx.hLock);
	xrtUnit();
	return 0;
}
```

这一步要记住：

- 线程能并行，不代表共享状态天然安全
- 锁保护的是“临界区”
- 锁应该尽量短，不要拿着锁做长时间阻塞操作


## 8. DEMO 4：条件变量解决“等条件成立”

如果你的需求不是“抢同一个计数器”，而是：

- 一个线程等另一个线程准备好
- 条件不成立时先睡着
- 条件成立后被唤醒

那比起忙等，更适合用条件变量。

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmutex hLock;
	xcond hCond;
	volatile int32 bReady;
	str sMessage;
} DemoCondCtx;

static uint32 procWaiter(ptr pArg)
{
	DemoCondCtx* pCtx = (DemoCondCtx*)pArg;

	xrtMutexLock(pCtx->hLock);
	while ( !pCtx->bReady ) {
		if ( xrtCondWaitTimeout(pCtx->hCond, pCtx->hLock, 1000) != XRT_WAIT_OK ) {
			xrtMutexUnlock(pCtx->hLock);
			return 1;
		}
	}

	printf("message = %s\n", (char*)pCtx->sMessage);
	xrtMutexUnlock(pCtx->hLock);
	return 0;
}

int main(void)
{
	DemoCondCtx tCtx;
	xthread hThread;

	xrtInit();

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hLock = xrtMutexCreate();
	tCtx.hCond = xrtCondCreate();

	if ( (tCtx.hLock == NULL) || (tCtx.hCond == NULL) ) {
		if ( tCtx.hCond ) xrtCondDestroy(tCtx.hCond);
		if ( tCtx.hLock ) xrtMutexDestroy(tCtx.hLock);
		xrtUnit();
		return 1;
	}

	hThread = xrtThreadCreate(procWaiter, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtCondDestroy(tCtx.hCond);
		xrtMutexDestroy(tCtx.hLock);
		xrtUnit();
		return 1;
	}

	xrtSleep(100);

	xrtMutexLock(tCtx.hLock);
	tCtx.sMessage = (str)"hello from main thread";
	tCtx.bReady = 1;
	xrtCondSignal(tCtx.hCond);
	xrtMutexUnlock(tCtx.hLock);

	xrtThreadWait(hThread);
	xrtThreadDestroy(hThread);
	xrtCondDestroy(tCtx.hCond);
	xrtMutexDestroy(tCtx.hLock);

	xrtUnit();
	return 0;
}
```

这一步要重点理解：

- `cond` 不是数据本身
- 条件变量总是和“受互斥体保护的条件”一起用
- `while` 比 `if` 更稳，因为被唤醒后仍应重新检查条件


## 9. 信号量和条件变量怎么选

初学时可以这样粗分：

### 9.1 更像“有几个令牌可以消费”

优先考虑：

- `xsem`

适合：

- 生产者 / 消费者计数
- 限流
- 固定资源数目


### 9.2 更像“某个条件什么时候成立”

优先考虑：

- `xcond + xmutex`

适合：

- 等配置就绪
- 等某个共享状态变成可用
- 多个等待者一起被唤醒


## 10. 常见错误

### 10.1 错误一：把线程当成“等待工具”

线程最适合执行，不是最适合等待。

如果你主要是在组织等待链，应该优先看：

- 协程
- `future / task`


### 10.2 错误二：`xrtThreadDestroy()` 当成 stop

这是错位使用。

`Destroy` 负责释放线程对象，不负责结束线程。


### 10.3 错误三：共享变量不加保护

只要多个线程同时读写同一份可变状态，就必须明确：

- 是不是要锁
- 是不是该改成消息交接
- 是不是该改成 `future` 结果而不是共享写入


### 10.4 错误四：动不动就 `xrtThreadKill()`

强杀线程会破坏清理顺序，通常只会让问题更难查。

推荐优先级始终是：

1. 协作式 stop
2. 等待线程正常退出
3. 最后再 destroy


## 11. 什么时候该从线程升级到别的模型

如果你已经开始遇到下面这些问题，就说明不能只靠线程硬撑了：

- 想把线程间交接写清楚
- 想做背压和批量投递
- 想把等待链写成顺序代码
- 想统一超时、取消和结果收口

这时下一步通常是：

- [Queue 队列模块](../api/api-queue.md)
- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)


## 12. 下一步应该读什么

推荐顺序：

1. [XRT 运行时与线程附加入门](runtime-thread-attach.md)
2. [Thread API](../api/api-thread.md)
3. [Queue 队列模块](../api/api-queue.md)
4. [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
5. [线程、协程与 Future 协同模型](../case/thread-coroutine-future.md)

---

**一句话总结：** 线程解决的是“独立执行单元”，不是“所有并发问题”；在 XRT 当前主线里，先把线程生命周期、停止协议和共享数据边界写清楚，再继续上队列、协程和 future，会稳很多。
