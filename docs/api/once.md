# Once 一次初始化

`once` 模块提供可失败、可重试的并发一次初始化。它使用固定存储，不分配内存，也不依赖完整线程模块。

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_ONCE` |
| 直接依赖 | `XRT_FEATURE_CORE` |
| 头文件 | `<xrt/thread.h>` 或 `<xrt.h>` |

## 类型与常量

### `xonce`

不透明的一次初始化对象。对象必须使用 `XRT_ONCE_INIT`、静态零初始化或完整清零初始化，初始化开始后不能复制、移动或再次清零。

### `xonceproc`

```c
typedef bool (*xonceproc)(ptr pData);
```

初始化过程返回 `true` 表示永久完成；返回 `false` 表示本次失败，后续调用可以重新执行。过程应在失败前设置能够说明原因的线程错误。

### `XRT_ONCE_INIT`

静态和自动对象的初始化值。`XRT_ONCE_STORAGE_SIZE` 表示公开不透明存储的字节数，不应依赖其内部布局。

### `xthreadstate`

线程只有运行和完成两种可观测状态，停止请求不伪装成执行状态。

```c
typedef enum xthreadstate {
	XTHREAD_RUNNING = 0,
	XTHREAD_FINISHED = 1
} xthreadstate;
```

| 值 | 语义 |
|---|---|
| `XTHREAD_RUNNING` | XTHREAD运行中 |
| `XTHREAD_FINISHED` | （见枚举语义） |

### `xthreadkey`

动态键按原生线程隔离，同一线程上的 Fiber 和协程共享其值。

```c
typedef struct xthreadkey xthreadkey;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xthread`

原生线程对象对外保持不透明，并使用引用计数管理生命周期。

```c
typedef struct xthread xthread;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xthreadkeyproc`

非空线程局部值在线程退出、显式清理或被替换时交给析构过程。

```c
typedef void (*xthreadkeyproc)(ptr pValue);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xthreadproc`

线程入口返回稳定的 32 位退出码。

```c
typedef int32 (*xthreadproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 函数

### `xrtOnce`

并发执行一次初始化；成功后所有调用返回 `true`，失败后状态回到未完成并允许后续调用重试。

```c
bool xrtOnce(xonce* pOnce, xonceproc pProc, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOnce` | 输入/输出 | 非空、已初始化 | 一次初始化对象 |
| `pProc` | 输入 | 非空 | 初始化过程，返回 false = 本次失败 |
| `pData` | 输入 | 任意值 | 过程数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 初始化已完成（本次或此前） | — |
| `false` | 本次初始化失败，可重试 | 过程保留的错误或 `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 同一线程从初始化过程递归进入相同对象（自死锁保护）
- 过程失败时应自行设置线程错误；未设置时不额外构造

#### 范例

[once](../../examples/concurrency/once/main.c) · 一次初始化

```c
	if ( !xrtOnce(&tOnce, initializeConfig, &iConfig) ) {
```

## 原生线程

### `xrtThreadCreate`

创建并立即启动原生线程；栈大小为零时使用平台默认值。对象使用引用计数，初始引用包含调用方一份和运行自持一份。

```c
xthread* xrtThreadCreate(xthreadproc pProc, ptr pData, size_t iStackSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProc` | 输入 | 非空 | 线程执行体 |
| `pData` | 输入 | 任意值 | 执行体数据 |
| `iStackSize` | 输入 | 0 = 平台默认 | 栈字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 线程对象（引用 1 + 运行自持） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象分配失败
- `xrt.thread` 域错误 — 平台线程创建失败，保留系统码

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 创建并启动

```c
	pThread = xrtThreadCreate(exampleWorker, NULL, 0u);
```

### `xrtThreadRef`

增加线程对象引用并返回原指针。

```c
xthread* xrtThreadRef(xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 共享引用

```c
	pRef = xrtThreadRef(pThread);
```

### `xrtThreadDestroy`

释放线程对象引用；运行线程自持引用，因此允许用它安全分离线程。

```c
void xrtThreadDestroy(xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1；最后一个引用在运行结束后释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 释放引用

```c
	xrtThreadDestroy(pRef);  /* Ref 那份 */
```

### `xrtThreadWait`

等待线程执行体和 XRT 线程上下文清理完成；允许多个线程同时等待同一个对象。

```c
xwaitresult xrtThreadWait(xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空、非自身 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 线程执行体和上下文清理完成 | — |
| `XWAIT_TIMEOUT` | 相对期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数非法或等待自身线程 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — `pThread` 为空
- `XERR_STATE` — 等待调用方所在的同一线程（自等待）

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 无限等待

```c
	if ( (void)xrtThreadWait(pStop), false ) {
```

### `xrtThreadWaitFor`

在相对微秒数内等待线程执行体和 XRT 线程上下文清理完成。

```c
xwaitresult xrtThreadWaitFor(xthread* pThread, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空、非自身 | 目标线程 |
| `iTimeout` | 输入 | — | 相对等待微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 线程执行体和上下文清理完成 | — |
| `XWAIT_TIMEOUT` | 相对期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数非法或等待自身线程 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — `pThread` 为空
- `XERR_STATE` — 等待调用方所在的同一线程（自等待）

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 限时等待

```c
		(xrtThreadWaitFor(pThread, 1000u) == XWAIT_OK) ) {
```

### `xrtThreadWaitUntil`

等待线程执行体和 XRT 线程上下文清理完成到指定单调时钟截止时间。

```c
xwaitresult xrtThreadWaitUntil(xthread* pThread, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空、非自身 | 目标线程 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 线程执行体和上下文清理完成 | — |
| `XWAIT_TIMEOUT` | 相对期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数非法或等待自身线程 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — `pThread` 为空
- `XERR_STATE` — 等待调用方所在的同一线程（自等待）

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 限期等待

```c
	if ( (xrtThreadWaitUntil(pThread,
			xrtDeadlineAfter(UINT64_C(3000000))) !=
			XWAIT_OK) ||
		(xrtThreadState(pThread) != XTHREAD_FINISHED) ||
		(xrtThreadExitCode(pThread) != 42) ||
		(xrtThreadId(pThread) == 0u) ) {
```

### `xrtThreadStop`

幂等地请求线程协作停止，不会强制终止执行。

```c
bool xrtThreadStop(xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已记录（重复请求同样成功） | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 请求停止

```c
	if ( !xrtThreadStop(pStop) ) {
```

### `xrtThreadStopRequested`

判断指定线程是否收到停止请求。

```c
bool xrtThreadStopRequested(const xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否收到请求 | 空句柄返回 `false`，不设错 |

#### 错误

- 无 — 纯原子查询，不设置错误

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 停止查询

```c
				xrtThreadStopRequested(pJob->pSelf);
```

### `xrtThreadStopping`

判断当前 XRT 线程是否收到停止请求。

```c
bool xrtThreadStopping(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 当前线程是否收到请求 | 不设错误 |

#### 错误

- 无 — 纯原子查询，不设置错误

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 自身停止查询

```c
		if ( xrtThreadStopping() ) {
```

### `xrtThreadState`

返回线程状态快照；只有运行和完成两种可观测状态，停止请求不伪装成执行状态。

```c
xthreadstate xrtThreadState(const xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTHREAD_RUNNING` | 执行体尚未结束 | — |
| `XTHREAD_FINISHED` | 执行体已结束 | 参数非法时返回 `XTHREAD_FINISHED` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 状态快照

```c
		(xrtThreadState(pThread) != XTHREAD_FINISHED) ||
```

### `xrtThreadExitCode`

返回完成线程的退出码；线程仍运行或参数无效时返回零并设置错误。

```c
int32 xrtThreadExitCode(const xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 退出码 | 执行体返回值 | — |
| `0` | 线程仍在运行或参数非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 线程仍在运行

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 退出码

```c
		(xrtThreadExitCode(pThread) != 42) ||
```

### `xrtThreadId`

返回线程对象在进程内稳定的非零平台标识；线程结束后该标识可能被平台复用。

```c
uint64 xrtThreadId(const xthread* pThread)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pThread` | 输入 | 非空 | 目标线程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 平台线程标识 | — |
| `0` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 线程标识

```c
		(xrtThreadId(pThread) == 0u) ) {
```

### `xrtThreadCurrentId`

返回当前线程在进程内稳定的非零平台标识，外部创建的线程同样可用。

```c
uint64 xrtThreadCurrentId(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 当前线程平台标识 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[thread](../../examples/concurrency/thread/main.c) · 当前线程标识

```c
		(unsigned long long)xrtThreadCurrentId());
```

### `xrtThreadCurrent`

返回当前 XRT 创建线程的借用对象；外部线程返回空指针。

```c
xthread* xrtThreadCurrent(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 当前线程对象借用，不增加引用 | — |
| `NULL` | 外部线程 | 不设错误 |

#### 错误

- 无错误 — 外部线程返回 `NULL` 且不设置错误

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 当前线程对象

```c
			pJob->bCurrentInside = xrtThreadCurrent() != NULL;
```

### `xrtThreadYield`

主动让出当前线程的处理器时间片。

```c
void xrtThreadYield(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已让出 | — |

#### 错误

- 无 — 让出不失败

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 让出时间片

```c
		xrtThreadYield();
```

## 线程局部键

### `xrtThreadKeyCreate`

创建动态原生线程局部键；析构过程可以为空。

```c
xthreadkey* xrtThreadKeyCreate(xthreadkeyproc pDestroy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDestroy` | 输入 | 允许空 | 线程退出时的值析构过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 键（引用 1） | — |
| `NULL` | 创建失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 键对象分配失败
- `xrt.thread` 域错误 — 平台 TLS 槽分配失败，保留系统码

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 创建键

```c
	g_pKey = xrtThreadKeyCreate(NULL);
```

### `xrtThreadKeyDestroy`

关闭键；其他线程不得再主动访问，已有线程槽会在退出时延迟释放。

```c
bool xrtThreadKeyDestroy(xthreadkey* pKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pKey` | 输入 | 非空、未关闭 | 目标键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 参数非法或已关闭 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 键已经关闭

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 关闭键

```c
	xrtThreadKeyDestroy(g_pKey);
```

### `xrtThreadKeyGet`

返回当前线程的借用值；当前线程尚未设置时返回空指针。

```c
ptr xrtThreadKeyGet(const xthreadkey* pKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pKey` | 输入 | 非空、未关闭 | 目标键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 当前线程的值借用 | — |
| `NULL` | 尚未设置 | 键已关闭时 `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 键已经关闭

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 读取值

```c
			(xrtThreadKeyGet(g_pKey) == NULL);
```

### `xrtThreadKeySet`

转移新值的所有权；成功替换后析构旧值，空值等价于清除。

```c
bool xrtThreadKeySet(xthreadkey* pKey, ptr pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pKey` | 输入 | 非空、未关闭 | 目标键 |
| `pValue` | 输入 | 任意值 | 新值，所有权转移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换并析构旧值 | — |
| `false` | 失败，新值仍归调用方 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 键已经关闭或线程槽扩展失败

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 设置值

```c
	if ( xrtThreadKeySet(g_pKey, &g_Sentinel) ) {
```

### `xrtThreadKeyTake`

取走当前线程的值但不执行析构；返回值所有权交给调用方。

```c
ptr xrtThreadKeyTake(xthreadkey* pKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pKey` | 输入 | 非空、未关闭 | 目标键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 取走的值，所有权归调用方 | — |
| `NULL` | 尚未设置 | 键已关闭时 `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 键已经关闭

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 取走值

```c
		ptr pTaken = xrtThreadKeyTake(g_pKey);
```

### `xrtThreadKeysClear`

析构并清除当前原生线程的全部动态键值；XRT 创建的线程退出时自动调用。

```c
bool xrtThreadKeysClear(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 当前线程全部键值已析构清除 | — |
| `false` | 当前线程无 XRT 上下文 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 外部线程尚未建立 XRT 线程上下文

#### 范例

[thread_tour](../../examples/concurrency/thread_tour/main.c) · 清空全部键值

```c
	xrtThreadKeysClear();
```

## 模块契约：所有权

`xonce` 为固定存储对象，初始化开始后不可复制或移动；线程键为引用计数对象（`Create` 引用 1，`Ref` 递增，`Destroy` 递减）；`KeySet`/`KeyTake` 转移槽内值的所有权。

## 示例

```c
static xonce gOnce = XRT_ONCE_INIT;
static int gValue;

static bool initialize(ptr pData)
{
	*(int*)pData = 42;
	return true;
}

if ( !xrtOnce(&gOnce, initialize, &gValue) ) {
	return false;
}
```

完整示例位于 `examples/concurrency/once/main.c`。
