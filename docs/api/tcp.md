# TCP 传输 API

TCP Dial Future 的 Promise、取消 Watch 与拥有图桥接现与 Proxy Dial Future
共用内部 `net_dial_future_common`。公开入口与 Stream 引用／显式关闭合同不变；
TCP 与 Proxy 的 adapter 仍按精确原生操作家族区分，不接受对方的生产者。
共用桥接不新增等待线程、协程栈或第二份 Promise。

## 拥有型 Stream 事件数据

`xrtNetStreamSetEventsOwnedV1(stream, data, policy)` 在 Stream 所属 Worker 上
为已打开的 Stream 安装 `xnetstreameventsownershipv1`。成功接管一个真实的
data 强引用；配置非法、线程/状态错误、读取 Future 冲突或分配失败均不接管。
描述符及其中的代码必须驻留且不可变。回调借用这个引用，不再把协议控制器
当成生命周期未知的裸 Data。`SetData` 不允许覆盖拥有型绑定；使用完整事件
替换来交接，或通过旧 `SetEvents` 显式切回借用模型。

每次成功安装仅分配一个由 Stream 独占的绑定记录，正常事件/收发没有新增
逐包分配或引用计数。旧绑定按退役顺序在当前 Worker 任务的完整回调栈退出后
释放；关闭也保留真实 runtime/Engine 占用直到 Drop 尾部完成。退役队列使用
嵌入式内部命令，不因错误清理额外分配命令或系统线程。Drop 可重入，但不在
拥有权 mutation/freeze 域内执行；不要在网络 Worker 上运行阻塞的语言回调。

绑定不是另一个虚构的引用计数节点。Stream 直接报告绑定所持的真实 data
强槽；尚未完成的 Worker 尾部明确为 BUSY。`xrtNetStreamOwnershipAdapterV1`
仍拒绝拥有型事件；新的 `V2` 必须收到精确的事件策略身份列表才接纳，拒绝
未知策略时不读取 data 元数据且不改变输出。只有所有回调和 Drop 真正完成，
Stream 才进入可准备/清理的终态。这是 Proxy Dial/Server/TLS 的原生基础，
不代表这些上层接入已经完成。

TCP 在 Engine 之上提供完整传输层：Listener/Server 接受、托管与 Future 拨号、有界背压的流式读写；代理层（proxy.h）各节复用自 [proxy.md](proxy.md)。

## 类型与常量

### `xnetproxytype`

代理类型只描述协议；TCP、TLS 和上层客户端决定如何承载协议。

```c
typedef enum xnetproxytype {
	XNET_PROXY_SOCKS5 = 1,
	XNET_PROXY_HTTP_CONNECT
} xnetproxytype;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_SOCKS5` | SOCKS5 代理 |
| `XNET_PROXY_HTTP_CONNECT` | HTTP CONNECT |


### `xnetproxyauth`

AUTO 在存在凭据时要求认证，否则只允许匿名；OPTIONAL 显式允许降级为匿名。

```c
typedef enum xnetproxyauth {
	XNET_PROXY_AUTH_AUTO = 0,
	XNET_PROXY_AUTH_NONE,
	XNET_PROXY_AUTH_REQUIRED,
	XNET_PROXY_AUTH_OPTIONAL
} xnetproxyauth;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_AUTH_AUTO` | 自动 |
| `XNET_PROXY_AUTH_NONE` | 无 |
| `XNET_PROXY_AUTH_REQUIRED` | 需要代理认证 |
| `XNET_PROXY_AUTH_OPTIONAL` | 可选认证 |


### `xnetproxyconfig`

代理对象持有配置深拷贝；主机不要求零结尾，凭据允许任意字节。

```c
typedef struct xnetproxyconfig {
	xnetproxytype Type;
	xstrview Host;
	uint16 Port;
	xnetproxyauth Auth;
	xbytesview Username;
	xbytesview Password;
} xnetproxyconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xnetproxytype` | 类型 |
| `Host` | `xstrview` | 主机名 |
| `Port` | `uint16` | 端口 |
| `Auth` | `xnetproxyauth` | Auth |
| `Username` | `xbytesview` | Username |
| `Password` | `xbytesview` | Password |


### `xnetproxyinfo`

信息视图由代理对象持有，只能在至少一个对象引用存活时借用。

```c
typedef struct xnetproxyinfo {
	xnetproxytype Type;
	xstrview Host;
	uint16 Port;
	xnetproxyauth Auth;
	xbytesview Username;
	xbytesview Password;
} xnetproxyinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xnetproxytype` | 类型 |
| `Host` | `xstrview` | 主机名 |
| `Port` | `uint16` | 端口 |
| `Auth` | `xnetproxyauth` | Auth |
| `Username` | `xbytesview` | Username |
| `Password` | `xbytesview` | Password |


### `xnetproxyhandshakestate`

握手状态同时告诉传输层下一步应发送、接收还是发布隧道。

```c
typedef enum xnetproxyhandshakestate {
	XNET_PROXY_HANDSHAKE_WRITE = 1,
	XNET_PROXY_HANDSHAKE_READ,
	XNET_PROXY_HANDSHAKE_READY,
	XNET_PROXY_HANDSHAKE_ERROR
} xnetproxyhandshakestate;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_HANDSHAKE_WRITE` | 写方向 |
| `XNET_PROXY_HANDSHAKE_READ` | 读方向 |
| `XNET_PROXY_HANDSHAKE_READY` | 就绪 |
| `XNET_PROXY_HANDSHAKE_ERROR` | 失败 |


### `xnetproxyendpoint`

域名端点使用 Host；数字端点使用 Address，端口始终保存在 Address.Port。

```c
typedef struct xnetproxyendpoint {
	xnetaddr Address;
	xstrview Host;
} xnetproxyendpoint;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Address` | `xnetaddr` | 地址 |
| `Host` | `xstrview` | 主机名 |


### `xnetproxyhandshakeconfig`

输入缓冲池由调用方借用，并且必须比握手对象存活更久。

```c
typedef struct xnetproxyhandshakeconfig {
	const xnetproxy* Proxy;
	xstrview TargetHost;
	uint16 TargetPort;
	size_t ReceiveLimit;
	xnetbufpool* Pool;
} xnetproxyhandshakeconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Proxy` | `const xnetproxy*` | Proxy |
| `TargetHost` | `xstrview` | TargetHost |
| `TargetPort` | `uint16` | TargetPort |
| `ReceiveLimit` | `size_t` | ReceiveLimit |
| `Pool` | `xnetbufpool*` | Pool |


### `xnetsocks5reply`

SOCKS5 CONNECT 回复码保留 RFC 1928 的线路值，便于日志和策略判断。

```c
typedef enum xnetsocks5reply {
	XNET_SOCKS5_SUCCEEDED = 0,
	XNET_SOCKS5_GENERAL_FAILURE = 1,
	XNET_SOCKS5_RULESET_DENIED = 2,
	XNET_SOCKS5_NETWORK_UNREACHABLE = 3,
	XNET_SOCKS5_HOST_UNREACHABLE = 4,
	XNET_SOCKS5_CONNECTION_REFUSED = 5,
	XNET_SOCKS5_TTL_EXPIRED = 6,
	XNET_SOCKS5_COMMAND_UNSUPPORTED = 7,
	XNET_SOCKS5_ADDRESS_UNSUPPORTED = 8
} xnetsocks5reply;
```

| 值 | 语义 |
|---|---|
| `XNET_SOCKS5_SUCCEEDED` | 成功 |
| `XNET_SOCKS5_GENERAL_FAILURE` | 通用失败 |
| `XNET_SOCKS5_RULESET_DENIED` | 被规则集拒绝 |
| `XNET_SOCKS5_NETWORK_UNREACHABLE` | 网络不可达 |
| `XNET_SOCKS5_HOST_UNREACHABLE` | 主机不可达 |
| `XNET_SOCKS5_CONNECTION_REFUSED` | 连接被拒绝 |
| `XNET_SOCKS5_TTL_EXPIRED` | TTL 过期 |
| `XNET_SOCKS5_COMMAND_UNSUPPORTED` | COMMAND不支持 |
| `XNET_SOCKS5_ADDRESS_UNSUPPORTED` | 地址类型不支持 |


### `xnetproxydialstate`

Proxy Dial 状态区分代理端点解析、TCP 连接和协议握手。

```c
typedef enum xnetproxydialstate {
	XNET_PROXY_DIAL_RESOLVING = 0,
	XNET_PROXY_DIAL_CONNECTING,
	XNET_PROXY_DIAL_HANDSHAKE,
	XNET_PROXY_DIAL_CONNECTED,
	XNET_PROXY_DIAL_FAILED,
	XNET_PROXY_DIAL_CANCELLED
} xnetproxydialstate;
```

| 值 | 语义 |
|---|---|
| `XNET_PROXY_DIAL_RESOLVING` | 解析中 |
| `XNET_PROXY_DIAL_CONNECTING` | 连接中 |
| `XNET_PROXY_DIAL_HANDSHAKE` | 握手阶段 |
| `XNET_PROXY_DIAL_CONNECTED` | 已连接 |
| `XNET_PROXY_DIAL_FAILED` | 已失败 |
| `XNET_PROXY_DIAL_CANCELLED` | 已取消 |


### `xnetproxydialconfig`

Timeout 覆盖 DNS、TCP 和代理握手全过程；零值保留各内层超时。

```c
typedef struct xnetproxydialconfig {
	xnetdialconfig Transport;
	int64 Timeout;
	size_t ReceiveLimit;
} xnetproxydialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Transport` | `xnetdialconfig` | Transport |
| `Timeout` | `int64` | 超时（毫秒） |
| `ReceiveLimit` | `size_t` | ReceiveLimit |


### `xnetproxydialstats`

Proxy Dial 保持底层 TCP Dial 统计，并补充当前协议阶段。

```c
typedef struct xnetproxydialstats {
	xnetproxydialstate State;
	xnetdialstats Transport;
} xnetproxydialstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetproxydialstate` | 状态 |
| `Transport` | `xnetdialstats` | Transport |


### `xnetproxy`

不可变代理端点可以跨请求和线程共享。

```c
typedef struct xnetproxy xnetproxy;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xnetproxyhandshake`

单个握手由一个传输执行上下文独占驱动。

```c
typedef struct xnetproxyhandshake xnetproxyhandshake;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xnetproxydial`

托管代理拨号对象（不透明）：内部串联名称解析、TCP 连接与代理握手。


托管代理拨号对象（不透明）：内部串联名称解析、TCP 连接与代理握手，完成时经回调移交 Stream。


```c
typedef struct xnetproxydial xnetproxydial;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xnetproxydialproc`

完成回调在代理传输 Worker 上至多执行一次，不会从提交调用栈重入。 pDial 和 Error 只在回调期间借用；成功回调接管隧道 Stream 引用。

```c
typedef void (*xnetproxydialproc)(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### `xnetstreamstate`

Stream 状态只向前推进，CLOSED 是唯一终态。

```c
typedef enum xnetstreamstate {
	XNET_STREAM_CONNECTING = 0,
	XNET_STREAM_OPEN,
	XNET_STREAM_CLOSING,
	XNET_STREAM_CLOSED
} xnetstreamstate;
```

| 值 | 语义 |
|---|---|
| `XNET_STREAM_CONNECTING` | 连接中 |
| `XNET_STREAM_OPEN` | OPEN |
| `XNET_STREAM_CLOSING` | 关闭中 |
| `XNET_STREAM_CLOSED` | 已关闭 |


### `xnetlistenerstate`

Listener 状态只向前推进，关闭后不能重新监听。

```c
typedef enum xnetlistenerstate {
	XNET_LISTENER_OPEN = 0,
	XNET_LISTENER_CLOSING,
	XNET_LISTENER_CLOSED
} xnetlistenerstate;
```

| 值 | 语义 |
|---|---|
| `XNET_LISTENER_OPEN` | OPEN |
| `XNET_LISTENER_CLOSING` | 关闭中 |
| `XNET_LISTENER_CLOSED` | 已关闭 |


### `xnetacceptdistribution`

接受结果可以跨 Worker 轮转，也可以固定留在 Listener 所属 Worker。

```c
typedef enum xnetacceptdistribution {
	XNET_ACCEPT_ROUND_ROBIN = 0,
	XNET_ACCEPT_LOCAL
} xnetacceptdistribution;
```

| 值 | 语义 |
|---|---|
| `XNET_ACCEPT_ROUND_ROBIN` | XNETACCEPTROUNDROBIN |
| `XNET_ACCEPT_LOCAL` | 固定留在监听器所属 Worker（不轮转） |


### `xnetdialstate`

Dial 状态只向前推进，连接成功、失败和取消都是不可变终态。

```c
typedef enum xnetdialstate {
	XNET_DIAL_RESOLVING = 0,
	XNET_DIAL_CONNECTING,
	XNET_DIAL_CONNECTED,
	XNET_DIAL_FAILED,
	XNET_DIAL_CANCELLED
} xnetdialstate;
```

| 值 | 语义 |
|---|---|
| `XNET_DIAL_RESOLVING` | 解析中 |
| `XNET_DIAL_CONNECTING` | 连接中 |
| `XNET_DIAL_CONNECTED` | 已连接 |
| `XNET_DIAL_FAILED` | 已失败 |
| `XNET_DIAL_CANCELLED` | 已取消 |


### `xnetstreamwait`

Stream 等待条件是水平条件；Future 只表示本次等待，不接管 Stream。

```c
typedef enum xnetstreamwait {
	XNET_STREAM_WAIT_OPEN = 0,
	XNET_STREAM_WAIT_READ,
	XNET_STREAM_WAIT_WRITE,
	XNET_STREAM_WAIT_DRAIN,
	XNET_STREAM_WAIT_CLOSE
} xnetstreamwait;
```

| 值 | 语义 |
|---|---|
| `XNET_STREAM_WAIT_OPEN` | OPEN |
| `XNET_STREAM_WAIT_READ` | 读方向 |
| `XNET_STREAM_WAIT_WRITE` | 写方向 |
| `XNET_STREAM_WAIT_DRAIN` | 排空策略 |
| `XNET_STREAM_WAIT_CLOSE` | 等待关闭 |


### `xnetstreamevents`

Stream 回调全部在所属 Worker 上串行执行。

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

| 字段 | 类型 | 语义 |
|---|---|---|
| `Open` | `void (*Open)(xnetstream* pStream, ptr pData)` | 流开放 |
| `Read` | `void (*Read)(xnetstream* pStream, xnetbuf* pBuffer, ptr pData)` | 收到数据缓冲 |
| `End` | `void (*End)(xnetstream* pStream, ptr pData)` | 对端写关闭 |
| `HighWater` | `void (*HighWater)(xnetstream* pStream, size_t iQueued, ptr pData)` | 发送占用越过高水位 |
| `LowWater` | `void (*LowWater)(xnetstream* pStream, size_t iQueued, ptr pData)` | 发送占用落回低水位 |
| `Drain` | `void (*Drain)(xnetstream* pStream, ptr pData)` | 发送队列排空 |
| `Close` | `void (*Close)(xnetstream* pStream, xnetresult Result, const xerror* pError, ptr pData)` | 流关闭（含终态结果与错误） |


### `xnetlistenerevents`

Accept 成功返回 true 并接管一个 Stream 引用，返回 false 会立即拒绝连接。

```c
typedef struct xnetlistenerevents {
	bool (*Accept)(xnetlistener* pListener,
		xnetstream* pStream, ptr pData);
	void (*Error)(xnetlistener* pListener,
		const xerror* pError, ptr pData);
	void (*Close)(xnetlistener* pListener, ptr pData);
} xnetlistenerevents;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Accept` | `bool (*Accept)(xnetlistener* pListener, xnetstream* pStream, ptr pData)` | 新流被接受（返回是否保留） |
| `Error` | `void (*Error)(xnetlistener* pListener, const xerror* pError, ptr pData)` | 监听级错误 |
| `Close` | `void (*Close)(xnetlistener* pListener, ptr pData)` | 监听器关闭 |


### `xnetstreamreadmode`

完成式读取可在吞吐、空闲内存和两者自适应之间选择。

```c
typedef enum xnetstreamreadmode {
	XNET_STREAM_READ_ADAPTIVE = 0,
	XNET_STREAM_READ_DIRECT,
	XNET_STREAM_READ_PROBE
} xnetstreamreadmode;
```

| 值 | 语义 |
|---|---|
| `XNET_STREAM_READ_ADAPTIVE` | 读方向 |
| `XNET_STREAM_READ_DIRECT` | 读方向 |
| `XNET_STREAM_READ_PROBE` | 探测模式（低内存优先） |


### `xnetstreamconfig`

所有字节容量都是硬边界，ConnectTimeout 使用毫秒。

```c
typedef struct xnetstreamconfig {
	size_t ReadSize;
	size_t ReadLimit;
	size_t WriteHighWater;
	size_t WriteLowWater;
	size_t WriteLimit;
	int64 ConnectTimeout;
	xnetstreamreadmode ReadMode;
	bool NoDelay;
	bool KeepAlive;
} xnetstreamconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `ReadSize` | `size_t` | ReadSize |
| `ReadLimit` | `size_t` | ReadLimit |
| `WriteHighWater` | `size_t` | WriteHighWater |
| `WriteLowWater` | `size_t` | WriteLowWater |
| `WriteLimit` | `size_t` | WriteLimit |
| `ConnectTimeout` | `uint64` | ConnectTimeout |
| `ReadMode` | `xnetstreamreadmode` | ReadMode |
| `NoDelay` | `bool` | NoDelay |
| `KeepAlive` | `bool` | KeepAlive |


### `xnetlistenconfig`

Listener 只绑定一个地址；多端口和复用端口由后续 Server 层管理。

```c
typedef struct xnetlistenconfig {
	xnetaddr Address;
	xnetstreamconfig Stream;
	uint64 Affinity;
	uint32 AcceptConcurrency;
	uint32 AcceptQueueLimit;
	int Backlog;
	xnetacceptdistribution Distribution;
	bool ReuseAddress;
	bool ReusePort;
	bool ExclusiveAddress;
	bool IPv6Only;
} xnetlistenconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Address` | `xnetaddr` | 地址 |
| `Stream` | `xnetstreamconfig` | 流选择 |
| `Affinity` | `uint64` | 亲和 Worker |
| `AcceptConcurrency` | `uint32` | AcceptConcurrency |
| `AcceptQueueLimit` | `uint32` | AcceptQueueLimit |
| `Backlog` | `int` | Backlog |
| `Distribution` | `xnetacceptdistribution` | Distribution |
| `ReuseAddress` | `bool` | ReuseAddress |
| `ReusePort` | `bool` | ReusePort |
| `ExclusiveAddress` | `bool` | ExclusiveAddress |
| `IPv6Only` | `bool` | IPv6Only |


### `xnetstreamstats`

Stream 统计是无锁并发快照，累计值在关闭后仍可读取。

```c
typedef struct xnetstreamstats {
	xnetstreamstate State;
	uint64 ReceivedBytes;
	uint64 SentBytes;
	uint64 ReadEvents;
	uint64 WriteEvents;
	uint64 SendRejected;
	size_t BufferedBytes;
	size_t QueuedBytes;
	size_t PeakQueuedBytes;
	bool ReadPaused;
	bool ReadBlocked;
	bool ReadEnded;
	bool WriteEnded;
	bool WriteBackpressured;
} xnetstreamstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetstreamstate` | 状态 |
| `ReceivedBytes` | `uint64` | ReceivedBytes |
| `SentBytes` | `uint64` | SentBytes |
| `ReadEvents` | `uint64` | ReadEvents |
| `WriteEvents` | `uint64` | WriteEvents |
| `SendRejected` | `uint64` | SendRejected |
| `BufferedBytes` | `size_t` | BufferedBytes |
| `QueuedBytes` | `size_t` | QueuedBytes |
| `PeakQueuedBytes` | `size_t` | PeakQueuedBytes |
| `ReadPaused` | `bool` | ReadPaused |
| `ReadBlocked` | `bool` | ReadBlocked |
| `ReadEnded` | `bool` | ReadEnded |
| `WriteEnded` | `bool` | WriteEnded |
| `WriteBackpressured` | `bool` | WriteBackpressured |


### `xnetdialconfig`

Timeout 和 FallbackDelay 使用毫秒；MaxAttempts 是解析结果的硬上限。

```c
typedef struct xnetdialconfig {
	xnetstreamconfig Stream;
	xnetfamily Family;
	uint64 Affinity;
	int64 Timeout;
	uint64 FallbackDelay;
	uint32 MaxAttempts;
} xnetdialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Stream` | `xnetstreamconfig` | 流选择 |
| `Family` | `xnetfamily` | 地址族 |
| `Affinity` | `uint64` | 亲和 Worker |
| `Timeout` | `int64` | 超时（毫秒） |
| `FallbackDelay` | `uint64` | FallbackDelay |
| `MaxAttempts` | `uint32` | MaxAttempts |


### `xnetdialstats`

Dial 统计是无锁快照，WinnerIndex 只在 HasWinner 为真时有效。

```c
typedef struct xnetdialstats {
	xnetdialstate State;
	uint32 Addresses;
	uint32 AttemptsStarted;
	uint32 AttemptsFailed;
	uint32 ActiveAttempts;
	uint32 PeakAttempts;
	size_t WinnerIndex;
	bool HasWinner;
} xnetdialstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetdialstate` | 状态 |
| `Addresses` | `uint32` | Addresses |
| `AttemptsStarted` | `uint32` | AttemptsStarted |
| `AttemptsFailed` | `uint32` | AttemptsFailed |
| `ActiveAttempts` | `uint32` | ActiveAttempts |
| `PeakAttempts` | `uint32` | PeakAttempts |
| `WinnerIndex` | `size_t` | WinnerIndex |
| `HasWinner` | `bool` | HasWinner |


### `xnetlistenerstats`

Listener 统计区分系统接受、用户拒绝和内部错误。

```c
typedef struct xnetlistenerstats {
	xnetlistenerstate State;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Errors;
	uint32 ActiveAccepts;
	uint32 ActiveDispatches;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xnetlistenerstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetlistenerstate` | 状态 |
| `Accepted` | `uint64` | Accepted |
| `Rejected` | `uint64` | Rejected |
| `Errors` | `uint64` | Errors |
| `ActiveAccepts` | `uint32` | ActiveAccepts |
| `ActiveDispatches` | `uint32` | ActiveDispatches |
| `QueuedAccepts` | `uint32` | QueuedAccepts |
| `PeakQueuedAccepts` | `uint32` | PeakQueuedAccepts |
| `AcceptWaiters` | `uint32` | AcceptWaiters |


### `xnetstream`

TCP 流对象（不透明）：绑定所属 Worker 的有界双向字节流。


```c
typedef struct xnetstream xnetstream;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xnetlistener`

TCP 监听器（不透明）：绑定所属 Worker，拉取模式下预投递 Accept。


```c
typedef struct xnetlistener xnetlistener;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xnetdial`

托管主机拨号对象（不透明）：内部完成名称解析、候选竞速与连接。


```c
typedef struct xnetdial xnetdial;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xnetdialproc`

完成回调在 Affinity Worker 上至多执行一次，不会从 xrtNetDial 调用栈重入。 pDial 和 Error 只在回调期间借用；成功回调接管 Stream 引用。

```c
typedef void (*xnetdialproc)(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### `xnetserverstate`

Server 只在全部端点绑定成功后进入 OPEN，CLOSED 是唯一终态。

```c
typedef enum xnetserverstate {
	XNET_SERVER_STARTING = 0,
	XNET_SERVER_OPEN,
	XNET_SERVER_CLOSING,
	XNET_SERVER_CLOSED
} xnetserverstate;
```

| 值 | 语义 |
|---|---|
| `XNET_SERVER_STARTING` | 启动中 |
| `XNET_SERVER_OPEN` | 服务端角色 |
| `XNET_SERVER_CLOSING` | 关闭中 |
| `XNET_SERVER_CLOSED` | 已关闭 |


### `xnetservermode`

SHARED 每端点使用一个 Listener，REUSE_PORT 为每个 Worker 建立一份。

```c
typedef enum xnetservermode {
	XNET_SERVER_SHARED = 0,
	XNET_SERVER_REUSE_PORT
} xnetservermode;
```

| 值 | 语义 |
|---|---|
| `XNET_SERVER_SHARED` | XNET服务端角色SHARED |
| `XNET_SERVER_REUSE_PORT` | 重用端口分担 |


### `xnetserverevents`

Server 回调收到逻辑端点索引，Accept 返回 true 后接管一个 Stream 引用。 Close 在状态进入 CLOSED 后发布；只轮询状态不能代替等待 Close 通知。

```c
typedef struct xnetserverevents {
	bool (*Accept)(xnetserver* pServer, size_t iEndpoint,
		xnetstream* pStream, ptr pData);
	void (*Error)(xnetserver* pServer, size_t iEndpoint,
		const xerror* pError, ptr pData);
	void (*Close)(xnetserver* pServer, ptr pData);
} xnetserverevents;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Accept` | `bool (*Accept)(xnetserver* pServer, size_t iEndpoint, xnetstream* pStream, ptr pData)` | 新流被接受（返回是否保留） |
| `Error` | `void (*Error)(xnetserver* pServer, size_t iEndpoint, const xerror* pError, ptr pData)` | 端点级错误 |
| `Close` | `void (*Close)(xnetserver* pServer, ptr pData)` | 服务器关闭 |


### `xnetserverconfig`

Listen 是第零个端点，Additional 只在启动调用期间借用。 SharedPort 让零端口继承整组首个实际端口，非零端口必须彼此一致。 REUSE_PORT 模式固定本地分发、关闭独占绑定，并为每个 Engine Worker 建立 Listener。

```c
typedef struct xnetserverconfig {
	xnetlistenconfig Listen;
	const xnetlistenconfig* Additional;
	size_t AdditionalCount;
	uint32 AcceptQueueLimit;
	xnetservermode Mode;
	bool SharedPort;
} xnetserverconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Listen` | `xnetlistenconfig` | Listen |
| `Additional` | `const xnetlistenconfig*` | Additional |
| `AdditionalCount` | `size_t` | AdditionalCount |
| `AcceptQueueLimit` | `uint32` | AcceptQueueLimit |
| `Mode` | `xnetservermode` | 模式 |
| `SharedPort` | `bool` | SharedPort |


### `xnetserverstats`

Server 统计聚合全部 Listener，并保留关闭后的累计值。

```c
typedef struct xnetserverstats {
	xnetserverstate State;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Errors;
	size_t Endpoints;
	size_t Listeners;
	size_t ClosedListeners;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xnetserverstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetserverstate` | 状态 |
| `Accepted` | `uint64` | Accepted |
| `Rejected` | `uint64` | Rejected |
| `Errors` | `uint64` | Errors |
| `Endpoints` | `size_t` | Endpoints |
| `Listeners` | `size_t` | Listeners |
| `ClosedListeners` | `size_t` | ClosedListeners |
| `QueuedAccepts` | `uint32` | QueuedAccepts |
| `PeakQueuedAccepts` | `uint32` | PeakQueuedAccepts |
| `AcceptWaiters` | `uint32` | AcceptWaiters |


### `xnetserver`

TCP Server（不透明）：多端点监听的组合服务对象，聚合 Listener 与 Accept 队列。


```c
typedef struct xnetserver xnetserver;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


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
	int64 ConnectTimeout;
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

### `xrtNetStreamConfigInit`

初始化 Stream 的自适应读取、背压和连接超时默认值。

```c
void xrtNetStreamConfigInit(xnetstreamconfig* pConfig)
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

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · Stream 配置

```c
	xrtNetStreamConfigInit(&StreamConfig);
```


### `xrtNetListenConfigInit`

初始化 IPv4 动态端口 Listener 及其默认 Stream 配置。

```c
void xrtNetListenConfigInit(xnetlistenconfig* pConfig)
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

[tcp](../../examples/network/tcp/main.c) · Listener 配置

```c
	xrtNetListenConfigInit(&ListenConfig);
```


### `xrtNetDialConfigInit`

初始化双栈交错、总超时和单地址 Stream 的默认策略。

```c
void xrtNetDialConfigInit(xnetdialconfig* pConfig)
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

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · Dial 配置

```c
	xrtNetDialConfigInit(&DialConfig);
```


### `xrtNetStreamConfigValid` / `xrtNetListenConfigValid`

```c
bool xrtNetStreamConfigValid(const xnetstreamconfig* pConfig);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |


#### 返回值

| 返回 | 含义 | 状态约定 |
|---|---|---|
| `true` | 操作完成或满足声明中的条件 | 按上述契约使用结果 |
| `false` | 操作拒绝或条件不成立 | 区分正常拒绝与实际错误 |


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。



只读验证完整配置，复用实际 Stream／Listener 创建路径的规则。Listener 同时
检查嵌套 Stream、地址族、接受并发、队列、分发、backlog 和绑定策略关系。
成功返回 true；非法参数（包括空指针）返回 false 并设置结构化错误。不修改
配置，不创建 Engine／Socket，不绑定端口，也不进行 DNS 或网络通信。
允许上层先构造不可变配置快照，再在实际创建传输时复验同一合同。

### `xrtNetDialConfigValid`

完整验证 Dial 策略及其嵌套 Stream 配置。

```c
bool xrtNetDialConfigValid(const xnetdialconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 待验证配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 配置自洽 | — |
| `false` | 不自洽 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · Dial 配置校验

```c
	printf("dial-config: valid=%d", xrtNetDialConfigValid(&DialConfig) ?
		1 : 0);
```


### `xrtNetServerConfigInit`

初始化单 IPv4 动态端口、共享 Listener 和有界 Accept 队列。

```c
void xrtNetServerConfigInit(xnetserverconfig* pConfig)
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

[tcp_server](../../examples/network/tcp_server/main.c) · Server 配置

```c
	xrtNetServerConfigInit(&ServerConfig);
```


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

### `xrtNetStreamConnect`

在指定 Worker 上连接数字地址并创建 Stream。

```c
xnetstream* xrtNetStreamConnect(xnetengine* pEngine, const xnetaddr* pRemote, uint64 iAffinity, const xnetstreamconfig* pConfig, const xnetstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pRemote` | 输入 | 非空 | 远端地址 |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pConfig` | 输入 | 允许空 | Stream 配置 |
| `pEvents` | 输入 | 允许空 | 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Stream（引用 1） | — |
| `NULL` | 创建或连接失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp](../../examples/network/tcp/main.c) · 数字地址连接

```c
	Example.Client = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		&StreamEvents,
		&Example
	);
```


### `xrtNetListen`

同步完成创建、选项、绑定和监听，再异步预投递 Accept。

```c
xnetlistener* xrtNetListen(xnetengine* pEngine, const xnetlistenconfig* pConfig, const xnetlistenerevents* pEvents, const xnetstreamevents* pStreamEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pConfig` | 输入 | 非空 | 监听配置 |
| `pEvents` | 输入 | 允许空 | Listener 事件表 |
| `pStreamEvents` | 输入 | 允许空 | Stream 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Listener（引用 1） | — |
| `NULL` | 创建失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp](../../examples/network/tcp/main.c) · 开始监听

```c
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Example
	);
```


### `xrtNetStreamRef`

线程安全地增加 Stream 引用；无效计数或溢出时返回空。

```c
xnetstream* xrtNetStreamRef(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 计数无效 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_dial](../../examples/network/tcp_dial/main.c) · Stream 共享引用

```c
	Context.Client = xrtNetStreamRef(
		(xnetstream*)xrtFutureValue(pDial)
	);
```


### `xrtNetStreamDestroy`

释放 Stream 引用；空指针视为空操作。

```c
void xrtNetStreamDestroy(xnetstream* pStream)
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

[tcp](../../examples/network/tcp/main.c) · Stream 释放

```c
	xrtNetStreamDestroy(Example.Client);
```


### `xrtNetListenerRef`

线程安全地增加 Listener 引用；无效计数或溢出时返回空。

```c
xnetlistener* xrtNetListenerRef(xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 计数无效 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · Listener 共享引用

```c
	pListenerRef = xrtNetListenerRef(pListener);
```


### `xrtNetListenerDestroy`

释放 Listener 引用；关闭操作必须另行请求。

```c
void xrtNetListenerDestroy(xnetlistener* pListener)
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

- 无 — 释放不失败

#### 范例

[tcp](../../examples/network/tcp/main.c) · Listener 释放

```c
	xrtNetListenerDestroy(pListener);
```


### `xrtNetListenerClose`

请求停止接受新连接并排空在途 Accept。

```c
bool xrtNetListenerClose(xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已请求关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp](../../examples/network/tcp/main.c) · 关闭监听

```c
	(void)xrtNetListenerClose(pListener);
```


### `xrtNetListenerState`

返回 Listener 当前状态的并发快照。

```c
xnetlistenerstate xrtNetListenerState(const xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举值 | 当前生命周期状态 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp](../../examples/network/tcp/main.c) · Listener 状态

```c
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
```


### `xrtNetListenerStats`

复制 Listener 并发统计。

```c
bool xrtNetListenerStats(const xnetlistener* pListener, xnetlistenerstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · Listener 统计

```c
		 !xrtNetListenerStats(pListener, &ListenStats) ||
```


### `xrtNetListenerLocal`

复制 Listener 实际绑定地址，支持查询动态端口。

```c
bool xrtNetListenerLocal(const xnetlistener* pListener, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |
| `pAddress` | 输出 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp](../../examples/network/tcp/main.c) · 绑定地址

```c
		 !xrtNetListenerLocal(pListener, &Address) ) {
```


### `xrtNetListenerWorker`

返回 Listener 所属的借用 Worker。

```c
xnetworker* xrtNetListenerWorker(const xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Worker 借用 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 所属 Worker

```c
		 (xrtNetListenerWorker(pListener) == NULL) ) {
```


### `xrtNetListenerData`

返回创建时保存的 Listener 用户数据，不延长目标生命周期。

```c
ptr xrtNetListenerData(const xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 创建时传入的用户数据 | — |
| `NULL` | 未设置 | 不设错误 |

#### 错误

- 无错误 — 未设置返回 `NULL` 且不设置错误

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · Listener 数据

```c
	(void)xrtNetListenerData(pListener);
```


### `xrtNetListenerAccept`

拉取模式下非阻塞取走一个已接受 Stream；空队列返回空指针，Stream 不继承 Listener 数据。

```c
xnetstream* xrtNetListenerAccept(xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已接受 Stream（引用 1） | — |
| `NULL` | 队列为空 | 不设错误 |

#### 错误

- 无错误 — 空队列返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 非阻塞接受

```c
		xnetstream* pStream = xrtNetListenerAccept(pListener);
```


### `xrtNetListenerAcceptAsync`

拉取模式下异步接受一个连接；成功值由 Future 持有一个 Stream 引用。

```c
xfuture* xrtNetListenerAcceptAsync(xnetlistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future，失败时完成并携带错误 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_future](../../examples/network/tcp_future/main.c) · 异步接受

```c
	pAccept = xrtNetListenerAcceptAsync(pListener);
```


### `xrtNetListenerAcceptWait`

阻塞接受一个不继承 Listener 数据的连接并返回调用方引用；禁止从 Listener Worker 调用。

```c
xnetstream* xrtNetListenerAcceptWait(xnetlistener* pListener, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | 目标 Listener |
| `iTimeout` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Stream（引用 1） | — |
| `NULL` | 创建或连接失败 | `xrt.net` 域错误 |
（超时/取消返回 `NULL` 不设错）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从 Listener 所属 Worker 调用
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链

#### 范例

[tcp_dial_sync](../../examples/network/tcp_dial_sync/main.c) · 阻塞接受

```c
	pServer = xrtNetListenerAcceptWait(
		pListener,
		3000,
		NULL
	);
```


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

### `xrtNetStreamSendFile`

把文件区间提交到发送队列；数据在队列排空前不得关闭文件。

```c
xnetresult xrtNetStreamSendFile(xnetstream* pStream, xfile File, uint64 iOffset, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `File` | 输入 | 非空 | 已打开文件 |
| `iOffset` | 输入 | — | 起始偏移 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 占用字节数达到 `WriteLimit`
- `XERR_STATE` — Stream 非开放状态

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 文件区间发送

```c
		 (xrtNetStreamSendFile(pClient, File, 0, 10) !=
		  XNET_RESULT_OK) ) {
```


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

### `xrtNetServerStart`

按配置在 Engine 上启动多端点 TCP Server。

```c
xnetserver* xrtNetServerStart(xnetengine* pEngine, const xnetserverconfig* pConfig, const xnetserverevents* pEvents, const xnetstreamevents* pStreamEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pConfig` | 输入 | 非空 | Server 配置 |
| `pEvents` | 输入 | 允许空 | Server 事件表 |
| `pStreamEvents` | 输入 | 允许空 | Stream 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Server（引用 1） | — |
| `NULL` | 启动失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_server](../../examples/network/tcp_server/main.c) · 启动 Server

```c
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents,
		&StreamEvents,
		NULL
	);
```


### `xrtNetServerRef`

线程安全地增加 Server 引用；无效计数或溢出时返回空。

```c
xnetserver* xrtNetServerRef(xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 计数无效 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · 共享引用

```c
	pServerRef = xrtNetServerRef(pServer);
```


### `xrtNetServerDestroy`

释放 Server 引用；不会隐式关闭仍在运行的 Server。

```c
void xrtNetServerDestroy(xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tcp_server](../../examples/network/tcp_server/main.c) · 释放引用

```c
	xrtNetServerDestroy(pServer);
```


### `xrtNetServerAccept`

拉取模式下非阻塞取走一个已接受 Stream；空队列返回空指针。

```c
xnetstream* xrtNetServerAccept(xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已接受 Stream（引用 1） | — |
| `NULL` | 队列为空 | 不设错误 |

#### 错误

- 无错误 — 空队列返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · 非阻塞接受

```c
			xnetstream* pOne = xrtNetServerAccept(pServer);
```


### `xrtNetServerAcceptAsync`

拉取模式下异步接受一个连接；成功值由 Future 持有一个 Stream 引用。

```c
xfuture* xrtNetServerAcceptAsync(xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future，失败时完成并携带错误 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_server_sync](../../examples/network/tcp_server_sync/main.c) · 异步接受

```c
	pAccept = xrtNetServerAcceptAsync(State.Server);
```


### `xrtNetServerAcceptWait`

阻塞接受一个连接；禁止从任意 Engine Worker 调用。

```c
xnetstream* xrtNetServerAcceptWait(xnetserver* pServer, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |
| `iTimeout` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Stream（引用 1） | — |
| `NULL` | 创建或连接失败 | `xrt.net` 域错误 |
（超时/取消返回 `NULL` 不设错）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从 Engine Worker 调用
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链

#### 范例

[tcp_server_sync](../../examples/network/tcp_server_sync/main.c) · 阻塞接受

```c
	State.AcceptedSync = xrtNetServerAcceptWait(
		State.Server,
		INT64_C(5000),
		NULL
	);
```


### `xrtNetServerClose`

原子停止全部 Listener，并丢弃尚未交给调用方的排队 Stream。

```c
bool xrtNetServerClose(xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_server](../../examples/network/tcp_server/main.c) · 关闭

```c
	(void)xrtNetServerClose(pServer);
```


### `xrtNetServerState`

返回 Server 当前生命周期状态。

```c
xnetserverstate xrtNetServerState(const xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举值 | 当前生命周期状态 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_server](../../examples/network/tcp_server/main.c) · 状态

```c
	while ( xrtNetServerState(pServer) != XNET_SERVER_CLOSED ) {
```


### `xrtNetServerEndpointCount`

返回配置中的逻辑端点数量。

```c
size_t xrtNetServerEndpointCount(const xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 逻辑端点数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · 端点数量

```c
	if ( (xrtNetServerEndpointCount(pServer) != 2u) ||
		 (xrtNetServerListenerCount(pServer) != 2u) ||
		 !xrtNetServerLocal(pServer, 0, &Addr0) ||
		 !xrtNetServerLocal(pServer, 1, &Addr1) ||
		 (Addr0.Port == 0u) || (Addr0.Port != Addr1.Port) ) {
```


### `xrtNetServerLocal`

复制指定逻辑端点的实际地址，支持共享动态端口。

```c
bool xrtNetServerLocal(const xnetserver* pServer, size_t iEndpoint, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |
| `iEndpoint` | 输入 | < 端点数量 | 端点索引 |
| `pAddress` | 输出 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已复制 | — |
| `false` | 索引越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_RANGE` — 端点索引越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[tcp_server](../../examples/network/tcp_server/main.c) · 端点地址

```c
		!xrtNetServerLocal(pServer, 0, &Address) ) {
```


### `xrtNetServerListenerCount`

返回实际 Listener 数量；reuse-port 模式通常是端点数乘 Worker 数。

```c
size_t xrtNetServerListenerCount(const xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际 Listener 数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · Listener 数量

```c
		 (xrtNetServerListenerCount(pServer) != 2u) ||
```


### `xrtNetServerListener`

返回借用 Listener；返回值只用于诊断，不参与引用计数。

```c
xnetlistener* xrtNetServerListener(xnetserver* pServer, size_t iListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |
| `iListener` | 输入 | < Listener 数量 | 序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Listener 借用 | — |
| `NULL` | 序号越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 序号越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · Listener 借用

```c
	pListener0 = xrtNetServerListener(pServer, 0);
```


### `xrtNetServerData`

返回创建时保存的用户数据，不延长目标生命周期。

```c
ptr xrtNetServerData(const xnetserver* pServer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 创建时传入的用户数据 | — |
| `NULL` | 未设置 | 不设错误 |

#### 错误

- 无错误 — 未设置返回 `NULL` 且不设置错误

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · 用户数据

```c
		 (xrtNetServerData(pServer) != &g_Tag) ) {
```


### `xrtNetServerStats`

复制 Server 并发统计。

```c
bool xrtNetServerStats(const xnetserver* pServer, xnetserverstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pServer` | 输入 | 非空 | 目标 Server |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_server_tour](../../examples/network/tcp_server_tour/main.c) · 统计

```c
	if ( !xrtNetServerStats(pServer, &Stats) ||
		 (Stats.Accepted < 2u) ||
		 (xrtNetServerData(pServer) != &g_Tag) ) {
```


## 托管主机连接

启用 `XRT_FEATURE_NET_TCP_DIAL` 后，主机名解析和多地址连接策略作为独立裁剪层建立在 Resolver 与数字地址 TCP 之上：

```c
typedef struct xnetdialconfig {
	xnetstreamconfig Stream;
	xnetfamily Family;
	uint64 Affinity;
	int64 Timeout;
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

### `xrtNetConnect`

阻塞完成主机解析、候选竞速与连接，并返回调用方 Stream 引用。

```c
xnetstream* xrtNetConnect(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 允许空 | 名称解析器 |
| `sHost` | 输入 | 非空、零结尾 | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pConfig` | 输入 | 允许空 | Dial 配置 |
| `pStreamEvents` | 输入 | 允许空 | Stream 事件表 |
| `pStreamData` | 输入 | 任意值 | Stream 数据 |
| `iTimeout` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Stream（引用 1） | — |
| `NULL` | 创建或连接失败 | `xrt.net` 域错误 |
（超时/取消返回 `NULL`）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_dial_sync](../../examples/network/tcp_dial_sync/main.c) · 阻塞主机连接

```c
	pClient = xrtNetConnect(
		pEngine,
		pResolver,
		"local.example",
		ExampleAddress.Port,
		NULL,
		NULL,
		NULL,
		3000,
		NULL
	);
```


### `xrtNetDial`

发起托管主机拨号；成功时 Stream 引用转移给完成回调，非 Worker 提交者可能与回调并发。

```c
xnetdial* xrtNetDial(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData, xnetdialproc pDone, ptr pDoneData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 允许空 | 名称解析器 |
| `sHost` | 输入 | 非空、零结尾 | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pConfig` | 输入 | 允许空 | Dial 配置 |
| `pStreamEvents` | 输入 | 允许空 | Stream 事件表 |
| `pStreamData` | 输入 | 任意值 | Stream 数据 |
| `pDone` | 输入 | 非空 | 完成回调 |
| `pDoneData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Dial（引用 1） | — |
| `NULL` | 提交失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` 域错误 — DNS 解析、连接或监听失败（`XNET_ERROR_DNS_*`、`SOCKET_CONNECT/LISTEN/BIND` 等），系统错误保留在原因链
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 托管拨号

```c
	pDial = xrtNetDial(pEngine, pResolver, "127.0.0.1", Address.Port,
		&DialConfig, NULL, NULL, exampleDialDone, &Task);
```


### `xrtNetDialAsync`

把托管连接包装为 Future；成功值是由 Future 持有的 Stream。

```c
xfuture* xrtNetDialAsync(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 允许空 | 名称解析器 |
| `sHost` | 输入 | 非空、零结尾 | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pConfig` | 输入 | 允许空 | Dial 配置 |
| `pStreamEvents` | 输入 | 允许空 | Stream 事件表 |
| `pStreamData` | 输入 | 任意值 | Stream 数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future，失败时完成并携带错误 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_dial](../../examples/network/tcp_dial/main.c) · Future 拨号

```c
	pDial = xrtNetDialAsync(
		pEngine,
		pResolver,
		"service.local",
		Address.Port,
		NULL,
		&StreamEvents,
		&Context
	);
```


### `xrtNetDialRef`

增加 Dial 引用并返回原指针。

```c
xnetdial* xrtNetDialRef(xnetdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标 Dial |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 共享引用

```c
		 (xrtNetDialRef(pDial) != pDial) ) {  /* 引用配对在收尾多一次 Destroy */
```


### `xrtNetDialDestroy`

释放 Dial 引用；空指针视为空操作。

```c
void xrtNetDialDestroy(xnetdial* pDial)
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

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 释放引用

```c
	xrtNetDialDestroy(pDial);
```


### `xrtNetDialCancel`

原子争取取消终态；返回真后完成结果必为 `CANCELLED`。

```c
bool xrtNetDialCancel(xnetdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标 Dial |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 本次调用赢得取消权 | — |
| `false` | 已终态或并发取消已被受理 | 不设错误 |

#### 错误

- 已终态或并发取消已被受理返回 `false` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 取消

```c
		bool bCancelled = xrtNetDialCancel(pCancelDial);
```


### `xrtNetDialState`

返回 Dial 当前状态的原子快照。

```c
xnetdialstate xrtNetDialState(const xnetdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标 Dial |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举值 | 当前拨号阶段或终态 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 状态

```c
	pTask->State = xrtNetDialState(pDial);
```


### `xrtNetDialError`

失败或取消后返回借用的结构化错误，其他状态返回空指针。

```c
const xerror* xrtNetDialError(const xnetdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标 Dial |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误借用（存活到对象销毁） | — |
| `NULL` | 进行中或已连接 | 不设错误 |

#### 错误

- 无错误 — 无失败时返回 `NULL` 且不设置错误

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 错误

```c
	(void)xrtNetDialError(pBadDial);
```


### `xrtNetDialStats`

取得解析地址、并发尝试和获胜地址的无锁统计快照。

```c
bool xrtNetDialStats(const xnetdial* pDial, xnetdialstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标 Dial |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 统计

```c
	if ( !xrtNetDialStats(pDial, &DialStats) ||
		 (DialStats.Addresses < 1u) ||
		 (DialStats.AttemptsStarted < 1u) ||
		 (DialStats.WinnerIndex != 0u) ) {
```


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
- `SendBuffer` 在任意线程接管符合池归属约束的完整 `xnetbuf` 块链。它只建立发送视图元数据，不复制载荷；适合把 Read 回调缓冲直接转发到另一个方向，或连接 TLS、HTTP 等协议队列。

只有返回 `XNET_RESULT_OK` 才接受数据和转移 ref/take/buffer 所有权。`SendRefs` 和 `SendBuffer` 都是全有或全无操作：元数据或任一发送视图建立失败时，不会释放外部片段、清空源缓冲或留下部分队列和预算。`SendBuffer` 成功后源缓冲恢复为空但保留原缓冲池，可立即复用；其中的借用块仍要求调用方保持载荷存活到发送队列离队。`XNET_RESULT_AGAIN` 表示加入后会超过 `WriteLimit`；调用方保留数据，可以等待 `LowWater` 或 `Drain` 后重试。`XNET_RESULT_CLOSED` 表示写半关闭、关闭或 Abort 已经封闭新发送。`XNET_RESULT_ERROR` 表示参数、线程归属、活动写预留、溢出、OOM 或命令提交失败，详细原因在当前执行上下文的 `xerror`。

发送预算在 API 入口原子占用，包含已经受理但尚未链接到 Worker 缓冲的跨线程命令。关闭过程先封闭新发送，再等待已经进入提交区的调用完成，因而不会越过一个已经线性化成功的 Send。

Worker 回调内的 `Send`/`SendVec` 直接复制到本 Worker 的拥有型发送缓冲；缓冲池已经有合适缓存块时不依赖全局分配器，也不再建立“载荷节点 + 引用块”两级对象。普通 Send 的其他线程调用通过 Engine 命令队列提交，使用独立拥有节点保证调用返回后源数据即可失效。`SendBuffer` 允许跨线程提交堆块或来自目标 Stream Worker 缓冲池的块；其他线程归属池的块被拒绝，调用方保留源链。调用方须串行访问源缓冲；所有可失败准备完成后才接管并发布整个块链。

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

### `xrtNetStreamSend`

有界复制发送；队列达到 `WriteLimit` 时返回 `AGAIN`。

```c
xnetresult xrtNetStreamSend(xnetstream* pStream, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pData` | 输入 | 非空 | 待发送数据 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 占用字节数达到 `WriteLimit`
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp](../../examples/network/tcp/main.c) · 复制发送

```c
	if ( xrtNetStreamSend(
		Example.Client,
		"hello TCP",
		9
	) != XNET_RESULT_OK ) {
```


### `xrtNetStreamSendVec`

有界聚集复制发送；所有片段在返回前完成复制。

```c
xnetresult xrtNetStreamSendVec(xnetstream* pStream, const xnetspan* pSpans, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pSpans` | 输入 | 非空数组 | 分片视图 |
| `iCount` | 输入 | > 0 | 分片数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 占用字节数达到 `WriteLimit`
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 聚集发送

```c
	if ( xrtNetStreamSendVec(pClient, Vec, 2) != XNET_RESULT_OK ) {
```


### `xrtNetStreamSendRef`

有界零复制发送；成功后在数据离开队列时执行一次释放过程。

```c
xnetresult xrtNetStreamSendRef(xnetstream* pStream, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pData` | 输入 | 非空 | 借用数据 |
| `iSize` | 输入 | > 0 | 字节数 |
| `pRelease` | 输入 | 非空 | 释放回调 |
| `pContext` | 输入 | 任意值 | 回调上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 占用字节数达到 `WriteLimit`
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 零复制发送

```c
	if ( xrtNetStreamSendRef(pClient, "ref-zero-copy", 13,
		exampleCountRelease, &g_Releases) != XNET_RESULT_OK ) {
```


### `xrtNetStreamSendRefs`

原子受理一组零复制引用；失败时全部所有权仍归调用方。

```c
xnetresult xrtNetStreamSendRefs(xnetstream* pStream, const xnetref* pRefs, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pRefs` | 输入 | 非空数组 | 引用数组 |
| `iCount` | 输入 | > 0 | 引用数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 占用字节数达到 `WriteLimit`，全部所有权不转移
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 零复制批量发送

```c
	if ( xrtNetStreamSendRefs(pClient, Refs, 2) != XNET_RESULT_OK ) {
```


### `xrtNetStreamSendTake`

有界接管非空数据；`NULL,0` 是无操作，非空指针配零长度是参数错误。

```c
xnetresult xrtNetStreamSendTake(xnetstream* pStream, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pData` | 输入 | 允许空 | 拥有的数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_ARGUMENT` — 非空指针配零长度
- `XERR_AGAIN` — 占用字节数达到 `WriteLimit`，所有权不转移
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 接管发送

```c
	if ( xrtNetStreamSendTake(pClient, pTake, 10) ==
		 XNET_RESULT_OK ) {
```


### `xrtNetStreamSendBuffer`

在所属 Worker 上零复制接管缓冲链；失败时源缓冲保持不变。

```c
xnetresult xrtNetStreamSendBuffer(xnetstream* pStream, xnetbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pBuffer` | 输入 | 非空 | 缓冲链 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 操作成功 | — |
| `XNET_RESULT_ERROR` | 失败 | — |
| `XNET_RESULT_AGAIN` | 非阻塞暂不可推进 | — |
| `XNET_RESULT_CLOSED` | 对象已关闭 | — |
| `XNET_RESULT_TRUNCATED` | 结果被截断 | — |
| `XNET_RESULT_TIMEOUT` | 期限先到期 | — |
| `XNET_RESULT_CANCELLED` | 被取消 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非所属 Worker 调用
- `XERR_AGAIN` — 预算已满，源缓冲保持不变

#### 范例

[tcp](../../examples/network/tcp/main.c) · 缓冲链发送

```c
		(void)xrtNetStreamSendBuffer(pStream, pBuffer);
```


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

### `xrtNetStreamPause`

暂停新读取；一个已经提交的 completion 仍可能到达。

```c
void xrtNetStreamPause(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已请求暂停 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 暂停读取

```c
	xrtNetStreamPause(pServer);
```


### `xrtNetStreamResume`

无分配恢复读取，并把并发请求合并后唤醒所属 Worker。

```c
bool xrtNetStreamResume(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已恢复 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 恢复读取

```c
	if ( !xrtNetStreamResume(pServer) ||
		 !xrtNetStreamWaitAvailable(pServer, 4u,
			EXAMPLE_DEADLINE_MS, NULL) ) {
```


### `xrtNetStreamShutdownWrite`

排空发送队列后执行 TCP 写半关闭，读取方向继续工作。

```c
bool xrtNetStreamShutdownWrite(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已请求 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 写半关闭

```c
	if ( !xrtNetStreamShutdownWrite(pClient) ) {
```


### `xrtNetStreamClose`

停止读取并在发送队列排空后正常关闭。

```c
bool xrtNetStreamClose(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已请求 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp](../../examples/network/tcp/main.c) · 正常关闭

```c
	(void)xrtNetStreamClose(Example.Client);
```


### `xrtNetStreamAbort`

取消在途 IO、丢弃发送队列并立即异常关闭。

```c
bool xrtNetStreamAbort(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp](../../examples/network/tcp/main.c) · 异常关闭

```c
		(void)xrtNetStreamAbort(Example.Client);
```


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

### `xrtNetStreamBuffer`

在所属 Worker 内返回借用的只读接收缓冲；不能保存或直接消费。

```c
const xnetbuf* xrtNetStreamBuffer(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读缓冲借用（回调期间有效） | — |
| `NULL` | 非所属 Worker | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非所属 Worker 调用

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 接收缓冲借用

```c
	pBuffer = xrtNetStreamBuffer(pTask->pStream);
```


### `xrtNetStreamAvailable`

返回当前累积的可读字节数；该并发快照不会借出缓冲。

```c
size_t xrtNetStreamAvailable(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 可读字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 可读字节数

```c
			if ( xrtNetStreamAvailable(pServer) != 0u ) {
```


### `xrtNetStreamConsume`

在所属 Worker 内消费最多指定字节而不复制。

```c
size_t xrtNetStreamConsume(xnetstream* pStream, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iSize` | 输入 | — | 最多消费字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际消费字节数 | — |
| 钳制值 | 越界钳制到可用量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非所属 Worker 调用

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 消费字节

```c
	pTask->iConsume = xrtNetStreamConsume(pTask->pStream, 4u);
```


### `xrtNetStreamRead`

在所属 Worker 内复制并消费最多指定字节。

```c
size_t xrtNetStreamRead(xnetstream* pStream, void* pOutput, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pOutput` | 输出 | 非空 | 输出缓冲 |
| `iSize` | 输入 | — | 最多复制字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际复制字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非所属 Worker 调用

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 复制并消费

```c
	pTask->iRead = xrtNetStreamRead(pTask->pStream,
		pTask->ReadOut, 4u);
```


### `xrtNetStreamWait`

阻塞等待一个 Stream 条件；禁止从该 Stream 所属 Worker 调用。

```c
bool xrtNetStreamWait(xnetstream* pStream, xnetstreamwait Wait, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `Wait` | 输入 | — | 等待条件 |
| `iTimeout` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 条件达成 | — |
| `false` | 超时或取消 | 不设错误 |
（错误见错误节）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用
- `xrt.net` 域错误 — 等待失败

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 阻塞等待条件

```c
		xrtNetStreamWait(pClient, XNET_STREAM_WAIT_READ,
			EXAMPLE_DEADLINE_MS, NULL) ? 1 : 0);
```


### `xrtNetStreamWaitAsync`

异步等待 Stream 条件；成功、失败、取消和关闭映射到统一 Future 终态。

```c
xfuture* xrtNetStreamWaitAsync(xnetstream* pStream, xnetstreamwait Wait)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
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

[tcp_future](../../examples/network/tcp_future/main.c) · 异步等待条件

```c
	pOpen = xrtNetStreamWaitAsync(pClient, XNET_STREAM_WAIT_OPEN);
```


### `xrtNetStreamWaitAvailable`

阻塞等待至少指定数量的可读字节。

```c
bool xrtNetStreamWaitAvailable(xnetstream* pStream, size_t iMinimum, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iMinimum` | 输入 | — | 最少字节数 |
| `iTimeout` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 字节已就绪 | — |
| `false` | 超时或取消 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 阻塞等待数据量

```c
	if ( !xrtNetStreamWaitAvailable(pServer, sizeof(sExpected) - 1u,
		EXAMPLE_DEADLINE_MS, NULL) ) {
```


### `xrtNetStreamWaitAvailableAsync`

异步等待至少指定数量的可读字节。

```c
xfuture* xrtNetStreamWaitAvailableAsync(xnetstream* pStream, size_t iMinimum)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iMinimum` | 输入 | — | 最少字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 异步等待数据量

```c
	pAvailable = xrtNetStreamWaitAvailableAsync(pClient, 4u);
```


### `xrtNetStreamRecvAsync`

拉取模式下异步接收当前可用字节；成功值是借用的 `xnetbytes`。

```c
xfuture* xrtNetStreamRecvAsync(xnetstream* pStream, size_t iMaxBytes)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iMaxBytes` | 输入 | — | 最多字节数，0 = 全部当前缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future，成功值借用字节 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[tcp_future](../../examples/network/tcp_future/main.c) · 异步接收

```c
	pRequest = xrtNetStreamRecvAsync(pServer, 0);
```


## 阻塞便利层

`XRT_FEATURE_NET_TCP_SYNC` 依赖 TCP Future 与通用网络同步桥，只阻塞调用线程，不创建隐藏 Engine、Worker、辅助线程或第二套连接状态机。以下函数都复用已有对象所属 Engine：

```c
bool xrtNetStreamWait(xnetstream* pStream, xnetstreamwait Wait,
	int64 iTimeout, xcancel* pCancel);
xnetstream* xrtNetListenerAcceptWait(xnetlistener* pListener,
	int64 iTimeout, xcancel* pCancel);
xnetbytes* xrtNetStreamRecv(xnetstream* pStream, size_t iMaxBytes,
	int64 iTimeout, xcancel* pCancel);
```

这些函数不能从目标 Stream 或 Listener 所属 Worker 调用，否则在阻塞事件循环前以 `XERR_STATE` 失败并撤销刚建立的 Future。成功接受返回调用方 Stream 引用；成功接收返回调用方 `xnetbytes` 引用，两者都独立于内部 Future，分别使用 `xrtNetStreamDestroy` 与 `xrtNetBytesDestroy` 释放。创建 Future、复制结果或增加结果引用发生 OOM 时不消费连接或接收字节。

同步等待只有一个线性化结果：Future 已经在等待锁内进入终态时，底层操作终态获胜；等待先返回超时或外部取消时，控制原因获胜，同步层立即向 Future 发出协作取消，并以对应 `XERR_TIMEOUT` 或 `XERR_CANCELLED` 返回。随后到达的网络终态不会改写本次调用的错误。对象关闭则保留 `XERR_CLOSED`，底层失败保留完整结构化原因链和 TCP 操作码。

`XRT_FEATURE_NET_TCP_DIAL_SYNC` 在相同契约上提供主机名连接：

```c
xnetstream* xrtNetConnect(xnetengine* pEngine, xnetresolver* pResolver,
	cstr sHost, uint16 iPort, const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents, ptr pStreamData,
	int64 iTimeout, xcancel* pCancel);
```

它不创建默认 Resolver 或隐藏 Engine。截止时间和外部取消结束本次阻塞连接并取消整个 Dial；成功返回已经 `OPEN` 的调用方 Stream 引用。Resolver、候选连接和系统错误的原因链与 `xrtNetDialAsync` 完全一致。

### `xrtNetStreamRecv`

阻塞接收一段拥有型字节；零上限表示读取全部当前缓冲。

```c
xnetbytes* xrtNetStreamRecv(xnetstream* pStream, size_t iMaxBytes, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `iMaxBytes` | 输入 | — | 最多字节数，0 = 全部 |
| `iTimeout` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有型字节，用后释放 | — |
| `NULL` | 超时、取消或失败 | 超时/取消不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 从所属 Worker 调用
- `xrt.net` 域错误 — 接收失败

#### 范例

[tcp_dial_tour](../../examples/network/tcp_dial_tour/main.c) · 阻塞接收

```c
		xnetbytes* pBytes = xrtNetStreamRecv(pServer, 4,
			EXAMPLE_DEADLINE_MS, NULL);
```


### `xrtNetStreamState`

返回 Stream 当前状态的并发快照。

```c
xnetstreamstate xrtNetStreamState(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举值 | 当前生命周期状态 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp](../../examples/network/tcp/main.c) · Stream 状态

```c
		 (xrtNetStreamState(Example.Client) != XNET_STREAM_CLOSED) ) {
```


### `xrtNetStreamError`

返回导致 Stream 关闭的借用错误；正常关闭时为空。

```c
const xerror* xrtNetStreamError(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 关闭原因借用 | — |
| `NULL` | 正常关闭 | 不设错误 |

#### 错误

- 无错误 — 正常关闭返回 `NULL` 且不设置错误

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · Stream 错误

```c
		xrtNetStreamError(pClient) == NULL ? "(none)" : "err");
```


### `xrtNetStreamStats`

复制 Stream 并发统计。

```c
bool xrtNetStreamStats(const xnetstream* pStream, xnetstreamstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · Stream 统计

```c
	if ( !xrtNetStreamStats(pClient, &Stats) ||
		 (Stats.SentBytes < 63u) ) {
```


### `xrtNetStreamLocal`

复制 Stream 本地地址，成功才修改输出。

```c
bool xrtNetStreamLocal(const xnetstream* pStream, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pAddress` | 输出 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 本地地址

```c
	(void)xrtNetStreamLocal(pClient, &Address);
```


### `xrtNetStreamRemote`

复制 Stream 远端地址，成功才修改输出。

```c
bool xrtNetStreamRemote(const xnetstream* pStream, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pAddress` | 输出 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 地址已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tls_stream](../../examples/tls/stream/main.c) · 远端地址

```c
	if ( xrtNetStreamRemote(
		xrtTlsStreamTransport(pStream),
		&Remote
	) && xrtNetAddrText(
		&Remote,
		Address,
		sizeof(Address)
	) ) {
```


### `xrtNetStreamPending`

返回已经占用发送预算但尚未离开队列的字节数。

```c
size_t xrtNetStreamPending(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 占用预算字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 占用字节数

```c
		while ( xrtNetStreamPending(pClient) != 0u ) {
```


### `xrtNetStreamWriteLimit`

返回创建 Stream 时固定的发送硬上限。

```c
size_t xrtNetStreamWriteLimit(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 发送硬上限 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 发送硬上限

```c
		xrtNetStreamWriteLimit(pClient) >= 65536u ? "64k" : "?",
```


### `xrtNetStreamWritable`

返回当前仍可受理的发送硬预算快照。

```c
size_t xrtNetStreamWritable(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 剩余发送预算 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 剩余预算

```c
		xrtNetStreamWritable(pClient) > 0u ? "0" : "?");
```


### `xrtNetStreamSocket`

只在 Stream Worker 回调内返回借用 Socket；不能关闭或接管其 IO。

```c
xnetsocket xrtNetStreamSocket(xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 句柄 | Socket 借用（回调期间有效） | — |
| 无效句柄 | 非回调内 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非所属 Worker 回调内调用

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · Socket 借用

```c
	pTask->bSocket = xrtNetStreamSocket(pTask->pStream) != NULL;
```


### `xrtNetStreamWorker`

返回 Stream 所属的借用 Worker。

```c
xnetworker* xrtNetStreamWorker(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Worker 借用 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 所属 Worker

```c
		xrtNetStreamWorker(pServer), &Post, exampleStreamTask,
```


### `xrtNetStreamData`

返回线程安全的 Stream 用户数据指针快照，不延长目标生命周期。

```c
ptr xrtNetStreamData(const xnetstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 用户数据快照 | — |
| `NULL` | 未设置 | 不设错误 |

#### 错误

- 无错误 — 未设置返回 `NULL` 且不设置错误

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 用户数据快照

```c
		 (xrtNetStreamData(pServer) != NULL)) ? "ok" : "fail");
```


### `xrtNetStreamSetData`

只在 Stream Worker 回调内替换用户数据。

```c
bool xrtNetStreamSetData(xnetstream* pStream, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pData` | 输入 | 任意值 | 新用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换 | — |
| `false` | 非回调内 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非所属 Worker 回调内调用

#### 范例

[tcp](../../examples/network/tcp/main.c) · 替换用户数据

```c
	(void)xrtNetStreamSetData(pStream, pExample);
```


### `xrtNetStreamSetEvents`

替换 Stream 事件表与用户数据。

```c
bool xrtNetStreamSetEvents(xnetstream* pStream, const xnetstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标 Stream |
| `pEvents` | 输入 | 非空 | 新事件表 |
| `pData` | 输入 | 任意值 | 事件数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（已关闭/非所属 Worker 等）

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 替换事件表

```c
	pTask->bSetEvents = xrtNetStreamSetEvents(pTask->pStream,
		&s_Events, NULL);
```


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

## 代理层（proxy.h）

以下各节复用自 [proxy.md](proxy.md)（tcp.md 与其共享 `include/xrt/proxy.h`）。

### `xrtNetProxyConfigInit`

初始化 SOCKS5、自动认证且没有固定容量字段的代理配置。

```c
void xrtNetProxyConfigInit(xnetproxyconfig* pConfig)
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

[proxy_tour](../../examples/network/proxy_tour/main.c) · 默认配置

```c
	xrtNetProxyConfigInit(&ProxyConfig);
```


### `xrtNetProxyCreate`

深拷贝代理端点和凭据，创建可跨线程共享的不可变对象。

```c
xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空且通过校验 | 代理配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 代理对象（引用 1） | — |
| `NULL` | 创建失败 | `xrt.net` 域错误 |

#### 错误

- `xrt.net` / `XNET_ERROR_PROXY_CONFIG`（`XERR_ARGUMENT` / `XERR_VALUE`） — 配置字段非法或组合不支持
- `xrt.net` / `XNET_ERROR_PROXY_CREATE` — 对象或内部缓冲分配失败（`XERR_MEMORY` 等）

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 创建代理对象

```c
	pProxy = xrtNetProxyCreate(&ProxyConfig);
```


### `xrtNetProxyRetain`

增加代理对象引用并返回原指针。

```c
xnetproxy* xrtNetProxyRetain(const xnetproxy* pProxy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProxy` | 输入 | 非空 | 目标代理 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 共享引用

```c
		((pRetained = xrtNetProxyRetain(pProxy)) != pProxy) ) {
```


### `xrtNetProxyRelease`

释放代理对象引用；最后一个引用会清零整块配置存储。

```c
void xrtNetProxyRelease(xnetproxy* pProxy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProxy` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 释放引用

```c
	xrtNetProxyRelease(pRetained);
```


### `xrtNetProxyInfo`

复制代理对象的只读信息视图。

```c
bool xrtNetProxyInfo(
	const xnetproxy* pProxy,
	xnetproxyinfo* pInfo
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProxy` | 输入 | 非空 | 目标代理 |
| `pInfo` | 输出 | 非空 | 接收信息快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 信息视图

```c
	if ( !xrtNetProxyInfo(pProxy, &Info) ||
		(Info.Type != XNET_PROXY_SOCKS5) ||
		(Info.Host.Size != 12u) ||
		(memcmp(Info.Host.Data, "socks5.local", 12u) != 0) ||
		(Info.Port != 1080u) ) {
```


### `xrtNetProxyHandshakeConfigInit`

初始化握手配置；64 KiB 上限主要约束后续 HTTP CONNECT Header。

```c
void xrtNetProxyHandshakeConfigInit(
	xnetproxyhandshakeconfig* pConfig
)
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

[proxy_tour](../../examples/network/proxy_tour/main.c) · 握手配置

```c
	xrtNetProxyHandshakeConfigInit(&HsConfig);
```


### `xrtNetProxyHandshakeCreate`

创建握手并立即生成首个协议报文；目标主机会被深拷贝。

```c
xnetproxyhandshake* xrtNetProxyHandshakeCreate(
	const xnetproxyhandshakeconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空且通过校验 | 握手配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 握手对象，首个报文已就绪 | — |
| `NULL` | 创建失败 | `xrt.net` 域错误 |

#### 错误

- `xrt.net` / `XNET_ERROR_PROXY_CONFIG`（`XERR_ARGUMENT` / `XERR_VALUE`） — 配置字段非法或组合不支持
- `xrt.net` / `XNET_ERROR_PROXY_CREATE` — 对象或内部缓冲分配失败（`XERR_MEMORY` 等）
- `xrt.net` / `XNET_ERROR_PROXY_LIMIT`（`XERR_RANGE`） — 超出协议上限

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 创建握手

```c
	pHandshake = xrtNetProxyHandshakeCreate(&HsConfig);
```


### `xrtNetProxyHandshakeDestroy`

销毁握手，并清零尚未发送的认证报文和内部目标信息。

```c
void xrtNetProxyHandshakeDestroy(xnetproxyhandshake* pHandshake)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 销毁握手

```c
	xrtNetProxyHandshakeDestroy(pHandshake);
```


### `xrtNetProxyHandshakeState`

返回当前握手状态；空指针返回 `ERROR`。

```c
xnetproxyhandshakestate xrtNetProxyHandshakeState(
	const xnetproxyhandshake* pHandshake
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 目标握手 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_PROXY_HANDSHAKE_WRITE` | 有输出待发送 | — |
| `XNET_PROXY_HANDSHAKE_READ` | 等待代理回复 | — |
| `XNET_PROXY_HANDSHAKE_READY` | 隧道已建立 | — |
| `XNET_PROXY_HANDSHAKE_ERROR` | 失败（含空句柄） | 见 `HandshakeError` |

#### 错误

- 无 — 状态查询不设置错误；空句柄返回 `ERROR`

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 握手状态

```c
		(xrtNetProxyHandshakeState(pHandshake) !=
			XNET_PROXY_HANDSHAKE_WRITE) ||
```


### `xrtNetProxyHandshakeStep`

处理输入链中的完整协议前缀；只消费代理回复，成功后的应用数据保持原位。`WRITE` 状态必须先发送并确认全部输出，`READ` 状态才会继续解析输入。

```c
xnetproxyhandshakestate xrtNetProxyHandshakeStep(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入/输出 | 非空 | 目标握手 |
| `pInput` | 输入/输出 | 非空 | 输入链 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_PROXY_HANDSHAKE_WRITE` | 有输出待发送 | — |
| `XNET_PROXY_HANDSHAKE_READ` | 等待代理回复 | — |
| `XNET_PROXY_HANDSHAKE_READY` | 隧道已建立 | — |
| `XNET_PROXY_HANDSHAKE_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` / `XNET_ERROR_PROXY_PROTOCOL` — 协议状态非法、回复不完整或回复码表示失败
- `xrt.net` / `XNET_ERROR_PROXY_LIMIT`（`XERR_RANGE`） — 超出协议上限

#### 范例

[proxy_socks5](../../examples/network/proxy_socks5/main.c) · 推进握手

```c
		(xrtNetProxyHandshakeStep(pHandshake, &Input) !=
		 XNET_PROXY_HANDSHAKE_WRITE) ||
```


### `xrtNetProxyHandshakeOutput`

借用当前待发送的首段连续输出；失败时把非空输出规范化为空 Span。

```c
bool xrtNetProxyHandshakeOutput(
	const xnetproxyhandshake* pHandshake,
	xnetspan* pOutput
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 目标握手 |
| `pOutput` | 输出 | 非空 | 接收输出 Span |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | Span 已写出（可为空） | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非 `WRITE` 状态请求输出

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 待发送输出

```c
		!xrtNetProxyHandshakeOutput(pHandshake, &Output) ||
```


### `xrtNetProxyHandshakeSent`

确认已经发送的输出前缀；支持 Socket 部分写入。

```c
size_t xrtNetProxyHandshakeSent(
	xnetproxyhandshake* pHandshake,
	size_t iSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入/输出 | 非空、`WRITE` 状态 | 目标握手 |
| `iSize` | 输入 | <= 待发送量 | 本次已发送字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 剩余待发送字节数，0 = 全部确认 | — |
| `0` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 非 `WRITE` 状态或确认量超出待发送量

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 确认发送

```c
	(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);
```


### `xrtNetProxyHandshakeBound`

`READY` 后复制可用的绑定端点；HTTP CONNECT 没有该信息并返回 `NOT_FOUND`。

```c
bool xrtNetProxyHandshakeBound(
	const xnetproxyhandshake* pHandshake,
	xnetproxyendpoint* pEndpoint
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空、已 READY | 目标握手 |
| `pEndpoint` | 输出 | 非空 | 接收端点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 端点已复制 | — |
| `false` | 无绑定信息或状态非法 | `XERR_NOT_FOUND` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_STATE` — 尚未 READY
- `XERR_NOT_FOUND`（`xrt.net` / `PROXY_PROTOCOL`） — HTTP CONNECT 无绑定端点

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 绑定端点

```c
	if ( xrtNetProxyHandshakeBound(pHandshake, &Endpoint) ||
		xrtNetProxyHandshakeCode(pHandshake, &iCode) ||
		(xrtNetProxyHandshakeError(pHandshake) != NULL) ) {
```


### `xrtNetProxyHandshakeError`

返回协议失败时捕获的不可变错误；对象所有权仍属于握手。

```c
const xerror* xrtNetProxyHandshakeError(
	const xnetproxyhandshake* pHandshake
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 目标握手 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误借用（存活到销毁或下一次 Step） | — |
| `NULL` | 尚无错误 | 不设错 |

#### 错误

- 无错误 — 非 `ERROR` 状态返回 `NULL` 且不设置错误

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 握手错误

```c
		(xrtNetProxyHandshakeError(pHandshake) != NULL) ) {
```


### `xrtNetProxyHandshakeCode`

复制 SOCKS5 线路回复码或 HTTP 状态码；尚未收到回复时返回 `false`。

```c
bool xrtNetProxyHandshakeCode(
	const xnetproxyhandshake* pHandshake,
	uint32* pCode
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandshake` | 输入 | 非空 | 目标握手 |
| `pCode` | 输出 | 非空 | 接收回复码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 回复码已复制 | — |
| `false` | 尚未收到回复 | 不设错 |

#### 错误

- 尚未收到回复返回 `false` 且不设置错误；句柄或输出为空 `XERR_ARGUMENT`

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 线路回复码

```c
		xrtNetProxyHandshakeCode(pHandshake, &iCode) ||
```


### `xrtNetProxyDialConfigInit`

初始化 TCP 拨号、64 KiB 协议上限和 30 秒全过程超时。

```c
void xrtNetProxyDialConfigInit(xnetproxydialconfig* pConfig)
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

[proxy_tour](../../examples/network/proxy_tour/main.c) · 拨号配置

```c
	xrtNetProxyDialConfigInit(&DialConfig);
```


### `xrtNetProxyDial`

连接代理端点并完成目标 CONNECT；成功 Stream 引用转移给完成回调。非 Worker 提交者可能与完成回调并发，不能依赖返回值已经完成赋值。

```c
xnetproxydial* xrtNetProxyDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xnetproxy* pProxy,
	cstr sTargetHost,
	uint16 iTargetPort,
	const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetproxydialproc pDone,
	ptr pDoneData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 允许空 | 名称解析器 |
| `pProxy` | 输入 | 非空 | 代理对象 |
| `sTargetHost` | 输入 | 非空、零结尾 | 目标主机 |
| `iTargetPort` | 输入 | — | 目标端口 |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |
| `pStreamEvents` | 输入 | 允许空 | Stream 事件表 |
| `pStreamData` | 输入 | 任意值 | Stream 数据 |
| `pDone` | 输入 | 非空 | 完成回调 |
| `pDoneData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拨号对象（引用 1） | — |
| `NULL` | 提交失败 | `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.net` / `XNET_ERROR_PROXY_CONFIG`（`XERR_ARGUMENT` / `XERR_VALUE`） — 配置字段非法或组合不支持
- `xrt.net` / `XNET_ERROR_PROXY_CREATE` — 对象或内部缓冲分配失败（`XERR_MEMORY` 等）
- `xrt.net` / `XNET_ERROR_PROXY_CONNECT` — 连接提交失败
- `xrt.net` / `XNET_ERROR_PROXY_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 配置的协议或地址族不支持

#### 范例

[proxy_dial](../../examples/network/proxy_dial/main.c) · 发起拨号

```c
	pDial = xrtNetProxyDial(
		pEngine,
		pResolver,
		pProxy,
		argv[3],
		iTargetPort,
		&DialConfig,
		&StreamEvents,
		&Example,
		exampleProxyDialDone,
		&Example
```


### `xrtNetProxyDialRef`

增加 Proxy Dial 引用并返回原指针。

```c
xnetproxydial* xrtNetProxyDialRef(xnetproxydial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标拨号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 共享引用

```c
	pDialRef = xrtNetProxyDialRef(pDial);
```


### `xrtNetProxyDialDestroy`

释放 Proxy Dial 引用；空指针视为空操作。

```c
void xrtNetProxyDialDestroy(xnetproxydial* pDial)
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

[proxy_tour](../../examples/network/proxy_tour/main.c) · 释放引用

```c
	xrtNetProxyDialDestroy(pDialRef);
```


### `xrtNetProxyDialCancel`

协作取消名称解析、TCP 连接或代理握手；首个取消请求获胜，已终态对象返回 `false` 且不设错。

```c
bool xrtNetProxyDialCancel(xnetproxydial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标拨号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 取消请求已被受理 | — |
| `false` | 已连接、已失败、已取消或并发取消已被受理 | 不设错误 |

#### 错误

- 已终态或并发取消已被受理返回 `false` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[proxy_dial](../../examples/network/proxy_dial/main.c) · 协作取消

```c
		(void)xrtNetProxyDialCancel(pDial);
```


### `xrtNetProxyDialState`

返回当前拨号阶段或不可变终态。

```c
xnetproxydialstate xrtNetProxyDialState(
	const xnetproxydial* pDial
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标拨号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_PROXY_DIAL_RESOLVING` / `CONNECTING` / `HANDSHAKE` | 进行中阶段 | — |
| `XNET_PROXY_DIAL_CONNECTED` / `FAILED` / `CANCELLED` | 不可变终态 | — |

#### 错误

- 无 — 原子状态查询不设置错误；空句柄返回 `RESOLVING`

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 拨号状态

```c
		xnetproxydialstate State = xrtNetProxyDialState(pDial);
```


### `xrtNetProxyDialError`

失败或取消后借用完整错误原因链。

```c
const xerror* xrtNetProxyDialError(
	const xnetproxydial* pDial
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标拨号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误借用（存活到对象销毁） | — |
| `NULL` | 进行中或已连接 | 不设错 |

#### 错误

- 无错误 — 无失败时返回 `NULL` 且不设置错误

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 拨号错误

```c
			(xrtNetProxyDialError(pDial) == NULL) ||
```


### `xrtNetProxyDialStats`

复制代理阶段和底层 TCP 地址竞速统计。

```c
bool xrtNetProxyDialStats(
	const xnetproxydial* pDial,
	xnetproxydialstats* pStats
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 目标拨号 |
| `pStats` | 输出 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已复制 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[proxy_tour](../../examples/network/proxy_tour/main.c) · 拨号统计

```c
			!xrtNetProxyDialStats(pDial, &DialStats) ||
```


## 原生 TCP 拥有图与非等待退役（V1）

`xrtNetStreamOwnership`、`xrtNetListenerOwnership` 返回真实物理引用计数和
强拥有边的借用视图。它们不是独立保活引用，必须在整个可达图冻结且代码常驻时使用。
相应 `OwnershipQuiescenceV1Get` 仅报告瞬时忙碌，不能代替生命周期准入。

Engine 真正持有一个侵入式运行时槽；该槽接管构造时已有的运行时引用，不增加第二份引用。
Stream／Listener 完成全部原生关闭义务后，从 Engine 摘除并消费该槽。
Listener 接受队列的每个位置另持有一次已接受 Stream 引用；链接不是额外拥有边。
Stream 的非静态 Error／Cause 诊断链也按实际强引用导出；不可变错误 DAG 不会反向引用
Stream，和 Error 自身的不可变尾部相同，Clear 后保留到最后一次 Drop 才释放。
连接超时和接受重试计时器仍使用原有回调在终态消费其已保留引用，元数据不再重复 Drop。
`xrtNetStreamRuntimeOwnershipV1Get` 和 `xrtNetListenerRuntimeOwnershipV1Get` 给出这些
不可变常驻合同，交由 `xrtNetEngineOwnershipAdapterV3` 做精确身份准入。

`xrtNetStreamOwnershipAdapterV1`、`xrtNetListenerOwnershipAdapterV1` 成功时一并返回独立
preparation 描述符。失败不修改输出或 ambient error。Hold／Drop 是真实引用；Claim／Restore
必须在冻结下执行，令牌唯一且稳定；Clear 只能发生在重新证明整个图不可达且 Ready 后。
Clear 不执行回调、不等待、不释放别的未保活目标；本版本无需额外 Finish。

Prepare 不等待，也不把排队成功误当成完成。它只发布借用原运行时槽的嵌入式关闭命令；
该命令不新建根引用，Prepare 返回 BUSY 时不改变拥有边或执行语义回调。
发布失败在冻结内回滚预约。命令尚未开始时，正常关闭也保留运行时槽及 Engine 占用，
避免悬空命令。实际端口完成、计时器终态、缓冲归还、通知和运行时槽摘除完成后才发布 Ready。
协调器每次推进后必须重新取得静止点、检查真实图，不能沿用旧快照。

当前 V1 只准入能够完整证明的无用户回调／无 Data、已认证缓冲及 Future 等待的原生状态。
拨号控制器、未认证缓冲和任意用户上下文尚未取得完整合同，因此仍明确拒绝，
不把它们伪装成叶子对象。这是后续受管理传输的原生前置，不代表整个 xlang TCP 接入完成。
外部调用和取消回调以真实接收者引用维持调用期，并登记完整元数据修改区间；不跨回调持有图锁。
原有普通 C 调用仍须保证 Engine 销毁与外部使用者静止，不得把借用 Worker 当成独立所有者。

验证工具 `tools/check_net_tcp_ownership.py` 只执行明确列出的离线图与自建回环生命周期测试，
包括 select、Windows IOCP／Linux epoll，模块化／单头、GCC／TCC 及 Linux 实际闭包的
ASan／UBSan。它不运行公网、DNS、扫描、畸形报文或网络安全专项测试。

## 示例与发布门槛

### 原生缓冲拥有图与 `xrtNetStreamSendBytes`

```c
xnetresult xrtNetStreamSendBytes(xnetstream* stream, xnetbytes* bytes,
    size_t offset, size_t size);
```

严格验证字节范围，成功后零拷贝保留指定不可变片段；拒绝／OOM 不消费调用者引用，空发送不增加引用。
释放行为与 SendTake／SendBuffer 一致：数据确实消费或关闭回收后归还引用，不能仅因调用已返回就释放。

Stream 拥有图现在枚举读写缓冲内每个实际池租约、原生不可变字节以及发送帧确实持有的 Stream 自引用。
本地复制块不产生额外 Stream 引用；跨线程复制、Take、Bytes 发送帧各持有其构造时取得的一份真实引用。
SendBuffer 的多个 FIFO 视图共同保留原始块链：第一份尚存视图枚举原始链和批次唯一的 Stream 引用，
不能把每个视图误算成独立 Stream 引用，也不能漏掉已经发出前缀仍由整批保留的原块。
块采用原生构造时的内部来源标记，不从任意用户释放回调猜测拥有权；不增加每块引用计数或影子登记表。
未知 SendRef／SendRefs 上下文、文件块和上层组合控制器仍拒绝图接纳，不能视为完整托管传输已完成。

专项普通回环测试记录实际留存的写队列数；Linux 必须覆盖全部六种真实留存路径。
Windows 回环可能立即将数据复制进内核，此时只算接收缓冲与完成释放的验证，不宣称覆盖了留存写队列。

### TCP Future 的真实生产者与终态结果

`xrtNetTcpFutureProducerPolicyV1Get`、`xrtNetTcpFutureCancelPolicyV1Get` 返回等待激活的
精确生产者／取消监听合同；`xrtNetTcpFutureBytesPayloadPolicyV1Get`、
`xrtNetTcpFutureStreamPayloadPolicyV1Get` 返回接收字节／已接受 Stream 的终态结果合同。
这些常驻描述符按身份准入，复制结构体不能冒充能力。应分别传入 Future V4、Cancel V2、
CancelWatch V1 的允许集；源 Stream／Listener 可追踪等待节点，不代表所有子节点自动获得准入。

一个物理等待激活分别由等待链、Future 生产者槽和取消 Watch.Data 各拥有一次真实引用；
激活本身保留源对象、缓存分配器 Engine 租约、Promise 和 Watch 登记端点。
Promise 与 Future 是同一物理身份，链表 Next 不是额外拥有引用，Cancel 的链接也不是 Watch 的拥有槽。
不增加单独激活分配或每节点锁，Listener 和 Stream 等待节点仍分别位于 64／128 字节缓存档。

`xrtNetTcpFutureOwnershipQuiescenceV1Get`、`xrtNetTcpFutureOwnershipAdapterV1` 只用于这些真实
激活节点。调用期间或发布中返回忙；Prepare 是有界、非等待的观察，不偷偷取消已受理的工作。
完整图内的源对象按正常原生关闭／通知推进，激活解除 Watch、交付 Promise 终态、归还源引用后才 Ready。
生产者 Drop 仅消费真实引用，不把“断开拥有槽”解释为取消或 Future 完成。

普通最后引用归还缓存；若收集器等确实保活了已完成激活，立即归还缓存分配器的 Engine 租约，
留下无源对象、无 Promise、无 Watch 的惰性外壳，避免 Engine 退役反过来等待协调器销毁节点。
非最后一次 Drop 不再访问可能由另一线程最终释放的节点。取消只结束当前等待，不关闭底层传输。
已完成接收 Future 只拥有不可变字节，已取消 Future 不再拥有传输；已接受结果则实际拥有 Stream。

Accept 已取得连接、但生产端 Promise 拒绝结果发布时，没有调用者取得该创建者引用。
此时先 Abort 再归还引用，不能只 Destroy 留下由 Engine 运行时槽独自保活的孤立连接。
单头普通生命周期夹具直接控制这一线性化边界，不增加生产测试钩子或伪造对象。

`tools/check_net_tcp_future_ownership.py` 批量运行精确图、普通收发、Accept、并发取消／关闭、
分配失败恢复和模块化／单头验证，包括 select、Windows IOCP／Linux epoll、GCC／TCC 和 Linux
实际闭包 ASan／UBSan；每个输入、日志和产物均记录摘要。它不运行网络安全专项测试，也不等于
xlang 全部受管理 TCP 或发布 DLL／SO 已完成验收。

### Dial 的真实拥有图及显式地址列表

`xrtNetDialOwnedV1` 使用常驻 `xnetdialownershipv1` 描述唯一完成回调及其上下文。
提交成功消费一份真实上下文引用，失败不消费；Done 借用上下文，Drop 在完成回调物理尾部后执行。
不把任意回调数据扁平化成叶节点，也不以状态或活动资源计数推算强引用。

Dial 在 Engine 中注册真实运行时槽。调用者、运行时槽和 Resolver 回调数据各有独立引用；
Resolver 回调把处理阶段交给网络 Worker 时，先取得命令的实际引用，再由 Resolver 归还回调数据。
候选的 intrusive 链只是借用链接，其真实 Dial 和 Stream 创建者引用属于 Stream.Control；
原有截止／fallback Timer 的引用分别由 Engine 的对应定时器槽枚举。
`xrtNetDialOwnership`、`xrtNetDialRuntimeOwnershipV1Get`、`xrtNetDialResolverPolicyV1Get`、
`xrtNetDialOwnershipQuiescenceV1Get`、`xrtNetDialOwnershipAdapterV1` 提供精确准入。
Engine V3、Resolver 请求及 Dial 的完成策略必须各自显式授予正确身份，复制的策略表不等同于原表。
构造、跨 Resolver 发布和实际 Worker 执行中拒绝检查；Prepare 非等待地请求取消，
真实结果回调和运行时槽移除后才 READY。最终局部引用的归还仍按真实 RC 记录；
协调器必须重新证明不可达，不能把 READY 当成所有物理尾部引用已归零。

`xrtNetDialAddresses`、`xrtNetDialAddressesOwnedV1`、`xrtNetDialAddressesAsync`
使用同一个 Dial 候选竞争算法，直接接收不可变 `xnetaddrlist`，不创建或查询 Resolver。
调用期间借用列表，异步装配保留独立引用；每个合格候选必须有非零端口，并保留各自端口。
`Config.Family` 显式筛选候选，双栈时保留原有族内顺序及交错规则；无合格地址时 checked 失败。
主机路径仍覆盖解析结果端口，语言或 C 调用者可显式使用 `xrtNetAddrListWithPort` 准备地址列表。

Dial Future 的所有可失败桥接装配先于工作受理。Bridge 只借用 Promise，真实激活持有 Promise、
Dial 和唯一取消 Watch；构造者、生产者、Watch 和 Dial 完成上下文分别拥有实际引用。
`xrtNetDialFutureProducerPolicyV1Get`、`xrtNetDialFutureCancelPolicyV1Get`、
`xrtNetDialFutureDialPolicyV1Get`、`xrtNetDialFuturePayloadPolicyV1Get`、
`xrtNetDialFutureOwnershipQuiescenceV1Get`、`xrtNetDialFutureOwnershipAdapterV1`
分别声明生产者、取消、完成、拥有型 Stream 结果和激活节点的合同。
取消不伪造终态，Prepare 不以放弃仍可能完成的操作来破环；失败或被拒绝交付的 Stream 先 Abort 再 Destroy。
成功结果维持既有 C 契约：Future 持有 Stream，调用者需显式 Ref 才取得独立引用，关闭与 Destroy 分开。

`tools/check_net_tcp_dial_ownership.py` 只执行显式允许的离线 ABI／生命周期／分配失败和
自建 loopback 正常通信，包含模块化／单头、GCC／TCC、两个平台原生后端与 Linux ASan／UBSan。
不执行 DNS、公网、扫描、畸形协议或网络安全测试，不宣称 xlang Dial／代理或完整发布产品已经接入。

推送模式回环示例位于 `examples/network/tcp/main.c`，聚合服务端示例位于 `examples/network/tcp_server/main.c`，拉取与 Future 请求响应示例位于 `examples/network/tcp_future/main.c`，阻塞 TCP 与托管连接示例位于 `examples/network/tcp_sync/main.c`、`examples/network/tcp_dial_sync/main.c`。测试使用同一契约分别验证 select 和 IOCP，并覆盖慢速对端大块发送、高低水位与 Drain、`ReadLimit` 自动停读和恢复、跨 Worker Accept/Close 排序、有界拉取 Accept 与溢出、多端点共享端口、reuse-port、聚合关闭、连接和 Accept Future、Resolver 合并、缓存命中时的 Worker 内非重入提交、跨线程 Dial 取消、双栈候选竞争、并发连接、结果复制与 Dial OOM、同步超时/取消/关闭竞态、结果 OOM 不消费、协程挂起恢复和单头文件真实收发。

TLS 传输适配已经作为独立 `XRT_FEATURE_TLS_STREAM` 裁剪单元落地，直接使用 `SendBuffer` 把完整密文块链移动到 TCP 队列。`xrtTlsStreamTransport()` 返回的原始 Stream 只供地址、统计和安全 socket 选项查询；直接关闭、收发、替换事件或消费其缓冲会破坏 TLS 组合状态机。更高层协议直接组合 TCP Server 和 TLS Stream，不改变这里的字节流、所有权、背压、等待、Dial 和关闭契约。

### 文件发送的后端能力边界

Windows 的非阻塞内核文件发送需要支持 `XNET_PORT_CAP_SEND_FILE` 的后端（IOCP）。
select 不具备此能力；非空 `xrtNetStreamSendFile` 现在在提交前返回
`XNET_RESULT_ERROR`／`XERR_UNSUPPORTED`，不预留发送预算、不复制句柄、不入队，
也不因能力不支持而中止现有连接。空区间维持原有无操作合同。
不自动切换后端或隐式把文件读取为内存；应用可显式选择 IOCP 或自己的分块发送策略。

`tools/check_net_tcp_file_capability.py` 验证自建字面量回环上的正常文件区间发送、
NUL、源游标不变、提交后立即关闭源文件、精确内存平衡，以及 Windows select
拒绝后连接仍可正常收发。覆盖 GCC／TCC、O0／O2、两端本机后端，Linux 另有
ASan／UBSan；不执行外网、探测、畸形协议或网络安全测试。

### Listener 的拥有型事件上下文

`xrtNetListenOwnedV1(engine, config, data, policy)` 与原有 `xrtNetListen` 共用创建、
绑定、预投递和分发实现；成功转移一份真实 Data 引用，任一同步失败不消费。
没有额外的上下文包装分配。调用者需保持不可变 `xnetlistenereventsownershipv1`
描述符与回调代码驻留；如果还要在调用后独立使用 Data，应在提交前保留另一份引用。
异步关闭可以先于成功创建调用的返回完成，不能假设成功返回后 Data 一定尚存。

Accept 可在不同目标 Worker 并发执行，调用时借用 Data；Error、Close 和 Drop 在
Listener Worker 执行。接受的 Stream 不继承 Data，也没有初始裸 Stream 回调；
需要推送事件时，在对应 Stream Worker 显式绑定拥有型事件。
拉取模式可令策略中的 Accept 为空，继续使用现有同步／Future 接受接口。

关闭等待实际 Accept／跨 Worker 分发完成，再发布 Close。之后使用嵌入式、无分配的
生命周期命令释放 Data，确保当前回调任务尾部已退出；不新增线程或协程栈。
Drop 不在拥有域锁内执行，可安全检查拥有域。关闭状态不代表 Drop／代码尾部已退休：
期间保留原来的 runtime 引用及 Engine 拥有边，`EventDropPending` 令拥有检查返回 BUSY。
`xrtNetListenerData` 返回原子快照，拥有绑定退役后为空；它本身不保活目标。

`xrtNetListenerOwnershipAdapterV2` 先按独立提供的描述符精确身份准入，再读取上下文
合同；伪造副本／未知地址／缺失集合不被准入，失败不修改输出或线程当前诊断。
Trace 仅增加一条实际的 Data 拥有边，既不把裸 Child 地址伪装成独立 RC 节点，
也不把内嵌生命周期命令算作新引用。V1 继续拒绝拥有型回调，旧裸回调仍不具备图准入能力。
Prepare 使用原有实际关闭与 Worker 排空机制，只有 Data／runtime／Engine 边实际退役后
才会 READY；Clear 不靠清零引用计数或丢失回调来制造成功。

`test_net_listener_events_ownership.c` 与对应单头入口验证策略准入、完整拥有图、
跨 Worker 的重入关闭与回调尾部、阻塞 Drop 的 BUSY、checked Prepare、全部同步创建
分配失败前缀及精确内存平衡，加入 `net_tcp_ownership_tests` 和
`tools/check_net_tcp_ownership.py` 的常规允许列表。此项是聚合 Server 的原生前置，
不是 Server 自身、语言公开面或五组库 shipping 验收完成。

2026-10-21 隔离 HEAD＋本批源码的最终矩阵：核心模块化／单头套件 Windows
306、Linux 381 阶段，旧聚合 Server 普通接受／Future 取消关闭兼容检查另为
16／20 阶段，共 723。GCC／TCC O0／O2，Linux 增加 ASan／UBSan；输入、
日志和产品哈希均不变。新夹具每次涵盖 6 个生命周期场景、31 次策略拒绝、
5 个构造分配预算（4 个失败前缀）。初轮夹具的 TCC 聚合初始化问题已改为等价
逐项赋值并用完整矩阵复测，没有为通过测试放宽产品合同。

证据位于 `F:/xlang-validation/five-groups-20261021/p3-listener-events` 的
`native-win-r2`／`native-linux-r2` 和 `server-compat-win-r1`／`server-compat-linux-r1`。
生成单头来自干净已提交父版本加此批精确源码，不夹带其他文件／内存库改动。

### 聚合 Server 的完整原生拥有图

`xrtNetServerOwnership` 与 `xrtNetServerRuntimeOwnershipV1Get` 声明 Server 的真实
Engine 拥有边和实际 runtime 槽。创建者、runtime、Listener 绑定、接受队列和
Future 激活各自按物理引用计数；不把 RuntimeHeld、队列链接或内部数组地址凭空当成根。

每个内嵌 Child 现在有实际绑定引用计数，并持有一份 Server 引用，保证内嵌存储寿命。
Listener 消费一份绑定引用，Drop 将同一份引用转移给同 Worker 的无分配尾部命令，
不是新增一份隐藏引用。Server 保留关闭后的 Listener 引用，直到它的回调和 Drop
确实退出；待全部尾部命令与公开借用退出，才释放这些 Listener 槽和自己的 runtime／
Engine 边。末次借用与退休选举在同一锁内，不存在计数归零后再访问已释放接收者的间隙。
绑定 Clear 仍保留其真实父引用到最后一次 Drop，不能提前释放还被计划 pin 持有的内嵌壳体。

`xrtNetServerListenerEventPolicyV1Get` 为上述 Listener 上下文提供精确策略身份，
`xrtNetServerOwnershipAdapterV1` 准入 Server／绑定节点；各自 Quiescence getter
提供对应静止合同。旧 Server Data／原始 Server 或 Stream 回调继续可用，但拥有图
保守拒绝它们，不以空叶对象冒充安全接入。拉取模式的已接受 Stream 不继承任何绑定地址。

Server Prepare 只投递嵌入式关闭命令，不等待，也不在宿主 Prepare 内执行 Future
完成回调。完整联合计划仍需准备 Listener、绑定、Future、Engine 等所有真实节点，
原生线程退出和宿主代码租约的卸载证明仍属于宿主责任。

Accept Future 现在具有真实的生产者／取消 Watch／拥有型 Stream 结果策略。
`xrtNetServerFutureProducerPolicyV1Get`、`xrtNetServerFutureCancelPolicyV1Get`、
`xrtNetServerFutureStreamPayloadPolicyV1Get`、`xrtNetServerFutureOwnershipQuiescenceV1Get`
及 `xrtNetServerFutureOwnershipAdapterV1` 提供精确准入与退休合同。构造／队列、
生产者、Watch 的引用独立计数，链表 Next 不是额外拥有边；取消只移除当前等待。
结果交付被拒绝时先 Abort 再 Destroy 无人接管的 Stream；成功交付仍遵循原有 C
合同，由调用者显式 Ref 取得独立引用并负责关闭。惰性激活壳保留的 Server 引用会
被 Trace 如实枚举，直到 Clear／最终 Drop；不阻止已关闭 Server 的 Engine 退休。

同步接受在整个调用期保留活动借用，并在读取 Engine 前检查关闭状态。Server 壳体
可以合法长于其 Engine，关闭后同步／异步调用不得解引用已经释放的 Engine。

最终验收 `F:/xlang-validation/five-groups-20261021/p3-server-owned`：
`native-win-r3`／`native-linux-r3` 为 231／288 阶段；
`listener-regression-win-r1`／`listener-regression-linux-r1` 复用已逐项验证哈希的
相同运行库对象，另为 24／30 阶段，共 573。GCC／TCC O0／O2、模块化／单头、
select／原生后端，Linux 增加 ASan／UBSan；输入、日志和产物哈希均不变。
新夹具每次包括 6 次整图退休、13 次登记的策略／裸回调拒绝、15 个分配预算
（13 个失败前缀）、两个端点 NUL 通信、9 次绑定／激活最终 Drop 竞态和 8 次
取消／关闭竞态，以及 Engine 已释放后的 Server 壳体调用。删除真实绑定父边的
离线代码变异会被未申报外部根检查拒绝；core-only 独立链接不包含 Server Future。
旧原始接受／重入关闭／Future／同步／OOM 与拥有型 Listener／Stream 一并回归。
全部通信仅为自建字面量回环正常功能，不执行 DNS、外网或网络安全性质专项。
此批不代表 xlang 固定快照、聚合 Server 公开面或 shipping 验收完成。

### 不可变字节片段的原子批量发送（2026-10-22）

`xrtNetStreamSendBytesVec(stream, refs, count)` 接收 `xnetbytesref` 数组：
每项明确指定 `Bytes`、`Offset`、`Size`。范围必须落在实际 NetBytes 内；空项不
持有额外引用。成功为全部非空项各保留一个原生引用，重复 NetBytes 也有真实的
重复边；失败、AGAIN、CLOSED 不消费调用者引用。没有用户释放回调或载荷复制。

沿用现有批次分配、完整写预算预留与 Worker 提交；只新增原生来源标记和 typed
构造，不将泛化 `SendRefs` 的任意回调推测为可信拥有边。FIFO 首个存活视图枚举
所有存活载荷及批次唯一 Stream 引用，部分消费后不访问已退休前缀。

`tools/check_net_tcp_bytes_vec.py` 使用自建字面量回环和分配失败检查，证据根为
`F:/xlang-validation/five-groups-20261022/p3-tcp-ref-batch`。Windows 8／Linux 10
个单头阶段覆盖 GCC／TCC O0／O2、select／IOCP 或 epoll，Linux ASan／UBSan。
每次进程包括 24 个预算、两个旧 opaque 回调回归、重复原生边和精确片段观察，
接收 NUL 内容并验证完整内存账本。Worker 高水位观察发生在实际写入驱动之前，
不把 active 回调冒称为整图 Freeze；去掉来源标记必须被原生块拥有遍历拒绝。

生成头来自隔离的已提交源码加本批五份核心文件，不包含工作区其他文件／内存
变更。本批当时不改变旧 SendBuffer 的 Worker-only 合同，也不宣称链式跨线程接管、
完整 xlang 集成、shipping 或网络安全专项已完成。

### 任意线程链式接管（2026-10-22）

`xrtNetStreamSendBuffer` 现可从任意线程提交完整链：先准备全部视图与预算，再
移动真实块并提交批次。OK 源为空；受理前 ERROR／AGAIN／CLOSED 完整保留源。
队列不持有源容器地址；受理后 Abort／I/O 失败释放已受理批次，不承诺对端交付。
源须由调用者串行化，拒绝活动写预留；异线程禁止外来线程亲和池，允许堆块及
所属 Worker 的池块。元数据准备失败不改变实际载荷地址或源链结构。

各排队发送命令在高水位等回调重入期间保留 Active，确保 Abort 和最后引用释放
不会使命令尾部访问已退役 Stream。补充 `xrtNetBufOwnershipTrace` 及
`xrtNetProxyHandshakeOwnershipTrace`，仅枚举真实原生 owning 边，不调用释放回调，
不认证未知借用。调用方须稳定链／握手及所访问拥有者，通常在 ownership Freeze 内。

`tools/check_net_tcp_chain_take.py` 在独立原生头和固定微秒快照验证 Worker／异线程、
48 个预算、65 引用块、背压、预留、池亲和、Abort、回调重入和内存平衡；Windows
GCC／TCC select／IOCP，Linux GCC／TCC select／epoll 及 ASan／UBSan。
仅离线生命周期和自建回环正常通信，没有外网、DNS 或网络安全专项。

## 原生对象与异步协议补充参考

拥有图引用描述实际强引用边，不是额外 pin；适配器要求精确策略身份和静止条件，只在准入成功后使用准备、接管和释放协议。普通查询与非等待拒绝须按各接口契约区分。

### `xnetproxydialownershipv1`

```c
typedef struct xnetproxydialownershipv1 {
	size_t size;
	xnetproxydialproc Done;
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xnetproxydialownershipv1;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `size` | `size_t` | 策略结构的字节大小，用于版本与形状校验。 |
| `Done` | `xnetproxydialproc` | 完成阶段回调。 |
| `Ops` | `const xrtownershipops*` | 精确匹配的所有权计数和追踪操作表。 |


字段、回调签名和版本布局如上；须与本模块相应操作的所有权、静止及精确策略身份契约配对。


### `xnetdialownershipv1`

```c
typedef struct xnetdialownershipv1 {
	size_t size;
	xnetdialproc Done;
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xnetdialownershipv1;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `size` | `size_t` | 策略结构的字节大小，用于版本与形状校验。 |
| `Done` | `xnetdialproc` | 完成阶段回调。 |
| `Ops` | `const xrtownershipops*` | 精确匹配的所有权计数和追踪操作表。 |


字段、回调签名和版本布局如上；须与本模块相应操作的所有权、静止及精确策略身份契约配对。


### `xnetlistenereventsownershipv1`

```c
typedef struct xnetlistenereventsownershipv1 {
	size_t size;
	xnetlistenerevents Events;
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xnetlistenereventsownershipv1;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `size` | `size_t` | 策略结构的字节大小，用于版本与形状校验。 |
| `Events` | `xnetlistenerevents` | 声明中的公开字段；取值和生命周期须符合本类型及使用接口的契约。 |
| `Ops` | `const xrtownershipops*` | 精确匹配的所有权计数和追踪操作表。 |


字段、回调签名和版本布局如上；须与本模块相应操作的所有权、静止及精确策略身份契约配对。


### `xnetstreameventsownershipv1`

```c
typedef struct xnetstreameventsownershipv1 {
	size_t size;
	xnetstreamevents Events;
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xnetstreameventsownershipv1;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `size` | `size_t` | 策略结构的字节大小，用于版本与形状校验。 |
| `Events` | `xnetstreamevents` | 声明中的公开字段；取值和生命周期须符合本类型及使用接口的契约。 |
| `Ops` | `const xrtownershipops*` | 精确匹配的所有权计数和追踪操作表。 |


字段、回调签名和版本布局如上；须与本模块相应操作的所有权、静止及精确策略身份契约配对。
### `xrtNetDialAddresses`

Race already-resolved immutable endpoints using the SAME Dial algorithm.
No Resolver is created or consulted. Eligible endpoints must have nonzero
ports; each port is preserved. Config.Family filters candidates explicitly.
The list is borrowed for this call and retained for asynchronous setup.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xnetdial* xrtNetDialAddresses(xnetengine* pEngine, const xnetaddrlist* pAddresses, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData, xnetdialproc pDone, ptr pDoneData);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pAddresses` | 显式候选网络地址数组。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pStreamEvents` | 连接事件回调表，须与相应上下文及拥有策略配对。 |
| `pStreamData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pDone` | 同步访问或完成回调；回调线程、借用参数和生命周期遵守当前接口契约。 |
| `pDoneData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialAddressesAsync`

使用显式地址集创建拨号 Future；结果和取消策略由 TCP Future 契约定义。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xfuture* xrtNetDialAddressesAsync(xnetengine* pEngine, const xnetaddrlist* pAddresses, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pAddresses` | 显式候选网络地址数组。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pStreamEvents` | 连接事件回调表，须与相应上下文及拥有策略配对。 |
| `pStreamData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialAddressesOwnedV1`

使用显式地址集发起带拥有型完成上下文的 TCP 拨号。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xnetdial* xrtNetDialAddressesOwnedV1(xnetengine* pEngine, const xnetaddrlist* pAddresses, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData, ptr pDoneData, const xnetdialownershipv1* pPolicy);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pAddresses` | 显式候选网络地址数组。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pStreamEvents` | 连接事件回调表，须与相应上下文及拥有策略配对。 |
| `pStreamData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pDoneData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pPolicy` | 对象或操作要求的精确策略描述符，生命周期覆盖受理的上下文。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialFutureCancelPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xcancelwatchownershipv1* xrtNetDialFutureCancelPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialFutureDialPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xnetdialownershipv1* xrtNetDialFutureDialPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialFutureOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetDialFutureOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialFutureOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipquiescencev1* xrtNetDialFutureOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialFuturePayloadPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xfuturepayloadownershipv1* xrtNetDialFuturePayloadPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialFutureProducerPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xfutureproducerownershipv1* xrtNetDialFutureProducerPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialOwnedV1`

发起带拥有型完成上下文的 TCP 拨号，仅在成功受理后转移上下文责任。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xnetdial* xrtNetDialOwnedV1(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xnetdialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData, ptr pDoneData, const xnetdialownershipv1* pPolicy);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pResolver` | 名称解析器句柄；调用方须按相应生命周期契约保持目标有效。 |
| `sHost` | 目标主机名。 |
| `iPort` | 目标网络端口。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pStreamEvents` | 连接事件回调表，须与相应上下文及拥有策略配对。 |
| `pStreamData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pDoneData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pPolicy` | 对象或操作要求的精确策略描述符，生命周期覆盖受理的上下文。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialOwnership`

Query adapters under whole-domain Freeze, with exact Done-policy identities
admitted before following context metadata. Real runtime, Resolver callback,
candidate and timer references are preserved; unknown callback/data remains
opaque. Prepare is nonwaiting and requires the actual terminal callback and
runtime-slot removal before READY. Diagnostics remain immutable owned DAGs.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xrtownershipref xrtNetDialOwnership(const xnetdial* pDial);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pDial` | 拨号操作句柄；调用方须按相应生命周期契约保持目标有效。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetDialOwnershipAdapterV1(xrtownershipref Reference, const xnetdialownershipv1* const* pPolicies, size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `pPolicies` | 允许的精确策略指针数组，与相应的策略数量参数配对。 |
| `iPolicyCount` | 对应输入数组的有效元素数量。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipquiescencev1* xrtNetDialOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialResolverPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xnetresolveownershipv1* xrtNetDialResolverPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetDialRuntimeOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xnetruntimeownershipv1* xrtNetDialRuntimeOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenConfigValid`

验证完整 Listener 及嵌套 Stream 配置，不绑定地址；不修改输入。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
bool xrtNetListenConfigValid(const xnetlistenconfig* pConfig);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |


#### 返回值

| 返回 | 含义 | 状态约定 |
|---|---|---|
| `true` | 操作完成或满足声明中的条件 | 按上述契约使用结果 |
| `false` | 操作拒绝或条件不成立 | 区分正常拒绝与实际错误 |


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenOwnedV1`

Same synchronous bind/listen and asynchronous accept as xrtNetListen.
Transfers one explicit event-context owner only on success; no inherited
Stream callbacks/data. Configure accepted Stream events explicitly on its
Worker (e.g. xrtNetStreamSetEventsOwnedV1) when push mode is needed.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xnetlistener* xrtNetListenOwnedV1(xnetengine* pEngine, const xnetlistenconfig* pConfig, ptr pData, const xnetlistenereventsownershipv1* pPolicy);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pPolicy` | 对象或操作要求的精确策略描述符，生命周期覆盖受理的上下文。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenerOwnership`

返回对象的借用物理所有权引用，不增加普通拥有引用；图检查须保持相应静止与冻结边界。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xrtownershipref xrtNetListenerOwnership(const xnetlistener* pListener);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pListener` | 监听器句柄；调用方须按相应生命周期契约保持目标有效。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenerOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetListenerOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenerOwnershipAdapterV2`

V1 refuses owned Listener events. V2 admits exact independently supplied
policy identities BEFORE inspecting context metadata. Callback-drop tails
remain BUSY until their real Worker task returns. Failure leaves output
and ambient diagnostic untouched, including unknown/forged policies.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetListenerOwnershipAdapterV2(xrtownershipref Reference, const xnetlistenereventsownershipv1* const* pPolicies, size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `pPolicies` | 允许的精确策略指针数组，与相应的策略数量参数配对。 |
| `iPolicyCount` | 对应输入数组的有效元素数量。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenerOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipquiescencev1* xrtNetListenerOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetListenerRuntimeOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xnetruntimeownershipv1* xrtNetListenerRuntimeOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialAsync`

Success holds one Stream; Value borrows it, Ref acquires another owner.
Cancel requests the real native operation and waits for its actual terminal
callback. No per-wait thread, coroutine stack or synthetic terminal state.

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
xfuture* xrtNetProxyDialAsync(xnetengine* pEngine, xnetresolver* pResolver, const xnetproxy* pProxy, cstr sTargetHost, uint16 iTargetPort, const xnetproxydialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pResolver` | 名称解析器句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pProxy` | 代理配置句柄；调用方须按相应生命周期契约保持目标有效。 |
| `sTargetHost` | 目标主机名。 |
| `iTargetPort` | 目标网络端口。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pStreamEvents` | 连接事件回调表，须与相应上下文及拥有策略配对。 |
| `pStreamData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialFutureCancelPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xcancelwatchownershipv1* xrtNetProxyDialFutureCancelPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialFutureDialPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xnetproxydialownershipv1* xrtNetProxyDialFutureDialPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialFutureOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xrtownershipadapterv1* xrtNetProxyDialFutureOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialFutureOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xrtownershipquiescencev1* xrtNetProxyDialFutureOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialFuturePayloadPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xfuturepayloadownershipv1* xrtNetProxyDialFuturePayloadPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialFutureProducerPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xfutureproducerownershipv1* xrtNetProxyDialFutureProducerPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialOwnedV1`

发起带拥有型完成上下文的代理拨号；成功受理后按所传策略追踪和释放上下文。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
xnetproxydial* xrtNetProxyDialOwnedV1(xnetengine* pEngine, xnetresolver* pResolver, const xnetproxy* pProxy, cstr sTargetHost, uint16 iTargetPort, const xnetproxydialconfig* pConfig, const xnetstreamevents* pStreamEvents, ptr pStreamData, ptr pDoneData, const xnetproxydialownershipv1* pPolicy);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pEngine` | 网络引擎句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pResolver` | 名称解析器句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pProxy` | 代理配置句柄；调用方须按相应生命周期契约保持目标有效。 |
| `sTargetHost` | 目标主机名。 |
| `iTargetPort` | 目标网络端口。 |
| `pConfig` | 当前操作的配置；合法字段和默认行为见相应配置类型。 |
| `pStreamEvents` | 连接事件回调表，须与相应上下文及拥有策略配对。 |
| `pStreamData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pDoneData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pPolicy` | 对象或操作要求的精确策略描述符，生命周期覆盖受理的上下文。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialOwnership`

返回对象的借用物理所有权引用，不增加普通拥有引用；图检查须保持相应静止与冻结边界。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
xrtownershipref xrtNetProxyDialOwnership(const xnetproxydial* pDial);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pDial` | 拨号操作句柄；调用方须按相应生命周期契约保持目标有效。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xrtownershipadapterv1* xrtNetProxyDialOwnershipAdapterV1(xrtownershipref Reference, const xnetproxydialownershipv1* const* pPolicies, size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `pPolicies` | 允许的精确策略指针数组，与相应的策略数量参数配对。 |
| `iPolicyCount` | 对应输入数组的有效元素数量。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xrtownershipquiescencev1* xrtNetProxyDialOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialRuntimeOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xnetruntimeownershipv1* xrtNetProxyDialRuntimeOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialStreamOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xnetstreameventsownershipv1* xrtNetProxyDialStreamOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyDialTransportOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xnetdialownershipv1* xrtNetProxyDialTransportOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyHandshakeOwnershipTrace`

Read-only owning edges of a standalone handshake: its retained immutable
proxy and output-chain owners. Caller excludes all handshake mutations
and stabilizes visited owners, normally under ownership Freeze.

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
bool xrtNetProxyHandshakeOwnershipTrace(const xnetproxyhandshake* pHandshake, xrtownershipvisitor pVisit, ptr pContext);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pHandshake` | 代理握手状态句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pVisit` | 同步访问或完成回调；回调线程、借用参数和生命周期遵守当前接口契约。 |
| `pContext` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |


#### 返回值

| 返回 | 含义 | 状态约定 |
|---|---|---|
| `true` | 操作完成或满足声明中的条件 | 按上述契约使用结果 |
| `false` | 操作拒绝或条件不成立 | 区分正常拒绝与实际错误 |


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyOwnership`

Physical immutable configuration owner. Count is the real native RC;
copied host/credential storage has no outgoing strong references. Inspect
and resolve the adapter only at a whole-graph ownership freeze.

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
xrtownershipref xrtNetProxyOwnership(const xnetproxy* pProxy);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pProxy` | 代理配置句柄；调用方须按相应生命周期契约保持目标有效。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetProxyOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [proxy.h](../../include/xrt/proxy.h)。

```c
const xrtownershipadapterv1* xrtNetProxyOwnershipAdapterV1(xrtownershipref Reference);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerBindingOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xrtownershipquiescencev1* xrtNetServerBindingOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerFutureCancelPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xcancelwatchownershipv1* xrtNetServerFutureCancelPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerFutureOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xrtownershipadapterv1* xrtNetServerFutureOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerFutureOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xrtownershipquiescencev1* xrtNetServerFutureOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerFutureProducerPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xfutureproducerownershipv1* xrtNetServerFutureProducerPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerFutureStreamPayloadPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xfuturepayloadownershipv1* xrtNetServerFutureStreamPayloadPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerListenerEventPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xnetlistenereventsownershipv1* xrtNetServerListenerEventPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerOwnership`

Frozen, exact native graph. Raw Server/Stream callbacks and Data remain
opaque and are refused. The owned Listener binding policy is resident and
admits real counted binding nodes, not borrowed interior aliases.

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
xrtownershipref xrtNetServerOwnership(const xnetserver* pServer);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pServer` | 聚合 TCP 服务端句柄；调用方须按相应生命周期契约保持目标有效。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerOwnershipAdapterV1`

Accepts exact Server or binding nodes; failure preserves output/error.
Prepare closes real sources without waiting; READY additionally requires
all owned Listener callbacks/Drop tails and Engine/runtime edges retired.

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xrtownershipadapterv1* xrtNetServerOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xrtownershipquiescencev1* xrtNetServerOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetServerRuntimeOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp_server.h](../../include/xrt/tcp_server.h)。

```c
const xnetruntimeownershipv1* xrtNetServerRuntimeOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamOwnership`

Whole-domain frozen views. Engine V3 must admit these exact runtime-family
descriptors. Real caller/runtime/timer/accept-queue references are counted;
opaque callbacks, live external buffers and unmodelled Future contexts are
refused, never flattened into a leaf. Preparation aborts/closes without
waiting and becomes READY only after the runtime owner has physically left.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xrtownershipref xrtNetStreamOwnership(const xnetstream* pStream);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pStream` | TCP 连接句柄；调用方须按相应生命周期契约保持目标有效。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetStreamOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamOwnershipAdapterV2`

V1 refuses owned event data. V2 additionally admits exact, separately
supplied event-policy identities before inspecting any context metadata.
Retired callback tails remain BUSY until real Worker drain, never a fake
zero count or a hidden context reference. Failure leaves output untouched.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetStreamOwnershipAdapterV2(xrtownershipref Reference, const xnetstreameventsownershipv1* const* pPolicies, size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `pPolicies` | 允许的精确策略指针数组，与相应的策略数量参数配对。 |
| `iPolicyCount` | 对应输入数组的有效元素数量。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipquiescencev1* xrtNetStreamOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamRuntimeOwnershipV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xnetruntimeownershipv1* xrtNetStreamRuntimeOwnershipV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamSendBytes`

有界接管非空数据；NULL,0 是无操作，非空指针配零长度是参数错误。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xnetresult xrtNetStreamSendBytes(xnetstream* pStream, xnetbytes* pBytes, size_t iOffset, size_t iSize);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pStream` | TCP 连接句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pBytes` | 不可变网络字节结果；切片与持有引用须遵守其有效范围和生命周期。 |
| `iOffset` | 在当前字节结果或存储中的起始偏移。 |
| `iSize` | 当前字节视图、切片或结构的长度；对应存储须覆盖声明的范围。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamSendBytesVec`

All-or-none bounded acceptance, no payload copy and no user releaser.
Failure/AGAIN/CLOSED leaves every caller reference unchanged. Thread-safe
like SendBytes; the owning graph includes each actual retained NetBytes.
Uses shared xnetbytesref from net.h; only nonempty slices retain owners.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
xnetresult xrtNetStreamSendBytesVec(xnetstream* pStream, const xnetbytesref* pRefs, size_t iCount);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pStream` | TCP 连接句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pRefs` | 工作条目、订阅分支或拥有型切片数组；与有效数量及接管规则配对。 |
| `iCount` | 对应输入数组的有效元素数量。 |


#### 返回值

返回声明中的状态、身份或计数；枚举取值见本模块类型及稳定契约。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetStreamSetEventsOwnedV1`

Worker-only, open Stream. Allocates one uniquely owned binding, not one
allocation per event/packet. SetData cannot overwrite an owned binding.
Legacy SetEvents may replace it, but does not retain its borrowed new Data.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
bool xrtNetStreamSetEventsOwnedV1(xnetstream* pStream, ptr pData, const xnetstreameventsownershipv1* pPolicy);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `pStream` | TCP 连接句柄；调用方须按相应生命周期契约保持目标有效。 |
| `pData` | 交给操作或回调的用户上下文；接管点和失败时释放责任见当前接口的所有权契约。 |
| `pPolicy` | 对象或操作要求的精确策略描述符，生命周期覆盖受理的上下文。 |


#### 返回值

| 返回 | 含义 | 状态约定 |
|---|---|---|
| `true` | 操作完成或满足声明中的条件 | 按上述契约使用结果 |
| `false` | 操作拒绝或条件不成立 | 区分正常拒绝与实际错误 |


#### 错误

依照上述参数、状态和回调契约处理拒绝；需要错误详情时读取 `xrtGetError()`。条件查询与非等待接口的正常拒绝不得当作部分成功。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetTcpFutureBytesPayloadPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xfuturepayloadownershipv1* xrtNetTcpFutureBytesPayloadPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetTcpFutureCancelPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xcancelwatchownershipv1* xrtNetTcpFutureCancelPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetTcpFutureOwnershipAdapterV1`

验证物理所有权描述、精确策略身份和静止条件，成功时返回接管适配器及相应准备协议。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipadapterv1* xrtNetTcpFutureOwnershipAdapterV1(xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
```

#### 参数

| 参数 | 说明 |
|---|---|
| `Reference` | 借用的物理对象与精确 Count/Trace 操作表身份；不增加普通拥有引用。 |
| `ppPreparation` | 适配成功后返回的准备协议描述符；只有准入成功后才能使用。 |


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

空指针表示对象、精确策略或静止条件不满足；准入拒绝不等同于系统调用失败，不得对拒绝的对象继续执行接管协议。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetTcpFutureOwnershipQuiescenceV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xrtownershipquiescencev1* xrtNetTcpFutureOwnershipQuiescenceV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetTcpFutureProducerPolicyV1Get`

Exact resident waiter/producer/cancel and terminal payload capabilities.
Frozen queries; the collector must independently admit every child. The
waiter preparation waits for its actual source's normal close/notification,
never turns Drop into cancellation or a synthetic Future completion.

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xfutureproducerownershipv1* xrtNetTcpFutureProducerPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。


### `xrtNetTcpFutureStreamPayloadPolicyV1Get`

返回该原生对象或异步结果的常驻只读策略描述符，供精确身份匹配使用。

公开声明与相关策略见 [tcp.h](../../include/xrt/tcp.h)。

```c
const xfuturepayloadownershipv1* xrtNetTcpFutureStreamPayloadPolicyV1Get(void);
```

#### 参数

无参数。


#### 返回值

返回声明类型的指针；借用/拥有规则及空指针含义遵循上述契约，取得有效结果后才能继续访问。


#### 错误

返回库内常驻只读策略，调用本身不分配动态内存。


#### 范例

[同模块资源的基础组合示例](../../examples/network/tcp/main.c)。本接口的策略身份、回调及生命周期限制见上述契约。

### `xrtNetStreamConnectOwnedV1`

创建数字地址连接，并在发布启动任务**之前**安装拥有型事件上下文。
不执行名称解析，不改变 `xnetstreamconfig` 的配置和时间单位。

```c
xnetstream* xrtNetStreamConnectOwnedV1(xnetengine* pEngine,
    const xnetaddr* pRemote, uint64 iAffinity, const xnetstreamconfig* pConfig,
    ptr pData, const xnetstreameventsownershipv1* pPolicy);
```

#### 参数

Engine、Remote、Affinity、Config 与 `xrtNetStreamConnect` 相同。Data 必须为
非空的一个真实拥有引用；Policy 必须常驻、size 精确匹配，并提供有效的
Drop 和拥有图 Count／Trace。事件表来自 Policy，而非可变的调用方临时表。

#### 返回值

成功返回一个 Stream 调用方引用，同时消费恰好一个 Data 引用；失败返回
NULL，完全不消费 Data。Open／Close 可能与提交者并发，甚至先于本函数
返回，调用方不能假定回调会等返回值赋值完成。事件只借用已登记的 Data。

#### 错误

无效描述符、无效连接配置、分配、套接字及启动任务发布失败使用既有
checked 错误。失败回滚不调用 Data 的 Drop；成功后的替换／关闭复用
`xrtNetStreamSetEventsOwnedV1` 的 Worker 延迟退役，待回调／分派栈退出后
才 Drop，不在拥有图 Freeze 内调用用户清理。

#### 范例

见 [拥有型 TCP／TLS 交接用例](../../tests/tls/test_tls_transport_handoff.c) 与
[交接合同](../design/tls-transport-handoff.md)。此入口只建立真实 TCP 拥有边，
不单独证明 TLS Session、Listener、Dial 或异步节点已经可以收集退役。

### 非阻塞 Dial Future 发布

TCP／Proxy／TLS Dial Future 共用真实拥有发布器，不再依赖
`xrtFutureBridgeWait`。Done 可以先于 Submit 返回：完成方保存实际结果
与错误后立即返回，发布方或后续完成方以互斥锁选择唯一处理者。发布前
取消保留并在 Dial 发布后转发，不能因句柄槽尚未赋值而丢失。

producer、Owned CancelWatch、已接受 Done 和提交者分别持有真实引用。
处理者在锁及 ownership Mutation 外取消订阅、释放原生句柄并完成
Promise；各族使用自己的拥有 payload。准备等待真实源完成；Clear 无
用户回调，Finish 可重复检查并保留首次清理错误。已有 TCP／Proxy
公开调用方式不变，TLS 的借用与 owned 入口仍严格区分。

合同与普通发布／回环验收见
[Dial Future 发布设计](../design/tls-dial-future-ownership.md)。
