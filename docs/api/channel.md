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


## 所有权

Channel 只保存指针位，不拥有指针目标，也不自动增加或减少引用。`NULL` 是合法
消息，必须通过返回结果和“没有消息”区分。发送失败时指针仍由调用方处理；发送
成功后如何转移目标所有权由上层协议决定。

所有接收输出必须按指针对齐，且不能覆盖 `xchannel` 对象或内部消息环。参数、
对齐或别名错误返回 `XCHANNEL_ERROR` / `XWAIT_ERROR`，设置 `XERR_ARGUMENT`，
不移动消息，也不写输出。


## 非阻塞操作

- `xrtChannelTrySend` 返回 `OK`、`FULL`、`CLOSED` 或 `ERROR`。
- `xrtChannelTryRecv` 返回 `OK`、`EMPTY`、`CLOSED` 或 `ERROR`。

无缓冲 `TrySend` 只有在接收者已经等待时才成功。成功表示 rendezvous 已不可
撤销地配对；即使随后关闭或取消，等待中的接收者仍会取得该值。


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


## 关闭

`xrtChannelClose` 幂等关闭发送端并唤醒全部发送者和接收者：

- 新发送返回 `CLOSED`；
- 有缓冲值仍按 FIFO 接收；
- 排空后接收返回 `CLOSED`；
- 未配对的无缓冲发送被撤回并返回 `CLOSED`；
- 已提交给等待接收者的 rendezvous 值不会被关闭撤销。

`xrtChannelIsDrained` 只在“已关闭且没有可接收值”时返回 true。


## 生命周期

`Unit` / `Destroy` 需要调用方独占对象。仍有发送者、接收者或 rendezvous 值时，
操作失败并设置 `XERR_STATE`。`Reset` 同样要求独占、无等待者且为空；成功后
清除关闭状态并重新开放。

`xrtChannelDrain` 处理调用开始时已有的值，供调用方释放剩余指针。用户回调
在 Channel 锁外执行，可以查询或发送到同一 Channel；回调新发送的值不会被本次
调用继续排空。回调期间不得执行 `Unit` / `Destroy` 这类生命周期操作。


## Select

`xrtChannelCaseSend` 和 `xrtChannelCaseRecv` 构造发送、接收 case。
`xrtChannelSelectTry` 是非阻塞 default 路径；`Select`、`SelectFor` 和
`SelectUntil` 分别提供无限、相对时间和绝对 deadline 等待。可取消层另提供
`xrtChannelSelectUntilCancel`。

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
