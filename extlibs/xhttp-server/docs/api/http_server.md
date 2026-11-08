# xhttp-server 运行时

请求/响应对象及无 IO Exchange 见 [消息层](../../../xhttp/docs/api/http_server.md)。本页的网络执行与生命周期由 xhttp-server 提供，可选模块按需选择。

## HTTP/1 Server 运行时

启用 `XHTTP_FEATURE_HTTP_SERVER` 需要 `HTTP_SERVER_EXCHANGE`、
`HTTP_SERVER_RESPONSE` 和 `NET_TCP_SERVER`。`xrtHttpServerStart` 创建并立即启动一个
明文 HTTP/1 Server；Server 直接建立在 `xnetengine`、聚合 TCP Server、Stream 和无
I/O 协议状态机上，不通过函数表隐藏依赖。

`xhttpserverconfig` 的超时单位统一为毫秒。`HeaderTimeout`、`BodyTimeout`、
`RequestTimeout`、`IdleTimeout` 和 `WriteTimeout` 分别保护请求头、请求正文、
应用处理、keep-alive 空闲和响应无进展阶段；零表示关闭对应保护。

`xrtHttpServerConfigInit` 与 `xrtHttpServerEventsInit` 支持未对齐的完整结构存储。
`xrtHttpServerStart` 在打开 Listener 前验证地址范围并复制顶层配置和事件表；TCP
Server 的 `Network.Additional` 数组只在启动调用期间借用，启动成功后不再借用描述符。
全部逻辑端点必须同时绑定成功，失败时不会留下部分可用 HTTP Server。HTTP1 静态限额
直接无分配验证，不再为了检查配置创建临时 Exchange。
`MaxConnections` 为零时不增加应用层连接上限，`MaxInformations` 限制单个请求
排队的信息响应数量。`WriteSize` 只限制一次零复制发送租约，不给每个连接
预留固定发送缓冲；接收内存由 TCP 的按需 `xnetbuf` 和硬限额管理。

`xhttpserverconfig.Network` 完整暴露 TCP Server 的多端点、共享动态端口、
accept 队列和 reuse-port 配置。`xrtHttpServerEndpointCount`、
`xrtHttpServerLocal` 与 `xrtHttpServerListenerCount` 返回稳定拓扑；每个 Connection
通过 `xrtHttpConnEndpoint` 返回接受它的逻辑端点。`xrtHttpServerNetwork` 返回一份
调用方拥有的底层 `xnetserver` 引用，供特殊场景继续使用低层统计和 Listener 能力，
使用结束后必须调用 `xrtNetServerDestroy`。意外网络终止的首个结构化错误可通过
`xrtHttpServerError` 借用，并以 TCP Server 错误链作为 cause 保留。

所有应用事件都在连接所属 Worker 上串行执行：

- `Open` 发布地址、Worker 和底层 TCP 连接事实。
- `Headers` 在读取正文前选择缓冲、流式、丢弃或拒绝策略，也可以直接提交最终
  响应。无正文请求仍完成协议并允许 HTTP/1.1 keep-alive；有正文请求停在 Header
  边界，忽略正文策略返回值，并在最终响应排空后关闭。需要按路由调整上传大小时调用
  `xrtHttpConnSetRequestBodyLimit`，它直接修改当前 Exchange 的唯一硬上限。
- `Body` 只接收流式模式的借用正文片段；返回 `false` 会产生结构化回调错误和
  500 响应，不再调用 `Request`。如果回调已经成功提交最终响应，该响应优先，
  当前片段被接受，剩余正文停止交付，连接在响应排空后关闭。回调可以调用
  `xrtHttpConnPauseRequestBody` 接受当前片段并暂停后续交付。
- `Request` 只接收完整请求。回调可以同步提交响应，也可以保留 Connection，
  稍后投递回所属 Worker 再响应。
- `Error` 对一个连接最多发布一次稳定错误，`Close` 只发布一次终态，
  传输失败时固定按 `Error -> Close` 顺序发布；`Shutdown` 在 Server 全部连接
  的协议对象、传输所有权和内部运行时退出，且已经受理的 Upgrade 交接回调返回后发布。
  尚未被 Close/Upgrade 适配器取走的内部上下文在所属 Worker 上清理后才退出运行时。
  调用方保留的已关闭/升级 Connection 引用不延迟这项清理；最终释放引用不会重复清理。


`xrtHttpConnInform` 提交 `100..199` 信息响应，`101` 除外。信息响应和最终响应
使用同一短写、背压和写超时线路；有正文的请求会暂停读取，无正文的请求可以
直接把最终响应排在信息响应之后。一个请求只允许一次
`xrtHttpConnRespond`、`xrtHttpConnReply` 或 `xrtHttpConnReplyBody` 最终提交。

`xrtHttpConnReply` 是固定 JSON、文本和短错误响应的直接路径：

```c
(void)xrtHttpConnReply(
	connection,
	200,
	XRT_STR_LITERAL("application/json; charset=utf-8"),
	XRT_BYTES_LITERAL("{\"code\":200,\"msg\":\"OK\"}")
);
```

它把正文复制为一个紧凑 Body，并直接进入与 Reply 共用的 HTTP/1 冻结内核，
不创建临时 Reply 或 Header 容器，也不要求调用方创建字典、JSON 对象。
`xrtHttpConnReplyBody` 是同一直接路径的正文来源版本：它保留一个可选
`xhttpbody` 引用，调用返回后调用方即可销毁自己的引用。Borrow、Take、Reference、
文件和自定义生产者因此可以避免正文复制，未知长度会按 HTTP/1.1 chunked 或
HTTP/1.0 关闭分帧。需要动态字段或 Trailer 时再使用 `xrtHttpConnRespond`。
流式出站正文、文件、TLS 传输和后续 Upgrade 都是独立
裁剪组合层，不能在核心里形成隐式的第二套协议状态机。

流式入站正文使用显式应用背压。`xrtHttpConnPauseRequestBody` 只允许在连接
Worker 的 `Body` 回调内调用；当前借用片段在回调返回后即失效，应用需要自行
复制或处理。异步消费者完成后，可以从任意线程调用
`xrtHttpConnResumeRequestBody`，运行时通过 Connection 内嵌命令恢复同一个
Exchange，不分配任务节点。重复恢复是成功的空操作，
`xrtHttpConnRequestBodyPaused` 和 `xhttpconnstats.RequestBodyPaused` 提供并发
快照。暂停期间 `BodyTimeout` 继续计时，应用必须在资源时限内恢复；这避免失联
消费者无限占住连接。暂停期间发送的信息响应不会隐式解除应用背压；最终响应
一旦提交，则终止公开暂停状态、在 Exchange 内固定停止消费边界，并按未完整消费
请求的关闭规则收敛连接。同一 TCP 输入片段中的剩余正文和流水线后缀不会继续
进入 HTTP Parser。

## HTTPS Server

启用 `XHTTP_FEATURE_HTTP_SERVER_TLS` 需要 `HTTP_SERVER` 与 `TLS_STREAM`。
`xrtHttpServerTlsConfigInit` 初始化 TLS 1.3 Server、组合 Stream 和
`http/1.1` ALPN 默认值，`xrtHttpServerStartTls` 创建并立即启动 HTTPS Server。
TLS 只替换连接传输，不复制 HTTP/1 解析器、请求状态机、响应准备、正文泵、
Timer、统计或优雅停机实现。

TLS 顶层配置与 ALPN 描述符可以存放在未对齐地址，但都必须覆盖完整且不回绕的
可读范围。启动函数在创建 Server 前只读取一次顶层配置；ALPN 描述符随后按值
读取，协议字节被深复制，Context、Identity 被保留。调用返回后可以立即修改或
释放配置结构和 ALPN 数组，只有选择器与恢复回调的上下文仍由调用方保持。

```c
xhttpservertlsconfig Tls;

xrtHttpServerTlsConfigInit(&Tls);
Tls.Handshake.Identity = Identity;
Server = xrtHttpServerStartTls(
	Engine,
	&ServerConfig,
	&Tls,
	&Events
);
```

Server 在启动成功前保留 `Handshake.Context` 和 `Handshake.Identity`，并深复制
`Handshake.Protocols` 数组及每个协议字节；调用方随后可以释放或改写这些来源。
`SelectContext` 与 `ResumeContext` 是回调上下文，仍由调用方保持到 Server 完全
关闭。启动前会创建并销毁一个真实服务端 Session，验证身份、策略和限额，
因此无效配置不会先开放 Listener 再在首个连接上失败。配置快照、Session
预验证或 Listener 启动失败都会释放已经保留的 TLS 对象和协议副本；可分配错误
对象时，错误统一提升为 `xrt.http.server` 并保留原始内存、TLS 或网络 Cause。

HTTP/1 Server 只接受未配置 ALPN，或只包含 `http/1.1` 的 ALPN 配置。它不会在
HTTP/1 解析器后面协商 `h2`；需要 HTTP/2 时必须使用未来独立的协议运行时。
`Open` 只在 TCP 已接收、TLS 握手完成且会话进入 `READY` 后发布。握手、证书、
ALPN、认证关闭和底层 I/O 错误进入 `xrt.http.server` 域的
`XHTTP_SERVER_ERROR_TLS`，并保留原 TLS 或网络 Cause。握手和认证关闭超时计入
Server 的 `Timeouts`，但不会误计为 HTTP `ProtocolErrors`。

明文 TCP 响应使用引用发送；HTTPS 把每个已受理明文前缀直接加密进 TLS
Session，受理后即可释放对应 Body 租约，不建立每连接固定发送缓冲。
TLS Session 暂存和 TCP 用户态队列共同受硬限额约束，短写后由同一个
`Writable` 路径继续。`xhttpconnstats.ResponseWireBytes` 统计 HTTP 明文字节，
`BufferedBytes` 统计未消费 TLS 明文，`QueuedBytes` 则统计 TLS Session 与
底层 TCP 中尚未排空的密文字节；`Drain` 发布时后者为零。

`xrtHttpServerSecure`、`xrtHttpConnSecure` 和统计中的 `Secure` 可以并发判断
传输类型。Connection Worker 回调内可以用 `xrtHttpConnTls` 借用 TLS Stream，
或用 `xrtHttpConnTcp` 借用它独占的底层 TCP Stream；二者只用于已公开的安全
查询和扩展入口，不能绕过 HTTP/TLS 状态机直接收发、关闭或替换事件。

完整可运行示例见 `examples/http/server_tls/main.c`。

## 原始响应

启用 `XHTTP_FEATURE_HTTP_SERVER_RAW` 需要 `HTTP_SERVER`。该模块用于已经拥有完整
HTTP/1 线缆报文的固定响应、代理或专用协议工具，不要求创建 `xhttpreply`，
也不会让 Reply 构建器成为服务器的强制路径。两种入口都只能在当前请求的
Connection Worker 上提交唯一最终响应：

- `xrtHttpConnRespondRaw` 复制完整报文，适合短小、静态或临时拼接的数据。
- `xrtHttpConnRespondRawBody` 保留已知非零长度的 `xhttpbody` 引用；调用方可以
  使用 `Borrow`、`Take` 或 `Reference` 选择借用、接管或自定义释放。

```c
static const uint8 response[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: application/json; charset=utf-8\r\n"
	"Content-Length: 23\r\n"
	"Connection: close\r\n"
	"\r\n"
	"{\"code\":200,\"msg\":\"OK\"}";

(void)xrtHttpConnRespondRaw(
	connection,
	(xbytesview){ response, sizeof(response) - 1u },
	XHTTP_SERVER_RAW_NONE
);
```

原始路径不会解析、验证、改写或补全报文。状态行、Header、`Content-Length`、
chunked 终止块、`HEAD`、无正文状态码和 Upgrade 语义均由调用方负责。Body 的
公开长度必须已知且非零，Reader 的实际总输出必须严格等于该长度；当前同步
组合层不接受正文源的 `AGAIN`。

默认策略是在报文排空后关闭连接。只有显式传入
`XHTTP_SERVER_RAW_KEEP_ALIVE`，并且请求版本与 `Connection` 允许复用、请求已
完整接收、对端没有半关闭且 Server 未进入排空，运行时才会读取下一条请求。
运行时不会根据原始报文中的 `Connection` 字段推断复用策略。无论选择哪种
所有权和关闭策略，原始路径仍与 Reply 路径共享最终响应门、传输发送租约、
短写推进、发送背压、写超时和统计。

完整可运行示例见 `examples/http/server_raw/main.c`。

## Upgrade 所有权转移

启用 `XHTTP_FEATURE_HTTP_SERVER_UPGRADE` 需要 `HTTP_SERVER`。该模块不解析
WebSocket，也不复制 TCP、TLS 或 HTTP 状态机；它只负责把已经形成 Tunnel
终态的 HTTP/1 响应完整排入发送队列，然后原子摘除 HTTP 事件与连接归属。

三个提交入口都只接受已经完整解析的当前请求，通常只能从 `Request` 回调或其
后续 Worker 任务调用。`Headers` 和 `Body` 回调中的请求尚未完成，提交会以
`XERR_STATE`、`XHTTP_SERVER_ERROR_STATE` 同步失败且不安排完成回调。这个边界
保证交给新协议的缓冲余量只来自 HTTP 请求报文之后，而不会包含尚未消费的请求体。
纯协议检查、鉴权和响应准备仍可提前执行，不会被接管时机反向绑定。

入口按由底到高分为三层：

- `xrtHttpConnUpgradeResponse` 接管公开的 `xhttp1serverresponse`，只接受
  已经验证为 Tunnel 的 101 Upgrade 或成功 CONNECT 计划。
- `xrtHttpConnUpgrade` 借用常规 `xhttpreply`，适合由协议 Helper 设置 101、
  `Connection`、`Upgrade` 和协商字段。
- `xrtHttpConnUpgradeRaw` 复制完整线缆切换响应，适合固定或已经自行拼好的
  Header；运行时不会再次解析、补全或改写这些字节。输入必须是完整、不回绕的
  非空字节范围，地址范围错误会在分配和响应提交前以参数错误失败。

受理成功后，`xhttpupgradeproc` 恰好执行一次，而且不会从提交函数调用栈重入。
成功结果的 `xhttpupgrade` 恰好拥有 `Tcp` 或 `Tls` 之一，回调必须把该引用移入
新协议对象，或者用 `xrtHttpUpgradeAbort` 关闭并释放。失败结果不携带传输，
`Error` 与 `Connection` 只在回调期间借用。受理失败时不会安排回调，低层
`xrtHttpConnUpgradeResponse` 仍会销毁传入的非空响应计划。

`xrtHttpUpgradeAbort` 接受未对齐但完整的 `xhttpupgrade`，会先清空结构再关闭
传输，并保持调用方已有错误；因此它可以直接用于错误出口而不会遮蔽原始失败。

HTTP 在切换前暂停明文读取，防止下一协议的字节进入 HTTP Parser。成功回调
必须先为 TCP 或 TLS 安装新协议事件，再显式处理传输缓冲；`Upgrade.Buffered`
给出切换时已经存在的明文字节数。TCP 接管者处理余量后调用
`xrtNetStreamResume`，TLS 则在明文被消费后按自身背压契约恢复。事件切换不会
隐式重放 Read，因而同一批后缀不会被 HTTP 与新协议重复消费。

切换成功后 `xhttpconn` 进入 `XHTTP_CONN_UPGRADED`，HTTP 的 Close 事件不再
代表该传输的终态，`xrtHttpConnClose` 与 `xrtHttpConnAbort` 也会拒绝继续控制
它。连接立即从 Server 列表摘除，所以 `xrtHttpServerDrain` 不等待 WebSocket
或其他已转交会话的生命周期；但 `Shutdown` 与 `xrtHttpServerWaitAsync` 会等待
已经受理的 Upgrade 交接回调返回，调用方可以在关闭等待完成后释放回调上下文。
`xhttpserverstats.Upgraded` 记录成功转移数，成功的 101
同时计入 `Responses`；切换前中止只执行 Upgrade 失败回调，不计成功响应。

最小所有权示例见 `examples/http/server_upgrade/main.c`。完整回环边界测试覆盖
Reply、拥有型 Response、完整线缆 Raw、切换前中止、Select、Windows IOCP 和
TLS 明文余量交接，见 `tests/http/test_http_server_upgrade*.c`。

`xrtHttpServerDrain` 停止 accept，关闭空闲连接，并允许活动请求发送最后一个
响应；`xrtHttpServerAbort` 丢弃全部连接。二者可从任意线程调用且幂等。
`xrtHttpServerDestroy` 只释放调用方引用，不会隐式停止运行中的 Server。
Connection 的 `Close`、`Abort` 可跨线程调用，响应提交必须回到所属 Worker。

运行时错误域固定为 `xrt.http.server`。错误保留 kind、code、operation、
message 和底层 cause；完全 OOM 时使用无分配的静态 `xrt.memory` 错误，
因此不能要求域包装再次分配成功。协议、回调、超时、OOM 与系统传输错误不会
被统一压成整数或线程局部文本。`xhttpserverstats` 和 `xhttpconnstats` 提供无锁
快照，覆盖安全传输标记、连接、请求、响应、信息响应、成功 Upgrade、协议错误、超时、
HTTP 明文线路字节和传输队列字节。

完整可运行示例见 `examples/http/server/main.c`。

## 认证

请求认证读取保持与其他结构化字段相同的三态契约：`XHTTP_NEXT_END` 表示缺失，`XHTTP_NEXT_ITEM` 表示有效，`XHTTP_NEXT_ERROR` 表示重复字段、非法语法、方案不匹配或解码失败。

```c
char decoded[256];
xhttpbasicauth basic;
size_t size;

if ( xrtHttpServerRequestBasicAuth(
	request,
	decoded,
	sizeof(decoded),
	&size,
	&basic
) != XHTTP_NEXT_ITEM ) {
	/* 缺失和错误由路由策略分别处理。 */
}
```

通用凭据使用 `xrtHttpServerRequestAuth`，Bearer 使用 `xrtHttpServerRequestBearerAuth`，Digest 使用 `xrtHttpServerRequestDigestAuth`；代理入口保持同名对称。通用和 Bearer 结果借用请求快照，Basic 与 Digest 的已解码结果借用调用方缓冲。重复 `Authorization` 归类为 Header 协议错误，其余认证值错误使用 `XHTTP_SERVER_REQUEST_ERROR_AUTH`。

Basic 入口在缺失时发布空描述符和零长度；在格式错误或 OOM 时清空结果描述符，但保持调用方长度和正文缓冲不变；短缓冲只发布精确所需长度。该规则与底层 `xrtHttpBasicRead` 一致，服务端便利层不会提前提交半份明文或错误长度。

Cookie 批量入口先严格校验全部 `Cookie` 字段，再原子写出借用项；容量不足只发布精确数量，不修改数组。单项查找同样先验证全部字段，并在未命中或字段错误时发布空 `xcookiepair`，调用方不会误用上一次命中的借用视图。

Reply 的 challenge 使用追加语义，以保留多个认证方案：`xrtHttpReplyAddChallenge` 接受任意合法方案和 token68/auth-param，Basic、Bearer、Digest 入口分别负责各自的完整语义校验和安全写出。代理响应使用带 `Proxy` 的对称入口。认证构建器不替应用自动修改 401/407 状态，因此它也可以用于上层策略生成或转发 challenge。

服务端验证 Digest 时，先解码凭据并按用户名或 userhash 查得持久化的 `H(username:realm:password)`，再用请求方法、原始 request-target 和实际发布的 challenge 填充 `xhttpdigestverification`。`xrtHttpDigestVerify` 一次验证 algorithm、qop、realm、opaque、nonce 签名与期限、目标绑定和 request-digest，并严格区分 `VALID/STALE/INVALID/ERROR`；只有签名与证明都正确的过期请求才返回 `STALE`。自定义 nonce 或轮换 key-ring 可以使用 `xrtHttpDigestProofVerify` 与 `xrtHttpDigestNonceVerify` 两层入口组合。只有证明返回 `VALID` 后才能提交单调 `nc`；提前提交会允许错误证明抢占合法客户端计数。`xrtHttpDigestReplayCheck` 提供线程安全、有硬容量的单进程重放表，多进程或分布式部署应使用 `xrtHttpDigestReplayKey` 生成同一规范键，再交给共享存储执行带过期时间的原子最大值更新。

验证成功后，可用同一证明上下文调用 `xrtHttpDigestRspAuth`，再通过 `xrtHttpReplySetDigestInfo` 设置唯一 `Authentication-Info`。该入口支持完整 `rspauth` 组和可选 `nextnonce`；代理回执使用对称入口。challenge 采用 Add、回执采用 Set，分别对应 RFC 字段的多值和唯一语义。

## Future 响应

启用 `XHTTP_FEATURE_HTTP_SERVER_FUTURE` 需要 `HTTP_SERVER` 与
`FUTURE_CONTINUE`。

`xrtHttpServerWaitAsync` 为 Server 的 `CLOSED` 终态建立独立 Future。它可以从任意
线程调用；每次调用都有独立的取消边界，取消只摘除当前等待，不会调用 drain、abort，
也不会影响其他等待者。Server 已经关闭时返回立即成功的 Future。已经受理的 Upgrade
交接回调先返回，`Shutdown` 回调再完成，随后已有关闭 Future 按登记顺序成功；等待返回
后可以确认 HTTP 协议对象、传输所有权、内部连接运行时和所有权回调均已退出，因此调用方
可以停止不再承载其他对象的 Engine；但这不代表已转交的新协议会话已经关闭。创建过程中
任意 OOM 都完整回滚，不在 Server 中残留节点或引用。

```c
xfuture* closed = xrtHttpServerWaitAsync(server);

if ( (closed == NULL) || !xrtHttpServerDrain(server) ||
	(xrtFutureWaitFor(closed, UINT64_C(5000000)) != XWAIT_OK) ||
	(xrtFutureState(closed) != XFUTURE_RESOLVED) ) {
	(void)xrtHttpServerAbort(server);
}
xrtFutureDestroy(closed);
```

协程直接 await 同一个 Future；同步宿主线程使用 `xrtFutureWait*`，HTTP 层不复制
超时、截止时间或协程 API。调用方需要在发起 drain/abort 前取得 Future，便于统一处理
正常关闭与错误清理；迟注册仍是合法的终态查询。

`xrtHttpConnRespondFuture` 只在当前完整请求的所属 Worker
上绑定一次最终响应；源可以是 Promise、任务池或协程产生的普通 `xfuture`，
HTTP 层不再建立另一套 Task、Coroutine 或 Async API。

Future 成功值必须是非空的借用 `xhttpreply*`。桥接层持有源 Future，完成后回到
Connection Worker 冻结 Reply，因此 Reply 的拥有者或 Future 值析构过程仍负责
最终销毁对象。响应准备完成后不再借用 Reply；Future 可以随即释放自己的值。
`Request` 回调收到的 `xhttpserverrequest*` 及其视图只借用到回调返回；后台任务
必须在回调内复制自己需要的方法、target、Header 与正文，不能保存请求指针。

```c
static void onRequest(
	xhttpserver* server,
	xhttpconn* connection,
	const xhttpserverrequest* request,
	ptr data
)
{
	/* 此函数在返回前复制任务需要的请求数据。 */
	xfuture* future = startReplyTaskCopy(request, data);

	(void)server;
	if ( (future == NULL) ||
		!xrtHttpConnRespondFuture(connection, future) ) {
		(void)xrtHttpConnReply(
			connection,
			500,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Internal Server Error")
		);
	}
	xrtFutureDestroy(future);
}
```

手工最终响应、请求超时、连接异常关闭和 Server abort 都会请求取消源 Future。
取消只是一条协作通知：不合作的生产者可以继续运行，但桥接对象会立即切断
Connection 引用，因此不会把连接或 Server 生命周期绑在失控任务上。手工响应
与已投递完成竞争时，最先提交的最终响应获胜。

Future 失败生成 500，错误 kind 为 `TIMEOUT` 时生成 504，取消或关闭终态生成
503；原始错误作为 `xrt.http.server` 原因链发布。无值成功和非法 Reply 都按
响应错误处理。注册期间任意 OOM 都原子回滚，当前请求仍可重新绑定 Future 或
直接响应。

等待应用响应时，运行时只继续把新字节收进 TCP 的硬上限缓冲，不会提前解析
下一条流水线请求。这样 completion 与 readiness 后端都能及时观察 RST 和接收
错误。TCP EOF 只表示对端写半关闭：已经完成的请求仍可正常返回响应；只有
异常关闭、应用超时或显式取消才终止 Future。

## 文件响应

启用 `XHTTP_FEATURE_HTTP_SERVER_FILE` 会组合文件 Body、Future 响应桥和异步
服务器 Body 传输，但不会把静态网站策略绑定进 Server 核心：

- `xrtHttpBodyFileFuture` 是可复用的底层异步文件正文。
- `xrtHttpReplyFileFuture` 和 `xrtHttpReplyFileRangeFuture` 构建 Future 拥有的 Reply。
- `xrtHttpConnFile` 和 `xrtHttpConnFileRange` 在 Connection Worker 内一次绑定最终响应。

它们共享 HTTP/1 分帧、TCP 背压、写时限和结构化错误线路，不建立第二套文件
读取或网络发送实现。Range、Content-Range、ETag 与条件请求已经由独立
`http_semantics` 公共层提供。完整所有权、任务池寿命和示例见
`docs/api/http_server_file.md`，协议原语见 `docs/api/http_semantics.md`。

启用 `XHTTP_FEATURE_HTTP_SERVER_STATIC` 会在上述文件 Body 和 HTTP 语义之上
增加可选静态资源策略层。`xrtHttpReplyStaticFuture` 组合安全路径映射、受限
文件根、MIME、条件请求、单范围与多范围响应；`xrtHttpConnStatic` 为常用服务端
路径提供一次调用。更低层的路径、计划、响应、已打开文件和 Reply 桥仍全部公开，
调用方可以插入路由、鉴权、缓存或自定义正文，而不必使用一站式接口。完整契约见
`docs/api/http_server_static.md`。

## 三条响应路径

服务端运行时将并列保留三种路径：

- 原始路径：直接提交完整 Header 或完整报文字节，适合预构建固定响应与代理。
- Reply 路径：提交 `xhttpreply`，由统一 HTTP/1 准备层生成准确分帧。
- 流式路径：先提交状态与 Header，再按背压发送正文片段、Trailer 和结束标记。

Reply 是可选数据构建器，不是服务器发送响应的强制前置对象。固定 JSON 响应
最终可使用单个连接便利函数直接提交状态、Content-Type 和字节，不需要创建
字典、JSON 对象或 Reply。
