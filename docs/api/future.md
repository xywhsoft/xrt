# Future 与 Promise

`future` 提供与网络无关的一次性异步结果。`xfuture` 是可共享的只读消费端，`xpromise` 是可引用的生产端；两者由一次分配共同创建，Future 的终态一旦写入便不可改变。

## 裁剪与分层

| 层 | 裁剪宏 | 依赖 | 能力 |
| --- | --- | --- | --- |
| Future 核心 | `XRT_FEATURE_FUTURE` | `cancel` | 结果、所有权、同步等待、协作取消 |
| Future 适配桥 | `XRT_FEATURE_FUTURE_BRIDGE` | `future`、`thread`、`atomic` | 异步操作、Promise 与取消监听的一次性装配 |
| Future 延续 | `XRT_FEATURE_FUTURE_CONTINUE` | `future` | Continue、Then、Catch、Finally 与安全结果透传 |
| Future 组合器 | `XRT_FEATURE_FUTURE_COMBINE` | `future` | Any、All、Race 与源结果保活 |
| 协程等待桥 | `XRT_FEATURE_FUTURE_COROUTINE` | `future`、`coroutine_scheduler` | 不阻塞调度线程的 await |

延续链、线程池执行器和结构化任务组属于后续独立层，不会增加只需要 Future 核心的程序体积。

## 统一等待边界

当前契约不再提供 `xwaitsrc` 或网络专用 `xnetwaitsrc`。异步操作直接返回同一种
`xfuture`：Future 同时携带终态、成功值、结构化错误、取消入口和所有权，因此线程
等待、协程 Await、延续、组合器和 TaskGroup 不需要先把它包装成另一个等待对象。

TCP 可读、可写、Drain、Accept 等只表示条件就绪的场景仍通过对应的
`xrtNet*WaitAsync` 返回 Future；真正接收数据或建立连接的操作返回拥有结果的 Future。
这种分层保留“只观察条件”和“取得结果”两类能力，但不复制 bool、`xnet_result` 和
Future 终态三套互相转换的状态模型。

需要组合不同对象类型时，上层只保存 `xfuture*`，通过 Any、All、Race 或 TaskGroup
协调；完成后按创建该 Future 的操作契约解释值。底层事件端口的 readiness/completion
观察仍属于网络引擎内部原语，不作为跨模块类型擦除接口泄漏到业务层。

## 状态

`xfuturestate` 只有五个稳定状态：

- `XFUTURE_PENDING`：生产端尚未写入结果。
- `XFUTURE_RESOLVED`：成功，可读取 `Value`。
- `XFUTURE_FAILED`：失败，可读取不可变 `xerror`。
- `XFUTURE_CANCELLED`：生产过程确认以取消结束。
- `XFUTURE_CLOSED`：最后一个生产端在未完成时离开，或显式关闭。

等待一个失败、取消或关闭的 Future 仍返回 `XWAIT_OK`，因为等待动作已经完成。调用方随后通过 `xrtFutureState` 或 `xrtFutureResult` 判断操作结果。`XWAIT_TIMEOUT` 和 `XWAIT_CANCELLED` 只描述等待者自身提前停止等待。

### `xrtFutureState`

返回 Future 状态快照；参数无效时返回 `CLOSED` 并设置错误。

```c
xfuturestate xrtFutureState(const xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 允许空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XFUTURE_PENDING/RESOLVED/FAILED/CANCELLED/CLOSED` | 终态枚举 | — |
| `CLOSED`（零值） | 参数无效 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · Promise 补集](../../examples/concurrency/future_tour/main.c) · 观察

```c
			(xrtFutureState(pSourceFut) != XFUTURE_FAILED) ) {
```


### `xrtFutureDone`

判断 Future 是否已经进入任一不可变终态。

```c
bool xrtFutureDone(const xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 允许空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已终态（RESOLVED/FAILED/CANCELLED/CLOSED） | — |
| `false` | 仍待定或参数非法 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · 等待族](../../examples/concurrency/future_tour/main.c) · 观察

```c
	if ( (pPromise == NULL) || (pFut1 == NULL) ||
		xrtFutureDone(pFut1) ||
```


## 创建与生命周期

```c
xfuture* pFuture;
xpromise* pPromise = xrtPromiseCreate(&pFuture, pParentCancel);
```

创建成功后，调用方各拥有一个 Promise 和 Future 引用。`pParentCancel` 可以为空；非空时，新 Future 的取消令牌继承父链。`xrtPromiseRef`、`xrtFutureRef` 分别增加端点引用，`xrtPromiseDestroy`、`xrtFutureDestroy` 分别释放。

最后一个 Promise 引用在 Pending 状态释放时，Future 自动进入 `XFUTURE_CLOSED` 并请求取消，避免消费者永久等待。已经完成的 Future 不会因 Promise 释放而改变状态或取消令牌。

Future 的内部完成监听按注册顺序执行。嵌套完成会追加到当前 Fiber 或线程的无分配派发队列，由最外层完成调用迭代排空；因此组合器和后续延续链的级联深度不会转化为 C 调用栈深度。

### `xrtPromiseCreate`

创建一对 Future/Promise；父取消令牌为空时使用独立取消源。

```c
xpromise* xrtPromiseCreate(xfuture** ppFuture, xcancel* pParentCancel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `ppFuture` | 输出 | 非空 | 接收消费端 Future |
| `pParentCancel` | 输入 | 允许空 | 父取消令牌；触发时级联到此 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 生产端 Promise（初始引用 1） | — |
| `NULL` | 创建失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[concurrency/future · 基础](../../examples/concurrency/future/main.c) · 观察

```c
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( (pPromise == NULL) || !xrtPromiseResolve(pPromise, &iValue) ) {
```


### `xrtPromiseRef`

增加 Promise 生产端引用并返回原指针。

```c
xpromise* xrtPromiseRef(xpromise* pPromise);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空 | 目标 Promise |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法或引用耗尽 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · Promise 补集](../../examples/concurrency/future_tour/main.c) · 观察

```c
		xpromise* pRef2 = xrtPromiseRef(pPromise);

		if ( (pRef2 != pPromise) ) {
			goto Cleanup;
		}
```


### `xrtPromiseDestroy`

释放生产端引用；最后一个未完成生产端会关闭 Future 并请求取消。

```c
void xrtPromiseDestroy(xpromise* pPromise);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 归零时 Future 进入 `CLOSED` 终态 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[concurrency/future · 基础](../../examples/concurrency/future/main.c) · 观察

```c
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
```


### `xrtFutureRef`

增加 Future 消费端引用并返回原指针。

```c
xfuture* xrtFutureRef(xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法或引用耗尽 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · 等待族](../../examples/concurrency/future_tour/main.c) · 观察

```c
	pRef = xrtFutureRef(pFut1);
	if ( (pRef != pFut1) || !xrtPromiseResolve(pPromise, (ptr)1) ) {
```


### `xrtFutureDestroy`

释放 Future 消费端引用；空指针视为空操作。

```c
void xrtFutureDestroy(xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 归零时释放值/错误与对象 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[concurrency/future_combine · 收尾](../../examples/concurrency/future_combine/main.c) · 观察

```c
	xrtFutureDestroy(pRace);
```


## 无分配终态监听

`xfuturewatch` 是调用方提供存储的底层终态监听节点，使用
`XRT_FUTURE_WATCH_STORAGE_SIZE` 固定不透明空间，不为每次监听分配内存。
`xfuturewatchproc` 接收通知上下文；可选的 `xfuturewatchreleaseproc` 在节点被
Future 接管后，于完成通知或成功摘除时执行一次。

```c
bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
);
xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
);
bool xrtFutureWatchDetach(xfuture* pFuture, xfuturewatch* pWatch);
void xrtFutureWatchRemove(xfuture* pFuture, xfuturewatch* pWatch);
```

`xrtFutureWatchAdd` 返回 `XFUTURE_WATCH_PENDING` 时，Future 已接管节点；返回
`XFUTURE_WATCH_READY` 时源已经完成，节点仍归调用方且不会执行 Release；
`XFUTURE_WATCH_ERROR` 表示参数或状态无效。`xrtFutureWatchDetach` 只尝试摘除尚未
开始通知的节点，成功时同步执行 Release；`xrtFutureWatchRemove` 还会等待已经开始的
通知结束，不能从该 Watch 自己的通知中调用。`xfuturewatchresult` 只描述注册结果，
不替代 Future 的终态。

### `xrtPromiseResolve`

以借用方式完成成功结果，值的生命周期由调用方保证。

```c
bool xrtPromiseResolve(xpromise* pPromise, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空、待定 | 目标 Promise |
| `pValue` | 输入 | 借用 | 成功值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已发布 `RESOLVED` 终态 | — |
| `false` | 已完成或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future · 基础](../../examples/concurrency/future/main.c) · 观察

```c
	if ( (pPromise == NULL) || !xrtPromiseResolve(pPromise, &iValue) ) {
```


### `xrtPromiseResolveOwned`

转移成功值所有权；完成失败时所有权仍归调用方。

```c
bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空、待定 | 目标 Promise |
| `pValue` | 输入 | 成功后移交 | 拥有值 |
| `pDestroy` | 输入 | 非空 | 值析构过程 |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已完成；Future 归零时执行析构恰好一次 | — |
| `false` | 已完成 | 所有权仍归调用方 |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法

#### 范例

[concurrency/worker · 结果移交](../../examples/concurrency/worker/main.c) · 观察

```c
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pJob,
			workerJobFree,
			NULL
		) ) {
```


### `xrtPromiseReject`

以增加引用方式完成失败结果。

```c
bool xrtPromiseReject(xpromise* pPromise, const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空、待定 | 目标 Promise |
| `pError` | 输入 | 非空 | 失败错误（内部增引用） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已发布 `FAILED` 终态 | — |
| `false` | 已完成或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · Promise 补集](../../examples/concurrency/future_tour/main.c) · 观察

```c
		if ( (pFail == NULL) ||
			!xrtPromiseReject(pFail, pError) ) {
```


### `xrtPromiseForward`

把已进入终态的源 Future 结果安全透传到 Promise。

```c
bool xrtPromiseForward(xpromise* pPromise, xfuture* pSource);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空、待定 | 目标 Promise |
| `pSource` | 输入 | 非空、已终态 | 源 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 源终态已透传 | — |
| `false` | 源待定或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · 透传](../../examples/concurrency/future_tour/main.c) · 观察

```c
			!xrtPromiseForward(pSource, pFailFut) ||
			!xrtPromiseDone(pSource) ||
```


## 结果与所有权

```c
bool xrtPromiseResolve(xpromise* pPromise, ptr pValue);
bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
bool xrtPromiseReject(xpromise* pPromise, const xerror* pError);
bool xrtPromiseCancel(xpromise* pPromise);
bool xrtPromiseClose(xpromise* pPromise);
```

- `xrtPromiseResolve` 只借用值。调用方必须保证值至少活到全部 Future 引用释放之后。
- `xrtPromiseResolveOwned` 仅在成功完成时接管值；重复完成失败时，值仍归调用方。
- `xrtPromiseReject` 增加不可变错误引用，调用方仍释放自己的错误引用。
- 只有第一个终态写入成功。重复写入返回 `false` 并设置 `XERR_STATE`。
- Future 最后释放时执行 owned 值析构；析构过程在 Future 内部锁外运行。

`xrtFutureDone` 和 `xrtPromiseDone` 判断对应端点是否已经进入终态。`xrtFutureResult` 返回借用的 `xfutureresult`，`xrtFutureError` 直接返回失败终态中的借用错误。结果中的值和错误只在调用方继续持有 Future 引用时有效。Pending 返回 `false` 并设置 `XERR_AGAIN`。`xrtFutureValue` 是常见成功路径助手；对失败、取消或关闭状态调用时，会把对应结构化错误设置到当前执行上下文。

`xrtPromiseForward` 把一个已经进入终态的源 Future 安全透传到目标 Promise。失败错误会增加引用；成功值保持借用语义，同时由目标 Future 保留源引用。透传所有者链使用迭代释放，因此深延续链的销毁不会递归耗尽 C 调用栈。源仍为 Pending、目标已经完成或发生自透传时返回 `false`。

### `xrtFutureResult`

复制借用结果三元组（State/Value/Error）；尚未完成时返回 false 并设置 `AGAIN`。

```c
bool xrtFutureResult(const xfuture* pFuture, xfutureresult* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `pResult` | 输出 | 非空 | 接收借用快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 终态快照已写出（值/错误由 Future 引用保护） | — |
| `false` | 待定或参数非法 | `XERR_AGAIN`（待定） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — Future 仍待定

#### 范例

[network/tcp_server_sync · 接受结果](../../examples/network/tcp_server_sync/main.c) · 观察

```c
		!xrtFutureResult(pAccept, &Result) ||
		(Result.State != XFUTURE_RESOLVED) ) {
```


### `xrtFutureValue`

返回成功值；非成功终态会把对应错误设置到当前执行上下文。

```c
ptr xrtFutureValue(const xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 成功值（借用，由 Future 引用保护） | — |
| `NULL` | 非成功终态或参数非法 | 对应错误已设置 |

#### 错误

- 失败终态 — 结构化错误设置到当前执行上下文
- 取消/关闭 — `XERR_CANCELLED` / `XERR_CLOSED` 设置

#### 范例

[concurrency/future · 基础](../../examples/concurrency/future/main.c) · 观察

```c
	printf("future value: %d\n", *(int*)xrtFutureValue(pFuture));
```


### `xrtFutureError`

返回失败终态借用的结构化错误，其他状态返回空指针。

```c
const xerror* xrtFutureError(const xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 允许空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 失败错误借用（存活到 Future 释放） | — |
| `NULL` | 非失败终态 | 纯查询 |

#### 错误

- 无 — 非失败状态返回空是查询结果

#### 范例

[network/resolver_future · 失败路径](../../examples/network/resolver_future/main.c) · 观察

```c
		const xerror* pError = xrtFutureError(pFuture);
```


## 取消

```c
bool xrtFutureCancel(xfuture* pFuture);
xcancel* xrtFutureCancelToken(const xfuture* pFuture);
xcancel* xrtPromiseCancelToken(const xpromise* pPromise);
```

`xrtFutureCancel` 只发出协作取消请求，Future 保持 Pending，直到生产端完成、确认取消或关闭。这样资源回收、内核 I/O 取消和任务退出不会被一个伪造终态掩盖。令牌访问函数返回新增引用，使用完毕后调用 `xrtCancelDestroy`。

生产过程可以处理取消后正常返回值；取消请求与最终状态是两个正交事实。只有 `xrtPromiseCancel` 才写入 `XFUTURE_CANCELLED`。

`xrtPromiseCancel`、`xrtPromiseClose` 和最后一个 Promise 引用的隐式关闭使用两阶段发布：先独占终态写入权并完成取消令牌通知，再发布 `CANCELLED`/`CLOSED`、唤醒等待者和执行 Future 回调。因此任何已经观察到取消或关闭终态的线程都必然也能观察到令牌已经请求；令牌监听执行期间 Future 仍显示为 Pending，但其他生产者已经不能抢占终态。取消监听不得等待同一个 Future 进入终态，否则会形成生产端自等待。

### `xrtFutureCancel`

请求生产过程协作取消；请求本身不伪造 Future 终态。

```c
bool xrtFutureCancel(xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 取消请求已发出（幂等） | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[network/tcp_server_sync · 清理](../../examples/network/tcp_server_sync/main.c) · 观察

```c
		(void)xrtFutureCancel(pAccept);
```


### `xrtFutureCancelToken`

返回增加引用后的取消令牌，调用方使用完毕后必须 `xrtCancelDestroy` 释放。

```c
xcancel* xrtFutureCancelToken(const xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 引用 +1 的令牌 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_combine · 令牌](../../examples/concurrency/future_combine/main.c) · 观察

```c
	pFirstCancel = xrtFutureCancelToken(pFirst);
```


### `xrtPromiseCancelToken`

返回增加引用后的生产端取消令牌。

```c
xcancel* xrtPromiseCancelToken(const xpromise* pPromise);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空 | 目标 Promise |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 引用 +1 的令牌 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/worker · 令牌检查](../../examples/concurrency/worker/main.c) · 观察

```c
		xcancel* pCancel = xrtPromiseCancelToken(pPromise);
```


### `xrtPromiseCancel`

完成取消终态；令牌请求通知结束后才向等待者发布取消终态。

```c
bool xrtPromiseCancel(xpromise* pPromise);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空 | 目标 Promise |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已发布 `CANCELLED` 终态 | — |
| `false` | 已完成或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/task_group_scope · 级联](../../examples/concurrency/task_group_scope/main.c) · 观察

```c
		!xrtPromiseCancel(pLeafPromise) ||
		(xrtFutureWait(pDone) != XWAIT_OK) ) {
```


## 同步等待

```c
xwaitresult xrtFutureWait(xfuture* pFuture);
xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout);
xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline);
xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

时间单位统一为微秒，截止时间使用单调时钟。等待支持任意数量线程，并对虚假唤醒、完成与超时竞争、取消监听注销竞争进行循环检查。Future 终态与外部取消回调在同一把 Future 锁下线性化：终态先取得锁时返回 `XWAIT_OK`；取消先取得锁时，本次等待固定返回 `XWAIT_CANCELLED`，即使 Future 在等待线程恢复前已经完成也不会覆盖该结果。截止时间在相同循环边界检查，Future 尚未终结时到期返回 `XWAIT_TIMEOUT`。调用方取消只停止当前等待，不取消 Future；需要同时取消生产过程时另行调用 `xrtFutureCancel`。

### `xrtPromiseClose`

请求生产过程停止，并在令牌通知结束后发布关闭终态。

```c
bool xrtPromiseClose(xpromise* pPromise);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 非空 | 目标 Promise |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已发布 `CLOSED` 终态 | — |
| `false` | 已完成或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/worker · 任务关闭](../../examples/concurrency/worker/main.c) · 观察

```c
	(void)xrtPromiseClose(pJob->Promise);
	xrtPromiseDestroy(pJob->Promise);
```


### `xrtPromiseDone`

判断 Promise 对应的 Future 是否已经完成。

```c
bool xrtPromiseDone(const xpromise* pPromise);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPromise` | 输入 | 允许空 | 目标 Promise |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Future 已终态 | — |
| `false` | 待定或参数非法 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · Promise 补集](../../examples/concurrency/future_tour/main.c) · 观察

```c
			!xrtPromiseForward(pSource, pFailFut) ||
			!xrtPromiseDone(pSource) ||
```


### `xrtFutureWatchInit`

初始化一个尚未注册的无分配 Future Watch。

```c
bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输出 | 非空 | 调用方固定存储（64 字节） |
| `pNotify` | 输入 | 非空 | 终态通知回调 |
| `pRelease` | 输入 | 允许空 | Watch 释放过程（恰好一次） |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Watch 已就绪 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · Watch](../../examples/concurrency/future_tour/main.c) · 观察

```c
			!xrtFutureWatchInit(&Watch, exampleWatchNotify,
				exampleWatchRelease, NULL) ||
```


### `xrtFutureWatchAdd`

Future 未完成时注册 Watch；`READY` 时 Watch 未被接管且不执行 Release。

```c
xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `pWatch` | 输入 | 已 Init 且未注册 | Watch |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XFUTURE_WATCH_PENDING` | 已进入等待链，完成时执行 Notify | — |
| `XFUTURE_WATCH_READY` | Future 已终态；回调不执行、Release 不执行 | — |
| `XFUTURE_WATCH_ERROR` | 参数或状态错误 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — Watch 已注册到其他 Future

#### 范例

[concurrency/future_tour · Watch](../../examples/concurrency/future_tour/main.c) · 观察

```c
			(xrtFutureWatchAdd(pWF, &Watch) !=
				XFUTURE_WATCH_PENDING) ) {
```


## 协程等待

```c
xwaitresult xrtFutureAwait(xfuture* pFuture);
xwaitresult xrtFutureAwaitFor(xfuture* pFuture, uint64 iTimeout);
xwaitresult xrtFutureAwaitUntil(xfuture* pFuture, xdeadline iDeadline);
```

这些函数只能在 `xcosched` 管理的协程中调用。等待节点由当前协程栈保存，不产生每次 await 堆分配；Future 可由任意线程完成，完成通知通过内部代际令牌投递回所属调度器。通知早于真正 park 时不会丢失，等待退出后也不会污染下一次 park；独立的公共 `xrtCoWake` 不会被 Await 清理过程误消费。提前唤醒、超时和协程取消都会安全摘除等待节点。

协程取消使 `Await` 返回 `XWAIT_CANCELLED`，但不改变被等待 Future 的状态，也不自动决定当前协程的终态。用户过程可以完成清理后正常返回；只有显式调用 `xrtCoConfirmCancel`，当前协程才以 `XCORO_TERM_CANCELLED` 终结。对于一次调用者独占的网络 Future，放弃操作时应先调用 `xrtFutureCancel`，让 TCP/UDP 等待节点及时摘除；共享 Future 则由拥有生产过程取消权的一层决定。

### `xrtFutureWatchDetach`

尝试摘除尚未开始通知的 Watch；成功时同步执行 Release。

```c
bool xrtFutureWatchDetach(
	xfuture* pFuture,
	xfuturewatch* pWatch
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `pWatch` | 输入 | 已注册 | 要摘除的 Watch |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已摘除并同步执行 Release | — |
| `false` | 通知已开始或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法
- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · Watch](../../examples/concurrency/future_tour/main.c) · 观察

```c
			!xrtFutureWatchDetach(pWF, &Watch) ) {
```


### `xrtFutureWatchRemove`

摘除 Watch 并等待已经开始的通知结束；禁止从自身通知中调用。

```c
void xrtFutureWatchRemove(
	xfuture* pFuture,
	xfuturewatch* pWatch
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `pWatch` | 输入 | 已注册 | 要移除的 Watch |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 返回后通知不再触发；Release 已执行 | — |

#### 错误

- `XERR_STATE` — 已进入终态（Promise 只能完成一次）或对象状态非法（从自身通知回调中调用会自等待死锁）

#### 范例

[concurrency/future_tour · Watch](../../examples/concurrency/future_tour/main.c) · 观察

```c
		xrtFutureWatchRemove(pWF, &Watch);
```


### `xrtFutureWait`

等待 Future 进入任一终态。

```c
xwaitresult xrtFutureWait(xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已终态 | — |
| `XWAIT_ERROR` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future · 基础](../../examples/concurrency/future/main.c) · 观察

```c
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
```


### `xrtFutureAwait`

在当前调度协程中挂起等待终态，不阻塞调度线程。

```c
xwaitresult xrtFutureAwait(xfuture* pFuture);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已终态 | — |
| `XWAIT_ERROR` | 参数非法或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在协程调度器内

#### 范例

[concurrency/future_coroutine · 挂起](../../examples/concurrency/future_coroutine/main.c) · 观察

```c
	if ( xrtFutureAwait(pFuture) != XWAIT_OK ) {
```


### `xrtFutureAwaitFor`

协程挂起到相对期限。

```c
xwaitresult xrtFutureAwaitFor(xfuture* pFuture, uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` | 终态/到期 | — |
| `XWAIT_ERROR` | 参数/上下文错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在协程调度器内

#### 范例

[concurrency/report · 超时控制](../../examples/concurrency/report/main.c) · 观察

```c
	eWait = xrtFutureAwaitFor(pDone, UINT64_C(2000000));
```


### `xrtFutureAwaitUntil`

协程挂起到绝对截止时间。

```c
xwaitresult xrtFutureAwaitUntil(xfuture* pFuture, xdeadline iDeadline);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` | 终态/到期 | — |
| `XWAIT_ERROR` | 参数/上下文错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在协程调度器内

#### 范例

[concurrency/future_tour · 协程](../../examples/concurrency/future_tour/main.c) · 观察

```c
	return (ptr)(uintptr_t)xrtFutureAwaitUntil(pFuture,
		xrtDeadlineAfter(EXAMPLE_TIMEOUT_US));
```


## 延续

```c
xfuture* xrtFutureContinue(xfuture* pSource, xfuturecontinueproc pProc, ptr pData);
xfuture* xrtFutureThen(xfuture* pSource, xfuturecontinueproc pProc, ptr pData);
xfuture* xrtFutureCatch(xfuture* pSource, xfuturecontinueproc pProc, ptr pData);
xfuture* xrtFutureFinally(xfuture* pSource, xfuturefinallyproc pProc, ptr pData);
```

延续返回一个独立输出 Future。`Continue` 对全部终态执行；`Then` 只处理 `RESOLVED`；`Catch` 只处理 `FAILED`，不会吞掉 `CANCELLED` 或 `CLOSED`；`Finally` 观察任意终态并自动透传原结果。未命中的条件延续也自动透传。

尚未完成的源在完成 Promise 的线程或 Fiber 中执行短回调；已经完成的源在注册延续的执行上下文中同步执行。内部按注册顺序派发，深链迭代排空，不需要 current-thread pump。回调属于完成路径，不得阻塞、挂起协程或执行长时间 CPU 工作；这些工作应显式提交到任务池、协程调度器或上层网络 worker。

`xfuturecontinueproc` 借用输入结果和输出 Promise。回调必须在返回前完成输出 Promise，或者先调用 `xrtPromiseRef` 保留它并转交异步路径；如果两者都不做，运行库释放最后一个生产端后，输出 Future 进入 `CLOSED`。回调不得释放借用的 Promise 引用。

每个延续都有对应的 `xrtFutureContinueOwned`、`xrtFutureThenOwned`、`xrtFutureCatchOwned` 或 `xrtFutureFinallyOwned` 入口。调用成功后，运行库接管 `pData`，并在回调执行、条件跳过或输出取消后调用一次析构过程；调用失败时所有权仍属于调用方。析构在回调返回后执行，因此异步带走 Promise 时，回调也必须自行转移异步工作所需的数据。

输出 Future 的取消请求会使尚未开始的 `Continue`、`Then` 或 `Catch` 跳过用户回调，并在源进入终态后确认输出取消。`Finally` 仍观察源终态，但输出结果改为取消。取消输出不会伪造或强制改变共享源 Future 的终态。

`xrtFutureContinueOwnedCancelSource` 与 `xrtFutureThenOwnedCancelSource` 用于调用方明确拥有完整生产链的组合层。它们保持对应延续的选择和透传语义，但输出被取消时也向源发出协作取消请求，使文件读取、网络操作或任务能够尽早停止。普通共享源不得使用这两个入口，否则一个消费者会意外取消其他消费者仍需等待的工作。

### `xrtFutureContinue`

对源的任意终态执行延续过程；过程负责完成或保留输出 Promise。

```c
xfuture* xrtFutureContinue(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · 非 Owned 补齐](../../examples/concurrency/future_tour/main.c) · 观察

```c
			pAnyChain = xrtFutureContinue(pPlainF,
				exampleContinueAny, NULL);
```


### `xrtFutureContinueOwned`

执行任意终态延续，并在执行、跳过或取消后释放受理的数据。

```c
xfuture* xrtFutureContinueOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pDestroy` | 输入 | 非空 | 受理数据析构（跳过/执行/取消后恰好一次） |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · Owned 注册](../../examples/concurrency/future_tour/main.c) · 观察

```c
		pB = xrtFutureContinueOwned(pA, exampleContinueAny,
			NULL, exampleDestroy, NULL);
```


### `xrtFutureContinueOwnedCancelSource`

对独占源的任意终态执行延续；取消输出时同时请求取消源。只适用于不与其他消费者共享源的组合层。

```c
xfuture* xrtFutureContinueOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pDestroy` | 输入 | 非空 | 受理数据析构（跳过/执行/取消后恰好一次） |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · Owned 注册](../../examples/concurrency/future_tour/main.c) · 观察

```c
			xfuture* pG = xrtFutureContinueOwnedCancelSource(
				pD ? pD : pB, exampleContinueAny, NULL,
				exampleDestroy, NULL);
```


### `xrtFutureThen`

仅在源成功时执行延续；其他终态自动透传。

```c
xfuture* xrtFutureThen(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_continue · 串联](../../examples/concurrency/future_continue/main.c) · 观察

```c
	pNext = xrtFutureThen(pSource, addFive, &iResult);
```


### `xrtFutureThenOwned`

仅在源成功时执行延续，并负责释放受理的数据。

```c
xfuture* xrtFutureThenOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pDestroy` | 输入 | 非空 | 受理数据析构（跳过/执行/取消后恰好一次） |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · 链](../../examples/concurrency/future_tour/main.c) · 观察

```c
		pChain = xrtFutureThenOwned(pPO ? pOk : NULL,
			exampleContinueAdd, NULL, exampleDestroy, NULL);
```


### `xrtFutureThenOwnedCancelSource`

仅在独占源成功时执行延续；取消输出时同时请求取消源。失败/取消/关闭仍按 Then 契约透传。

```c
xfuture* xrtFutureThenOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pDestroy` | 输入 | 非空 | 受理数据析构（跳过/执行/取消后恰好一次） |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · Owned 注册](../../examples/concurrency/future_tour/main.c) · 观察

```c
		pD = xrtFutureThenOwnedCancelSource(pC,
			exampleContinueAdd, NULL, exampleDestroy, NULL);
```


### `xrtFutureCatch`

仅在源失败时执行延续；成功、取消和关闭终态自动透传。

```c
xfuture* xrtFutureCatch(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · 非 Owned 补齐](../../examples/concurrency/future_tour/main.c) · 观察

```c
			pCatchChain = xrtFutureCatch(pPlainBad,
				exampleContinueRescue, NULL);
```


### `xrtFutureCatchOwned`

仅在源失败时执行延续，并负责释放受理的数据。

```c
xfuture* xrtFutureCatchOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pDestroy` | 输入 | 非空 | 受理数据析构（跳过/执行/取消后恰好一次） |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · 补救](../../examples/concurrency/future_tour/main.c) · 观察

```c
			xfuture* pCatch = xrtFutureCatchOwned(pBad,
				exampleContinueRescue, NULL,
				exampleDestroy, NULL);
```


### `xrtFutureFinally`

观察源的任意终态，再把原结果安全透传到输出 Future。

```c
xfuture* xrtFutureFinally(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · 观察](../../examples/concurrency/future_tour/main.c) · 观察

```c
			pFin = xrtFutureFinally(pFut2,
				exampleFinallyObserve, NULL);
```


### `xrtFutureFinallyOwned`

观察源的任意终态、透传结果，并负责释放受理的数据。

```c
xfuture* xrtFutureFinallyOwned(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSource` | 输入 | 非空 | 源 Future |
| `pProc` | 输入 | 非空 | 延续过程 |
| `pData` | 输入 | 任意值 | 原样传给过程 |
| `pDestroy` | 输入 | 非空 | 受理数据析构（跳过/执行/取消后恰好一次） |
| `pDestroyData` | 输入 | 任意值 | 原样传给析构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 输出 Future | — |
| `NULL` | 参数非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_tour · Owned 注册](../../examples/concurrency/future_tour/main.c) · 观察

```c
			xfuture* pF = xrtFutureFinallyOwned(pE,
				exampleFinallyObserve, NULL,
				exampleDestroy, NULL);
```


## 组合器

```c
xfuture* xrtFutureAny(xfuture* const* pFutures, size_t iCount);
xfuture* xrtFutureAll(xfuture* const* pFutures, size_t iCount);
xfuture* xrtFutureRace(xfuture* const* pFutures, size_t iCount);
```

组合器只协调多个 Future 的生命周期，不复制、不接管源值，也不把某个源的失败冒充为组合器自身失败：

- `Any` 在第一个源进入任意终态时，以 `xfuturepick` 成功完成，不改变其余源。
- `Race` 与 `Any` 的选择结果相同，随后向其余源发出协作取消请求；源生产端仍决定各自最终状态。
- `All` 等全部输入槽位进入终态后，以保序的 `xfutureall` 成功完成。重复源按重复槽位计数；空集合立即成功完成。
- 输入数量同时受连续分配大小和内部引用计数上限约束，超出任一边界都会在读取输入数组前以 `XERR_RANGE` 失败。
- `xfuturepick` 和 `xfutureall` 都是借用视图，只在持有组合 Future 引用时有效。组合器内部保留全部源引用，因此通过视图读取源结果不会悬空。
- 取消尚未完成的组合 Future 会确认组合器自身为 `XFUTURE_CANCELLED`、摘除全部源监听，并向源发出取消请求；源 Future 不会被伪造为已取消。

这种契约把“多个操作何时达到协调点”和“每个操作以什么结果结束”分开。调用方可以直接检查 `pPick->Future` 或 `pAll->Futures[i]` 的状态、值和结构化错误，也可以在上层任务组中实现失败即取消、收集全部错误等策略。

### `xrtFutureAny`

在任一源进入终态后，以 `xfuturepick` 成功完成；不改变其余源。

```c
xfuture* xrtFutureAny(xfuture* const* pFutures, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFutures` | 输入 | 非空数组 | 源 Future 数组 |
| `iCount` | 输入 | `> 0` | 源数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 组合 Future；成功值为 `xfuturepick{Index,Future}` | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_combine · Any](../../examples/concurrency/future_combine/main.c) · 观察

```c
	pAny = xrtFutureAny(arrFuture, 2);
```


### `xrtFutureAll`

在全部源进入终态后，以保序的 `xfutureall` 成功完成；空集合立即完成。

```c
xfuture* xrtFutureAll(xfuture* const* pFutures, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFutures` | 输入 | 允许空（`iCount==0`） | 源数组 |
| `iCount` | 输入 | — | 源数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 组合 Future；成功值按输入顺序借用全部源 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_combine · All](../../examples/concurrency/future_combine/main.c) · 观察

```c
	pAll = xrtFutureAll(arrFuture, 2);
```


### `xrtFutureRace`

在任一源进入终态后完成，并向其余未完成源发出协作取消请求。

```c
xfuture* xrtFutureRace(xfuture* const* pFutures, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFutures` | 输入 | 非空数组 | 源 Future 数组 |
| `iCount` | 输入 | `> 0` | 源数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 组合 Future；其余源收到取消请求 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/future_combine · Race](../../examples/concurrency/future_combine/main.c) · 观察

```c
	pRace = xrtFutureRace(arrFuture, 2);
```


## 示例

```c
xfuture* pFuture;
xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
int iValue = 42;

if ( (pPromise == NULL) || !xrtPromiseResolve(pPromise, &iValue) ) {
	return false;
}
if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
	return false;
}
printf("%d\n", *(int*)xrtFutureValue(pFuture));
xrtPromiseDestroy(pPromise);
xrtFutureDestroy(pFuture);
```

网络适配层直接返回同一种 `xfuture`。TCP、UDP、TLS Stream、HTTP 和 WebSocket
的 Future 都可以交给上述同步等待、组合器、延续和协程 Await；协议层只定义
成功值、取消点和终态映射，不复制 Future 或协程状态机。

标准库和扩展库的异步适配器共用公开的 `xfuturebridge`；对象使用
`XRT_FUTURE_BRIDGE_STORAGE_SIZE` 字节不透明存储，可直接嵌入操作上下文。底层操作可以
在监听安装期间并发完成，但只有装配进入 READY 后才能发布 Future；监听分配
失败会进入 FAILED，底层晚到结果只执行对应资源回收，不会留下回调。这个底层
契约由 `tests/concurrency/test_future_bridge_oom.c` 独立验证，DNS、TCP、TLS 与
HTTP 适配器不再分别复制这一段易错生命周期代码。

```c
xfuturebridge bridge;
xfuture* future = xrtFutureBridgeCreate(&bridge, parentCancel);
xpromise* promise = xrtFutureBridgePromise(&bridge);

if ( !xrtFutureBridgeWatch(&bridge, cancelOperation, operation) ) {
	(void)xrtFutureBridgeFail(&bridge);
	cancelUnderlyingOperation(operation);
	return NULL;
}
(void)xrtFutureBridgeReady(&bridge);
```

`xrtFutureBridgeCreate` 创建 Future/Promise 对；`xrtFutureBridgeInit` 则借用一个
已有 Promise，两种路径都不接管 Promise 所有权。适配器最终仍须调用
`xrtPromiseDestroy(xrtFutureBridgePromise(&bridge))`。`xrtFutureBridgeReady` 与
`xrtFutureBridgeFail` 只能发布一次，重复发布以状态错误失败。完成回调先调用
`xrtFutureBridgeWait` 跨过极短装配窗口，再调用 `xrtFutureBridgeUnwatch` 与正在
执行的取消回调汇合。Wait 返回 `false` 时只回收底层结果，不写入 Promise。

适配器发布终态前必须先固定成功值或错误引用、注销取消监听、销毁底层操作，
并归还等待节点、缓存预算、网络对象引用和临时 Engine 租约。因而观察到网络
Future 终态时，适配器上下文不会再被迟到回调访问，也没有只为该操作保留的
Engine 活动对象。成功 Future 明确拥有的 Stream、Packet、响应或连接是公开结果，
其生命周期继续由 Future 引用保护，不属于内部临时资源。协作取消同样遵守该屏障，
不会先伪造 `CANCELLED` 再在后台回收操作。

完整示例位于 `examples/concurrency/future/main.c`、
`examples/concurrency/future_combine/main.c`、
`examples/concurrency/future_coroutine/main.c` 与
`examples/tls/stream_future/main.c`、`examples/tls/dial_future/main.c`。
其中 `future_combine` 同时演示 Any、All、Race、胜出源索引和 Race 的协作取消语义。
