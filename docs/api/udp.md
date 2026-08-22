# UDP 传输 API

## 分层与设计

`XRT_FEATURE_NET_UDP` 依赖 `XRT_FEATURE_NET_ENGINE`，公开头文件为 `<xrt/udp.h>`。这一层提供未连接与连接式 UDP、推送与拉取接收、多种发送所有权、批量 API、硬背压、组播、关闭契约和并发统计。

UDP 对象不隐式执行 DNS，也不添加可靠、有序、重传或会话语义。需要这些能力的上层协议可以直接使用数据报边界、来源地址、统计和 Worker 串行保证，不需要更改底层 UDP 契约。

select fallback 和 IOCP 使用相同公开语义：

- readiness 后端在可读事件中按 `ReceiveBatch` 排空数据报。
- completion 后端预投递 `ReceiveConcurrency` 个接收操作。
- completion 后端最多并行提交 `SendConcurrency` 个发送操作；readiness 后端把它作为一次发送批次上限。
- 两条路径都保留零长度报文、来源地址、可选接收元数据、截断信息和单报文发送原子性。

`ReceiveBatch` 是每次 readiness 驱动的公平性预算，`xrtNetUdpReceiveBatch` 是一次锁内批量领取用户队列前缀；两者都不承诺由一次 `recvmmsg`、multishot 或其他平台批量系统调用完成。Socket 原语层另行公开 `xrtNetSocketRecvBatch/SendBatch`，供自定义事件循环直接处理独立报文批次。completion 后端通过多个独立在途槽取得并行度；readiness 发送在 `SendConcurrency > 1` 时使用 Socket 批量原语。

## 状态与线程

```c
typedef enum xnetudpstate {
	XNET_UDP_OPENING = 0,
	XNET_UDP_OPEN,
	XNET_UDP_CLOSING,
	XNET_UDP_CLOSED
} xnetudpstate;
```

状态只向前推进。`Open` 只在 Socket 已绑定、地址已发布、接收缓冲已预热且首次接收或 readiness 观察已成功提交后发生。因此 `Open` 不会把一个尚未完成 IO 初始化的对象暴露给用户。

`CLOSED` 是可靠的外部同步点：Socket、在途 IO、发送预算和 Worker 缓冲已终结，对 Engine 的活动对象占用也已释放。观察到 `CLOSED` 后，调用方可立即释放自己的 UDP 引用，并在没有其他活动对象时销毁 Engine。

```c
typedef struct xnetudpevents {
	void (*Open)(xnetudp* pUdp, ptr pData);
	void (*Receive)(xnetudp* pUdp,
		const xnetudpmessage* pMessage, ptr pData);
	void (*DatagramError)(xnetudp* pUdp,
		const xnetudperrormessage* pMessage, ptr pData);
	void (*Error)(xnetudp* pUdp, const xerror* pError, ptr pData);
	void (*HighWater)(xnetudp* pUdp,
		size_t iBytes, size_t iPackets, ptr pData);
	void (*LowWater)(xnetudp* pUdp,
		size_t iBytes, size_t iPackets, ptr pData);
	void (*Drain)(xnetudp* pUdp, ptr pData);
	void (*Close)(xnetudp* pUdp, xnetresult Result,
		const xerror* pError, ptr pData);
} xnetudpevents;
```

同一 UDP 对象的全部回调都在其所属 Worker 上串行执行。`Receive` 模式下的 `xnetudpmessage` 及其数据只在当次回调期间借用。`DatagramError` 报告显式启用的 ICMP、路径 MTU 或本地数据报错误，其消息与负载前缀同样只在回调期间借用。`Error` 报告其他可恢复错误，例如单次收发失败、拉取队列溢出或接收包分配失败；对象可继续工作。`Close` 只发生一次，终止错误可通过 `xrtNetUdpError` 在终态后查询。

`xrtNetUdpSetData` 只允许在所属 Worker 上切换协议状态，`xrtNetUdpData` 使用 acquire 语义返回线程安全的借用指针快照；快照不持有指针目标，目标内存的生命周期仍由调用方同步。

## 打开与配置

```c
xnetudp* xrtNetUdpOpen(xnetengine* pEngine,
	const xnetaddr* pLocal, const xnetaddr* pPeer,
	uint64 iAffinity, const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents, ptr pData);
xnetudp* xrtNetUdpBind(xnetengine* pEngine,
	const xnetaddr* pLocal, uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents, ptr pData);
xnetudp* xrtNetUdpConnect(xnetengine* pEngine,
	const xnetaddr* pPeer, uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents, ptr pData);
```

`Open` 是完整入口；`Bind` 和 `Connect` 是常用路径 Helper。未连接 UDP 可以和多个对端通信，适合服务器、发现协议和组播。连接式 UDP 由内核固定 Peer，可以使用更短的 `Send` API，并过滤非固定 Peer 的入站报文。

```c
typedef struct xnetudpconfig {
	size_t ReceiveSize;
	uint32 ReceiveConcurrency;
	uint32 ReceiveBatch;
	uint32 ReceiveMeta;
	size_t ReceiveQueueLimit;
	size_t ReceiveQueueByteLimit;
	xnetudpoverflow Overflow;
	xnetudptruncation Truncation;
	size_t ErrorSize;
	size_t ErrorQueueLimit;
	size_t ErrorQueueByteLimit;
	xnetudpoverflow ErrorOverflow;
	size_t SendHighWater;
	size_t SendLowWater;
	size_t SendLimit;
	size_t SendPacketLimit;
	uint32 SendConcurrency;
	int ReceiveBuffer;
	int SendBuffer;
	int HopLimit;
	int TrafficClass;
	xnetpmtumode PathMtu;
	bool ReuseAddress;
	bool ReusePort;
	bool ExclusiveAddress;
	bool Broadcast;
	bool IPv6Only;
	bool ReceiveErrors;
} xnetudpconfig;
```

`xrtNetUdpConfigInit` 的默认值为：2 KiB 接收大小、1 个 completion 接收槽、readiness 每轮最多 16 包、关闭接收元数据、拉取队列 256 包/1 MiB、丢弃最新包、投递截断前缀、错误负载前缀 256 字节、错误队列 64 项/64 KiB、系统默认 PMTU、关闭错误接收、发送高/低水位 256/64 KiB、发送硬上限 1 MiB/1024 包、1 个 completion 发送槽。

`ReceiveSize` 是每个在途接收操作的最大载荷，不是每对象固定 8K 缓冲。默认对象只使用一个 2 KiB 自适应块；缓冲由 Worker 池共享，关闭时归还。高吞吐 completion 服务可提高 `ReceiveConcurrency`，但应明确计算并发连接数与预投递内存的乘积。readiness 后端始终只分配一个接收槽，并且不为 `SendConcurrency` 分配发送槽；completion 后端的轻量槽随显式并发配置分配，不包含载荷缓冲。推送模式在回调期间直接借用接收块；拉取模式才为每个入队数据报分配一个带来源地址的精确长度拥有对象。

推送模式直接交付完成式接收槽或 readiness 临时块，不再分配包对象，也不复制载荷。
拉取模式保留一次精确长度复制，使排队包不占用 `ReceiveSize` 大小的接收槽，并允许
接收槽立即重新投递；这是高并发内存上限和零复制之间的显式取舍。

`ReceiveBuffer`/`SendBuffer`/`HopLimit`/`TrafficClass` 为 `-1` 时保留系统默认。地址复用和 Windows 独占地址不能同时开启；平台不支持的 Socket 选项会以结构化错误失败，不会被静默忽略。

`ReceiveMeta` 使用 `XNET_DGRAM_META_DESTINATION`、`INTERFACE`、`HOP_LIMIT`、`TRAFFIC_CLASS` 和 `SEGMENT_SIZE` 的组合。UDP 在绑定和投递首个接收前验证并启用全部请求字段，任何字段不可用都会让打开明确失败，不会降级为部分元数据。默认零值保持普通 `recvfrom` 路径；readiness 仅在显式启用后调用 `RecvMsg`，completion 仅在显式启用后为接收槽增加控制消息尾部，因此未使用该能力的对象没有控制缓冲和解析成本。

## 接收模式

设置 `Events.Receive` 时使用推送模式，没有额外拉取包分配和队列锁，适合高吞吐协议处理。回调内必须处理或复制需要保留的数据。

不设置 `Events.Receive` 时使用拉取模式：

```c
typedef struct xnetudpmessage {
	xnetaddr Remote;
	xnetdgrammeta Meta;
	cbytes Data;
	size_t Size;
	uint32 Flags;
} xnetudpmessage;

xnetudppacket* xrtNetUdpReceive(xnetudp* pUdp);
size_t xrtNetUdpReceiveBatch(xnetudp* pUdp,
	xnetudppacket** pPackets, size_t iCapacity);
xnetudppacket* xrtNetUdpPacketRef(xnetudppacket* pPacket);
const xnetaddr* xrtNetUdpPacketRemote(const xnetudppacket* pPacket);
const xnetdgrammeta* xrtNetUdpPacketMeta(
	const xnetudppacket* pPacket);
void xrtNetUdpPacketDestroy(xnetudppacket* pPacket);
```

推送消息在回调期间借用载荷，但 `Remote` 和 `Meta` 本身是消息内的值。拉取返回的 `xnetudppacket` 完整拥有载荷、来源地址和接收元数据，可跨线程保留；`PacketRemote` 与 `PacketMeta` 返回借用到数据包销毁前的只读字段。`PacketRef` 增加一个并发安全引用，每个引用最终都必须调用一次 `PacketDestroy`。`Receive` 为空队列返回空指针，这不是错误。`ReceiveBatch` 在一次锁内取出一个前缀，减少高包率下的锁开销。

`Meta.Flags` 逐字段表达有效性；不能假定配置请求的每个字段在每个数据报上都存在。目标地址不包含端口，接口使用索引，IPv6 目标地址的 Scope 同样保留接收接口。载荷截断继续由 `XNET_UDP_MESSAGE_TRUNCATED` 表达；平台控制消息截断则由 `XNET_DGRAM_META_TRUNCATED` 表达，两者互不替代。

拉取队列同时受 `ReceiveQueueLimit` 和 `ReceiveQueueByteLimit` 约束，任一值为零都表示不允许排队，但等待中的消费式 Future 仍可直接接收。达到任一上限时，`DROP_NEWEST`、`DROP_OLDEST`、`DROP_ERROR` 分别丢弃新包、移除足够多的最旧包后接纳新包、丢弃新包并通知可恢复错误。单个新包本身超过字节上限时不会清空旧队列，而是按 newest 处理。Worker 绝不会因用户消费速度而阻塞。

数据报超过 `ReceiveSize` 时，`TRUNCATE_DELIVER` 投递带 `XNET_UDP_MESSAGE_TRUNCATED` 的前缀；`TRUNCATE_DROP` 静默丢弃；`TRUNCATE_ERROR` 丢弃并通知可恢复错误。统计同时区分截断总数和因策略丢弃的数量。

## PMTU、异步错误与分段合并

高层 UDP 不假定平台具备高级数据报能力。先在 Socket 原语层用 `xrtNetSocketDgramCapabilities` 查询 `PATH_MTU_MODE`、`PATH_MTU_QUERY`、`ERROR_QUEUE`、`SEGMENT_SEND` 和 `SEGMENT_RECEIVE`；请求不可用能力时，打开或发送明确返回 `XERR_UNSUPPORTED`。Windows 提供 PMTU，并在 Winsock Provider 支持时提供 UDP 分段发送和合并接收；Linux 提供 PMTU、错误队列，并在内核运行期支持时提供 UDP GSO/GRO；其他平台保持零能力，后续可以在不改变公开契约的前提下补充实现。

`PathMtu` 控制对象级路径 MTU 策略：`SYSTEM` 保留系统默认，`DISCOVER` 禁止 IP 分片并使用路径发现，`FRAGMENT` 允许分片，`PROBE` 忽略缓存 MTU 进行探测。`ReceiveErrors` 显式启用异步错误接收，默认关闭，因此普通 UDP 对象不分配错误状态、错误缓冲或队列。启用时使用 `ErrorSize` 限制保存的原数据报负载前缀，并用 `ErrorQueueLimit`、`ErrorQueueByteLimit`、`ErrorOverflow` 建立独立有界队列；等待中的消费 Future 可以绕过零队列上限直接取得错误，但 Worker 永不等待用户消费。

```c
xnetudperrorpacket* xrtNetUdpReceiveError(xnetudp* pUdp);
size_t xrtNetUdpReceiveErrorBatch(xnetudp* pUdp,
	xnetudperrorpacket** pPackets, size_t iCapacity);
const xnetdgramerror* xrtNetUdpErrorPacketInfo(
	const xnetudperrorpacket* pPacket);
cbytes xrtNetUdpErrorPacketData(const xnetudperrorpacket* pPacket);
size_t xrtNetUdpErrorPacketSize(const xnetudperrorpacket* pPacket);
xnetudperrorpacket* xrtNetUdpErrorPacketRef(xnetudperrorpacket* pPacket);
void xrtNetUdpErrorPacketDestroy(xnetudperrorpacket* pPacket);
size_t xrtNetUdpPathMtu(const xnetudp* pUdp);
```

设置 `Events.DatagramError` 时使用推送模式；未设置时使用 `xrtNetUdpReceiveError`、`ReceiveErrorBatch` 拉取拥有型 `xnetudperrorpacket`。`xnetdgramerror` 分离 `Origin`、系统错误码、ICMP Type/Code、远端、错误源和路径 MTU，有效字段只由 Flags 决定。异步网络错误是协议数据，不写入当前线程的 `xrtGetError`。`xrtNetUdpPathMtu` 返回最近一次错误队列确认的 MTU，未知为零；统计提供错误总数、丢弃数、队列当前/峰值和 MTU 更新次数。

发送控制中的 `XNET_DGRAM_CONTROL_SEGMENT_SIZE` 把一块聚合载荷交给内核分成最多 64 个 UDP 数据报，最后一段允许较短。XRT 在系统调用前验证分段大小为 1 到 65535、载荷非空且分段数不超过 64。接收端显式启用 `XNET_DGRAM_META_SEGMENT_SIZE` 后，合并结果仍作为一个消息或 Packet 交付，`Meta.SegmentSize` 给出原始数据报边界；上层按该值遍历前缀，最后一段取剩余长度。平台可以选择不合并，此时每个 Packet 保持原始边界且该位为零，调用方不能把能力位理解为每次交付必有分段元数据。启用合并时应把 `ReceiveSize` 配置为可容纳的最大聚合载荷，较小缓冲仍按 `TRUNCATED` 契约交付。GSO/GRO 只减少系统调用和协议栈开销，不引入可靠、有序或重传语义。

## Future 与协程

`XRT_FEATURE_NET_UDP_FUTURE` 是独立裁剪层，依赖 UDP 与通用 Future，但 UDP 核心不反向依赖它。它不创建隐藏 Engine，也不复制协程状态机。启用 `XRT_FEATURE_FUTURE_COROUTINE` 后，调度协程直接使用通用 `xrtFutureAwait`、`AwaitFor`、`AwaitUntil`；协程取消只结束 Await，放弃独占网络操作时还要显式调用 `xrtFutureCancel`。

```c
typedef enum xnetudpwait {
	XNET_UDP_WAIT_OPEN = 0,
	XNET_UDP_WAIT_RECEIVE,
	XNET_UDP_WAIT_ERROR,
	XNET_UDP_WAIT_DRAIN,
	XNET_UDP_WAIT_CLOSE
} xnetudpwait;

xfuture* xrtNetUdpWaitAsync(xnetudp* pUdp, xnetudpwait Wait);
xfuture* xrtNetUdpWritableAsync(xnetudp* pUdp, size_t iSize);
xfuture* xrtNetUdpReceiveAsync(xnetudp* pUdp);
xfuture* xrtNetUdpReceiveErrorAsync(xnetudp* pUdp);
xfuture* xrtNetUdpReceiveBatchAsync(xnetudp* pUdp, size_t iCapacity);
```

等待条件均为水平条件：`OPEN` 在对象可用时完成；`RECEIVE` 在普通拉取队列非空时完成且不消费；`ERROR` 在错误拉取队列非空时完成且不消费；`DRAIN` 在 XRT 已受理发送包数归零时完成；`CLOSE` 在 UDP 达到完整 `CLOSED` 后完成。`WritableAsync` 比 `DRAIN` 更精确，它同时检查包数上限和指定数据报所需的字节预算。可写结果只是瞬时提示，其他生产者可能先占用预算，调用方仍必须检查下一次 `Send` 的返回值并在 `AGAIN` 后重试。

`ReceiveAsync` 消费一个包。`ReceiveBatchAsync` 的容量必须为 1 到 256，它消费完成时刻已经排队的 FIFO 前缀，最多达到请求容量；它不会为了填满容量继续等待。多个消费式 Future 按注册顺序与数据包配对。批量结果使用以下所有权 API：

```c
size_t xrtNetUdpBatchCount(const xnetudpbatch* pBatch);
xnetudpbatch* xrtNetUdpBatchRef(xnetudpbatch* pBatch);
xnetudppacket* xrtNetUdpBatchPacket(
	const xnetudpbatch* pBatch, size_t iIndex);
xnetudppacket* xrtNetUdpBatchTake(
	xnetudpbatch* pBatch, size_t iIndex);
void xrtNetUdpBatchDestroy(xnetudpbatch* pBatch);
```

成功 Future 拥有普通 Packet、错误 Packet 或批量结果，`xrtFutureValue` 返回的指针只借用到 Future 销毁前。普通包使用 `PacketRef` 保留，错误包使用 `ErrorPacketRef` 保留；批量容器使用 `BatchRef` 保留，也可用 `BatchTake` 转移其中一个数据包的所有权。`BatchPacket` 只借用，已经 `Take` 的位置返回空指针。批量容器在取包前完成分配，因此 OOM 不会消费排队数据。

消费式 `ReceiveAsync`/`ReceiveBatchAsync` 会直接取得数据包，不能与非消费式 `WAIT_RECEIVE` 或直接 `Receive`/`ReceiveBatch` 并发登记。错误方向同样只能在 `ReceiveErrorAsync` 与 `WAIT_ERROR` 加直接 Error Receive 两种模型中选择一种，两个方向的 FIFO 和统计彼此独立。条件 Future 本身只提供水平通知，完成后使用对应直接 API 消费；若其他直接消费者先取走结果，调用方重新等待。发生消费模式冲突时，以 `XERR_STATE/XNET_ERROR_UDP_RECEIVE_QUEUE` 失败。设置 `Events.Receive` 或 `Events.DatagramError` 的推送方向不能建立该方向的 Future，其他状态等待不受影响。

`xrtFutureCancel` 只撤销对应等待节点，不关闭 UDP，也不消费或丢弃已经排队的数据包。取消与到包、关闭在线程间线性化，Future 只会进入一个终态。`xrtFutureWaitFor` 或 `AwaitFor` 返回超时仅表示本次等待超时，不会隐式取消 Future；需要放弃操作时必须显式取消。直接销毁调用方持有的最后一个 Future 引用同样不会取消操作，等待节点会继续持有内部引用，完成后自动释放无人接收的结果。正常关闭使未完成接收进入 `XFUTURE_CLOSED`，主动中止进入 `XFUTURE_CANCELLED`，底层失败进入带结构化 `xerror` 的 `XFUTURE_FAILED`；正常 `CLOSE` 条件本身解析成功。

关闭门与 `OPEN`、可写、可读条件使用同一条线性化边界：`Close` 或 `Abort` 一旦受理，新的条件 Future 不会利用尚未来得及推进的旧 `OPEN` 状态错误解析成功。等待节点会保持到 `CLOSED` 的 release 发布点，再读取稳定的取消或错误原因。

`xnetudpstats.ReceiveWaiters` 与 `ErrorWaiters` 分别是当前普通包和错误包的消费 Future 数量，可用于关闭和压力测试后的泄漏检查。Future 等待节点持有 UDP 引用，调用方可以在 Future 完成前释放自己的引用，但标准关闭流程仍应先请求 `Close` 或 `Abort`，等待终态，再销毁最后一个外部引用。

UDP Future 等待节点保持在 128 字节尺寸类以内，并共享所属 Worker 的
`NodeCacheBytes` 预算。节点先在线性化边界离开条件或消费链，再解除取消监听、
归还缓存、释放持有的 UDP 引用和临时 Engine 租约，最后完成 Promise。因此跨线程
取消和关闭不会在 Future 终态之后继续访问已经停止的 Worker。`CLOSED` 发布后登记的新 Future 使用
独立堆，不再接触 Worker。调用方可以保留终态 UDP 引用到 Engine 销毁之后，再读取
稳定 Close 结果并释放引用。

### 阻塞便利层

`XRT_FEATURE_NET_UDP_SYNC` 只在 Future 之上提供阻塞外观，不创建隐藏 Engine、Worker 或辅助线程：

```c
bool xrtNetUdpWait(xnetudp* pUdp, xnetudpwait Wait,
	xdeadline iDeadline, xcancel* pCancel);
bool xrtNetUdpWritable(xnetudp* pUdp, size_t iSize,
	xdeadline iDeadline, xcancel* pCancel);
xnetudppacket* xrtNetUdpReceiveWait(xnetudp* pUdp,
	xdeadline iDeadline, xcancel* pCancel);
xnetudperrorpacket* xrtNetUdpReceiveErrorWait(xnetudp* pUdp,
	xdeadline iDeadline, xcancel* pCancel);
xnetudpbatch* xrtNetUdpReceiveBatchWait(xnetudp* pUdp, size_t iCapacity,
	xdeadline iDeadline, xcancel* pCancel);
```

这些函数不能从目标 UDP 所属 Worker 调用。成功接收返回调用方拥有的普通包、错误包或批量结果，分别使用 `xrtNetUdpPacketDestroy`、`xrtNetUdpErrorPacketDestroy`、`xrtNetUdpBatchDestroy` 释放。批量构造 OOM 不会取走已排队数据包；同步 Future 构造失败也不会留下等待节点。

Future 已经在等待锁内进入终态时，网络终态获胜；否则先观察到的截止时间或外部取消获胜，同步层取消 Future 并稳定返回 `XERR_TIMEOUT` 或 `XERR_CANCELLED`。关闭返回 `XERR_CLOSED`，底层失败保留 UDP 操作码与结构化原因。超时和取消只撤销本次等待，不关闭 UDP，也不改变队列中的数据包。

## 发送、所有权与背压

```c
xnetresult xrtNetUdpSendTo(xnetudp* pUdp,
	const xnetaddr* pRemote, const void* pData, size_t iSize);
xnetresult xrtNetUdpSendVecTo(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetspan* pSpans, size_t iCount);
xnetresult xrtNetUdpSendRefTo(xnetudp* pUdp,
	const xnetaddr* pRemote, const void* pData, size_t iSize,
	xnetreleaseproc pRelease, ptr pContext);
xnetresult xrtNetUdpSendTakeTo(xnetudp* pUdp,
	const xnetaddr* pRemote, ptr pData, size_t iSize);
xnetresult xrtNetUdpSendMsg(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl,
	const void* pData, size_t iSize);
xnetresult xrtNetUdpSendMsgRef(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl,
	const void* pData, size_t iSize,
	xnetreleaseproc pRelease, ptr pContext);
xnetresult xrtNetUdpSendMsgTake(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl,
	ptr pData, size_t iSize);
xnetresult xrtNetUdpSendBatch(xnetudp* pUdp,
	const xnetdgramsend* pItems, size_t iCount, size_t* pAccepted);
uint32 xrtNetUdpSendControlAvailable(const xnetudp* pUdp);
```

`Send`/`SendVec` 在返回前复制数据；`SendRef` 借用外部数据，只有成功受理后才会在终态执行一次释放过程；`SendTake` 只有成功受理后才接管 XRT 分配的内存。失败和 `AGAIN` 都不转移 ref/take 所有权。

没有 `To` 后缀的对称 API 发送到连接式 UDP 的固定 Peer。`SendBatch` 按输入前缀顺序复制受理，`pAccepted` 在所有返回路径都给出已进入队列的项数。批量中遇到硬上限时返回 `AGAIN`，遇到参数、溢出或 OOM 时返回 `ERROR`；已受理的前缀仍会正常发送。

`SendMsg` 系列为单次提交覆盖源地址、发送接口、Hop Limit、Traffic Class 或 GSO 分段大小，不修改对象级 Socket 默认值。先用 `SendControlAvailable` 查询当前对象实际支持的字段，再构造 `xnetdgramcontrol`；具体字段约束与平台能力见 `net.md`。空控制或零 Flags 与普通发送完全等价。控制描述符和显式远端地址总是在公开调用返回前复制，因此调用方可以立即复用；载荷继续遵循函数名对应的 copy/ref/take 契约。

多宿主服务器通常把接收元数据转换为回包控制：把 `PacketMeta(packet)->Destination` 复制到 `Control.Source`、将端口清零，并只在 `XNET_DGRAM_CONTROL_SOURCE` 可用时设置该位。这样固定回复可以从客户端实际访问的本地 IP 发出，不需要为每个地址创建独立 UDP 对象。接口索引、Hop Limit 与 Traffic Class 都是可选覆盖；库不会为了“看似成功”而忽略不可用字段。

`SendLimit` 和 `SendPacketLimit` 是字节数与包数的硬上限。预算在公开 API 入口原子占用，包含已受理但尚未挂入 Worker 的跨线程命令。任一上限不足都返回 `XNET_RESULT_AGAIN`，不分配节点、不转移所有权。

`SendConcurrency` 的有效范围为 1 到 64，默认值 1 保持本地提交、完成和 ref/take 释放顺序。completion 后端把它作为独立在途槽数；readiness 后端把它作为单次 Socket 发送批次上限，并且仍不分配 completion 发送槽。Linux 模块化构建可由一次 `sendmmsg` 提交前缀，其他平台使用相同语义的有界非阻塞回退。提交都从 XRT 队列前缀向后进行，但 completion 的系统完成、释放过程和网络到达均可乱序；readiness 同一成功批次中的所有项会先标记为已提交，再逐项发布完成，因此首项回调请求中止也不会误丢后续已发送项。UDP 本身不提供有序语义，需要顺序的上层协议必须携带序号并自行恢复。

发送节点中的小复制载荷和 ref/take 元数据在不超过 1 KiB 时共享所属 Worker 的
`NodeCacheBytes` 预算；更大数据报直接使用全局堆。节点回收不改变 UDP 的单报文
原子性，也不改变 ref/take 释放过程恰好执行一次的所有权契约。只有显式受控发送节点
才在节点尾部增加一份 `xnetdgramcontrol`；普通发送节点不携带控制缓冲。IOCP/io_uring
操作同样只为 `SEND_MSG` 按需保留平台控制状态。

`SendRef`/`SendMsgRef` 避免“调用方载荷到 XRT 队列”的复制，`SendTake`/`SendMsgTake`
避免该复制并把 XRT 分配内存交给队列；它们不承诺操作系统或网卡层零复制。UDP GSO/GRO
只通过独立能力位和逐包控制/元数据进入，不改变 copy/ref/take 的跨平台所有权含义。

`HighWater` 和 `LowWater` 是边沿通知，`Drain` 表示发送包预算回到零。它们用于调度重试，不代替硬上限。`xrtNetUdpPending` 返回当前 XRT 发送字节预算，不是内核 Socket 缓冲大小。

## 组播与底层逃生口

```c
bool xrtNetUdpJoin(xnetudp* pUdp,
	const xnetaddr* pGroup, const xnetaddr* pInterface);
bool xrtNetUdpLeave(xnetudp* pUdp,
	const xnetaddr* pGroup, const xnetaddr* pInterface);
bool xrtNetUdpMulticastLoop(xnetudp* pUdp, bool bEnabled);
bool xrtNetUdpMulticastHopLimit(xnetudp* pUdp, int iHopLimit);
bool xrtNetUdpMulticastInterface(xnetudp* pUdp,
	const xnetaddr* pInterface);
```

组播操作只允许在 UDP 所属 Worker 回调内执行，以便和关闭、Socket 选项及 IO 提交串行化。IPv4 接口使用本地接口地址；IPv6 接口使用 `xnetaddr.Scope` 的接口索引。空发送接口恢复系统默认。

`xrtNetUdpSocket` 同样只在 Worker 中返回借用 Socket，供高级用户配置 XRT 未直接暴露的平台选项。调用方不得保存、关闭、改变阻塞状态、执行收发或注册到其他事件循环；这些操作会破坏 UDP 状态机。

## 关闭、错误与统计

`xrtNetUdpClose` 原子封闭新发送，等待已进入提交区的跨线程调用退出，停止接收，并排空所有已成功受理的发送。`xrtNetUdpAbort` 取消每个在途收发槽、丢弃尚未提交的发送并以 `XNET_RESULT_CANCELLED` 关闭；被丢弃的 ref/take 项仍执行唯一释放。两者均幂等，Abort 可以把尚未完成的普通关闭升级为异常关闭。关闭请求之后迟到的 completion 接收只负责归还缓冲和在途槽，不再投递 Receive 或 Error；全部收发槽、发送命令和预算归零前不会发布 `CLOSED`，Close 回调也严格执行一次。

UDP 专用结构化错误码为 `XNET_ERROR_UDP_CONFIG`、`CREATE`、`RECEIVE`、`RECEIVE_QUEUE`、`SEND`、`CLOSE`，域为 `xrt.net`。平台失败同时保留系统错误码。公开 API 的参数/OOM/提交失败写入当前执行上下文错误；Worker 内错误通过 `Error` 事件或终态 `xrtNetUdpError` 表达。

`xrtNetUdpStats` 返回无锁快照，包括收发包/字节、截断与丢包原因、可恢复错误、异步数据报错误与 MTU 更新、硬上限拒绝、当前/峰值发送预算、普通与错误拉取队列、两类消费 waiter，以及当前接收和当前/峰值发送槽数。`ReceivedBytes` 统计实际写入接收块的前缀；截断报文在各平台都只计可见前缀，不虚构已经被内核丢弃的尾部长度。多个字段之间不保证来自同一时刻，终态后可获得稳定的最终值。

## 示例与发布门禁

完整回环与按接收目标地址回包示例位于 `examples/network/udp/main.c`，底层元数据和逐包发送控制能力协商示例位于 `examples/network/socket/main.c`，Future 请求响应示例位于 `examples/network/udp_future/main.c`，阻塞外观示例位于 `examples/network/udp_sync/main.c`，PMTU 与异步错误示例位于 `examples/network/udp_errors/main.c`。当前回归门禁覆盖 select、IOCP 与 io_uring，包含同步/完成式接收元数据、逐包发送控制、GSO/GRO、PMTU/错误队列、错误 Future/同步等待、零长度/截断/向量/批量报文、硬字节和包数上限、普通与错误队列溢出策略、completion 并发发送槽、OOM 恢复、真实 IPv4 组播回环、多生产者 Send/Close/Abort 线性化、Future 取消/关闭竞态、同步终态竞争、协程挂起恢复、控制值复制、所有权唯一释放、单头文件真实收发和裁剪依赖。
