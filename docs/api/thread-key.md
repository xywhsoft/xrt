# 动态线程局部键

`thread_key` 模块为库和嵌入式运行时提供动态原生线程局部值。值按操作系统线程隔离，同一原生线程上的宿主、Fiber 和 XRT 协程看见同一个值，不会因执行上下文切换而变化。

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_THREAD_KEY` |
| 直接依赖 | `XRT_FEATURE_CORE` |
| 头文件 | `<xrt/thread.h>` 或 `<xrt.h>` |

## 类型

### `xthreadkey`

不透明动态键。一个键可以在多个线程中分别保存一个值。调用方结束键的主动使用后可以立即销毁；已经安装的线程槽会持有内部引用，在线程退出或显式清理后完成最终释放。

### `xthreadkeyproc`

```c
typedef void (*xthreadkeyproc)(ptr pValue);
```

可选值析构过程。非空值被替换、清除、随线程退出或随键销毁时调用。调用过程取得值的所有权。

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
| `Alignment` | `uint64` | 对齐（二次幂） |

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

## 所有权与线程规则

- `xrtThreadKeySet` 成功后，键取得非空值的所有权。
- 设置同一个指针是无操作，不会重复析构。
- `xrtThreadKeyTake` 把所有权交还调用方，不执行析构。
- 每个原生线程的值互相隔离，同一线程上的全部协程共享线程值。
- XRT 创建的线程退出时自动清理。POSIX 外部线程也由 `pthread` 自动清理；Windows 外部线程必须在退出前调用 `xrtThreadKeysClear`，这是静态库和单头文件无法注册通用线程退出析构的明确平台边界。
- 销毁键前，调用方必须保证其他线程不再主动调用该键的 Get/Set/Take；其他线程可以尚未退出并保留已安装值。
- 销毁会立即关闭新访问并析构当前线程的值；其他线程的值在线程退出或显式清理时析构。
- 析构过程可以操作其他键。正在关闭的键会拒绝析构过程中的重入访问。

## 模块契约：错误

失败经 `xrtGetError()` 报告：

| 错误 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` | 指针为空或参数非法 |
| `XERR_STATE` | 键已关闭、槽状态非法或外部线程无 XRT 上下文 |
| `XERR_MEMORY` | 键对象分配失败 |
| `xrt.thread` 域错误 | 平台 TLS 槽创建失败（保留系统码） |

## 函数

### `xrtThreadKeyCreate`

```c
xthreadkey* xrtThreadKeyCreate(xthreadkeyproc pDestroy);
```

创建动态键。`pDestroy` 可以为空。失败返回空指针并设置结构化错误。

### `xrtThreadKeyDestroy`

```c
bool xrtThreadKeyDestroy(xthreadkey* pKey);
```

关闭键并析构当前线程仍拥有的值。空指针视为成功。成功后不能再主动访问键；平台 TLS key 和键对象会在最后一个线程槽清理后释放，因此其他线程可以安全完成退出析构。当前线程的平台槽清理失败时返回 `false`，键仍然有效。

### `xrtThreadKeyGet`

```c
ptr xrtThreadKeyGet(const xthreadkey* pKey);
```

返回当前线程值的借用指针；未设置时返回空指针。键为空时也返回空指针，但会设置 `XERR_ARGUMENT`。

### `xrtThreadKeySet`

```c
bool xrtThreadKeySet(xthreadkey* pKey, ptr pValue);
```

成功时转移新值所有权，并在替换完成后析构旧值。空值用于清除。分配或平台写入失败时返回 `false`，原值和新值的所有权均不改变。

### `xrtThreadKeyTake`

```c
ptr xrtThreadKeyTake(xthreadkey* pKey);
```

移除并返回当前线程的值，不执行析构。无值时返回空指针。平台清除失败时保留原值并设置错误。

### `xrtThreadKeysClear`

```c
bool xrtThreadKeysClear(void);
```

析构并移除当前原生线程保存的全部动态键值。函数幂等；析构过程重新安装值时最多继续清理四轮，与 POSIX 线程键退出语义一致。四轮后仍有值时返回 `false`、保留剩余值并设置 `XERR_STATE`，避免恶意或错误析构过程造成无限循环。

## 示例

```c
xthreadkey* pKey = xrtThreadKeyCreate(xrtFree);
int* pValue = (int*)xrtMalloc(sizeof(int));

if ( (pKey == NULL) || (pValue == NULL) ) {
	return false;
}
*pValue = 42;
if ( !xrtThreadKeySet(pKey, pValue) ) {
	xrtFree(pValue);
	xrtThreadKeyDestroy(pKey);
	return false;
}
printf("%d\n", *(int*)xrtThreadKeyGet(pKey));
return xrtThreadKeyDestroy(pKey);
```

完整示例位于 `examples/concurrency/thread_key/main.c`。

## API 参考

以下各节复用自 [once.md](once.md)（两者映射同一 `include/xrt/thread.h`），以线程局部键为主线重组。

## Once 一次初始化

线程模块共享的并发一次初始化原语。

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

线程局部键配套的线程生命周期 API。

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

创建、读取、设置、取走与清空动态键值。

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
