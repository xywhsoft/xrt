# 任务与任务池

任务层把阻塞工作、Future 结果、结构化错误、取消和资源所有权组合成独立于网络的并发契约。`xfuture` 就是任务句柄，不再额外维护一套任务状态对象。

## 类型与常量

### `xtaskoutcome`

任务过程必须显式说明成功、失败或协作取消，避免依赖残留错误状态。

```c
typedef enum xtaskoutcome {
	XTASK_SUCCESS = 0,
	XTASK_FAILED = 1,
	XTASK_CANCELLED = 2
} xtaskoutcome;
```

| 值 | 语义 |
|---|---|
| `XTASK_SUCCESS` | SUCCESS |
| `XTASK_FAILED` | 已失败 |

### `xtaskvalue`

成功结果可以借用值，也可以把值及其析构过程转移给 Future。

```c
typedef struct xtaskvalue {
	ptr Value;
	xfuturefreeproc Destroy;
	ptr DestroyData;
} xtaskvalue;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Value` | `ptr` | 值 |
| `Destroy` | `xfuturefreeproc` | Destroy |
| `DestroyData` | `ptr` | DestroyData |

### `xtaskargs`

提交参数控制父取消关系及任务数据在受理后的释放方式。

```c
typedef struct xtaskargs {
	xcancel* Cancel;
	xfuturefreeproc Destroy;
	ptr DestroyData;
} xtaskargs;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Cancel` | `xcancel*` | 取消令牌 |
| `Destroy` | `xfuturefreeproc` | Destroy |
| `DestroyData` | `ptr` | DestroyData |

### `xtaskgroupconfig`

全零配置表示不限活动项数量、不自动取消兄弟项且使用独立取消源。

```c
typedef struct xtaskgroupconfig {
	xcancel* Cancel;
	size_t Limit;
	uint32 CancelOn;
} xtaskgroupconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Cancel` | `xcancel*` | 取消令牌 |
| `Limit` | `size_t` | 上限 |
| `CancelOn` | `uint32` | CancelOn |

### `xtaskgroupstats`

任务组统计保留全部历史终态计数，但只为当前活动项占用节点内存。

```c
typedef struct xtaskgroupstats {
	size_t Active;
	uint64 Added;
	uint64 Completed;
	uint64 Succeeded;
	uint64 Failed;
	uint64 Cancelled;
	uint64 Closed;
	uint64 Rejected;
	size_t FirstIndex;
	xfuturestate FirstState;
	bool Accepting;
	bool Cancelling;
} xtaskgroupstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Active` | `size_t` | Active |
| `Added` | `uint64` | Added |
| `Completed` | `uint64` | Completed |
| `Succeeded` | `uint64` | Succeeded |
| `Failed` | `uint64` | Failed |
| `Cancelled` | `uint64` | Cancelled |
| `Closed` | `uint64` | Closed |
| `Rejected` | `uint64` | Rejected |
| `FirstIndex` | `size_t` | FirstIndex |
| `FirstState` | `xfuturestate` | FirstState |
| `Accepting` | `bool` | Accepting |
| `Cancelling` | `bool` | Cancelling |

### `xtaskpoolconfig`

全零配置使用逻辑处理器数量、默认队列上限和平台默认线程栈。

```c
typedef struct xtaskpoolconfig {
	uint32 Threads;
	size_t QueueLimit;
	size_t StackSize;
} xtaskpoolconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Threads` | `uint32` | 线程数 |
| `QueueLimit` | `size_t` | QueueLimit |
| `StackSize` | `size_t` | 栈大小 |

### `xtaskpoolstats`

统计快照区分瞬时负载、终态分布、拒绝量和生命周期状态。

```c
typedef struct xtaskpoolstats {
	uint32 Threads;
	size_t QueueLimit;
	size_t Queued;
	size_t Running;
	uint64 Submitted;
	uint64 Completed;
	uint64 Succeeded;
	uint64 Failed;
	uint64 Cancelled;
	uint64 Rejected;
	bool Closed;
	bool Cancelling;
} xtaskpoolstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Threads` | `uint32` | 线程数 |
| `QueueLimit` | `size_t` | QueueLimit |
| `Queued` | `size_t` | Queued |
| `Running` | `size_t` | Running |
| `Submitted` | `uint64` | Submitted |
| `Completed` | `uint64` | Completed |
| `Succeeded` | `uint64` | Succeeded |
| `Failed` | `uint64` | Failed |
| `Cancelled` | `uint64` | Cancelled |
| `Rejected` | `uint64` | Rejected |
| `Closed` | `bool` | Closed |
| `Cancelling` | `bool` | Cancelling |

### `xtaskgroup`

任务组跟踪一组 Future，并在关闭且全部完成后发布唯一 Done Future。

```c
typedef struct xtaskgroup xtaskgroup;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtaskpool`

任务池对外保持不透明；销毁期间调用方必须停止其他并发访问。

```c
typedef struct xtaskpool xtaskpool;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtaskproc`

任务过程借用取消令牌，并把成功值写入预先清零的结果结构。

```c
typedef xtaskoutcome (*xtaskproc)(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtaskgroupstartproc`

Future 启动器同步返回一个新引用，返回空时保留自己的结构化错误。

```c
typedef xfuture* (*xtaskgroupstartproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtasknetproc`

网络任务过程额外借用亲和 Worker，并复用任务核心的取消和结果合同。

```c
typedef xtaskoutcome (*xtasknetproc)(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 裁剪

- `XRT_FEATURE_TASK`：任务过程、显式结果和内部执行生命周期，依赖 Future 与临时内存。
- `XRT_FEATURE_TASK_GROUP`：结构化 Future 作用域、嵌套组与结果统计，只依赖 Future。
- `XRT_FEATURE_TASK_POOL`：有界原生线程池，依赖任务核心与线程。
- `XRT_FEATURE_TASK_GROUP_POOL`：任务组与有界任务池的原子提交便利层，只在两者同时启用时引入。
- `XRT_FEATURE_TASK_COROUTINE`：协程调度任务，依赖任务核心与协程调度器。
- `XRT_FEATURE_TASK_GROUP_COROUTINE`：任务组与协程任务的原子提交便利层。
- `XRT_FEATURE_TASK_NET`：Engine 亲和任务与延迟任务，依赖任务核心和网络 Engine。
- `XRT_FEATURE_TASK_GROUP_NET`：任务组与网络任务的原子提交便利层。

只使用 Future/Promise 时不需要引入任务池；网络引擎、协程调度器和用户执行器也可以复用任务核心，而不依赖线程池。

## 任务结果

`xtaskproc` 接收借用的 `xcancel`、用户数据和预先清零的 `xtaskvalue`，并显式返回 `xtaskoutcome`：

- `XTASK_SUCCESS`：Future 进入 `XFUTURE_RESOLVED`。
- `XTASK_FAILED`：Future 进入 `XFUTURE_FAILED`，保存当前结构化错误；没有错误时运行库使用 `XERR_INTERNAL`。
- `XTASK_CANCELLED`：Future 进入 `XFUTURE_CANCELLED`。

成功值默认是借用值。设置 `xtaskvalue.Destroy` 后，值和析构过程转移给 Future，并在最后一个 Future 引用释放时销毁。失败、取消或完成冲突时，尚未转移的 owned 值由任务运行库销毁。

任务过程返回值是最终结果。取消请求只表示协作意图：过程观察并处理取消后仍可明确返回成功；只有返回 `XTASK_CANCELLED` 或执行前已经取消时，Future 才进入取消终态。

## 执行上下文

每个任务绑定独立的错误上下文和临时 arena：

- 前一个任务遗留的错误不会污染后一个任务。
- `xrtTempCurrent` 和 `xrtTemp` 只在当前任务生命周期内有效。
- 任务返回后，临时内存整体释放，不能作为 Future 结果返回。
- 任务数据析构也在该隔离上下文内执行。

## 数据所有权

`xtaskargs.Destroy` 描述任务数据在受理后的析构方式：

- 任一种提交函数成功后，执行器取得数据所有权；执行、跳过或取消后恰好析构一次。
- 提交因 OOM、队列已满、等待超时、等待取消或任务池关闭而失败时，调用方仍持有数据，析构过程不会执行。
- 数据析构发生在任务过程返回后、Future 终态发布前，因此观察到终态时任务上下文已经完成回收。结果若借用任务数据，不能同时配置数据析构；需要长期保存时应把结果所有权转移给 Future。

`xtaskargs.Cancel` 是可空父令牌。任务使用自己的子令牌，因此任务取消不会反向取消父作用域，而父取消会传播到任务。

## 结构化任务组

`xtaskgroup` 可以跟踪任务 Future、网络 Future 或用户 Promise，不要求源来自某一种执行器。它只为当前活动项保留一个监听节点和源引用，项进入终态后立即摘除并释放，因此内存占用与并发量相关，不随历史任务总数增长。

```c
xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
xrtTaskGroupAdd(pGroup, pFuture);
xrtTaskGroupWait(pGroup);
```

已有 Future 使用 `xrtTaskGroupAdd`。需要先启动异步操作再返回 Future 时，使用 `xrtTaskGroupStart` 和同步启动器 `xtaskgroupstartproc`：运行库先预留活动槽位，预留成功后才调用启动器，随后把返回的 Future 提交到同一槽位。关闭、活动上限、预留 OOM 或启动失败都不会留下已经启动却未登记的操作；关闭和取消发生在启动器执行期间时，组 Done Future 会等待预留完成，取消还会在 Future 提交后补发。

```c
xfuture* pFuture = xrtTaskGroupStart(pGroup, startRequest, pRequest);
```

生命周期语义保持单一口径：

- `xrtTaskGroupClose` 停止接纳新项，让当前项自然结束；重复关闭返回 `false`。
- `xrtTaskGroupCancel` 同时关闭组，并向当前项发出协作取消请求；源生产端仍决定各自最终状态。
- `xrtTaskGroupFuture` 返回稳定的 Done Future。只有组已关闭且活动项归零时，它才成功完成；子项失败不会被冒充为 Done Future 自身失败。
- `xrtTaskGroupWait`、`xrtTaskGroupWaitFor`、`xrtTaskGroupWaitUntil` 和 `xrtTaskGroupWaitUntilCancel` 会先关闭组，再复用 Future 的统一等待语义。
- `xrtTaskGroupCancelToken` 返回组取消令牌的新增引用，调用方负责释放。
- `xrtTaskGroupDestroy` 会关闭并取消仍活动的项，但不伪造完成；活动监听持有内部引用，允许源稍后安全确认终态并完成延迟回收。

`xtaskgroupconfig.Limit` 是活动项硬上限，达到上限时 Add 返回 `XERR_AGAIN`；完成项立即释放槽位。`CancelOn` 可以组合 `XRT_TASK_GROUP_CANCEL_ON_FAILED`、`XRT_TASK_GROUP_CANCEL_ON_CANCELLED` 和 `XRT_TASK_GROUP_CANCEL_ON_CLOSED`，在指定异常出现后关闭组并取消仍活动的兄弟项。`XRT_TASK_GROUP_CANCEL_ON_STOPPED` 表示三者合集。默认值为零，即收集全部结果而不自动取消。

`xrtTaskGroupChild` 创建由父组跟踪的嵌套作用域。父 `Close` 只关闭子组并等待叶任务自然结束；父 `Cancel` 才向子组和叶任务传播取消。这样正常作用域退出与异常停止不会混为同一行为。

`xrtTaskGroupGet` 把活动数、受理数、完成数、成功/失败/取消/关闭数、拒绝数和首个非成功槽位写入 `xtaskgroupstats`。`xrtTaskGroupError` 借用首个失败项的结构化错误。组排空后满足：

```text
Completed = Succeeded + Failed + Cancelled + Closed
Added = Completed
```

### `xrtTaskGroupCreate`

创建结构化任务组；配置为空时使用全零默认值。

```c
xtaskgroup* xrtTaskGroupCreate(const xtaskgroupconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 组配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务组（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 创建任务组

```c
	pGroup = xrtTaskGroupCreate(&GroupConfig);
```

### `xrtTaskGroupChild`

创建由父组跟踪的子组；父关闭时关闭子组，父取消时取消子组。

```c
xtaskgroup* xrtTaskGroupChild(xtaskgroup* pParent, const xtaskgroupconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pParent` | 输入 | 非空 | 父组 |
| `pConfig` | 输入 | 允许空 | 组配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 子组 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_group_scope](../../examples/concurrency/task_group_scope/main.c) · 创建子组

```c
	pChild = xrtTaskGroupChild(pParent, NULL);
```

### `xrtTaskGroupAdd`

跟踪一个 Future；成功时组保留到该 Future 终态为止的引用。

```c
bool xrtTaskGroupAdd(xtaskgroup* pGroup, xfuture* pFuture)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pFuture` | 输入 | 非空 | 要跟踪的 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已纳入跟踪 | — |
| `false` | 失败 | `XERR_ARGUMENT` / `XERR_CLOSED` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_STATE` — 组已进入完成路径

#### 范例

[task_group_scope](../../examples/concurrency/task_group_scope/main.c) · 跟踪 Future

```c
		!xrtTaskGroupAdd(pChild, pLeaf) ) {
```

### `xrtTaskGroupStart`

先预留组槽位，再同步启动并跟踪 Future；失败时不会留下未登记操作。

```c
xfuture* xrtTaskGroupStart(xtaskgroup* pGroup, xtaskgroupstartproc pProc, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pProc` | 输入 | 非空 | 启动过程 |
| `pData` | 输入 | 任意值 | 过程数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 同步启动

```c
			xfuture* pStarted = xrtTaskGroupStart(pGroup,
				exampleStarter, pPool2);
```

### `xrtTaskGroupFuture`

返回增加引用后的 Done Future；它在组关闭且活动项归零时成功完成。

```c
xfuture* xrtTaskGroupFuture(const xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Done Future 新引用，用后释放 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[task_group_scope](../../examples/concurrency/task_group_scope/main.c) · Done Future

```c
	pDone = xrtTaskGroupFuture(pParent);
```

### `xrtTaskGroupClose`

停止接纳新项，并让当前项及子组自然结束。

```c
bool xrtTaskGroupClose(xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 组已销毁

#### 范例

[task](../../examples/network/task/main.c) · 关闭

```c
			!xrtTaskGroupClose(pGroup) ||
```

### `xrtTaskGroupCancel`

停止接纳新项，并向当前项及子组发出协作取消请求。

```c
bool xrtTaskGroupCancel(xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已发出 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 组已销毁

#### 范例

[task_group_scope](../../examples/concurrency/task_group_scope/main.c) · 取消

```c
	if ( !xrtTaskGroupCancel(pParent) ||
		!xrtCancelRequested(pLeafCancel) ||
		!xrtPromiseCancel(pLeafPromise) ||
		(xrtFutureWait(pDone) != XWAIT_OK) ) {
```

### `xrtTaskGroupCancelToken`

返回增加引用后的组取消令牌。

```c
xcancel* xrtTaskGroupCancelToken(const xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 取消令牌新引用，用后 `xrtCancelDestroy` | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 组取消令牌

```c
		if ( (xrtTaskGroupCancelToken(pGroup) == NULL) ) {
```

### `xrtTaskGroupGet`

复制当前负载、累计结果、首个异常槽位和生命周期状态。

```c
bool xrtTaskGroupGet(const xtaskgroup* pGroup, xtaskgroupstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[task_group_scope](../../examples/concurrency/task_group_scope/main.c) · 组统计

```c
		!xrtTaskGroupGet(pParent, &tStats) ) {
```

### `xrtTaskGroupError`

返回首个失败项的借用结构化错误；任务组存活期间保持有效。

```c
const xerror* xrtTaskGroupError(const xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误借用 | — |
| `NULL` | 尚无失败项 | 不设错误 |

#### 错误

- 尚无失败项返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 首个异常

```c
		pError = xrtTaskGroupError(pGroup);
```

### `xrtTaskGroupWait`

关闭任务组并等待全部当前项进入终态。

```c
xwaitresult xrtTaskGroupWait(xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 组已销毁

#### 范例

[task_group_pool](../../examples/concurrency/task_group_pool/main.c) · 关闭并等待

```c
	if ( xrtTaskGroupWait(pGroup) != XWAIT_OK ) {
```

### `xrtTaskGroupWaitFor`

关闭任务组并在相对微秒数内等待全部当前项。

```c
xwaitresult xrtTaskGroupWaitFor(xtaskgroup* pGroup, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 组已销毁

#### 范例

[task](../../examples/network/task/main.c) · 关闭并限时等待

```c
			(xrtTaskGroupWaitFor(pGroup, 3000000u) !=
				XWAIT_OK) ) {
```

### `xrtTaskGroupWaitUntil`

关闭任务组并等待到指定单调时钟截止时间。

```c
xwaitresult xrtTaskGroupWaitUntil(xtaskgroup* pGroup, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 组已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 关闭并限期等待

```c
		if ( (xrtTaskGroupWaitUntil(pGroup,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
				XWAIT_OK) ||
			(xrtTaskGroupWaitUntilCancel(pGroup,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel) != XWAIT_OK) ) {
```

### `xrtTaskGroupWaitUntilCancel`

关闭任务组，并等待组完成、截止时间或调用方取消中的首个事件。

```c
xwaitresult xrtTaskGroupWaitUntilCancel(xtaskgroup* pGroup, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |
| `pCancel` | 输入 | 允许空 | 调用方取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 组已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 关闭并可取消等待

```c
			(xrtTaskGroupWaitUntilCancel(pGroup,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel) != XWAIT_OK) ) {
```

### `xrtTaskGroupDestroy`

关闭并取消仍活动的项，随后以延迟回收方式释放任务组。

```c
void xrtTaskGroupDestroy(xtaskgroup* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 销毁不失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 销毁

```c
	xrtTaskGroupDestroy(pGroup);
```

## 有界任务池

`xrtTaskPoolCreate` 接受可空配置。全零字段分别表示逻辑处理器数量、`XRT_TASK_POOL_QUEUE_LIMIT_DEFAULT` 指定的默认队列上限 `1024` 和平台默认线程栈。线程数最大值由 `XRT_TASK_POOL_THREAD_LIMIT` 指定，当前为 `256`。

队列上限只统计尚未开始运行的任务。达到上限时提交会先回收已经请求取消的排队任务；仍无可用槽位时，调用方可以选择立即返回或等待背压解除：

- `xrtTaskSubmit`：立即尝试，队列已满时返回空并设置 `XERR_AGAIN`。
- `xrtTaskSubmitWait`：一直等待槽位。
- `xrtTaskSubmitFor`：在相对微秒数内等待槽位。
- `xrtTaskSubmitUntil`：等待到指定单调时钟截止时间。
- `xrtTaskSubmitUntilCancel`：同时等待槽位、截止时间或调用方取消。

槽位已经可用时，受理成功优先于同时到达的超时或等待取消。等待调用尚未受理任务时，超时设置 `XERR_TIMEOUT`，调用方取消设置 `XERR_CANCELLED`，任务池关闭设置 `XERR_CLOSED`。关闭和整体取消都会唤醒全部容量等待者。

`xrtTaskSubmitUntilCancel` 的取消令牌只约束当前容量等待，不会成为任务的父令牌，也不会取消已经受理的任务。任务执行期取消必须通过 `xtaskargs.Cancel` 或 `xrtFutureCancel` 表达。任务池工作线程可以在槽位立即可用时提交到所属池，但队列已满时不得等待自身释放槽位，运行库返回 `XERR_STATE`，从契约上阻止自锁。

生命周期分为：

- 打开：接收任务。
- `xrtTaskPoolClose`：停止接收普通任务，排空已受理任务；工作线程保持休眠，继续服务运行库已经接管资源的 finalizer。
- `xrtTaskPoolCancel`：停止接收普通任务，取消排队任务，并向运行任务发出协作取消请求；资源回收过程不会被取消。
- `xrtTaskPoolDestroy`：等待普通任务和资源回收过程全部结束，再终止工作线程并释放任务池。

`xrtTaskPoolWait`、`xrtTaskPoolWaitFor`、`xrtTaskPoolWaitUntil` 和 `xrtTaskPoolWaitUntilCancel` 只接受已经关闭的池。`xrtTaskPoolWaitUntilCancel` 的令牌只中止调用方等待，不改变池或池内任务状态；池已经排空时完成优先于同时到达的超时或取消。任务池自己的工作线程不能等待或销毁所属池，否则返回 `XERR_STATE`。`xrtTaskPoolDestroy` 默认关闭并自然排空；需要快速停止时先调用 `xrtTaskPoolCancel`。

内部资源回收通道只接收已经由运行库受理的资源，节点嵌入资源对象，不发生投递分配，也不占用 `QueueLimit`。它不是第二条用户任务队列，公共代码不能用它绕过背压。

销毁期间，调用方必须保证没有其他线程继续调用该任务池，并且所有绑定该池的异步资源都已经进入关闭流程。Future 生命周期独立于任务池，池销毁后，调用方已经持有的 Future 仍然有效。

### `xrtTaskPoolCreate`

创建有界工作线程池；配置为空或字段为零时使用对应默认值。

```c
xtaskpool* xrtTaskPoolCreate(const xtaskpoolconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 池配置，空/零字段 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务池 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量或线程数字段越界
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 创建任务池

```c
	pPool = xrtTaskPoolCreate(&PoolConfig);
```

### `xrtTaskSubmit`

提交任务并返回其 Future；失败时任务数据所有权仍属于调用方。

```c
xfuture* xrtTaskSubmit(xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列已满
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 提交

```c
	return xrtTaskSubmit((xtaskpool*)pData, exampleTask, NULL,
		NULL);
```

### `xrtTaskSubmitWait`

等待任务池出现队列槽位后提交；任务池工作线程不得阻塞等待所属池。

```c
xfuture* xrtTaskSubmitWait(xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_STATE` — 在所属池工作线程内阻塞等待
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 等待槽位提交

```c
		((arrFutures[0] = xrtTaskSubmitWait(pPool, exampleTask,
			NULL, NULL)) == NULL) ||
```

### `xrtTaskSubmitFor`

在相对微秒数内等待任务池出现队列槽位并提交。

```c
xfuture* xrtTaskSubmitFor(xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TIMEOUT` — 槽位等待超时
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 限时槽位提交

```c
		((arrFutures[1] = xrtTaskSubmitFor(pPool, exampleTask,
			NULL, NULL, EXAMPLE_TIMEOUT_US)) == NULL) ||
```

### `xrtTaskSubmitUntil`

等待到指定单调时钟截止时间；槽位已经可用时成功优先于超时。

```c
xfuture* xrtTaskSubmitUntil(xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TIMEOUT` — 槽位等待超时
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 限期槽位提交

```c
		((arrFutures[2] = xrtTaskSubmitUntil(pPool, exampleTask,
			NULL, NULL,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US))) == NULL) ||
```

### `xrtTaskSubmitUntilCancel`

等待槽位、截止时间或调用方取消；等待取消不取消已经受理的任务。

```c
xfuture* xrtTaskSubmitUntilCancel(xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |
| `pCancel` | 输入 | 允许空 | 调用方取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TIMEOUT` — 槽位等待超时
- `XERR_CANCELLED` — 等待被取消，任务未受理
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 可取消槽位提交

```c
		((arrFutures[4] = xrtTaskSubmitUntilCancel(pPool,
			exampleTask, NULL, NULL,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
			pCancel)) == NULL) ) {
```

### `xrtTaskPoolClose`

停止接收普通任务，并让已经受理的任务与内部资源回收过程自然排空。

```c
bool xrtTaskPoolClose(xtaskpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 池已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 关闭

```c
	if ( !xrtTaskPoolClose(pPool) ||
		(xrtTaskPoolWaitFor(pPool, EXAMPLE_TIMEOUT_US) !=
			XWAIT_OK) ||
		!xrtTaskPoolGet(pPool, &Stats) ||
		(Stats.Completed < 5u) ||
		(Stats.Succeeded < 5u) ||
		!Stats.Closed ) {
```

### `xrtTaskPoolCancel`

停止接收新任务，取消排队任务并请求运行任务协作取消。

```c
bool xrtTaskPoolCancel(xtaskpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已发出 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 池已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 取消

```c
				(xrtTaskPoolCancel(pFullPool) ) ) {
```

### `xrtTaskPoolWait`

等待已关闭任务池中的全部受理任务进入终态。

```c
xwaitresult xrtTaskPoolWait(xtaskpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 池未关闭或已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 等待排空

```c
		(void)xrtTaskPoolWait(pFullPool);
```

### `xrtTaskPoolWaitFor`

在相对微秒数内等待已关闭任务池排空。

```c
xwaitresult xrtTaskPoolWaitFor(xtaskpool* pPool, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 池未关闭或已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 限时等待排空

```c
		(xrtTaskPoolWaitFor(pPool, EXAMPLE_TIMEOUT_US) !=
			XWAIT_OK) ||
```

### `xrtTaskPoolWaitUntil`

等待已关闭任务池排空到指定单调时钟截止时间。

```c
xwaitresult xrtTaskPoolWaitUntil(xtaskpool* pPool, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 池未关闭或已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 限期等待排空

```c
		if ( (xrtTaskPoolWaitUntil(pFullPool,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
				XWAIT_OK) ||
			!xrtTaskPoolGet(pFullPool, &Stats) ) {
```

### `xrtTaskPoolWaitUntilCancel`

等待池排空、截止时间或调用方取消中的首个事件。

```c
xwaitresult xrtTaskPoolWaitUntilCancel(xtaskpool* pPool, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |
| `pCancel` | 输入 | 允许空 | 调用方取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 全部项进入终态 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_CANCELLED` | 调用方取消触发 | 不设错误 |
| `XWAIT_ERROR` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 池未关闭或已销毁

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 可取消等待排空

```c
			xwaitresult iWait = xrtTaskPoolWaitUntilCancel(
				pFullPool,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel);
```

### `xrtTaskPoolGet`

复制任务池统计快照。

```c
bool xrtTaskPoolGet(const xtaskpool* pPool, xtaskpoolstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 池统计

```c
		!xrtTaskPoolGet(pPool, &Stats) ||
```

### `xrtTaskPoolDestroy`

关闭、排空、终止工作线程并释放任务池；工作线程不能销毁自身所属的池。

```c
bool xrtTaskPoolDestroy(xtaskpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标任务池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已销毁 | — |
| `false` | 工作线程自销毁或状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 在所属池工作线程内调用或池状态非法

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 销毁

```c
		xrtTaskPoolDestroy(pPool2);
```

## 任务组与任务池

`XRT_FEATURE_TASK_GROUP_POOL` 提供原子组合入口，避免先调用任务池、再调用 `xrtTaskGroupAdd` 时在两步之间关闭任务组：

- `xrtTaskGroupSubmit`：立即尝试任务池提交。
- `xrtTaskGroupSubmitWait`：永久等待任务池槽位。
- `xrtTaskGroupSubmitFor`：在相对微秒数内等待。
- `xrtTaskGroupSubmitUntil`：等待到单调时钟截止时间。
- `xrtTaskGroupSubmitUntilCancel`：再叠加调用方取消。

可等待版本会自动把组取消合并到容量等待。组在任务尚未受理时取消，提交返回 `XERR_CANCELLED`、组预留回滚且数据仍归调用方；任务已经受理时，Future 正常纳入组并收到协作取消请求。正常 `Close` 不取消容量等待，因为 Close 的含义是停止新增预留并自然等待已经预留的操作。

### `xrtTaskGroupSubmit`

立即向任务池提交并原子纳入组；队列已满时完整回滚组预留。

```c
xfuture* xrtTaskGroupSubmit(xtaskgroup* pGroup, xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数（名称、取消令牌等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列已满，组预留完整回滚
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[report](../../examples/concurrency/report/main.c) · 组提交

```c
		arrFuture[i] = xrtTaskGroupSubmit(
			pGroup,
			pContext->Pool,
			reportPartRun,
			&pContext->Parts[i],
			NULL
		);
```

### `xrtTaskGroupSubmitWait`

等待任务池槽位后提交；组取消会中止尚未受理的容量等待。

```c
xfuture* xrtTaskGroupSubmitWait(xtaskgroup* pGroup, xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数（名称、取消令牌等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_STATE` — 在所属池工作线程内阻塞等待
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 组等待槽位提交

```c
			((arrFutures[5] = xrtTaskGroupSubmitWait(pGroup,
				pPool2, exampleTask, NULL, NULL)) ==
				NULL) ||
```

### `xrtTaskGroupSubmitFor`

在相对微秒数内等待任务池槽位并原子纳入组。

```c
xfuture* xrtTaskGroupSubmitFor(xtaskgroup* pGroup, xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数（名称、取消令牌等） |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TIMEOUT` — 槽位等待超时
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_group_pool](../../examples/concurrency/task_group_pool/main.c) · 组限时槽位提交

```c
		arrFuture[i] = xrtTaskGroupSubmitFor(
			pGroup,
			pPool,
			groupedWork,
			&arrValue[i],
			NULL,
			UINT64_C(2000000)
		);
```

### `xrtTaskGroupSubmitUntil`

等待任务池槽位到指定单调时钟截止时间并原子纳入组。

```c
xfuture* xrtTaskGroupSubmitUntil(xtaskgroup* pGroup, xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数（名称、取消令牌等） |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TIMEOUT` — 槽位等待超时
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 组限期槽位提交

```c
			((arrFutures[6] = xrtTaskGroupSubmitUntil(pGroup,
				pPool2, exampleTask, NULL, NULL,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US))) ==
				NULL) ) {
```

### `xrtTaskGroupSubmitUntilCancel`

同时受截止时间、调用方取消和任务组取消约束地等待提交。

```c
xfuture* xrtTaskGroupSubmitUntilCancel(xtaskgroup* pGroup, xtaskpool* pPool, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pPool` | 输入 | 非空 | 目标任务池 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数（名称、取消令牌等） |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |
| `pCancel` | 输入 | 允许空 | 调用方取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_TIMEOUT` — 槽位等待超时
- `XERR_CANCELLED` — 等待被取消
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_tour](../../examples/concurrency/task_tour/main.c) · 组可取消槽位提交

```c
			xfuture* pThird = xrtTaskGroupSubmitUntilCancel(
				pGroup, pPool2, exampleTask, NULL, NULL,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel);
```

## 协程任务

`xrtTaskCo` 把同一个 `xtaskproc` 提交到指定协程调度器；调度器为空时使用当前协程所属调度器。显式指定调度器时可以从任意线程提交，内部先进入调度器 FIFO post 队列，再由所属线程创建协程。调用发生在普通执行路径且没有显式调度器时返回 `XERR_STATE`。

协程任务使用分离协程，用户只持有 Future。Future 的取消令牌是协程取消令牌的父级，因此取消 Future 会唤醒正在 park、sleep 或 await 的任务协程。首次调度前取消不会进入任务过程，但协程终结过程仍会把 Future 完成为取消并释放受理后的任务数据。

任务过程的显式结果优先于取消请求：过程从取消等待恢复后仍可返回 `XTASK_SUCCESS`，此时 Future 成功；返回 `XTASK_CANCELLED` 时 Future 才是取消终态。协程栈大小为零时使用协程默认值。

提交成功表示调度器已经受理任务数据和 Future 生命周期，不表示协程已经创建。调度器在处理该投递前关闭时，Future 以 `XERR_CLOSED` 失败并析构受理的数据；关闭后直接提交返回空、设置 `XERR_CLOSED`，数据所有权仍归调用方。

`XRT_FEATURE_TASK_GROUP_COROUTINE` 提供 `xrtTaskGroupCo`，在任务组预留窗口中调用 `xrtTaskCo`。组已经关闭时不会触碰调度器或任务数据；提交与组取消竞争时，成功受理的 Future 会先登记，再收到取消请求。

### `xrtTaskCo`

从任意线程向指定或当前协程调度器提交任务，并返回独立生命周期的 Future。

```c
xfuture* xrtTaskCo(struct xcosched* pSched, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, size_t iStackSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSched` | 输入 | 允许空 | 协程调度器，空 = 当前 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iStackSize` | 输入 | 0 = 默认 | 协程栈字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_coroutine](../../examples/concurrency/task_coroutine/main.c) · 协程任务

```c
	pFuture = xrtTaskCo(pSched, delayedValue, &iValue, NULL, 0);
```

### `xrtTaskGroupCo`

向协程调度器提交任务，并在同一预留窗口内原子纳入任务组。

```c
xfuture* xrtTaskGroupCo(xtaskgroup* pGroup, struct xcosched* pSched, xtaskproc pProc, ptr pData, const xtaskargs* pArgs, size_t iStackSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pSched` | 输入 | 允许空 | 协程调度器，空 = 当前 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iStackSize` | 输入 | 0 = 默认 | 协程栈字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_group_coroutine](../../examples/concurrency/task_group_coroutine/main.c) · 组协程任务

```c
	pFuture = xrtTaskGroupCo(
		pGroup,
		pSched,
		groupedCoroutine,
		&iValue,
		NULL,
		0
	);
```

## 网络 Engine 任务

`XRT_FEATURE_TASK_NET` 是任务体系到网络 Engine 的可选桥，不会让 Future、Task 或
TaskGroup 核心反向依赖网络。`xtasknetproc` 在普通任务参数之前额外借用亲和
`xnetworker`，因此过程可以直接访问 Worker 缓冲池和 Engine 上下文；它仍使用相同的
`xcancel`、`xtaskvalue`、结构化错误、临时 arena 和数据所有权合同。

- `xrtTaskNet`：尽快在指定亲和 Worker 上执行。
- `xrtTaskNetAfter`：在相对微秒数到期后执行。
- `xrtTaskNetUntil`：在单调时钟截止时间到期后执行。

网络任务运行在事件循环线程，不得执行阻塞系统调用或长时间 CPU 工作；这些工作应
提交到 `xtaskpool`。立即任务一旦受理，即使 Engine 随后停止也会在排空阶段执行。
延迟任务在 Engine 到期时执行；Future 取消会请求摘除 Timer，确认后以
`XFUTURE_CANCELLED` 终结。Engine 在截止时间前停止时，任务不进入用户过程，并以
`XERR_CLOSED` 的结构化错误失败。底层 Timer 完成、取消监听注销和任务数据析构全部
先于 Future 终态发布，因此消费者不会观察到仍被 Worker 使用的任务上下文。

`XRT_FEATURE_TASK_GROUP_NET` 提供 `xrtTaskGroupNet`、
`xrtTaskGroupNetAfter` 和 `xrtTaskGroupNetUntil`。这些函数先预留 TaskGroup 活动槽位，
再提交 Engine 操作；组已关闭、达到活动上限或预留 OOM 时不会启动网络任务，也不会
接管用户数据。组取消会传播到任务 Future，并等待 Timer 的真实取消回调完成。

### `xrtTaskNet`

向指定亲和 Worker 提交任务；任务过程不得阻塞网络事件循环。

```c
xfuture* xrtTaskNet(xnetengine* pEngine, uint64 iAffinity, xtasknetproc pProc, ptr pData, const xtaskargs* pArgs)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task](../../examples/network/task/main.c) · 网络任务

```c
	pFuture = xrtTaskNet(
		pEngine,
		0,
		buildValue,
		&iValue,
		NULL
	);
```

### `xrtTaskNetAfter`

在相对微秒数到期后向指定亲和 Worker 提交任务。

```c
xfuture* xrtTaskNetAfter(xnetengine* pEngine, uint64 iAffinity, xtasknetproc pProc, ptr pData, const xtaskargs* pArgs, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task](../../examples/network/task/main.c) · 延迟网络任务

```c
	pFuture = xrtTaskNetAfter(pEngine, 0, buildValue, &iValue,
		NULL, 0u);
```

### `xrtTaskNetUntil`

在指定单调时钟截止时间到期后向亲和 Worker 提交任务。

```c
xfuture* xrtTaskNetUntil(xnetengine* pEngine, uint64 iAffinity, xtasknetproc pProc, ptr pData, const xtaskargs* pArgs, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task](../../examples/network/task/main.c) · 限期网络任务

```c
	pFuture = xrtTaskNetUntil(pEngine, 0, buildValue, &iValue,
		NULL, xrtDeadlineAfter(0u));
```

### `xrtTaskGroupNet`

向亲和 Worker 提交任务，并在同一预留窗口内原子纳入任务组。

```c
xfuture* xrtTaskGroupNet(xtaskgroup* pGroup, xnetengine* pEngine, uint64 iAffinity, xtasknetproc pProc, ptr pData, const xtaskargs* pArgs)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_group](../../examples/network/task_group/main.c) · 组网络任务

```c
		pFirst = xrtTaskGroupNet(
			pGroup,
			pEngine,
			0,
			buildGroupValue,
			&iFirst,
			NULL
		);
```

### `xrtTaskGroupNetAfter`

延迟提交网络任务，并在同一预留窗口内原子纳入任务组。

```c
xfuture* xrtTaskGroupNetAfter(xtaskgroup* pGroup, xnetengine* pEngine, uint64 iAffinity, xtasknetproc pProc, ptr pData, const xtaskargs* pArgs, uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task_group](../../examples/network/task_group/main.c) · 组延迟网络任务

```c
		pSecond = xrtTaskGroupNetAfter(
			pGroup,
			pEngine,
			0,
			buildGroupValue,
			&iSecond,
			NULL,
			1000u
		);
```

### `xrtTaskGroupNetUntil`

按单调截止时间提交网络任务，并原子纳入任务组。

```c
xfuture* xrtTaskGroupNetUntil(xtaskgroup* pGroup, xnetengine* pEngine, uint64 iAffinity, xtasknetproc pProc, ptr pData, const xtaskargs* pArgs, xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroup` | 输入 | 非空 | 目标任务组 |
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pProc` | 输入 | 非空 | 任务过程 |
| `pData` | 输入 | 任意值 | 任务数据，受理后所有权转移 |
| `pArgs` | 输入 | 允许空 | 任务参数 |
| `iDeadline` | 输入 | — | 单调时钟截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 任务 Future（引用 1） | — |
| `NULL` | 提交失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_CLOSED` — 组或池已关闭，不再接纳新项
- `XERR_MEMORY` — 对象或槽位分配失败

#### 范例

[task](../../examples/network/task/main.c) · 组限期网络任务

```c
			(xrtTaskGroupNetUntil(pGroup, pEngine, 0,
				buildValue, &iValue, NULL,
				xrtDeadlineAfter(0u)) == NULL) ||
```

## 统计

`xrtTaskPoolGet` 把一致快照写入 `xtaskpoolstats`：线程数、队列硬上限、排队数、运行数、受理数、完成数、成功数、失败数、取消数、拒绝数，以及关闭和取消状态。

恒等式在池排空后成立：

```text
Completed = Succeeded + Failed + Cancelled
Submitted = Completed
```

拒绝任务不计入 `Submitted`，只计入 `Rejected`。

## 示例

```c
xtaskpoolconfig tConfig = { 4, 1024, 0 };
xtaskpool* pPool = xrtTaskPoolCreate(&tConfig);
xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
xfuture* pFuture = xrtTaskGroupSubmitFor(
	pGroup,
	pPool,
	work,
	pData,
	NULL,
	UINT64_C(2000000)
);

if ( (pFuture != NULL) && (xrtTaskGroupWait(pGroup) == XWAIT_OK) ) {
	ptr pValue = xrtFutureValue(pFuture);
	(void)pValue;
}
xrtFutureDestroy(pFuture);
xrtTaskGroupDestroy(pGroup);
xrtTaskPoolDestroy(pPool);
```

完整示例位于 `examples/concurrency/task_group/main.c`、`examples/concurrency/task_group_scope/main.c`、`examples/concurrency/task_pool/main.c`、`examples/concurrency/task_coroutine/main.c`、`examples/network/task/main.c` 和 `examples/network/task_group/main.c`。`task_group_scope` 明确演示父级取消、子组传播和叶生产端确认终态之间的边界。
