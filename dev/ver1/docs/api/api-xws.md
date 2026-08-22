# XWS WebSocket 标准库契约

> XWS 是建立在 `xnet-v2`、HTTP/1.1 Upgrade 和 XRT TLS 之上的 RFC 6455 客户端与服务端实现。

[返回索引](README.md)

---

## 1. 支持范围

当前稳定主线支持：

- `ws://` 与 `wss://` 客户端；
- 独立 WebSocket 服务端；
- 从 `xhttpd` / `xweb` 请求升级为 WebSocket；
- text、binary、ping、pong、close；
- 分片消息发送与接收重组；
- subprotocol、Origin、Path 和自定义请求头策略；
- 严格的 UTF-8、mask、RSV、opcode、控制帧、长度编码和 close code 校验；
- `permessage-deflate`，只协商无上下文接管模式；
- 发送硬上限、high/low watermark、writable/drain 通知；
- copy 与 `xnetbufref` 引用发送；
- Future、同步等待和协程等待；
- 结构化错误与完整关闭快照。

本契约不包含 HTTP/2、HTTP/3、QUIC，也不承诺任意第三方 WebSocket extension。接收侧以完整消息回调为边界，不提供逐帧或流式消息体回调。

## 2. ABI 与初始化

公共结构使用 `iSize + iVersion` 进行 ABI 协商：

| 结构 | 当前版本 |
| --- | ---: |
| `xwsclientconfig` | V4 |
| `xwsserverconfig` | V4 |
| `xwsclientevents` | V3 |
| `xwsserverevents` | V3 |
| `xwscloseinfo` | V1 |
| `xwserrorinfo` | V1 |
| `xwshandshakeresponse` | V1 |

始终使用对应初始化函数，不要自行假设结构尾部：

```c
xrtWsClientConfigInit(&cfg);
xrtWsClientEventsInit(&events);
xrtWsServerConfigInit(&server_cfg);
xrtWsServerEventsInit(&server_events);
```

V4 配置在 V3 尾部追加自定义 TLS 配置所有权字段；创建函数仍接受合法的旧版本结构。事件复制也只读取调用方通过 `iSize` 声明的部分。

## 3. 默认资源限制

`ConfigInit` 提供适合通用服务的有限默认值：

| 配置 | 客户端默认值 | 服务端默认值 |
| --- | ---: | ---: |
| `iRecvLimit` | 1 MiB | 1 MiB |
| `iMaxFrameBytes` | 1 MiB | 1 MiB |
| `iHighWater` | 256 KiB | 256 KiB |
| `iLowWater` | 64 KiB | 64 KiB |
| `iMaxQueuedBytes` | 1 MiB | 1 MiB |
| `iHandshakeMaxBytes` | HTTP/1 默认头部上限 | HTTP/1 默认头部上限 |
| `iHandshakeTimeoutMs` | 10 s | 10 s |
| `iCloseTimeoutMs` | 5 s | 5 s |
| `iCompressMinBytes` | 256 B | 256 B |
| `iCompressionLevel` | 6 | 6 |
| `iBacklog` | - | 128 |

`iRecvLimit` 限制重组后的消息，`iMaxFrameBytes` 限制单个帧，`iHandshakeMaxBytes` 独立限制 Upgrade 头部。服务端应按业务场景收紧这些值；不要依赖无限缓冲。

`XWS_URL_CAP`、`XWS_PATH_CAP` 等宏只是配置结构的内联存储阈值。setter 会为更长的合法值动态分配，不是协议长度上限。

## 4. 配置所有权

推荐通过 setter 设置动态值：

```c
xrtWsClientConfigSetURL(&cfg, "wss://example.com/socket");
xrtWsClientConfigSetOrigin(&cfg, "https://example.com");
xrtWsClientConfigSetProtocols(&cfg, "chat.v2, chat.v1");
xrtWsClientConfigSetHeader(&cfg, "Authorization", "Bearer ...");
xrtWsClientConfigSetTlsConfig(&cfg, &tls);
```

客户端和服务端创建时会深拷贝字符串、请求头和 TLS 配置，因此创建成功后可以立即调用 `xrtWs*ConfigUnit` 并释放源配置。TLS clone 深拷贝主机名、ALPN、证书、私钥、CA、CRL 和 session 数据；C 回调函数及其 `pUserData` 是借用值，调用方必须让它们至少存活到 WebSocket 对象销毁。

`pProxy` 是引用计数对象。客户端创建时 retain，客户端销毁时 release；调用方可以在创建后释放自己的引用。

配置 setter 分配过存储时必须调用对应 `ConfigUnit`。不要手动释放 `p*Storage` 字段。

## 5. 握手与策略

客户端 URL 必须是绝对 `ws://` 或 `wss://` URL。userinfo 和 fragment 会被拒绝。连接顺序固定为：

1. DNS/TCP connect；
2. 可选代理握手；
3. `wss://` 的 TLS 握手；
4. HTTP/1.1 Upgrade；
5. 进入 OPEN 状态并触发 `OnOpen`。

客户端可用 `OnHandshakeResponse` 检查服务器的 101 响应；返回 `false` 会拒绝该连接。

独立服务端可配置固定 Path、Origin 和 subprotocol，也可在 `OnHandshake` 中进行逐请求策略：

- 返回 `XWS_HANDSHAKE_ACCEPT` 接受；
- 返回 `XWS_HANDSHAKE_REJECT` 发送配置的 HTTP 拒绝响应；
- 返回 `XWS_HANDSHAKE_ERROR` 终止握手。

`xwshandshakeresponse` 可设置状态码、原因、响应头、正文、选中的 subprotocol、连接私有数据及 `XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE`。回调返回前必须保证选中的协议来自客户端提议列表。

已有 HTTP 服务可使用：

```c
xrtWsServerUpgradeHttpd(ws_server, http_conn, request, &ws_conn);
xrtWebResponseUpgradeWebSocket(response, ws_server, &ws_conn);
```

Upgrade 必须在 HTTP worker/handler 上下文调用。成功的 101 会把底层 stream 所有权交给 XWS；HTTP 拒绝响应则由 HTTP 层提交，`ws_conn` 为 `NULL`。

## 6. 事件与线程契约

事件在网络引擎 worker 线程上调用。除非 API 明确允许，否则不要在回调中阻塞 worker。`OnText`、`OnBinary`、`OnPing`、`OnPong` 的 payload，以及握手请求/响应视图，都只在当前回调期间有效；需要异步保存时必须复制。

服务端 `xwsconn*` 在回调期间有效。要跨回调、跨任务或放入用户容器，必须先调用 `xrtWsConnRetain`，使用完成后调用 `xrtWsConnRelease`。`xrtWsConnSetData/GetData` 只保存借用指针，不接管用户对象。

旧事件 `OnClose`、`OnError` 保留兼容性；新代码应优先使用 `OnCloseEx` 和 `OnErrorEx`。

## 7. 消息与压缩

接收端会重组分片并以完整消息触发 `OnText` 或 `OnBinary`。text 消息和 close reason 必须是完整、最短编码的 UTF-8；非法数据会触发协议关闭。Ping 会自动回复 Pong，同时仍触发 `OnPing`。

启用压缩：

```c
cfg.iWebSocketFlags |= XWS_F_PERMESSAGE_DEFLATE;
```

只有在 `XWS_HAS_PERMESSAGE_DEFLATE == 1` 时可用。XWS 只协商 client/server no-context-takeover，避免跨消息字典带来的内存不可控与并发状态复杂度。发送端仅在消息达到 `iCompressMinBytes` 且压缩后确实更小时使用压缩。通过 `xrtWsClientPerMessageDeflate` 或 `xrtWsConnPerMessageDeflate` 查询实际协商结果。

## 8. 背压与发送所有权

`iMaxQueuedBytes` 是硬上限，不只是通知阈值。数据帧还会为 Ping/Pong/Close 预留控制帧预算。发送队列无法接收完整帧时，发送 API 返回 `XRT_NET_AGAIN`，不会部分接收该消息。

处理方式：

1. 停止继续生产；
2. 等待 `OnWritable`、`WritableFuture` 或 `XNET_STREAM_WAIT_WRITABLE`；
3. 原样重试；
4. 需要等待全部排空时使用 `OnDrain`、`DrainFuture` 或 `XNET_STREAM_WAIT_DRAIN`。

`OnBackpressure` 在越过 high watermark 时触发，`OnWritable` 在回落到可写区间时触发，`OnDrain` 表示发送队列归零。`xrtWs*PendingSend` 返回当前排队字节数。

普通 `SendText/SendBinary` 在调用期间复制或构帧，调用返回后源内存即可复用。

`SendTextRef/SendBinaryRef` 和 writer 的 Ref API 使用 `xnetbufref`：

- 返回 `XRT_NET_OK` 表示所有权已被接受，`pfnRelease` 恰好调用一次，可能在调用内立即发生；
- 返回 `XRT_NET_AGAIN`、`XRT_NET_ERROR` 或 `XRT_NET_CLOSED` 表示未接受，调用方仍拥有引用且 release 不会被 XWS 调用；
- 客户端 mask 和 TLS 路径可能退化为一次构帧复制，但仍保持相同所有权契约；
- 明文服务端发送可直接把 payload 引用交给底层 scatter/gather 队列。

## 9. 分片 writer

`xrtWsClientBeginText/Binary` 与 `xrtWsConnBeginText/Binary` 创建未压缩的分片消息 writer。writer 存活期间独占该连接的数据消息发送；另一个 text/binary send 或 writer 会失败，但 Ping 和 Close 仍可发送。

```c
xwswriter* writer = xrtWsConnBeginText(conn);
xrtWsWriterWrite(writer, first, first_len);
xrtWsWriterFinish(writer, last, last_len);
xrtWsWriterDestroy(writer);
```

`Write` 发送非终帧，`Finish` 发送终帧。返回 `XRT_NET_AGAIN` 时 writer 状态不推进，可在 writable 后用相同输入重试。文本 writer 会跨分片保持增量 UTF-8 状态，最后一片必须结束在完整码点。销毁未完成 writer 会释放独占权，但不会自动补发终帧。

## 10. 关闭状态机

`xrtWs*Close(code, reason)` 发送 Close 后禁止新的数据消息，但允许完成关闭握手。收到对端 Close 时 XWS 会按 RFC 6455 回复；超过 `iCloseTimeoutMs` 仍未完成则中止 transport。

`xwscloseinfo` 保存：

- 本地与远端 close code；
- 远端 reason；
- transport 结果；
- 是否已发送、已接收、clean、由远端发起等 flags。

`CloseFuture`、`XNET_STREAM_WAIT_CLOSE` 和 `OnCloseEx` 都观察同一个终态。`Stop`/`Destroy` 可从非 worker 线程调用；销毁前仍应释放应用持有的服务端连接引用。

## 11. Future、同步与协程

客户端启动有三种等价观察方式：

- 事件：`xrtWsClientStart` + `OnOpen/OnErrorEx`；
- Future：`xrtWsClientStartFuture` 或 `xrtWsClientOpenFuture`；
- 等待：`WaitOpen`、`WaitOpenTimeout/Until`、`WaitOpenCo*`。

客户端与服务端连接都提供 writable、drain、close Future，并支持：

```c
xrtWsClientWaitTimeoutEx(client, XNET_STREAM_WAIT_WRITABLE, 5000);
xrtWsConnWaitCoEx(conn, XNET_STREAM_WAIT_DRAIN);
```

支持的 wait kind 只有 `XNET_STREAM_WAIT_WRITABLE`、`XNET_STREAM_WAIT_DRAIN` 和 `XNET_STREAM_WAIT_CLOSE`。同步等待不要在同一网络 worker 上调用；协程中使用 `WaitCo*`。

## 12. 错误模型

`OnErrorEx`、`xrtWsClientLastError`、`xrtWsConnLastError` 和 `xrtWsServerLastError` 返回 `xwserrorinfo` 快照，包含：

- `iResult`；
- category：transport、handshake、protocol、UTF-8、limit、compression、memory、callback、internal；
- operation：connect、handshake、receive、send、message、close、accept；
- phase：validate、build、parse、policy、submit、process、timeout、complete；
- system error、HTTP status、protocol error、close code 和稳定的诊断文本。

错误快照由对象拥有；accessor 会复制到调用方结构。不要仅依赖线程全局错误字符串诊断异步连接。

## 13. 最小客户端示例

```c
xwsclientconfig cfg;
xwsclientevents events;
xwsclient* client;

xrtWsClientConfigInit(&cfg);
xrtWsClientEventsInit(&events);
xrtWsClientConfigSetURL(&cfg, "wss://example.com/ws");
cfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
client = xrtWsClientCreate(engine, &cfg, &events, user_data);
xrtWsClientConfigUnit(&cfg);

if (client && xrtWsClientStart(client) == XRT_NET_OK) {
    /* OnOpen / future / wait reports handshake completion. */
}
```

## 14. 发布门禁

协议 parser、互操作、1009 限制关闭恢复、重连与 single-header 的统一命令见 [HTTP / WebSocket 发布门禁](../HTTP_WEBSOCKET_RELEASE_GATES.md)。确定性驱动不替代 Clang/libFuzzer 覆盖率引导测试。

## 15. 相关文档

- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [XHTTP](api-xhttp.md)
- [XHTTPD](api-xhttpd.md)
