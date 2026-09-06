# Channel

Channel 是建立在线程同步原语之上的指针消息通道。它覆盖精确容量的有缓冲
MPMC 通信和容量为零的同步 rendezvous；无锁 SPSC、MPSC、MPMC 队列仍由
`queue.h` 独立提供。


## 裁剪

| 宏 | 能力 | 依赖 |
|---|---|---|
| `XRT_FEATURE_CHANNEL` | 基础、deadline、关闭和 rendezvous | `XRT_FEATURE_COND` |
| `XRT_FEATURE_CHANNEL_CANCEL` | 可取消发送和接收 | `XRT_FEATURE_CHANNEL`、`XRT_FEATURE_CANCEL` |
| `XRT_FEATURE_CHANNEL_SELECT` | 多 Channel 原子选择 | `XRT_FEATURE_CHANNEL`、`XRT_FEATURE_ATOMIC`、`XRT_FEATURE_EVENT` |
| `XRT_FEATURE_CHANNEL_SELECT_CANCEL` | 可取消多路选择 | `XRT_FEATURE_CHANNEL_SELECT`、`XRT_FEATURE_CANCEL` |
| `XRT_FEATURE_CHANNEL_COROUTINE` | 不阻塞调度线程的发送、接收和多路等待 | `XRT_FEATURE_CHANNEL`、`XRT_FEATURE_ATOMIC`、`XRT_FEATURE_COROUTINE_SCHEDULER` |

旧版 `MPSCQWait` 不再保留。它的等待、超时、唤醒和关闭资产已合并到 Channel，
避免无锁队列、等待队列与 Channel 三套近义 API 长期并存。


## 容量与内存

`xrtChannelInit` 和 `xrtChannelCreate` 使用调用方指定的精确容量：

- 容量大于零时只分配 `capacity * sizeof(ptr)` 的消息环。
- 容量等于零时不分配消息缓冲，发送与接收执行同步 rendezvous。
- 容量 `1` 就只能保存一个值，不会向上取整。

`xrtChannelInitBuffer` 使用调用方提供的非空指针环，不分配消息区。该数组必须
按指针对齐、在 Channel 存活期间保持地址和长度不变，并且不能覆盖 Channel
对象。

`xchannel` 使用固定大小的不透明存储。内部状态不进入公共 ABI，预留空间可供
后续 `channel_select` 适配使用；每个对象不包含固定 8K 消息缓冲。

### `xrtChannelInit`

初始化精确容量的 Channel；容量为零时创建同步 rendezvous Channel。

```c
bool xrtChannelInit(xchannel* pChannel, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输出 | 非空、按自然对齐 | 内嵌形态的 Channel 存储 |
| `iCapacity` | 输入 | — | 精确容量；零表示无缓冲 rendezvous |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Channel 已就绪；`Unit` 释放 | — |
| `false` | 参数非法或容量溢出 | 结构清零；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pChannel` 为空或未对齐
- `XERR_RANGE` — 容量超出 `SIZE_MAX / sizeof(ptr)`
- `XERR_MEMORY` — 消息环分配失败

#### 范例

[concurrency/channel_tour · 协程族](../../examples/concurrency/channel_tour/main.c) · 容量 0 的同步 Channel

```c
if ( !xrtChannelInit(&tSilent, 0u) ) {
	goto Cleanup;
}
```

### `xrtChannelInitBuffer`

在调用方提供的精确容量指针环上初始化有缓冲 Channel。

```c
bool xrtChannelInitBuffer(
	xchannel* pChannel,
	ptr* pItems,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输出 | 非空、按自然对齐 | 内嵌形态的 Channel 存储 |
| `pItems` | 输入 | 非空、按指针对齐 | 调用方指针环；存活期覆盖 Channel |
| `iCapacity` | 输入 | `> 0` | 精确容量；与 `pItems` 数组长度一致 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Channel 已就绪，不分配消息区 | — |
| `false` | 参数非法 | 结构不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空/未对齐、容量为零、地址回绕，或指针环覆盖 Channel 对象

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 调用方槽数组配容量 2

```c
if ( !xrtChannelInitBuffer(&tEmbedded, arrSlots, 2u) ||
	(xrtChannelCapacity(&tEmbedded) != 2u) ) {
	goto Cleanup;
}
```

### `xrtChannelCreate`

创建精确容量的 Channel；容量为零时不分配消息缓冲。

```c
xchannel* xrtChannelCreate(size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCapacity` | 输入 | — | 精确容量；零表示无缓冲 rendezvous |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆分配的 Channel；`Destroy` 释放 | — |
| `NULL` | 参数非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 内部校验失败
- `XERR_RANGE` — 容量溢出
- `XERR_MEMORY` — Channel 或消息环分配失败

#### 范例

[concurrency/channel_tour · 阻塞族](../../examples/concurrency/channel_tour/main.c) · 容量 2 的堆通道

```c
pHeap = xrtChannelCreate(2u);
if ( (pHeap == NULL) ||
	(xrtChannelSend(pHeap, (ptr)1) != XWAIT_OK) ||
```

### `xrtChannelUnit`

释放 Channel 内部资源；仍有等待者或 rendezvous 消息时失败。

```c
bool xrtChannelUnit(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 内嵌形态（`Init`/`InitBuffer` 产物）；需调用方独占 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 内部资源已释放，对象可弃置或重新 `Init` | — |
| `false` | 参数非法或仍有等待者/挂起值 | 对象保留；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pChannel` 为空
- `XERR_STATE` — 结构非初始化状态，或仍有发送/接收等待者、rendezvous 挂起值或 Select 等待节点

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 自省段结束即释放

```c
printf("channel: buffer count=1/2 recv=try-ok reset=ok\n");
xrtChannelUnit(&tEmbedded);
```

### `xrtChannelDestroy`

释放 `Create` 返回的 Channel；`Unit` 失败时保留对象。

```c
bool xrtChannelDestroy(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 允许空 | 堆形态产物；空指针是空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 全部资源与结构已释放 | — |
| `false` | 同 `Unit` 的失败条件 | 对象保留，可重试；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 仍有等待者或挂起值

#### 范例

[concurrency/channel · 收尾](../../examples/concurrency/channel/main.c) · 排空接收后销毁

```c
return xrtChannelDestroy(pChannel) ? 0 : 3;
```

## 所有权

Channel 只保存指针位，不拥有指针目标，也不自动增加或减少引用。`NULL` 是合法
消息，必须通过返回结果和“没有消息”区分。发送失败时指针仍由调用方处理；发送
成功后如何转移目标所有权由上层协议决定。

所有接收输出必须按指针对齐，且不能覆盖 `xchannel` 对象或内部消息环。参数、
对齐或别名错误返回 `XCHANNEL_ERROR` / `XWAIT_ERROR`，设置 `XERR_ARGUMENT`，
不移动消息，也不写输出。

## 非阻塞操作

无缓冲 `TrySend` 只有在接收者已经等待时才成功。成功表示 rendezvous 已不可
撤销地配对；即使随后关闭或取消，等待中的接收者仍会取得该值。

### `xrtChannelTrySend`

非阻塞发送一个可为空的指针值。

```c
xchannelresult xrtChannelTrySend(xchannel* pChannel, ptr pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值；Channel 不拥有目标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XCHANNEL_OK` | 已提交（有缓冲入环，或 rendezvous 已配对） | — |
| `XCHANNEL_FULL` | 缓冲已满，值仍归调用方 | 不设置错误 |
| `XCHANNEL_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XCHANNEL_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告；消息不移动 |

#### 错误

- `XERR_ARGUMENT` — `pChannel` 为空
- `XERR_STATE` — 结构不是已初始化的 Channel

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 发送后核对计数

```c
if ( (xrtChannelTrySend(&tEmbedded, (ptr)5) != XCHANNEL_OK) ||
	(xrtChannelCount(&tEmbedded) != 1u) ||
```

### `xrtChannelTryRecv`

非阻塞接收；输出必须对齐且不能覆盖 Channel 或内部指针环。

```c
xchannelresult xrtChannelTryRecv(
	xchannel* pChannel,
	ptr* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出；不得覆盖 Channel 或消息环 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XCHANNEL_OK` | 已取得一个值写入 `*pItem` | — |
| `XCHANNEL_EMPTY` | 暂无可接收值 | 不设置错误 |
| `XCHANNEL_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XCHANNEL_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、输出未对齐，或输出与 Channel/消息环别名
- `XERR_STATE` — 结构不是已初始化的 Channel

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 取回发送的值

```c
(xrtChannelTryRecv(&tEmbedded, &pItem) != XCHANNEL_OK) ||
(pItem != (ptr)5) ) {
	goto Cleanup;
}
```

## 等待操作

`xrtChannelSend` / `Recv` 无限等待。`SendFor` / `RecvFor` 接收相对微秒数，
`SendUntil` / `RecvUntil` 接收由单调时钟构造的 `xdeadline`。可取消层保持
同一口径：`SendCancel` / `RecvCancel` 无限等待，`SendForCancel` /
`RecvForCancel` 使用相对时限，`SendUntilCancel` / `RecvUntilCancel` 使用
绝对截止时间。传入空取消令牌时，行为与对应的普通 deadline API 一致。

等待 API 返回：

- `XWAIT_OK`：操作已经提交；
- `XWAIT_TIMEOUT`：到达截止时间前没有提交；
- `XWAIT_CANCELLED`：可取消版本在提交前被取消；
- `XWAIT_CLOSED`：关闭阻止发送，或接收端已经关闭且排空；
- `XWAIT_ERROR`：参数、状态或平台同步错误。

每次等待都会先检查可执行操作，再检查关闭、取消和截止时间。因此零超时可作为
等待语义的 try：已经可执行时仍成功，否则返回 `XWAIT_TIMEOUT`。若取消和截止
时间在操作提交前同时成立，取消优先返回 `XWAIT_CANCELLED`；已关闭且不可提交的
操作仍返回 `XWAIT_CLOSED`。

取消监听只在操作确实需要阻塞时建立。取消回调和条件谓词共用 Channel mutex，
没有固定轮询周期。无缓冲发送一旦与等待接收者配对，就忽略随后到达的取消，
避免调用方收到失败但对端已经取得指针。多个等待可以共享同一取消令牌，也可以
使用子令牌；父令牌取消会唤醒子令牌等待。API 返回前会同步注销监听，调用方随后
可以立即释放令牌并在满足独占条件时释放 Channel，不会留下迟到回调。

### `xrtChannelSend`

等待发送一个可为空的指针值。

```c
xwaitresult xrtChannelSend(xchannel* pChannel, ptr pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pChannel` 为空
- `XERR_STATE` — 结构不是已初始化的 Channel

#### 范例

[concurrency/channel_tour · 阻塞族](../../examples/concurrency/channel_tour/main.c) · 容量 2 的首笔立即成功

```c
(xrtChannelSend(pHeap, (ptr)1) != XWAIT_OK) ||
(xrtChannelSendFor(pHeap, (ptr)2,
	EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
```

### `xrtChannelSendFor`

在相对微秒数内等待发送一个指针值。

```c
xwaitresult xrtChannelSendFor(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `iTimeout` | 输入 | 微秒 | 相对时限；零等价 try 语义 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_TIMEOUT` | 到期未提交，值仍归调用方 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSend`

#### 范例

[concurrency/channel_tour · 阻塞族](../../examples/concurrency/channel_tour/main.c) · 满通道第三笔必然超时

```c
(xrtChannelSendFor(pHeap, (ptr)4,
	EXAMPLE_TIMEOUT_US) != XWAIT_TIMEOUT) ||
(xrtChannelSendUntil(pHeap, (ptr)4,
	xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
	XWAIT_TIMEOUT) ) {
```

### `xrtChannelSendUntil`

等待发送一个指针值到指定单调时钟截止时间。

```c
xwaitresult xrtChannelSendUntil(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_TIMEOUT` | 到期未提交 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSend`

#### 范例

[concurrency/channel_tour · 阻塞族](../../examples/concurrency/channel_tour/main.c) · 空通道 + 远期截止的正常路径

```c
if ( xrtChannelSendUntil(pHeap, (ptr)3,
		xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) != XWAIT_OK ) {
	goto Cleanup;
}
```

### `xrtChannelRecv`

等待接收一个指针值。

```c
xwaitresult xrtChannelRecv(xchannel* pChannel, ptr* pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出；不得覆盖 Channel 或消息环 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值写入 `*pItem` | — |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或输出别名非法
- `XERR_STATE` — 结构不是已初始化的 Channel

#### 范例

[concurrency/channel · 消费循环](../../examples/concurrency/channel/main.c) · 关闭后排空接收

```c
while ( xrtChannelRecv(pChannel, &pItem) == XWAIT_OK ) {
	printf("%llu\n", (unsigned long long)(uintptr_t)pItem);
}
```

### `xrtChannelRecvFor`

在相对微秒数内等待接收一个指针值。

```c
xwaitresult xrtChannelRecvFor(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `iTimeout` | 输入 | 微秒 | 相对时限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_TIMEOUT` | 到期无值 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelRecv`

#### 范例

[concurrency/channel_tour · 阻塞族](../../examples/concurrency/channel_tour/main.c) · FIFO 顺序核对

```c
if ( (xrtChannelRecvFor(pHeap, &pItem,
		EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
	(pItem != (ptr)1) ||
	(xrtChannelRecvUntil(pHeap, &pItem,
		xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
		XWAIT_OK) ||
```

### `xrtChannelRecvUntil`

等待接收一个指针值到指定单调时钟截止时间。

```c
xwaitresult xrtChannelRecvUntil(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_TIMEOUT` | 到期无值 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelRecv`

#### 范例

[concurrency/channel_tour · 阻塞族](../../examples/concurrency/channel_tour/main.c) · Until 形态取第二个值

```c
(xrtChannelRecvUntil(pHeap, &pItem,
	xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
	XWAIT_OK) ||
(pItem != (ptr)2) ||
```

## 可取消等待

可取消层在 deadline 语义之上增加取消令牌中断：令牌触发时，尚未提交的操作返回 `XWAIT_CANCELLED`。空令牌等价于对应的普通 deadline API。

### `xrtChannelSendCancel`

无限等待发送，并允许取消令牌中断尚未提交的操作。

```c
xwaitresult xrtChannelSendCancel(
	xchannel* pChannel,
	ptr pItem,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `pCancel` | 输入 | 允许空 | 取消令牌；空指针走普通无限等待 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_CANCELLED` | 提交前被令牌中断，值仍归调用方 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSend`

#### 范例

[concurrency/channel_tour · 取消族](../../examples/concurrency/channel_tour/main.c) · 已触发令牌立即返回 CANCELLED

```c
if ( (xrtChannelSendCancel(pHeap, (ptr)9,
		SendJob.pCancel) != XWAIT_CANCELLED) ||
```

### `xrtChannelSendForCancel`

在相对微秒数内等待发送，并允许取消令牌中断尚未提交的操作。

```c
xwaitresult xrtChannelSendForCancel(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `iTimeout` | 输入 | 微秒 | 相对时限 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_TIMEOUT` | 到期未提交 | 不设置错误 |
| `XWAIT_CANCELLED` | 提交前被令牌中断 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSend`

#### 范例

[concurrency/channel_tour · 取消族](../../examples/concurrency/channel_tour/main.c) · 等待线程满通道上挂起

```c
pJob->Result = xrtChannelSendForCancel(pJob->pChannel, (ptr)1,
	UINT64_C(10000000), pJob->pCancel);
```

### `xrtChannelSendUntilCancel`

等待发送到截止时间，并允许取消令牌中断尚未提交的操作。

```c
xwaitresult xrtChannelSendUntilCancel(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_TIMEOUT` | 到期未提交 | 不设置错误 |
| `XWAIT_CANCELLED` | 提交前被令牌中断 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSend`

#### 范例

[concurrency/channel_tour · 取消族](../../examples/concurrency/channel_tour/main.c) · 已触发令牌的立即路径

```c
(xrtChannelSendUntilCancel(pHeap, (ptr)9,
	xrtDeadlineAfter(UINT64_C(1000000)),
	SendJob.pCancel) != XWAIT_CANCELLED) ||
```

### `xrtChannelRecvCancel`

无限等待接收，并允许取消令牌中断尚未完成的操作。

```c
xwaitresult xrtChannelRecvCancel(
	xchannel* pChannel,
	ptr* pItem,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_CANCELLED` | 完成前被令牌中断，输出未写入 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelRecv`

#### 范例

[concurrency/channel_cancel · 可取消接收](../../examples/concurrency/channel_cancel/main.c) · 令牌触发即中断

```c
iResult = xrtChannelRecvCancel(&tChannel, &pItem, pCancel);
printf("cancelled: %s\n", iResult == XWAIT_CANCELLED ? "yes" : "no");
```

### `xrtChannelRecvForCancel`

在相对微秒数内等待接收，并允许取消令牌中断尚未完成的操作。

```c
xwaitresult xrtChannelRecvForCancel(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `iTimeout` | 输入 | 微秒 | 相对时限 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_TIMEOUT` | 到期无值 | 不设置错误 |
| `XWAIT_CANCELLED` | 完成前被令牌中断 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelRecv`

#### 范例

[concurrency/channel_tour · 取消族](../../examples/concurrency/channel_tour/main.c) · 已触发令牌的五变体之一

```c
(xrtChannelRecvForCancel(pEmpty, &pOne,
	UINT64_C(1000000), RecvJob.pCancel) !=
	XWAIT_CANCELLED) ||
```

### `xrtChannelRecvUntilCancel`

等待接收到截止时间，并允许取消令牌中断尚未完成的操作。

```c
xwaitresult xrtChannelRecvUntilCancel(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_TIMEOUT` | 到期无值 | 不设置错误 |
| `XWAIT_CANCELLED` | 完成前被令牌中断 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数或状态错误 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelRecv`

#### 范例

[concurrency/channel_tour · 取消族](../../examples/concurrency/channel_tour/main.c) · 等待线程空通道上挂起

```c
pJob->Result = xrtChannelRecvUntilCancel(pJob->pChannel, &pItem,
	xrtDeadlineAfter(UINT64_C(10000000)), pJob->pCancel);
```

## 查询、关闭与重置

### `xrtChannelCount`

返回有缓冲 Channel 的精确元素数量；同步 Channel 始终返回零。

```c
size_t xrtChannelCount(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 目标 Channel |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 当前缓冲元素数；容量 0 恒为 0 | — |
| `0` | 空通道或参数/状态非法 | 非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 经内部锁路径报告；返回值为 0

#### 范例

[concurrency/channel_tour · 取消族](../../examples/concurrency/channel_tour/main.c) · 满发两笔后核对

```c
if ( (pEmpty == NULL) ||
	(xrtChannelCount(pHeap) != 2u) ) {
	goto Cleanup;
}
```

### `xrtChannelCapacity`

返回创建时指定的精确容量。

```c
size_t xrtChannelCapacity(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 目标 Channel |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 精确容量（同步 Channel 为 0） | — |
| `0` | 容量 0 或参数/状态非法 | 非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 经内部锁路径报告

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 初始化即核对容量

```c
if ( !xrtChannelInitBuffer(&tEmbedded, arrSlots, 2u) ||
	(xrtChannelCapacity(&tEmbedded) != 2u) ) {
	goto Cleanup;
}
```

### `xrtChannelIsClosed`

判断发送端是否已经关闭。

```c
bool xrtChannelIsClosed(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 目标 Channel |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已 `Close` | — |
| `false` | 未关闭或参数/状态非法 | 非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 经内部锁路径报告

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 关闭后置位、重置后清除

```c
xrtChannelClose(&tEmbedded);
if ( !xrtChannelIsClosed(&tEmbedded) ||
	!xrtChannelIsDrained(&tEmbedded) ||
	!xrtChannelReset(&tEmbedded) ||
	xrtChannelIsClosed(&tEmbedded) ) {
	goto Cleanup;
}
```

### `xrtChannelIsDrained`

判断 Channel 是否已经关闭且没有可接收值。

```c
bool xrtChannelIsDrained(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 目标 Channel |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已关闭且缓冲已排空 | — |
| `false` | 未关闭、仍有值，或参数/状态非法 | 非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 经内部锁路径报告

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 关闭排空后为真

```c
xrtChannelClose(&tEmbedded);
if ( !xrtChannelIsClosed(&tEmbedded) ||
	!xrtChannelIsDrained(&tEmbedded) ||
```

### `xrtChannelClose`

幂等关闭发送端；已有缓冲值仍可继续接收。

```c
void xrtChannelClose(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 目标 Channel；唤醒全部等待者 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 新发送返回 `CLOSED`；未配对的无缓冲发送被撤回；已配对的 rendezvous 值不被撤销 |

#### 范例

[concurrency/channel · 收尾](../../examples/concurrency/channel/main.c) · 关闭后消费循环自然退出

```c
xrtChannelClose(pChannel);
while ( xrtChannelRecv(pChannel, &pItem) == XWAIT_OK ) {
	printf("%llu\n", (unsigned long long)(uintptr_t)pItem);
}
```

### `xrtChannelDrain`

排空调用开始时已有的值；用户回调在 Channel 锁外执行。

```c
size_t xrtChannelDrain(
	xchannel* pChannel,
	xchanneldrainfn pDrain,
	ptr pContext
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pDrain` | 输入 | 非空 | 排空回调；可查询或发送到同一 Channel，不得做生命周期操作 |
| `pContext` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 移除并交给回调的值数量；回调新发送的值不计入 | — |
| `0` | 无值或参数/状态非法 | 非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 经内部锁路径报告

#### 范例

[concurrency/worker · 释放剩余任务](../../examples/concurrency/worker/main.c) · 关闭等待后回收未消费任务

```c
xrtChannelClose(pChannel);
if ( xrtThreadWait(pWorker) == XWAIT_OK ) {
	bWorkerOk = (xrtThreadExitCode(pWorker) == 0);
}
(void)xrtChannelDrain(pChannel, workerJobDrain, NULL);
```

### `xrtChannelReset`

在独占、无等待者且为空时重置并重新开放 Channel。

```c
bool xrtChannelReset(xchannel* pChannel);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel；需调用方独占 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 关闭状态清除，容量保留，可重新使用 | — |
| `false` | 非空、有挂起值或有 Select 等待者 | 状态不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 指针/状态非法
- `XERR_AGAIN` — 缓冲非空、rendezvous 挂起或存在 Select 等待节点

#### 范例

[concurrency/channel_tour · 内嵌形态](../../examples/concurrency/channel_tour/main.c) · 重置后 IsClosed 恢复假

```c
!xrtChannelReset(&tEmbedded) ||
xrtChannelIsClosed(&tEmbedded) ) {
	goto Cleanup;
}
```

## Select

`xchannelselectresult` 的三个字段必须一起判断：

- `Wait == XWAIT_OK` 表示某个 case 已被选中，`Index` 是 case 索引；
- `Result` 是该 case 的 `OK` 或 `CLOSED` 等 Channel 结果；
- 超时、取消和选择器错误不会选择 case，`Index == XCHANNEL_SELECT_NONE`；
- 关闭是立即可选择的 case，不会伪装成整个 Select 的等待错误。

选择器使用轮转起点避免固定偏爱第一个就绪 case。阻塞选择注册轻量等待节点，
任意时刻只有一个原子赢家；多个 Channel 同时就绪也只能提交一个操作。无缓冲的
发送 Select 与接收 Select 可以直接 rendezvous，不会先在多个 Channel 中发布
随后难以撤销的半提交值。

最多 8 个 case 的常见阻塞选择使用栈内节点，不经过 XRT 分配器；更大的选择才按
case 数量分配临时节点。立即就绪和 `SelectTry` 路径始终不分配。

接收输出只在对应 case 被选中时写入。每个输出必须与 case 数组、全部参与
Channel 对象及其消息环分离。case 数组和输出在 Select 返回前必须保持有效，
Channel 生命周期也必须覆盖整个等待过程。

### `xrtChannelCaseSend`

构造一个发送 case。

```c
xchannelcase xrtChannelCaseSend(
	xchannel* pChannel,
	ptr pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 参与 Select 的 Channel |
| `pItem` | 输入 | 允许空 | case 选中时要发送的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| case 值 | 只描述操作，不持有 Channel 或消息所有权 | — |

#### 范例

[concurrency/channel_tour · Select 族](../../examples/concurrency/channel_tour/main.c) · case0=发送、case1=接收

```c
Cases[0] = xrtChannelCaseSend(pHeap, (ptr)20);
Cases[1] = xrtChannelCaseRecv(pEmpty, &pItem);
```

### `xrtChannelCaseRecv`

构造一个接收 case。

```c
xchannelcase xrtChannelCaseRecv(
	xchannel* pChannel,
	ptr* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入 | 非空 | 参与 Select 的 Channel |
| `pItem` | 输出 | 非空、独立 | case 选中时的接收输出 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| case 值 | 输出只在 case 被选中时写入 | — |

#### 范例

[concurrency/channel_select · 双通道](../../examples/concurrency/channel_select/main.c) · 两条通道各一个接收 case

```c
arrCase[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
arrCase[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
```

### `xrtChannelSelectTry`

公平地尝试全部 case，不可立即提交时返回 TIMEOUT 和无效索引。

```c
xchannelselectresult xrtChannelSelectTry(
	const xchannelcase* pCases,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组；调用期间保持有效 |
| `iCount` | 输入 | `> 0` | case 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 有 case 立即提交；`Index` 为其下标，`Result` 为其 Channel 结果 | — |
| `Wait == XWAIT_TIMEOUT` | 无 case 可立即提交；`Index == XCHANNEL_SELECT_NONE` | 不设置错误 |
| `Wait == XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — case 数组为空或计数为零
- `XERR_STATE` — 任一 Channel 非初始化状态

#### 范例

[concurrency/channel_tour · Select 族](../../examples/concurrency/channel_tour/main.c) · 只有发送侧就绪

```c
Select = xrtChannelSelectTry(Cases, 2u);
if ( (Select.Wait != XWAIT_OK) || (Select.Index != 0u) ||
	(Select.Result != XCHANNEL_OK) ) {
```

### `xrtChannelSelect`

等待任意一个 case 原子提交。

```c
xchannelselectresult xrtChannelSelect(
	const xchannelcase* pCases,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交；`Index` 与 `Result` 有效 | — |
| `Wait == XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSelectTry`

#### 范例

[concurrency/channel_select · 双通道](../../examples/concurrency/channel_select/main.c) · 命中第二条预填通道

```c
tResult = xrtChannelSelect(arrCase, 2u);
if (
	(tResult.Wait != XWAIT_OK) ||
	(tResult.Index != 1u)
) {
```

### `xrtChannelSelectFor`

在相对微秒数内等待任意一个 case 原子提交。

```c
xchannelselectresult xrtChannelSelectFor(
	const xchannelcase* pCases,
	size_t iCount,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |
| `iTimeout` | 输入 | 微秒 | 相对时限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交 | — |
| `Wait == XWAIT_TIMEOUT` | 到期无 case 提交 | 不设置错误 |
| `Wait == XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSelectTry`

#### 范例

[concurrency/channel_tour · Select 族](../../examples/concurrency/channel_tour/main.c) · 两侧都不就绪必然超时

```c
Select = xrtChannelSelectFor(Cases, 2u, EXAMPLE_TIMEOUT_US);
if ( Select.Wait != XWAIT_TIMEOUT ) {
	goto Cleanup;
}
```

### `xrtChannelSelectUntil`

等待任意一个 case 原子提交到指定单调时钟截止时间。

```c
xchannelselectresult xrtChannelSelectUntil(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交 | — |
| `Wait == XWAIT_TIMEOUT` | 到期无 case 提交 | 不设置错误 |
| `Wait == XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSelectTry`

#### 范例

[concurrency/channel_tour · Select 族](../../examples/concurrency/channel_tour/main.c) · 放入一条后选中接收 case

```c
Select = xrtChannelSelectUntil(Cases, 2u,
	xrtDeadlineAfter(UINT64_C(1000000)));
if ( (Select.Wait != XWAIT_OK) || (Select.Index != 1u) ||
	(pItem != (ptr)30) ) {
```

### `xrtChannelSelectUntilCancel`

等待任意 case 提交，并允许取消令牌中断未提交的选择。

```c
xchannelselectresult xrtChannelSelectUntilCancel(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌；空走普通 Until 语义 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交 | — |
| `Wait == XWAIT_TIMEOUT` | 到期无 case 提交 | 不设置错误 |
| `Wait == XWAIT_CANCELLED` | 提交前被令牌中断 | 不设置错误 |
| `Wait == XWAIT_ERROR` | 参数或状态错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSelectTry`

#### 范例

[concurrency/channel_select_cancel · 可取消选择](../../examples/concurrency/channel_select_cancel/main.c) · 已触发令牌 + 永不超时

```c
Case = xrtChannelCaseRecv(&Channel, &pValue);
Result = xrtChannelSelectUntilCancel(
	&Case,
	1u,
	XRT_DEADLINE_NEVER,
	pCancel
);
```

## 协程等待

`xrtChannelSendAwait` / `RecvAwait` 在当前调度协程中挂起，不会阻塞调度器所属
原生线程。`AwaitFor` 与 `AwaitUntil` 分别接收相对微秒数和单调时钟截止时间。
它们自动响应当前协程的取消请求，不需要另传一个取消令牌。

`xrtChannelSelectAwait`、`SelectAwaitFor` 和 `SelectAwaitUntil` 是同一原子选择
协议的协程驱动版本。它们与同步 Select 共享 case、结果、公平轮转、唯一提交、
rendezvous 和输出所有权契约，但不创建原生事件。同步 Select 与协程 Select
可以独立裁剪；只启用协程层不会引入 `XRT_FEATURE_EVENT`。

调用必须位于 `xrtCoSchedCreate` 管理的当前协程中，否则返回 `XWAIT_ERROR` 并
设置 `XERR_STATE`。一个操作提交后返回 `XWAIT_OK`；Channel 已关闭时单路
Send/Recv Await 返回 `XWAIT_CLOSED`。多路 Await 仍通过
`xchannelselectresult.Result` 表达被选 case 的关闭状态。

最多 8 个 case 的协程等待使用协程栈上的节点，不执行堆分配；更大的选择按 case
数量申请临时节点。状态变化通过内部代际令牌投递到所属调度器，跨线程发送、关闭
和取消都不会轮询。令牌只对当前这次 Await 有效：完成发生在真正 park 之前不会
丢失通知，等待结束后也不会把通知泄漏给下一次 park；独立的公共 `xrtCoWake`
不会被 Await 清理过程误消费。返回前会同步摘除全部 Channel 注册。

### `xrtChannelSendAwait`

在当前调度协程中挂起发送，不阻塞调度线程。

```c
xwaitresult xrtChannelSendAwait(
	xchannel* pChannel,
	ptr pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数/状态错误，或不在调度协程中 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pChannel` 为空
- `XERR_STATE` — Channel 非初始化状态，或当前线程不在协程调度器内

#### 范例

[concurrency/channel_coroutine · rendezvous](../../examples/concurrency/channel_coroutine/main.c) · 无缓冲发送协程

```c
if (
	xrtChannelSendAwait(
		pChannel,
		(ptr)(uintptr_t)42u
	) != XWAIT_OK
) {
```

### `xrtChannelSendAwaitFor`

在当前调度协程中挂起发送，直到相对期限结束。

```c
xwaitresult xrtChannelSendAwaitFor(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_TIMEOUT` | 到期未提交 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSendAwait`

#### 范例

[concurrency/channel_tour · 协程四](../../examples/concurrency/channel_tour/main.c) · 满通道发送到期

```c
return (ptr)(uintptr_t)xrtChannelSendAwaitFor(pChannel, (ptr)1,
	EXAMPLE_TIMEOUT_US);
```

### `xrtChannelSendAwaitUntil`

在当前调度协程中挂起发送，直到绝对截止时间。

```c
xwaitresult xrtChannelSendAwaitUntil(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输入 | 允许空 | 要发送的指针值 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已提交 | — |
| `XWAIT_TIMEOUT` | 到期未提交 | 不设置错误 |
| `XWAIT_CLOSED` | 发送端已关闭 | 不设置错误 |
| `XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSendAwait`

#### 范例

[concurrency/channel_tour · 协程二](../../examples/concurrency/channel_tour/main.c) · 由接收协程唤醒

```c
return (ptr)(uintptr_t)xrtChannelSendAwaitUntil(pChannel,
	(ptr)77, xrtDeadlineAfter(UINT64_C(3000000)));
```

### `xrtChannelRecvAwait`

在当前调度协程中挂起接收，不阻塞调度线程。

```c
xwaitresult xrtChannelRecvAwait(
	xchannel* pChannel,
	ptr* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSendAwait`

#### 范例

[concurrency/channel_coroutine · rendezvous](../../examples/concurrency/channel_coroutine/main.c) · 接收协程挂起等待

```c
if ( xrtChannelRecvAwait(pChannel, &pMessage) != XWAIT_OK ) {
	return NULL;
}
```

### `xrtChannelRecvAwaitFor`

在当前调度协程中挂起接收，直到相对期限结束。

```c
xwaitresult xrtChannelRecvAwaitFor(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_TIMEOUT` | 到期无值 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSendAwait`

#### 范例

[concurrency/channel_tour · 协程一](../../examples/concurrency/channel_tour/main.c) · 空通道接收到期

```c
return (ptr)(uintptr_t)xrtChannelRecvAwaitFor(pChannel, &pItem,
	EXAMPLE_TIMEOUT_US);
```

### `xrtChannelRecvAwaitUntil`

在当前调度协程中挂起接收，直到绝对截止时间。

```c
xwaitresult xrtChannelRecvAwaitUntil(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pChannel` | 输入/输出 | 非空 | 目标 Channel |
| `pItem` | 输出 | 非空、独立 | 接收输出 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XWAIT_OK` | 已取得值 | — |
| `XWAIT_TIMEOUT` | 到期无值 | 不设置错误 |
| `XWAIT_CLOSED` | 已关闭且已排空 | 不设置错误 |
| `XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 输出未写入；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSendAwait`

#### 范例

[concurrency/channel_tour · 协程五](../../examples/concurrency/channel_tour/main.c) · 过期截止立即超时

```c
return (ptr)(uintptr_t)xrtChannelRecvAwaitUntil(pChannel, &pItem,
	xrtDeadlineAfter(UINT64_C(1)));
```

### `xrtChannelSelectAwait`

在当前调度协程中挂起，直到任意一个 case 原子提交。

```c
xchannelselectresult xrtChannelSelectAwait(
	const xchannelcase* pCases,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交；`Index` 与 `Result` 有效 | — |
| `Wait == XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — case 数组非法
- `XERR_STATE` — Channel 状态非法或不在调度协程中

#### 范例

[concurrency/channel_tour · 协程六](../../examples/concurrency/channel_tour/main.c) · 无限期版本命中预填 case

```c
Result = xrtChannelSelectAwait(Cases, 2u);
if ( Result.Wait == XWAIT_OK ) {
	return (ptr)(uintptr_t)(0x20 + Result.Index);
}
```

### `xrtChannelSelectAwaitFor`

在当前调度协程中挂起，直到任意 case 提交或相对期限结束。

```c
xchannelselectresult xrtChannelSelectAwaitFor(
	const xchannelcase* pCases,
	size_t iCount,
	uint64 iTimeout
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |
| `iTimeout` | 输入 | 微秒 | 相对期限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交 | — |
| `Wait == XWAIT_TIMEOUT` | 到期无 case 提交 | 不设置错误 |
| `Wait == XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSelectAwait`

#### 范例

[concurrency/channel_tour · 协程三](../../examples/concurrency/channel_tour/main.c) · 两条接收通道间选择

```c
Cases[0] = xrtChannelCaseRecv(&arrChannel[0], &pItem);
Cases[1] = xrtChannelCaseRecv(&arrChannel[1], &pItem);
Result = xrtChannelSelectAwaitFor(Cases, 2u, UINT64_C(3000000));
```

### `xrtChannelSelectAwaitUntil`

在当前调度协程中挂起，直到任意 case 提交或到达截止时间。

```c
xchannelselectresult xrtChannelSelectAwaitUntil(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCases` | 输入 | 非空 | case 数组 |
| `iCount` | 输入 | `> 0` | case 数量 |
| `iDeadline` | 输入 | 单调时钟 | 绝对截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Wait == XWAIT_OK` | 某 case 已提交 | — |
| `Wait == XWAIT_TIMEOUT` | 到期无 case 提交 | 不设置错误 |
| `Wait == XWAIT_ERROR` | 参数/状态错误或非协程上下文 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_STATE` — 同 `xrtChannelSelectAwait`

#### 范例

[concurrency/channel_tour · 协程七](../../examples/concurrency/channel_tour/main.c) · 专用空通道必然到期

```c
Cases[0] = xrtChannelCaseRecv(pEmpty, &pItem);
Cases[1] = xrtChannelCaseSend(pEmpty, (ptr)1);
Result = xrtChannelSelectAwaitUntil(Cases, 2u,
	xrtDeadlineAfter(UINT64_C(1)));
```

## 示例

可取消等待示例位于 `examples/concurrency/channel_cancel/main.c`。

```c
xchannel* pChannel = xrtChannelCreate(16u);
ptr pMessage = NULL;

xrtChannelSend(pChannel, message);
xrtChannelClose(pChannel);

while ( xrtChannelRecv(pChannel, &pMessage) == XWAIT_OK ) {
	handle(pMessage);
}
xrtChannelDestroy(pChannel);
```

无缓冲同步通道：

```c
xchannel tChannel;

xrtChannelInit(&tChannel, 0);
/* 一个线程 Send，另一个线程 Recv。 */
xrtChannelUnit(&tChannel);
```

协程通道：

```c
static ptr consume(ptr pData)
{
	ptr pItem = NULL;

	return xrtChannelRecvAwait((xchannel*)pData, &pItem) ==
		XWAIT_OK ? pItem : NULL;
}
```

其余可运行示例：`examples/concurrency/channel`（基础收发）、`channel_select`
（双通道选择）、`channel_select_cancel`（可取消选择）、`channel_coroutine`
（协程 rendezvous）、`channel_tour`（全接口巡览）。
