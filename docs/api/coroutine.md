# Coroutine

`XRT_FEATURE_COROUTINE` 提供不依赖调度器的有栈协程核心。对象、平台上下文和栈布局均不公开；启用该功能会同时启用 `thread`、`wait`、`cancel`、`temp_memory`、`mutex` 和 `cond` 依赖。

## 类型与常量

### `xcorostate`

协程状态只描述可恢复性，退出原因由 xcoroterm 单独表达。

```c
typedef enum xcorostate {
	XCORO_READY = 0,
	XCORO_RUNNING = 1,
	XCORO_SUSPENDED = 2,
	XCORO_DONE = 3
} xcorostate;
```

| 值 | 语义 |
|---|---|
| `XCORO_READY` | 就绪 |
| `XCORO_RUNNING` | 运行中 |
| `XCORO_SUSPENDED` | 已挂起 |

### `xcoroterm`

协程终态区分正常返回、协作取消和未处理错误。

```c
typedef enum xcoroterm {
	XCORO_TERM_NONE = 0,
	XCORO_TERM_RETURNED = 1,
	XCORO_TERM_CANCELLED = 2,
	XCORO_TERM_ERROR = 3
} xcoroterm;
```

| 值 | 语义 |
|---|---|
| `XCORO_TERM_NONE` | 无 |
| `XCORO_TERM_RETURNED` | RETURNED |
| `XCORO_TERM_CANCELLED` | 已取消 |

### `xcoroargs`

创建配置只保存会改变核心执行契约的选项。

```c
typedef struct xcoroargs {
	size_t StackSize;
	xcancel* Cancel;
	xcorofinalproc Finalize;
	ptr FinalizeData;
} xcoroargs;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `StackSize` | `size_t` | StackSize |
| `Cancel` | `xcancel*` | Cancel |
| `Finalize` | `xcorofinalproc` | Finalize |
| `FinalizeData` | `ptr` | FinalizeData |

### `xcocleanup`

调用方提供清理节点存储，避免每次压栈产生堆分配。

```c
typedef struct xcocleanup {
	struct xcocleanup* Previous;
	xcoro* Owner;
	xcocleanupproc Proc;
	ptr Data;
	bool Active;
	bool Managed;
} xcocleanup;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Previous` | `struct xcocleanup*` | Previous |
| `Owner` | `xcoro*` | Owner |
| `Proc` | `xcocleanupproc` | Proc |
| `Data` | `ptr` | Data |
| `Active` | `bool` | Active |
| `Managed` | `bool` | Managed |

### `xcoevent`

协程事件允许嵌入调用方结构，不需要为对象本身分配内存。

```c
typedef union xcoevent {
	uint64 Alignment;
	uint8 Storage[XRT_CO_EVENT_STORAGE_SIZE];
} xcoevent;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | Alignment |

### `xcoro`

协程对象对外保持不透明，并且固定归属于创建它的原生线程。

```c
typedef struct xcoro xcoro;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xcosched`

单线程协程调度器对外保持不透明。

```c
typedef struct xcosched xcosched;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xcoroproc`

协程过程返回的指针由调用方定义所有权。

```c
typedef ptr (*xcoroproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xcocleanupproc`

协程退出清理过程在所属协程的执行上下文中运行。

```c
typedef void (*xcocleanupproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xcorofinalproc`

终结过程接收最终终态快照，不能让出、恢复或销毁当前协程。

```c
typedef void (*xcorofinalproc)(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xcoschedpostproc`

调度器投递过程运行在所属线程的普通调用栈中，适合短小的调度操作。

```c
typedef void (*xcoschedpostproc)(xcosched* pSched, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 执行契约

- 协程固定归属于创建它的原生线程，`Resume`、活跃对象的销毁和调度操作只能在该线程执行。
- Windows 使用 `CreateFiberEx` 的可增长栈，仅保留 `StackSize` 指定的虚拟地址空间，并从较小提交量开始按需增长。
- POSIX 原生后端使用匿名映射和不可访问保护页；可写映射由操作系统按首次访问提交物理页面。
- 每个协程拥有独立的临时内存 arena、结构化错误上下文和取消令牌。
- `xrtCoCancelToken` 返回增加引用后的取消令牌，调用方使用完毕后必须调用 `xrtCancelDestroy`。
- 取消是协作式的。`xrtCoYield` 在恢复后返回 `XWAIT_CANCELLED`，未启动的已取消协程不会执行用户过程。
- 取消请求与终态正交：处理请求后正常返回仍是 `XCORO_TERM_RETURNED`；调用 `xrtCoConfirmCancel` 后返回才是 `XCORO_TERM_CANCELLED`。
- 用户过程正常返回时若仍留有未处理错误，终态为 `XCORO_TERM_ERROR`；显式确认取消优先于残留错误。

## 生命周期

`xrtCoCreate` 创建 `XCORO_READY` 对象。`xrtCoResume` 运行到下一次 `xrtCoYield` 或过程返回。只有未启动的 `READY` 对象和 `DONE` 对象可以由 `xrtCoDestroy` 销毁；拒绝销毁活跃栈可避免跳过清理过程或留下悬空上下文。

外部创建的线程在最后一个协程结束后可调用 `xrtCoThreadDetach` 释放惰性线程运行时。`xrtThreadCreate` 创建的线程会在退出时自动执行该步骤；仍有挂起协程时 detach 会失败。

`xrtCoStopping` 用于查询当前协程是否收到取消请求。等待函数返回 `XWAIT_CANCELLED` 后，用户代码可以清理并正常返回结果；确实要把本次执行记为取消时调用 `xrtCoConfirmCancel`。没有取消请求、普通线程路径或清理栈中调用确认函数都会失败并设置 `XERR_STATE`。

### 终结过程

`xcoroargs.Finalize` 是创建时绑定的一次性终结过程。协程进入 `XCORO_DONE` 时，它在清理栈全部执行之后、终态发布之前收到 `xcoroterm`、正常返回值或未处理错误的借用快照。终结过程使用独立错误上下文；它调用 `xrtClearError` 或产生新错误都不会清除、替换协程已经确定的终态错误，也不会把终结过程内部错误泄漏给宿主线程。

首次恢复前已经取消的协程不会进入用户过程，但仍会以 `XCORO_TERM_CANCELLED` 调用终结过程。这一契约用于释放由调度器受理、不能依赖用户过程启动的资源。正常路径中的终结过程运行在协程栈上；首次运行前取消路径运行在所属宿主线程栈上，因此终结过程只能做不可挂起的收尾，不能调用 yield、park、await、resume、清理栈操作或销毁当前协程。这些受限操作会失败并设置 `XERR_STATE`。

销毁一个由 `xrtCoCreate` 直接创建、从未恢复也未进入终态的 `XCORO_READY` 对象不会调用终结过程，创建者仍负责这一放弃路径。调度器成功受理的协程保证执行终结过程；保留句柄在首次运行前被销毁时以 `XCORO_TERM_CANCELLED` 终结，然后才从调度器移除。

## 平台后端

| 平台 | 后端 | 栈策略 |
| --- | --- | --- |
| Windows x86/x64/ARM64 | Fiber | `CreateFiberEx` 保留上限、按需提交 |
| POSIX x86-64 | 原生汇编 | `mmap` 惰性页面和保护页 |
| POSIX ARM64 | AAPCS64 原生汇编 | `mmap` 惰性页面和保护页 |
| POSIX RISC-V 64 | LP64 原生汇编，按浮点 ABI 保存 | `mmap` 惰性页面和保护页 |
| POSIX LoongArch64 | LP64 原生汇编，按浮点 ABI 保存 | `mmap` 惰性页面和保护页 |

所有原生后端保存 ABI 规定的整数和浮点非易失寄存器。当前发布构建必须在对应目标上执行上下文、深栈和浮点寄存器测试；仅在其他平台编译成功不能替代运行验证。

POSIX 手工栈切换在 AddressSanitizer 构建中使用官方 fiber switch 接口登记栈范围和 fake stack，协程永久结束时主动释放其 fake stack；MemorySanitizer 构建使用对应的 fiber switch 接口登记新旧栈边界；ThreadSanitizer 构建为每个协程维护独立 fiber 身份，避免把不同协程的访问历史混为同一执行上下文。Sanitizer 发布门禁必须使用包含对应运行时的插桩工具链实际执行，只有编译或链接结果不能替代运行验证。Linux x86-64 在启用 CET 时同时支持 IBT 和 shadow stack：内部恢复点包含 `ENDBR64`，每个协程按需映射独立 shadow stack 并随协程销毁。其他 x86-64 系统若要求 SHSTK，编译阶段会明确拒绝，不能静默生成不完整上下文。

## 清理栈

清理栈提供两层 API：

- `xrtCoDefer` 是常用路径，由协程分配并管理节点，过程返回后仍可可靠执行。
- `xrtCoCleanupPush` 是无分配路径，节点必须先使用 `XRT_CO_CLEANUP_INIT` 或全零初始化，且存储期必须覆盖协程终态；不能把未弹出的节点放在协程过程的自动局部变量中。

`xrtCoCleanupPop` 可以弹出任一种节点，并可选择立即执行。所有仍在栈上的清理过程按后进先出顺序执行，清理过程中禁止让出协程。

## 调度器

`XRT_FEATURE_COROUTINE_SCHEDULER` 在核心之上增加单线程调度器。调度器、执行队列、timer heap 和 join 链只由所属线程修改；跨线程入口只把工作放入互斥保护的 FIFO 队列，再由所属线程修改执行结构。

- `xrtCoSpawn` 返回保留句柄。协程完成后仍可读取 `Term`、`Result` 和 `Error`，最后由 `xrtCoDestroy` 或调度器销毁。
- `xrtCoGo` 创建分离协程，完成后自动回收，适合不需要结果的后台过程。
- `xrtCoSchedPost` 从任意线程投递借用数据的短过程；`xrtCoSchedPostOwned` 在受理后接管数据，并在过程返回后恰好析构一次。失败时 Owned 数据仍归调用方。
- `xrtCoPark`、`ParkFor`、`ParkUntil` 区分 `OK`、`TIMEOUT` 和 `CANCELLED`；提前 wake 会保留到下一次 park，不会丢失。
- `xrtCoSleep` 和 `SleepUntil` 自然到期返回 `OK`，取消返回 `CANCELLED`。
- `xrtCoJoin` 只接受同一调度器的保留句柄，支持多个等待者、deadline、取消和依赖环检测。
- `xrtCoSchedClose` 停止接收新协程和新投递，并协作取消全部活跃协程；关闭前已经受理的投递仍按 FIFO 顺序排空。

投递过程运行在所属线程的普通调用栈中，不在协程内，不能调用 yield、park 或 await；它应只做创建协程、完成 Promise、更新短状态等不可阻塞操作。每次 `xrtCoSchedStep` 或 `Poll` 至多执行一个通用投递并恢复一个协程。没有活跃协程和待执行投递时返回 `XWAIT_CLOSED`，用户截止时间到达但仍有挂起协程时返回 `XWAIT_TIMEOUT`。

`xrtCoSchedRun` 会排空调用时已经受理的投递和由它们创建的协程。它不是永久驻留的事件循环：当活跃协程和投递同时归零时返回。销毁前必须停止其他线程访问；有尚未执行的投递时，`xrtCoSchedDestroy` 返回 `XERR_STATE`，不会静默丢弃回调或 Owned 数据。

`xrtCoSchedCreate()` 默认限制 1024 个尚未执行的用户投递（`XRT_CO_SCHED_POST_LIMIT_DEFAULT`）。`xrtCoSchedCreateLimit(n)` 可在创建时调整；0 使用默认值，`SIZE_MAX` 显式取消实际限额。队满立即返回 `false / XERR_AGAIN`，不接管 Owned 数据。过程从队列取出时释放名额，因此过程内部也可以继续投递。内部协程唤醒使用独立的无分配链，不受用户队列预算影响；已有批量生产者若一次提交超过 1024 项，需要显式预算或处理背压。

`xrtCoWake` 线程安全，但句柄销毁不是并发操作。调用方必须保证 wake 返回前目标及所属调度器仍然有效；需要跨线程管理寿命时，应由更高层 Future/Task 持有引用，而不是裸传 `xcoro*`。

## 协程事件

`XRT_FEATURE_COROUTINE_EVENT` 提供调度器原生的 `xcoevent`，用于不阻塞所属线程地等待一次跨线程通知。它依赖 `coroutine_scheduler`，但不依赖线程事件的内核等待对象。

- `xrtCoEventInit` / `Unit` 使用调用方固定存储；`Create` / `Destroy` 是对应的堆对象易用层。
- 自动复位事件每次 `Set` 按 FIFO 唤醒一个等待者；没有等待者时保存一个信号。
- 手动复位事件每次 `Set` 唤醒全部等待者并保持信号态，直到 `Reset`。
- `Await`、`TryAwait`、`AwaitFor` 和 `AwaitUntil` 分别返回 `OK`、`TIMEOUT`、`CANCELLED` 或 `ERROR`。
- 每次 Await 使用协程栈上的等待节点，不产生堆分配；跨线程 `Set` 通过调度器等待代际投递，不会丢失注册期间的信号。
- `Unit` / `Destroy` 与其他操作不能并发。只要仍有 Await 尚未返回，即使它已经获得信号，释放也会以 `XERR_STATE` 失败并保持对象有效。

通用 `xrtCoWake` 只能促使 Await 重新检查条件，不能伪造事件信号。终结过程中的 Await 在读取或消费事件信号前失败。

## 公共类型与常量

| 标识符 | 契约 |
| --- | --- |
| `xcoro` | 线程归属且不透明的协程句柄。 |
| `xcoroproc` | 协程入口过程；返回值的所有权由调用方约定。 |
| `xcocleanupproc` | 不可挂起的清理过程。 |
| `xcorostate` | 可恢复状态：`XCORO_READY`、`XCORO_RUNNING`、`XCORO_SUSPENDED`、`XCORO_DONE`。 |
| `xcoroterm` | 终态原因：`XCORO_TERM_NONE`、`XCORO_TERM_RETURNED`、`XCORO_TERM_CANCELLED`、`XCORO_TERM_ERROR`。 |
| `xcorofinalproc` | 一次性终结过程，接收终态、借用结果和借用错误快照。 |
| `xcoroargs` | 栈大小、父取消令牌、终结过程及其数据。未指定字段保持零值。 |
| `xcocleanup` | 调用方存储的无分配清理节点。 |
| `xcosched` | 线程归属且不透明的调度器句柄。 |
| `xcoschedpostproc` | 在调度器所属线程普通调用栈执行的短投递过程。 |
| `xcoevent` | 可嵌入调用方结构的固定存储协程事件。 |
| `XRT_CO_CLEANUP_INIT` | `xcocleanup` 的静态零值初始化器。 |
| `XRT_CORO_STACK_DEFAULT` | 默认栈保留大小：64 位目标 128 KiB，32 位目标 64 KiB。 |
| `XRT_CORO_STACK_MIN` | 最小栈保留大小 32 KiB。 |
| `XRT_CORO_STACK_MAX` | 最大栈保留大小 64 MiB。 |
| `XRT_CO_EVENT_STORAGE_SIZE` | 当前平台的 `xcoevent` 内部存储容量；不能作为跨平台 ABI 尺寸。 |
| `XRT_CO_SCHED_POST_LIMIT_DEFAULT` | 默认外部投递队列上限：1024 项，不限制内部协程唤醒队列。 |

## 协程核心

### `xrtCoCreate`

创建 `XCORO_READY` 状态的协程对象；未恢复前不分配运行栈之外的状态。

```c
xcoro* xrtCoCreate(xcoroproc pProc, ptr pData, const xcoroargs* pArgs);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProc` | 输入 | 非空 | 协程入口过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pArgs` | 输入 | 允许空 | 栈大小/父取消令牌/终结过程；空 = 全默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | `READY` 协程，归属当前线程 | — |
| `NULL` | 参数非法、栈大小越界或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 过程为空或 `pArgs` 字段非法
- `XERR_RANGE` — 栈大小超出 32 KiB–64 MiB 边界
- `XERR_MEMORY` — 对象或栈分配失败

#### 范例

[concurrency/coroutine · 基础](../../examples/concurrency/coroutine/main.c) · 创建-恢复-取结果-销毁

```c
xcoro* pCo = xrtCoCreate(exampleCoroutine, &iValue, NULL);

if ( pCo == NULL ) {
	return 1;
}
(void)xrtCoResume(pCo);
```

### `xrtCoDestroy`

销毁未启动或已完成对象；活跃对象失败且保持有效。

```c
bool xrtCoDestroy(xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 允许空 | 目标协程；只接受 `READY` 或 `DONE` 状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 对象已释放 | — |
| `false` | 对象活跃或非本线程 | 对象保持有效；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `XERR_STATE` — 栈仍活跃（拒绝跳过清理）或从其他线程销毁

#### 范例

[concurrency/coroutine · 收尾](../../examples/concurrency/coroutine/main.c) · 完成后销毁并 detach

```c
(void)xrtCoDestroy(pCo);
(void)xrtCoThreadDetach();
return 0;
```

### `xrtCoResume`

在所属线程恢复协程，直到让出或完成。

```c
bool xrtCoResume(xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空 | 目标协程；须处于 `READY`/`SUSPENDED` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 本次恢复段已执行到让出或返回 | — |
| `false` | 状态不可恢复或非本线程 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `XERR_STATE` — `RUNNING`/`DONE` 状态或从其他线程恢复

#### 范例

[concurrency/coroutine · 基础](../../examples/concurrency/coroutine/main.c) · 两次恢复走完 yield 前后两段

```c
(void)xrtCoResume(pCo);
printf("after yield: %d\n", iValue);
(void)xrtCoResume(pCo);
printf("result: %d\n", *(int*)xrtCoResult(pCo));
```

### `xrtCoYield`

让出当前协程，并在恢复后返回取消状态。

```c
xwaitresult xrtCoYield(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 仅协程内可调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 正常恢复 | — |
| `XWAIT_CANCELLED` | 恢复时收到取消请求 | 协作取消的检查点 |
| `XWAIT_ERROR` | 不在协程内 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 普通调用栈（非协程）中调用

#### 范例

[concurrency/coroutine · 基础](../../examples/concurrency/coroutine/main.c) · yield 即取消检查点

```c
if ( xrtCoYield() != XWAIT_OK ) {
	return NULL;
}
```

### `xrtCoCurrent`

返回当前协程的借用句柄；普通调用栈返回空指针。

```c
xcoro* xrtCoCurrent(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 当前协程借用（回调期间有效） |
| `NULL` | 普通线程调用栈（正常结果） |

#### 错误

- 无 — 空返回是上下文查询结果

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 协程内自省起点

```c
xcoro* pSelf = xrtCoCurrent();
```

### `xrtCoState`

返回可恢复状态快照。

```c
xcorostate xrtCoState(const xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 允许空 | 目标协程 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XCORO_READY` / `XCORO_RUNNING` / `XCORO_SUSPENDED` / `XCORO_DONE` | 状态枚举；空句柄返回 `READY`（零值） |

#### 错误

- 无 — 快照查询

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 运行中自检 + 终态核对

```c
pJob->iRunning = xrtCoState(pSelf) == XCORO_RUNNING ? 1 : 0;
```

### `xrtCoTerm`

仅在状态已发布为 `DONE` 后返回终态，否则返回 `NONE`。

```c
xcoroterm xrtCoTerm(const xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 允许空 | 目标协程 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XCORO_TERM_NONE` | 尚未终结 |
| `XCORO_TERM_RETURNED` / `XCORO_TERM_CANCELLED` / `XCORO_TERM_ERROR` | 终态原因 |

#### 错误

- 无 — 终态查询

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 完成后核对 RETURNED

```c
(xrtCoState(pLife) != XCORO_DONE) ||
(xrtCoTerm(pLife) != XCORO_TERM_RETURNED) ||
```

### `xrtCoResult`

返回正常终态的借用结果。

```c
ptr xrtCoResult(const xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 允许空 | 目标协程 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 过程返回值（所有权由调用方约定） |
| `NULL` | 未完成、取消终态或过程返回了空 |

#### 错误

- 无 — 借用查询

#### 范例

[concurrency/coroutine · 取结果](../../examples/concurrency/coroutine/main.c) · 完成后读取返回值

```c
printf("result: %d\n", *(int*)xrtCoResult(pCo));
```

### `xrtCoError`

返回错误终态的借用结构化错误。

```c
const xerror* xrtCoError(const xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 允许空 | 目标协程 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | `XCORO_TERM_ERROR` 终态的错误借用 |
| `NULL` | 非错误终态或未完成 |

#### 错误

- 无 — 借用查询

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 正常终态应为空错误

```c
(xrtCoError(pLife) != NULL) ||
(xrtCoResult(pLife) != (ptr)1) ) {
```

### `xrtCoCancel`

线程安全且幂等地请求协作取消，并通知可选调度器。

```c
bool xrtCoCancel(xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空 | 目标协程；可从任意线程调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 请求已受理（重复请求同样成功） | — |
| `false` | 句柄为空或已终结 | 已终结不设错；空句柄设 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 句柄为空

#### 范例

[concurrency/coroutine_lifecycle · 协作取消](../../examples/concurrency/coroutine_lifecycle/main.c) · 工作协程挂起后请求取消

```c
return xrtCoCancel(pContext->Worker) ? pContext : NULL;
```

### `xrtCoCancelToken`

返回增加引用后的取消令牌。

```c
xcancel* xrtCoCancelToken(const xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空、当前协程 | 只能在协程内取自己的令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 引用 +1 的令牌；调用方 `xrtCancelDestroy` 释放 | — |
| `NULL` | 非协程上下文或句柄为空 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `XERR_STATE` — 普通调用栈中调用

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 协程内取令牌传给可中断等待

```c
pJob->pToken = xrtCoCancelToken(pSelf);
```

### `xrtCoStopping`

查询当前协程是否收到取消请求。

```c
bool xrtCoStopping(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 仅协程内有意义 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 已收到取消请求（尚未确认） |
| `false` | 无请求或普通线程路径（正常结果） |

#### 错误

- 无 — 查询语义

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 自省字段之一

```c
pJob->iStopping = xrtCoStopping() ? 1 : 0;
```

### `xrtCoConfirmCancel`

确认用户过程返回时发布取消终态。

```c
bool xrtCoConfirmCancel(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 仅协程内、收到请求后可调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 终态将发布为 `XCORO_TERM_CANCELLED` | — |
| `false` | 无取消请求、普通线程或清理栈中 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 无请求或非法上下文

#### 范例

[concurrency/coroutine_lifecycle · 协作取消](../../examples/concurrency/coroutine_lifecycle/main.c) · park 被取消后确认终态

```c
Result = xrtCoPark();
if ( Result == XWAIT_CANCELLED ) {
	(void)xrtCoConfirmCancel();
}
```

### `xrtCoThreadDetach`

释放当前外部线程的惰性协程运行时。

```c
bool xrtCoThreadDetach(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 外部创建的线程在最后一个协程结束后调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 运行时已释放 | — |
| `false` | 仍有挂起协程 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 仍有协程未终结

#### 范例

[concurrency/coroutine · 收尾](../../examples/concurrency/coroutine/main.c) · 销毁全部协程后 detach

```c
(void)xrtCoDestroy(pCo);
(void)xrtCoThreadDetach();
return 0;
```

### `xrtCoCleanupPush`

压入零初始化、调用方存储的无分配清理节点。

```c
bool xrtCoCleanupPush(
	xcocleanup* pCleanup,
	xcocleanupproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCleanup` | 输入/输出 | `XRT_CO_CLEANUP_INIT` 或全零 | 存储期须覆盖协程终态 |
| `pProc` | 输入 | 非空 | 不可挂起的清理过程 |
| `pData` | 输入 | 任意值 | 原样传给清理过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已按后进先出压栈 | — |
| `false` | 参数非法或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 普通调用栈调用

#### 范例

[concurrency/coroutine_lifecycle · 清理栈](../../examples/concurrency/coroutine_lifecycle/main.c) · 调用方存储节点

```c
if ( !xrtCoCleanupPush(
	&pContext->ManualCleanup,
	exampleCleanup,
	&pContext->ManualCleaned
) ) {
```

### `xrtCoDefer`

分配并压入由协程管理存储期的清理节点。

```c
xcocleanup* xrtCoDefer(xcocleanupproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProc` | 输入 | 非空 | 清理过程 |
| `pData` | 输入 | 任意值 | 原样传给清理过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 节点句柄，可用于提前弹出 | — |
| `NULL` | 参数非法、非协程上下文或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` / `XERR_MEMORY`

#### 范例

[concurrency/coroutine_lifecycle · 清理栈](../../examples/concurrency/coroutine_lifecycle/main.c) · 托管节点

```c
pDeferred = xrtCoDefer(exampleCleanup, &pContext->DeferredCleaned);
if ( pDeferred == NULL ) {
	return NULL;
}
```

### `xrtCoCleanupPop`

弹出栈顶节点，并可立即执行清理过程。

```c
bool xrtCoCleanupPop(xcocleanup* pCleanup, bool bRun);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCleanup` | 输入 | 非空、栈顶节点 | `Defer` 句柄或 `CleanupPush` 节点 |
| `bRun` | 输入 | — | 为真时立即执行清理过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已弹出（并按需执行） | — |
| `false` | 节点不在栈顶或非协程上下文 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 节点非栈顶

#### 范例

[concurrency/coroutine_lifecycle · 清理栈](../../examples/concurrency/coroutine_lifecycle/main.c) · 失败路径弹出托管节点

```c
(void)xrtCoCleanupPop(pDeferred, true);
return NULL;
```

### `xrtCoBackend`

返回当前目标的静态后端名称。

```c
cstr xrtCoBackend(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 静态字符串（如 `"fiber"`、`"asm"`），进程存活期有效 |

#### 错误

- 无 — 静态查询

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 自省字段之一

```c
pJob->sBackend = xrtCoBackend();
```

## 调度器

### `xrtCoSchedCreate`

在当前线程创建调度器；默认投递上限 1024。

```c
xcosched* xrtCoSchedCreate(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 归属当前线程的调度器 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 调度器结构分配失败

#### 范例

[concurrency/coroutine_event · 事件](../../examples/concurrency/coroutine_event/main.c) · 创建后 spawn 消费者

```c
pSched = xrtCoSchedCreate();
if ( pSched == NULL ) {
	(void)xrtCoEventUnit(&tEvent);
	return 1;
}
```

### `xrtCoSchedCreateLimit`

创建时指定用户投递上限；0 使用默认 1024。

```c
xcosched* xrtCoSchedCreateLimit(size_t iPostLimit);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iPostLimit` | 输入 | — | 尚未执行的用户投递上限；0 = 默认，`SIZE_MAX` = 不限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 调度器 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 显式 16 项上限

```c
pStep = xrtCoSchedCreateLimit(16u);
```

### `xrtCoSchedDestroy`

销毁空闲调度器及其保留的完成句柄。

```c
bool xrtCoSchedDestroy(xcosched* pSched);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 允许空 | 须在所属线程且无未执行投递 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 调度器与保留句柄已释放 | — |
| `false` | 仍有未执行投递或活跃协程句柄 | 不静默丢弃；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 有未执行投递/保留句柄未销毁，或非所属线程

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 排空后销毁

```c
!xrtCoSchedDestroy(pStep) ) {
	goto Cleanup;
}
```

### `xrtCoSchedCurrent`

返回当前协程所属的借用调度器。

```c
xcosched* xrtCoSchedCurrent(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 当前协程的调度器借用 |
| `NULL` | 普通调用栈（正常结果） |

#### 错误

- 无 — 上下文查询

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 协程内反查调度器

```c
pJob->pSched = xrtCoSchedCurrent();
```

### `xrtCoSchedPost`

从任意线程 FIFO 投递借用数据过程。

```c
bool xrtCoSchedPost(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空 | 目标调度器 |
| `pProc` | 输入 | 非空 | 所属线程普通栈执行的短过程 |
| `pData` | 输入 | 借用 | 原样传给过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已入队 | — |
| `false` | 队满、已关闭或参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_AGAIN` — 投递队列达到上限（背压）
- `XERR_STATE` — 调度器已 Close

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 投递后单步执行

```c
!xrtCoSchedPost(pStep, examplePostProc, NULL) ||
(xrtCoSchedAlive(pStep) != 0u) ||
(xrtCoSchedStep(pStep) != XWAIT_OK) ||
```

### `xrtCoSchedPostOwned`

从任意线程投递过程并接管数据；失败不接管，受理后在过程返回后恰好析构一次。

```c
bool xrtCoSchedPostOwned(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData,
	xcocleanupproc pDestroy
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空 | 目标调度器 |
| `pProc` | 输入 | 非空 | 短过程 |
| `pData` | 输入 | 成功受理后移交 | 数据所有权 |
| `pDestroy` | 输入 | 非空 | 析构过程，恰好执行一次 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；析构由调度器保证 | — |
| `false` | 队满、已关闭或参数非法 | 数据仍归调用方 |

#### 错误

- `XERR_ARGUMENT` / `XERR_AGAIN` / `XERR_STATE` — 同 `xrtCoSchedPost`

#### 范例

[concurrency/coroutine_tour · PostOwned](../../examples/concurrency/coroutine_tour/main.c) · 析构恰好一次核对

```c
if ( !xrtCoSchedPostOwned(pSched, examplePostProc, (ptr)7,
		examplePostDestroy) ||
	!xrtCoSchedRun(pSched) ||
	(g_Destroyed != 1) ) {
```

### `xrtCoSpawn`

创建由调度器管理且在完成后保留句柄的协程。

```c
xcoro* xrtCoSpawn(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空、未关闭 | 所属调度器 |
| `pProc` | 输入 | 非空 | 协程过程 |
| `pData` | 输入 | 任意值 | 过程数据 |
| `pArgs` | 输入 | 允许空 | 栈/令牌/终结配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 保留句柄；完成后可读终态，最后 `Destroy` | — |
| `NULL` | 调度器关闭、参数非法或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE`（已关闭）/ `XERR_MEMORY`

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · spawn 后 Run 到完成

```c
pLife = xrtCoSpawn(pSched, exampleCoLife, &Life, NULL);
if ( (pLife == NULL) || !xrtCoSchedRun(pSched) ) {
	goto Cleanup;
}
```

### `xrtCoGo`

创建完成后由调度器自动回收的分离协程。

```c
bool xrtCoGo(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空、未关闭 | 所属调度器 |
| `pProc` | 输入 | 非空 | 协程过程 |
| `pData` | 输入 | 任意值 | 过程数据 |
| `pArgs` | 输入 | 允许空 | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；完成后自动回收，无句柄 | — |
| `false` | 同 `xrtCoSpawn` 失败条件 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoSpawn`

#### 范例

[concurrency/coroutine_lifecycle · 三协程同场](../../examples/concurrency/coroutine_lifecycle/main.c) · 工作协程 + 两个 Go 辅助

```c
xrtCoGo(pSched, exampleCancel, &Context, NULL) &&
xrtCoGo(pSched, exampleJoin, &Context, NULL) ) {
```

### `xrtCoSchedClose`

请求取消全部活跃协程并停止接收新协程和新投递。

```c
bool xrtCoSchedClose(xcosched* pSched);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空 | 目标调度器；幂等 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已停止受理；已受理投递仍按 FIFO 排空 | — |
| `false` | 指针非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空

#### 范例

[concurrency/coroutine_lifecycle · 失败路径](../../examples/concurrency/coroutine_lifecycle/main.c) · spawn 失败时关闭排空

```c
(void)xrtCoSchedClose(pSched);
(void)xrtCoSchedRun(pSched);
```

### `xrtCoSchedStep`

非阻塞地执行至多一个投递和一个就绪协程。

```c
xwaitresult xrtCoSchedStep(xcosched* pSched);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空、所属线程 | 目标调度器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 执行了工作 | — |
| `XWAIT_CLOSED` | 无活跃协程与待执行投递 | 正常排空结果 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 非所属线程

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 投递后单步执行

```c
(xrtCoSchedStep(pStep) != XWAIT_OK) ||
```

### `xrtCoSchedPollFor`

在相对微秒期限内等待，并执行至多一个调度步。

```c
xwaitresult xrtCoSchedPollFor(xcosched* pSched, uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空、所属线程 | 目标调度器 |
| `iTimeout` | 输入 | 微秒 | 等待期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 执行了工作 | — |
| `XWAIT_TIMEOUT` | 到期但仍有挂起协程 | 正常结果 |
| `XWAIT_CLOSED` | 全部排空 | 正常结果 |
| `XWAIT_ERROR` | 参数/状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoSchedStep`

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 期限轮询

```c
(xrtCoSchedPollFor(pStep, EXAMPLE_LONG_US) !=
	XWAIT_OK) ||
```

### `xrtCoSchedPollUntil`

在绝对截止时间前等待，并执行至多一个调度步。

```c
xwaitresult xrtCoSchedPollUntil(xcosched* pSched, xdeadline iDeadline);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空、所属线程 | 目标调度器 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CLOSED` | 同 `xrtCoSchedPollFor` | — |
| `XWAIT_ERROR` | 参数/状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoSchedStep`

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 截止轮询

```c
(xrtCoSchedPollUntil(pStep,
	xrtDeadlineAfter(EXAMPLE_LONG_US)) !=
	XWAIT_OK) ||
```

### `xrtCoSchedRun`

运行到活跃协程和已受理投递全部排空。

```c
bool xrtCoSchedRun(xcosched* pSched);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 非空、所属线程 | 目标调度器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已排空（不驻留的事件循环） | — |
| `false` | 参数/状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 非所属线程

#### 范例

[concurrency/coroutine_event · 事件](../../examples/concurrency/coroutine_event/main.c) · 消费者 + 生产者一次排空

```c
bOkay =
	(pConsumer != NULL) &&
	xrtCoGo(pSched, producer, &tEvent, NULL) &&
	xrtCoSchedRun(pSched) &&
	(xrtCoResult(pConsumer) == &tEvent) &&
```

### `xrtCoSchedAlive`

在所属线程返回尚未完成的协程数量。

```c
size_t xrtCoSchedAlive(const xcosched* pSched);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 允许空 | 目标调度器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `>= 0` | 活跃协程计数（含分离协程） |
| `0` | 无活跃协程或句柄为空 |

#### 错误

- 无 — 计数查询

#### 范例

[concurrency/coroutine_tour · 单步模式](../../examples/concurrency/coroutine_tour/main.c) · 空转核对为零

```c
(xrtCoSchedAlive(pStep) != 0u) ||
```

### `xrtCoWake`

线程安全且幂等地唤醒仍然有效的调度协程。

```c
bool xrtCoWake(xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空 | 目标协程；句柄销毁不是并发操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 唤醒已投递（提前 wake 保留到下一次 park） | — |
| `false` | 句柄为空或非调度协程 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `XERR_STATE` — 目标不由调度器管理

#### 范例

[concurrency/coroutine_tour · Join/Park](../../examples/concurrency/coroutine_tour/main.c) · 唤醒挂起的 parker

```c
(void)xrtCoWake(pParker);
```

### `xrtCoPark`

挂起当前协程直到 wake 或取消。

```c
xwaitresult xrtCoPark(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 仅调度协程内可调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 被 wake 唤醒 | — |
| `XWAIT_CANCELLED` | 收到取消请求 | 协作检查点 |
| `XWAIT_ERROR` | 非调度协程上下文 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 普通协程或普通调用栈

#### 范例

[concurrency/coroutine_lifecycle · 协作取消](../../examples/concurrency/coroutine_lifecycle/main.c) · park 被取消打断

```c
Result = xrtCoPark();
if ( Result == XWAIT_CANCELLED ) {
	(void)xrtCoConfirmCancel();
}
```

### `xrtCoParkFor`

挂起到相对微秒期限、wake 或取消。

```c
xwaitresult xrtCoParkFor(uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CANCELLED` | wake/到期/取消 | — |
| `XWAIT_ERROR` | 非调度协程 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 非调度协程上下文

#### 范例

[concurrency/coroutine_tour · Join/Park](../../examples/concurrency/coroutine_tour/main.c) · 被唤醒返回 OK

```c
pJob->iForResult = (int)xrtCoParkFor(EXAMPLE_LONG_US);
```

### `xrtCoParkUntil`

挂起到绝对截止时间、wake 或取消。

```c
xwaitresult xrtCoParkUntil(xdeadline iDeadline);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CANCELLED` | 同 `xrtCoParkFor` | — |
| `XWAIT_ERROR` | 非调度协程 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 非调度协程上下文

#### 范例

[concurrency/coroutine_tour · Join/Park](../../examples/concurrency/coroutine_tour/main.c) · 过期返回 TIMEOUT

```c
pJob->iUntilResult = (int)xrtCoParkUntil(
```

### `xrtCoSleep`

睡眠相对微秒数；自然到期或提前 wake 返回 `OK`。

```c
xwaitresult xrtCoSleep(uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTimeout` | 输入 | 微秒 | 睡眠时长 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 到期或被 wake 提前结束 | — |
| `XWAIT_CANCELLED` | 取消请求打断 | — |
| `XWAIT_ERROR` | 非调度协程 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 非调度协程上下文

#### 范例

[concurrency/coroutine_event · 生产者](../../examples/concurrency/coroutine_event/main.c) · 短睡后置位事件

```c
if ( xrtCoSleep(1000) != XWAIT_OK ) {
```

### `xrtCoSleepUntil`

睡眠到绝对截止时间。

```c
xwaitresult xrtCoSleepUntil(xdeadline iDeadline);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_CANCELLED` | 到期/取消 | — |
| `XWAIT_ERROR` | 非调度协程 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 非调度协程上下文

#### 范例

[concurrency/coroutine_tour · 生命周期](../../examples/concurrency/coroutine_tour/main.c) · 协程内短睡

```c
(void)xrtCoSleepUntil(xrtDeadlineAfter(EXAMPLE_SHORT_US));
```

### `xrtCoJoin`

等待同一调度器的保留句柄完成。

```c
xwaitresult xrtCoJoin(xcoro* pCo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空 | 保留句柄；须与等待者同调度器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 目标已完成 | — |
| `XWAIT_CANCELLED` | 等待者自身被取消 | — |
| `XWAIT_ERROR` | 参数非法或跨调度器 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 句柄为空或分离协程
- `XERR_STATE` — 跨调度器 join 或检测到依赖环

#### 范例

[concurrency/coroutine_lifecycle · Join](../../examples/concurrency/coroutine_lifecycle/main.c) · 同调度器等待工作协程

```c
pContext->JoinResult = xrtCoJoin(pContext->Worker);
```

### `xrtCoJoinFor`

在相对微秒期限内等待目标完成。

```c
xwaitresult xrtCoJoinFor(xcoro* pCo, uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空 | 保留句柄 |
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CANCELLED` | 完成/到期/取消 | — |
| `XWAIT_ERROR` | 同 `xrtCoJoin` 错误条件 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoJoin`

#### 范例

[concurrency/coroutine_tour · Join/Park](../../examples/concurrency/coroutine_tour/main.c) · 短窗超时

```c
pJob->iForResult = (int)xrtCoJoinFor(pJob->pTarget,
```

### `xrtCoJoinUntil`

在绝对截止时间前等待目标完成。

```c
xwaitresult xrtCoJoinUntil(xcoro* pCo, xdeadline iDeadline);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCo` | 输入 | 非空 | 保留句柄 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CANCELLED` | 完成/到期/取消 | — |
| `XWAIT_ERROR` | 同 `xrtCoJoin` | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoJoin`

#### 范例

[concurrency/coroutine_tour · Join/Park](../../examples/concurrency/coroutine_tour/main.c) · 截止等待完成

```c
pJob->iUntilResult = (int)xrtCoJoinUntil(pJob->pTarget,
```

## 协程事件

### `xrtCoEventInit`

初始化调用方存储的自动或手动复位事件。

```c
bool xrtCoEventInit(
	xcoevent* pEvent,
	bool bManualReset,
	bool bSignaled
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输出 | 非空 | 固定存储（可嵌入调用方结构） |
| `bManualReset` | 输入 | — | 手动复位唤醒全部；自动复位按 FIFO 唤醒一个 |
| `bSignaled` | 输入 | — | 初始信号态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 事件已就绪 | — |
| `false` | 指针非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空

#### 范例

[concurrency/coroutine_event · 事件](../../examples/concurrency/coroutine_event/main.c) · 自动复位、无初始信号

```c
if ( !xrtCoEventInit(&tEvent, false, false) ) {
	return 1;
}
```

### `xrtCoEventUnit`

释放协程事件；仍有尚未返回的等待者时失败并保持对象有效。

```c
bool xrtCoEventUnit(xcoevent* pEvent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | `Init` 产物；与其他操作不能并发 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 固定存储已释放 | — |
| `false` | 仍有 Await 未返回 | 对象保持有效；`XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 仍有等待者（即使已获信号）

#### 范例

[concurrency/coroutine_event · 收尾](../../examples/concurrency/coroutine_event/main.c) · 排空后释放

```c
(void)xrtCoSchedDestroy(pSched);
(void)xrtCoEventUnit(&tEvent);
(void)xrtCoThreadDetach();
```

### `xrtCoEventCreate`

创建自动或手动复位协程事件（堆存储易用层）。

```c
xcoevent* xrtCoEventCreate(
	bool bManualReset,
	bool bSignaled
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `bManualReset` | 输入 | — | 复位模式 |
| `bSignaled` | 输入 | — | 初始信号态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆事件；`EventDestroy` 释放 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[concurrency/coroutine_tour · 事件族](../../examples/concurrency/coroutine_tour/main.c) · 自动复位事件

```c
pAuto = xrtCoEventCreate(false, false);
if ( (pAuto == NULL) || !xrtCoEventSet(pAuto) ) {
	goto Cleanup;  /* 预置位供 TryAwait 立即消费 */
}
```

### `xrtCoEventDestroy`

释放 `Create` 返回的协程事件；仍有等待者时失败且不释放对象。

```c
bool xrtCoEventDestroy(xcoevent* pEvent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 允许空 | `Create` 产物 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已释放 | — |
| `false` | 仍有等待者 | 对象不释放；`XERR_STATE` |

#### 错误

- `XERR_STATE` — 仍有 Await 未返回

#### 范例

[concurrency/coroutine_tour · 事件族](../../examples/concurrency/coroutine_tour/main.c) · 全部唤醒核对后销毁

```c
if ( (pWaiter == NULL) || (pEvtDriver == NULL) ||
	!xrtCoSchedRun(pSched) ||
	(Evt.iWoken != 3) ||
	!xrtCoEventDestroy(pAuto) ) {
```

### `xrtCoEventSet`

置位事件；手动复位唤醒全部等待者，自动复位按 FIFO 唤醒一个。

```c
bool xrtCoEventSet(xcoevent* pEvent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件；线程安全 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已置位并投递唤醒 | — |
| `false` | 指针非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空

#### 范例

[concurrency/coroutine_event · 生产者](../../examples/concurrency/coroutine_event/main.c) · 短睡后置位事件

```c
return xrtCoEventSet(pEvent) ? pEvent : NULL;
```

### `xrtCoEventReset`

清除事件的信号态；已经获得信号的等待者不受影响。

```c
bool xrtCoEventReset(xcoevent* pEvent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 后续等待不再看到信号 | — |
| `false` | 指针非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空

#### 范例

[concurrency/coroutine_tour · 事件族](../../examples/concurrency/coroutine_tour/main.c) · 双 Set 后清信号覆盖

```c
(void)xrtCoEventReset(pJob->pAuto);  /* 清信号（覆盖点） */
```

### `xrtCoEventAwait`

挂起当前调度协程，直到事件置位或协程取消。

```c
xwaitresult xrtCoEventAwait(xcoevent* pEvent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 取得信号（自动复位信号被消费） | — |
| `XWAIT_CANCELLED` | 协程被取消 | — |
| `XWAIT_ERROR` | 非调度协程或终结过程内 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 非调度协程，或终结过程中调用

#### 范例

[concurrency/coroutine_event · 消费者](../../examples/concurrency/coroutine_event/main.c) · 等待生产者置位

```c
if ( xrtCoEventAwait(pEvent) != XWAIT_OK ) {
```

### `xrtCoEventTryAwait`

非阻塞地检查并消费自动复位事件。

```c
xwaitresult xrtCoEventTryAwait(xcoevent* pEvent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 有信号并已消费（自动复位） | — |
| `XWAIT_TIMEOUT` | 无信号（正常结果） | 不设错 |
| `XWAIT_CANCELLED` | 已取消 | — |
| `XWAIT_ERROR` | 参数/状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoEventAwait`

#### 范例

[concurrency/coroutine_tour · 事件族](../../examples/concurrency/coroutine_tour/main.c) · 预置位立即消费

```c
if ( xrtCoEventTryAwait(pJob->pAuto) == XWAIT_OK ) {
```

### `xrtCoEventAwaitFor`

在相对微秒数内等待事件置位。

```c
xwaitresult xrtCoEventAwaitFor(
	xcoevent* pEvent,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CANCELLED` | 信号/到期/取消 | — |
| `XWAIT_ERROR` | 参数/状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoEventAwait`

#### 范例

[concurrency/coroutine_tour · 事件族](../../examples/concurrency/coroutine_tour/main.c) · 期限等待由 Set 唤醒

```c
if ( xrtCoEventAwaitFor(pJob->pAuto, EXAMPLE_LONG_US) ==
```

### `xrtCoEventAwaitUntil`

等待事件置位、协程取消或到达截止时间。

```c
xwaitresult xrtCoEventAwaitUntil(
	xcoevent* pEvent,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEvent` | 输入 | 非空 | 目标事件 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_CANCELLED` | 信号/到期/取消 | — |
| `XWAIT_ERROR` | 参数/状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtCoEventAwait`

#### 范例

[concurrency/coroutine_tour · 事件族](../../examples/concurrency/coroutine_tour/main.c) · 截止等待由 Set 唤醒

```c
if ( xrtCoEventAwaitUntil(pJob->pAuto,
```

## 示例

```c
static ptr work(ptr data)
{
	int* value = (int*)data;

	(*value)++;
	if ( xrtCoYield() != XWAIT_OK ) {
		return NULL;
	}
	return value;
}

int value = 1;
xcoro* co = xrtCoCreate(work, &value, NULL);

xrtCoResume(co);
xrtCoResume(co);
xrtCoDestroy(co);
xrtCoThreadDetach();
```

协程事件适合表达一次或广播式就绪通知：

```c
static ptr wait_ready(ptr data)
{
	xcoevent* ready = (xcoevent*)data;

	return xrtCoEventAwait(ready) == XWAIT_OK ? ready : NULL;
}

xcoevent ready;
xcosched* sched;
xcoro* waiter;

xrtCoEventInit(&ready, false, false);
sched = xrtCoSchedCreate();
waiter = xrtCoSpawn(sched, wait_ready, &ready, NULL);
xrtCoSchedStep(sched);
xrtCoEventSet(&ready);
xrtCoSchedRun(sched);
xrtCoDestroy(waiter);
xrtCoSchedDestroy(sched);
xrtCoEventUnit(&ready);
xrtCoThreadDetach();
```

从任意线程投递创建工作时，使用调度器 post，而不是直接跨线程调用 `xrtCoGo`：

```c
static void post_task(xcosched* sched, ptr data)
{
	(void)xrtCoGo(sched, task, data, NULL);
}

xrtCoSchedPost(sched, post_task, data);
xrtCoSchedRun(sched);
```

调度器常用路径：

```c
static ptr task(ptr data)
{
	if ( xrtCoSleep(1000) != XWAIT_OK ) {
		return NULL;
	}
	return data;
}

xcosched* sched = xrtCoSchedCreate();
xcoro* co = xrtCoSpawn(sched, task, data, NULL);

xrtCoSchedRun(sched);
use_result(xrtCoResult(co));
xrtCoDestroy(co);
xrtCoSchedDestroy(sched);
xrtCoThreadDetach();
```

完整生命周期示例位于 `examples/concurrency/coroutine_lifecycle/main.c`，同时展示协作取消、同调度器 Join、调用方存储清理节点、托管清理节点，以及清理完成后收到的终态快照。
