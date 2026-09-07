# Executor

`Executor` 是独立可裁剪的高吞吐 detached 工作执行器。它不创建 `Future`，不传递返回值，
也不提供运行中任务的强制取消；需要结果、错误传播或协作取消时应使用 `TaskPool`。

## 类型与常量

### `xexecutoritem`

批量描述符只在提交调用期间借用，成功后各项数据责任转移给执行器。

```c
typedef struct xexecutoritem {
	xexecutorproc Proc;
	ptr Data;
	xexecutorfreeproc Destroy;
	ptr DestroyContext;
} xexecutoritem;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Proc` | `xexecutorproc` | Proc |
| `Data` | `ptr` | Data |
| `Destroy` | `xexecutorfreeproc` | Destroy |
| `DestroyContext` | `ptr` | DestroyContext |

### `xexecutorconfig`

QueueLimit 是每个 Worker 的硬队列上限；总排队容量等于 Threads 乘 QueueLimit。

```c
typedef struct xexecutorconfig {
	uint32 Threads;
	size_t QueueLimit;
	size_t StackSize;
} xexecutorconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Threads` | `uint32` | Threads |
| `QueueLimit` | `size_t` | QueueLimit |
| `StackSize` | `size_t` | StackSize |

### `xexecutorstats`

统计区分瞬时负载、累计受理、执行、窃取、取消和拒绝数量。

```c
typedef struct xexecutorstats {
	uint32 Threads;
	size_t QueueLimit;
	size_t Queued;
	size_t Running;
	uint64 Submitted;
	uint64 Completed;
	uint64 Executed;
	uint64 Stolen;
	uint64 Cancelled;
	uint64 Rejected;
	bool Closed;
	bool Cancelling;
} xexecutorstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Threads` | `uint32` | Threads |
| `QueueLimit` | `size_t` | QueueLimit |
| `Queued` | `size_t` | Queued |
| `Running` | `size_t` | Running |
| `Submitted` | `uint64` | Submitted |
| `Completed` | `uint64` | Completed |
| `Executed` | `uint64` | Executed |
| `Stolen` | `uint64` | Stolen |
| `Cancelled` | `uint64` | Cancelled |
| `Rejected` | `uint64` | Rejected |
| `Closed` | `bool` | Closed |
| `Cancelling` | `bool` | Cancelling |

### `xexecutor`

Executor 是无 Future 的有界高吞吐执行器，与传播结果和取消的 TaskPool 分工。

```c
typedef struct xexecutor xexecutor;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xexecutorproc`

工作过程不返回结果；错误上下文只在当前工作内有效，返回后由执行器清理。

```c
typedef void (*xexecutorproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xexecutorfreeproc`

工作数据析构在过程返回或排队工作被取消后执行一次。

```c
typedef void (*xexecutorfreeproc)(ptr pData, ptr pContext);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_EXECUTOR_QUEUE_LIMIT_DEFAULT` | `((size_t)1024u)` | QUEUE超限默认值 |
| `XRT_EXECUTOR_THREAD_LIMIT` | `256u` | 线程标识超限 |

## 性能契约

- 创建时按 `Threads * QueueLimit` 预分配全部作业槽。
- 提交、批量提交、窃取和完成不申请通用堆内存。
- 每个 Worker 独立维护双端队列，本地从新端执行，空闲 Worker 从旧端窃取。
- `QueueLimit` 是每个 Worker 的硬上限；满载立即返回 `XERR_AGAIN`。
- 批量提交在一个 Worker 队列中全成或全败，成功后才整体转移数据析构责任。

## 所有权

`xrtExecutorSubmit` 和 `xrtExecutorSubmitBatch` 成功后接管每项 `Destroy`。工作执行完成，或
`xrtExecutorCancel` 丢弃尚未开始的工作时，析构恰好执行一次。提交失败时执行器不会调用析构。

## API

### `xrtExecutorCreate`

创建预分配作业槽、Worker 本地队列和工作窃取线程。

```c
xexecutor* xrtExecutorCreate(const xexecutorconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 空 = 默认配置；`Threads` 与 `QueueLimit` 零值自动取默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 运行中的执行器 | — |
| `NULL` | 配置非法、溢出或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 配置字段非法
- `XERR_RANGE` — `Threads * QueueLimit` 槽位计算溢出
- `XERR_MEMORY` — 作业槽或线程分配失败

#### 范例

[concurrency/executor_tour · 批量](../../examples/concurrency/executor_tour/main.c) · 显式 2 线程 8 槽

```c
xexecutorconfig Config = { 2, 8, 0 };
```

### `xrtExecutorSubmit`

提交一个 detached 工作；成功后接管数据析构，失败时所有权仍属于调用方。

```c
bool xrtExecutorSubmit(
	xexecutor* pExecutor,
	xexecutorproc pProc,
	ptr pData,
	xexecutorfreeproc pDestroy,
	ptr pDestroyContext
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入/输出 | 非空、未关闭 | 目标执行器 |
| `pProc` | 输入 | 非空 | 工作过程 |
| `pData` | 输入 | 任意值 | 过程数据；成功后析构责任移交 |
| `pDestroy` | 输入 | 允许空 | 数据析构过程；空 = 无析构 |
| `pDestroyContext` | 输入 | 任意值 | 原样传给析构过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；执行完成或被取消丢弃时析构恰好一次 | — |
| `false` | 队列满、已关闭或参数非法 | 数据所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 执行器或过程为空
- `XERR_AGAIN` — 目标 Worker 队列达到 `QueueLimit`（满载背压）
- `XERR_CLOSED` — 执行器已 Close/Cancel

#### 范例

[concurrency/executor · 提交](../../examples/concurrency/executor/main.c) · 千次提交后销毁收口

```c
if ( !xrtExecutorSubmit(
	pExecutor,
	executorExampleRun,
	&Count,
	NULL,
	NULL
) ) {
	(void)xrtExecutorCancel(pExecutor);
```

### `xrtExecutorSubmitBatch`

原子提交一组 detached 工作；整组成功或整组失败，不保证执行顺序。

```c
bool xrtExecutorSubmitBatch(
	xexecutor* pExecutor,
	const xexecutoritem* pItems,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入/输出 | 非空、未关闭 | 目标执行器 |
| `pItems` | 输入 | 非空 | 工作数组（每项含 Proc/Data/Destroy/DestroyContext） |
| `iCount` | 输入 | `> 0` | 工作数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整组受理；析构责任整体移交 | — |
| `false` | 容量不足、已关闭或参数非法 | 整组所有权不变（全成或全败） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或计数为零
- `XERR_AGAIN` — 单 Worker 队列容不下整组
- `XERR_CLOSED` — 已停止受理

#### 范例

[concurrency/executor_tour · 批量](../../examples/concurrency/executor_tour/main.c) · 三项原子受理

```c
if ( !xrtExecutorSubmitBatch(pExecutor, Items, 3u) ) {
	goto Cleanup;
}
```

### `xrtExecutorClose`

停止受理新工作，并让已经受理的工作自然排空。

```c
bool xrtExecutorClose(xexecutor* pExecutor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入/输出 | 非空 | 目标执行器；幂等 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已停止受理；已受理工作继续执行 | — |
| `false` | 指针为空 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空

#### 范例

[concurrency/executor_tour · 收口](../../examples/concurrency/executor_tour/main.c) · Close 后 Wait 族收口

```c
if ( !xrtExecutorClose(pExecutor) ||
	(xrtExecutorWaitFor(pExecutor,
		UINT64_C(3000000)) != XWAIT_OK) ||
```

### `xrtExecutorCancel`

停止受理，丢弃尚未开始的工作并执行其数据析构；运行中工作不会被强停。

```c
bool xrtExecutorCancel(xexecutor* pExecutor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入/输出 | 非空 | 目标执行器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已停止受理；排队工作的析构被立即执行 | — |
| `false` | 指针为空 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空

#### 范例

[concurrency/executor · 失败路径](../../examples/concurrency/executor/main.c) · 提交失败即取消销毁

```c
	(void)xrtExecutorCancel(pExecutor);
	(void)xrtExecutorDestroy(pExecutor);
	return 2;
```

### `xrtExecutorWait`

永久等待已经关闭的执行器排空；Executor Worker 不能等待自身。

```c
xwaitresult xrtExecutorWait(xexecutor* pExecutor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入 | 非空、已关闭 | 目标执行器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已排空 | — |
| `XWAIT_ERROR` | 未关闭、参数非法或本执行器 Worker 调用 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 未先 Close，或从本执行器 Worker 线程等待自身

#### 范例

[concurrency/executor_tour · 收口](../../examples/concurrency/executor_tour/main.c) · Close 后三形态依次核对

```c
	(xrtExecutorWait(pExecutor) != XWAIT_OK) ||
	!xrtExecutorGet(pExecutor, &Stats) ||
	!Stats.Closed ||
```

### `xrtExecutorWaitFor`

在相对微秒数内等待已经关闭的执行器排空。

```c
xwaitresult xrtExecutorWaitFor(xexecutor* pExecutor, uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入 | 非空 | 目标执行器 |
| `iTimeout` | 输入 | 微秒 | 相对时限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已排空（排空优先于超时） | — |
| `XWAIT_TIMEOUT` | 到期未排空 | 不设置错误 |
| `XWAIT_ERROR` | 同 `xrtExecutorWait` 的失败条件 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtExecutorWait`（`XERR_ARGUMENT` / `XERR_STATE`）

#### 范例

[concurrency/executor_tour · 收口](../../examples/concurrency/executor_tour/main.c) · 关闭后的限期等待

```c
(xrtExecutorWaitFor(pExecutor,
	UINT64_C(3000000)) != XWAIT_OK) ||
```

### `xrtExecutorWaitUntil`

等待到指定单调时钟截止时间；已排空优先于超时。

```c
xwaitresult xrtExecutorWaitUntil(
	xexecutor* pExecutor,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入 | 非空 | 目标执行器 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_ERROR` | 同 `xrtExecutorWaitFor` 口径 | — |

#### 错误

- 同 `xrtExecutorWait`

#### 范例

[concurrency/executor_tour · 收口](../../examples/concurrency/executor_tour/main.c) · 截止形态

```c
(xrtExecutorWaitUntil(pExecutor,
	xrtDeadlineAfter(UINT64_C(3000000))) !=
	XWAIT_OK) ||
```

### `xrtExecutorGet`

复制当前负载和累计统计快照。

```c
bool xrtExecutorGet(
	const xexecutor* pExecutor,
	xexecutorstats* pStats
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入 | 非空 | 目标执行器 |
| `pStats` | 输出 | 非空 | 接收 Threads/QueueLimit/Queued/Running 与 Submitted/Completed/Executed/Stolen/Cancelled/Rejected |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 并发一致快照已写入 | — |
| `false` | 指针为空 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[concurrency/executor_tour · 统计](../../examples/concurrency/executor_tour/main.c) · 受理/完成/线程数核对

```c
if ( !xrtExecutorGet(pExecutor, &Stats) ||
	(Stats.Submitted < 3u) ||
	(Stats.Completed != 3u) ||
	(Stats.Threads != 2u) ) {
```

### `xrtExecutorDestroy`

关闭、排空、停止 Worker 并释放执行器；Worker 不能销毁自身执行器。

```c
bool xrtExecutorDestroy(xexecutor* pExecutor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExecutor` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已关闭、排空并释放 | — |
| `false` | 参数非法或 Worker 自销毁 | 对象保留；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空（非空操作路径）
- `XERR_STATE` — 从本执行器 Worker 线程销毁自身

#### 范例

[concurrency/executor · 收口](../../examples/concurrency/executor/main.c) · 内部执行 Close+排空+释放

```c
if ( !xrtExecutorDestroy(pExecutor) ) {
	return 3;
}
printf("completed: %u\n", xrtAtomic32Load(&Count, XMEMORY_ACQUIRE));
```

## 示例

```c
xexecutor* pExecutor = xrtExecutorCreate(NULL);

xrtExecutorSubmit(pExecutor, run, pData, destroy, pContext);
xrtExecutorClose(pExecutor);
xrtExecutorWait(pExecutor);
xrtExecutorDestroy(pExecutor);
```

完整示例位于 `examples/concurrency/executor/main.c` 与 `examples/concurrency/executor_tour/main.c`。
