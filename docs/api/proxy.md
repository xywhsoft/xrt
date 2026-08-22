# Proxy

`<xrt/proxy.h>` 把代理能力拆成不可变配置、纯协议握手和托管传输三层。SOCKS5 CONNECT 与 HTTP CONNECT 共用同一套对象、缓冲、错误和托管拨号契约；HTTP 后端直接复用公开的 HTTP/1 Header 解析与封包能力，代理模块不维护第二套解析器。

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

## 所有权

- `xnetproxyconfig` 和 `xnetproxyhandshakeconfig` 中的视图在创建调用期间借用。
- `xnetproxy` 不可变并使用共享引用。
- `xnetproxyhandshake` 唯一所有，使用 `xrtNetProxyHandshakeDestroy` 销毁。
- `xnetproxydial` 使用共享引用；运行阶段持有自己的内部引用，用户使用 `xrtNetProxyDialRef` 和 `xrtNetProxyDialDestroy` 管理外部引用。
- 托管拨号成功时完成回调接管 Stream；失败时组合层回收内部 Stream，不向用户暴露半成品。
- `xnetproxyhandshakeconfig.Pool` 只借用；非空时必须比握手对象存活更久。
- `xrtNetProxyHandshakeOutput`、`xrtNetProxyInfo` 和 `xrtNetProxyHandshakeBound` 返回的视图都不能越过其所有者生命周期。
