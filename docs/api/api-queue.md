# Queue 队列模块

> XRT 当前正式的有界指针队列主线，提供 `SPSC / MPSC / MPMC` 和可选 `MPSC wait wrapper`。

[English](api-queue.en.md) | [中文](api-queue.md) | [返回索引](README.md)

---

## 1. 定位

该模块面向这些场景：

- 线程间 FIFO 投递
- `xnet` worker command queue
- 高并发指针消息传递
- 需要 close / drain / reset 生命周期的有界队列

当前主线特征：

- 固定容量
- 容量自动向上取 2 的幂
- 指针队列，不负责对象所有权
- `SPSC / MPSC / MPMC` 三条并发模型分开暴露
- 可选 `MPSC wait wrapper`，用于阻塞式消费者

当前明确不做：

- 无界队列
- 优先级队列
- 延时队列
- 内建对象析构与引用计数


## 2. 核心类型

### `xqueue_result`

```c
typedef enum xqueue_result
{
	XQUEUE_OK = 0,
	XQUEUE_EMPTY = 1,
	XQUEUE_FULL = 2,
	XQUEUE_CLOSED = 3,
	XQUEUE_TIMEOUT = 4,
	XQUEUE_ERROR = -1
} xqueue_result;
```

常见含义：

- `XQUEUE_OK`：操作成功
- `XQUEUE_EMPTY`：当前为空
- `XQUEUE_FULL`：当前已满
- `XQUEUE_CLOSED`：队列已关闭，不能再继续该路径操作
- `XQUEUE_TIMEOUT`：等待型接口超时
- `XQUEUE_ERROR`：参数错误或内部错误


### `xqueue_kind`

```c
typedef enum xqueue_kind
{
	XQUEUE_KIND_SPSC = 1,
	XQUEUE_KIND_MPSC = 2,
	XQUEUE_KIND_MPMC = 3
} xqueue_kind;
```

用于标识底层队列种类。


### `xqueuebase`

```c
typedef struct xqueuebase_struct
{
	uint32 iKind;
	volatile uint32 bClosed;
} xqueuebase;
```

用途：

- 供通用辅助函数读取关闭状态
- 作为具体队列结构体的公共头部


### `xqueue_config`

```c
typedef struct xqueue_config
{
	uint32 iCapacity;
	uint32 iFlags;
} xqueue_config;
```

说明：

- `iCapacity`：请求容量，实际容量会向上取到 2 的幂
- `iFlags`：当前保留，建议设为 `0`


### 队列句柄

- `xspscq`
	- 单生产者、单消费者
- `xmpscq`
	- 多生产者、单消费者
- `xmpmcq`
	- 多生产者、多消费者
- `xmpscqwait`
	- `xmpscq` 的等待包装层


### `xqueue_drain_fn`

```c
typedef void (*xqueue_drain_fn)(ptr pItem, ptr pUserData);
```

用于 `Drain` 时消费残留元素。


## 3. 通用约定

- `iCapacity == 0` 或超过实现上限会初始化失败
- 实际容量会自动向上取到 2 的幂
- 这是指针 FIFO，不复制 payload，不管理 payload 生命周期
- `Close` 之后不能再 `Push`
- `Close` 之后仍可继续 `Pop` / `Drain` 已经入队的元素
- `Drained` 表示“已关闭且已经取空”
- `ApproxCount` 在并发模型下只适合作为观测值，尤其是 `MPSC / MPMC`
- `Reset` 只适用于“已关闭且已 drained”的队列

裁剪宏：

- `XRT_NO_QUEUE`
	- 关闭整个队列模块
- `XRT_NO_QUEUE_WAIT`
	- 保留核心队列，移除等待包装层


## 4. 通用辅助 API

```c
XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue);
XXAPI bool xrtQueueIsDrained(const xqueuebase* pQueue);
```

建议用途：

- 用于 shutdown / drain 流程中的状态判断
- 不要把它们当成并发下的强同步屏障


## 5. SPSC

适合：

- 单 producer 线程
- 单 consumer 线程
- 最轻量的热路径

```c
XXAPI bool xrtSPSCQInit(xspscq pQueue, const xqueue_config* pCfg);
XXAPI void xrtSPSCQUnit(xspscq pQueue);
XXAPI xspscq xrtSPSCQCreate(const xqueue_config* pCfg);
XXAPI void xrtSPSCQDestroy(xspscq pQueue);
XXAPI xqueue_result xrtSPSCQTryPush(xspscq pQueue, ptr pItem);
XXAPI xqueue_result xrtSPSCQTryPop(xspscq pQueue, ptr* ppItem);
XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue);
XXAPI void xrtSPSCQClose(xspscq pQueue);
XXAPI uint32 xrtSPSCQDrain(xspscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
XXAPI bool xrtSPSCQReset(xspscq pQueue);
```


## 6. MPSC

适合：

- 多个 producer 并发投递
- 单 consumer 出队
- worker command queue 一类场景

```c
XXAPI bool xrtMPSCQInit(xmpscq pQueue, const xqueue_config* pCfg);
XXAPI void xrtMPSCQUnit(xmpscq pQueue);
XXAPI xmpscq xrtMPSCQCreate(const xqueue_config* pCfg);
XXAPI void xrtMPSCQDestroy(xmpscq pQueue);
XXAPI xqueue_result xrtMPSCQTryPush(xmpscq pQueue, ptr pItem);
XXAPI xqueue_result xrtMPSCQTryPop(xmpscq pQueue, ptr* ppItem);
XXAPI uint32 xrtMPSCQPushBatch(xmpscq pQueue, ptr* arrItems, uint32 iCount);
XXAPI uint32 xrtMPSCQPopBatch(xmpscq pQueue, ptr* arrItems, uint32 iCap);
XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue);
XXAPI void xrtMPSCQClose(xmpscq pQueue);
XXAPI uint32 xrtMPSCQDrain(xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
XXAPI bool xrtMPSCQReset(xmpscq pQueue);
```

批量接口说明：

- 返回值是本次成功 push / pop 的元素个数
- 不额外引入新的结果枚举
- 部分成功属于正常结果


## 7. MPMC

适合：

- 多 producer
- 多 consumer
- 通用并发消息队列

```c
XXAPI bool xrtMPMCQInit(xmpmcq pQueue, const xqueue_config* pCfg);
XXAPI void xrtMPMCQUnit(xmpmcq pQueue);
XXAPI xmpmcq xrtMPMCQCreate(const xqueue_config* pCfg);
XXAPI void xrtMPMCQDestroy(xmpmcq pQueue);
XXAPI xqueue_result xrtMPMCQTryPush(xmpmcq pQueue, ptr pItem);
XXAPI xqueue_result xrtMPMCQTryPop(xmpmcq pQueue, ptr* ppItem);
XXAPI uint32 xrtMPMCQPushBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCount);
XXAPI uint32 xrtMPMCQPopBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCap);
XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue);
XXAPI void xrtMPMCQClose(xmpmcq pQueue);
XXAPI uint32 xrtMPMCQDrain(xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
XXAPI bool xrtMPMCQReset(xmpmcq pQueue);
```


## 8. MPSC Wait Wrapper

仅在未定义 `XRT_NO_QUEUE_WAIT` 时提供。

```c
XXAPI bool xrtMPSCQWaitInit(xmpscqwait pQueue, const xqueue_config* pCfg);
XXAPI void xrtMPSCQWaitUnit(xmpscqwait pQueue);
XXAPI xmpscqwait xrtMPSCQWaitCreate(const xqueue_config* pCfg);
XXAPI void xrtMPSCQWaitDestroy(xmpscqwait pQueue);
XXAPI xqueue_result xrtMPSCQWaitTryPush(xmpscqwait pQueue, ptr pItem);
XXAPI xqueue_result xrtMPSCQWaitTryPop(xmpscqwait pQueue, ptr* ppItem);
XXAPI xqueue_result xrtMPSCQWaitPop(xmpscqwait pQueue, ptr* ppItem);
XXAPI xqueue_result xrtMPSCQWaitPopTimeout(xmpscqwait pQueue, ptr* ppItem, uint32 iTimeoutMs);
XXAPI uint32 xrtMPSCQWaitApproxCount(xmpscqwait pQueue);
XXAPI void xrtMPSCQWaitClose(xmpscqwait pQueue);
```

说明：

- 核心存储仍是 `MPSC`
- 生产者路径保持并发
- 阻塞式 `Pop` 路径会引入等待原语
- 内部会串行化 `Pop` 路径，因此它不是“完全无锁的阻塞队列”
- `Close` 会唤醒等待中的消费者
- 超时接口会返回 `XQUEUE_TIMEOUT`


## 9. 常见用法

### 9.1 SPSC 基本流程

```c
#include "xrt.h"
#include <string.h>

int main()
{
	xqueue_config tCfg;
	xspscq hQueue;
	ptr pItem = NULL;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 1000u;

	hQueue = xrtSPSCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	xrtSPSCQTryPush(hQueue, (ptr)"hello");
	if ( xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_OK ) {
		/* pItem == "hello" */
	}

	xrtSPSCQClose(hQueue);
	xrtSPSCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```


### 9.2 MPSC 等待式消费

```c
xqueue_config tCfg;
xmpscqwait hQueue;
ptr pItem = NULL;

memset(&tCfg, 0, sizeof(tCfg));
tCfg.iCapacity = 1024u;

hQueue = xrtMPSCQWaitCreate(&tCfg);

if ( xrtMPSCQWaitTryPush(hQueue, pJob) == XQUEUE_OK ) {
	/* producer 发布成功 */
}

switch ( xrtMPSCQWaitPopTimeout(hQueue, &pItem, 500u) ) {
	case XQUEUE_OK:
		/* 成功取到元素 */
		break;
	case XQUEUE_TIMEOUT:
		/* 500ms 内没有元素 */
		break;
	case XQUEUE_CLOSED:
		/* 队列已关闭且 drained */
		break;
	default:
		break;
}

xrtMPSCQWaitClose(hQueue);
xrtMPSCQWaitDestroy(hQueue);
```


## 10. 使用建议

- 能用 `SPSC` 就不要先上 `MPMC`
- 热路径优先使用 `TryPush / TryPop`
- 需要吞吐时再评估 `PushBatch / PopBatch`
- payload 生命周期由调用方自己管理
- 如果只是需要阻塞式单消费，不要把等待逻辑揉进核心队列，直接用 `xmpscqwait`
