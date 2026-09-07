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

### `xrtNetUdpConfigInit`

初始化低内存默认配置：一个 2 KiB 接收槽和有界收发队列。

```c
void xrtNetUdpConfigInit(xnetudpconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[udp](../../examples/network/udp/main.c) · 默认配置

```c
	xrtNetUdpConfigInit(&UdpConfig);
```

### `xrtNetUdpOpen`

打开、绑定并可选连接 UDP；至少一个地址必须确定地址族。

```c
xnetudp* xrtNetUdpOpen(xnetengine* pEngine, const xnetaddr* pLocal, const xnetaddr* pPeer, uint64 iAffinity, const xnetudpconfig* pConfig, const xnetudpevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pLocal` | 输入 | 允许空 | 本地绑定地址 |
| `pPeer` | 输入 | 允许空 | 固定 Peer |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pConfig` | 输入 | 允许空 | UDP 配置 |
| `pEvents` | 输入 | 允许空 | 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | UDP 对象（引用 1） | — |
| `NULL` | 打开失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — 打开、绑定或连接失败（`SOCKET_OPEN/BIND/CONNECT/OPTION` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 打开完整形态

```c
	pUdp = xrtNetUdpOpen(pEngine, &Local, &Peer, 0, &UdpConfig,
		NULL, NULL);
```

### `xrtNetUdpBind`

打开未连接 UDP，适合服务器、多对端客户端和多播接收。

```c
xnetudp* xrtNetUdpBind(xnetengine* pEngine, const xnetaddr* pLocal, uint64 iAffinity, const xnetudpconfig* pConfig, const xnetudpevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pLocal` | 输入 | 非空 | 本地绑定地址 |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pConfig` | 输入 | 允许空 | UDP 配置 |
| `pEvents` | 输入 | 允许空 | 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | UDP 对象（引用 1） | — |
| `NULL` | 打开失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — 打开、绑定或连接失败（`SOCKET_OPEN/BIND/CONNECT/OPTION` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[udp](../../examples/network/udp/main.c) · 打开未连接形态

```c
	pServer = xrtNetUdpBind(pEngine, &Address, 0,
		&UdpConfig, NULL, NULL);
```

### `xrtNetUdpConnect`

打开连接式 UDP，并自动绑定同地址族的任意本地地址。

```c
xnetudp* xrtNetUdpConnect(xnetengine* pEngine, const xnetaddr* pPeer, uint64 iAffinity, const xnetudpconfig* pConfig, const xnetudpevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pPeer` | 输入 | 非空 | 固定 Peer |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pConfig` | 输入 | 允许空 | UDP 配置 |
| `pEvents` | 输入 | 允许空 | 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | UDP 对象（引用 1） | — |
| `NULL` | 打开失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — 打开、绑定或连接失败（`SOCKET_OPEN/BIND/CONNECT/OPTION` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[udp](../../examples/network/udp/main.c) · 打开连接形态

```c
	pClient = xrtNetUdpConnect(pEngine, &Address, 1,
		NULL, NULL, NULL);
```

### `xrtNetUdpRef`

增加 UDP 引用并返回原指针。

```c
xnetudp* xrtNetUdpRef(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 共享引用

```c
	pRef = xrtNetUdpRef(pUdp);
```

### `xrtNetUdpDestroy`

释放 UDP 引用；关闭操作必须另行请求。

```c
void xrtNetUdpDestroy(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[udp](../../examples/network/udp/main.c) · 释放引用

```c
	xrtNetUdpDestroy(pClient);
```

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

### `xrtNetUdpReceive`

未设置 Receive 回调时，非阻塞取出一个拥有型数据包。

```c
xnetudppacket* xrtNetUdpReceive(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有型数据包，用后销毁 | — |
| `NULL` | 队列为空 | 不设错误 |

#### 错误

- 无错误 — 空队列返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp](../../examples/network/udp/main.c) · 非阻塞接收

```c
		pPacket = xrtNetUdpReceive(pUdp);
```

### `xrtNetUdpReceiveBatch`

未设置 Receive 回调时，在一次锁内取出最多指定数量的数据包。

```c
size_t xrtNetUdpReceiveBatch(xnetudp* pUdp, xnetudppacket** pPackets, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pPackets` | 输出 | 非空数组 | 接收数据包数组 |
| `iCapacity` | 输入 | > 0 | 数组容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际取出的数据包数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 批量非阻塞接收

```c
		iCount = xrtNetUdpReceiveBatch(pServer, pPackets, 8);
```

### `xrtNetUdpReceiveError`

未设置 DatagramError 回调时，非阻塞取出一个拥有型错误包。

```c
xnetudperrorpacket* xrtNetUdpReceiveError(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有型错误包，用后销毁 | — |
| `NULL` | 队列为空 | 不设错误 |

#### 错误

- 无错误 — 空队列返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 非阻塞接收错误

```c
			(size_t)(xrtNetUdpReceiveError(pServer) != NULL ?
				1u : 0u),
```

### `xrtNetUdpReceiveErrorBatch`

未设置 DatagramError 回调时，在一次锁内取出一批错误包。

```c
size_t xrtNetUdpReceiveErrorBatch(xnetudp* pUdp, xnetudperrorpacket** pPackets, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pPackets` | 输出 | 非空数组 | 接收错误包数组 |
| `iCapacity` | 输入 | > 0 | 数组容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际取出的错误包数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 批量非阻塞接收错误

```c
			xrtNetUdpReceiveErrorBatch(pServer, pErrors, 4));
```

### `xrtNetUdpReceiveWait`

阻塞接收一个拥有型数据包。

```c
xnetudppacket* xrtNetUdpReceiveWait(xnetudp* pUdp, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iDeadline` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有型数据包 | — |
| `NULL` | 超时、取消或队列为空 | 超时/取消不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用

#### 范例

[udp_sync](../../examples/network/udp_sync/main.c) · 阻塞接收

```c
	pPacket = xrtNetUdpReceiveWait(
		pServer,
		xrtDeadlineAfter(3000000u),
		NULL
	);
```

### `xrtNetUdpReceiveErrorWait`

阻塞接收一个拥有型结构化数据报错误。

```c
xnetudperrorpacket* xrtNetUdpReceiveErrorWait(xnetudp* pUdp, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iDeadline` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有型错误包 | — |
| `NULL` | 超时、取消或队列为空 | 超时/取消不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 阻塞接收错误

```c
	pPacket = xrtNetUdpReceiveErrorWait(
		pUdp,
		xrtDeadlineAfter(3000000u),
		NULL
	);
```

### `xrtNetUdpReceiveBatchWait`

阻塞接收一个拥有型批量结果。

```c
xnetudpbatch* xrtNetUdpReceiveBatchWait(xnetudp* pUdp, size_t iCapacity, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iCapacity` | 输入 | 1–256 | 批量容量 |
| `iDeadline` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有型批量结果，用后销毁 | — |
| `NULL` | 超时或取消 | 超时/取消不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不在 1–256 范围
- `XERR_STATE` — 从所属 Worker 调用

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 阻塞批量接收

```c
	pBatch = xrtNetUdpReceiveBatchWait(pServer, 4,
		xrtDeadlineAfter(3000000u), NULL);
```

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

### `xrtNetUdpErrorPacketRef`

增加错误包引用并返回原指针。

```c
xnetudperrorpacket* xrtNetUdpErrorPacketRef(xnetudperrorpacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标错误包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 错误包引用

```c
		xnetudperrorpacket* pRef = xrtNetUdpErrorPacketRef(pPacket);
```

### `xrtNetUdpErrorPacketDestroy`

销毁拥有型数据报错误包；空指针视为空操作。

```c
void xrtNetUdpErrorPacketDestroy(xnetudperrorpacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 销毁错误包

```c
		xrtNetUdpErrorPacketDestroy(pRef);
```

### `xrtNetUdpErrorPacketInfo`

返回错误包内拥有的结构化数据报错误。

```c
const xnetdgramerror* xrtNetUdpErrorPacketInfo(const xnetudperrorpacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标错误包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 结构化错误借用 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 错误信息

```c
	pError = xrtNetUdpErrorPacketInfo(pPacket);
```

### `xrtNetUdpErrorPacketData`

返回错误包内原数据报负载前缀。

```c
cbytes xrtNetUdpErrorPacketData(const xnetudperrorpacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标错误包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 前缀借用（包存活期间有效） | — |
| `NULL` | 空前缀 | 不设错误 |

#### 错误

- 空前缀返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 负载前缀借用

```c
			 (xrtNetUdpErrorPacketData(pRef) != NULL) ) {
```

### `xrtNetUdpErrorPacketSize`

返回错误包负载前缀长度。

```c
size_t xrtNetUdpErrorPacketSize(const xnetudperrorpacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标错误包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 前缀字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 前缀长度

```c
		xrtNetUdpErrorPacketSize(pPacket)
```

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

### `xrtNetUdpReceiveAsync`

拉取模式下异步接收一个数据包；成功值由 Future 持有一个 Packet 引用。

```c
xfuture* xrtNetUdpReceiveAsync(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[udp_future](../../examples/network/udp_future/main.c) · 异步接收

```c
	pRequest = xrtNetUdpReceiveAsync(pServer);
```

### `xrtNetUdpReceiveErrorAsync`

拉取模式下异步接收一个结构化数据报错误；成功值由 Future 持有引用。

```c
xfuture* xrtNetUdpReceiveErrorAsync(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 异步接收错误

```c
	pErrorFuture = xrtNetUdpReceiveErrorAsync(pServer);
```

### `xrtNetUdpReceiveBatchAsync`

拉取模式下异步接收当前可用批次；容量必须位于 1 到 256 之间。

```c
xfuture* xrtNetUdpReceiveBatchAsync(xnetudp* pUdp, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iCapacity` | 输入 | 1–256 | 批量容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future，成功值为批量结果 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不在 1–256 范围
- `XERR_MEMORY` — 分配失败

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 异步批量接收

```c
	pFuture = xrtNetUdpReceiveBatchAsync(pServer, 4);
```

### `xrtNetUdpWaitAsync`

异步等待 UDP 条件；成功、失败、取消和关闭映射到统一 Future 终态。

```c
xfuture* xrtNetUdpWaitAsync(xnetudp* pUdp, xnetudpwait Wait)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `Wait` | 输入 | — | 等待条件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[udp_future](../../examples/network/udp_future/main.c) · 异步等待条件

```c
	pServerOpen = xrtNetUdpWaitAsync(pServer, XNET_UDP_WAIT_OPEN);
```

### `xrtNetUdpWritableAsync`

异步等待发送队列能够原子接纳指定大小的数据报。

```c
xfuture* xrtNetUdpWritableAsync(xnetudp* pUdp, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iSize` | 输入 | — | 数据报字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 异步等待可写

```c
	pWritable = xrtNetUdpWritableAsync(pClient, 64);
```

### `xrtNetUdpBatchRef`

增加批量结果引用并返回原指针。

```c
xnetudpbatch* xrtNetUdpBatchRef(xnetudpbatch* pBatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBatch` | 输入 | 非空 | 目标批量结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 批量结果引用

```c
	pBatchRef = xrtNetUdpBatchRef(pBatch);
```

### `xrtNetUdpBatchCount`

返回 Future 批量结果中的数据包数量。

```c
size_t xrtNetUdpBatchCount(const xnetudpbatch* pBatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBatch` | 输入 | 非空 | 目标批量结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数据包数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 批量数量

```c
	if ( (pBatch == NULL) || (xrtNetUdpBatchCount(pBatch) < 1u) ) {
```

### `xrtNetUdpBatchPacket`

返回批量结果中一个借用的数据包。

```c
xnetudppacket* xrtNetUdpBatchPacket(const xnetudpbatch* pBatch, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBatch` | 输入 | 非空 | 目标批量结果 |
| `iIndex` | 输入 | < 批量数量 | 序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 数据包借用（批量结果存活期间有效） | — |
| `NULL` | 序号越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 序号越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 借用数据包

```c
		xnetudppacket* pBorrowed = xrtNetUdpBatchPacket(pBatch, 0);
```

### `xrtNetUdpBatchTake`

从批量结果转移一个数据包所有权；对应位置随后为空。

```c
xnetudppacket* xrtNetUdpBatchTake(xnetudpbatch* pBatch, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBatch` | 输入 | 非空 | 目标批量结果 |
| `iIndex` | 输入 | < 批量数量 | 序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 转移出的数据包所有权 | — |
| `NULL` | 序号越界或已转移 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 序号越界或该位置已被转移；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 转移数据包

```c
		xnetudppacket* pTaken = xrtNetUdpBatchTake(pBatch,
			xrtNetUdpBatchCount(pBatch) - 1u);
```

### `xrtNetUdpBatchDestroy`

销毁批量结果及其中仍未转移的数据包。

```c
void xrtNetUdpBatchDestroy(xnetudpbatch* pBatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBatch` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 销毁批量结果

```c
	xrtNetUdpBatchDestroy(pBatch);
```

### `xrtNetUdpPacketRef`

增加数据包引用并返回原指针，便于跨 Future 和线程保留零复制结果。

```c
xnetudppacket* xrtNetUdpPacketRef(xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标数据包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 数据包引用

```c
	pKept = xrtNetUdpPacketRef(xrtNetUdpBatchPacket(pBatch, 0));
```

### `xrtNetUdpPacketDestroy`

销毁拥有型数据包；空指针视为空操作。

```c
void xrtNetUdpPacketDestroy(xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[udp](../../examples/network/udp/main.c) · 销毁数据包

```c
	xrtNetUdpPacketDestroy(pPacket);
```

### `xrtNetUdpPacketRemote`

返回数据包的借用远端地址。

```c
const xnetaddr* xrtNetUdpPacketRemote(const xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标数据包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 远端地址借用（包存活期间有效） | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp](../../examples/network/udp/main.c) · 远端地址

```c
		xrtNetUdpPacketRemote(pPacket),
```

### `xrtNetUdpPacketMeta`

返回数据包内拥有的接收元数据；空数据包返回空指针。

```c
const xnetdgrammeta* xrtNetUdpPacketMeta(const xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标数据包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元数据借用 | — |
| `NULL` | 无元数据 | 不设错误 |

#### 错误

- 无元数据返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp](../../examples/network/udp/main.c) · 接收元数据

```c
	if ( ((xrtNetUdpPacketMeta(pPacket)->Flags &
		  XNET_DGRAM_META_DESTINATION) == 0) ||
		 ((xrtNetUdpSendControlAvailable(pServer) &
		  XNET_DGRAM_CONTROL_SOURCE) == 0) ) {
```

### `xrtNetUdpPacketData`

返回数据包的借用连续载荷。

```c
cbytes xrtNetUdpPacketData(const xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标数据包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 载荷借用（包存活期间有效） | — |
| `NULL` | 空载荷 | 不设错误 |

#### 错误

- 空载荷返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[udp](../../examples/network/udp/main.c) · 载荷借用

```c
		(cstr)xrtNetUdpPacketData(pPacket));
```

### `xrtNetUdpPacketSize`

返回数据包载荷长度。

```c
size_t xrtNetUdpPacketSize(const xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标数据包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 载荷字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp](../../examples/network/udp/main.c) · 载荷长度

```c
		(int)xrtNetUdpPacketSize(pPacket),
```

### `xrtNetUdpPacketTruncated`

返回数据包是否只包含一个被截断的前缀。

```c
bool xrtNetUdpPacketTruncated(const xnetudppacket* pPacket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPacket` | 输入 | 非空 | 目标数据包 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否被截断 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 截断判断

```c
		if ( (pPacket == NULL) || !xrtNetUdpPacketTruncated(pPacket) ||
			 (xrtNetUdpPacketSize(pPacket) != 16u) ) {
```

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

### `xrtNetUdpSend`

复制发送到连接式 UDP 的固定 Peer。

```c
xnetresult xrtNetUdpSend(xnetudp* pUdp, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pData` | 输入 | 非空 | 待发送数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp](../../examples/network/udp/main.c) · 复制发送

```c
	if ( xrtNetUdpSend(pClient, "hello UDP", 9) !=
		 XNET_RESULT_OK ) {
```

### `xrtNetUdpSendTo`

复制发送到指定对端；空对端使用连接式 UDP 的固定 Peer。

```c
xnetresult xrtNetUdpSendTo(xnetudp* pUdp, const xnetaddr* pRemote, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 非空 | 目标对端 |
| `pData` | 输入 | 非空 | 待发送数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_future](../../examples/network/udp_future/main.c) · 复制发送到指定对端

```c
	if ( (pReply == NULL) || (xrtNetUdpSendTo(
		pServer,
		xrtNetUdpPacketRemote(pPacket),
		sReply,
		sizeof(sReply) - 1u
	) != XNET_RESULT_OK) || !exampleUdpFutureWait(pReply) ) {
```

### `xrtNetUdpSendVec`

聚集复制为一个数据报后发送，所有 Span 在返回前完成复制。

```c
xnetresult xrtNetUdpSendVec(xnetudp* pUdp, const xnetspan* pSpans, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pSpans` | 输入 | 非空数组 | 分片视图 |
| `iCount` | 输入 | > 0 | 分片数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 聚集发送

```c
	if ( xrtNetUdpSendVec(pClient, VecIn, 2) != XNET_RESULT_OK ) {
```

### `xrtNetUdpSendVecTo`

聚集复制为一个数据报后发送到固定 Peer。

```c
xnetresult xrtNetUdpSendVecTo(xnetudp* pUdp, const xnetaddr* pRemote, const xnetspan* pSpans, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 非空 | 目标对端 |
| `pSpans` | 输入 | 非空数组 | 分片视图 |
| `iCount` | 输入 | > 0 | 分片数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 聚集发送到指定对端

```c
	if ( xrtNetUdpSendVecTo(pServer, &ClientAddress, VecOut, 2) !=
		 XNET_RESULT_OK ) {
```

### `xrtNetUdpSendRef`

零复制发送；成功受理后在数据报离开队列时执行一次释放过程。

```c
xnetresult xrtNetUdpSendRef(xnetudp* pUdp, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pData` | 输入 | 非空 | 借用数据 |
| `iSize` | 输入 | > 0 | 字节数 |
| `pRelease` | 输入 | 非空 | 释放回调 |
| `pContext` | 输入 | 任意值 | 回调上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 零复制发送

```c
	if ( xrtNetUdpSendRef(pClient, "ref-zero-copy", 13,
		countRelease, &iReleases) != XNET_RESULT_OK ) {
```

### `xrtNetUdpSendRefTo`

零复制发送到固定 Peer。

```c
xnetresult xrtNetUdpSendRefTo(xnetudp* pUdp, const xnetaddr* pRemote, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 非空 | 目标对端 |
| `pData` | 输入 | 非空 | 借用数据 |
| `iSize` | 输入 | > 0 | 字节数 |
| `pRelease` | 输入 | 非空 | 释放回调 |
| `pContext` | 输入 | 任意值 | 回调上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 零复制发送到指定对端

```c
	if ( xrtNetUdpSendRefTo(pServer, &ClientAddress, "sref-zero-copy", 14,
		countRelease, &iReleases) != XNET_RESULT_OK ) {
```

### `xrtNetUdpSendTake`

接管 XRT 分配的数据并发送；失败时所有权仍属于调用方。

```c
xnetresult xrtNetUdpSendTake(xnetudp* pUdp, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pData` | 输入 | 非空 | 拥有的数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）
- 受理失败时所有权不转移

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 接管发送

```c
	if ( xrtNetUdpSendTake(pClient, pTakeIn, 10) == XNET_RESULT_OK ) {
```

### `xrtNetUdpSendTakeTo`

接管 XRT 分配的数据并发送到固定 Peer。

```c
xnetresult xrtNetUdpSendTakeTo(xnetudp* pUdp, const xnetaddr* pRemote, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 非空 | 目标对端 |
| `pData` | 输入 | 非空 | 拥有的数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）
- 受理失败时所有权不转移

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 接管发送到指定对端

```c
	if ( xrtNetUdpSendTakeTo(pServer, &ClientAddress, pTakeOut, 11) ==
		 XNET_RESULT_OK ) {
```

### `xrtNetUdpSendMsg`

复制发送带逐包控制的数据报；空远端使用连接式 UDP 的固定 Peer。

```c
xnetresult xrtNetUdpSendMsg(xnetudp* pUdp, const xnetaddr* pRemote, const xnetdgramcontrol* pControl, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 允许空 | 目标对端 |
| `pControl` | 输入 | 允许空 | 逐包控制 |
| `pData` | 输入 | 非空 | 待发送数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp](../../examples/network/udp/main.c) · 带控制发送

```c
	if ( xrtNetUdpSendMsg(
		pServer,
		xrtNetUdpPacketRemote(pPacket),
		&Control,
		"reply",
		5
	) != XNET_RESULT_OK ) {
```

### `xrtNetUdpSendMsgRef`

引用发送带逐包控制的数据报，终态执行一次释放过程。

```c
xnetresult xrtNetUdpSendMsgRef(xnetudp* pUdp, const xnetaddr* pRemote, const xnetdgramcontrol* pControl, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 允许空 | 目标对端 |
| `pControl` | 输入 | 允许空 | 逐包控制 |
| `pData` | 输入 | 非空 | 借用数据 |
| `iSize` | 输入 | > 0 | 字节数 |
| `pRelease` | 输入 | 非空 | 释放回调 |
| `pContext` | 输入 | 任意值 | 回调上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 带控制零复制发送

```c
	if ( xrtNetUdpSendMsgRef(pClient, NULL, NULL, "msgref-owned", 12,
		countRelease, &iReleases) != XNET_RESULT_OK ) {
```

### `xrtNetUdpSendMsgTake`

接管 XRT 分配的数据并按逐包控制发送；失败时所有权不转移。

```c
xnetresult xrtNetUdpSendMsgTake(xnetudp* pUdp, const xnetaddr* pRemote, const xnetdgramcontrol* pControl, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pRemote` | 输入 | 允许空 | 目标对端 |
| `pControl` | 输入 | 允许空 | 逐包控制 |
| `pData` | 输入 | 非空 | 拥有的数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 队列字节预算已满
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）
- 受理失败时所有权不转移

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 带控制接管发送

```c
	if ( xrtNetUdpSendMsgTake(pServer, &ClientAddress, &Control,
		pTakeOut, 9) == XNET_RESULT_OK ) {
```

### `xrtNetUdpSendBatch`

按前缀批量受理复制发送；受理计数输出不能为空。

```c
xnetresult xrtNetUdpSendBatch(xnetudp* pUdp, const xnetdgramsend* pItems, size_t iCount, size_t* pAccepted)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pItems` | 输入 | 非空数组 | 发送项数组 |
| `iCount` | 输入 | > 0 | 发送项数量 |
| `pAccepted` | 输出 | 非空 | 接收受理计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 全部受理 | — |
| `XNET_AGAIN` | 前缀受理后队列已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 仅前缀被受理，`pAccepted` 写出实际数量
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 批量受理发送

```c
		if ( (xrtNetUdpSendBatch(pClient, Batch, 3, &iAccepted) !=
			  XNET_RESULT_OK) || (iAccepted != 3) ) {
```

### `xrtNetUdpSendControlAvailable`

返回此 UDP 对象可用于 `SendMsg` 的逐数据报发送控制位。

```c
uint32 xrtNetUdpSendControlAvailable(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 控制位掩码 | 平台支持的逐包控制 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp](../../examples/network/udp/main.c) · 可用控制位

```c
		 ((xrtNetUdpSendControlAvailable(pServer) &
		  XNET_DGRAM_CONTROL_SOURCE) == 0) ) {
```

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

### `xrtNetUdpJoin`

只在 UDP Worker 内加入多播组。

```c
bool xrtNetUdpJoin(xnetudp* pUdp, const xnetaddr* pGroup, const xnetaddr* pInterface)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pGroup` | 输入 | 非空 | 组播组地址 |
| `pInterface` | 输入 | 非空 | 本地接口地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已加入 | — |
| `false` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用
- `xrt.net` / `SOCKET_OPTION` — 平台加入失败

#### 范例

[udp_multicast](../../examples/network/udp_multicast/main.c) · 加入组播组

```c
	pTask->bJoin = xrtNetUdpJoin(pTask->pUdp, &pTask->Group,
		&pTask->Iface);
```

### `xrtNetUdpLeave`

只在 UDP Worker 内离开多播组。

```c
bool xrtNetUdpLeave(xnetudp* pUdp, const xnetaddr* pGroup, const xnetaddr* pInterface)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pGroup` | 输入 | 非空 | 组播组地址 |
| `pInterface` | 输入 | 非空 | 本地接口地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已离开 | — |
| `false` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用
- `xrt.net` / `SOCKET_OPTION` — 平台离开失败

#### 范例

[udp_multicast](../../examples/network/udp_multicast/main.c) · 离开组播组

```c
	pTask->bLeave = xrtNetUdpLeave(pTask->pUdp, &pTask->Group, NULL);
```

### `xrtNetUdpMulticastLoop`

只在 UDP Worker 内设置多播回环。

```c
bool xrtNetUdpMulticastLoop(xnetudp* pUdp, bool bEnabled)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `bEnabled` | 输入 | — | 是否回环 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用
- `xrt.net` / `SOCKET_OPTION` — 平台设置失败

#### 范例

[udp_multicast](../../examples/network/udp_multicast/main.c) · 多播回环

```c
	pTask->bLoop = xrtNetUdpMulticastLoop(pTask->pUdp, true);
```

### `xrtNetUdpMulticastHopLimit`

只在 UDP Worker 内设置多播跳数。

```c
bool xrtNetUdpMulticastHopLimit(xnetudp* pUdp, int iHopLimit)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iHopLimit` | 输入 | > 0 | 跳数上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用
- `xrt.net` / `SOCKET_OPTION` — 平台设置失败

#### 范例

[udp_multicast](../../examples/network/udp_multicast/main.c) · 多播跳数

```c
	pTask->bHop = xrtNetUdpMulticastHopLimit(pTask->pUdp, 1);
```

### `xrtNetUdpMulticastInterface`

只在 UDP Worker 内选择多播发送接口，空接口恢复系统默认。

```c
bool xrtNetUdpMulticastInterface(xnetudp* pUdp, const xnetaddr* pInterface)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pInterface` | 输入 | 允许空 | 接口地址，空 = 系统默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用
- `xrt.net` / `SOCKET_OPTION` — 平台设置失败

#### 范例

[udp_multicast](../../examples/network/udp_multicast/main.c) · 多播接口

```c
	pTask->bIface = xrtNetUdpMulticastInterface(pTask->pUdp,
		&pTask->Iface);
```

## 关闭、错误与统计

`xrtNetUdpClose` 原子封闭新发送，等待已进入提交区的跨线程调用退出，停止接收，并排空所有已成功受理的发送。`xrtNetUdpAbort` 取消每个在途收发槽、丢弃尚未提交的发送并以 `XNET_RESULT_CANCELLED` 关闭；被丢弃的 ref/take 项仍执行唯一释放。两者均幂等，Abort 可以把尚未完成的普通关闭升级为异常关闭。关闭请求之后迟到的 completion 接收只负责归还缓冲和在途槽，不再投递 Receive 或 Error；全部收发槽、发送命令和预算归零前不会发布 `CLOSED`，Close 回调也严格执行一次。

UDP 专用结构化错误码为 `XNET_ERROR_UDP_CONFIG`、`CREATE`、`RECEIVE`、`RECEIVE_QUEUE`、`SEND`、`CLOSE`，域为 `xrt.net`。平台失败同时保留系统错误码。公开 API 的参数/OOM/提交失败写入当前执行上下文错误；Worker 内错误通过 `Error` 事件或终态 `xrtNetUdpError` 表达。

`xrtNetUdpStats` 返回无锁快照，包括收发包/字节、截断与丢包原因、可恢复错误、异步数据报错误与 MTU 更新、硬上限拒绝、当前/峰值发送预算、普通与错误拉取队列、两类消费 waiter，以及当前接收和当前/峰值发送槽数。`ReceivedBytes` 统计实际写入接收块的前缀；截断报文在各平台都只计可见前缀，不虚构已经被内核丢弃的尾部长度。多个字段之间不保证来自同一时刻，终态后可获得稳定的最终值。

### `xrtNetUdpClose`

停止接收，并在已受理发送全部终结后正常关闭。

```c
bool xrtNetUdpClose(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已请求关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp](../../examples/network/udp/main.c) · 正常关闭

```c
		(void)xrtNetUdpClose(pClient);
```

### `xrtNetUdpAbort`

取消在途 IO、丢弃发送队列并尽快关闭。

```c
bool xrtNetUdpAbort(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 异常关闭

```c
	(void)xrtNetUdpAbort(pServer);
```

### `xrtNetUdpState`

返回 UDP 当前状态的并发快照。

```c
xnetudpstate xrtNetUdpState(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举值 | 当前生命周期状态 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp](../../examples/network/udp/main.c) · 状态

```c
	while ( xrtNetUdpState(pUdp) != State ) {
```

### `xrtNetUdpLocal`

复制实际绑定的本地地址。

```c
bool xrtNetUdpLocal(const xnetudp* pUdp, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pAddress` | 输出 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp](../../examples/network/udp/main.c) · 本地地址

```c
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
```

### `xrtNetUdpPeer`

复制连接式 UDP 的固定 Peer，未连接时失败。

```c
bool xrtNetUdpPeer(const xnetudp* pUdp, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pAddress` | 输出 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已复制 | — |
| `false` | 未连接 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 未连接式 UDP

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 固定 Peer

```c
	if ( !xrtNetUdpPeer(pUdp, &Peer) || !xrtNetUdpConnected(pUdp) ) {
```

### `xrtNetUdpConnected`

返回 UDP 是否具有固定 Peer。

```c
bool xrtNetUdpConnected(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否连接式 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 连接判断

```c
		xrtNetUdpConnected(pUdp) ? 1 : 0);
```

### `xrtNetUdpError`

返回导致 UDP 终止的借用错误，正常关闭和未关闭时为空。

```c
const xerror* xrtNetUdpError(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 终止原因借用 | — |
| `NULL` | 正常关闭或未关闭 | 不设错误 |

#### 错误

- 无错误 — 正常关闭或未关闭返回 `NULL` 且不设置错误

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 终止错误

```c
		xrtNetUdpError(pUdp) == NULL ? "(none)" : "err");
```

### `xrtNetUdpQueued`

返回当前拉取接收队列中的数据包数量。

```c
size_t xrtNetUdpQueued(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 队列数据包数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 接收包数

```c
		while ( xrtNetUdpQueued(pServer) < 3u ) {
```

### `xrtNetUdpQueuedBytes`

返回当前拉取接收队列中的载荷字节数。

```c
size_t xrtNetUdpQueuedBytes(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 队列载荷字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 接收字节数

```c
		xrtNetUdpQueued(pUdp), xrtNetUdpQueuedBytes(pUdp),
```

### `xrtNetUdpQueuedErrors`

返回当前拉取错误队列中的条目数量。

```c
size_t xrtNetUdpQueuedErrors(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 队列错误条数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 错误条数

```c
		xrtNetUdpQueuedErrors(pUdp), xrtNetUdpQueuedErrorBytes(pUdp),
```

### `xrtNetUdpQueuedErrorBytes`

返回当前拉取错误队列中的负载前缀字节数。

```c
size_t xrtNetUdpQueuedErrorBytes(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 队列错误字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 错误字节数

```c
		xrtNetUdpQueuedErrors(pUdp), xrtNetUdpQueuedErrorBytes(pUdp),
```

### `xrtNetUdpPending`

返回当前发送队列占用的字节数。

```c
size_t xrtNetUdpPending(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 发送队列占用字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_send_tour](../../examples/network/udp_send_tour/main.c) · 发送占用

```c
	while ( (xrtNetUdpPending(pClient) != 0) ||
			(xrtNetUdpPending(pServer) != 0) ) {
```

### `xrtNetUdpPathMtu`

返回最近一次错误队列确认的路径 MTU，未知时为零。

```c
size_t xrtNetUdpPathMtu(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 路径 MTU；0 = 未知 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 路径 MTU

```c
		xrtNetUdpPathMtu(pUdp),
```

### `xrtNetUdpStats`

复制 UDP 并发统计。

```c
bool xrtNetUdpStats(const xnetudp* pUdp, xnetudpstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 统计

```c
	if ( !xrtNetUdpStats(pUdp, &Stats) ||
		 (Stats.SentPackets < 1u) || (Stats.ReceivedPackets < 1u) ) {
```

### `xrtNetUdpWait`

阻塞等待一个 UDP 条件；禁止从该 UDP 所属 Worker 调用。

```c
bool xrtNetUdpWait(xnetudp* pUdp, xnetudpwait Wait, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `Wait` | 输入 | — | 等待条件 |
| `iDeadline` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 条件达成 | — |
| `false` | 超时或取消 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用

#### 范例

[udp_errors](../../examples/network/udp_errors/main.c) · 阻塞等待条件

```c
	if ( (pUdp == NULL) || !xrtNetUdpWait(
		pUdp,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(3000000u),
		NULL
	) ) {
```

### `xrtNetUdpWritable`

阻塞等待发送队列能够原子接纳指定大小的数据报。

```c
bool xrtNetUdpWritable(xnetudp* pUdp, size_t iSize, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `iSize` | 输入 | — | 数据报字节数 |
| `iDeadline` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 可接纳 | — |
| `false` | 超时或取消 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用

#### 范例

[udp_batch](../../examples/network/udp_batch/main.c) · 阻塞等待可写

```c
		xrtNetUdpWritable(pClient, 64, xrtDeadlineAfter(3000000u),
			NULL) ? 1 : 0);
```

### `xrtNetUdpWorker`

返回 UDP 所属的借用 Worker。

```c
xnetworker* xrtNetUdpWorker(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Worker 借用 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 所属 Worker

```c
		xrtNetUdpWorker(pUdp), &Post, exampleWorkerTask, &Task) ||
```

### `xrtNetUdpSocket`

只在 UDP Worker 内返回借用 Socket，调用方不得关闭或接管 IO。

```c
xnetsocket xrtNetUdpSocket(xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 句柄 | Socket 借用（回调期间有效） | — |
| 无效句柄 | 非回调内 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · Socket 借用

```c
	pTask->bSocketOk = xrtNetUdpSocket(pTask->pUdp) != NULL;
```

### `xrtNetUdpData`

原子读取借用的用户数据快照，不延长指针目标生命周期。

```c
ptr xrtNetUdpData(const xnetudp* pUdp)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 用户数据快照 | — |
| `NULL` | 未设置 | 不设错误 |

#### 错误

- 无错误 — 未设置返回 `NULL` 且不设置错误

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 用户数据快照

```c
	pTask->bWorkerDataOk = xrtNetUdpData(pTask->pUdp) == &g_Tag;
```

### `xrtNetUdpSetData`

只在 UDP Worker 内替换用户数据。

```c
bool xrtNetUdpSetData(xnetudp* pUdp, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pUdp` | 输入 | 非空 | 目标 UDP |
| `pData` | 输入 | 任意值 | 新用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换 | — |
| `false` | 非回调内 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法- `XERR_STATE` — 非所属 UDP Worker 内调用

#### 范例

[udp_introspect](../../examples/network/udp_introspect/main.c) · 替换用户数据

```c
	pTask->bSetDataOk = xrtNetUdpSetData(pTask->pUdp, &g_Tag);
```

## 示例与发布门禁

完整回环与按接收目标地址回包示例位于 `examples/network/udp/main.c`，底层元数据和逐包发送控制能力协商示例位于 `examples/network/socket/main.c`，Future 请求响应示例位于 `examples/network/udp_future/main.c`，阻塞外观示例位于 `examples/network/udp_sync/main.c`，PMTU 与异步错误示例位于 `examples/network/udp_errors/main.c`。当前回归门禁覆盖 select、IOCP 与 io_uring，包含同步/完成式接收元数据、逐包发送控制、GSO/GRO、PMTU/错误队列、错误 Future/同步等待、零长度/截断/向量/批量报文、硬字节和包数上限、普通与错误队列溢出策略、completion 并发发送槽、OOM 恢复、真实 IPv4 组播回环、多生产者 Send/Close/Abort 线性化、Future 取消/关闭竞态、同步终态竞争、协程挂起恢复、控制值复制、所有权唯一释放、单头文件真实收发和裁剪依赖。
