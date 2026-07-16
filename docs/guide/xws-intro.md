# XWS 入门：建立可长期维护的 WebSocket 会话

> 本页介绍 XWS 的对象边界、连接时序、背压处理，以及何时把业务移交给 Queue、Task 和 Coroutine。

[返回教学文档](README.md) | [完整 API 契约](../api/api-xws.md)

---

## 1. XWS 位于哪一层

| 层 | 责任 |
| --- | --- |
| `xurl` / `xhttp` | URL 与 HTTP/1.1 Upgrade 语义 |
| `xnet-v2` | engine、TCP stream、代理、等待和背压 |
| `xtlssession` | `wss://` 的 TLS 会话 |
| `xws` | 握手、帧、消息、压缩和关闭状态机 |

XWS 不替代 TCP，也不是普通 HTTP 长连接。进入 101 后，stream 的协议所有权交给 XWS，后续按 RFC 6455 处理。

## 2. 三个对象

| 对象 | 作用 |
| --- | --- |
| `xwsclient` | 主动连接一个远端 WebSocket 服务 |
| `xwsserver` | 监听或为 HTTP Upgrade 提供策略与事件 |
| `xwsconn` | 服务端的一条已接受连接 |

服务端发送消息必须使用 `xwsconn`。如果需要把连接保存到回调外，先 `xrtWsConnRetain`，完成后 `xrtWsConnRelease`。

## 3. 最小客户端

```c
static void on_open(ptr owner, xwsclient* client)
{
    (void)owner;
    (void)xrtWsClientSendText(client, "hello", 5u);
}

static void on_text(ptr owner, xwsclient* client,
    const char* data, size_t len)
{
    (void)owner;
    (void)client;
    printf("recv: %.*s\n", (int)len, data);
}

xwsclientconfig cfg;
xwsclientevents events;
xwsclient* client;

xrtWsClientConfigInit(&cfg);
xrtWsClientEventsInit(&events);
events.OnOpen = on_open;
events.OnText = on_text;

xrtWsClientConfigSetURL(&cfg, "wss://example.com/ws");
cfg.iWebSocketFlags |= XWS_F_PERMESSAGE_DEFLATE;
client = xrtWsClientCreate(engine, &cfg, &events, NULL);
xrtWsClientConfigUnit(&cfg);

if (client) {
    (void)xrtWsClientStart(client);
}
```

客户端连接顺序是 `TCP -> 可选代理 -> 可选 TLS -> HTTP Upgrade -> OPEN`。`Start` 只表示启动成功；用 `OnOpen`、`StartFuture` 或 `WaitOpen*` 观察 Upgrade 完成。

生产环境保持 `bVerifyPeer = true`。需要自定义 CA、SNI、ALPN、证书验证回调时，通过 `xrtWsClientConfigSetTlsConfig` 设置完整 `xtlsconfig`。

## 4. 最小独立服务端

```c
static void on_server_text(ptr owner, xwsserver* server, xwsconn* conn,
    const char* data, size_t len)
{
    (void)owner;
    (void)server;
    (void)xrtWsConnSendText(conn, data, len);
}

xwsserverconfig cfg;
xwsserverevents events;
xwsserver* server;

xrtWsServerConfigInit(&cfg);
xrtWsServerEventsInit(&events);
events.OnText = on_server_text;

xrtNetAddrInitAny(&cfg.tBindAddr, AF_INET, 8080u);
xrtWsServerConfigSetPath(&cfg, "/ws");
server = xrtWsServerCreate(engine, &cfg, &events, NULL);
xrtWsServerConfigUnit(&cfg);

if (server) {
    (void)xrtWsServerStart(server);
}
```

固定 `Path`、`Origin` 和 subprotocol 适合简单服务。认证、多租户、动态协议选择和自定义拒绝正文应放在 `OnHandshake`。

## 5. 接入现有 HTTP 服务

不要为同一端口再建立一套 WebSocket listener。`xhttpd` handler 中使用 `xrtWsServerUpgradeHttpd`，`xweb` handler 中使用 `xrtWebResponseUpgradeWebSocket`：

```c
xwsconn* conn = NULL;

if (!xrtWebResponseUpgradeWebSocket(response, ws_server, &conn)) {
    /* Internal upgrade failure. */
}
```

返回成功但 `conn == NULL` 表示握手策略已经提交 HTTP 拒绝响应。返回有效连接表示 101 已提交，不能再写普通 HTTP response。

## 6. 回调只处理协议边界

事件运行在网络 worker 上。适合在回调中执行：

- 校验消息类型和最小业务 envelope；
- 复制借用 payload；
- 更新轻量连接状态；
- 把工作投递到 Queue/Task；
- 发送可立即构造的小响应。

不要在回调中执行阻塞数据库、文件、外部 HTTP、长事务或重 CPU。典型结构是：

```text
XWS callback -> copy/retain -> bounded queue -> task/coroutine -> send
```

跨任务保存 `xwsconn` 必须 retain。回调 payload 只在回调期间有效。

## 7. 必须处理背压

发送返回 `XRT_NET_AGAIN` 是正常流控，不是连接错误。不要循环忙重试：

```c
xnet_result result = xrtWsConnSendBinary(conn, data, len);

if (result == XRT_NET_AGAIN) {
    /* Keep the message and retry after writable. */
}
```

可以选择：

- 事件模型：`OnBackpressure`、`OnWritable`、`OnDrain`；
- Future：`WritableFuture`、`DrainFuture`；
- 同步线程：`WaitTimeoutEx(..., XNET_STREAM_WAIT_WRITABLE, ...)`；
- 协程：`WaitCoEx(..., XNET_STREAM_WAIT_WRITABLE)`。

应用自身的消息队列也必须有上限。XWS 的 `iMaxQueuedBytes` 只限制 transport 发送队列，不能替代业务队列配额。

## 8. 大消息和零拷贝

普通 Send 适合短消息。服务端明文大消息可用 `xnetbufref`，由 release 回调精确管理 payload 生命周期。客户端必须 mask，TLS 也需要加密，因此 Ref API 不保证物理零拷贝，只保证一致的所有权契约。

需要边生成边发送一条消息时使用 `xwswriter`。writer 期间不能并发发送另一条数据消息；Ping 和 Close 不受影响。`AGAIN` 时保留相同输入并在 writable 后重试。

接收端仍按完整消息交付，因此 `iRecvLimit` 应设为业务允许的最大消息，而不是机器可用内存。

## 9. 关闭与错误

正常退出调用 `xrtWs*Close(1000, reason)`，然后观察 `OnCloseEx` 或 `CloseFuture`。对端不完成握手时，`iCloseTimeoutMs` 到期后 XWS 会中止 transport。

诊断异步错误使用 `OnErrorEx` 或 `LastError`。结构化错误能区分 connect、TLS/Upgrade、protocol、UTF-8、limit、compression、send 和 close 阶段；线程全局错误文本不足以定位并发连接。

## 10. 选型表

| 需求 | 推荐入口 |
| --- | --- |
| 简单事件驱动客户端/服务端 | callbacks |
| 启动后等待连接结果 | `StartFuture` / `WaitOpen*` |
| 协程内等待流控或关闭 | `WaitCo*` |
| 现有 HTTP 路由升级 | HTTPD/XWeb Upgrade API |
| 慢业务和顺序编排 | bounded queue + task/coroutine |
| 大消息分片生成 | `xwswriter` |
| 引用计数 payload | `Send*Ref` |

## 11. 下一步

- [XWS 完整 API 契约](../api/api-xws.md)
- [用 XWS + Queue + Coroutine 写双向会话服务](../case/xws-session-queue-coroutine.md)
- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [Proxy 入门](proxy-intro.md)
