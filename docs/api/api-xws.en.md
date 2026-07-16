# XWS WebSocket Standard Library Contract

> XWS is the RFC 6455 client and server implementation built on `xnet-v2`, HTTP/1.1 Upgrade, and XRT TLS.

[Back to Index](README.en.md)

---

## 1. Supported Scope

The stable mainline supports:

- `ws://` and `wss://` clients;
- standalone WebSocket servers;
- upgrades from `xhttpd` and `xweb` requests;
- text, binary, ping, pong, and close;
- fragmented-message send and receive reassembly;
- subprotocol, Origin, Path, and custom request-header policies;
- strict UTF-8, mask, RSV, opcode, control-frame, length, and close-code validation;
- `permessage-deflate` with no-context-takeover negotiation;
- hard send limits, high/low watermarks, and writable/drain notification;
- copy and `xnetbufref` reference sends;
- Future, blocking wait, and coroutine wait surfaces;
- structured errors and complete close snapshots.

This contract excludes HTTP/2, HTTP/3, QUIC, and arbitrary third-party WebSocket extensions. Receive delivery is message-oriented; frame-level and streaming receive callbacks are not exposed.

## 2. ABI and Initialization

Public structures negotiate ABI through `iSize + iVersion`:

| Structure | Current version |
| --- | ---: |
| `xwsclientconfig` | V4 |
| `xwsserverconfig` | V4 |
| `xwsclientevents` | V3 |
| `xwsserverevents` | V3 |
| `xwscloseinfo` | V1 |
| `xwserrorinfo` | V1 |
| `xwshandshakeresponse` | V1 |

Always use the matching initializer instead of assuming a structure tail:

```c
xrtWsClientConfigInit(&cfg);
xrtWsClientEventsInit(&events);
xrtWsServerConfigInit(&server_cfg);
xrtWsServerEventsInit(&server_events);
```

V4 appends owned custom-TLS storage after the V3 configuration. Create functions continue to accept valid older structure versions. Event copying reads only the prefix declared by the caller's `iSize`.

## 3. Default Resource Limits

`ConfigInit` installs finite defaults suitable for general services:

| Setting | Client default | Server default |
| --- | ---: | ---: |
| `iRecvLimit` | 1 MiB | 1 MiB |
| `iMaxFrameBytes` | 1 MiB | 1 MiB |
| `iHighWater` | 256 KiB | 256 KiB |
| `iLowWater` | 64 KiB | 64 KiB |
| `iMaxQueuedBytes` | 1 MiB | 1 MiB |
| `iHandshakeMaxBytes` | HTTP/1 default header limit | HTTP/1 default header limit |
| `iHandshakeTimeoutMs` | 10 s | 10 s |
| `iCloseTimeoutMs` | 5 s | 5 s |
| `iCompressMinBytes` | 256 B | 256 B |
| `iCompressionLevel` | 6 | 6 |
| `iBacklog` | - | 128 |

`iRecvLimit` limits a reassembled message, `iMaxFrameBytes` limits one frame, and `iHandshakeMaxBytes` independently limits Upgrade headers. Servers should tighten these values for their workload. XWS never relies on an unbounded receive buffer.

`XWS_URL_CAP`, `XWS_PATH_CAP`, and related macros are inline-storage thresholds. Setters allocate longer valid values dynamically; these macros are not protocol limits.

## 4. Configuration Ownership

Use setters for dynamically sized values:

```c
xrtWsClientConfigSetURL(&cfg, "wss://example.com/socket");
xrtWsClientConfigSetOrigin(&cfg, "https://example.com");
xrtWsClientConfigSetProtocols(&cfg, "chat.v2, chat.v1");
xrtWsClientConfigSetHeader(&cfg, "Authorization", "Bearer ...");
xrtWsClientConfigSetTlsConfig(&cfg, &tls);
```

Client and server creation deep-copy strings, request headers, and TLS configuration. The source config may be released with `xrtWs*ConfigUnit` immediately after a successful create. TLS cloning deep-copies host names, ALPN, certificates, private keys, CA data, CRLs, and session data. C callbacks and their `pUserData` are borrowed and must remain alive until the WebSocket object is destroyed.

`pProxy` is reference-counted. Client creation retains it and client destruction releases it, so the caller may release its own reference after create.

Call the matching `ConfigUnit` whenever setters may have allocated storage. Never free `p*Storage` fields directly.

## 5. Handshake and Policy

Client URLs must be absolute `ws://` or `wss://` URLs. Userinfo and fragments are rejected. Connection order is fixed:

1. DNS/TCP connect;
2. optional proxy handshake;
3. TLS handshake for `wss://`;
4. HTTP/1.1 Upgrade;
5. enter OPEN and invoke `OnOpen`.

Clients may inspect the 101 response through `OnHandshakeResponse`; returning `false` rejects the connection.

A standalone server may configure fixed Path, Origin, and subprotocol rules or apply per-request policy in `OnHandshake`:

- return `XWS_HANDSHAKE_ACCEPT` to accept;
- return `XWS_HANDSHAKE_REJECT` to send the configured HTTP rejection;
- return `XWS_HANDSHAKE_ERROR` to terminate the handshake.

`xwshandshakeresponse` controls status, reason, headers, body, selected subprotocol, per-connection data, and `XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE`. A selected protocol must be one offered by the client.

Existing HTTP services can upgrade with:

```c
xrtWsServerUpgradeHttpd(ws_server, http_conn, request, &ws_conn);
xrtWebResponseUpgradeWebSocket(response, ws_server, &ws_conn);
```

Upgrade must run in the HTTP worker/handler context. A successful 101 transfers the stream to XWS. An HTTP rejection remains owned and committed by the HTTP layer, with `ws_conn == NULL`.

## 6. Event and Thread Contract

Events run on network-engine worker threads. Do not block a worker unless an API explicitly permits it. Payloads passed to `OnText`, `OnBinary`, `OnPing`, and `OnPong`, as well as handshake request/response views, are valid only for the current callback. Copy data that must outlive the callback.

A server `xwsconn*` is valid for the callback duration. Call `xrtWsConnRetain` before storing it across callbacks or tasks, then `xrtWsConnRelease` when done. `xrtWsConnSetData/GetData` store a borrowed pointer and never own the user object.

Legacy `OnClose` and `OnError` remain for compatibility. New code should use `OnCloseEx` and `OnErrorEx`.

## 7. Messages and Compression

The receiver reassembles fragments and delivers complete messages through `OnText` or `OnBinary`. Text and close reasons must contain complete, minimally encoded UTF-8. Invalid input causes a protocol close. Ping receives an automatic Pong while still invoking `OnPing`.

Enable compression with:

```c
cfg.iWebSocketFlags |= XWS_F_PERMESSAGE_DEFLATE;
```

This is available only when `XWS_HAS_PERMESSAGE_DEFLATE == 1`. XWS negotiates client/server no-context-takeover only, keeping memory bounded and avoiding cross-message compression state. A sender compresses only when the message reaches `iCompressMinBytes` and the result is smaller. Query the negotiated state with `xrtWsClientPerMessageDeflate` or `xrtWsConnPerMessageDeflate`.

## 8. Backpressure and Send Ownership

`iMaxQueuedBytes` is a hard limit, not only a notification threshold. Data sends reserve budget for Ping/Pong/Close control frames. If the complete frame cannot be accepted, a send returns `XRT_NET_AGAIN`; no partial message is accepted.

The required flow is:

1. stop producing more data;
2. await `OnWritable`, `WritableFuture`, or `XNET_STREAM_WAIT_WRITABLE`;
3. retry the same operation;
4. use `OnDrain`, `DrainFuture`, or `XNET_STREAM_WAIT_DRAIN` when complete queue drain is required.

`OnBackpressure` fires after crossing the high watermark, `OnWritable` after returning to the writable range, and `OnDrain` when the send queue reaches zero. `xrtWs*PendingSend` reports currently queued bytes.

Plain `SendText/SendBinary` operations copy or frame during the call; the source storage may be reused after return.

`SendTextRef/SendBinaryRef` and writer Ref methods use `xnetbufref`:

- `XRT_NET_OK` means ownership was accepted and `pfnRelease` will run exactly once, possibly before the call returns;
- `XRT_NET_AGAIN`, `XRT_NET_ERROR`, or `XRT_NET_CLOSED` means ownership was not accepted and XWS will not call release;
- client masking and TLS may require one framing copy while preserving the same ownership contract;
- plaintext server sends can pass the payload reference directly to the scatter/gather queue.

## 9. Fragment Writers

`xrtWsClientBeginText/Binary` and `xrtWsConnBeginText/Binary` create an uncompressed fragmented-message writer. While alive, the writer owns the connection's data-message send slot. Other text/binary sends or writers fail, while Ping and Close remain available.

```c
xwswriter* writer = xrtWsConnBeginText(conn);
xrtWsWriterWrite(writer, first, first_len);
xrtWsWriterFinish(writer, last, last_len);
xrtWsWriterDestroy(writer);
```

`Write` sends a non-final fragment and `Finish` sends the final fragment. On `XRT_NET_AGAIN`, writer state does not advance and the same input may be retried after writable. Text writers carry incremental UTF-8 state across fragments; the final fragment must end on a complete code point. Destroying an unfinished writer releases exclusivity but does not synthesize a final frame.

## 10. Close State Machine

`xrtWs*Close(code, reason)` sends Close and disallows new data messages while allowing the close handshake to complete. XWS replies to a peer Close as required by RFC 6455. The transport is aborted if the handshake does not finish within `iCloseTimeoutMs`.

`xwscloseinfo` records local and remote close codes, remote reason, transport result, and flags for sent, received, clean, and remote-initiated state.

`CloseFuture`, `XNET_STREAM_WAIT_CLOSE`, and `OnCloseEx` observe the same terminal state. `Stop` and `Destroy` may be called outside worker threads. Release application-held server connection references before final teardown.

## 11. Future, Blocking, and Coroutine Surfaces

Client startup can be observed through equivalent surfaces:

- events: `xrtWsClientStart` plus `OnOpen/OnErrorEx`;
- Future: `xrtWsClientStartFuture` or `xrtWsClientOpenFuture`;
- wait: `WaitOpen`, `WaitOpenTimeout/Until`, or `WaitOpenCo*`.

Clients and server connections expose writable, drain, and close Futures and waits:

```c
xrtWsClientWaitTimeoutEx(client, XNET_STREAM_WAIT_WRITABLE, 5000);
xrtWsConnWaitCoEx(conn, XNET_STREAM_WAIT_DRAIN);
```

Supported wait kinds are `XNET_STREAM_WAIT_WRITABLE`, `XNET_STREAM_WAIT_DRAIN`, and `XNET_STREAM_WAIT_CLOSE`. Never perform a blocking wait on the same network worker; use `WaitCo*` from a coroutine.

## 12. Error Model

`OnErrorEx`, `xrtWsClientLastError`, `xrtWsConnLastError`, and `xrtWsServerLastError` return an `xwserrorinfo` snapshot containing:

- `iResult`;
- category: transport, handshake, protocol, UTF-8, limit, compression, memory, callback, or internal;
- operation: connect, handshake, receive, send, message, close, or accept;
- phase: validate, build, parse, policy, submit, process, timeout, or complete;
- system error, HTTP status, protocol error, close code, and stable diagnostic text.

The object owns its current snapshot and accessors copy it to caller storage. Do not rely only on a thread-global error string for asynchronous diagnostics.

## 13. Minimal Client

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
    /* OnOpen, a Future, or a wait reports handshake completion. */
}
```

## 14. Release Gates

See [HTTP / WebSocket Release Gates](../HTTP_WEBSOCKET_RELEASE_GATES.en.md) for the parser, interoperability, 1009 recovery, reconnect, and single-header commands. The deterministic drivers do not replace Clang/libFuzzer coverage-guided fuzzing.

## 15. Related Documentation

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [XHTTP](api-xhttp.en.md)
- [XHTTPD](api-xhttpd.en.md)
