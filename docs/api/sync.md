# Sync

同步体系提供 mutex、condition、semaphore、RWLock 和 event。每种能力独立裁剪，
公共对象不暴露 Win32 或 pthread 类型。

## 裁剪宏

| 能力 | 启用宏 | 依赖 |
|---|---|---|
| 公共同步底座 | `XRT_FEATURE_SYNC` | `core` |
| Mutex | `XRT_FEATURE_MUTEX` | `sync` |
| Condition | `XRT_FEATURE_COND` | `mutex`、`wait` |
| Semaphore | `XRT_FEATURE_SEM` | `sync`、`wait` |
| RWLock | `XRT_FEATURE_RWLOCK` | `sync` |
| Event | `XRT_FEATURE_EVENT` | `sync`、`wait` |

## 存储与所有权

每种对象都是 8 字节对齐的固定存储 union，可嵌入结构、放在栈上或批量分配。
`XRT_MUTEX_STORAGE_SIZE`、`XRT_COND_STORAGE_SIZE`、`XRT_SEM_STORAGE_SIZE`、
`XRT_RWLOCK_STORAGE_SIZE` 和 `XRT_EVENT_STORAGE_SIZE` 是对应公共存储大小。
平台内部布局有编译期越界检查。

所有对象提供两种生命周期：

- `Init/Unit` 初始化和释放调用方提供的存储，不产生堆分配。
- `Create/Destroy` 分配和释放对象，适合普通拥有式用法。

`Init` 只能用于尚未初始化或已经成功 `Unit` 的存储，不能覆盖仍有效的对象。
`Unit/Destroy` 时不得还有持有者、等待者或并发调用者。`Destroy(NULL)` 成功且无操作。

## Mutex

`xmutex` 是非递归独占锁。递归锁定和非持有者解锁失败并设置状态错误；
`TryLock` 因其他线程持有锁而返回 `false` 时不设置错误。

| 函数 | 说明 |
|---|---|
| `xrtMutexInit(pMutex)` | 初始化调用方存储。 |
| `xrtMutexUnit(pMutex)` | 释放平台资源；锁仍被持有时失败。 |
| `xrtMutexCreate()` | 创建 mutex，失败返回 `NULL`。 |
| `xrtMutexDestroy(pMutex)` | 释放拥有式 mutex。 |
| `xrtMutexLock(pMutex)` | 阻塞到当前线程获得锁。 |
| `xrtMutexTryLock(pMutex)` | 非阻塞尝试获得锁。 |
| `xrtMutexUnlock(pMutex)` | 由持有线程释放锁。 |

```c
xmutex tMutex;

if ( xrtMutexInit(&tMutex) ) {
	xrtMutexLock(&tMutex);
	updateSharedState();
	xrtMutexUnlock(&tMutex);
	xrtMutexUnit(&tMutex);
}
```

## Condition

`xcond` 必须与 `xmutex` 配合。等待会原子释放 mutex，返回前重新获得 mutex。
通知不保存状态，因此必须始终在受 mutex 保护的谓词循环中等待。
调用等待函数时，当前线程必须已经持有传入的 mutex；否则返回 `XWAIT_ERROR` 并设置
`XERR_STATE`。在满足此前置条件时，等待无论因通知、超时还是平台错误返回，当前线程
仍持有 mutex。

| 函数 | 说明 |
|---|---|
| `xrtCondInit(pCond)` | 初始化调用方存储。 |
| `xrtCondUnit(pCond)` | 释放平台资源。 |
| `xrtCondCreate()` | 创建 condition。 |
| `xrtCondDestroy(pCond)` | 释放拥有式 condition。 |
| `xrtCondWait(pCond, pMutex)` | 无限等待通知。 |
| `xrtCondWaitFor(pCond, pMutex, iTimeout)` | 最多等待相对微秒数。 |
| `xrtCondWaitUntil(pCond, pMutex, iDeadline)` | 等待到绝对单调 deadline。 |
| `xrtCondSignal(pCond)` | 唤醒一个当前等待者。 |
| `xrtCondBroadcast(pCond)` | 唤醒全部当前等待者。 |

```c
xrtMutexLock(&tState.Mutex);
while ( !tState.Ready ) {
	xwaitresult Result = xrtCondWaitUntil(
		&tState.Cond,
		&tState.Mutex,
		iDeadline
	);
	if ( Result != XWAIT_OK ) {
		xrtMutexUnlock(&tState.Mutex);
		return Result;
	}
}
consumeState(&tState);
xrtMutexUnlock(&tState.Mutex);
```

修改谓词的线程应在同一 mutex 内更新状态，再调用 `Signal` 或 `Broadcast`。

## Semaphore

`xsem` 是范围统一为 `[0, INT32_MAX]` 的计数信号量。等待成功会消费一个计数。
`PostMany` 是全有或全无操作，超过最大值时原计数不变。

| 函数 | 说明 |
|---|---|
| `xrtSemInit(pSem, iInitial, iMaximum)` | 初始化有界计数；最大值必须非零且不超过 `INT32_MAX`。 |
| `xrtSemUnit(pSem)` | 释放平台资源。 |
| `xrtSemCreate(iInitial, iMaximum)` | 创建 semaphore。 |
| `xrtSemDestroy(pSem)` | 释放拥有式 semaphore。 |
| `xrtSemWait(pSem)` | 无限等待并消费一个计数。 |
| `xrtSemTryWait(pSem)` | 非阻塞尝试消费，空时返回 `XWAIT_TIMEOUT`。 |
| `xrtSemWaitFor(pSem, iTimeout)` | 在相对微秒数内等待。 |
| `xrtSemWaitUntil(pSem, iDeadline)` | 等待到绝对单调 deadline。 |
| `xrtSemPost(pSem)` | 发布一个计数。 |
| `xrtSemPostMany(pSem, iCount)` | 原子发布多个计数；`0` 成功且无操作。 |

```c
xsem* pSlots = xrtSemCreate(4, 4);

if ( xrtSemWaitFor(pSlots, UINT64_C(500000)) == XWAIT_OK ) {
	useOneSlot();
	xrtSemPost(pSlots);
}
xrtSemDestroy(pSlots);
```

## RWLock

`xrwlock` 允许多个读者或一个写者，并采用写者优先策略，避免持续读流量饿死写者。
写锁记录持有线程，拒绝递归写、非持有者释放和非法降级。

| 函数 | 说明 |
|---|---|
| `xrtRWLockInit(pLock)` | 初始化调用方存储。 |
| `xrtRWLockUnit(pLock)` | 无持有者和等待者时释放平台资源。 |
| `xrtRWLockCreate()` | 创建 RWLock。 |
| `xrtRWLockDestroy(pLock)` | 释放拥有式 RWLock。 |
| `xrtRWLockRead(pLock)` | 获得共享读锁。 |
| `xrtRWLockTryRead(pLock)` | 无写者或等待写者时尝试获得读锁。 |
| `xrtRWLockReadUnlock(pLock)` | 释放调用方持有的一个读锁。 |
| `xrtRWLockWrite(pLock)` | 获得独占写锁。 |
| `xrtRWLockTryWrite(pLock)` | 无读者和写者时尝试获得写锁。 |
| `xrtRWLockWriteUnlock(pLock)` | 由写持有线程释放写锁。 |
| `xrtRWLockDowngrade(pLock)` | 原子把当前写锁降级成一个读锁。 |
| `xrtRWLockUpgrade(pLock)` | 释放调用方的一个读锁并排队获得写锁。 |

读锁不维护每线程所有权表，以保持读路径紧凑。调用方必须只释放自己持有的读锁；
读锁是非递归的，同一线程不得在释放前再次获取。调用 `Upgrade` 时必须确实持有且只
持有一个读锁。Upgrade 先释放该读锁再进入写者队列，
因此多个升级者不会彼此保留读锁而死锁。

```c
xrtRWLockRead(&tCache.Lock);
if ( cacheNeedsRefresh(&tCache) ) {
	if ( xrtRWLockUpgrade(&tCache.Lock) ) {
		refreshCache(&tCache);
		xrtRWLockDowngrade(&tCache.Lock);
	}
}
readCache(&tCache);
xrtRWLockReadUnlock(&tCache.Lock);
```

## Event

`xevent` 保存显式信号状态。自动复位事件的一次信号只释放一个等待者；没有等待者时
保留一个信号。手动复位事件在 `Reset` 前保持有信号并释放全部等待者。

| 函数 | 说明 |
|---|---|
| `xrtEventInit(pEvent, bManualReset, bSignaled)` | 初始化事件并指定复位方式和初始状态。 |
| `xrtEventUnit(pEvent)` | 释放平台资源。 |
| `xrtEventCreate(bManualReset, bSignaled)` | 创建事件。 |
| `xrtEventDestroy(pEvent)` | 释放拥有式事件。 |
| `xrtEventWait(pEvent)` | 无限等待信号。 |
| `xrtEventTryWait(pEvent)` | 非阻塞检查；自动复位事件成功时消费信号。 |
| `xrtEventWaitFor(pEvent, iTimeout)` | 在相对微秒数内等待。 |
| `xrtEventWaitUntil(pEvent, iDeadline)` | 等待到绝对单调 deadline。 |
| `xrtEventSet(pEvent)` | 设置有信号状态并按复位方式唤醒等待者。 |
| `xrtEventReset(pEvent)` | 清除有信号状态。 |

```c
xevent* pStopped = xrtEventCreate(true, false);

startWorker(pStopped);
if ( xrtEventWaitFor(pStopped, UINT64_C(2000000)) == XWAIT_OK ) {
	consumeWorkerResult();
}
xrtEventReset(pStopped);
xrtEventDestroy(pStopped);
```

Event 适合完成通知、一次性闸门和取消唤醒；由共享数据决定的复杂条件应使用
mutex、condition 和谓词循环。

## 错误口径

等待函数返回 `xwaitresult`。竞争、超时和尚未触发属于正常控制流，不设置错误；
无效参数、未初始化对象、所有权错误和平台失败会设置结构化 `xrt.sync` 错误。
返回 `bool` 的函数失败后，可通过 `xrtErrorGet()` 或 `xrtGetError()` 取得详情。

基础可运行示例见 `examples/concurrency/sync/main.c`、`condition/main.c`、
`semaphore/main.c` 和 `rwlock/main.c`。每种原语的跨线程、超时、
多等待者、错误状态和 OOM 边界见 `tests/concurrency/test_mutex.c`、
`test_cond.c`、`test_sem.c`、`test_rwlock.c`、`test_event.c` 和
`test_sync_oom.c`。
