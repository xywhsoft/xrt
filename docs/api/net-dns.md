# DNS

`XRT_FEATURE_NET_DNS` 只依赖网络地址基础层。它提供数字主机快路、系统名称解析、反向解析和不可变地址列表，不创建线程、Engine 或隐藏缓存。

## 类型与常量

### `xnetfamily`

地址族使用稳定值，不依赖平台 AF_INET 和 AF_INET6 常量。

```c
typedef enum xnetfamily {
	XNET_FAMILY_UNSPEC = 0,
	XNET_FAMILY_IPV4 = 4,
	XNET_FAMILY_IPV6 = 6
} xnetfamily;
```

| 值 | 语义 |
|---|---|
| `XNET_FAMILY_UNSPEC` | 不指定（双栈） |
| `XNET_FAMILY_IPV4` | IPv4 |
| `XNET_FAMILY_IPV6` | IPv6 |

### `xnetaddr`

端口使用主机字节序，地址字节始终使用网络字节序。

```c
typedef struct xnetaddr {
	uint16 Family;
	uint16 Port;
	uint32 Scope;
	uint8 Address[16];
} xnetaddr;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Family` | `uint16` | 地址族 |
| `Port` | `uint16` | 端口 |
| `Scope` | `uint32` | 作用域标识 |

### `xnetresult`

网络操作把正常控制结果与结构化错误分开表达。

```c
typedef enum xnetresult {
	XNET_RESULT_ERROR = -1,
	XNET_RESULT_OK = 0,
	XNET_RESULT_AGAIN,
	XNET_RESULT_CLOSED,
	XNET_RESULT_TRUNCATED,
	XNET_RESULT_TIMEOUT,
	XNET_RESULT_CANCELLED
} xnetresult;
```

| 值 | 语义 |
|---|---|
| `XNET_RESULT_ERROR` | 失败 |
| `XNET_RESULT_OK` | 成功 |
| `XNET_RESULT_AGAIN` | 暂不可推进 |
| `XNET_RESULT_CLOSED` | 已关闭 |
| `XNET_RESULT_TRUNCATED` | 已截断 |
| `XNET_RESULT_TIMEOUT` | 超时 |
| `XNET_RESULT_CANCELLED` | 已取消 |

### `xneterror`

网络基础层稳定错误代码。

```c
typedef enum xneterror {
	XNET_ERROR_NONE = 0,
	XNET_ERROR_FORMAT,
	XNET_ERROR_FAMILY,
	XNET_ERROR_PORT,
	XNET_ERROR_SCOPE,
	XNET_ERROR_BUFFER,
	XNET_ERROR_NATIVE,
	XNET_ERROR_SYSTEM,
	XNET_ERROR_INTERFACE_QUERY,
	XNET_ERROR_INTERFACE_NAME,
	XNET_ERROR_INTERFACE_INDEX,
	XNET_ERROR_INTERFACE_ADDRESS,
	XNET_ERROR_INTERFACE_HARDWARE,
	XNET_ERROR_HOST_NAME,
	XNET_ERROR_DNS_RESOLVE,
	XNET_ERROR_DNS_REVERSE,
	XNET_ERROR_DNS_RESULT,
	XNET_ERROR_RESOLVER_CREATE,
	XNET_ERROR_RESOLVER_SUBMIT,
	XNET_ERROR_RESOLVER_CLOSED,
	XNET_ERROR_RESOLVER_QUERY,
	XNET_ERROR_BUFFER_STATE,
	XNET_ERROR_POOL_BUSY,
	XNET_ERROR_FRAME_CONFIG,
	XNET_ERROR_FRAME_STATE,
	XNET_ERROR_FRAME_LIMIT,
	XNET_ERROR_FRAME_LENGTH,
	XNET_ERROR_SOCKET_OPEN,
	XNET_ERROR_SOCKET_CLOSE,
	XNET_ERROR_SOCKET_OPTION,
	XNET_ERROR_SOCKET_BIND,
	XNET_ERROR_SOCKET_LISTEN,
	XNET_ERROR_SOCKET_ACCEPT,
	XNET_ERROR_SOCKET_CONNECT,
	XNET_ERROR_SOCKET_SHUTDOWN,
	XNET_ERROR_SOCKET_READ,
	XNET_ERROR_SOCKET_WRITE,
	XNET_ERROR_SOCKET_DGRAM_ERROR,
	XNET_ERROR_PORT_CREATE,
	XNET_ERROR_PORT_CLOSE,
	XNET_ERROR_PORT_WATCH,
	XNET_ERROR_PORT_WAIT,
	XNET_ERROR_PORT_POST,
	XNET_ERROR_PORT_SUBMIT,
	XNET_ERROR_PORT_CANCEL,
	XNET_ERROR_ENGINE_CREATE,
	XNET_ERROR_ENGINE_START,
	XNET_ERROR_ENGINE_STOP,
	XNET_ERROR_ENGINE_POST,
	XNET_ERROR_ENGINE_TIMER,
	XNET_ERROR_STREAM_CONFIG,
	XNET_ERROR_STREAM_CREATE,
	XNET_ERROR_STREAM_CONNECT,
	XNET_ERROR_STREAM_READ,
	XNET_ERROR_STREAM_WRITE,
	XNET_ERROR_STREAM_CLOSE,
	XNET_ERROR_DIAL_CONFIG,
	XNET_ERROR_DIAL_CREATE,
	XNET_ERROR_DIAL_RESOLVE,
	XNET_ERROR_DIAL_CONNECT,
	XNET_ERROR_LISTENER_CREATE,
	XNET_ERROR_LISTENER_ACCEPT,
	XNET_ERROR_LISTENER_CLOSE,
	XNET_ERROR_SERVER_CONFIG,
	XNET_ERROR_SERVER_START,
	XNET_ERROR_SERVER_ACCEPT,
	XNET_ERROR_UDP_CONFIG,
	XNET_ERROR_UDP_CREATE,
	XNET_ERROR_UDP_RECEIVE,
	XNET_ERROR_UDP_RECEIVE_QUEUE,
	XNET_ERROR_UDP_SEND,
	XNET_ERROR_UDP_CLOSE,
	XNET_ERROR_PROXY_CONFIG,
	XNET_ERROR_PROXY_CREATE,
	XNET_ERROR_PROXY_PROTOCOL,
	XNET_ERROR_PROXY_AUTH,
	XNET_ERROR_PROXY_CONNECT,
	XNET_ERROR_PROXY_LIMIT,
	XNET_ERROR_PROXY_UNSUPPORTED
} xneterror;
```

| 值 | 语义 |
|---|---|
| `XNET_ERROR_NONE` | 无 |
| `XNET_ERROR_FORMAT` | 格式非法 |
| `XNET_ERROR_FAMILY` | 失败 |
| `XNET_ERROR_PORT` | 失败 |
| `XNET_ERROR_SCOPE` | 失败 |
| `XNET_ERROR_BUFFER` | 失败 |
| `XNET_ERROR_NATIVE` | 失败 |
| `XNET_ERROR_SYSTEM` | 失败 |
| `XNET_ERROR_INTERFACE_QUERY` | 失败 |
| `XNET_ERROR_INTERFACE_NAME` | INTERFACE名称 |
| `XNET_ERROR_INTERFACE_INDEX` | INTERFACE索引 |
| `XNET_ERROR_INTERFACE_ADDRESS` | 失败 |
| `XNET_ERROR_INTERFACE_HARDWARE` | 失败 |
| `XNET_ERROR_HOST_NAME` | HOST名称 |
| `XNET_ERROR_DNS_RESOLVE` | 失败 |
| `XNET_ERROR_DNS_REVERSE` | 失败 |
| `XNET_ERROR_DNS_RESULT` | 失败 |
| `XNET_ERROR_RESOLVER_CREATE` | RESOLVER创建 |
| `XNET_ERROR_RESOLVER_SUBMIT` | 失败 |
| `XNET_ERROR_RESOLVER_CLOSED` | RESOLVER已关闭 |
| `XNET_ERROR_RESOLVER_QUERY` | 失败 |
| `XNET_ERROR_BUFFER_STATE` | BUFFER状态非法 |
| `XNET_ERROR_POOL_BUSY` | 失败 |
| `XNET_ERROR_FRAME_CONFIG` | FRAME配置非法 |
| `XNET_ERROR_FRAME_STATE` | FRAME状态非法 |
| `XNET_ERROR_FRAME_LIMIT` | FRAME超限 |
| `XNET_ERROR_FRAME_LENGTH` | 失败 |
| `XNET_ERROR_SOCKET_OPEN` | 失败 |
| `XNET_ERROR_SOCKET_CLOSE` | 失败 |
| `XNET_ERROR_SOCKET_OPTION` | 失败 |
| `XNET_ERROR_SOCKET_BIND` | 失败 |
| `XNET_ERROR_SOCKET_LISTEN` | 失败 |
| `XNET_ERROR_SOCKET_ACCEPT` | 失败 |
| `XNET_ERROR_SOCKET_CONNECT` | Socket连接失败 |
| `XNET_ERROR_SOCKET_SHUTDOWN` | 失败 |
| `XNET_ERROR_SOCKET_READ` | SOCKET读方向 |
| `XNET_ERROR_SOCKET_WRITE` | SOCKET写方向 |
| `XNET_ERROR_SOCKET_DGRAM_ERROR` | SOCKETDGRAM失败 |
| `XNET_ERROR_PORT_CREATE` | PORT创建 |
| `XNET_ERROR_PORT_CLOSE` | 失败 |
| `XNET_ERROR_PORT_WATCH` | 失败 |
| `XNET_ERROR_PORT_WAIT` | 失败 |
| `XNET_ERROR_PORT_POST` | 端口投递失败 |
| `XNET_ERROR_PORT_SUBMIT` | 失败 |
| `XNET_ERROR_PORT_CANCEL` | 失败 |
| `XNET_ERROR_ENGINE_CREATE` | ENGINE创建 |
| `XNET_ERROR_ENGINE_START` | 失败 |
| `XNET_ERROR_ENGINE_STOP` | 失败 |
| `XNET_ERROR_ENGINE_POST` | Engine投递失败 |
| `XNET_ERROR_ENGINE_TIMER` | 失败 |
| `XNET_ERROR_STREAM_CONFIG` | STREAM配置非法 |
| `XNET_ERROR_STREAM_CREATE` | STREAM创建 |
| `XNET_ERROR_STREAM_CONNECT` | 流连接失败 |
| `XNET_ERROR_STREAM_READ` | STREAM读方向 |
| `XNET_ERROR_STREAM_WRITE` | STREAM写方向 |
| `XNET_ERROR_STREAM_CLOSE` | 失败 |
| `XNET_ERROR_DIAL_CONFIG` | DIAL配置非法 |
| `XNET_ERROR_DIAL_CREATE` | DIAL创建 |
| `XNET_ERROR_DIAL_RESOLVE` | 失败 |
| `XNET_ERROR_DIAL_CONNECT` | Dial连接失败 |
| `XNET_ERROR_LISTENER_CREATE` | LISTENER创建 |
| `XNET_ERROR_LISTENER_ACCEPT` | 失败 |
| `XNET_ERROR_LISTENER_CLOSE` | 失败 |
| `XNET_ERROR_SERVER_CONFIG` | 服务端角色配置非法 |
| `XNET_ERROR_SERVER_START` | 服务端角色START |
| `XNET_ERROR_SERVER_ACCEPT` | 服务端角色ACCEPT |
| `XNET_ERROR_UDP_CONFIG` | UDP配置非法 |
| `XNET_ERROR_UDP_CREATE` | UDP创建 |
| `XNET_ERROR_UDP_RECEIVE` | 接收方向 |
| `XNET_ERROR_UDP_RECEIVE_QUEUE` | 接收方向 |
| `XNET_ERROR_UDP_SEND` | 发送方向 |
| `XNET_ERROR_UDP_CLOSE` | 失败 |
| `XNET_ERROR_PROXY_CONFIG` | PROXY配置非法 |
| `XNET_ERROR_PROXY_CREATE` | PROXY创建 |
| `XNET_ERROR_PROXY_PROTOCOL` | PROXY协议非法 |
| `XNET_ERROR_PROXY_AUTH` | 失败 |
| `XNET_ERROR_PROXY_CONNECT` | Proxy连接失败 |
| `XNET_ERROR_PROXY_LIMIT` | PROXY超限 |
| `XNET_ERROR_PROXY_UNSUPPORTED` | 代理协议不支持 |

### `xnetspan`

只读 Span 借用调用方内存，不拥有数据。

```c
typedef struct xnetspan {
	cbytes Data;
	size_t Size;
} xnetspan;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cbytes` | 数据 |
| `Size` | `size_t` | 字节数 |

### `xnetwspan`

可写 Span 借用调用方内存，不拥有数据。

```c
typedef struct xnetwspan {
	bytes Data;
	size_t Size;
} xnetwspan;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `bytes` | 数据 |
| `Size` | `size_t` | 字节数 |

### `xnetresolveopstate`

解析操作只允许从等待态进入运行态，再进入一个不可变终态。

```c
typedef enum xnetresolveopstate {
	XNET_RESOLVE_PENDING = 0,
	XNET_RESOLVE_RUNNING,
	XNET_RESOLVE_RESOLVED,
	XNET_RESOLVE_FAILED,
	XNET_RESOLVE_CANCELLED
} xnetresolveopstate;
```

| 值 | 语义 |
|---|---|
| `XNET_RESOLVE_PENDING` | 等待中 |
| `XNET_RESOLVE_RUNNING` | 运行中 |
| `XNET_RESOLVE_RESOLVED` | 已解析 |
| `XNET_RESOLVE_FAILED` | 已失败 |
| `XNET_RESOLVE_CANCELLED` | 已取消 |

### `xnetresolverconfig`

所有限额都是硬边界；TTL 使用单调微秒，零值关闭对应缓存。

```c
typedef struct xnetresolverconfig {
	uint32 Workers;
	size_t RequestLimit;
	size_t QueryLimit;
	size_t CacheEntries;
	uint64 SuccessTTL;
	uint64 FailureTTL;
	size_t HostLimit;
	size_t ThreadStack;
	xnetresolverlookup Lookup;
	ptr LookupData;
} xnetresolverconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Workers` | `uint32` | 工作线程数 |
| `RequestLimit` | `size_t` | RequestLimit |
| `QueryLimit` | `size_t` | QueryLimit |
| `CacheEntries` | `size_t` | CacheEntries |
| `SuccessTTL` | `uint64` | SuccessTTL |
| `FailureTTL` | `uint64` | FailureTTL |
| `HostLimit` | `size_t` | HostLimit |
| `ThreadStack` | `size_t` | ThreadStack |
| `Lookup` | `xnetresolverlookup` | Lookup |
| `LookupData` | `ptr` | LookupData |

### `xnetresolverstats`

统计值在 Resolver 锁内取得一致快照，不要求调用方停止提交。

```c
typedef struct xnetresolverstats {
	uint32 Workers;
	uint64 Submitted;
	uint64 Rejected;
	uint64 CacheHits;
	uint64 CacheMisses;
	uint64 Coalesced;
	uint64 QueriesStarted;
	uint64 Resolved;
	uint64 Failed;
	uint64 Cancelled;
	size_t Outstanding;
	size_t ActiveQueries;
	size_t QueuedQueries;
	size_t RunningQueries;
	size_t ReadyCallbacks;
	size_t CachedResults;
} xnetresolverstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Workers` | `uint32` | 工作线程数 |
| `Submitted` | `uint64` | Submitted |
| `Rejected` | `uint64` | Rejected |
| `CacheHits` | `uint64` | CacheHits |
| `CacheMisses` | `uint64` | CacheMisses |
| `Coalesced` | `uint64` | Coalesced |
| `QueriesStarted` | `uint64` | QueriesStarted |
| `Resolved` | `uint64` | Resolved |
| `Failed` | `uint64` | Failed |
| `Cancelled` | `uint64` | Cancelled |
| `Outstanding` | `size_t` | Outstanding |
| `ActiveQueries` | `size_t` | ActiveQueries |
| `QueuedQueries` | `size_t` | QueuedQueries |
| `RunningQueries` | `size_t` | RunningQueries |
| `ReadyCallbacks` | `size_t` | ReadyCallbacks |
| `CachedResults` | `size_t` | CachedResults |

### `xnetsockettype`

Socket 类型使用稳定值，不直接暴露平台 SOCK_* 常量。

```c
typedef enum xnetsockettype {
	XNET_SOCKET_STREAM = 1,
	XNET_SOCKET_DGRAM = 2
} xnetsockettype;
```

| 值 | 语义 |
|---|---|
| `XNET_SOCKET_STREAM` | 流式（TCP） |
| `XNET_SOCKET_DGRAM` | 数据报（UDP） |

### `xnetdgrammetaflag`

数据报元数据位既用于能力、启用配置，也用于标记每个报文的有效字段。

```c
typedef enum xnetdgrammetaflag {
	XNET_DGRAM_META_DESTINATION = 0x0001,
	XNET_DGRAM_META_INTERFACE = 0x0002,
	XNET_DGRAM_META_HOP_LIMIT = 0x0004,
	XNET_DGRAM_META_TRAFFIC_CLASS = 0x0008,
	/* 合并接收时表示每个原始数据报的分段大小，最后一段允许更短。 */
	XNET_DGRAM_META_SEGMENT_SIZE = 0x0010,
	/* 控制缓冲被平台截断时保留已解析字段，并显式标记结果不完整。 */
	XNET_DGRAM_META_TRUNCATED = 0x40000000
} xnetdgrammetaflag;
```

| 值 | 语义 |
|---|---|
| `XNET_DGRAM_META_DESTINATION` | 目的地址（辅助消息） |
| `XNET_DGRAM_META_INTERFACE` | 到达接口索引 |
| `XNET_DGRAM_META_HOP_LIMIT` | HOP超限 |
| `XNET_DGRAM_META_TRAFFIC_CLASS` | 流量类别 |
| `XNET_DGRAM_META_SEGMENT_SIZE` | SEGMENT尺寸 |
| `XNET_DGRAM_META_TRUNCATED` | 元数据被截断 |

### `xnetdgrammeta`

Destination 的端口恒为零；Flags 决定其余字段是否有效。

```c
typedef struct xnetdgrammeta {
	uint32 Flags;
	xnetaddr Destination;
	uint32 Interface;
	int HopLimit;
	int TrafficClass;
	uint32 SegmentSize;
} xnetdgrammeta;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Destination` | `xnetaddr` | 目的地址 |
| `Interface` | `uint32` | Interface |
| `HopLimit` | `int` | HopLimit |
| `TrafficClass` | `int` | TrafficClass |
| `SegmentSize` | `uint32` | SegmentSize |

### `xnetdgramcontrolflag`

逐数据报发送控制位与接收元数据分离，避免 Source 和 Destination 语义混淆。

```c
typedef enum xnetdgramcontrolflag {
	XNET_DGRAM_CONTROL_SOURCE = 0x0001,
	XNET_DGRAM_CONTROL_INTERFACE = 0x0002,
	XNET_DGRAM_CONTROL_HOP_LIMIT = 0x0004,
	XNET_DGRAM_CONTROL_TRAFFIC_CLASS = 0x0008,
	/* 把一个聚合负载分段发送；最后一个数据报允许短于 SegmentSize。 */
	XNET_DGRAM_CONTROL_SEGMENT_SIZE = 0x0010
} xnetdgramcontrolflag;
```

| 值 | 语义 |
|---|---|
| `XNET_DGRAM_CONTROL_SOURCE` | 源码位置 |
| `XNET_DGRAM_CONTROL_INTERFACE` | 指定发送接口 |
| `XNET_DGRAM_CONTROL_HOP_LIMIT` | HOP超限 |
| `XNET_DGRAM_CONTROL_TRAFFIC_CLASS` | 指定流量类别 |
| `XNET_DGRAM_CONTROL_SEGMENT_SIZE` | 指定 UDP 分段大小 |

### `xnetdgramcontrol`

Source 的端口必须为零；Flags 决定本次发送覆盖哪些 Socket 默认值。

```c
typedef struct xnetdgramcontrol {
	uint32 Flags;
	xnetaddr Source;
	uint32 Interface;
	int HopLimit;
	int TrafficClass;
	uint32 SegmentSize;
} xnetdgramcontrol;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Source` | `xnetaddr` | 源视图 |
| `Interface` | `uint32` | Interface |
| `HopLimit` | `int` | HopLimit |
| `TrafficClass` | `int` | TrafficClass |
| `SegmentSize` | `uint32` | SegmentSize |

### `xnetdgramcap`

数据报高级能力按当前平台和 Socket 类型查询，不能用编译平台作运行时假设。

```c
typedef enum xnetdgramcap {
	XNET_DGRAM_CAP_PATH_MTU_MODE = 0x0001,
	XNET_DGRAM_CAP_PATH_MTU_QUERY = 0x0002,
	XNET_DGRAM_CAP_ERROR_QUEUE = 0x0004,
	XNET_DGRAM_CAP_SEGMENT_SEND = 0x0008,
	XNET_DGRAM_CAP_SEGMENT_RECEIVE = 0x0010
} xnetdgramcap;
```

| 值 | 语义 |
|---|---|
| `XNET_DGRAM_CAP_PATH_MTU_MODE` | 支持路径 MTU 模式选项 |
| `XNET_DGRAM_CAP_PATH_MTU_QUERY` | 支持路径 MTU 查询 |
| `XNET_DGRAM_CAP_ERROR_QUEUE` | 失败QUEUE |
| `XNET_DGRAM_CAP_SEGMENT_SEND` | 发送方向 |
| `XNET_DGRAM_CAP_SEGMENT_RECEIVE` | 接收方向 |

### `xnetpmtumode`

SYSTEM 保留平台默认；DISCOVER 禁止 IP 分片；FRAGMENT 允许分片；PROBE 忽略路径缓存。

```c
typedef enum xnetpmtumode {
	XNET_PMTU_SYSTEM = 0,
	XNET_PMTU_DISCOVER,
	XNET_PMTU_FRAGMENT,
	XNET_PMTU_PROBE
} xnetpmtumode;
```

| 值 | 语义 |
|---|---|
| `XNET_PMTU_SYSTEM` | 沿用系统设置 |
| `XNET_PMTU_DISCOVER` | 启用发现 |
| `XNET_PMTU_FRAGMENT` | 禁用（允许分片） |
| `XNET_PMTU_PROBE` | 探测模式 |

### `xnetdgramerrororigin`

错误来源独立于 ICMP Type/Code，LOCAL 也可以携带路径 MTU。

```c
typedef enum xnetdgramerrororigin {
	XNET_DGRAM_ERROR_UNKNOWN = 0,
	XNET_DGRAM_ERROR_LOCAL,
	XNET_DGRAM_ERROR_ICMP,
	XNET_DGRAM_ERROR_ICMP6
} xnetdgramerrororigin;
```

| 值 | 语义 |
|---|---|
| `XNET_DGRAM_ERROR_UNKNOWN` | 未知 |
| `XNET_DGRAM_ERROR_LOCAL` | 失败 |
| `XNET_DGRAM_ERROR_ICMP` | 失败 |
| `XNET_DGRAM_ERROR_ICMP6` | ICMPv6 错误 |

### `xnetdgramerrorflag`

Flags 精确说明地址、路径 MTU 与负载截断字段是否有效。

```c
typedef enum xnetdgramerrorflag {
	XNET_DGRAM_ERROR_REMOTE = 0x0001,
	XNET_DGRAM_ERROR_OFFENDER = 0x0002,
	XNET_DGRAM_ERROR_PATH_MTU = 0x0004,
	XNET_DGRAM_ERROR_PAYLOAD_TRUNCATED = 0x0008,
	XNET_DGRAM_ERROR_META_TRUNCATED = 0x0010
} xnetdgramerrorflag;
```

| 值 | 语义 |
|---|---|
| `XNET_DGRAM_ERROR_REMOTE` | 失败 |
| `XNET_DGRAM_ERROR_OFFENDER` | 失败 |
| `XNET_DGRAM_ERROR_PATH_MTU` | 失败 |
| `XNET_DGRAM_ERROR_PAYLOAD_TRUNCATED` | 已截断 |
| `XNET_DGRAM_ERROR_META_TRUNCATED` | 元数据被截断 |

### `xnetdgramerror`

异步网络错误是可读取数据，不会污染当前执行上下文的错误对象。

```c
typedef struct xnetdgramerror {
	uint32 Flags;
	xnetdgramerrororigin Origin;
	xerrkind Kind;
	int SystemCode;
	int Type;
	int Code;
	uint32 Info;
	uint32 Data;
	size_t PathMtu;
	xnetaddr Remote;
	xnetaddr Offender;
} xnetdgramerror;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Origin` | `xnetdgramerrororigin` | Origin |
| `Kind` | `xerrkind` | 错误种类 |
| `SystemCode` | `int` | 平台错误码 |
| `Type` | `int` | 类型 |
| `Code` | `int` | 错误码 |
| `Info` | `uint32` | 信息输出 |
| `Data` | `uint32` | 数据 |
| `PathMtu` | `size_t` | PathMtu |
| `Remote` | `xnetaddr` | Remote |
| `Offender` | `xnetaddr` | Offender |

### `xnetdgramrecv`

批量接收项由调用方提供缓冲，函数写入来源、可见长度和单报文结果。

```c
typedef struct xnetdgramrecv {
	void* Data;
	size_t Capacity;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	size_t Size;
	xnetresult Result;
} xnetdgramrecv;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `void*` | 数据 |
| `Capacity` | `size_t` | 容量 |
| `Remote` | `xnetaddr` | Remote |
| `Meta` | `xnetdgrammeta` | 元数据 |
| `Size` | `size_t` | 字节数 |
| `Result` | `xnetresult` | 结果输出 |

### `xnetdgramsend`

批量发送项在调用期间借用数据；空远端表示使用连接式 UDP 的固定 Peer。

```c
typedef struct xnetdgramsend {
	const xnetaddr* Remote;
	const void* Data;
	size_t Size;
} xnetdgramsend;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Remote` | `const xnetaddr*` | Remote |
| `Data` | `const void*` | 数据 |
| `Size` | `size_t` | 字节数 |

### `xnetsocketflag`

Socket 打开标志可以组合；系统句柄始终禁止被子进程继承。

```c
typedef enum xnetsocketflag {
	XNET_SOCKET_NONBLOCK = 0x01
} xnetsocketflag;
```

| 值 | 语义 |
|---|---|
| `XNET_SOCKET_NONBLOCK` | 非阻塞模式 |

### `xnetshutdown`

半关闭方向与平台 SHUT_* 常量保持隔离。

```c
typedef enum xnetshutdown {
	XNET_SHUTDOWN_READ = 1,
	XNET_SHUTDOWN_WRITE,
	XNET_SHUTDOWN_BOTH
} xnetshutdown;
```

| 值 | 语义 |
|---|---|
| `XNET_SHUTDOWN_READ` | 读方向 |
| `XNET_SHUTDOWN_WRITE` | 写方向 |
| `XNET_SHUTDOWN_BOTH` | 双向关断 |

### `xnetoption`

通用 Socket 选项以 int64 表达；负 linger 值表示关闭 linger。

```c
typedef enum xnetoption {
	XNET_OPTION_NONBLOCK = 1,
	XNET_OPTION_REUSE_ADDRESS,
	XNET_OPTION_REUSE_PORT,
	XNET_OPTION_EXCLUSIVE_ADDRESS,
	XNET_OPTION_NO_DELAY,
	XNET_OPTION_KEEP_ALIVE,
	XNET_OPTION_BROADCAST,
	XNET_OPTION_IPV6_ONLY,
	XNET_OPTION_RECEIVE_BUFFER,
	XNET_OPTION_SEND_BUFFER,
	XNET_OPTION_LINGER,
	XNET_OPTION_HOP_LIMIT,
	XNET_OPTION_TRAFFIC_CLASS,
	XNET_OPTION_PATH_MTU_MODE,
	XNET_OPTION_PATH_MTU,
	XNET_OPTION_DGRAM_ERRORS,
	XNET_OPTION_ERROR
} xnetoption;
```

| 值 | 语义 |
|---|---|
| `XNET_OPTION_NONBLOCK` | 非阻塞模式 |
| `XNET_OPTION_REUSE_ADDRESS` | 重用本地地址 |
| `XNET_OPTION_REUSE_PORT` | 重用端口（负载分担） |
| `XNET_OPTION_EXCLUSIVE_ADDRESS` | 独占地址 |
| `XNET_OPTION_NO_DELAY` | 禁用 Nagle |
| `XNET_OPTION_KEEP_ALIVE` | TCP 保活 |
| `XNET_OPTION_BROADCAST` | 广播 |
| `XNET_OPTION_IPV6_ONLY` | 仅 IPv6 |
| `XNET_OPTION_RECEIVE_BUFFER` | 接收方向 |
| `XNET_OPTION_SEND_BUFFER` | 发送方向 |
| `XNET_OPTION_LINGER` | 迟滞关闭 |
| `XNET_OPTION_HOP_LIMIT` | HOP超限 |
| `XNET_OPTION_TRAFFIC_CLASS` | 流量类别 |
| `XNET_OPTION_PATH_MTU_MODE` | 路径 MTU 模式 |
| `XNET_OPTION_PATH_MTU` | 路径 MTU 探测上限 |
| `XNET_OPTION_DGRAM_ERRORS` | ICMP 错误投递 |
| `XNET_OPTION_ERROR` | 失败 |

### `xnetportbackend`

AUTO 选择当前平台最优后端，显式值用于测试、降级和部署控制。

```c
typedef enum xnetportbackend {
	XNET_PORT_AUTO = 0,
	XNET_PORT_IOCP,
	XNET_PORT_URING,
	XNET_PORT_EPOLL,
	XNET_PORT_KQUEUE,
	XNET_PORT_SELECT
} xnetportbackend;
```

| 值 | 语义 |
|---|---|
| `XNET_PORT_AUTO` | 自动 |
| `XNET_PORT_IOCP` | Windows IOCP |
| `XNET_PORT_URING` | Linux io_uring |
| `XNET_PORT_EPOLL` | Linux epoll |
| `XNET_PORT_KQUEUE` | BSD kqueue |
| `XNET_PORT_SELECT` | select 就绪 |

### `xnetportcap`

能力位明确区分完成式 IO 与 readiness，避免后端伪造同一种语义。

```c
typedef enum xnetportcap {
	XNET_PORT_CAP_READINESS = 0x0001,
	XNET_PORT_CAP_COMPLETION = 0x0002,
	XNET_PORT_CAP_ONESHOT = 0x0004,
	XNET_PORT_CAP_EDGE = 0x0008,
	XNET_PORT_CAP_BATCH = 0x0010,
	XNET_PORT_CAP_WAKE = 0x0020,
	XNET_PORT_CAP_POST = 0x0040,
	XNET_PORT_CAP_CANCEL = 0x0080,
	XNET_PORT_CAP_READ_PROBE = 0x0100,
	XNET_PORT_CAP_DGRAM_ERROR = 0x0200,
	XNET_PORT_CAP_SEND_FILE = 0x0400,
	XNET_PORT_CAP_FILE_IO = 0x0800
} xnetportcap;
```

| 值 | 语义 |
|---|---|
| `XNET_PORT_CAP_READINESS` | 就绪通知 |
| `XNET_PORT_CAP_COMPLETION` | 完成通知 |
| `XNET_PORT_CAP_ONESHOT` | 一次性注册 |
| `XNET_PORT_CAP_EDGE` | 边缘触发 |
| `XNET_PORT_CAP_BATCH` | 批量收割 |
| `XNET_PORT_CAP_WAKE` | 跨线程唤醒 |
| `XNET_PORT_CAP_POST` | POST 方法 |
| `XNET_PORT_CAP_CANCEL` | 在途取消 |
| `XNET_PORT_CAP_READ_PROBE` | 读方向PROBE |
| `XNET_PORT_CAP_DGRAM_ERROR` | DGRAM失败 |
| `XNET_PORT_CAP_SEND_FILE` | 发送方向 |
| `XNET_PORT_CAP_FILE_IO` | 原生文件 I/O |

### `xnetpoll`

readiness 关注位；错误与挂断始终隐式观察。

```c
typedef enum xnetpoll {
	XNET_POLL_READ = 0x01,
	XNET_POLL_WRITE = 0x02
} xnetpoll;
```

| 值 | 语义 |
|---|---|
| `XNET_POLL_READ` | XNETPOLL读方向 |
| `XNET_POLL_WRITE` | 可写 |

### `xnetporteventtype`

一个事件只属于一个稳定类别，具体 readiness 状态由 Flags 组合表达。

```c
typedef enum xnetporteventtype {
	XNET_PORT_EVENT_READY = 1,
	XNET_PORT_EVENT_ACCEPT,
	XNET_PORT_EVENT_CONNECT,
	XNET_PORT_EVENT_READ_PROBE,
	XNET_PORT_EVENT_RECV,
	XNET_PORT_EVENT_SEND,
	XNET_PORT_EVENT_RECV_FROM,
	XNET_PORT_EVENT_RECV_MSG,
	XNET_PORT_EVENT_RECV_ERROR,
	XNET_PORT_EVENT_SEND_TO,
	XNET_PORT_EVENT_SEND_MSG,
	XNET_PORT_EVENT_SEND_FILE,
	XNET_PORT_EVENT_FILE_READ,
	XNET_PORT_EVENT_FILE_WRITE,
	XNET_PORT_EVENT_USER,
	XNET_PORT_EVENT_WAKE
} xnetporteventtype;
```

| 值 | 语义 |
|---|---|
| `XNET_PORT_EVENT_READY` | 就绪 |
| `XNET_PORT_EVENT_ACCEPT` | 可接受连接 |
| `XNET_PORT_EVENT_CONNECT` | CONNECT 方法 |
| `XNET_PORT_EVENT_READ_PROBE` | 读方向PROBE |
| `XNET_PORT_EVENT_RECV` | 可读 |
| `XNET_PORT_EVENT_SEND` | 发送方向 |
| `XNET_PORT_EVENT_RECV_FROM` | 可读（带来源） |
| `XNET_PORT_EVENT_RECV_MSG` | 可读（带元数据） |
| `XNET_PORT_EVENT_RECV_ERROR` | RECV失败 |
| `XNET_PORT_EVENT_SEND_TO` | 发送方向 |
| `XNET_PORT_EVENT_SEND_MSG` | 发送方向 |
| `XNET_PORT_EVENT_SEND_FILE` | 发送方向 |
| `XNET_PORT_EVENT_FILE_READ` | FILE读方向 |
| `XNET_PORT_EVENT_FILE_WRITE` | FILE写方向 |
| `XNET_PORT_EVENT_USER` | 用户唤醒 |
| `XNET_PORT_EVENT_WAKE` | 用户唤醒 |

### `xnetporteventflag`

事件标志表达 readiness 方向及完成式 EOF、错误等稳定状态。

```c
typedef enum xnetporteventflag {
	XNET_PORT_EVENT_READ = 0x0001,
	XNET_PORT_EVENT_WRITE = 0x0002,
	XNET_PORT_EVENT_ERROR = 0x0004,
	XNET_PORT_EVENT_HANGUP = 0x0008,
	XNET_PORT_EVENT_EOF = 0x0010,
	XNET_PORT_EVENT_MORE = 0x0020
} xnetporteventflag;
```

| 值 | 语义 |
|---|---|
| `XNET_PORT_EVENT_READ` | 读方向 |
| `XNET_PORT_EVENT_WRITE` | 写方向 |
| `XNET_PORT_EVENT_ERROR` | 失败 |
| `XNET_PORT_EVENT_HANGUP` | 对端关闭 |
| `XNET_PORT_EVENT_EOF` | 读到末尾 |
| `XNET_PORT_EVENT_MORE` | 需要更多输入 |

### `xnetportconfig`

配置不带版本字段；OperationCache 是每个完成操作尺寸类的缓存上限，零值关闭缓存。

```c
typedef struct xnetportconfig {
	xnetportbackend Backend;
	uint32 Flags;
	size_t PostLimit;
	/* Watch/Operation 为零时按实际后端选择可扩展默认值。 */
	size_t WatchLimit;
	size_t OperationLimit;
	size_t OperationCache;
} xnetportconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Backend` | `xnetportbackend` | Backend |
| `Flags` | `uint32` | 标志位 |
| `PostLimit` | `size_t` | PostLimit |
| `WatchLimit` | `size_t` | WatchLimit |
| `OperationLimit` | `size_t` | OperationLimit |
| `OperationCache` | `size_t` | OperationCache |

### `xnetportevent`

端口事件借用原 Socket；只有 Accepted 在接受完成时转移新对象所有权。

```c
typedef struct xnetportevent {
	xnetporteventtype Type;
	uint32 Flags;
	xnetresult Result;
	int SystemCode;
	size_t Bytes;
	uint64 Id;
	xnetsocket Socket;
	xnetsocket Accepted;
	xnetaddr Address;
	xnetdgrammeta Meta;
	xnetdgramerror DgramError;
	ptr User;
} xnetportevent;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xnetporteventtype` | 类型 |
| `Flags` | `uint32` | 标志位 |
| `Result` | `xnetresult` | 结果输出 |
| `SystemCode` | `int` | 平台错误码 |
| `Bytes` | `size_t` | Bytes |
| `Id` | `uint64` | 标识 |
| `Socket` | `xnetsocket` | Socket |
| `Accepted` | `xnetsocket` | Accepted |
| `Address` | `xnetaddr` | 地址 |
| `Meta` | `xnetdgrammeta` | 元数据 |
| `DgramError` | `xnetdgramerror` | DgramError |
| `User` | `ptr` | User |

### `xnetenginestate`

Engine 生命周期状态可安全地跨线程查询。

```c
typedef enum xnetenginestate {
	XNET_ENGINE_STOPPED = 0,
	XNET_ENGINE_STARTING,
	XNET_ENGINE_RUNNING,
	XNET_ENGINE_STOPPING,
	XNET_ENGINE_DESTROYING
} xnetenginestate;
```

| 值 | 语义 |
|---|---|
| `XNET_ENGINE_STOPPED` | 已停止 |
| `XNET_ENGINE_STARTING` | 启动中 |
| `XNET_ENGINE_RUNNING` | 运行中 |
| `XNET_ENGINE_STOPPING` | 停止中 |
| `XNET_ENGINE_DESTROYING` | 销毁中 |

### `xnetpost`

Post 的队列节点与并发门保持不透明，允许嵌入网络对象。

```c
typedef union xnetpost {
	uint64 Alignment;
	uint8 Storage[XNET_POST_STORAGE_SIZE];
} xnetpost;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### `xnetcompletion`

Completion 借用过程与数据，必须存活到对应端口事件回调结束。

```c
typedef struct xnetcompletion {
	xnetcompletionproc Proc;
	ptr Data;
} xnetcompletion;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Proc` | `xnetcompletionproc` | 过程指针 |
| `Data` | `ptr` | 数据 |

### `xnetengineconfig`

容量字段都是硬边界；CommandCapacity 会向上取整为 2 次幂。

```c
typedef struct xnetengineconfig {
	xnetportbackend Backend;
	uint32 Workers;
	const xnetbufpoolconfig* BufferPool;
	size_t CommandCapacity;
	size_t NodeCacheBytes;
	size_t TimerLimit;
	size_t EventBatch;
	size_t PortPostLimit;
	/* 两个端口容量为零时由每个 Worker 的实际后端解析。 */
	size_t PortWatchLimit;
	size_t PortOperationLimit;
	size_t PortOperationCache;
	uint64 IdleWait;
	size_t ThreadStack;
} xnetengineconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Backend` | `xnetportbackend` | Backend |
| `Workers` | `uint32` | 工作线程数 |
| `BufferPool` | `const xnetbufpoolconfig*` | BufferPool |
| `CommandCapacity` | `size_t` | CommandCapacity |
| `NodeCacheBytes` | `size_t` | NodeCacheBytes |
| `TimerLimit` | `size_t` | TimerLimit |
| `EventBatch` | `size_t` | EventBatch |
| `PortPostLimit` | `size_t` | PortPostLimit |
| `PortWatchLimit` | `size_t` | PortWatchLimit |
| `PortOperationLimit` | `size_t` | PortOperationLimit |
| `PortOperationCache` | `size_t` | PortOperationCache |
| `IdleWait` | `uint64` | IdleWait |
| `ThreadStack` | `size_t` | ThreadStack |

### `xnetworkerstats`

Worker 统计是并发快照，计数在 Engine 重启后继续累计。

```c
typedef struct xnetworkerstats {
	uint64 PostsAccepted;
	uint64 PostsRejected;
	uint64 PostsExecuted;
	uint64 TimersAccepted;
	uint64 TimersRejected;
	uint64 TimersFired;
	uint64 TimersCancelled;
	uint64 TimersClosed;
	uint64 TimerErrors;
	uint64 Events;
	uint64 WaitErrors;
	uint64 WakeErrors;
	/* BASIC 以上统计中，停机任务链未在安全代数内收敛的累计次数。 */
	uint64 ShutdownStalls;
	/* 最近一次端口等待错误；没有错误时 Code 为 XNET_ERROR_NONE。 */
	xneterror LastWaitError;
	int LastWaitSystemCode;
	uint64 NodeCacheHits;
	uint64 NodeCacheMisses;
	size_t PendingCommands;
	size_t ActiveTimers;
	size_t NodeCachedBytes;
} xnetworkerstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `PostsAccepted` | `uint64` | PostsAccepted |
| `PostsRejected` | `uint64` | PostsRejected |
| `PostsExecuted` | `uint64` | PostsExecuted |
| `TimersAccepted` | `uint64` | TimersAccepted |
| `TimersRejected` | `uint64` | TimersRejected |
| `TimersFired` | `uint64` | TimersFired |
| `TimersCancelled` | `uint64` | TimersCancelled |
| `TimersClosed` | `uint64` | TimersClosed |
| `TimerErrors` | `uint64` | TimerErrors |
| `Events` | `uint64` | 事件表 |
| `WaitErrors` | `uint64` | WaitErrors |
| `WakeErrors` | `uint64` | WakeErrors |
| `ShutdownStalls` | `uint64` | ShutdownStalls |
| `LastWaitError` | `xneterror` | LastWaitError |
| `LastWaitSystemCode` | `int` | LastWaitSystemCode |
| `NodeCacheHits` | `uint64` | NodeCacheHits |
| `NodeCacheMisses` | `uint64` | NodeCacheMisses |
| `PendingCommands` | `size_t` | PendingCommands |
| `ActiveTimers` | `size_t` | ActiveTimers |
| `NodeCachedBytes` | `size_t` | NodeCachedBytes |

### `xnetenginestats`

Engine 统计聚合全部 Worker，并附带当前生命周期状态。

```c
typedef struct xnetenginestats {
	xnetenginestate State;
	uint32 Workers;
	uint64 PostsAccepted;
	uint64 PostsRejected;
	uint64 PostsExecuted;
	uint64 TimersAccepted;
	uint64 TimersRejected;
	uint64 TimersFired;
	uint64 TimersCancelled;
	uint64 TimersClosed;
	uint64 TimerErrors;
	uint64 Events;
	uint64 WaitErrors;
	uint64 WakeErrors;
	/* BASIC 以上统计中，全部 Worker 停机任务链未收敛次数之和。 */
	uint64 ShutdownStalls;
	uint64 NodeCacheHits;
	uint64 NodeCacheMisses;
	size_t PendingCommands;
	size_t ActiveTimers;
	size_t NodeCachedBytes;
	size_t LiveObjects;
} xnetenginestats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xnetenginestate` | 状态 |
| `Workers` | `uint32` | 工作线程数 |
| `PostsAccepted` | `uint64` | PostsAccepted |
| `PostsRejected` | `uint64` | PostsRejected |
| `PostsExecuted` | `uint64` | PostsExecuted |
| `TimersAccepted` | `uint64` | TimersAccepted |
| `TimersRejected` | `uint64` | TimersRejected |
| `TimersFired` | `uint64` | TimersFired |
| `TimersCancelled` | `uint64` | TimersCancelled |
| `TimersClosed` | `uint64` | TimersClosed |
| `TimerErrors` | `uint64` | TimerErrors |
| `Events` | `uint64` | 事件表 |
| `WaitErrors` | `uint64` | WaitErrors |
| `WakeErrors` | `uint64` | WakeErrors |
| `ShutdownStalls` | `uint64` | ShutdownStalls |
| `NodeCacheHits` | `uint64` | NodeCacheHits |
| `NodeCacheMisses` | `uint64` | NodeCacheMisses |
| `PendingCommands` | `size_t` | PendingCommands |
| `ActiveTimers` | `size_t` | ActiveTimers |
| `NodeCachedBytes` | `size_t` | NodeCachedBytes |
| `LiveObjects` | `size_t` | LiveObjects |

### `xnetref`

引用 Span 在受理后转移释放责任，零长度 Span 不转移所有权。

```c
typedef struct xnetref {
	cbytes Data;
	size_t Size;
	xnetreleaseproc Release;
	ptr Context;
} xnetref;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cbytes` | 数据 |
| `Size` | `size_t` | 字节数 |
| `Release` | `xnetreleaseproc` | Release |
| `Context` | `ptr` | 回调上下文 |

### `xnetbufpoolinfo`

缓冲池统计区分实时、缓存、分配、复用和动态大块。

```c
typedef struct xnetbufpoolinfo {
	size_t LiveBlocks;
	size_t LiveBytes;
	size_t PeakBlocks;
	size_t PeakBytes;
	size_t CachedBlocks;
	size_t CachedBytes;
	uint64 AllocCount;
	uint64 ReuseCount;
	uint64 DynamicCount;
	uint64 RefCount;
} xnetbufpoolinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `LiveBlocks` | `size_t` | LiveBlocks |
| `LiveBytes` | `size_t` | LiveBytes |
| `PeakBlocks` | `size_t` | PeakBlocks |
| `PeakBytes` | `size_t` | PeakBytes |
| `CachedBlocks` | `size_t` | CachedBlocks |
| `CachedBytes` | `size_t` | CachedBytes |
| `AllocCount` | `uint64` | AllocCount |
| `ReuseCount` | `uint64` | ReuseCount |
| `DynamicCount` | `uint64` | DynamicCount |
| `RefCount` | `uint64` | RefCount |

### `xnetbuf`

缓冲链可栈上使用；字段用于零分配查询，调用方不得直接修改。

```c
typedef struct xnetbuf {
	xnetblock* Head;
	xnetblock* Tail;
	xnetblock* Reserved;
	xnetbufpool* Pool;
	size_t Size;
	size_t Blocks;
	bool ReservedNew;
} xnetbuf;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Head` | `xnetblock*` | 头指针 |
| `Tail` | `xnetblock*` | 尾指针 |
| `Reserved` | `xnetblock*` | 保留（必须为零） |
| `Pool` | `xnetbufpool*` | Pool |
| `Size` | `size_t` | 字节数 |
| `Blocks` | `size_t` | Blocks |
| `ReservedNew` | `bool` | ReservedNew |

### `xnetaddrlist`

地址列表是不可变共享结果，解析器缓存和调用方可以独立持有引用。

```c
typedef struct xnetaddrlist xnetaddrlist;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetresolver`

Resolver 与解析操作均保持不透明，解析操作可以独立于调用方引用继续执行。

```c
typedef struct xnetresolver xnetresolver;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetresolveop`

解析操作对象（不透明）：线程安全的可等待句柄，内含查询状态与结果地址列表。


```c
typedef struct xnetresolveop xnetresolveop;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetport`

网络端口隐藏平台事件对象、注册表和跨线程唤醒资源。

```c
typedef struct xnetport_impl xnetport;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetbufpoolconfig`

网络缓冲池配置：按尺寸类预分配缓冲，供 `xrtNetBufPool` 创建共享池。


```c
typedef struct xnetbufpoolconfig xnetbufpoolconfig;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetbufpool`

网络缓冲池（不透明）：进程内共享的按类缓冲池，降低热路径分配。


```c
typedef struct xnetbufpool xnetbufpool;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetbytes`

拥有型字节结果（不透明）：一次性消费的动态缓冲，用后销毁。


```c
typedef struct xnetbytes xnetbytes;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetengine`

Engine 与 Worker 对外保持不透明，所有 Worker 资源都归所属 Engine 管理。

```c
typedef struct xnetengine xnetengine;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetworker`

网络 Worker（不透明）：Engine 的执行单元，流与监听器绑定其上。


```c
typedef struct xnetworker xnetworker;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetblock`

网络分块借用（opaque 由 net.md 正文描述）：面向 Completion 的零拷贝视图单元。


```c
typedef struct xnetblock xnetblock;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xnetresolverlookup`

自定义查询过程必须返回端口为零的不可变地址列表，并在失败时设置结构化错误。

```c
typedef xnetaddrlist* (*xnetresolverlookup)(
	cstr sHost,
	xnetfamily Family,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xnetresolveproc`

完成回调借用操作对象；保留到回调之后时必须显式增加引用。

```c
typedef void (*xnetresolveproc)(xnetresolveop* pOperation, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xnettaskproc`

Engine 任务始终在选定 Worker 上串行执行。

```c
typedef void (*xnettaskproc)(xnetworker* pWorker, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xnettimerproc`

Timer 受理后恰好终结一次，结果区分到期、取消、停止和内部失败。

```c
typedef void (*xnettimerproc)(xnetworker* pWorker,
	uint64 Id, xnetresult Result, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xnetcompletionproc`

Engine 内的端口事件通过 Completion 回到所属 Worker。

```c
typedef void (*xnetcompletionproc)(xnetworker* pWorker,
	const xnetportevent* pEvent, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xnetreleaseproc`

外部引用的释放过程在最后一段数据离开缓冲时执行一次。

```c
typedef void (*xnetreleaseproc)(ptr pContext, cbytes pData, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XNET_STATS_OFF` | `0` | 网络累计统计按编译期级别裁剪；状态量和背压计数始终保留。 |
| `XNET_STATS_BASIC` | `1` | STATSBASIC |
| `XNET_STATS_FULL` | `2` | STATS已满 |
| `XNET_DGRAM_BATCH_MAX` | `64u` | DGRAMBATCH上限 |
| `XNET_POST_STORAGE_SIZE` | `64u` | 嵌入式 Post 提供不分配内存且不占公开命令队列容量的 Worker 投递。 |
| `XNET_BUFFER_CLASS_COUNT` | `4u` | 网络缓冲池包含四个可配置尺寸类。 |

## API 速览

```c
xnetaddrlist* xrtNetLookup(cstr sHost, xnetfamily Family);
xnetaddrlist* xrtNetResolve(cstr sHost, uint16 iPort,
	xnetfamily Family);
bool xrtNetResolveOne(xnetaddr* pAddr, cstr sHost,
	uint16 iPort, xnetfamily Family);
str xrtNetReverse(const xnetaddr* pAddr);

xnetaddrlist* xrtNetAddrListCreate(const xnetaddr* pAddresses,
	size_t iCount);
xnetaddrlist* xrtNetAddrListWithPort(xnetaddrlist* pList,
	uint16 iPort);
xnetaddrlist* xrtNetAddrListRef(xnetaddrlist* pList);
void xrtNetAddrListDestroy(xnetaddrlist* pList);
size_t xrtNetAddrListCount(const xnetaddrlist* pList);
const xnetaddr* xrtNetAddrListGet(const xnetaddrlist* pList,
	size_t iIndex);
```

`xrtNetLookup` 只解析主机，返回的所有端口均为零，适合作为缓存、连接竞速和自定义 Resolver 的底层结果。`xrtNetResolve` 在同一完整查询基础上写入调用方端口，适合直接建立端点；两者都返回系统顺序中的全部 IPv4/IPv6 地址并去重。

两类查询都不使用固定 64 项临时数组，也不把主机名复制到固定 256 字节字段。数字 IPv4、IPv6 和方括号 IPv6 不进入系统 DNS。`Family` 可以是 `UNSPEC`、`IPV4` 或 `IPV6`；单地址端点场景使用 `xrtNetResolveOne`，失败时输出保持不变。

地址列表不可变且引用计数安全。`xrtNetAddrListCreate` 校验、复制并按完整端点去重外部地址；`xrtNetAddrListWithPort` 保持地址顺序并统一端口，端口未变化时直接增加原列表引用，否则返回独立列表。`Get` 返回的地址只在对应列表引用存活期间有效；缓存或异步解析器可以通过 `Ref` 与调用方共享同一完整结果。

`xrtNetReverse` 使用 `NI_NAMEREQD`。没有 PTR 名称时明确失败，不把数字地址回退伪装成主机名。名称服务失败使用 `XNET_ERROR_DNS_RESOLVE`、`XNET_ERROR_DNS_REVERSE` 或 `XNET_ERROR_DNS_RESULT`，`SystemCode` 保存平台 `getaddrinfo/getnameinfo` 返回码。

完整示例位于 `examples/network/dns/main.c`。

## 构造与解析

### `xrtNetAddrAny`
构造指定族的未指定地址（IPv4 `0.0.0.0` 或 IPv6 `::`）。

```c
bool xrtNetAddrAny(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 接收构造结果 |
| `Family` | 输入 | `IPV4` 或 `IPV6` | 其他值失败 |
| `iPort` | 输入 | — | 填入端口；`0` 合法（服务器动态端口绑定） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已构造 | — |
| `false` | 族不合法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pAddr == NULL` 或 `Family` 非法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 通配地址构造与判定

```c
if ( !xrtNetAddrAny(&Any, XNET_FAMILY_IPV4, 0u) ||
```

### `xrtNetAddrLoopback`
构造指定族的回环地址（IPv4 `127.0.0.1` 或 IPv6 `::1`）。

```c
bool xrtNetAddrLoopback(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 接收构造结果 |
| `Family` | 输入 | `IPV4` 或 `IPV6` | — |
| `iPort` | 输入 | — | 填入端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已构造 | — |
| `false` | 族不合法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pAddr == NULL` 或 `Family` 非法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 回环以 `Parse("127.0.0.1")` 等价构造（见 `xrtNetAddrParse` 范例）

```c
if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
```

### `xrtNetAddrParse`
严格解析数字 IPv4 或 IPv6 文本；不执行 DNS。

```c
bool xrtNetAddrParse(xnetaddr* pAddr, cstr sIP, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sIP` | 输入 | 非空 | IPv4 四段十进制（拒绝越界/缺段/前导零歧义）；IPv6 支持 `::`、嵌入式 IPv4、`%42` 数字 Scope；启用 `XRT_FEATURE_NET_INTERFACE` 后还接受 `%eth0` 接口名 Scope |
| `iPort` | 输入 | — | 填入端口，与文本无关 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析构造 | — |
| `false` | 文本非法 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FORMAT` — 地址文本不符合严格语法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 双地址构造供比较族使用

```c
if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
	!xrtNetAddrParse(&Private, "10.0.0.5", 8080u) ||
```

### `xrtNetAddrParseEndpoint`
解析 `IPv4:port`、`[IPv6]:port` 或使用默认端口的裸地址。

```c
bool xrtNetAddrParseEndpoint(xnetaddr* pAddr, cstr sEndpoint, uint16 iDefaultPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sEndpoint` | 输入 | 非空 | 裸 IPv6 的最后一段不会被猜测为端口——IPv6 显式端口必须方括号；地址按切片解析、不复制到定长临时数组 |
| `iDefaultPort` | 输入 | — | 文本未带端口时使用；`0` 合法 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析构造 | — |
| `false` | 文本非法 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FORMAT` — 端点语法非法

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · 带 Scope 的 IPv6 端点

```c
if ( !xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0) ) {
	return 1;
}
```

## 文本输出

### `xrtNetAddrText`
输出规范 IP 文本，返回不含结尾零字节的所需长度。

```c
size_t xrtNetAddrText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv6 按 RFC 5952：小写、去前导零、压缩第一个最长零段；IPv4 映射输出 `::ffff:192.0.2.1` |
| `sText` | 输出 | 允许空指针 | `NULL, 0` 为零分配查询；容量不足仍尽量写零结尾文本 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 所需长度 | 不含结尾零；写入成功时即实际字节数 | — |
| `XRT_NPOS` | 地址非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_BUFFER` — 容量不足（仍返回所需长度并尽量写出）
- `XERR_ARGUMENT` — 参数非法

#### 范例

[network/interface · 基础范例](../../examples/network/interface/main.c) · 枚举接口地址文本

```c
if ( xrtNetAddrText(
	&pAddress->Address, sAddress, sizeof(sAddress)
) == XRT_NPOS ) {
```

### `xrtNetAddrEndpointText`
输出带端口的规范端点文本，IPv6 始终使用方括号。

```c
size_t xrtNetAddrEndpointText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 端口始终出现 |
| `sText` | 输出 | 允许空指针 | 同 `xrtNetAddrText` 的两段式口径 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 所需长度 | 不含结尾零 | — |
| `XRT_NPOS` | 地址非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_BUFFER` — 容量不足
- `XERR_ARGUMENT` — 参数非法

#### 范例

[network/interface · 基础范例](../../examples/network/interface/main.c) · 端点输出与 `AddrText` 同口径（见其范例）

```c
if ( xrtNetAddrText(
	&pAddress->Address, sAddress, sizeof(sAddress)
) == XRT_NPOS ) {
```

### `xrtNetAddrString`
分配并返回规范 IP 文本。

```c
str xrtNetAddrString(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式零结尾文本，`xrtFree` 释放；可长期保存、跨函数传递、同表达式多次调用（替换旧版线程局部环形缓冲） | — |
| `NULL` | 地址非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 参数非法
- 分配失败 — 文本内存申请失败

#### 范例

[network/dns · 基础范例](../../examples/network/dns/main.c) · 同族拥有式端点文本用法

```c
str sEndpoint = xrtNetAddrEndpointString(
```

### `xrtNetAddrEndpointString`
分配并返回带端口的规范端点文本。

```c
str xrtNetAddrEndpointString(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv6 自动加方括号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式零结尾文本，`xrtFree` 释放 | — |
| `NULL` | 地址非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 参数非法
- 分配失败 — 文本内存申请失败

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · 端点往返

```c
sEndpoint = xrtNetAddrEndpointString(&Addr);
if ( sEndpoint == NULL ) {
	return 1;
}
```

## 比较与分类

### `xrtNetAddrEqual`
比较完整端点：族、地址、IPv6 Scope 与端口全等。

```c
bool xrtNetAddrEqual(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 完整端点相同（端口不同即不等） |
| `false` | 任一分量不同 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · SameIP 分界：同 IP 换端口

```c
if ( xrtNetAddrEqual(&Loopback, &Private) ||
	!xrtNetAddrEqual(&Private, &Private) ||
	xrtNetAddrEqual(&Private, &Other) ||
```

### `xrtNetAddrSameIP`
只比较地址族、地址与 IPv6 Scope，不比较端口。

```c
bool xrtNetAddrSameIP(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 同族同地址同 Scope（端口可不同） |
| `false` | 地址分量不同 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 与 Equal 对照

```c
xrtNetAddrSameIP(&Loopback, &Private) ||
	!xrtNetAddrSameIP(&Private, &Other) ) {
```

### `xrtNetAddrCompare`
为 Map、排序和稳定去重提供完整端点全序。

```c
int xrtNetAddrCompare(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 负数 | 左端点按序在前（比较序：族 → 地址 → Scope → 端口） |
| `0` | 完整端点相同 |
| 正数 | 左端点在后 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 自反为零、10 < 127

```c
if ( (xrtNetAddrCompare(&Private, &Private) != 0) ||
	(xrtNetAddrCompare(&Private, &Loopback) >= 0) ||
```

### `xrtNetAddrIsUnspecified`
判断地址是否为 IPv4 `0.0.0.0` 或 IPv6 `::`。

```c
bool xrtNetAddrIsUnspecified(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只看地址，端口无关 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 未指定地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · Any 构造后判定

```c
xrtNetAddrIsUnspecified(&Loopback) ||
	!xrtNetAddrIsUnspecified(&Any) ||
```

### `xrtNetAddrIsLoopback`
判断地址是否属于 IPv4 `127/8` 或 IPv6 `::1`。

```c
bool xrtNetAddrIsLoopback(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv4 整个 `127/8` 段都算回环 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 回环地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 正反判定

```c
!xrtNetAddrIsLoopback(&Loopback) ||
	xrtNetAddrIsLoopback(&Private) ||
```

### `xrtNetAddrIsMulticast`
判断地址是否属于 IPv4 `224/4` 或 IPv6 `ff00::/8`。

```c
bool xrtNetAddrIsMulticast(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 多播组地址 |
| `false` | 单播地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `224.0.0.1` 命中

```c
!xrtNetAddrParse(&Other, "224.0.0.1", 0u) ||
	!xrtNetAddrIsMulticast(&Other) ||
```

### `xrtNetAddrIsLinkLocal`
判断地址是否属于 IPv4 `169.254/16` 或 IPv6 `fe80::/10`。

```c
bool xrtNetAddrIsLinkLocal(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 链路本地地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · Scope 端点解析后判定

```c
xrtNetAddrIsLinkLocal(&Addr) ? "yes" : "no");
```

### `xrtNetAddrIsPrivate`
判断地址是否属于 RFC 1918 IPv4 或 RFC 4193 IPv6 私有范围。

```c
bool xrtNetAddrIsPrivate(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只含私有段——不把回环、链路本地、文档地址混入；按安全策略组合多个明确谓词 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 私有范围地址 |
| `false` | 公网或其他范围 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `192.168/10` 命中、`8.8.8.8` 不命中

```c
!xrtNetAddrParse(&Other, "192.168.1.1", 0u) ||
	!xrtNetAddrIsPrivate(&Other) ||
	!xrtNetAddrIsPrivate(&Private) ||
```

### `xrtNetAddrIsMapped`
判断 IPv6 地址是否为 `::ffff:0:0/96` IPv4 映射地址。

```c
bool xrtNetAddrIsMapped(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv4 地址恒为假 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | IPv4 映射 IPv6 |
| `false` | 普通地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `::ffff:192.168.0.1` 命中

```c
if ( !xrtNetAddrParse(&Mapped, "::ffff:192.168.0.1", 443u) ||
	!xrtNetAddrIsMapped(&Mapped) ||
```

### `xrtNetAddrUnmap`
把 IPv4 映射 IPv6 地址转换为 IPv4；其他地址原样复制（保留端口）。

```c
bool xrtNetAddrUnmap(const xnetaddr* pAddr, xnetaddr* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |
| `pResult` | 输出 | 非空 | 非映射地址原样复制，允许统一规范化不加分支 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（转换或原样） | — |
| `false` | 参数非法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 映射还原 + 非映射原样

```c
!xrtNetAddrUnmap(&Mapped, &Unmapped) ||
	!xrtNetAddrParse(&Other, "192.168.0.1", 443u) ||
	!xrtNetAddrEqual(&Unmapped, &Other) ||
```

## Native 逃生口

### `xrtNetAddrToNative`
转换为平台 `sockaddr`；空输出可查询所需大小。

```c
bool xrtNetAddrToNative(const xnetaddr* pAddr, void* pNative, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |
| `pNative` | 输出 | 允许空指针 | `NULL` 时只经 `*pSize` 返回所需 `sockaddr_in`/`sockaddr_in6` 大小 |
| `pSize` | 输入输出 | 非空 | 入参为容量、出参为实际大小；容量不足也会更新大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（或已报告大小） | — |
| `false` | 参数非法或容量不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_BUFFER` — 缓冲不足（`*pSize` 已更新为所需大小）

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 先查询后写出的两段式

```c
if ( !xrtNetAddrToNative(&Loopback, NULL, &iSize) ||
```

### `xrtNetAddrFromNative`
从平台 `sockaddr` 转换为稳定地址结构。

```c
bool xrtNetAddrFromNative(xnetaddr* pAddr, const void* pNative, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `pNative` | 输入 | 非空 | 平台 `sockaddr` |
| `iSize` | 输入 | — | 检查地址族与结构长度合法性 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已转换（端口与 IPv6 Scope 保留） | — |
| `false` | 族或长度非法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FAMILY` — 不支持的地址族
- `XNET_ERROR_FORMAT` — 结构长度与族不符

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · sockaddr 往返等价

```c
!xrtNetAddrToNative(&Loopback, arrSockaddr, &iSize) ||
	!xrtNetAddrFromNative(&Native, arrSockaddr, iSize) ||
	!xrtNetAddrEqual(&Native, &Loopback) ) {
```

主机名与服务名解析属于独立 DNS 模块，不塞进地址语法函数。这组 Native 接口是有意保留的底层扩展路径：自定义 Socket 选项、第三方事件循环和上层协议可以直接连接平台 API，不需要复制 XRT 内部实现，也不会迫使公开地址结构绑定平台头文件。

## 网络缓冲

### `xrtNetBufPoolConfigInit`
初始化默认尺寸类与有界缓存策略的池配置。

```c
void xrtNetBufPoolConfigInit(xnetbufpoolconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 填入 512/2048/8192/32768 尺寸类与 2 MiB 总缓存上限 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化；`pConfig == NULL` 是参数错误（经 `xrtGetError()` 可查） |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 池生命周期起点

```c
xrtNetBufPoolConfigInit(&Config);
pPool = xrtNetBufPoolCreate(&Config);
```

### `xrtNetBufPoolCreate`
创建一个缓冲池；空配置使用默认值。

```c
xnetbufpool* xrtNetBufPoolCreate(const xnetbufpoolconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空指针 | `NULL` 使用默认配置；配置内容在创建期间复制 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新池，配对 `xrtNetBufPoolDestroy()` 释放 | — |
| `NULL` | 分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 分配失败 — 池结构内存申请失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 默认配置建池并挂两个缓冲

```c
if ( (pPool == NULL) ||
	!xrtNetBufInit(&BufA, pPool) ||
	!xrtNetBufInit(&BufB, pPool) ) {
```

### `xrtNetBufPoolDestroy`
销毁已无实时块的池；仍有外借块时失败并保留池。

```c
bool xrtNetBufPoolDestroy(xnetbufpool* pPool);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 允许空指针 | 空指针是成功空操作；块即使被 `Move` 到别的缓冲也仍计入原池实时统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 池已释放（先 `Trim` 清空缓存） | — |
| `false` | 仍有实时块 | 池保持完整、块中无悬空指针；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_POOL_BUSY` — `LiveBlocks != 0`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 失败路径的销毁收尾

```c
if ( pPool != NULL ) {
	xrtNetBufPoolDestroy(pPool);
}
```

### `xrtNetBufPoolTrim`
把缓存裁剪到不超过指定字节数，返回真正释放的块数。

```c
size_t xrtNetBufPoolTrim(xnetbufpool* pPool, size_t iRetainBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 只释放缓存块，不影响实时数据 |
| `iRetainBytes` | 输入 | — | 保留的缓存预算；`0` 清空全部缓存 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 释放块数 | 可能大于等于 0 | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pPool == NULL`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 清空全部缓存

```c
iGot = xrtNetBufPoolTrim(pPool, 0u);
if ( iGot < 1u ) {
```

### `xrtNetBufPoolGet`
复制缓冲池当前统计，不分配内存。

```c
void xrtNetBufPoolGet(const xnetbufpool* pPool, xnetbufpoolinfo* pInfo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | — |
| `pInfo` | 输出 | 非空 | 实时/峰值块数与容量、缓存块数与容量、分配/复用/动态大块/外部引用计数；`LiveBytes` 对拥有块统计容量、对引用块统计逻辑长度 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 参数非法时不修改输出（经 `xrtGetError()` 可查） |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 空池统计全零

```c
xrtNetBufPoolGet(pPool, &Info);
if ( (Info.LiveBlocks != 0u) ||
	(Info.AllocCount != 0u) ) {
```

### `xrtNetBufInit`
初始化空缓冲链；池为空时使用全局分配器且不缓存。

```c
bool xrtNetBufInit(xnetbuf* pBuffer, xnetbufpool* pPool);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输出 | 非空 | 可栈上或嵌入连接对象；结构可继续 `Clear` 复用 |
| `pPool` | 输入 | 允许空指针 | 空池适合跨线程所有权和低频独立使用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化为空链 | — |
| `false` | 参数非法 | 结构不被触碰；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer == NULL`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 挂到所属池

```c
!xrtNetBufInit(&BufA, pPool) ||
```

### `xrtNetBufClear`
释放全部块并放弃尚未提交的写入预留。

```c
void xrtNetBufClear(xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 允许空指针 | 幂等；引用块的释放过程在此时执行 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · Clear 触发 AppendRef 释放回调

```c
xrtNetBufClear(&BufA);
xrtNetBufClear(&BufB);
if ( iReleased != 1 ) {
```

### `xrtNetBufSize`
返回缓冲链总字节数。

```c
size_t xrtNetBufSize(const xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 字节数 | 跨全部块的活动数据总量 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 四类追加后恰 11 字节

```c
iSize = xrtNetBufSize(&BufA);
```

### `xrtNetBufEmpty`
返回缓冲链是否为空。

```c
bool xrtNetBufEmpty(const xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 无活动数据 |
| `false` | 至少一个字节 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · Move 后源为空

```c
!xrtNetBufMove(&BufA, &BufB) ||
	!xrtNetBufEmpty(&BufB) ||
```

### `xrtNetBufSpanCount`
返回当前缓冲链的只读 Span 总数。

```c
size_t xrtNetBufSpanCount(const xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| Span 数 | 完整消费所需的数组容量 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 四类追加后至少 4 段

```c
iSpans = xrtNetBufSpanCount(&BufA);
```

### `xrtNetBufSpans`
借用最多指定数量的只读 Span。

```c
size_t xrtNetBufSpans(const xnetbuf* pBuffer, xnetspan* pSpans, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |
| `pSpans` | 输出 | `iCapacity > 0` 时非空 | 接收 Span 数组；Span 借用块内数据 |
| `iCapacity` | 输入 | — | 数组容量；完整数量由 `SpanCount` 查询 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际写入数 | `<= iCapacity` | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer == NULL` 或 `iCapacity > 0 && pSpans == NULL`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 首段恰 "hello" 5 字节

```c
(xrtNetBufSpans(&BufA, Spans, 8u) != iSpans) ||
	(Spans[0].Size != 5u) ||
```

### `xrtNetBufFront`
借用明文队列的第一个连续 Span；空缓冲返回假。

```c
bool xrtNetBufFront(const xnetbuf* pBuffer, xnetspan* pSpan);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |
| `pSpan` | 输出 | 非空 | 空缓冲时被清为 `{ NULL, 0 }` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 首段非空 | — |
| `false` | 缓冲为空或参数非法 | 空缓冲属正常路径不设错；参数非法设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[network/buffer · 基础范例](../../examples/network/buffer/main.c) · Reserve→Commit 后取首段

```c
if ( !xrtNetBufCommit(&Buffer, 6) ||
	!xrtNetBufFront(&Buffer, &Read) ) {
```

### `xrtNetBufAppend`
复制一段数据到链尾；整个追加失败原子。

```c
bool xrtNetBufAppend(xnetbuf* pBuffer, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 成功后 `Size += iSize` |
| `pData` | 输入 | `iSize > 0` 时非空 | 来源借用调用期间 |
| `iSize` | 输入 | — | 字节数；`0` 为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制追加 | — |
| `false` | 参数非法或分配失败 | 原数据不变（OOM 不留部分数据）；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出/分配失败 — 容量或总长字节数溢出、块分配失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 四类追加的第一段

```c
if ( !xrtNetBufAppend(&BufA, "hello", 5u) ||
	!xrtNetBufAppendBorrow(&BufA, arrBorrow, 2u) ) {
```

### `xrtNetBufAppendBorrow`
追加借用数据；调用方保证数据存活到该段被消费或清除。

```c
bool xrtNetBufAppendBorrow(xnetbuf* pBuffer, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | **借用**——不复制，存活期由调用方保证 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已借用追加（零复制） | — |
| `false` | 参数非法或登记失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出 — 总长溢出

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 与 Append 连用

```c
!xrtNetBufAppendBorrow(&BufA, arrBorrow, 2u) ) {
```

### `xrtNetBufAppendTake`
接管由 `xrtMalloc` 家族分配的数据；成功后由缓冲最终 `xrtFree`。

```c
bool xrtNetBufAppendTake(xnetbuf* pBuffer, ptr pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | **接管**——必须来自 `xrtMalloc` 家族；失败时所有权仍归调用方 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管追加 | — |
| `false` | 参数非法或登记失败 | 数据仍归调用方释放；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出 — 总长溢出

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 失败路径自行释放

```c
if ( !xrtNetBufAppendTake(&BufA, pTaken, 2u) ) {
	xrtFree(pTaken);
```

### `xrtNetBufAppendRef`
接管带自定义释放过程的外部数据。

```c
bool xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | **接管**；失败时不会调用释放过程 |
| `iSize` | 输入 | — | 字节数 |
| `pRelease` | 输入 | 非空 | 释放过程；最后一部分离开缓冲（消费完或 `Clear`）时恰好执行一次 |
| `pContext` | 输入 | — | 释放过程的用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管追加 | — |
| `false` | 参数非法 | 数据与释放责任仍归调用方；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 释放回调恰好一次（Clear 时触发）

```c
if ( !xrtNetBufAppendRef(&BufA, arrRef, 2u,
		exampleRelease, (ptr)&iReleased) ) {
```

### `xrtNetBufPrepend`
把一段数据复制到新首块；不移动已有负载块。

```c
bool xrtNetBufPrepend(xnetbuf* pBuffer, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 原有块和外部引用保持不动——适合加协议头 |
| `pData` | 输入 | `iSize > 0` 时非空 | 头内容 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已加首块 | — |
| `false` | 参数非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出/分配失败 — 溢出或首块分配失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 加 ">> " 前缀

```c
if ( !xrtNetBufPrepend(&BufA, ">> ", 3u) ||
	(xrtNetBufSize(&BufA) != 14u) ||
```

### `xrtNetBufReserve`
预留至少指定大小的连续尾部可写区。

```c
bool xrtNetBufReserve(xnetbuf* pBuffer, size_t iMinimum, xnetwspan* pSpan);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 预留期间不能执行其他改变链结构的操作 |
| `iMinimum` | 输入 | — | 最小连续字节数；IOCP/io_uring/`recv`/TLS 解密器可直接写入 |
| `pSpan` | 输出 | 非空 | 可写 Span；完成后 `Commit` 实际字节数、`Cancel` 放弃 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已预留并借出可写区 | — |
| `false` | 参数/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 已有在途预留（重复预留）
- 溢出/分配失败 — 尾块扩容失败

#### 范例

[network/buffer · 基础范例](../../examples/network/buffer/main.c) · 直接写入式接收

```c
if ( (pPool == NULL) || !xrtNetBufInit(&Buffer, pPool) ||
	!xrtNetBufReserve(&Buffer, 6, &Write) ) {
```

### `xrtNetBufCommit`
提交预留空间中已经写入的字节数。

```c
bool xrtNetBufCommit(xnetbuf* pBuffer, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 提交后预留结束，可再次 `Reserve` |
| `iSize` | 输入 | `<=` 预留容量 | 实际写入字节数；`0` 合法（丢弃预留区） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已计入缓冲 | — |
| `false` | 无预留、超量或溢出 | 预留仍可正确提交或取消；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 没有在途预留
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — `iSize` 超过预留容量
- 溢出 — 总长溢出

#### 范例

[network/buffer · 基础范例](../../examples/network/buffer/main.c) · 写入 6 字节后提交

```c
memcpy(Write.Data, "packet", 6);
if ( !xrtNetBufCommit(&Buffer, 6) ||
```

### `xrtNetBufCancel`
放弃当前写入预留，缓冲内容保持不变。

```c
bool xrtNetBufCancel(xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | EAGAIN、取消或零字节结果用本接口收尾 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 预留已放弃（新块释放、登记复原） | — |
| `false` | 没有在途预留 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 无预留可放弃

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · Cancel 后总长不变

```c
if ( !xrtNetBufReserve(&BufA, 8u, &Reserve) ||
	!xrtNetBufCancel(&BufA) ||
```

### `xrtNetBufMove`
把源缓冲的全部块移动到目标尾部，源恢复为空但保留池配置。

```c
bool xrtNetBufMove(xnetbuf* pTarget, xnetbuf* pSource);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入输出 | 非空 | 接收全部块；只重连块链不复制负载 |
| `pSource` | 输出 | 非空 | 恢复为空；AGAIN 或失败时源缓冲保持不变 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已转移 | — |
| `false` | 参数非法（如自移） | 双方不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针或 `pTarget == pSource`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 块链零复制转移

```c
if ( !xrtNetBufAppend(&BufB, "!", 1u) ||
	!xrtNetBufMove(&BufA, &BufB) ||
```

### `xrtNetBufPullup`
确保指定长度的内存前缀连续。

```c
bool xrtNetBufPullup(xnetbuf* pBuffer, size_t iSize, xnetspan* pSpan);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 首块足够时零复制返回，否则只复制指定前缀到首块——适合解析固定协议头 |
| `iSize` | 输入 | `<= Size` | 需要连续的前缀字节数 |
| `pSpan` | 输出 | 非空 | 连续前缀 Span；`iSize == 0` 输出空 Span 且成功 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 前缀已连续并借出 | — |
| `false` | 参数/状态非法或前缀超长 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — `iSize` 超过缓冲现有数据
- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 预留期间操作
- 溢出/分配失败 — 首块扩容失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 拼合 5 字节协议头

```c
!xrtNetBufPullup(&BufA, 5u, &Span) ||
	(Span.Size < 5u) ||
```

### `xrtNetBufPeek`
从指定偏移复制最多给定字节；不消费。

```c
size_t xrtNetBufPeek(const xnetbuf* pBuffer, size_t iOffset, void* pOutput, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |
| `iOffset` | 输入 | — | 起始偏移；遇到块边界外的引用段语义见实现（跨块复制、引用块按逻辑长度） |
| `pOutput` | 输出 | `iSize > 0` 时非空 | 接收副本 |
| `iSize` | 输入 | — | 最多复制字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际复制数 | 从 `iOffset` 起的可用前缀 | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 偏移 8 读 3 字节

```c
(xrtNetBufPeek(&BufA, 8u, arrText, 3u) != 3u) ||
```

### `xrtNetBufRead`
复制并消费：从链首取走最多给定字节。

```c
size_t xrtNetBufRead(xnetbuf* pBuffer, void* pOutput, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 已消费部分脱离活动区 |
| `pOutput` | 输出 | `iSize > 0` 时非空 | 接收副本 |
| `iSize` | 输入 | — | 请求字节数；可用不足时短读 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际读取数 | 短读合法 | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[network/tcp · 基础范例](../../examples/network/tcp/main.c) · 接收队列整段取出

```c
iSize = xrtNetBufRead(pBuffer, Data, sizeof(Data));
```

### `xrtNetBufFind`
从指定偏移查找一个字节，未找到返回 `XRT_NPOS`。

```c
size_t xrtNetBufFind(const xnetbuf* pBuffer, uint8 iByte, size_t iOffset);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | 跨块查找 |
| `iByte` | 输入 | — | 目标字节 |
| `iOffset` | 输入 | — | 起始偏移 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 偏移 | 第一个命中位置 |
| `XRT_NPOS` | 未命中 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 命中 'w' 与未命中 'z'

```c
(xrtNetBufFind(&BufA, 'w', 0u) != 9u) ||
	(xrtNetBufFind(&BufA, 'z', 0u) != XRT_NPOS) ||
```

### `xrtNetBufConsume`
消费最多给定字节的前缀。

```c
size_t xrtNetBufConsume(xnetbuf* pBuffer, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 不会释放仍被活动写预留借用的尾块；引用块消费到最后一段时执行释放 |
| `iSize` | 输入 | — | 请求量；允许超过剩余数据（按剩余消费） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际消费量 | `<= iSize` 且 `<= Size` | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 消费 ">> " 三字节

```c
(xrtNetBufConsume(&BufA, 3u) != 3u) ||
```

## 拥有型字节结果

### `xrtNetBytesRef`
增加拥有型网络字节结果的引用并返回原指针。

```c
xnetbytes* xrtNetBytesRef(xnetbytes* pBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBytes` | 输入 | 非空 | 接收结果（如 `xrtNetStreamRecv`）的拥有式对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 原指针 | 引用已增加；每次成功须一次 `xrtNetBytesDestroy` 配对 | — |
| `NULL` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBytes == NULL`

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 共享持有接收结果

```c
xnetbytes* pShared = xrtNetBytesRef(pBytes);
```

### `xrtNetBytesDestroy`
释放拥有型网络字节结果；空指针视为空操作。

```c
void xrtNetBytesDestroy(xnetbytes* pBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBytes` | 输入 | 允许空指针 | 最后一个引用释放时对象销毁 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 视图取出后释放

```c
xrtNetBytesDestroy(pBytes);
```

### `xrtNetBytesView`
返回拥有型网络字节结果的借用视图。

```c
xbytesview xrtNetBytesView(const xnetbytes* pBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBytes` | 输入 | 允许空指针 | 空指针返回空视图 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 视图 | 数据与长度的只读借用；对象销毁后失效 |

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 与期望输出逐位核对

```c
xbytesview View = xrtNetBytesView(pBytes);
```

## 地址列表与 DNS

### `xrtNetAddrListCreate`
复制、校验并去重调用方地址，建立不可变列表。

```c
xnetaddrlist* xrtNetAddrListCreate(const xnetaddr* pAddresses, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddresses` | 输入 | `iCount > 0` 时非空 | 来源数组在创建期间复制；重复项去除 |
| `iCount` | 输入 | — | 地址数；`0` 建空列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新列表，配对 `xrtNetAddrListDestroy()` 释放 | — |
| `NULL` | 参数非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iCount > 0 && pAddresses == NULL`
- 分配失败 — 列表内存申请失败

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 双地址建表

```c
pList = xrtNetAddrListCreate(arrTwo, 2u);
if ( (pList == NULL) ||
	(xrtNetAddrListCount(pList) != 2u) ) {
```

### `xrtNetAddrListWithPort`
复制列表并统一替换端口；端口已一致时只增加引用。

```c
xnetaddrlist* xrtNetAddrListWithPort(xnetaddrlist* pList, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 非空 | 原列表不受影响 |
| `iPort` | 输入 | — | 新端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新列表（或原列表的引用）；独立 `Destroy` 配对 | — |
| `NULL` | 参数非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pList == NULL`
- 分配失败 — 新列表申请失败

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 两地址统一换 443

```c
pPortList = xrtNetAddrListWithPort(pList, 443u);
if ( (pPortList == NULL) ||
	(xrtNetAddrListCount(pPortList) != 2u) ||
	(xrtNetAddrListGet(pPortList, 0u)->Port != 443u) ||
```

### `xrtNetAddrListRef`
增加不可变列表引用并返回原指针。

```c
xnetaddrlist* xrtNetAddrListRef(xnetaddrlist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 非空 | 对象内容在全部引用之间只读 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 原指针 | 引用已增加；每次成功须一次 `Destroy` 配对 | — |
| `NULL` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pList == NULL`

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 共享引用

```c
pRef = xrtNetAddrListRef(pPortList);
if ( pRef != pPortList ) {
```

### `xrtNetAddrListDestroy`
释放列表引用；空指针是空操作。

```c
void xrtNetAddrListDestroy(xnetaddrlist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 允许空指针 | 最后一个引用释放时对象销毁 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 引用与原件各自配对释放

```c
xrtNetAddrListDestroy(pRef);
xrtNetAddrListDestroy(pPortList);
```

### `xrtNetAddrListCount`
返回地址数量；空列表返回零。

```c
size_t xrtNetAddrListCount(const xnetaddrlist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 允许空指针 | 空指针返回 0 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 数量 | 列表内地址数 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 见 `xrtNetAddrListCreate` 范例

```c
(xrtNetAddrListCount(pList) != 2u) ) {
```

### `xrtNetAddrListGet`
返回借用地址；索引越界返回空指针并设置范围错误。

```c
const xnetaddr* xrtNetAddrListGet(const xnetaddrlist* pList, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 非空 | — |
| `iIndex` | 输入 | `< Count` | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用地址；视图随列表最后一个引用失效 | — |
| `NULL` | 越界或参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_ARGUMENT` — `pList == NULL`

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 读换端口后的首地址

```c
(xrtNetAddrListGet(pPortList, 0u)->Port != 443u) ||
```

### `xrtNetLookup`
解析主机的全部地址；保留系统顺序、去重、端口为零。

```c
xnetaddrlist* xrtNetLookup(cstr sHost, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sHost` | 输入 | 非空 | 主机名或数字地址 |
| `Family` | 输入 | — | 限定族；`UNSPEC` 接受全部 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只查询主机地址的列表（端口全零），配对 `Destroy` 释放 | — |
| `NULL` | 解析失败 | 错误经 `xrtGetError()` 报告（`XNET_ERROR_DNS_*` 族） |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_DNS_RESOLVE` 等域名解析错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · localhost 本机解析不依赖外网

```c
xnetaddrlist* pLocal = xrtNetLookup("localhost",
	XNET_FAMILY_IPV4);
```

### `xrtNetResolve`
解析主机并统一端口。

```c
xnetaddrlist* xrtNetResolve(cstr sHost, uint16 iPort, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sHost` | 输入 | 非空 | — |
| `iPort` | 输入 | — | 写入全部结果的端口 |
| `Family` | 输入 | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 带端口的列表，配对 `Destroy` 释放 | — |
| `NULL` | 解析失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetLookup`

#### 范例

[network/dns · 基础范例](../../examples/network/dns/main.c) · 完整列表遍历

```c
xnetaddrlist* pList = xrtNetResolve(
	"localhost",
	443,
	XNET_FAMILY_UNSPEC
);
```

### `xrtNetResolveOne`
解析并复制系统顺序中的第一个地址；单地址场景无需管理列表。

```c
bool xrtNetResolveOne(xnetaddr* pAddr, cstr sHost, uint16 iPort, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sHost` | 输入 | 非空 | — |
| `iPort` | 输入 | — | 写入端口 |
| `Family` | 输入 | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制首地址 | — |
| `false` | 解析失败 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetLookup`

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 端口 80 的单地址

```c
if ( !xrtNetResolveOne(&Resolved, "localhost", 80u,
		XNET_FAMILY_IPV4) ||
```

### `xrtNetReverse`
反向解析一个数字地址；成功返回调用方拥有的主机名。

```c
str xrtNetReverse(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只接受数字地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式主机名，`xrtFree` 释放 | — |
| `NULL` | 反查失败或参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_DNS_REVERSE` — 反查失败

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 回环地址反查

```c
sHost = xrtNetReverse(&Loopback);
if ( sHost == NULL ) {
```

## Socket 原语

### `xrtNetSocketOpen`
打开一个流式或数据报 Socket。

```c
xnetsocket xrtNetSocketOpen(xnetfamily Family, xnetsockettype Type, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Family` | 输入 | `IPV4` 或 `IPV6` | — |
| `Type` | 输入 | `STREAM` 或 `DGRAM` | — |
| `iFlags` | 输入 | `XNET_SOCKET_NONBLOCK` 或 `0` | 非阻塞从创建即生效 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 成功返回的对象拥有原生句柄；配对 `xrtNetSocketClose()` 释放 | — |
| `NULL` | 参数非法或系统创建失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 族/类型非法
- `XNET_ERROR_SOCKET_OPEN`（系统错误）— 平台 `socket()` 失败，保留 `SystemCode`

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 双 UDP 套接字

```c
A = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0u);
B = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0u);
```

### `xrtNetSocketClose`
关闭原生句柄并销毁对象；即使系统关闭失败，对象也立即失效。

```c
bool xrtNetSocketClose(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 允许空指针（0） | 空指针是空操作；关闭后句柄不可再用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 系统关闭成功或对象为空 | — |
| `false` | 系统关闭失败 | **对象仍已销毁**；错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_CLOSE`（系统错误）— `closesocket()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 收尾统一关闭

```c
xrtNetSocketClose(C);
xrtNetSocketClose(L);
```

### `xrtNetSocketNative`
返回借用的原生句柄，调用方不得自行关闭。

```c
intptr_t xrtNetSocketNative(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 句柄值 | 平台 SOCKET/fd；所有权仍在 XRT 对象 |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 创建后即验证非零

```c
(xrtNetSocketNative(A) == 0) ||
```

### `xrtNetSocketFamily`
返回 Socket 创建时确定的地址族。

```c
xnetfamily xrtNetSocketFamily(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 族枚举 | 与 `Open` 传入值一致 |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 创建后核对

```c
(xrtNetSocketFamily(A) != XNET_FAMILY_IPV4) ||
```

### `xrtNetSocketType`
返回 Socket 创建时确定的类型。

```c
xnetsockettype xrtNetSocketType(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 类型枚举 | 与 `Open` 传入值一致 |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 Family 成对核对

```c
(xrtNetSocketType(A) != XNET_SOCKET_DGRAM) ||
```

### `xrtNetSocketSet`
设置一个通用 Socket 选项。

```c
bool xrtNetSocketSet(xnetsocket Socket, xnetoption Option, int64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `Option` | 输入 | 见 `xnetoption` 枚举 | 部分选项仅特定平台支持 |
| `iValue` | 输入 | — | 选项值；`NONBLOCK` 用 0/1 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已生效 | — |
| `false` | 参数非法、不支持或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空对象或非法选项
- `XERR_UNSUPPORTED` — 平台不支持该选项
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `setsockopt()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 接收缓冲与逐字节读回

```c
!xrtNetSocketSet(A, XNET_OPTION_RECEIVE_BUFFER, 65536) ||
	!xrtNetSocketGet(A, XNET_OPTION_RECEIVE_BUFFER, &iValue) ||
```

### `xrtNetSocketGet`
查询一个通用 Socket 选项。

```c
bool xrtNetSocketGet(xnetsocket Socket, xnetoption Option, int64* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `Option` | 输入 | — | — |
| `pValue` | 输出 | 非空 | 接收选项当前值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 参数非法、不支持或系统失败 | `*pValue` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针或非法选项
- `XERR_UNSUPPORTED` — 平台不支持
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `getsockopt()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 见 `xrtNetSocketSet` 范例

```c
!xrtNetSocketGet(A, XNET_OPTION_RECEIVE_BUFFER, &iValue) ||
	(iValue <= 0) ||
```

### `xrtNetSocketAvailable`
查询当前可立即读取的字节数；成功才修改输出。

```c
bool xrtNetSocketAvailable(xnetsocket Socket, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSize` | 输出 | 非空 | 空接收队列上成功返回 0 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出可读量 | — |
| `false` | 参数非法或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_READ`（系统错误）— `ioctlsocket`/`ioctl` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 空队列上恰为 0

```c
!xrtNetSocketAvailable(A, &iGot) ||
	(iGot != 0u) ) {
```

### `xrtNetSocketBind`
把 Socket 绑定到本地地址；端口为零时由系统分配。

```c
bool xrtNetSocketBind(xnetsocket Socket, const xnetaddr* pAddress);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pAddress` | 输入 | 非空 | 通配地址绑定全部接口；实际端口由 `Local` 读回 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已绑定 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_BIND`（系统错误）— `bind()` 失败（端口占用等）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 通配绑定后 Local 读回实际端口

```c
!xrtNetSocketBind(A, &AddrA) ||
	!xrtNetSocketLocal(A, &AddrA) ||
```

### `xrtNetSocketListen`
把已绑定的流式 Socket 转为监听状态。

```c
bool xrtNetSocketListen(xnetsocket Socket, int iBacklog);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 必须已 `Bind` 且为 `STREAM` |
| `iBacklog` | 输入 | `> 0` | 等待连接队列长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已监听 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_LISTEN`（系统错误）— `listen()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 绑定→监听→接受

```c
!xrtNetSocketListen(Listener, 16) ||
```

### `xrtNetSocketAccept`
接受一个连接；非阻塞 Socket 暂无连接时返回 `AGAIN`。

```c
xnetresult xrtNetSocketAccept(xnetsocket Socket, xnetsocket* pClient, xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 监听中的流式 Socket |
| `pClient` | 输出 | 非空 | 接受成功时获得新建 Socket（调用方拥有） |
| `pRemote` | 输出 | 非空 | 接受成功时获得对端地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `OK` | 已接受，`*pClient`/`*pRemote` 已写出 | — |
| `AGAIN` | 非阻塞暂无连接 | 输出不被修改 |
| `ERROR` | 参数或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_ACCEPT`（系统错误）— `accept()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 连接后立即接受

```c
(xrtNetSocketAccept(Listener,
	&Accepted, &Remote) != XNET_RESULT_OK) ||
```

### `xrtNetSocketConnect`
发起连接；非阻塞连接尚未完成时返回 `AGAIN`。

```c
xnetresult xrtNetSocketConnect(xnetsocket Socket, const xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pRemote` | 输入 | 非空 | 目标地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `OK` | 连接建立（含环回立即完成） | — |
| `AGAIN` | 非阻塞连接在途 | **不得二次调用**，交给 `FinishConnect` 轮询收口 |
| `ERROR` | 系统拒绝或失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_CONNECT`（系统错误）— `connect()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · OK/AGAIN 双路收口

```c
xnetresult ConnResult = xrtNetSocketConnect(C, &AddrA);

if ( (ConnResult != XNET_RESULT_OK) &&
	(ConnResult != XNET_RESULT_AGAIN) ) {
```

### `xrtNetSocketFinishConnect`
在可写事件到达后读取 `SO_ERROR`，完成非阻塞连接判定。

```c
xnetresult xrtNetSocketFinishConnect(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 处于在途连接的 Socket |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | 连接已建立 |
| `AGAIN` | 仍在途，继续轮询 |
| `ERROR` | 连接被拒或失败（`SO_ERROR`） |

#### 错误

- `XERR_ARGUMENT` — 空对象
- `XNET_ERROR_SOCKET_CONNECT`（系统错误）— 远端拒绝等

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 2000ms 轮询收口

```c
Result = xrtNetSocketFinishConnect(Socket);
```

### `xrtNetSocketShutdown`
半关闭指定方向，不销毁 Socket 对象。

```c
bool xrtNetSocketShutdown(xnetsocket Socket, xnetshutdown Direction);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `Direction` | 输入 | `READ`/`WRITE`/`BOTH` | `WRITE` 后对端读到 EOF |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭指定方向 | — |
| `false` | 系统失败 | 对象仍可用；错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_SHUTDOWN`（系统错误）— `shutdown()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 客户端写半关

```c
if ( !xrtNetSocketShutdown(Client, XNET_SHUTDOWN_WRITE) ) {
```

### `xrtNetSocketLocal`
查询实际本地地址。

```c
bool xrtNetSocketLocal(xnetsocket Socket, xnetaddr* pAddress);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pAddress` | 输出 | 非空 | 接收本地地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 系统失败 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_NATIVE`（系统错误）— `getsockname()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 绑定后读回实际端口

```c
!xrtNetSocketLocal(A, &AddrA) ||
	(AddrA.Port == 0u) ||
```

### `xrtNetSocketRemote`
查询已连接的对端地址。

```c
bool xrtNetSocketRemote(xnetsocket Socket, xnetaddr* pAddress);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 须为已连接 Socket |
| `pAddress` | 输出 | 非空 | 接收对端地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 未连接或系统失败 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_NATIVE`（系统错误）— `getpeername()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 连接后与目标地址全等

```c
!xrtNetSocketRemote(A, &Remote) ||
	!xrtNetAddrEqual(&Remote, &DestB) ) {
```

### `xrtNetSocketSend`
单次发送；允许成功短写，非阻塞无法推进时返回 `AGAIN`。

```c
xnetresult xrtNetSocketSend(xnetsocket Socket, const void* pData, size_t iSize, size_t* pSent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | 发送内容 |
| `iSize` | 输入 | — | 字节数 |
| `pSent` | 输出 | 非空 | 成功时接收实际发送量（可短写） |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | `*pSent` 字节已受理 |
| `AGAIN` | 非阻塞暂无法推进 |
| `ERROR` | 系统失败 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_WRITE`（系统错误）— `send()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 连接后即发送

```c
(xrtNetSocketSend(Client, "hello", 5,
	&iSize) != XNET_RESULT_OK) ||
```

### `xrtNetSocketRecv`
单次接收；流式 EOF 返回 `CLOSED`，非阻塞无数据返回 `AGAIN`。

```c
xnetresult xrtNetSocketRecv(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | `iSize > 0` 时非空 | 接收缓冲 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 成功时接收实际读取量 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | `*pReceived` 字节可用 |
| `AGAIN` | 非阻塞暂无数据 |
| `CLOSED` | 流式对端已 EOF（`*pReceived` 为 0） |
| `ERROR` | 系统失败 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_READ`（系统错误）— `recv()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 服务端读取五字节

```c
(xrtNetSocketRecv(Accepted, sData, sizeof(sData) - 1,
	&iSize) != XNET_RESULT_OK) ) {
```

### `xrtNetSocketSendVec`
单次聚集发送；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketSendVec(xnetsocket Socket, const xnetspan* pSpans, size_t iCount, size_t* pSent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 连接式 |
| `pSpans` | 输入 | `iCount > 0` 时非空 | 只读 Span 数组 |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pSent` | 输出 | 非空 | 实际发送量 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `Send` |

#### 错误

- `XERR_ARGUMENT` — 空指针或 Span 数超限
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 两段 "xy"+"z" 三字节

```c
(xrtNetSocketSendVec(A, Out, 2u, &iSent) != XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvVec`
单次分散接收；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketRecvVec(xnetsocket Socket, xnetwspan* pSpans, size_t iCount, size_t* pReceived);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 连接式 |
| `pSpans` | 输出 | `iCount > 0` 时非空 | 可写 Span 数组 |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pReceived` | 输出 | 非空 | 实际读取量 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `CLOSED` / `ERROR` | 同 `Recv` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendVec 配对

```c
(xrtNetSocketRecvVec(B, In, 2u, &iGot) != XNET_RESULT_OK) ||
```

### `xrtNetSocketSendTo`
单次发送数据报；允许发送零长度数据报。

```c
xnetresult xrtNetSocketSendTo(xnetsocket Socket, const void* pData, size_t iSize, size_t* pSent, const xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输入 | 允许空指针（零长度报文） | 发送内容 |
| `iSize` | 输入 | — | 字节数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `Send` |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 消息形态三字节

```c
(xrtNetSocketSendTo(A, "msg", 3u, &iSent, &DestB) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvFrom`
单次接收数据报；零长度返回 `OK`，缓冲不足返回 `TRUNCATED`。

```c
xnetresult xrtNetSocketRecvFrom(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | `iSize > 0` 时非空 | 接收缓冲 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | `*pReceived` 字节可用（可为 0） |
| `AGAIN` | 非阻塞暂无数据 |
| `TRUNCATED` | 报文超过缓冲容量（余量被截去） |
| `ERROR` | 系统失败 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 多播自收一包

```c
McResult = xrtNetSocketRecvFrom(B, arrBuf, 8u, &iGot,
	&From);
```

### `xrtNetSocketSendToVec`
单次聚集发送数据报；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketSendToVec(xnetsocket Socket, const xnetspan* pSpans, size_t iCount, size_t* pSent, const xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输入 | `iCount > 0` 时非空 | 聚集 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `SendTo` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 两段 "ab"+"cd" 四字节

```c
if ( (xrtNetSocketSendToVec(A, Out, 2u, &iSent, &DestB) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvFromVec`
单次分散接收数据报；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketRecvFromVec(xnetsocket Socket, xnetwspan* pSpans, size_t iCount, size_t* pReceived, xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输出 | `iCount > 0` 时非空 | 分散 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `TRUNCATED` / `ERROR` | 同 `RecvFrom` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendToVec 四字节配对

```c
(xrtNetSocketRecvFromVec(B, In, 2u, &iGot, &From) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketSendMsg`
发送数据报并覆盖本包源地址、接口、Hop Limit 或 Traffic Class。

```c
xnetresult xrtNetSocketSendMsg(xnetsocket Socket, const void* pData, size_t iSize, size_t* pSent, const xnetaddr* pRemote, const xnetdgramcontrol* pControl);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输入 | 允许空指针 | 发送内容 |
| `iSize` | 输入 | — | 字节数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |
| `pControl` | 输入 | 允许空指针 | 逐包控制；空表示无覆盖 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `SendTo` |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 多播自收一包（SendTo 同型路径）

```c
McResult = xrtNetSocketSendTo(A, "m", 1u, &iSent, &Group);
```

### `xrtNetSocketSendMsgVec`
聚集发送带逐包控制的数据报；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketSendMsgVec(xnetsocket Socket, const xnetspan* pSpans, size_t iCount, size_t* pSent, const xnetaddr* pRemote, const xnetdgramcontrol* pControl);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输入 | `iCount > 0` 时非空 | 聚集 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |
| `pControl` | 输入 | 允许空指针 | 逐包控制 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `SendTo` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 零控制双段四字节

```c
if ( (xrtNetSocketSendMsgVec(A, Out, 2u, &iSent, &DestB,
		&Control) != XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvMsg`
接收数据报及已启用的目标、接口、Hop Limit 和 Traffic Class 元数据。

```c
xnetresult xrtNetSocketRecvMsg(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote, xnetdgrammeta* pMeta);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | `iSize > 0` 时非空 | 接收缓冲 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |
| `pMeta` | 输出 | **非空** | 元数据结构；传 `NULL` 是参数错误——与 `RecvFrom` 的差异点 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `TRUNCATED` / `ERROR` | 同 `RecvFrom`；`Meta.Flags` 标记有效字段 |

#### 错误

- `XERR_ARGUMENT` — 空指针（含 `pMeta == NULL`）
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendTo 三字节配对

```c
(xrtNetSocketRecvMsg(B, arrBuf, 64u, &iGot, &From, &Meta) !=
		XNET_RESULT_OK) ||
	(iGot != 3u) ||
```

### `xrtNetSocketRecvMsgVec`
分散接收数据报及元数据，Span 数量不能超过 64。

```c
xnetresult xrtNetSocketRecvMsgVec(xnetsocket Socket, xnetwspan* pSpans, size_t iCount, size_t* pReceived, xnetaddr* pRemote, xnetdgrammeta* pMeta);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输出 | `iCount > 0` 时非空 | 分散 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |
| `pMeta` | 输出 | 非空 | 元数据结构 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `TRUNCATED` / `ERROR` | 同 `RecvFrom` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendMsgVec 四字节配对

```c
(xrtNetSocketRecvMsgVec(B, In, 2u, &iGot, &From, &Meta) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvBatch`
接收最多 64 个数据报；返回已消费前缀，每项独立记录 `OK` 或 `TRUNCATED`。

```c
xnetresult xrtNetSocketRecvBatch(xnetsocket Socket, xnetdgramrecv* pItems, size_t iCapacity, size_t* pReceived);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pItems` | 输出 | `iCapacity > 0` 时非空 | 调用方提供缓冲的接收项数组 |
| `iCapacity` | 输入 | `<= 64` | 最多接收数 |
| `pReceived` | 输出 | 非空 | 已到达前缀数（可能小于发送量） |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 每项自带 `Result`/`Size`/`Remote` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 两包批量含部分到达补收

```c
if ( (xrtNetSocketSendBatch(A, Send, 2u, &iSent) != XNET_RESULT_OK) ||
	(iSent != 2u) ||
	(xrtNetSocketRecvBatch(B, Recv, 2u, &iGot) != XNET_RESULT_OK) ) {
```

### `xrtNetSocketSendBatch`
发送最多 64 个数据报；返回已经完整发送的输入前缀。

```c
xnetresult xrtNetSocketSendBatch(xnetsocket Socket, const xnetdgramsend* pItems, size_t iCount, size_t* pSent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pItems` | 输入 | `iCount > 0` 时非空 | 调用期间借用数据 |
| `iCount` | 输入 | `<= 64` | 发送项数 |
| `pSent` | 输出 | 非空 | 已完整发送的项数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | `AGAIN` 表示队列暂满，稍后重试剩余 |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XERR_IO` + `XNET_ERROR_SOCKET_WRITE` — 某项系统失败
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 双项批量（连接式空远端）

```c
if ( (xrtNetSocketSendBatch(A, Send, 2u, &iSent) != XNET_RESULT_OK) ||
```

### `xrtNetSocketDgramMetaAvailable`
返回当前平台和地址族可能提供的数据报接收元数据位。

```c
uint32 xrtNetSocketDgramMetaAvailable(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 平台可能支持的 `XNET_DGRAM_META_*` 位组合 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— 探测失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 Enabled 成对使用（见 DgramMetaSet 范例）

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramMetaEnabled`
返回 Socket 当前已经启用的数据报接收元数据位。

```c
uint32 xrtNetSocketDgramMetaEnabled(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 已启用位（默认 0） |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 默认全零

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramMetaSet`
成功后精确设置接收元数据位。

```c
bool xrtNetSocketDgramMetaSet(xnetsocket Socket, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 数据报 Socket |
| `iFlags` | 输入 | 合法元数据位组合 | 期望启用的位集 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已生效；`Enabled` 反映实际位 | — |
| `false` | 参数非法、平台不支持或系统失败 | 可查询 `Enabled` 看实际生效状态；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空对象
- `XERR_RANGE` — 含非法元数据位
- `XERR_UNSUPPORTED` — 平台不支持全部请求位
- `XNET_ERROR_SOCKET_OPTION`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 启用 Hop Limit 后收包带 HopLimit

```c
if ( !xrtNetSocketDgramMetaSet(B, XNET_DGRAM_META_HOP_LIMIT) ||
```

### `xrtNetSocketDgramControlAvailable`
返回当前平台、地址族和 Socket Provider 可用的逐数据报发送控制位。

```c
uint32 xrtNetSocketDgramControlAvailable(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 可用的 `XNET_DGRAM_CONTROL_*` 位组合 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— 探测失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 DgramMetaAvailable 同族用法

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramCapabilities`
返回 PMTU、错误队列及后续高级数据报能力。

```c
uint32 xrtNetSocketDgramCapabilities(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 非数据报 Socket 返回零 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 数据报高级能力位；含 `XNET_OPTION_DGRAM_ERRORS` 可设性等 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— 探测失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 同族用法见 DgramMetaSet 范例

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramRecvError`
非阻塞读取一个已启用的异步数据报错误。

```c
xnetresult xrtNetSocketDgramRecvError(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived, xnetdgramerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | 允许空指针 | 错误附带的原始数据 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 附带数据量 |
| `pError` | 输出 | 非空 | 错误描述结构 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | 已读取一个排队错误 |
| `AGAIN` | 队列为空（Linux 上空队列返回本值） |
| `ERROR` | 平台不支持或失败（Windows 恒 `ERROR`："not supported on this platform"） |

#### 错误

- `XERR_UNSUPPORTED` — 平台无 `IP_RECVERR` 等价物

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 平台能力门控（Windows 不支持）

```c
xnetresult ErrResult = xrtNetSocketDgramRecvError(A,
	arrBuf, sizeof(arrBuf), &iGot, &DgramError);

if ( ErrResult == XNET_RESULT_OK ) {
```

### `xrtNetSocketMulticastJoin`
将数据报 Socket 加入一个同地址族多播组。

```c
bool xrtNetSocketMulticastJoin(xnetsocket Socket, const xnetaddr* pGroup, const xnetaddr* pInterface);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 数据报 Socket |
| `pGroup` | 输入 | 非空、同族多播地址 | 目标组 |
| `pInterface` | 输入 | 非空 | 指定出接口；IPv6 使用 Scope 作为接口索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已加入 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `setsockopt(IP_ADD_MEMBERSHIP)` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · Loop+Hop+Iface+Join 全指环回

```c
!xrtNetSocketMulticastJoin(B, &Group, &Iface) ) {
```

### `xrtNetSocketMulticastLeave`
将数据报 Socket 移出一个多播组。

```c
bool xrtNetSocketMulticastLeave(xnetsocket Socket, const xnetaddr* pGroup, const xnetaddr* pInterface);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pGroup` | 输入 | 非空 | 已加入的组 |
| `pInterface` | 输入 | 非空 | 与加入时相同的接口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已移出 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_DROP_MEMBERSHIP` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 自收验证后移出

```c
if ( !xrtNetSocketMulticastLeave(B, &Group, &Iface) ||
	!xrtNetSocketMulticastInterface(A, NULL) ) {
```

### `xrtNetSocketMulticastLoop`
设置数据报 Socket 是否接收自己发出的多播报文。

```c
bool xrtNetSocketMulticastLoop(xnetsocket Socket, bool bEnabled);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `bEnabled` | 输入 | — | 自收开关 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_MULTICAST_LOOP` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 自收前提

```c
!xrtNetSocketMulticastLoop(A, true) ||
```

### `xrtNetSocketMulticastHopLimit`
设置数据报 Socket 的多播跳数，合法范围为 0 到 255。

```c
bool xrtNetSocketMulticastHopLimit(xnetsocket Socket, int iHopLimit);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `iHopLimit` | 输入 | `[0, 255]` | 多播 TTL |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_MULTICAST_TTL` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 单跳自收

```c
!xrtNetSocketMulticastHopLimit(A, 1) ||
```

### `xrtNetSocketMulticastInterface`
选择多播发送接口；空接口恢复系统默认。

```c
bool xrtNetSocketMulticastInterface(xnetsocket Socket, const xnetaddr* pInterface);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pInterface` | 输入 | 允许空指针 | 空恢复默认；IPv6 使用 Scope 接口索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空对象
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_MULTICAST_IF` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 设环回接口并在收尾恢复默认

```c
!xrtNetSocketMulticastInterface(A, &Iface) ||
```

## 网络事件端口

### `xrtNetPortConfigInit`
初始化端口配置为默认值：`Backend=AUTO`、`PostLimit=4096`、`WatchLimit=0`、`OperationLimit=0`、`OperationCache=64`。

```c
void xrtNetPortConfigInit(xnetportconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · 每次创建端口前重新初始化

```c
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_IOCP;
pIocp = xrtNetPortCreate(&Config);
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_SELECT;
pSelect = xrtNetPortCreate(&Config);
```

### `xrtNetPortCreate`
创建事件端口。`AUTO` 在当前已编译后端中选择最高能力实现：Windows 优先 IOCP，Linux 优先 epoll，Darwin/BSD 优先 kqueue，其他平台使用 select；显式指定的后端不可用时返回 `XERR_UNSUPPORTED`，不会静默换后端。

```c
xnetport* xrtNetPortCreate(const xnetportconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 通常为 `ConfigInit` 产物，可按需覆盖字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 端口已创建，创建线程即拥有线程 | — |
| `NULL` | 配置非法、后端不可用或资源不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_CREATE` — 配置指针为空或字段非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_CREATE` — 显式后端未编译进当前构建
- 内存分配失败 — 内部资源（唤醒通道、索引表）分配失败

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · 显式指定 IOCP 与 SELECT 各建一个

```c
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_IOCP;
pIocp = xrtNetPortCreate(&Config);
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_SELECT;
pSelect = xrtNetPortCreate(&Config);
```

### `xrtNetPortDestroy`
取消并排空全部在途 IO，再销毁端口、观察、用户事件及唤醒资源；返回后系统不再引用任何调用方缓冲。只能由拥有线程调用。

```c
bool xrtNetPortDestroy(xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 允许空指针 | 空指针是空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 端口已销毁 | — |
| `false` | 参数非法、非拥有线程或底层关闭失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pPort` 非法
- `XERR_STATE` — 从非拥有线程调用
- `XERR_IO` + `XNET_ERROR_PORT_CLOSE` — 关闭后端句柄失败

#### 范例

[network/port_tour · 收尾](../../examples/network/port_tour/main.c) · 先关 Socket 再销毁端口

```c
if ( pSelect != NULL ) {
	xrtNetPortDestroy(pSelect);
}
if ( pIocp != NULL ) {
	xrtNetPortDestroy(pIocp);
}
```

### `xrtNetPortBackend`
返回端口实际启用的后端。

```c
xnetportbackend xrtNetPortBackend(const xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XNET_PORT_AUTO` | `pPort` 为空（`AUTO` 是零值，仅作失败哨兵） |
| `XNET_PORT_IOCP`/`XNET_PORT_URING`/`XNET_PORT_EPOLL`/`XNET_PORT_KQUEUE`/`XNET_PORT_SELECT` | 实际后端；`AUTO` 创建后此处是解析结果 |

#### 错误

- `XERR_ARGUMENT` — `pPort` 为空（返回值仍是 `XNET_PORT_AUTO`）

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · 校验显式后端未被替换

```c
if ( (pIocp == NULL) || (pSelect == NULL) ||
	(xrtNetPortBackend(pIocp) != XNET_PORT_IOCP) ||
	(xrtNetPortBackend(pSelect) != XNET_PORT_SELECT) ) {
	goto Cleanup;
}
```

### `xrtNetPortGetConfig`
返回已经解析 `AUTO` 容量和实际后端的有效配置。返回值是副本，修改它不影响端口。

```c
bool xrtNetPortGetConfig(
	const xnetport* pPort,
	xnetportconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |
| `pConfig` | 输出 | 非空 | 接收解析后的配置副本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | `*pConfig` 已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/port_tour · 自省](../../examples/network/port_tour/main.c) · 零值上限被解析为后端硬上限

```c
{
	xnetportconfig Resolved;

	if ( !xrtNetPortGetConfig(pIocp, &Resolved) ||
		(Resolved.Backend != XNET_PORT_IOCP) ) {
		goto Cleanup;
	}
}
```

### `xrtNetPortName`
返回静态后端名称（如 `"iocp"`、`"select"`），字符串存活期与进程相同。

```c
cstr xrtNetPortName(const xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 `cstr` | 静态后端名称 |
| `NULL` | `pPort` 为空 |

#### 错误

- `XERR_ARGUMENT` — `pPort` 为空

#### 范例

[network/port_iocp · 完成式](../../examples/network/port_iocp/main.c) · 诊断输出中打印后端名

```c
printf("backend=%s bytes=%zu data=%s\n",
	xrtNetPortName(pPort), Events[i].Bytes, sData);
```

### `xrtNetPortCapabilities`
返回实际后端能力位（`XNET_PORT_CAP_*` 的按位或）。

```c
uint32 xrtNetPortCapabilities(const xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 能力位组合 | `READINESS`/`COMPLETION`/`ONESHOT`/`EDGE`/`BATCH`/`WAKE`/`POST`/`CANCEL`/`READ_PROBE` 的按位或 |
| `0` | `pPort` 为空（无后端具备零能力，可作失败哨兵） |

#### 错误

- `XERR_ARGUMENT` — `pPort` 为空

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · IOCP 只有 COMPLETION，没有 READINESS

```c
iCaps = xrtNetPortCapabilities(pIocp);
if ( ((iCaps & XNET_PORT_CAP_COMPLETION) == 0u) ||
	((iCaps & XNET_PORT_CAP_READINESS) != 0u) ) {
	goto Cleanup;
}
```

### `xrtNetPortAccept`
异步接受一个连接。成功提交后 `Accepted` Socket 由终态事件（`ACCEPT`）转移给调用方，调用方成为其唯一拥有者。

```c
bool xrtNetPortAccept(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已监听 | 必须是 `Listen` 状态的流 Socket |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`ACCEPT` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或 `Socket` 不是监听流
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力（如 SELECT）
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · `Event.Accepted` 是新 Socket

```c
!xrtNetPortConnect(pIocp, Client, &AddrListen, 201u, NULL) ||
!xrtNetPortAccept(pIocp, Listener, 202u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_CONNECT, 201u,
	&Event, 2000000ull) ||
(Event.Result != XNET_RESULT_OK) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_ACCEPT, 202u,
	&Event, 2000000ull) ||
(Event.Accepted == 0) ) {
	goto Cleanup;
}
```

### `xrtNetPortConnect`
异步连接远端地址。终态事件（`CONNECT`）到达前 Socket 必须保持有效。

```c
bool xrtNetPortConnect(xnetport* pPort, xnetsocket Socket,
	const xnetaddr* pRemote, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已打开 | 未连接的流 Socket |
| `pRemote` | 输入 | 非空 | 目标地址；族必须与 Socket 一致 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`CONNECT` 终态事件待提取，结果在 `Event.Result` | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或地址族与 Socket 不匹配
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 连接结果在终态事件的 `Result` 字段

```c
!xrtNetPortConnect(pIocp, Client, &AddrListen, 201u, NULL) ||
!xrtNetPortAccept(pIocp, Listener, 202u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_CONNECT, 201u,
	&Event, 2000000ull) ||
(Event.Result != XNET_RESULT_OK) ||
```

### `xrtNetPortReadProbe`
异步等待流 Socket 可读；不借用数据缓冲，终态（`READ_PROBE`，`Bytes == 0`）到达后再提交 `Recv` 才读取数据或确认 EOF。适合 TLS 等由上层状态机驱动读取的场景。

```c
bool xrtNetPortReadProbe(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 具备 `READ_PROBE` 能力的后端 |
| `Socket` | 输入 | 已连接流 | 只接受流 Socket |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；可读或 EOF 时产生 `READ_PROBE` 终态 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或 `Socket` 不是流
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力，或后端无法探询流可读性（无 `READ_PROBE` 能力）
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 探针终态后再提交 `Recv`

```c
if ( !xrtNetPortReadProbe(pIocp, Server, 203u, NULL) ||
	(xrtNetSocketSend(Client, "hi", 2u, &iSent) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_READ_PROBE, 203u,
		&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### `xrtNetPortRecv`
异步接收到调用方缓冲；支持流和已连接数据报，单次最多 `INT_MAX` 字节。等价于单跨度 `RecvVec`，错误集与之相同。

```c
bool xrtNetPortRecv(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pData` | 输入/输出 | 借用至终态 | 接收缓冲，流接收必须非空 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、流接收缓冲为空或大小超限
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 请求字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 实际读取量在 `Event.Bytes`

```c
if ( !xrtNetPortRecv(pIocp, Server, arrBuf, 8u, 204u, NULL) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 204u,
		&Event, 2000000ull) ||
	(Event.Bytes != 2u) ||
	(memcmp(arrBuf, "hi", 2u) != 0) ||
```

### `xrtNetPortRecvVec`
异步分散接收；支持流和已连接数据报，Span 总长度最多 `INT_MAX` 字节。

```c
bool xrtNetPortRecvVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pSpans` | 输入/输出 | 借用至终态 | 可写跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV` 终态事件待提取，`Bytes` 为全部跨度写入总量 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空、单跨度非法或流接收总长为零
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 流向量](../../examples/network/port_tour/main.c) · 两段分散接收

```c
if ( !xrtNetPortRecvVec(pIocp, Server, In, 2u, 210u, NULL) ||
	(xrtNetSocketSendVec(Client, Out, 2u, &iSent) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 210u,
		&Event, 2000000ull) ||
	(Event.Bytes != 4u) ||
```

### `xrtNetPortSend`
异步发送调用方缓冲；支持流和已连接数据报，单次最多 `INT_MAX` 字节。等价于单跨度 `SendVec`，错误集与之相同。

```c
bool xrtNetPortSend(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pData` | 输入 | 借用至终态 | 只读发送数据，期间不得修改 |
| `iSize` | 输入 | `<= INT_MAX` | 发送字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`SEND` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或发送缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 发送字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 发送完成即缓冲可复用

```c
!xrtNetPortSend(pIocp, Server, "ok", 2u, 205u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND, 205u,
	&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### `xrtNetPortSendVec`
异步聚集发送；支持流和已连接数据报，Span 总长度最多 `INT_MAX` 字节。

```c
bool xrtNetPortSendVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pSpans` | 输入 | 借用至终态 | 只读跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`SEND` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 流向量](../../examples/network/port_tour/main.c) · 两段聚集发送

```c
!xrtNetPortSendVec(pIocp, Server, Out, 2u, 211u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND, 211u,
	&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### `xrtNetPortRecvFrom`
异步接收数据报；远端地址由终态事件（`RECV_FROM`）的 `Address` 字段返回。等价于单跨度 `RecvFromVec`，错误集与之相同。

```c
bool xrtNetPortRecvFrom(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接或已连接数据报 |
| `pData` | 输入/输出 | 借用至终态 | 接收缓冲 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数；不足时终态为 `TRUNCATED` |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV_FROM` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 缓冲超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_iocp · 完成式](../../examples/network/port_iocp/main.c) · 终态事件带来源地址

```c
!xrtNetPortRecvFrom(pPort, Server,
	sData, sizeof(sData) - 1, 1, NULL) ||
```

### `xrtNetPortRecvFromVec`
异步分散接收数据报；缓冲不足由终态事件返回 `TRUNCATED`，`Bytes` 保留实际写入长度。

```c
bool xrtNetPortRecvFromVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接或已连接数据报 |
| `pSpans` | 输入/输出 | 借用至终态 | 可写跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV_FROM` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报](../../examples/network/port_tour/main.c) · 来源端口在 `Event.Address.Port`

```c
if ( !xrtNetPortRecvFromVec(pIocp, UdpA, In, 2u, 220u, NULL) ||
	(xrtNetSocketSendTo(UdpB, "d1", 2u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_FROM, 220u,
		&Event, 2000000ull) ||
```

### `xrtNetPortRecvMsg`
异步接收数据报及 Socket 已启用的元数据；终态事件（`RECV_MSG`）同时返回 `Address` 与 `Meta`。等价于单跨度 `RecvMsgVec`，错误集与之相同。

```c
bool xrtNetPortRecvMsg(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已启用元数据 | 须先 `xrtNetSocketDgramMetaSet` 启用至少一位 |
| `pData` | 输入/输出 | 借用至终态 | 接收缓冲 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV_MSG` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_PORT_SUBMIT` — Socket 未启用任何接收元数据
- `XERR_ARGUMENT` / `XERR_RANGE` / `XERR_UNSUPPORTED` / `XERR_STATE`（拥有线程）— 同 `RecvVec`
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`

#### 范例

[network/port_tour · 数据报](../../examples/network/port_tour/main.c) · 终态事件携带已启用的元数据位

```c
if ( !xrtNetPortRecvMsg(pIocp, UdpA, arrBuf, 16u, 221u, NULL) ||
	(xrtNetSocketSendTo(UdpB, "d2", 2u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_MSG, 221u,
		&Event, 2000000ull) ||
	(Event.Bytes != 2u) ||
```

### `xrtNetPortRecvMsgVec`
异步分散接收数据报及元数据；终态事件同时返回 `Address` 和 `Meta`。

```c
bool xrtNetPortRecvMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已启用元数据 | 须先 `xrtNetSocketDgramMetaSet` 启用至少一位 |
| `pSpans` | 输入/输出 | 借用至终态 | 可写跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV_MSG` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_PORT_SUBMIT` — Socket 未启用任何接收元数据
- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报](../../examples/network/port_tour/main.c) · 分散接收第三形态

```c
if ( !xrtNetPortRecvMsgVec(pIocp, UdpA, In, 2u, 222u, NULL) ||
	(xrtNetSocketSendTo(UdpB, "d3", 2u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_MSG, 222u,
		&Event, 2000000ull) ||
```

### `xrtNetPortRecvError`
异步等待并读取一个数据报错误；终态（`RECV_ERROR`）同时返回原负载前缀和 `DgramError`。要求 Socket 已启用数据报错误队列，且后端支持等待该队列。

```c
bool xrtNetPortRecvError(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 支持数据报错误等待的后端 |
| `Socket` | 输入 | 已启用错误队列 | 须先 `xrtNetSocketDgramErrorSet` 启用 |
| `pData` | 输入/输出 | 借用至终态 | 接收原负载前缀的缓冲 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`RECV_ERROR` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_PORT_SUBMIT` — Socket 未启用数据报错误队列
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — Socket 不支持错误队列（平台限制），或后端无法等待数据报错误（如 IOCP）
- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 缓冲超过 `INT_MAX`
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 平台门控](../../examples/network/port_tour/main.c) · Windows IOCP 上提交被拒是预期行为

```c
if ( xrtNetPortRecvError(pIocp, UdpA, arrBuf, 16u, 300u, NULL) ) {
	goto Cleanup;
}
```

### `xrtNetPortSendTo`
异步发送数据报；远端地址在提交时复制，提交返回后可立即复用地址对象。等价于单跨度 `SendToVec`，错误集与之相同。

```c
bool xrtNetPortSendTo(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接数据报 Socket |
| `pData` | 输入 | 借用至终态 | 只读发送数据 |
| `iSize` | 输入 | `<= INT_MAX` | 发送字节数，允许零长度报文 |
| `pRemote` | 输入 | 非空 | 目标地址；族与 Socket 一致，提交时复制 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`SEND_TO` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 发送字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_uring · 完成式](../../examples/network/port_uring/main.c) · 提交后地址对象即可复用

```c
!xrtNetPortSendTo(
	pPort,
	Client,
	"completion",
	10,
	&Address,
	2,
	NULL
 ) ||
```

### `xrtNetPortSendToVec`
异步聚集发送数据报；远端地址和 Span 描述符在提交时复制。

```c
bool xrtNetPortSendToVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接数据报 Socket |
| `pSpans` | 输入 | 借用至终态 | 只读跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `pRemote` | 输入 | 非空 | 目标地址；提交时复制 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`SEND_TO` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报发送](../../examples/network/port_tour/main.c) · 两段聚集发送

```c
if ( !xrtNetPortSendToVec(pIocp, UdpB, Out, 2u, &DestUdpA,
		230u, NULL) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND_TO, 230u,
		&Event, 2000000ull) ||
	(Event.Result != XNET_RESULT_OK) ||
```

### `xrtNetPortSendMsg`
异步发送带逐包控制的数据报；地址和控制值在提交时复制。非零控制 `Flags` 的终态为 `SEND_MSG`；空控制或零 `Flags` 走普通发送路径，有 `pRemote` 时终态为 `SEND_TO`，否则为 `SEND`（Socket 须已连接）。等价于单跨度 `SendMsgVec`，错误集与之相同。

```c
bool xrtNetPortSendMsg(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 数据报 Socket |
| `pData` | 输入 | 借用至终态 | 只读发送数据 |
| `iSize` | 输入 | `<= INT_MAX` | 发送字节数 |
| `pRemote` | 输入 | 可空 | 目标地址；空表示 Socket 已连接 |
| `pControl` | 输入 | 可空 | 逐包控制；空或零 `Flags` 走普通发送 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；终态类型按控制与地址组合（见上文） | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 发送字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_iocp · 完成式](../../examples/network/port_iocp/main.c) · 逐包覆盖源地址发送

```c
!xrtNetPortSendMsg(pPort, Client,
	"completion", 10, &Address, &Control, 2, NULL) ||
```

### `xrtNetPortSendMsgVec`
异步聚集发送带逐包控制的数据报；Span 描述符、地址和控制值在提交时复制。终态类型与 `SendMsg` 相同。

```c
bool xrtNetPortSendMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 数据报 Socket |
| `pSpans` | 输入 | 借用至终态 | 只读跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `pRemote` | 输入 | 可空 | 目标地址；空表示 Socket 已连接 |
| `pControl` | 输入 | 可空 | 逐包控制；空或零 `Flags` 走普通发送 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；终态类型按控制与地址组合（同 `SendMsg`） | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报发送](../../examples/network/port_tour/main.c) · 零 `Flags` 控制走 `SEND_TO` 路径

```c
!xrtNetPortSendMsgVec(pIocp, UdpB, Out, 2u, &DestUdpA,
	&Control, 231u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND_TO, 231u,
	&Event, 2000000ull) ||
(Event.Result != XNET_RESULT_OK) ) {
	goto Cleanup;
}
```

### `xrtNetPortCancel`
请求取消指定 `Id` 的在途操作。操作仍以一个终态事件结束：取消成功时为 `CANCELLED`，完成先于取消发生时仍是原完成。

```c
bool xrtNetPortCancel(xnetport* pPort, uint64 Id);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 支持取消的后端 |
| `Id` | 输入 | 非零 | 要取消的在途操作 ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 取消请求已受理；仍须等待该操作唯一终态 | — |
| `false` | 参数非法、后端不支持取消或非拥有线程 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_CANCEL` — `pPort`/`Id` 非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_CANCEL` — 后端不支持取消
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 取消](../../examples/network/port_tour/main.c) · 在途 `Recv` 以 `CANCELLED` 终结

```c
if ( !xrtNetPortRecv(pIocp, UdpA, arrBuf, 8u, 600u, NULL) ||
	!xrtNetPortCancel(pIocp, 600u) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 600u,
		&Event, 2000000ull) ||
	(Event.Result != XNET_RESULT_CANCELLED) ) {
```

### `xrtNetPortWatch`
替换一个 Socket 的 readiness 关注位；事件为零等价于 `Unwatch`。每 Socket 单份观察，重复 `Watch` 覆盖旧关注位与事件身份。

```c
bool xrtNetPortWatch(xnetport* pPort, xnetsocket Socket,
	uint64 Id, uint32 iEvents, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | readiness 后端端口 |
| `Socket` | 输入 | 已打开 | 要观察的 Socket |
| `Id` | 输入 | 非零 | 事件身份；替换观察时同时替换 |
| `iEvents` | 输入 | `XNET_PORT_EVENT_*` 位 | 关注方向；`READ`/`WRITE` 等，零等价 `Unwatch` |
| `pUser` | 输入 | 任意值 | 原样进入事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 内核观察与用户身份均已登记 | — |
| `false` | 未登记 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_WATCH` — 参数非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_WATCH` — 后端没有 readiness 能力（如 IOCP）
- `XERR_RANGE` + `XNET_ERROR_PORT_WATCH` — 观察数达到 `WatchLimit`（select 还受 `FD_SETSIZE` 约束）
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · readiness](../../examples/network/port_tour/main.c) · SELECT 端口上观察读方向

```c
if ( !xrtNetPortWatch(pSelect, UdpA, 100u, XNET_PORT_EVENT_READ,
		NULL) ||
	(xrtNetSocketSendTo(UdpB, "r", 1u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pSelect, XNET_PORT_EVENT_READY, 100u,
		&Event, 2000000ull) ||
	!xrtNetPortUnwatch(pSelect, UdpA) ) {
	goto Cleanup;
}
```

### `xrtNetPortUnwatch`
幂等移除观察。失败也会退休用户身份，调用方随后必须关闭该 Socket，不能继续观察或执行 IO。

```c
bool xrtNetPortUnwatch(xnetport* pPort, xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | readiness 后端端口 |
| `Socket` | 输入 | 已打开 | 要移除观察的 Socket |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 内核观察和用户身份均已移除 | — |
| `false` | 移除失败，但用户身份仍已退休；须立即关闭该 Socket | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_WATCH` — 参数非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_WATCH` — 后端没有 readiness 能力
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · readiness](../../examples/network/port_tour/main.c) · 消费事件后移除观察

```c
!exampleWaitFor(pSelect, XNET_PORT_EVENT_READY, 100u,
	&Event, 2000000ull) ||
!xrtNetPortUnwatch(pSelect, UdpA) ) {
	goto Cleanup;
}
```

### `xrtNetPortWait`
等待到事件、截止时间或错误；成功和超时都会先清零 `*pCount`。只能由拥有线程调用。

```c
xnetresult xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	xdeadline iDeadline, size_t* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |
| `pEvents` | 输出 | 非空数组 | 事件输出缓冲 |
| `iCapacity` | 输入 | `> 0` | 输出容量；后端一次最多写入这么多事件 |
| `iDeadline` | 输入 | 单调微秒 | `xrtClock` 截止时间；`xrtDeadlineAfter`/`xrtDeadlineNever` |
| `pCount` | 输出 | 非空 | 实际写入事件数；进入时即清零 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 提取到 `*pCount > 0` 个事件 | — |
| `XNET_RESULT_TIMEOUT` | 到达截止时间，`*pCount == 0` | — |
| `XNET_RESULT_ERROR` | 参数非法、非拥有线程或后端等待失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_WAIT` — 缓冲为空、容量为零或 `pCount` 为空
- `XERR_STATE` — 从非拥有线程调用
- `XERR_IO` + `XNET_ERROR_PORT_WAIT` — 后端等待系统调用失败

#### 范例

[network/port_tour · 等待辅助](../../examples/network/port_tour/main.c) · 截止时间分片轮询

```c
if ( xrtNetPortWait(pPort, Events, 8u,
		xrtDeadlineAfter(iTimeoutUs / 100u),
		&iCount) != XNET_RESULT_OK ) {
	continue;
}
```

### `xrtNetPortPost`
跨线程投递一个不会合并的用户事件（`USER`）。`Id` 与 `pUser` 原样进入事件。

```c
bool xrtNetPortPost(xnetport* pPort, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针；跨线程调用仍须保证对象存活 |
| `Id` | 输入 | 非零 | 事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 事件已入队，等待中必然可提取 | — |
| `false` | 未入队，不留幽灵事件 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_POST` — `pPort`/`Id` 非法
- `XERR_STATE` + `XNET_ERROR_PORT_POST` — 端口正在关闭
- `XERR_AGAIN` + `XNET_ERROR_PORT_POST` — 用户事件队列达到 `PostLimit`

#### 范例

[network/port_tour · 用户事件](../../examples/network/port_tour/main.c) · `USER` 事件携带 `Id`

```c
if ( !xrtNetPortPost(pIocp, 777u, NULL) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_USER, 777u,
		&Event, 2000000ull) ||
```

### `xrtNetPortWake`
跨线程请求一个可合并的 `WAKE` 事件；连续请求只产生一个事件。适合“命令队列已有工作”式通知。

```c
bool xrtNetPortWake(xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已有挂起 `WAKE` 或新请求了一个 | — |
| `false` | 未请求 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_POST` — `pPort` 非法
- `XERR_STATE` + `XNET_ERROR_PORT_POST` — 端口正在关闭

#### 范例

[network/port_tour · 唤醒](../../examples/network/port_tour/main.c) · `WAKE` 事件 `Id` 为零

```c
!xrtNetPortWake(pIocp) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_WAKE, 0u,
	&Event, 2000000ull) ) {
	goto Cleanup;
}
```

## 嵌入式任务投递

### `xrtNetPostInit`
初始化一个尚未投递的嵌入式 Post；清零节点并写入内部魔数。

```c
bool xrtNetPostInit(xnetpost* pPost);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPost` | 输出 | 非空 | 嵌入调用方结构的 `xnetpost` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 节点已就绪，可投递 | — |
| `false` | `pPost` 非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pPost` 为空或范围非法

#### 范例

[network/engine_tour · 嵌入式 Post](../../examples/network/engine_tour/main.c) · 初始化后立即投递

```c
if ( !xrtNetPostInit(&Post) ||
	!xrtNetPost(pWorker0, &Post,
		exampleSimpleTask, (ptr)&bTaskDone) ) {
	goto Cleanup;
}
```

### `xrtNetPostPending`
判断嵌入式 Post 是否仍在 Worker 队列中等待执行；执行完成后清除。

```c
bool xrtNetPostPending(const xnetpost* pPost);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPost` | 输入 | 非空 | 已 `PostInit` 的节点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理投递、尚未执行 | — |
| `false` | 已执行完成，或从未投递成功 | 参数非法/状态非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pPost` 为空或范围非法
- `XERR_STATE` — 节点未经 `xrtNetPostInit` 初始化（魔数不匹配）

#### 范例

[network/engine_tour · 嵌入式 Post](../../examples/network/engine_tour/main.c) · 受理为真，执行后为假

```c
if ( !xrtNetPostPending(&Post) ) {
	goto Cleanup;
}
if ( !exampleSpinUntil(&bTaskDone, 2000u) ||
	xrtNetPostPending(&Post) ) {
	goto Cleanup;
}
```

### `xrtNetPost`
无分配地投递到指定 Worker；同一 Post 在出队前不能再次投递。从 Worker 自身线程调用时直接入队不阻塞，跨线程投递经有界提交段进入。受理失败时节点状态回滚，不留半提交状态。

```c
bool xrtNetPost(
	xnetworker* pWorker,
	xnetpost* pPost,
	xnettaskproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 目标 Worker；须保持存活到回调返回 |
| `pPost` | 输入/输出 | 已 `PostInit` | 嵌入式任务节点 |
| `pProc` | 输入 | 非空 | 任务回调，在亲和 Worker 上执行一次 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；回调必在目标 Worker 上执行一次 | — |
| `false` | 未受理，节点无残留状态 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker`/`pPost`/`pProc` 为空或非法
- `XERR_STATE` — `pPost` 未经 `xrtNetPostInit` 初始化，或仍在队列中等待（不能重复投递）
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_POST` — Worker 未运行、不再受理投递或关停已封口

#### 范例

[network/engine_tour · 嵌入式 Post](../../examples/network/engine_tour/main.c) · 投递固定任务并自旋等待

```c
if ( !xrtNetPostInit(&Post) ||
	!xrtNetPost(pWorker0, &Post,
		exampleSimpleTask, (ptr)&bTaskDone) ) {
	goto Cleanup;
}
```

## 网络 Engine

### `xrtNetEngineConfigInit`
初始化兼顾吞吐与内存占用的 Engine 默认配置（见上表）。

```c
void xrtNetEngineConfigInit(xnetengineconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 覆盖 Worker 数后创建

```c
xrtNetEngineConfigInit(&Config);
Config.Workers = 2;
pEngine = xrtNetEngineCreate(&Config);
```

### `xrtNetEngineCreate`
创建停止状态的 Engine；Worker 线程和端口在 `Start` 时建立。`BufferPool` 指向的配置在返回前完整复制。

```c
xnetengine* xrtNetEngineCreate(const xnetengineconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 通常为 `ConfigInit` 产物；字段越界（如 `Workers > 256`、`EventBatch > 4096`）立即失败 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 停止状态的 Engine | — |
| `NULL` | 配置非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_ENGINE_CREATE` — 配置指针为空或字段非法
- 内存分配失败 — Engine 结构或初始表分配失败

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 创建即 `STOPPED`

```c
pEngine = xrtNetEngineCreate(&Config);
if ( (pEngine == NULL) ||
	(xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED) ||
	!xrtNetEngineStart(pEngine) ) {
	goto Cleanup;
}
```

### `xrtNetEngineStart`
建立全部 Worker、端口和线程；已经运行时幂等成功。部分启动失败会完整回滚到 `STOPPED`，允许重试。

```c
bool xrtNetEngineStart(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 停止或运行状态的 Engine |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已运行（或原本已运行） | — |
| `false` | 参数非法、状态切换冲突或资源创建失败；失败后回到 `STOPPED` | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空
- `XERR_STATE` + `XNET_ERROR_ENGINE_START` — 状态正在切换（并发 Start/Stop）
- `XERR_INTERNAL` + `XNET_ERROR_ENGINE_START` — Worker 端口、线程或缓冲池创建失败（含底层端口错误传播）

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · Start 后自旋到 `RUNNING`

```c
!xrtNetEngineStart(pEngine) ) {
	goto Cleanup;
}
while ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
	xrtSleep(1u);
}
```

### `xrtNetEngineStop`
排空任务并释放运行资源。任务链不收敛或仍有外借池块时返回失败，但 Engine 仍进入可重启的停止状态。

```c
bool xrtNetEngineStop(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行或停止状态的 Engine |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已排空并停止；可再次 `Start` | — |
| `false` | 已进入 `STOPPED` 但排空不完整（见错误） | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空
- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 从 Worker 回调内调用（自等待死锁），或状态正在切换
- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 任务链不收敛触发封口（`ShutdownStalls` 计数）
- `XNET_ERROR_POOL_BUSY` — Worker 缓冲池仍有外借块；池和 Engine 保留，可 `Start` 后归还再 `Stop`

#### 范例

[network/engine_tour · 收尾](../../examples/network/engine_tour/main.c) · 先停再销毁

```c
(xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED) ) {
	xrtNetEngineStop(pEngine);
}
```

### `xrtNetEngineDestroy`
停止并销毁 Engine；仍有高层对象或外借池块时失败并保留对象。

```c
bool xrtNetEngineDestroy(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针是空操作；运行中会先执行停止流程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | Engine 已销毁 | — |
| `false` | 仍有活动对象或外借池块，Engine 保留 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 从 Worker 回调内调用
- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 状态正在切换或仍有活动网络对象
- `XNET_ERROR_POOL_BUSY` — 缓冲池仍有外借块

#### 范例

[network/engine_tour · 收尾](../../examples/network/engine_tour/main.c) · Stop 之后再 Destroy

```c
if ( pEngine != NULL ) {
	xrtNetEngineDestroy(pEngine);
}
```

### `xrtNetEngineState`
返回当前生命周期状态，可安全跨线程查询。

```c
xnetenginestate xrtNetEngineState(const xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针返回 `XNET_ENGINE_STOPPED`（零值） |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XNET_ENGINE_STOPPED` | 停止（或空指针）；可 `Start` |
| `XNET_ENGINE_STARTING` | 正在建立 Worker |
| `XNET_ENGINE_RUNNING` | 运行中 |
| `XNET_ENGINE_STOPPING` | 正在排空停机 |
| `XNET_ENGINE_DESTROYING` | 正在销毁 |

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 自旋等待进入运行态

```c
while ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
	xrtSleep(1u);
}
```

### `xrtNetEnginePin`
占用一个正在运行的 Engine 生命周期，供组合网络对象保存借用指针。每次成功占用必须由一次 `xrtNetEngineUnpin` 配对释放。

```c
bool xrtNetEnginePin(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 必须处于 `RUNNING` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 生命周期占用 +1，Destroy 被阻止 | — |
| `false` | 未占用 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_CLOSED` + `XNET_ERROR_ENGINE_POST` — Engine 未运行（Pin 只对运行中的 Engine 有意义）

#### 范例

[network/engine_tour · 占用](../../examples/network/engine_tour/main.c) · Pin/Unpin 严格配对

```c
if ( !xrtNetEnginePin(pEngine) ||
	!xrtNetEngineUnpin(pEngine) ) {
	goto Cleanup;
}
```

### `xrtNetEngineUnpin`
释放一次 Engine 生命周期占用；没有匹配占用时返回状态错误。

```c
bool xrtNetEngineUnpin(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 必须有未释放的 Pin |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 占用 -1；归零后 Destroy 可继续 | — |
| `false` | 参数非法或没有匹配占用 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空
- `XERR_STATE` — 没有匹配的 Pin 占用

#### 范例

[network/engine_tour · 占用](../../examples/network/engine_tour/main.c) · Pin/Unpin 严格配对

```c
if ( !xrtNetEnginePin(pEngine) ||
	!xrtNetEngineUnpin(pEngine) ) {
	goto Cleanup;
}
```

### `xrtNetEngineWorkerCount`
返回 Engine 固定的 Worker 数量。

```c
uint32 xrtNetEngineWorkerCount(const xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针返回 0 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `> 0` | Worker 数；创建后固定，跨 `Stop`/`Start` 不变 |
| `0` | `pEngine` 为空 |

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 显式 2 Worker

```c
if ( (xrtNetEngineWorkerCount(pEngine) != 2u) ||
	((pWorker0 = xrtNetEngineWorker(pEngine, 0u)) == NULL) ||
```

### `xrtNetEngineWorker`
返回借用的指定 Worker；索引越界时返回空指针。

```c
xnetworker* xrtNetEngineWorker(xnetengine* pEngine, uint32 iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Engine 指针 |
| `iIndex` | 输入 | `< WorkerCount` | Worker 索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用 Worker，存活期由 Engine 决定 | — |
| `NULL` | 索引越界 | `XERR_RANGE`（越界时设置） |

#### 错误

- `XERR_RANGE` — `iIndex >= WorkerCount`

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 越界索引返回空

```c
((pWorker0 = xrtNetEngineWorker(pEngine, 0u)) == NULL) ||
((pWorker1 = xrtNetEngineWorker(pEngine, 1u)) == NULL) ||
(xrtNetEngineWorker(pEngine, 99u) != NULL) ||
```

### `xrtNetEngineCurrent`
返回当前线程所属的借用 Worker，不属于该 Engine 时返回空指针。

```c
xnetworker* xrtNetEngineCurrent(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针返回 `NULL` |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 当前线程正是该 Worker（在其回调内调用） |
| `NULL` | 当前线程不属于该 Engine（如外部主线程） |

#### 错误

- 无 — 不属于任何 Worker 是查询结果而非错误，不设置线程错误

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 主线程不属于任何 Worker

```c
(xrtNetEngineWorker(pEngine, 99u) != NULL) ||
/* 主线程不属于任何 Worker → Current 为空。 */
(xrtNetEngineCurrent(pEngine) != NULL) ) {
	goto Cleanup;
}
```

### `xrtNetWorkerEngine`
返回 Worker 所属的借用 Engine。

```c
xnetengine* xrtNetWorkerEngine(const xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 允许空指针 | 空指针返回 `NULL` |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 所属 Engine（借用） |
| `NULL` | `pWorker` 为空 |

#### 错误

- 无 — 空指针返回 `NULL`，不设置线程错误

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内反查 Engine

```c
pTask->bEngineOk =
	(xrtNetWorkerEngine(pWorker) == pTask->pEngine);
```

### `xrtNetWorkerIndex`
返回 Worker 在所属 Engine 内的稳定索引。

```c
uint32 xrtNetWorkerIndex(const xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | Worker 指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `[0, WorkerCount)` | 稳定索引，跨 `Stop`/`Start` 不变 |
| `UINT32_MAX` | `pWorker` 为空 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空（返回 `UINT32_MAX`）

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内核对亲和索引

```c
pTask->bIndexOk = (xrtNetWorkerIndex(pWorker) == 0u);
```

### `xrtNetWorkerIsCurrent`
判断调用线程是否正是指定 Worker。

```c
bool xrtNetWorkerIsCurrent(const xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 允许空指针 | 空指针返回 `false` |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 当前线程正是该 Worker |
| `false` | 不是，或 `pWorker` 为空 |

#### 错误

- 无 — 否定回答与空指针都是查询结果，不设置线程错误

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内确认自身

```c
pTask->bIsCurrent = xrtNetWorkerIsCurrent(pWorker);
```

### `xrtNetWorkerPort`
返回运行期间借用的端口；调用方必须保证借用操作先于 Stop 结束。

```c
xnetport* xrtNetWorkerPort(xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 运行中的 Worker |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用端口；除跨线程明确允许的操作外应在所属 Worker 使用 | — |
| `NULL` | Worker 未运行 | `XERR_STATE`（未运行时设置） |

#### 错误

- `XERR_STATE` + `XNET_ERROR_ENGINE_POST` — Worker 未运行（Stop 后端口已释放）

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内取端口

```c
pTask->bPortOk = (xrtNetWorkerPort(pWorker) != NULL);
```

### `xrtNetWorkerBufPool`
返回 Worker 独占的自适应缓冲池；只能从该 Worker 的回调中调用。

```c
xnetbufpool* xrtNetWorkerBufPool(xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 目标 Worker |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用缓冲池；池块必须在所属 Worker 上归还 | — |
| `NULL` | 参数非法或不在该 Worker 回调内 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空
- `XERR_STATE` + `XNET_ERROR_ENGINE_POST` — 不在所属 Worker 上调用（缓冲池仅 Worker 内可用）

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 只在本 Worker 回调内可用

```c
/* BufPool 只能从本 Worker 回调内调用。 */
pPool = xrtNetWorkerBufPool(pWorker);
pTask->bBufPoolOk = (pPool != NULL);
```

### `xrtNetWorkerAlloc`
从 Worker 的线程安全分级缓存分配并清零一块内存；调用方必须保持 Worker 生命周期有效。

```c
ptr xrtNetWorkerAlloc(xnetworker* pWorker, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 分配归属的 Worker；归还也须同一 Worker |
| `iSize` | 输入 | `> 0` | 请求字节数；按 64–1024 尺寸类圆整，更大直接堆分配 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已清零的内存块 | — |
| `NULL` | 参数非法或内存不足 | 参数非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空或 `iSize` 为零
- 内存分配失败 — 缓存未命中且堆分配失败

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 分配即清零，配对归还

```c
pBlock = xrtNetWorkerAlloc(pWorker, 32u);
pTask->bAllocOk = (pBlock != NULL) &&
	(((const uint8*)pBlock)[0] == 0u) &&
	(((const uint8*)pBlock)[31] == 0u);
xrtNetWorkerFree(pWorker, pBlock, 32u);
```

### `xrtNetWorkerFree`
把 Worker 分配的内存归还同一 Worker；空指针可以直接释放。

```c
void xrtNetWorkerFree(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 必须与分配时的 Worker 相同 |
| `pMemory` | 输入 | 允许空指针 | `Alloc` 返回的块；空指针为空操作 |
| `iSize` | 输入 | 同分配时 | 必须传回原始请求大小 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 小节点回缓存，大节点直接释放 |

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 原始大小配对归还

```c
xrtNetWorkerFree(pWorker, pBlock, 32u);
pTask->bFreeOk = true;
```

### `xrtNetWorkerOperationId`
分配 Engine 内唯一的非零端口操作 ID，可从任意线程调用。

```c
uint64 xrtNetWorkerOperationId(xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 任意 Worker；ID 空间按 Engine 全局划分 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | Engine 内唯一 ID，可直接作端口操作 `Id` | — |
| `0` | 参数非法或 ID 空间耗尽 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空
- `XERR_INTERNAL` + `XNET_ERROR_ENGINE_POST` — 64 位 ID 空间耗尽（实际不可达）

#### 范例

[network/engine_tour · 占用](../../examples/network/engine_tour/main.c) · 不同 Worker 的 ID 互不相同

```c
IdOpA = xrtNetWorkerOperationId(pWorker0);
IdOpB = xrtNetWorkerOperationId(pWorker1);
if ( (IdOpA == 0u) || (IdOpB == 0u) || (IdOpA == IdOpB) ) {
	goto Cleanup;
}
```

### `xrtNetEnginePost`
有界投递任务；成功受理后必在亲和 Worker（`iAffinity % Workers`）上执行一次。

```c
bool xrtNetEnginePost(xnetengine* pEngine,
	uint64 iAffinity, xnettaskproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行中的 Engine |
| `iAffinity` | 输入 | 任意值 | 亲和键；按模 Worker 数选目标 |
| `pProc` | 输入 | 非空 | 任务回调，短小非阻塞 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；含与 `Stop` 并发时也必执行一次 | — |
| `false` | 未受理，不留任务 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine`/`pProc` 为空
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_POST` — Engine 未运行或停机封口已触发
- `XERR_AGAIN` — 目标 Worker 命令队列达到 `CommandCapacity`
- `XERR_INTERNAL` — Worker 唤醒失败

#### 范例

[network/engine_tour · 任务](../../examples/network/engine_tour/main.c) · 投递到 0 号亲和并自旋等待

```c
if ( !xrtNetEnginePost(pEngine, 0u, exampleWorkerTask,
		(ptr)&Task) ||
	!exampleSpinUntil(&Task.bDone, 2000u) ||
```

### `xrtNetEngineSchedule`
按单调时钟截止时间调度 Timer；成功返回非零 ID。

```c
uint64 xrtNetEngineSchedule(xnetengine* pEngine,
	uint64 iAffinity, xdeadline iDeadline,
	xnettimerproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行中的 Engine |
| `iAffinity` | 输入 | 任意值 | 亲和键；Timer 归属该 Worker |
| `iDeadline` | 输入 | 单调微秒 | `xrtClock` 绝对截止时间 |
| `pProc` | 输入 | 非空 | 终态回调（四种结果见上表） |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | Timer ID，Engine 内唯一；恰好一次终态回调 | — |
| `0` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine`/`pProc` 为空
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_TIMER` — Engine 未运行或停机封口
- `XERR_AGAIN` + `XNET_ERROR_ENGINE_TIMER` — 在途 Timer 数达到 `TimerLimit`
- `XERR_RANGE` + `XNET_ERROR_ENGINE_TIMER` — Timer 表无法表示更多条目
- `XERR_INTERNAL` — 跨线程命令入队后唤醒失败

#### 范例

[network/engine_tour · 定时器](../../examples/network/engine_tour/main.c) · 即时到期 + 长时定时器

```c
IdFire = xrtNetEngineSchedule(pEngine, 0u,
	xrtDeadlineAfter(0u), exampleFireTimer, (ptr)&Timers);
Timers.iLongId = xrtNetEngineSchedule(pEngine, 0u,
	xrtDeadlineAfter(3600000000ull), exampleLongTimer,
	(ptr)&Timers);
```

### `xrtNetEngineAfter`
按相对微秒数调度 Timer；零表示在下一次 Worker 循环到期。等价于 `Schedule` + `xrtDeadlineAfter`，错误集与之相同。

```c
uint64 xrtNetEngineAfter(xnetengine* pEngine,
	uint64 iAffinity, uint64 iTimeout,
	xnettimerproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行中的 Engine |
| `iAffinity` | 输入 | 任意值 | 亲和键 |
| `iTimeout` | 输入 | 微秒 | 相对延迟；零表示尽快到期 |
| `pProc` | 输入 | 非空 | 终态回调 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | Timer ID | — |
| `0` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetEngineSchedule`（`ARGUMENT`/`CLOSED`/`AGAIN`/`RANGE`/`INTERNAL`）

#### 范例

[network/engine · 延迟任务](../../examples/network/engine/main.c) · 100 毫秒后触发

```c
(xrtNetEngineAfter(
	pEngine,
	1,
	100000u,
	exampleTimer,
	&State
) == 0) ) {
```

### `xrtNetEngineTimerCancel`
异步请求取消 Timer；成功只表示取消命令已进入目标 Worker。

```c
bool xrtNetEngineTimerCancel(xnetengine* pEngine, uint64 Id);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Timer 所属 Engine |
| `Id` | 输入 | 非零 | `Schedule`/`After` 返回的 ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 取消请求已入队；最终结果仍由唯一终态回调给出 | — |
| `false` | 请求未入队 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空或 `Id` 为零
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_TIMER` — Engine 未运行或停机封口
- `XERR_AGAIN` — 目标 Worker 命令队列达到容量
- `XERR_INTERNAL` — 唤醒失败

#### 范例

[network/engine_tour · 定时器](../../examples/network/engine_tour/main.c) · 异步取消长定时器

```c
if ( (IdFire == 0u) || (Timers.iLongId == 0u) ||
	!xrtNetEngineTimerCancel(pEngine, Timers.iLongId) ) {
	goto Cleanup;
}
```

### `xrtNetEngineTimerCancelCurrent`
只在 Timer 所属 Worker 上立即取消且不分配内存。不在所属 Worker、Timer 尚未入堆或已经终结时返回 `false`，且不修改线程错误。

```c
bool xrtNetEngineTimerCancelCurrent(
	xnetengine* pEngine,
	uint64 Id
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Timer 所属 Engine |
| `Id` | 输入 | 非零 | 要取消的 Timer ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已取消；终态回调以 `CANCELLED` 触发 | — |
| `false` | 不在所属 Worker、尚未入堆或已终结 | 不修改线程错误 |

#### 错误

- 无 — 失败路径刻意不设置错误（调用方可先试本函数，失败再走 `TimerCancel`）

#### 范例

[network/engine_tour · CancelCurrent](../../examples/network/engine_tour/main.c) · 同亲和回调内取消另一个 Timer

```c
pTimers->bCancelCurrentOk = xrtNetEngineTimerCancelCurrent(
	xrtNetWorkerEngine(pWorker), pTimers->iLongId);
```

### `xrtNetCompletionInit`
初始化一个借用过程和数据的端口 Completion；空指针是空操作，不设置错误。

```c
void xrtNetCompletionInit(xnetcompletion* pCompletion,
	xnetcompletionproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCompletion` | 输出 | 建议非空 | 调用方拥有的 Completion；空指针是空操作 |
| `pProc` | 输入 | 可空 | 终态回调 `Proc(worker, event, data)`，在所属 Worker 上执行 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化；存活期约束见上文 Completion 契约 |

#### 错误

- 无 — 初始化不失败，空指针静默忽略

#### 范例

[network/engine_tour · Completion](../../examples/network/engine_tour/main.c) · 借用过程与数据的初始化形态

```c
xrtNetCompletionInit(&Completion, exampleCompletionProc,
	(ptr)&bTaskDone);
```

### `xrtNetWorkerStats`
读取一个 Worker 的统计快照。

```c
bool xrtNetWorkerStats(const xnetworker* pWorker,
	xnetworkerstats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 目标 Worker |
| `pStats` | 输出 | 非空 | 接收并发快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/engine_tour · 统计](../../examples/network/engine_tour/main.c) · 任务执行后核对计数

```c
if ( !xrtNetWorkerStats(pWorker0, &WorkerStats) ||
	(WorkerStats.PostsExecuted < 1u) ) {
	goto Cleanup;
}
```

### `xrtNetEngineStats`
聚合全部 Worker 的统计快照。

```c
bool xrtNetEngineStats(const xnetengine* pEngine,
	xnetenginestats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Engine 指针 |
| `pStats` | 输出 | 非空 | 接收聚合快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 聚合快照已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/engine_tour · 统计](../../examples/network/engine_tour/main.c) · 全 Worker 聚合计数

```c
if ( !xrtNetEngineStats(pEngine, &EngineStats) ||
	(EngineStats.PostsExecuted < 2u) ||
	(EngineStats.TimersFired < 2u) ) {
	goto Cleanup;
}
```

## 名称解析（Resolver）

### `xrtNetResolverConfigInit`
写入兼顾桌面与高并发服务的默认配置：`Workers=2`、`RequestLimit=8192`、`QueryLimit=4096`、`CacheEntries=256`、`SuccessTTL=60s`、`FailureTTL=5s`、`HostLimit=1024`。

```c
void xrtNetResolverConfigInit(xnetresolverconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 建议非空 | 接收默认配置；空指针是空操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[network/resolve_tour · 创建](../../examples/network/resolve_tour/main.c) · 初始化后直接创建

```c
xrtNetResolverConfigInit(&Config);
pResolver = xrtNetResolverCreate(&Config);
```

### `xrtNetResolverCreate`
创建并立即启动独立解析工作池；空配置使用默认值（内部先取默认再整体复制调用方配置）。

```c
xnetresolver* xrtNetResolverCreate(
	const xnetresolverconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空指针 | 空指针用默认值；非空时字段任一上限为零或不支持即失败 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 运行中的 Resolver | — |
| `NULL` | 配置非法或资源不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XNET_ERROR_RESOLVER_CREATE` — 配置含零值或不支持的限额（如 `Workers > 32`、`RequestLimit == 0`）
- 内存分配失败 — 结构或线程池创建失败

#### 范例

[network/resolve_tour · 创建](../../examples/network/resolve_tour/main.c) · 默认配置 + 初始统计核对

```c
pResolver = xrtNetResolverCreate(&Config);
if ( (pResolver == NULL) ||
	!xrtNetResolverStats(pResolver, &Stats) ||
	(Stats.Submitted != 0u) ) {
	goto Cleanup;
}
```

### `xrtNetResolverDestroy`
排空已受理请求并等待全部回调；必须与其他 Resolver 所有者操作串行，返回后指针失效。

```c
bool xrtNetResolverDestroy(xnetresolver* pResolver);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 允许空指针 | 空指针是空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已排空并销毁 | — |
| `false` | 参数非法、状态非法或回调内部失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_RESOLVER_CLOSED` — 从 Resolver 自己的 Worker 回调内调用，或已在关闭/已销毁

#### 范例

[network/resolve_tour · 收尾](../../examples/network/resolve_tour/main.c) · 销毁失败以退出码暴露

```c
if ( (pResolver != NULL) &&
	!xrtNetResolverDestroy(pResolver) ) {
	iResult = 2;
}
```

### `xrtNetResolverResolve`
提交主机查询；同一规范化主机与地址族只执行一次底层查询，命中缓存立即返回已关联的 Op。

```c
xnetresolveop* xrtNetResolverResolve(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	xnetresolveproc pDone,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | 运行中的 Resolver |
| `sHost` | 输入 | 非空、以零结尾 | 主机名；长度不得超过 `HostLimit` |
| `Family` | 输入 | `UNSPEC`/`IPV4`/`IPV6` | 目标地址族 |
| `pDone` | 输入 | 非空 | 完成回调，在 Resolver Worker 上恰好执行一次 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 解析操作（引用归调用方，用后 `ResolveOpDestroy`） | — |
| `NULL` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pResolver`/`sHost`/`pDone` 为空
- `XERR_VALUE` + `XNET_ERROR_FAMILY` — `Family` 不是三种支持值之一
- `XERR_CLOSED` + `XNET_ERROR_RESOLVER_CLOSED` — Resolver 已关闭或正在关闭
- `XERR_RANGE` + `XNET_ERROR_RESOLVER_SUBMIT` — 主机名超过 `HostLimit`
- `XERR_AGAIN` + `XNET_ERROR_RESOLVER_SUBMIT` — 在途请求达到 `RequestLimit`，或唯一查询数达到 `QueryLimit`

#### 范例

[network/resolve_tour · 查询](../../examples/network/resolve_tour/main.c) · 提交 localhost 并核对初始状态

```c
pOperation = xrtNetResolverResolve(pResolver, "localhost",
	XNET_FAMILY_IPV4, exampleResolveDone, (ptr)&State);
```

### `xrtNetResolverClear`
清空成功和失败缓存；已经运行或排队的查询不受影响。

```c
bool xrtNetResolverClear(xnetresolver* pResolver);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | 运行中的 Resolver |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 缓存已清空 | — |
| `false` | 参数非法或 Resolver 已关闭 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pResolver` 为空
- `XERR_STATE` + `XNET_ERROR_RESOLVER_CLOSED` — 正在关闭或已销毁

#### 范例

[network/resolve_tour · 清缓存](../../examples/network/resolve_tour/main.c) · 清空后缓存计数归零

```c
if ( !xrtNetResolverClear(pResolver) ) {
	goto Cleanup;
}
```

### `xrtNetResolverStats`
取得 Resolver 的并发一致统计快照。

```c
bool xrtNetResolverStats(
	const xnetresolver* pResolver,
	xnetresolverstats* pStats
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | Resolver 指针 |
| `pStats` | 输出 | 非空 | 接收快照（Submitted/Resolved/CachedResults 等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/resolve_tour · 统计](../../examples/network/resolve_tour/main.c) · 查询后核对计数推进

```c
if ( !xrtNetResolverStats(pResolver, &Stats) ||
	(Stats.Submitted < 1u) ||
	(Stats.Resolved < 1u) ) {
	goto Cleanup;
}
```

### `xrtNetResolveOpRef`
增加解析操作引用并返回原指针。跨线程保留 Op（回调后仍要查询）必须先取引用。

```c
xnetresolveop* xrtNetResolveOpRef(xnetresolveop* pOperation);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1；之后必须多一次 `ResolveOpDestroy` | — |
| `NULL` | 参数非法或引用耗尽 | 参数非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOperation` 为空

#### 范例

[network/resolve_tour · OpRef](../../examples/network/resolve_tour/main.c) · 共享引用后双份销毁

```c
pRef = xrtNetResolveOpRef(pOperation);
if ( (pRef == NULL) || (pRef != pOperation) ) {
	goto Cleanup;
}
xrtNetResolveOpDestroy(pRef);
xrtNetResolveOpDestroy(pOperation);
```

### `xrtNetResolveOpDestroy`
释放解析操作引用；空指针视为空操作。归零时释放操作与关联结果。

```c
void xrtNetResolveOpDestroy(xnetresolveop* pOperation);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 允许空指针 | 要释放引用的操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[network/resolve_tour · OpRef](../../examples/network/resolve_tour/main.c) · 每份引用各自配对销毁

```c
xrtNetResolveOpDestroy(pRef);
xrtNetResolveOpDestroy(pOperation);
```

### `xrtNetResolveOpCancel`
协作取消尚未进入终态的操作；回调仍在 Resolver Worker 上执行一次（以 `CANCELLED` 终态）。

```c
bool xrtNetResolveOpCancel(xnetresolveop* pOperation);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 要取消的操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 取消请求已受理（或查询组已在取消） | — |
| `false` | 参数非法或操作已进入终态 | 参数非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOperation` 为空
- 无额外错误 — 已终态的取消失败是查询结果，不设置错误

#### 范例

[network/resolver · 超时取消](../../examples/network/resolver/main.c) · 等待截止后协作取消

```c
if ( xrtDeadlineExpired(iDeadline) ) {
	(void)xrtNetResolveOpCancel(pOperation);
	break;
}
```

### `xrtNetResolveOpState`
返回解析操作当前状态的原子快照。

```c
xnetresolveopstate xrtNetResolveOpState(
	const xnetresolveop* pOperation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XNET_RESOLVE_PENDING` | 已受理、尚未开始底层查询 |
| `XNET_RESOLVE_RUNNING` | 底层查询进行中 |
| `XNET_RESOLVE_RESOLVED` | 成功终态（结果可取） |
| `XNET_RESOLVE_FAILED` | 失败终态（错误可借） |
| `XNET_RESOLVE_CANCELLED` | 取消终态 |

#### 范例

[network/resolve_tour · 状态机](../../examples/network/resolve_tour/main.c) · 提交时未完成、回调后已解析

```c
if ( (pOperation == NULL) ||
	(xrtNetResolveOpState(pOperation) ==
		XNET_RESOLVE_RESOLVED) ) {
	goto Cleanup;
}
```

### `xrtNetResolveOpResult`
成功时返回增加引用的完整地址列表，其他状态返回空指针并设置对应错误。

```c
xnetaddrlist* xrtNetResolveOpResult(
	const xnetresolveop* pOperation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 地址列表（引用 +1，调用方 `xrtNetAddrListDestroy`） | — |
| `NULL` | 未终态或非成功终态 | 错误经 `xrtGetError()` 报告；失败详情另见 `ResolveOpError` |

#### 错误

- `XERR_ARGUMENT` — `pOperation` 为空
- `XERR_VALUE` + `XNET_ERROR_RESOLVER_QUERY` — 操作尚未完成（终态前查询结果）
- 失败/取消状态 — `NULL`，结构化错误经 `ResolveOpError` 借用返回

#### 范例

[network/resolve_tour · 结果](../../examples/network/resolve_tour/main.c) · 返回的列表由调用方销毁

```c
pList = xrtNetResolveOpResult(pOperation);
if ( (pList == NULL) ||
	(xrtNetAddrListCount(pList) < 1u) ) {
	goto Cleanup;
}
xrtNetAddrListDestroy(pList);
```

### `xrtNetResolveOpError`
失败或取消时返回借用的结构化错误，其他状态返回空指针。

```c
const xerror* xrtNetResolveOpError(
	const xnetresolveop* pOperation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用的 `xerror`，存活到操作销毁；不转移所有权 | — |
| `NULL` | 非失败/取消状态 | 非错误状态，不设置线程错误 |

#### 错误

- 无 — 空返回是状态查询结果而非错误

#### 范例

[network/resolver · 失败路径](../../examples/network/resolver/main.c) · 借用错误打印消息

```c
const xerror* pError = xrtNetResolveOpError(pOperation);

fprintf(stderr, "%s\n", pError != NULL ?
	xrtErrorMessage(pError) : "resolve failed");
```

### `xrtNetResolveAsync`
把 Resolver 查询包装为 Future；成功值是由 Future 持有的地址列表。

```c
xfuture* xrtNetResolveAsync(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | 运行中的 Resolver |
| `sHost` | 输入 | 非空 | 主机名 |
| `Family` | 输入 | 三种支持值 | 目标地址族 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future：成功值 `xnetaddrlist*`（Future 持有），失败值 `xerror` | — |
| `NULL` | 提交失败（同 `ResolverResolve`）或 Future 分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetResolverResolve`（`ARGUMENT`/`VALUE`/`CLOSED`/`RANGE`/`AGAIN`）
- 内存分配失败 — Future 包装失败

#### 范例

[network/resolver_future · Future 形态](../../examples/network/resolver_future/main.c) · 提交后 `xrtFutureWaitFor` 等待

```c
pFuture = xrtNetResolveAsync(
	pResolver,
	"localhost",
	XNET_FAMILY_UNSPEC
);
```

## 模块契约：错误

本文件与 [net.md](net.md) 共享 `include/xrt/net.h` 的错误体系；失败经 `xrtGetError()` 报告，`SystemCode` 保存平台码：

| 域/种类 | 触发场景 |
|---|---|
| `xrt.net` / `XNET_ERROR_DNS_RESOLVE`·`DNS_REVERSE`·`DNS_RESULT` | 名称服务解析、反查与结果转换失败 |
| `XERR_AGAIN` / `XERR_CLOSED` | 资源暂不可用 / 对象已关闭 |
| `XERR_UNSUPPORTED`·`XERR_IO`·`XERR_STATE`·`XERR_RANGE`·`XERR_VALUE` | 能力、系统调用、状态、范围与配置类失败 |
