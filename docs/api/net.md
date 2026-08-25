# 网络地址基础 API

## 设计契约

`XRT_FEATURE_NET` 是网络体系最底层的独立裁剪单元，只依赖 `core`。这一层不创建 Socket，不执行 DNS，也不依赖线程、任务或协程；它负责稳定的 IP 地址表示、数字地址与端点解析、规范输出、地址分类和平台 `sockaddr` 转换。

公开头文件不包含 Winsock 或 POSIX Socket 头文件。`xnetaddr` 因此可以直接进入 FFI 值类型、容器键、配置结构和协议对象，不会把平台 ABI 扩散到上层。Windows 网络运行时由后续 Socket 操作按需初始化，使用者不需要配对调用启动与清理函数。

```c
typedef enum xnetfamily {
	XNET_FAMILY_UNSPEC = 0,
	XNET_FAMILY_IPV4 = 4,
	XNET_FAMILY_IPV6 = 6
} xnetfamily;

typedef struct xnetaddr {
	uint16 Family;
	uint16 Port;
	uint32 Scope;
	uint8 Address[16];
} xnetaddr;
```

`Port` 使用主机字节序，`Address` 始终使用网络字节序。IPv4 只使用前 4 字节；`Scope` 只对 IPv6 有意义。地址族常量是 XRT 自己的稳定值，不能与 `AF_INET`、`AF_INET6` 混用。

## 构造与解析

```c
bool xrtNetAddrAny(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
bool xrtNetAddrLoopback(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
bool xrtNetAddrParse(xnetaddr* pAddr, cstr sIP, uint16 iPort);
bool xrtNetAddrParseEndpoint(xnetaddr* pAddr, cstr sEndpoint, uint16 iDefaultPort);
```

`Any` 构造 `0.0.0.0` 或 `::`；`Loopback` 构造 `127.0.0.1` 或 `::1`。族不合法时失败且不修改输出。

`xrtNetAddrParse` 只解析数字地址，不隐式执行 DNS：

- IPv4 必须是四段十进制，拒绝越界、缺段和带前导零的歧义形式。
- IPv6 支持 `::`、嵌入式 IPv4 和 `%42` 数字 Scope。
- 启用独立的 `XRT_FEATURE_NET_INTERFACE` 后，也接受 `%eth0` 一类接口名称
  Scope；未启用时基础层不会访问系统接口表。
- 失败不修改 `*pAddr`。

`xrtNetAddrParseEndpoint` 接受 `127.0.0.1:80`、`[::1]:80`、裸 `::1` 和不带显式端口的 `[::1]`。裸 IPv6 的最后一段不会被猜测为端口；IPv6 需要显式端口时必须使用方括号。地址部分直接按切片解析，不复制到固定长度临时数组。端口允许 `0`，因此同一接口可用于服务器动态端口绑定。

主机名与服务名解析属于独立 DNS 模块，不塞进地址语法函数。这样数字地址路径保持零分配、确定性和无阻塞。

## 文本输出

```c
size_t xrtNetAddrText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
size_t xrtNetAddrEndpointText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
str xrtNetAddrString(const xnetaddr* pAddr);
str xrtNetAddrEndpointString(const xnetaddr* pAddr);
```

两个 `Text` 函数返回不含结尾零字节的所需长度。传入 `NULL, 0` 可零分配查询大小；容量不足时仍返回所需长度、尽可能写入零结尾文本，并设置 `XNET_ERROR_BUFFER`。其他失败返回 `XRT_NPOS`。

IPv6 按 RFC 5952 输出小写、无前导零并压缩第一个最长零段。IPv4 映射地址输出为 `::ffff:192.0.2.1`。端点输出始终包含端口，IPv6 始终带方括号。

两个 `String` Helper 返回拥有字符串，调用方使用 `xrtFree` 释放。它们替换了旧版线程局部环形缓冲，因此结果可以长期保存、跨函数传递并在同一表达式中多次使用。

## 比较与分类

```c
bool xrtNetAddrEqual(const xnetaddr* pLeft, const xnetaddr* pRight);
bool xrtNetAddrSameIP(const xnetaddr* pLeft, const xnetaddr* pRight);
int xrtNetAddrCompare(const xnetaddr* pLeft, const xnetaddr* pRight);
```

`Equal` 比较完整端点；`SameIP` 忽略端口；两者都把 IPv6 Scope 纳入比较。`Compare` 按地址族、地址、Scope、端口提供稳定全序，可用于排序、二叉树和确定性去重。

```c
bool xrtNetAddrIsUnspecified(const xnetaddr* pAddr);
bool xrtNetAddrIsLoopback(const xnetaddr* pAddr);
bool xrtNetAddrIsMulticast(const xnetaddr* pAddr);
bool xrtNetAddrIsLinkLocal(const xnetaddr* pAddr);
bool xrtNetAddrIsPrivate(const xnetaddr* pAddr);
bool xrtNetAddrIsMapped(const xnetaddr* pAddr);
bool xrtNetAddrUnmap(const xnetaddr* pAddr, xnetaddr* pResult);
```

`Private` 只表示 RFC 1918 IPv4 和 RFC 4193 IPv6 范围，不把回环、链路本地、文档地址或所有非公网地址混在一起。调用方可以按安全策略组合多个明确谓词。

`IsMapped` 识别 IPv4 映射 IPv6；`Unmap` 转为普通 IPv4 并保留端口。非映射地址原样复制，允许调用方统一执行规范化而不增加分支。

## Native 逃生口

```c
bool xrtNetAddrToNative(const xnetaddr* pAddr, void* pNative, size_t* pSize);
bool xrtNetAddrFromNative(xnetaddr* pAddr, const void* pNative, size_t iSize);
```

`ToNative` 在 `pNative == NULL` 时通过 `*pSize` 返回所需的 `sockaddr_in` 或 `sockaddr_in6` 大小；缓冲不足也会更新大小。`FromNative` 接受平台 `sockaddr`，检查地址族和结构长度。两者都保留端口和 IPv6 Scope。

这组接口是有意保留的底层扩展路径：自定义 Socket 选项、第三方事件循环和上层协议可以直接连接平台 API，不需要复制 XRT 内部实现，也不会迫使公开地址结构绑定平台头文件。

## 网络缓冲

`XRT_FEATURE_NET_BUFFER` 依赖 `XRT_FEATURE_NET`，提供 TCP、UDP、TLS、HTTP 和 WebSocket 共用的可变尺寸缓冲底座。它保留旧版 `xnetchain` 的分块、Span 和引用数据优势，但删除“每个连接常驻固定 8K 缓冲”的模型。

缓冲只在实际收到或排队数据时持有块。默认池尺寸类为 512、2048、8192、32768 字节；超过最大类的请求按实际大小单独分配。默认总缓存硬上限为 2 MiB，并且缓存属于 worker，而不是连接：一万个空闲连接不会因此各自占用 8K。

### 池

```c
typedef struct xnetbufpoolconfig {
	size_t BlockSize[4];
	size_t CacheLimit[4];
	size_t MaxCacheBytes;
} xnetbufpoolconfig;

void xrtNetBufPoolConfigInit(xnetbufpoolconfig* pConfig);
xnetbufpool* xrtNetBufPoolCreate(const xnetbufpoolconfig* pConfig);
bool xrtNetBufPoolDestroy(xnetbufpool* pPool);
size_t xrtNetBufPoolTrim(xnetbufpool* pPool, size_t iRetainBytes);
void xrtNetBufPoolGet(const xnetbufpool* pPool, xnetbufpoolinfo* pInfo);
```

池是线程归属对象，不在热路径加锁。一个网络 worker 持有一个池；同一时刻只能由该 worker 操作池及其块。不同池可以并行。跨线程长期保存数据时，使用不绑定池的缓冲、复制到调用方内存，或让所属 worker 执行最终释放。

`Destroy` 在仍有实时块时返回 `false` 和 `XNET_ERROR_POOL_BUSY`，不会留下块中的悬空池指针。块即使被 `Move` 到另一个缓冲，也仍计入原池实时统计。`Trim` 只释放缓存块，不影响实时数据。

`xnetbufpoolinfo` 提供实时/峰值块数和容量、缓存块数和容量、底层分配、复用、动态大块和外部引用计数。`LiveBytes` 对拥有块统计容量，对引用块统计逻辑引用长度，用于诊断实际压力和在途数据。

### 生命周期与查询

```c
bool xrtNetBufInit(xnetbuf* pBuffer, xnetbufpool* pPool);
void xrtNetBufClear(xnetbuf* pBuffer);
size_t xrtNetBufSize(const xnetbuf* pBuffer);
bool xrtNetBufEmpty(const xnetbuf* pBuffer);
size_t xrtNetBufSpanCount(const xnetbuf* pBuffer);
size_t xrtNetBufSpans(const xnetbuf* pBuffer, xnetspan* pSpans, size_t iCapacity);
bool xrtNetBufFront(const xnetbuf* pBuffer, xnetspan* pSpan);
```

`xnetbuf` 可以栈上或嵌入连接对象。传入空池时使用全局分配器且不缓存，适合跨线程所有权和低频独立使用。`Clear` 释放全部块并放弃未提交预留，结构保持可继续使用。

`xnetspan` 只借用块内数据。任何追加、预留提交、消费、Pullup、Move 或 Clear 后都必须重新获取 Span。`Spans` 返回实际写入数组的数量；完整所需数量由 `SpanCount` 查询。`Front` 只在存在非空首段时返回 `true`；空缓冲会把输出清为 `{ NULL, 0 }` 并返回 `false`。合法的 UDP 零长度数据报由 UDP 消息自身的 `Size == 0` 表达，不依赖空缓冲视图。

### 四种写入所有权

```c
bool xrtNetBufAppend(xnetbuf* pBuffer, const void* pData, size_t iSize);
bool xrtNetBufPrepend(xnetbuf* pBuffer, const void* pData, size_t iSize);
bool xrtNetBufAppendBorrow(xnetbuf* pBuffer, const void* pData, size_t iSize);
bool xrtNetBufAppendTake(xnetbuf* pBuffer, ptr pData, size_t iSize);
bool xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData,
	size_t iSize, xnetreleaseproc pRelease, ptr pContext);
```

- `Append`：复制数据。整个追加具有失败原子性，OOM 不会留下部分数据。
- `Prepend`：把协议头复制到新首块，不移动或复制已有负载块。
- `AppendBorrow`：零复制借用，调用方保证数据存活到该段被消费或清除。
- `AppendTake`：接管由 `xrtMalloc` 家族分配的数据，最终调用 `xrtFree`。
- `AppendRef`：接管带自定义释放过程的数据，最后一部分离开缓冲时执行一次释放过程。

`Take` 和 `Ref` 只有成功后才转移所有权；失败时调用方仍负责数据。部分消费不会提前释放引用块。

### 直接接收与编码

```c
bool xrtNetBufReserve(xnetbuf* pBuffer, size_t iMinimum, xnetwspan* pSpan);
bool xrtNetBufCommit(xnetbuf* pBuffer, size_t iSize);
bool xrtNetBufCancel(xnetbuf* pBuffer);
```

`Reserve` 返回至少 `iMinimum` 字节的连续可写区。IOCP、io_uring、`recv`、TLS 解密器和协议编码器可以直接把结果写进该区，完成后 `Commit` 实际字节数；EAGAIN、取消或零字节结果使用 `Cancel`。预留期间不能执行其他改变链结构的操作，重复预留或错误提交会返回 `XNET_ERROR_BUFFER_STATE`，原预留仍可正确提交或取消。

后端的推荐路径是：为一次接收创建空缓冲，Reserve 自适应块，直接把 Span 提交给操作系统，完成后 Commit，再把整条缓冲 Move 到连接接收队列。这个路径没有“固定 8K 接收区 -> 再复制到 Chain”的第二份内存和复制开销。

### 协议操作

```c
bool xrtNetBufMove(xnetbuf* pTarget, xnetbuf* pSource);
bool xrtNetBufPullup(xnetbuf* pBuffer, size_t iSize, xnetspan* pSpan);
size_t xrtNetBufPeek(const xnetbuf* pBuffer,
	size_t iOffset, void* pOutput, size_t iSize);
size_t xrtNetBufRead(xnetbuf* pBuffer, void* pOutput, size_t iSize);
size_t xrtNetBufFind(const xnetbuf* pBuffer, uint8 iByte, size_t iOffset);
size_t xrtNetBufConsume(xnetbuf* pBuffer, size_t iSize);
```

`Move` 只重连块链，不复制负载；源缓冲恢复为空但保留自己的池配置。`Pullup` 在首块足够时零复制返回，否则只复制指定前缀，适合解析固定协议头。`Peek` 不消费，`Read` 复制并消费，`Find` 跨块查找，`Consume` 返回实际消费量并允许请求超过剩余数据。

## 错误

网络错误域是 `xrt.net`，稳定代码为：

| 代码 | 含义 |
| --- | --- |
| `XNET_ERROR_NONE` | 没有网络错误；用于统计快照等非失败状态 |
| `XNET_ERROR_FORMAT` | 数字地址或端点语法错误 |
| `XNET_ERROR_FAMILY` | 地址族不受支持 |
| `XNET_ERROR_PORT` | 端口为空、非十进制或越界 |
| `XNET_ERROR_SCOPE` | IPv6 Scope 非法或越界 |
| `XNET_ERROR_BUFFER` | 文本或 native 输出缓冲不足 |
| `XNET_ERROR_NATIVE` | 平台地址结构截断或不支持 |
| `XNET_ERROR_SYSTEM` | 平台网络运行时失败 |
| `XNET_ERROR_DNS_RESOLVE` | 正向 DNS 解析失败 |
| `XNET_ERROR_DNS_REVERSE` | 反向 DNS 解析失败 |
| `XNET_ERROR_DNS_RESULT` | DNS 结果索引、地址族或输出非法 |
| `XNET_ERROR_RESOLVER_CREATE` | 异步 Resolver 配置或创建失败 |
| `XNET_ERROR_RESOLVER_SUBMIT` | Resolver 请求非法、超限或提交失败 |
| `XNET_ERROR_RESOLVER_CLOSED` | Resolver 已关闭或关闭过程失败 |
| `XNET_ERROR_RESOLVER_QUERY` | Resolver 后台查询失败 |
| `XNET_ERROR_BUFFER_STATE` | 缓冲预留、池配置或修改状态非法 |
| `XNET_ERROR_POOL_BUSY` | 缓冲池仍有实时块，不能销毁 |
| `XNET_ERROR_SOCKET_OPEN` | 创建或初始化 Socket 失败 |
| `XNET_ERROR_SOCKET_CLOSE` | 关闭 Socket 失败；对象仍立即失效 |
| `XNET_ERROR_SOCKET_OPTION` | Socket 选项非法、不支持或系统操作失败 |
| `XNET_ERROR_SOCKET_BIND` | Socket 绑定失败 |
| `XNET_ERROR_SOCKET_LISTEN` | Socket 监听失败 |
| `XNET_ERROR_SOCKET_ACCEPT` | 接受连接失败 |
| `XNET_ERROR_SOCKET_CONNECT` | 发起或完成连接失败 |
| `XNET_ERROR_SOCKET_SHUTDOWN` | 半关闭失败 |
| `XNET_ERROR_SOCKET_READ` | Socket 接收失败 |
| `XNET_ERROR_SOCKET_WRITE` | Socket 发送失败 |
| `XNET_ERROR_PORT_CREATE` | 事件端口配置、后端选择或初始化失败 |
| `XNET_ERROR_PORT_CLOSE` | 事件端口资源关闭失败 |
| `XNET_ERROR_PORT_WATCH` | readiness 观察非法、超限或后端不支持 |
| `XNET_ERROR_PORT_WAIT` | 事件等待参数或平台调用失败 |
| `XNET_ERROR_PORT_POST` | 用户事件队列满、投递或唤醒失败 |
| `XNET_ERROR_PORT_SUBMIT` | 完成式 IO 提交失败 |
| `XNET_ERROR_PORT_CANCEL` | 完成式 IO 取消失败 |
| `XNET_ERROR_ENGINE_CREATE` | Engine 配置、Worker 或资源创建失败 |
| `XNET_ERROR_ENGINE_START` | Engine 启动失败 |
| `XNET_ERROR_ENGINE_STOP` | Engine 停止或对象排空失败 |
| `XNET_ERROR_ENGINE_POST` | Worker 命令投递失败 |
| `XNET_ERROR_ENGINE_TIMER` | 定时器参数、容量或操作失败 |
| `XNET_ERROR_STREAM_CONFIG` | TCP Stream 配置或 Worker 限定操作非法 |
| `XNET_ERROR_STREAM_CREATE` | TCP Stream 创建失败 |
| `XNET_ERROR_STREAM_CONNECT` | TCP Stream 异步连接失败 |
| `XNET_ERROR_STREAM_READ` | TCP Stream 读取、缓冲或协议消费失败 |
| `XNET_ERROR_STREAM_WRITE` | TCP Stream 发送或写半关闭失败 |
| `XNET_ERROR_STREAM_CLOSE` | TCP Stream 关闭失败 |
| `XNET_ERROR_LISTENER_CREATE` | TCP Listener 配置或创建失败 |
| `XNET_ERROR_LISTENER_ACCEPT` | TCP Listener 接受或分发失败 |
| `XNET_ERROR_LISTENER_CLOSE` | TCP Listener 关闭失败 |

完整示例位于 `examples/network/address/main.c` 和 `examples/network/buffer/main.c`。

## Socket 原语

`XRT_FEATURE_NET_SOCKET` 只依赖 `XRT_FEATURE_NET`。这一层拥有平台 Socket 句柄，但不包含事件循环、线程、协程、隐藏收发缓冲、TCP 重连或 UDP 队列；TCP/UDP 客户端与服务器模型在它和事件端口之上组合。这样原生扩展可以直接使用轻量原语，高层对象也不会各自重复跨平台 Socket 处理。

```c
typedef struct xnetsocket_impl* xnetsocket;

typedef enum xnetresult {
	XNET_RESULT_ERROR = -1,
	XNET_RESULT_OK = 0,
	XNET_RESULT_AGAIN,
	XNET_RESULT_CLOSED,
	XNET_RESULT_TRUNCATED,
	XNET_RESULT_TIMEOUT,
	XNET_RESULT_CANCELLED
} xnetresult;

#define XNET_DGRAM_BATCH_MAX 64u

typedef enum xnetdgrammetaflag {
	XNET_DGRAM_META_DESTINATION = 0x0001,
	XNET_DGRAM_META_INTERFACE = 0x0002,
	XNET_DGRAM_META_HOP_LIMIT = 0x0004,
	XNET_DGRAM_META_TRAFFIC_CLASS = 0x0008,
	XNET_DGRAM_META_SEGMENT_SIZE = 0x0010,
	XNET_DGRAM_META_TRUNCATED = 0x40000000
} xnetdgrammetaflag;

typedef struct xnetdgrammeta {
	uint32 Flags;
	xnetaddr Destination;
	uint32 Interface;
	int HopLimit;
	int TrafficClass;
	uint32 SegmentSize;
} xnetdgrammeta;

typedef enum xnetdgramcontrolflag {
	XNET_DGRAM_CONTROL_SOURCE = 0x0001,
	XNET_DGRAM_CONTROL_INTERFACE = 0x0002,
	XNET_DGRAM_CONTROL_HOP_LIMIT = 0x0004,
	XNET_DGRAM_CONTROL_TRAFFIC_CLASS = 0x0008,
	XNET_DGRAM_CONTROL_SEGMENT_SIZE = 0x0010
} xnetdgramcontrolflag;

typedef struct xnetdgramcontrol {
	uint32 Flags;
	xnetaddr Source;
	uint32 Interface;
	int HopLimit;
	int TrafficClass;
	uint32 SegmentSize;
} xnetdgramcontrol;

typedef enum xnetdgramcap {
	XNET_DGRAM_CAP_PATH_MTU_MODE = 0x0001,
	XNET_DGRAM_CAP_PATH_MTU_QUERY = 0x0002,
	XNET_DGRAM_CAP_ERROR_QUEUE = 0x0004,
	XNET_DGRAM_CAP_SEGMENT_SEND = 0x0008,
	XNET_DGRAM_CAP_SEGMENT_RECEIVE = 0x0010
} xnetdgramcap;

typedef struct xnetdgramrecv {
	void* Data;
	size_t Capacity;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	size_t Size;
	xnetresult Result;
} xnetdgramrecv;

typedef struct xnetdgramsend {
	const xnetaddr* Remote;
	const void* Data;
	size_t Size;
} xnetdgramsend;

xnetsocket xrtNetSocketOpen(xnetfamily Family,
	xnetsockettype Type, uint32 iFlags);
bool xrtNetSocketClose(xnetsocket Socket);
intptr_t xrtNetSocketNative(xnetsocket Socket);
bool xrtNetSocketAvailable(xnetsocket Socket, size_t* pSize);
```

对象拥有原生句柄。`Close` 即使遇到系统关闭错误也销毁对象，不能重试关闭；POSIX `close` 被信号中断时同样不能重试，以免误关已经复用的文件描述符。生命周期操作不带隐藏锁，调用方必须把关闭与其他操作串行化；不同线程并行执行由操作系统允许的独立收发没有额外包装开销。

所有新句柄都禁止被子进程继承。Windows 使用 `WSA_FLAG_NO_HANDLE_INHERIT` 与 Overlapped Socket，在创建时同时建立不可继承和 IOCP 能力；不支持该标志的旧系统退回 `SetHandleInformation`。Linux 在平台支持时使用 `SOCK_CLOEXEC`，并通过 `SOCK_NONBLOCK` 原子建立初始非阻塞状态；接受连接时使用同口径的 `accept4`。其他 POSIX 平台以及旧内核退回 `fcntl` 二次设置。所有快路径之后仍会校验并补设 `FD_CLOEXEC` 或句柄继承位，不能建立安全属性时直接关闭句柄并报告错误，不返回半初始化对象。

`XNET_SOCKET_NONBLOCK` 决定初始阻塞模式，也可通过 `XNET_OPTION_NONBLOCK` 修改。`xrtNetSocketNative` 返回借用句柄，调用方可以连接第三方事件循环或平台专用选项，但不能自行关闭；通过 Native 修改状态后，调用方负责继续满足 XRT 契约。

`Available` 是底层查询能力：流式 Socket 返回当前可立即读取的字节数，数据报 Socket 返回下一报文可读长度。结果只代表查询瞬间，不能代替非阻塞读取、事件通知或背压策略；它主要用于自定义事件循环、诊断和按需选择接收缓冲。

### 生命周期与连接

```c
bool xrtNetSocketBind(xnetsocket Socket, const xnetaddr* pAddress);
bool xrtNetSocketListen(xnetsocket Socket, int iBacklog);
xnetresult xrtNetSocketAccept(xnetsocket Socket,
	xnetsocket* pClient, xnetaddr* pRemote);
xnetresult xrtNetSocketConnect(xnetsocket Socket,
	const xnetaddr* pRemote);
xnetresult xrtNetSocketFinishConnect(xnetsocket Socket);
bool xrtNetSocketShutdown(xnetsocket Socket, xnetshutdown Direction);
bool xrtNetSocketLocal(xnetsocket Socket, xnetaddr* pAddress);
bool xrtNetSocketRemote(xnetsocket Socket, xnetaddr* pAddress);
```

阻塞 Socket 的 `Connect` 和 `Accept` 直接等待系统完成。非阻塞操作暂时不能推进时返回 `XNET_RESULT_AGAIN`，这不是结构化错误；只有 `Connect` 返回 `AGAIN` 后，事件端口才能在观察到可写时调用 `FinishConnect` 读取 `SO_ERROR`，不能把“可写”直接当作连接成功。没有待完成连接时调用 `FinishConnect` 会返回契约错误。`Accept` 成功返回的对象继承 XRT 层记录的非阻塞模式，并再次显式设置平台状态，避免依赖各系统不同的继承规则。

`Bind` 要求地址族与 Socket 一致，允许端口为零；随后用 `Local` 取得实际端口。`Shutdown` 明确区分读、写和双向半关闭。流式接收遇到正常 EOF 返回 `XNET_RESULT_CLOSED`，连接复位等异常仍返回 `ERROR` 并保留系统代码，不把异常关闭伪装成正常 EOF。

### 收发、向量与报文批量

```c
xnetresult xrtNetSocketRecv(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived);
xnetresult xrtNetSocketSend(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent);
xnetresult xrtNetSocketRecvVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived);
xnetresult xrtNetSocketSendVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent);
xnetresult xrtNetSocketRecvFrom(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote);
xnetresult xrtNetSocketSendTo(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent,
	const xnetaddr* pRemote);
xnetresult xrtNetSocketRecvFromVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived,
	xnetaddr* pRemote);
xnetresult xrtNetSocketRecvMsg(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived,
	xnetaddr* pRemote, xnetdgrammeta* pMeta);
xnetresult xrtNetSocketRecvMsgVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived,
	xnetaddr* pRemote, xnetdgrammeta* pMeta);
xnetresult xrtNetSocketSendToVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent,
	const xnetaddr* pRemote);
xnetresult xrtNetSocketSendMsg(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl);
xnetresult xrtNetSocketSendMsgVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl);
xnetresult xrtNetSocketRecvBatch(xnetsocket Socket,
	xnetdgramrecv* pItems, size_t iCapacity, size_t* pReceived);
xnetresult xrtNetSocketSendBatch(xnetsocket Socket,
	const xnetdgramsend* pItems, size_t iCount, size_t* pSent);
```

标量和向量函数只执行一次有效系统 IO，成功短读和短写由实际字节数表达。输出计数会先清零，地址只在成功后更新。单次标量或向量总长度最多为 `INT_MAX`；向量最多 64 段，平台描述符存放在栈上，不在热路径分配。POSIX 发送路径抑制 `SIGPIPE`，错误通过返回值和 `xrt.net` 结构化错误表达。

UDP 的零长度报文是有效报文，标量与向量 `Recv`/`RecvFrom` 都返回 `OK, 0` 并真正消费报文，不能与 TCP 的 `CLOSED` 混用；零长度发送也会真正发出一个报文。缓冲不足时返回 `XNET_RESULT_TRUNCATED`，`pReceived` 是实际复制进调用方缓冲的数据量，来源地址仍有效，且不会把跨平台正常截断伪装成结构化错误。

`RecvFrom`/`SendTo` 和对应向量函数保留一个系统报文边界；连接式 UDP 通过普通 `Connect` 配合 `Recv`/`Send` 使用，语义与显式地址版本一致。

`RecvBatch`/`SendBatch` 是独立报文批量原语，容量上限为 64，不包含队列、所有权转移或隐藏分配。函数在任何系统调用前校验整批缓冲与显式地址，因此无效后项不会造成有效前缀提前发送。空批次返回 `OK, 0`；没有推进且非阻塞操作需要等待时返回 `AGAIN, 0`；已经推进正前缀时返回 `OK` 并由输出计数给出前缀长度；后续硬错误返回 `ERROR`，输出计数仍保留已经完成的前缀。

接收项的 `Data/Capacity` 是输入，`Remote/Meta/Size/Result` 是输出。函数先把全部输出初始化为零地址、零元数据、零长度与 `AGAIN`；每个已消费报文独立返回 `OK` 或 `TRUNCATED`，截断长度仍是实际写入容量内的可见前缀。Socket 启用元数据后，`RecvBatch` 同时收集每个报文的控制消息；未启用时没有控制缓冲和解析开销。发送项的 `Remote == NULL` 表示使用连接式 UDP 的固定 Peer。Linux 模块化构建使用 `recvmmsg/sendmmsg`；其他平台和未在包含单头文件前启用 `_GNU_SOURCE` 的 Linux 单头文件构建使用同契约的有界回退。阻塞接收回退只消费首个报文，避免为了填满批次产生隐藏的第二次阻塞。

### 数据报元数据与逐包发送控制

```c
uint32 xrtNetSocketDgramMetaAvailable(xnetsocket Socket);
uint32 xrtNetSocketDgramMetaEnabled(xnetsocket Socket);
bool xrtNetSocketDgramMetaSet(xnetsocket Socket, uint32 iFlags);
uint32 xrtNetSocketDgramControlAvailable(xnetsocket Socket);
uint32 xrtNetSocketDgramCapabilities(xnetsocket Socket);
```

接收元数据默认关闭。`Available` 在实际 Socket 和地址族上探测平台选项，返回可请求字段；调用方可以选取所需子集，再用 `DgramMetaSet` 精确启用。成功返回后 `Enabled` 与请求完全一致。原生套接字选项按组提交，罕见的中途系统失败可能已经改变前面的选项；此时函数返回失败，`Enabled` 返回实际已经生效的字段，调用方应关闭 Socket 或显式重设。配置必须由 Socket owner 在没有在途接收时串行执行。

`RecvMsg`/`RecvMsgVec` 与 `RecvFrom` 保持相同报文、零长度和载荷截断语义，并额外返回已启用字段。未启用任何字段时它们退回普通接收并将 `Meta` 清零；普通 `RecvFrom` 始终不请求控制消息。`Flags` 决定每个输出字段是否有效，平台未随该报文返回某字段时对应位保持零。`Destination` 表示报文到达的本地目标 IP，端口恒为零；IPv6 Scope 和 `Interface` 使用接口索引；Hop Limit 和 Traffic Class 保持系统返回的整数。Linux GRO 合并接收通过 `SEGMENT_SIZE` 返回原始数据报边界，最后一段允许短于该值。控制消息自身被截断时保留已经解析的字段，并增加只读结果位 `XNET_DGRAM_META_TRUNCATED`，该位不能用于配置。

发送控制与接收元数据是两个方向明确的结构，不能混用。调用方先用 `DgramControlAvailable` 查询当前 Socket、地址族和平台真正可构建的字段，再把所需子集写入 `xnetdgramcontrol.Flags`。`SOURCE` 要求具体、同地址族且端口为零的本地 IP，拒绝 `0.0.0.0` 和 `::`；IPv6 可以通过 `Source.Scope` 携带接口索引。`INTERFACE` 使用系统接口索引；Hop Limit 与 Traffic Class 的有效范围均为 0 到 255。`SEGMENT_SIZE` 要求 1 到 65535 字节、非空聚合载荷且最多产生 64 个数据报。未知、不可用或越界字段在系统调用前失败，不会静默忽略。

Windows 在运行期取得 `WSASendMsg` 后提供 Source 与 Interface；Linux 按地址族提供 Source、Interface、Hop Limit、Traffic Class，并在内核支持时提供 UDP GSO；BSD/Darwin 按系统头实际具备的 `IP_SENDSRCADDR`、`IPV6_PKTINFO`、`IPV6_HOPLIMIT` 和 `IPV6_TCLASS` 返回能力。`SendMsg`/`SendMsgVec` 只覆盖当前提交，不修改 Socket 默认选项；空控制或零 Flags 直接退回普通发送路径。同步调用期间借用控制和载荷。普通 `Send`/`SendTo` 不构建控制缓冲，也不承担这项开销。

### PMTU 与异步数据报错误

```c
uint32 xrtNetSocketDgramCapabilities(xnetsocket Socket);
xnetresult xrtNetSocketDgramRecvError(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived,
	xnetdgramerror* pError);
```

`DgramCapabilities` 是运行时能力查询，不以编译平台代替实际 Socket Provider。Windows 提供 PMTU，并在当前 Winsock Provider 支持 `UDP_SEND_MSG_SIZE`、`UDP_RECV_MAX_COALESCED_SIZE` 与消息扩展时提供分段发送和合并接收；Linux 提供 PMTU、错误队列，并在对应 `SOL_UDP` 选项可用时提供 GSO/GRO。其他平台返回零。PMTU 使用统一 `XNET_OPTION_PATH_MTU_MODE` 设置，使用只读 `XNET_OPTION_PATH_MTU` 查询；不支持的模式明确失败。

Windows 分段大小通过 `WSASendMsg` 的 `IPPROTO_UDP/UDP_SEND_MSG_SIZE` 控制消息逐包提交，不修改 Socket 全局状态，因此不会在并发发送之间串值。合并接收通过 `UDP_RECV_MAX_COALESCED_SIZE` 显式启用，`WSARecvMsg` 的 `UDP_COALESCED_INFO` 返回原始分段大小。合并属于允许发生的优化，不是每次接收的强制结果；未合并数据报保持原边界且不设置 `SEGMENT_SIZE`。启用合并后，接收缓冲仍由调用方提供，必须能够容纳期望的最大聚合载荷，否则按普通数据报截断契约返回。

Linux 错误队列由 `XNET_OPTION_DGRAM_ERRORS` 显式启用。`DgramRecvError` 使用 `MSG_ERRQUEUE` 非阻塞读取一个 ICMP、ICMPv6 或本地错误，空队列返回 `AGAIN`；函数同时返回原始数据报负载前缀和结构化 `xnetdgramerror`。`Flags` 决定 Remote、Offender、PathMtu 和截断字段是否有效。该结果属于异步协议状态，不覆盖当前线程错误。事件端口的 `xrtNetPortRecvError` 提供相同结果的完成式操作，io_uring 通过错误就绪轮询后读取错误队列实现唯一终态。

### 选项

```c
bool xrtNetSocketSet(xnetsocket Socket,
	xnetoption Option, int64 iValue);
bool xrtNetSocketGet(xnetsocket Socket,
	xnetoption Option, int64* pValue);
```

统一选项覆盖非阻塞、地址复用、Windows 独占地址、TCP NoDelay、KeepAlive、广播、IPv6-only、系统收发缓冲、linger、Hop Limit、Traffic Class 和只读 `SO_ERROR`。NoDelay、KeepAlive 和 linger 只接受流式 Socket，广播只接受数据报 Socket，不依赖平台碰巧接受无意义选项。负 linger 值关闭 linger，零表示 abortive close，正值为等待秒数。平台没有等价语义时返回 `XERR_UNSUPPORTED`，不会静默忽略；`REUSE_PORT` 与 `EXCLUSIVE_ADDRESS` 因此分别保持 POSIX 和 Windows 的真实边界。

### 多播

```c
bool xrtNetSocketMulticastJoin(xnetsocket Socket,
	const xnetaddr* pGroup, const xnetaddr* pInterface);
bool xrtNetSocketMulticastLeave(xnetsocket Socket,
	const xnetaddr* pGroup, const xnetaddr* pInterface);
bool xrtNetSocketMulticastLoop(xnetsocket Socket, bool bEnabled);
bool xrtNetSocketMulticastHopLimit(xnetsocket Socket, int iHopLimit);
bool xrtNetSocketMulticastInterface(xnetsocket Socket,
	const xnetaddr* pInterface);
```

多播 API 只接受数据报 Socket，组地址必须与 Socket 地址族一致且确实属于多播范围。IPv4 接收和发送接口使用本地接口地址；IPv6 使用 `xnetaddr.Scope` 中的接口索引。空接口表示系统默认，多播跳数的有效范围为 0 到 255。

Socket 原语层不添加隐式成员管理：加入和离开必须显式成对，调用方可以在同一 Socket 上管理多个组和接口。高层 Engine UDP 对象通过 `<xrt/udp.h>` 提供 Worker 串行化包装。

UDP 与 TCP 原语示例分别位于 `examples/network/socket/main.c` 和 `examples/network/socket_tcp/main.c`。

## 网络事件端口

`XRT_FEATURE_NET_PORT` 依赖 Socket、单调截止时间和 Mutex，提供 backend-neutral 事件端口核心；具体后端使用独立裁剪宏。`XRT_FEATURE_NET_PORT_SELECT` 提供全平台 select fallback，`XRT_FEATURE_NET_PORT_EPOLL` 提供 Linux 原生 readiness，`XRT_FEATURE_NET_PORT_KQUEUE` 提供 Darwin/BSD 原生 readiness，`XRT_FEATURE_NET_PORT_IOCP` 提供 Windows 原生完成式 IO，`XRT_FEATURE_NET_PORT_URING` 提供 Linux 原生完成式 IO。readiness 与 completion 使用同一组事件、截止时间、错误和所有权口径，但不会被强迫伪装成相同执行模型。

### Readiness 与 Completion

旧版 select/epoll/kqueue 为了模仿 IOCP，在每个后端中重复执行 recv/send、分配事件 Chain，并持有固定 8K 临时接收区。这既重复网络与缓冲逻辑，也让 TLS 等需要 `WANT_READ`/`WANT_WRITE` 的协议无法自然使用端口。

新端口通过能力位诚实区分两条路径：

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
	XNET_PORT_CAP_READ_PROBE = 0x0100
} xnetportcap;
```

- select/epoll/kqueue 只报告 readiness；transport 收到事件后使用 `xrtNetSocketRecv` 和 `xrtNetBufReserve/Commit` 排空到 `AGAIN`。
- IOCP/io_uring 使用 completion；transport 将调用方拥有的 Span 直接提交给内核，完成后提交实际字节数。
- `READ_PROBE` 表示 completion 后端可以不持有载荷缓冲等待流变为可读；readiness 后端直接使用 `Watch`，不重复公开该能力。
- 上层按 `xrtNetPortCapabilities` 只在后端边界分流一次，不需要每个协议复制平台分支。
- 端口不拥有每连接缓冲，也不解析 TCP、UDP、TLS 或应用协议。

### 创建与能力

```c
typedef struct xnetportconfig {
	xnetportbackend Backend;
	uint32 Flags;
	size_t PostLimit;
	size_t WatchLimit;
	size_t OperationLimit;
	size_t OperationCache;
} xnetportconfig;

void xrtNetPortConfigInit(xnetportconfig* pConfig);
xnetport* xrtNetPortCreate(const xnetportconfig* pConfig);
bool xrtNetPortDestroy(xnetport* pPort);
xnetportbackend xrtNetPortBackend(const xnetport* pPort);
bool xrtNetPortGetConfig(const xnetport* pPort,
	xnetportconfig* pConfig);
cstr xrtNetPortName(const xnetport* pPort);
uint32 xrtNetPortCapabilities(const xnetport* pPort);
```

`AUTO` 在已编译后端中选择当前平台最高优先级实现：Windows 优先 IOCP，Linux 优先 epoll，Darwin/BSD 优先 kqueue，其他平台当前使用 select；标准 Engine 同时编入 select 作为可显式选择的保底路径。显式后端不可用时返回 `XERR_UNSUPPORTED`，不会静默换后端。

默认配置为 `PostLimit=4096`、`WatchLimit=0`、`OperationLimit=0`、`OperationCache=64`。两个零值表示按实际后端自动选择硬上限，不表示禁用：select 的观察上限为 1024，epoll/kqueue 为 65536，其他后端为 4096；IOCP 的在途操作上限为 65536，其他后端为 4096。显式非零值始终作为调用方选择的硬边界。`xrtNetPortGetConfig` 返回实际后端和已经解析的容量，适合诊断、测试与部署检查；返回的结构是副本，修改它不会影响端口。

`PostLimit`、`WatchLimit` 和 `OperationLimit` 分别约束跨线程用户事件、readiness 观察和原生在途 IO。`OperationCache` 表示 completion 后端每个 256/512/1024/2048 字节尺寸类最多缓存的终态操作描述符数，零值完全关闭缓存。缓存只复用描述符和 Span 副本空间，不持有 Socket 或载荷缓冲；超过 2048 字节的罕见描述符直接使用堆。completion 操作 ID 索引从 16 个桶开始，活动操作超过两倍桶数时才逐级扩展到由 `OperationLimit` 确定的上限；因此为高并发配置较大的硬上限不会让空端口提前分配整张索引。扩展内存不足会让触发扩展的当前提交明确失败，已有在途操作保持有效。用户事件队列或操作队列满时返回 `XERR_AGAIN`；失败提交不会留下节点、幽灵事件或被系统继续引用的缓冲。`Wait` 每轮最多先提取输出容量的一半 Post；队列仍有积压时，下一轮先给原生后端一次非阻塞提取机会，因此即使调用方使用容量为一的事件数组，持续跨线程 Post 也不会永久饿死 Socket 完成或 readiness。已经从 Post 队列取出的事件不会因为同轮后端等待失败而丢失。

### 完成式 IO

```c
bool xrtNetPortAccept(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);
bool xrtNetPortConnect(xnetport* pPort, xnetsocket Socket,
	const xnetaddr* pRemote, uint64 Id, ptr pUser);
bool xrtNetPortReadProbe(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);
bool xrtNetPortRecv(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
bool xrtNetPortRecvVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
bool xrtNetPortSend(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, uint64 Id, ptr pUser);
bool xrtNetPortSendVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
bool xrtNetPortRecvFrom(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
bool xrtNetPortRecvFromVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
bool xrtNetPortRecvMsg(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
bool xrtNetPortRecvMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
bool xrtNetPortSendTo(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);
bool xrtNetPortSendToVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);
bool xrtNetPortSendMsg(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);
bool xrtNetPortSendMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);
bool xrtNetPortCancel(xnetport* pPort, uint64 Id);
```

完成式 API 以 bytes 为基础，不创建隐藏 `chain`，也没有每对象 8K 或每数据报 64K 固定缓冲。后端在提交时复制 Span 描述符与地址，但不复制载荷；成功提交后，Socket、缓冲和只读发送数据必须保持有效且不变，直到同一 `Id` 的终态事件到达。`SendMsg` 还会复制 `xnetdgramcontrol` 并为该操作按需保留平台控制缓冲，终态类型为 `SEND_MSG`；调用方可在提交返回后立即复用地址、Span 数组和控制对象。常规 `SendTo` 操作不携带控制状态。`ReadProbe` 只接受流 Socket，成功事件为 `READ_PROBE`、`Bytes == 0`，且不会消费字节；其后仍须提交 `Recv` 才能读取数据或确认 EOF。`RecvMsg` 只接受已启用接收元数据的数据报 Socket，终态类型为 `RECV_MSG`，来源地址写入事件 `Address`，有效元数据写入事件 `Meta`。常规 `RecvFrom` 操作描述符不携带控制缓冲，只有显式 `RecvMsg` 才增加固定的小型尾部状态。单个 Span 与一次操作的 Span 总长度最多为 `INT_MAX`，超限在进入系统前返回 `XERR_ARGUMENT` 或 `XERR_RANGE`。`Id` 必须非零并在当前端口全部在途操作中唯一。每次成功提交恰好产生一个对应类型的终态；短读和短写由 `Bytes` 表达，调用方决定是否继续提交。

终态失败位于事件的 `Result` 与 `SystemCode`，不把一次操作失败误报为端口等待失败。流接收零字节返回 `CLOSED|EOF`；零长度 UDP 报文返回 `OK`；UDP 缓冲不足返回 `TRUNCATED` 并保留实际写入长度、远端地址和已取得的元数据。事件的 `Address` 与 `Meta` 都是值对象，不借用平台控制缓冲。`Cancel` 只请求取消，成功后仍等待原操作唯一的 `CANCELLED` 终态；完成已经先于取消发生时，仍提取原完成。`Destroy` 会取消并排空全部在途操作，返回后系统不再引用调用方缓冲。

IOCP 的 Socket 首次提交时永久关联当前完成端口，同一端口后续提交不再执行关联系统调用；Windows 不支持把该 Socket 改绑到另一个 IOCP。关联身份使用进程内单调 owner 标识，不使用可被分配器复用的上下文地址，因此旧端口销毁后仍不会把原 Socket 误认成新端口成员。操作描述符只把 `OVERLAPPED`、活动索引和稳定事件字段放在公共头中，Accept、地址和接收标志等类型专属状态按需放在尾部；因此空闲读探针与常规流收发落入 256 字节尺寸类，不再为每个连接携带 `sockaddr_storage` 和 Accept 状态。正常关闭顺序是取消并提取终态、关闭 Socket、销毁端口；也可以直接销毁端口来同步取消并排空在途 IO，再关闭相关 Socket，但这些 Socket 不能继续提交或改绑到另一个端口。IOCP 提交、取消和等待属于 owner 线程，`Post` 与 `Wake` 可跨线程。

### Io_uring 边界

io_uring 是 Linux 原生 completion 后端，能力为 completion、原生取消、read probe、batch completion、wake 和 post。它直接映射稳定 Linux UAPI，不依赖 liburing；在提交时复制 `iovec`、地址描述符和显式发送控制值，载荷始终借用调用方内存。接收和发送统一使用 `RECVMSG`/`SENDMSG`，读探针使用 `POLL_ADD`，因此流、未连接 UDP、已连接 UDP、零长度报文、逐包发送控制与截断共用一套完成映射，不引入固定 2K 流缓冲或固定 64K 数据报缓冲。

普通操作只在用户态 SQ 中依次保留并发布 SQE，不为每条操作单独调用 `io_uring_enter`。
达到 SQ 容量、进入 `Wait` 或提交取消前，端口一次发布整批待提交项；内核只接受部分
SQE 或系统调用被信号中断时会继续提交剩余项。提交函数成功表示操作已经进入端口的
有序提交队列，后续批量系统调用失败由下一次 `Wait` 作为端口错误报告。取消会先发布
此前普通操作，再立即发布取消 SQE，保持“先提交、后取消”的顺序，不会让取消越过仍在
用户态 SQ 中的原操作。该批处理只合并系统调用，不改变每个操作唯一终态、缓冲借用期或
完成顺序契约。

创建端口时必须同时具有 `IORING_FEAT_NODROP`、`IORING_FEAT_FAST_POLL`，并通过 probe 确认 `POLL_ADD`、`SENDMSG`、`RECVMSG`、`ACCEPT`、`ASYNC_CANCEL` 与 `CONNECT`；能力不足会明确返回 `XERR_UNSUPPORTED`。环使用单生产者 SQ 与单消费者 CQ，提交、取消和等待属于 owner 线程；`Post` 与 `Wake` 通过非阻塞 `eventfd` 可跨线程且唤醒可合并。取消 SQE 的 `user_data` 使用带标记的操作令牌；原操作 CQE 和取消控制 CQE 都到达前，描述符退出公开活动索引但不会释放或进入缓存，避免地址复用后迟到取消误命中新操作。控制 CQE 不占公共事件容量，也不产生第二个用户终态。映射前会按内核返回的全部 SQ/CQ 偏移检查加法和数组乘法，32 位构建不会因尺寸回绕映射过小区域；映射后还会校验 entries 与 mask 的幂次契约。

`OperationLimit` 对 io_uring 的最大值为 32768，CQ 至少按活动操作与取消控制完成的总量配置。SQ dropped 或 CQ overflow 被视为端口一致性故障，不会继续交付可能缺失的终态。销毁会先请求取消并排空全部活动操作；只有端口进入不可恢复故障时才关闭 ring，利用内核关闭语义同步撤销剩余引用。

当前 `AUTO` 仍在 Linux 选择 epoll。io_uring 必须由 `XNET_PORT_URING` 显式选择，待 Linux 真实运行期的 TCP、UDP、取消、慢端、OOM 和长稳压力门禁在发布 runner 上持续稳定后再提升默认优先级；交叉编译和链接证据不冒充运行期证据。基础、OOM、预提交批量压力、示例和单头文件入口分别位于 `tests/network/test_net_port_uring*.c`、`examples/network/port_uring/main.c` 与 `tests/single/test_single_net_port_uring.c`。TCP、UDP、Future、Dial、慢对端和并发关闭沿用原有 transport 契约测试，Linux io_uring 入口为 `tests/network/test_net_*_uring.c`；单头 transport 入口为 `tests/single/test_single_net_tcp_uring.c` 与 `tests/single/test_single_net_udp_uring.c`，不复制另一套测试逻辑。

### 观察与等待

```c
bool xrtNetPortWatch(xnetport* pPort, xnetsocket Socket,
	uint64 Id, uint32 iEvents, ptr pUser);
bool xrtNetPortUnwatch(xnetport* pPort, xnetsocket Socket);
xnetresult xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	xdeadline iDeadline, size_t* pCount);
```

一个端口对一个 Socket 保留一份 readiness 观察。`Watch` 替换关注位和事件身份，零关注位等价于 `Unwatch`；`Unwatch` 幂等。成功返回表示内核观察和用户身份都已移除；失败返回仍会退休用户身份并屏蔽迟到事件，但调用方必须立即关闭该 Socket，不能继续观察或执行 IO。`Id` 与 `User` 原样进入事件，端口不拥有用户上下文。错误和挂断由后端隐式观察，不需要加入关注掩码。

readiness 采用 one-shot 契约：已报告的读写方向自动清除，transport 排空或推进状态机后显式重新观察。这样 level、edge 和 completion 后端都不会因未消费状态持续空转。`Wait` 使用 `xrtClock` 单调微秒截止时间；有事件返回 `OK`，到期返回 `TIMEOUT`，两者都会先清零 `*pCount`。定时器不再由每个后端各自维护链表，后续 engine 使用统一最小堆，并把最近截止时间直接传给 `Wait`。

`Watch`、`Unwatch` 和 `Wait` 属于端口 owner 线程；`Post` 与 `Wake` 可跨线程。关闭 Socket 前必须先移除仍在生效的观察，销毁端口前必须停止所有生产者。

### 用户事件与唤醒

```c
bool xrtNetPortPost(xnetport* pPort, uint64 Id, ptr pUser);
bool xrtNetPortWake(xnetport* pPort);
```

每次成功 `Post` 产生一个 FIFO `USER` 事件，不会合并。`Wake` 只请求一个可合并的 `WAKE` 事件，适合“命令队列已有工作”通知；连续 Wake 不会让 worker 重复空转。端口会合并底层通知而不合并用户事件，因此突发的数千次 `Post` 不需要执行同等数量的唤醒系统调用。事件入队与首次底层通知在同一临界区原子成立，`Post` 返回 `false` 时不会留下随后仍可提取的幽灵事件。select 后端使用两个非阻塞 UDP Socket 形成全平台唤醒通道；epoll 使用 `EFD_NONBLOCK|EFD_CLOEXEC` eventfd，并为不支持原子标志的旧内核补设 `O_NONBLOCK` 与 `FD_CLOEXEC`；kqueue 使用可合并的 `EVFILT_USER/NOTE_TRIGGER`，不再创建旧版 pipe。三者都只承载通知，不承载用户事件本体。

### Epoll 边界

epoll 是 Linux Tier A readiness 后端，能力为 readiness、内核 one-shot、batch wait、wake 和 post，不伪装 completion，也不在后端中执行 `recv`、`send` 或分配载荷缓冲。观察索引从 16 个桶开始，活动观察超过两倍桶数时才逐级扩展到由 `WatchLimit` 确定的上限，新增、替换和移除的平均复杂度为 O(1)；配置较大硬上限不会提高空端口成本。单次 `epoll_wait` 最多提取 256 个内核事件，调用方容量更小时严格服从调用方容量。

内核注册使用 level-triggered `EPOLLONESHOT`。一次事件只清除真正报告的读写方向，未报告方向会立即重新武装；`EPOLLHUP`、`EPOLLRDHUP` 会同时推进受关注的方向，使 transport 能执行最终读写并观察 EOF。每次 `Watch` 都生成包含 fd 与代际的内部令牌，迟到事件无法错配给被替换的身份或复用同一 fd 的新 Socket。`EPOLLERR` 只报告错误标志，不提前读取会清除待处理错误的 `SO_ERROR`；非阻塞连接由 `xrtNetSocketFinishConnect` 唯一读取并判定真实结果。观察节点只保存 Socket、身份、令牌和掩码，不包含旧版固定 8K 接收区，也不产生“临时接收区再复制到链”的第二次复制。`epoll_create1` 不可用时回退到 `epoll_create` 并显式设置 `FD_CLOEXEC`。

epoll 基础、OOM、数百 Socket 批量压力和单头文件回归分别位于 `tests/network/test_net_port_epoll*.c` 与 `tests/single/test_single_net_port_epoll.c`。

### Kqueue 边界

kqueue 是 Darwin 与 FreeBSD、OpenBSD、NetBSD、DragonFly BSD 的 Tier A readiness 后端，能力为 readiness、内核 one-shot、batch wait、wake 和 post。读写方向使用独立 `EV_ONESHOT` 过滤器，因此一个方向触发不会误删另一个方向；同一次 `kevent` 返回的同观察读写事件会合并为一个公共事件。`EV_EOF` 映射为挂断并推进对应方向，非零 `fflags` 与 `EV_ERROR` 保存到 `SystemCode`。Apple、FreeBSD 和 NetBSD 在头文件提供能力时使用 `EVFILT_USER` 唤醒；OpenBSD、DragonFly BSD 以及缺少该过滤器的目标使用非阻塞、禁止继承的 pipe，写满按已有唤醒合并，读事件会完整排空到 `EAGAIN`。

观察表分别按原生描述符和整数代际令牌建立有界哈希索引，两张索引均从 16 个桶开始并同步动态扩展，新增、替换、移除和迟到事件校验平均为 O(1)。内核 `udata` 不保存可释放节点指针；替换或 fd 复用后，旧令牌事件无法访问新对象。替换过滤器失败会优先恢复旧集合；若内核连恢复也失败，则尽力删除全部过滤器并移除用户态观察，绝不保留两边分叉的状态。OOM 或首次注册失败不会留下可见观察。后端不执行 Socket IO，不保存发送向量，不分配载荷 Chain，也没有旧版固定 8K 接收区。

kqueue 基础、OOM、数百 Socket 批量压力和单头文件回归分别位于 `tests/network/test_net_port_kqueue*.c` 与 `tests/single/test_single_net_port_kqueue.c`。当前 Windows 开发机只执行其不可用契约；Darwin/BSD 运行期结果属于对应平台发布门禁。

### Select 边界

select 是 Tier C fallback，能力为 readiness、one-shot、batch wait、wake 和 post，不宣称 completion 或 edge。Windows 的 `fd_set` 受 `FD_SETSIZE` 数量限制，内部唤醒 Socket 占一个槽位；POSIX 同时要求原生 fd 小于 `FD_SETSIZE`。用户 Socket 超限在 `Watch` 时确定性失败；进程已有大量文件导致内部唤醒 fd 无法表示时，端口直接在 `Create` 阶段返回 `XNET_ERROR_PORT_CREATE`，两条路径都不会进入越界的 `FD_SET`。高连接数部署应选择 IOCP、io_uring、epoll 或 kqueue。

select、epoll、kqueue、IOCP 和 io_uring 示例分别位于 `examples/network/port_select/main.c`、`examples/network/port_epoll/main.c`、`examples/network/port_kqueue/main.c`、`examples/network/port_iocp/main.c` 与 `examples/network/port_uring/main.c`。

## 网络 Engine

启用 `XRT_FEATURE_NET_ENGINE` 后，Engine 在事件端口之上提供固定 Worker、
有界跨线程命令、可取消 Timer 和统一 Completion 分发。它不实现 TCP、UDP、DNS
或应用协议，因此自定义传输与 XRT 高层传输可以建立在同一套运行时契约上。

标准 Engine 的平台闭包为 Windows `IOCP + select`、Linux `epoll + select`、Darwin/BSD `kqueue + select`、其他 POSIX 平台当前 `select`。Linux 的精细裁剪构建也允许 `io_uring + select`，不要求同时编入 epoll；此时调用方必须显式选择 `XNET_PORT_URING`，`AUTO` 仍只在已经编译的稳定默认后端中选择。显式 `XNET_PORT_SELECT` 可用于受限环境、诊断和后端差异回归；具体端口后端仍可脱离 Engine 独立裁剪。

### 配置

`xrtNetEngineConfigInit` 初始化 `xnetengineconfig`。配置没有版本字段，也没有旧版
兼容分支。

| 字段 | 默认值 | 契约 |
| --- | ---: | --- |
| `Backend` | `XNET_PORT_AUTO` | 每个 Worker 使用的端口后端 |
| `Workers` | `0` | 自动取在线处理器数，自动值最多 64，显式值最多 256 |
| `BufferPool` | `NULL` | 每 Worker 自适应缓冲池配置；空指针使用网络缓冲默认值 |
| `CommandCapacity` | 4096 | 每 Worker MPSC 命令容量，向上取整为 2 次幂 |
| `NodeCacheBytes` | 64 KiB | 每 Worker 命令、Timer 与协议小节点的共享缓存预算；零关闭缓存 |
| `TimerLimit` | 65536 | 已受理但尚未终结的 Timer 硬上限；同时受目标平台指针数组可表示长度约束 |
| `EventBatch` | 128 | 每 Worker 的端口事件批容量，最大 4096 |
| `PortPostLimit` | 4096 | 端口用户事件硬上限 |
| `PortWatchLimit` | 0 | readiness 观察硬上限；零由每个 Worker 的实际后端自动选择 |
| `PortOperationLimit` | 0 | completion 在途操作硬上限；零由每个 Worker 的实际后端自动选择 |
| `PortOperationCache` | 64 | 每 Worker、每 completion 操作尺寸类的缓存上限；零关闭 |
| `IdleWait` | 1000000 | 无命令、无 Timer 时的最大等待微秒数；端口等待失败时还作为退避上限输入 |
| `ThreadStack` | 0 | Worker 栈大小，零使用平台默认值 |

`NodeCacheBytes` 使用 64、128、256、512、1024 字节五个尺寸类，按实际流量惰性增长，
全部尺寸类共享同一个每 Worker 字节硬上限。缓存承载 Engine 命令、Timer、TCP 接受
分发、Dial 候选与发送元数据、TCP/UDP Future 等待节点、TLS Stream 小型 Future 节点、UDP
发送元数据，不给每个连接、Future 或数据报预留固定空间。超过 1 KiB 的节点直接使用
全局堆；零预算只关闭这一层
Worker 缓存，不改变 xrt 全局堆自己的尺寸类策略。Worker 停止时释放全部缓存节点，
重新启动后按需建立。

活动网络对象只有在自己的等待锁内确认 Worker 仍然有效后，才为缓存节点取得一个临时
Engine 生命周期租约；节点先归还 Worker 缓存，再释放该租约。进入终态后新建的 Future
节点不再访问 Worker，而是使用独立堆。因此调用方可以让已经完整终止的 Stream、
Listener、UDP 或 TLS Stream 引用晚于 Engine 销毁，并继续查询它们的固定终态；活动
对象和活动 Future 则仍会阻止 Engine 完成销毁。

组合式网络库可以使用 `xrtNetEnginePin` 为借用的 Engine 指针取得一个显式生命周期
占用，并在自身最后一个引用释放时调用 `xrtNetEngineUnpin`。成功的 Pin 必须严格一一
配对；Engine 未运行时 Pin 失败，未匹配的 Unpin 返回 `XERR_STATE`。这组 API 只管理
生命周期，不赋予跨线程访问 Worker 私有状态的权限，也不替代上层对象自己的关闭与
排空状态机。

`xrtNetEngineCreate` 在返回前复制 `BufferPool` 指向的完整配置，不保留该指针；
调用方随后可以修改或释放原配置。它只创建停止状态对象。`xrtNetEngineStart`
为每个 Worker 建立一个独占缓冲池、事件端口和线程；
部分启动失败会完整回滚到 `XNET_ENGINE_STOPPED`，之后允许重试。`Start` 和 `Stop`
在已经处于目标状态时幂等成功。

`xrtNetEngineStop` 先阻止新提交，再等待正在提交的调用离开，关闭各命令队列，
唤醒 Worker，并排空已经受理的普通任务。尚未到期的 Timer 以
`XNET_RESULT_CLOSED` 终结。Timer 关闭回调及其投递的有限后续任务仍会在所属
Worker 上执行。停机排空按任务投递代推进；任务链在固定安全代数内没有收敛时，
Worker 会原子封闭后续投递，继续执行封口前已经受理的任务，然后完整释放运行资源。
此时 Engine 进入 `XNET_ENGINE_STOPPED`，`Stop` 返回 `false`、`XERR_STATE` 和
`XNET_ERROR_ENGINE_STOP`，`ShutdownStalls` 增加一次；Engine 可以再次启动。

调用方和高层网络对象必须在停止前归还 Worker
缓冲池分配的全部块；若仍有外借块，Worker 线程已经停止且 Engine 进入
`XNET_ENGINE_STOPPED`，但 `Stop` 返回 `XNET_ERROR_POOL_BUSY`，并保留池和 Engine。
调用方可以重新 `Start`，在所属 Worker 上归还旧块，再次 `Stop` 完成清理。
`Destroy` 遇到相同情况也返回失败并保留 Engine，不会制造悬空缓冲引用。
不能从 Engine 自己的 Worker 回调中调用 `Stop` 或 `Destroy`，这类调用返回
`XERR_STATE`，避免自等待死锁。

### Worker 与任务

- `xrtNetEngineWorkerCount` 返回固定 Worker 数。
- `xrtNetEngineWorker` 返回指定索引的借用 Worker。
- `xrtNetEngineCurrent` 返回当前线程在指定 Engine 中的 Worker。
- `xrtNetWorkerEngine`、`xrtNetWorkerIndex` 和 `xrtNetWorkerIsCurrent` 提供稳定查询。
- `xrtNetWorkerPort` 只在运行期返回借用端口。
- `xrtNetWorkerBufPool` 只在所属 Worker 回调内返回借用的共享自适应缓冲池。
- `xrtNetWorkerAlloc/Free` 为协议对象提供线程安全的分级小对象缓存；内存由 `Alloc` 清零，必须把原始大小和同一 Worker 传回 `Free`，并由调用方保证 Worker 生命周期覆盖分配与归还。
- `xrtNetWorkerOperationId` 从任意线程分配 Engine 内唯一的非零端口操作 ID。
- `xrtNetEnginePost` 按 `affinity % workers` 选择 Worker。

`Post` 返回成功后，任务必在目标 Worker 上执行一次，包括与 `Stop` 并发时已经
受理的任务。队列达到硬容量返回 `false` 和 `XERR_AGAIN`；停止后返回
`XERR_CLOSED`。停机期间，当前 Worker 只允许投递完成清理所需的有限后续任务；
一旦不收敛保护触发，公开与内部投递均以 `XERR_CLOSED` 拒绝。普通命令和 transport
内部生命周期命令分别按每轮 256 个的预算消费，
随后必须回到 Timer 与端口事件，避免大批连接同时关闭、取消或完成解析时饿死 IO；
未消费的内部命令由 Worker 自己保留，不重新入队，也不增加分配。Worker 回调应保持
短小，阻塞一个回调会同时阻塞该 Worker 的 IO、Timer 和后续命令。

端口等待失败不会让 Worker 无界热循环，也不会立即丢弃仍可处理的关闭与控制命令。
Worker 记录结构化网络错误和系统错误码，清理线程局部错误后退避 1 到 10 毫秒；
实际退避由 `IdleWait` 限制在该范围内。终止性后端故障会在后续循环继续计数，调用方
可通过统计发现并决定停止、重建或降级 Engine。

`Stop` 先原子关闭全部 Worker 的提交门，再等待已经进入提交区的调用离开，因此
不会释放仍可能被生产者访问的命令队列。`xrtNetWorkerPort` 返回的端口只是借用：
除端口 API 明确允许跨线程的操作外，应在所属 Worker 使用；外部线程直接使用
`Post` 或 `Wake` 时，调用方必须先停止自己的生产者，再停止或销毁 Engine。

Worker 缓冲池供 TCP、UDP、TLS 和自定义协议共享，缓存预算按 Worker 而不是按连接
计算。通过该池建立的 `xnetbuf` 必须在所属 Worker 上操作，并在回调返回前清空或把
后续处理继续投递到同一 Worker；不能把带池块跨线程释放。需要跨线程长期保存时，
应复制数据或使用 `Pool == NULL` 的独立缓冲。

### Timer

`xrtNetEngineSchedule` 使用 `xrtClock` 的绝对微秒截止时间；
`xrtNetEngineAfter` 是相对微秒 Helper。成功返回的非零 ID 在所属 Engine 内唯一，
回调只在所属 Worker 执行且绝不在非 Worker 调用线程内联；跨线程调度时，Worker
可能在调度函数返回前并发完成回调。每个成功返回的 ID 恰好发生一次终态回调：

| 结果 | 含义 |
| --- | --- |
| `XNET_RESULT_OK` | Timer 到期 |
| `XNET_RESULT_CANCELLED` | 取消命令在到期前生效 |
| `XNET_RESULT_CLOSED` | Engine 停止或销毁 |
| `XNET_RESULT_ERROR` | 受理后的 Worker 内部资源扩展失败 |

`xrtNetEngineTimerCancel` 是异步请求。返回成功表示取消命令已进入目标 Worker，
不表示 Timer 一定尚未到期；Timer 自己的唯一终态回调给出最终结果。Timer 表使用
自适应最小堆和 ID 哈希：插入、到期和有效取消不扫描全部 Timer，空闲 Worker 也
只保留很小的初始表。从 Timer 所属 Worker 调度时直接完成容量检查和入堆，不经过
命令队列，也不为后续同 Worker 生命周期终结强制分配取消命令；跨线程调度和公开
取消仍通过有界命令队列保持有序。

已经位于 Timer 所属 Worker 的协议状态机可以调用
`xrtNetEngineTimerCancelCurrent` 立即取消。该路径不分配命令节点，也不经过有界队列；
不在所属 Worker、Timer 尚未安装或已经终结时返回 `false`，且不会覆盖当前线程错误。
调用方因此可以先尝试 Current 路径，再用 `xrtNetEngineTimerCancel` 处理跨线程情况。

Timer 终态会先从堆和活动索引移除、归还节点并更新统计，再调用用户回调。因此周期
任务可以在回调中重新调度，并在 Worker 缓存已有节点时不依赖新的底层内存分配。
回调收到的 ID、结果和用户数据已经保存为局部值，节点复用不会改变本次终态参数。

### Completion

`xrtNetCompletionInit` 初始化调用方拥有的 `xnetcompletion`。通过
`xrtNetWorkerPort` 直接提交端口操作时，`User` 必须指向一个有效 Completion，且
Completion、Socket 和 IO 缓冲都必须存活到终态事件回调结束。Engine 在所属
Worker 上调用 `Proc(worker, event, data)`。

该约束取代旧版 Engine 只有两个全局端口回调槽的设计。每个在途操作都能携带自己
的 Completion，因此 TCP、UDP、监听器和自定义协议可以任意组合。无人接收的
Accept 结果由 Engine 自动关闭，避免泄漏已接受 Socket。

### 统计

`xrtNetWorkerStats` 返回单 Worker 并发快照，`xrtNetEngineStats` 聚合全部 Worker。
统计覆盖任务受理、拒绝、执行，Timer 受理、拒绝及四种终态，端口事件、等待错误、
唤醒错误、停机任务链不收敛次数、小节点缓存命中/未命中、当前缓存字节、当前命令
深度和活动 Timer。`ShutdownStalls` 从 `XNET_STATS_BASIC` 开始记录，是跨
`Stop/Start` 累计的诊断计数；一次失败停机中每个不收敛 Worker 最多增加一次。
单 Worker 的 `LastWaitError` 和 `LastWaitSystemCode` 保留最近一次端口等待失败详情；
从未失败时分别为 `XNET_ERROR_NONE` 和零。Engine 聚合统计只累计 `WaitErrors`，需要
定位具体后端错误时应读取各 Worker 快照。
`NodeCacheHits`/`NodeCacheMisses` 是累计分配路径计数；`NodeCachedBytes` 是并发快照，
始终不超过各 Worker 的 `NodeCacheBytes`，停止后归零。累计计数跨 `Stop/Start` 保留，
当前深度在停止后归零。
