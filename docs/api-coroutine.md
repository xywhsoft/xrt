# Coroutine 协程运行时

> 栈式协程运行时、调度器、事件等待与生命周期管理

[English](api-coroutine.en.md) | [中文](api-coroutine.md) | [返回索引](README.md)

---

## 目录

- [概述](#概述)
- [常量定义](#常量定义)
- [核心类型](#核心类型)
- [生命周期 API](#生命周期-api)
- [调度器 API](#调度器-api)
- [事件与 deadline 等待](#事件与-deadline-等待)
- [结果、用户数据、cleanup](#结果用户数据cleanup)
- [后端自检](#后端自检)
- [协程与 XNet 等待桥接](#协程与-xnet-等待桥接)
- [使用模式](#使用模式)
- [注意事项](#注意事项)

---

## 概述

XRT 的协程是“线程绑定、栈式、协作式”的任务运行时。

它适合：

- 单线程内的高并发任务编排
- 由调度器驱动的 sleep、deadline、event wait
- 与未来 xnet/event-loop 的非阻塞等待集成

它不适合：

- 替代线程做抢占式并行
- 跨线程迁移执行对象
- 充当 `xrtThreadCreate()` 的替代品

运行时合同：

- 使用协程 API 前先调用 `xrtInit()`
- 由 `xrtThreadCreate()` 创建的线程会自动附着到运行时
- 宿主自己创建的线程，使用运行时绑定 API 前应先调用 `xrtThreadAttachCurrent()`
- 一个协程在任意时刻只属于一个 owner thread 和一个 scheduler

---

## 常量定义

### 协程状态

```c
#define XRT_CO_READY         0
#define XRT_CO_RUNNING       1
#define XRT_CO_SUSPENDED     2
#define XRT_CO_DEAD          3
```

### 终态原因

```c
#define XRT_CO_TERM_NONE         0
#define XRT_CO_TERM_RETURNED     1
#define XRT_CO_TERM_CANCELLED    2
```

### 后端分层 / 风格

```c
#define XRT_CO_BACKEND_TIER_COMPAT       1
#define XRT_CO_BACKEND_TIER_PRODUCTION   2

#define XRT_CO_BACKEND_STYLE_COMPAT      1
#define XRT_CO_BACKEND_STYLE_INLINE_ASM  2
```

### 栈与等待默认值

```c
#define XRT_CO_STACK_DEFAULT (64 * 1024)
#define XRT_CO_STACK_MIN     (8 * 1024)
#define XRT_CO_STACK_MAX     (8 * 1024 * 1024)
#define XRT_CO_WAIT_INFINITE UINT32_C(0xffffffff)
```

---

## 核心类型

### 入口函数与 cleanup

```c
typedef void (*xco_entry)(ptr pParam);
typedef void (*xco_cleanup_proc)(ptr pArg);
```

### 创建参数

```c
typedef struct {
    size_t iStackSize;
    ptr pUserData;
    uint32 iFlags;
    uint32 iReserved;
} xco_create_args;
```

需要自定义栈大小或初始 `user data` 时，优先使用 `xrtCoCreateEx()`。

### 主对象句柄

```c
typedef struct xcoro_struct* xcoro;
typedef struct xrt_co_scheduler xcosched;
typedef struct xcoevent_struct* xcoevent;
```

- `xcoro`：单个协程句柄
- `xcosched`：单线程私有调度器
- `xcoevent`：最小协程等待对象，当前版本为单 waiter 模型

---

## 生命周期 API

### 创建 / 销毁

```c
XXAPI xcoro xrtCoCreateEx(xco_entry pfnEntry, ptr pParam, const xco_create_args* pArgs);
XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize);
XXAPI void xrtCoDestroy(xcoro pCo);
```

`xrtCoDestroy()` 不是“任何时候都能 free”的强制接口。被销毁的句柄必须已经脱离调度器，并且处于安全终态。

### 运行 / 挂起 / 退出

```c
XXAPI bool xrtCoResume(xcoro pCo);
XXAPI void xrtCoYield();
XXAPI void xrtCoExit(int64 iExitCode);
```

- `xrtCoResume()`：直接从调用方切换到协程
- `xrtCoYield()`：协作式把控制权交回调用方或调度器
- `xrtCoExit()`：立即结束当前协程，并记录退出码

### 取消 / 关闭 / Join

```c
XXAPI bool xrtCoCancel(xcoro pCo);
XXAPI bool xrtCoClose(xcoro pCo);
XXAPI bool xrtCoJoin(xcoro pCo);
```

当前合同：

- cancel 是协作式，不做强制栈卸载
- `join` 当前只支持单等待者
- `close` 是安全的“我不再持有这个句柄”路径，不等于“立刻杀死协程”

### 状态查询

```c
XXAPI int xrtCoGetState(xcoro pCo);
XXAPI xcoro xrtCoGetCurrent();
XXAPI bool xrtCoIsCancelRequested();
XXAPI bool xrtCoWasCancelled(xcoro pCo);
XXAPI int64 xrtCoGetExitCode(xcoro pCo);
```

---

## 调度器 API

```c
XXAPI xcosched* xrtCoSchedCreate();
XXAPI xcosched* xrtCoSchedCurrent();
XXAPI void xrtCoSchedDestroy(xcosched* pSched);
XXAPI xcoro xrtCoSchedSpawn(xcosched* pSched, xco_entry pfnEntry, ptr pParam, size_t iStackSize);
XXAPI bool xrtCoSchedPost(xcosched* pSched, xcoro pCo);
XXAPI bool xrtCoSchedPollOnce(xcosched* pSched, uint32 iTimeout);
XXAPI bool xrtCoSchedStep(xcosched* pSched);
XXAPI void xrtCoSchedRun(xcosched* pSched);
XXAPI int xrtCoSchedGetAlive(xcosched* pSched);
```

只要协程需要下面这些能力，就应该跑在调度器上：

- `sleep`
- `deadline wait`
- `event wait`
- 跨线程 `post` 唤醒

`xrtCoSchedPost()` 可以跨线程调用，但真正的唤醒与入队必须由 owner thread 在安全点处理。

---

## 事件与 deadline 等待

### 时间等待

```c
XXAPI void xrtCoSleep(uint32 iMs);
XXAPI void xrtCoSleepUntil(int64 iDeadlineMs);
XXAPI bool xrtCoWaitDeadline(int64 iDeadlineMs);
```

### 事件等待

```c
XXAPI xcoevent xrtCoEventCreate(bool bManualReset, bool bInitialState);
XXAPI void xrtCoEventDestroy(xcoevent pEvent);
XXAPI void xrtCoEventSet(xcoevent pEvent);
XXAPI void xrtCoEventReset(xcoevent pEvent);
XXAPI bool xrtCoWaitEvent(xcoevent pEvent);
XXAPI bool xrtCoWaitEventTimeout(xcoevent pEvent, uint32 iTimeoutMs);
XXAPI bool xrtCoWaitEventUntil(xcoevent pEvent, int64 iDeadlineMs);
```

说明：

- `xcoevent` 是协程原生等待对象
- `xrtCoEventSet()` 可以跨线程唤醒 waiter
- timeout / deadline 版本在超时或等待期间收到取消时返回 `false`

---

## 结果、用户数据、cleanup

### 结果与用户数据

```c
XXAPI void xrtCoSetResult(ptr pResult);
XXAPI ptr xrtCoGetResult(xcoro pCo);
XXAPI void xrtCoSetUserData(xcoro pCo, ptr pData);
XXAPI ptr xrtCoGetUserData(xcoro pCo);
```

### Cleanup 栈

```c
XXAPI bool xrtCoPushCleanup(xco_cleanup_proc proc, ptr pArg);
XXAPI bool xrtCoPopCleanup(xco_cleanup_proc proc, ptr pArg, bool bExecute);
```

cleanup 会在协程通过 `return`、`cancel`、`close`、`xrtCoExit()` 等路径结束时按 LIFO 顺序执行。

重要约束：

- cleanup 回调里不要 `yield`，也不要等待事件

---

## 后端自检

```c
XXAPI str xrtCoGetBackendName();
XXAPI int xrtCoGetBackendTier();
XXAPI int xrtCoGetBackendStyle();
```

它们适合用于：

- 确认当前机器是否命中了预期 backend
- 区分 production backend 和 compatibility fallback
- 给 benchmark 和问题定位打上明确 backend 标签

---

## 协程与 XNet 等待桥接

XRT 目前已经在 `xnet-v2` 的 `future / stream / datagram / listener` 之上提供了协程原生等待入口。

代表性 API：

```c
static xnet_result xrtNetFutureWaitCo(xnetfuture* pFuture);
static xnet_result xrtNetFutureWaitCoTimeout(xnetfuture* pFuture, uint32 iTimeoutMs);
static xnet_result xrtNetFutureWaitCoUntil(xnetfuture* pFuture, int64 iDeadlineMs);

static xnet_result xrtNetStreamWaitReadableCo(xnetstream* pStream);
static xnet_result xrtNetStreamWaitWritableCo(xnetstream* pStream);
static xnet_result xrtNetStreamWaitDrainCo(xnetstream* pStream);
static xnet_result xrtNetStreamWaitCloseCo(xnetstream* pStream);

static xnet_result xrtNetDgramRecvCo(xdgramsock* pSock, xnetdgrampkt** ppPacket);
static xnet_result xrtNetListenerAcceptCo(xnetlistener* pListener, xnetstream** ppStream);

static xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
static xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue);
```

当前稳定合同建议这样理解：

- future wait 会挂起当前协程，但不会阻塞 owner thread
- stream `readable` 等待针对的是 buffered paused-read 路径，不是原始 socket readiness
- stream `writable` 等待表达的是背压缓解，不是通用 writable 轮询
- dgram recv 是 one-shot packet 语义，并且当前与 `OnRecv` 互斥
- listener accept 已接入真实 listener/worker 生命周期，而不是轮询式伪等待

在使用层面，最稳定的统一抽象是 `xnetwaitsrc`，它可以封装：

- future
- stream wait-kind
- datagram receive
- listener accept

网络层仍在继续演进，所以这里刻意只写“稳定合同”和“推荐理解方式”，不把所有 helper 变体都提前固定成冗长目录。

---

## 使用模式

### 直接 Resume / Yield

```c
static void CoMain(ptr pArg)
{
    int* pCounter = (int*)pArg;
    (*pCounter)++;
    xrtCoYield();
    (*pCounter)++;
}

int main(void)
{
    int iCounter = 0;
    xcoro pCo;

    xrtInit();

    pCo = xrtCoCreate(CoMain, &iCounter, 0);
    xrtCoResume(pCo);
    xrtCoResume(pCo);

    xrtCoDestroy(pCo);
    xrtUnit();
    return 0;
}
```

### 调度器 + Sleep

```c
static void Worker(ptr pArg)
{
    (void)pArg;
    xrtCoSleep(100);
}

xcosched* pSched = xrtCoSchedCreate();
xrtCoSchedSpawn(pSched, Worker, NULL, 0);
xrtCoSchedRun(pSched);
xrtCoSchedDestroy(pSched);
```

### 事件等待

```c
static void Waiter(ptr pArg)
{
    xcoevent pEvent = (xcoevent)pArg;
    if ( xrtCoWaitEventTimeout(pEvent, 1000) ) {
        /* event signaled */
    }
}
```

---

## 注意事项

- 协程不能从错误线程恢复执行。
- 调度器是线程私有对象，不要跨线程迁移。
- 线程负责 CPU 并行，协程负责单线程内的协作式并发。
- 协程侧的 `xnet` wait bridge 已经存在，但网络等待面还在继续演进，建议结合网络文档一起使用。
