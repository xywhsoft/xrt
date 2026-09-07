# Thread

`thread` 提供可等待、可共享、可安全分离的原生线程对象。启用宏为
`XRT_FEATURE_THREAD`，依赖 `XRT_FEATURE_WAIT`。

## 类型

```c
typedef struct xthread xthread;
typedef int32 (*xthreadproc)(ptr pData);
```

`xthread` 是不透明对象。线程入口接收创建时的 `pData`，返回稳定的 32 位退出码。

```c
typedef enum xthreadstate {
	XTHREAD_RUNNING = 0,
	XTHREAD_FINISHED = 1
} xthreadstate;
```

状态只表达“运行”和“完成”。协作停止请求不是第三种执行状态。

### `xonce`

Once 对象的字段保持不透明，只能使用 XRT_ONCE_INIT 或全零初始化。

```c
typedef union xonce {
	uint64 Alignment;
	uint8 Storage[XRT_ONCE_STORAGE_SIZE];
} xonce;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | Alignment |

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

### `xonceproc`

初始化过程返回 true 时永久完成，返回 false 时允许后续调用重试。

```c
typedef bool (*xonceproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

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

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_ONCE_STORAGE_SIZE` | `16u` | Once 使用固定存储并允许静态零初始化。 |
| `XRT_ONCE_INIT` | `{ 0 }` | 静态 Once 对象初始化值。 |

## 所有权

- `xrtThreadCreate()` 返回一个拥有引用，运行线程另外持有一个内部引用。
- 调用方可以立即 `xrtThreadDestroy()`，让线程在没有外部句柄时安全运行到结束。
- 跨所有权边界共享对象时调用 `xrtThreadRef()`；每个拥有引用最终释放一次。
- `xrtThreadCurrent()` 返回借用对象，只在线程入口执行期间有效，不得释放或保存到入口之外。
- 等待者必须在整个等待期间持有对象引用；多个等待者可以并发等待同一对象。

## 函数

| 函数 | 说明 |
|---|---|
| `xrtThreadCreate(pProc, pData, iStackSize)` | 创建并立即启动线程；栈大小为 `0` 时使用平台默认值。失败返回 `NULL`。 |
| `xrtThreadRef(pThread)` | 增加拥有引用并返回原指针；无效对象返回 `NULL`。 |
| `xrtThreadDestroy(pThread)` | 释放一个拥有引用；传入 `NULL` 无操作，不等待也不强制停止线程。 |
| `xrtThreadWait(pThread)` | 无限等待线程执行体和 XRT 线程上下文清理完成，返回 `xwaitresult`。 |
| `xrtThreadWaitFor(pThread, iTimeout)` | 最多等待相对微秒数。`0` 是非阻塞状态检查。 |
| `xrtThreadWaitUntil(pThread, iDeadline)` | 等待线程执行体和 XRT 线程上下文清理完成到单调时钟 deadline。 |
| `xrtThreadStop(pThread)` | 幂等地发布协作停止请求。 |
| `xrtThreadStopRequested(pThread)` | 查询指定线程是否收到停止请求。 |
| `xrtThreadStopping()` | 在线程入口内查询当前线程的停止请求；外部线程返回 `false`。 |
| `xrtThreadState(pThread)` | 返回线程状态快照。 |
| `xrtThreadExitCode(pThread)` | 线程完成后返回退出码；仍运行时返回 `0` 并设置状态错误。 |
| `xrtThreadId(pThread)` | 返回对象记录的进程内非零线程标识。 |
| `xrtThreadCurrentId()` | 返回调用线程的进程内非零线程标识，外部创建的线程也可使用。 |
| `xrtThreadCurrent()` | 返回当前 XRT 线程的借用对象；外部线程返回 `NULL`。 |
| `xrtThreadYield()` | 主动让出当前处理器时间片。 |

`xrtThreadWait()`、`xrtThreadWaitFor()` 和 `xrtThreadWaitUntil()` 可以重复调用，
超时不会改变对象状态，也不会消费后续等待机会。线程等待自己会返回 `XWAIT_ERROR`
并设置状态错误。成功返回时，线程入口已经返回，线程键、协程及当前错误等 XRT
线程上下文已经清理，内部运行引用也已经释放。

线程标识只用于同一进程生命周期内的相等性比较，不是可持久化编号。线程结束后，
操作系统可以把相同标识分配给后续线程。`xrtThreadExitCode()` 在线程仍运行时返回零并
设置 `XERR_STATE`，因此调用方应先等待完成，不能仅凭返回的零判断真实退出码。

## 创建和等待

```c
static int32 worker(ptr pData)
{
	return *(int*)pData;
}

int iValue = 42;
xthread* pThread = xrtThreadCreate(worker, &iValue, 0);

if ( pThread == NULL ) {
	return -1;
}
if ( xrtThreadWaitFor(pThread, UINT64_C(2000000)) != XWAIT_OK ) {
	xrtThreadDestroy(pThread);
	return -1;
}
printf("%d\n", xrtThreadExitCode(pThread));
xrtThreadDestroy(pThread);
```

## 共享与分离

```c
xthread* pShared = xrtThreadRef(pThread);

xrtThreadDestroy(pThread);       /* 释放原引用 */
consumeThread(pShared);          /* 接收方拥有 pShared */

xthread* pDetached = xrtThreadCreate(backgroundWorker, NULL, 0);
xrtThreadDestroy(pDetached);     /* 不等待，线程仍可安全结束 */
```

## 协作停止

```c
static int32 serviceLoop(ptr pData)
{
	(void)pData;
	while ( !xrtThreadStopping() ) {
		processOneBatch();
	}
	return 0;
}

xthread* pThread = xrtThreadCreate(serviceLoop, NULL, 0);
xrtThreadStop(pThread);
xrtThreadWait(pThread);
xrtThreadDestroy(pThread);
```

库不提供强制终止、挂起或恢复线程。这些操作会破坏资源清理顺序，且跨平台语义不一致。

## 线程安全与错误

状态、退出码、停止请求和等待都可并发访问。引用计数只保护已经合法持有的引用，
不能让一个线程在另一线程释放最后一个外部引用的同时从裸指针获取新引用。
API 返回失败时，当前线程可通过结构化错误接口读取 `xrt.thread` 错误域信息。

可运行示例见 `examples/concurrency/thread/main.c`；生命周期、detach、多等待者、
自等待和 OOM 边界见 `tests/concurrency/test_thread.c` 与
`tests/concurrency/test_thread_oom.c`。

## API 参考

以下各节复用自 [once.md](once.md)（两者映射同一 `include/xrt/thread.h`），按线程视角重组。

## Once 一次初始化

可失败、可重试的并发一次初始化。

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

引用计数线程对象、等待与协作停止。

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

动态原生线程局部存储。

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
