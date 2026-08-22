# XRT Guide: Maintainable WebSocket Sessions with XWS

> This page introduces XWS object boundaries, connection sequencing, backpressure, and the handoff to queues, tasks, and coroutines.

[中文](xws-intro.md) | [Back to Guides](README.en.md) | [Full API Contract](../api/api-xws.en.md)

---

## 1. Where XWS Sits

| Layer | Responsibility |
| --- | --- |
| `xurl` / `xhttp` | URL and HTTP/1.1 Upgrade semantics |
| `xnet-v2` | engine, TCP stream, proxy, waits, and backpressure |
| `xtlssession` | TLS session for `wss://` |
| `xws` | handshake, frames, messages, compression, and close state |

XWS does not replace TCP and is not an ordinary HTTP long connection. After 101, protocol ownership of the stream moves to XWS and RFC 6455 rules apply.

## 2. Three Objects

| Object | Role |
| --- | --- |
| `xwsclient` | actively connects to one remote WebSocket service |
| `xwsserver` | listens or provides policy/events for HTTP upgrades |
| `xwsconn` | one accepted server-side connection |

Server sends always target `xwsconn`. Retain a connection with `xrtWsConnRetain` before keeping it beyond a callback, then release it with `xrtWsConnRelease`.

## 3. Minimal Client

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

Client sequencing is `TCP -> optional proxy -> optional TLS -> HTTP Upgrade -> OPEN`. `Start` reports startup, not handshake completion. Observe completion through `OnOpen`, `StartFuture`, or `WaitOpen*`.

Keep `bVerifyPeer = true` in production. Use `xrtWsClientConfigSetTlsConfig` for custom CA, SNI, ALPN, certificates, or verification callbacks.

## 4. Minimal Standalone Server

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

Fixed Path, Origin, and subprotocol settings suit simple services. Authentication, tenancy, dynamic protocol selection, and custom rejection bodies belong in `OnHandshake`.

## 5. Upgrade an Existing HTTP Service

Do not open a second WebSocket listener for the same port. Use `xrtWsServerUpgradeHttpd` from an `xhttpd` handler or `xrtWebResponseUpgradeWebSocket` from an `xweb` handler:

```c
xwsconn* conn = NULL;

if (!xrtWebResponseUpgradeWebSocket(response, ws_server, &conn)) {
    /* Internal upgrade failure. */
}
```

Success with `conn == NULL` means handshake policy committed an HTTP rejection. A valid connection means 101 was committed and ordinary HTTP response writes are no longer valid.

## 6. Keep Callbacks at the Protocol Boundary

Events run on network workers. Good callback work includes:

- validating message type and a small business envelope;
- copying borrowed payloads;
- updating lightweight connection state;
- posting work to a queue or task;
- sending a small response that is already available.

Do not perform blocking database, file, remote HTTP, long transaction, or heavy CPU work in a callback. A typical architecture is:

```text
XWS callback -> copy/retain -> bounded queue -> task/coroutine -> send
```

Retain `xwsconn` across tasks. Callback payloads are valid only during the callback.

## 7. Backpressure Is Mandatory

`XRT_NET_AGAIN` is normal flow control, not a broken connection. Never spin-retry:

```c
xnet_result result = xrtWsConnSendBinary(conn, data, len);

if (result == XRT_NET_AGAIN) {
    /* Keep the message and retry after writable. */
}
```

Choose one surface:

- events: `OnBackpressure`, `OnWritable`, `OnDrain`;
- Future: `WritableFuture`, `DrainFuture`;
- blocking thread: `WaitTimeoutEx(..., XNET_STREAM_WAIT_WRITABLE, ...)`;
- coroutine: `WaitCoEx(..., XNET_STREAM_WAIT_WRITABLE)`.

The application message queue must also be bounded. XWS `iMaxQueuedBytes` limits only the transport send queue and is not a substitute for a business-queue quota.

## 8. Large Messages and Reference Sends

Plain Send methods suit short messages. A plaintext server can use `xnetbufref` for large payloads with exact release-callback ownership. Clients must mask and TLS must encrypt, so Ref methods do not promise physical zero-copy; they promise one consistent ownership contract.

Use `xwswriter` when producing one message incrementally. Another data message cannot be sent while a writer owns the slot, but Ping and Close remain available. Preserve the same input and retry after writable on `XRT_NET_AGAIN`.

Receive delivery remains message-oriented. Set `iRecvLimit` to the maximum message your application accepts, not to available machine memory.

## 9. Close and Errors

For normal shutdown, call `xrtWs*Close(1000, reason)` and observe `OnCloseEx` or `CloseFuture`. XWS aborts the transport after `iCloseTimeoutMs` when the peer does not finish the handshake.

Use `OnErrorEx` or `LastError` for asynchronous diagnostics. Structured errors distinguish connect, TLS/Upgrade, protocol, UTF-8, limit, compression, send, and close phases. A thread-global error string is insufficient for concurrent connections.

## 10. Selection Table

| Requirement | Preferred surface |
| --- | --- |
| simple event-driven client/server | callbacks |
| wait for startup result | `StartFuture` / `WaitOpen*` |
| await flow control or close in a coroutine | `WaitCo*` |
| upgrade an existing HTTP route | HTTPD/XWeb Upgrade API |
| slow work and sequential orchestration | bounded queue + task/coroutine |
| incrementally generated large message | `xwswriter` |
| reference-counted payload | `Send*Ref` |

## 11. Next Reading

- [XWS Full API Contract](../api/api-xws.en.md)
- [Case: XWS Session with Queue and Coroutine](../case/xws-session-queue-coroutine.md)
- [xnet-v2 and TLS Session Intro](xnet-v2-tls-intro.en.md)
- [Proxy Intro](proxy-intro.en.md)
