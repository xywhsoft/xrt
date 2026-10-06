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

需要端到端 TLS 时，启用 `XRT_FEATURE_TLS_STREAM_DIAL_PROXY` 并调用
`xrtTlsDialProxy()`；该模块闭包显式包含 TLS Stream Dial 与 Proxy Dial。该入口
复用本节的代理拨号器建立隧道，但由外层 TLS Dial
统一拥有全过程期限、取消终态、错误链和传输统计。`xtlsdialconfig` 不嵌入代理
指针；直连 `xrtTlsDial()` 与代理 `xrtTlsDialProxy()` 是两个显式入口，以保持配置
ABI 不随功能宏改变，并避免裁剪构建静默退化为直连。

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

### `xrtTlsDial`

解析主机、竞争 TCP 地址并完成 TLS 握手；成功 Stream 引用转移给完成回调。

```c
xtlsdial* xrtTlsDial(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xtlsclientconfig* pTls, const xtlsdialconfig* pConfig, const xtlsstreamevents* pStreamEvents, ptr pStreamData, xtlsdialproc pDone, ptr pDoneData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 非空 | 名称解析器 |
| `sHost` | 输入 | — | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pStreamEvents` | 输入 | 非空 | 流事件表 |
| `pStreamData` | 输入 | — | 流用户数据 |
| `pDone` | 输入 | — | 完成回调 |
| `pDoneData` | 输入 | — | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 算法、版本或能力不受支持
- `XERR_MEMORY` — 分配失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	pDial = xrtTlsDial(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
		&Events,
		&Example,
		exampleTlsDialDone,
		&Example
```



### `xrtTlsDialAsync`

以 Future 接收完成握手的 TLS Stream；Open 先于成功终态发布。Future 持有一个 Stream 引用，取消请求协作终止 DNS、TCP 或 TLS 当前阶段。

```c
xfuture* xrtTlsDialAsync(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xtlsclientconfig* pTls, const xtlsdialconfig* pConfig, const xtlsstreamevents* pStreamEvents, ptr pStreamData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 非空 | 名称解析器 |
| `sHost` | 输入 | — | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pStreamEvents` | 输入 | 非空 | 流事件表 |
| `pStreamData` | 输入 | — | 流用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[dial_future](../../examples/tls/dial_future/main.c) · stream tour

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



### `xrtTlsDialCancel`

原子受理取消；返回真保证最终结果不会再变为成功。

```c
bool xrtTlsDialCancel(xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
			bool bCancelled = xrtTlsDialCancel(pMidair);
```



### `xrtTlsDialConfigInit`

初始化 TCP 拨号、TLS Stream 和总超时策略。

```c
void xrtTlsDialConfigInit(xtlsdialconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	xrtTlsDialConfigInit(&DialConfig);
```



### `xrtTlsDialDestroy`

释放 TLS Dial 引用；空指针视为空操作。

```c
void xrtTlsDialDestroy(xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	xrtTlsDialDestroy(pDial);
```



### `xrtTlsDialError`

失败或取消后借用完整错误原因链。

```c
const xerror* xrtTlsDialError(const xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		xrtTlsDialError(pDial) == NULL ? "(none)" : "err");
```



### `xrtTlsDialProxy`

经代理 CONNECT 隧道连接目标并继续同一个受管 TLS 状态机。代理对象只在调用期间借用；提交成功后，组合拨号持有其自己的代理引用。

```c
xtlsdial* xrtTlsDialProxy(xnetengine* pEngine, xnetresolver* pResolver, const xnetproxy* pProxy, cstr sHost, uint16 iPort, const xtlsclientconfig* pTls, const xtlsdialconfig* pConfig, const xtlsstreamevents* pStreamEvents, ptr pStreamData, xtlsdialproc pDone, ptr pDoneData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 非空 | 名称解析器 |
| `pProxy` | 输入 | 非空、调用期间借用 | 代理配置 |
| `sHost` | 输入 | 非空 | CONNECT 目标主机；默认也用于 SNI 与证书名称 |
| `iPort` | 输入 | 非零 | CONNECT 目标端口 |
| `pTls` | 输入 | 允许空 | TLS 客户端配置 |
| `pConfig` | 输入 | 允许空 | TCP、TLS 与全过程超时配置 |
| `pStreamEvents` | 输入 | 允许空 | 成功 TLS Stream 的事件表 |
| `pStreamData` | 输入 | — | Stream 用户数据 |
| `pDone` | 输入 | 非空 | 唯一终态完成回调 |
| `pDoneData` | 输入 | — | 完成回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | TLS Dial 调用方引用 | — |
| `NULL` | 参数、配置、分配或提交失败 | 见当前线程错误 |

#### 错误

- `XERR_ARGUMENT` — Engine、Resolver、Proxy、目标或完成回调非法
- `XERR_RANGE` — Stream 或代理握手硬上限与传输配置冲突
- `XERR_UNSUPPORTED` — 代理类型对应的握手协议未编译
- `XERR_CLOSED` — Engine 已停止接受新对象
- `XERR_MEMORY` — 分配失败

#### 范例

参见已注册的 [examples/tls/dial/main.c](../../examples/tls/dial/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTlsDialProxy(pEngine, pResolver, pProxy, sHost,
				(uint16)iPort, &TlsConfig, &DialConfig, &Events, &Example,
				exampleTlsDialDone, &Example);
```
### `xrtTlsDialRef`

增加 TLS Dial 引用并返回原指针。

```c
xtlsdial* xrtTlsDialRef(xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(xrtTlsDialRef(pDial) != pDial) ) {
```



### `xrtTlsDialState`

返回当前拨号阶段或不可变终态。

```c
xtlsdialstate xrtTlsDialState(const xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pSlot->DialState = xrtTlsDialState(pDial);
```



### `xrtTlsDialTransportStats`

取得底层 TCP Dial 统计；TLS 握手阶段仍保留获胜地址信息。

```c
bool xrtTlsDialTransportStats(const xtlsdial* pDial, xnetdialstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |
| `pStats` | 输入 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	if ( !xrtTlsDialTransportStats(pDial, &TransportStats) ||
		(TransportStats.AttemptsStarted < 1u) ) {
```



### `xrtTlsListenerAccept`

pull 模式下非阻塞取得一个已完成握手的 Stream；空队列返回空指针。

```c
xtlsstream* xrtTlsListenerAccept(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		pServerA = xrtTlsListenerAccept(pListener);
```



### `xrtTlsListenerAcceptAsync`

pull 模式下异步接受一个已完成握手的 Stream；Future 持有结果引用。

```c
xfuture* xrtTlsListenerAcceptAsync(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pAcceptFuture = xrtTlsListenerAcceptAsync(pListener);
```



### `xrtTlsListenerAcceptWait`

阻塞接受一个已完成握手的 Stream；禁止从该 Engine 的 Worker 调用。

```c
xtlsstream* xrtTlsListenerAcceptWait(xtlslistener* pListener, int64 iTimeout, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |
| `iTimeout` | 输入 | — | 单调截止时间 |
| `pCancel` | 输入 | — | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pServerC = xrtTlsListenerAcceptWait(pListener,
		EXAMPLE_DEADLINE_MS, NULL);
```



### `xrtTlsListenerClose`

原子停止接入并丢弃尚未交付的连接；已交付连接保持独立生命周期。

```c
bool xrtTlsListenerClose(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(void)xrtTlsListenerClose(pListener);
```



### `xrtTlsListenerConfigInit`

初始化单 IPv4 动态端口、有界握手与有界完成队列。

```c
void xrtTlsListenerConfigInit(xtlslistenerconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	xrtTlsListenerConfigInit(&ListenerConfig);
```



### `xrtTlsListenerData`

返回创建时保存的用户数据快照。

```c
ptr xrtTlsListenerData(const xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(xrtTlsListenerData(pListener) != &EngineConfig) ) {
```



### `xrtTlsListenerDestroy`

释放 Listener 引用；不会隐式关闭仍在监听的对象。

```c
void xrtTlsListenerDestroy(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	xrtTlsListenerDestroy(pListenerRef);
```



### `xrtTlsListenerLocal`

复制监听 Socket 的实际本地地址，支持动态端口。

```c
bool xrtTlsListenerLocal(xtlslistener* pListener, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |
| `pAddress` | 输入 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		!xrtTlsListenerLocal(pListener, &Address) ||
```



### `xrtTlsListenerRef`

增加 Listener 引用并返回原指针。

```c
xtlslistener* xrtTlsListenerRef(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pListenerRef = xrtTlsListenerRef(pListener);
```



### `xrtTlsListenerStart`

同步完成 TCP 绑定并开始异步接入；配置数组只在调用期间借用。Listener 会保留 Context、Identity，并深复制 ALPN 协议列表。SelectContext 与 ResumeContext 由调用方持有，必须存活到 Listener 关闭回调结束。

```c
xtlslistener* xrtTlsListenerStart(xnetengine* pEngine, const xtlslistenerconfig* pConfig, const xtlslistenerevents* pEvents, const xtlsstreamevents* pStreamEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pStreamEvents` | 输入 | 非空 | 流事件表 |
| `pData` | 输入 | — | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pListener = xrtTlsListenerStart(pEngine, &ListenerConfig, NULL,
		NULL, &EngineConfig);
```



### `xrtTlsListenerState`

返回 Listener 当前生命周期状态。

```c
xtlslistenerstate xrtTlsListenerState(const xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN) ||
```



### `xrtTlsListenerStats`

复制 Listener 的并发统计快照。

```c
bool xrtTlsListenerStats(const xtlslistener* pListener, xtlslistenerstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |
| `pStats` | 输入 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	if ( !xrtTlsListenerStats(pListener, &ListenerStats) ||
		(ListenerStats.Accepted < 3u) ||
		(ListenerStats.Handshakes < 3u) ) {
```



### `xrtTlsStreamAbort`

从任意线程立即放弃 TLS 与 TCP 会话。失败收尾尚未完成时仍会中止 TCP，但不会覆盖已经保存的首个根因。

```c
bool xrtTlsStreamAbort(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
			(void)xrtTlsStreamAbort(pStream);
```



### `xrtTlsStreamAccept`

在 TCP Accept 回调内接管 Stream；返回值应直接作为该回调结果。

```c
bool xrtTlsStreamAccept(xnetstream* pTransport, const xtlsserverconfig* pTls, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData, xtlsstream** ppStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |
| `ppStream` | 输入 | 非空 | 接收流对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 接受的字节不是有效 TLS 记录
- `XERR_MEMORY` — 分配失败

#### 范例

[stream](../../examples/tls/stream/main.c) · stream tour

```c
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pExample->ServerConfig,
		&pExample->StreamConfig,
		&pExample->StreamEvents,
		pExample,
		&pStream
	);
```



### `xrtTlsStreamAsyncBytes`

返回尚未由所属 Worker 终结的异步发送负载字节数。

```c
size_t xrtTlsStreamAsyncBytes(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamAsyncBytes(pStream);
```



### `xrtTlsStreamAsyncCount`

返回异步发送、接收和条件等待的合计操作数。

```c
uint32 xrtTlsStreamAsyncCount(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamAsyncCount(pStream);
```



### `xrtTlsStreamAttach`

在已公开的 TCP Stream 所属 Worker 上接管 Transport 和 Session。Transport 必须仍可双向收发，调用方必须停止直接操作其 IO。成功时接管两者的调用方引用；失败时所有权、Session 分配归属和 Transport 事件均保持不变，输出清空。

```c
bool xrtTlsStreamAttach(xnetstream* pTransport, xtlssession* pSession, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData, xtlsstream** ppStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |
| `ppStream` | 输入 | 非空 | 接收流对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		pTask->bOk = xrtTlsStreamAttach(pTask->pTcp,
			pTask->pSession, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
```



### `xrtTlsStreamAvailable`

返回当前待应用消费明文字节数的并发快照。

```c
size_t xrtTlsStreamAvailable(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
```



### `xrtTlsStreamBuffer`

在所属 Worker 上借用明文块链，借用期不超过本次回调。默认在当前明文消费前暂停底层读取；增量协议解析器可显式请求 ReadMore。

```c
const xnetbuf* xrtTlsStreamBuffer(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		const xnetbuf* pPlain = xrtTlsStreamBuffer(pStream);
```



### `xrtTlsStreamClient`

在已连接 TCP Stream 上创建 TLS 客户端。适用于代理隧道、STARTTLS 和自定义拨号；成功时接管 Transport 引用。

```c
bool xrtTlsStreamClient(xnetstream* pTransport, const xtlsclientconfig* pTls, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData, xtlsstream** ppStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |
| `ppStream` | 输入 | 非空 | 接收流对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		pTask->bOk = xrtTlsStreamClient(pTask->pTcp,
			pTask->pClient, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
```



### `xrtTlsStreamClose`

从任意线程请求 close_notify、等待对端认证关闭并排空 TCP。调用前已接纳的异步发送会先按 FIFO 完成；调用后的新发送不再接纳。

```c
bool xrtTlsStreamClose(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		if ( !xrtTlsStreamClose(pStream) ) {
```



### `xrtTlsStreamConfigInit`

初始化握手与认证关闭超时。

```c
void xrtTlsStreamConfigInit(xtlsstreamconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[stream](../../examples/tls/stream/main.c) · stream tour

```c
	xrtTlsStreamConfigInit(&Example.StreamConfig);
```



### `xrtTlsStreamConnect`

创建 TLS 客户端并异步连接数字 TCP 地址。

```c
xtlsstream* xrtTlsStreamConnect(xnetengine* pEngine, const xnetaddr* pRemote, uint64 iAffinity, const xnetstreamconfig* pTransport, const xtlsclientconfig* pTls, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pRemote` | 输入 | 非空 | 远端地址 |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `xrt.net` 域错误 — TCP 连接提交失败
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pClientB = xrtTlsStreamConnect(pEngine, &Address, 0, NULL,
		&ClientConfigB, NULL, &ClientEvents, &ClientB);
```



### `xrtTlsStreamConsume`

在所属 Worker 上安全消费精确数量的明文。

```c
bool xrtTlsStreamConsume(xtlsstream* pStream, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		if ( !xrtTlsStreamConsume(pStream, Span.Size) ) {
```



### `xrtTlsStreamData`

返回线程安全的用户数据指针快照，不延长目标生命周期。

```c
ptr xrtTlsStreamData(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pClient->bDataOk = xrtTlsStreamData(pStream) == pClient;
```



### `xrtTlsStreamDestroy`

释放 TLS Stream 引用；关闭必须另行请求。

```c
void xrtTlsStreamDestroy(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	xrtTlsStreamDestroy(Example.Stream);
```



### `xrtTlsStreamError`

终态失败时借用保存的 TLS 或传输根因。

```c
const xerror* xrtTlsStreamError(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		xrtTlsStreamError(pClientA) == NULL ? "(none)" : "err");
```



### `xrtTlsStreamPending`

返回 TLS 密文暂存与底层 TCP 队列的总待发字节并发快照。

```c
size_t xrtTlsStreamPending(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	while ( xrtTlsStreamPending(pClientB) != 0u ) {
```



### `xrtTlsStreamPullup`

在所属 Worker 上把精确明文前缀按需连续化并返回借用视图。不消费明文；视图在下一次明文缓冲修改或消费前有效，零长度和越界请求失败。

```c
bool xrtTlsStreamPullup(xtlsstream* pStream, size_t iSize, xnetspan* pSpan)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iSize` | 输入 | — | 字节数 |
| `pSpan` | 输入 | 非空 | 接收分片 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		if ( !xrtTlsStreamPullup(pStream, 3u, &Span) ||
			(Span.Size < 3u) ) {
```



### `xrtTlsStreamRead`

在所属 Worker 上复制并安全消费明文。

```c
xtlsresult xrtTlsStreamRead(xtlsstream* pStream, void* pOutput, size_t iCapacity, size_t* pRead)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pRead` | 输入 | 非空 | 接收读取游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 需要更多输入 | — |
| `XTLS_CLOSED` | 会话已关闭 | — |
| `XTLS_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 记录解密失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamRead(pStream, pClient->ReadOut, 4u,
		&pClient->iRead);
```



### `xrtTlsStreamReadMore`

在 Read 回调保留现有明文时，请求继续解密并在明文增长后再次发布 Read。累积量受 Context PlainLimit 硬约束，并必须为一条最大明文 record 留出空间。普通消费者无需调用，重复请求是幂等的；请求待完成时不能替换事件接收者。

```c
bool xrtTlsStreamReadMore(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		pClient->bReadMore = xrtTlsStreamReadMore(pStream);
```



### `xrtTlsStreamRecvAsync`

在拉取模式下复制并消费当前可用明文。零上限表示读取全部当前明文；成功值是由 Future 持有的 xnetbytes。

```c
xfuture* xrtTlsStreamRecvAsync(xtlsstream* pStream, size_t iMaxBytes)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iMaxBytes` | 输入 | — | 最多字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_future](../../examples/tls/stream_future/main.c) · stream tour

```c
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pStream,
		64u * 1024u
	);
```



### `xrtTlsStreamRef`

增加 TLS Stream 引用并返回原指针；引用耗尽时返回空并设置状态错误。

```c
xtlsstream* xrtTlsStreamRef(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[dial_future](../../examples/tls/dial_future/main.c) · stream tour

```c
	pStream = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
```



### `xrtTlsStreamSend`

在所属 Worker 上把明文编码为记录；允许成功短写。

```c
xtlsresult xrtTlsStreamSend(xtlsstream* pStream, const void* pData, size_t iSize, size_t* pWritten)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |
| `pWritten` | 输入 | 非空 | 接收写出数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 需要更多输入 | — |
| `XTLS_CLOSED` | 会话已关闭 | — |
| `XTLS_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_PROTOCOL` — 记录保护失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		xtlsresult Result = xrtTlsStreamSend(
			pStream,
			pExample->Request + pExample->Sent,
			pExample->RequestSize - pExample->Sent,
			&iWritten
		);
```



### `xrtTlsStreamSendAsync`

从任意线程复制并按 FIFO 提交一段完整明文。Future 在全部明文被 TLS 会话受理时完成；排空必须另行等待 DRAIN。取消只在首个字节受理前有效，已开始的发送保持完整和有序。Close 线性化前已接纳的发送保证先完成，之后的发送以 STATE 拒绝。

```c
xfuture* xrtTlsStreamSendAsync(xtlsstream* pStream, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_future](../../examples/tls/stream_future/main.c) · stream tour

```c
	if ( !exampleTlsFutureResolved(xrtTlsStreamSendAsync(
		pStream,
		pData,
		iSize
	)) ) {
```



### `xrtTlsStreamSendBound`

在所属 Worker 上返回一次明文发送产生的精确密文线路字节数。结果包含记录头、显式 nonce、内层类型和认证标签，失败不修改 pBound。pBound 不得与 Stream 或其 Session 对象存储重叠。

```c
bool xrtTlsStreamSendBound(xtlsstream* pStream, size_t iPlainSize, size_t* pBound)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iPlainSize` | 输入 | — | 明文长度 |
| `pBound` | 输入 | 非空 | 接收输出上界 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamSendBound(pStream, 64u, &pClient->iBound);
```



### `xrtTlsStreamSendVec`

在所属 Worker 上依次编码明文片段；返回跨片段的连续受理前缀。

```c
xtlsresult xrtTlsStreamSendVec(xtlsstream* pStream, const xnetspan* pSpans, size_t iCount, size_t* pWritten)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pSpans` | 输入 | 非空 | 分片数组 |
| `iCount` | 输入 | — | 数量 |
| `pWritten` | 输入 | 非空 | 接收写出数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 需要更多输入 | — |
| `XTLS_CLOSED` | 会话已关闭 | — |
| `XTLS_ERROR` | 失败 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamSendVec(pStream, Vec, 2, &iWritten);
```



### `xrtTlsStreamSendVecAsync`

从任意线程复制片段并按 FIFO 提交为一段连续明文。全部片段在返回前完成校验和复制，失败不会发布部分操作。

```c
xfuture* xrtTlsStreamSendVecAsync(xtlsstream* pStream, const xnetspan* pSpans, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pSpans` | 输入 | 非空 | 分片数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pSendFuture = xrtTlsStreamSendVecAsync(pClientB, AsyncVec, 2);
```



### `xrtTlsStreamSession`

在所属 Worker 上借用协议会话，供 ALPN、票据等高级查询。

```c
xtlssession* xrtTlsStreamSession(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pClient->bSessionOk = xrtTlsStreamSession(pStream) != NULL;
```



### `xrtTlsStreamSetEvents`

在所属 Worker 上替换已打开 TLS Stream 的事件与用户数据。不会自动重放当前明文缓冲，协议升级层必须显式处理已有后缀。

```c
bool xrtTlsStreamSetEvents(xtlsstream* pStream, const xtlsstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

参见已注册的 [examples/tls/stream_tour/main.c](../../examples/tls/stream_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTlsStreamSetEvents(pStream,
		&g_SwappedEvents, pClient);
```
### `xrtTlsStreamState`

返回组合 Stream 状态的并发快照。

```c
xtlsstreamstate xrtTlsStreamState(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[dial_future](../../examples/tls/dial_future/main.c) · stream tour

```c
	while ( (xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
```



### `xrtTlsStreamTransport`

借用底层 TCP Stream，调用方不得改变其 IO 状态机。

```c
xnetstream* xrtTlsStreamTransport(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		xrtNetStreamWorker(xrtTlsStreamTransport(pServerA)),
```



### `xrtTlsStreamWaitAsync`

建立 OPEN、READ、WRITE、DRAIN、END 或 CLOSE 条件 Future。WRITE 要求发送 FIFO 清空且当前至少可受理明文；DRAIN 还要求 TLS 与 TCP 两级发送队列归零。END 在已认证明文全部交付后完成。取消只移除本次等待。

```c
xfuture* xrtTlsStreamWaitAsync(xtlsstream* pStream, xtlsstreamwait Wait)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `Wait` | 输入 | 非空 | 等待条件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_future](../../examples/tls/stream_future/main.c) · stream tour

```c
	return exampleTlsFutureResolved(xrtTlsStreamWaitAsync(
		pStream,
		XTLS_STREAM_WAIT_DRAIN
	));
```


## TLS-over-TCP 组合流

`<xrt/tls_stream.h>` 以 `XRT_FEATURE_TLS_STREAM` 独立裁剪，把公开 TCP Stream 与公开 TLS 客户端/服务端会话组合成事件驱动明文字节流。它不复制握手、验证、身份或记录状态机，也不让 TLS 原语反向依赖 socket；高级用户仍可直接使用传输无关会话层。

客户端连接数字地址使用 `xrtTlsStreamConnect()`。已经通过代理、自定义拨号或
明文协议协商得到 TCP Stream 时，在其所属 Worker 上调用
`xrtTlsStreamClient()`；它成功后接管 TCP 引用，后续只从 TLS Stream 读写。
需要自行创建角色 Session 的底层用户可以调用 `xrtTlsStreamAttach()`，成功时
同时转移 Session 与 TCP 引用。服务端在 TCP `Accept` 回调内调用
`xrtTlsStreamAccept()`，并把其布尔结果直接作为 Accept 结果返回：

```c
static bool acceptTls(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	server* pServer = (server*)pData;

	(void)pListener;
	return xrtTlsStreamAccept(
		pTransport,
		&pServer->Tls,
		&pServer->Stream,
		&pServer->Events,
		pServer,
		NULL
	);
}
```

`xrtTlsStreamAccept()` 成功后接管 TCP Accept 交付的调用方引用；失败时保持原
Accept 失败回收规则。`xrtTlsStreamAttach()` 和 `xrtTlsStreamClient()` 只接受
已经发布 Open、尚未读结束、写结束、Close 或 Abort 的双向 Stream，必须在该
Stream 的 Worker 上调用。调用方必须在升级点停止直接收发和改变 TCP 状态。
失败时输出清空，不接管输入引用，不改变 Session 的缓冲池归属，也不替换 TCP
事件或用户数据；修正配置后可用同一 Session 与 Transport 重试。它们会立即
处理 TCP 缓冲中已有的密文，因此代理响应后的尾随 TLS 记录不会丢失。成功时
`ppStream` 返回独立调用方引用。
`xrtTlsStreamConnect()` 成功同样返回调用方引用。`xrtTlsStreamDestroy()` 只释放
引用，不隐式关闭；运行时引用会保持对象活到唯一 `Close` 回调结束。

完成握手后，可以在所属 Worker 上调用
`xrtTlsStreamSetEvents(pStream, pEvents, pData)` 原子替换事件表和用户数据。
它用于 HTTPS Upgrade、WebSocket 和应用协议协商后的处理器接管；不会再次发布
`Open`，也不会重放切换前已经留在明文缓冲中的数据。接管层必须在切换点显式
取得并处理协议余量。原始 TCP 仍由 TLS 组合流独占，不能借此绕过 TLS 接管。

`xrtTlsStreamData()` 以 acquire 语义返回当前用户数据的借用指针快照；事件切换
在所属 Worker 上保持顺序，但查询可以来自任意线程。快照不延长目标生命周期，
指针所指对象的存活期与并发访问仍由调用方管理。`xrtTlsStreamRef()` 在引用计数
耗尽时返回空并设置状态错误，不会让已经释放或饱和的对象重新进入生命周期。

需要主机名时启用 `XRT_FEATURE_TLS_STREAM_DIAL` 并调用 `xrtTlsDial()`。该可选层
直接复用 `xrtNetDial()` 的 Resolver、双栈候选竞速、回退、取消和统计，不复制
另一套 DNS 或 TCP 连接器。数字地址使用 `xrtTlsStreamConnect()`；已有 TCP
Stream 使用 `xrtTlsStreamClient()` 或 `xrtTlsStreamAttach()`；服务端 Accept
使用 `xrtTlsStreamAccept()`；完全自定义传输仍可直接使用会话层。

启用 `XRT_FEATURE_TLS_STREAM_DIAL_PROXY` 时，使用 `xrtTlsDialProxy()` 明确选择代理
路径。目标主机和端口先用于 CONNECT，隧道建立后再对同一目标执行 TLS；直连入口
不会读取代理配置，代理入口也拒绝空代理。两条入口共享同一个 TLS Dial 状态机、
终态门、错误链和统计接口。`Config.Timeout` 是覆盖代理端点解析、TCP、CONNECT 和
TLS 握手的唯一全过程期限；代理组合层不会再启动一只竞争的总定时器，但
`Config.Transport` 中的各阶段限制仍然生效。取消或总超时在 CONNECT 已完成后会
继续中止 TLS Stream，不会停留在永久 `HANDSHAKE` 状态。

```c
xtlsdialconfig Config;
xtlsdial* pDial;

xrtTlsDialConfigInit(&Config);
Config.Timeout = 15000000u;
pDial = xrtTlsDial(
	Engine,
	Resolver,
	"example.com",
	443,
	&Tls,
	&Config,
	&Events,
	App,
	dialDone,
	App
);
```

`Config.Transport` 控制 DNS、地址族、Happy Eyeballs、候选数和 TCP 阶段超时；`Config.Stream` 控制 TLS 握手、认证关闭和缓冲硬上限；`Config.Timeout` 是从提交开始覆盖 DNS、TCP 与 TLS 的全过程硬上限，零值表示只使用各阶段超时。`ServerNameFromHost` 默认开启，仅在 `Tls.ServerName` 为空时用 `sHost` 填充 SNI 与证书名称；显式名称始终优先，关闭该选项则不做自动填充。

通过配置校验后，受管 TLS Dial 会先取得一份 Engine 初始化租约，再创建客户端会话、组合 Stream、全过程 Timer 和底层 TCP Dial。只有这些活动对象已经接管 Engine 生命周期后，临时租约才会释放；任一创建步骤失败则在返回前完整回滚并保留原始 cause。并发 `xrtNetEngineDestroy()` 不能跨过这段尚未返回对象的初始化窗口。

成功时，最终 Stream 的用户 `Open` 先执行，随后 `xtlsdialproc` 以 `XNET_RESULT_OK` 交付同一 Stream 的调用方引用。握手完成前失败只发布一次 Dial 完成回调；半初始化 TLS Stream 在回调前释放，也不额外调用用户 Stream `Close`。`xrtTlsDialCancel()` 可以在解析、TCP 连接或 TLS 握手阶段竞争取消；返回真表示取消已经赢得唯一终态门，之后不能再发布成功。安全 Stream 已赢得发布权或 Dial 已终止时返回假。终态由 `xrtTlsDialState()` 与 `xrtTlsDialError()` 固定保存；底层 TCP Dial 已经终结 Engine 活动占用，但其只读快照保留到 TLS Dial 销毁，因此 `xrtTlsDialTransportStats()` 在 TLS 阶段和终态继续提供候选、尝试、失败和获胜地址统计。

启用 `XRT_FEATURE_TLS_STREAM_DIAL_FUTURE` 后，可以用同一受管拨号状态机直接取得
Future：

```c
xfuture* pDial = xrtTlsDialAsync(
	Engine,
	Resolver,
	"example.com",
	443,
	&Tls,
	&Config,
	&Events,
	App
);
```

成功 Future 持有一个已经 `OPEN` 的 `xtlsstream` 引用，用户 `Open` 回调仍先于
`XFUTURE_RESOLVED` 发布。`xrtFutureValue()` 返回借用指针；需要在销毁 Future 后
继续使用时，先调用 `xrtTlsStreamRef()` 保留独立引用。Future 值析构只释放它所
持有的引用，不隐式关闭其他调用方引用。

`xrtFutureCancel()` 把协作取消传递到当前 DNS、TCP 或 TLS 阶段，只有底层完成
清理后 Future 才进入 `XFUTURE_CANCELLED`。连接失败与全过程超时进入
`XFUTURE_FAILED`，`xrtFutureError()` 保留 TLS 包装错误和底层原因链。需要观察
候选统计或在完成前查询阶段时继续使用回调式 `xrtTlsDial()`；Future 入口是常见
连接路径的轻量适配器，不复制 Dial 状态机。

`Open` 只在 TCP 已连接、TLS 已到 `READY` 且 SNI/ALPN/证书验证全部完成后发布。客户端与服务端事件都在底层 TCP 所属 Worker 上串行执行。`Send`、`SendBound`、`Buffer`、`Read`、`Consume` 和 `Session` 必须在该 Worker 上调用；`Close`、`Abort`、`State`、`Available`、`Pending`、`Transport`、`Data` 和终态 `Error` 支持并发调用或快照读取。

```c
size_t iWritten = 0;
xtlsresult Result = xrtTlsStreamSend(
	Stream,
	Data,
	Size,
	&iWritten
);

size_t WireSize;
bool Sized = xrtTlsStreamSendBound(Stream, Size, &WireSize);
```

`Send` 允许成功短写。`XTLS_OK` 且 `iWritten < Size` 表示该前缀已原子受理，剩余数据由调用方保留；`XTLS_AGAIN` 保证 `iWritten == 0`。`xrtTlsStreamSendVec()` 先校验全部 Span 与总长度，再直接逐片生成记录，不分配一块拼接副本；`iWritten` 是跨 Span 的连续受理前缀，输入无效或总长度溢出时保持为零且不会修改会话。两种发送入口使用相同的背压契约。

`xrtTlsStreamSendBound()` 在所属 Worker 上返回一次 `Send` 对指定明文产生的精确密文线路长度，包含每条记录的头、显式 nonce、TLS 1.3 内层类型和认证标签。它不修改会话、序列号或发送队列；零长度返回零，算术溢出或输出与 Stream/Session 重叠时失败且不修改输出。协议适配层可用它在接受明文前把自身队列预算换算成真实 TLS 线路成本。

背压会登记一个边沿，TLS 发送队列和 TCP 队列重新具备容量后发布 `Writable`。应用必须在 `Writable` 中从未受理偏移继续发送，不能重发已经计入 `iWritten` 的前缀。每次至少受理一个字节后会登记 `Drain`；只有 TLS 密文队列与 TCP 用户态发送预算同时归零才发布该事件。`Writable` 与 `Drain` 均不会重入尚未返回的 `xrtTlsStreamSend()` 或 `xrtTlsStreamSendVec()`，因此调用方可以在返回后再统一提交 `iWritten`。

`xrtTlsStreamPending()` 返回 TLS Session 尚未转移的密文与底层 TCP 用户态发送
队列的饱和相加快照。它不包含已被操作系统接受的内核缓冲字节，也不代表对端
已经读取；成功短写后允许立即为零。该查询可从任意线程用于统计和限流，
`Drain` 回调发布时它必须为零。

适配器要求 TCP `WriteLimit >= TLS SendLimit`，从而把一批完整 TLS 密文块链全有或全无地转移给 TCP。常规路径不复制密文；TCP ReadBuffer 可以整体移动进 TLS Feed。当一次 TCP 输入大于 Feed 剩余容量时，只复制可容纳的前部 Span，消费后继续，避免以固定 8 KiB 缓冲或无界增长掩盖压力。成功短写与同步 TCP 低水位回调之间有重入门；同步产生的 `Writable`/`Drain` 会转为同一 Worker 的内部命令，外层发送返回后才允许进入应用。

`Read` 回调借用 `const xnetbuf*`。应用可以用 `xrtTlsStreamRead()` 复制并消费，也可以检查 `xrtTlsStreamBuffer()` 后以 `xrtTlsStreamConsume()` 精确确认已处理字节。默认通知采用受控边沿语义：当前明文没有全部消费前暂停新的 TCP 接收。增量协议解析器在保留不完整前缀时可调用 `xrtTlsStreamReadMore()`；TLS 会在 `PlainLimit` 内继续解密，只在明文增长后再次发布 `Read`，并要求限制中仍能容纳一条最大明文 record，避免有空间但无法取得下一条完整记录的永久停滞。请求待完成期间不能替换事件接收者。`xrtTlsStreamPullup()` 只按需连续化精确前缀，不消费明文。借用不能保存到下一次回调；消费到零后恢复普通残留密文处理和底层读取。

### TLS Stream Future

启用 `XRT_FEATURE_TLS_STREAM_FUTURE` 后，同一个 TLS Stream 可以从任意线程使用
Future 入口，不需要为同步等待或协程再建立一套连接对象：

```c
xfuture* pSend = xrtTlsStreamSendAsync(Stream, Data, Size);
xfuture* pDrain = xrtTlsStreamWaitAsync(
	Stream,
	XTLS_STREAM_WAIT_DRAIN
);
xfuture* pReceive = xrtTlsStreamRecvAsync(Stream, 64u * 1024u);
```

`xrtTlsStreamSendAsync()` 和 `xrtTlsStreamSendVecAsync()` 在提交期间复制完整输入，
调用返回后不再借用原数据。多个发送保持严格 FIFO；成功 Future 表示全部明文
已经被 TLS 会话受理，不表示密文已经离开 TCP 用户态队列，更不表示对端已经
读取。需要本地排空时另行等待 `XTLS_STREAM_WAIT_DRAIN`。

`xrtTlsStreamClose()` 与异步发送接纳共享一个线性化门。关闭调用之前已经成功
返回 Future 的发送会先保持 FIFO 完整受理，随后才生成 `close_notify`；关闭门
生效后的新发送立即以 `XERR_STATE/XTLS_ERROR_STATE` 拒绝。构造期间的 OOM
会完整归还预算并继续被延迟的关闭，不会让连接永久停在 OPEN。零字节发送是
合法的 FIFO 节点，在轮到它时成功完成且不产生 TLS 应用记录。

发送取消只在首个字节被 TLS 会话受理前有效。尚未开始的节点确认
`XFUTURE_CANCELLED` 并完整归还预算；已经发生成功短写的节点忽略后续取消请求，
继续按原顺序发送剩余后缀，最终成功或报告真实连接终态。这个规则避免取消把
一个应用消息静默截成线路前缀。

`xrtTlsStreamRecvAsync()` 是 pull 模式入口。它在所属 Worker 上先为结果分配独立
存储，再消费当前可用明文；成功值是由 Future 持有的 `xnetbytes`。读取内容统一
调用 `xrtNetBytesView()`；通过 `xrtNetBytesRef()` 增加引用后，结果可以越过 Future
生命周期继续使用。`iMaxBytes == 0` 读取当前全部明文。结果分配失败不会
消费任何字节，恢复内存后可以重试。一个 Stream 不能同时安装 `Read` 回调并
登记 READ/Recv Future；双向切换都以 `XERR_STATE/XTLS_ERROR_STATE` 拒绝，防止
两条路径竞争消费同一明文。

已经通过 TLS 记录认证并解密的明文不会因为随后发生 TCP 截断、协议失败或本地 Abort
而被丢弃。终态 Stream 的 `RecvAsync` 先返回这些缓冲字节；缓冲耗尽后，下一次接收
才返回稳定的失败或关闭结果。Stream 的 `FAILED` 状态和 `xrtTlsStreamError()` 在读取
期间保持不变，因此截断敏感的上层协议仍能明确拒绝不完整消息。

`xrtTlsStreamWaitAsync()` 提供六个水平条件：

| 条件 | 完成点 |
| --- | --- |
| `OPEN` | TCP、TLS 握手和认证全部完成 |
| `READ` | 至少一个明文字节可消费 |
| `WRITE` | 异步发送 FIFO 为空且当前可受理明文 |
| `DRAIN` | 异步发送 FIFO、TLS 密文和 TCP 用户态发送队列全部为空 |
| `END` | 收到并认证对端 `close_notify`，且此前明文已经全部交付 |
| `CLOSE` | TLS Stream 到达最终传输终态 |

`xtlsstreamconfig` 的 `AsyncBytesLimit`、`AsyncCountLimit` 是独立硬边界，
`AsyncBatch` 限制一次 Worker 轮转最多完成的节点数。三个值都必须非零。
超出字节边界返回 `XERR_RANGE/XTLS_ERROR_LIMIT`，并发操作数饱和返回
`XERR_AGAIN/XTLS_ERROR_LIMIT`；失败提交不会残留节点或预算。
`xrtTlsStreamAsyncBytes()` 与 `xrtTlsStreamAsyncCount()` 提供无锁并发快照。

所有 Promise 终态都由所属 Worker 确认；已经终止并失去 Worker 的对象允许在
调用线程立即返回固定结果。认证关闭使 END/CLOSE 成功，普通对端 EOF 或 TLS
协议错误使挂起操作失败并保留 `xrtTlsStreamError()` 根因，本地主动 Abort 使
挂起操作进入 `XFUTURE_CANCELLED`。通用 `xrtFutureWait*()` 和
`xrtFutureAwait*()` 可以直接消费这些 Future，不增加 TLS 专用协程 API。
已认证应用明文始终先于 END 和接收侧 `XFUTURE_CLOSED` 交付；即使最后一个
应用记录与 `close_notify` 同批到达，也不会出现先观察 EOF、后出现残留明文的
窗口。

固定大小等待、接收元数据和总分配不超过 1 KiB 的发送节点共享所属 Worker 的
`NodeCacheBytes` 预算；较大发送保持一次普通堆分配，不把载荷塞入小节点缓存。活动
缓存节点持有临时 Engine 租约，节点归还后才释放。TLS Stream 发布最终 Close 后，
新建 Future 使用独立堆且不访问底层 TCP Worker，因此调用方保留的终态 TLS Stream
可以晚于 Engine 销毁，并继续取得 Close、EOF 或固定失败结果。

`xrtTlsStreamClose()` 排队一次 `close_notify`，排空 TCP，并等待对端经过认证的 `close_notify`。收到对端通知时先发布一次 `End`，双向认证关闭和 TCP 终态都完成后才发布 `CLOSED/XNET_RESULT_OK`。握手超时默认 10 秒，认证关闭超时默认 5 秒；配置值为零显式禁用对应 Timer。对端直接 EOF 映射为 `XTLS_ERROR_TRUNCATED`，关闭等待到期映射为 `XERR_TIMEOUT/XTLS_ERROR_CLOSED`。`Abort` 不生成 Alert 并立即放弃 TCP；若 TLS 已经失败但仍在发送 fatal Alert 或等待传输收尾，`Abort` 仍会加速关闭，同时保留原来的失败结果和根因。只有尚无失败根因的主动 Abort 才发布取消结果。

第一个 TLS、验证、内存、Timer 或传输根因保存在对象中，不会被后续关闭错误覆盖。失败 `Close` 回调中的 `pError` 和 `xrtTlsStreamError()` 都借用该稳定原因；底层 TCP 失败以 TLS 组合错误包装并保留原 Cause。正常 `CLOSED` 的 `xrtTlsStreamError()` 始终为空。

`xrtTlsStreamSession()` 是 Worker 内高级查询入口，可读取 ALPN、恢复票据等会话资产。`xrtTlsStreamTransport()` 借用原始 TCP Stream，只用于地址、统计和标准库尚未覆盖的只读/安全选项；应用不得关闭、收发、切换阻塞模式、替换事件或直接消费其缓冲。确实需要自定义传输行为时应回到公开会话层，而不是破坏组合对象状态机。

启用 `XRT_FEATURE_TLS_CLIENT_RESUME` 时，`xtlsstreamevents.Ticket` 在客户端
恢复队列新增 ticket 后于所属 Worker 发布。回调只表示“现在有票据可取”，
不转移 ticket，也不延迟 `Open`、HTTP 完成或连接关闭；处理器应通过
`xrtTlsStreamSession()` 取得会话，再循环调用 `xrtTlsClientTakeResume()`，
直到队列为空。队列已满并用新 ticket 替换最旧项时，长度虽然不变，仍会发布
新的边沿。切换 `xrtTlsStreamSetEvents()` 后，后续 ticket 只通知新的
处理器；已经在队列中的票据不会重放事件，接管层应在切换时主动排空一次。

完整 Echo 服务示例位于 `examples/tls/stream/main.c`，Future 用法位于
`examples/tls/stream_future/main.c`，使用系统信任库的主机名客户端位于
`examples/tls/dial/main.c`，Future Dial 位于
`examples/tls/dial_future/main.c`。select、IOCP 与 io_uring 共用同一份 TLS
Stream 和 TLS Dial 测试主体；后端文件只选择端口实现，不复制握手、背压、超时
或关闭断言。生命周期、Attach 失败原子性、失败后 Abort 根因保持、截断 EOF、
握手超时、认证关闭超时、单段和向量发送、向量失败原子校验、成功短写、
`Writable`/`Drain` 非重入、小 FeedLimit 大 TCP Read、延迟明文消费、组合对象
OOM、Timer 调度拒绝回滚、会话恢复、非法参数和单头文件真实传输门禁位于
`tests/tls/test_tls_stream*.c` 与 `tests/single/test_single_tls_stream*.c`。
Future 门禁另外覆盖 callback/pull 排他、并发硬预算、锁外错误构造、构造与结果
OOM、构造预留回滚、零字节发送、开始前取消、成功短写后的取消、关闭门之前
4 MiB 发送完整交付、关闭后的发送拒绝、同批应用记录先于 END、认证关闭、Abort、
异常关闭后的已认证明文、Worker 节点缓存复用、Engine 销毁后的终态 Future、
GCC/TinyCC、Select/IOCP 和通用协程恢复。主机名、验证名称、IPv6 到 IPv4 回退、
TCP 耗尽、解析期取消、全过程
超时、Timer 拒绝恢复、传输统计和 Open-before-Done 顺序由
`tests/tls/test_tls_stream_dial*.c` 压实。Engine Timer 扩容 OOM 的独立边界由
`tests/network/test_net_engine_oom.c` 压实。Future Dial 另外验证成功 Stream 在
Future 发布前已经 OPEN、保留引用后销毁 Future 不关闭连接、DNS/TCP/TLS 各阶段
协作取消、全过程超时、结构化原因链、Select/IOCP/io_uring 后端包装与通用协程
Await；公共桥接器的监听安装 OOM 由
`tests/concurrency/test_future_bridge_oom.c` 确定性覆盖。

### `xtlsdial`

托管 TLS 拨号对象（不透明）：串联 TCP 拨号与 TLS 握手。


```c
typedef struct xtlsdial xtlsdial;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。



### `xtlsdialconfig`

Timeout 覆盖 DNS、TCP 和 TLS 全过程；零值只保留各阶段超时。

```c
typedef struct xtlsdialconfig {
	xnetdialconfig Transport;
	xtlsstreamconfig Stream;
	int64 Timeout;
	bool ServerNameFromHost;
} xtlsdialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Transport` | `xnetdialconfig` | Transport |
| `Stream` | `xtlsstreamconfig` | 流选择 |
| `Timeout` | `int64` | 超时（毫秒） |
| `ServerNameFromHost` | `bool` | ServerNameFromHost |



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



### `xtlslistener`

TLS 监听器（不透明）：在 TCP 监听器上完成 TLS 接受。


```c
typedef struct xtlslistener xtlslistener;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。



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
| `Accept` | `bool (*Accept)(xtlslistener* pListener, xtlsstream* pStream, ptr pData)` | 新 TLS 流完成接受（返回是否保留） |
| `HandshakeError` | `void (*HandshakeError)(xtlslistener* pListener, const xerror* pError, ptr pData)` | 接受后握手失败 |
| `Error` | `void (*Error)(xtlslistener* pListener, const xerror* pError, ptr pData)` | 监听级错误 |
| `Close` | `void (*Close)(xtlslistener* pListener, ptr pData)` | 监听器关闭 |



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



### `xtlsstreamconfig`

两个超时都使用毫秒；零值显式关闭对应计时器。 AsyncBytesLimit 和 AsyncCountLimit 是未完成操作的独立硬边界， AsyncBatch 限制一次 Worker 轮转完成的操作数。

```c
typedef struct xtlsstreamconfig {
	int64 HandshakeTimeout;
	int64 CloseTimeout;
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
| `Open` | `void (*Open)(xtlsstream* pStream, ptr pData)` | 流开放（握手完成） |
| `Read` | `void (*Read)(xtlsstream* pStream, const xnetbuf* pBuffer, ptr pData)` | 收到解密缓冲 |
| `End` | `void (*End)(xtlsstream* pStream, ptr pData)` | 对端写关闭 |
| `Writable` | `void (*Writable)(xtlsstream* pStream, ptr pData)` | 发送预算可用 |
| `Drain` | `void (*Drain)(xtlsstream* pStream, ptr pData)` | 发送队列排空 |
| `Close` | `void (*Close)(xtlsstream* pStream, xnetresult Result, const xerror* pError, ptr pData)` | 流关闭（含错误） |



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


`XTLS_STREAM_ASYNC_BATCH_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XTLS_STREAM_ASYNC_BATCH_DEFAULT UINT32_C(64)
```

`XTLS_STREAM_ASYNC_BYTES_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XTLS_STREAM_ASYNC_BYTES_DEFAULT ((size_t)1048576u)
```

`XTLS_STREAM_ASYNC_COUNT_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XTLS_STREAM_ASYNC_COUNT_DEFAULT UINT32_C(1024)
```

`XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT INT64_C(5000)
```

`XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT INT64_C(10000)
```
