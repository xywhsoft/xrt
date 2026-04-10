# XNet V2 Network Mainline

> Overview of the current formal network mainline. This page no longer documents the legacy network module family, and instead focuses on `xnet-v2 + xtlssession + xhttp/xhttpd/xws`.

[Back to Index](README.en.md)

---

## 1. Positioning

XNet V2 is the current formal networking mainline in XRT. Its goals are:

- high performance
- cross-platform behavior
- consistent async semantics across threads, coroutines, futures, and wait-sources
- a foundation that can keep growing into HTTP, WebSocket, and AI-facing scenarios

It is not a simple refresh of the old TCP/UDP client helpers. It is the new unified network base.

---

## 2. Mainline Structure

The current network mainline is organized around:

- `xurl` and `xhttp_util` for URL and HTTP text utilities
- `xnet_base / xnet_port / xnet_engine` for the base runtime
- `xnet_stream / xnet_dgram / listener` for network objects
- `xnet_sync` for futures and wait-sources
- `xtlssession` for TLS
- `xhttp / xhttpd / xws` for the application layer

This is the reason the modern XRT network stack behaves like one infrastructure line instead of a pile of unrelated helpers.

---

## 3. Official API Surface

### Engine

```c
XXAPI void xrtNetEngineConfigInit(xnetengineconfig* pCfg);
XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg);
XXAPI void xrtNetEngineDestroy(xnetengine* pEngine);
XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine);
XXAPI void xrtNetEngineStop(xnetengine* pEngine);
XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine);
XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey, xnet_task_fn pfnTask, ptr pArg);
XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg);
```

### TLS Session

```c
XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer);
XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession);
XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession);
```

See also:

- [Network TLS](api-network-tls.en.md)

### Proxy

```c
XXAPI void xrtNetProxyConfigInit(xnetproxyconfig* pCfg);
XXAPI xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pCfg);
XXAPI xnetproxy* xrtNetProxyAddRef(xnetproxy* pProxy);
XXAPI void xrtNetProxyRelease(xnetproxy* pProxy);
```

Notes:

- `xnetproxy` is a shared, refcounted proxy object
- live connection state stores only `xnetproxy*`, not a copied full config block
- currently supported:
	- `SOCKS5 CONNECT`
	- `HTTP CONNECT`
- `xnetconnectconfig.pProxy == NULL` means direct connection

### Listener

```c
XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine, const xnetlistenconfig* pCfg, ...);
XXAPI void xrtNetListenerDestroy(xnetlistener* pListener);
XXAPI xnet_result xrtNetListenerStart(xnetlistener* pListener);
XXAPI void xrtNetListenerStop(xnetlistener* pListener);
```

### Future / Wait-Source

```c
XXAPI xnetfuture* xrtNetFutureCreate(void);
XXAPI void xrtNetFutureDestroy(xnetfuture* pFuture);
XXAPI xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs);
XXAPI ptr xrtNetFutureValue(xnetfuture* pFuture);
```

Official wait-source coverage currently includes:

- future
- stream wait-kind
- dgram recv
- listener accept

and supports:

- sync-thread wait
- coroutine wait
- timeout / deadline
- value-returning wait paths

---

## 4. Recommended Mental Model

The most useful way to understand the current network mainline is:

- engine: execution and ownership
- stream / dgram / listener: network objects
- future: async result object
- wait-source: unified waiting bridge
- coroutine / thread wait: the two formal consumption paths

That is why HTTP, WebSocket, TLS, and AI-facing streaming logic can continue to grow on one runtime instead of inventing a new waiting model in every layer.

---

## 5. Suggested Reading

- [Network TLS](api-network-tls.en.md)
- [XHTTP](api-xhttp.en.md)
- [XHTTPD](api-xhttpd.en.md)
- [XWS](api-xws.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
