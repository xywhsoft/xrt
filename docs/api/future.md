# Future 与 Promise

`future` 提供与网络无关的一次性异步结果。`xfuture` 是可共享的只读消费端，`xpromise` 是可引用的生产端；两者由一次分配共同创建，Future 的终态一旦写入便不可改变。

## 类型与常量

### `xfuturestate`

Future 终态明确区分成功、失败、协作取消和生产端关闭。

```c
typedef enum xfuturestate {
	XFUTURE_PENDING = 0,
	XFUTURE_RESOLVED = 1,
	XFUTURE_FAILED = 2,
	XFUTURE_CANCELLED = 3,
	XFUTURE_CLOSED = 4
} xfuturestate;
```

| 值 | 语义 |
|---|---|
| `XFUTURE_PENDING` | 等待中 |
| `XFUTURE_RESOLVED` | RESOLVED |
| `XFUTURE_FAILED` | 已失败 |
| `XFUTURE_CANCELLED` | 已取消 |
| `XFUTURE_CLOSED` | 已关闭 |

### `xfutureresult`

Future 结果只借用值和错误，其生命周期由 Future 引用保护。

```c
typedef struct xfutureresult {
	xfuturestate State;
	ptr Value;
	const xerror* Error;
} xfutureresult;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xfuturestate` | 状态 |
| `Value` | `ptr` | 值 |
| `Error` | `const xerror*` | 错误输出 |

### `xfuturewatch`

Watch 的内部链表和并发状态保持不透明。

```c
typedef union xfuturewatch {
	uint64 Alignment;
	uint8 Storage[XRT_FUTURE_WATCH_STORAGE_SIZE];
} xfuturewatch;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xfuturewatchresult`

注册结果区分错误、Future 已完成和成功进入等待链。

```c
typedef enum xfuturewatchresult {
	XFUTURE_WATCH_ERROR = -1,
	XFUTURE_WATCH_READY = 0,
	XFUTURE_WATCH_PENDING = 1
} xfuturewatchresult;
```

| 值 | 语义 |
|---|---|
| `XFUTURE_WATCH_ERROR` | 失败 |
| `XFUTURE_WATCH_READY` | 就绪 |
| `XFUTURE_WATCH_PENDING` | 等待事件期间 |

### `xfuturepick`

Any 与 Race 的结果借用胜出源 Future；组合 Future 负责保留该引用。

```c
typedef struct xfuturepick {
	size_t Index;
	xfuture* Future;
} xfuturepick;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Index` | `size_t` | 索引 |
| `Future` | `xfuture*` | Future |

### `xfutureall`

All 的结果按输入顺序借用全部源 Future；组合 Future 负责保留这些引用。

```c
typedef struct xfutureall {
	size_t Count;
	xfuture* const* Futures;
} xfutureall;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Count` | `size_t` | 数量 |
| `Futures` | `xfuture* const*` | Futures |

### `xfuture`

Future 是只读共享结果，Promise 是唯一终态写入端。

```c
typedef struct xfuture xfuture;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xpromise`

Promise 对象（不透明）：解析端单写、Future 端多读的一次性同步原语，随 future_bridge 启用。


```c
typedef struct xpromise xpromise;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xfuturewatchproc`

Watch 回调在线程安全的 Future 完成路径中执行，不得重入同一个 Watch。

```c
typedef void (*xfuturewatchproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfuturewatchreleaseproc`

Watch 释放过程在线性化完成通知或成功摘除后执行一次。

```c
typedef void (*xfuturewatchreleaseproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfuturefreeproc`

成功值析构过程接收创建者提供的值和上下文。

```c
typedef void (*xfuturefreeproc)(ptr pValue, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfuturecontinueproc`

延续过程借用源结果和输出 Promise；保留 Promise 时必须先增加引用。

```c
typedef void (*xfuturecontinueproc)(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfuturefinallyproc`

Finally 过程只观察源结果，输出 Future 自动安全透传源终态。

```c
typedef void (*xfuturefinallyproc)(const xfutureresult* pInput, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfuturebridge`

Future 桥的内部状态保持不透明，可直接嵌入异步操作上下文。

```c
typedef union xfuturebridge {
	uint64 Alignment;
	uint8 Storage[XRT_FUTURE_BRIDGE_STORAGE_SIZE];
} xfuturebridge;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xtlsstreamconfig`

两个超时都使用微秒；零值显式关闭对应计时器。 AsyncBytesLimit 和 AsyncCountLimit 是未完成操作的独立硬边界， AsyncBatch 限制一次 Worker 轮转完成的操作数。

```c
typedef struct xtlsstreamconfig {
	uint64 HandshakeTimeout;
	uint64 CloseTimeout;
	size_t AsyncBytesLimit;
	uint32 AsyncCountLimit;
	uint32 AsyncBatch;
} xtlsstreamconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `HandshakeTimeout` | `uint64` | HandshakeTimeout |
| `CloseTimeout` | `uint64` | CloseTimeout |
| `AsyncBytesLimit` | `size_t` | AsyncBytesLimit |
| `AsyncCountLimit` | `uint32` | AsyncCountLimit |
| `AsyncBatch` | `uint32` | AsyncBatch |

### `xtlsstreamstate`

FAILED 保存 TLS 或传输根因；CLOSED 只表示完成认证关闭。

```c
typedef enum xtlsstreamstate {
	XTLS_STREAM_CONNECTING = 0,
	XTLS_STREAM_HANDSHAKE,
	XTLS_STREAM_OPEN,
	XTLS_STREAM_CLOSING,
	XTLS_STREAM_CLOSED,
	XTLS_STREAM_FAILED
} xtlsstreamstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_STREAM_CONNECTING` | 连接中 |
| `XTLS_STREAM_HANDSHAKE` | 握手阶段 |
| `XTLS_STREAM_OPEN` | 开放（握手完成） |
| `XTLS_STREAM_CLOSING` | 关闭中 |
| `XTLS_STREAM_CLOSED` | 已关闭 |
| `XTLS_STREAM_FAILED` | 已失败 |

### `xtlsstreamwait`

条件 Future 是水平条件；END 表示收到认证 close_notify， CLOSE 表示底层传输和 TLS 组合对象进入最终终态。

```c
typedef enum xtlsstreamwait {
	XTLS_STREAM_WAIT_OPEN = 0,
	XTLS_STREAM_WAIT_READ,
	XTLS_STREAM_WAIT_WRITE,
	XTLS_STREAM_WAIT_DRAIN,
	XTLS_STREAM_WAIT_END,
	XTLS_STREAM_WAIT_CLOSE
} xtlsstreamwait;
```

| 值 | 语义 |
|---|---|
| `XTLS_STREAM_WAIT_OPEN` | 等待开放 |
| `XTLS_STREAM_WAIT_READ` | 读方向 |
| `XTLS_STREAM_WAIT_WRITE` | 写方向 |
| `XTLS_STREAM_WAIT_DRAIN` | 排空策略 |
| `XTLS_STREAM_WAIT_END` | 等待关闭完成 |
| `XTLS_STREAM_WAIT_CLOSE` | 等待关闭 |

### `xtlsdialstate`

Dial 状态区分名称解析、TCP 连接和 TLS 握手三个可取消阶段。

```c
typedef enum xtlsdialstate {
	XTLS_DIAL_RESOLVING = 0,
	XTLS_DIAL_CONNECTING,
	XTLS_DIAL_HANDSHAKE,
	XTLS_DIAL_CONNECTED,
	XTLS_DIAL_FAILED,
	XTLS_DIAL_CANCELLED
} xtlsdialstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_DIAL_RESOLVING` | 解析中 |
| `XTLS_DIAL_CONNECTING` | 连接中 |
| `XTLS_DIAL_HANDSHAKE` | 握手阶段 |
| `XTLS_DIAL_CONNECTED` | 已连接 |
| `XTLS_DIAL_FAILED` | 已失败 |
| `XTLS_DIAL_CANCELLED` | 已取消 |

### `xtlsdialconfig`

Timeout 覆盖 DNS、TCP 和 TLS 全过程；零值只保留各阶段超时。

```c
typedef struct xtlsdialconfig {
	xnetdialconfig Transport;
	xtlsstreamconfig Stream;
	uint64 Timeout;
	bool ServerNameFromHost;
} xtlsdialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Transport` | `xnetdialconfig` | Transport |
| `Stream` | `xtlsstreamconfig` | 流选择 |
| `Timeout` | `uint64` | 超时（微秒） |
| `ServerNameFromHost` | `bool` | ServerNameFromHost |

### `xtlsstreamevents`

全部回调都在底层 TCP Stream 所属 Worker 上串行执行。

```c
typedef struct xtlsstreamevents {
	void (*Open)(xtlsstream* pStream, ptr pData);
	void (*Read)(xtlsstream* pStream,
		const xnetbuf* pBuffer, ptr pData);
	void (*End)(xtlsstream* pStream, ptr pData);
	void (*Writable)(xtlsstream* pStream, ptr pData);
	void (*Drain)(xtlsstream* pStream, ptr pData);
	void (*Close)(xtlsstream* pStream, xnetresult Result,
		const xerror* pError, ptr pData);
	/*
		客户端恢复队列新增票据时发布边沿；未启用恢复实现时不会调用。
		回调使用 xrtTlsClientTakeResume 接管一张或全部票据。
	*/
	void (*Ticket)(xtlsstream* pStream, ptr pData);
} xtlsstreamevents;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Open` | `回调` | 流开放（握手完成） |
| `Read` | `回调` | 收到解密缓冲 |
| `End` | `回调` | 对端写关闭 |
| `Writable` | `回调` | 发送预算可用 |
| `Drain` | `回调` | 发送队列排空 |
| `Close` | `回调` | 流关闭（含错误） |

### `xtlslistenerstate`

Listener 只发布已经完成 TLS 握手的 Stream，关闭监听不会隐式关闭已发布连接。

```c
typedef enum xtlslistenerstate {
	XTLS_LISTENER_OPEN = 0,
	XTLS_LISTENER_CLOSING,
	XTLS_LISTENER_CLOSED
} xtlslistenerstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_LISTENER_OPEN` | 监听中 |
| `XTLS_LISTENER_CLOSING` | 关闭中 |
| `XTLS_LISTENER_CLOSED` | 已关闭 |

### `xtlslistenerevents`

Accept 在目标 Stream 的 Worker 上执行，返回 true 后接管一个 Stream 引用。 Error 只报告监听层错误；单连接握手失败通过 HandshakeError 独立报告。

```c
typedef struct xtlslistenerevents {
	bool (*Accept)(xtlslistener* pListener,
		xtlsstream* pStream, ptr pData);
	void (*HandshakeError)(xtlslistener* pListener,
		const xerror* pError, ptr pData);
	void (*Error)(xtlslistener* pListener,
		const xerror* pError, ptr pData);
	void (*Close)(xtlslistener* pListener, ptr pData);
} xtlslistenerevents;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Open` | `回调` | （无操作占位） |
| `Accept` | `回调` | 新 TLS 流就绪 |
| `Close` | `回调` | 监听器关闭 |

### `xtlslistenerconfig`

Listen 负责 TCP 接入，Tls 和 Stream 负责每条连接的 TLS 会话与组合层限制。 AcceptQueueLimit 只限制完成握手但尚未被 pull/Future 消费的连接； HandshakeLimit 在分配 TLS 会话前硬性限制并发握手数。 初始化默认完成队列 1024 条、并发握手 128 条，均可显式调整。

```c
typedef struct xtlslistenerconfig {
	xnetlistenconfig Listen;
	xtlsserverconfig Tls;
	xtlsstreamconfig Stream;
	uint32 AcceptQueueLimit;
	uint32 HandshakeLimit;
} xtlslistenerconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Listen` | `xnetlistenconfig` | Listen |
| `Tls` | `xtlsserverconfig` | Tls |
| `Stream` | `xtlsstreamconfig` | 流选择 |
| `AcceptQueueLimit` | `uint32` | AcceptQueueLimit |
| `HandshakeLimit` | `uint32` | HandshakeLimit |

### `xtlslistenerstats`

统计值均为并发快照，累计计数在关闭后仍可读取。

```c
typedef struct xtlslistenerstats {
	xtlslistenerstate State;
	uint64 Handshakes;
	uint64 Accepted;
	uint64 Rejected;
	uint64 HandshakeErrors;
	uint32 ActiveHandshakes;
	uint32 PeakHandshakes;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xtlslistenerstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xtlslistenerstate` | 状态 |
| `Handshakes` | `uint64` | Handshakes |
| `Accepted` | `uint64` | Accepted |
| `Rejected` | `uint64` | Rejected |
| `HandshakeErrors` | `uint64` | HandshakeErrors |
| `ActiveHandshakes` | `uint32` | ActiveHandshakes |
| `PeakHandshakes` | `uint32` | PeakHandshakes |
| `QueuedAccepts` | `uint32` | QueuedAccepts |
| `PeakQueuedAccepts` | `uint32` | PeakQueuedAccepts |
| `AcceptWaiters` | `uint32` | AcceptWaiters |

### `xtlsstream`

公开句柄声明不随 TLS Stream 实现裁剪变化。

```c
typedef struct xtlsstream xtlsstream;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlslistener`

TLS 监听器（不透明）：在 TCP 监听器上完成 TLS 接受。


TLS 监听器（不透明）：在 TCP 监听器上完成 TLS 接受，产出 TLS 组合流。


```c
typedef struct xtlslistener xtlslistener;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsdial`

托管 TLS 拨号对象（不透明）：串联 TCP 拨号与 TLS 握手。


托管 TLS 拨号对象（不透明）：串联 TCP 拨号与 TLS 握手，完成时经回调移交 TLS 流。


```c
typedef struct xtlsdial xtlsdial;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsdialproc`

成功回调接管 TLS Stream 引用；失败时 Stream 为空且 Error 只在回调期间借用。

```c
typedef void (*xtlsdialproc)(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT` | `UINT64_C(10000000)` | 握手阶段超时默认值 |
| `XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT` | `UINT64_C(5000000)` | CLOSE超时默认值 |
| `XTLS_STREAM_ASYNC_BYTES_DEFAULT` | `((size_t)1048576u)` | ASYNCBYTES默认值 |
| `XTLS_STREAM_ASYNC_COUNT_DEFAULT` | `UINT32_C(1024)` | ASYNC数量默认值 |
| `XTLS_STREAM_ASYNC_BATCH_DEFAULT` | `UINT32_C(64)` | ASYNCBATCH默认值 |

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
|---|---|---|
| `XFUTURE_PENDING` | 等待中 | — |
| `XFUTURE_RESOLVED` | 已完成 | — |
| `XFUTURE_FAILED` | 已失败 | — |
| `XFUTURE_CANCELLED` | 已取消 | — |
| `XFUTURE_CLOSED` | 已关闭 | — |

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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
| `XWAIT_OK` | 已终态 | — |
| `XWAIT_ERROR` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future · 基础](../../examples/concurrency/future/main.c) · 观察

```c
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
```



### `xrtFutureWaitFor`

在相对微秒数内等待终态。

```c
xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `iTimeout` | 输入 | 微秒 | 相对时限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 已终态 | — |
| `XWAIT_TIMEOUT` | 到期仍待定（不设错） | — |
| `XWAIT_ERROR` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · 延续核对](../../examples/concurrency/future_tour/main.c) · 观察

```c
				(xrtFutureWaitFor(pCatch,
					EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
```


### `xrtFutureWaitUntil`

等待到指定单调时钟截止时间。

```c
xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` / `XWAIT_ERROR` | 同 `WaitFor` 口径 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · 等待族](../../examples/concurrency/future_tour/main.c) · 观察

```c
		(xrtFutureWaitUntil(pFut1,
			xrtDeadlineAfter(100000u)) != XWAIT_TIMEOUT) ) {
```


### `xrtFutureWaitUntilCancel`

等待首个线性化事件；取消先取得等待锁后不会被迟到终态覆盖。

```c
xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFuture` | 输入 | 非空 | 目标 Future |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` / `XWAIT_TIMEOUT` | 终态/到期 | — |
| `XWAIT_CANCELLED` | 令牌触发（优先于迟到终态） | 不设错 |
| `XWAIT_ERROR` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/future_tour · 等待族](../../examples/concurrency/future_tour/main.c) · 观察

```c
		(xrtFutureWaitUntilCancel(pFut1,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
			pCancel) != XWAIT_CANCELLED) ) {
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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


## Future 桥

桥解决"异步操作先返回 Future、结果稍后才到"的装配竞态：`Ready` 之前的终态写入被挂起，`Ready`/`Fail` 决定放行或回收。

### `xrtFutureBridgeInit`

使用一个已有 Promise 初始化桥；Promise 的所有权仍由调用方持有。桥解决"异步操作先返回 Future、结果稍后才到"的装配竞态。

```c
bool xrtFutureBridgeInit(
	xfuturebridge* pBridge,
	xpromise* pPromise
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输出 | 非空、32 字节固定存储 | 可嵌入异步操作上下文 |
| `pPromise` | 输入 | 非空、借用 | 调用方持有的 Promise |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 桥已就绪 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/bridge_tour · Init](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
		!xrtFutureBridgeInit(&Bridge2, pPromise2) ||
```


### `xrtFutureBridgeCreate`

创建 Future/Promise 对并初始化桥；返回的 Future 由调用方持有。

```c
xfuture* xrtFutureBridgeCreate(
	xfuturebridge* pBridge,
	xcancel* pParent
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输出 | 非空 | 桥存储 |
| `pParent` | 输入 | 允许空 | 父取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 消费端 Future | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[concurrency/bridge_tour · Create](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
	pFuture = xrtFutureBridgeCreate(&Bridge, NULL);
```


### `xrtFutureBridgePromise`

返回桥借用的 Promise；调用方负责按原有所有权契约销毁它。

```c
xpromise* xrtFutureBridgePromise(
	const xfuturebridge* pBridge
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输入 | 非空 | 桥 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用的 Promise（不增引用） | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[concurrency/bridge_tour · Create](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
		((pBorrowed = xrtFutureBridgePromise(&Bridge)) == NULL) ) {
```


### `xrtFutureBridgeWatch`

把 Future 的协作取消转发给底层异步操作。

```c
bool xrtFutureBridgeWatch(
	xfuturebridge* pBridge,
	xcancelproc pCancelProc,
	ptr pCancelData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输入/输出 | 非空 | 桥 |
| `pCancelProc` | 输入 | 非空 | 取消回调 |
| `pCancelData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 监听已建立 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 未初始化或已注销

#### 范例

[concurrency/bridge_tour · Watch](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
	if ( !xrtFutureBridgeWatch(&Bridge, exampleCancel,
			(ptr)&bCancelled) ||
```


### `xrtFutureBridgeReady`

发布装配成功，允许底层完成回调向 Promise 写入终态。

```c
bool xrtFutureBridgeReady(xfuturebridge* pBridge);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输入/输出 | 非空 | 桥 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | Ready 之前的挂起终态写入被放行 | — |
| `false` | 状态非法（已发布） | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 已发布过 Ready/Fail

#### 范例

[concurrency/bridge_tour · Watch](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
		!xrtFutureBridgeReady(&Bridge) ||
```


### `xrtFutureBridgeFail`

发布装配失败，要求底层完成回调只回收结果。

```c
bool xrtFutureBridgeFail(xfuturebridge* pBridge);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输入/输出 | 非空 | 桥 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 后续终态写入将被拒收 | — |
| `false` | 状态非法（已发布） | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 已发布过 Ready/Fail
- 注：Promise 归调用方所有，直接向 Promise 写终态仍合法

#### 范例

[concurrency/bridge_tour · Fail](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
		!xrtFutureBridgeFail(&Bridge2) ||
		xrtFutureBridgeWait(&Bridge2) ) {
```


### `xrtFutureBridgeWait`

等待极短的装配窗口，并返回底层结果能否写入 Promise。

```c
bool xrtFutureBridgeWait(const xfuturebridge* pBridge);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输入 | 非空 | 桥 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已 Ready：结果可写入 Promise | — |
| `false` | 已 Fail：只回收结果（不设错，是查询结果） | — |

#### 错误

- 无 — 返回值即答案

#### 范例

[concurrency/bridge_tour · Watch](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
		!xrtFutureBridgeWait(&Bridge) ||
```


### `xrtFutureBridgeUnwatch`

注销取消监听，并与正在执行的取消回调汇合。

```c
void xrtFutureBridgeUnwatch(xfuturebridge* pBridge);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBridge` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 返回后取消回调不再触发 | — |

#### 错误

- 无 — 注销不失败

#### 范例

[concurrency/bridge_tour · 收尾](../../examples/concurrency/bridge_tour/main.c) · 观察

```c
	xrtFutureBridgeUnwatch(&Bridge);
```



## TLS 监听器

监听器把 TCP 绑定与异步接入组合为单入口；Accept 三形态覆盖 pull（非阻塞/Future/阻塞）消费模型。

### `xrtTlsListenerConfigInit`

初始化单 IPv4 动态端口、有界握手与有界完成队列的默认配置。

```c
void xrtTlsListenerConfigInit(xtlslistenerconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[tls/listener_tour · 监听器](../../examples/tls/listener_tour/main.c) · 观察

```c
	xrtTlsListenerConfigInit(&ListenerConfig);
```


### `xrtTlsListenerStart`

同步完成 TCP 绑定并开始异步接入。Context/Identity 被 Listener 保留；ALPN 列表深复制；SelectContext 与 ResumeContext 由调用方持有到关闭回调结束。

```c
xtlslistener* xrtTlsListenerStart(
	xnetengine* pEngine,
	const xtlslistenerconfig* pConfig,
	const xtlslistenerevents* pEvents,
	const xtlsstreamevents* pStreamEvents,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空、运行中 | 网络引擎 |
| `pConfig` | 输入 | 非空 | 监听配置 |
| `pEvents` | 输入 | 允许空 | 监听器事件回调 |
| `pStreamEvents` | 输入 | 允许空 | 每连接流事件 |
| `pData` | 输入 | 任意值 | 用户数据（`ListenerData` 取回） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 监听器（`OPEN` 状态） | — |
| `NULL` | 绑定或启动失败 | `xrt.tls` 域错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.tls` 域错误 — TCP 绑定失败（cause 链保留底层）
- `XERR_ARGUMENT` — 参数非法

#### 范例

[tls/listener_tour · 监听器](../../examples/tls/listener_tour/main.c) · 观察

```c
	pListener = xrtTlsListenerStart(pEngine, &ListenerConfig, NULL,
		NULL, &EngineConfig);
```


### `xrtTlsListenerRef`

增加 Listener 引用并返回原指针。

```c
xtlslistener* xrtTlsListenerRef(xtlslistener* pListener);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 引用](../../examples/tls/listener_tour/main.c) · 观察

```c
	pListenerRef = xrtTlsListenerRef(pListener);
```


### `xrtTlsListenerDestroy`

释放 Listener 引用；不会隐式关闭仍在监听的对象。

```c
void xrtTlsListenerDestroy(xtlslistener* pListener);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败；仍监听时需先 `Close`

#### 范例

[tls/listener_tour · 收尾](../../examples/tls/listener_tour/main.c) · 观察

```c
	xrtTlsListenerDestroy(pListenerRef);
	xrtTlsListenerDestroy(pListener);
```


### `xrtTlsListenerAccept`

pull 模式下非阻塞取得一个已完成握手的 Stream；空队列返回空指针。

```c
xtlsstream* xrtTlsListenerAccept(xtlslistener* pListener);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已完成握手的 Stream（引用转移给调用方） | — |
| `NULL` | 队列空（正常结果）或失败 | 空队列不设错 |

#### 错误

- 无 — 空队列是正常结果

#### 范例

[tls/listener_tour · Accept 三态](../../examples/tls/listener_tour/main.c) · 观察

```c
		pServerA = xrtTlsListenerAccept(pListener);
```


### `xrtTlsListenerAcceptAsync`

pull 模式下异步接受一个已完成握手的 Stream；Future 持有结果引用。

```c
xfuture* xrtTlsListenerAcceptAsync(xtlslistener* pListener);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future；成功值为 Stream 引用 | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误

#### 范例

[tls/listener_tour · Accept 三态](../../examples/tls/listener_tour/main.c) · 观察

```c
	pAcceptFuture = xrtTlsListenerAcceptAsync(pListener);
```


### `xrtTlsListenerAcceptWait`

阻塞接受一个已完成握手的 Stream；禁止从该 Engine 的 Worker 调用。

```c
xtlsstream* xrtTlsListenerAcceptWait(
	xtlslistener* pListener,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器 |
| `iDeadline` | 输入 | 单调时钟 | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已完成握手的 Stream | — |
| `NULL` | 到期/取消/失败 | 到期与取消不设错 |

#### 错误

- `XERR_STATE` — 从所属 Engine Worker 调用（自等待死锁）
- 无 — 到期/取消返回空是等待结果

#### 范例

[tls/listener_tour · Accept 三态](../../examples/tls/listener_tour/main.c) · 观察

```c
	pServerC = xrtTlsListenerAcceptWait(pListener,
		xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
```


### `xrtTlsListenerClose`

原子停止接入并丢弃尚未交付的连接；已交付连接保持独立生命周期。

```c
bool xrtTlsListenerClose(xtlslistener* pListener);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器；幂等 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已停止接入 | — |
| `false` | 参数非法或已关闭 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 已 CLOSED（幂等失败口径）

#### 范例

[tls/listener_tour · 收尾](../../examples/tls/listener_tour/main.c) · 观察

```c
		(void)xrtTlsListenerClose(pListener);
		while ( xrtTlsListenerState(pListener) !=
			XTLS_LISTENER_CLOSED ) {
```


### `xrtTlsListenerState`

返回 Listener 当前生命周期状态。

```c
xtlslistenerstate xrtTlsListenerState(
	const xtlslistener* pListener
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 允许空 | 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_LISTENER_OPEN/CLOSED/...` | 状态枚举 | — |
| 零值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 监听器](../../examples/tls/listener_tour/main.c) · 观察

```c
		(xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN) ||
```


### `xrtTlsListenerLocal`

复制监听 Socket 的实际本地地址，支持动态端口。

```c
bool xrtTlsListenerLocal(
	xtlslistener* pListener,
	xnetaddr* pAddress
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器 |
| `pAddress` | 输出 | 非空 | 接收本地地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已写出 | — |
| `false` | 参数或系统错误 | `XERR_ARGUMENT` / 系统错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.tls` 域）

#### 范例

[tls/listener_tour · 监听器](../../examples/tls/listener_tour/main.c) · 观察

```c
		!xrtTlsListenerLocal(pListener, &Address) ||
```


### `xrtTlsListenerData`

返回创建时保存的用户数据快照。

```c
ptr xrtTlsListenerData(const xtlslistener* pListener);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 允许空 | 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 任意值 | 创建时传入的 `pData` | — |
| `NULL` | 未设置或参数非法 | — |

#### 错误

- 无 — 数据查询

#### 范例

[tls/listener_tour · 监听器](../../examples/tls/listener_tour/main.c) · 观察

```c
		(xrtTlsListenerData(pListener) != &EngineConfig) ) {
```


### `xrtTlsListenerStats`

复制 Listener 的并发统计快照。

```c
bool xrtTlsListenerStats(
	const xtlslistener* pListener,
	xtlslistenerstats* pStats
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 监听器 |
| `pStats` | 输出 | 非空 | 接收 Accepted/Handshakes 等统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 统计](../../examples/tls/listener_tour/main.c) · 观察

```c
	if ( !xrtTlsListenerStats(pListener, &ListenerStats) ||
		(ListenerStats.Accepted < 3u) ||
```



## TLS 拨号

拨号把 DNS 解析、TCP 竞争连接与 TLS 握手组合为一次异步操作，提供回调式与 Future 式两个入口。

### `xrtTlsDialConfigInit`

初始化 TCP 拨号、TLS Stream 和总超时策略的默认配置。

```c
void xrtTlsDialConfigInit(xtlsdialconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[tls/dial · 配置](../../examples/tls/dial/main.c) · 观察

```c
	xrtTlsDialConfigInit(&DialConfig);
```


### `xrtTlsDial`

解析主机、竞争 TCP 地址并完成 TLS 握手；成功 Stream 引用转移给完成回调。

```c
xtlsdial* xrtTlsDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData,
	xtlsdialproc pDone,
	ptr pDoneData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络引擎 |
| `pResolver` | 输入 | 非空 | DNS 解析器 |
| `sHost` | 输入 | 非空 | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pTls` | 输入 | 非空 | 客户端 TLS 配置 |
| `pConfig` | 输入 | 允许空 | 拨号配置（超时等） |
| `pStreamEvents` | 输入 | 允许空 | 流事件 |
| `pStreamData` | 输入 | 任意值 | 流用户数据 |
| `pDone` | 输入 | 非空 | 完成回调 |
| `pDoneData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Dial 对象（终态后 Destroy 释放） | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误 — 提交失败

#### 范例

[tls/dial · 回调式](../../examples/tls/dial/main.c) · 观察

```c
	pDial = xrtTlsDial(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
```


### `xrtTlsDialAsync`

以 Future 接收完成握手的 TLS Stream；`Open` 先于成功终态发布。Future 持有一个 Stream 引用，取消请求协作终止当前阶段。

```c
xfuture* xrtTlsDialAsync(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络引擎 |
| `pResolver` | 输入 | 非空 | 解析器 |
| `sHost` | 输入 | 非空 | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pTls` | 输入 | 非空 | TLS 配置 |
| `pConfig` | 输入 | 允许空 | 拨号配置 |
| `pStreamEvents` | 输入 | 允许空 | 流事件 |
| `pStreamData` | 输入 | 任意值 | 流用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future；成功值为 Stream 引用，失败值为 `xrt.tls` 错误链 | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误 — Future 失败值或提交失败

#### 范例

[tls/dial_future · Future 化](../../examples/tls/dial_future/main.c) · 观察

```c
	pFuture = xrtTlsDialAsync(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
		NULL,
		NULL
	);
```


### `xrtTlsDialRef`

增加 TLS Dial 引用并返回原指针。

```c
xtlsdial* xrtTlsDialRef(xtlsdial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 拨号自省](../../examples/tls/listener_tour/main.c) · 观察

```c
		(xrtTlsDialRef(pDial) != pDial) ) {
```


### `xrtTlsDialDestroy`

释放 TLS Dial 引用；空指针视为空操作。

```c
void xrtTlsDialDestroy(xtlsdial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tls/dial · 收尾](../../examples/tls/dial/main.c) · 观察

```c
	xrtTlsDialDestroy(pDial);
```


### `xrtTlsDialCancel`

原子受理取消；返回真保证最终结果不会再变为成功。

```c
bool xrtTlsDialCancel(xtlsdial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 取消已受理；最终结果必为 CANCELLED/FAILED | — |
| `false` | 已终态（成功无法撤回）或参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 在途取消](../../examples/tls/listener_tour/main.c) · 观察

```c
			bool bCancelled = xrtTlsDialCancel(pMidair);
```


### `xrtTlsDialState`

返回当前拨号阶段或不可变终态。

```c
xtlsdialstate xrtTlsDialState(const xtlsdial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 允许空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 阶段/终态枚举 | 解析/连接/握手或终态 | — |
| 零值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 拨号自省](../../examples/tls/listener_tour/main.c) · 观察

```c
	pSlot->DialState = xrtTlsDialState(pDial);
```


### `xrtTlsDialError`

失败或取消后借用完整错误原因链。

```c
const xerror* xrtTlsDialError(const xtlsdial* pDial);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 允许空 | Dial 对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误借用（含 DNS/TCP/TLS 分层 cause 链） | — |
| `NULL` | 未失败或参数非法 | 纯查询 |

#### 错误

- 无 — 非失败状态返回空是查询结果

#### 范例

[tls/listener_tour · 拨号自省](../../examples/tls/listener_tour/main.c) · 观察

```c
		xrtTlsDialError(pDial) == NULL ? "(none)" : "err");
```


### `xrtTlsDialTransportStats`

取得底层 TCP Dial 统计；TLS 握手阶段仍保留获胜地址信息。

```c
bool xrtTlsDialTransportStats(
	const xtlsdial* pDial,
	xnetdialstats* pStats
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | Dial 对象 |
| `pStats` | 输出 | 非空 | 接收拨号统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 统计已写出 | — |
| `false` | 参数非法或尚未开始 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/listener_tour · 拨号自省](../../examples/tls/listener_tour/main.c) · 观察

```c
	if ( !xrtTlsDialTransportStats(pDial, &TransportStats) ||
```



## TLS Stream

TLS Stream 组合 TCP 传输与 TLS 会话为单一明文接口：Worker 专用同步收发（Send/Buffer/Pullup/Read/Consume）与任意线程 Future 收发（SendAsync/RecvAsync/WaitAsync）双模型。

### 构造

### `xrtTlsStreamConfigInit`

初始化握手与认证关闭超时的默认配置。

```c
void xrtTlsStreamConfigInit(xtlsstreamconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[tls/stream · 配置](../../examples/tls/stream/main.c) · 观察

```c
	xrtTlsStreamConfigInit(&Example.StreamConfig);
```


### `xrtTlsStreamConnect`

创建 TLS 客户端并异步连接数字 TCP 地址（免 DNS，适用于直连/测试）。

```c
xtlsstream* xrtTlsStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络引擎 |
| `pRemote` | 输入 | 非空 | 数字地址 |
| `iAffinity` | 输入 | — | Worker 亲和键 |
| `pTransport` | 输入 | 允许空 | TCP 配置 |
| `pTls` | 输入 | 非空 | 客户端 TLS 配置 |
| `pConfig` | 输入 | 允许空 | 流配置 |
| `pEvents` | 输入 | 非空 | 流事件 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | TLS Stream（握手异步完成，`Open` 事件通知） | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误 — 提交失败

#### 范例

[tls/listener_tour · 直连](../../examples/tls/listener_tour/main.c) · 观察

```c
	pClientB = xrtTlsStreamConnect(pEngine, &Address, 0, NULL,
		&ClientConfigB, NULL, &ClientEvents, &ClientB);
```


### `xrtTlsStreamAttach`

在已公开的 TCP Stream 所属 Worker 上接管 Transport 和 Session；失败时所有权与事件均保持不变、输出清空。

```c
bool xrtTlsStreamAttach(
	xnetstream* pTransport,
	xtlssession* pSession,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 双向可用 | 成功后调用方引用被接管 |
| `pSession` | 输入 | 非空 | 已就绪会话 |
| `pConfig` | 输入 | 允许空 | 流配置 |
| `pEvents` | 输入 | 非空 | 流事件 |
| `pData` | 输入 | 任意值 | 用户数据 |
| `ppStream` | 输出 | 非空、独立 | 接收 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管两者引用 | — |
| `false` | 参数/状态非法 | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · 会话接管](../../examples/tls/stream_tour/main.c) · 观察

```c
		pTask->bOk = xrtTlsStreamAttach(pTask->pTcp,
			pTask->pSession, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
```


### `xrtTlsStreamClient`

在已连接 TCP Stream 上创建 TLS 客户端；适用于代理隧道、STARTTLS 和自定义拨号，成功时接管 Transport 引用。

```c
bool xrtTlsStreamClient(
	xnetstream* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 已连接 | 成功后接管引用 |
| `pTls` | 输入 | 非空 | 客户端 TLS 配置 |
| `pConfig` | 输入 | 允许空 | 流配置 |
| `pEvents` | 输入 | 非空 | 流事件 |
| `pData` | 输入 | 任意值 | 用户数据 |
| `ppStream` | 输出 | 非空、独立 | 接收 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管 | — |
| `false` | 参数/状态非法 | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · STARTTLS](../../examples/tls/stream_tour/main.c) · 观察

```c
		pTask->bOk = xrtTlsStreamClient(pTask->pTcp,
```


### `xrtTlsStreamAccept`

在 TCP Accept 回调内接管 Stream；返回值应直接作为该回调结果。

```c
bool xrtTlsStreamAccept(
	xnetstream* pTransport,
	const xtlsserverconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 刚被接受 | 成功后接管引用 |
| `pTls` | 输入 | 非空 | 服务端 TLS 配置 |
| `pConfig` | 输入 | 允许空 | 流配置 |
| `pEvents` | 输入 | 非空 | 流事件 |
| `pData` | 输入 | 任意值 | 用户数据 |
| `ppStream` | 输出 | 非空、独立 | 接收 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管（作为回调结果放行） | — |
| `false` | 参数/状态非法 | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream · 服务端](../../examples/tls/stream/main.c) · 观察

```c
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pExample->ServerConfig,
		&pExample->StreamConfig,
		&pExample->StreamEvents,
```


### 生命周期与查询

### `xrtTlsStreamRef`

增加 TLS Stream 引用并返回原指针；引用耗尽时返回空并设置状态错误。

```c
xtlsstream* xrtTlsStreamRef(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法或引用耗尽 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/dial_future · Future 值引用](../../examples/tls/dial_future/main.c) · 观察

```c
	pStream = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
```


### `xrtTlsStreamDestroy`

释放 TLS Stream 引用；关闭必须另行请求。

```c
void xrtTlsStreamDestroy(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tls/stream · 收尾](../../examples/tls/stream/main.c) · 观察

```c
		xrtTlsStreamDestroy(pStream);
```


### `xrtTlsStreamSetEvents`

在所属 Worker 上替换已打开 Stream 的事件与用户数据；不自动重放当前明文缓冲。

```c
bool xrtTlsStreamSetEvents(
	xtlsstream* pStream,
	const xtlsstreamevents* pEvents,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pEvents` | 输入 | 非空 | 新事件表 |
| `pData` | 输入 | 任意值 | 新用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换 | — |
| `false` | 参数/状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · 协议升级](../../examples/tls/stream_tour/main.c) · 观察

```c
	pTask->bOk = xrtTlsStreamSetEvents(pTask->pStream,
```


### `xrtTlsStreamState`

返回组合 Stream 状态的并发快照。

```c
xtlsstreamstate xrtTlsStreamState(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举 | 连接/握手/开放/关闭等阶段 | — |
| 零值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream_tour · 状态](../../examples/tls/stream_tour/main.c) · 观察

```c
					xrtTlsStreamState(Streams[i]);
```


### `xrtTlsStreamTransport`

借用底层 TCP Stream，调用方不得改变其 IO 状态机。

```c
xnetstream* xrtTlsStreamTransport(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 底层 TCP Stream 借用 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream · 传输层](../../examples/tls/stream/main.c) · 观察

```c
		xrtTlsStreamTransport(pStream),
```


### `xrtTlsStreamSession`

在所属 Worker 上借用协议会话，供 ALPN、票据等高级查询。

```c
xtlssession* xrtTlsStreamSession(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 会话借用（Worker 内） | — |
| `NULL` | 无会话或参数非法 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream_tour · 自省](../../examples/tls/stream_tour/main.c) · 观察

```c
	pClient->bSessionOk = xrtTlsStreamSession(pStream) != NULL;
```


### `xrtTlsStreamData`

返回线程安全的用户数据指针快照，不延长目标生命周期。

```c
ptr xrtTlsStreamData(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 任意值 | 创建时/Switch 后的 `pData` | — |
| `NULL` | 未设置或参数非法 | — |

#### 错误

- 无 — 数据查询

#### 范例

[tls/stream_tour · 自省](../../examples/tls/stream_tour/main.c) · 观察

```c
	pClient->bDataOk = xrtTlsStreamData(pStream) == pClient;
```


### `xrtTlsStreamError`

终态失败时借用保存的 TLS 或传输根因。

```c
const xerror* xrtTlsStreamError(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 根因错误借用 | — |
| `NULL` | 无失败或参数非法 | 纯查询 |

#### 错误

- 无 — 非失败状态返回空是查询结果

#### 范例

[tls/stream_tour · 失败输出](../../examples/tls/stream_tour/main.c) · 观察

```c
		xrtTlsStreamError(pClientA) == NULL ? "(none)"
```


### `xrtTlsStreamPending`

返回 TLS 密文暂存与底层 TCP 队列的总待发字节并发快照。

```c
size_t xrtTlsStreamPending(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 两级队列总待发字节 | — |
| `0` | 无待发或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream_tour · 排空等待](../../examples/tls/stream_tour/main.c) · 观察

```c
	while ( xrtTlsStreamPending(pClientB) != 0u ) {
```


### Worker 专用收发

### `xrtTlsStreamSend`

在所属 Worker 上把明文编码为记录；允许成功短写。

```c
xtlsresult xrtTlsStreamSend(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize,
	size_t* pWritten
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pData` | 输入 | 借用 | 明文 |
| `iSize` | 输入 | — | 字节数 |
| `pWritten` | 输出 | 可空 | 实际受理量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 需要更多输入 | — |
| `XTLS_CLOSED` | 会话已关闭 | — |
| `XTLS_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · Worker 收发](../../examples/tls/stream_tour/main.c) · 观察

```c
		pTask->bOk = (xrtTlsStreamSend(pTask->pStream, arrRequest,
```


### `xrtTlsStreamSendVec`

在所属 Worker 上依次编码明文片段；返回跨片段的连续受理前缀。

```c
xtlsresult xrtTlsStreamSendVec(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pWritten
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pSpans` | 输入 | 借用 | 明文片段数组 |
| `iCount` | 输入 | — | 片段数 |
| `pWritten` | 输出 | 可空 | 连续前缀量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 需要更多输入 | — |
| `XTLS_CLOSED` | 会话已关闭 | — |
| `XTLS_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · Worker 收发](../../examples/tls/stream_tour/main.c) · 观察

```c
	(void)xrtTlsStreamSendVec(pStream, Vec, 2, &iWritten);
```


### `xrtTlsStreamSendBound`

在所属 Worker 上返回一次明文发送的精确密文线路字节数（含记录头/nonce/标签）；失败不修改 `pBound`。

```c
bool xrtTlsStreamSendBound(
	xtlsstream* pStream,
	size_t iPlainSize,
	size_t* pBound
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iPlainSize` | 输入 | — | 明文字节数 |
| `pBound` | 输出 | 非空、不得与 Stream/Session 重叠 | 接收密文上界 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 上界已写出 | — |
| `false` | 参数/状态非法 | `*pBound` 不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · 密文上界](../../examples/tls/stream_tour/main.c) · 观察

```c
	(void)xrtTlsStreamSendBound(pStream, 64u, &pClient->iBound);
```


### `xrtTlsStreamAvailable`

返回当前待应用消费明文字节数的并发快照。

```c
size_t xrtTlsStreamAvailable(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 待消费明文字节 | — |
| `0` | 无明文或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream_tour · 明文循环](../../examples/tls/stream_tour/main.c) · 观察

```c
	while ( xrtTlsStreamAvailable(pStream) >= 3u ) {
```


### `xrtTlsStreamBuffer`

在所属 Worker 上借用明文块链，借用期不超过本次回调；默认暂停底层读取。

```c
const xnetbuf* xrtTlsStreamBuffer(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 明文链借用（回调期间有效） | — |
| `NULL` | 无明文或参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream · 明文消费](../../examples/tls/stream/main.c) · 观察

```c
		const xnetbuf* pBuffer = xrtTlsStreamBuffer(pStream);
```


### `xrtTlsStreamPullup`

在所属 Worker 上把精确明文前缀按需连续化并返回借用视图；不消费明文，零长度和越界请求失败。

```c
bool xrtTlsStreamPullup(
	xtlsstream* pStream,
	size_t iSize,
	xnetspan* pSpan
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iSize` | 输入 | `> 0` 且 `<= Available` | 请求前缀长度 |
| `pSpan` | 输出 | 非空 | 接收连续借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 视图已写出（下次缓冲修改前有效） | — |
| `false` | 零长度/越界或状态非法 | `XERR_RANGE` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法
- `XERR_RANGE` — 零长度或超过可用量

#### 范例

[tls/stream_tour · 协议解析](../../examples/tls/stream_tour/main.c) · 观察

```c
		if ( !xrtTlsStreamPullup(pStream, 3u, &Span) ||
```


### `xrtTlsStreamReadMore`

在 Read 回调保留现有明文时请求继续解密；累积受 PlainLimit 硬约束，重复请求幂等。

```c
bool xrtTlsStreamReadMore(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已请求（明文增长后再次发布 Read） | — |
| `false` | 无 Read 回调保留或超限 | `XERR_STATE` / `XERR_RANGE` |

#### 错误

- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法
- `XERR_RANGE` — 累积将超过 PlainLimit

#### 范例

[tls/stream_tour · 增量解析](../../examples/tls/stream_tour/main.c) · 观察

```c
	pClient->bReadMore = xrtTlsStreamReadMore(pStream);
```


### `xrtTlsStreamRead`

在所属 Worker 上复制并安全消费明文。

```c
xtlsresult xrtTlsStreamRead(
	xtlsstream* pStream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pOutput` | 输出 | 非空 | 接收缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pRead` | 输出 | 可空 | 实际复制量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 需要更多输入 | — |
| `XTLS_CLOSED` | 会话已关闭 | — |
| `XTLS_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · Worker 收发](../../examples/tls/stream_tour/main.c) · 观察

```c
	(void)xrtTlsStreamRead(pStream, pClient->ReadOut, 4u,
```


### `xrtTlsStreamConsume`

在所属 Worker 上安全消费精确数量的明文。

```c
bool xrtTlsStreamConsume(xtlsstream* pStream, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iSize` | 输入 | `<= Available` | 消费字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已消费并恢复底层读取 | — |
| `false` | 越界或状态非法 | `XERR_RANGE` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法
- `XERR_RANGE` — 超过可用明文

#### 范例

[tls/stream_tour · 协议解析](../../examples/tls/stream_tour/main.c) · 观察

```c
		if ( !xrtTlsStreamConsume(pStream, 1u) ) {
```


### 关闭

### `xrtTlsStreamClose`

从任意线程请求 close_notify、等待对端认证关闭并排空 TCP；已接纳的异步发送先按 FIFO 完成。

```c
bool xrtTlsStreamClose(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream；幂等 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 关闭流程已发起 | — |
| `false` | 参数/状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · 关闭](../../examples/tls/stream_tour/main.c) · 观察

```c
				(void)xrtTlsStreamClose(Streams[i]);
```


### `xrtTlsStreamAbort`

立即中止：不发送 close_notify，直接丢弃在途状态并触发底层 TCP 复位语义。

```c
bool xrtTlsStreamAbort(xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 中止已发起 | — |
| `false` | 参数/状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 不在所属 Worker 上（Worker 专用操作）或状态非法

#### 范例

[tls/stream_tour · 错误路径](../../examples/tls/stream_tour/main.c) · 观察

```c
			(void)xrtTlsStreamAbort(pStream);
```


### 异步观测与 Future 收发

### `xrtTlsStreamAsyncBytes`

返回尚未由所属 Worker 终结的异步发送负载字节数。

```c
size_t xrtTlsStreamAsyncBytes(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 在途异步发送负载字节 | — |
| `0` | 无在途或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream_tour · 异步观测](../../examples/tls/stream_tour/main.c) · 观察

```c
	(void)xrtTlsStreamAsyncBytes(pClientA);
```


### `xrtTlsStreamAsyncCount`

返回异步发送、接收和条件等待的合计操作数。

```c
uint32 xrtTlsStreamAsyncCount(const xtlsstream* pStream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 在途异步操作合计 | — |
| `0` | 无在途或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls/stream_tour · 异步观测](../../examples/tls/stream_tour/main.c) · 观察

```c
	(void)xrtTlsStreamAsyncCount(pClientA);
```


### `xrtTlsStreamWaitAsync`

建立 OPEN/READ/WRITE/DRAIN/END/CLOSE 条件 Future；取消只移除本次等待。

```c
xfuture* xrtTlsStreamWaitAsync(
	xtlsstream* pStream,
	xtlsstreamwait Wait
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `Wait` | 输入 | 枚举 | 条件类型（无值 Future） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 条件 Future | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误 — 条件非法或提交失败

#### 范例

[tls/stream_future · 排空](../../examples/tls/stream_future/main.c) · 观察

```c
	return exampleTlsFutureResolved(xrtTlsStreamWaitAsync(
		pStream,
		XTLS_STREAM_WAIT_DRAIN
	));
```


### `xrtTlsStreamRecvAsync`

在拉取模式下复制并消费当前可用明文；成功值是由 Future 持有的 `xnetbytes`。

```c
xfuture* xrtTlsStreamRecvAsync(
	xtlsstream* pStream,
	size_t iMaxBytes
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iMaxBytes` | 输入 | 零 = 全部当前明文 | 读取上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future；成功值为 `xnetbytes` | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误

#### 范例

[tls/stream_future · 拉取](../../examples/tls/stream_future/main.c) · 观察

```c
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pStream,
		64u * 1024u
	);
```


### `xrtTlsStreamSendAsync`

从任意线程复制并按 FIFO 提交一段完整明文；取消只在首个字节受理前有效，Close 线性化前已接纳的发送保证先完成。

```c
xfuture* xrtTlsStreamSendAsync(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pData` | 输入 | 借用、提交时复制 | 明文 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future（全部明文被会话受理时完成） | — |
| `NULL` | 提交失败 | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误 — 提交失败；Close 后新发送以 STATE 拒绝

#### 范例

[tls/stream_future · 发送](../../examples/tls/stream_future/main.c) · 观察

```c
	if ( !exampleTlsFutureResolved(xrtTlsStreamSendAsync(
		pStream,
		pData,
		iSize
	)) ) {
```


### `xrtTlsStreamSendVecAsync`

从任意线程复制片段并按 FIFO 提交为一段连续明文；全部片段在返回前完成校验和复制，失败不发布部分操作。

```c
xfuture* xrtTlsStreamSendVecAsync(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pSpans` | 输入 | 借用、提交时复制 | 片段数组 |
| `iCount` | 输入 | — | 片段数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future | — |
| `NULL` | 校验/提交失败（不发布部分操作） | `xrt.tls` 域错误 |

#### 错误

- `xrt.tls` 域错误

#### 范例

[tls/stream_tour · Future 式发送](../../examples/tls/stream_tour/main.c) · 观察

```c
	pSendFuture = xrtTlsStreamSendVecAsync(pClientB, AsyncVec, 2);
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
