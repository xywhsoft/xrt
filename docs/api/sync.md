# Sync

同步体系提供 mutex、condition、semaphore、RWLock 和 event。每种能力独立裁剪，
公共对象不暴露 Win32 或 pthread 类型。

## 类型与常量

### `xmutex`

Mutex 使用固定对齐存储，允许嵌入用户结构且不暴露平台头。

```c
typedef union xmutex {
	uint64 Alignment;
	uint8 Storage[XRT_MUTEX_STORAGE_SIZE];
} xmutex;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xcond`

条件变量必须和 XRT mutex 配合使用。

```c
typedef union xcond {
	uint64 Alignment;
	uint8 Storage[XRT_COND_STORAGE_SIZE];
} xcond;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xsem`

信号量的计数范围在所有平台统一为 [0, INT32_MAX]。

```c
typedef union xsem {
	uint64 Alignment;
	uint8 Storage[XRT_SEM_STORAGE_SIZE];
} xsem;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xrwlock`

读写锁采用写者优先策略并支持升级和降级。

```c
typedef union xrwlock {
	uint64 Alignment;
	uint8 Storage[XRT_RWLOCK_STORAGE_SIZE];
} xrwlock;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xevent`

事件保存显式信号状态，可选择自动或手动复位。

```c
typedef union xevent {
	uint64 Alignment;
	uint8 Storage[XRT_EVENT_STORAGE_SIZE];
} xevent;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

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

### `xrtMutexInit`

初始化调用方存储中的非递归互斥锁。

```c
bool xrtMutexInit(xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMutex` | 输出 | 非空 | 接收互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sync](../../examples/concurrency/sync/main.c) · 初始化

```c
	if ( !xrtMutexInit(&tMutex) ) {
```

### `xrtMutexUnit`

释放互斥锁平台资源；仍被持有时失败且保持对象有效。

```c
bool xrtMutexUnit(xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMutex` | 输入 | 非空、未持有 | 目标互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 仍被持有 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync](../../examples/concurrency/sync/main.c) · 释放平台资源

```c
	(void)xrtMutexUnit(&tMutex);
```

### `xrtMutexCreate`

创建一个非递归互斥锁。

```c
xmutex* xrtMutexCreate(void)
```

#### 参数

| 无参数 | — | — | — |


#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 互斥锁 | — |
| `NULL` | 分配或初始化失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 堆创建

```c
	pMutex = xrtMutexCreate();
```

### `xrtMutexDestroy`

释放 Create 返回的互斥锁；仍被持有时失败且不释放对象。

```c
bool xrtMutexDestroy(xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMutex` | 输入 | 非空、未持有 | 目标互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已销毁 | — |
| `false` | 仍被持有 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 销毁

```c
	xrtMutexDestroy(pMutex);
```

### `xrtMutexLock`

阻塞到获得互斥锁；同线程递归加锁返回错误。

```c
bool xrtMutexLock(xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMutex` | 输入 | 非空、未由本线程持有 | 目标互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已持有 | — |
| `false` | 递归加锁或状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 同线程递归加锁

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 加锁

```c
	(void)xrtMutexLock(pJob->pMutex);
```

### `xrtMutexTryLock`

尝试获得互斥锁；锁正忙时返回 `false` 且不设置错误。

```c
bool xrtMutexTryLock(xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMutex` | 输入 | 非空 | 目标互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已持有 | — |
| `false` | 正忙 | 不设错误 |

#### 错误

- 正忙返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 尝试加锁

```c
		xrtMutexTryLock(pMutex) ||  /* 已持有：Try 必失败 */
```

### `xrtMutexUnlock`

释放当前线程持有的互斥锁。

```c
bool xrtMutexUnlock(xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMutex` | 输入 | 非空、本线程持有 | 目标互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 未持有 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 解锁

```c
	(void)xrtMutexUnlock(pJob->pMutex);
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
条件变量允许虚假唤醒，通知也不会保存为可供未来等待者消费的状态。因此 `Wait`、`WaitFor` 和 `WaitUntil` 只能放在受同一 mutex 保护的谓词循环中；返回 `XWAIT_OK` 只表示线程已经重新取得 mutex，不表示谓词必然成立。

### `xrtCondInit`

初始化调用方存储中的条件变量。

```c
bool xrtCondInit(xcond* pCond)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输出 | 非空 | 接收条件变量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[condition](../../examples/concurrency/condition/main.c) · 初始化

```c
	if ( !xrtCondInit(&Cond) ) {
```

### `xrtCondUnit`

释放条件变量平台资源。

```c
bool xrtCondUnit(xcond* pCond)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[condition](../../examples/concurrency/condition/main.c) · 释放平台资源

```c
		(void)xrtCondUnit(&Cond);
```

### `xrtCondCreate`

创建条件变量。

```c
xcond* xrtCondCreate(void)
```

#### 参数

| 无参数 | — | — | — |


#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 条件变量 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 堆创建

```c
	pCond = xrtCondCreate();
```

### `xrtCondDestroy`

释放 Create 返回的条件变量。

```c
bool xrtCondDestroy(xcond* pCond)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已销毁 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 销毁

```c
	xrtCondDestroy(pCond);
```

### `xrtCondWait`

当前线程持有 mutex 时原子释放并等待；允许虚假唤醒，必须在谓词循环中调用。

```c
xwaitresult xrtCondWait(xcond* pCond, xmutex* pMutex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |
| `pMutex` | 输入 | 非空、本线程持有 | 配对互斥锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 虚假唤醒后的再次超时 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法
- 允许虚假唤醒，返回 `OK` 后仍须循环检查谓词

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 等待

```c
		pJob->iWaitResult = xrtCondWait(pJob->pCond,
			pJob->pMutex);
```

### `xrtCondWaitFor`

在相对微秒数内等待；超时和成功后都重新持有 mutex。

```c
xwaitresult xrtCondWaitFor(xcond* pCond, xmutex* pMutex, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |
| `pMutex` | 输入 | 非空、本线程持有 | 配对互斥锁 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法
- 允许虚假唤醒，应循环检查受 mutex 保护的谓词

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 限时等待

```c
	pJob->iForResult = xrtCondWaitFor(pJob->pCond, pJob->pMutex,
		EXAMPLE_TIMEOUT_US);  /* 无人 Signal：到期 */
```

### `xrtCondWaitUntil`

等待到单调时钟截止时间；允许虚假唤醒。

```c
xwaitresult xrtCondWaitUntil(xcond* pCond, xmutex* pMutex, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |
| `pMutex` | 输入 | 非空、本线程持有 | 配对互斥锁 |
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法
- 允许虚假唤醒，应循环检查受 mutex 保护的谓词

#### 范例

[condition](../../examples/concurrency/condition/main.c) · 限期等待

```c
	Result = xrtCondWaitUntil(
		&Cond,
		&Mutex,
		xrtDeadlineAfter(UINT64_C(1000))
	);
```

### `xrtCondSignal`

唤醒一个等待者；通知本身不保存状态。

```c
bool xrtCondSignal(xcond* pCond)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已发出通知 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 唤醒一个

```c
	(void)xrtCondSignal(pCond);
```

### `xrtCondBroadcast`

唤醒全部当前等待者；通知本身不保存状态。

```c
bool xrtCondBroadcast(xcond* pCond)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCond` | 输入 | 非空 | 目标条件变量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已发出通知 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[condition](../../examples/concurrency/condition/main.c) · 唤醒全部

```c
	if ( !xrtCondSignal(&Cond) || !xrtCondBroadcast(&Cond) ||
		!xrtMutexLock(&Mutex) ) {
```

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

### `xrtSemInit`

初始化计数信号量。

```c
bool xrtSemInit(xsem* pSem, uint32 iInitial, uint32 iMaximum)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输出 | 非空 | 接收信号量 |
| `iInitial` | 输入 | <= `iMaximum` | 初始计数 |
| `iMaximum` | 输入 | > 0 | 最大计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[semaphore](../../examples/concurrency/semaphore/main.c) · 初始化

```c
	if ( !xrtSemInit(&Semaphore, 0, 1) ||
		!xrtSemPost(&Semaphore) ) {
```

### `xrtSemUnit`

释放信号量平台资源。

```c
bool xrtSemUnit(xsem* pSem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[semaphore](../../examples/concurrency/semaphore/main.c) · 释放平台资源

```c
	return xrtSemUnit(&Semaphore) &&
```

### `xrtSemCreate`

创建计数信号量。

```c
xsem* xrtSemCreate(uint32 iInitial, uint32 iMaximum)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iInitial` | 输入 | <= `iMaximum` | 初始计数 |
| `iMaximum` | 输入 | > 0 | 最大计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 信号量 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 堆创建

```c
	pSem = xrtSemCreate(0u, 4u);
```

### `xrtSemDestroy`

释放 Create 返回的信号量。

```c
bool xrtSemDestroy(xsem* pSem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已销毁 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 销毁

```c
	xrtSemDestroy(pSem);
```

### `xrtSemWait`

等待并消费一个信号。

```c
xwaitresult xrtSemWait(xsem* pSem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 等待

```c
		(xrtSemWait(pSem) != XWAIT_OK) ||
```

### `xrtSemTryWait`

非阻塞地尝试消费一个信号。

```c
xwaitresult xrtSemTryWait(xsem* pSem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 已消费一个信号 | — |
| `XWAIT_TIMEOUT` | 当前计数为零 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 尝试等待

```c
		(xrtSemTryWait(pSem) != XWAIT_TIMEOUT) ) {
```

### `xrtSemWaitFor`

在相对微秒数内等待并消费一个信号。

```c
xwaitresult xrtSemWaitFor(xsem* pSem, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 限时等待

```c
		(xrtSemWaitFor(pSem, EXAMPLE_TIMEOUT_US) != XWAIT_OK) ) {
```

### `xrtSemWaitUntil`

等待并消费一个信号到指定单调时钟截止时间。

```c
xwaitresult xrtSemWaitUntil(xsem* pSem, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[semaphore](../../examples/concurrency/semaphore/main.c) · 限期等待

```c
	Result = xrtSemWaitUntil(
		&Semaphore,
		xrtDeadlineAfter(UINT64_C(1000000))
	);
```

### `xrtSemPost`

发布一个信号；达到上限时失败且计数不变。

```c
bool xrtSemPost(xsem* pSem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已发布 | — |
| `false` | 达到上限或状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 计数达到上限，计数不变

#### 范例

[semaphore](../../examples/concurrency/semaphore/main.c) · 发布

```c
		!xrtSemPost(&Semaphore) ) {
```

### `xrtSemPostMany`

原子发布多个信号；超过上限时失败且不部分发布。

```c
bool xrtSemPostMany(xsem* pSem, uint32 iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSem` | 输入 | 非空 | 目标信号量 |
| `iCount` | 输入 | — | 发布数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已全部发布 | — |
| `false` | 超过上限 | `XERR_STATE`，不部分发布 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 发布后超过上限，计数不变

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 批量发布

```c
	if ( !xrtSemPostMany(pSem, 3u) ||
		(xrtSemTryWait(pSem) != XWAIT_OK) ||
		(xrtSemWait(pSem) != XWAIT_OK) ||
		(xrtSemWaitFor(pSem, EXAMPLE_TIMEOUT_US) != XWAIT_OK) ) {
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

### `xrtRWLockInit`

初始化写者优先的读写锁。

```c
bool xrtRWLockInit(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输出 | 非空 | 接收读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[rwlock](../../examples/concurrency/rwlock/main.c) · 初始化

```c
	if ( !xrtRWLockInit(&Lock) ) {
```

### `xrtRWLockUnit`

释放读写锁平台资源；仍被持有或等待时失败。

```c
bool xrtRWLockUnit(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空、无持有无等待 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 仍被持有或等待 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[rwlock](../../examples/concurrency/rwlock/main.c) · 释放平台资源

```c
	return xrtRWLockUnit(&Lock) && bOkay && (iValue == 15) ? 0 : 2;
```

### `xrtRWLockCreate`

创建写者优先的读写锁。

```c
xrwlock* xrtRWLockCreate(void)
```

#### 参数

| 无参数 | — | — | — |


#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 读写锁 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 堆创建

```c
	pLock = xrtRWLockCreate();
```

### `xrtRWLockDestroy`

释放 Create 返回的读写锁。

```c
bool xrtRWLockDestroy(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空、无持有无等待 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已销毁 | — |
| `false` | 仍被持有或等待 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 销毁

```c
	xrtRWLockDestroy(pLock);
```

### `xrtRWLockRead`

获得非递归共享读锁；读锁所有权由调用方保证。

```c
bool xrtRWLockRead(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已持有读锁 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 读加锁

```c
		!xrtRWLockRead(pLock) ||
```

### `xrtRWLockTryRead`

尝试获得共享读锁；写者存在或等待时返回 `false`。

```c
bool xrtRWLockTryRead(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已持有读锁 | — |
| `false` | 写者存在或等待 | 不设错误 |

#### 错误

- 写者存在或等待返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[rwlock](../../examples/concurrency/rwlock/main.c) · 尝试读加锁

```c
		!xrtRWLockTryRead(&Lock) ) {
```

### `xrtRWLockReadUnlock`

释放当前线程持有的一个读锁。

```c
bool xrtRWLockReadUnlock(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空、本线程持有读锁 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 未持有 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 读解锁

```c
		!xrtRWLockReadUnlock(pLock) ||
```

### `xrtRWLockWrite`

获得独占写锁。

```c
bool xrtRWLockWrite(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已持有写锁 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[rwlock](../../examples/concurrency/rwlock/main.c) · 写加锁

```c
		!xrtRWLockWrite(&Lock) ) {
```

### `xrtRWLockTryWrite`

尝试获得独占写锁。

```c
bool xrtRWLockTryWrite(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已持有写锁 | — |
| `false` | 存在其他持有者 | 不设错误 |

#### 错误

- 存在其他持有者返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 尝试写加锁

```c
		xrtRWLockTryWrite(pLock) ||  /* 读持有时写必失败 */
```

### `xrtRWLockWriteUnlock`

释放当前线程持有的写锁。

```c
bool xrtRWLockWriteUnlock(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空、本线程持有写锁 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 未持有 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 写解锁

```c
		!xrtRWLockWriteUnlock(pLock) ) {
```

### `xrtRWLockDowngrade`

原子地把当前线程的写锁降级为一个读锁。

```c
bool xrtRWLockDowngrade(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空、本线程持有写锁 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已降级为读锁 | — |
| `false` | 未持有写锁 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[rwlock](../../examples/concurrency/rwlock/main.c) · 降级

```c
	if ( !xrtRWLockDowngrade(&Lock) ||
		!xrtRWLockReadUnlock(&Lock) ||
		!xrtRWLockWrite(&Lock) ) {
```

### `xrtRWLockUpgrade`

当前线程只持有一个读锁时，释放它并排队获得写锁。

```c
bool xrtRWLockUpgrade(xrwlock* pLock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLock` | 输入 | 非空、本线程恰好持有一个读锁 | 目标读写锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已升级为写锁 | — |
| `false` | 持有数不是一或状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 当前线程持有的读锁数量不是恰好一个

#### 范例

[rwlock](../../examples/concurrency/rwlock/main.c) · 升级

```c
		!xrtRWLockUpgrade(&Lock) ) {
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

### `xrtEventInit`

初始化自动或手动复位事件。

```c
bool xrtEventInit(xevent* pEvent, bool bManualReset, bool bSignaled)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输出 | 非空 | 接收事件 |
| `bManualReset` | 输入 | — | 手动复位 |
| `bSignaled` | 输入 | — | 初始信号态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 初始化

```c
	if ( !xrtEventInit(&tManual, true, false) ) {
```

### `xrtEventUnit`

释放事件平台资源。

```c
bool xrtEventUnit(xevent* pEvent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 释放平台资源

```c
	xrtEventUnit(&tManual);
```

### `xrtEventCreate`

创建自动或手动复位事件。

```c
xevent* xrtEventCreate(bool bManualReset, bool bSignaled)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `bManualReset` | 输入 | — | 手动复位 |
| `bSignaled` | 输入 | — | 初始信号态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 事件 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 堆创建

```c
	pAuto = xrtEventCreate(false, false);
```

### `xrtEventDestroy`

释放 Create 返回的事件。

```c
bool xrtEventDestroy(xevent* pEvent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已销毁 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 销毁

```c
	xrtEventDestroy(pAuto);
```

### `xrtEventWait`

等待事件进入信号态。

```c
xwaitresult xrtEventWait(xevent* pEvent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 不会发生（无限等待） | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法
- 自动复位事件等到即消费信号

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 等待

```c
		(xrtEventWait(&tManual) != XWAIT_OK) ||
```

### `xrtEventTryWait`

非阻塞地检查并消费自动复位事件。

```c
xwaitresult xrtEventTryWait(xevent* pEvent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 已等到并消费 | — |
| `XWAIT_TIMEOUT` | 未处于信号态 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 尝试等待

```c
		(xrtEventTryWait(pAuto) != XWAIT_TIMEOUT) ||
```

### `xrtEventWaitFor`

在相对微秒数内等待事件。

```c
xwaitresult xrtEventWaitFor(xevent* pEvent, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 限时等待

```c
		(xrtEventWaitFor(pAuto, EXAMPLE_TIMEOUT_US) !=
			XWAIT_TIMEOUT) ) {
```

### `xrtEventWaitUntil`

等待事件到指定单调时钟截止时间。

```c
xwaitresult xrtEventWaitUntil(xevent* pEvent, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 获得/等到 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 限期等待

```c
		(xrtEventWaitUntil(&tManual,
			xrtDeadlineAfter(UINT64_C(1))) != XWAIT_OK) ||
```

### `xrtEventSet`

设置事件；手动复位唤醒全部等待者，自动复位唤醒一个等待者。

```c
bool xrtEventSet(xevent* pEvent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已置位 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 置位

```c
		!xrtEventSet(pAuto) ||
```

### `xrtEventReset`

清除事件信号态。

```c
bool xrtEventReset(xevent* pEvent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已清除 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 对象仍被持有、等待或状态非法

#### 范例

[sync_tour](../../examples/concurrency/sync_tour/main.c) · 复位

```c
		!xrtEventReset(&tManual) ||
```

## 错误口径

等待函数返回 `xwaitresult`。竞争、超时和尚未触发属于正常控制流，不设置错误；
无效参数、未初始化对象、所有权错误和平台失败会设置结构化 `xrt.sync` 错误。
返回 `bool` 的函数失败后，可通过 `xrtErrorGet()` 或 `xrtGetError()` 取得详情。

基础可运行示例见 `examples/concurrency/sync/main.c`、`condition/main.c`、
`semaphore/main.c` 和 `rwlock/main.c`。每种原语的跨线程、超时、
多等待者、错误状态和 OOM 边界见 `tests/concurrency/test_mutex.c`、
`test_cond.c`、`test_sem.c`、`test_rwlock.c`、`test_event.c` 和
`test_sync_oom.c`。
