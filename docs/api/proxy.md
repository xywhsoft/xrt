# Proxy

`<xrt/proxy.h>` 把代理能力拆成不可变配置、纯协议握手和托管传输三层。SOCKS5 CONNECT 与 HTTP CONNECT 共用同一套对象、缓冲、错误和托管拨号契约；HTTP 后端直接复用公开的 HTTP/1 Header 解析与封包能力，代理模块不维护第二套解析器。

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
| `XNET_PROXY_SOCKS5` | XNETPROXYSOCKS5 |

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
| `XNET_PROXY_AUTH_REQUIRED` | REQUIRED |

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
| `Type` | `xnetproxytype` | Type |
| `Host` | `xstrview` | Host |
| `Port` | `uint16` | Port |
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
| `Type` | `xnetproxytype` | Type |
| `Host` | `xstrview` | Host |
| `Port` | `uint16` | Port |
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
| `Address` | `xnetaddr` | Address |
| `Host` | `xstrview` | Host |

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
| `XNET_SOCKS5_SUCCEEDED` | SUCCEEDED |
| `XNET_SOCKS5_GENERAL_FAILURE` | GENERALFAILURE |
| `XNET_SOCKS5_RULESET_DENIED` | RULESETDENIED |
| `XNET_SOCKS5_NETWORK_UNREACHABLE` | NETWORKUNREACHABLE |
| `XNET_SOCKS5_HOST_UNREACHABLE` | HOSTUNREACHABLE |
| `XNET_SOCKS5_CONNECTION_REFUSED` | CONNECTIONREFUSED |
| `XNET_SOCKS5_TTL_EXPIRED` | TTLEXPIRED |
| `XNET_SOCKS5_COMMAND_UNSUPPORTED` | COMMAND不支持 |

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

### `xnetproxydialconfig`

Timeout 覆盖 DNS、TCP 和代理握手全过程；零值保留各内层超时。

```c
typedef struct xnetproxydialconfig {
	xnetdialconfig Transport;
	uint64 Timeout;
	size_t ReceiveLimit;
} xnetproxydialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Transport` | `xnetdialconfig` | Transport |
| `Timeout` | `uint64` | Timeout |
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
| `State` | `xnetproxydialstate` | State |
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

## 裁剪宏

| 宏 | 依赖 | 能力 |
| --- | --- | --- |
| `XRT_FEATURE_NET_PROXY` | `XRT_FEATURE_NET` | 不可变代理端点和凭据 |
| `XRT_FEATURE_NET_PROXY_HANDSHAKE` | Proxy、Net Buffer | 传输无关的增量握手框架 |
| `XRT_FEATURE_NET_PROXY_SOCKS5` | Proxy Handshake | SOCKS5 CONNECT 和用户名密码认证 |
| `XRT_FEATURE_NET_PROXY_HTTP_CONNECT` | Proxy Handshake、HTTP/1 Head、Base64 | HTTP CONNECT 和 Basic 认证 |
| `XRT_FEATURE_NET_PROXY_DIAL` | Proxy Handshake、TCP Dial | 托管 DNS、TCP 与代理握手的完整拨号生命周期 |

代理协议层不依赖 TCP、TLS、HTTP 客户端或 WebSocket。调用方可以把同一个握手对象接到 XRT TCP、自定义 Socket、测试传输或其他双向字节流。

## 代理对象

`xnetproxyconfig` 的所有视图只在 `xrtNetProxyCreate` 调用期间借用。创建成功后，`xnetproxy` 在一块精确分配中持有主机、用户名和密码的深拷贝，可以跨线程和请求共享。

```c
xnetproxyconfig Config;
xnetproxy* pProxy;

xrtNetProxyConfigInit(&Config);
Config.Host = XRT_STR_LITERAL("127.0.0.1");
Config.Port = 1080;
Config.Username = XRT_BYTES_LITERAL("user");
Config.Password = XRT_BYTES_LITERAL("pass");
pProxy = xrtNetProxyCreate(&Config);
```

`XNET_PROXY_AUTH_AUTO` 是默认策略：没有凭据时规范化为 `NONE`，存在凭据时规范化为 `REQUIRED`。SOCKS5 的 `OPTIONAL` 会同时提供匿名和用户名密码方法；HTTP CONNECT 的 `REQUIRED` 与 `OPTIONAL` 都预先发送 Basic 字段，避免在同一 TCP 连接上隐式重放 CONNECT。HTTP Basic 用户名不能包含冒号，密码允许任意字节。

`xrtNetProxyRetain` 和 `xrtNetProxyRelease` 管理共享引用。最后一个引用释放前会清零整个对象分配，避免凭据留在堆内存中。

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

## 增量握手

`xnetproxyhandshake` 由一个传输执行上下文独占驱动。创建时会深拷贝目标主机、增加代理引用并立即生成首个协议报文。

```c
xnetproxyhandshakeconfig Config;
xnetproxyhandshake* pHandshake;

xrtNetProxyHandshakeConfigInit(&Config);
Config.Proxy = pProxy;
Config.TargetHost = XRT_STR_LITERAL("origin.example");
Config.TargetPort = 443;
pHandshake = xrtNetProxyHandshakeCreate(&Config);
```

状态驱动规则：

- `XNET_PROXY_HANDSHAKE_WRITE`：用 `xrtNetProxyHandshakeOutput` 借用当前连续输出，真正写出的字节数交给 `xrtNetProxyHandshakeSent`。部分写入受支持。
- `XNET_PROXY_HANDSHAKE_READ`：把网络数据追加到调用方自己的 `xnetbuf`，再调用 `xrtNetProxyHandshakeStep`。
- `XNET_PROXY_HANDSHAKE_READY`：隧道已经建立。SOCKS5 可以读取 `xrtNetProxyHandshakeBound`；HTTP CONNECT 没有绑定端点，该函数返回 `XERR_NOT_FOUND`。握手只消费输入链中的协议回复前缀，同包到达的应用数据仍留在原输入链中。
- `XNET_PROXY_HANDSHAKE_ERROR`：使用 `xrtNetProxyHandshakeError` 读取握手捕获的结构化错误，使用 `xrtNetProxyHandshakeCode` 读取已经收到的 SOCKS5 线路回复码或 HTTP 状态码。

`xrtNetProxyHandshakeOutput` 只借用握手对象当前输出的首段。没有待发送数据或参数无效时，它返回 `false`，并把非空的输出参数规范化为 `{ NULL, 0 }`，调用方不会误用上一次查询留下的借用指针。

握手输出可能包含用户名和密码。确认发送的输出前缀会在释放或返回缓冲池前清零；销毁握手也会清零尚未发送的输出。

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
| 新状态 | `WRITE` / `READ` / `READY` / `ERROR` | `ERROR` 时见 `HandshakeError` |

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

## SOCKS5 契约

当前 SOCKS5 后端实现 RFC 1928 CONNECT 和 RFC 1929 用户名密码认证：

- 目标支持域名、IPv4 和 IPv6；数字地址直接使用对应线路类型。
- 用户名和密码各自遵守 255 字节线路上限。
- 回复支持任意分片和同包合并。
- 绑定端点支持 IPv4、IPv6 和域名回复。
- 标准失败码 `1` 到 `8` 保留在线路码中，并映射为 `XNET_ERROR_PROXY_CONNECT`。
- `ReceiveLimit` 是硬限制；默认 64 KiB，足以覆盖 SOCKS5，并为后续 HTTP CONNECT Header 提供统一容量契约。

当前 SOCKS5 公共握手只实现 CONNECT。未来的 BIND 和 UDP ASSOCIATE 应作为独立命令能力加入，不会改变 CONNECT 的认证、缓冲和错误契约。

## HTTP CONNECT 契约

HTTP CONNECT 后端使用 HTTP/1.1 authority-form 请求，并始终生成与目标一致的 `Host` 字段：

```http
CONNECT origin.example:443 HTTP/1.1
Host: origin.example:443
```

- 域名和 IPv4 使用 `host:port`；IPv6 使用 `[address]:port`，作用域百分号在线路上编码为 `%25`。
- 目标必须是纯主机或数字地址，不能传入 URL、用户信息、路径、查询或片段。
- 存在凭据时发送 `Proxy-Authorization: Basic ...`；认证临时区、未发送输出和托管发送副本都会安全清零。
- 最终任意 `2xx` 都建立隧道；最多接受 8 个非 `101` 的 `1xx` 中间响应，`101` 明确视为协议错误。
- `407` 映射为 `XERR_PERMISSION/XNET_ERROR_PROXY_AUTH`，其他拒绝状态映射为 `XERR_IO/XNET_ERROR_PROXY_CONNECT`，畸形响应保留 `xrt.http1` 原因链。
- 响应头增量接收并受 `ReceiveLimit` 硬限制。只有找到完整空行后才拉直一次，不为字段表分配内存，也不保留代理响应字段。
- `xrtNetProxyHandshakeCode` 返回最终 HTTP 状态码；HTTP CONNECT 不虚构 SOCKS5 风格的绑定端点。

## 托管代理拨号

`xrtNetProxyDial` 是常用路径：它先复用 TCP Dial 解析并连接代理端点，再在获胜 Stream 的所属 Worker 上驱动传输无关握手。应用只提供最终目标、Stream 事件和一个完成回调，不需要手工搬运握手字节。

```c
xnetproxydialconfig Config;
xnetproxydial* pDial;

xrtNetProxyDialConfigInit(&Config);
Config.Timeout = 10000000u;
Config.ReceiveLimit = 4096;
pDial = xrtNetProxyDial(
	pEngine,
	pResolver,
	pProxy,
	"origin.example",
	443,
	&Config,
	&pStreamEvents,
	pStreamData,
	onProxyDial,
	pDoneData
);
```

`Config.Transport` 完整保留 TCP Dial 的地址族、候选竞速、Worker 亲和性、连接超时与 Stream 硬边界。`Config.Timeout` 是代理拨号的全过程上限，覆盖 DNS、TCP 和握手；零值关闭这一层总超时，但不会改写 `Transport.Timeout`。`ReceiveLimit` 是握手协议输入硬上限，并且必须不大于 TCP `ReadLimit`。

托管 Dial 状态与最终目标主机名使用单块拥有分配，不为目标字符串建立第二个堆节点。代理配置、TCP Dial、协议握手和最终 Stream 仍保持独立引用，因为它们具有不同终态和公开生命周期；组合层只消除相同所有权边界内的重复分配，不用对象拼接换取脆弱的隐式依赖。

通过参数和代理配置校验后，组合层会先取得一份 Engine 初始化租约，再分配 Dial、安装全过程 Timer 并创建底层 TCP Dial。只有 Timer、TCP Dial 或最终 Stream 已经接管 Engine 生命周期后，这份临时租约才会释放；任一初始化步骤失败则在返回前回滚。初始化正在其他线程执行时，`xrtNetEngineDestroy()` 会以 `XERR_STATE` 拒绝销毁并保持 Engine 为 `RUNNING`，不会留下一个尚未返回、却已经引用失效 Engine 的半成品组合对象。

终态回调只在代理传输所属 Worker 发布一次，并且不会从
`xrtNetProxyDial` 的调用栈直接重入。从其他线程提交时，Worker 仍可以与提交
线程并发执行，回调可能早于调用方保存返回值；必须使用回调参数中的 `pDial`
识别操作。该参数与错误都只在回调期间借用，跨回调保留 Dial 时先调用
`xrtNetProxyDialRef`。

终态规则：

- 成功时先安装最终用户事件，再按 `Open`、已预读 `Read`、完成回调的顺序发布。完成回调接管一个已经建立隧道的 `xnetstream*` 引用。
- 失败、超时和取消时 Stream 参数为空，错误参数只在回调期间借用；Dial 对象保留完整错误原因链，可用 `xrtNetProxyDialError` 继续查询。
- `xrtNetProxyDialDestroy` 只释放调用方持有的 Dial 引用，不等同于取消。放弃未完成操作时先调用 `xrtNetProxyDialCancel`，再释放引用。

`xrtNetProxyDial` 的非空返回值包含一份调用方引用。借用的回调参数不是额外
引用，不能无条件销毁；只有调用方已经同步取得返回引用，或者回调先显式增加
引用后，才能在对应所有权路径释放 Dial。

`xrtNetProxyDialCancel` 可从任意线程调用并且只允许第一次成功。取消会协作终止当前 DNS、候选 TCP 或握手 Stream；所有 Stream 操作仍被投递到其所属 Worker，不会从取消线程直接访问 Worker 私有缓冲。总超时复用同一取消路径，但终态规范化为 `XNET_RESULT_TIMEOUT` 和 `XERR_TIMEOUT`。

握手输出按 TCP `WriteLimit` 的剩余预算分段提交。短写、高低水位和同步 `LowWater`/`Drain` 重入都由组合层串行折叠；认证输出的托管副本在离开发送队列时安全清零。只有完成代理回复所需的字节会被消费，同包到达的应用数据仍留在 Stream 读缓冲，并在最终 `Read` 中原样交付。

`xrtNetProxyDialState` 区分 `RESOLVING`、`CONNECTING`、`HANDSHAKE` 和三个终态；`xrtNetProxyDialStats` 同时返回代理阶段与底层 TCP Dial 候选统计。DNS、TCP 和代理协议错误保留原始 cause，外层统一使用 `xrt.net` 代理错误码表达失败阶段。OOM 时如果连外层组合错误也无法分配，顶层可以直接是静态 `XERR_MEMORY`；调用方应使用 `xrtErrorIs(error, XERR_MEMORY)` 检查整条原因链。

托管路径同时支持已经编译进依赖闭包的 SOCKS5 CONNECT 和 HTTP CONNECT。只编译其中一个协议时，另一个类型会明确返回 `XERR_UNSUPPORTED`；`XRT_FEATURE_NET_PROXY_DIAL` 本身不强制携带任何具体代理协议，保持裁剪边界清晰。

托管代理层不识别端口后端。select、IOCP 与 io_uring 共用同一份 SOCKS5、HTTP CONNECT、并发取消、OOM 回收和单头拨号断言；后端测试入口只选择 `xnetportkind`。新增端口实现时，应先通过 TCP Dial 契约，再直接复用这些组合测试，不能在代理状态机中增加平台分支。

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
	);
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

## 所有权

- `xnetproxyconfig` 和 `xnetproxyhandshakeconfig` 中的视图在创建调用期间借用。
- `xnetproxy` 不可变并使用共享引用。
- `xnetproxyhandshake` 唯一所有，使用 `xrtNetProxyHandshakeDestroy` 销毁。
- `xnetproxydial` 使用共享引用；运行阶段持有自己的内部引用，用户使用 `xrtNetProxyDialRef` 和 `xrtNetProxyDialDestroy` 管理外部引用。
- 托管拨号成功时完成回调接管 Stream；失败时组合层回收内部 Stream，不向用户暴露半成品。
- `xnetproxyhandshakeconfig.Pool` 只借用；非空时必须比握手对象存活更久。
- `xrtNetProxyHandshakeOutput`、`xrtNetProxyInfo` 和 `xrtNetProxyHandshakeBound` 返回的视图都不能越过其所有者生命周期。
