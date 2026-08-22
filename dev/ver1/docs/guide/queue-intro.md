# XRT 队列入门：什么时候该用消息交接，而不是共享状态

> 目标：把“为什么需要队列、它和共享状态 + 锁有什么区别、SPSC / MPSC / MPMC / wait wrapper 该怎么选、Close / Drain / Reset 应该怎样理解”一次讲清楚。

[返回教学文档](README.md) | [返回文档中心](../README.md)

---

## 0. 建议先读什么

如果你还没有建立 XRT 多任务整体心智模型，建议先读：

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
- [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)

本文只聚焦一件事：

- 线程之间为什么需要“交接协议”
- 队列解决的到底是什么问题
- XRT 当前队列主线应该怎样落到代码里


## 1. 为什么需要队列

很多程序一开始都是这样长出来的：

1. 主线程做所有事
2. 后来发现太慢，于是开一个 worker 线程
3. 再后来两个线程开始共用同一批数据
4. 最后到处都是共享字段、锁、标志位和“这个字段到底谁在改”

问题通常不是“线程不够多”，而是：

> 缺少一个清晰的交接边界。


### 1.1 单主线程的问题

如果只有一条主线程，那么“生产工作”和“消费工作”只能顺序发生：

- 先准备数据
- 再处理数据
- 处理没做完，下一批就只能等

这时你几乎没有“交接”这个概念，因为一切都在同一个执行流里。


### 1.2 多线程直接改共享状态的问题

一旦变成多线程，很多人第一反应是：

- 多个线程一起改一个数组
- 多个线程一起往一个列表塞数据
- 再用锁把它们勉强护住

这样短期能跑，但很快会冒出几个问题：

- 谁拥有这份数据，不清楚
- 谁该负责释放，不清楚
- 满了怎么办，不清楚
- 生产者结束了，消费者何时退出，不清楚


### 1.3 队列真正解决的 3 件事

队列的价值不是“让程序更高级”，而是把下面 3 件事明确下来：

1. 谁负责生产，谁负责消费
2. 数据怎样按 FIFO 顺序交接
3. 队列满了、关闭了、排空了时，各方该怎么收口

一句话理解：

> 线程解决“谁来执行”，队列解决“执行单元之间怎样交接”。


## 2. 队列不是执行单元

这点一定要先讲透。

队列本身不会：

- 帮你开线程
- 帮你做计算
- 帮你处理任务

队列只负责：

- 存放待交接的指针
- 按顺序交给消费者
- 在“满 / 空 / 关闭 / 排空”这些状态上给出明确结果

所以正确分工通常是：

- 线程负责执行
- 队列负责交接
- `future` 负责结果
- 协程负责编排

如果把队列当成“执行模型”，后面一定会混乱。


## 3. XRT 当前队列家族怎么选

XRT 当前正式队列主线有 4 个入口：

| 类型 | 生产者 | 消费者 | 最适合 | 你要付出的代价 |
|------|--------|--------|--------|----------------|
| `xspscq` | 1 | 1 | 单生产者单消费者热路径 | 只能严格 1 对 1 |
| `xmpscq` | 多 | 1 | 多 worker 投递到单消费端 | 消费端仍然只有 1 个 |
| `xmpmcq` | 多 | 多 | 通用 worker pool | 成本最高，不该默认先上 |
| `xmpscqwait` | 多 | 1 | 需要阻塞式消费的 `MPSC` | `Pop` 路径串行化，不是“阻塞版 MPMC” |

初学时先记住这条原则：

1. 能用 `SPSC`，就不要先上 `MPSC`
2. 能用 `MPSC`，就不要先上 `MPMC`
3. 只有消费端确实需要阻塞等待时，才上 `xmpscqwait`


## 4. 使用前先记住 6 条规则

### 4.1 这是有界队列，不是无界队列

也就是说：

- 容量是固定的
- 满了会返回 `XQUEUE_FULL`
- 调用方必须自己决定是重试、让步、丢弃还是降速

这也是背压的一部分。


### 4.2 实际容量会向上取 2 的幂

例如你填：

- `3`

实际容量会变成：

- `4`

所以不要把“请求容量”和“最终容量”混为一谈。


### 4.3 这是指针队列，不管理对象生命周期

队列里放的是：

- `ptr`

不是对象副本。

这意味着：

- `Push` 成功前，生产者仍然拥有 payload
- `Push` 成功后，通常就把处理责任交给消费者
- `Drain` 回调只是帮你消费残留元素，不会自动释放任何对象


### 4.4 `Close` 不等于立刻清空

`Close` 的意义是：

- 不再允许新的 `Push`
- 已经进队的元素仍然可以继续 `Pop`

所以常见 shutdown 顺序通常是：

1. 停止继续生产
2. `Close`
3. 把剩余元素消费完或 `Drain`
4. 再销毁


### 4.5 `Reset` 只能用于“已关闭且已排空”的队列

也就是：

- 队列还在用，不能 `Reset`
- 队列还没排空，不能 `Reset`

如果把 `Reset` 当成“粗暴重置”，很容易把仍在流转的元素直接弄丢。


### 4.6 `ApproxCount` 只是观察值

尤其在 `MPSC / MPMC` 下，它只适合：

- 监控
- 打日志
- 粗略估算负载

不适合：

- 当作严格同步条件
- 当作“看到是 0 就一定没人再 push 了”


## 5. DEMO 1：先学 `SPSC`，理解最小交接

先从最简单的模型开始：

- 一个生产者
- 一个消费者
- 按 FIFO 交接 5 个整数

```c
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	xspscq hQueue;
	volatile int32 iSum;
} DemoSPSCConsumerCtx;

static uint32 procConsumer(ptr pArg)
{
	DemoSPSCConsumerCtx* pCtx = (DemoSPSCConsumerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtSPSCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			pCtx->iSum += (int32)(uintptr_t)pItem;
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

int main(void)
{
	xqueue_config tCfg;
	xspscq hQueue;
	xthread hThread;
	DemoSPSCConsumerCtx tCtx;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 8u;

	hQueue = xrtSPSCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = hQueue;

	hThread = xrtThreadCreate(procConsumer, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtSPSCQDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	for ( i = 1; i <= 5; i++ ) {
		for ( ;; ) {
			xqueue_result iRet = xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)i);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}

			xrtSPSCQClose(hQueue);
			xrtThreadWait(hThread);
			xrtThreadDestroy(hThread);
			xrtSPSCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
	}

	xrtSPSCQClose(hQueue);
	xrtThreadWait(hThread);
	printf("sum = %d\n", (int)tCtx.iSum);

	xrtThreadDestroy(hThread);
	xrtSPSCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

这个 DEMO 最重要的不是“把 1 到 5 加起来”，而是先理解：

- `SPSC` 只允许一个生产者和一个消费者
- `FULL` 和 `EMPTY` 都是正常状态，不是异常
- `Close` 之后，消费者会在取空后收到 `XQUEUE_CLOSED`

这里把整数强转成 `ptr` 只是为了演示最小流程。真实项目里，更推荐传：

- 堆对象指针
- 生命周期稳定的结构体指针


## 6. DEMO 2：再学 `MPSC`，理解“多个生产者投递到一个消费者”

很多真实项目里的队列，都是这个形状：

- 多个 worker 负责产出
- 一个保存线程、日志线程或主线程负责收口

这正是 `MPSC` 的典型场景。

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iProducerId;
	int32 iValue;
} DemoJob;

typedef struct
{
	xmpscq hQueue;
	int32 iProducerId;
	int32 iCount;
} DemoMPSCProducerCtx;

static void procDrainJob(ptr pItem, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pItem);
}

static uint32 procProducer(ptr pArg)
{
	DemoMPSCProducerCtx* pCtx = (DemoMPSCProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iCount; i++ ) {
		DemoJob* pJob = (DemoJob*)xrtMalloc(sizeof(DemoJob));
		if ( pJob == NULL ) {
			return 1;
		}

		pJob->iProducerId = pCtx->iProducerId;
		pJob->iValue = i + 1;

		for ( ;; ) {
			xqueue_result iRet = xrtMPSCQTryPush(pCtx->hQueue, pJob);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}

			xrtFree(pJob);
			return 2;
		}
	}

	return 0;
}

int main(void)
{
	xqueue_config tCfg;
	xmpscq hQueue;
	xthread arrThreads[3];
	DemoMPSCProducerCtx arrCtx[3];
	int32 iReceived = 0;
	int32 iStarted = 0;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 16u;

	hQueue = xrtMPSCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(arrThreads, 0, sizeof(arrThreads));
	memset(arrCtx, 0, sizeof(arrCtx));

	for ( i = 0; i < 3; i++ ) {
		arrCtx[i].hQueue = hQueue;
		arrCtx[i].iProducerId = i + 1;
		arrCtx[i].iCount = 4;
		arrThreads[i] = xrtThreadCreate(procProducer, &arrCtx[i], 0);
		if ( arrThreads[i] == NULL ) {
			xrtMPSCQClose(hQueue);
			for ( i = 0; i < iStarted; i++ ) {
				xrtThreadWait(arrThreads[i]);
				xrtThreadDestroy(arrThreads[i]);
			}
			xrtMPSCQDrain(hQueue, procDrainJob, NULL);
			xrtMPSCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
		iStarted++;
	}

	for ( i = 0; i < 3; i++ ) {
		xrtThreadWait(arrThreads[i]);
		xrtThreadDestroy(arrThreads[i]);
	}

	xrtMPSCQClose(hQueue);

	for ( ;; ) {
		ptr pItem = NULL;
		xqueue_result iRet = xrtMPSCQTryPop(hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			DemoJob* pJob = (DemoJob*)pItem;
			printf("producer=%d value=%d\n", (int)pJob->iProducerId, (int)pJob->iValue);
			xrtFree(pJob);
			iReceived++;
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
	}

	printf("received=%d\n", (int)iReceived);
	xrtMPSCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

这个 DEMO 要重点观察 2 个边界：

### 6.1 队列把共享写入改成了消息交接

生产者不再去碰消费者的内部状态，而是：

- 只负责构造 `DemoJob`
- 把 `DemoJob*` 推进队列

这会让模块边界清楚很多。


### 6.2 所有权在 `Push` 成功后才真正交出去

这里的规则要记死：

- `Push` 成功前，`pJob` 还是生产者自己的
- `Push` 成功后，`pJob` 才进入消费者责任域
- 如果 `Push` 失败并准备放弃，本轮仍然要由生产者自己 `xrtFree()`

这就是指针队列最核心的所有权规则。


## 7. DEMO 3：需要阻塞式消费时，使用 `xmpscqwait`

前面的 `TryPop()` 模型是忙轮询：

- 空了就 `Yield`
- 过一会儿再试

这在热路径里很常见，但有些场景你更想要的是：

- 没有任务就睡着
- 有任务再被唤醒

这时就该用 `xmpscqwait`。

```c
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmpscqwait hQueue;
} DemoWaitConsumerCtx;

static uint32 procWaitConsumer(ptr pArg)
{
	DemoWaitConsumerCtx* pCtx = (DemoWaitConsumerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, 500u);
		if ( iRet == XQUEUE_OK ) {
			printf("pop value = %d\n", (int)(uintptr_t)pItem);
			continue;
		}
		if ( iRet == XQUEUE_TIMEOUT ) {
			printf("consumer idle...\n");
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

int main(void)
{
	xqueue_config tCfg;
	xmpscqwait hQueue;
	xthread hThread;
	DemoWaitConsumerCtx tCtx;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 8u;

	hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = hQueue;

	hThread = xrtThreadCreate(procWaitConsumer, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtMPSCQWaitDestroy(hQueue);
		xrtUnit();
		return 1;
	}

	xrtSleep(200);

	for ( i = 1; i <= 3; i++ ) {
		if ( xrtMPSCQWaitTryPush(hQueue, (ptr)(uintptr_t)(100 + i)) != XQUEUE_OK ) {
			xrtMPSCQWaitClose(hQueue);
			xrtThreadWait(hThread);
			xrtThreadDestroy(hThread);
			xrtMPSCQWaitDestroy(hQueue);
			xrtUnit();
			return 1;
		}
	}

	xrtSleep(200);
	xrtMPSCQWaitClose(hQueue);

	xrtThreadWait(hThread);
	xrtThreadDestroy(hThread);
	xrtMPSCQWaitDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

这一页最重要的结论不是 API 名字，而是：

- `xmpscqwait` 仍然是 `MPSC`
- 只是额外给了你阻塞式 `Pop`
- 当前只有“单消费者阻塞等待”这条路，没有“阻塞版 MPMC”

所以如果你的模型是：

- 多生产者
- 一个专门的消费线程
- 消费端没活时应该睡眠

那 `xmpscqwait` 很合适。

如果你需要：

- 多消费者同时阻塞等待

那就不该把它误当成 `MPMC wait`。


## 8. DEMO 4：最后学 `MPMC`，理解 worker pool

`MPMC` 是通用性最强的一种，但也最不该一上来就默认用它。

只有在你真的需要：

- 多个生产者同时投递
- 多个消费者同时取任务

时，才应该切到 `MPMC`。

```c
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmpmcq hQueue;
	int32 iBaseValue;
	int32 iCount;
} DemoMPMCProducerCtx;

typedef struct
{
	xmpmcq hQueue;
	volatile int32 iHandled;
} DemoMPMCConsumerCtx;

static uint32 procMPMCProducer(ptr pArg)
{
	DemoMPMCProducerCtx* pCtx = (DemoMPMCProducerCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iCount; i++ ) {
		for ( ;; ) {
			xqueue_result iRet = xrtMPMCQTryPush(
				pCtx->hQueue,
				(ptr)(uintptr_t)(pCtx->iBaseValue + i + 1)
			);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			return 1;
		}
	}

	return 0;
}

static uint32 procMPMCConsumer(ptr pArg)
{
	DemoMPMCConsumerCtx* pCtx = (DemoMPMCConsumerCtx*)pArg;
	ptr pItem = NULL;

	for ( ;; ) {
		xqueue_result iRet = xrtMPMCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			pCtx->iHandled++;
			printf("consumer got value=%d\n", (int)(uintptr_t)pItem);
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}
		return 1;
	}
}

int main(void)
{
	xqueue_config tCfg;
	xmpmcq hQueue;
	xthread arrProducer[2];
	xthread arrConsumer[2];
	DemoMPMCProducerCtx arrProdCtx[2];
	DemoMPMCConsumerCtx arrConsCtx[2];
	int32 iConsumerStarted = 0;
	int32 iProducerStarted = 0;
	int32 i;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 16u;

	hQueue = xrtMPMCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrConsumer, 0, sizeof(arrConsumer));
	memset(arrProdCtx, 0, sizeof(arrProdCtx));
	memset(arrConsCtx, 0, sizeof(arrConsCtx));

	for ( i = 0; i < 2; i++ ) {
		arrConsCtx[i].hQueue = hQueue;
		arrConsumer[i] = xrtThreadCreate(procMPMCConsumer, &arrConsCtx[i], 0);
		if ( arrConsumer[i] == NULL ) {
			xrtMPMCQClose(hQueue);
			for ( i = 0; i < iConsumerStarted; i++ ) {
				xrtThreadWait(arrConsumer[i]);
				xrtThreadDestroy(arrConsumer[i]);
			}
			xrtMPMCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
		iConsumerStarted++;
	}

	for ( i = 0; i < 2; i++ ) {
		arrProdCtx[i].hQueue = hQueue;
		arrProdCtx[i].iBaseValue = i * 100;
		arrProdCtx[i].iCount = 4;
		arrProducer[i] = xrtThreadCreate(procMPMCProducer, &arrProdCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			xrtMPMCQClose(hQueue);
			for ( i = 0; i < iProducerStarted; i++ ) {
				xrtThreadWait(arrProducer[i]);
				xrtThreadDestroy(arrProducer[i]);
			}
			for ( i = 0; i < iConsumerStarted; i++ ) {
				xrtThreadWait(arrConsumer[i]);
				xrtThreadDestroy(arrConsumer[i]);
			}
			xrtMPMCQDestroy(hQueue);
			xrtUnit();
			return 1;
		}
		iProducerStarted++;
	}

	for ( i = 0; i < 2; i++ ) {
		xrtThreadWait(arrProducer[i]);
		xrtThreadDestroy(arrProducer[i]);
	}

	xrtMPMCQClose(hQueue);

	for ( i = 0; i < 2; i++ ) {
		xrtThreadWait(arrConsumer[i]);
		printf("consumer[%d] handled=%d\n", (int)i, (int)arrConsCtx[i].iHandled);
		xrtThreadDestroy(arrConsumer[i]);
	}

	xrtMPMCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```

这个 DEMO 真正想让你理解的是：

- `MPMC` 适合 worker pool
- 关闭时不需要先等队列变空，再 `Close`
- `Close` 后消费者仍然可以把剩余任务取完，直到收到 `XQUEUE_CLOSED`

如果你的消费端始终只有 1 个，就不要为了“看起来更通用”直接用 `MPMC`。


## 9. `Close / Drain / Reset` 到底怎么理解

这是 XRT 队列最容易被写错的一段生命周期。

### 9.1 `Close`

`Close` 的含义是：

- 不再接受新的 `Push`
- 允许已入队元素继续被消费

适合时机：

- 生产阶段结束
- 程序准备 shutdown
- 某个 worker 不再接收新任务


### 9.2 `Drain`

`Drain` 的含义是：

- 把残留元素逐个取出
- 交给你的回调处理

它常用于：

- 清理未消费完的堆对象
- shutdown 时统一回收

如果 payload 是堆对象，你通常会写出这种回调：

```c
static void procDrain(ptr pItem, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pItem);
}
```


### 9.3 `Reset`

`Reset` 的前提一定要满足：

- 队列已经 `Close`
- 队列已经 `Drain` 完或被正常 `Pop` 取空

它适合：

- 重复使用一个嵌入式队列对象
- 做测试或阶段性复用

它不适合：

- 还在运行中的生产 / 消费队列


## 10. 批量接口、近似计数和性能选择

`MPSC / MPMC` 还提供：

- `PushBatch`
- `PopBatch`

它们适合：

- 一次处理一批任务
- 想减少逐个调用开销

要注意两点：

### 10.1 批量接口允许部分成功

例如本次想 push 8 个元素，最终只成功 5 个，这是正常结果。

所以批量接口的正确理解是：

- 返回值表示本次成功数量
- 剩余元素仍然归调用方负责


### 10.2 先把模型选对，再谈批量

性能优化优先级通常应该是：

1. 先选对 `SPSC / MPSC / MPMC`
2. 再决定是否需要 `wait wrapper`
3. 最后再看 `Batch` 和容量调优

不要一上来就冲着“最强的 MPMC + Batch”去写。


## 11. Windows / Linux 跨平台注意点

XRT 的队列 API 本身已经帮你抹平了平台差异，但写法上仍要注意：

### 11.1 不要假设调度顺序完全一致

也就是说不要依赖：

- 某个线程一定先抢到执行权
- `Yield` 后一定马上轮到另一个线程

正确做法是依赖：

- 队列返回值
- `Close` / `CLOSED`
- 显式的等待与退出协议


### 11.2 示例里的“整数转指针”只适合演示

真实跨平台代码里，更稳的方式始终是：

- 传结构体指针
- 明确对象所有权

这样不会把“指针宽度”和“业务数据”混在一起。


### 11.3 不要把栈上临时变量地址塞进异步队列

例如生产者函数里定义了局部变量：

```c
DemoJob tJob;
```

然后把 `&tJob` 推进队列，生产者返回后这个地址就失效了。

这类 bug 在 Windows 和 Linux 上都很难查。


## 12. 常见错误

### 12.1 错误一：把队列当成线程替代品

队列不执行任务，只交接任务。


### 12.2 错误二：默认直接上 `MPMC`

并发模型越宽，复杂度和成本通常越高。


### 12.3 错误三：看到 `ApproxCount == 0` 就认定系统空闲

这只是观测值，不是严格同步结论。


### 12.4 错误四：`Close` 之后立刻 `Destroy`

如果消费者还没退、残留元素还没处理完，这样会把 shutdown 写乱。


### 12.5 错误五：`Push` 失败后忘了处理 payload

指针队列不会替你兜底释放。


## 13. 下一步应该读什么

推荐顺序：

1. [Queue API](../api/api-queue.md)
2. [用 Queue + Future 写一个多生产者 Worker](../case/queue-worker-future.md)
3. [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
4. [Wait-Source 入门：把 Future 和网络等待说成同一种语言](wait-source-intro.md)
5. [xnet-v2 Stream Wait-Source 入门](xnet-stream-wait-source-intro.md)
6. [线程、协程与 Future 协同模型](../case/thread-coroutine-future.md)

---

**一句话总结：** 队列不是为了“显得并发高级”，而是为了把线程之间的交接、背压和收口写清楚；在 XRT 当前主线里，先选对 `SPSC / MPSC / MPMC / xmpscqwait`，再谈批量和性能，程序结构会稳很多。
