# XWS WebSocket Mainline

> The current WebSocket mainline built on `xnet-v2 + xnet_stream + HTTP upgrade + xtlssession`.

[Back to Index](README.en.md)

---

## 1. Positioning

`xws` is the current WebSocket layer. It provides:

- WebSocket client
- WebSocket server
- HTTP upgrade handshake
- WebSocket client proxy access
- text / binary messaging
- ping / pong
- close
- fragmented-message reassembly

Its goals are:

- complete enough for infrastructure use
- still lightweight
- naturally aligned with the `xnet-v2` and TLS session mainlines

---

## 2. Core Types

Important configuration and event types include:

- `xwsclientconfig`
- `xwsserverconfig`
- `xwsclientevents`
- `xwsserverevents`

These define URL / origin / protocol, timeout / recv limit, optional shared proxy objects, TLS config for server mode, and message/lifecycle callbacks.

---

## 3. Official API

```c
XXAPI void xrtWsClientConfigInit(xwsclientconfig* pCfg);
XXAPI void xrtWsClientConfigUnit(xwsclientconfig* pCfg);
XXAPI bool xrtWsClientConfigSetURL(xwsclientconfig* pCfg, const char* sURL);
XXAPI bool xrtWsClientConfigSetOrigin(xwsclientconfig* pCfg, const char* sOrigin);
XXAPI bool xrtWsClientConfigSetProtocols(xwsclientconfig* pCfg, const char* sProtocols);
XXAPI const char* xrtWsClientConfigURL(const xwsclientconfig* pCfg);
XXAPI const char* xrtWsClientConfigOrigin(const xwsclientconfig* pCfg);
XXAPI const char* xrtWsClientConfigProtocols(const xwsclientconfig* pCfg);
XXAPI xwsclient* xrtWsClientCreate(xnetengine* pEngine, const xwsclientconfig* pCfg, const xwsclientevents* pEvents, ptr pUserData);
XXAPI xnet_result xrtWsClientStart(xwsclient* pClient);
XXAPI void xrtWsClientStop(xwsclient* pClient);
XXAPI void xrtWsClientDestroy(xwsclient* pClient);
XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen);
XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const void* pData, size_t iLen);
XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const void* pData, size_t iLen);
XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason);
XXAPI const char* xrtWsClientProtocol(const xwsclient* pClient);

XXAPI void xrtWsServerConfigInit(xwsserverconfig* pCfg);
XXAPI void xrtWsServerConfigUnit(xwsserverconfig* pCfg);
XXAPI bool xrtWsServerConfigSetProtocol(xwsserverconfig* pCfg, const char* sProtocol);
XXAPI const char* xrtWsServerConfigProtocol(const xwsserverconfig* pCfg);
XXAPI xwsserver* xrtWsServerCreate(xnetengine* pEngine, const xwsserverconfig* pCfg, const xwsserverevents* pEvents, ptr pUserData);
XXAPI uint16 xrtWsServerBoundPort(const xwsserver* pServer);
XXAPI xnet_result xrtWsServerStart(xwsserver* pServer);
XXAPI void xrtWsServerStop(xwsserver* pServer);
XXAPI void xrtWsServerDestroy(xwsserver* pServer);
XXAPI const char* xrtWsConnProtocol(const xwsconn* pConn);
```

Client notes:

- `xwsclientconfig.pProxy` is an optional shared proxy object
- create-time retains one reference
- destroy-time releases that reference
- the config setters preserve exact-length URL, Origin, and protocol-list values; the legacy `XWS_*_CAP` macros are inline-storage thresholds, not protocol limits
- call the matching config `Unit` function when setters were used; client/server creation deep-copies config text, so the source config may be released immediately afterward
- negotiated subprotocol strings remain owned by the client or connection and are available through `xrtWsClientProtocol()` and `xrtWsConnProtocol()` until that object is destroyed

---

## 4. Current Protocol Coverage

The current mainline includes:

- HTTP upgrade handshake
- client-side proxy handshake
- `Sec-WebSocket-Key / Accept`
- text / binary frames
- ping / pong
- close frames
- fragmented-message reassembly
- strict UTF-8 validation for text messages and close reasons
- strict close-code, control-frame, masking, RSV, opcode, and minimal-length validation

Client URLs must be absolute `ws://` or `wss://` URLs. Userinfo and fragments are rejected; long hosts and request targets are allocated dynamically. Handshakes validate unique, syntactically valid upgrade fields, keys, versions, and selected subprotocols. Servers require masked client frames, while clients require unmasked server frames.

Still explicitly deferred:

- extensions
- `permessage-deflate`
- more complex subprotocol negotiation strategies

Client connect sequencing is:

- `TCP connect`
- if `pProxy` is configured, complete the proxy handshake first
- if the URL is `wss://`, complete TLS next
- finally complete HTTP upgrade and enter the open state

---

## 5. Relationship to Other Modules

- the handshake phase reuses HTTP upgrade semantics
- each WebSocket connection is carried by `xnet_stream`
- `wss://` uses the current TLS session mainline
- when a proxy is configured, it is negotiated before the TLS / upgrade path
- the mainline is event-driven today, but already sits on the same async/network foundation as the rest of XRT

---

## 6. Suggested Reading

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [XHTTP](api-xhttp.en.md)
