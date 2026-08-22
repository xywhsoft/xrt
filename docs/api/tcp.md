# TCP 传输 API

## 分层

`XRT_FEATURE_NET_TCP` 依赖 `XRT_FEATURE_NET_ENGINE`，公开头文件为 `<xrt/tcp.h>`。这一层提供数字地址 TCP 连接、监听、字节流、硬背压和异步回调，不隐式执行 DNS、TLS、代理、协议解析或同步等待。

这种边界允许上层按需组合：

- 数字地址客户端直接使用 `xrtNetStreamConnect`。
- 主机名客户端先使用 Resolver，再把选定地址交给 TCP。
- HTTP、WebSocket、RPC 和自定义协议直接消费 `xnetbuf`。
- `XRT_FEATURE_NET_TCP_FUTURE` 在同一传输契约上提供 Future；`XRT_FEATURE_NET_TCP_SYNC` 提供阻塞便利层；协程直接使用通用 Future Await。三条路径不把等待状态机复制进核心 Stream。

TCP 不把 IOCP 设为硬依赖。只启用 `XRT_FEATURE_NET_PORT_SELECT` 时，连接、监听、收发、背压和关闭均可完整工作；同时启用 IOCP 后，Engine 可选择完成式后端，公开语义不变。

`XRT_FEATURE_NET_TCP_FILE` 是独立可裁剪层，依赖 TCP 与 File。它只为明文 TCP
增加文件区间发送，不把文件模块拖入普通 TCP，也不会被 TLS 隐式使用。

## 状态

```c
typedef enum xnetstreamstate {
	XNET_STREAM_CONNECTING = 0,
	XNET_STREAM_OPEN,
	XNET_STREAM_CLOSING,
	XNET_STREAM_CLOSED
} xnetstreamstate;

typedef enum xnetlistenerstate {
	XNET_LISTENER_OPEN = 0,
	XNET_LISTENER_CLOSING,
	XNET_LISTENER_CLOSED
} xnetlistenerstate;
```

状态只向前推进，`CLOSED` 是唯一终态。创建函数返回的对象包含一个调用方引用；运行时另持有一个内部引用，直到唯一 Close 回调结束。调用方必须先请求关闭，再使用 `xrtNetStreamDestroy` 或 `xrtNetListenerDestroy` 释放自己的引用。

`CLOSED` 发布前，Socket、缓冲、在途操作和对象对 Engine 的活动占用都已经终结。看到 `CLOSED` 后可以立即释放调用方引用并销毁 Engine；Close 回调可能仍在 Worker 上收尾，Engine 销毁会等待该 Worker 正常退出。

`Ref`/`Destroy` 可以跨线程使用；`Ref` 不允许从零计数复活对象，并在引用计数达到 `INT32_MAX` 时失败：

```c
xnetstream* xrtNetStreamRef(xnetstream* pStream);
void xrtNetStreamDestroy(xnetstream* pStream);
xnetlistener* xrtNetListenerRef(xnetlistener* pListener);
void xrtNetListenerDestroy(xnetlistener* pListener);
```

## 回调与线程

```c
typedef struct xnetstreamevents {
	void (*Open)(xnetstream* pStream, ptr pData);
	void (*Read)(xnetstream* pStream, xnetbuf* pBuffer, ptr pData);
	void (*End)(xnetstream* pStream, ptr pData);
	void (*HighWater)(xnetstream* pStream, size_t iQueued, ptr pData);
	void (*LowWater)(xnetstream* pStream, size_t iQueued, ptr pData);
	void (*Drain)(xnetstream* pStream, ptr pData);
	void (*Close)(xnetstream* pStream, xnetresult Result,
		const xerror* pError, ptr pData);
} xnetstreamevents;
```

一个 Stream 的全部回调都在其所属 Worker 上串行执行，不会并发进入同一 Stream。`xrtNetStreamWorker` 返回 Stream Worker，`xrtNetListenerWorker` 返回 Listener Worker；回调内可用 `xrtNetWorkerIsCurrent` 验证线程归属。

事件含义：

- `Open`：连接和地址已经发布，可以查询本地、远端地址并开始协议状态机。
- `Read`：收到新字节。`pBuffer` 借用 Stream 的累积接收缓冲，只在回调期间操作。
- `End`：对端发送 FIN，读方向结束；写方向仍可继续。
- `HighWater`：发送预算第一次达到或越过高水位。
- `LowWater`：越过高水位后，预算回落到低水位或以下。
- `Drain`：发送预算回到零；一次新的非空队列周期最多产生一次 Drain。若 `LowWater` 回调内立即加入新数据，Drain 会等重入发送结束后再按最终空队列合并发布，不会从嵌套写入和外层写入重复进入。
- `Close`：唯一终态。`pError` 只在回调期间借用；正常关闭为 `XNET_RESULT_OK, NULL`。

Read 回调必须消费已处理字节，通常使用 `xrtNetBufConsume`、`xrtNetBufRead` 或协议解析器。允许保留未完成帧的前缀；缓冲达到 `ReadLimit` 时 Stream 自动停止继续接收，消费后低于上限会自动恢复。长期不消费会保持有界读背压，但不会因一次完整缓冲而断开连接。

```c
typedef struct xnetlistenerevents {
	bool (*Accept)(xnetlistener* pListener,
		xnetstream* pStream, ptr pData);
	void (*Error)(xnetlistener* pListener,
		const xerror* pError, ptr pData);
	void (*Close)(xnetlistener* pListener, ptr pData);
} xnetlistenerevents;
```

`Accept` 在新 Stream 的目标 Worker 上执行。返回 `true` 表示接受连接，并接管传入的一个调用方引用；随后才发布 Stream `Open`。返回 `false` 表示拒绝，XRT 静默关闭 Stream，调用方不得保存或释放该引用。

`Error` 和 Listener `Close` 在 Listener 所属 Worker 上执行。`Error` 表示可观察的接受或初始化失败，不等同于用户拒绝；错误对象只在回调期间借用。

## 配置

```c
typedef struct xnetstreamconfig {
	size_t ReadSize;
	size_t ReadLimit;
	size_t WriteHighWater;
	size_t WriteLowWater;
	size_t WriteLimit;
	uint64 ConnectTimeout;
	xnetstreamreadmode ReadMode;
	bool NoDelay;
	bool KeepAlive;
} xnetstreamconfig;

void xrtNetStreamConfigInit(xnetstreamconfig* pConfig);
```

默认值为：读块建议值 2 KiB、接收硬上限 1 MiB、写高水位 256 KiB、低水位 64 KiB、写硬上限 1 MiB、连接超时 30 秒、`ReadMode=XNET_STREAM_READ_ADAPTIVE`、启用 `TCP_NODELAY`。

`ReadSize` 是每次 Reserve 的最小建议，不是每连接常驻缓冲。空闲 Stream 不持有固定 8K；实际接收块来自所属 Worker 的自适应共享池。`ReadLimit` 和 `WriteLimit` 是硬边界，不是通知阈值。

完成式后端有三种读取策略：`ADAPTIVE` 默认在空闲时用零载荷探针等待可读；真实读取填满块后查询内核接收队列，仅在仍有字节可立即读取时连续预投递，队列排空或发生短读后立刻回到探针。这个边界既保留长流吞吐，也避免恰好填满一个块的短突发让空闲连接长期持有缓冲。`DIRECT` 始终预投递真实接收缓冲，减少持续大流量下的一次完成往返，代价是每个空闲 Stream 持有一个接收块。`PROBE` 每次真实接收前都先探测，常驻载荷内存最低，适合长连接和稀疏消息。completion 后端不具备 `READ_PROBE` 能力时，三者都安全退回直接接收；readiness 后端本身不预投递载荷缓冲，因此不受该选项影响。

```c
typedef struct xnetlistenconfig {
	xnetaddr Address;
	xnetstreamconfig Stream;
	uint64 Affinity;
	uint32 AcceptConcurrency;
	uint32 AcceptQueueLimit;
	int Backlog;
	bool ReuseAddress;
	bool ReusePort;
	bool ExclusiveAddress;
} xnetlistenconfig;

void xrtNetListenConfigInit(xnetlistenconfig* pConfig);
```

`Address` 支持 IPv4/IPv6 和端口 0；创建成功后用 `xrtNetListenerLocal` 查询系统分配的端口。`Affinity` 选择 Listener Worker。完成式后端预投递 `AcceptConcurrency` 个独立 Accept 槽，终态直接由槽身份 O(1) 回收；readiness 后端把该值作为每次可读事件的批量接受预算。接受后的 Stream 按 Worker 轮转分发。

`AcceptQueueLimit` 是拉取模式尚未领取连接数的硬上限，默认 256，不能为零。队列直接复用已接受 Stream 内的链接，不为每个连接另分配队列节点。达到上限的新连接会被静默拒绝并计入 `Rejected`，因此慢速 Accept 消费者不会造成无界内存增长。

跨 Worker 初始化接受结果时使用短生命周期分发节点。该节点不常驻 Listener 或
Stream，终态后归还目标 Worker 的统一小节点缓存；缓存总量由 Engine 的
`NodeCacheBytes` 控制，关闭缓存时直接回到全局堆。

默认策略在所有平台都拒绝第二个活动绑定：Windows 默认启用 `ExclusiveAddress`，其他平台默认启用 `ReuseAddress` 以同时保留服务重启能力。需要多个进程或 Listener 共享同一端口时，调用方必须显式关闭默认选项并启用 `ReusePort`。

`ExclusiveAddress` 不能和 `ReuseAddress`、`ReusePort` 同时启用。`ReusePort` 是否可用仍由平台 Socket 层决定，失败会返回结构化错误。

## 连接与监听

```c
xnetstream* xrtNetStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pConfig,
	const xnetstreamevents* pEvents,
	ptr pData
);

xnetlistener* xrtNetListen(
	xnetengine* pEngine,
	const xnetlistenconfig* pConfig,
	const xnetlistenerevents* pEvents,
	const xnetstreamevents* pStreamEvents,
	ptr pData
);
```

两者都要求 Engine 已运行。`Connect` 返回后处于 `CONNECTING`，允许预先发送；成功后回调 `Open`，超时、取消或失败直接进入 `Close`，不会先发布 Open。任何连接终态都会立即撤销连接超时 Timer 及其 Stream 引用，不会让已经失败的连接继续占用 Engine 定时器资源。调用方可以在 `Connect` 返回后立即关闭或 Abort：运行时会先收回尚未执行的启动任务，再发布唯一终态，迟到的启动任务不会重新打开对象或访问已释放内存。

`Listen` 同步完成 Socket 创建、选项、Bind、Listen 和实际地址查询，因此返回非空时端口已经占用；Accept 的预投递异步开始。配置和事件表都在创建时复制，调用方不需要保持其内存。

Listener 有两种明确的接受模式：设置 `Events.Accept` 是推送模式；不设置则是有界拉取模式。两种模式不能混用。拉取模式可以非阻塞领取已经排队的连接：

```c
xnetstream* xrtNetListenerAccept(xnetlistener* pListener);
```

返回空指针表示当前没有可领取连接，关闭状态可用 `xrtNetListenerState` 区分。成功返回会把队列持有的一个 Stream 引用转移给调用方，最终必须先请求关闭，再调用 `xrtNetStreamDestroy`。同一个 Listener 的直接轮询与异步 Accept 可以按阶段顺序使用，但不能作为多个并发消费者争抢同一队列；已有异步等待者时直接轮询会返回状态错误。

`xrtNetListen` 的 `pData` 只属于 Listener。拉取接受得到的 Stream 不继承该指针；调用方可以继续使用拉取接收，或在 Stream 所属 Worker 上调用 `xrtNetStreamSetEvents`，为每条连接安装独立事件和数据。推送模式也应在 `Accept` 回调中完成同样的每连接接管。

## 文件区间发送

```c
xnetresult xrtNetStreamSendFile(
	xnetstream* stream,
	xfile file,
	uint64 offset,
	size_t size
);
```

`xrtNetStreamSendFile` 把文件区间作为发送队列中的普通有序段。它与之前和之后的
`Send`、`SendRef`、`SendTake`、`SendBuffer` 共用同一 FIFO 顺序、`WriteLimit`、
高低水位、Drain、取消和关闭契约。区间必须完整落在调用时的文件大小内；零长度是
无操作；超过硬发送预算返回 `XNET_RESULT_AGAIN`。

成功受理时，XRT 复制原生文件句柄，调用方可以立即 `xrtClose(file)`。失败不接管
文件对象或句柄。复制句柄会在区间完整发送、Abort、错误或关闭清理时准确释放一次。
Windows IOCP 使用异步 `TransmitFile`，Linux io_uring 在内核支持时使用异步
`IORING_OP_SPLICE`，Linux readiness 使用非阻塞 `sendfile`，macOS 与 FreeBSD 使用
各自 `sendfile`。完成式后端运行时不具备文件发送 opcode 时，不发布对应能力。

该入口仅用于明文 TCP。TLS 必须把文件数据读入用户态完成加密，HTTP/TLS 组合层应
选择异步文件正文路径，不能把明文 `sendfile` 绕过加密层。

## TCP Server

`XRT_FEATURE_NET_TCP_SERVER` 在 Listener 之上提供面向服务端的聚合层，公开头文件为
`<xrt/tcp_server.h>`。它不复制 TCP 收发状态机，只负责多端点绑定、Listener 生命周期、
Accept 汇聚、服务端分发和聚合统计。只需要一个低级 Listener 的代码仍可直接使用
`xrtNetListen`；需要双栈、多 worker 或稳定服务端手感时使用 `xrtNetServerStart`：

Server 在全部 Listener 退出后先发布 `XNET_SERVER_CLOSED`，随后完成关闭 Future 并调用
唯一 `Close` 回调，因此回调内必然能观察终态；其他线程只看到 `CLOSED` 时，Close 通知
仍可能正在 Worker 上发布。需要把应用资源释放与通知完成线性化时，应等待 Close 回调或
Server 关闭 Future，不能把状态轮询当作回调屏障。

```c
typedef struct xnetserverconfig {
	xnetlistenconfig Listen;
	const xnetlistenconfig* Additional;
	size_t AdditionalCount;
	uint32 AcceptQueueLimit;
	xnetservermode Mode;
	bool SharedPort;
} xnetserverconfig;

xnetserver* xrtNetServerStart(
	xnetengine* engine,
	const xnetserverconfig* config,
	const xnetserverevents* events,
	const xnetstreamevents* streamEvents,
	ptr data
);
```

`Listen` 是端点 0，`Additional[0..AdditionalCount)` 依次映射为后续逻辑端点。
启动函数在返回前同步完成全部绑定；任意端点失败都会关闭已经创建的 Listener，并以
`XNET_ERROR_SERVER_START` 包装原始 Socket、配置或平台错误，不会留下部分可用 Server。
`Additional` 只在启动调用期间借用，其余配置、事件表和实际本地地址都由 Server 保存。

`SharedPort` 用于双栈或多地址动态端口：端点 0 使用端口 0 成功绑定后，后续端点继承
同一个实际端口；显式非零端口必须彼此一致。每个连接通过 Accept 回调的
`iEndpoint` 和上层协议保存的端点索引保持逻辑归属，不需要按本地地址反查配置。

`XNET_SERVER_SHARED` 为每个逻辑端点创建一个 Listener。`XNET_SERVER_REUSE_PORT`
为每个端点、每个 Engine Worker 创建一份 Listener，并自动启用本地分发、关闭独占绑定、
启用 `ReusePort`；平台不支持时启动原子失败。该模式用于内核级负载分发，不是 Windows
或其他缺少 reuse-port 语义平台上的可移植默认值。

Server 同样提供推送和拉取两种互斥使用方式。设置 `xnetserverevents.Accept` 时，回调
在已接受 Stream 的目标 Worker 上执行；返回 `true` 会接管一个 Stream 引用，返回
`false` 由 Server 拒绝并回收。未设置 Accept 回调时，`AcceptQueueLimit` 是跨全部
Listener 的硬队列上限，`xrtNetServerAccept` 从统一 FIFO 非阻塞领取连接。队列满时
拒绝新连接而不增长内存。

启用 `XRT_FEATURE_NET_TCP_SERVER_FUTURE` 后，`xrtNetServerAcceptAsync` 等待同一
FIFO；启用 `XRT_FEATURE_NET_TCP_SERVER_SYNC` 后，`xrtNetServerAcceptWait` 复用
该 Future 和统一 deadline/cancel 桥。Future 取消只撤销本次等待，不关闭 Server；
同步入口不能从任意 Engine Worker 调用。三条路径共享同一连接所有权和错误语义。

`xrtNetServerClose` 原子停止全部 Listener，丢弃尚未领取的 Stream，并在 Listener、
分发任务和等待者全部退出后发布唯一 Close。已经由应用接管的 Stream 独立存活，Server
不会擅自关闭业务连接。调用方应先关闭 Server，再释放自己的 `xnetserver` 引用；
`xrtNetServerListener` 返回的低层 Listener 是额外引用，必须独立销毁。

`xrtNetServerEndpointCount`、`xrtNetServerLocal` 和
`xrtNetServerListenerCount` 暴露稳定拓扑；`xrtNetServerStats` 汇总接受、拒绝、
错误、队列、等待者和已关闭 Listener。完整示例位于
`examples/network/tcp_server/main.c`，双端点、reuse-port、关闭重入、队列溢出、
OOM、Future、同步和单头真实收发均有独立回归测试。

## 托管主机连接

启用 `XRT_FEATURE_NET_TCP_DIAL` 后，主机名解析和多地址连接策略作为独立裁剪层建立在 Resolver 与数字地址 TCP 之上：

```c
typedef struct xnetdialconfig {
	xnetstreamconfig Stream;
	xnetfamily Family;
	uint64 Affinity;
	uint64 Timeout;
	uint64 FallbackDelay;
	uint32 MaxAttempts;
} xnetdialconfig;

xnetdial* xrtNetDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetdialproc pDone,
	ptr pDoneData
);
```

`xrtNetDialConfigValid` 完整校验 Dial 策略及其嵌套 `xnetstreamconfig`，适合在 HTTP、
WebSocket、RPC 等组合库复制配置前复用同一口径。成功不修改线程错误；失败返回
`false` 并发布具体参数错误。上层库不应复制 XRT 私有校验逻辑，否则默认值和边界会
随底层演进发生分叉。

Dial 的默认总超时为 30 秒、候选间隔为 250 毫秒、最多尝试 8 个地址。`Timeout` 从 API 调用时开始计算，包含排队、DNS 和所有连接候选；零表示不设置总截止时间。单个候选仍受 `Stream.ConnectTimeout` 约束。`FallbackDelay == 0` 会立即并发启动所有允许的候选，适合调用方明确控制地址数量的低延迟场景。

Resolver 返回顺序决定首选地址族；Dial 保持各地址族内部顺序，并交错 IPv6/IPv4 候选。第一个候选立即启动，仍未成功时按 `FallbackDelay` 逐个增加并发候选；候选同步失败会立即补充下一个地址。`MaxAttempts` 是复制到 Dial 内的硬上限，既限制连接放大，也限制每次操作的地址内存。

Dial 直接从 Resolver 的不可变结果交错复制一份有界候选数组并写入目标端口，不创建中间地址列表。Dial 状态与拥有的主机名使用单块分配；每个活动连接候选只建立一个短生命周期节点，并在释放 Stream 持有后归还所属 Worker 的统一小节点缓存。缓存受 Engine `NodeCacheBytes` 硬上限约束，关闭缓存后语义不变并直接使用全局堆。

`xrtNetDial` 只在入口调用期间借用 Resolver；成功创建的 `xnetresolveop` 独立承担后续解析生命周期。Resolver 的销毁操作会在尚有解析任务时失败，调用方应在全部解析工作排空后销毁它。Dial 取消、超时或提前失败时会从尚未开始的 Resolver 回调原子分离；不可中断的系统解析函数可以继续在 Resolver Worker 中收尾，但不再持有 Dial 或 Engine。Dial 的 Timer、候选和 Engine 活动占用全部归零后才发布状态与完成回调，因此失败或取消回调本身就是稳定清理屏障；没有其他活动对象时可以立即销毁 Engine，不需要重试。成功交付的 Stream 是调用方拥有的独立活动对象，仍应先关闭并释放。

完成回调只执行一次，并在 `Affinity` 选定的网络 Worker 上运行。它不会从
`xrtNetDial` 的调用栈直接重入；从该 Worker 内提交时，回调一定在当前 Worker
任务返回后发生。从其他线程提交时，Worker 可以与提交线程并发执行，因此回调
可能早于调用方保存返回值。回调必须使用参数中的 `pDial` 识别操作，且该参数
本身是借用引用；需要跨回调保存时先调用 `xrtNetDialRef`。成功时满足以下条件：

- Stream 已经完成用户 `Open` 回调且状态为 `XNET_STREAM_OPEN`。
- `pStream` 的一个调用方引用转移给完成回调，Dial 不再拥有它。
- `xrtNetDialState(pDial)` 已经是 `XNET_DIAL_CONNECTED`。

`xrtNetDial` 非空返回值包含一个调用方引用。只有调用方已经同步取得该引用后，
才能在完成回调内释放它；不能把借用的回调参数直接当成一份额外引用销毁。若
回调需要独立保留 Dial，应在回调内先 `xrtNetDialRef`，再在使用结束后销毁。

失败或取消时 `pStream == NULL`，`pError` 是只在回调期间借用的结构化错误。Dial 会保存终态错误，保留 Dial 引用时可继续通过 `xrtNetDialError` 借用。解析错误以 `XNET_ERROR_DIAL_RESOLVE` 包装 Resolver 根因；候选错误以 `XNET_ERROR_DIAL_CONNECT` 包装最后一个端点错误；OOM、超时和取消保留对应 `xerrkind`，不会统一降级为普通 IO 错误。

```c
bool xrtNetDialCancel(xnetdial* pDial);
xnetdialstate xrtNetDialState(const xnetdial* pDial);
const xerror* xrtNetDialError(const xnetdial* pDial);
bool xrtNetDialStats(const xnetdial* pDial, xnetdialstats* pStats);
```

取消和成功、失败共用同一个原子终态仲裁门。`xrtNetDialCancel` 返回 `true` 表示取消已经赢得竞争，Resolver 操作和全部未公开候选将被取消，唯一完成结果保证为 `XNET_RESULT_CANCELLED`；返回 `false` 表示取消没有取得所有权，既不会改写已经保留的获胜连接，也不会产生“取消已受理但随后成功”的矛盾结果。重复取消或对象已经终结时同样返回 `false`。获胜候选会在公开用户 `Open` 前保留成功终态，因此 `Open` 回调内再取消必然返回 `false`。

统计包含可用地址数、已经启动和失败的候选数、当前/峰值并发数以及获胜索引。它是跨线程无锁快照，只有 `HasWinner == true` 时 `WinnerIndex` 才有效。多个计数字段不承诺来自同一个瞬间。

## 发送与背压

```c
xnetresult xrtNetStreamSend(
	xnetstream* pStream, const void* pData, size_t iSize);
xnetresult xrtNetStreamSendVec(
	xnetstream* pStream, const xnetspan* pSpans, size_t iCount);
xnetresult xrtNetStreamSendRef(
	xnetstream* pStream, const void* pData, size_t iSize,
	xnetreleaseproc pRelease, ptr pContext);
xnetresult xrtNetStreamSendRefs(
	xnetstream* pStream, const xnetref* pRefs, size_t iCount);
xnetresult xrtNetStreamSendTake(
	xnetstream* pStream, ptr pData, size_t iSize);
xnetresult xrtNetStreamSendBuffer(
	xnetstream* pStream, xnetbuf* pBuffer);
```

- `Send` 在返回前复制一个连续字节区。
- `SendVec` 检查总长度溢出，并在返回前聚集复制多个 Span。
- `SendRef` 零复制引用外部数据，非空数据必须提供释放过程；零长度是无操作，不转移所有权，也不调用释放过程。
- `SendRefs` 原子受理多个零复制引用，适合 Header、固定响应片段和文件块的聚集发送；空片段被忽略，非空片段分别在离队时执行一次释放过程。
- `SendTake` 接管由 `xrtMalloc` 家族取得的非空数据，最终调用 `xrtFree`；`NULL, 0` 是无操作，非空指针配零长度属于参数错误且不转移所有权。
- `SendBuffer` 在 Stream 所属 Worker 上接管完整 `xnetbuf` 块链。它只建立发送视图元数据，不复制载荷；适合把 Read 回调缓冲直接转发到另一个方向，或连接 TLS、HTTP 等协议队列。

只有返回 `XNET_RESULT_OK` 才接受数据和转移 ref/take/buffer 所有权。`SendRefs` 和 `SendBuffer` 都是全有或全无操作：元数据或任一发送视图建立失败时，不会释放外部片段、清空源缓冲或留下部分队列和预算。`SendBuffer` 成功后源缓冲恢复为空但保留原缓冲池，可立即复用；其中的借用块仍要求调用方保持载荷存活到发送队列离队。`XNET_RESULT_AGAIN` 表示加入后会超过 `WriteLimit`；调用方保留数据，可以等待 `LowWater` 或 `Drain` 后重试。`XNET_RESULT_CLOSED` 表示写半关闭、关闭或 Abort 已经封闭新发送。`XNET_RESULT_ERROR` 表示参数、线程归属、活动写预留、溢出、OOM 或命令提交失败，详细原因在当前执行上下文的 `xerror`。

发送预算在 API 入口原子占用，包含已经受理但尚未链接到 Worker 缓冲的跨线程命令。关闭过程先封闭新发送，再等待已经进入提交区的调用完成，因而不会越过一个已经线性化成功的 Send。

Worker 回调内的 `Send`/`SendVec` 直接复制到本 Worker 的拥有型发送缓冲；缓冲池已经有合适缓存块时不依赖全局分配器，也不再建立“载荷节点 + 引用块”两级对象。普通 Send 的其他线程调用通过 Engine 命令队列提交，使用独立拥有节点保证调用返回后源数据即可失效。`SendBuffer` 只能在 Stream Worker 内调用，因为缓冲池和源块链都是线程归属对象；跨线程调用返回 `XNET_RESULT_ERROR/XERR_STATE`，不会接管源缓冲。需要跨线程发送时使用复制、ref、refs 或 take 路径，或者先把操作投递到 Stream Worker。

跨线程复制发送的小载荷节点，以及 `SendRef`、`SendRefs`、`SendTake`、`SendBuffer`
所需的不超过 1 KiB 的短生命周期元数据，共享 Stream 所属 Worker 的小节点缓存。
较大载荷和元数据直接走全局堆，避免为了少数大请求扩大常驻缓存。节点在释放外部
所有权并保存必要生命周期引用后回收，不会发生回收后读取或重复释放。

TCP 是无消息边界的字节流，核心不会把一次 Read 回调解释成一条业务消息，也不会内置聊天室、主题或广播组。需要广播时，应用应先用 Line/Length Frame 建立明确消息边界；连接注册表为每个成员持有一个 Stream 引用，在短锁内建立稳定快照，离开锁后分别发送并释放快照引用。每个成员的 `AGAIN`、关闭和错误必须独立处理，不能因为部分成员成功便丢弃慢成员的发送所有权，也不能用无限队列掩盖背压。具有消息语义的 WebSocket 连接组与聚合发送状态机属于 `xws` 扩展层，不在 TCP 或 XRT WebSocket 协议核心中重复实现。

```c
size_t xrtNetStreamPending(const xnetstream* pStream);
size_t xrtNetStreamWriteLimit(const xnetstream* pStream);
size_t xrtNetStreamWritable(const xnetstream* pStream);
```

`Pending` 返回当前已占用的发送预算，不等同于内核 Socket 缓冲字节数。成功短写或 completion 每确认一段前缀，值就按实际发送字节下降；外部 ref/take 的释放过程仍只在所属完整片段离队后调用。`WriteLimit` 返回创建时固定的发送硬上限，供上层协议判断一条原子输出是否永远能够容纳。`Writable` 返回 `WriteLimit - Pending` 的并发快照，供 HTTP、TLS 和自定义协议限制下一段输出；它只是观察值，实际受理结果始终以 `Send` 返回值为准。

## 读取控制与关闭

```c
void xrtNetStreamPause(xnetstream* pStream);
bool xrtNetStreamResume(xnetstream* pStream);
bool xrtNetStreamShutdownWrite(xnetstream* pStream);
bool xrtNetStreamClose(xnetstream* pStream);
bool xrtNetStreamAbort(xnetstream* pStream);
bool xrtNetListenerClose(xnetlistener* pListener);
```

`Pause` 立即阻止预投递新的接收；完成式后端已经提交的一次接收仍可能到达并产生 Read。
`Resume` 只能用于读方向仍开放的 Stream，并唤醒所属 Worker。恢复使用 Stream 内嵌命令，
不分配内存；并发调用会合并为一次待执行驱动，但不会丢失最后一次恢复请求。

`ShutdownWrite` 封闭新发送，排空已接受队列后发送 TCP FIN，读方向继续工作。典型“发送完请求并等待响应”直接使用该函数。

`Close` 封闭读写入口、停止新读取、排空已经接受的发送队列，然后正常关闭 Socket。它不等待对端 FIN；需要该语义时先 `ShutdownWrite`，在 `End` 后调用 `Close`。

`Abort` 取消在途 IO、丢弃发送队列并使用异常关闭。被丢弃的 ref/take 数据仍执行释放过程。三种操作都幂等；Abort 可以把尚未完成的普通关闭升级为异常关闭，即使 Close 与 Abort 已经按顺序进入同一 Worker 的生命周期命令队列，最终结果仍为 `XNET_RESULT_CANCELLED`。

Listener `Close` 停止接受新连接，取消全部预投递 Accept，并在其终态到达后发布唯一 Close。关闭使用对象内预留命令，不受公开命令容量和运行期分配失败影响；对有效 Listener 的首次和重复关闭都返回 `true`。已经交给 Accept 回调并返回 `true` 的 Stream 独立存活。

## 拉取读取与 Future

不设置 `Events.Read` 时，Stream 进入拉取模式。核心 TCP 层提供 Worker 内零额外状态机的读取接口：

```c
size_t xrtNetStreamAvailable(const xnetstream* pStream);
const xnetbuf* xrtNetStreamBuffer(xnetstream* pStream);
size_t xrtNetStreamRead(xnetstream* pStream, void* pOutput, size_t iSize);
size_t xrtNetStreamConsume(xnetstream* pStream, size_t iSize);
```

`Available` 是可跨线程查询的字节数快照。`Buffer`、`Read` 和 `Consume` 只允许在所属 Worker 内调用；`Buffer` 返回的只读借用不能保存，也不能绕过 Stream 直接消费。`Read` 与 `Consume` 会更新 `BufferedBytes`、解除 `ReadLimit` 背压并继续驱动后端接收。

启用 `XRT_FEATURE_NET_TCP_FUTURE` 后，可以从任意线程登记统一等待：

```c
typedef enum xnetstreamwait {
	XNET_STREAM_WAIT_OPEN = 0,
	XNET_STREAM_WAIT_READ,
	XNET_STREAM_WAIT_WRITE,
	XNET_STREAM_WAIT_DRAIN,
	XNET_STREAM_WAIT_CLOSE
} xnetstreamwait;

xfuture* xrtNetListenerAcceptAsync(xnetlistener* pListener);
xfuture* xrtNetStreamWaitAsync(xnetstream* pStream, xnetstreamwait Wait);
xfuture* xrtNetStreamRecvAsync(xnetstream* pStream, size_t iMaxBytes);
```

等待条件是水平条件：连接已经打开、已有可读字节、当前可写、发送预算为零或对象已经关闭时，新登记的 Future 可以立即结束。`OPEN` 在连接成功后完成，连接超时或失败会保留原始结构化连接错误；`READ` 和 `RecvAsync` 要求 Stream 没有 Read 回调，避免两个消费者竞争同一字节流；写、Drain 和 Close 等待可以和推送读取并用。

`AcceptAsync` 只用于没有 Accept 回调的拉取 Listener。它与非阻塞 Accept 消费同一个有界 FIFO 队列；取消只撤销本次等待，不停止 Listener，也不丢弃随后到达的连接。成功值是 Future 持有的 `xnetstream*`，只在销毁 Future 前保持该引用；需要让 Stream 独立存活时先调用 `xrtNetStreamRef`。Listener 关闭会回收所有未领取 Stream，并使待定和迟到的 Accept Future 进入 `CLOSED`。

`RecvAsync` 按登记顺序消费当前可用的前缀，`iMaxBytes == 0` 表示取本次可用的全部字节。成功值是 Future 持有的 `xnetbytes`；`xrtNetBytesView` 返回只在继续持有结果引用时有效的借用视图，数据不是零结尾文本。需要让结果独立于 Future 存活时先调用 `xrtNetBytesRef`，最后以 `xrtNetBytesDestroy` 释放。结果内存分配失败时 Future 进入 `FAILED/XERR_MEMORY`，原接收字节保持未消费，可在恢复后重试。

`examples/network/tcp_future/main.c` 可直接运行，连续演示 Accept、Open、Read、Drain、Recv 和 Close Future；其中 Read 只观察水平可读条件，随后由 Recv 消费字节，Drain 只表示本地发送预算归零。

正常 FIN 且没有剩余字节时，接收 Future 进入 `CLOSED`；网络错误进入 `FAILED`；Stream 异常取消进入 `CANCELLED`。取消一个等待 Future 只移除本次等待，不关闭、半关闭或中断 Stream。Close 等待在正常关闭时解析成功，即使登记发生在对象进入 `CLOSED` 之后。

通用 Future 已经提供协程 Await，因此 TCP 不导出网络专用 Co API。调度协程直接等待 TCP Future；协程取消只结束本次 Await，不会隐式取消可能共享的 Future。不再需要独占网络操作时，应在清理路径显式调用 `xrtFutureCancel`。取消成功后 Stream 保持打开并可继续登记新等待。

Stream 与 Listener 的 Future 等待节点是短生命周期对象，分别保持在 128 字节和
64 字节以内，并共享所属 Worker 的 `NodeCacheBytes` 预算。节点先从等待链线性化
移除，再解除取消监听、归还缓存并释放其持有的网络对象引用，最后发布 Promise；
取消、关闭和就绪竞争仍只有一个路径能够回收节点。活动节点在借用缓存期间持有临时
Engine 租约，终态发布前已经归还缓存并释放租约；已经进入 `CLOSED` 的 Stream 或 Listener 新建
Future 时不再访问 Worker，即使 Engine 已经销毁，也可以稳定取得 Close、EOF 或
关闭的 Accept 结果。

启用 `XRT_FEATURE_NET_TCP_DIAL_FUTURE` 后，托管主机连接使用同一套 Future 契约：

```c
xfuture* xrtNetDialAsync(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData
);
```

成功值是 Future 持有一个调用方引用的 `xnetstream*`，取值时 Stream 已经处于 `OPEN`。需要在销毁 Future 后继续使用时，先调用 `xrtNetStreamRef`。Future 取消会转发给整个 Dial，而不是某一个候选；成功与取消相撞时只发布一个终态，未被 Promise 接受的成功 Stream 会自动 Abort 并释放。解析和连接的结构化原因链原样进入 Future 错误。Future 进入失败或取消终态前，Dial 已经分离迟到的 Resolver 回调并清空 Timer、候选和 Engine 活动占用；成功终态只保留作为公开结果交付的 Stream。

## 阻塞便利层

`XRT_FEATURE_NET_TCP_SYNC` 依赖 TCP Future 与通用网络同步桥，只阻塞调用线程，不创建隐藏 Engine、Worker、辅助线程或第二套连接状态机。以下函数都复用已有对象所属 Engine：

```c
bool xrtNetStreamWait(xnetstream* pStream, xnetstreamwait Wait,
	xdeadline iDeadline, xcancel* pCancel);
xnetstream* xrtNetListenerAcceptWait(xnetlistener* pListener,
	xdeadline iDeadline, xcancel* pCancel);
xnetbytes* xrtNetStreamRecv(xnetstream* pStream, size_t iMaxBytes,
	xdeadline iDeadline, xcancel* pCancel);
```

这些函数不能从目标 Stream 或 Listener 所属 Worker 调用，否则在阻塞事件循环前以 `XERR_STATE` 失败并撤销刚建立的 Future。成功接受返回调用方 Stream 引用；成功接收返回调用方 `xnetbytes` 引用，两者都独立于内部 Future，分别使用 `xrtNetStreamDestroy` 与 `xrtNetBytesDestroy` 释放。创建 Future、复制结果或增加结果引用发生 OOM 时不消费连接或接收字节。

同步等待只有一个线性化结果：Future 已经在等待锁内进入终态时，底层操作终态获胜；等待先返回超时或外部取消时，控制原因获胜，同步层立即向 Future 发出协作取消，并以对应 `XERR_TIMEOUT` 或 `XERR_CANCELLED` 返回。随后到达的网络终态不会改写本次调用的错误。对象关闭则保留 `XERR_CLOSED`，底层失败保留完整结构化原因链和 TCP 操作码。

`XRT_FEATURE_NET_TCP_DIAL_SYNC` 在相同契约上提供主机名连接：

```c
xnetstream* xrtNetConnect(xnetengine* pEngine, xnetresolver* pResolver,
	cstr sHost, uint16 iPort, const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents, ptr pStreamData,
	xdeadline iDeadline, xcancel* pCancel);
```

它不创建默认 Resolver 或隐藏 Engine。截止时间和外部取消结束本次阻塞连接并取消整个 Dial；成功返回已经 `OPEN` 的调用方 Stream 引用。Resolver、候选连接和系统错误的原因链与 `xrtNetDialAsync` 完全一致。

## 查询与统计

```c
xnetstreamstate xrtNetStreamState(const xnetstream* pStream);
xnetlistenerstate xrtNetListenerState(const xnetlistener* pListener);
bool xrtNetStreamLocal(const xnetstream* pStream, xnetaddr* pAddress);
bool xrtNetStreamRemote(const xnetstream* pStream, xnetaddr* pAddress);
bool xrtNetListenerLocal(const xnetlistener* pListener, xnetaddr* pAddress);
xnetworker* xrtNetStreamWorker(const xnetstream* pStream);
xnetworker* xrtNetListenerWorker(const xnetlistener* pListener);
xnetsocket xrtNetStreamSocket(xnetstream* pStream);
bool xrtNetStreamSetEvents(
	xnetstream* pStream,
	const xnetstreamevents* pEvents,
	ptr pData
);
bool xrtNetStreamSetData(xnetstream* pStream, ptr pData);
ptr xrtNetStreamData(const xnetstream* pStream);
ptr xrtNetListenerData(const xnetlistener* pListener);
const xerror* xrtNetStreamError(const xnetstream* pStream);
bool xrtNetStreamStats(const xnetstream* pStream, xnetstreamstats* pStats);
bool xrtNetListenerStats(const xnetlistener* pListener,
	xnetlistenerstats* pStats);
```

`xrtNetStreamSocket` 只在所属 Worker 的 Stream 或 Accept 回调内返回借用 Socket，供高级用户通过 `xrtNetSocketSet`/`Get` 或 `xrtNetSocketNative` 配置标准库尚未覆盖的选项。调用方不能关闭 Socket、改变阻塞模式、重新连接、执行收发或把它注册到另一个事件循环；违反这些约束会破坏 Stream 状态机。返回后不能保存 Socket。

`SetEvents` 在所属 Worker 上原子替换事件表和用户数据，用于 HTTP Upgrade、WebSocket、自定义协议协商以及分层协议处理器接管连接。Stream 必须已经处于 `OPEN`；Listener `Accept` 在公开 `Open` 之前也满足该条件。切换不会隐式调用新 `Open`，也不会重放已经留在接收缓冲中的数据；接管层必须在切换点显式取得并处理协议余量。安装 `Read` 回调时若仍有挂起的读/接收 Future 会返回 `XERR_STATE`，并保持原事件表不变；Future 登记与该检查由同一把短锁线性化，推送与拉取消费者不能竞态并存。

`SetData` 同样只允许在 Stream Worker 回调中调用，使协议状态切换与事件处理保持顺序一致。`Data` 查询以 acquire 语义返回线程安全的借用指针快照，但不延长目标对象生命周期；调用方仍需保证指针目标在使用期间存活。

Listener 用户数据在创建时保存，之后不可替换，因此 `xrtNetListenerData` 返回稳定的借用指针；它同样不延长目标生命周期，指针所指对象的并发访问仍由调用方同步。

`xrtNetStreamError` 只在 Stream 到达 `CLOSED` 后返回导致终止的借用错误；正常关闭或尚未终止返回空。状态的 acquire 读取保证终态错误已经发布。

Stream 统计包含收发字节、Read/Write 完成次数、硬上限拒绝次数、当前/峰值发送预算、当前接收缓冲、自动读背压、写背压和读写终态。Listener 统计区分已接受、用户或队列拒绝、内部错误、内核在途 Accept、跨 Worker 初始化任务、当前/峰值拉取队列和异步等待者；关闭会等待在途 Accept 和初始化任务归零，并把队列与等待者清空。统计是无锁快照，多个字段之间不保证同一瞬间一致。

## 示例与发布门槛

推送模式回环示例位于 `examples/network/tcp/main.c`，聚合服务端示例位于 `examples/network/tcp_server/main.c`，拉取与 Future 请求响应示例位于 `examples/network/tcp_future/main.c`，阻塞 TCP 与托管连接示例位于 `examples/network/tcp_sync/main.c`、`examples/network/tcp_dial_sync/main.c`。测试使用同一契约分别验证 select 和 IOCP，并覆盖慢速对端大块发送、高低水位与 Drain、`ReadLimit` 自动停读和恢复、跨 Worker Accept/Close 排序、有界拉取 Accept 与溢出、多端点共享端口、reuse-port、聚合关闭、连接和 Accept Future、Resolver 合并、缓存命中时的 Worker 内非重入提交、跨线程 Dial 取消、双栈候选竞争、并发连接、结果复制与 Dial OOM、同步超时/取消/关闭竞态、结果 OOM 不消费、协程挂起恢复和单头文件真实收发。

TLS 传输适配已经作为独立 `XRT_FEATURE_TLS_STREAM` 裁剪单元落地，直接使用 `SendBuffer` 把完整密文块链移动到 TCP 队列。`xrtTlsStreamTransport()` 返回的原始 Stream 只供地址、统计和安全 socket 选项查询；直接关闭、收发、替换事件或消费其缓冲会破坏 TLS 组合状态机。更高层协议直接组合 TCP Server 和 TLS Stream，不改变这里的字节流、所有权、背压、等待、Dial 和关闭契约。
